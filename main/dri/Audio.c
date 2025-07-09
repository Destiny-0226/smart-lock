#include "Audio.h"
#include "driver/gpio.h"
#include "utils.h"
void Audio_Init(void)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << 9);
    gpio_config(&io_conf);

    DelayMs(100);
    gpio_set_level(9, 1);
    DelayMs(10);
}

void Audio_Play(uint8_t data)
{
    // 根据文档，发送数据之前先拉低引脚10ms
    gpio_set_level(9, 0);
    DelayMs(10);

    for (int i = 0; i < 8; i++)
    {
        if (data & 0x01)
        {
            gpio_set_level(9, 1);
            DelayUs(600);
            gpio_set_level(9, 0);
            DelayUs(200);
        }
        else
        {
            gpio_set_level(9, 1);
            DelayUs(200);
            gpio_set_level(9, 0);
            DelayUs(600);
        }

        data >>= 1;
    }

    gpio_set_level(9, 1);
    DelayMs(10);
}