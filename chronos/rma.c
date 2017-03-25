/* chronos/rma.c
 *
 * RMA Single-Core Scheduler Module for ChronOS
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

struct rt_info* sched_rma(struct list_head *head, int flags)
{
	struct rt_info *best = local_task(head->next);

	if(flags & SCHED_FLAG_PI)
		best = get_pi_task(best, head, flags);

	return best;
}

struct rt_sched_local rma = {
	.base.name = "RMA",
	.base.id = SCHED_RT_RMA,
	.flags = 0,
	.schedule = sched_rma,
	.base.sort_key = SORT_KEY_PERIOD,
	.base.list = LIST_HEAD_INIT(rma.base.list)
};

static int __init rma_init(void)
{
	return add_local_scheduler(&rma);
}
module_init(rma_init);

static void __exit rma_exit(void)
{
	remove_local_scheduler(&rma);
}
module_exit(rma_exit);

MODULE_DESCRIPTION("RMA Single-Core Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

