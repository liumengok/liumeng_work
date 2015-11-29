/*
 * MyThread.h
 *
 *  Created on: 2015年7月7日
 *     @Author:刘梦
 */

#ifndef MYTHREAD_H_
#define MYTHREAD_H_
#include <sqlite3.h>

typedef unsigned int uint;
typedef struct senddstr
{
	unsigned char head[10];
	unsigned char data[100];
	unsigned char tail[6];
}sendstr;

#define 	TYPE_T_ASK_NET 	0x01				//注册
#define 	TYPE_T_ACT_NET 	0x02				//响应注册
#define	 TYPE_T_ASK_KEY 		0x08				//请求密钥
#define 	TYPE_T_ACT_KEY 		0x09				//发送密钥
#define 	TYPE_G_RUN            0x11               //网关像终端发送执行命令
#define 	TYPE_G_RUN_INFO	 0x14			//命令执行情况

#define 	TYPE_G_ASK_INFO  0x12             //
#define 	TYPE_G_INFO           0x10

#define 	TYPE_T_ASK_RUN 	0x13				//情景模式
#define 	TYPE_T_RET_INFO 		0x15			//心跳
#define 	TYPE_T_ASK_RELATION 	0x16	//开关请求映射关系
#define 	TYPE_T_ACT_RELATION		 0x17	//返回映射关系
#define 	TYPE_G_REGISTERAGAIN		0x21   //重新注册

#define 	SUBTYPE_SWITCH   							0x11   		//开关
//#define SUBTYPE_BRIGHT	   							0x12    		//灯亮度
//#define SUBTYPE_COLOR   							0x13			//灯颜色
#define 	SUBTYPE_CURTAIN 							0x14          //窗帘
#define 	SUBTYPE_ALIVE      							0x16			//心跳包
#define 	SUBTYPE_SOCKET      						0x19         //插座
#define 	SUBTYPE_TEMPERATURE   				0x22    		//温度
#define 	SUBTYPE_HUMIDITY   						0x23			//湿度
#define 	SUBTYPE_TEMP_HUM		 				0x24          //温湿度
#define 	SUBTYPE_LIGHT_INTENSITY      	0x27			//光照强度
#define 	SUBTYPE_RGB_LIGHT	 					0x28        //RGB  LED
#define 	SUBTYPE_YW_LIGHT   	 					0x29        //YW LED
#define 	SUBTYPE_INFRARED_CONTROL 	0x40   	//红外
#define 	SUBTYPE_BODY_SENSOR				0x40 			//人体感应
#define 	SUBTYPE_SMOKE								0x41			//烟雾报警
#define	SUBTYPE_INFRARED						0x42			//红外报警
#define	SUBTYPE_485_MUSIC						0x50			//485
#define 	SUBTYPE_485_AC								0x51          //485

#define  	CRON_VIEW   				0x33
#define 	CRON_CANCEL			0x34




//"up"=>"1","down"=>"2","left"=>"3","right"=>"4","vol_up"=>"5",
//   "vol_down"=>"6","channel_up"=>"7","channel_down"=>"8","power"=>"9","menu"=>"10",
//   "confirm"=>"11","return"=>"12","start"=>"13","stop"=>"14","mute"=>"15",
//   "power_on"=>"16","power_off"=>"17",
//   "key_1"=>"18","key_2"=>"19","key_3"=>"20","key_4"=>"21","key_5"=>"22",
//   "key_6"=>"23","key_7"=>"24","key_8"=>"25","key_9"=>"26","key_0"=>"27",
//   "elec"=>"28","warmcold"=>"29","brightness"=>"30","RGB"=>"31",
//   "open"=>"32","close"=>"33","scene"=>"34","cron"=>"35","synstatus"=>"36","pause"=>"37",
//   "temp_hum"=>"38","body_sensor"=>"39","smoke"=>"40","ir_alarm"=>"41","music_play"=>"42",
//
//   "AWARM"=>"80","ACOLD"=>"81","COLD_TO_WARM"=>"82","NORMAL"=>"83","COLD_DECREASE"=>"84",
//   "WARM_INCREASE"=>"85","HWARM"=>"86",
//   "REDGRADUAL"=>"88","BLUEGRADUAL"=>"89","GREENGRADUAL"=>"90","GRADUAL"=>"91",
//   "MODUAL1"=>"92","MODUAL2"=>"93","MODUAL3"=>"94","GRADUAL_MODUEL"=>"95",
//   "MUTATION_SLOW"=>"96","CHANGE_COLOR"=>"100","CHANGE_BRIGHT"=>"101","SWITCH_BRIGHT"=>"102",

