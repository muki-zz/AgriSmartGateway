#ifndef _APP_DATAHUB_H
#define _APP_DATAHUB_H

#include <stdint.h>
#include "rtthread.h"

typedef struct{
    uint16_t co2_value;        // co2浓度 (ppm)
    float temperature;         // 温度 (°C)
    float humidity;            // 湿度 (%RH)
    uint16_t lux;              // 光照 (lux)
    uint16_t soil_humidity;    // 土壤湿度
} EnvironmentData_t;


typedef enum{
    LED_CTRL_EVT,
    WATER_CTRL_EVT,
}ctrl_device_evt_t;

typedef struct{
    ctrl_device_evt_t evt;
    union data_u
    {
        uint8_t led_ctrl;
        uint32_t water_ctrl[2];
    } data; 
}ctrl_device_task_msg_t;


extern rt_mq_t            g_ctrl_device_mq;
extern EnvironmentData_t  g_sensor_data;
extern rt_mutex_t         g_sensor_mutex;
extern rt_sem_t           g_mqtt_sem;
#endif