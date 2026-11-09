#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "counter.h"
#include "zephyr_lab.h"

static K_SEM_DEFINE(worker_wake, 0, 1);

static void on_timer(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	k_sem_give(&worker_wake);
}

static K_TIMER_DEFINE(worker_timer, on_timer, NULL);

static void worker(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1)
	{
		(void)k_sem_take(&worker_wake, K_FOREVER);
		counter_add(COUNTER_WORKER);
	}
}

K_THREAD_DEFINE(worker_tid, WORKER_STACK_SIZE, worker, NULL, NULL, NULL,
		WORKER_PRIORITY, 0, 0);

static void report(void)
{
	static uint32_t timer_fires;
	struct counter_snapshot now;
	uint32_t total;

	counter_read(&now);
	timer_fires += k_timer_status_get(&worker_timer);
	total = now.adds[COUNTER_MAIN] + now.adds[COUNTER_WORKER];

	printk("main %u + worker %u = %u, shared %u, lost %u, timer %u\n",
	       now.adds[COUNTER_MAIN], now.adds[COUNTER_WORKER], total,
	       now.shared, total - now.shared, timer_fires);
}

int main(void)
{
	int64_t next_report = REPORT_PERIOD_MS;

	k_timer_start(&worker_timer, K_MSEC(WORKER_PERIOD_MS),
		      K_MSEC(WORKER_PERIOD_MS));

	while (1)
	{
		counter_add(COUNTER_MAIN);

		if (k_uptime_get() >= next_report)
		{
			report();
			next_report += REPORT_PERIOD_MS;
		}

		k_msleep(MAIN_PERIOD_MS);
	}
	return 0;
}
