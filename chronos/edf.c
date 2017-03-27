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

struct rt_info* sched_edf(struct list_head *head, int flags)
{
	struct list_head *curr;
	struct rt_info *best_task = local_task(head->next), *curr_task;
	struct timespec btspec, ctspec;
	sub_ts(&best_task->deadline, &best_task->left, &btspec);
	list_for_each(curr, head) {
		curr_task = local_task(curr);
		sub_ts(&curr_task->deadline, &curr_task->left, &ctspec);
		if (compare_ts(&ctspec, &btspec)) {
			btspec = ctspec;
			best_task = curr_task;
		}
	}

	return best_task;
}

struct rt_sched_local edf = {
	.base.name = "EDF",
	.base.id = SCHED_RT_EDF,
	.flags = 0,
	.schedule = sched_edf,
	.base.sort_key = SORT_KEY_PERIOD,
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

MODULE_DESCRIPTION("EDF Single-Core Scheduling Module for ChronOS");
MODULE_AUTHOR("Geordon Worley <vadixidav@gmail.com>");
MODULE_LICENSE("GPL");

