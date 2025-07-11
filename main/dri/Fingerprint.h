#pragma once
#include "utils.h"

#define FINGER_TOUCH_INT_PIN GPIO_NUM_10

/// 初始化指纹模块
void Fingerprint_Init(void);

int8_t Finger_Sleep(void);

uint8_t Finger_Enroll(void);

uint8_t Finger_Identifiy(void);