/* chronos/chronos_mutex_stats.c
 *
 * Print statistics about ChronOS real-time scheduling
 *
 * Author(s)
 *	- Matthew Dellinger, mdelling@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/chronos_version.h>

#ifdef CONFIG_CHRONOS_MUTEX_STATS
# define cmutexstat_inc(field)	do { atomic_inc(&field); } while (0)
# define cmutexstat_dec(field)	do { atomic_dec(&field); } while (0)
#else /* !CONFIG_CHRONOS_MUTEX_STATS */
# define cmutexstat_inc(field)	do { } while (0)
# define cmutexstat_dec(field)	do { } while (0)
#endif

#define SEQ_printf(m, x...)			\
 do {						\
	if (m)					\
		seq_printf(m, x);		\
	else					\
		printk(x);			\
 } while (0)

#ifdef CONFIG_CHRONOS_MUTEX_STATS
atomic_t processes, locks, locking_success, locking_failure;

static void print_mutex(struct seq_file *m)
{
	SEQ_printf(m, "\nReal-Time Locking Stats\n");

#define P(x) \
	SEQ_printf(m, "  .%-30s: %d\n", #x, atomic_read(&x))

	P(processes);
	P(locks);
	P(locking_success);
	P(locking_failure);

#undef P
}

static int mutex_stats_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "ChronOS Version: %s, %s %.*s\n",
		CHRONOS_VERSION_STRING,
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

	print_mutex(m);

	SEQ_printf(m, "\n");

	return 0;
}

static int mutex_stats_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, mutex_stats_show, NULL);
}

static const struct file_operations mutex_stats_fops = {
	.open		= mutex_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

int __init init_mutex_procfs(struct proc_dir_entry *chronos_dir)
{
#ifdef CONFIG_CHRONOS_MUTEX_STATS
	struct proc_dir_entry *pe_chronos_stats;

	pe_chronos_stats = proc_create("mutex", 0444, chronos_dir, &mutex_stats_fops);

	if(!pe_chronos_stats) {
		printk(KERN_ERR "Failed making ChronOS mutex procfs!\n");
		return -ENOMEM;
	}

	atomic_set(&processes, 0);
	atomic_set(&locks, 0);
	atomic_set(&locking_success, 0);
	atomic_set(&locking_failure, 0);
	printk("Initializing ChronOS mutex procfs\n");
#endif
	return 0;
}
