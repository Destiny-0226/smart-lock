#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"
#include "unistd.h"
#include <stdint.h>
#include "driver/gpio.h"

// 延时函数

// 微秒级别的延时
#define DelayUs(time) usleep(time)
// 毫秒级别的延时
#define DelayMs(time) time % 10 ? vTaskDelay(pdMS_TO_TICKS(time)) : DelayUs(time * 1000)