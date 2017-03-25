/* kernel/chronos_sched.c
 *
 * Functions for the management of global lists, scheduler lists, architecture
 * functions, and mapping functions.
 *
 * Author(s):
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/chronos_sched.h>
#include <linux/chronos_types.h>
#include <linux/mcslock.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <asm/atomic.h>

/* List of all the real-time scheduling algorithms in the system */
LIST_HEAD(rt_sched_list);
DEFINE_RWLOCK(rt_sched_list_lock);
LIST_HEAD(global_domain_list);
DEFINE_RWLOCK(global_domain_list_lock);

/* The task_struct pointers the global scheduler fills */
DEFINE_PER_CPU(struct task_struct *, global_task);
/* The MCS lock node for the global scheduling lock */
DEFINE_PER_CPU(mcs_node_t, global_sched_lock_node);
/* The last queue state seen by this cpu */
DEFINE_PER_CPU(unsigned int, last_queue_event);

void chronos_init_cpu(int cpu)
{
	mcs_node_init(&per_cpu(global_sched_lock_node, cpu));
	per_cpu(global_task, cpu) = NULL;
	per_cpu(last_queue_event, cpu) = 0;
}

/* FIFO, just so that by default we don't muck with Linux */
struct rt_info * sched_fifo (struct list_head *head, int flags) {
	return local_task(head->next);
}

struct rt_sched_local fifo = {
	.base.name = "FIFO",
	.base.id = 0,
	.base.sort_key = SORT_KEY_NONE,
	.flags = 0,
	.schedule = sched_fifo
};

/* Should only be called with the task list lock locked */
inline int check_queue_stamp(struct global_sched_domain *g) {
	return g->queue_stamp == per_cpu(last_queue_event, raw_smp_processor_id());
}

/* Scheduler list management */
/* Actually add the scheduler. Add at different ends for readability */
void add_scheduler_nocheck(struct sched_base *scheduler, int is_global)
{
	write_lock(&rt_sched_list_lock);
	if(is_global)
		list_add_tail(&scheduler->list, &rt_sched_list);
	else
		list_add(&scheduler->list, &rt_sched_list);

	write_unlock(&rt_sched_list_lock);
}

static int add_rt_scheduler(struct sched_base *scheduler, int is_global)
{
	if(!list_empty(&scheduler->list))
		return -EEXIST;

	cpumask_clear(&scheduler->active_mask);
	add_scheduler_nocheck(scheduler, is_global);

	return 0;
}

int add_local_scheduler(struct rt_sched_local *scheduler)
{
	return add_rt_scheduler(&scheduler->base, 0);
}
EXPORT_SYMBOL(add_local_scheduler);

int add_global_scheduler(struct rt_sched_global *scheduler)
{
	return add_rt_scheduler(&scheduler->base, 1);
}
EXPORT_SYMBOL(add_global_scheduler);

static void remove_scheduler(struct sched_base *scheduler)
{
	/* Set scheduler to FIFO to prevent problems */
 	set_scheduler_mask(&fifo, NULL, &scheduler->active_mask, 0);

	write_lock(&rt_sched_list_lock);
	list_del_init(&scheduler->list);
	write_unlock(&rt_sched_list_lock);
}

void remove_local_scheduler(struct rt_sched_local *global) {
	remove_scheduler(&global->base);
}
EXPORT_SYMBOL(remove_local_scheduler);

void remove_global_scheduler(struct rt_sched_global *global) {
	remove_scheduler(&global->base);
}
EXPORT_SYMBOL(remove_global_scheduler);

static struct sched_base * get_scheduler(int scheduler)
{
	struct sched_base *it, *found = NULL;

	read_lock(&rt_sched_list_lock);
	list_for_each_entry(it, &rt_sched_list, list) {
		if(it->id == scheduler) {
			found = it;
			break;
		}
	}
	read_unlock(&rt_sched_list_lock);

	return found;
}

struct rt_sched_local * get_local_scheduler(int scheduler) {
	struct sched_base *base = get_scheduler(scheduler);
	if(base)
		return container_of(base, struct rt_sched_local, base);

	return NULL;
}

struct rt_sched_global * get_global_scheduler(int scheduler) {
	struct sched_base *base = get_scheduler(scheduler);
	if(base)
		return container_of(base, struct rt_sched_global, base);

	return NULL;
}

/* Global task list management
 * test_* add and remove functions can be called without knowing the validity of
 * the domain, normal add and remove functions require a valid domain
 */

