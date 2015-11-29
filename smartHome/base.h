/*
 * base.h
 *
 *  Created on: Oct 9, 2015
 *      Author: openstack
 */

#ifndef BASE_H_
#define BASE_H_



#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

unsigned char* base64Encode(unsigned char *value,int len,void* persist);
unsigned char  base64Decode(unsigned char *value,int len,void* persist);

#endif /* BASE_H_ */
