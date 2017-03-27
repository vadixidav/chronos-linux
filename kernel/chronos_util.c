/* kernel/chronos_util.c
 *
 * Fucntions for list[] management, time calculations, and other real-time
 * specific calls.
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/chronos_util.h>
#include <linux/chronos_types.h>
#include <linux/module.h>

int (*kernel_set_task_aborting) (pid_t pid);
EXPORT_SYMBOL(kernel_set_task_aborting);
int (*kernel_clear_task_aborting) (pid_t pid);
EXPORT_SYMBOL(kernel_clear_task_aborting);

/* Simple function to set a task's abort shared memory */
inline void set_task_aborting(pid_t pid)
{
	if(kernel_set_task_aborting)
		kernel_set_task_aborting(pid);
}

/* Simple function to clear a task's abort shared memory */
inline void clear_task_aborting(pid_t pid)
{
	if(kernel_clear_task_aborting)
		kernel_clear_task_aborting(pid);
}

/* New sorting headers for pending sort rewrite */

/* Compare two tasks. Returns 1 if t1 is before t2, according to the sorting
 * method. If the two are equal, we return 0 */
inline int compare_after(struct rt_info *t1, struct rt_info *t2, int key)
{
	switch(key) {
		case SORT_KEY_DEADLINE:
			return (earlier_deadline(&t1->deadline, &t2->deadline));
		case SORT_KEY_PERIOD:
			return (lower_period(&t1->period, &t2->period));
		case SORT_KEY_LVD:
			return (t1->local_ivd < t2->local_ivd);
		case SORT_KEY_GVD:
			return (t1->global_ivd < t2->global_ivd);
		case SORT_KEY_TDEADLINE:
			return (earlier_deadline(&t1->temp_deadline, &t2->temp_deadline));
		case SORT_KEY_NONE:
			return 1;
		default :
			return 0;
	}
}

/* Compare two tasks. Returns 1 if t1 is before t2, according to the sorting
 * method. If the two are equal, we return 1 */
inline int compare_before(struct rt_info *t1, struct rt_info *t2, int key)
{
	switch(key) {
		case SORT_KEY_DEADLINE:
			return (earlier_deadline(&t1->deadline, &t2->deadline));
		case SORT_KEY_PERIOD:
			return (lower_period(&t1->period, &t2->period));
		case SORT_KEY_LVD:
			return (t1->local_ivd <= t2->local_ivd);
		case SORT_KEY_GVD:
			return (t1->global_ivd <= t2->global_ivd);
		case SORT_KEY_TDEADLINE:
			return (earlier_deadline(&t1->temp_deadline, &t2->temp_deadline));
		default :
			return 1;
	}
}

static int compare(struct rt_info *t1, struct rt_info *t2, int key, int before)
{
	if(before)
		return compare_before(t1, t2, key);
	else
		return compare_after(t1, t2, key);
}

/* Quicksort a doubly linked list */
static void _quicksort(struct rt_info *start, struct rt_info *end, int i, int key, int before)
{
	struct rt_info *pivot = task_list_entry(start->task_list[i].next, i);
	struct rt_info *it = task_list_entry(pivot->task_list[i].next, i);
	struct rt_info *next;
	int low = 0, high = 0;

	while(it != end) {
		next = task_list_entry(it->task_list[i].next, i);
		if(compare(it, pivot, key, before)) {
			list_move_after(start, it, i);
			low++;
		} else
			high++;
		it = next;
	}

	if(high > 1)
		_quicksort(pivot, end, i, key, before);
	if(low > 1)
		_quicksort(start, pivot, i, key, before);

}

void quicksort(struct rt_info *head, int i, int key, int before)
{
	if(!list_empty(&head->task_list[i]) && head->task_list[i].next != head->task_list[i].prev)
		_quicksort(head, head, i, key, before);
}
EXPORT_SYMBOL(quicksort);

/* Check a dependancy chain built on the fly for loops */
inline int check_dependancy_chain(struct rt_info *start, struct rt_info *next)
{
	struct rt_info *n;

	for(n = start; n != NULL; n = n->dep) {
		if(next == n)
			return 1;
	}

	return 0;
}

/* Check a prebuilt list and flag every task in a deadlock */
static void mark_deadlocks(struct list_head *head, int i)
{
	struct rt_info *it, *next;

	list_for_each_entry(it, head, task_list[i]) {
		next = it;

		while (next->dep != NULL && !task_check_flag(next, DEADLOCKED)) {
			if(task_check_flag(next, MARKED))
				task_set_flag(next, DEADLOCKED);
			task_set_flag(next, MARKED);
			next = next->dep;
		}

		next = it;
		while(next->dep != NULL && task_check_flag(next, MARKED)) {
			task_clear_flag(next, MARKED);
			next = next->dep;
		}
	}
}

