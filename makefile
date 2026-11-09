NASM    = nasm
CC      = gcc
LD      = ld
OBJCOPY = objcopy

CFLAGS  = -m32 -ffreestanding -fno-pic -fno-pie \
          -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -O2 -Wall -Wextra

LDFLAGS = -m elf_i386 -T kernel.ld -nostdlib

IMG = os.img

KERNEL_C   = kernel.c output.c proc.c sem.c fifo.c \
             desctbl.c int.c keyboard.c trap.c timer.c \
			 tss.c pmm.c paging.c klibc.c
KERNEL_ASM = ioctl.asm swtch.asm trap_entry.asm user_prog.asm

KERNEL_OBJS = $(KERNEL_C:.c=.o) $(KERNEL_ASM:.asm=.o)

all: $(IMG)

boot.bin:  boot.asm
	$(NASM) -f bin $< -o $@

setup.bin: setup.asm
	$(NASM) -f bin $< -o $@

%.o: %.asm
	$(NASM) -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(KERNEL_OBJS) kernel.ld
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

$(IMG): boot.bin setup.bin kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=boot.bin   of=$@ conv=notrunc bs=512 seek=0 status=none
	dd if=setup.bin  of=$@ conv=notrunc bs=512 seek=1 status=none
	dd if=kernel.bin of=$@ conv=notrunc bs=512 seek=9 status=none

run: $(IMG)
	qemu-system-i386 -fda $(IMG) -boot a

clean:
	rm -f *.bin *.o *.elf $(IMG)

.PHONY: all clean run
