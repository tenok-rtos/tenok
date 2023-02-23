PROJECT=neo-rtenv
ELF=$(PROJECT).elf
BIN=$(PROJECT).bin

-include config.mk

CFLAGS=-g -mlittle-endian -mthumb \
	-mcpu=cortex-m4 \
	-mfpu=fpv4-sp-d16 -mfloat-abi=hard \
	--specs=nano.specs \
	--specs=nosys.specs
CFLAGS+=-D USE_STDPERIPH_DRIVER
CFLAGS+=-D STM32F4xx
CFLAGS+=-D ARM_MATH_CM4 \
	-D __FPU_PRESENT=1 \
	-D __FPU_USED=1

USER=$(shell whoami)
CFLAGS+=-D__USER_NAME__=\"$(USER)\"

CFLAGS+=-Wl,-T,platform/stm32f407.ld

ST_LIB=./lib/STM32F4xx_StdPeriph_Driver

CFLAGS+=-I./lib/CMSIS/ST/STM32F4xx/Include
CFLAGS+=-I./lib/CMSIS/Include
CFLAGS+=-I$(ST_LIB)/inc

CFLAGS+=-I./
CFLAGS+=-I./platform
CFLAGS+=-I./drivers
CFLAGS+=-I./kernel
CFLAGS+=-I./mm
CFLAGS+=-I./apps
CFLAGS+=-I./examples

SRC=./lib/CMSIS/system_stm32f4xx.c

SRC+=$(ST_LIB)/src/misc.c \
	$(ST_LIB)/src/stm32f4xx_rcc.c \
	$(ST_LIB)/src/stm32f4xx_dma.c \
	$(ST_LIB)/src/stm32f4xx_flash.c \
	$(ST_LIB)/src/stm32f4xx_gpio.c \
	$(ST_LIB)/src/stm32f4xx_usart.c \
	$(ST_LIB)/src/stm32f4xx_tim.c \
	$(ST_LIB)/src/stm32f4xx_spi.c \
	$(ST_LIB)/src/stm32f4xx_i2c.c

SRC+=./kernel/fifo.c \
	./kernel/mqueue.c \
	./kernel/ringbuf.c \
	./kernel/list.c \
	./kernel/kernel.c \
	./kernel/fs.c \
	./kernel/semaphore.c \
	./kernel/mutex.c \
	./kernel/rom_dev.c \
	./kernel/reg_file.c \
	./mm/mpool.c \
	./drivers/gpio.c \
	./drivers/uart.c \
	./apps/shell.c \
	./apps/shell_cmd.c \
	./examples/fifo-ex.c \
	./examples/mutex-ex.c \
	./examples/mqueue-ex.c \
	./main.c \

OBJS=$(SRC:.c=.o)
OBJS+=./tools/romfs.o

DEPEND=$(SRC:.c=.d)

ASM=./platform/startup_stm32f4xx.s \
	./kernel/context_switch.s \
	./kernel/syscall.s \
	./kernel/spinlock.s

all:$(ELF)

$(ELF): $(ASM) $(OBJS)
	@echo "LD" $@
	@$(CC) $(CFLAGS) $(OBJS) $(ASM) $(LDFLAGS) -o $@

$(BIN): $(ELF)
	@echo "OBJCPY" $@
	@$(OBJCOPY) -O binary $(PROJECT).elf $(PROJECT).bin

-include $(DEPEND)

tools/romfs.o:
	@$(MAKE) -C ./tools -f Makefile

%.o: %.s 
	@echo "CC" $@
	@$(CC) $(CFLAGS) $^ $(LDFLAGS) -c $<

%.o: %.c
	@echo "CC" $@
	@$(CC) $(CFLAGS) -MMD -MP -c $< $(LDFLAGS) -o $@

clean:
	rm -rf $(ELF)
	rm -rf $(OBJS)
	rm -rf $(DEPEND)
	rm -rf *.orig
	@$(MAKE) -C ./tools -f Makefile clean

qemu: all
	$(QEMU) -cpu cortex-m4 \
	-M netduinoplus2 \
	-serial /dev/null \
	-serial /dev/null \
	-serial stdio \
	-gdb tcp::3333 \
	-kernel ./$(ELF)

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

gdbauto:
	cgdb -d $(GDB) -x ./gdb/openocd_gdb.gdb

astyle:
	astyle --style=linux --indent=spaces=4 --indent-switches \
		--suffix=none  --exclude=lib --recursive "*.c,*.h"


size:
	$(SIZE) $(ELF)

.PHONY: all clean qemu flash openocd gdbauto astyle size
