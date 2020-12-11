#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "types.h"
#include "list_head.h"
#include "vm.h"

/**
 * Ready queue of the system
 */
extern struct list_head processes;

/**
 * Currently running process
 */
extern struct process *current;

/**
 * Page Table Base Register that MMU will walk through for address translation
 */
extern struct pagetable *ptbr;


/**
 * The number of mappings for each page frame. Can be used to determine how
 * many processes are using the page frames.
 */
extern unsigned int mapcounts[];


/**
 * alloc_page(@vpn, @rw)
 *
 * DESCRIPTION
 *   Allocate a page frame that is not allocated to any process, and map it
 *   to @vpn. When the system has multiple free pages, this function should
 *   allocate the page frame with the **smallest pfn**.
 *   You may construct the page table of the @current process. When the page
 *   is allocated with RW_WRITE flag, the page may be later accessed for writes.
 *   However, the pages populated with RW_READ only should not be accessed with
 *   RW_WRITE accesses.
 *
 * RETURN
 *   Return allocated page frame number.
 *   Return -1 if all page frames are allocated.
 */

//vpn    10 -> 10   0x10 -> 0x10 , 16
//rw     r=1 w=2 rw=3
unsigned int alloc_page(unsigned int vpn, unsigned int rw)  
{
	int small_pfn=-1;
	for(int i=0;i<NR_PAGEFRAMES;i++)
	{
		if(mapcounts[i] == 0)
		{
			small_pfn = i;
			break;
		}
	}
	if(small_pfn == -1) // all page frames are allocated
		return -1;
	
	int pd_index = vpn / NR_PTES_PER_PAGE; //NR_PTES_PER_PAGE = 16
	int pte_index = vpn % NR_PTES_PER_PAGE;
	
	if (!current->pagetable.outer_ptes[pd_index]) 
	{
		current->pagetable.outer_ptes[pd_index] = malloc(sizeof(struct pte_directory));
	}
	
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].valid = true;

	if(rw==1)
		current->pagetable.outer_ptes[pd_index]->ptes[pte_index].writable = false;
	else
		current->pagetable.outer_ptes[pd_index]->ptes[pte_index].writable = true;

	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn = small_pfn;	
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].private = rw;

	mapcounts[small_pfn]++;

	return small_pfn;	
}


/**
 * free_page(@vpn)
 *
 * DESCRIPTION
 *   Deallocate the page from the current processor. Make sure that the fields
 *   for the corresponding PTE (valid, writable, pfn) is set @false or 0.
 *   Also, consider carefully for the case when a page is shared by two processes,
 *   and one process is to free the page.
 */
void free_page(unsigned int vpn)
{
	int pd_index = vpn / NR_PTES_PER_PAGE; //NR_PTES_PER_PAGE = 16
	int pte_index = vpn % NR_PTES_PER_PAGE;
			
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].valid = false;

	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].writable = false;

	mapcounts[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn]--;
	current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn = 0;		
}

/**
 * handle_page_fault()
 *
 * DESCRIPTION
 *   Handle the page fault for accessing @vpn for @rw. This function is called
 *   by the framework when the __translate() for @vpn fails. This implies;
 *   0. page directory is invalid
 *   1. pte is invalid
 *   2. pte is not writable but @rw is for write
 *   This function should identify the situation, and do the copy-on-write if
 *   necessary.
 *
 * RETURN
 *   @true on successful fault handling
 *   @false otherwise
 */
bool handle_page_fault(unsigned int vpn, unsigned int rw)
{
	int pd_index = vpn / NR_PTES_PER_PAGE; //NR_PTES_PER_PAGE = 16
	int pte_index = vpn % NR_PTES_PER_PAGE;

	if(current->pagetable.outer_ptes[pd_index]->ptes[pte_index].private == 3)
	{
		if(mapcounts[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn] > 1)
		{		
			int small_pfn=-1;
			for(int i=0;i<NR_PAGEFRAMES;i++)
			{
				if(mapcounts[i] == 0)
				{
					small_pfn = i;
					break;
				}
			}
			if(small_pfn == -1) // all page frames are allocated
				return false;

			mapcounts[current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn]--;

			current->pagetable.outer_ptes[pd_index]->ptes[pte_index].writable = true;

			current->pagetable.outer_ptes[pd_index]->ptes[pte_index].pfn = small_pfn;
			mapcounts[small_pfn]++;

			return true;
		}
		else
		{
			current->pagetable.outer_ptes[pd_index]->ptes[pte_index].writable = true;

			return true;
		}
		
	}

	return false;
}


/**
 * switch_process()
 *
 * DESCRIPTION
 *   If there is a process with @pid in @processes, switch to the process.
 *   The @current process at the moment should be put into the @processes
 *   list, and @current should be replaced to the requested process.
 *   Make sure that the next process is unlinked from the @processes, and
 *   @ptbr is set properly.
 *
 *   If there is no process with @pid in the @processes list, fork a process
 *   from the @current. This implies the forked child process should have
 *   the identical page table entry 'values' to its parent's (i.e., @current)
 *   page table. 
 *   To implement the copy-on-write feature, you should manipulate the writable
 *   bit in PTE and mapcounts for shared pages. You may use pte->private for 
 *   storing some useful information :-)
 */
void switch_process(unsigned int pid)
{
	struct process *target;
	struct process *p;
	int target_flag = 0;
	
	if(!list_empty(&processes))
	{		
		list_for_each_entry(p,&processes,list)
		{
			if(p->pid == pid)
			{
				target = p;
				target_flag = 1;
				break;
			}
		}
	}

	if(target_flag == 1)
	{		
		list_del_init(&target->list);
		list_add_tail(&current->list,&processes);
		current = target;
		ptbr = &current->pagetable;
	}
	else
	{
		struct process *new = malloc(sizeof(struct process));
		new->pid = pid;
		int malloc_flag=0;

		for(int i=0;i<NR_PTES_PER_PAGE;i++)
		{
			if(!current->pagetable.outer_ptes[i])
				continue;
			
			malloc_flag = 0;
			for(int j=0;j<NR_PTES_PER_PAGE;j++)
			{
				if(!current->pagetable.outer_ptes[i]->ptes[j].valid)
					continue;
				
				if(malloc_flag==0)
				{
					new->pagetable.outer_ptes[i] = malloc(sizeof(struct pte_directory));
					malloc_flag++;
				}
				new->pagetable.outer_ptes[i]->ptes[j].valid = current->pagetable.outer_ptes[i]->ptes[j].valid;
				new->pagetable.outer_ptes[i]->ptes[j].writable = false;
				current->pagetable.outer_ptes[i]->ptes[j].writable = false;
				new->pagetable.outer_ptes[i]->ptes[j].pfn = current->pagetable.outer_ptes[i]->ptes[j].pfn;
				new->pagetable.outer_ptes[i]->ptes[j].private = current->pagetable.outer_ptes[i]->ptes[j].private;	
				mapcounts[new->pagetable.outer_ptes[i]->ptes[j].pfn]++;					
			}
		}
		
		list_add_tail(&current->list,&processes);
		current = new;
		ptbr = &new->pagetable;
	}	
}