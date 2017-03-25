/* kernel/chronos_global.c
 *
 * ChronOS Kernel Scheduler helper functions for global schedulers
 *
 * Author(s):
 *	- Piyush Garyali, piyushg@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/chronos_global.h>
#include <linux/module.h>

struct cpu_info chronos_cpu_state[NR_CPUS];

/* Return the least LVD from the deadlocked list, pivot is the one
 * which is dead-locked */
struct rt_info* find_least_local_pud_task(struct rt_info *head,
							struct rt_info *pivot)
{
	struct rt_info *cur = pivot;
	struct rt_info *leastvd = pivot;
	struct rt_info *next = NULL;

	if(cur)
		next = cur->graph.parent;

	while(next != cur) {
		if(leastvd->local_ivd > next->local_ivd)
			leastvd = next;
		next = next->graph.parent;
	}

	return leastvd;
}
EXPORT_SYMBOL(find_least_local_pud_task);



// FIXME:	There might be a condition in which the GVD creation might loop
//		forever, happens is there is a deadlock
/* Insert edge to P from depP. Also update the aggregate utility and
 *	aggregate left for the graph
 *
 * Return 1 if the edge already exists between depP and P, 0 otherwise
 */
int insert_link_in_graph(struct rt_info *to, struct rt_info *from)
{
	int ret = 0;
	struct timespec old_agg_left, older_agg_left;
	unsigned long 	old_agg_util = 0, older_agg_util = 0;
	struct rt_info *cur = NULL, *next = NULL;

	zero_ts(&old_agg_left);
	zero_ts(&older_agg_left);

	if(!to->graph.parent || to->graph.parent != from) {
		if(from->graph.neighbor_list == NULL) {
			from->graph.neighbor_list = to;
			to->graph.parent = from;
		} else {
			to->graph.next_neighbor = from->graph.neighbor_list;
			from->graph.neighbor_list = to;
			to->graph.parent = from;
		}

		from->graph.out_degree++;
		to->graph.in_degree++;

		cur = to;
		next = cur->graph.parent;

		while(next) {
			old_agg_left = next->graph.agg_left;
			old_agg_util = next->graph.agg_util;

			if(older_agg_util != 0  &&
						!is_zero_ts(&older_agg_left)) {
				sub_ts(&next->graph.agg_left, &older_agg_left,
							&next->graph.agg_left);
				next->graph.agg_util -= older_agg_util;
			}

			older_agg_left = old_agg_left;
			older_agg_util = old_agg_util;

			add_ts(&next->graph.agg_left, &cur->graph.agg_left,
							&next->graph.agg_left);
			next->graph.agg_util += cur->graph.agg_util;

			cur = next;
			next = next->graph.parent;
		}
	} else
		ret = 1;

	return ret;
}
EXPORT_SYMBOL(insert_link_in_graph);

/* Remove edged in DAG for p */
void remove_link_in_graph(struct rt_info *p)
{
	struct rt_info *parent = NULL, *next = NULL;
	int flag = 0;

	if(p) {
		parent = p->graph.parent;
		next = parent->graph.neighbor_list;

		while(next) {
			if(next == p) {
				if(!flag)
					parent->graph.neighbor_list = NULL;
				else
					parent->graph.next_neighbor = NULL;

				p->graph.parent->graph.out_degree--;
				p->graph.in_degree--;
				p->graph.parent = NULL;
				break;
			} else {
				flag = 1;
				parent = next;
				next = next->graph.next_neighbor;
			}
		}
	}
}
EXPORT_SYMBOL(remove_link_in_graph);

/* Return the processor with the least sum of execution costs */
int find_processor(int cpus)
{
	int least_cpu = 0, cpu;
	long least_exec = -1;

	for(cpu = 0; cpu < cpus; cpu++) {

		/* If any cpu has 0 least_exec, it has never been assigned before */
		if(least_exec == 0) {
			goto out;
		}

		/* Check if this is first time */
		if(least_exec == -1) {
			least_cpu = cpu;
			least_exec = chronos_cpu_state[cpu].exec_times;
		} else {
			if(least_exec > chronos_cpu_state[cpu].exec_times) {
				least_cpu = cpu;
				least_exec = chronos_cpu_state[cpu].exec_times;
			}
		}
	}

out:
	return least_cpu;
}
EXPORT_SYMBOL(find_processor);

/* This one is used by G-GUA. If the task is not feasible on one processor, try
 * another processor. Keep trying till all the processors have been used, then
 * return -1. Use mask to check for processors that have been used. Mask is set
 & by G-GUA */
