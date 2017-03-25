/* include/linux/chronos_global.h
 *
 * Scheduler helper functions for global schedulers
 *
 * Author(s):
 *	- Piyush Garyali, piyushg@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#ifndef _CHRONOS_GLOBAL_H_
#define _CHRONOS_GLOBAL_H_

#include <linux/chronos_types.h>
#include <linux/chronos_util.h>

/* Re-definitions of scheduler-specific list offsets for common global schedulers */
#define LIST_ZINDEG  SCHED_LIST1	/* List of zero indegree tasks */
#define LIST_CPUTSK  SCHED_LIST2	/* List of tasks on same cpu, sorted deadline */
#define LIST_CPUIVD  SCHED_LIST3	/* List of tasks on same cpu, sorted ivd */
#define LIST_TDEAD   SCHED_LIST4	/* List of zero indegree tasks, sorted temp deadline */
#define LIST_CPUDEAD SCHED_LIST4	/* List used by G-GUA, best-dead during feasibility */

/* Used for the find_processor_ex mask */
#define ISBITSET(x,i) ((x[i>>3] & (1<<(i&7)))!=0)
#define SETBIT(x,i) x[i>>3]|=(1<<(i&7));
#define CLEARBIT(x,i) x[i>>3]&=(1<<(i&7))^0xFF;

struct cpu_info
{
	long exec_times;
	struct rt_info *head;
	struct rt_info *tail;
	struct rt_info *best_dead;
	struct rt_info *best_ivd;
	struct rt_info *last_ivd;
};

/* initialize the graph data structure for a task */
static inline void initialize_graph(struct rt_info *task)
{
	task->global_ivd = 0;
	task->graph.agg_left = task->left;
	task->graph.agg_util = task->max_util;
	task->graph.in_degree = 0;
	task->graph.out_degree = 0;
	task->graph.neighbor_list = NULL;
	task->graph.next_neighbor = NULL;
	task->graph.parent = NULL;
	task->graph.depchain = NULL;
}

/* Check if p is present in dep list starting at 'node' */
static inline int is_present(struct rt_info *head, struct rt_info *p)
{
	struct rt_info *it = head;

	while(it != NULL) {
		if (it == p)
			 return 1;
		it = it->graph.depchain;
	}

	return 0;
}

/* Adds the entry as a depchain for head.
 * Returns entry as the new tails, or NULL in case of error */
static inline struct rt_info* insert_deplist(struct rt_info *tail,
							struct rt_info *entry)
{
	if(tail) {
		tail->graph.depchain = entry;
		entry->graph.depchain = NULL;
	}

	return entry;
}

static inline void compute_global_pud(struct rt_info *p)
{
	long _left = timespec_to_long (&p->graph.agg_left);

	if(_left) {
		p->global_ivd = (signed long)(_left/(p->graph.agg_util));
		p->global_ivd = (p->global_ivd == 0) ? 1 : p->global_ivd;
	} else
		p->global_ivd = LONG_MAX;
}


/* CPU state related functions */
struct cpu_info* get_cpu_state(int cpu_id);
void initialize_cpu_state(void);

struct rt_info* find_least_local_pud_task(struct rt_info *head,
							struct rt_info *pivot);

struct rt_info* find_zero_indegree_tasks(struct list_head *head, int flags);

int insert_link_in_graph(struct rt_info *to, struct rt_info *from);
void remove_link_in_graph(struct rt_info *p);

void insert_cpu_task(struct rt_info *p, int cpu);
void update_cpu_exec_times(int cpu, struct rt_info *p, bool status);
int find_processor(int cpus);
int find_processor_ex(int *mask, int cpus);
struct rt_info* find_least_pip(struct rt_info *task, struct rt_info *least);

#endif

