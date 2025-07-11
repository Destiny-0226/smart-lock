#pragma once
#include "utils.h"

void LED_RMT_Init(void);
void LED_Keyoard_Light(uint8_t led_num, uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness, uint16_t delay_ms);
void LED_Background_Light(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);