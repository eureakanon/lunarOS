#ifndef _TIMER_H
#define _TIMER_H
#include"types.h"

#define PIT_BASE_FREQ 1193182 /*基频*/
#define HZ 100 /*赫兹*/

extern volatile uint32 timer_ticks;/*从开机到现在的ticks*/

void init_timer(void);
void timer_handler(void);
#endif
