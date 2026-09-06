/* 主机示例：无需握手，持续被动接收数据。 */
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "llcc68_app.h"
#include "llcc68_stm32_port.h"

#define NODE_MASTER       0x01U
#define NODE_BROADCAST    0xFFU
#define CMD_SENSOR_DATA   0x20U

/* DIO1 中断只置位，SPI 中断处理留在主循环执行。 */
static volatile uint8_t lora_dio1_pending;

/** 校验地址和帧长度，然后按命令处理从机上报的数据。 */
static void master_radio_rx(const uint8_t* data, const uint8_t length)
{
    if ((length < 4U) ||
        ((data[0] != NODE_MASTER) && (data[0] != NODE_BROADCAST)) ||
        (data[3] != (uint8_t)(length - 4U)))
        return;

    /* 帧格式：[目标地址][源地址][命令][载荷长度][载荷]。 */
    const uint8_t source = data[1];
    const uint8_t command = data[2];
    const uint8_t* payload = &data[4];
    const uint8_t payload_length = data[3];

    if (command == CMD_SENSOR_DATA)
    {
        /* 在此处理 payload[0..payload_length-1] 中的数据。 */
        (void)source;
        (void)payload;
        (void)payload_length;
    }
}

/** 初始化 MCU 和射频芯片，然后永久保持被动接收。 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    llcc68_app_init();
    llcc68_app_set_rx_callback(master_radio_rx);
    llcc68_app_start_receive();

    while (1)
    {
        if (lora_dio1_pending != 0U)
        {
            lora_dio1_pending = 0U;
            llcc68_app_process_irq();
        }
    }
}

/** DIO1 上升沿回调；这里只记录事件，避免在中断内阻塞。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LLCC68_DIO1_Pin)
        lora_dio1_pending = 1U;
}