/* Add and remove a task from the global scheduling queue */

inline void _add_task_global(struct rt_info *r, struct global_sched_domain *g)
{
	g->queue_stamp++;
	insert_on_global_queue(r, &g->global_task_list, g->scheduler->base.sort_key);
}

void add_task_global(struct rt_info *r, struct global_sched_domain *g)
{
	lock_global_task_list(g);
	_add_task_global(r, g);
	unlock_global_task_list(g);
}

void test_add_task_global(struct rt_info *r, struct global_sched_domain *g)
{
	if(g)
		add_task_global(r, g);
}

/* Check if a task needs to be inserted on the global list for a given domain */
void check_global_insert(struct task_struct *p, struct global_sched_domain *g)
{
	if(is_realtime(p) && task_check_flag(&p->rtinfo, INSERT_GLOBAL)) {
		test_add_task_global(&p->rtinfo, g);
		clear_global_insert(&p->rtinfo);
	}
}

void _remove_task_global(struct rt_info *r, struct global_sched_domain *g)
{
	g->queue_stamp++;
	list_del_init(&r->task_list[GLOBAL_LIST]);
	atomic_dec(&g->tasks);
}
EXPORT_SYMBOL(_remove_task_global);

void remove_task_global(struct rt_info *r, struct global_sched_domain *g)
{
	lock_global_task_list(g);
	_remove_task_global(r, g);
	unlock_global_task_list(g);
}

void test_remove_task_global(struct rt_info *r, struct global_sched_domain *g)
{
	if(g && in_global_list(r))
		remove_task_global(r, g);
}

/* A task is pullable iff its cpumask allows it to be on our cpu and it is not
 * current executing on another cpu. Note that rq->curr gets switched before
 * task->oncpu, so we check task->oncpu by calling task_is_current(). There can
 * be tasks which aren't rq->curr, but are still oncpu.
 */ 
int task_pullable(struct rt_info *r, int cpu)
{
#ifdef CONFIG_SMP
	struct task_struct *p = task_of_rtinfo(r);
	if(task_cpu(p) == cpu || (!p->on_cpu &&
	   cpumask_test_cpu(cpu, &p->cpus_allowed)))
		return 1;
	return 0;
#else
	return 1;
#endif
}
EXPORT_SYMBOL(task_pullable);

/* Create and initialize a global scheduling domain */
struct global_sched_domain *create_global_domain(struct rt_sched_global *g, int prio)
{
	struct global_sched_domain *domain;

	domain = kmalloc(sizeof(struct global_sched_domain), GFP_KERNEL);

	if(domain) {
		INIT_LIST_HEAD(&domain->global_task_list);
		INIT_LIST_HEAD(&domain->list);
		raw_spin_lock_init(&domain->global_task_list_lock);
		mcs_lock_init(&domain->global_sched_lock);
		atomic_set(&domain->tasks, 0);
		domain->scheduler = g;
		domain->prio = prio;
		domain->queue_stamp = 1;
	} else
		printk("Failed creating global scheduling domain with %s\n", g->base.name);


	return domain;
}

void add_global_domain(struct global_sched_domain *domain)
{
	write_lock(&global_domain_list_lock);
	list_add(&domain->list, &global_domain_list);
	write_unlock(&global_domain_list_lock);
}

void remove_global_domain(struct global_sched_domain *domain)
{
	write_lock(&global_domain_list_lock);
	list_del(&domain->list);
	write_unlock(&global_domain_list_lock);
}

void cpu_init_global_domain(int cpu) {
	per_cpu(last_queue_event, cpu) = 0;
	per_cpu(global_task, cpu) = NULL;
}

/* Global list building function
 *
 * This takes a list built on the rt_info->list pointers and sets it up to be
 * passed to a mapping function.
 */
void build_list(struct rt_info *head, int list, int cpus)
{
	if(!head)
		return;

	/* The final list is always built on SCHED_LIST1 */
	trim_list(head, list, cpus);

	if(list)
		copy_list(head, list, SCHED_LIST1);
}
EXPORT_SYMBOL(build_list);

/* Global list building function
 *
 * This takes a array of tasks and sets it up to be passed to a mapping function.
 */
void build_list_array(struct rt_info **head, int cpus)
{
	int i;
	struct rt_info *curr = NULL, *first = head[0];

	if(!head || !first)
		return;

	initialize_lists(first);

	for(i = 1; i < cpus; i++) {
		curr = head[i];
		if (curr)
			list_add(&curr->task_list[SCHED_LIST1], &first->task_list[SCHED_LIST1]);
		else
			break;
	}
}
EXPORT_SYMBOL(build_list_array);

