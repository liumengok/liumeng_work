/*
 * MyUart.h
 *
 *  Created on: 2015年5月23日
 *      Author: ys
 */

#ifndef MYUART_H_
#define MYUART_H_

int uart_set(int fd,int nSpeed, int nBits, char nEvent, int nStop);
int uart_open(int fd,char * dev);

#endif /* MYUART_H_ */
