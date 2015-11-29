/*
 * MyThread.c
 *主要线程
 * Created on: 2015年7月7日
 * @Author: liumeng
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include "MyUart.h"
#include <sys/types.h>
#include <dirent.h>
#include <log4c.h>
#include "MyThread.h"
#include "zigbee.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include "sqlite3.h"
#include <time.h>
#include <signal.h>
#include "myList.h"
#include "myQueue.h"
#include "base.h"
//#include <openssl/aes.h>
#define MAXLINE 80
#define SERV_PORT 8000
#define CRON_PORT 8001
#define GET_PORT 8002
#define SQL_PORT 8003
#define RAMDIR "./ram1/"
#define UARTDIR "/dev/ttyS3"
#define DBDIR "/opt/smarthome.db"
Queue * dataqu; 						//发送命令队列
Queue *recvqu;							//接收命令队列
List * confirmList;
List * list;										//返回命令队列
sqlite3 * db=NULL;					//内存数据库
char mymac_addr[2];				//zigbee地址`
int UARTID=0;							//zigbee 接口
extern log4c_category_t * logcg;
int commandnum = 0;         //由网关发送命令commandId记录
int entry_num = 0;				 //红外计数
clock_t entry_start = 0,entry_end = 0;           //两个红外进出时间记录
//unsigned char user_key[16];	//aes密钥

pthread_mutex_t mutex;
//开关资源锁
pthread_mutex_t mutex_switch;
pthread_mutex_t mutexStatus;
//发送队列资源锁
pthread_mutex_t mutex_queue;
//接收队列资源锁
pthread_mutex_t mutex_reciqu;
//写入数据时，数据库资源锁
pthread_mutex_t mutex_sqlite;
//写入数据库，内存数据库资源锁
pthread_mutex_t mutex_mem_sqlite;

int SystemStart()
{
		//普通业务命令线程
		pthread_t CommandThrId;
		//串口接收线程
		pthread_t UartRXThrId;
		//查询状态命令线程
		pthread_t StatusThrId;
		//定时命令线程
		pthread_t CronThrId;
		//数据库更新线程
		pthread_t Sqlite3ThrId;
		//发送命令线程
		pthread_t SendDataThrId;

		int err;
		log4c_category_log(logcg, LOG4C_PRIORITY_INFO, "zigbee地址: %s",UARTDIR);
		log4c_category_log(logcg, LOG4C_PRIORITY_INFO, "数据库地址: %s",DBDIR);

		//发送命令队列
		dataqu =  InitQueue();
		//接收返回信息队列
		recvqu = InitQueue();

		//接收命令列表
		list =  InitList();
		//开关命令接收列表
		confirmList = InitList();
		UARTID = uartInit(UARTDIR);
		DEV_INFO zgbinfo = GetZigbeeInfo2(UARTID);
		memcpy(mymac_addr,zgbinfo.info+36,2);
		//数据库的初始化
		db = sqliteInit(DBDIR);
		//    createAesKey();
		err = pthread_create(&CommandThrId,NULL,CommandThr,NULL);
		if(err != 0)
		{
				printf("cannot create thread search db:%s\n",strerror(err));
				return -1;
		}
		err = pthread_create(&UartRXThrId,NULL,UartRXThr,NULL);
		if(err != 0)
		{
				printf("cannot create thread serial port RX:%s\n",strerror(err));
				return -1;
		}
		err = pthread_create(&StatusThrId,NULL,StatusThr,NULL);
		if(err != 0)
		{
				return -1;
		}
		//定时命令线程
		err = pthread_create(&CronThrId,NULL,CronThr,NULL);
		if(err != 0)
		{
				return -1;
		}
		err = pthread_create(&Sqlite3ThrId,NULL,Sqlite3Thr,NULL);
		if(err != 0)
		{
				return -1;
		}
		err = pthread_create(&SendDataThrId,NULL,SendDataThr,NULL);
		if(err != 0)
		{
				return -1;
		}

		pthread_join(UartRXThrId,NULL);   						 //回收资源
		pthread_join(CommandThrId,NULL);
		pthread_join(StatusThrId,NULL);
		pthread_join(CronThrId,NULL);
		pthread_join(Sqlite3ThrId,NULL);
		pthread_join(SendDataThrId,NULL);
		sqlite3_close(db);
		return 0;
}

sqlite3 * sqliteInit(char * name)    //数据库初始化
{
	  	  int ret = 0;
	  	  sqlite3 *memoryDb;
	  	  ret = sqlite3_open(":memory:", &memoryDb);
	  	  ret = loadOrSaveDb(memoryDb, name, 0) ;//文件数据库导入到内存数据库

	  	  if(ret)
	  	  {
	  		  	  printf("fail  :  %s\n",sqlite3_errmsg(memoryDb));
	  		  	  exit(EXIT_FAILURE);
	  	  }
	  	  else
	  	  {
	  		  	  printf("success open memoryDb!\n");
	  	  }
	  	  return memoryDb;
}

int uartInit(char * name)            //串口初始化
{
		int fd = 0;

		if((fd = uart_open(fd,name))<0)
		{
				perror("open port error");
				exit(EXIT_FAILURE);
		}
		if(uart_set(fd,115200,8,'N',1)<0)
		{
				perror("set port error");
				exit(EXIT_FAILURE);
		}
		return fd;
}

int socketInit(int port)
{
	    struct sockaddr_in servaddr;//服务器网络地址结构体
	    int listenfd;
	    int opt = 1;

	    /*创建服务器端套接字--IPv4协议，面向连接通信，TCP协议*/
	    if((listenfd = socket(AF_INET, SOCK_STREAM, 0))<0)
	    {
	        perror("socket error");
	        return 0;
	    }

	    bzero(&servaddr, sizeof(servaddr));//数据初始化--清零
	    servaddr.sin_family = AF_INET;//设置为IP通信
	    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//服务器IP地址--允许连接到所有本地地址上
	    servaddr.sin_port = htons(port);//服务器端口号

	    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	    /*将套接字绑定到服务器的网络地址上*/
	    if((bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)))<0)
	    {
	    	perror("bind error");
	    	return 0;
	    }
	    /*监听连接请求--监听队列长度为20*/
	    listen(listenfd, 30);

	    return listenfd;
}

void * UartRXThr(void *args)         //串口接收线程
{
		struct timeval timeout = {3,0};//select等待3秒，3秒轮询 sec usec
		fd_set fs_read;
		char name[10];
		int i = 0,num = 0,flag = 0,num_clear = 0,err  =  0;
		unsigned char array[128];
		unsigned char temp[128];
		unsigned char valid[100];
		char str[30];
		char str_comp[30];
		int type,rc,infrared_route  =  -1,j = 1;
		char  sql[256];
		sqlite3_stmt * stmt;
		while(1)
		{
				FD_ZERO(&fs_read);
				FD_SET(UARTID,&fs_read);
				timeout.tv_sec = 3;
				timeout.tv_usec = 0;
				if(select(UARTID+1,&fs_read,NULL,NULL,&timeout) < 0)
				{
						perror("select");
						continue;
				}
				else
				{
						if(FD_ISSET(UARTID,&fs_read))
						{
								while((num = read(UARTID,temp,100)) > 0)
								{
										// printf("%d: ",num);
										for(i = 0;i<num;i++)
										{
												// printf("%x ",temp[i]);
												if(temp[i]==0x80||temp[i]==0x82)
												{
														array[0] = temp[i];
														j = 1;
														flag = 1;
												}
												else if(flag==1)
												{
													   array[j] = temp[i];
													   if(array[j]==0x40)
														   	   break;
													   	j++;
												}
										}
										if(array[j]==0x40)
										{
												memset(temp,0,128);
												break;
										}
										memset(temp,0,128);
								}
								if(array[j]==0x40)
								{
										array[j] = '\0';
										flag = 0;
										num = 0;
										int base_len = base64Decode(array+1,j-1,NULL);
										int len = array[8];
										printf("real_array: ");
										int ti = 0;
										for(ti = 0;ti<base_len+1;ti++)
										{
												printf("%x ",array[ti]);
										}
										printf("\n");
										j = 1;
										if(len<10)
										{
												memset(array,0,128);
												memset(valid,0,100);
												continue;
										}
										memcpy(valid,array+1,len-4);
										uint rcrc = CRC32Software((unsigned char *)valid,len-4);
										uint crc = ((array[len-3]&0xFF)<<24)| ((array[len-2]&0xFF)<<16)| ((array[len-1]&0xFF)<<8)| (array[len]&0xFF);

										if(crc != rcrc)
										{
												memset(array,0,128);
												memset(valid,0,100);
												continue;
										}
										//			   	printf("crc\n");
										type = array[7];

										switch(type)
										{
												case TYPE_T_ASK_NET:
												{
														int commandid = array[1]*256+array[2];
														int destzigbee = array[3]*256+array[4];
														int flag = array[10];
														int phy_addr = array[11];
														char mac_addr[20];
														HexToStr(mac_addr,(unsigned char *)array+12,8);
														int type = array[20];
														printf("mac_addr = %s type =%x \n",mac_addr,type);
														answerRegister(commandid,destzigbee,phy_addr,mac_addr,flag,type);
														break;
												}
												case  TYPE_T_ASK_RELATION:
												{
														int commandid = array[1]*256+array[2];
														int destzigbee = array[3]*256+array[4];
														int flag = array[10];
														int phy_addr = array[11];
														char mac_addr[20];
														HexToStr(mac_addr,(unsigned char *)array+12,8);
														sendSwitchMapping(commandid,destzigbee,phy_addr,mac_addr,flag);
														break;
												}
												case TYPE_T_ASK_RUN:
												{
														char mac_addr[20];
														HexToStr(mac_addr,(unsigned char *)array+12,8);
														if(array[9] == SUBTYPE_SWITCH)//开关 情景模式
														{
															//  scenCommand(mac_addr,route);
																pthread_t switchCommandThrId;
																pthread_mutex_lock(&mutex_switch);
																err = pthread_create(&switchCommandThrId,NULL,switchCommandThr,&array);
																if(err != 0)
																{
																		printf("cannot create thread %s\n",strerror(err));
																}
																usleep(1000);
														}
														if(array[9]==0x25)//红外感应
														{
																sprintf(sql,"select route from phymapping where mac_addr='%s' ",mac_addr);
																rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
																if(rc!=SQLITE_OK)
																{
																		printf("select route error\n");
																}
																if(sqlite3_step(stmt)==SQLITE_ROW)
																{
																		infrared_route=sqlite3_column_int(stmt,0);
																}
																sqlite3_finalize(stmt);
																//	printf("infrared_route = %d ",infrared_route);
																if(infrared_route == -1)
																{
																		continue;
																}
																if(infrared_route == 1)
																{
																		entry_start = clock();
																		//printf("entry_start = %d ",entry_start);
																		if(-1<entry_start-entry_end&&entry_start-entry_end<35000)
																		{
																				entry_num--;
																				//	printf("entry_num = %d ",entry_num);
																				infraredInductionCommand(mac_addr);
																		}
																}
																else if(infrared_route == 2)
																{
																		entry_end = clock();
																		//printf("entry_end = %d ",entry_end);
																		if(-1<entry_end-entry_start&&entry_end-entry_start<35000) 		//0ms---19ms
																		{
																				entry_num++;
																				//	printf("entry_num = %d ",entry_num);
																				infraredInductionCommand(mac_addr);
																		}
																}
														}
														break;
												}
												case TYPE_G_RUN_INFO:
												{
														int commandid = array[1]*256+array[2];
														sprintf(name,"%d",commandid);
														pthread_mutex_lock(&mutexStatus);
														//      printf("commandid = %d\n",commandid);
														if(list->size>0)
														{
																strcpy(str,list->front->data);
																if(strcmp(str_comp,str))
																{
																		strcpy(str_comp,str);
																		num_clear = 0;
																}
																else
																{
																		num_clear++;
																		if(num_clear>10)
																		{
																				DeList(list);
																				printf("\n delete \n");
																				num_clear = 0;
																		}
																}
														}
														EnList(list,name);
														ListTraverse(list);
														pthread_mutex_unlock(&mutexStatus);
														break;
												}
												case 0x10:
												{
														pthread_mutex_lock(&mutex_reciqu);
														EnQueuebyArray(recvqu,array);
														QueueTraverse(recvqu);
														pthread_mutex_unlock(&mutex_reciqu);
														break;
												}
												case TYPE_T_RET_INFO:					//心跳
												{
														int zgb_addr=array[3]*256+array[4];
														int deviceNum = array[12];
														SetZigbeeAlive(zgb_addr,deviceNum);
														break;
												}
												default:
													break;
										}
										memset(array,0,128);
										memset(valid,0,100);
								}
						}
				}
		}
		return NULL;
}

