// `#pragma once`等价于以下代码
// #ifndef KEYBOARD_H
// #define KEYBOARD_H
// ...
// #endif
#pragma once

#include "utils.h"

// 0 表示第 0 个引脚
#define KEYBOARD_INT_PIN GPIO_NUM_0

void Keyboard_Init(void);
uint8_t Keyboard_ReadKey(void);