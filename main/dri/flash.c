#include "flash.h"
#include "nvs.h"
#include "nvs_flash.h"

void Flash_Init(void)
{
    // nvs_flash是esp32自带的flash的某个分区（nvs分区，non volatile storage）
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void Flash_InitPassword()
{
    // 将密码 123456 保存到nvs flash中
    // 声明指向存储空间的句柄
    nvs_handle_t my_handle;
    nvs_open("smart-lock", NVS_READWRITE, &my_handle);
    static size_t pw_len = 7;
    if (nvs_get_str(my_handle, "password", NULL, &pw_len) == ESP_ERR_NVS_NOT_FOUND) {
        // 如果不存在，则写入
        nvs_set_str(my_handle, "password", "123456");
        nvs_commit(my_handle);
    }
    nvs_close(my_handle);
}

void Flash_WritePassword(char *password)
{
    // 将密码 123456 保存到nvs flash中
    // 声明指向存储空间的句柄
    nvs_handle_t my_handle;
    nvs_open("smart-lock", NVS_READWRITE, &my_handle);
    // 将密码写入nvs flash
    nvs_set_str(my_handle, "password", password);
    nvs_commit(my_handle);
    nvs_close(my_handle);
}

void Flash_ReadPassword(char *password, size_t *pw_len)
{
    // 读取密码
    nvs_handle_t my_handle;
    nvs_open("smart-lock", NVS_READWRITE, &my_handle);
    nvs_get_str(my_handle, "password", password, pw_len);
    nvs_close(my_handle);
}