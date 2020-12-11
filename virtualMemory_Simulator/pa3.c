#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <ctype.h>
#include <inttypes.h>
#include <strings.h>

#include "types.h"
#include "parser.h"

#include "list_head.h"
#include "vm.h"

static bool verbose = true;


/**
 * Initial process
 */
static struct process init = {
	.pid = 0,
	.list = LIST_HEAD_INIT(init.list),
	.pagetable = {
		.outer_ptes = { NULL },
	},
};

/**
 * Current process. Should not be listed in the @processes
 */
struct process *current = &init;

/**
 * Ready queue. Put @current process to the tail of this list on
 * switch_process(). Don't forget to remove the switched process from the list.
 */
LIST_HEAD(processes);

/**
 * Page table base register
 */
struct pagetable *ptbr = NULL;

/**
 * Map count for each page frame
 */
unsigned int mapcounts[NR_PAGEFRAMES] = { 0 };


extern unsigned int alloc_page(unsigned int vpn, unsigned int rw);
extern void free_page(unsigned int vpn);
extern bool handle_page_fault(unsigned int vpn, unsigned int rw);
extern void switch_process(unsigned int pid);


/**
 * __translate()
 *
 * DESCRIPTION
 *   This function simulates the address translation in the processor.
 *   It translates @vpn to @pfn using the page table pointed by @ptbr.
 *
 * RETURN
 *   @true on successful translation
 *   @false if unable to translate. This includes the case when the page access
 *   is for write (indicated in @rw), but the @writable of the pte is @false.
 */
static bool __translate(unsigned int rw, unsigned int vpn, unsigned int *pfn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE;
	int pte_index = vpn % NR_PTES_PER_PAGE;

	struct pagetable *pt = ptbr;
	struct pte_directory *pd;
	struct pte *pte;

	/***
	 * Advanced tasks: Implement TLB hook here
	 */

	/* Page table is invalid */
	if (!pt) return false;
	
	pd = pt->outer_ptes[pd_index];
	
	/* Page directory does not exist */
	if (!pd) return false;
	
	pte = &pd->ptes[pte_index];

	/* PTE is invalid */
	if (!pte->valid) return false;

	/* Unable to handle the write access */
	if (rw == RW_WRITE) {
		if (!pte->writable) return false;
	}
	*pfn = pte->pfn;

	return true;
}

/**
 * __access_memory
 *
 * DESCRIPTION
 *   Simulate the MMU in the processor and call page fault handler
 *   if necessary.
 *
 * RETURN
 *   @true on successful access
 *   @false if unable to access @vpn for @rw
 */
static bool __access_memory(unsigned int vpn, unsigned int rw)
{
	unsigned int pfn;
	int ret;
	int nr_retries = 0;

	/* Cannot read and write at the same time!! */
	assert((rw & RW_READ) ^ (rw & RW_WRITE));

	/**
	 * We have NR_PTES_PER_PAGE entries in the outer table and so do for
	 * inner page table. Thus each process can have up to NR_PTES_PER_PAGE^2
	 * as its VPN
	 */
	assert(vpn < NR_PTES_PER_PAGE * NR_PTES_PER_PAGE);

	do {
		/* Ask MMU to translate VPN */
		if (__translate(rw, vpn, &pfn)) {
			/* Success on address translation */
			fprintf(stderr, "%3u --> %-3u\n", vpn, pfn);
			return true;
		}

		/**
		 * Failed to translate the address. So, call OS through the page fault
		 * and restart the translation if the fault is successfully handled.
		 * Count the number of retries to prevent buggy translation.
		 */
		nr_retries++;
	} while ((ret = handle_page_fault(vpn, rw)) == true && nr_retries < 2);

	if (ret == false) {
		fprintf(stderr, "Unable to access %u\n", vpn);
	}

	return ret;
}

