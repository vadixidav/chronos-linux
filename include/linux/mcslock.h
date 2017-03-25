/* include/linux/mcslock.h
 *
 * Generic implementation of the MCS queue-based lock, as introduced by
 * http://doi.acm.org/10.1145/103727.103729
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *	- Aaron Lindsay, aaron@aclindsay.com
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#ifndef MCSLOCK_H
#define MCSLOCK_H

#include <asm/mcslock.h>

typedef struct mcs_lock {
	arch_mcs_lock_t raw_lock;
} mcs_lock_t;

typedef struct mcs_qnode {
	arch_mcs_node_t raw_node;
} mcs_node_t;


static inline void mcs_lock_init(mcs_lock_t *lock)
{
	arch_mcs_lock_init(&lock->raw_lock);
}

static inline void mcs_node_init(mcs_node_t *node)
{
	arch_mcs_node_init(&node->raw_node);
}

static inline int mcs_is_locked(mcs_lock_t *lock)
{
	return arch_mcs_is_locked(&lock->raw_lock);
}

static inline int mcs_trylock(mcs_lock_t *lock, mcs_node_t *node)
{
	return arch_mcs_trylock(&lock->raw_lock, &node->raw_node);
}

static inline void mcs_lock(mcs_lock_t *lock, mcs_node_t *node)
{
	arch_mcs_lock(&lock->raw_lock, &node->raw_node);
}

static inline void mcs_unlock(mcs_lock_t *lock, mcs_node_t *node)
{
	arch_mcs_unlock(&lock->raw_lock, &node->raw_node);
}
#endif /* MCSLOCK_H */