void HexToStr(char *pbDest,unsigned char *pbSrc, int nLen)
{
		char ddl,ddh;
		int i;

		for (i=0; i<nLen; i++)
		{
				ddh = 48 + pbSrc[i] / 16;
				ddl = 48 + pbSrc[i] % 16;
				if (ddh > 57)
						ddh = ddh + 7;
				if (ddl > 57)
						ddl = ddl + 7;
				pbDest[i*2] = ddh;
				pbDest[i*2+1] = ddl;
		}
		pbDest[nLen*2] = '\0';
}
/**
 * 查询状态命令线程
 */
void * StatusThr()                   //查询状态命令线程
{
    	struct sockaddr_in cliaddr;
    	socklen_t cliaddr_len;
    	int listenfd, connfd,n;
    	char buf[MAXLINE];//数据传送的缓冲区

    	listenfd=socketInit(GET_PORT);
    	if(listenfd==0)
    	{
    			fprintf(stderr,"socket initialize failed!\n");
    			return NULL;
    	}
    	while (1)
    	{
    			cliaddr_len = sizeof(cliaddr);
    			connfd = accept(listenfd,(struct sockaddr *)&cliaddr, &cliaddr_len);
    			bzero(buf, MAXLINE);
    			n = read(connfd, buf, MAXLINE);
    			if (n == 0)
    			{
    					printf("the other side has been closed.\n//回收资源");
    					continue;
    			}
//	      printf("n:%d, %s\n",n,buf);
    			int status=searchStatus(buf);
		//   printf("status:%d\n",status);
    			char str[5];
    			if(status!=3)
    			{
    					sprintf(str,"%d",status);
    					write(connfd, str, 2);
    			}
    			close(connfd);
    	}
    	close(listenfd);
    	return NULL;
}
/**
 * 数据库同步线程
 */
void * Sqlite3Thr()
{
		struct sockaddr_in cliaddr;
		socklen_t cliaddr_len;
		int listenfd, connfd,n,rc;
		char buf[400];//数据传送的缓冲区
		char * errmsg;

		listenfd=socketInit(SQL_PORT);
		if(listenfd==0)
		{
				fprintf(stderr,"socket initialize failed!\n");
				return NULL;
		}

		while (1)
		{
				cliaddr_len = sizeof(cliaddr);
				connfd = accept(listenfd,(struct sockaddr *)&cliaddr, &cliaddr_len);
				bzero(buf, 400);
				n = read(connfd, buf, 400);
				if (n == 0)
				{
					printf("the other side has been closed.\n");
					continue;
				}
		//   printf("n:%d, %s\n",n,buf);
				pthread_mutex_lock(&mutex_mem_sqlite);
				rc	= sqlite3_exec(db,buf,NULL,NULL,&errmsg);
				pthread_mutex_unlock(&mutex_mem_sqlite);
				if(rc != SQLITE_OK)
				{
						printf("update sqlite3 error:%s\n",errmsg);
				}
				close(connfd);
		}
		close(listenfd);
		return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////
//参数说明:
//pInMemory: 指向内存数据库指针
//zFilename: 指向文件数据库目录的字符串指针
//isSave  0: 从文件数据库载入到内存数据库 1：从内存数据库备份到文件数据库
////////////////////////////////////////////////////////////////////////////////////////////

int loadOrSaveDb(sqlite3 *pInMemeory, const char *zFilename, int isSave)  //内存数据库导入导出
{
         int rc;
         sqlite3 *pFile;
         sqlite3_backup *pBackup;
         sqlite3 *pTo;
         sqlite3 *pFrom;

         rc = sqlite3_open(zFilename, &pFile);

         if(rc == SQLITE_OK)
         {
                   pFrom = (isSave?pInMemeory:pFile);
                   pTo = (isSave?pFile:pInMemeory);
                   pBackup = sqlite3_backup_init(pTo,"main",pFrom,"main");
                   if(pBackup)
                   {
                            (void)sqlite3_backup_step(pBackup,-1);
                            (void)sqlite3_backup_finish(pBackup);
                   }
                   rc = sqlite3_errcode(pTo);
         }

         (void)sqlite3_close(pFile);

         return rc;
}

static uint Crc32Table[] =
        {
            0x00000000,0x04C11DB7,0x09823B6E,0x0D4326D9,0x130476DC,0x17C56B6B,0x1A864DB2,0x1E475005,
            0x2608EDB8,0x22C9F00F,0x2F8AD6D6,0x2B4BCB61,0x350C9B64,0x31CD86D3,0x3C8EA00A,0x384FBDBD,
            0x4C11DB70,0x48D0C6C7,0x4593E01E,0x4152FDA9,0x5F15ADAC,0x5BD4B01B,0x569796C2,0x52568B75,
            0x6A1936C8,0x6ED82B7F,0x639B0DA6,0x675A1011,0x791D4014,0x7DDC5DA3,0x709F7B7A,0x745E66CD,
            0x9823B6E0,0x9CE2AB57,0x91A18D8E,0x95609039,0x8B27C03C,0x8FE6DD8B,0x82A5FB52,0x8664E6E5,
            0xBE2B5B58,0xBAEA46EF,0xB7A96036,0xB3687D81,0xAD2F2D84,0xA9EE3033,0xA4AD16EA,0xA06C0B5D,
            0xD4326D90,0xD0F37027,0xDDB056FE,0xD9714B49,0xC7361B4C,0xC3F706FB,0xCEB42022,0xCA753D95,
            0xF23A8028,0xF6FB9D9F,0xFBB8BB46,0xFF79A6F1,0xE13EF6F4,0xE5FFEB43,0xE8BCCD9A,0xEC7DD02D,
            0x34867077,0x30476DC0,0x3D044B19,0x39C556AE,0x278206AB,0x23431B1C,0x2E003DC5,0x2AC12072,
            0x128E9DCF,0x164F8078,0x1B0CA6A1,0x1FCDBB16,0x018AEB13,0x054BF6A4,0x0808D07D,0x0CC9CDCA,
            0x7897AB07,0x7C56B6B0,0x71159069,0x75D48DDE,0x6B93DDDB,0x6F52C06C,0x6211E6B5,0x66D0FB02,
            0x5E9F46BF,0x5A5E5B08,0x571D7DD1,0x53DC6066,0x4D9B3063,0x495A2DD4,0x44190B0D,0x40D816BA,
            0xACA5C697,0xA864DB20,0xA527FDF9,0xA1E6E04E,0xBFA1B04B,0xBB60ADFC,0xB6238B25,0xB2E29692,
            0x8AAD2B2F,0x8E6C3698,0x832F1041,0x87EE0DF6,0x99A95DF3,0x9D684044,0x902B669D,0x94EA7B2A,
            0xE0B41DE7,0xE4750050,0xE9362689,0xEDF73B3E,0xF3B06B3B,0xF771768C,0xFA325055,0xFEF34DE2,
            0xC6BCF05F,0xC27DEDE8,0xCF3ECB31,0xCBFFD686,0xD5B88683,0xD1799B34,0xDC3ABDED,0xD8FBA05A,
            0x690CE0EE,0x6DCDFD59,0x608EDB80,0x644FC637,0x7A089632,0x7EC98B85,0x738AAD5C,0x774BB0EB,
            0x4F040D56,0x4BC510E1,0x46863638,0x42472B8F,0x5C007B8A,0x58C1663D,0x558240E4,0x51435D53,
            0x251D3B9E,0x21DC2629,0x2C9F00F0,0x285E1D47,0x36194D42,0x32D850F5,0x3F9B762C,0x3B5A6B9B,
            0x0315D626,0x07D4CB91,0x0A97ED48,0x0E56F0FF,0x1011A0FA,0x14D0BD4D,0x19939B94,0x1D528623,
            0xF12F560E,0xF5EE4BB9,0xF8AD6D60,0xFC6C70D7,0xE22B20D2,0xE6EA3D65,0xEBA91BBC,0xEF68060B,
            0xD727BBB6,0xD3E6A601,0xDEA580D8,0xDA649D6F,0xC423CD6A,0xC0E2D0DD,0xCDA1F604,0xC960EBB3,
            0xBD3E8D7E,0xB9FF90C9,0xB4BCB610,0xB07DABA7,0xAE3AFBA2,0xAAFBE615,0xA7B8C0CC,0xA379DD7B,
            0x9B3660C6,0x9FF77D71,0x92B45BA8,0x9675461F,0x8832161A,0x8CF30BAD,0x81B02D74,0x857130C3,
            0x5D8A9099,0x594B8D2E,0x5408ABF7,0x50C9B640,0x4E8EE645,0x4A4FFBF2,0x470CDD2B,0x43CDC09C,
            0x7B827D21,0x7F436096,0x7200464F,0x76C15BF8,0x68860BFD,0x6C47164A,0x61043093,0x65C52D24,
            0x119B4BE9,0x155A565E,0x18197087,0x1CD86D30,0x029F3D35,0x065E2082,0x0B1D065B,0x0FDC1BEC,
            0x3793A651,0x3352BBE6,0x3E119D3F,0x3AD08088,0x2497D08D,0x2056CD3A,0x2D15EBE3,0x29D4F654,
            0xC5A92679,0xC1683BCE,0xCC2B1D17,0xC8EA00A0,0xD6AD50A5,0xD26C4D12,0xDF2F6BCB,0xDBEE767C,
            0xE3A1CBC1,0xE760D676,0xEA23F0AF,0xEEE2ED18,0xF0A5BD1D,0xF464A0AA,0xF9278673,0xFDE69BC4,
            0x89B8FD09,0x8D79E0BE,0x803AC667,0x84FBDBD0,0x9ABC8BD5,0x9E7D9662,0x933EB0BB,0x97FFAD0C,
            0xAFB010B1,0xAB710D06,0xA6322BDF,0xA2F33668,0xBCB4666D,0xB8757BDA,0xB5365D03,0xB1F740B4
        };
/**
 * 查表法
 */
uint CRC32Software(unsigned char * pData, int Length)
{
    	uint uCRCValue;
    	unsigned char chtemp;
    	int i,uSize;

    	uSize = Length/4;
    	uCRCValue = 0xFFFFFFFF;
    	while (uSize --)
    	{
    			for(i = 3;i >= 0;i--)
    			{
    					chtemp=*(pData + i);
    					uCRCValue = Crc32Table[((uCRCValue>>24)&0xFF) ^ chtemp] ^ (uCRCValue<<8);
    			}
    			pData += 4;
    	}
    	return uCRCValue;
}

/**
 * 发送心跳包
 */
void SetZigbeeAlive(int zgb_addr,int deviceNum)
{
		char  sql[256];
		int rc,count,status=1;
		char * errmsg;
		sqlite3_stmt * stmt;

		sprintf(sql,"select count(*) from phymapping where zigbee_addr=%d  ",zgb_addr);

		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if( rc != SQLITE_OK)
		{
				printf("set alive error:%s\n",errmsg);
				return;
		}

        if(sqlite3_step(stmt) == SQLITE_ROW)
        {
                count = sqlite3_column_int(stmt,0);
        }
        else
        {
        		return;
        }
        printf("count %d deviceNum %d \n",count,deviceNum);
        if(count != 0)
        {
        		sprintf(sql,"update phymapping set alive=datetime('now','localtime') where zigbee_addr=%d  ",zgb_addr);
				pthread_mutex_lock(&mutex_mem_sqlite);
        		rc = sqlite3_exec(db,sql,NULL,NULL,&errmsg);
				pthread_mutex_unlock(&mutex_mem_sqlite);
        		if(rc != SQLITE_OK)
        		{
        				printf("set alive error:%s\n",errmsg);
        				sqlite3_finalize(stmt);
        				return;
        		}
        }
        if(count == 0 || count < deviceNum )
        		status = 0;
        //回复心跳
        sendstr senddata;
        senddata.head[0] = 0x80;//head
        senddata.head[1] = 0xff;//commandid
        senddata.head[2] = 0xff;
        senddata.head[3] = mymac_addr[0];//from
        senddata.head[4] = mymac_addr[1];
        senddata.head[5] = zgb_addr/256;//to
        senddata.head[6] = zgb_addr%256;
        if(status == 0)
        {
        		senddata.head[7] = TYPE_G_REGISTERAGAIN;					//重新注册
        }
        else
        {
        		senddata.head[7]=0x99;//网关心跳
        }
        senddata.head[8] = 0x10;

        senddata.data[0] = 0x00;
        senddata.data[1] = 0x00;
        senddata.data[2] = 0x00;
        senddata.data[3] = 0x00;
        senddata.tail[0] = 0x00;
        senddata.tail[1] = 0x00;
        senddata.tail[2] = 0x00;
        senddata.tail[3] = 0x00;
        senddata.tail[4] = 0x40;
		sqlite3_finalize(stmt);
        pthread_mutex_lock(&mutex_queue);
        EnQueue(dataqu,&senddata);
        pthread_mutex_unlock(&mutex_queue);
}


/**
 * 定时命令线程
 */
void * CronThr()           //定时命令线程
{
		struct sockaddr_in cliaddr;
		socklen_t cliaddr_len;
		int listenfd, connfd,n,rc;
		char buf[MAXLINE];//数据传送的缓冲区
		char sql[256];
		sqlite3 * db2=NULL;
		sqlite3_stmt * stmt;
		listenfd = socketInit(CRON_PORT);
		int time = -1;
		if(listenfd == 0)
		{
				fprintf(stderr,"socket initialize failed!\n");
				return NULL;
		}
		if(signal(SIGALRM,wakeup)==SIG_ERR)
		{
				perror("signal");
		}
		else
		{
				printf("sigalrm is start\n");
		}

		rc = sqlite3_open("/opt/smarthome.db",&db2);
		if( rc != SQLITE_OK)
		{
				printf("fail:%s\n",sqlite3_errmsg(db2));
				return 0;
		}

		while (1)
		{
				//启动最先到达的定时任务
				sprintf(sql,"select (julianday(exctime)-julianday('now','localtime'))*24*60*60 from cron where "
						"status!=1  and (julianday(exctime)-julianday('now','localtime'))*24*60*60>=0 "
						"order by (julianday(exctime)-julianday('now','localtime'))*24*60*60");
				printf("sql = %s\n",sql);
				rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
				if(rc != SQLITE_OK)
				{
						fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
						sqlite3_close(db2);
						return 0 ;
				}
				if(sqlite3_step(stmt) == SQLITE_ROW)
				{
						time = sqlite3_column_int(stmt,0);
				}
			   if(time == 0 ||time == 1)
			   {
				   	   wakeup();
			   }
			   else if(time>1)
			   {
				   	   printf("time = %d seconds\n",time);
				   	   alarm(time-1);
			   }
				sqlite3_finalize(stmt);

				cliaddr_len = sizeof(cliaddr);
				connfd = accept(listenfd,(struct sockaddr *)&cliaddr, &cliaddr_len);
				bzero(buf, MAXLINE);
				n = read(connfd, buf, MAXLINE);
				if (n == 0)
				{
						printf("the other side has been closed.\n");
						continue;
				}
				printf("n:%d, %s\n",n,buf);
				int res =	TransferCommand(buf,connfd);
				if(res == 0)
				{
						write(connfd, "fail", 4);
				}
				else if(res == 1)
				{
						write(connfd,"success",7);
				}
				alarm(0);
				time = -1;
				//命令更新后，再次查询数据库
				//wakeup();
				close(connfd);
		}
		sqlite3_close(db2);
		return NULL;
}
/**
 * 转换命令
 * @return 0 失败
 * @return 1 成功
 * @return 2 查看命令
 */
int TransferCommand(char * origin,int connfd)
{
		int    result = 0,zigbee_addr = -1;
		int command_id,command_type,logic_id,action,mode,rc;
		char * errmsg;
		char sql[256];
		sqlite3 * db2=NULL;
		sqlite3_stmt * stmt;
		unsigned int value = 0;
		long int time;

		char  * command = (char *)malloc(strlen(origin)+1);
		strcpy(command,origin);
		char * token=strtok(command," ");
		if(token==NULL)
		{
				return result;
		}
		command_id = strtol(token,NULL,10);
		token = strtok( NULL, " ");
		if(token == NULL)
		{
				return result;
		}
		command_type = strtol(token,NULL,10);
		token = strtok(NULL," ");
		if(token == NULL)
		{
				return result;
		}
		logic_id = strtol(token,NULL,10);

		token = strtok(NULL," ");

		if(token == NULL)
		{
				return result;
		}
		action = strtol(token,NULL,10);

		rc = sqlite3_open("/opt/smarthome.db",&db2);
		if(rc  != SQLITE_OK)
		{
				printf("fail:%s\n",sqlite3_errmsg(db2));
				return 0;
		}
		//cron cancel 命令 或者  cron view命令
		//取消定时设定   cron_cancel
		if(action == CRON_CANCEL)
		{
			//	sprintf( sql,"delete from  cron  where status=1 and cron_id=%d",logic_id);
				sprintf( sql,"delete from  cron  where  cron_id=%d",logic_id);
				printf("sql = %s \n",sql);
				pthread_mutex_lock(&mutex_sqlite);
				rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
				pthread_mutex_unlock(&mutex_sqlite);
				if(rc == SQLITE_OK)
				{
						result = 1;
				}
				else
				{
						printf("delete cron error:%s\n",errmsg);
						result = 0;
				}
				sqlite3_close(db2);
				return result ;
		}
		//查看定时任务  cron_view
		else if(action == CRON_VIEW)
		{
				int cron_id,mode,route,cron_value;
				sprintf(sql,"select cron_id,mode,route,value  from cron where exctime>datetime('now','localtime') and status!=1 ");
				printf("sql = %s \n",sql);
				rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
				if(rc == SQLITE_OK)
				{
						while(sqlite3_step(stmt) == SQLITE_ROW)
						{
								cron_id = sqlite3_column_int(stmt,0);
								mode = sqlite3_column_int(stmt,1);
								route = sqlite3_column_int(stmt,2);
								cron_value = sqlite3_column_int(stmt,3);
								char  returnStr[100];
								sprintf(returnStr,"%d %d %d %d",cron_id,mode,route,cron_value);
								write(connfd, returnStr, strlen(returnStr));
								result = 2;
						}
				}
				else
				{
						fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
				}
				sqlite3_finalize(stmt);
				sqlite3_close(db2);
				return result;
		}

		//选择设备，设定时间，执行方式
		token = strtok(NULL," ");
		if(token == NULL)
		{
				sqlite3_close(db2);
				return result;
		}
		time = strtol(token,NULL,10);     //route  or subtype(scen,model)

		token = strtok(NULL," ");
		if(token == NULL)
		{
				sqlite3_close(db2);
				return result;
		}
		mode  = strtol(token,NULL,10);

		token = strtok(NULL," ");
		if(token != NULL)
				value = strtol(token,NULL,10);

		//选择设备
		sprintf( sql,"select zigbee_addr from phymapping  where logic_id=%d",logic_id);
		printf("sql = %s\n",sql);
		rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
				sqlite3_finalize(stmt);
				sqlite3_close(db2);
				return 0;
		}
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
		}
		if(zigbee_addr == -1)
		{
				printf("can't get zigbee_addr in TransferCommand\n ");
				sqlite3_finalize(stmt);
				sqlite3_close(db2);
				return 0;
		}
		sqlite3_finalize(stmt);

		printf("logic_id =%d zigbee_addr = %d\n",logic_id,zigbee_addr);
		//设定时间执行方式
		sprintf( sql,"insert into  cron (cron_id,mode,route,exctime,zigbee_addr,value,status) values (%d,%d,%d,datetime('now','localtime','+%d second'),%d,%d,0)"
				,logic_id,mode,action,time,zigbee_addr,value);
		printf("sql = %s\n",sql);
		pthread_mutex_lock(&mutex_sqlite);
		rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
		pthread_mutex_unlock(&mutex_sqlite);
		sqlite3_close(db2);
		return 1;
}
/**
 * 启动定时命令
 */
