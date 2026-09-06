#ifndef _APP_LLCC68_SERVIEC_H
#define _APP_LLCC68_SERVICE_H

#define NODE_MASTER                 0x01U
#define NODE_SLAVE                  0x02U   /* 每个从机必须分配唯一地址。 */
#define CMD_SENSOR_DATA             0x02U
#define CMD_CTRL_SLAVE_WATER_VALVE 	0x03U   //网关下发：网关控制终端灌溉电磁阀
#define CMD_SET_WATER_PARMA         0x04U   //小程序修改终端参数
#define CMD_CTRL_SLAVE_LED          0x05U   //小程序控制led
#define CMD_CTRL_SLAVE_WATER        0x06U   //小程序手动控制灌溉机
#define NODE_BROADCAST              0xFFU
#define REPORT_PERIOD_MS            2000U


/**
 * @brief:llcc68初始化
 */
void app_llcc68_service_init();

#endif