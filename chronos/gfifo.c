/* chronos/gfifo.c
 *
 * Global FIFO Scheduler Module for ChronOS
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
#include <linux/smp.h>

struct rt_info * sched_gfifo(struct list_head *head, struct global_sched_domain *g)
{
	int cpu = raw_smp_processor_id();
	struct rt_info *it;

	list_for_each_entry(it, head, task_list[GLOBAL_LIST]) {
		if(task_pullable(it, cpu)) {
			_remove_task_global(it, g);
			return it;
		}
	}

	return NULL;
}

struct rt_sched_global gfifo = {
	.base.name = "GFIFO",
	.base.id = SCHED_RT_GFIFO,
	.schedule = sched_gfifo,
	.preschedule = presched_concurrent_generic,
	.arch = &rt_sched_arch_concurrent,
	.local = SCHED_RT_FIFO,
	.base.sort_key = SORT_KEY_NONE,
	.base.list = LIST_HEAD_INIT(gfifo.base.list)
};

static int __init gfifo_init(void)
{
	return add_global_scheduler(&gfifo);
}
module_init(gfifo_init);

static void __exit gfifo_exit(void)
{
	remove_global_scheduler(&gfifo);
}
module_exit(gfifo_exit);

MODULE_DESCRIPTION("Global FIFO Scheduling Module for ChronOS");
MODULE_AUTHOR("Matthew Dellinger <matthew@mdelling.com>");
MODULE_LICENSE("GPL");