void wakeup()
{
		sqlite3 * db2 = NULL;
		char * errmsg;
	    int rc;
	    char sql[256];
	    sqlite3_stmt * stmt,*stmt1;
	    sendstr senddata;
		printf("sqlite3_open \n");
		rc = sqlite3_open("/opt/smarthome.db",&db2);
		if(rc != SQLITE_OK)
		{
				printf("fail:%s\n",sqlite3_errmsg(db2));
				return ;
		}

		sprintf(sql,"select (julianday(exctime)-julianday('now','localtime'))*24*60*60,id,zigbee_addr,route,value,subtype,mode  from cron where "
						"status!=1  and (julianday(exctime)-julianday('now','localtime'))>=0 "
						"order by (julianday(exctime)-julianday('now','localtime')) ");
		printf("sql_select_time = %s \n",sql);
		rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
				sqlite3_finalize(stmt);
				sqlite3_close(db2);
				return ;
		}
		int compare = 2147483647,time;
		while(sqlite3_step(stmt) == SQLITE_ROW)
		{
				 time = sqlite3_column_int(stmt,0);
				printf("time = %d ",time);
				if (time>compare)
				{
						break;
				}
				else
				{
					 compare = time;
				}
				int id = sqlite3_column_int(stmt,1);
				int zigbee_addr = sqlite3_column_int(stmt,2);
				int route = sqlite3_column_int(stmt,3);
				int value = sqlite3_column_int(stmt,4);
				int subtype = sqlite3_column_int(stmt,5);
				int mode = sqlite3_column_int(stmt,6);

				senddata.head[0] = 0x80;							//head
				senddata.head[1] = 0xef;							//commandid
				senddata.head[2] = id;
				senddata.head[3] = mymac_addr[0];		//from
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = zigbee_addr/256;	//to
				senddata.head[6] = zigbee_addr%256;
				senddata.head[7] = 0x11;							//type工作模式
				senddata.head[8] = 0x10;							//length

				senddata.data[0] = subtype;					//subtype
				senddata.data[1] = route;							//route
				senddata.data[2] = value/256;					//value
				senddata.data[3] = value%256;

				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;

				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);

				//每日执行
				if(mode == 1)
				{
						sprintf( sql,"update cron set exctime=datetime(exctime,'+1 day') where id=%d ",id);
						printf("sql_update = %s \n",sql);
						pthread_mutex_lock(&mutex_sqlite);
						rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
						pthread_mutex_unlock(&mutex_sqlite);
						if(rc != SQLITE_OK)
						{
								printf("query SQL error:%s\n",errmsg);
								sqlite3_finalize(stmt);
								sqlite3_close(db2);
								return ;
						}
				}
				//每周执行
				else if(mode == 2)
				{
						sprintf( sql,"update cron set exctime=datetime(exctime,'+7 day') where id=%d ",id);
						printf("sql_update = %s \n",sql);
						pthread_mutex_lock(&mutex_sqlite);
						rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
						pthread_mutex_unlock(&mutex_sqlite);
						if(rc != SQLITE_OK)
						{
								printf("query SQL error:%s\n",errmsg);
								sqlite3_finalize(stmt);
								sqlite3_close(db2);
								return ;
						}
				}
				//每月执行
				else if(mode ==3)
				{
						sprintf( sql,"update cron set exctime=datetime(exctime,'+1 month') where id=%d",id);
						printf("sql_update = %s \n",sql);
						pthread_mutex_lock(&mutex_sqlite);
						rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
						pthread_mutex_unlock(&mutex_sqlite);
						if(rc != SQLITE_OK)
						{
								printf("query SQL error:%s\n",errmsg);
								sqlite3_finalize(stmt);
								sqlite3_close(db2);
								return ;
						}
				}
				//仅执行一次  更新状态（暂时） 或者 删除
				else
				{
						sprintf(sql,"update cron set status=1 where id=%d",id);
						pthread_mutex_lock(&mutex_sqlite);
						rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
						pthread_mutex_unlock(&mutex_sqlite);
						if(rc != SQLITE_OK)
						{
								printf("query SQL error:%s\n",errmsg);
								sqlite3_finalize(stmt);
								sqlite3_close(db2);
								return ;
						}
				}
		}
		sqlite3_finalize(stmt);

		//启动下一个最先到达的任务
		sprintf(sql,"select (julianday(exctime)-julianday('now','localtime'))*24*60*60 from cron where "
							"status!=1  and (julianday(exctime)-julianday('now','localtime'))*24*60*60>=0 "
							"order by (julianday(exctime)-julianday('now','localtime'))*24*60*60");
		rc = sqlite3_prepare(db2,sql,-1,&stmt1,NULL);
		if(rc != SQLITE_OK)
		{
				fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
				return ;
		}
		//如果没有找到则退出
		if(sqlite3_step(stmt1) == SQLITE_ROW)
		{
				time = sqlite3_column_int(stmt1,0);
		}
		else
		{
				sqlite3_finalize(stmt1);
				sqlite3_close(db2);
				return;
		}

		if(time == 0 ||time == 1)
		{
//				sqlite3_finalize(stmt);
//				sqlite3_close(db2);
		   	   wakeup();
//		   	   return;
		 }
		 else if(time>1)
		  {
			 	 printf("time = %d seconds\n",time);
//			 	 sqlite3_finalize(stmt);
//			 	 sqlite3_close(db2);
			 	 alarm(time-1);
	//		 	 return;
		  }
		sqlite3_finalize(stmt1);
		sqlite3_close(db2);
		printf("sqlite3_close\n");
}

