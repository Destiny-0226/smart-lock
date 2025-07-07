#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"

// 延时函数
#define DelayMs(time) vTaskDelay(time / portTICK_PERIOD_MS)