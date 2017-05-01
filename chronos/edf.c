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
#include <linux/chronos_util.h>
#include <linux/list.h>

struct rt_info* sched_edf(struct list_head *head, int flags)
{
	struct rt_info *best = local_task(head->next);

	if(flags & SCHED_FLAG_PI)
		best = get_pi_task(best, head, flags);

	return best;
}

struct rt_sched_local edf = {
	.base.name = "EDF",
	.base.id = SCHED_RT_EDF,
	.flags = 0,
	.schedule = sched_edf,
	.base.sort_key = SORT_KEY_DEADLINE,
	.base.list = LIST_HEAD_INIT(edf.base.list)
};

static int __init edf_init(void)
{
	return add_local_scheduler(&edf);
}
module_init(edf_init);

static void __exit edf_exit(void)
{
	remove_local_scheduler(&edf);
}
module_exit(edf_exit);

MODULE_DESCRIPTION("EDF Scheduling Module for ChronOS");
MODULE_AUTHOR("Geordon Worley <vadixidav@gmail.com>");
MODULE_LICENSE("GPL");

