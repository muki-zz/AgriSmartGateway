#include "app_mqtt.h"
#include "app_config.h"
#include "esp8266_mqtt.h"
#include "rtthread.h"
#include "app_datahub.h"
#include <string.h>

#define PUBLISH_PERIOD_S 10U
#define LED_ON "on"
#define LED_OFF "off"

rt_mq_t  g_ctrl_device_mq;


static rt_thread_t tid;
static char co2_json_buf[16];
static char dht22_json_buf[32];
static char soil_json_buf[16];
static char light_json_buf[16];


/**
 * @brief MQTT下行消息 强定义，覆盖库的__weak默认函数
 */
void mqtt_message_received(const uint8_t *topic,
                                uint16_t topic_length,
                                const uint8_t *payload,
                                uint32_t payload_length){
    ctrl_device_task_msg_t ctrl_device;
    if(strcmp(topic,MQTT_SUBSCRIBE_LED_TOPIC)){
       
    }
       
        
}   


static void mqtt_thread_entry(void *param){
    if (esp8266_mqtt_init())
    {
        return;
    }
    uint32_t tick_now;
    uint32_t tick_last = 0;
    while(1){
        tick_now = rt_tick_get();
        mqtt_receive_poll();
        if(rt_sem_take(g_mqtt_sem,100)){
            //上传频率:每10秒上传一次
            if(tick_now - tick_last >= RT_TICK_PER_SECOND * PUBLISH_PERIOD_S){
                //发布温湿度传感器数据
                snprintf(dht22_json_buf,sizeof(dht22_json_buf),"%.2f,%.2f",
                    g_sensor_data.temperature,
                    g_sensor_data.humidity);
                    mqtt_publish_data(MQTT_SUBSCRIBE_DHT22_TOPIC,dht22_json_buf,0);

                //发布co2传感器数据
                snprintf(co2_json_buf,sizeof(co2_json_buf),"%d",
                    g_sensor_data.co2_value);
                    mqtt_publish_data(MQTT_SUBSCRIBE_CO2_TOPIC,co2_json_buf,0);

                //发布光照传感器数据
                snprintf(light_json_buf,sizeof(light_json_buf),"%d",
                    g_sensor_data.lux);
                    mqtt_publish_data(MQTT_SUBSCRIBE_LIGHT_TOPIC,light_json_buf,0);

                //发布土壤湿度传感器数据
                snprintf(soil_json_buf,sizeof(soil_json_buf),"%d",
                    g_sensor_data.soil_humidity);
                    mqtt_publish_data(MQTT_SUBSCRIBE_SOIL_TOPIC,soil_json_buf,0);
                
                tick_last = tick_now;
            }
        }
    }
}


void app_mqtt_init(){
    g_ctrl_device_mq=rt_mq_create("ctrl_device_mq",sizeof(ctrl_device_task_msg_t),8,RT_IPC_FLAG_FIFO);

    tid=rt_thread_create("mqtt_ctrl",
                            mqtt_thread_entry,
                            RT_NULL,
                            MQTT_THREAD_STACK,
                            MQTT_THREAD_PRIORITY,
                            MQTT_THREAD_TICK);
    if(tid){
        rt_thread_startup(tid);
    }
} 

INIT_APP_EXPORT(app_mqtt_init);
