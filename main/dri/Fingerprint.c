#include "fingerprint.h"
#include "driver/uart.h"

#define FINGER_UART_TX_PIN GPIO_NUM_21
#define FINGER_UART_RX_PIN GPIO_NUM_20

#define RX_BUF_SIZE        2048

// 有效指纹数量
static uint8_t finger_num = 0;

static int8_t Finger_GetSN(void);
static int8_t Finger_GetImage(void);
static int8_t Finger_GenChar(uint8_t BufferID);
static int8_t Finger_RegModel(void);
static int8_t Finger_StoreChar(uint8_t PageID);
static int8_t Finger_SetSecurityLevel(uint8_t security_level);
static int8_t Finger_ValidTemplateNum(uint8_t *valid_template_num);

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
    // 获取SN
    Finger_GetSN();
    // 设置安全等级为0
    Finger_SetSecurityLevel(0);
    // 获取有效模板数量
    Finger_ValidTemplateNum(&finger_num);
    while (Finger_Sleep()) {
        DelayMs(10);
    }

    printf("指纹模块初始化成功。\r\n");
}

/// 获取指纹芯片的序列号
static int8_t Finger_GetSN(void)
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
            printf("指纹模块序列号:%.32s\r\n", &recv_data[10]);
        } else if (recv_data[9] == 0x01) {
            printf("指纹模块序列号:收包错误\r\n");
        }
        return recv_data[9];
    }

    printf("指纹模块序列号:无响应\r\n");
    return -1;
}

// 3.3.1.19 休眠指令PS_Sleep
// 包头 设备地址 包标识 包长度 指令码 校验和
// 2 bytes 4bytes 1 byte 2bytes 1 byte 2bytes
// 0xEF01 xxxx 01H 0003H 33H 0037H
int8_t Finger_Sleep(void)
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
        } else if (recv_data[9] == 0x01) {
            printf("休眠失败:收包错误\r\n");
        }
        return recv_data[9];
    }

    printf("休眠失败:无响应\r\n");
    return -1;
}

/**
 * @brief 获取指纹图像
 *
 * 本函数通过UART向指纹模块发送命令以获取指纹图像，并根据返回数据判断操作结果。
 *
 * @return static int8_t 返回结果代码：
 *         - 0x00: 指纹获取成功
 *         - 0x01: 收包错误
 *         - 0x02: 传感器上无手指
 *         - -1: 无响应
 */
static int8_t Finger_GetImage(void)
{

    // 表 3-5 录入图像指令包格式
    //  包头    设备地址   包标识     包长度       指令码     校验和
    // 2bytes   4bytes    1byte   2 bytes     1 byte     2 bytes
    // 0xEF 01   xxxx      01H      00 03H        01H      00 05H

    // 构造发送给指纹模块的命令包
    uint8_t cmd[12] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x03,             // 包长度
        0x01,                   // 指令码
        0x00, 0x05              // 校验和
    };

    // 通过UART发送命令包
    uart_write_bytes(UART_NUM_1, cmd, 12);

    // 接收指纹模块返回的数据
    uint8_t recv_data[64];
    int length = uart_read_bytes(UART_NUM_1, recv_data, 1024, 100 / portTICK_PERIOD_MS);

    // 根据接收的数据长度和内容判断操作结果
    if (length > 0) {
        if (recv_data[9] == 0) {
            printf("指纹获取成功\r\n");
        } else if (recv_data[9] == 0x01) {
            printf("指纹获取:收包错误\r\n");
        } else if (recv_data[9] == 0x02) {
            printf("指纹获取:传感器上无手指\r\n");
        }
        return recv_data[9];
    }
    printf("指纹获取:无响应\r\n");
    return -1;
}

/**
 * @brief  生成指纹特征 将图像缓冲区中的原始图像生成指纹特征文件存于模板缓冲区。
 *
 * @param  BufferID 缓冲区编号（正整数），指定此次提取的特征存放位置。
 *
 * @return uint8_t 返回确认码:
 *         - 0: 生成特征成功
 *         - other: 失败
 **/
