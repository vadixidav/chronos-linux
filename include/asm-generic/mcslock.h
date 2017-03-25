/* include/asm-generic/mcslock.h
 *
 * Generic implementation of the MCS queue-based lock, as introduced by
 * http://doi.acm.org/10.1145/103727.103729. This should most likely be
 * optimized for any architecture which relies upon it for performance.
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *	- Aaron Lindsay, aaron@aclindsay.com
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#ifndef ARCH_MCSLOCK_H
#define ARCH_MCSLOCK_H

#include <linux/atomic.h>

typedef struct arch_mcs_qnode * arch_mcs_lock_t;
typedef struct arch_mcs_qnode arch_mcs_node_t;

struct arch_mcs_qnode {
	struct arch_mcs_qnode *next;
	char locked;
};

static __always_inline void arch_mcs_lock_init(arch_mcs_lock_t *lock)
{
	*lock = NULL;
}

static __always_inline void arch_mcs_node_init(arch_mcs_node_t *node)
{
	node->next = NULL;
	node->locked = 0;
}

static __always_inline int arch_mcs_is_locked(arch_mcs_lock_t *lock)
{
	return *lock != NULL;
}

static __always_inline int arch_mcs_trylock(arch_mcs_lock_t *lock, arch_mcs_node_t *node)
{
	return cmpxchg(lock, NULL, node) == NULL;
}

static __always_inline void arch_mcs_lock(arch_mcs_lock_t *lock, arch_mcs_node_t *node)
{
	arch_mcs_node_t *pred = xchg(lock, node);

	if(pred) {
		node->locked = 1;
		pred->next = node;
		while(node->locked)
			cpu_relax();
	}
}

static __always_inline void arch_mcs_unlock(arch_mcs_lock_t *lock, arch_mcs_node_t *node)
{
	if(!node->next) {
		if(cmpxchg(lock, node, NULL) == node)
			return;

		while(!node->next)
			cpu_relax();
	}

	node->next->locked = 0;
	node->next = NULL;
}
#endif /* ARCH_MCSLOCK_H */
