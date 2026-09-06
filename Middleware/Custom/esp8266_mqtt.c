#include "esp8266_mqtt.h"
#include "esp8266.h"
#include "rtthread.h"
#include <string.h>



static uint32_t g_mqtt_tx_len=0;

/* 从 USART2 环形缓冲区逐字节组装的单个 MQTT 数据包及当前长度。 */
static uint8_t g_mqtt_rx_packet[sizeof(g_esp8266_rx_buf)];
static uint32_t g_mqtt_rx_packet_length = 0U;



/**
 * @brief MQTT 下行消息的默认处理函数。
 * @note  应用层可以提供同名强定义，根据 Topic 和 Payload 控制具体设备。
 */
__weak void mqtt_message_received(const uint8_t *topic,
                                  uint16_t topic_length,
                                  const uint8_t *payload,
                                  uint32_t payload_length)
{
    /* Topic 和 Payload 带有明确长度，因此无需依赖字符串结束符。 */
    printf("MQTT RX topic: %.*s\r\n", 	(int)topic_length, (const char *)topic);
    printf("MQTT RX payload: %.*s\r\n", (int)payload_length, (const char *)payload);

}

/**
 * @brief 判断接收缓冲区开头是否包含一个完整 MQTT 数据包。
 * @param data 当前接收数据。
 * @param data_length 当前接收字节数。
 * @param packet_length 返回完整 MQTT 包的总长度。
 * @retval 1 表示完整，0 表示数据不足，-1 表示包头非法。
 */
static int32_t mqtt_get_packet_length(const uint8_t *data,
                                      uint32_t data_length,
                                      uint32_t *packet_length)
{
    uint32_t index = 1U;
    uint32_t multiplier = 1U;
    uint32_t remaining_length = 0U;
    uint32_t encoded_count = 0U;
    uint8_t packet_type;
    if(data_length<2U){
        return 0;
    }
    /* MQTT 控制报文类型的有效范围为 1～14。 */
    packet_type = data[0] >> 4U;
    if ((packet_type == 0U) || (packet_type == 15U))
    {
        return -1;
    }

    /* Remaining Length 使用最多四字节的 MQTT 变长编码。 */
     while (encoded_count < 4U){
        uint8_t encoded_byte;
        if(index>=data_length){
            return 0;
        }
        encoded_byte=data[index++];
        remaining_length += (uint32_t)(encoded_byte & 0x7FU) * multiplier;//0x7F:0111 1111
        encoded_count++;

        if ((encoded_byte & 0x80U) == 0U)
        {
            /* 本实现只接收能够装入现有 512 字节缓冲区的数据包。 */
            if (remaining_length > ((sizeof(g_mqtt_rx_packet) - 1U) - index))
            {
                return -1;
            }

            *packet_length = index + remaining_length;
            return (data_length >= *packet_length) ? 1 : 0;
        }
        multiplier *=128U;
        
    }
    return -1;
}

/**
 * @brief 解析一个已经接收完整的 MQTT PUBLISH 数据包。
 */
static void mqtt_parse_publish_packet(const uint8_t *packet,
                                      uint32_t packet_length)
{   
    uint32_t index = 1U;
    uint32_t multiplier = 1U;
    uint16_t topic_length;
    uint8_t qos;
    const uint8_t *topic;
    const uint8_t *payload;
    uint32_t payload_length;
    /* 跳过变长编码的 Remaining Length 字段。 */
    while (index < packet_length)
    {
        uint8_t encoded_byte = packet[index++];

        if ((encoded_byte & 0x80U) == 0U)
        {
            break;
        }

        multiplier *= 128U;
        if (multiplier > (128U * 128U * 128U))
        {
            return;
        }
    }
    if ((index + 2U) > packet_length)
    {
        return;
    }
    /* MQTT 的 Topic 长度采用高字节在前的网络字节序。 */
    topic_length = ((uint16_t)packet[index] << 8U) |
                   (uint16_t)packet[index + 1U];
    index += 2U;

    if ((uint32_t)topic_length > (packet_length - index))
    {
        return;
    }
    topic = &packet[index];
    index += topic_length;
    /* QoS 1/2 的 PUBLISH 可变头中还包含两字节报文标识符。 */
    //qos = (packet[0] >> 1U) & 0x03U; qos占的是LSB的bit1 bit2
    qos = (packet[0]>>1U) & 0x03U;
    if (qos == 3U)
    {
        return;
    }

    if (qos > 0U)
    {
        if ((index + 2U) > packet_length)
        {
            return;
        }

        index += 2U;
    }
    payload = &packet[index];
    payload_length = packet_length - index;
    mqtt_message_received(topic, topic_length, payload, payload_length);
}


