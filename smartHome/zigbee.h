/*
 * zigbee.h
 *
 *  Created on: 2015年6月5日
 *      Author: ys
 */

#ifndef ZIGBEE_H_
#define ZIGBEE_H_

typedef struct{
	char info[65];
}DEV_INFO;

DEV_INFO GetZigbeeInfo(char * dev);//获取zigbee信息
DEV_INFO GetZigbeeInfo2(int fd);//获取zigbee信息
void ZigbeeConfig(int oldaddr,int newaddr,int dial,int target,int model);//配置zigbee

#endif /* ZIGBEE_H_ */
