#include "rawudp.h" 
#include "usart.h"
#include "malloc.h"
#include "stdio.h"
#include "string.h" 
#include "timer.h"
#include "lwip/ip4_addr.h"
#include "lan8720.h"
#include "lwip_comm.h"
#include "app.h"
 




struct udp_pcb *udppcb;  	//定义一个UDP服务器控制块


OS_TCB 	rawudp_task_TCB;
CPU_STK	rawudp_task_stk[RAWUDP_TASK_STK_SIZE];



u8 rawudp_recv_buff[RAWUDP_RX_BUFSIZE];		//UDP接收数据缓冲区 
u8 rawudp_send_buff[RAWUDP_RX_BUFSIZE];		//UDP发送数据内容


static void rawudp_task_fun(void *p_arg);


void rawudp_task_create(void)
{
	OS_ERR os_err;
	CPU_SR_ALLOC();

	
	CPU_CRITICAL_ENTER();
	OSTaskCreate((OS_TCB 	* )&rawudp_task_TCB,		
				 (CPU_CHAR	* )"rawudp_task", 		
                 (OS_TASK_PTR )rawudp_task_fun, 			
                 (void		* )0,					
                 (OS_PRIO	  )RAWUDP_TASK_PRIO,     
                 (CPU_STK   * )&rawudp_task_stk[0],	
                 (CPU_STK_SIZE)RAWUDP_TASK_STK_SIZE/10,	
                 (CPU_STK_SIZE)RAWUDP_TASK_STK_SIZE,		
                 (OS_MSG_QTY  )0,					
                 (OS_TICK	  )0,					
                 (void   	* )0,					
                 (OS_OPT      )OS_OPT_TASK_STK_CHK|OS_OPT_TASK_STK_CLR,
                 (OS_ERR 	* )&os_err); 
				 
	CPU_CRITICAL_EXIT();
				 
	OSTimeDly(10, OS_OPT_TIME_DLY, &os_err);
	
	USART_OUT(USART3, "\rrawudp_task err=%d\r", os_err);	

}


static void rawudp_task_fun(void *p_arg)
{
	OS_ERR err;

	struct ip4_addr rmtipaddr;  	//远端ip地址
 	
	
	udppcb = udp_new();

	if( udppcb )//创建成功
	{		
		IP4_ADDR(&rmtipaddr, lwip_dev.remote_ip[0], lwip_dev.remote_ip[1], lwip_dev.remote_ip[2], lwip_dev.remote_ip[3]);
		USART_OUT(USART3,   "远程IP........................%d.%d.%d.%d\r\n", lwip_dev.remote_ip[0], lwip_dev.remote_ip[1], lwip_dev.remote_ip[2], lwip_dev.remote_ip[3]);
		err = udp_connect(udppcb, &rmtipaddr, 16650);			//UDP客户端连接到指定IP地址和端口号的服务器
		if(err == ERR_OK)
		{
			err = udp_bind(udppcb, IP_ADDR_ANY, 16650);			//绑定本地IP地址与端口号
			if(err == ERR_OK)	//绑定完成
			{						
				udp_recv(udppcb, rawudp_recv_callback, NULL);	//注册接收回调函数 
			}
		}		
	}

	rawudp_send_data(udppcb, "www");
	while(DEF_TRUE)
	{
		
		OSTimeDly(250, OS_OPT_TIME_DLY, &err);
	}
	
}





void rawudp_recv_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	u32 data_len = 0;
	struct pbuf *q;
	LWIP_UNUSED_ARG(arg);
	
	if(p != NULL)	//接收到不为空的数据时
	{
		memset(rawudp_recv_buff, 0, RAWUDP_RX_BUFSIZE);  //数据接收缓冲区清零
		
		for(q=p; q!=NULL; q=q->next)  //遍历完整个pbuf链表
		{
			//判断要拷贝到UDP_DEMO_RX_BUFSIZE中的数据是否大于UDP_DEMO_RX_BUFSIZE的剩余空间，如果大于
			//的话就只拷贝UDP_DEMO_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数据
			if(q->len > (RAWUDP_RX_BUFSIZE - data_len)) 
			{	
				memcpy(rawudp_recv_buff+data_len,q->payload,(RAWUDP_RX_BUFSIZE-data_len));//拷贝数据
			}
			else
			{
				memcpy(rawudp_recv_buff+data_len,q->payload,q->len);
			}
			data_len += q->len;  
			USART_OUT(USART3, rawudp_recv_buff);			
			if(data_len > RAWUDP_RX_BUFSIZE) break; //超出TCP客户端接收数组,跳出	
		}
		
		upcb->remote_ip=*addr; 				//记录远程主机的eIP地址
		upcb->remote_port=port;  			//记录远程主机的端口号
		lwip_dev.remote_ip[0] = upcb->remote_ip.addr&0xff; 		//IADDR4
		lwip_dev.remote_ip[1] = (upcb->remote_ip.addr>>8)&0xff; //IADDR3
		lwip_dev.remote_ip[2] = (upcb->remote_ip.addr>>16)&0xff;//IADDR2
		lwip_dev.remote_ip[3] = (upcb->remote_ip.addr>>24)&0xff;//IADDR1 
		pbuf_free(p);//释放内存
	}else
	{
		udp_disconnect(upcb); 
	
	} 
} 


void rawudp_send_data(struct udp_pcb *upcb, u8 *pdata)
{
	struct pbuf *ptr;
	
	sprintf((char*)rawudp_send_buff, "%s\r", (char*)pdata);
	ptr=pbuf_alloc(PBUF_TRANSPORT, strlen((char*)rawudp_send_buff), PBUF_POOL); //申请内存
	if(ptr)
	{
		pbuf_take(ptr, (char*)rawudp_send_buff, strlen((char*)rawudp_send_buff)); //将tcp_demo_sendbuf中的数据打包进pbuf结构中
		udp_send(upcb, ptr);	//udp发送数据 
		pbuf_free(ptr);//释放内存
	} 
} 
//关闭UDP连接
void rawudp_connection_close(struct udp_pcb *upcb)
{
	udp_disconnect(upcb); 
	udp_remove(upcb);			//断开UDP连接 
}




























