#include "sched.h"

/*
 * All the scheduling class methods:
 */
const struct sched_class sched_wrr_class = {
        /* .next is NULL */
        /* no enqueue/yield_task for idle tasks */

        /* dequeue is not valid, we print a debug message there: */
        .dequeue_task           = dequeue_task_idle,

        .check_preempt_curr     = check_preempt_curr_idle,

        .pick_next_task         = pick_next_task_idle,
        .put_prev_task          = put_prev_task_idle,
        .set_next_task          = set_next_task_idle,

#ifdef CONFIG_SMP
        .balance                = balance_idle,
        .select_task_rq         = select_task_rq_idle,
        .set_cpus_allowed       = set_cpus_allowed_common,
#endif

        .task_tick              = task_tick_idle,

        .get_rr_interval        = get_rr_interval_idle,

        .prio_changed           = prio_changed_idle,
        .switched_to            = switched_to_idle,
        .update_curr            = update_curr_idle,
};