void mqtt_receive_poll(void){
    uint8_t rx_byte;
    /*
    * 环形缓冲区溢出意味着当前 MQTT 包可能缺少字节。
    * 丢弃残包并重新同步，比继续解析损坏数据更安全。
    */
    if(g_esp8266_rx_stream_overflow>0U){
        uint32_t overflow_count = g_esp8266_rx_stream_overflow;
        esp8266_reset();
        g_mqtt_rx_packet_length = 0U;
        printf("ESP8266 RX stream overflow: %lu\r\n",
               (unsigned long)overflow_count);
    }
    /*
    * 主循环只移动环形缓冲区读指针，USART2 中断只移动写指针。
    * 整个处理过程不关闭中断，因此连续接收时不会因 memcpy/memmove 阻塞 RXNE。
    */
   while(esp8266_rx_stream_read_byte(&rx_byte)==0){
        uint32_t expected_packet_length = 0U;
        int32_t packet_status;
        /* 新包的第一个字节必须包含有效的 MQTT 控制报文类型。 */
        if (g_mqtt_rx_packet_length == 0U)
        {   
            uint8_t packet_type = rx_byte >> 4U;
            if ((packet_type == 0U) || (packet_type == 15U))
            {
                continue;
            }
        }
        /* 预留一个字节空间，保持与原 512 字节接收缓冲区容量一致。 */
        if (g_mqtt_rx_packet_length >= (sizeof(g_mqtt_rx_packet) - 1U))
        {
            g_mqtt_rx_packet_length = 0U;
            continue;
        }
        g_mqtt_rx_packet[g_mqtt_rx_packet_length++] = rx_byte;
        /*判断MQTT数据包是否完整*/
        packet_status = mqtt_get_packet_length(g_mqtt_rx_packet,
                                               g_mqtt_rx_packet_length,
                                               &expected_packet_length);
        if(packet_status>0){
            /* 固定头高四位为 3 时，当前数据包是服务器下发的 PUBLISH。 */
            if ((g_mqtt_rx_packet[0] >> 4U) == 3U)
            {
                mqtt_parse_publish_packet(g_mqtt_rx_packet,
                                          expected_packet_length);

            }
            /* 当前包已消费，后续环形缓冲区数据将从新包头开始组装。 */
            g_mqtt_rx_packet_length = 0U;
        }
        else if (packet_status < 0)
        {
            /* 变长字段非法或数据包过大时放弃当前包，等待下一个有效包头。 */
            g_mqtt_rx_packet_length = 0U;
        }
   }
}

void mqtt_send_bytes(uint8_t *buf,uint32_t len)
{
    esp8266_send_bytes(buf,len);
}

