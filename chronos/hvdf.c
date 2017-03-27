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

struct rt_info* sched_hvdf(struct list_head *head, int flags)
{
	struct list_head *curr;
	struct rt_info *best_task = local_task(head->next), *curr_task;
	livd(best_task, 0, flags);
	list_for_each(curr, head) {
		curr_task = local_task(curr);
		if (check_task_aborted(curr_task)) {
			return curr_task;
		}

		livd(curr_task, 0, flags);
		switch (curr_task->local_ivd) {
		case -1:
			abort_thread(curr_task);
			// Fall through into returning curr_task.
		case -2:
		case LONG_MAX:
			return curr_task;
		default:
			if (curr_task->local_ivd < best_task->local_ivd) {
				best_task = curr_task;
			}
			break;
		}
	}

	return best_task;
}

struct rt_sched_local hvdf = {
	.base.name = "HVDF",
	.base.id = SCHED_RT_HVDF,
	.flags = 0,
	.schedule = sched_hvdf,
	.base.sort_key = SORT_KEY_PERIOD,
	.base.list = LIST_HEAD_INIT(hvdf.base.list)
};

static int __init hvdf_init(void)
{
	return add_local_scheduler(&hvdf);
}
module_init(hvdf_init);

static void __exit hvdf_exit(void)
{
	remove_local_scheduler(&hvdf);
}
module_exit(hvdf_exit);

MODULE_DESCRIPTION("HVDF Scheduling Module for ChronOS");
MODULE_AUTHOR("Geordon Worley <vadixidav@gmail.com>");
MODULE_LICENSE("GPL");

