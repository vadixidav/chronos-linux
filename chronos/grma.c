/* chronos/grma.c
 *
 * Global RMA Scheduler Module for ChronOS
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

struct rt_info * sched_grma(struct list_head *head, struct global_sched_domain *g)
{
	int count = 1, cpus = count_global_cpus(g);
	struct rt_info *it, *lowest = get_global_task(head->next);

	it = lowest;
	INIT_LIST_HEAD(&lowest->task_list[SCHED_LIST1]);

	list_for_each_entry_continue(it, head, task_list[GLOBAL_LIST]) {
		count++;
		list_add_before(lowest, it, SCHED_LIST1);

		if(count == cpus)
			break;
	}

	return lowest;
}

struct rt_sched_global grma = {
	.base.name = "GRMA",
	.base.id = SCHED_RT_GRMA,
	.schedule = sched_grma,
	.preschedule = presched_stw_generic,
	.arch = &rt_sched_arch_stw,
	.local = SCHED_RT_FIFO,
	.base.sort_key = SORT_KEY_PERIOD,
	.base.list = LIST_HEAD_INIT(grma.base.list)
};

static int __init grma_init(void)
{
	return add_global_scheduler(&grma);
}
module_init(grma_init);

static void __exit grma_exit(void)
{
	remove_global_scheduler(&grma);
}
module_exit(grma_exit);

MODULE_DESCRIPTION("Global RMA Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

