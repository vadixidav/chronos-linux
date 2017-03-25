/* chronos/set_scheduler.c
 *
 * Sys_setscheduler system call, used for setting real-time schedulers
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/cpumask.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>

/* Set the real-time scheduling algorithm to use for SCHED_CHRONOS tasks */

SYSCALL_DEFINE4(set_scheduler, int, rt_sched, int, prio, unsigned int, len,
		unsigned long __user, *user_mask_ptr)
{
	int ret = -EINVAL;
#ifdef CONFIG_CHRONOS
	int flags = rt_sched & SCHED_FLAGS_MASK;
	int scheduler = (rt_sched >> 8) & 0xFF;
	struct rt_sched_local *l_sched = NULL;
	struct rt_sched_global *g_sched = NULL;

	if(scheduler & SCHED_GLOBAL_MASK) {
		g_sched = get_global_scheduler(scheduler);
		if(g_sched) {
			l_sched = get_local_scheduler(g_sched->local);
		}
	} else
		l_sched = get_local_scheduler(scheduler);

	if(!l_sched) {
		if(g_sched)
			printk("set_scheduler failed: local scheduler not found for global scheduler %s\n", g_sched->base.name);
		else
			printk("set_scheduler failed: scheduler not found!\n");
		goto out;
	}

	l_sched->flags = flags;

	ret = set_scheduler_mask_user(l_sched, g_sched, len, user_mask_ptr, prio);
out:
#endif
	return ret;
}

