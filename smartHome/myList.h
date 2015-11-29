#ifndef LIST_H
#define LIST_H

typedef char * Item;
typedef struct node * PNode;

typedef struct node
{
	Item data;

	PNode next;
}Node;

typedef struct List
{
	PNode front;
	PNode rear;
	int size;
}List;

/*构造一个空队列*/
List *InitList();

/*销毁一个队列*/
void DestroyList(List *pList);

/*清空一个队列*/
void ClearList(List *pList);

/*判断队列是否为空*/
int IsEmpty(List *pList);

/*返回队列大小*/
int GetSize(List *pList);

/*返回队头元素*/
PNode GetFront(List *pList,Item *pitem);

/*返回队尾元素*/
PNode GetRear(List *pList,Item *pitem);

/*将新元素入队*/
PNode EnList(List *pList,Item item);

/*队头元素出队*/
PNode DeList(List *pList);
int DelList(List *pList,Item *pitem);

int deleteNode(List *pList,Item pitem);
/*遍历队列并对各数据项调用visit函数*/
void ListTraverse(List *pList);

#endif

