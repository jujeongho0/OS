#include <stdio.h>
#include "types.h"
#include "list_head.h"

extern struct list_head stack;

struct entry {
	struct list_head list;
	int value;
};

#include <stdlib.h>                 

void push_stack(int value)
{
	/* TODO: Implement this function */
	struct entry* new;
	new = malloc(sizeof(struct entry));
	new->value = value;
	list_add(&new->list,&stack);
}

int pop_stack(void)
{
	/* TODO: Implement this function */
	if(list_empty(&stack))
	{
		return -1;
	}
	else
	{	
		int res = list_first_entry(&stack,struct entry,list)->value;	
		__list_del_entry(stack.next);
		return res;
	}	
	/* Must fix so that it returns the popped value instead of 0 */
}


void dump_stack(void)
{
	/* TODO: Implement this function */
	struct list_head *curr;
	struct entry *temp;

	list_for_each(curr,&stack)
	{
		temp=list_entry(curr,struct entry,list);
		fprintf(stderr, "%d\n",temp->value);
	}	
	//fprintf(stderr, "%d\n", 0xdeadbeef); Example. Print out values in this form */
}
