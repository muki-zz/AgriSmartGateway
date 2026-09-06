#ifndef _APP_DATA_CENTER
#define _APP_DATA_CENTER

#include <stdint.h>
#include "rtthread.h"

//传感器数据
typedef struct{
    uint16_t co2_value;
    float temp;
    float humi;
    uint16_t soil_humi;
    uint16_t lux;
    uint8_t  humi_ctrl;//0关，1开
}g_sensor_data_t;


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


extern rt_mq_t          g_ctrl_device_mq;
extern g_sensor_data_t  g_sensor_data;
extern rt_mutex_t       g_sensor_mutex;
extern rt_sem_t         g_mqtt_sem;
#endif