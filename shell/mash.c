#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include "types.h"
#include "parser.h"

pid_t pid;

static int alarm_check;

void alarmHandler(int sig)
{
	alarm_check=1;
	kill(pid,SIGKILL);
}

static char __prompt[MAX_TOKEN_LEN] = "$";

static unsigned int __timeout = 2;

static void set_timeout(unsigned int timeout)
{
	__timeout = timeout;

	if (__timeout == 0) {
		fprintf(stderr, "Timeout is disabled\n");
	} else {
		fprintf(stderr, "Timeout is set to %d second%s\n",
				__timeout,
				__timeout >= 2 ? "s" : "");
	}
}

static int run_command(int nr_tokens, char *tokens[])
{
	/* This function is all yours. Good luck! */

	if (strncmp(tokens[0], "exit", strlen("exit")) == 0) {
		return 0;
	}

	if(strncmp(tokens[0],"prompt",strlen("prompt"))==0) {
		strcpy(__prompt,tokens[1]);
		return 1;
	}	

	if(strncmp(tokens[0],"cd",strlen("cd"))==0) {
		if(nr_tokens == 1) 
			chdir(getenv("HOME"));			
		else {
			if(strncmp(tokens[1],"~",strlen("~"))==0) 
				chdir(getenv("HOME"));
			else 
				chdir(tokens[1]);	
		}	
		return 1;
	}	

	if(strncmp(tokens[0],"pwd",strlen("pwd"))==0) {	
		char cwd[255];
		getcwd(cwd,255);
		fprintf(stderr,"%s\n",cwd);
		return 1;
	}

	if(strncmp(tokens[0],"for",strlen("for"))==0) {					
		int n = atoi(tokens[1]);	
		for(int i=0;i<n;i++)			
			run_command(nr_tokens-2,tokens+2);
		return 1;
	}

	if(strncmp(tokens[0],"timeout",strlen("timeout"))==0) {			
		if(nr_tokens == 1)			
			fprintf(stderr,"Current timeout is %d second\n",__timeout);
		else {
			if(isdigit(*tokens[1]))
				set_timeout(atoi(tokens[1]));
			else
				execvp(tokens[0],tokens);
		}
		return 1;
	}

	pid=fork();
	struct sigaction sa; 
	sa.sa_handler = alarmHandler;
	sigemptyset(&sa.sa_mask); 
	sa.sa_flags=0;
	sigaction(SIGALRM,&sa,0);	

	//fork error
	if(pid<0)	
		return -1;	
		
	//child process
	else if(pid==0)	{				
		execvp(tokens[0],tokens);
		fprintf(stderr,"No such file or directory\n");	
		fclose(stdin);
		fclose(stdout);
		return 0;				
	}

	//parent proces
	else {
		int num,status;	
		alarm(__timeout);
		num=waitpid(pid,&status,0);

		if(alarm_check==1) {
			fprintf(stderr,"%s is timed out\n",tokens[0]);	
			alarm_check=0;
			return 1;
		}
		
		else {			
			alarm(0);			
			return 1;
		}	
		
		return 1;		
	}
}

static int initialize(int argc, char *argv[])
{
	return 0;
}


static void finalize(int argc, char *argv[])
{

}


static bool __verbose = true;
static char *__color_start = "[0;31;40m";
static char *__color_end = "[0m";

int main(int argc, char *argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	if (__verbose)
		fprintf(stderr, "%s%s%s ", __color_start, __prompt, __color_end);

	while (fgets(command, sizeof(command), stdin)) {	
		char *tokens[MAX_NR_TOKENS] = { NULL };
		int nr_tokens = 0;

		if (parse_command(command, &nr_tokens, tokens) == 0)
			goto more; /* You may use nested if-than-else, however .. */

		ret = run_command(nr_tokens, tokens);
		if (ret == 0) {
			break;
		}

more:
		if (__verbose)
			fprintf(stderr, "%s%s%s ", __color_start, __prompt, __color_end);
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
