#ifndef ALPACOS_TIMER_H
#define ALPACOS_TIMER_H

#include <stdint.h>

#define TIMER_HZ 100u

void timer_init(void);
uint32_t timer_uptime(void);

#endif
