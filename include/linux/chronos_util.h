/* include/linux/chronos_util.h
 *
 * Macros for list management, time calculations, and other real-time
 * specific calls.
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *	- Aaron Lindsay, aaron@aclindsay.com
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#ifndef _CHRONOS_UTIL_H
#define _CHRONOS_UTIL_H

#define BILLION 1000000000
#define MILLION 1000000
#define THOUSAND 1000

#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/time.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>

extern int (*kernel_set_task_aborting) (pid_t pid);
extern int (*kernel_clear_task_aborting) (pid_t pid);

/* Shortcut for fetching rt_info struct from corresponding list_head */
#define task_list_entry(ptr, list) \
	list_entry(ptr, struct rt_info, task_list[list])

#define list_add_before(head, add, list) \
	list_add_tail(&(add)->task_list[list], &(head)->task_list[list])

#define list_add_after(head, add, list) \
	list_add(&(add)->task_list[list], &(head)->task_list[list])

#define list_remove(task, list) \
	__list_del_entry(&(task)->task_list[list]);

#define list_remove_init(task, list) \
	list_del_init(&(task)->task_list[list]);

#define list_move_after(head, add, list) \
	list_move(&(add)->task_list[list], &(head)->task_list[list]);

/* compare two timespecs and return the smaller non-zero one
 * 0 is generally used to denote no value.
 *
 * For deadlines, checking the nsecs against 0 isn't needed, because absolute
 * deadlines will never have a 0 for the seconds unless both members are 0.
 */
static inline int compare_ts(struct timespec *t1, struct timespec *t2)
{
	return t1->tv_sec < t2->tv_sec || (t1->tv_sec == t2->tv_sec &&
			t1->tv_nsec < t2->tv_nsec);
}

#define lower_period(t1, t2) compare_ts(t1, t2)
#define earlier_deadline(t1, t2) compare_ts(t1, t2)

/* adds t1 and t2 and returns the results in t3 */
static inline void add_ts(struct timespec *t1, struct timespec *t2, struct timespec *t3)
{
	t3->tv_sec = t1->tv_sec + t2->tv_sec;
	t3->tv_nsec = t1->tv_nsec + t2->tv_nsec;
	if(t3->tv_nsec > BILLION) {
		t3->tv_sec++;
		t3->tv_nsec = t3->tv_nsec - BILLION;
	}
}

/* subtracts t2 from t1 and returns the results in t3 */
static inline void sub_ts(struct timespec *t1, struct timespec *t2, struct timespec *t3)
{
	if (t1->tv_sec < t2->tv_sec) {
		t3->tv_sec = t2->tv_sec - t1->tv_sec;
		t3->tv_nsec = t2->tv_nsec - t1->tv_nsec;
	} else {
		t3->tv_sec = t1->tv_sec - t2->tv_sec;
		t3->tv_nsec = t1->tv_nsec - t2->tv_nsec;
	}

	if (t3->tv_nsec < 0) {
		t3->tv_sec--;
		t3->tv_nsec = BILLION - t3->tv_nsec;
	}
}

/* sets both members of a timespec to 0 */
static inline void zero_ts(struct timespec *t1)
{
	t1->tv_sec = 0;
	t1->tv_nsec = 0;
}

static inline int is_zero_ts(struct timespec *t1)
{
	if ((t1->tv_sec == 0) && (t1->tv_nsec == 0))
		return 1;
	else
		return 0;
}

void quicksort(struct rt_info *head, int i, int key, int before);
int insert_on_list(struct rt_info *item, struct rt_info *list, int i, int key, int before);
void insert_on_local_queue(struct rt_info *item, struct list_head *list, int key);
void insert_on_global_queue(struct rt_info *item, struct list_head *list, int key);

/* Check a dependancy chain built on the fly for loops */
inline int check_dependancy_chain(struct rt_info *start, struct rt_info *next);

#define TASK_FLAG_MARKED 0x10
/* Check a prebuilt list and flag every task in a deadlock */
void mark_local_deadlocks(struct list_head *head);
void mark_global_deadlocks(struct list_head *head);

/* Return the owner of a resource */
struct rt_info* get_mutex_owner(const struct mutex_head *m);

/* Return the resource requested by a task */
inline struct mutex_head* get_requested_resource(const struct rt_info *task);

/* Return the owner of the requested resource for a task */
struct rt_info *get_requested_mutex_owner(const struct rt_info *task);

/* Convert between long and timespecs */
inline void long_to_timespec(unsigned long l, struct timespec *tspec);

/* Convert between long and timespecs */
inline unsigned long timespec_to_long(const struct timespec *ts);

void set_task_aborting(pid_t pid);
void clear_task_aborting(pid_t pid);

void abort_thread(struct rt_info *r);

/* Check if a task has failed. If so, it will be aborted.
 * Returns true if it is aborted or has previously been.
 */
inline int check_task_failure(struct rt_info *task, int flags);

/* Check if a task has been aborted */
static inline int check_task_aborted(struct rt_info *task)
{
	return task_check_flag(task, ABORTED);
}

/* Check if a task has been aborted previously and does not have an HUA handler */
static inline int check_task_abort_nohua(struct rt_info *task)
{
	return check_task_aborted(task) && (task->local_ivd == -1);
}

/*Calculate the inverse value density of a task
 *
 *	Parameters:
 *		rtinfo: the task to calculate for
 *		calc_dep: A int specifying whether or not to calculate
 * 			value densities and dependacies for the entire
 * 			dependancy tree
 * 		flags: the set of flags passed to the scheduler from the userspace
 *
 *	In general, tasks are given an inverse value density of their time
 * 	remaining in microseconds/thier utility. The following special cases
 *	exist.
 *
 *	Tasks who's ivd cannot be computed for some reason are given
 * 	LONG_MAX (2147483647 on 32-bit systems). This is equal to a utility
 * 	of 1 and a remaining time of ~36 minutes.
 *
 *	Tasks that have failed and don't have (or aren't using) an handler TUF
 * 	are designated with an IVD of -1. Tasks with an IVD of -1 will never
 *	have thier IVD or dependency chains calculated.
 */
long livd(struct rt_info *task, int calc_dep, int flags);

struct rt_info* get_pi_task(struct rt_info* best, struct list_head *head, int flags);

inline void initialize_lists(struct rt_info *task);
inline void initialize_dep(struct rt_info *task);

int list_is_feasible(struct rt_info *head, int i);
void copy_list(struct rt_info *head, int from, int to);
void trim_list(struct rt_info *head, int list, int items);
#endif

