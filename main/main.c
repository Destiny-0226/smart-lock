/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "portmacro.h"
#include "utils.h"
#include "inttypes.h"
#include "esp_intr_alloc.h"

#include "dri/keyboard.h"
#include "dri/Motor.h"
#include "dri/Audio.h"
#include "dri/LED.h"
#include "dri/Fingerprint.h"
#include "dri/flash.h"
#include "dri/bluetooth.h"
#include "dri/wifi.h"

// 读取按键值任务
#define READ_KEY_TASK_NAME       "read_key_task"
#define READ_KEY_TASK_STACK_SIZE 2048
#define READ_KEY_TASK_PRIORITY   5
#define READ_KEY_TASK_CYCLE_MS   10
static void read_key_task(void *arg);
static TaskHandle_t read_key_task_handle;
// 按键获取任务
#define PASSWORD_LEN        6                   // 密码是长度
#define PASSWORD_TIMEOUT_MS pdMS_TO_TICKS(5000) // 超时间隔 (每两次按键按下的间隔 ms)
static void led_task(void *arg);
char password[PASSWORD_LEN]         = {0}; // 密码
static uint8_t pw_index             = 0;   // 密码输入索引
static TickType_t pw_input_run_time = 0;   // 密码输入运行时间

// 指纹识别任务
#define FINGERPRINT_TASK_NAME       "fingerprint_task"
#define FINGERPRINT_TASK_STACK_SIZE 2048
#define FINGERPRINT_TASK_PRIORITY   5
#define FINGERPRINT_TASK_CYCLE_MS   10
TaskHandle_t fingerprint_task_handle;
static void fingerprint_task(void *arg);

// LED任务
#define LED_TASK_NAME       "LED_task"
#define LED_TASK_STACK_SIZE 2048
#define LED_TASK_PRIORITY   5
#define LED_TASK_CYCLE_MS   10
TaskHandle_t led_task_handle;

// 超时检测任务
#define TIMEOUT_TASK_NAME       "timeout_task"
#define TIMEOUT_TASK_STACK_SIZE 2048
#define TIMEOUT_TASK_PRIORITY   6
#define TIMEOUT_TASK_CYCLE_MS   1000
TaskHandle_t timeout_task_handle;
static void timeout_task(void *arg);

static uint8_t finger_enroll       = 0;
static uint8_t finger_enroll_count = 0;

static uint8_t finger_xx = 0;

// 中断处理函数
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int32_t gpio_num = (int32_t)arg;
    if (gpio_num == KEYBOARD_INT_PIN) {
        // 直接任务通知：通知读取按键任务
        vTaskNotifyGiveFromISR(read_key_task_handle, NULL);
    } else if (gpio_num == FINGER_TOUCH_INT_PIN && finger_xx == 0) {
        finger_xx = 1;
        // 直接任务通知：通知指纹任务处理中断
        vTaskNotifyGiveFromISR(fingerprint_task_handle, NULL);
    }
    gpio_num = -1;
}

void app_main(void)
{

    // 初始化蓝牙
    Bluetooth_Init();

    // 初始化wifi
    Wifi_Init();
    // 中断配置

    // 初始化NVS_Flash
    Flash_Init();

    // 初始化 原始密码
    Flash_InitPassword();

    // 初始化keyboard
    Keyboard_Init();

    // 初始化电机
    Motor_Init();

    // 初始化音频
    Audio_Init();

    // 初始化RTM模块
    LED_RMT_Init();

    // 初始化指纹模块
    Fingerprint_Init();

    // 启用esp32的gpio中断，使用默认配置启用中断
    gpio_install_isr_service(0);

    // 将KEYBOARD_INT和gpio_isr_handler绑定起来
    gpio_isr_handler_add(KEYBOARD_INT_PIN, gpio_isr_handler, (void *)KEYBOARD_INT_PIN);
    gpio_isr_handler_add(FINGER_TOUCH_INT_PIN, gpio_isr_handler, (void *)FINGER_TOUCH_INT_PIN);

    // 创建 按键读取任务
    xTaskCreate(read_key_task, READ_KEY_TASK_NAME, READ_KEY_TASK_STACK_SIZE, NULL, READ_KEY_TASK_PRIORITY, &read_key_task_handle);

    // 创建 指纹读取任务
    xTaskCreate(fingerprint_task, FINGERPRINT_TASK_NAME, FINGERPRINT_TASK_STACK_SIZE, NULL, FINGERPRINT_TASK_PRIORITY, &fingerprint_task_handle);

    // 创建 LED任务
    xTaskCreate(led_task, LED_TASK_NAME, LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, &led_task_handle);

    // 创建 超时检测任务
    xTaskCreate(timeout_task, TIMEOUT_TASK_NAME, TIMEOUT_TASK_STACK_SIZE, NULL, TIMEOUT_TASK_PRIORITY, &timeout_task_handle);
}