static unsigned int __make_rwflag(const char *rw) // r=1 w=2 rw=3
{
	int len = strlen(rw);
	unsigned int rwflag = 0;

	for (int i = 0; i < len; i++) {
		if (rw[i] == 'r' || rw[i] == 'R') {
			rwflag |= RW_READ; 
		}
		if (rw[i] == 'w' || rw[i] == 'W') {
			rwflag |= RW_WRITE;
		}
	}
	return rwflag;
}

static bool __alloc_page(unsigned int vpn, unsigned int rw)
{
	unsigned int pfn;

	assert(rw);

	if (__translate(RW_READ, vpn, &pfn)) {
		fprintf(stderr, "%u is already allocated to %u\n", vpn, pfn);
		return false;
	}

	pfn = alloc_page(vpn, rw);
	if (pfn == -1) {
		fprintf(stderr, "memory is full\n");
		return false;
	}
	fprintf(stderr, "alloc %3u --> %-3u\n", vpn, pfn);
	
	return true;
}

static bool __free_page(unsigned int vpn)
{
	unsigned int pfn;

	if (!__translate(RW_READ, vpn, &pfn)) {
		fprintf(stderr, "%u is not allocated\n", vpn);
		return false;
	}
	fprintf(stderr, "free %u (pfn %u)\n", vpn, pfn);
	free_page(vpn);

	return true;
}

static void __init_system(void)
{
	ptbr = &init.pagetable;
}

static void __show_pageframes(void)
{
	for (unsigned int i = 0; i < NR_PAGEFRAMES; i++) {
		if (!mapcounts[i]) continue;
		fprintf(stderr, "%3u: %d\n", i, mapcounts[i]);
	}
	fprintf(stderr, "\n");
}

static void __show_pagetable(void)
{
	fprintf(stderr, "\n*** PID %u ***\n", current->pid);

	for (int i = 0; i < NR_PTES_PER_PAGE; i++) {
		struct pte_directory *pd = current->pagetable.outer_ptes[i];

		if (!pd) 
		{
			continue;
		}
		
		for (int j = 0; j < NR_PTES_PER_PAGE; j++) {
			struct pte *pte = &pd->ptes[j];

			if (!verbose && !pte->valid) 
			{	
				continue;
			}
			fprintf(stderr, "%02d:%02d %c%c | %-3d\n", i, j,
				pte->valid ? 'v' : ' ',
				pte->writable ? 'w' : ' ',
				pte->pfn);
		}
		printf("\n");
	}
}

static void __print_help(void)
{
	printf("  help | ?     : Print out this help message \n");
	printf("  exit         : Exit the simulation\n");
	printf("\n");
	printf("  switch [pid] : Do context switch to pid @pid\n");
	printf("                 Fork @pid if there is no process with the pid\n");
	printf("  show         : Show the page table of the current process\n");
	printf("  pages        : Show the status for each page frame\n");
	printf("\n");
	printf("  alloc [vpn] r|w  : Allocate a page for the rw flag\n");
	printf("  free [vpn]       : Deallocate the page at VPN @vpn\n");
	printf("  access [vpn] r|w : Access VPN @vpn for read or write\n");
	printf("  read [vpn]       : Equivalent to access @vpn r\n");
	printf("  write [vpn]      : Equivalent to access @vpn w\n");
	printf("\n");
}

static bool strmatch(char * const str, const char *expect)
{
	return (strlen(str) == strlen(expect)) &&
			(strncmp(str, expect, strlen(expect)) == 0);
}

