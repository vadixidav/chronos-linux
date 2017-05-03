#include <linux/module.h>
#include <linux/chronos_types.h>
#include <linux/chronos_sched.h>
#include <linux/chronos_util.h>
#include <linux/list.h>

struct rt_info* sched_rma_ocpp(struct list_head *head, int flags)
{
	struct rt_info *best = local_task(head->next);

	if(flags & SCHED_FLAG_PI)
		best = get_pi_task(best, head, flags);

	return best;
}

struct rt_sched_local rma_ocpp = {
	.base.name = "RMA-OCPP",
	.base.id = SCHED_RT_RMA_OCPP,
	.flags = 0,
	.schedule = sched_rma_ocpp,
	.base.sort_key = SORT_KEY_PERIOD,
	.base.list = LIST_HEAD_INIT(rma_ocpp.base.list)
};

static int __init rma_ocpp_init(void)
{
	return add_local_scheduler(&rma_ocpp);
}
module_init(rma_ocpp_init);

static void __exit rma_ocpp_exit(void)
{
	remove_local_scheduler(&rma_ocpp);
}
module_exit(rma_ocpp_exit);

MODULE_DESCRIPTION("RMA-OCPP Scheduling Module for ChronOS");
MODULE_AUTHOR("Geordon Worley <vadixidav@gmail.com>");
MODULE_LICENSE("GPL");
