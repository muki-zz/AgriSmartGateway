/* 请将相关代码复制到 Core/Src/main.c，不要把本文件作为第二个 main() 加入编译。 */
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "llcc68_app.h"
#include "llcc68_stm32_port.h"

/* DIO1 事件标志，避免在外部中断回调中直接执行 SPI。 */
static volatile uint8_t lora_dio1_pending;

/** 通用收发测试主函数：持续接收，并每 5 秒发送一次测试字符串。 */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();

    llcc68_app_init();
    llcc68_app_start_receive();

    uint32_t last_tx_ms = HAL_GetTick();
    for (;;)
    {
        if (lora_dio1_pending != 0U)
        {
            lora_dio1_pending = 0U;
            llcc68_app_process_irq();
        }
        if ((HAL_GetTick() - last_tx_ms) >= 5000U)
        {
            static const uint8_t message[] = "Hello LLCC68";
            llcc68_app_send(message, sizeof(message) - 1U);
            last_tx_ms = HAL_GetTick();
        }
    }
}

/** 记录 LLCC68 DIO1 上升沿事件。 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LLCC68_DIO1_Pin)
        lora_dio1_pending = 1U;
}