int32_t mqtt_connect(char *client_id,char *user_name,char *password){
    uint32_t client_id_len = strlen(client_id);
    uint32_t user_name_len = strlen(user_name);
    uint32_t password_len = strlen(password);
    uint32_t data_len;
    uint32_t cnt=2;
    uint32_t wait=0;
    uint8_t connect_flag = 0x02; // cleanSession=1，用户名密码位默认0

    g_mqtt_tx_len=0;

    // 根据用户名密码是否存在设置标志位
    if(user_name_len > 0){
        connect_flag |= (1 <<7);
    }
    if(password_len >0){
        connect_flag |= (1 <<6);
    }

    //==================== CONNECT可变报头固定10字节 ====================
    data_len = 10 + (client_id_len + 2);
    if(user_name_len>0){
        data_len += (user_name_len + 2);
    }
    if(password_len>0){
        data_len += (password_len + 2);
    }

    //固定报头 0x10
    g_esp8266_tx_buf[g_mqtt_tx_len++]=0x10;

    //剩余长度编码
    do{
        uint8_t encodedByte = data_len % 128;
        data_len = data_len / 128;
        if(data_len> 0 ){
            encodedByte =encodedByte | 0x80;
        }
        g_esp8266_tx_buf[g_mqtt_tx_len++]=encodedByte;
    }while(data_len>0);

    //可变报头：协议名 MQTT
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 4;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'M';
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'Q';
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'T';
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 'T';
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 4;        // MQTT3.1.1 version=4
    g_esp8266_tx_buf[g_mqtt_tx_len++] = connect_flag; //修复标志位！
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0;
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 60;       // keepalive 60s

    //有效载荷 clientId
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(client_id_len);
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(client_id_len);
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],client_id,client_id_len);
    g_mqtt_tx_len += client_id_len;

    //用户名，非空才输出
    if(user_name_len > 0)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(user_name_len);
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(user_name_len);
        memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],user_name,user_name_len);
        g_mqtt_tx_len += user_name_len;
    }
    //密码，非空才输出
    if(password_len > 0)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(password_len);
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(password_len);
        memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],password,password_len);
        g_mqtt_tx_len += password_len;
    }

    while(cnt--)
    {
        memset((void *)g_esp8266_rx_buf,0,sizeof(g_esp8266_rx_buf));
		g_esp8266_rx_cnt=0;
		
        mqtt_send_bytes(g_esp8266_tx_buf,g_mqtt_tx_len);
		
        wait=3000;//等待3s时间
		
        while(wait--)
        {
			rt_thread_mdelay(1);

			//检查连接确认固定报头
            if((g_esp8266_rx_buf[0]==0x20) && (g_esp8266_rx_buf[1]==0x02)) 
            {
				if(g_esp8266_rx_buf[3] == 0x00)
				{
					printf("连接已被服务器端接受，连接确认成功\r\n");
					return 0;//连接成功
				}
				else
				{
					switch(g_esp8266_rx_buf[3])
					{
						case 1:printf("连接已拒绝，不支持的协议版本\r\n");
						break;
						case 2:printf("连接已拒绝，不合格的客户端标识符\r\n");
						break;		
						case 3:printf("连接已拒绝，服务端不可用\r\n");
						break;		
						case 4:printf("连接已拒绝，无效的用户或密码\r\n");
						break;	
						case 5:printf("连接已拒绝，未授权\r\n");
						break;
						default:printf("未知响应\r\n");
						break;
					}
					return 0;
				} 
            }  
        }
    }
	
    return -1;
}

void mqtt_disconnect(void)
{
	uint8_t buf[2]={0xe0,0x00};
	
    mqtt_send_bytes(buf,2);
	
	esp8266_disconnect_server();
}

