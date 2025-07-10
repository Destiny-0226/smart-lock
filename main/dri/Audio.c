#include "Audio.h"
#include "driver/gpio.h"
#include "utils.h"

#define AUDIO_SDA_PIN GPIO_NUM_9
#define AUDIO_BUSY_PIN GPIO_NUM_7

#define AUDIO_SDA_H gpio_set_level(AUDIO_SDA_PIN, 1)
#define AUDIO_SDA_L gpio_set_level(AUDIO_SDA_PIN, 0)

#define AUDIO_READ_BUSY gpio_get_level(AUDIO_BUSY_PIN)

void Audio_Init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << AUDIO_SDA_PIN);
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << AUDIO_BUSY_PIN);
    gpio_config(&io_conf);
}

void Audio_Play(uint8_t data)
{
    /// 先拉高2ms
    AUDIO_SDA_H;
    DelayMs(2);
    // 拉低10ms
    AUDIO_SDA_L;
    DelayMs(10);
    for (int i = 0; i < 8; i++)
    {
        if (data & 0x01)
        {
            AUDIO_SDA_H;
            DelayUs(600);
            AUDIO_SDA_L;
            DelayUs(200);
        }
        else
        {
            AUDIO_SDA_H;
            DelayUs(200);
            AUDIO_SDA_L;
            DelayUs(600);
        }
        data >>= 1;
    }
    // 拉高2ms
    AUDIO_SDA_H;
    DelayMs(2);
}