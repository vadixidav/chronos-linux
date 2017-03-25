/* chronos/chronos_seg.c
 *
 * Start and end real-time scheduling segments
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/cpumask.h>
#include <linux/linkage.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/chronos_sched.h>

#ifdef CONFIG_CHRONOS

unsigned long set_ts_from_user(struct timespec *dst, struct timespec __user *src)
{
	if(copy_from_user(dst, src, sizeof(struct timespec)))
		return -EFAULT;

	return 0;
}

/* Begin a real-time segment for a given thread
 * If we want to end one segment and immediately begin a new segment, just make
 * a single begin call, since it will erase all the old data. The only problem
 * right now is that it won't be properly accounted in the sched_stats.
 */
unsigned long begin_rt_seg(struct rt_data __user *data, struct task_struct *p,
			   struct rt_info *task)
{
	int ret = 0;
	struct sched_param param;

	/* Kill all flags, except whether it is has an abort handler or not  */
	task_and_flag(task, HUA);

	/* Initialize the deadline and period */
	ret |= set_ts_from_user(&task->deadline, data->deadline);
	ret |= set_ts_from_user(&task->period, data->period);

	/* Initialize the execution time, schedule, utility, and IVD */
	task->exec_time = data->exec_time;
	task->max_util = data->max_util;
	task->local_ivd = data->max_util == 0 ? LONG_MAX : data->exec_time/data->max_util;
	task->global_ivd = task->local_ivd;
	task->seg_start_us = jiffies_to_usecs(p->utime + p->stime);

	/* Initialize things that shouldn't have a value yet */
	task->dep = NULL;
	task->requested_resource = NULL;

	/* Initialize cpu to -1, since this task hasn't been selected yet. */
	task->cpu = -1;

	/* Make sure the task isn't set to be aborting */
	clear_task_aborting(p->pid);

	param.sched_priority = data->prio;
	sched_setscheduler_nocheck(p, SCHED_CHRONOS, &param);
	force_sched_event(p);
	schedule();
	return ret;
}

/* End a real-time segment for a given thread */
unsigned long end_rt_seg(struct rt_data __user *data, struct task_struct *p,
			 struct rt_info *task)
{
	struct sched_param param;
	int policy, oldprio;

	if(data->prio) {
		param.sched_priority = data->prio;
		policy = SCHED_FIFO;
	} else {
		param.sched_priority = DEFAULT_PRIO;
		policy = SCHED_NORMAL;
	}

	oldprio = p->prio;
	sched_setscheduler_nocheck(p, policy, &param);
	force_sched_event(p);
	if(oldprio >= param.sched_priority)
		schedule();

	/* Clear abort info, so it'll be clean for the next begin_rt_seg  */
	task->abortinfo.deadline.tv_sec = 0;
	task->abortinfo.deadline.tv_nsec = 0;
	task->abortinfo.exec_time = 0;
	task->abortinfo.max_util = 0;
	task_init_flags(task);

	return 0;
}

/* Add an abort handler to a task
 * If NULL is given for the deadline, then it is
 * assumed to be infinite
 */
unsigned long add_abort_handler(struct rt_data __user *data, struct task_struct *p,
				struct rt_info *task)
{
	task->abortinfo.exec_time = data->exec_time;
	task->abortinfo.max_util = data->max_util;
	task_set_flag(task, HUA);
	return set_ts_from_user(&task->abortinfo.deadline, data->deadline);
}
#endif

SYSCALL_DEFINE2(do_rt_seg, int, op, struct rt_data __user, *data)
{
	struct task_struct *p;

	/* We have to check this every time, so just do it here */
	if(!data || !access_ok(VERIFY_READ, data, sizeof(*data)))
		return -EFAULT;

	if(!data->tid)
		p = current;
	else
		p = find_task_by_vpid(data->tid);

	switch(op) {
#ifdef CONFIG_CHRONOS
		case RT_SEG_BEGIN:
			return begin_rt_seg(data, p, &p->rtinfo);
		case RT_SEG_END:
			return end_rt_seg(data, p, &p->rtinfo);
		case RT_SEG_ADD_ABORT:
			return add_abort_handler(data, p, &p->rtinfo);
#endif
		default:
			return -EINVAL;
	}
}