/* Three different ways to IPI other cpus */
static void reschedule_count_global_cpus(struct global_sched_domain *g, int prio, int tasks)
{
	int count = tasks, cpu;
	cpumask_t mask;

	cpumask_copy(&mask, &g->global_sched_mask);
	cpumask_clear_cpu(raw_smp_processor_id(), &mask);

	for_each_cpu_mask(cpu, mask) {
		if(prio_resched_cpu(cpu, prio))
			count--;
		if(count == 0)
			return;
	}
}

static void reschedule_trycount_global_cpus(struct global_sched_domain *g, int prio, int tasks)
{
	int count = tasks, cpu;
	cpumask_t mask;

	cpumask_copy(&mask, &g->global_sched_mask);
	cpumask_clear_cpu(raw_smp_processor_id(), &mask);

	for_each_cpu_mask(cpu, mask) {
		count--;
		prio_resched_cpu(cpu, prio);
		if(count == 0)
			return;
	}
}

static void reschedule_all_global_cpus(struct global_sched_domain *g, int prio)
{
	int cpu;
	cpumask_t mask;

	cpumask_copy(&mask, &g->global_sched_mask);
	cpumask_clear_cpu(raw_smp_processor_id(), &mask);

	for_each_cpu_mask(cpu, mask) {
		prio_resched_cpu(cpu, prio);
	}
}

/*
 * Finds the best task to execute on a particular core and sets
 * that core's mask if a task was found.
 */
static struct task_struct * find_best_task(int cpu,
					   struct global_sched_domain *g,
					   cpumask_t *m, struct rt_info **head)
{
	struct task_struct *task, *best;
	struct rt_info *curr, *lhead = *head;

	if(!lhead)
		return NULL;

	curr = lhead;
	best = NULL;

	/* If any task out of the "best tasks" is already executing on this
	 * CPU, pick it. Otherwise, pick the last task in the list that is
	 * assigned to run on this CPU */
	do {
		task = task_of_rtinfo(curr);

		if(task_cpu(task) == cpu) {
			best = task;
			if(task_curr(task))
				goto out;
		}

		curr = task_list_entry(curr->task_list[SCHED_LIST1].next, SCHED_LIST1);
	} while (curr != lhead);

	if(!best)
		return NULL;

	curr = &(best->rtinfo);
out:
	cpumask_clear_cpu(cpu, m);
	*head = list_empty(&curr->task_list[SCHED_LIST1]) ? NULL : task_list_entry(curr->task_list[SCHED_LIST1].next, SCHED_LIST1);
	list_remove(curr, SCHED_LIST1);
	return best;
}

/*
 * Return the first task in the list, or NULL if the list is empty.
 */
static struct task_struct * find_any_task(struct global_sched_domain *g,
					  struct rt_info **head)
{
	struct rt_info *curr = *head;

	if(!curr)
		return NULL;
	
	*head = list_empty(&curr->task_list[SCHED_LIST1]) ? NULL : task_list_entry(curr->task_list[SCHED_LIST1].next, SCHED_LIST1);
	list_remove(curr, SCHED_LIST1);
	return task_of_rtinfo(curr);
}

/* The default mapping function
 *
 * The goal is to assign m tasks to m cpus with the least migration possible.
 * This is used for algorithms like GEDF that just select the m best tasks.
 */
void generic_map_all_tasks(struct rt_info *best, struct global_sched_domain *g)
{
	int cpu;
	cpumask_t mask;
	struct rt_info *head = best;

	cpumask_copy(&mask, &g->global_sched_mask);

	/* Try to pick the best task for each CPU */
	for_each_cpu_mask(cpu, g->global_sched_mask) {
		per_cpu(global_task, cpu) = find_best_task(cpu, g, &mask, &head);
	}

	/* For the CPUs for which a best task was not chosen,
	   pick any task */
	for_each_cpu_mask(cpu, mask) {
		per_cpu(global_task, cpu) = find_any_task(g, &head);
	}
}

/*
 * For concurrent scheduling -- if the best task is not NULL,
 * map it to the current CPU
 */
void map_to_me(struct rt_info *best, struct global_sched_domain *g)
{
	int cpu = raw_smp_processor_id();

	if(best)
		per_cpu(global_task, cpu) = task_of_rtinfo(best);
	else
		per_cpu(global_task, cpu) = NULL;
}

