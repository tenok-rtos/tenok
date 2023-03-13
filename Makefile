include config.mk

.DEFAULT_GOAL=all

CFLAGS=
SRC=

#board selection
include platform/qemu.mk
#include platform/stm32f4disc.mk
#include platform/stm32f429disc.mk

CFLAGS+=-g -mlittle-endian -mthumb \
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

ST_LIB=./lib/STM32F4xx_StdPeriph_Driver

CFLAGS+=-I./lib/CMSIS/ST/STM32F4xx/Include
CFLAGS+=-I./lib/CMSIS/Include
CFLAGS+=-I$(ST_LIB)/inc

CFLAGS+=-I./
CFLAGS+=-I./platform
CFLAGS+=-I./kernel
CFLAGS+=-I./mm
CFLAGS+=-I./apps
CFLAGS+=-I./examples

SRC+=lib/CMSIS/DSP_Lib/Source/CommonTables/arm_common_tables.c \
	lib/CMSIS/DSP_Lib/Source/FastMathFunctions/arm_cos_f32.c \
	lib/CMSIS/DSP_Lib/Source/FastMathFunctions/arm_sin_f32.c \
	lib/CMSIS/DSP_Lib/Source/StatisticsFunctions/arm_power_f32.c \
	lib/CMSIS/DSP_Lib/Source/StatisticsFunctions/arm_max_f32.c \
	lib/CMSIS/DSP_Lib/Source/BasicMathFunctions/arm_sub_f32.c \
	lib/CMSIS/DSP_Lib/Source/BasicMathFunctions/arm_dot_prod_f32.c \
	lib/CMSIS/DSP_Lib/Source/SupportFunctions/arm_copy_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_init_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_scale_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_add_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_sub_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_mult_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_trans_f32.c \
	lib/CMSIS/DSP_Lib/Source/MatrixFunctions/arm_mat_inverse_f32.c

SRC+=./lib/CMSIS/system_stm32f4xx.c

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
        ./kernel/file.c \
	./kernel/semaphore.c \
	./kernel/mutex.c \
	./kernel/rom_dev.c \
	./kernel/reg_file.c \
	./mm/mpool.c \
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
	@$(MAKE) -C ./tools -f Makefile

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

check:
	$(CPPCHECK) . -i lib/

clean:
	rm -rf $(ELF)
	rm -rf $(OBJS)
	rm -rf $(DEPEND)
	rm -rf *.orig
	@$(MAKE) -C ./tools -f Makefile clean

gdbauto:
	cgdb -d $(GDB) -x ./gdb/openocd_gdb.gdb

astyle:
	astyle --style=linux --indent=spaces=4 --indent-switches \
		--suffix=none  --exclude=lib --recursive "*.c,*.h"

size:
	$(SIZE) $(ELF)

.PHONY: all check clean gdbauto astyle size
