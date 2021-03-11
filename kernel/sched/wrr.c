/*
 * Weight Round-Robin Scheduling Class (mapped to the SCHED_WRR policy)
 */

#include "sched.h"
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/init_task.h>

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
	struct wrr_rq *wrr_rq = &rq->wrr;
	if (flags & ENQUEUE_HEAD)
		list_add(&wrr_se->run_list, &rq->wrr.wrr_rq_list);
	else
		list_add_tail(&wrr_se->run_list, &rq->wrr.wrr_rq_list);
	rq->wrr.total_rq_weight += wrr_se->wrr_se_weight;
	wrr_rq->wrr_nr_running += 1;
}

static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	
	if (flags & ENQUEUE_WAKEUP)
		wrr_se->timeout = 0;
	
	__enqueue_wrr_entity(rq, wrr_se, flags);
	add_nr_running(rq, 1);

}

/* 
 *The function keeps track of the time the current process spent 
 *executing on the CPU and updates the current task's runtime 
 *statistics.
 */
static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct sched_wrr_entity *wrr_se = &curr->wrr;
	u64 delta_exec;
	u64 now;

	if (curr->sched_class != &wrr_sched_class)
		return;

	now = rq_clock_task(rq);
	
	/* delta_exec shows the time that current process ran */
	delta_exec = now - curr->se.exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;
	
	/* if delta is greater than we have ever seen before, we update stats. */
	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));
	curr->se.sum_exec_runtime += delta_exec;
	
	/* update the thread group cpu time */
	account_group_exec_runtime(curr, delta_exec);
	
	/*  update exec_start time for next time */
	curr->se.exec_start = now;
	cgroup_account_cputime(curr, delta_exec);

}

static void __dequeue_wrr_entity(struct rq *rq, struct sched_wrr_entity *wrr_se, unsigned int flags)
{
	list_del_init(&wrr_se->run_list);
	rq->wrr.total_rq_weight -= wrr_se->wrr_se_weight;
	--rq->wrr.wrr_nr_running;
}

/* Dequeue non-runnable wrr task_struct from wrr_rq*/
static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);
	__dequeue_wrr_entity(rq, wrr_se, flags);
	sub_nr_running(rq, 1);
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
	return container_of(wrr_se, struct task_struct, wrr);
}

static struct task_struct *__pick_next_task_wrr(struct rq *rq)
{
	struct sched_wrr_entity *next_se = NULL;
	struct list_head *queue  = &(rq->wrr.wrr_rq_list);
	next_se = list_first_entry(queue, struct sched_wrr_entity, run_list);
	
	return wrr_task_of(next_se);
}

static struct task_struct *
pick_next_task_wrr(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	struct task_struct *p;
    
	if (!rq->wrr.wrr_nr_running)
		return NULL;
	
	p = __pick_next_task_wrr(rq);
	
	/* Setting up start time for task_struct p*/
	p->se.exec_start = rq_clock_task(rq); 
	return p;
}

/*
 * Put task to the head or the end of the run list without the overhead of dequeue followed by enqueue.
 */
static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;
	struct list_head *queue  = &(rq->wrr.wrr_rq_list);
	
	if (head)
		list_move(&wrr_se->run_list, queue);
	else
		list_move_tail(&wrr_se->run_list, queue);
}

static void yield_task_wrr(struct rq *rq)
{
	requeue_task_wrr(rq, rq->curr, 0);
}

/* scheduler tick hitting a task of our scheduling class. */
static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
	struct sched_wrr_entity *wrr_se = &p->wrr;

	update_curr_wrr(rq);
	
	/* TODO: Check if we require this function
	* update_wrr_rq_load_avg(rq_clock_pelt(rq), rq, 1);
	*/
	
	if (p->policy != SCHED_WRR)
		return;

	if (--p->wrr.wrr_se_timeslice)
		return;

	wrr_se->wrr_se_timeslice = DEFAULT_WRR_TIMESLICE * wrr_se->wrr_se_weight;

	/* Requeue to the end of queue */
	if (wrr_se->run_list.prev != wrr_se->run_list.next) {
		requeue_task_wrr(rq, p, 0);
		resched_curr(rq);
	}
}

/* Called right before prev is going to be taken off the CPU. */
static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev)
{
	update_curr_wrr(rq);
}


static inline void set_next_task_wrr(struct rq *rq, struct task_struct *p)
{
	p->se.exec_start = rq_clock_task(rq);
}


static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
	BUG();
}


static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	return task->wrr.wrr_se_weight * DEFAULT_WRR_TIMESLICE;
}

/*
 * All the scheduling class methods:
 */
const struct sched_class wrr_sched_class = {

	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
        .dequeue_task           = dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
	//.check_preempt_curr     = check_preempt_curr_wrr,

        .pick_next_task         = pick_next_task_wrr,
        .put_prev_task          = put_prev_task_wrr,
        .set_next_task          = set_next_task_wrr,

#ifdef CONFIG_SMP
        /*.balance                = balance_wrr,
        .select_task_rq         = select_task_rq_wrr,*/
        .set_cpus_allowed       = set_cpus_allowed_common,
#endif

        .task_tick              = task_tick_wrr,

        .get_rr_interval        = get_rr_interval_wrr,

        .prio_changed           = prio_changed_wrr,
        /*.switched_to            = switched_to_wrr,*/
        .update_curr            = update_curr_wrr,
};