int32_t mqtt_subscribe_topic(char *topic,uint8_t qos,uint8_t whether){
    uint32_t cnt=2;
    uint32_t wait=0;

    uint32_t topiclen = strlen(topic);
    g_mqtt_tx_len=0;
    //==================== SUBSCRIBE 固定报头 ====================
    //控制报文类型
    if(whether)
        g_esp8266_tx_buf[g_mqtt_tx_len++]=0x82;//订阅
    else 
        g_esp8266_rx_buf[g_mqtt_tx_len++]=0xA2;//取消订阅
    //剩余长度    
    uint32_t data_len = 2 + (topiclen+2) + (whether?1:0);//剩余长度可变报头的长度（2字节）加上有效载荷的长度
    do
    {
        uint8_t encodedByte = data_len % 128;
        data_len = data_len / 128;
        // if there are more data to encode, set the top bit of this byte
        if ( data_len > 0 )
            encodedByte = encodedByte | 128;
        g_esp8266_tx_buf[g_mqtt_tx_len++] = encodedByte;
    } while ( data_len > 0 );    

    //==================== SUBSCRIBE 可变报头 ====================
    g_esp8266_rx_buf[g_mqtt_tx_len++]=0;        //消息标识符 MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++]=0x01;     //消息标识符 LSB

    //==================== SUBSCRIBE 有效载荷 ====================     
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(topiclen);//主题长度 MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(topiclen);//主题长度 LSB
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],topic,topiclen);
    g_mqtt_tx_len += topiclen;

    if(whether)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = qos;//QoS级别
    }

    while(cnt--)
    {
		g_esp8266_rx_cnt=0;
        memset((void *)g_esp8266_rx_buf,0,sizeof(g_esp8266_rx_buf));
        mqtt_send_bytes(g_esp8266_tx_buf,g_mqtt_tx_len);
		
        wait=3000;//等待3s时间
        while(wait--)
        {
			rt_thread_mdelay(1);
			
			//检查订阅确认报头
            if(g_esp8266_rx_buf[0]==0x90)
            {
				printf("订阅主题确认成功\r\n");
				
				//获取剩余长度
				if(g_esp8266_rx_buf[1]==3)
				{
					printf("Success - Maximum QoS 0 is %02X\r\n",g_esp8266_rx_buf[2]);
					printf("Success - Maximum QoS 2 is %02X\r\n",g_esp8266_rx_buf[3]);		
					printf("Failure is %02X\r\n",g_esp8266_rx_buf[4]);	
				}
				//获取剩余长度
				if(g_esp8266_rx_buf[1]==2)
				{
					printf("Success - Maximum QoS 0 is %02X\r\n",g_esp8266_rx_buf[2]);
					printf("Success - Maximum QoS 2 is %02X\r\n",g_esp8266_rx_buf[3]);			
				}				
				
				//获取剩余长度
				if(g_esp8266_rx_buf[1]==1)
				{
					printf("Success - Maximum QoS 0 is %02X\r\n",g_esp8266_rx_buf[2]);		
				}	
			
                return 0;//订阅成功
            }
            
        }
    }
	
    if(cnt) 
		return 0;	//订阅成功
	
    return -1;

}

uint32_t mqtt_publish_data(char *topic, char *message, uint8_t qos)
{
static 
	uint16_t id=0;	
    uint32_t topicLength = strlen(topic);
    uint32_t messageLength = strlen(message);

    uint32_t data_len;
	uint8_t encodedByte;

    g_mqtt_tx_len=0;
    //有效载荷的长度这样计算：用固定报头中的剩余长度字段的值减去可变报头的长度
    //QOS为0时没有标识符
    //数据长度             主题名   报文标识符   有效载荷
    if(qos)	data_len = (2+topicLength) + 2 + messageLength;
    else	data_len = (2+topicLength) + messageLength;

    //固定报头
    //控制报文类型
    g_esp8266_tx_buf[g_mqtt_tx_len++] = 0x30;    // MQTT Message Type PUBLISH

    //剩余长度
    do
    {
        encodedByte = data_len % 128;
        data_len = data_len / 128;
        // if there are more data to encode, set the top bit of this byte
        if ( data_len > 0 )
            encodedByte = encodedByte | 128;
        g_esp8266_tx_buf[g_mqtt_tx_len++] = encodedByte;
    } while ( data_len > 0 );

    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(topicLength);//主题长度MSB
    g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(topicLength);//主题长度LSB
	
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],topic,topicLength);//拷贝主题
	
    g_mqtt_tx_len += topicLength;

    //报文标识符
    if(qos)
    {
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE1(id);
        g_esp8266_tx_buf[g_mqtt_tx_len++] = BYTE0(id);
        id++;
    }
	
    memcpy(&g_esp8266_tx_buf[g_mqtt_tx_len],message,messageLength);
	
    g_mqtt_tx_len += messageLength;
	

	mqtt_send_bytes(g_esp8266_tx_buf,g_mqtt_tx_len);
	
	
	//咱们的Qos等级设置的是00，因此阿里云物联网平台是没有返回响应信息的
	return g_mqtt_tx_len;
}


