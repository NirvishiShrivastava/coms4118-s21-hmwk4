/*
 * Weight Round-Robin Scheduling Class (mapped to the SCHED_WRR policy)
 */

#include "sched.h"
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/init_task.h>
#include <linux/jiffies.h>

unsigned long wrr_next_balance;

DEFINE_SPINLOCK(wrr_timer_lock);

/* Initializes wrr_rq variables, called by sched_fork()*/
void init_wrr_rq(struct wrr_rq *wrr_rq)
{
	INIT_LIST_HEAD(&wrr_rq->wrr_rq_list);
	wrr_rq->total_rq_weight = 0;
	wrr_rq->wrr_nr_running = 0;
	raw_spin_lock_init(&wrr_rq->wrr_rq_lock);

	/* Initialize wrr_next_balance by any one CPU */
	spin_lock(&wrr_timer_lock);
	if (!wrr_next_balance)
		wrr_next_balance = jiffies;
	spin_unlock(&wrr_timer_lock);
}

/* Enqueue runnable wrr task_struct to wrr_rq*/

static void __enqueue_wrr_entity(struct rq *rq,
		struct sched_wrr_entity *wrr_se, unsigned int flags)
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

/* The function keeps track of the time the current process spent
 * executing on the CPU and updates the current task's runtime statistics.
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
	/* if delta is greater than we have ever seen before,
	 * we update stats.
	 */
	schedstat_set(curr->se.statistics.exec_max,
		      max(curr->se.statistics.exec_max, delta_exec));
	curr->se.sum_exec_runtime += delta_exec;
	/* update the thread group cpu time */
	account_group_exec_runtime(curr, delta_exec);
	/*  update exec_start time for next time */
	curr->se.exec_start = now;
	cgroup_account_cputime(curr, delta_exec);
}

static void __dequeue_wrr_entity(struct rq *rq,
		struct sched_wrr_entity *wrr_se, unsigned int flags)
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

	/* idle balance  */
#ifdef CONFIG_SMP
	/*if (rq->wrr.wrr_nr_running == 0)
		pull_wrr_task(rq);*/
#endif
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

/* Put task to the head or the end of the run list,
 * without the overhead of dequeue followed by enqueue.
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

	/*
	if (time_after(jiffies, rq->wrr.wrr_next_balance)) {
		rq_unpin_lock(rq, rf);
		wrr_periodic_load_balance();
		rq_repin_lock(rq, rf);
		rq->wrr.wrr_next_balance = jiffies + HZ / 2;
	}
	*/

	if (p->policy != SCHED_WRR)
		return;

	if (--p->wrr.wrr_se_timeslice)
		return;
	wrr_se->wrr_se_timeslice = DEFAULT_WRR_TIMESLICE *
					wrr_se->wrr_se_weight;

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

#ifdef CONFIG_SMP

static inline void idle_pull_wrr_task(struct rq *this_rq)
{
	int this_cpu = this_rq->cpu;
	int src_cpu, cpu_iter, max_wrr_weight;
	struct rq *src_rq;
	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;

	src_cpu = -1;
	max_wrr_weight = 0;

	/* Do not pull tasks from non-active CPUs */
	if (!cpu_active(this_cpu))
		return;

	/* Iterate through CPUs to find highest weight CPU */
	rcu_read_lock();
	for_each_online_cpu(cpu_iter) {
		if (cpu_iter == this_cpu)
			continue;

		src_rq = cpu_rq(cpu_iter);
		if (src_rq->wrr.wrr_nr_running <= 1)
			continue;

		if (max_wrr_weight < src_rq->wrr.total_rq_weight) {
			src_cpu = cpu_iter;
			max_wrr_weight = src_rq->wrr.total_rq_weight;
		}
	}
	rcu_read_unlock();

	/* If no CPU found return */
	if (src_cpu == -1)
		return;

	/* Source CPU found from which task can be pulled */
	src_rq = cpu_rq(src_cpu);

	double_lock_balance(this_rq, src_rq);

	/* Iterate over src_rq to find the task to be pulled */
	list_for_each_entry(wrr_se, &src_rq->wrr.wrr_rq_list, run_list) {
		p = wrr_task_of(wrr_se);

		if (task_running(src_rq, p)
				|| !cpumask_test_cpu(this_cpu, p->cpus_ptr))
			continue;

		//WARN_ON(p == src_rq->curr);
		WARN_ON(!task_on_rq_queued(p));
		deactivate_task(src_rq, p, 0);
		set_task_cpu(p, this_cpu);
		activate_task(this_rq, p, 0);
		double_unlock_balance(this_rq, src_rq);
		return;
	}
	double_unlock_balance(this_rq, src_rq);
}

