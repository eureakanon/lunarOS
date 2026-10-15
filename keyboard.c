#include"fifo.h"
#include"types.h"
#include"ioctl.h"
#include"int.h"
#include"keyboard.h"
struct FIFO8 keyfifo;
unsigned char keybuf[KEY_FIFO_BUF_SIZE];

void wait_KBC_sendready(void)
{
    for(;;)
    {
        if((io_in8(PORT_KEYSTA)&KEYSTA_SEND_NOTREADY)==0)/*忙等直至就绪*/
        {
            break;
        }
    }
}

void init_keyboard(void) {
    fifo8_init(&keyfifo, KEY_FIFO_BUF_SIZE, keybuf); 
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
}

void keyboard_handler() {
  uchar data = io_in8(PORT_KEYDAT);

  fifo8_put(&keyfifo, data);
}
