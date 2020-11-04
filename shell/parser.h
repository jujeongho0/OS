
#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_NR_TOKENS	32	/* Maximum length of tokens in a command */
#define MAX_TOKEN_LEN	128	/* Maximum length of single token */
#define MAX_COMMAND_LEN	4096 /* Maximum length of assembly string */


int parse_command(char *command, int *nr_tokens, char *tokens[]);

#endif
