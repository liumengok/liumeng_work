/*
 * queueStruct.h
 *
 *  Created on: Sep 2, 2015
 *      Author: openstack
 */

#ifndef QUEUE_H_
#define QUEUE_H_
#include "MyThread.h"
typedef  struct senddstr * Itemt;
typedef struct node1 * PNode1;
typedef struct node1
{
	Itemt data;
	PNode1 next;
}Node1;

typedef struct Queue
{
	PNode1 front;
	PNode1 rear;
	int size;
}Queue;

/*构造一个空队列*/
Queue *InitQueue();

/*销毁一个队列*/
void DestroyQueue(Queue *pqueue);

/*清空一个队列*/
void ClearQueue(Queue *pqueue);

/*判断队列是否为空*/
int IsEmptyQueue(Queue *pqueue);

/*返回队列大小*/
int GetSizeQueue(Queue *pqueue);

/*返回队头元素*/
PNode1 GetFrontQueue(Queue *pqueue,Itemt *pitem);

/*返回队尾元素*/
PNode1 GetRearQueue(Queue *pqueue,Itemt *pitem);

/*将新元素入队*/
PNode1 EnQueue(Queue *pqueue,Itemt item);

/*将新元素入队      将array字符串封装成sendstr 进入队列*/
PNode1 EnQueuebyArray(Queue *pqueue,unsigned char *array);

/*队头元素出队*/
PNode1 DeQueue(Queue *pqueue);

//delete Node where node->item=pitem
//int deleteNode(List *pList,Item pitem);
//int DelQueue(Queue *pqueue,Itemt *pitem);
int FindQueueValue(Queue *pqueue,Itemt pitem);  //查找并删除节点

int DeleteQueueNode(Queue *pqueue,Itemt pitem);

//int deleteNode(Queue *pqueue,Itemt pitem);
/*遍历队列并对各数据项调用visit函数*/
void QueueTraverse(Queue *pqueue);

#endif /* Queue_H_ */
