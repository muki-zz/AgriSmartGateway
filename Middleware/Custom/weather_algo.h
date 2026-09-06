#ifndef _WEATHER_ALGO_H
#define _WEATHER_ALGO_H

#include <stdint.h>

typedef struct{
    float base_hum_low;
    float base_hum_high;
    uint8_t max_irr_sec;
    uint8_t min_gap_sec;
}humi_ctrl_t;

extern humi_ctrl_t g_humi_base;
extern humi_ctrl_t g_humi_algo;

/**
 * @brief:决策
 * @details:执行灌溉状态机决策，得到输出动作：VALVE_OPEN / VALVE_CLOSE
 * @retval:1灌溉，0关闭灌溉
 */
uint8_t make_decision(float *humi,float *temp);

#endif