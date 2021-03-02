#include "sched.h"

/*
 * All the scheduling class methods:
 */
const struct sched_class sched_wrr_class = {

	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
        .dequeue_task           = dequeue_task_wrr,

        .check_preempt_curr     = check_preempt_curr_wrr,

        .pick_next_task         = pick_next_task_wrr,
        .put_prev_task          = put_prev_task_wrr,
        .set_next_task          = set_next_task_wrr,

#ifdef CONFIG_SMP
        .balance                = balance_wrr,
        .select_task_rq         = select_task_rq_wrr,
        .set_cpus_allowed       = set_cpus_allowed_common,
#endif

        .task_tick              = task_tick_wrr,

        .get_rr_interval        = get_rr_interval_wrr,

        .prio_changed           = prio_changed_wrr,
        .switched_to            = switched_to_wrr,
        .update_curr            = update_curr_wrr,
};

