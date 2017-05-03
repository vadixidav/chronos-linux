#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/chronos_util.h>
#include <linux/list.h>

struct rt_info* sched_rma_icpp(struct list_head *head, int flags)
{
	struct rt_info *best = local_task(head->next);

	if(flags & SCHED_FLAG_PI)
		best = get_pi_task(best, head, flags);

	return best;
}

struct rt_sched_local rma_icpp = {
	.base.name = "RMA-ICPP",
	.base.id = SCHED_RT_RMA_ICPP,
	.flags = 0,
	.schedule = sched_rma_icpp,
	.base.sort_key = SORT_KEY_PERIOD,
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
