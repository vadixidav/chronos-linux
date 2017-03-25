/* kernel/chronos_sched_stats.c
 *
 * Print statistics about ChronOS real-time scheduling
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/chronos_version.h>

/*
 * This allows printing both to /proc/sched_debug and
 * to the console
 */
#define SEQ_printf(m, x...)			\
 do {						\
	if (m)					\
		seq_printf(m, x);		\
	else					\
		printk(x);			\
 } while (0)

static void print_rt_sched_list(struct seq_file *m)
{
	int cpu;
	struct sched_base *it;

	read_lock(&rt_sched_list_lock);
	list_for_each_entry(it, &rt_sched_list, list) {
		SEQ_printf(m, "%s     \t%u\t [ ", it->name, it->id);
		for_each_cpu_mask(cpu, it->active_mask) {
			SEQ_printf(m, "%i ", cpu);
		}
		SEQ_printf(m, "]\n");
	}
	read_unlock(&rt_sched_list_lock);
}

static int chronos_sched_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "ChronOS Version: %s, %s %.*s\n",
		CHRONOS_VERSION_STRING,
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

	print_rt_sched_list(m);

	SEQ_printf(m, "\n");

	return 0;
}

#ifdef CONFIG_CHRONOS_SCHED_STATS
/* global flag of whether to clear per-cpu sched stats on resetting the scheduler
 * for that cpu
 */
int should_clear_chronos_stats = 0;

void clear_chronos_stats(struct rq *rq) {
	schedstat_set(rq->sched_count_global, 0);
	schedstat_set(rq->sched_count_local, 0);
	schedstat_set(rq->sched_count_block, 0);
	schedstat_set(rq->sched_count_presched, 0);
	schedstat_set(rq->sched_ipi_sent, 0);
	schedstat_set(rq->sched_ipi_received, 0);
	schedstat_set(rq->sched_ipi_missed, 0);
	schedstat_set(rq->task_pulled_from, 0);
	schedstat_set(rq->task_pulled_to, 0);
	schedstat_set(rq->task_pull_failed, 0);
	schedstat_set(rq->seg_begin_count, 0);
	schedstat_set(rq->seg_end_count, 0);
	schedstat_set(rq->seg_abort_count, 0);
}

#ifdef CONFIG_SYSCTL
static __initdata struct ctl_path sched_chronos_path[] = {
	{ .procname = "chronos", },
	{ }
};

static struct ctl_table sched_chronos_clear_table[] = {
	{
		.procname       = "clear_on_sched_set",
		.data           = &should_clear_chronos_stats,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec
	},
	{ }
};

static struct ctl_table_header *sched_chronos_clear_header;
#endif

static void print_cpu_chronos(struct seq_file *m, int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	SEQ_printf(m, "\nReal-Time Stats for CPU[%d]\n", cpu);

#define P(x) \
	SEQ_printf(m, "  .%-30s: %Ld\n", #x, (long long)(rq->x))
#define PS(x) \
	if(rq->rt.x) SEQ_printf(m, "  .%-30s: %s\n", #x, rq->rt.x->base.name); \
	else SEQ_printf(m, "  .%-30s: NONE\n", #x)
#define PN(x, y) \
	if(rq->rt.x) SEQ_printf(m, "  .%s/%-16s: %d\n", #x, #y, rq->rt.x->y); \
	else SEQ_printf(m, "  .%-30s: NONE\n", #x)

	PS(chronos_local);
	PN(chronos_local, base.id);
	PN(chronos_local, flags);
	P(sched_count_global);
	P(sched_count_block);
	P(sched_count_presched);
	P(sched_count_local);
	P(sched_ipi_sent);
	P(sched_ipi_received);
	P(sched_ipi_missed);
	P(task_pulled_from);
	P(task_pulled_to);
	P(task_pull_failed);
	P(seg_begin_count);
	P(seg_end_count);
	P(seg_abort_count);

#undef P
#undef PS
#undef PN
}

void print_global_domain(struct global_sched_domain *domain, struct seq_file *m)
{
	int cpu;
	struct sched_base *sched = &domain->scheduler->base;
	SEQ_printf(m, "Global domain on CPUs [ ");

 	for_each_cpu_mask(cpu, domain->global_sched_mask)
		SEQ_printf(m, "%d ", cpu);

	SEQ_printf(m, "]\n  Scheduler:\t%s\n  Number:\t%d\n",
		sched->name, sched->id);
	SEQ_printf(m, "  Priority:\t%d\n  Tasks:\t%d\n",
		domain->prio, atomic_read(&domain->tasks));
}

void print_global_domains(struct seq_file *m)
{
	struct global_sched_domain *it;

	read_lock(&global_domain_list_lock);
	list_for_each_entry(it, &global_domain_list, list) {
		print_global_domain(it, m);
	}
	read_unlock(&global_domain_list_lock);
}

static int chronos_stats_show(struct seq_file *m, void *v)
{
	int cpu;

	SEQ_printf(m, "ChronOS Version: %s, %s %.*s\n",
		CHRONOS_VERSION_STRING,
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

	print_global_domains(m);

	for_each_online_cpu(cpu)
		print_cpu_chronos(m, cpu);

	SEQ_printf(m, "\n");

	return 0;
}

static int sched_chronos_stats_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, chronos_stats_show, NULL);
}

static const struct file_operations sched_chronos_stats_fops = {
	.open		= sched_chronos_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static int sched_chronos_sched_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, chronos_sched_show, NULL);
}

static const struct file_operations sched_chronos_sched_fops = {
	.open		= sched_chronos_sched_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init init_sched_chronos_procfs(struct proc_dir_entry *chronos_dir)
{
	struct proc_dir_entry *pe_chronos_sched;
#ifdef CONFIG_CHRONOS_SCHED_STATS
	struct proc_dir_entry *pe_chronos_stats;
	pe_chronos_stats = proc_create("stats", 0444, chronos_dir, &sched_chronos_stats_fops);
	if(!pe_chronos_stats) {
		printk(KERN_ERR "Failed making ChronOS scheduling stats procfs!\n");
		return -ENOMEM;
	}

#ifdef CONFIG_SYSCTL
	sched_chronos_clear_header = register_sysctl_paths(sched_chronos_path, sched_chronos_clear_table);

	if(!sched_chronos_clear_header)
		return -ENOMEM;
#endif
#endif

	pe_chronos_sched = proc_create("schedulers", 0444, chronos_dir, &sched_chronos_sched_fops);

	if(!pe_chronos_sched) {
		printk(KERN_ERR "Failed making ChronOS scheduling procfs!\n");
		return -ENOMEM;
	}

	printk("Initializing ChronOS scheduling procfs\n");
	return 0;
}
