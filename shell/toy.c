#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

int main(int argc, const char *argv[])
{
	alarm(0);

	fprintf(stderr, "pid  = %d\n", getpid());
	fprintf(stderr, "argc = %d\n", argc);

	for (int i = 0; i < argc; i++) {
		fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
	}

	if (argc >= 3 && strncmp(argv[1], "zzz", strlen("zzz")) == 0) {
		int sleep_sec = sleep_sec = atoi(argv[2]);
		sleep(sleep_sec);
	}

	fprintf(stderr, "done!\n");

	return 0;
}