static void read_key_task(void *arg)
{

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint8_t key_content = Keyboard_ReadKey();

        // key_content: 0~9 数字键，10=#，11=* （需确认实际键盘映射）

        // 直接通知LED任务
        xTaskNotify(led_task_handle, key_content, eSetValueWithOverwrite);
        Audio_Play(11);

        if (key_content == 10) { // 按下三次 "#" 进入录入指纹模式
            pw_index = 0;
            finger_enroll_count++;
        } else {
            finger_enroll_count = 0;
        }

        if (finger_enroll_count == 3) {
            xTaskNotifyGive(fingerprint_task_handle);
            finger_enroll       = 1;
            finger_enroll_count = 0;
        }

        // 键盘按键 输入密码
        if (key_content < 10) {
            // 触发 超时检测 任务
            // xTaskNotifyGive(timeout_task_handle);
            // 更新 密码输入时间
            // 进入临界区
            taskENTER_CRITICAL(NULL);
            pw_input_run_time = xTaskGetTickCount();
            taskEXIT_CRITICAL(NULL);
            password[pw_index] = '0' + key_content;
            pw_index++;
        }

        if (pw_index == PASSWORD_LEN) {
            char password_flash[PASSWORD_LEN] = {0};
            size_t pw_len                     = sizeof(password_flash);
            Flash_ReadPassword(password_flash, &pw_len);
            if (memcmp(password, password_flash, sizeof(password)) == 0) {
                printf("密码正确\r\n");
                Motor_OpenLock();
            } else {
                printf("密码错误\r\n");
            }

            pw_index = 0;
            memset(password, 0, sizeof(password)); // 清空密码
        }
    }
}

// 指纹任务
static void fingerprint_task(void *arg)
{
    while (1) {
        // 等待任务通知，无限期阻塞
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // 阻塞直到下一次通知
        if (finger_enroll == 1) {
            // 指纹录入模式
            finger_xx = 1;
            Finger_Enroll();
            finger_enroll       = 0;
            finger_enroll_count = 0;
        } else { // 指纹检索模式
            if (Finger_Identifiy() == 0) {
                printf("指纹验证成功\r\n");
                Motor_OpenLock();
            } else {
                printf("指纹验证失败\r\n");
            }
        }

        while (Finger_Sleep() != 0) {
            DelayMs(50);
        }
        finger_xx = 0;
    }
}

// LED任务
static void led_task(void *arg)
{
    // LED_Background_Light(170, 228, 26, 100);

    uint32_t key_content = 0;
    while (1) {

        // 等待任务通知（无限阻塞），获取通知值
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &key_content, portMAX_DELAY)) {
            if (key_content < 12) {
                LED_Keyoard_Light((uint8_t)key_content, 236, 56, 216, 10, 100);
            }
            // LED_Background_Light(170, 228, 26, 100);
        }
        DelayMs(LED_TASK_CYCLE_MS);
    }
}

static void timeout_task(void *arg)
{
    while (1) {
        // 密码输入 超时检测
        if (pw_input_run_time > 0) {
            if (xTaskGetTickCount() - pw_input_run_time > PASSWORD_TIMEOUT_MS) {
                pw_input_run_time = 0;
                pw_index          = 0;
                memset(password, 0, sizeof(password)); // 清空密码
                printf("密码输入超时,请重新输入\r\n");
            }
        }
        DelayMs(TIMEOUT_TASK_CYCLE_MS);
    }
}