//	重新设定闹钟时间
//		sprintf(sql,"select (julianday(min(exctime))-julianday('now','localtime'))*24*60*60 as remain from cron where status!=1 ");
//		rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
//		if(rc != SQLITE_OK)
//		{
//				fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
//				return ;
//		}
//		while(sqlite3_step(stmt)==SQLITE_ROW)
//		{
//				int seconds = sqlite3_column_int(stmt,0);
//				alarm(seconds);
//				printf("next alarm %d\n",seconds);
//		}
//		sqlite3_finalize(stmt);

//void wakeup()
//{
//		sqlite3 * db2=NULL;
//		char * errmsg;database is locked
//	    int rc;
//	    char sql[256];
//	    sqlite3_stmt * stmt;
//	    sendstr senddata;
//		rc = sqlite3_open("/opt/smarthome.db",&db2);
//		if(rc)
//		{
//				printf("fail:%s\n",sqlite3_errmsg(db2));
//				return ;
//		}
//		else
//		{
//	    		printf("success open db2!\n");
//		}
//
//		sprintf(sql,"select id,zigbee_addr,route,value,type,subtype from cron where exctime<datetime('now','localtime') and status!=1 ");
//		rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
//		if(rc != SQLITE_OK)
//		{
//				fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
//				return ;
//		}
//
//		while(sqlite3_step(stmt) == SQLITE_ROW)
//		{
//				int id = sqlite3_column_int(stmt,0);
//				int zigbee_addr = sqlite3_column_int(stmt,1);
//				int route = sqlite3_column_int(stmt,2);
//				int value = sqlite3_column_int(stmt,3);
//				int subtype = sqlite3_column_int(stmt,5);
//
//				senddata.head[0] = 0x80;							//head
//				senddata.head[1] = 0xef;							//commandid
//				senddata.head[2] = id;
//				senddata.head[3] = mymac_addr[0];		//from
//				senddata.head[4] = mymac_addr[1];
//				senddata.head[5] = zigbee_addr/256;	//to
//				senddata.head[6] = zigbee_addr%256;
//				senddata.head[7] = 0x11;							//type工作模式
//				senddata.head[8] = 0x10;							//length
//
//				senddata.data[0] = subtype;					//subtype
//				senddata.data[1] = route;							//route
//				senddata.data[2] = value/256;					//value
//				senddata.data[3] = value%256;
//
//				senddata.tail[0] = 0x00;
//				senddata.tail[1] = 0x00;
//				senddata.tail[2] = 0x00;
//				senddata.tail[3] = 0x00;
//				senddata.tail[4] = 0x40;
//
//				pthread_mutex_lock(&mutex_queue);
//				EnQueue(dataqu,&senddata);
//				pthread_mutex_unlock(&mutex_queue);
//
//				sprintf(sql,"update cron set status=1 where id=%d and type=0 ",id);
//				rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
//				if(rc != SQLITE_OK)
//				{
//						printf("query SQL error:%s\n",errmsg);
//						return ;
//				}
//
//				sprintf( sql,"update cron set exctime=datetime(exctime,'+24 hour') where id=%d and type=1 ",id);
//				rc = sqlite3_exec(db2,sql,NULL,NULL,&errmsg);
//				if(rc != SQLITE_OK)
//				{
//						printf("query SQL error:%s\n",errmsg);
//						return ;
//				}
//		}
//		sqlite3_finalize(stmt);
//		//重新设定闹钟时间
//		sprintf(sql,"select (julianday(min(exctime))-julianday('now','localtime'))*24*60*60 as remain from cron where status!=1 ");
//		rc = sqlite3_prepare(db2,sql,-1,&stmt,NULL);
//		if(rc != SQLITE_OK)
//		{
//				fprintf(stderr,"cron:%s\n", sqlite3_errmsg(db2));
//				return ;
//		}
//		while(sqlite3_step(stmt)==SQLITE_ROW)
//		{
//				int seconds = sqlite3_column_int(stmt,0);
//				alarm(seconds);
//				printf("next alarm %d\n",seconds);
//		}
//		sqlite3_finalize(stmt);
//		sqlite3_close(db2);
//}

/**
 * 普通业务命令线程Node
 */
void * CommandThr()
{
    	struct sockaddr_in  cliaddr;//客户端网络地址结构体
    	socklen_t cliaddr_len;
    	int listenfd, connfd,err;

    	listenfd = socketInit(SERV_PORT);
    	if(listenfd==0)
    	{
    			fprintf(stderr,"socket initialize failed!\n");
    			return NULL;
    	}
    	pthread_mutex_init(&mutex,NULL);
    	while (1)
    	{
    			cliaddr_len = sizeof(cliaddr);
    			pthread_t UartTXThrId;//串口发送线程

    			pthread_mutex_lock(&mutex);
    			connfd = accept(listenfd,(struct sockaddr *)&cliaddr, &cliaddr_len);
    			if(connfd>=0)
    			{
    					if((err=pthread_create(&UartTXThrId,NULL,UartTXThr,&connfd))!=0)
    					{
    							printf(" create thread error:%s\n",strerror(err));
    							sleep(3);                                                //waiting 3s create again
    							close(connfd);
    							pthread_mutex_unlock(&mutex);
    							continue;
    					}
    			}
    	}
    	close(listenfd);
    	pthread_mutex_destroy(&mutex);
    	return NULL;
}
/**
 * 串口发送线程
 */
void * UartTXThr(void *args)  //串口发送线程   args=confdid
{
		int connfd = *((int*)args);
		pthread_mutex_unlock(&mutex);
		char buf[MAXLINE];//数据传送的缓冲区
		int n;
		sendstr senddata;

		pthread_detach(pthread_self());
		while(1)
		{
			bzero(buf, MAXLINE);
			n = recv(connfd, buf, MAXLINE,0);
			if (n<=0)
			{
				break;
			}
			if(n>0)
			{
				printf("n:%d, %s  connfd = %d\n",n,buf,connfd);
				senddata = TranSendCommand(buf);

				if(senddata.head[7]==204)
				{
					write(connfd, "success", 7);
					break;
				}

				if(senddata.head[0]=='\0'||senddata.data[0]=='\0'||senddata.tail[4]=='\0')
				{
					fprintf(stderr,"convert command error: %s\n",buf);
					write(connfd, "fail", 4);
					break;
				}
				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
				//		  int ack = senddata.data[2];
				if(senddata.head[7] == 0x12 )
				{
				//	PNode1 node;
					printf("flag0 \n");
					usleep(50000);
					int t = 0;
					//等待1s时间，判断队列是否为空
					while(IsEmptyQueue(recvqu)&&t<20)
					{
							usleep(50000);
							t++;
					}
					//如果不为空则搜索相应的值，没取到则等待500ms，
					while( !IsEmptyQueue(recvqu))
					{
								printf("flag1 \n");
								//	node = recvqu->front;
								pthread_mutex_lock(&mutex_reciqu);
								int value = FindQueueValue(recvqu,&senddata);
								pthread_mutex_unlock(&mutex_reciqu);
								//int value = node->data->data[2]*256*256*256 + node->data->data[3]*256*256 +node->data->data[4]*256 +node->data->data[5];
								if (value == -1)
								{
									if(++t<30)
									{
											usleep(50000);
    										continue;
									}
									write(connfd, "fail", 4);
									break;
								}
								else
								{
									    t = 21;
										char  str[10];
										sprintf(str,"%d",value);
										printf("str = %s len = %d \n",str,strlen(str));
										write(connfd, str, strlen(str));
										break;
										//	DeleteQueueNode(recvqu,&senddata);
								}
						}
						if(t == 20)
						{
								write(connfd, "fail", 4);
						}
				}
				else
				{
						write(connfd, "success", 7);
				}
				break;
			}
		}
		close(connfd);
		pthread_exit(NULL);
}

//			pthread_t ConfirmThrId;
//			err = pthread_create(&ConfirmThrId,NULL,ConfirmThr,&senddata);
//					sleep(1);
//				if(err != 0)
//				{
//				    printf("cannot create thread %s\n",strerror(err));
//				}
//			  int commandid = senddata.head[1]*256+senddata.head[2];
//			  int zigbee_addr = senddata.head[5]*256+senddata.head[6];
//			  int route = senddata.data[1];
//			  int value = 1;
//			  if(route == 19)
//			  {
//				  value=senddata.data[3]*256*256+senddata.data[4]*256+senddata.data[5];    //status
//			  }
//			  else if(route ==	20)
//			  {
//				  value = senddata.data[3];    //status
//			  }
//			  if(UpdateStatus(zigbee_addr,commandid,route,value))
//			  {
//				  write(connfd, "success", 7);
//			  }
//			  else
//			  {
//				  write(connfd, "fail", 4);
//			  }

/**
 * 将命令转换为符合协议16进制数组
 */
sendstr TranSendCommand(char * origin)
{
		sendstr result;
		int commandid,lgcid,action,ack = 0;
		unsigned int value = 0;
		int command_type,type,rc,res;
		sqlite3_stmt * stmt;
		char  sql[256];

		char * command = (char *)malloc(strlen(origin)+1);
		strcpy(command,origin);

		char * token = strtok(command," ");
		if(token==NULL)
		{
				return result;
		}
		commandid = strtol(token,NULL,10);
		token = strtok( NULL, " ");
		if(token==NULL)
		{
				return result;
		}
		command_type = strtol(token,NULL,10);
		token = strtok(NULL," ");
		if(token==NULL)
		{
				return result;
		}
		lgcid = strtol(token,NULL,10);

		token = strtok(NULL," ");
		if(token==NULL)
		{
				return result;
		}
		action = strtol(token,NULL,10);     //route  or subtype(scen,model)

		if(command_type == 6)
		{
				ack	= 1;
		}

		token = strtok(NULL," ");
		if(token != NULL)
		{
				if(action==113 || action == 1)
				{
						char *tokstr;
						tokstr = strtok(token,",");
						while(tokstr !=NULL)
						{
								value = value *256 + strtol(tokstr,NULL,16);
								tokstr = strtok(NULL,",");
						}
				}
				else
				{
						unsigned int val1,val2,val3;
						char *strp;
						strp = strstr(token,",");
						if(strp != NULL && strstr(strp+1,",") != NULL)
						{
								sscanf(token,"%x,%x,%x",&val1,&val2,&val3);
								value = val1*256*256+val2*256+val3;
						}
						else if(strp != NULL)
						{
								sscanf( token,"%x,%x",&val1,&val2 );
								value = val1*256*256 + val2*256 ;
						}
						else
						{
								value = strtol(token,NULL,10);
						}
				}
		}
		sprintf(sql,"select type from phymapping where logic_id=%d ",lgcid);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return result;
		}
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
				type = sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);
		//情景模式
		if( action == 204)
		{
				res = sceneCommand(lgcid,commandid,action,value,ack);
				if(res == 1)
				{
						result.head[7] = 204;
				}
				return result;
		}
		else  if(action == 99)   //清除开关的buff
		{
				res = clearBuffCommand(lgcid,commandid,action,type);
				if(res==1)
				{
						result.head[7] = 204;
				}
				return result;
		}

		switch(type)
		{
				case SUBTYPE_SWITCH:								//封装开关命令
				{
						result = switchCommand(lgcid,commandid,action,value);
						break;
				}
				case SUBTYPE_YW_LIGHT:						//封装YW灯命令
				{
						result = YWModuleCommand(lgcid,commandid,action,value,ack);
						break;
				}
				case  SUBTYPE_RGB_LIGHT:						//封装RGB灯命令
				{
						result = RGBModuleCommand(lgcid,commandid,action,value,ack);
						break;
				}
				case SUBTYPE_CURTAIN:						  //封装窗帘命令
				{
						result = CurtainCommand(lgcid,commandid,action,value);
						break;
				}
				case SUBTYPE_SOCKET:								//封装插座命令
				{
						result = SocketCommand(lgcid,commandid,action,value);
						break;
				}
				case	SUBTYPE_INFRARED_CONTROL:  	//封装红外命令
				{
						result = infraredCommand(lgcid,commandid,action);
						break;
				}
				case SUBTYPE_LIGHT_INTENSITY:					//封装光照度命令
				{
						result = LightIntensityCommand(lgcid,commandid,action,value,ack);
						break;
				}
				case SUBTYPE_HUMIDITY:									//封装湿度命令
				{
						result = HumidityCommand(lgcid,commandid,action,value,ack);
						break;
				}
				case SUBTYPE_TEMPERATURE:								//封装温度命令
				{
						result = TemperatureCommand(lgcid,commandid,action,value,ack);
						break;
				}
				case SUBTYPE_TEMP_HUM:											//封装温湿度命令
				{
						result = Tem_Hum_Command(lgcid,commandid,action,value,ack);
						break;
				}
				case SUBTYPE_485_AC:													//封装485命令
				{
					      result = AC_485_Command(lgcid,commandid,action,ack);
						  break;
				}
			//	case
				default:
						break;
			}
		free(command);
		return result;
}
/**
 * 清除开关的buff
 */
