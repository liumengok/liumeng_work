/*
 * zigbee.c
 *
 *  Created on: 2015年6月5日
 *      Author: ys
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MyThread.h"
#include "zigbee.h"

//char * dev="/dev/ttyUSB0";
//extern char mymac_addr[2];//zigbee地址
//extern int UARTID;//zigbee 接口

DEV_INFO GetZigbeeInfo(char * dev)
{
	char cmd[5]={0xab,0xbc,0xcd,0xd1,0x05};//读取本地配置命令
	DEV_INFO zigbeeinfo;
	char message[73];

	 int fd=uartInit(dev);
	 write(fd,cmd,5);
	 sleep(1);
	 int num=read(fd,message,73);
	 if(num>0)
	 {
//		 if(zigbee.info[])
		 memcpy(zigbeeinfo.info,message+4,65);
	 }
	 return zigbeeinfo;
}

DEV_INFO GetZigbeeInfo2(int fd)
{
	char cmd[5]={0xab,0xbc,0xcd,0xd1,0x05};//读取本地配置命令
	DEV_INFO zigbeeinfo;
	char message[73];

//	 int fd=uartInit(dev);
	 write(fd,cmd,5);
	 sleep(1);
	 int num=read(fd,message,73);
	 if(num>0)
	 {
//		 if(zigbee.info[])
		 memcpy(zigbeeinfo.info,message+4,65);
	 }
	 return zigbeeinfo;
}

//void ZigbeeConfig(int oldaddr,int newaddr,int dial,int target,int model)
//{
//    sendstr senddata;
//	senddata.head[0]=0xfe;//head
//	senddata.head[1]=0xff;
//	senddata.head[2]=0xff;//commandid
//	senddata.head[3]=0x01;
//	senddata.head[4]=mymac_addr[0];//from
//	senddata.head[5]=mymac_addr[1];
//	senddata.head[6]=oldaddr/256;//to
//	senddata.head[7]=oldaddr%256;
//	senddata.head[8]=0x04;//type工作模式 配置
//	senddata.head[9]=0x20;//length
//
//	senddata.data[0]=0x01;//拨码开关
//	senddata.data[1]=0x00;//route
//	senddata.data[2]=dial/256;//value
//	senddata.data[3]=dial%256;
//	senddata.data[4]=0x02;//设备类型 路由 终端
//	senddata.data[5]=0x00;
//	senddata.data[6]=0x00;
//	senddata.data[7]=0x00;
//	senddata.data[8]=0x05;//本地地址
//	senddata.data[9]=0x00;
//	senddata.data[10]=newaddr/256;
//	senddata.data[11]=newaddr%256;
//	senddata.data[12]=0x06;//目的地址
//	senddata.data[13]=0x00;
//	senddata.data[14]=target/256;
//	senddata.data[15]=target%256;
//	senddata.data[16]=0x09;//发送模式 广播 单播
//	senddata.data[17]=0x00;
//	senddata.data[18]=0x00;
//	senddata.data[19]=model;
//
//	senddata.tail[0]=0x00;
//	senddata.tail[1]=0x00;
//	senddata.tail[2]=0x00;
//	senddata.tail[3]=0x00;
//	senddata.tail[4]=0xfd;
//	senddata.tail[5]=0xdf;
//
//	SendData(UARTID,senddata);
//}
//
