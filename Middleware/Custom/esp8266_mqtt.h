#ifndef _ESP8266_MQTT_H
#define _ESP8266_MQTT_H

#include <stdint.h>

#define BYTE0(dwTemp)       (*( char *)(&dwTemp))
#define BYTE1(dwTemp)       (*((char *)(&dwTemp) + 1))
#define BYTE2(dwTemp)       (*((char *)(&dwTemp) + 2))
#define BYTE3(dwTemp)       (*((char *)(&dwTemp) + 3))

#define WIFI_SSID "muki"
#define WIFI_PASSWORD "88888888"

#define MQTT_BROKERADDRESS "mqtt.bemfa.com"
#define MOTT_PORT           9501
#define MQTT_CLIENTID "b1bfb4d1bcc549b8965be23893d2c134"
#define MQTT_USERNAME  ""//AppID
#define MQTT_PASSWORD "" //SecretKey



#define MQTT_SUBSCRIBE_CO2_TOPIC "co2001"
#define MQTT_SUBSCRIBE_DHT22_TOPIC "humiture001"
#define MQTT_SUBSCRIBE_LIGHT_TOPIC "light001"
#define MQTT_SUBSCRIBE_SOIL_TOPIC "soil001"
#define MQTT_SUBSCRIBE_LED_TOPIC "led001"
#define MQTT_SUBSCRIBE_WATER_TOPIC "led001"

/**
 * @brief:从 ESP8266 环形缓冲区逐字节组装并处理服务器 MQTT 数据。
 */
void mqtt_receive_poll(void);

/**
 * @brief：通过 TCP 将CONNECT报文发给 MQTT 服务器
 */
int32_t mqtt_connect(char *client_id,char *user_name,char *password);

/**
 * @brief：MQTT无条件断开
 */
void mqtt_disconnect(void);

/**
 * @brief：MQTT订阅/取消订阅
 * @param：topic主题，qos信息等级，whether订阅/取消订阅请求包
 */
int32_t mqtt_subscribe_topic(char *topic,uint8_t qos,uint8_t whether);

/**
 * @brief：发布信息
 */
uint32_t mqtt_publish_data(char *topic, char *message, uint8_t qos);

/**
 * @brief:：MQTT初始化
 */
int32_t esp8266_mqtt_init(void);

#endif