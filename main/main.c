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

// 读取按键值任务
#define READ_KEY_TASK_NAME       "read_key_task"
#define READ_KEY_TASK_STACK_SIZE 2048
#define READ_KEY_TASK_PRIORITY   5
#define READ_KEY_TASK_CYCLE_MS   10
static void read_key_task(void *arg);
static TaskHandle_t read_key_task_handle;

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
static void led_task(void *arg);

static uint8_t finger_enroll       = 0;
static uint8_t finger_enroll_count = 0;

// 中断处理函数
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    if (gpio_num == KEYBOARD_INT_PIN) {
        // 直接任务通知：通知读取按键任务
        vTaskNotifyGiveFromISR(read_key_task_handle, NULL);
    } else if (gpio_num == FINGER_TOUCH_INT_PIN) {
        // 直接任务通知：通知指纹任务处理中断
        vTaskNotifyGiveFromISR(fingerprint_task_handle, NULL);
    }
}

void app_main(void)
{
    // 初始化蓝牙
    Bluetooth_Init();
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

    // 中断配置

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
}

// 按键获取任务
static void read_key_task(void *arg)
{

    char password[10]       = {0};
    static uint8_t pw_index = 0;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint8_t key_content = Keyboard_ReadKey();
        if (key_content < 12) {
            // 直接任务通知LED任务
            xTaskNotify(led_task_handle, key_content, eSetValueWithOverwrite);
            // 播放按下数字键盘音效
            Audio_Play(11);

            if (key_content == 10) {
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
            if (key_content < 10 && key_content > 0) {
                password[pw_index] = '0' + key_content;
                pw_index++;
            }
            if (pw_index == 6) {
                char password_flash[6] = {0};
                static size_t pw_len   = 7;
                Flash_ReadPassword(password_flash, &pw_len);
                if (memcmp(password, password_flash, 6) == 0) {
                    printf("密码正确\r\n");
                    Motor_OpenLock();
                } else {
                    printf("密码错误\r\n");
                }
                pw_index = 0;
            }
        }
        DelayMs(READ_KEY_TASK_CYCLE_MS);
    }
}

// 指纹任务
static void fingerprint_task(void *arg)
{
    while (1) {
        // 等待任务通知，无限期阻塞
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (finger_enroll == 1) { // 指纹录入模式
            // LED_Background_Light(89, 20, 62, 100);
            Finger_Enroll();
            finger_enroll       = 0;
            finger_enroll_count = 0;
        } else if (finger_enroll == 0) { // 指纹检索模式
            // LED_Background_Light(10, 10, 10, 50);
            if (Finger_Identifiy() == 0) {
                // 开锁
                Motor_OpenLock();
            }
        }
        // printf("指纹中断被触发了！\r\n");
        DelayMs(10);
        Finger_Sleep();
    }
    DelayMs(READ_KEY_TASK_CYCLE_MS);
}

// LED任务
static void led_task(void *arg)
{
    uint32_t key_content = 0;
    while (1) {
        // if (finger_enroll == 1) {
        //     LED_Background_Light(89, 20, 62, 100);
        // } else {
        //     LED_Background_Light(0, 0, 0, 0);
        // }
        // 等待任务通知（无限阻塞），获取通知值
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &key_content, portMAX_DELAY)) {
            if (key_content < 12) {
                LED_Keyoard_Light((uint8_t)key_content, 236, 56, 216, 10, 100);
            }
        }
        DelayMs(LED_TASK_CYCLE_MS);
    }
}