int clearBuffCommand(int logicId,int commandId,int action,int type)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc,err;
		pthread_t ConfirmThrId,ConfirmThrId1;
		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicId);

		rc	 = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
			printf("prepare error :%d\n",rc);
			return 0;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt) == SQLITE_ROW	)
		{
			zigbee_addr = sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr == 0)
		{
				printf("can't found the right record!\n");
				return 0;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandId/256;//commandid
		command.head[2] = commandId%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = 0xF1;//type
		command.head[8] = 0x13;//length

		command.data[0] = type;//subtype
		command.data[1] = 0;//route
		command.data[2] = 0;//value
		command.data[3] = 0xFF;//value
		command.data[4] = 0xFF;//value
		command.data[5] = 0xFF;
		command.data[6] = 0xFF;//value

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;

		pthread_mutex_lock(&mutex_queue);
		EnQueue(dataqu,&command);
		pthread_mutex_unlock(&mutex_queue);

		err = pthread_create(&ConfirmThrId,NULL,ConfirmThr,&command);
		if(err != 0)
		{
				printf("cannot create thread %s\n",strerror(err));
		}
		usleep(50000);
		command.head[7] = 0xFF;//type
		pthread_mutex_lock(&mutex_queue);
		EnQueue(dataqu,&command);
		pthread_mutex_unlock(&mutex_queue);
		err = pthread_create(&ConfirmThrId1,NULL,ConfirmThr,&command);
		if(err != 0)
		{
				printf("cannot create thread %s\n",strerror(err));
		}
		usleep(50000);
		return 1;
}
/**
 * AC_485_Command
 */
sendstr AC_485_Command(int logicid,int commandid,int route,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select A.zigbee_addr,B.value from phymapping A,device485 B where A.logic_id=%d and A.type=B.devicetype and B.code=%d ",logicid,route);
		printf(" sql = %s \n",sql);
		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr = 1;
		const unsigned char * value;
		if(sqlite3_step(stmt) == SQLITE_ROW	)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				value = sqlite3_column_text(stmt,1);
		}
		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		printf("value = %s\n",value);
		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] =  zigbee_addr%256;
		command.head[7] =  TYPE_G_RUN  ;//type
		command.head[8] = 14+strlen(value)/2;//length
		command.data[0]	 =	SUBTYPE_485_AC;//subtype
		command.data[1] = 0x00;//
		char  dest[2];
		int i,j;
		for(i=0,j=2;i<strlen(value);i=i+2,j++)
		{
			strncpy(dest,value+i,2);
			command.data[j] = strtol(dest,NULL,16);
			printf(" %c%c ",dest[0],dest[1]);
		    printf(" %x ",command.data[j]);
		}
		printf("\n");
		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		sqlite3_finalize(stmt);
		return command;
}

/**
 *温湿度命令封装发送
 *  */
sendstr Tem_Hum_Command(int logicid,int commandid,int route,int value,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr=0;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
				zigbee_addr=sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] =TYPE_G_ASK_INFO;//type
		command.head[8] = 0x10;//length

		command.data[0] = SUBTYPE_TEMP_HUM;//subtype
		command.data[1] = route;
		command.data[2] = value/256;	//value
		command.data[3] =  value%256;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		return command;
}
/**
 *湿度命令封装发送
 *  */
sendstr HumidityCommand(int logicid,int commandid,int route,int value,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
				zigbee_addr=sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] =TYPE_G_ASK_INFO;//type
		command.head[8] = 0x10;//length

		command.data[0] = SUBTYPE_HUMIDITY;//subtype
		command.data[1] = route;
		command.data[2] = value/256;	//value
		command.data[3] =  value%256;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		return command;
}

/**
 * 温度命令封装发送
 *  */
sendstr TemperatureCommand(int logicid,int commandid,int route,int value,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
			printf("prepare error :%d\n",rc);
			return command;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
			zigbee_addr = sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_ASK_INFO;//type
		command.head[8] = 0x10;//length

		command.data[0] = SUBTYPE_TEMPERATURE;//subtype
		command.data[1] = route;
		command.data[2] = value/256;	//value
		command.data[3] =  value%256;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		return command;
}

/**
 * 光照强度命令封装
 */
sendstr LightIntensityCommand(int logicid,int commandid,int route,int value,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);

		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
			printf("prepare error :%d\n",rc);
			return command;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
			zigbee_addr = sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] =TYPE_G_ASK_INFO;//type
		command.head[8] = 0x10;//length

		command.data[0] = SUBTYPE_LIGHT_INTENSITY;//subtype
		command.data[1] = route;
		command.data[2] = value/256;	//value
		command.data[3] =  value%256;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		return command;
}

/**
 * YWModule
 */
sendstr YWModuleCommand(int logicid,int commandid,int route,int value,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);

		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr=0;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
				zigbee_addr=sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_RUN;//type
		command.head[8] = 0x12;//length
		command.data[0] = SUBTYPE_YW_LIGHT;//subtype
		command.data[1] = route;//route
		command.data[2] = ack;//value
		command.data[3] = value%256;//value
		command.data[4] = 0xFF;//value
		command.data[5] = 0xFF;//value
		printf("value = %x \n"	,command.data[3]);
		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		return command;
}
/**
 * RGBModule
 */
sendstr RGBModuleCommand(int logicid,int commandid,int route,int value,int ack)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt)==SQLITE_ROW)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_RUN;//type
		command.head[8] = 0x13;//length

		command.data[0] = SUBTYPE_RGB_LIGHT;							//subtype
		command.data[1] = (unsigned char)route;						//route
		command.data[2] = (unsigned char)ack;							//value

		if(route == 0x65)
		{
				command.data[3] = value%256;										//value
				command.data[4] = 0xFF;
				command.data[5] = 0xFF;												//value
		}
		else
		{
				command.data[3] = value/(256*256);										//value
				command.data[4] = (value%(256*256))/256;
				command.data[5] = value%256;												//value
		}
		command.data[6] = 0xFF;															//value

		printf("value = %x %x %x \n",command.data[3],command.data[4],command.data[5]);
		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		return command;
}

/**
 * 开关命令封装发送
 */
sendstr switchCommand(int logicid,int commandid,int action,int value)   //开关命令
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

//	sprintf(sql,"select zigbee_addr,route from phymapping where logic_id=%d and type=%d ",logicid,type);
		sprintf(sql,"select zigbee_addr,route from phymapping where logic_id=%d ",logicid);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr = 0,route;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				route = sqlite3_column_int(stmt,1);
		}

		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				sqlite3_finalize(stmt);
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_RUN;//type
		command.head[8] = 0x10;//length

		command.data[0]  = SUBTYPE_SWITCH; //subtype
		command.data[1] = action; //route
		command.data[2] = 0;//value
		command.data[3] = route;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		sqlite3_finalize(stmt);
		return command;
}
/**
 * 插座命令封装发送
 */
sendstr SocketCommand(int logic_id,int commandid,int action,int value)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select zigbee_addr  from phymapping where logic_id=%d ",logic_id);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
		}
		if(zigbee_addr == 0)
		{
				printf("CurtainCommand can't found the right record!\n");
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_RUN;//type
		command.head[8] = 0x10;//length

		command.data[0] = SUBTYPE_SOCKET;//subtype
		command.data[1] = action;//route
		command.data[2] =  1;	//value
		command.data[3] =  value%256;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		sqlite3_finalize(stmt);
		return command;
}

/**
 * 窗帘消息封装发送
 */
sendstr CurtainCommand(int logic_id,int commandid,int action,int value)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc,route=1;

		sprintf(sql,"select zigbee_addr route from phymapping where logic_id=%d ",logic_id);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr = 0;
		if(sqlite3_step(stmt) == SQLITE_ROW)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				route = sqlite3_column_int(stmt,1);
		}
		if(zigbee_addr == 0)
		{
				printf("CurtainCommand can't found the right record!\n");
				sqlite3_finalize(stmt);
				return command;
		}

		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_RUN;//type
		command.head[8] = 0x10;//length

		command.data[0] = SUBTYPE_CURTAIN;//subtype
		command.data[1] = action;//route
		command.data[2] =  0;	//value
		command.data[3] = route;

		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;
		sqlite3_finalize(stmt);
		return command;
}
/**
 * 红外命令封装
 */
sendstr infraredCommand(int logicid,int commandid,int type)
{
		sendstr command;
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;

		sprintf(sql,"select A.zigbee_addr,B.value from phymapping A,irctrl B where A.logic_id=%d and A.type=B.devicetype and B.code=%d ",logicid,type);
		//sprintf(sql,"select A.zigbee_addr,B.value  from phymapping A,irctrl B where A.logic_id=B.deviceType and A.logic_id=%d and B.code=%d ",logicid,type);
		printf(" sql = %s \n",sql);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return command;
		}
		int zigbee_addr=1;
		const unsigned char * value;
		if(sqlite3_step(stmt)==SQLITE_ROW	)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				value = sqlite3_column_text(stmt,1);
		}
		if(zigbee_addr==0)
		{
				printf("can't found the right record!\n");
				sqlite3_finalize(stmt);
				return command;
		}

		printf("value = %s\n",value);
//		printf("value = %d\n",strlen(value));
		command.head[0] = 0x80;//head
		command.head[1] = commandid/256;//commandid
		command.head[2] = commandid%256;
		command.head[3] = mymac_addr[0];//from
		command.head[4] = mymac_addr[1];
		command.head[5] = zigbee_addr/256;//to
		command.head[6] = zigbee_addr%256;
		command.head[7] = TYPE_G_RUN;//type
		command.head[8] = 14+strlen(value)/2;//length

		command.data[0]=SUBTYPE_INFRARED_CONTROL;//subtype
		command.data[1]=0x00;//

		char  dest[2];
		int i,j;

		for(i=0,j=2;i<strlen(value);i=i+2,j++)
		{
				strncpy(dest,value+i,2);
				command.data[j] = strtol(dest,NULL,16);
				printf(" %c%c ",dest[0],dest[1]);
				printf(" %x ",command.data[j]);
		}
		printf("\n");
		command.tail[0] = 0x00;
		command.tail[1] = 0x00;
		command.tail[2] = 0x00;
		command.tail[3] = 0x00;
		command.tail[4] = 0x40;

		sqlite3_finalize(stmt);
		return command;
}
/**
 * 发送开关映射关系
 */
