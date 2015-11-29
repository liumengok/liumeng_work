/*
 * Main.c
 *
 *  Created on: 2015年5月22日
 *      Author: ys
 */

#include <unistd.h>//windows下为<windows.h>
#include <log4c.h>
#include "MyThread.h"

#define LOGNAME "log_File"

log4c_category_t * logcg;//日志

int main()
{
  //日志
	log4c_init();
	logcg = log4c_category_get(LOGNAME);
	log4c_category_log(logcg, LOG4C_PRIORITY_INFO, "System Start! 时间为格林威治时间");
	//puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */

  SystemStart();
//  sleep(1);
  //看门狗

//  while(1)
//  {
//    sleep(10);//挂起10s
//  }
  	log4c_fini();
    return 0;
}
