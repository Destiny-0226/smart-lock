#pragma once
#include "utils.h"

#define FINGER_TOUCH_INT_PIN GPIO_NUM_10

/// 初始化指纹模块
void Fingerprint_Init(void);

uint8_t Finger_Sleep(void);

/// 获取指纹图像
uint8_t Finger_GetImage(void);

/// 获取指纹特征
uint8_t Finger_GenChar(uint8_t BufferID);

/// 搜索指纹
int search(void);

/// 读取指纹芯片配置参数
void read_sys_params(void);