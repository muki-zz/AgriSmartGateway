#include "esp8266.h"
#include "usart.h"
#include "rtthread.h"
#include <string.h>




uint8_t g_esp8266_tx_buf[512];

/**
 * 环形接收数据缓冲区
 * 
 */
volatile uint8_t  g_esp8266_rx_buf[512];
volatile uint32_t g_esp8266_rx_cnt=0;
volatile uint32_t g_esp8266_rx_end=0;

/* MQTT 异步数据使用的单生产者、单消费者环形缓冲区。 */
#define ESP8266_RX_STREAM_SIZE 512U
static uint8_t g_esp8266_rx_stream_buf[ESP8266_RX_STREAM_SIZE];
static volatile uint16_t g_esp8266_rx_stream_write = 0U; //写指针（队尾指针）
static volatile uint16_t g_esp8266_rx_stream_read = 0U; //读指针（头指针）
volatile uint32_t g_esp8266_rx_stream_overflow = 0U;

volatile uint32_t g_esp8266_transparent_transmission_sta=0;

static uint8_t g_esp8266_rx_byte;

/**
 * @brief: uart2中断回调注册函数
 */
static void esp8266_rx_handle(uint8_t byte){
	g_esp8266_rx_byte = byte;
	uint16_t next_write;
	if(g_esp8266_rx_cnt<sizeof(g_esp8266_rx_buf)-1U){
		g_esp8266_rx_buf[g_esp8266_rx_cnt++] = g_esp8266_rx_byte;
		/* 保留字符串结束符，供原有 strstr 解析逻辑直接使用。 */
		g_esp8266_rx_buf[g_esp8266_rx_cnt] = '\0';
		if (g_esp8266_rx_byte == '\n')
		{
			g_esp8266_rx_end = 1U;
		}		
	}
	else
	{
		/* 缓冲区满后停止写入，避免覆盖已接收的数据。 */
		g_esp8266_rx_end = 1U;
	}
	/*
	* 同一个接收字节同时写入 MQTT 环形缓冲区。写满时丢弃最新字节，
	* 并记录溢出次数，避免覆盖主循环尚未处理的数据。
	*/
	next_write=g_esp8266_rx_stream_write+1;
	if(next_write>=ESP8266_RX_STREAM_SIZE){
		next_write=0U;
	}
	if(next_write!=g_esp8266_rx_stream_read){
		g_esp8266_rx_stream_buf[g_esp8266_rx_stream_write]=g_esp8266_rx_byte;
		g_esp8266_rx_stream_write=next_write;
	}
	else{
		g_esp8266_rx_stream_overflow++;
	}
}


int32_t esp8266_rx_stream_read_byte(uint8_t *data){
	uint16_t read_index;

	if (data == NULL)
	{
		return -1;
	}
	read_index = g_esp8266_rx_stream_read;
	if (read_index == g_esp8266_rx_stream_write)
	{
		return -1;
	}
	*data = g_esp8266_rx_stream_buf[read_index];
	read_index++;
	if (read_index >= ESP8266_RX_STREAM_SIZE)
	{
		read_index = 0U;
	}
	g_esp8266_rx_stream_read = read_index;
	return 0;

}

/**
 * @brief 丢弃环形缓冲区内尚未读取的历史数据。
 */
void esp8266_rx_stream_reset(void){
	/* 读指针追上写指针即可清空，不需要移动任何接收数据。 */
	g_esp8266_rx_stream_read = g_esp8266_rx_stream_write;
	g_esp8266_rx_stream_overflow = 0U;
}

/**
 * @brief:启动esp8266中断接收
 */
static void esp8266_start_receive_it(void)
{
	uart2_start_read();
}


void esp8266_init(void)
{
	memset((void *)g_esp8266_rx_buf, 0, sizeof(g_esp8266_rx_buf));
	g_esp8266_rx_cnt = 0;
	g_esp8266_rx_end = 0;
	esp8266_rx_stream_reset();
	uart_set_rx_callback(&huart2,esp8266_rx_handle);
	esp8266_start_receive_it();
}


void esp8266_send_at(char *str){
    //清空接收缓冲区
    memset(g_esp8266_rx_buf,0,sizeof(g_esp8266_rx_buf));
    //清空接收计数值
    g_esp8266_rx_cnt=0;
    HAL_UART_Transmit(&huart2,(uint8_t*)str,(uint16_t)strlen(str),HAL_MAX_DELAY);
}

void esp8266_send_bytes(uint8_t *buf,uint32_t len)
{
	/* 本库的数据缓冲区不超过 512 字节，可转换为 HAL 的 16 位长度。 */
	(void)HAL_UART_Transmit(&huart2, buf, (uint16_t)len, HAL_MAX_DELAY);
}

