/* chronos/chronos_mutex.c
 *
 * Scheduler-managed mutex to be used by real-time tasks
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 *
 * Based on normal futexes, (C) Rusty Russell.
 *
 * Quick guide:
 * We are using a data structure (struct mutex_data, in linux/chronos_types.h)
 * as a mutex. The user creates it through C/C++/Java/etc, and then passes the
 * pointer down in a system call.
 */

#include <asm/current.h>
#include <asm/atomic.h>
#include <asm/futex.h>
#include <linux/futex.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/chronos_types.h>

#ifdef CONFIG_CHRONOS

#include <linux/chronos_sched.h>
#include "chronos_mutex_stats.c"

/* The reason we need a list at all is that A) we need the address we're
 * offsetting from in the userspace to be a slab-allocated address so that it
 * isn't findable via objdumping the vmlinx, and B) we need a way to reference
 * the memory to free it if the application dies without properly informing us,
 * since we have to assume userspace is unreliable.
 */

struct process_mutex_list {
	pid_t tgid;
	struct list_head p_list;
	struct list_head m_list;
	rwlock_t lock;
};

static LIST_HEAD(chronos_mutex_list);
static DEFINE_RWLOCK(chronos_mutex_list_lock);

static struct process_mutex_list * find_by_tgid(pid_t pid)
{
	struct list_head *curr;
	struct process_mutex_list *entry, *ret = NULL;

	read_lock(&chronos_mutex_list_lock);
	list_for_each(curr, &chronos_mutex_list) {
		entry = list_entry(curr, struct process_mutex_list, p_list);
		if(entry->tgid == pid) {
			ret = entry;
			break;
		}
	}
	read_unlock(&chronos_mutex_list_lock);

	return ret;
}

static struct mutex_head * find_in_process(struct mutex_data *m, struct process_mutex_list * process)
{
	struct mutex_head *head = (struct mutex_head *)((unsigned long)process + m->id);

	if(head->id != m->id)
		head = NULL;

	return head;
}

static struct mutex_head * find_mutex(struct mutex_data *m, int tgid)
{
	struct process_mutex_list *p = NULL;

	p = find_by_tgid(tgid);
	return find_in_process(m, p);
}

static void futex_wait(u32 __user *uaddr, u32 val)
{
	do_futex(uaddr, FUTEX_WAIT, val, NULL, NULL, 0, 0);
}

/* It might seem counter-intutitive that we're only waking one task, rather than
 * all of them, since all of them should be eligeble for being run. However, if
 * we get here, the scheduler isn't managing wake-ups, and so we do this to
 * reduce unnecessary wakeups.
 */
static void futex_wake(u32 __user *uaddr)
{
	do_futex(uaddr, FUTEX_WAKE, 1, NULL, NULL, 0, 0);
}

static int init_rt_resource(struct mutex_data __user *mutexreq)
{
	struct process_mutex_list *process = find_by_tgid(current->tgid);
	struct mutex_head *m = kmalloc(sizeof(struct mutex_head), GFP_KERNEL);

	if(!m)
		return -ENOMEM;

	m->mutex = mutexreq;
	m->owner_t = NULL;

	if(!process) {
		process = kmalloc(sizeof(struct process_mutex_list), GFP_KERNEL);

		if(!process) {
			kfree(m);
			return -ENOMEM;
		}

		write_lock(&chronos_mutex_list_lock);
		list_add(&process->p_list, &chronos_mutex_list);
		write_unlock(&chronos_mutex_list_lock);

		process->tgid = current->tgid;
		INIT_LIST_HEAD(&process->m_list);
		rwlock_init(&process->lock);
		cmutexstat_inc(processes);
	}

	write_lock(&process->lock);
	list_add(&m->list, &process->m_list);
	write_unlock(&process->lock);

	mutexreq->id = (unsigned long)m - (unsigned long)process;
	m->id = mutexreq->id;
	cmutexstat_inc(locks);

	return 0;
}

static int destroy_rt_resource(struct mutex_data __user *mutexreq)
{
	int empty;
	struct process_mutex_list *process = find_by_tgid(current->tgid);
	struct mutex_head *m = find_in_process(mutexreq, process);

	if(!m || !process)
		return 1;

	// Remove the mutex_head
	write_lock(&process->lock);
	list_del(&m->list);
	empty = list_empty(&process->m_list);
	write_unlock(&process->lock);
	kfree(m);

	if(empty) {
		write_lock(&chronos_mutex_list_lock);
		list_del(&process->p_list);
		write_unlock(&chronos_mutex_list_lock);
		kfree(process);
		cmutexstat_dec(processes);
	}

	cmutexstat_dec(locks);

	return 0;
}

/* Returning 0 means everything was fine, returning > -1 means we got the lock */
static int request_rt_resource(struct mutex_data __user *mutexreq)
{
	int c, ret = 0;
	struct rt_info *r = &current->rtinfo;
	struct mutex_head *m;

	/* This is for reentrant locking */
	if(mutexreq->owner == current->pid)
		return 0;
	else if(check_task_abort_nohua(r))
		return -EOWNERDEAD;

	m = find_mutex(mutexreq, current->tgid);
	if(!m)
		return -EINVAL;

	/* Notify that we are requesting the resource and call the scheduler */
	r->requested_resource = m;
	force_sched_event(current);
	schedule();

	/* Our request may have been cancelled for some reason */
	if(r->requested_resource != m)
		return -EOWNERDEAD;

	/* Try to take the resource */
	if((c = cmpxchg(&(mutexreq->value), 0, 1)) != 0) {
		ret = 1;
		do {
			if(c == 2 || cmpxchg(&(mutexreq->value), 1, 2) != 1) {
				futex_wait(&(mutexreq->value), 2); }
		} while((c = cmpxchg(&(mutexreq->value), 0, 2)) != 0);
		cmutexstat_inc(locking_failure);
	} else
		cmutexstat_inc(locking_success);

	mutexreq->owner = current->pid;
	m->owner_t = r;
	r->requested_resource = NULL;

	return ret;
}

static int release_rt_resource(struct mutex_data __user *mutexreq)
{
	struct mutex_head *m = find_mutex(mutexreq, current->tgid);

	if(!m)
		return -EINVAL;

	if(mutexreq->owner != current->pid)
		return -EACCES;

	mutexreq->owner = 0;
	m->owner_t = NULL;

	if(cmpxchg(&(mutexreq->value), 1, 0) == 2) {
		mutexreq->value = 0;
		futex_wake(&(mutexreq->value));
	}

	force_sched_event(current);
	schedule();

	return 0;
}
#endif

SYSCALL_DEFINE2(do_chronos_mutex, struct mutex_data __user, *mutexreq, int, op)
{
	/* We have to check this every time, so just do it here */
	if(!mutexreq || !access_ok(VERIFY_WRITE, mutexreq, sizeof(*mutexreq)))
		return -EFAULT;

	switch(op) {
#ifdef CONFIG_CHRONOS
		case CHRONOS_MUTEX_REQUEST:
			return request_rt_resource(mutexreq);
		case CHRONOS_MUTEX_RELEASE:
			return release_rt_resource(mutexreq);
		case CHRONOS_MUTEX_INIT:
			return init_rt_resource(mutexreq);
		case CHRONOS_MUTEX_DESTROY:
			return destroy_rt_resource(mutexreq);
#endif
		default:
			return -EINVAL;
	}
}

