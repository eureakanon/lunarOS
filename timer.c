#include"timer.h"
#include"ioctl.h"
#include"proc.h"

#define PIT_CH0 0x40 /*计数器端口*/
#define PIT_CMD 0x43 /*命令端口*/

volatile uint32 timer_ticks=0;/*时间刻*/

void init_timer(void)
{
    uint32 divisor=PIT_BASE_FREQ / HZ;

    io_out8(PIT_CMD,0x36);
    io_out8(PIT_CH0,(uint8)(divisor&0xFF));
    io_out8(PIT_CH0,(uint8)((divisor>>8)&0xFF));

    timer_ticks=0;
}

void timer_handler()
{
    timer_ticks++;
    yield();
}