void mark_local_deadlocks(struct list_head *head)
{
	mark_deadlocks(head, 0);
}

void mark_global_deadlocks(struct list_head *head)
{
	mark_deadlocks(head, 1);
}

/* Return the owner of a resource */
struct rt_info* get_mutex_owner(const struct mutex_head *m)
{
	return m->owner_t;
}

/* Return the resource requested by a task */
inline struct mutex_head* get_requested_resource(const struct rt_info *task)
{
	return task->requested_resource;
}

/* Return the owner of a tasks requested resource */
struct rt_info* get_requested_mutex_owner(const struct rt_info *task)
{
	if(!task || !task->requested_resource)
		return NULL;

	return get_mutex_owner(task->requested_resource);
}

/* Convert between long and timespecs */
inline void long_to_timespec(unsigned long l, struct timespec *tspec)
{
	tspec->tv_sec = l/MILLION;
	tspec->tv_nsec = (l%MILLION)*THOUSAND;
}

static unsigned int task_time(struct rt_info *task)
{
	struct task_struct *ts = container_of(task, struct task_struct, rtinfo);

	return jiffies_to_usecs(ts->utime + ts->stime) - task->seg_start_us;
}

/* Convert between long and timespecs */
inline unsigned long timespec_to_long(const struct timespec *ts)
{
	return ts->tv_sec*MILLION + ts->tv_nsec/THOUSAND;
}

long calc_left(struct rt_info *task)
{
	long left = 0;

	left = task->exec_time - task_time(task);
	return (left <= 0) ? 1 : left;
}

long update_left(struct rt_info *task)
{
	long left = 0;

	left = calc_left(task);
	long_to_timespec(left, &(task->left));
	return left;
}

static void abort_deadlock(struct rt_info *task)
{
	int worst_ivd = calc_left(task)/task->max_util;
	int curr_ivd = 0;
	struct rt_info *worst = task;
	struct rt_info *curr = task;

	do {
		curr_ivd = calc_left(task)/task->max_util;
		if(curr_ivd > worst_ivd) {
			worst = curr;
			worst_ivd = curr_ivd;
		}
		task_clear_flag(curr, DEADLOCKED);
	} while(task_check_flag(task, DEADLOCKED));

	abort_thread(worst);
}

/* Task abortion functions */

/* Handle a task failure
 * four cases - we are or aren't using abort handlers, the task does or
 * does not have a handler
 */
static void handle_task_failure(struct rt_info *task, int flags)
{
		if ((flags & SCHED_FLAG_HUA) && task_check_flag(task, HUA)) {
			task->deadline.tv_sec = task->abortinfo.deadline.tv_sec;
			task->deadline.tv_nsec = task->abortinfo.deadline.tv_nsec;
			task->exec_time = task->abortinfo.exec_time + task_time(task);
			task->max_util = task->abortinfo.max_util;
		} else
			task->local_ivd = -1;

		abort_thread(task);
}

/* Check if a task has failed
 * All failure conditions should be added here
 * Eventually add other possible conditions here
 */
static inline void check_failure_conditions(struct rt_info *task, int flags)
{
	struct timespec t1;

	t1 = current_kernel_time();

	if(earlier_deadline(&(task->deadline), &t1))
		handle_task_failure(task, flags);
}

/* Returns true if the task is or has been aborted, and doesn't have a handler */
inline int check_task_failure(struct rt_info *task, int flags)
{
	if(!check_task_aborted(task))
		check_failure_conditions(task, flags);

	return task->local_ivd == -1;
}
EXPORT_SYMBOL(check_task_failure);

/* Signal a thread that you want it to abort via shared memory */
void abort_thread(struct rt_info *r)
{
	struct task_struct *p = task_of_rtinfo(r);

	/* set the byte in the shared memory to abort the task */
	set_task_aborting(p->pid);

	/* Set the flag so we know this has been marked for abortion */
	task_set_flag(r, ABORTED);
	r->requested_resource = NULL;

	/* Increment the count of segments aborted */
	inc_abort_count(p);
}

long livd(struct rt_info *task, int calc_dep, int flags)
{
	struct rt_info *next, *curr;
	long max_util, left;
	unsigned long exec_time;

	if(task->local_ivd == -1)
		return -1;

	left = update_left(task);
	max_util = task->max_util;
	exec_time = task->exec_time;

	if(calc_dep && task->dep != NULL)
	{
		if(task_check_flag(task, DEADLOCKED))
			abort_deadlock(task);

		curr = task;
		next = curr->dep;

		while (next != NULL) {
			max_util = max_util + next->max_util;
			left += calc_left(next);
			curr = next;
			next = curr->dep;
		}
	}

	if(max_util == 0 || left == 0 )
		task->local_ivd = LONG_MAX;
	else {
		task->local_ivd = (signed long)(left/max_util);
		task->local_ivd = task->local_ivd == 0 ? 1 : task->local_ivd;
	}

	return task->local_ivd;
}
EXPORT_SYMBOL(livd);

