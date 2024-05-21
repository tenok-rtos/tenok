# STM32F4DISCOVERY board (https://www.st.com/en/evaluation-tools/stm32f4discovery.html)

LD_SCRIPT += platform/stm32f407.ld

CFLAGS += -D STM32F40_41xxx \
          -D SYSTEM_CORE_CLOCK=168000000 \
          -D HSE_VALUE=8000000 \
          -D PLL_M=8 \
          -D PLL_N=336 \
          -D PLL_P=2 \
          -D PLL_Q=7 \
	  -D ENABLE_UART1_DMA=1 \
	  -D ENABLE_UART3_DMA=1 \
          -D __ARCH__=\"armv7m\" \
          -D __BOARD_NAME__=\"stm32f407\"

CFLAGS += -I./drivers/boards
CFLAGS += -I./user/tasks

# Board specific driver
SRC += ./drivers/boards/stm32f4disc.c

# Example tasks
SRC += ./user/tasks/led_task.c
SRC += ./user/tasks/shell_task.c
#SRC += ./user/tasks/debug_task.c # Run `scripts/download-examples.sh` first
SRC += ./user/tasks/mavlink_task.c
#SRC += ./user/tasks/examples/fifo-ex.c
#SRC += ./user/tasks/examples/mqueue-ex.c
#SRC += ./user/tasks/examples/semaphore.c
#SRC += ./user/tasks/examples/mutex-ex.c
#SRC += ./user/tasks/examples/priority-inversion.c
#SRC += ./user/tasks/examples/signal-ex.c
#SRC += ./user/tasks/examples/timer-ex.c
#SRC += ./user/tasks/examples/poll-ex.c
#SRC += ./user/tasks/examples/pthread-ex.c

flash:
	openocd -f interface/stlink.cfg \
	-f target/stm32f4x.cfg \
	-c "init" \
	-c "reset init" \
	-c "halt" \
	-c "flash write_image erase $(ELF)" \
	-c "verify_image $(ELF)" \
	-c "reset run" -c shutdown

openocd:
	openocd -s /opt/openocd/share/openocd/scripts/ -f ./gdb/openocd.cfg

.PHONY: flash openocd
