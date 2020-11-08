#ifndef __RESOURCE_H__
#define __RESOURCE_H__

struct process;
struct list_head;

/**
 * Resources in the system.
 */
struct resource {
	/**
	 * The owner process of this resource. NULL implies the resource is free
	 * whereas non-NULL implies @owner process owns this resource
	 */
	struct process *owner;

	/**
	 * list head to list processes that are wanting for the resource
	 */
	struct list_head waitqueue;
};

/**
 * This system has 32 different resources. It is defined in pa2.c as an array of
 * struct resource (i.e., struct resource resources[NR_RESOURCES];)
 */
#define NR_RESOURCES 32

#endif
