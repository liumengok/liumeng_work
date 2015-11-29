/*
 * Queue.c
 *
 *  Created on: Sep 2, 2015
 *      Author: openstack
 */

#include "myQueue.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>

/*构造一个空队列*/
Queue *InitQueue()
{
	Queue *pqueue = (Queue *)malloc(sizeof(Queue));
	if(pqueue!=NULL)
	{
		pqueue->front = NULL;
		pqueue->rear = NULL;
		pqueue->size = 0;
	}
	return pqueue;
}

/*销毁一个队列*/
void DestroyQueue(Queue *pqueue)
{
	if(IsEmptyQueue(pqueue)!=1)
		ClearQueue(pqueue);
	free(pqueue);
}

/*清空一个队列*/
void ClearQueue(Queue *pqueue)
{
	while(IsEmptyQueue(pqueue)!=1)
	{
		DeQueue(pqueue);
	}
}

/*判断队列是否为空*/
int IsEmptyQueue(Queue *pqueue)
{
	if(pqueue->front==NULL&&pqueue->rear==NULL&&pqueue->size==0)
		return 1;
	else
		return 0;
}

/*返回队列大小*/
int GetSizeQueue(Queue *pqueue)
{
	return pqueue->size;
}

/*返回队头元素*/
PNode1 GetFrontQueue(Queue *pqueue,Itemt *pitem)
{
	if(IsEmptyQueue(pqueue)!=1&&pitem!=NULL)
	{
		*pitem = pqueue->front->data;
	}
	return pqueue->front;
}

/*返回队尾元素*/
PNode1 GetRearQueue(Queue *pqueue,Itemt *pitem)
{
	if(IsEmptyQueue(pqueue)!=1&&pitem!=NULL)
	{
		*pitem = pqueue->rear->data;
	}
	return pqueue->rear;
}

PNode1 EnQueue(Queue *pqueue,Itemt item)
{
	PNode1 pnode = (PNode1)malloc(sizeof(Node1));

	int length = item->head[8];
	int i = 0;
	if(pnode != NULL)
	{
		pnode->data=(struct senddstr *)malloc(sizeof(struct senddstr));     //新开辟一块地址，存储入队数据

		//	strcpy(pnode->data->head,item->head);

			pnode->data->head[0] = item->head[0];
			pnode->data->head[1] = item->head[1];
			pnode->data->head[2] = item->head[2];
			pnode->data->head[3] = item->head[3];
			pnode->data->head[4] = item->head[4];
			pnode->data->head[5] = item->head[5];
			pnode->data->head[6] = item->head[6];
			pnode->data->head[7] = item->head[7];
			pnode->data->head[8] = item->head[8];
		//	pnode->data->head[9] = item->head[9];

			while(length-12-i > 0)
			{
				pnode->data->data[i]=item->data[i];
				i++;
			}

			pnode->data->tail[0]=item->tail[0];
			pnode->data->tail[1]=item->tail[1];
			pnode->data->tail[2]=item->tail[2];
			pnode->data->tail[3]=item->tail[3];
			pnode->data->tail[4]=item->tail[4];
			//pnode->data->tail[5]=item->tail[5];

			pnode->next = NULL;

//			if(pqueue->size>15){
//				DeQueue(pqueue);
//			}
			if(IsEmptyQueue(pqueue))
			{
				pqueue->front = pnode;
			}
			else
			{
				pqueue->rear->next = pnode;
			}
			pqueue->rear = pnode;
			pqueue->size++;
		}
		return pnode;
}

/*将新元素入队      将array字符串封装成sendstr 进入队列*/
PNode1 EnQueuebyArray(Queue *pqueue,unsigned char  * array)
{
	PNode1 pnode = (PNode1)malloc(sizeof(Node1));

	int length = array[8];
	int i = 0;
	if(pnode != NULL)
	{
		pnode->data=(struct senddstr *)malloc(sizeof(struct senddstr));     //新开辟一块地址，存储入队数据

		//	strcpy(pnode->data->head,item->head);
			pnode->data->head[0] = array[0];
			pnode->data->head[1] = array[1];
			pnode->data->head[2] = array[2];
			pnode->data->head[3] = array[3];
			pnode->data->head[4] = array[4];
			pnode->data->head[5] = array[5];
			pnode->data->head[6] = array[6];
			pnode->data->head[7] = array[7];
			pnode->data->head[8] = array[8];
		//	pnode->data->head[9] = item->head[9];
			while(length-10-i > 0)
			{
				pnode->data->data[i] = array[9 + i];
				i++;
			}
			pnode->data->tail[0] = array[9 + i];
			pnode->data->tail[1] = array[10 + i];
			pnode->data->tail[2] = array[11 + i];
			pnode->data->tail[3] = array[12 + i];
			pnode->data->tail[4] = 0x40;
			//pnode->data->tail[5]=item->tail[5];
			pnode->next = NULL;
//			if(pqueue->size>15){
//				DeQueue(pqueue);
//			}
			if(IsEmptyQueue(pqueue))
			{
				pqueue->front = pnode;
			}
			else
			{
				pqueue->rear->next = pnode;
			}
			pqueue->rear = pnode;
			pqueue->size++;
		}
		return pnode;
}

