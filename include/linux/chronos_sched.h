/* include/linux/chronos_sched.h
 *
 * Functions for the management of global lists, scheduler lists, architecture
 * functions, and mapping functions.
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 * Written by Matthew Dellinger
 */

#ifndef _CHRONOS_SCHED_H
#define _CHRONOS_SCHED_H

#ifdef CONFIG_CHRONOS

#include <linux/list.h>
#include <linux/chronos_util.h>
#include <linux/chronos_types.h>

#ifdef CONFIG_CHRONOS_SCHED_STATS
# define cschedstat_inc(rq, field)	do { (rq)->field++; } while (0)
# define cschedstat_add(rq, field, amt)	do { (rq)->field += (amt); } while (0)
# define cschedstat_set(var, val)	do { var = (val); } while (0)
#else /* !CONFIG_CHRONOS_SCHED_STATS */
# define cschedstat_inc(rq, field)	do { } while (0)
# define cschedstat_add(rq, field, amt)	do { } while (0)
# define cschedstat_set(var, val)	do { } while (0)
#endif

#define seg_just_started(r)	((r)->cpu == -1)
#define is_realtime(p) 		((p)->policy == SCHED_CHRONOS)
#define is_global(s)		((s)->id & SCHED_GLOBAL_MASK)

/* The task_struct pointers the global scheduler fills */
DECLARE_PER_CPU(struct task_struct *, global_task);
/* The MCS lock node for the global scheduling lock */
DECLARE_PER_CPU(mcs_node_t, global_sched_lock_node);
/* The last queue state seen by this cpu */
DECLARE_PER_CPU(unsigned int, last_queue_event);

extern struct rt_sched_local fifo;

/* Need these defined here for cschedstat related stuff */
extern struct list_head rt_sched_list;
extern rwlock_t rt_sched_list_lock;
extern struct list_head global_domain_list;
extern rwlock_t global_domain_list_lock;

void chronos_init_cpu(int cpu);

/* All the prio functions can be called without knowing if we have a valid domain
 * such as in sched_setscheduler. Hence we check g.
 */
static inline int get_global_chronos_prio(struct global_sched_domain *g)
{
	return g ? g->prio : 0;
}

static inline int get_global_chronos_sys_prio(struct global_sched_domain *g)
{
	return g ? MAX_RT_PRIO - g->prio - 1 : MAX_RT_PRIO;
}

/* Count how many cpus are in our current domain */
static inline int count_global_cpus(struct global_sched_domain *g)
{
	return cpumask_weight(&g->global_sched_mask);
}

static inline struct task_struct * task_of_rtinfo(const struct rt_info *r)
{
	return container_of(r, struct task_struct, rtinfo);
}

static inline struct rt_info *local_task(struct list_head *task)
{
	return list_entry(task, struct rt_info, task_list[LOCAL_LIST]);
}

static inline struct rt_info *get_global_task(struct list_head *task)
{
	return list_entry(task, struct rt_info, task_list[GLOBAL_LIST]);
}

/* Returns 1 if a task is in the global list */
static inline int in_global_list(struct rt_info *r)
{
	return (!list_empty(&r->task_list[GLOBAL_LIST]));
}

static inline void mark_for_global_insert(struct rt_info *r, struct global_sched_domain *g)
{
	if(g) {
		atomic_inc(&g->tasks);
		task_set_flag(r, INSERT_GLOBAL);
	}
}

static inline void clear_global_insert(struct rt_info *r)
{
	task_clear_flag(r, INSERT_GLOBAL);
}

/* Lock and unlock the task list */
static inline void lock_global_task_list(struct global_sched_domain *g)
{
	raw_spin_lock(&g->global_task_list_lock);
}

static inline void unlock_global_task_list(struct global_sched_domain *g)
{
	unsigned int stamp = g->queue_stamp;
	raw_spin_unlock(&g->global_task_list_lock);
	per_cpu(last_queue_event, raw_smp_processor_id()) = stamp;
}

/* Lock or unlock the global scheduling lock */
static inline int is_locked_global_sched_lock(struct global_sched_domain *g)
{
	return mcs_is_locked(&g->global_sched_lock);
}

static inline int trylock_global_sched_lock(struct global_sched_domain *g)
{
	return mcs_trylock(&g->global_sched_lock,
			   &per_cpu(global_sched_lock_node, raw_smp_processor_id()));
}

static inline void lock_global_sched_lock(struct global_sched_domain *g)
{
	mcs_lock(&g->global_sched_lock, &per_cpu(global_sched_lock_node, raw_smp_processor_id()));
}

static inline void unlock_global_sched_lock(struct global_sched_domain *g)
{
	mcs_unlock(&g->global_sched_lock, &per_cpu(global_sched_lock_node, raw_smp_processor_id()));
}

/* Add a local real-time scheduler */
int add_local_scheduler(struct rt_sched_local *scheduler);
void remove_local_scheduler(struct rt_sched_local *scheduler);

/* Add a global real-time scheduler */
int add_global_scheduler(struct rt_sched_global *scheduler);
void remove_global_scheduler(struct rt_sched_global *scheduler);

void add_scheduler_nocheck(struct sched_base *scheduler, int is_global);

/* Find a scheduler */
struct rt_sched_local * get_local_scheduler(int scheduler);
struct rt_sched_global * get_global_scheduler(int scheduler);


/* Two ways to check for global tasks
 *
 * has_global_tasks() returns whether the list is empty
 * global_tasks() returns the task count, which is more optimistic and includes
 *	tasks that may not yet have been added to the list. It can also be
 *	called on an invalid domain.
 */
static inline int has_global_tasks(struct global_sched_domain *g)
{
	return !list_empty(&g->global_task_list);
}

static inline int global_tasks(struct global_sched_domain *g)
{
	return g ? atomic_read(&g->tasks) : 0;
}

void test_add_task_global(struct rt_info *task, struct global_sched_domain *g);
void test_remove_task_global(struct rt_info *task, struct global_sched_domain *g);
void exit_chronos(struct task_struct *t);
void check_global_insert(struct task_struct *t, struct global_sched_domain *g);
void _remove_task_global(struct rt_info *r, struct global_sched_domain *g);
int task_pullable(struct rt_info *task, int cpu);

/* Add and remove a domain from the list */
struct global_sched_domain *create_global_domain(struct rt_sched_global *g, int prio);
void add_global_domain(struct global_sched_domain *domain);
void remove_global_domain(struct global_sched_domain *domain);
void print_global_domain(struct global_sched_domain *domain, struct seq_file *m);
void print_global_domains(struct seq_file *m);
void cpu_init_global_domain(int cpu);

void build_list(struct rt_info *head, int list, int cpus);
void build_list_array (struct rt_info **head, int cpus);

/* Mapping functions */
void generic_map_all_tasks(struct rt_info *best, struct global_sched_domain *g);
void map_to_me(struct rt_info *best, struct global_sched_domain *g);

/* Architecture init functions */
int init_concurrent(struct global_sched_domain *g, int block);
int init_stw(struct global_sched_domain *g, int block);

/* Architecture release functions */
void release_concurrent(struct global_sched_domain *g);
void release_generic(struct global_sched_domain *g);
#define release_stw release_generic

struct rt_info * presched_stw_generic(struct list_head *head);
struct rt_info * presched_concurrent_generic(struct list_head *head);
struct rt_info * presched_abort_generic(struct list_head *head);

extern struct rt_sched_arch rt_sched_arch_concurrent;
extern struct rt_sched_arch rt_sched_arch_stw;
extern struct rt_sched_arch rt_sched_arch_stw_jd;

#endif	/* CONFIG_CHRONOS */
#endif

