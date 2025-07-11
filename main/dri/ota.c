#include "ota.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#define HASH_LEN               32
#define OTA_URL_SIZE           256
#define EXAMPLE_NETIF_DESC_STA "example_netif_sta"

static const char *bind_interface_name = EXAMPLE_NETIF_DESC_STA;

static const char *TAG = "OTA任务: ";

/// sha256是一种加密算法，类似于md5
/// 将输入转换成256个bit的数值
/// sha512, sha128
static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

/// 获取分区的sha256值
void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = {0};
    esp_partition_t partition;

    // get sha256 digest for bootloader
    // 计算bootloader分区的sha256哈希值
    partition.address = ESP_BOOTLOADER_OFFSET;
    partition.size    = ESP_PARTITION_TABLE_OFFSET;
    partition.type    = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    // 获取正在运行的分区的哈希值
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}

/// 处理一系列的HTTP事件
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

void ota_init(void)
{
    // 计算当前正在运行的分区的sha256数字签名，并保存到otadata分区
    // 重新启动时，校验每个APP分区的哈希值，并和otadata中的哈希值进行比较
    // 不相同，则跳过去执行。
    get_sha256_of_partitions();
    ESP_LOGI(TAG, "OTA任务开始了。\r\n");
    /// ota相关配置
    esp_http_client_config_t config = {
        /// 新固件的下载地址
        .url               = "https://tutuzhizhi.oss-cn-beijing.aliyuncs.com/led_strip.bin?Expires=1752316055&OSSAccessKeyId=TMP.3KrpHg75JCK9pXsdf7hgYVNxC1U3tkKUWwdVGmhAv6nNC8PimJXAJhktQemw6gXJacfq5ZeTPBs1QtpCGuJDXKF3EGykhg&Signature=uyEIA%2BljbK2NNRSBK%2FRagikww9c%3D",
        .crt_bundle_attach = esp_crt_bundle_attach, // 加密相关
        .event_handler     = _http_event_handler,   /// 注册处理HTTP事件的回调函数
        .keep_alive_enable = true,                  // 保持活跃连接
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    /// 开始下载远程的二进制文件
    esp_err_t ret = esp_https_ota(&ota_config);
    /// 下载完成
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        /// 重启esp32开发板
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
}
