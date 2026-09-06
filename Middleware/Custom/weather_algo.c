#include "weather_algo.h"
#include "app_data_center.h"

humi_ctrl_t g_humi_base;
humi_ctrl_t g_humi_algo;

static void param_init(){

}

/**
 * @brief:气象补偿,算出补偿后阈
 * @details:温度越高，土壤蒸发越快，需要更早开启灌溉；不要固定死阈值。
 * 只修正开启阈值 hum_low；停止阈值 hum_high 保持不变。
 * 
 */
static void weather_alog_process(float *temp){
    float air_temp=*temp;
    //高温，提前浇水
    if(air_temp >=33.0){
        g_humi_algo.base_hum_low=g_humi_base.base_hum_low+4;
    }
    else if(air_temp>=30.0){
        g_humi_algo.base_hum_low=g_humi_base.base_hum_low+2;
    }
    //低温蒸发慢，延后浇水
    else if(air_temp<=10.0){
        g_humi_algo.base_hum_low=g_humi_base.base_hum_high-3;
    }
}


uint8_t make_decision(float *humi,float *temp){
    float air_humi=*humi,air_temp=*temp;
    weather_alog_process(&air_temp);
    //执行灌溉
    if(air_humi<=g_humi_algo.base_hum_low){
        return 1;
    }
    //关闭灌溉
    else if(air_humi>=g_humi_algo.base_hum_high){
        return 0;
    }
}