int32_t esp8266_mqtt_init(void){
    int32_t ret;
    //初始化esp8266模块
    esp8266_init();
    ret=esp8266_exit_transparent_transmission();
    if(ret){
        printf("esp8266_exit_transparent_transmission failed\r\n");
        return -1;
    }
    printf("esp8266_exit_transparent_transmission success\r\n");
	rt_thread_mdelay(2000);

    //复位模块
    ret=esp8266_reset();
    if(ret)
	{
		printf("esp8266_reset fail\r\n");
		return -2;
	}
	printf("esp8266_reset success\r\n");
	rt_thread_mdelay(2000);	

    //关闭回显
    ret=esp8266_enable_echo(0);
	if(ret)
	{
		printf("esp8266_enable_echo(0) fail\r\n");
		return -3;
	}	
	printf("esp8266_enable_echo(0)success\r\n");
	rt_thread_mdelay(2000);		

    //连接热点
    ret=esp8266_connect_ap(WIFI_SSID,WIFI_PASSWORD);
    if(ret){
        printf("esp8266_connect_ap failed\r\n");
        return -4;
    }
    printf("esp8266_connect_ap success\r\n");
    rt_thread_mdelay(2000);

    //连接中转站
    ret =esp8266_connect_server("TCP",MQTT_BROKERADDRESS,MOTT_PORT);
	if(ret)
	{
		printf("esp8266_connect_server fail\r\n");
		return -5;
	}	
	printf("esp8266_connect_server success\r\n");
	rt_thread_mdelay(2000);

    //进入透传模式
    ret=esp8266_entry_transparent_transmission();
    if(ret){
        printf("esp8266_entry_transparent_transmission failed\r\n");
        return -6;
    }
    printf("esp8266_entry_transparent_transmission success\r\n");
    rt_thread_mdelay(2000);

    /*
	 * 进入透传模式后清除此前积累的 AT 文本，只保留后续原始 MQTT 数据。
	 * 同时清空可能残留的半包组装状态，便于重新连接后再次解析。
	*/
    esp8266_rx_stream_reset();
	g_mqtt_rx_packet_length = 0U;
    //MQTT客户端
    ret=mqtt_connect(MQTT_CLIENTID,MQTT_USERNAME,MQTT_PASSWORD);
    if(ret){
        printf("mqtt_connect failed\r\n");
        return -7;
    }
    printf("mqtt_connect success\r\n");
	rt_thread_mdelay(2000);	

    //订阅主题
    ret=mqtt_subscribe_topic(MQTT_SUBSCRIBE_LED_TOPIC,0,1);
    if(ret){
		printf("mqtt_subscribe_topic fail\r\n");
		return -8;
	}	
	printf("mqtt_subscribe_topic success\r\n");
    rt_thread_mdelay(2000);	

	ret=mqtt_subscribe_topic(MQTT_SUBSCRIBE_CO2_TOPIC,0,1);
    if(ret){
		printf("mqtt_subscribe_topic fail\r\n");
		return -8;
	}
    rt_thread_mdelay(2000);	

    ret=mqtt_subscribe_topic(MQTT_SUBSCRIBE_DHT22_TOPIC,0,1);
    if(ret){
		printf("mqtt_subscribe_topic fail\r\n");
		return -8;
	}
    rt_thread_mdelay(2000);	

    ret=mqtt_subscribe_topic(MQTT_SUBSCRIBE_SOIL_TOPIC,0,1);
    if(ret){
		printf("mqtt_subscribe_topic fail\r\n");
		return -8;
	
    }
    
    rt_thread_mdelay(2000);	
    
    ret=mqtt_subscribe_topic(MQTT_SUBSCRIBE_LIGHT_TOPIC,0,1);
    if(ret){
		printf("mqtt_subscribe_topic fail\r\n");
		return -8;
	}	
	return 0;

}