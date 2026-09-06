#include "app_llcc68_service.h"
#include "app_config.h"
#include "llcc68_app.h"
#include "rtthread.h"
#include "bsp_gpio.h"
#include "app_data_center.h"
#include "main.h"
#include "weather_algo.h"
#include <string.h>


#define IRQ_POLL_MS 10U   // 和裸机demo保持10ms轮询
#define REPORT_PERIOD_MS  10000U 

static rt_thread_t tid;
static rt_sem_t lora_dio1_sem=RT_NULL;
static uint8_t humi_frame[5];
rt_sem_t g_mqtt_sem;
rt_mutex_t g_sensor_mutex;
/**
 * @brief:构造气象补偿算法数据帧
 */
static void build_humi_ctrl_frame(uint8_t* buf, const uint8_t humi_ctrl){
    uint8_t payload_len=1U;
    //帧头
    buf[0] = NODE_SLAVE;
    buf[1] = NODE_MASTER;
    buf[2] = CMD_CTRL_SLAVE_WATER_VALVE;
    buf[3] = payload_len;
    
    uint8_t pos = 4U;
    buf[pos++]= humi_ctrl ;
 }

static void build_led_ctrl_frame(uint8_t* buf,const uint8_t led_ctrl){
    uint8_t payload_len=1U;
    //帧头
    buf[0] = NODE_SLAVE;
    buf[1] = NODE_MASTER;
    buf[2] = CMD_CTRL_SLAVE_LED;
    buf[3] = payload_len;
    
    uint8_t pos = 4U;
    buf[pos++]= led_ctrl;
}

 /**
 * @brief:将一个 16 位采样值封装成应用帧并立即发射
 * @retval:
 */
static int8_t master_send_sensor_data()
{
    g_sensor_data_t temp;
    rt_mutex_take(g_sensor_mutex,RT_WAITING_FOREVER);
    memcpy(&temp,&g_sensor_data,sizeof(temp));
    rt_mutex_release(g_sensor_mutex);
    uint8_t humi_ctrl_update=make_decision(&temp.humi,&temp.temp);
    if(humi_ctrl_update==temp.humi_ctrl){
        return -1;
    }
    build_humi_ctrl_frame(humi_frame,humi_ctrl_update);
    llcc68_app_send(humi_frame, sizeof(humi_frame));
    return 0;
}

/**
 * @brief:注册gpio_exit回调函数
 */
static void llcc68_dio1_cb_handle(){
    rt_sem_release(lora_dio1_sem);
}

g_sensor_data_t g_sensor_data;


/** 校验地址和帧长度，然后按命令处理从机上报的数据。 */
static void master_radio_rx(const uint8_t* data, const uint8_t length)
{
    // //打印原始帧，方便调试
    // for(int i=0;i<length;i++)
    // {
    //     printf("%02X ",data[i]);
    // }
    // printf("\r\n");

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
        if(payload_length != 14U)
        {
            return;
        }
        const uint8_t *p = payload;

        g_sensor_data.co2_value  = ((uint16_t)p[0] << 8) | p[1];
        p += 2;

        uint32_t temp_raw = ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3];
        memcpy(&g_sensor_data.temp, &temp_raw, sizeof(float));
        p +=4;

        uint32_t humi_raw = ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3];
        memcpy(&g_sensor_data.humi, &humi_raw, sizeof(float));
        p +=4;

        g_sensor_data.soil_humi = ((uint16_t)p[0] <<8) | p[1];
        p +=2;

        g_sensor_data.lux = ((uint16_t)p[0] <<8) | p[1];
        p +=2;
        g_sensor_data.humi_ctrl= p[0];
        p +=1;
        rt_sem_release(g_mqtt_sem);
    }
}

static void llcc68_init(){
    llcc68_app_set_rx_callback(master_radio_rx);
    bsp_gpio_set_exit_callback(LORA_DIO1_Pin,llcc68_dio1_cb_handle);
    lora_dio1_sem=rt_sem_create("dio1",0,RT_IPC_FLAG_FIFO);
    g_mqtt_sem=rt_sem_create("mqtt_sem",0,RT_IPC_FLAG_FIFO);
    g_sensor_mutex = rt_mutex_create("sensor_mutex",RT_IPC_FLAG_PRIO);
}


/**
 * @brief:llcc68线程例程函数
 */
static void llcc68_service_thread_entry(void *param){
    llcc68_app_init();
    llcc68_app_start_receive();
    uint32_t last_report_tick=0;
    rt_tick_t period_tick = rt_tick_from_millisecond(REPORT_PERIOD_MS);
    ctrl_device_task_msg_t ctrl_device;
    while(1){
        rt_sem_take(lora_dio1_sem, rt_tick_from_millisecond(IRQ_POLL_MS));
        llcc68_app_process_irq();
        //主动发送，补偿算法控制灌溉机
        rt_tick_t now = rt_tick_get();
        if ((now - last_report_tick) >= period_tick)
        {
            master_send_sensor_data();
            last_report_tick = now;
        }
        //被动发送，小程序控制终端
        rt_mq_recv(g_ctrl_device_mq,&ctrl_device,sizeof(ctrl_device),10);
        switch (ctrl_device.evt)
        {
            case LED_CTRL_EVT:
            {
                uint8_t led_frame[5];
                build_led_ctrl_frame(led_frame,ctrl_device.data.led_ctrl);
                llcc68_app_send(led_frame, sizeof(led_frame));
                break;   
            }
            case WATER_CTRL_EVT:
                break;
            default:
                break;
        }
    }
}

void app_llcc68_service_init(){
    llcc68_init();
    tid=rt_thread_create("llcc68_service",
                            llcc68_service_thread_entry,
                            RT_NULL,
                            LLCC68_SERVICE_THREAD_STACK,
                            LLCC68_SERVICE_THREAD_PRIORITY,
                            LLCC68_SERVICE_THREAD_TICK
                            );
    if(tid){
        rt_thread_startup(tid);
    }else{
        printf("thread create fail\r\n");
    }
}