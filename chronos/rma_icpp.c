#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/chronos_util.h>
#include <linux/list.h>

struct list_head * get_current_task_mutex_list(pid_t tgid);

struct rt_info* sched_rma_icpp(struct list_head *head, int flags)
{
	struct rt_info *best_task = local_task(head->next), *curr_task;
	struct mutex_head *curr_mutex;
	struct list_head *mutex_header_list = get_current_task_mutex_list(task_of_rtinfo(best_task));

	// Iterate through every task in the local list.
	list_for_each_entry(curr_task, head, task_list[LOCAL_LIST]) {
		curr_task->period_floor = curr_task->period;
	}

	// Check if there were any mutexes.
	if (mutex_header_list) {
		// Iterate through every mutex of the process.
		list_for_each_entry(curr_mutex, mutex_header_list, list) {
			// If the mutex has an owner.
			if (curr_mutex->owner_t) {
				// If the mutex's period_floor is lower than the period floor of the process.
				if (compare_ts(&curr_mutex->period_floor, &curr_mutex->owner_t->period_floor)) {
					// Lower the process' period floor to this new minimum period.
					curr_mutex->owner_t->period_floor = curr_mutex->period_floor;
				}
			}
		}
	}

	// Iterate through every task in the local list.
	list_for_each_entry(curr_task, head, task_list[LOCAL_LIST]) {
		// If any task is better than the best task, make it the best task.
		if (compare_ts(&curr_task->period_floor, &best_task->period_floor)) {
			best_task = curr_task;
		}
	}

	return get_pi_task(best_task, head, flags);
}

struct rt_sched_local rma_icpp = {
	.base.name = "RMA-ICPP",
	.base.id = SCHED_RT_RMA_ICPP,
	.flags = 0,
	.schedule = sched_rma_icpp,
	.base.sort_key = SORT_KEY_NONE,
	.base.list = LIST_HEAD_INIT(rma_icpp.base.list)
};

static int __init rma_icpp_init(void)
{
	return add_local_scheduler(&rma_icpp);
}
module_init(rma_icpp_init);

static void __exit rma_icpp_exit(void)
{
	remove_local_scheduler(&rma_icpp);
}
module_exit(rma_icpp_exit);

MODULE_DESCRIPTION("RMA-ICPP Scheduling Module for ChronOS");
MODULE_AUTHOR("Geordon Worley <vadixidav@gmail.com>");
MODULE_LICENSE("GPL");