/* Block on the global scheduling lock */
static void block_generic(struct global_sched_domain *g)
{
	if(is_locked_global_sched_lock(g)) {
		lock_global_sched_lock(g);
		unlock_global_sched_lock(g);
	}
}

/* Architecture Init functions */
int init_concurrent(struct global_sched_domain *g, int block)
{
	lock_global_task_list(g);

	return 1;
}

/*
 * Stop-the-world scheduling init function
 */
int init_stw(struct global_sched_domain *g, int block)
{
	int prio, tasks;

	/* If another CPU has already scheduled globally, block on the
	   global scheduling lock and then return. */
	if(block == BLOCK_FLAG_MUST_BLOCK || !trylock_global_sched_lock(g)) {
		block_generic(g);
		return 0;
	}

	lock_global_task_list(g);

	/* If the current task has not yet been assigned a CPU (cpu is -1),
	 * or if the queue stamp from the global domain is not the same as this
	 * CPU's last_queue_event, then schedule globally and inform the other
	 * CPUs. */
	if(seg_just_started(&current->rtinfo) || !check_queue_stamp(g)) {
		unlock_global_task_list(g);

		tasks = atomic_read(&g->tasks);
		prio = get_global_chronos_sys_prio(g);

		if(tasks <= count_global_cpus(g))
			reschedule_trycount_global_cpus(g, prio + 1, tasks);
		else
			reschedule_all_global_cpus(g, prio);

		lock_global_task_list(g);
	}

	return 1;
}

/*
 * Job-dynamic stop-the-world scheduling init function
 */
int init_stw_jd(struct global_sched_domain *g, int block)
{
	int prio, tasks;

	/* If another CPU has already scheduled globally, block on the
	   global scheduling lock and then return. */
	if(block == BLOCK_FLAG_MUST_BLOCK || !trylock_global_sched_lock(g)) {
		block_generic(g);
		return 0;
	}

	/* Otherwise, this CPU needs to schedule globally no matter what,
	   since job priorities may have changed. */
	tasks = atomic_read(&g->tasks);
	prio = get_global_chronos_sys_prio(g);

	if(tasks <= count_global_cpus(g))
		reschedule_trycount_global_cpus(g, prio + 1, tasks);
	else
		reschedule_all_global_cpus(g, prio);

	lock_global_task_list(g);

	return 1;
}

/* Architecture release functions */
void release_concurrent(struct global_sched_domain *g)
{
	int prio, tasks;

	unlock_global_task_list(g);

	tasks = atomic_read(&g->tasks);
	prio = get_global_chronos_sys_prio(g) + 1;

	if(tasks >= count_global_cpus(g))
		reschedule_all_global_cpus(g, prio);
	else
		reschedule_count_global_cpus(g, prio, tasks);
}

void release_generic(struct global_sched_domain *g)
{
	unlock_global_sched_lock(g);
	unlock_global_task_list(g);
}

struct rt_info * presched_stw_generic(struct list_head *head)
{
	return NULL;
}
EXPORT_SYMBOL(presched_stw_generic);

struct rt_info * presched_abort_generic(struct list_head *head)
{
	struct rt_info *it;

	list_for_each_entry(it, head, task_list[LOCAL_LIST]) {
		if(check_task_abort_nohua(it))
			return it;
	}

	return NULL;
}
EXPORT_SYMBOL(presched_abort_generic);

struct rt_info * presched_concurrent_generic(struct list_head *head)
{
	int cpu = raw_smp_processor_id();
	struct rt_info *it;

	list_for_each_entry(it, head, task_list[LOCAL_LIST]) {
		if(it->cpu == cpu)
			return it;
	}

	return NULL;
}
EXPORT_SYMBOL(presched_concurrent_generic);

struct rt_sched_arch rt_sched_arch_concurrent = {
	.arch_init = init_concurrent,
	.arch_release = release_concurrent,
	.map_tasks = map_to_me
};
EXPORT_SYMBOL(rt_sched_arch_concurrent);

struct rt_sched_arch rt_sched_arch_stw = {
	.arch_init = init_stw,
	.arch_release = release_stw,
	.map_tasks = generic_map_all_tasks
};
EXPORT_SYMBOL(rt_sched_arch_stw);

struct rt_sched_arch rt_sched_arch_stw_jd = {
	.arch_init = init_stw_jd,
	.arch_release = release_stw,
	.map_tasks = generic_map_all_tasks
};
EXPORT_SYMBOL(rt_sched_arch_stw_jd);

