#include"timer.h"
#include"ioctl.h"
#include"proc.h"

#define PIT_CH0 0x40
#define PIT_CMD 0x43

volatile uint32 timer_ticks = 0;
volatile int    need_resched = 0;   /* 新增：请求调度的标志 */
void init_timer(void)
{
    uint32 divisor = PIT_BASE_FREQ / HZ;
    io_out8(PIT_CMD, 0x36);
    io_out8(PIT_CH0, (uint8)(divisor & 0xFF));
    io_out8(PIT_CH0, (uint8)((divisor >> 8) & 0xFF));
    timer_ticks = 0;
    need_resched = 0;
}

void timer_handler(void)
{
    timer_ticks++;
    need_resched = 1;    /* 只设标志，不切 */
}
