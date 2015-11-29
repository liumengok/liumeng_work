/*
 * base.c
 *
 *  Created on: Oct 9, 2015
 *      Author: openstack
 */

#include "base.h"
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

unsigned char  base64Decode(unsigned char *value,int len,void* persist){
	int i=0,j=0;
	unsigned int data;
	len&=0xfffffffc;
	for(i=0,j=0;i<len;i+=4,j++){
		data=value[i]<<26;
		data+=value[i+1]<<20;
		data+=value[i+2]<<14;
		data+=value[i+3]<<8;
		value[i-j]=data>>24;
		value[i+1-j]=data>>16;
		value[i+2-j]=data>>8;
	}
	return (len>>2)*3;
}

//unsigned char base64EncodeBuff[128] ={0};

unsigned char* base64Encode(unsigned char *value,int len,void* persist){
	int i,j;
	unsigned char rmd[3];
	unsigned int data;
	unsigned char *base64EncodeBuff;
	base64EncodeBuff = (unsigned char *)malloc(128*sizeof(char));
	for(i=0;i<len;i++){
		base64EncodeBuff[i]=value[i];
	}

	for(i=3,j=0;i<=len&&(i+j+1)<128;i+=3,j++){
		base64EncodeBuff[i-3+j]=value[i-3]>>2;
		base64EncodeBuff[i-2+j]=value[i-2]>>4;
		base64EncodeBuff[i-1+j]=value[i-1]>>6;
		base64EncodeBuff[i+j]=value[i-1]&0x3f;
		base64EncodeBuff[i-1+j]+=(value[i-2]&0x0f)<<2;
		base64EncodeBuff[i-2+j]+=(value[i-3]&0x03)<<4;
	}

	if(i>len&&(i+j+1)<128){
		for(i-=3,data=0;i<len;i++,data++) rmd[data]=value[i];
		for(;data<3;data++,i++) rmd[data]=0;
		base64EncodeBuff[i-3+j]=rmd[0]>>2;
		base64EncodeBuff[i-2+j]=rmd[1]>>4;
		base64EncodeBuff[i-1+j]=rmd[2]>>6;
		base64EncodeBuff[i+j]=rmd[2]&0x3f;
		base64EncodeBuff[i-1+j]+=(rmd[1]&0x0f)<<2;
		base64EncodeBuff[i-2+j]+=(rmd[0]&0x03)<<4;
	}

	return base64EncodeBuff;

}
