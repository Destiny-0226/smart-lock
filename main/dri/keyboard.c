#include "keyboard.h"
// #include "hal/gpio_types.h"
#include "utils.h"

// sda引脚号
#define IIC_SDA_PIN GPIO_NUM_2
// scl引脚号
#define IIC_SCL_PIN GPIO_NUM_1

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
#define IIC_SDA_READ        gpio_get_level(IIC_SDA_PIN)

#define IIC_SDA_SCL_PIN_SEL (1ULL << IIC_SDA_PIN) | (1ULL << IIC_SCL_PIN)

// IIC 启动信号函数
void IIC_Start(void)
{
    IIC_SDA_OUT;
    IIC_SDA_HIGH;
    IIC_SCL_HIGH;
    DelayMs(1);
    IIC_SDA_LOW;
    DelayMs(1);
    IIC_SCL_LOW;
    DelayMs(1);
}

// IIC 停止信号
void IIC_Stop(void)
{
    IIC_SCL_LOW;
    IIC_SDA_OUT;
    IIC_SDA_LOW;
    DelayMs(1);
    IIC_SCL_HIGH;
    DelayMs(1);
    IIC_SDA_HIGH;
}

// 发送一个字节并读取ACK
uint8_t IIC_SendByteAndGetNACK(uint8_t byte)
{
    uint8_t i = 0;
    IIC_SDA_OUT;
    for (i = 0; i < 8; i++) {
        IIC_SCL_LOW;
        DelayMs(1);
        if ((byte >> 7) & 0x01) {
            IIC_SDA_HIGH;
        } else {
            IIC_SDA_LOW;
        }
        DelayMs(1);
        IIC_SCL_HIGH;
        DelayMs(1);
        byte <<= 1;
    }
    IIC_SCL_LOW;
    DelayMs(3);
    IIC_SDA_IN;
    DelayMs(3);
    IIC_SCL_HIGH;
    DelayMs(1);
    i = 250;
    while (i--) {
        if (!IIC_SDA_READ) {
            IIC_SCL_LOW;
            return 0;
        }
    }
    IIC_SCL_LOW;
    return 1;
}

// 下发应答命令
void IIC_Respond(uint8_t ack)
{
    IIC_SDA_OUT;
    IIC_SDA_LOW;
    IIC_SCL_LOW;
    if (ack) {
        IIC_SDA_HIGH;
    } else {
        IIC_SDA_LOW;
    }
    DelayMs(1);
    IIC_SCL_HIGH;
    DelayMs(1);
    IIC_SCL_LOW;
}

// 读取一个字节
uint8_t IIC_ReadByte(void)
{
    uint8_t i      = 0;
    uint8_t buffer = 0;
    IIC_SDA_IN;
    IIC_SCL_LOW;
    for (i = 0; i < 8; i++) {
        DelayMs(1);
        IIC_SCL_HIGH;
        buffer = (buffer << 1) | IIC_SDA_READ;
        DelayMs(1);
        IIC_SCL_LOW;
    }

    return buffer;
}

uint8_t IIC_SimpleRead(uint16_t *result)
{
    uint8_t buf1 = 0;
    uint8_t buf2 = 0;
    IIC_Start();
    if (IIC_SendByteAndGetNACK((0x42 << 1) | 0x01)) {
        IIC_Stop();
        return 1;
    }

    buf1 = IIC_ReadByte();
    IIC_Respond(0);
    buf2 = IIC_ReadByte();
    IIC_Respond(1);
    IIC_Stop();
    *result = ((uint16_t)buf1 << 8) | buf2;
    return 0;
}

void Keyboard_Init(void)
{
    // 初始化sda和scl引脚
    gpio_config_t io_conf = {};
    // 设置为输出模式
    io_conf.mode = GPIO_MODE_OUTPUT;
    // 禁用中断
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // 设置要使用的引脚号
    io_conf.pin_bit_mask = IIC_SDA_SCL_PIN_SEL;
    gpio_config(&io_conf);

    // 初始化中断引脚
    io_conf.mode      = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_POSEDGE; // 上升沿中断
    // 无论按下哪一个按键，KEYBOARD_INT都会被拉高
    io_conf.pin_bit_mask = (1ULL << KEYBOARD_INT);
    gpio_config(&io_conf);

    // 初始化时间300ms
    DelayMs(300);
}

uint8_t Keyboard_ReadKey(void)
{
    uint16_t result = 0;
    IIC_SimpleRead(&result);
    if (result == 0x8000)
        return 0;
    if (result == 0x4000)
        return 1;
    if (result == 0x2000)
        return 2;
    if (result == 0x1000)
        return 3;
    if (result == 0x0100)
        return 4;
    if (result == 0x0400)
        return 5;
    if (result == 0x0200)
        return 6;
    if (result == 0x0800)
        return 7;
    if (result == 0x0040)
        return 8;
    if (result == 0x0020)
        return 9;
    if (result == 0x0010)
        return 10; // `#`
    if (result == 0x0080)
        return 11; // `M`
    return 0xFF;
}