void wrr_periodic_load_balance(void)
{
	int min_cpu, max_cpu, cpu_iter;
	int min_wrr_weight, max_wrr_weight;
	int eligible_task_weight;
	struct rq *min_rq, *max_rq, *curr_rq;
	struct task_struct *p;
	struct sched_wrr_entity *wrr_se;
	int imb, old_imb;
	unsigned long now;

	min_wrr_weight = INT_MAX;
	max_wrr_weight = INT_MIN;

	min_cpu = -1;
	max_cpu = -1;

	spin_lock(&wrr_timer_lock);

	now = jiffies;
	if time_after_eq(now, wrr_next_balance) {
		wrr_next_balance = now + HZ / 2;
		spin_unlock(&wrr_timer_lock);
	} else {
		spin_unlock(&wrr_timer_lock);
		return;
	}

	/* Iterate through CPUs to find highest weight CPU */
	rcu_read_lock();
	for_each_online_cpu(cpu_iter) {
		curr_rq = cpu_rq(cpu_iter);

		pr_info("Curr weight %d", curr_rq->wrr.total_rq_weight);
		pr_info("Initial Max weight %d", max_wrr_weight);
		pr_info("Initial Min Weight %d", min_wrr_weight);
		if (max_wrr_weight < curr_rq->wrr.total_rq_weight) {
			pr_info("Updating wrr_max_weight");
			max_cpu = cpu_iter;
			max_wrr_weight = curr_rq->wrr.total_rq_weight;
		}

		if (min_wrr_weight > curr_rq->wrr.total_rq_weight) {
			min_cpu = cpu_iter;
			min_wrr_weight = curr_rq->wrr.total_rq_weight;
		}
	}
	rcu_read_unlock();

	pr_info("Initial Max weight %d", max_wrr_weight);
	pr_info("Initial Min Weight %d", min_wrr_weight);
	pr_info("(Max, Min) CPUs: (%d, %d)", max_cpu, min_cpu);
	if (min_cpu == -1 || max_cpu == -1)
		return;

	old_imb = abs(max_wrr_weight - min_wrr_weight);

	/* If imbalance is 0 */
	if (!old_imb)
		return;

	/* Run Queues between which migration may occur */
	min_rq = cpu_rq(min_cpu);
	max_rq = cpu_rq(max_cpu);

	double_lock_balance(min_rq, max_rq);

	/* Iterate over max_rq to find the task to be pulled */
	list_for_each_entry(wrr_se, &max_rq->wrr.wrr_rq_list, run_list) {
		p = wrr_task_of(wrr_se);

		if (task_running(max_rq, p)
				|| !cpumask_test_cpu(min_cpu, p->cpus_ptr))
			continue;

		eligible_task_weight = p->wrr.wrr_se_weight;
		imb = abs(min_wrr_weight + eligible_task_weight -
				(max_wrr_weight - eligible_task_weight));

		/* Check if imbalance is reversed */
		if (imb > old_imb)
			continue;

		WARN_ON(!task_on_rq_queued(p));
		deactivate_task(max_rq, p, 0);
		set_task_cpu(p, min_cpu);
		activate_task(min_rq, p, 0);
		double_unlock_balance(min_rq, max_rq);
		return;
	}
	double_unlock_balance(min_rq, max_rq);
}

static int balance_wrr(struct rq *rq, struct task_struct *p,
		struct rq_flags *rf)
{
	rq_unpin_lock(rq, rf);

	/* Idle balance check */
	if (rq->wrr.wrr_nr_running == 0)
		idle_pull_wrr_task(rq);

	rq_repin_lock(rq, rf);

	return rq->wrr.wrr_nr_running;
}

static int
select_task_rq_wrr(struct task_struct *p, int cpu, int sd_flag, int flags)
{
	int min_weight, temp_weight;
	struct rq *rq;
	int cpu_iter;

	min_weight = INT_MAX;
	rcu_read_lock();
	for_each_online_cpu(cpu_iter) {
		rq = cpu_rq(cpu_iter);
		temp_weight = rq->wrr.total_rq_weight;
		if (min_weight > temp_weight) {
			min_weight = temp_weight;
			cpu = cpu_iter;
		} else if (min_weight == temp_weight && cpu > cpu_iter)
			cpu = cpu_iter;
	}

	rcu_read_unlock();
	return cpu;
}

#endif

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
}

static void check_preempt_curr_wrr(struct rq *rq,
		struct task_struct *p, int flags)
{
}

/*
 * All the scheduling class methods:
 */
const struct sched_class wrr_sched_class = {

	.next			= &fair_sched_class,
	.enqueue_task		= enqueue_task_wrr,
	.dequeue_task           = dequeue_task_wrr,
	.yield_task		= yield_task_wrr,
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