#define 	COLDWARM 								110
#define 	BRIGHTNESS 								0x65
#define 	COLDWARM_BRIGHTNESS 	112
#define 	RGB_COLOR 								113

int SystemStart();
void * CommandThr();
void * UartTXThr(void *args);
void * UartRXThr(void *args);
void * StatusThr();
void * CronThr();
void * Sqlite3Thr();
void * ConfirmThr();
void * SendDataThr();
void * switchCommandThr();
void * TestThr();

sqlite3 * sqliteInit(char * name);//数据库初始化
int uartInit(char * name);//串口初始化
int socketInit(int port);//建立socket连接
sendstr TranSendCommand(char * command);//将命令转换为符合协议16进制数组
int loadOrSaveDb(sqlite3 *pInMemeory, const char *zFilename, int isSave);//内存数据库导入导出
uint CRC32Software(unsigned char * pData, int Length);
void SendData(int fd,sendstr senddata);//zigbee发送命令
void SetZigbeeAlive(int zgb_addr,int deviceNum);//zigbee心跳
int UpdateStatus(int zigbee_addr,int commandid,int route,int value);//更新phymapping表状态
sendstr infraredCommand(int logicid,int commandid,int type);//红外命令
sendstr switchCommand(int logicid,int commandid,int  action,int value);//开关命令
sendstr RGBModuleCommand(int logicid,int commandid,int type,int value,int ack);//color命令
sendstr YWModuleCommand(int logicid,int commandid,int type,int value,int ack);//bright命令
sendstr CurtainCommand(int logic_id,int commandid,int action,int value);
sendstr SocketCommand(int logic_id,int commandid,int action,int value);
sendstr TemperatureCommand(int logicid,int commandid,int route,int value,int ack);
sendstr HumidityCommand(int logicid,int commandid,int route,int value,int ack);
sendstr LightIntensityCommand(int lgcid,int commandid,int action,int value,int ack);
sendstr Tem_Hum_Command(int logicid,int commandid,int route,int value,int ack);
sendstr AC_485_Command(int logicid,int commandid,int route,int ack);

int TransferCommand(char * buff,int connfd);
void wakeup();
void answerRegister(int commandid,int destzigbee,int phy_addr,char mac_addr[],int flag,int type);//响应注册
void sendSwitchMapping(int commandid,int destzigbee,int phy_addr,char mac_addr[],int flag);//注册返回映射关系
int searchStatus(char buf[]);//查询状态
void createAesKey();//生成密钥
void sendUserKey(int commandid,int destzigbee);//发送密钥
void HexToStr(char *pbDest,unsigned char *pbSrc, int nLen);//16进制数转换成字符串
void infraredInductionCommand(char mac_addr[]);//红外感应命令
int sceneCommand(int logic_id,int commandid,int route,int value1,int ack);
int clearBuffCommand(int logicId,int commandId,int action,int type);
void updateMapping(int logic_id);     //更新开关映射关系
#endif /* MYTHREAD_H_ */

//#define 	SOCKET_CURRENT		 					0x01         //采集电流
//#define 	SOCKET_CONTROL	 						0x02			//控制通断
//#define 	SOCKET_OPEN				 					0x01			//开
//#define 	SOCKET_CLOSE 									0x02          //关
//void encrypt(unsigned char * in,unsigned char * out);//加密
//void decrypt(unsigned char * in,unsigned char * out);//解密
//void StrToHex(char *pbDest, char *pbSrc, int nLen);//字符串转16进制
//sendstr  RGB_LightCommand(int logicid,int commandid,int route,int value,int ack);
//int sqlite3_exec_callback(void *data, int nColumn, char **colValues, char **colNames);//数据库查询回调函数