static void __do_simulation(FILE *input)
{
	char command[MAX_COMMAND_LEN] = { 0 };

	__init_system();

	while (fgets(command, sizeof(command), input)) {
		char *tokens[MAX_NR_TOKENS] = { NULL };
		int nr_tokens = 0;

		/* Make the command lowercase */
		for (size_t i = 0; i < strlen(command); i++) {
			command[i] = tolower(command[i]);
		}

		if (parse_command(command, &nr_tokens, tokens) < 0) {
			continue;
		}
		if (nr_tokens == 0) continue;

		if (nr_tokens == 1) {
			if (strmatch(tokens[0], "exit")) break;
			if (strmatch(tokens[0], "show")) {
				__show_pagetable();
			} else if (strmatch(tokens[0], "pages")) {
				__show_pageframes();
			} else if (strmatch(tokens[0], "help") || strmatch(tokens[0], "?")) {
				__print_help();
			} else {
				printf("Unknown command %s\n", tokens[0]);
			}
		} else if (nr_tokens == 2) {
			unsigned int arg = strtoimax(tokens[1], NULL, 0);

			if (strmatch(tokens[0], "switch") || strmatch(tokens[0], "s")) {
				switch_process(arg);
			} else if (strmatch(tokens[0], "free") || strmatch(tokens[0], "f")) {
				__free_page(arg);
			} else if (strmatch(tokens[0], "read") || strmatch(tokens[0], "r")) {
				__access_memory(arg, RW_READ);
			} else if (strmatch(tokens[0], "write") || strmatch(tokens[0], "w")) {
				__access_memory(arg, RW_WRITE);
			} else {
				printf("Unknown command %s\n", tokens[0]);
			}
		} else if (nr_tokens == 3) {
			unsigned int vpn = strtoimax(tokens[1], NULL, 0);
			unsigned int rw = __make_rwflag(tokens[2]);

			if (strmatch(tokens[0], "alloc") || strmatch(tokens[0], "a")) {
				if (!__alloc_page(vpn, rw)) break;
			} else if (strmatch(tokens[0], "access")) {
				__access_memory(vpn, rw);
			} else {
				printf("Unknown command %s\n", tokens[0]);
			}
		} else {
			assert(!"Unknown command in trace");
		}

		if (verbose) printf(">> ");
	}
}

static void __print_usage(const char * name)
{
	printf("Usage: %s {-q} {-f [workload file]}\n", name);
	printf("\n");
	printf("  -q: Run quietly\n\n");
}

int main(int argc, char * argv[])
{
	int opt;
	FILE *input = stdin;

	while ((opt = getopt(argc, argv, "qh")) != -1) {
		switch (opt) {
		case 'q':
			verbose = false;
			break;
		case 'h':
		default:
			__print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (verbose && !argv[optind]) {
		printf("***************************************************************************\n");
		printf(" __      ____  __     _____ _                 _       _\n");
		printf(" \\ \\    / /  \\/  |   / ____(_)               | |     | |\n");
		printf("  \\ \\  / /| \\  / |  | (___  _ _ __ ___  _   _| | __ _| |_ ___  _ __ \n");
		printf("   \\ \\/ / | |\\/| |   \\___ \\| | '_ ` _ \\| | | | |/ _` | __/ _ \\| '__|\n");
		printf("    \\  /  | |  | |   ____) | | | | | | | |_| | | (_| | || (_) | |   \n");
		printf("     \\/   |_|  |_|  |_____/|_|_| |_| |_|\\__,_|_|\\__,_|\\__\\___/|_|\n");
		printf("\n");
		printf("                                              >> SCE213 2020.12 <<\n");
		printf("\n");
		printf("***************************************************************************\n");
	}

	if (argv[optind]) {
		if (verbose) printf("Use file \"%s\" for input.\n", argv[optind]);

		input = fopen(argv[optind], "r");
		if (!input) {
			fprintf(stderr, "No input file %s\n", argv[optind]);
			return EXIT_FAILURE;
		}
		verbose = false;
	} else {
		if (verbose) printf("Use stdin for input.\n");
	}

	if (verbose) {
		printf("Enter 'help' or '?' for help.\n\n");
		printf(">> ");
	}

	__do_simulation(input);

	if (input != stdin) fclose(input);

	return EXIT_SUCCESS;
}
