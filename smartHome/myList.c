#include "myList.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

/**
 * 构造一个空队列
 * */
List *InitList()
{
		List *pList = (List *)malloc(sizeof(List));
		if(pList!=NULL)
		{
				pList->front = NULL;
				pList->rear = NULL;
				pList->size = 0;
		}
		return pList;
}

/**
 * 销毁一个队列
 * */
void DestroyList(List *pList)
{
		if(IsEmpty(pList)!=1)
				ClearList(pList);
		free(pList);
}

/**
 * 清空一个队列
 * */
void ClearList(List *pList)
{
		while(IsEmpty(pList)!=1)
		{
				DeList(pList);
		}
}

/**
 * 判断队列是否为空
 * */
int IsEmpty(List *pList)
{
		if(pList->front==NULL&&pList->rear==NULL&&pList->size==0)
				return 1;
		else
				return 0;
}

/**
 * 返回队列大小
 * */
int GetSize(List *pList)
{
		return pList->size;
}

/**
 * 返回队头元素
 * */
PNode GetFront(List *pList,Item *pitem)
{
		if(IsEmpty(pList)!=1&&pitem!=NULL)
		{
				*pitem = pList->front->data;
		}
		return pList->front;
}

/**
 * 返回队尾元素
 * */
PNode GetRear(List *pList,Item *pitem)
{
		if(IsEmpty(pList)!=1&&pitem!=NULL)
		{
				*pitem = pList->rear->data;
		}
		return pList->rear;
}

/**
 * 将新元素入队
 * */
PNode EnList(List *pList,Item item)
{
		PNode pnode = (PNode)malloc(sizeof(Node));
		if(pnode != NULL)
		{
//			pnode->data = item;//此处为指针复制，所以当指针指向内容改变时也会自动改变
				pnode->data=(char *)malloc(strlen(item)+1);     //新开辟一块地址，存储入队数据
				strcpy(pnode->data,item);
				pnode->next = NULL;
				// 队列长度不用超过20位，1，用不到这么多,2,防止队列内数据没处理导致队列过长
				if(pList->size>20)
				{
						DeList(pList);
				}
				if(IsEmpty(pList))
				{
						pList->front = pnode;
				}
				else
				{
						pList->rear->next = pnode;
				}
				pList->rear = pnode;
				pList->size++;
		}
		return pnode;
}
/**
 * 队头元素出队
 * */
PNode DeList(List *pList)
{
	//Item *pitem;
		PNode pnode = pList->front;
		if(IsEmpty(pList)!=1&&pnode!=NULL)
		{
				pList->size--;
				pList->front = pnode->next;
				free(pnode->data);
				free(pnode);
				if(pList->size==0)
						pList->rear = NULL;
		}
		return pList->front;
}
/**
 * delete Node where node->item=pitem
 */
int deleteNode(List *pList,Item pitem)
{
		Node * node,*tempNode;
		node = pList->front;
		int i = 2;
		if(pitem!=NULL)
		{
				if(strcmp(node->data,pitem)==0)
				{
						//	printf(" del %s ",pitem);
						pList->size--;
						pList->front = node->next;
						free(node);
						if(pList->size==0)
								pList->rear = NULL;
						return 1;
				}

				while(node->next)
				{
						//printf(" node->next->data = %s",node->next->data);
						if(strcmp(node->next->data,pitem)==0)
						{
								if(strcmp(pitem,pList->rear->data)==0&&i==pList->size)
								{
										pList->rear = node;
								}
								tempNode = node->next;
								node->next = node->next->next;
								free(tempNode);
								pList->size--;
								//	ListTraverse(pList);
								return 1;
						}
						node = node->next;
						i++;
				}
		}
		return 0;
}

/**
 * 遍历队列并对各数据项
 * */
void ListTraverse(List *pList)
{
		PNode pnode = pList->front;
		int i = pList->size;
		while(i--)
		{
				printf("data= %s ",pnode->data);
				pnode = pnode->next;
		}
}