void sendSwitchMapping(int commandid,int destzigbee,int phy_addr,char mac_addr[],int flag)
{
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;
		sendstr senddata;

		if(flag==1)
		{
				sprintf(sql,"select route,key from phymapping where phy_addr=%d ",phy_addr);
		}
		else
		{
				sprintf(sql,"select route,key from phymapping where mac_addr='%s' ",mac_addr);
		}

		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				fprintf(stderr,"phymapping:%s\n", sqlite3_errmsg(db));
				return;
		}

		senddata.head[0]=0x80;
		senddata.head[1]=commandid/256;
		senddata.head[2]=commandid%256;
		senddata.head[3]=mymac_addr[0];
		senddata.head[4]=mymac_addr[1];
		senddata.head[5]=destzigbee/256;
		senddata.head[6]=destzigbee%256;
		senddata.head[7]=0x17;
		senddata.head[8]=0x10;

		senddata.data[0]=0x00;
		senddata.data[1]=0x00;
		int route,key,count=0;
		while(sqlite3_step(stmt)==SQLITE_ROW)
		{
				route = sqlite3_column_int(stmt,0);
				key = sqlite3_column_int(stmt,1);
				if( key == 0 )
				{
						senddata.data[1 + ++count] = route;//value
						senddata.data[1 + ++count] = key;
						senddata.data[1 + ++count] = 0xFF;
						senddata.head[8] = 14 + count;
						continue;
				}

				int i = 10,len=1;
				while(key/i>=1)
				{
						i = i * 10;
						len++;
				}
				i = i/10;
				senddata.data[1 + ++count]=route;//value
				while(len>0)
				{
						senddata.data[1 + ++count]=(key%(i*10))/i;
						i = i/10;
						len--;
				}
				senddata.data[1 + ++count]=0xFF;
				senddata.head[8] = 14 + count;

//		senddata.head[8]=0x10+2*count;
//		senddata.data[2+2*count]=route;//value
//		senddata.data[3+2*count]=key;
//		count++;
		}
		senddata.tail[0]=0x00;
		senddata.tail[1]=0x00;
		senddata.tail[2]=0x00;
		senddata.tail[3]=0x00;
		senddata.tail[4]=0x40;

		if(count>0)
		{
				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
		}
		else
		{
				printf("send SwitchMaping fail\n");
		}
		sqlite3_finalize(stmt);
}
/**
 * 搜索状态线程
 */
int searchStatus(char buf[])
{
		char * token = strtok(buf," ");
		if(token == NULL)
		{
				return -1;
		}

		token = strtok( NULL, " ");
		if(token==NULL)
		{
			 	 return -1;
		}

		token = strtok(NULL," ");
		if(token==NULL)
		{
				return -1;
		}
		int lgcid = strtol(token,NULL,10);

		token = strtok( NULL, " ");
		if(token==NULL)
		{
				return -1;
		}
		int action = atoi(token);

		sqlite3_stmt * stmt;
		char  sql[256];
		int rc, status=-1;
	//	char * errmsg;
		if(action==0x16)
		{
				sprintf(sql,"select 1 from phymapping where logic_id=%d and alive>datetime('now','localtime','-2 minute') ",lgcid);
		}
		else if(action==70)
		{
				updateMapping(lgcid);
				status =3;
				return status;
		}
		else
		{
				sprintf(sql,"select status from phymapping where logic_id=%d ",lgcid);
		}
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				printf("prepare error :%d\n",rc);
				return -1;
		}

		if(sqlite3_step(stmt)==SQLITE_ROW)
		{
				status=sqlite3_column_int(stmt,0);
		}
		sqlite3_finalize(stmt);

		return status;
}

/**
 * 网关映射关系改变，更新logic_id的Mapping
 */
void updateMapping(int logic_id)
{
    	sqlite3_stmt * stmt;
		char  sql[256];
		int rc;
		sendstr senddata;
		int zigbee_addr=0,route,key;
//		int err;

		sprintf(sql,"select zigbee_addr,route,key from phymapping  where logic_id=%d ",logic_id);
		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				fprintf(stderr,"%s\n", sqlite3_errmsg(db));
		}

		if(sqlite3_step(stmt) == SQLITE_ROW	)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				route = sqlite3_column_int(stmt,1);
				key = sqlite3_column_int(stmt,2);

				if(zigbee_addr==0)
				{
					printf("can't found the right record!\n");
				}
				else
				{
					senddata.head[0] = 0x80;//head
					senddata.head[1] = 0xee;//commandid
					senddata.head[2] = commandnum%256;
					senddata.head[3] = mymac_addr[0];//from
					senddata.head[4] = mymac_addr[1];
					senddata.head[5] = zigbee_addr/256;//to
					senddata.head[6] = zigbee_addr%256;
					senddata.head[7] = TYPE_G_RUN;//type工作模式
					senddata.head[8] = 0x10;//length

					senddata.data[0] =  SUBTYPE_SWITCH;//subtype
					senddata.data[1] = 0x03;//route
			//		senddata.data[2] = key;//value
					int count=0;
					if( key == 0 )
					{
							senddata.data[ 1+ ++count] = route;//value
							senddata.data[ 1+ ++count] = key;
							printf(" %x",senddata.data[1+count]);
							senddata.head[8] = 14 + count;
					}
					else
					{
							int i = 10,len=1;
							while(key/i>=1)
							{
									i = i * 10;
									len++;
							}
							i = i/10;
							printf("ken =  %d len = %d  i = %d",key,len,i);
							senddata.data[1+ ++count]=route;//value
							while(len>0)
							{
									senddata.data[1+  ++count]=(key%(i*10))/i;
									printf(" %x ",senddata.data[1+count]);
									i = i/10;
									len--;
							}
							senddata.head[8] = 14 + count;
					}

					senddata.tail[0] = 0x00;
					senddata.tail[1] = 0x00;
					senddata.tail[2] = 0x00;
					senddata.tail[3] = 0x00;
					senddata.tail[4] = 0x40;

					pthread_mutex_lock(&mutex_queue);
					EnQueue(dataqu,&senddata);
					pthread_mutex_unlock(&mutex_queue);
//			commandnum++;
//			pthread_t ConfirmThrId;
//			err = pthread_create(&ConfirmThrId,NULL,ConfirmThr,&senddata);
//
//			if(err != 0)
//			{
//			    printf("cannot create thread %s\n",strerror(err));
//			}
//	        usleep(30000);
				}
		}
		sqlite3_finalize(stmt);
}

void answerRegister(int commandid,int zigbee_addr,int phy_addr,char mac_addr[],int flag,int type)
{
		sqlite3_stmt * stmt;
		char  sql[256];
		int rc;
		sendstr senddata;
//	if(flag==1)
//	{
//		sprintf(sql,"select * from phymapping where phy_addr=%d ",phy_addr);
//	}
//	else
//	{
		sprintf(sql,"select type from phymapping where mac_addr='%s' ",mac_addr);
//	}
		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				fprintf(stderr,"phymapping:%s\n", sqlite3_errmsg(db));
				return;
		}
		int compareType =  -1;
	 //查看是否注册过，如果注册过则发送注册命令
		while(sqlite3_step(stmt)==SQLITE_ROW)
		{
				compareType =  sqlite3_column_int(stmt,0);
				if(compareType == type)
			    break;
		}
		//注册过
		if(compareType == type)
		{
				senddata.head[0]=0x80;
				senddata.head[1]=commandid/256;//commandid
				senddata.head[2]=commandid%256;
				senddata.head[3]=mymac_addr[0];//from
				senddata.head[4]=mymac_addr[1];
				senddata.head[5]=zigbee_addr/256;//to
				senddata.head[6]=zigbee_addr%256;
				senddata.head[7]=0x02;
				senddata.head[8]=0x10;
				senddata.data[0]=type;
				senddata.data[1]=0x00;
				senddata.data[2]=0x00;
				senddata.data[3]=0x00;
				senddata.tail[0]=0x00;
				senddata.tail[1]=0x00;
				senddata.tail[2]=0x00;
				senddata.tail[3]=0x00;
				senddata.tail[4]=0x40;

				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
				sqlite3_finalize(stmt);
		}
		else
		{
				printf("insert query\n");
				sqlite3_stmt * stmt1;
				char *errmsg;
			  //查询最大的logic_id
				char sql_register[200] = "select logic_id from phymapping order by logic_id desc";
				char sql_insert[400];
				int logic_id;
				rc = sqlite3_prepare(db,sql_register,-1,&stmt1,NULL);
				if(rc != SQLITE_OK)
				{
						fprintf(stderr,"phymapping:%s\n", sqlite3_errmsg(db));
						return;
				}
				//如果数据库里有值取最大的  如果没有值logic_id为1
				if(sqlite3_step(stmt1)==SQLITE_ROW)//while
				{
						logic_id = sqlite3_column_int(stmt1,0);
						logic_id++;
				}
				else
				{
						logic_id = 1;
				}
				sqlite3_finalize(stmt1);
				printf("logic_id = %d \n",logic_id);
				//insert smarthome.db
				sqlite3 * db2 = NULL;
				rc = sqlite3_open("/opt/smarthome.db",&db2);
				if(rc != SQLITE_OK)
				{
						printf("fail:%s\n",sqlite3_errmsg(db2));
						sqlite3_close(db2);
						return ;
				}
				//设置事务操作
				rc = sqlite3_exec(db2,"BEGIN",NULL,NULL,&errmsg);
				if(rc != SQLITE_OK)
				{
						printf("BEGIN:%s\n",errmsg);
						sqlite3_close(db2);
						return ;
				}
				rc = sqlite3_exec(db,"BEGIN",NULL,NULL,&errmsg);
				if(rc != SQLITE_OK)
				{
								printf("BEGIN:%s\n",errmsg);
								return ;
				}

				//开关注册需要注册6个数据
				if(type == SUBTYPE_SWITCH)
				{
						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr,route,key) values "
								"(%d,%d,%d,'%s',1,1)",logic_id,type,zigbee_addr,mac_addr);
						rc=sqlite3_exec(db2,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						rc=sqlite3_exec(db,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert memory sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr,route,key) values "
										"(%d,%d,%d,'%s',2,2)",++logic_id,type,zigbee_addr,mac_addr);
						rc=sqlite3_exec(db2,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						rc=sqlite3_exec(db,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert memory sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr,route,key) values "
										"(%d,%d,%d,'%s',3,3)",++logic_id,type,zigbee_addr,mac_addr);
						rc=sqlite3_exec(db2,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						rc=sqlite3_exec(db,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert memory sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr,route,key) values "
										"(%d,%d,%d,'%s',4,4)",++logic_id,type,zigbee_addr,mac_addr);
						rc=sqlite3_exec(db2,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						rc=sqlite3_exec(db,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert memory sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr,route,key) values "
										"(%d,%d,%d,'%s',5,0)",++logic_id,type,zigbee_addr,mac_addr);
						rc=sqlite3_exec(db2,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}
						rc=sqlite3_exec(db,sql_insert,NULL,NULL,&errmsg);
						if(rc!=SQLITE_OK)
						{
								printf("insert memory sqlite3 error:%s\n",errmsg);
								sqlite3_close(db2);
								return ;
						}

						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr,route,key) values "
												"(%d,%d,%d,'%s',6,0)",++logic_id,type,zigbee_addr,mac_addr);
					}
					else
					{
						sprintf(sql_insert,"insert into  phymapping (logic_id,type,zigbee_addr,mac_addr) values "
										"(%d,%d,%d,'%s')",logic_id,type,zigbee_addr,mac_addr);
					}

				    printf("sql_insert =%s \n",sql_insert);
					rc = sqlite3_exec(db,sql_insert,NULL,NULL,&errmsg);
					if(rc != SQLITE_OK)
					{
							printf("insert memory sqlite3 error:%s\n",errmsg);
							sqlite3_close(db2);
							return ;
					}

					rc=sqlite3_exec(db2,sql_insert,NULL,NULL,&errmsg);
					if(rc==SQLITE_OK)
					{
							pthread_mutex_lock(&mutex_sqlite);
							rc = sqlite3_exec(db2,"COMMIT",NULL,NULL,&errmsg);
							pthread_mutex_unlock(&mutex_sqlite);
							if(rc!=SQLITE_OK)
							{
									printf("ret = %d,COMMIT:%s\n",rc,errmsg);
									sqlite3_close(db2);
									return;
							}
							pthread_mutex_lock(&mutex_mem_sqlite);
							rc = sqlite3_exec(db,"COMMIT",NULL,NULL,&errmsg);
							pthread_mutex_unlock(&mutex_mem_sqlite);
							if(rc!=SQLITE_OK)
							{
									printf("ret = %d,COMMIT:%s\n",rc,errmsg);
									return;
							}

							senddata.head[0]=0x80;
							senddata.head[1]=commandid/256;//commandid
							senddata.head[2]=commandid%256;
							senddata.head[3]=mymac_addr[0];//from
							senddata.head[4]=mymac_addr[1];
							senddata.head[5]=zigbee_addr/256;//to
							senddata.head[6]=zigbee_addr%256;
							senddata.head[7]=0x02;
							senddata.head[8]=0x10;

							senddata.data[0]=type;
							senddata.data[1]=0x00;
							senddata.data[2]=0x00;
							senddata.data[3]=0x00;

							senddata.tail[0]=0x00;
							senddata.tail[1]=0x00;
							senddata.tail[2]=0x00;
							senddata.tail[3]=0x00;
							senddata.tail[4]=0x40;

							pthread_mutex_lock(&mutex_queue);
							EnQueue(dataqu,&senddata);
							pthread_mutex_unlock(&mutex_queue);
					}
					else
					{
							printf("insert sqlite3 error:%s\n",errmsg);
							rc = sqlite3_exec(db2,"ROLLBACK",NULL,NULL,&errmsg);
							if(rc!=SQLITE_OK)
							{
									printf("ret = %d,ROLLBACK: %s\n",rc,errmsg);
							}
					}
					sqlite3_close(db2);
		}
}
/**
 * 情景模式命令
 */
