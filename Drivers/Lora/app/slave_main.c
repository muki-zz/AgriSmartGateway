/* 从机示例：无需握手，周期性主动发送数据。 */
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "llcc68_app.h"
#include "llcc68_stm32_port.h"

#define NODE_MASTER       0x01U
#define NODE_SLAVE        0x02U /* 每个从机必须分配唯一地址。 */
#define CMD_SENSOR_DATA   0x20U
#define REPORT_PERIOD_MS  1000U

/* DIO1 中断事件标志，由 EXTI 写入、主循环读取。 */
static volatile uint8_t lora_dio1_pending;

/** 将一个 16 位采样值封装成应用帧并立即发射。 */
static void slave_send_sensor_data(const uint16_t sample)
{
    const uint8_t frame[6] = {
        NODE_MASTER, NODE_SLAVE, CMD_SENSOR_DATA, 2U,
        (uint8_t)(sample >> 8), (uint8_t)sample,
    };
    llcc68_app_send(frame, sizeof(frame));
}

/** 初始化 MCU 和射频芯片，然后按固定周期主动上报数据。 */
int main(void)
{
    uint32_t last_report_ms;
    uint16_t sample = 0U;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    llcc68_app_init();
    llcc68_app_start_receive();
    last_report_ms = HAL_GetTick();

    while (1)
    {
        if (lora_dio1_pending != 0U)
        {
            lora_dio1_pending = 0U;
            llcc68_app_process_irq();
        }

        if ((HAL_GetTick() - last_report_ms) >= REPORT_PERIOD_MS)
        {
            /* 实际使用时，将 sample++ 替换为传感器读数。 */
            slave_send_sensor_data(sample++);
            last_report_ms = HAL_GetTick();
        }
    }
}

/** DIO1 上升沿回调；实际射频中断处理在主循环完成。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LLCC68_DIO1_Pin)
        lora_dio1_pending = 1U;
}
