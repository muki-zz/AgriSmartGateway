#ifndef _APP_MQTT_SERVICE_H
#define _APP_MQTT_SERVICE_H

typedef enum{
    MQTT_TYPE_CO2_PUBLISH,
    MQTT_TYPE_DHT22_PUBLISH,
    MQTT_TYPE_LIGHT_PUBLISH,
    MQTT_TYPE_SOIL_PUBLISH,
}mqtt_task_type_t;

//消息结构体
typedef struct
{
    mqtt_task_type_t task;
}mqtt_task_msg_t;

/**
 * @brief:mqtt_service线程初始化
 */
void app_mqtt_service_init();

#endif