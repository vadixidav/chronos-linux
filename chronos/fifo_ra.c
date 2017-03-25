/* chronos/fifo_ra.c
 *
 * FIFO Resource-Aware Single-Core Scheduler Module for ChronOS
 *
 * Returns the first non-blocked task in the local queue
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/list.h>

struct rt_info* sched_fifo_ra(struct list_head *head, int flags)
{
	struct rt_info *it;

	list_for_each_entry(it, head, task_list[LOCAL_LIST]) {
		initialize_dep(it);
		if(!it->dep)
			return it;
	}

	return NULL;
}

struct rt_sched_local fifo_ra = {
	.base.name = "FIFO_RA",
	.base.id = SCHED_RT_FIFO_RA,
	.flags = 0,
	.schedule = sched_fifo_ra,
	.base.sort_key = SORT_KEY_NONE,
	.base.list = LIST_HEAD_INIT(fifo_ra.base.list)
};

static int __init fifo_ra_init(void)
{
	return add_local_scheduler(&fifo_ra);
}
module_init(fifo_ra_init);

static void __exit fifo_ra_exit(void)
{
	remove_local_scheduler(&fifo_ra);
}
module_exit(fifo_ra_exit);

MODULE_DESCRIPTION("FIFO_RA Single-Core Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