static int8_t Finger_GenChar(uint8_t BufferID)
{
    uint8_t cmd[13] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x04,             // 包长度（注意这里是 0x04）
        0x02,                   // 指令码:PS_GenChar
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
            printf("指纹特征生成:成功\r\n");
        } else if (recv_data[9] == 0x01) {
            printf("指纹特征生成:收包错误\r\n");
        } else if (recv_data[9] == 0x06) {
            printf("指纹特征生成:指纹图像太乱而生不成特征\r\n");
        } else if (recv_data[9] == 0x07) {
            printf("指纹特征生成:指纹图像正常,但特征点太少而生不成特征\r\n");
        } else if (recv_data[9] == 0x08) {
            printf("指纹特征生成:当前指纹特征与之前特征之间无关联\r\n");
        } else if (recv_data[9] == 0x0a) {
            printf("指纹特征生成:合并失败\r\n");
        } else if (recv_data[9] == 0x15) {
            printf("指纹特征生成:图像缓冲区内没有有效原始图而生不成图像\r\n");
        } else if (recv_data[9] == 0x28) {
            printf("指纹特征生成:当前指纹特征与之前特征之间有关联\r\n");
        }
        return recv_data[9];
    }
    printf("指纹特征生成:无响应\r\n");
    return -1;
}

// 3.3.1.5 合并特征（生成模板）PS_RegModel
// 包头 设备地址 包标识 包长度 指令码 校验和
// 2 bytes 4bytes 1 byte 2 bytes 1 byte 2 bytes
// 0xEF01 xxxx 01H 0003H 05H 0009H
static int8_t Finger_RegModel(void)
{
    uint8_t cmd[12] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x03,             // 包长度
        0x05,                   // 指令码
        0x00, 0x09              // 校验和
    };

    uart_write_bytes(UART_NUM_1, cmd, 12);

    uint8_t recv_data[64];
    // 合并特征需要的时间也比较长，给500ms的延时
    int length =
        uart_read_bytes(UART_NUM_1, recv_data, 1024, 500 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0) {
            printf("合并特征:成功\r\n");
        } else if (recv_data[9] == 1) {
            printf("合并特征:收包错误\r\n");
        } else if (recv_data[9] == 0x0a) {
            printf("合并特征:合并失败\r\n");
        }
        return recv_data[9];
    }

    printf("合并特征:无响应\r\n");
    return -1;
}

/**
 * @brief 储存指纹模板字符
 *
 * 本函数通过UART接口向指纹模块发送命令，以储存指纹模板字符到指定的PageID位置。
 * 它首先构造一个命令包，计算校验和，然后发送给指纹模块，并等待响应。
 * 根据响应内容判断储存操作是否成功，或遇到何种错误。
 *
 * @param PageID 指纹模板储存的位置ID
 * @return int8_t 返回储存结果的错误代码，0表示成功，其他值表示不同错误。
 *
 */
static int8_t Finger_StoreChar(uint8_t PageID)
{
    // 3.3.1.6 储存模板PS_StoreChar
    // 包头 设备地址 包标识 包长度 指令码 缓冲区号 位置号 校验和
    // 2 bytes 4bytes 1 byte 2 bytes 1 byte 1 byte 2 bytes 2 bytes
    // 0xEF01 xxxx 01H 0006H 06H BufferID PageID sum

    uint8_t cmd[15] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x06,             // 包长度
        0x06,                   // 指令码
        0x01,                   // 缓冲区号，默认为1
        0x00, PageID,           // PageID
        0x00, 0x00              // 校验和
    };

    uint16_t checksum = 0;
    for (size_t i = 6; i < 13; i++) {
        checksum += cmd[i];
    }
    cmd[13] = checksum >> 8;
    cmd[14] = checksum;

    uart_write_bytes(UART_NUM_1, cmd, 15);

    uint8_t recv_data[64];
    // 保存模板需要时间比较长
    int length =
        uart_read_bytes(UART_NUM_1, recv_data, 1024, 500 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0x00) {
            printf("保存模板:成功\r\n");
        } else if (recv_data[9] == 0x01) {
            printf("保存模板:收包错误\r\n");
        } else if (recv_data[9] == 0x0b) {
            printf("保存模板:PageID超出指纹库范围\r\n");
        } else if (recv_data[9] == 0x18) {
            printf("保存模板:写FLASH错误\r\n");
        } else if (recv_data[9] == 0x31) {
            printf("保存模板:加密等级不匹配\r\n");
        }
        return recv_data[9];
    }
    printf("保存模板:无响应\r\n");
    return -1;
}