int find_processor_ex(int *mask, int cpus)
{
	int least_cpu = -1, cpu;
	long least_exec = -1;

	for(cpu = 0; cpu < cpus; cpu++) {
		if(ISBITSET(mask, cpu))
			continue;
		else {
			/* If any cpu has 0 least_exec, it has never been assigned before */
			if(least_exec == 0)
				goto out;

			/* Check if this is first time */
			if(least_exec == -1) {
				least_cpu = cpu;
				least_exec = chronos_cpu_state[cpu].exec_times;
			} else {
				if(least_exec > chronos_cpu_state[cpu].exec_times) {
					least_cpu = cpu;
					least_exec = chronos_cpu_state[cpu].exec_times;
				}
			}
		}
	}
out:
	return least_cpu;
}
EXPORT_SYMBOL(find_processor_ex);

/* Get the cpu state object */
struct cpu_info* get_cpu_state(int cpu_id)
{
	return &chronos_cpu_state[cpu_id];
}
EXPORT_SYMBOL(get_cpu_state);

/* Initialize the cpu state for each scheduling event */
void initialize_cpu_state(void)
{
	int cpu;

	for(cpu = 0; cpu < NR_CPUS; cpu++) {
		chronos_cpu_state[cpu].exec_times = 0;
		chronos_cpu_state[cpu].head = NULL;
		chronos_cpu_state[cpu].tail = NULL;
		chronos_cpu_state[cpu].best_dead = NULL;
		chronos_cpu_state[cpu].best_ivd = NULL;
		chronos_cpu_state[cpu].last_ivd = NULL;
	}
}
EXPORT_SYMBOL(initialize_cpu_state);

/* Insert task into cpu list */
void insert_cpu_task(struct rt_info *p, int cpu)
{
	if(!chronos_cpu_state[cpu].head)
		chronos_cpu_state[cpu].head = p;
	else
		list_add_after(chronos_cpu_state[cpu].tail, p, LIST_CPUTSK);

	chronos_cpu_state[cpu].tail = p;
}
EXPORT_SYMBOL(insert_cpu_task);

void update_cpu_exec_times(int cpu, struct rt_info *p, bool status)
{
	if(status == true)
		chronos_cpu_state[cpu].exec_times += p->exec_time;
	else
		chronos_cpu_state[cpu].exec_times -= p->exec_time;
}
EXPORT_SYMBOL(update_cpu_exec_times);

/* Return the task with the least deadline in order to default to EDF-PIP */
struct rt_info* find_least_pip(struct rt_info *next, struct rt_info *task)
{
	struct rt_info *it = next;

	if(!next)
		return task;

	if(earlier_deadline(&it->deadline, &task->deadline))
		task = it;

	do {
		task = find_least_pip(it->graph.neighbor_list, task);
		it = it->graph.next_neighbor;
		if(it && earlier_deadline(&it->deadline, &task->deadline))
			task = it;
	} while(it);

	return task;
}

/*
 * This function takes the list head and does the following
 * - Create a precendence graph based on the task dependencies
 * - Check for deadlocks and resolve them by aborting tasks that are deadlocked
 * - Compute the global PUD for the zero indegree tasks
 * - Find the temp_deadline which is equal to the least deadline in graph (PIP)
 * - return the head of the zero indegree tasks to the scheduler
 */
struct rt_info* find_zero_indegree_tasks(struct list_head *head, int flags)
{
	struct rt_info *dephead, *tail, *entry, *next, *leastvd, *zihead = NULL, *least;

	list_for_each_entry(entry, head, task_list[GLOBAL_LIST]) {
		dephead = entry;
		tail = entry;
		next = get_requested_mutex_owner(entry);

		/* Insert edge in graph, if the edge already exists, we
		 * are done with this edge and its dependents, move to
		 * the next one */
		while(next && insert_link_in_graph(entry, next)) {
			if(is_present(dephead, next)) {
				leastvd = find_least_local_pud_task(dephead, next);
				abort_thread(leastvd);
				remove_link_in_graph(leastvd);
				break;
			}

			tail = insert_deplist(tail, next);
			entry = next;
		 	next = get_requested_mutex_owner(next);
		}
	}

	/* Find the zero indegree tasks */
	list_for_each_entry(entry, head, task_list[GLOBAL_LIST]) {
		if(entry->graph.in_degree == 0) {
			compute_global_pud(entry);

			if(!zihead)
				zihead = entry;
			else
				list_add_before(zihead, entry, LIST_ZINDEG);

			/* Find the temp_deadline for each ZI task which is equal
			 * to the least deadline in the graph for the ZI task, to
			 * default to EDF-PIP behavior */
			least = entry;
			least = find_least_pip(entry->graph.neighbor_list, least);
			entry->temp_deadline = least->deadline;
		}
	}

	return zihead;
}
EXPORT_SYMBOL(find_zero_indegree_tasks);

