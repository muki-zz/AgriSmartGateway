#ifndef _ESP_H
#define _ESP_H

#include <stdint.h>

extern volatile uint8_t  g_esp8266_rx_buf[512];
extern volatile uint32_t g_esp8266_rx_cnt;
extern volatile uint32_t g_esp8266_rx_end;
extern volatile uint32_t g_esp8266_rx_stream_overflow;
extern uint8_t g_esp8266_tx_buf[512];
/**
 * @brief 从环形接收缓冲区读取一个字节。
 * @note  主循环只修改读指针，中断只修改写指针，因此无需关闭中断。
 */
int32_t esp8266_rx_stream_read_byte(uint8_t *data);  

/**
 * @brief:初始化esp8266
 */
void esp8266_init(void);

/**
 * @brief:主机发送at指令
 */
void esp8266_send_at(char *str);

/**
 * @brief:主机发送字节数组
 */
void esp8266_send_bytes(uint8_t *buf,uint32_t len);

/**
 * @brief:查找接收数据包的目标字符串
 */
int32_t esp8266_find_str_in_rx_packet(char *str,uint32_t timeout);

/**
 * @brief:测试函数
 */
int32_t  esp8266_self_test(void);

/**
 * @brief：复位
 */
int32_t esp8266_reset(void);

/**
 * @brief：打开或关闭回显
 */
int32_t esp8266_enable_echo(uint32_t b);

/**
 * @brief:进入透传模式
 */
int32_t esp8266_entry_transparent_transmission(void);

 /**
 * @brief:退出透传模式
 */
int32_t esp8266_exit_transparent_transmission(void);

/**
 * @brief:连接热点
 * @param:ssid：Service Set Identifier服务集标识符即热点名
 * @retval:0连接成功，非0连接失败 
 */
int32_t esp8266_connect_ap(char* ssid,char* pswd);

/**
 * @brief:使用指定协议(TCP/UDP)连接到服务器
 * @param：mode:协议类型 "TCP","UDP"
 * @param: ip：服务器，port: 服务器端口号
 */
int32_t esp8266_connect_server(char* mode,char* ip,uint16_t port);

/**
 * @brief:断开连接
 */
int32_t esp8266_disconnect_server(void);

#endif