/*队头元素出队*/
PNode1 DeQueue(Queue *pqueue)
{
	//Item *pitem;
	PNode1 pnode = pqueue->front;
	if(IsEmptyQueue(pqueue)!=1&&pnode!=NULL)
	{
		pqueue->size--;
		pqueue->front = pnode->next;
		free(pnode->data);
		free(pnode);
		if(pqueue->size==0)
		pqueue->rear = NULL;
	}
	return pqueue->front;
}

//查找value  并删除查找到的节点
int FindQueueValue(Queue *pqueue,Itemt pitem)
{
	Node1  * node,*tempNode;
	node = pqueue->front;
	int i = 2,value = -1;
    int queueCommandId,commandId,rearCommandId ;
    if(pqueue != NULL)
    {
    	queueCommandId = node->data->head[1]*256 + node->data->head[2];
    	printf("head[1] =  %x head[2] = %x",node->data->head[1], node->data->head[2]);
    }
	if(pitem != NULL)
	{
		commandId = pitem->head[1] * 256 + pitem->head[2];
		printf("qCommandId = %d commandId = %d \n",queueCommandId,commandId);
		if(queueCommandId == commandId)
		{
			value =  node->data->data[2]*256*256*256 + node->data->data[3]*256*256 +node->data->data[4]*256 +node->data->data[5];
			pqueue->size--;
			pqueue->front = node->next;
			free(node);
			if(pqueue->size == 0)
				pqueue->rear = NULL;
			return value;
		}
		while(node->next)
		{
			//printf(" node->next->data = %s",node->next->data);//		sprintf(queueCommandId,"%d",node->next->data->head[1]*256 + node->next->data->head[2]);
			queueCommandId = node->next->data->head[1]*256 + node->next->data->head[2];
			if(queueCommandId == commandId)
			{
				value =  node->next->data->data[2]*256*256*256 + node->next->data->data[3]*256*256 +node->next->data->data[4]*256 +node->next->data->data[5];
				rearCommandId = node->next->data->head[1]*256 + node->next->data->head[2];
				if( commandId == rearCommandId && i == pqueue->size)
				{
					pqueue->rear = node;
				}
				tempNode = node->next;
				node->next = node->next->next;
				free(tempNode);
				pqueue->size--;
			//	ListTraverse(pList);
				return value;
			}
			node = node->next;
			i++;
		}
	}
	return value;
}
//delete Node where node->item=pitem
int DeleteQueueNode(Queue *pqueue,Itemt pitem)     //	(List *pList,Item pitem)
{
	Node1  * node,*tempNode;
	node = pqueue->front;
	int i = 2;
    int queueCommandId,commandId,rearCommandId ;
    if(pqueue != NULL)
    {
    	queueCommandId = node->data->head[1]*256 + node->data->head[2];
    }
	if(pitem != NULL)
	{
		commandId = pitem->head[1] * 256 + pitem->head[2];
		printf("qCommandId = %d commandId = %d \n",queueCommandId,commandId);
		if(queueCommandId == commandId)
		{
		//	printf(" del %s ",pitem);
			pqueue->size--;
			pqueue->front = node->next;
			free(node);
			if(pqueue->size == 0)
				pqueue->rear = NULL;
			return 1;
		}
		while(node->next)
		{
			//printf(" node->next->data = %s",node->next->data);//		sprintf(queueCommandId,"%d",node->next->data->head[1]*256 + node->next->data->head[2]);
			queueCommandId = node->next->data->head[1]*256 + node->next->data->head[2];

			if(queueCommandId == commandId)
			{
				rearCommandId = node->next->data->head[1]*256 + node->next->data->head[2];
				if( commandId == rearCommandId && i == pqueue->size)
				{
					pqueue->rear = node;
				}
				tempNode = node->next;
				node->next = node->next->next;
				free(tempNode);
				pqueue->size--;
			//	ListTraverse(pList);
				return 1;
			}
			node = node->next;
			i++;
		}
	}
	return 0;
}

/*遍历队列并对各数据项调用visit函数*/
void QueueTraverse(Queue *pqueue)
{
	PNode1 pnode = pqueue->front;
	int i = pqueue->size;
	printf("i = %d",i);
	while(i--)
	{
		//visit(pnode->data);
		printf(" %d %d %d %d",pnode->data->data[0],pnode->data->data[1],pnode->data->data[2],pnode->data->data[3]);
		pnode = pnode->next;
	}
}
