#include <string.h>
#include <ctype.h>

#include "types.h"
#include "parser.h"

int parse_command(char *command, int *nr_tokens, char *tokens[])
{
	char *curr = command;
	int token_started = false;
	*nr_tokens = 0;

	while (*curr != '\0') {  
		if (isspace(*curr)) {  
			*curr = '\0';
			token_started = false;
		} else {
			if (!token_started) {
				tokens[*nr_tokens] = curr;
				*nr_tokens += 1;
				token_started = true;
			}
		}

		curr++;
	}

	return (*nr_tokens > 0);
}
