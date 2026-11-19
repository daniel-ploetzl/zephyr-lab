#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include "counter.h"
#include "zephyr_lab.h"

struct timer_event
{
	uint32_t sequence;
	uint32_t created_ms;
};

K_MSGQ_DEFINE_STATIC(worker_events, sizeof(struct timer_event),
		EVENT_QUEUE_CAPACITY, 1);

static atomic_t dropped_events;
static atomic_t last_sequence;
static atomic_t last_wait_ms;

static void on_timer(struct k_timer *timer)
{
	static uint32_t sequence;

	ARG_UNUSED(timer);

	sequence++;
	struct timer_event event = {
		.sequence = sequence,
		.created_ms = k_uptime_get_32(),
	};
	int err = k_msgq_put(&worker_events, &event, K_NO_WAIT);

	if (err != 0)
	{
		(void)atomic_inc(&dropped_events);
	}
}

static K_TIMER_DEFINE(worker_timer, on_timer, NULL);

static void worker(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1)
	{
		struct timer_event event;
		int err = k_msgq_get(&worker_events, &event, K_FOREVER);

		if (err != 0)
		{
			__ASSERT(err == 0, "queue receive failed: %d", err);
			continue;
		}

		uint32_t wait_ms = k_uptime_get_32() - event.created_ms;

		counter_add(COUNTER_WORKER);
		(void)atomic_set(&last_sequence, (atomic_val_t)event.sequence);
		(void)atomic_set(&last_wait_ms, (atomic_val_t)wait_ms);
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
	uint32_t dropped = (uint32_t)atomic_get(&dropped_events);
	uint32_t sequence = (uint32_t)atomic_get(&last_sequence);
	uint32_t wait_ms = (uint32_t)atomic_get(&last_wait_ms);

	printk("main %u + worker %u = %u, shared %u, lost %u, timer %u, "
	       "dropped %u, seq %u, wait %u ms\n",
	       now.adds[COUNTER_MAIN], now.adds[COUNTER_WORKER], total,
	       now.shared, total - now.shared, timer_fires, dropped, sequence,
	       wait_ms);
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
