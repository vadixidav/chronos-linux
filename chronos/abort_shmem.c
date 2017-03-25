/* chronos/abort_shmem.c
 *
 * Shared memory device for communicating task abortions to userspace
 *
 * Author(s)
 *	- Aaron Lindsay, aaron.lindsay@vt.edu
 *
 * Copyright (C) 2009-2012 Virginia Tech Real Time Systems Lab
 */

#include <asm/io.h>
#include <linux/chronos_sched.h>
#include <linux/chronos_util.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/threads.h>

/* Current maximum number of PIDs (defined in kernel/pid.c) */
extern int pid_max;
extern int pid_max_min, pid_max_max;

#define MIN_ABORTABLE_PID 1
static int aborts_pid_max; /* pid_max as it was when this module was loaded */

static unsigned char *shmem; /* Pointer to shared memory buffer */
static int pageorder; /* We will allocate 2^pageorder pages of memory */

/*
 * Set the pid as 'aborted' in the shared memory buffer.
 * Returns 0 on success, -EINVAL if the pid is out of bounds
 */
int _set_task_aborting(pid_t pid){
	if(pid > aborts_pid_max || pid < MIN_ABORTABLE_PID)
		return -EINVAL;

	shmem[pid-MIN_ABORTABLE_PID] = 1;
	return 0;
}

/*
 * Set the pid as not being aborted in the shared memory buffer.
 * Returns 0 on success, -EINVAL if the pid is out of bounds
 */
int _clear_task_aborting(pid_t pid){
	if(pid > aborts_pid_max || pid < MIN_ABORTABLE_PID)
		return -EINVAL;

	shmem[pid-MIN_ABORTABLE_PID] = 0;
	return 0;
}

/*
 * Called by the kernel when a user uses the mmap() call to map /dev/aborts
 * into their memory map. Returns 0 on success, -EAGAIN on failure.
 */
static int shmem_mmap(struct file *fp, struct vm_area_struct *vma){
	if (vma->vm_end - vma->vm_start > aborts_pid_max)
		return -EINVAL;

	if(remap_pfn_range(vma,
		vma->vm_start,
		virt_to_phys((void*)((unsigned long)shmem)) >> PAGE_SHIFT,
		vma->vm_end-vma->vm_start,
		PAGE_SHARED))
		return -EAGAIN;

	return 0;
}

static struct file_operations fops = {
	.read = NULL,
	.write = NULL,
	.open = NULL,
	.release = NULL,
	.mmap = shmem_mmap,
};

/*
 * Initializes the device buffer. Returns 0 on success, -ENOMEM if memory
 * allocation failed, and another nonzero value if registering the character
 * device failed.
 */
static int shmem_init(void){
	int retval = 0;
	pageorder = 0;

	/* make our local copy of pid_max */
	aborts_pid_max = pid_max;

	/*Register the character device*/
	if((retval = register_chrdev(222,"aborts",&fops))) {
		printk("abort_shmem.o: Failed to register abort character device\n");
		goto chdev_out;
	}

	/*Figure out how many pages to allocate based on page size and the number of pids*/
	while ((1 << pageorder)*PAGE_SIZE < (aborts_pid_max - MIN_ABORTABLE_PID + 1)*sizeof(char))
		pageorder++;

	/*Attempt to allocate them*/
	if ((shmem = (unsigned char*)__get_free_pages(GFP_KERNEL | __GFP_ZERO, pageorder)) == NULL) {
		retval = -ENOMEM;
		printk("abort_shmem.o: Failed to allocate %d pages of memory.\n", 1 << pageorder);
		goto mem_out;
	}

	/*
	 * Set up pointers for the scheduler and chronos_util.c
	 * to use to get to the set/unset functions
	 */
	kernel_set_task_aborting = _set_task_aborting;
	kernel_clear_task_aborting = _clear_task_aborting;

	return retval;

mem_out:
	unregister_chrdev(222,"aborts");
chdev_out:
	return retval;
}
module_init(shmem_init);

/*
 * Tear-down function for module. Unsets the function pointers, frees memory,
 * and unregisters the character device.
 */
static void shmem_exit(void){
	/* Free the pages we allocated in shmem_init */
	free_pages((unsigned long)shmem, pageorder);

	/* Unregister the character device */
	unregister_chrdev(222,"aborts");

	/* Unset the function pointers */
	kernel_set_task_aborting = NULL;
	kernel_clear_task_aborting = NULL;
}
module_exit(shmem_exit);

MODULE_DESCRIPTION("Abort Notification Shared Memory Module");
MODULE_AUTHOR("Aaron Lindsay <aaron@aclindsay.com>");
MODULE_LICENSE("GPL");
