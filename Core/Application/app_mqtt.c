#include "app_mqtt_service.h"
#include "app_config.h"
#include "esp8266_mqtt.h"
#include "rtthread.h"
#include "app_data_center.h"
#include <string.h>

#define LED_ON "on"
#define LED_OFF "off"

static rt_mq_t g_mqtt_mq = RT_NULL;
rt_mq_t         g_ctrl_device_mq;

//软件定时器
static rt_timer_t co2_timer = RT_NULL;
static rt_timer_t dht22_timer = RT_NULL;
static rt_timer_t light_timer = RT_NULL;
static rt_timer_t soil_timer = RT_NULL;

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
        if(strcmp(payload,LED_ON)){
            ctrl_device.evt=LED_CTRL_EVT;
            ctrl_device.data.led_ctrl=1U;
            rt_mq_send(g_ctrl_device_mq,&ctrl_device,sizeof(ctrl_device));
        }
        else if(strcmp(payload,LED_OFF)){
            ctrl_device.evt=LED_CTRL_EVT;
            ctrl_device.data.led_ctrl=0U;
            rt_mq_send(g_ctrl_device_mq,&ctrl_device,sizeof(ctrl_device));
        }
    }   
    else if(strcmp(topic,MQTT_SUBSCRIBE_WATER_TOPIC)){
        ctrl_device.evt=WATER_CTRL_EVT;
    }
}


/**
 * @brief:定时器回调函数
 */
static void co2_timer_cb(void *arg){
    mqtt_task_msg_t msg={.task=MQTT_TYPE_CO2_PUBLISH};
    rt_mq_send(g_mqtt_mq,&msg,sizeof(msg));
}

static void dht22_timer_cb(void *arg){
    mqtt_task_msg_t msg={.task=MQTT_TYPE_DHT22_PUBLISH};
    rt_mq_send(g_mqtt_mq,&msg,sizeof(msg));
}

static void light_timer_cb(void* arg){
    mqtt_task_msg_t msg={.task=MQTT_TYPE_LIGHT_PUBLISH};
    rt_mq_send(g_mqtt_mq,&msg,sizeof(msg));
}

static void soil_timer_cb(void* arg){
    mqtt_task_msg_t msg={.task=MQTT_TYPE_SOIL_PUBLISH};
    rt_mq_send(g_mqtt_mq,&msg,sizeof(msg));
}


static void mqtt_service_thread_entry(void *param){
    if (esp8266_mqtt_init())
    {
        return;
    }
    mqtt_task_msg_t msg;
    while(1){
        mqtt_receive_poll();
        if(rt_mq_recv(g_mqtt_mq,&msg,sizeof(msg),RT_WAITING_FOREVER)==RT_EOK){
            switch (msg.task)
            {
                case MQTT_TYPE_CO2_PUBLISH:
                {   
                    snprintf(co2_json_buf,sizeof(co2_json_buf),"%d",
                        g_sensor_data.co2_value);
                    mqtt_publish_data(MQTT_SUBSCRIBE_CO2_TOPIC,co2_json_buf,0);
                    break;
                }

                case MQTT_TYPE_DHT22_PUBLISH:
                {
                    snprintf(dht22_json_buf,sizeof(dht22_json_buf),"%.2f,%.2f",
                        g_sensor_data.temp,
                        g_sensor_data.humi);
                    mqtt_publish_data(MQTT_SUBSCRIBE_DHT22_TOPIC,dht22_json_buf,0);
                    break;
                }
                
                case MQTT_TYPE_LIGHT_PUBLISH:
                {
                    snprintf(light_json_buf,sizeof(light_json_buf),"%d",
                        g_sensor_data.lux);
                    mqtt_publish_data(MQTT_SUBSCRIBE_LIGHT_TOPIC,light_json_buf,0);
                    break;
                }
                case MQTT_TYPE_SOIL_PUBLISH:
                {
                    snprintf(soil_json_buf,sizeof(soil_json_buf),"%d",
                        g_sensor_data.soil_humi);
                    mqtt_publish_data(MQTT_SUBSCRIBE_SOIL_TOPIC,soil_json_buf,0);
                    break;
                }
                default:
                break;
            }
        }
    }
}


void app_mqtt_service_init(){
    g_ctrl_device_mq=rt_mq_create("ctrl_device_mq",sizeof(ctrl_device_task_msg_t),8,RT_IPC_FLAG_FIFO);
    g_mqtt_mq = rt_mq_create("g_mqtt_mq", 
                                sizeof(mqtt_task_msg_t), 
                                16, 
                                RT_IPC_FLAG_FIFO);
    tid=rt_thread_create("led_ctrl",
                            mqtt_service_thread_entry,
                            RT_NULL,
                            MQTT_SERVICE_THREAD_STACK,
                            MQTT_SERVICE_THREAD_PRIORITY,
                            MQTT_SERVICE_THREAD_TICK);
    if(tid){
        rt_thread_startup(tid);
    }
     //开启co2_timer
    co2_timer = rt_timer_create("t_co2", 
                                    co2_timer_cb, 
                                    RT_NULL, 
                                    rt_tick_from_millisecond(8000), 
                                    RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(co2_timer);
    //开启dht22_timer
    dht22_timer = rt_timer_create("t_dht22", 
                                    dht22_timer_cb, 
                                    RT_NULL, 
                                    rt_tick_from_millisecond(9000), 
                                    RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(dht22_timer);
    //开启light_timer
    light_timer = rt_timer_create("t_light", 
                                    light_timer_cb, 
                                    RT_NULL, 
                                    rt_tick_from_millisecond(7000), 
                                    RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(light_timer);
    //开启soil_timer        
    soil_timer = rt_timer_create("t_soil", 
                                    soil_timer_cb, 
                                    RT_NULL, 
                                    rt_tick_from_millisecond(10000), 
                                    RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(soil_timer);
} 
