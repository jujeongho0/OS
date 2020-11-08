#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;

/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;

bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}
	
	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	struct process *next = NULL;
	struct process *p;

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		return current;
	}

pick_next:

	if (!list_empty(&readyqueue)) 
	{	
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(p, &readyqueue, list) 
		{
			if(next->lifespan > p->lifespan)
				next = p;
		}

		list_del_init(&next->list);
	}

	return next;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, 
	.release = fcfs_release, 
	.schedule = sjf_schedule, 
};


/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
static struct process *srtf_schedule(void)
{	
	struct process *next = NULL;
	struct process *p;
	
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		if (!list_empty(&readyqueue)) 
		{	
			next = list_first_entry(&readyqueue, struct process, list);
			list_for_each_entry(p, &readyqueue, list) 
			{
				if(next->lifespan - next->age > p->lifespan - p->age)
					next = p;
			}

			if(next->lifespan - next->age < current->lifespan - current->age)//preemtive
			{
				list_del_init(&next->list);
				list_add_tail(&current->list, &readyqueue);
				return next;
			}			
		}
		return current;
	}
	
	

pick_next:

	if (!list_empty(&readyqueue)) 
	{	
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(p, &readyqueue, list) 
		{
			if(next->lifespan - next->age > p->lifespan - p->age)
				next = p;
		}

		list_del_init(&next->list);
	}

	return next;
}

struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, 
	.release = fcfs_release, 
	.schedule = srtf_schedule,	
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void)
{
	struct process *next = NULL;

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		if (!list_empty(&readyqueue)) 
		{	
			next = list_first_entry(&readyqueue, struct process, list);		
			list_del_init(&next->list);
			list_add_tail(&current->list, &readyqueue);
			return next;
		}					
		return current;
	}	

pick_next:

	if (!list_empty(&readyqueue)) 
	{	
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
	}

	return next;	
}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, 
	.release = fcfs_release, 
	.schedule = rr_schedule,	
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) 
	{		
		r->owner = current;
		return true;
	}

	current->status = PROCESS_WAIT;
	list_add_tail(&current->list, &r->waitqueue);

	return false;
}

void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	assert(r->owner == current); // r->owner != current -> program finished

	r->owner = NULL;
	struct process *p;

	if (!list_empty(&r->waitqueue)) 
	{
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);
		assert(waiter->status == PROCESS_WAIT); //waiter->status != PROCESS_WAIT -> program finished
		list_for_each_entry(p, &r->waitqueue, list) 
		{
			if(waiter->prio < p->prio)
					waiter = p;
		}

		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}	

static struct process *prio_schedule(void)
{	
	struct process *next = NULL;
	struct process *p;	

	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	if (current->age < current->lifespan) {
		if (!list_empty(&readyqueue)) 
		{	
			next = list_first_entry(&readyqueue, struct process, list);	
			list_for_each_entry(p, &readyqueue, list) 
			{
				if(next->prio < p->prio)
					next = p;
			}	
			if(current->prio > next->prio)
				return current;

			list_del_init(&next->list);
			list_add_tail(&current->list, &readyqueue);
			return next;
		}					
		return current;
	}	

pick_next:

	if (!list_empty(&readyqueue)) 
	{	
		next = list_first_entry(&readyqueue, struct process, list);
		list_for_each_entry(p, &readyqueue, list) 
		{
			if(next->prio < p->prio)
				next = p;
		}

		list_del_init(&next->list);
	}

	return next;	
}
//acqure 1 2 4   (resource id / at / duration)
struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = prio_acquire, 
	.release = prio_release, 
	.schedule = prio_schedule,
};


/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
int prio_tmp1[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) 
	{		
		r->owner = current;
		prio_tmp1[r->owner->pid-1] = r->owner->prio;
		r->owner->prio = MAX_PRIO;
		return true;
	}

	current->status = PROCESS_WAIT;
	list_add_tail(&current->list, &r->waitqueue);

	return false;
}

void pcp_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	assert(r->owner == current); // r->owner != current -> program finished
	if(prio_tmp1[r->owner->pid-1] >= 0 )
		r->owner->prio = prio_tmp1[r->owner->pid-1];
	r->owner = NULL;
	struct process *p;

	if (!list_empty(&r->waitqueue)) 
	{
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);
		assert(waiter->status == PROCESS_WAIT); //waiter->status != PROCESS_WAIT -> program finished
		list_for_each_entry(p, &r->waitqueue, list) 
		{
			if(waiter->prio < p->prio)
					waiter = p;
		}

		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = pcp_acquire, 
	.release = pcp_release, 
	.schedule = prio_schedule,
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
int prio_tmp2[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) 
	{		
		r->owner = current;
		return true;
	}
	if(r->owner->prio < current->prio)
	{	
		prio_tmp2[r->owner->pid-1] = r->owner->prio_orig;
		r->owner->prio = current->prio;
	}
	current->status = PROCESS_WAIT;
	list_add_tail(&current->list, &r->waitqueue);

	return false;
}

void pip_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	assert(r->owner == current); // r->owner != current -> program finished
	if(prio_tmp2[r->owner->pid-1] >= 0 )
		r->owner->prio = prio_tmp2[r->owner->pid-1];
	r->owner = NULL;
	struct process *p;

	if (!list_empty(&r->waitqueue)) 
	{
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);
		assert(waiter->status == PROCESS_WAIT); //waiter->status != PROCESS_WAIT -> program finished
		list_for_each_entry(p, &r->waitqueue, list) 
		{
			if(waiter->prio < p->prio)
					waiter = p;
		}

		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}	

struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.acquire = pip_acquire, 
	.release = pip_release, 
	.schedule = prio_schedule,
};
