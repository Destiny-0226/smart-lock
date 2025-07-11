#include "LED.h"
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/// ESP32 错误处理的API
#include "esp_check.h"
/// RMT编码器相关API
#include "driver/rmt_encoder.h"
/// RMT发送数据的API
#include "driver/rmt_tx.h"

// 10MHz的分辨率，1 tick = 0.1us，LED条带需要高分辨率。
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000
// 只需要一个引脚，引脚号为6。
#define RMT_LED_STRIP_GPIO_NUM GPIO_NUM_6

// 共12个LED灯。
#define LED_NUMBERS 12

// LED编码器配置结构体，主要配置编码器的频率。
typedef struct
{
    uint32_t resolution;
} led_strip_encoder_config_t;

// 编码器结构体
typedef struct
{
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

// 颜色的初始值。
uint32_t red   = 0;
uint32_t green = 0;
uint32_t blue  = 0;
uint16_t hue   = 0;

// 日志前缀
const char *TAG = "LED ENCODER";

// 每个灯需要发送RGB三个颜色，所以一共发送的数量要乘以3。
// 这个数组用来存放待发送的RGB像素值。
uint8_t led_strip_pixels[LED_NUMBERS * 3];

// LED通道
rmt_channel_handle_t led_chan = NULL;
// LED编码器
rmt_encoder_handle_t led_encoder = NULL;
// 发送配置，不进行循环发送
rmt_transmit_config_t tx_config = {
    .loop_count = 0,
};

size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder   = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder    = led_encoder->copy_encoder;
    rmt_encode_state_t session_state     = RMT_ENCODING_RESET;
    rmt_encode_state_t state             = RMT_ENCODING_RESET;
    size_t encoded_symbols               = 0;
    switch (led_encoder->state) {
        case 0: // send RGB data
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = 1; // switch to next state when current encoding session finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space for encoding artifacts
            }
        // fall-through
        case 1: // send reset code
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                                                    sizeof(led_encoder->reset_code), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
                state |= RMT_ENCODING_COMPLETE;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space for encoding artifacts
            }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret                        = ESP_OK;
    rmt_led_strip_encoder_t *led_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    led_encoder = calloc(1, sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del    = rmt_del_led_strip_encoder;
    led_encoder->base.reset  = rmt_led_strip_encoder_reset;
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0    = 1,
            .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
            .level1    = 0,
            .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
        },
        .bit1 = {
            .level0    = 1,
            .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
            .level1    = 0,
            .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
        },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    uint32_t reset_ticks    = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us
    led_encoder->reset_code = (rmt_symbol_word_t){
        .level0    = 0,
        .duration0 = reset_ticks,
        .level1    = 0,
        .duration1 = reset_ticks,
    };
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i    = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
        case 0:
            *r = rgb_max;
            *g = rgb_min + rgb_adj;
            *b = rgb_min;
            break;
        case 1:
            *r = rgb_max - rgb_adj;
            *g = rgb_max;
            *b = rgb_min;
            break;
        case 2:
            *r = rgb_min;
            *g = rgb_max;
            *b = rgb_min + rgb_adj;
            break;
        case 3:
            *r = rgb_min;
            *g = rgb_max - rgb_adj;
            *b = rgb_max;
            break;
        case 4:
            *r = rgb_min + rgb_adj;
            *g = rgb_min;
            *b = rgb_max;
            break;
        default:
            *r = rgb_max;
            *g = rgb_min;
            *b = rgb_max - rgb_adj;
            break;
    }
}

void LED_RMT_Init(void)
{
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,    // 选择时钟源
        .gpio_num          = RMT_LED_STRIP_GPIO_NUM, // GPIO引脚设置
        .mem_block_symbols = 64,                     // increase the block size can make the LED less flickering
        .resolution_hz     = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_ERROR_CHECK(rmt_enable(led_chan));
}

// LED 背景灯
void LED_Background_Light(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    for (uint8_t i = 1; i <= 12; i++) {
        LED_Keyoard_Light(i, red, green, blue, brightness, 0);
    }
}

/**
 * @brief 设置指定 LED 的颜色并点亮
 *
 * 该函数用于设置指定编号的 LED 的 RGB 颜色，并通过 RMT 发送数据点亮 LED。
 * 支持亮度调节功能，通过比例缩放 RGB 值实现。
 *
 * @param led_num      要设置的 LED 编号 (0 到 LED_NUMBERS - 1)
 * @param red          红色分量 (0 到 255)
 * @param green        绿色分量 (0 到 255)
 * @param blue         蓝色分量 (0 到 255)
 * @param brightness   亮度百分比 (0 到 100)，0 表示熄灭，100 表示原始颜色亮度
 * @param delay_ms     延迟时间，单位毫秒
 */
void LED_Keyoard_Light(uint8_t led_num, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness, uint16_t delay_ms)
{
    // 根据亮度调整 RGB 值
    if (brightness > 100) {
        brightness = 100;
    }

    uint8_t adjusted_red   = (uint32_t)red * brightness / 100;
    uint8_t adjusted_green = (uint32_t)green * brightness / 100;
    uint8_t adjusted_blue  = (uint32_t)blue * brightness / 100;

    // 设置像素值到对应 LED
    led_strip_pixels[led_num * 3 + 0] = adjusted_green; // Green 分量放在第一位
    led_strip_pixels[led_num * 3 + 1] = adjusted_blue;  // Blue 分量放在第二位
    led_strip_pixels[led_num * 3 + 2] = adjusted_red;   // Red 分量放在第三位

    // 发送数据到 LED
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

    if (delay_ms > 0) {
        DelayMs(delay_ms); // 等待一段时间

        // 清空像素数组，熄灭 LED
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));

        // 再次发送清零数据，确保 LED 熄灭
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    }
}