int sceneCommand(int logic_id,int commandid,int route,int value1,int ack)
{
		sqlite3_stmt * stmt,* stmt1;
		char  sql[256];
		int rc,err;
		sendstr senddata;
		int zigbee_addr=0,route_scen,value,subtype=0;

		sprintf(sql,"select zigbee_addr,route,value from scen  where scen_id=%d ",logic_id);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc != SQLITE_OK)
		{
				fprintf(stderr,"%s\n", sqlite3_errmsg(db));
				return 0;
		}

		while(sqlite3_step(stmt) == SQLITE_ROW	)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				route_scen = sqlite3_column_int(stmt,1);
				value = sqlite3_column_int(stmt,2);

				if(zigbee_addr==0)
				{
						printf("can't found the right record!\n");
						continue;
				}
				else
				{
						sprintf(sql,"select type from phymapping where zigbee_addr=%d ",zigbee_addr);
						rc = sqlite3_prepare(db,sql,-1,&stmt1,NULL);
						if(rc != SQLITE_OK)
						{
								printf("select type error\n");
						}
						if(sqlite3_step(stmt1)==SQLITE_ROW)
						{
								subtype = sqlite3_column_int(stmt1,0);
						}
						sqlite3_finalize(stmt1);
				}

				if(subtype == 0)
				{
						continue;
				}

				senddata.head[0] = 0x80;//head
				senddata.head[1] = 0xee;//commandid
				senddata.head[2] = commandnum%256;
				senddata.head[3] = mymac_addr[0];//from
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = zigbee_addr/256;//to
				senddata.head[6] = zigbee_addr%256;
				senddata.head[7] = TYPE_G_RUN;//type工作模式
				//如果是控制开关的开和关，则进行封装
				if(subtype==SUBTYPE_SWITCH)
				{
						senddata.head[8] = 0x10;//length
						senddata.data[0] = subtype;//subtype
						senddata.data[1] = route_scen;
						senddata.data[2] = value/256;//value
						senddata.data[3] = value%256;
				}
				else if(subtype == SUBTYPE_CURTAIN)  //控制窗帘的情景模式
				{
						senddata.head[8] = 0x10;//length

						senddata.data[0] = subtype;//subtype
						senddata.data[1] = route_scen; //route
						senddata.data[2] = 1;//value
						senddata.data[3] = value%256;
				}
				else
				{
						senddata.head[8] = 0x13;//length
						senddata.data[0] = subtype;//subtype

						if(route_scen!=114)
						{
								senddata.data[1] = route_scen;//route
						}
						else
						{
								senddata.data[1] = value;
						}
						senddata.data[2] = 1;//value

						if(route_scen == 113)
						{
								senddata.data[3] = value/(256*256);
								senddata.data[4] = (value%(256*256))/256;
								senddata.data[5] = value%256;//value
								senddata.data[6] = 0xFF;
						}
						else if(route_scen ==111)
						{
								senddata.data[3] = value;
								senddata.data[4] = 0xFF;
								senddata.data[5] = 0xFF;
								senddata.data[6] = 0xFF;
						}
						else if(route_scen == 21)
						{
								senddata.data[3] = value;
								senddata.data[4] = 0xFF;
								senddata.data[5] = 0xFF;
								senddata.data[6] = 0xFF;
						}
						else
						{
								senddata.data[3] = 0xFF;
								senddata.data[4] = 0xFF;
								senddata.data[5] = 0xFF;
								senddata.data[6] = 0xFF;
						}
				}
				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;

				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);

				commandnum++;
				pthread_t ConfirmThrId;
				err = pthread_create(&ConfirmThrId,NULL,ConfirmThr,&senddata);

				if(err != 0)
				{
						printf("cannot create thread %s\n",strerror(err));
				}
				usleep(30000);
		}
		sqlite3_finalize(stmt);
		return 1;
}
/**
 * 红外触发命令
 */
void infraredInductionCommand(char mac_addr[])
{
		sqlite3_stmt * stmt,* stmt1;
		char  sql[256];
		int rc,value,route,subtype = 0;
		sendstr senddata;
		int zigbee_addr = 0;
		int err;

		sprintf(sql,"select B.zigbee_addr,B.value,B.route from phymapping A,scen B where A.id=B.scen_id and A.mac_addr='%s' ",mac_addr);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				fprintf(stderr,"%s\n", sqlite3_errmsg(db));
				return;
		}

		while(sqlite3_step(stmt)==SQLITE_ROW)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				value = sqlite3_column_int(stmt,1);
				route = sqlite3_column_int(stmt,2);

				if(zigbee_addr==0)
				{
						printf("can't found the right record!\n");
						continue;
				}
				else
				{
						sprintf(sql,"select type from phymapping where zigbee_addr=%d ",zigbee_addr);
						rc = sqlite3_prepare(db,sql,-1,&stmt1,NULL);
						if(rc != SQLITE_OK)
						{
								printf("select type error\n");
						}

						while(sqlite3_step(stmt1)==SQLITE_ROW)
						{
								subtype = sqlite3_column_int(stmt1,0);
						}
						sqlite3_finalize(stmt1);
				}

				if(subtype == 0)
				{
						continue;
				}
				senddata.head[0] = 0x80;
				senddata.head[1] = 0xee;
				senddata.head[2] = commandnum%256;
				senddata.head[3] = mymac_addr[0];
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = zigbee_addr/256;
				senddata.head[6] = zigbee_addr%256;
				senddata.head[7] = TYPE_G_RUN;
				senddata.head[8] = 0x13;

				senddata.data[0] = subtype;
				senddata.data[1] = route;
				senddata.data[2] = 1;

				if(route == 113)
				{
						senddata.data[3] = value/(256*256);
						senddata.data[4] = (value%(256*256))/256;
						senddata.data[5] = value%256;
						senddata.data[6] = 0xFF;
				}
				else if(route == 110||route == 111)
				{
						senddata.data[3] = value;
						senddata.data[4] = 0xFF;
						senddata.data[5] = 0xFF;
						senddata.data[6] = 0xFF;
				}
				else
				{
						senddata.data[3] = 0xFF;
						senddata.data[4] = 0xFF;
						senddata.data[5] = 0xFF;
						senddata.data[6] = 0xFF;
				}
				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;
				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
				commandnum++;

				pthread_t ConfirmThrId;
				err = pthread_create(&ConfirmThrId,NULL,ConfirmThr,&senddata);

				if(err != 0)
				{
						printf("cannot create thread search db:%s\n",strerror(err));
				}
				usleep(100000);
			}
			sqlite3_finalize(stmt);
}

/**
 * 开关控制命令封装
 */
void * switchCommandThr(void * args)
{
		char* array = (char*)args;
		pthread_mutex_unlock(&mutex_switch);
		pthread_detach(pthread_self());

		int route=array[10];
		char mac_addr[20];
		HexToStr(mac_addr,(unsigned char *)array+12,8);

		sqlite3_stmt * stmt,* stmt1;
		char  sql[256],str[30];
		int rc;
		sendstr senddata;
		int zigbee_addr=0,route_scen,value,subtype;
		int err;

		int commandId = array[1]*256+array[2];
		sprintf(str,"%d",commandId);
		//开关命令 回复
		if(IsEmpty(confirmList))
		{
				senddata.head[0] = 0x81;
				senddata.head[1] = array[1];
				senddata.head[2] = array[2];
				senddata.head[3] = mymac_addr[0];
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = array[3];
				senddata.head[6] = array[4];
				senddata.head[7] = array[7];
				senddata.head[8] = 0x13;

				senddata.data[0] = array[9];
				senddata.data[1] = array[10];
				senddata.data[2] = array[11];
				senddata.data[3] = array[12];
				senddata.data[4] = array[13];
				senddata.data[5] = array[14];
				senddata.data[6] = array[15];

				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;

				EnList(confirmList,str);
				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
		}
		else if(deleteNode(confirmList,str))
		{
				senddata.head[0] = 0x81;
				senddata.head[1] = array[1];
				senddata.head[2] = array[2];
				senddata.head[3] = mymac_addr[0];
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = array[3];
				senddata.head[6] = array[4];
				senddata.head[7] = array[7];
				senddata.head[8] = 0x13;

				senddata.data[0] = array[9];
				senddata.data[1] = array[10];
				senddata.data[2] = array[11];
				senddata.data[3] = array[12];
				senddata.data[4] = array[13];
				senddata.data[5] = array[14];
				senddata.data[6] = array[15];

				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;

				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
				printf("delete success \n");
				pthread_exit(NULL);
		}
		else
		{
				senddata.head[0] = 0x81;
				senddata.head[1] = array[1];
				senddata.head[2] = array[2];
				senddata.head[3] = mymac_addr[0];
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = array[3];
				senddata.head[6] = array[4];
				senddata.head[7] = array[7];
				senddata.head[8] = 0x13;
				senddata.data[0] = array[9];
				senddata.data[1] = array[10];
				senddata.data[2] = array[11];
				senddata.data[3] = array[12];
				senddata.data[4] = array[13];
				senddata.data[5] = array[14];
				senddata.data[6] = array[15];

				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;

				if(confirmList->size>1)
				{
						DeList(confirmList);
				}

				EnList(confirmList,str);
				//	printf("confirmList size %d \n",confirmList->size);
				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
		}

		sprintf(sql,"select B.zigbee_addr,B.route,B.value from phymapping A,scen B where A.id=B.scen_id and A.mac_addr='%s' and A.route=%d ",mac_addr,route);
		rc = sqlite3_prepare(db,sql,-1,&stmt,NULL);
		if(rc!=SQLITE_OK)
		{
				fprintf(stderr,"%s\n", sqlite3_errmsg(db));
		}
		while(sqlite3_step(stmt) == SQLITE_ROW	)
		{
				zigbee_addr = sqlite3_column_int(stmt,0);
				route_scen = sqlite3_column_int(stmt,1);
				value = sqlite3_column_int(stmt,2);

				if(zigbee_addr==0)
				{
						printf("can't found the right record!\n");
						continue;
				}
				else
				{
						sprintf(sql,"select type from phymapping where zigbee_addr=%d ",zigbee_addr);
						rc = sqlite3_prepare(db,sql,-1,&stmt1,NULL);
						if(rc != SQLITE_OK)
						{
								printf("select type error\n");
						}
						while(sqlite3_step(stmt1)==SQLITE_ROW)
						{
								subtype = sqlite3_column_int(stmt1,0);
						}
						sqlite3_finalize(stmt1);
				}
				if(subtype == 0)
				{
						continue;
				}
				senddata.head[0] = 0x80;//head
				senddata.head[1] = 0xee;//commandid
				senddata.head[2] = commandnum%256;
				senddata.head[3] = mymac_addr[0];//from
				senddata.head[4] = mymac_addr[1];
				senddata.head[5] = zigbee_addr/256;//to
				senddata.head[6] = zigbee_addr%256;
				senddata.head[7] = TYPE_G_RUN;//type工作模式

				//开关的情景模式
				if(subtype == SUBTYPE_SWITCH)
				{
						senddata.head[8] = 0x10;//length

						senddata.data[0] = subtype;//subtype
						senddata.data[1]=route_scen; //route
						senddata.data[2]=value/256;//value
						senddata.data[3]=value%256;
				}
				else if(subtype == SUBTYPE_CURTAIN)  //控制窗帘的情景模式
				{
						senddata.head[8] = 0x10;//length

						senddata.data[0] = subtype;//subtype
						senddata.data[1] = route_scen; //route
						senddata.data[2] = 1;//value
						senddata.data[3] = value%256;
				}
				else
				{
						senddata.head[8] = 0x13;//length
						senddata.data[0] = subtype;//subtype
						senddata.data[1] = route_scen;//route
						senddata.data[2] = 1;//value
						//灯的情景模式
						if(route_scen == 113)
						{
								senddata.data[3] = value/(256*256);
								senddata.data[4] = (value%(256*256))/256;
								senddata.data[5] = value%256;//value
								senddata.data[6] = 0xFF;
						}
						else if(route_scen == 111)
						{
								senddata.data[3] = value;
								senddata.data[4] = 0xFF;
								senddata.data[5] = 0xFF;
								senddata.data[6] = 0xFF;
						}
						else if(route_scen == 110)
						{
								senddata.data[3] = value;
								senddata.data[4] = 0xFF;
								senddata.data[5] = 0xFF;
								senddata.data[6] = 0xFF;
						}
						else
						{
								senddata.data[3] = 0xFF;
								senddata.data[4] = 0xFF;
								senddata.data[5] = 0xFF;
								senddata.data[6] = 0xFF;
						}
				}
				senddata.tail[0] = 0x00;
				senddata.tail[1] = 0x00;
				senddata.tail[2] = 0x00;
				senddata.tail[3] = 0x00;
				senddata.tail[4] = 0x40;

				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
//		int commandid = senddata.head[2]*256+senddata.head[3];
//		char str[30];
//		sprintf(str,"%d",commandid);
//		int i = 0;
//		while(i<5)
//		{
//			usleep(90000);
//			printf("\n str = %s\n",str);
//
//		    pthread_mutex_lock(&mutexStatus);
//			if(!IsEmpty(list))
//			{
//				if(deleteNode(list,str))
//				{
//					pthread_mutex_unlock(&mutexStatus);
//					break;
//				}
//			}
//		    pthread_mutex_unlock(&mutexStatus);
//
//			pthread_mutex_lock(&mutexStruct);
//			EnQueueStruct(dataqu,&senddata);
//			pthread_mutex_unlock(&mutexStruct);
//			i++;
//			printf("fail\n");
//		}
				commandnum++;
				pthread_t ConfirmThrId;
				err = pthread_create(&ConfirmThrId,NULL,ConfirmThr,&senddata);

				if(err != 0)
				{
						printf("cannot create thread %s\n",strerror(err));
				}
				usleep(30000);
		}
		sqlite3_finalize(stmt);
		pthread_exit(NULL);
}
/**
 *
 */