/**
 *  @brief 设置指纹模块的安全级别为零
 *
 * 本函数通过发送特定指令给指纹模块，以设置其安全级别为零
 * 安全级别影响指纹匹配的严格程度，级别越低，匹配越容易
 *
 * @param uint8_t security_level 安全等级 0~255
 * @return int8_t 返回设置结果的代码
 *         0x00: 设置成功
 *         0x01: 收包错误
 *         0x18: 读写Flash错误
 *         0x1a: 寄存器序号错误
 *         0x1b: 存器设定内容错误
 *         -1: 无响应
 */
static int8_t Finger_SetSecurityLevel(uint8_t security_level)
{
    // 3.3.1.12 写系统寄存器PS_WriteReg
    // 将安全等级设置为0
    // 包头 设备地址 包标识 包长度 指令码 寄存器序号 内容 校验和
    // 2 bytes 4bytes 1 byte 2 bytes 1 byte 1byte 1byte 2 bytes
    // 0xEF01 xxxx 01H 0005H 0eH xxH xxH sum

    // 构造发送给指纹模块的指令包
    uint8_t cmd[14] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x05,             // 包长度
        0x0E,                   // 指令码
        0x07,                   // 寄存器序号
        security_level,         // 内容
        0x00, 0x00              // 校验和
    };

    // 计算校验和
    uint16_t checksum = 0;
    for (int i = 6; i < 12; i++) {
        checksum += cmd[i];
    }
    cmd[12] = checksum >> 8;
    cmd[13] = checksum & 0xFF;

    // 发送指令包到指纹模块
    uart_write_bytes(UART_NUM_1, cmd, 14);

    // 接收指纹模块的响应数据
    uint8_t recv_data[64] = {0};
    int length            = uart_read_bytes(UART_NUM_1, recv_data, 64, 100 / portTICK_PERIOD_MS);

    // 根据响应数据判断设置结果
    if (length > 0) {
        if (recv_data[9] == 0x00) {
            printf("设置安全等级%d:成功\r\n", security_level);
        } else if (recv_data[9] == 0x01) {
            printf("安全等级设置:收包错误\r\n");
        } else if (recv_data[9] == 0x18) {
            printf("安全等级设置:读写Flash错误\r\n");
        } else if (recv_data[9] == 0x1a) {
            printf("安全等级设置:寄存器序号错误\r\n");
        } else if (recv_data[9] == 0x1b) {
            printf("安全等级设置:存器设定内容错误\r\n");
        }
        return recv_data[9];
    }
    printf("安全等级设置:无响应\r\n");
    return -1;
}

/**
 * @brief 获取有效指纹模板数量
 *
 * @param valid_template_num 获取有效指纹模板数量
 * @return int8_t 0x00成功
 *          0x01: 收包失败
 *          -1: 无响应
 */
static int8_t Finger_ValidTemplateNum(uint8_t *valid_template_num)
{
    // 3.3.1.16 读有效模板个数PS_ValidTempleteNum
    // 包头 设备地址 包标识 包长度 指令码 校验和
    // 2 bytes 4bytes 1 byte 2 bytes 1 byte 2 bytes
    // 0xEF01 xxxx 01H 0003H 1dH 0021H

    // 构造发送给指纹识别模块的命令包。
    uint8_t cmd[12] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x03,             // 包长度
        0x1D,                   // 指令码
        0x00, 0x21              // 校验和
    };

    // 通过UART串口发送命令包。
    uart_write_bytes(UART_NUM_1, cmd, 12);

    // 准备接收指纹识别模块的响应数据。
    uint8_t recv_data[64];
    int length =
        uart_read_bytes(UART_NUM_1, recv_data, 1024, 100 / portTICK_PERIOD_MS);

    // 检查接收到的数据长度，以确定响应是否有效。
    if (length > 0) {
        // 根据响应数据的第9字节判断操作结果。
        if (recv_data[9] == 0) {
            // 成功获取有效模板数量，打印数量并返回0x00。
            printf("有效模板个数:%d\r\n", recv_data[11]);
            *valid_template_num = recv_data[11];
        } else if (recv_data[9] == 0x01) {
            // 收包错误，打印错误信息并返回0x01。
            printf("获取有效模板数量:收包错误\r\n");
        }
        return recv_data[9];
    }

    // 如果没有接收到任何数据，打印无响应信息并返回-1。
    printf("获取有效模板数量:无响应\r\n");
    return -1;
}

/**
 * @brief 指纹检索
 *
 * @return int8_t 0: 成功 -1: 失败
 */
