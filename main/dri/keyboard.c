#include "keyboard.h"
#include "hal/gpio_types.h"
#include "utils.h"

// "driver/gpio.h" 来自于 esp-idf 的源码
#include "driver/gpio.h"
#include <stdint.h>

// sda引脚号
#define IIC_SDA_PIN 2
// scl引脚号
#define IIC_SCL_PIN 1

// 将SDA设置为输出方向
#define IIC_SDA_OUT gpio_set_direction(IIC_SDA_PIN, GPIO_MODE_OUTPUT)
// 将SDA设置为输入方向
#define IIC_SDA_IN gpio_set_direction(IIC_SDA_PIN, GPIO_MODE_INPUT)

// SDA输出高电平
#define IIC_SDA_HIGH gpio_set_level(IIC_SDA_PIN, 1)
// SDA输出低电平
#define IIC_SDA_LOW gpio_set_level(IIC_SDA_PIN, 0)
// SCL输出高电平
#define IIC_SCL_HIGH gpio_set_level(IIC_SCL_PIN, 1)
// SCL输出低电平
#define IIC_SCL_LOW gpio_set_level(IIC_SCL_PIN, 0)

// 读取SDA的电平
#define IIC_SDA_READ gpio_get_level(IIC_SDA_PIN)

#define IIC_SDA_SCL_PIN_SEL ((1ULL << IIC_SDA_PIN) | (1ULL << IIC_SCL_PIN))

// IIC 启动信号函数
void IIC_Start(void)
{
}

// IIC 停止信号
void IIC_Stop(void)
{
}

// 发送一个字节并读取ACK
uint8_t IIC_SendByteAndGetNACK(uint8_t byte)
{
}

// 下发应答命令
void IIC_Respond(uint8_t ack)
{
}

// 读取一个字节
uint8_t IIC_ReadByte(void)
{
}

uint8_t IIC_SimpleRead(uint16_t *result)
{
}

void KEYBOARD_Init(void)
{
}

uint8_t KEYBOARD_ReadKey(void)
{
}