int32_t esp8266_find_str_in_rx_packet(char *str,uint32_t timeout){
    char *dest=str;
    char *src =(char*)g_esp8266_rx_buf;
    while((strstr(src,dest)==NULL) && timeout)
	{		
		rt_thread_mdelay(1);
		timeout--;
	}
    if(timeout) 
		return 0; 
		                    
	return -1; 
}

/* 自检程序 */
int32_t esp8266_self_test(void)
{
	esp8266_send_at("AT\r\n");
    rt_thread_mdelay(1000);
	printf("%s",g_esp8266_rx_buf);
	return esp8266_find_str_in_rx_packet("OK",1000);
}


int32_t esp8266_reset(void)
{
	esp8266_send_at("AT+RST\r\n");
	
	if(esp8266_find_str_in_rx_packet("OK",10000))
		return -1;

	return 0;
}


int32_t esp8266_enable_echo(uint32_t b)
{
	if(b)
		esp8266_send_at("ATE1\r\n"); 
	else
		esp8266_send_at("ATE0\r\n"); 
	
	if(esp8266_find_str_in_rx_packet("OK",5000))
		return -1;

	return 0;
}

int32_t esp8266_entry_transparent_transmission(void){
    //进入透传模式
	esp8266_send_at("AT+CIPMODE=1\r\n");  
	if(esp8266_find_str_in_rx_packet("OK",5000))
		return -1;

    ////开启发送状态
    esp8266_send_at("AT+CIPSEND\r\n");
	if(esp8266_find_str_in_rx_packet("OK",5000))
		return -2;

	//记录当前esp8266工作在透传模式
	g_esp8266_transparent_transmission_sta = 1;
	return 0;
}

int32_t esp8266_exit_transparent_transmission(void){
    esp8266_send_at ("+++");
	
	//退出透传模式，发送下一条AT指令要间隔1秒
	rt_thread_mdelay ( 1000 ); 
	
	//记录当前esp8266工作在非透传模式
	g_esp8266_transparent_transmission_sta = 0;

	return 0;
}

int32_t esp8266_connect_ap(char* ssid,char* pswd){
#if 0
	//不建议使用以下sprintf，占用过多的栈
	char buf[128]={0};
	
	sprintf(buf,"AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",ssid,pswd);
#endif
    //设置为STATION模式	
	esp8266_send_at("AT+CWMODE_CUR=1\r\n"); 
	
	if(esp8266_find_str_in_rx_packet("OK",1000))
		return -1;
	
    //连接目标AP
	//sprintf(buf,"AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",ssid,pswd);
    esp8266_send_at("AT+CWJAP_CUR="); 
	esp8266_send_at("\"");esp8266_send_at(ssid);esp8266_send_at("\"");	
	esp8266_send_at(",");	
	esp8266_send_at("\"");esp8266_send_at(pswd);esp8266_send_at("\"");	
	esp8266_send_at("\r\n");
	if(esp8266_find_str_in_rx_packet("OK",5000)==0)
		if(esp8266_find_str_in_rx_packet("CONNECT",5000)==0)
			return 0;

	return -2;
}

int32_t esp8266_connect_server(char* mode,char* ip,uint16_t port){
#if 0	
	//使用MQTT传递的ip地址过长，不建议使用以下方法，否则导致栈溢出
	//AT+CIPSTART="TCP","a10tC4OAAPc.iot-as-mqtt.cn-shanghai.aliyuncs.com",1883，该字符串占用内存过多了
	
	char buf[128]={0};
	
	//连接服务器
	sprintf((char*)buf,"AT+CIPSTART=\"%s\",\"%s\",%d\r\n",mode,ip,port);
	
	esp8266_send_at(buf);
#elif 1
    char buf[16]={0};
	esp8266_send_at("AT+CIPSTART=");
	esp8266_send_at("\"");	esp8266_send_at(mode);	esp8266_send_at("\"");
    esp8266_send_at(",");
	esp8266_send_at("\"");	esp8266_send_at(ip);	esp8266_send_at("\"");	
    esp8266_send_at(",");
	sprintf(buf,"%d",port);
	esp8266_send_at(buf);	
	esp8266_send_at("\r\n");

#endif 
if(esp8266_find_str_in_rx_packet("CONNECT",5000)==0)
		if(esp8266_find_str_in_rx_packet("OK",5000)==0)
			return 0;
	return -1;
}

int32_t esp8266_disconnect_server(void){
	esp8266_send_at("AT+CIPCLOSE\r\n");	
	if(esp8266_find_str_in_rx_packet("CLOSED",5000))
		if(esp8266_find_str_in_rx_packet("OK",5000))
			return -1;
	
	return 0;	
}

