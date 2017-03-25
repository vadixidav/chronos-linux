/* chronos/chronos_proc.c
 *
 * Initialize the different ChronOS /proc/ interfaces, making sure they
 * get initialized in the proper order.
 *
 * Author(s)
 *	- Aaron Lindsay, aaron@aclindsay.com
 *
 * Copyright (C) 2012 Virginia Tech Real Time Systems Lab
 */

#include <linux/proc_fs.h>

int __init init_sched_chronos_procfs(struct proc_dir_entry *chronos_dir);
int __init init_mutex_procfs(struct proc_dir_entry *chronos_dir);

static struct proc_dir_entry *chronos_dir;

static int __init init_chronos_procfs(void)
{
	chronos_dir = proc_mkdir("chronos", NULL);

	if (!chronos_dir) {
		printk(KERN_ERR "Failed creating ChronOS procfs!\n");
		return -ENOMEM;
	}

	printk("Initializing ChronOS procfs...\n");

	init_sched_chronos_procfs(chronos_dir);
	init_mutex_procfs(chronos_dir);

	return 0;
}

__initcall(init_chronos_procfs);

