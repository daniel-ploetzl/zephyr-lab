#include <zephyr/kernel.h>
#include "counter.h"
#include "zephyr_lab.h"

static uint32_t shared_count;
static uint32_t adds[COUNTER_SOURCES];

static K_MUTEX_DEFINE(count_lock);

void counter_add(enum counter_source source)
{
	uint32_t value;

	__ASSERT(source < COUNTER_SOURCES, "bad source %d", source);

	(void)k_mutex_lock(&count_lock, K_FOREVER);

	value = shared_count;
	k_busy_wait(INCREMENT_DELAY_US);
	shared_count = value + 1;
	adds[source]++;

	(void)k_mutex_unlock(&count_lock);
}

void counter_read(struct counter_snapshot *out)
{
	(void)k_mutex_lock(&count_lock, K_FOREVER);

	out->shared = shared_count;
	for (int i = 0; i < COUNTER_SOURCES; i++)
		out->adds[i] = adds[i];

	(void)k_mutex_unlock(&count_lock);
}