/* PI only makes sense on a local queue, so hardcode that */
struct rt_info* get_pi_task(struct rt_info* best, struct list_head *head, int flags)
{
	struct rt_info *best_pi = best, *next = NULL, *curr;

	if(flags & SCHED_FLAG_NO_DEADLOCKS) {
		list_for_each_entry(curr, head, task_list[LOCAL_LIST]) {
			curr->dep = NULL;
		}
	}

	while(best_pi->requested_resource != NULL) {
		next = get_requested_mutex_owner(best_pi);
		if(next == NULL)
			return best_pi;

		best_pi->dep = NULL;

		if((flags & SCHED_FLAG_NO_DEADLOCKS) && next->dep) {
			abort_thread(next);
			return next;
		}

		best_pi->dep = next;
		best_pi = best_pi->dep;
	}

	return best_pi;
}
EXPORT_SYMBOL(get_pi_task);

inline void initialize_lists(struct rt_info *task)
{
	int i;

	for(i = 2; i < SCHED_LISTS+2; i++)
		INIT_LIST_HEAD(&task->task_list[i]);

	task->flags &= (TASK_FLAG_ABORTED | TASK_FLAG_HUA);
}
EXPORT_SYMBOL(initialize_lists);

inline void initialize_dep(struct rt_info *task)
{
	task->dep = get_requested_mutex_owner(task);
	task_clear_flag(task, DEADLOCKED);
}
EXPORT_SYMBOL(initialize_dep);

/* Insert an item on a list
 * Returns 1 if the task was inserted at the head, otherwise returns 0
 */
int insert_on_list(struct rt_info *item, struct rt_info *list, int i, int key, int before)
{
	struct rt_info *it = list;

	do {
		if(compare(item, it, key, before)) {
			list_add_before(it, item, i);
			break;
		}
		it = task_list_entry(it->task_list[i].next, i);
	} while (it != list);

	if(task_list_entry(item->task_list[i].next, i) == list)
		return 1;
	else if(task_list_entry(item->task_list[i].next, i) == item)
		list_add_before(list, item, i);

	return 0;
}
EXPORT_SYMBOL(insert_on_list);

/* Insert on a queue. Break all ties with FIFO. */
static void insert_on_queue(struct rt_info *item, struct list_head *list, int key, int i)
{
	struct rt_info *it;

	if(key == SORT_KEY_NONE)
		goto tail;

	list_for_each_entry(it, list, task_list[i]) {
		if(compare(item, it, key, 0)) {
			list_add_tail(&item->task_list[i], &it->task_list[i]);
			return;
		}
	}

tail:
	list_add_tail(&item->task_list[i], list);
}

void insert_on_local_queue(struct rt_info *item, struct list_head *list, int key)
{
	insert_on_queue(item, list, key, LOCAL_LIST);
}
EXPORT_SYMBOL(insert_on_local_queue);

void insert_on_global_queue(struct rt_info *item, struct list_head *list, int key)
{
	insert_on_queue(item, list, key, GLOBAL_LIST);
}
EXPORT_SYMBOL(insert_on_global_queue);

int list_is_feasible(struct rt_info *head, int i)
{
	struct rt_info *it = head;
	struct timespec exec_ts = CURRENT_TIME;
	do {
		add_ts(&exec_ts, &(it->left), &exec_ts);
		if(earlier_deadline(&(it->deadline), &exec_ts))
			return 0;
		else
			it = task_list_entry(it->task_list[i].next, i);
	} while (it != head);

	return 1;
}
EXPORT_SYMBOL(list_is_feasible);

void copy_list(struct rt_info *head, int from, int to)
{
	struct rt_info *curr = head, *next, *prev;
	do {
		next = task_list_entry(curr->task_list[from].next, from);
		prev = task_list_entry(curr->task_list[from].prev, from);
		curr->task_list[to].next = &next->task_list[to];
		curr->task_list[to].prev = &prev->task_list[to];
		curr = next;
	} while (curr != head);
}
EXPORT_SYMBOL(copy_list);

void trim_list(struct rt_info *head, int list, int items)
{
	int i;
	struct rt_info *curr = head;
	for(i = 0; i < items; i++) {
		curr = task_list_entry(curr->task_list[list].next, list);
		if(curr == head)
			return;
	}
	curr->task_list[list].next = &head->task_list[list];
	head->task_list[list].prev = &curr->task_list[list];
}

