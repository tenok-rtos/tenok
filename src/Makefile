PROJECT=naive_f4_os
ELF=$(PROJECT).elf
BIN=$(PROJECT).bin

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
GDB=arm-none-eabi-gdb
SIZE=arm-none-eabi-size

CFLAGS=-g -mlittle-endian -mthumb \
	-mcpu=cortex-m4 \
	-mfpu=fpv4-sp-d16 -mfloat-abi=hard \
	--specs=nano.specs \
	--specs=nosys.specs
CFLAGS+=-D USE_STDPERIPH_DRIVER
CFLAGS+=-D STM32F4xx
CFLAGS+=-D __FPU_PRESENT=1 \
        -D ARM_MATH_CM4 \
	-D __FPU_USED=1

USER=$(shell whoami)
CFLAGS+=-D__USER_NAME__=\"$(USER)\"

CFLAGS+=-Wl,-T,startup/stm32_flash.ld

ST_LIB=./lib/STM32F4xx_StdPeriph_Driver

CFLAGS+=-I./lib/CMSIS/ST/STM32F4xx/Include
CFLAGS+=-I./lib/CMSIS/Include
CFLAGS+=-I$(ST_LIB)/inc

CFLAGS+=-I./
CFLAGS+=-I./startup/
CFLAGS+=-I./common
CFLAGS+=-I./drivers
CFLAGS+=-I./kernel
CFLAGS+=-I./mm
CFLAGS+=-I./apps

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
	./kernel/ringbuf.c \
	./kernel/list.c \
	./kernel/kernel.c \
	./kernel/file.c \
	./mm/mpool.c \
	./drivers/gpio.c \
	./drivers/uart.c \
	./apps/shell.c \
	./main.c \

OBJS=$(SRC:.c=.o)
DEPEND=$(SRC:.c=.d)

ASM=./startup/startup_stm32f4xx.s \
	./kernel/context_switch.s \
	./kernel/syscall.s \
	./kernel/porting.s

all:$(ELF)

$(ELF): $(ASM) $(OBJS)
	@echo "LD" $@
	@$(CC) $(CFLAGS) $(OBJS) $(ASM) $(LDFLAGS) -o $@

-include $(DEPEND)

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
	astyle --style=linux --indent=tab=8 --exclude=lib --recursive "*.c,*.h"

size:
	$(SIZE) $(ELF)

.PHONY:all clean flash openocd gdbauto