int8_t Finger_Search(void)
{
    // 3.3.1.4 搜索指纹PS_Search
    // 包头 设备地址 包标识 包长度 指令码 缓冲区号 参数 参数 校验和
    // 2 bytes 4bytes 1 byte 2 bytes 1 byte 1 byte   2 bytes   2 bytes 2 bytes
    // 0xEF01  xxxx   01H    0008H   04H    BufferID StartPage PageNum sum

    uint8_t cmd[17] = {
        0xEF, 0x01,             // 包头
        0xFF, 0xFF, 0xFF, 0xFF, // 默认设备地址
        0x01,                   // 包标识
        0x00, 0x08,             // 包长度
        0x04,                   // 指令码
        0x01,                   // 缓冲区号
        0x00, 0x00,             // StartPage
        0xFF, 0xFF,             // PageNum
        0x00, 0x00              // 校验和
    };

    uint16_t checksum = 0;
    for (int i = 6; i <= 14; i++) {
        checksum += cmd[i];
    }
    cmd[15] = checksum >> 8;
    cmd[16] = checksum;

    uart_write_bytes(UART_NUM_1, cmd, 17);

    uint8_t recv_data[64];
    // 搜索时间长一点
    int length =
        uart_read_bytes(UART_NUM_1, recv_data, 1024, 500 / portTICK_PERIOD_MS);
    if (length > 0) {
        if (recv_data[9] == 0) {
            printf("指纹检索:成功\r\n");
        } else if (recv_data[9] == 0x01) {
            printf("指纹检索:收包失败\r\n");
        } else if (recv_data[9] == 0x09) {
            printf("指纹检索:未找到指纹\r\n");
        } else if (recv_data[9] == 0x17) {
            printf("指纹检索:残留指纹或两次采集之间手指没有移动过\r\n");
        } else if (recv_data[9] == 0x31) {
            printf("指纹检索:功能与加密等级不匹配\r\n");
        }
        return recv_data[9];
    }

    printf("指纹检索:无响应\r\n");
    return -1;
}

/**
 * @brief 执行指纹录入流程
 *
 * 本函数负责指导用户完成指纹录入过程它通过多次获取指纹图像并生成特征值，
 * 最后注册模型并存储特征值整个过程包括多次提示用户放置和拿开手指，以确保
 * 获取不同角度的指纹图像
 *
 * @return
 *  0: 成功录入指纹
 *  1: 录入过程中发生错误
 */
uint8_t Finger_Enroll(void)
{
    static uint32_t finger_enroll_run_time = 0;

    printf("进入指纹录入模式\r\n");
    // 按下指纹的次数
    int N = 4;
    int n = 1;

    // 获取指纹图像并生成特征值的循环过程
SendGetImageCmd:

    // 超时检测
    if (finger_enroll_run_time == 0) {
        finger_enroll_run_time = xTaskGetTickCount();
    } else if (xTaskGetTickCount() - finger_enroll_run_time > pdMS_TO_TICKS(5000)) {
        printf("指纹录入超时,退出指纹录入\r\n");
        finger_enroll_run_time = 0;
        return 1;
    }
    // 指示用户放置手指并获取图像如果获取失败，则重新尝试
    if (Finger_GetImage()) goto SendGetImageCmd;
    // 生成特征值如果失败，则重新尝试获取图像
    if (Finger_GenChar(n)) goto SendGetImageCmd;
    // 提示用户拿开手指
    printf("请拿开手指\r\n");
    DelayMs(500);
    // 提示用户再次放置手指
    printf("请再次放置手指\r\n");
    // 如果尚未达到预定的指纹数量，则继续循环
    if (n < N) {
        // 如果录制成功了 更新超时时间
        finger_enroll_run_time = xTaskGetTickCount();
        n++;
        goto SendGetImageCmd;
    }
    // 注册指纹模型如果失败，则跳转到错误处理
    if (Finger_RegModel()) goto ERROR;
    // 获取当前有效的模板数量
    if (Finger_ValidTemplateNum(&finger_num)) goto ERROR;
    // 存储特征值如果失败，则跳转到错误处理
    if (Finger_StoreChar(finger_num + 1)) goto ERROR;
    // 获取当前有效的模板数量
    if (Finger_ValidTemplateNum(&finger_num)) goto ERROR;
    // 成功完成指纹录入
    return 0;

ERROR:
    printf("指纹录入失败\r\n");
    // 录入过程中发生错误
    return 1;
}

uint8_t Finger_Identifiy(void)
{
    if (Finger_GetImage()) return 1;
    if (Finger_GenChar(1)) return 1;
    if (Finger_Search()) return 1;
    return 0;
}