void * ConfirmThr(void * args)
{
		sendstr senddata = *((sendstr*)args);
		pthread_detach(pthread_self());
		int commandid = senddata.head[1]*256+senddata.head[2];
		printf("commandid = %d\n",commandid);
		char str[30];
		int i = 2;
		sprintf(str,"%d",commandid);

		while(i>0)
		{
				usleep(300000);
				pthread_mutex_lock(&mutexStatus);
				if(!IsEmpty(list))
				{
						printf("is not empty\n");
						if(deleteNode(list,str))
						{
								pthread_mutex_unlock(&mutexStatus);
								break;
						}
				}
				pthread_mutex_unlock(&mutexStatus);

				pthread_mutex_lock(&mutex_queue);
				EnQueue(dataqu,&senddata);
				pthread_mutex_unlock(&mutex_queue);
				printf("  fail\n");
				i--;
		}
		pthread_exit(NULL);
}


void * SendDataThr()
{
		PNode1 front;
		while(1)
		{
				sendstr senddata;
				usleep(30000);
				pthread_mutex_lock(&mutex_queue);
				if(!IsEmptyQueue(dataqu))
				{
						front = dataqu->front;
						printf(" send : ");
						senddata.head[0] = front->data->head[0];
						senddata.head[1] = front->data->head[1];
						senddata.head[2] = front->data->head[2];
						senddata.head[3] = front->data->head[3];
						senddata.head[4] = front->data->head[4];
						senddata.head[5] = front->data->head[5];
						senddata.head[6] = front->data->head[6];
						senddata.head[7] = front->data->head[7];
						senddata.head[8] = front->data->head[8];
						int length = senddata.head[8];
						int i = 0;
						printf("%x %x %x %x %x %x %x %x %x ",senddata.head[0],senddata.head[1],senddata.head[2],senddata.head[3] ,senddata.head[4],senddata.head[5],senddata.head[6],senddata.head[7],senddata.head[8]);
						while(length-12-i > 0)
						{
								senddata.data[i] = front->data->data[i];
								printf("%x ",senddata.data[i]);
								i++;
						}
						printf("%x %x %x %x %x \n",senddata.tail[0],senddata.tail[1],senddata.tail[2],senddata.tail[3],senddata.tail[4]);
						senddata.tail[0] = front->data->tail[0];
						senddata.tail[1] = front->data->tail[1];
						senddata.tail[2] = front->data->tail[2];
						senddata.tail[3] = front->data->tail[3];
						senddata.tail[4] = front->data->tail[4];

						char array[100];
						memcpy(array,senddata.head,9);
						memcpy(array+9,senddata.data,length-12);

						uint crc = CRC32Software((unsigned char *)(array+1),length-4);//()7.27 crc加密不包括本身部分
						memcpy(senddata.tail,&crc,4);   //此处会反掉！！
						char temp = senddata.tail[0];
						senddata.tail[0] = senddata.tail[3];
						senddata.tail[3] = temp;
						temp = senddata.tail[1];
						senddata.tail[1] = senddata.tail[2];
						senddata.tail[2] = temp;

						memcpy(array+length-3,senddata.tail,5);
						unsigned char *buff;
						array[length+1] = '\0';
						buff =  base64Encode(array+1,length,NULL);
						//			int alen = strlen(buff);
						//			printf("alen = %d",alen);
						int buff_len;
						if(length%3==0)
						{
								buff_len = length/3*4;
						}
						else
						{
								buff_len = length/3*4+4;
						}
						memcpy(array+1,buff,buff_len);
						array[0] = front->data->head[0];
						array[buff_len+1] = 0x40;
						//			printf(" %x",array[0]);
						//			for(i=1;i<buff_len+1;i++)
						//			{
						//				printf(" %x",array[i]);
						//			}
						//			printf(" \n");
						write(UARTID,array,buff_len+2);
						log4c_category_log(logcg, LOG4C_PRIORITY_TRACE, "send:%s",array);
						DeQueue(dataqu);
				}
				pthread_mutex_unlock(&mutex_queue);
		}
		pthread_exit(NULL);
}

//void SendData(int fd,sendstr senddata)
//{
//		char array[100];
//		int length=senddata.head[9];
//		memcpy(array,senddata.head,10);
//		memcpy(array+10,senddata.data,length-12);
//		uint crc=CRC32Software((unsigned char *)(array+2),length-4);//()7.27 crc加密不包括本身部分
//		memcpy(senddata.tail,&crc,4);//此处会反掉！！
//		char temp=senddata.tail[0];
//		senddata.tail[0]=senddata.tail[3];
//		senddata.tail[3]=temp;
//		temp=senddata.tail[1];
//		senddata.tail[1]=senddata.tail[2];
//		senddata.tail[2]=temp;
//
//		memcpy(array+length-2,senddata.tail,6);
//
//		write(fd,array,length+4);
//		printf("send command ok\n");
//		log4c_category_log(logcg, LOG4C_PRIORITY_TRACE, "send:%s",array);
//}
//更新phymapping表状态
//int UpdateStatus(int zigbee_addr,int commandid,int route,int value)
//{
//	clock_t start,end;
//	sqlite3_stmt * stmt;
//	char  sql[256];
//	int rc;
//	char * errmsg;
//	char str[30];
//	sprintf(str,"%d",commandid);
//	start=clock();
//	while(1)
//	{
//		pthread_mutex_lock(&mutexStatus);
//		if(!IsEmpty(list)&&deleteNode(list,str))
//		{
//			sprintf(sql,"select logic_id,type from phymapping where zigbee_addr=%d",zigbee_addr);
//			rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
//			if(rc!=SQLITE_OK)
//			{
//				fprintf(stderr,"phymapping:%s\n", sqlite3_errmsg(db));
//				return 0;
//			}
//			int logic_id,type;
//			while(sqlite3_step(stmt)==SQLITE_ROW)//只查到一行？
//			{
//				logic_id=sqlite3_column_int(stmt,0);
//				type=sqlite3_column_int(stmt,1);
//			}
//			sqlite3_finalize(stmt);
//
//			if(route == 0x13)
//			{
//				sprintf(sql,"update phymapping set status=%d where logic_id=%d",value,logic_id);
//			}else if(route == 0x14)
//			{
//				sprintf(sql,"update phymapping set status2=%d where logic_id=%d",value,logic_id);
//			}elseff ff 0
//			{
//				sprintf(sql,"update phymapping set route=%d where logic_id=%d",route,logic_id);
//			}
//
//			int rc=sqlite3_exec(db,sql,NULL,NULL,&errmsg);
//
//			if(rc!=SQLITE_OK)
//		    {
//			  printf("update status fail:%s\n",errmsg);
//			  pthread_mutex_unlock(&mutexStatus);
//			  return 0;
//		    }
//		    pthread_mutex_unlock(&mutexStatus);
//			return 1;
//		}
//		pthread_mutex_unlock(&mutexStatus);
//		end=clock();
//		if((end-start)/CLOCKS_PER_SEC>=1)  //1s延时 without '=' 2s延时
//		{
//			return 0;
//		}
//
//	}
//}
///**
// * 光照度封装
// */
//sendstr  RGB_LightCommand(int logicid,int commandid,int route,int value,int ack)
//{
//		sendstr command;
//		sqlite3_stmt * stmt;
//		char  sql[256];
//		int rc;
//		sprintf(sql,"select zigbee_addr from phymapping where logic_id=%d ",logicid);
//		rc=sqlite3_prepare(db,sql,-1,&stmt,NULL);
//		if(rc!=SQLITE_OK)
//		{
//				printf("prepare error :%d\n",rc);
//				return command;
//		}
//		int zigbee_addr=0;
//		if(sqlite3_step(stmt)==SQLITE_ROW	)
//		{
//				zigbee_addr=sqlite3_column_int(stmt,0);
//		}
//		sqlite3_finalize(stmt);
//
//		if(zigbee_addr==0)
//		{
//				printf("can't found the right record!\n");
//				return command;
//		}
//
//		command.head[0] = 0x80;//head
//		command.head[1] = commandid/256;//commandid
//		command.head[2] = commandid%256;
//		command.head[3] = mymac_addr[0];//from
//		command.head[4] = mymac_addr[1];
//		command.head[5] = zigbee_addr/256;//to
//		command.head[6] = zigbee_addr%256;
//		command.head[7] = TYPE_G_RUN;//type
//		command.head[8] = 0x12;//length
//		command.data[0] = SUBTYPE_YW_LIGHT;//subtype
//		command.data[1] = route;
//		command.data[2] = value/(256*256);	//value
//		command.data[3] = (value%(256*256))/256;
//		command.data[4] = value%256;	//value
//		command.data[5] =  0xFF;
//		command.tail[0] = 0x00;
//		command.tail[1] = 0x00;
//		command.tail[2] = 0x00;
//		command.tail[3] = 0x00;
//		command.tail[4] = 0x40;
//		return command;
//}
//数据库查询回调函数
//int sqlite3_exec_callback(void *data, int nColumn, char **colValues, char **colNames)
//{
//	sendstr senddata;
//
//	senddata.head[0]=0x80;//head
//	senddata.head[1]=0xef;
//	senddata.head[2]=atoi(colValues[0]);
//	senddata.head[3]=mymac_addr[0];//from
//	senddata.head[4]=mymac_addr[1];
//	senddata.head[5]=atoi(colValues[1])/256;//to
//	senddata.head[6]=atoi(colValues[1])%256;
//	senddata.head[7]=0x11;//type工作模式
//	senddata.head[8]=0x10;//length
//ff ff 0
//	senddata.data[0]=atoi(colValues[5]);//subtype
//	senddata.data[1]=atoi(colValues[2]);//route
//	senddata.data[2]=atoi(colValues[3])/256;//value
//	senddata.data[3]=atoi(colValues[3])%256;
//
//	senddata.tail[0]=0x00;
//	senddata.tail[1]=0x00;
//	senddata.tail[2]=0x00;
//	senddata.tail[3]=0x00;
//	senddata.tail[4]=0x40;
//
//	pthread_mutex_lock(&mutex_queue);
//	EnQueue(dataqu,&senddata);
//	pthread_mutex_unlock(&mutex_queue);
//
//    return 0;
//}
