#ifndef COUNTER_H
#define COUNTER_H

#include <stdint.h>

enum counter_source
{
	COUNTER_MAIN,
	COUNTER_WORKER,
	COUNTER_SOURCES
};

struct counter_snapshot
{
	uint32_t shared;
	uint32_t adds[COUNTER_SOURCES];
};

void counter_add(enum counter_source source);
void counter_read(struct counter_snapshot *out);

#endif
