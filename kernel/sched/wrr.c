#include "sched.h"
#include <linux/sched.h>
#include <linux/spinlock.h>

/* Initializes wrr_rq variables, called by sched_fork()*/
void init_wrr_rq(struct wrr_rq *wrr_rq)
{
	INIT_LIST_HEAD(&wrr_rq->wrr_rq_list);
        wrr_rq->total_rq_weight = 0;
	wrr_rq->wrr_nr_running = 0;
        wrr_rq->current_task = NULL;
	raw_spin_lock_init(&wrr_rq->wrr_rq_lock);
}

/* Enqueue runnable wrr task_struct to wrr_rq*/

static void __enqueue_wrr_entity(struct rq *rq, struct sched_wrr_entity *wrr_se, unsigned int flags)
{
    struct wrr_rq *wrr_rq = rq->wrr;
    if (flags & ENQUEUE_HEAD)
        list_add(&wrr_se->run_list, &rq->wrr.wrr_rq_list);
    else
        list_add_tail(&wrr_se->run_list, &rq->wrr.wrr_rq_list);
    rq->wrr.total_weight += wrr_se->wrr_weight;
    wrr_rq->wrr_nr_running += 1;
    add_nr_running(rq, 1);
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    struct sched_wrr_entity *wrr_se = &p->wrr;

    if (flags & ENQUEUE_WAKEUP)
        wrr_se->timeout = 0;

    __enqueue_wrr_entity(rq, wrr_se, flags);
}

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

