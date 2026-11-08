#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "counter.h"
#include "zephyr_lab.h"

static void worker(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1)
	{
		counter_add(COUNTER_WORKER);
		k_msleep(WORKER_PERIOD_MS);
	}
}

K_THREAD_DEFINE(worker_tid, WORKER_STACK_SIZE, worker, NULL, NULL, NULL,
		WORKER_PRIORITY, 0, 0);

static void report(void)
{
	struct counter_snapshot now;

	counter_read(&now);

	uint32_t total = now.adds[COUNTER_MAIN] + now.adds[COUNTER_WORKER];

	printk("main %u + worker %u = %u, shared %u, lost %u\n",
	       now.adds[COUNTER_MAIN], now.adds[COUNTER_WORKER], total,
	       now.shared, total - now.shared);
}

int main(void)
{
	int64_t next_report = REPORT_PERIOD_MS;

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
