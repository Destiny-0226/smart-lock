#include "fingerprint.h"
#include "driver/uart.h"

#define FINGER_UART_TX_PIN GPIO_NUM_21
#define FINGER_UART_RX_PIN GPIO_NUM_20

#define RX_BUF_SIZE        2048

static void get_chip_sn(void);

/// 初始化指纹模块
void Fingerprint_Init(void)
{
    // UART 配置
    // 定义一个UART配置结构体变量uart_config，用于设置UART的通信参数
    uart_config_t uart_config = {
        // 设置波特率为57600，这是UART通信的数据传输速率
        .baud_rate = 57600,
        // 设置数据位为8位，这是UART通信中数据帧的数据部分的位数
        .data_bits = UART_DATA_8_BITS,
        // 禁用奇偶校验，UART通信中不使用奇偶校验位来检测传输错误
        .parity = UART_PARITY_DISABLE,
        // 设置停止位为1位，这是UART通信中帧的结束信号
        .stop_bits = UART_STOP_BITS_1,
        // 禁用硬件流控，UART通信中不使用硬件握手信号来控制数据传输
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // 设置UART的时钟源为默认值，这是UART通信中用于生成波特率的时钟源
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    // 为串口1分配tx和rx引脚
    uart_set_pin(UART_NUM_1, FINGER_UART_TX_PIN, FINGER_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // 中断
    gpio_config_t io_conf = {0};
    io_conf.intr_type     = GPIO_INTR_POSEDGE;
    io_conf.mode          = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask  = (1ULL << FINGER_TOUCH_INT_PIN);
    gpio_config(&io_conf);

    DelayMs(150);
    get_chip_sn();
    while (Finger_Sleep()) {
        DelayMs(10);
    }

    printf("指纹模块初始化成功。\r\n");

    while (Finger_GetImage()) {
        DelayMs(10);
    }
    while (Finger_GenChar(1)) {
        DelayMs(10);
    }
}

/// 获取指纹芯片的序列号
static void get_chip_sn(void)
{
    uint8_t cmd[13] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x04,             // 包长度
        0x34,                   // 指令码
        0x00,                   // 参数
        0x00, 0x39              // 校验和
    };

    uart_write_bytes(UART_NUM_1, cmd, 13);

    uint8_t recv_data[64];
    int length = uart_read_bytes(UART_NUM_1, recv_data, 1024, 100 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0) {
            printf("指纹模块序列号：%.32s\r\n", &recv_data[10]);
        } else if (recv_data[9] == 0x01) {
            printf("指纹模块序列号：收包错误\r\n");
        }
    }
}

// 3.3.1.19 休眠指令PS_Sleep
// 包头 设备地址 包标识 包长度 指令码 校验和
// 2 bytes 4bytes 1 byte 2bytes 1 byte 2bytes
// 0xEF01 xxxx 01H 0003H 33H 0037H
uint8_t Finger_Sleep(void)
{
    uint8_t cmd[12] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x03,             // 包长度
        0x33,                   // 指令码
        0x00, 0x37              // 校验和
    };

    uart_write_bytes(UART_NUM_1, cmd, 12);

    uint8_t recv_data[64];
    int length = uart_read_bytes(UART_NUM_1, recv_data, 1024, 100 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0) {
            printf("休眠成功\r\n");
            return 0;
        } else if (recv_data[9] == 0x01) {
            printf("休眠失败：收包错误\r\n");
        }
    }

    return 1;
}

/// 获取指纹图像
// 验证用获取图像PS_GetImage
//  功能说明： 验证指纹时，探测手指，探测到后录入指纹图像存于图像缓冲区。返回确认码
// 表示：录入成功、无手指等。
//  输入参数： none
//  返回参数： 确认字
//  指令代码： 01H
//  指令包格式：
// 表 3-5 录入图像指令包格式
//  包头    设备地址   包标识     包长度       指令码     校验和
// 2bytes   4bytes    1byte   2 bytes     1 byte     2 bytes
// 0xEF 01   xxxx      01H      00 03H        01H      00 05H

// 应答包格式：
// 表 3-6 录入图像指令应答包格式
// 包头      设备地址      包标识     包长度      确认码    校验和
// 2bytes    4bytes      1 byte      2 bytes    1 byte    2 bytes
// 0xEF01     xxxx       07H         0003H       xxH       sum

uint8_t Finger_GetImage(void)
{
    uint8_t cmd[12] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x03,             // 包长度
        0x01,                   // 指令码
        0x00, 0x05              // 校验和
    };

    uart_write_bytes(UART_NUM_1, cmd, 12);

    uint8_t recv_data[64];
    int length = uart_read_bytes(UART_NUM_1, recv_data, 1024, 100 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0) {
            printf("指纹获取成功\r\n");
            return 0;
        } else if (recv_data[9] == 0x01) {
            printf("指纹获取：收包错误\r\n");
        } else if (recv_data[9] == 0x02) {
            printf("指纹获取：传感器上无手指\r\n");
        }
    }
    return 1;
}

/// 生成指纹特征
/**
 * @brief  将图像缓冲区中的原始图像生成指纹特征文件存于模板缓冲区。
 *
 * @param  BufferID 缓冲区编号（正整数），指定此次提取的特征存放位置。
 *
 * @return uint8_t 返回确认码：
 *         - 0: 生成特征成功
 *         - 1: 失败
 **/
uint8_t Finger_GenChar(uint8_t BufferID)
{
    uint8_t cmd[13] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x04,             // 包长度（注意这里是 0x04）
        0x02,                   // 指令码：PS_GenChar
        BufferID,               // 缓冲区编号（用户输入参数）
        0x00, 0x00              // 校验和（高位 + 低位，可根据实际指令计算）
    };

    uint16_t checksum = cmd[6] + cmd[7] + cmd[8] + cmd[9] + cmd[10];
    cmd[11]           = checksum >> 8;
    cmd[12]           = checksum & 0xFF;

    uart_write_bytes(UART_NUM_1, cmd, 13);

    uint8_t recv_data[64];
    int length = uart_read_bytes(UART_NUM_1, recv_data, 1024, 500 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0x00) {
            printf("指纹特征生成：成功\r\n");
            return 0;
        } else if (recv_data[9] == 0x01) {
            printf("指纹特征生成：收包错误\r\n");
        } else if (recv_data[9] == 0x06) {
            printf("指纹特征生成：指纹图像太乱而生不成特征\r\n");
        } else if (recv_data[9] == 0x07) {
            printf("指纹特征生成：指纹图像正常,但特征点太少而生不成特征\r\n");
        } else if (recv_data[9] == 0x08) {
            printf("指纹特征生成：当前指纹特征与之前特征之间无关联\r\n");
        } else if (recv_data[9] == 0x0a) {
            printf("指纹特征生成：合并失败\r\n");
        } else if (recv_data[9] == 0x15) {
            printf("指纹特征生成：图像缓冲区内没有有效原始图而生不成图像\r\n");
        } else if (recv_data[9] == 0x28) {
            printf("指纹特征生成：当前指纹特征与之前特征之间有关联\r\n");
        }
    }
    return 1;
}

/// 搜索指纹
int search(void);

/// 读取指纹芯片配置参数
void read_sys_params(void);