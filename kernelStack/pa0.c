#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/* Stack instance */
LIST_HEAD(stack);

void push_stack(int value);
int pop_stack(void);
void dump_stack(void);

int main(int argc, const char *argv[])
{
	int ret;

	/* Push 4 values */
	push_stack(27);
	push_stack(53);
	push_stack(1);
	push_stack(43);

	/* Dump the current stack */
	dump_stack();
	fprintf(stderr, "\n");

	/* Pop 2 values */
	fprintf(stderr, "%d\n", pop_stack());
	fprintf(stderr, "%d\n", pop_stack());
	fprintf(stderr, "\n");

	/* Dump the current stack */
	dump_stack();
	fprintf(stderr, "\n");

	/* And you so on ..... */
	push_stack(72);
	push_stack(999999);
	push_stack(55);
	
	ret = pop_stack();

	dump_stack();
	fprintf(stderr, "\n");

	fprintf(stderr, "%d\n", pop_stack());
	fprintf(stderr, "\n");

	/* Empty the stack by popping out all entries */
	while ((ret = pop_stack()) >= 0) {
		fprintf(stderr, "%d\n", ret);
	}

	return EXIT_SUCCESS;
}
