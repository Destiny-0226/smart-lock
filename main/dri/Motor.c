#include "motor.h"
#include "driver/gpio.h"
#include "utils.h"

void Motor_Init(void)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << 4) | (1ULL << 5);
    gpio_config(&io_conf);

    // 将4和5引脚拉高
    gpio_set_level(4, 1);
    gpio_set_level(5, 1);
    DelayMs(10);
}
void Motor_OpenLock(void)
{
    // 正转2s
    gpio_set_level(4, 1);
    gpio_set_level(5, 0);
    DelayMs(2000);

    // 停止2s
    gpio_set_level(4, 1);
    gpio_set_level(5, 1);
    DelayMs(2000);

    // 反转2s
    gpio_set_level(4, 0);
    gpio_set_level(5, 1);
    DelayMs(2000);

    // 停止
    gpio_set_level(4, 1);
    gpio_set_level(5, 1);
    DelayMs(10);
}