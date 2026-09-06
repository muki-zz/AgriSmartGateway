#include "llcc68_hal.h"
#include "llcc68_stm32_port.h"
#include "rtthread.h"

static llcc68_hal_status_t llcc68_wait_on_busy(void)
{
    const uint32_t started_at = rt_tick_get();

    while (HAL_GPIO_ReadPin(LLCC68_BUSY_GPIO_Port, LLCC68_BUSY_Pin) == GPIO_PIN_SET)
    {
        if ((rt_tick_get() - started_at) >= LLCC68_BUSY_TIMEOUT_MS)
        {
            return LLCC68_HAL_STATUS_ERROR;
        }
    }
    return LLCC68_HAL_STATUS_OK;
}


llcc68_hal_status_t llcc68_hal_write(const void* context, const uint8_t* command,
                                     const uint16_t command_length, const uint8_t* data,
                                     const uint16_t data_length)
{
    (void)context;
    if (llcc68_wait_on_busy() != LLCC68_HAL_STATUS_OK)
        return LLCC68_HAL_STATUS_ERROR;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)command, command_length,
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
        goto error;

    if (data_length)
        if(HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)data, data_length,
                          LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
            goto error;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return llcc68_wait_on_busy();

error:
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return LLCC68_HAL_STATUS_ERROR;
}


llcc68_hal_status_t llcc68_hal_read(const void* context, const uint8_t* command,
                                    const uint16_t command_length, uint8_t* data,
                                    const uint16_t data_length)
{
    (void)context;
    if (llcc68_wait_on_busy() != LLCC68_HAL_STATUS_OK)
        return LLCC68_HAL_STATUS_ERROR;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)command, command_length,
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
        goto error;

    if ((data_length != 0U) &&
        (HAL_SPI_Receive(&LLCC68_SPI_HANDLE, data, data_length,
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK))
        goto error;

    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return llcc68_wait_on_busy();

error:
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return LLCC68_HAL_STATUS_ERROR;
}


llcc68_hal_status_t llcc68_hal_reset(const void* context)
{
    (void)context;
    HAL_GPIO_WritePin(LLCC68_RESET_GPIO_Port, LLCC68_RESET_Pin, GPIO_PIN_RESET);
    rt_thread_mdelay(10);
    HAL_GPIO_WritePin(LLCC68_RESET_GPIO_Port, LLCC68_RESET_Pin, GPIO_PIN_SET);
    rt_thread_mdelay(10);
    return llcc68_wait_on_busy();
}


llcc68_hal_status_t llcc68_hal_wakeup(const void* context)
{
    const uint8_t get_status[2] = { 0xC0U, LLCC68_NOP };
    (void)context;


    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_RESET);
    if (HAL_SPI_Transmit(&LLCC68_SPI_HANDLE, (uint8_t*)get_status, sizeof(get_status),
                         LLCC68_SPI_TIMEOUT_MS) != HAL_OK)
    {
        HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
        return LLCC68_HAL_STATUS_ERROR;
    }
    HAL_GPIO_WritePin(LLCC68_NSS_GPIO_Port, LLCC68_NSS_Pin, GPIO_PIN_SET);
    return llcc68_wait_on_busy();
}
