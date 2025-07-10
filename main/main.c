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

// 读取按键值任务
#define READ_KEY_TASK_NAME       "read_key_task"
#define READ_KEY_TASK_STACK_SIZE 2048
#define READ_KEY_TASK_PRIORITY   5
#define READ_KEY_TASK_CYCLE_MS   10
static void read_key_task(void *arg);

// // 指纹识别任务
// #define FINGERPRINT_TASK_NAME       "fingerprint_task"
// #define FINGERPRINT_TASK_STACK_SIZE 2048
// #define FINGERPRINT_TASK_PRIORITY   5
// #define FINGERPRINT_TASK_CYCLE_MS   10
// static void fingerprint_task(void *arg);

// // LED任务

// #define LED_TASK_NAME       "LED_task"
// #define LED_TASK_STACK_SIZE 2048
// #define LED_TASK_PRIORITY   5
// #define LED_TASK_CYCLE_MS   10
// static void led_task(void *arg);

// 1. 要将KEYBOARD_INT中断引脚和对应的回调函数绑定起来
// 2. 处理GPIO中断的回调函数的逻辑怎么编写？
//    - 中断对应的回调函数要很快运行完。
//    -
//    中断对应的回调函数必须能够执行成功。（不能写和外界通信的代码，例如uart，iic，spi等等）
//    - 当有按键按下时，触发回调函数，将中断的GPIO引脚号，发送到一个队列中。
// 3. 编写一个实时任务
//    while (1) {
//         1. 从队列中读取引脚号，如果队列为空，则阻塞等待
//         2. 如果读取成功
//            判断是否为引脚号 KEYBOARD_INT ，如果是，则调用
//            Keyboard_ReadKey()，如果不是则执行其他逻辑
//    }

// 队列
static QueueHandle_t gpio_evt_queue = NULL;
// 中断处理函数
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
void app_main(void)
{
    // 初始化keyboard
    Keyboard_Init();

    // 初始化电机
    Motor_Init();
    // 开锁 =====================
    // Motor_OpenLock();

    // 初始化音频
    Audio_Init();

    // 初始化RTM模块
    RMT_Init();

    // 初始化指纹模块
    Fingerprint_Init();

    // 创建队列
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // 中断配置

    // 启用esp32的gpio中断，使用默认配置启用中断
    gpio_install_isr_service(0);

    // 将KEYBOARD_INT和gpio_isr_handler绑定起来
    gpio_isr_handler_add(KEYBOARD_INT, gpio_isr_handler, (void *)KEYBOARD_INT);
    gpio_isr_handler_add(FINGER_TOUCH_INT_PIN, gpio_isr_handler, (void *)FINGER_TOUCH_INT_PIN);

    // 创建 按键读取任务
    xTaskCreate(read_key_task, READ_KEY_TASK_NAME, READ_KEY_TASK_STACK_SIZE, NULL, READ_KEY_TASK_PRIORITY, NULL);

    // 创建 指纹读取任务
    // xTaskCreate(fingerprint_task, FINGERPRINT_TASK_NAME, FINGERPRINT_TASK_STACK_SIZE, NULL, FINGERPRINT_TASK_PRIORITY, NULL);

    // 创建LED任务
    // xTaskCreate(led_task, LED_TASK_NAME, LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);
}

static void read_key_task(void *arg)
{
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (io_num == KEYBOARD_INT) {
                uint8_t key_content = Keyboard_ReadKey();
                if (key_content < 12) {
                    Audio_Play(11);
                    light_led(key_content);
                }
            }
            if (io_num == FINGER_TOUCH_INT_PIN) {
                printf("指纹中断被触发了！\r\n");
                while (Finger_Sleep()) {
                    DelayMs(10);
                }
            }
            DelayMs(READ_KEY_TASK_CYCLE_MS);
        }
    }
}
// static void fingerprint_task(void *arg)
// {
//     while (1) {
//     }
// }

// LED任务
// static void led_task(void *arg)
// {
//     while (1) {
//     }
// }