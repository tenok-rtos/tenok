include config.mk

.DEFAULT_GOAL := all

LDFLAGS :=
CFLAGS :=
SRC :=
LD_SCRIPT :=
LD_GENERATED := generated.ld

ST_LIB := ./lib/STM32F4xx_StdPeriph_Driver

#board selection
include platform/qemu.mk
#include platform/stm32f4disc.mk
#include platform/stm32f429disc.mk

MSG_DIR   := ./msg
MSG_BUILD := ./build/msg

LDFLAGS += -Wl,--no-warn-rwx-segments

CFLAGS += -g -mlittle-endian -mthumb \
          -fcommon \
          -mcpu=cortex-m4 \
          -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
          --specs=nano.specs \
          --specs=nosys.specs
CFLAGS += -Wall \
          -Werror=undef \
          -Wno-unused-function \
          -Wno-format-truncation \
          -Wno-address-of-packed-member
CFLAGS += -D USE_STDPERIPH_DRIVER
CFLAGS += -D STM32F4xx
CFLAGS += -D ARM_MATH_CM4 \
          -D __FPU_PRESENT=1 \
          -D __FPU_USED=1
CFLAGS += -Wl,-T,$(LD_GENERATED)

USER = $(shell whoami)
CFLAGS += -D__USER_NAME__=\"$(USER)\"

CFLAGS += -I ./lib/CMSIS/ST/STM32F4xx/Include
CFLAGS += -I ./lib/CMSIS/Include
CFLAGS += -I $(ST_LIB)/inc

CFLAGS += -I ./lib/mavlink
CFLAGS += -I ./lib/mavlink/common

CFLAGS += -I ./
CFLAGS += -I ./platform
CFLAGS += -I ./include
CFLAGS += -I ./include/arch
CFLAGS += -I ./include/common
CFLAGS += -I ./include/fs
CFLAGS += -I ./include/tenok
CFLAGS += -I ./include/tenok/sys
CFLAGS += -I ./include/kernel
CFLAGS += -I ./user
CFLAGS += -I ./user/debug-link
CFLAGS += -I ./build/msg

SRC += lib/CMSIS/DSP_Lib/Source/CommonTables/arm_common_tables.c \
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

SRC += ./lib/CMSIS/system_stm32f4xx.c

SRC += $(ST_LIB)/src/misc.c \
       $(ST_LIB)/src/stm32f4xx_rcc.c \
       $(ST_LIB)/src/stm32f4xx_dma.c \
       $(ST_LIB)/src/stm32f4xx_flash.c \
       $(ST_LIB)/src/stm32f4xx_gpio.c \
       $(ST_LIB)/src/stm32f4xx_usart.c \
       $(ST_LIB)/src/stm32f4xx_tim.c \
       $(ST_LIB)/src/stm32f4xx_spi.c \
       $(ST_LIB)/src/stm32f4xx_i2c.c

SRC += ./kernel/arch/v7m_port.c \
       ./kernel/kfifo.c \
       ./kernel/list.c \
       ./kernel/kernel.c \
       ./kernel/task.c \
       ./kernel/sched.c \
       ./kernel/fcntl.c \
       ./kernel/pipe.c \
       ./kernel/mqueue.c \
       ./kernel/mutex.c \
       ./kernel/semaphore.c \
       ./kernel/pthread.c \
       ./kernel/signal.c \
       ./kernel/time.c \
       ./kernel/printf.c \
       ./kernel/printk.c \
       ./kernel/softirq.c \
       ./kernel/fs/fs.c \
       ./kernel/fs/file.c \
       ./kernel/fs/reg_file.c \
       ./kernel/fs/rom_dev.c \
       ./kernel/mm/mpool.c \
       ./kernel/mm/mm.c \
       ./kernel/mm/page.c \
       ./kernel/mm/slab.c \
       ./main.c

SRC += ./user/debug-link/tenok_link.c 

-include ./drivers/drivers.mk
-include ./user/shell/shell.mk
-include ./user/tasks/tasks.mk
-include ./user/mavlink/mavlink.mk

OBJS := $(SRC:.c=.o)
OBJS += ./tools/mkromfs/romfs.o

DEPEND = $(SRC:.c=.d)

ASM := ./platform/startup_stm32f4xx.s \
       ./kernel/arch/context_switch.S \
       ./kernel/arch/spinlock.S

all: gen_syscalls msggen $(LD_GENERATED) $(ELF)
	@$(MAKE) -C ./tools/mkromfs/ -f Makefile

$(ELF): $(ASM) $(OBJS)
	@echo "LD" $@
	@$(CC) $(CFLAGS) $(OBJS) $(ASM) $(LDFLAGS) -o $@
	@rm $(LD_GENERATED)

$(BIN): $(ELF)
	@echo "OBJCPY" $@
	@$(OBJCOPY) -O binary $(PROJECT).elf $(PROJECT).bin

gen_syscalls:
	./scripts/gen-syscalls.py > include/kernel/syscall.h

-include $(DEPEND)

tools/mkromfs/romfs.o:
	@$(MAKE) -C ./tools/mkromfs/ -f Makefile

$(LD_GENERATED): $(LD_SCRIPT) 
	@echo "CC" $< ">" $@
	@$(CC) -E -P -x c $(CFLAGS) $<>$@  

%.o: %.s 
	@echo "CC" $@
	@$(CC) $(CFLAGS) $^ $(LDFLAGS) -c $<

%.o: %.c
	@echo "CC" $@
	@$(CC) $(CFLAGS) -MMD -MP -c $< $(LDFLAGS) -o $@

check:
	$(CPPCHECK) . -i lib/

clean:
	rm -rf $(LD_GENERATED)
	rm -rf $(ELF)
	rm -rf $(OBJS)
	rm -rf $(DEPEND)
	rm -rf *.orig
	@$(MAKE) -C ./tools/mkromfs -f Makefile clean

msggen:
	@$(MAKE) -C ./tools/msggen/ -f Makefile
	rm -rf $(MSG_BUILD)
	mkdir -p $(MSG_BUILD)
	@echo "msggen" $(MSG_DIR) $(MSG_BUILD)
	@./tools/msggen/msggen $(MSG_DIR) $(MSG_BUILD)

gdbauto:
	cgdb -d $(GDB) -x ./gdb/openocd_gdb.gdb

FORMAT_EXCLUDE = -path ./lib -o -path ./platform
FORMAT_FILES = ".*\.\(c\|h\)"

format:
	@echo "Execute clang-format"
	@find . -type d \( $(FORMAT_EXCLUDE) \) -prune -o \
                -regex $(FORMAT_FILES) -print \
                -exec clang-format -style=file -i {} \;

size:
	$(SIZE) $(ELF)

objdump:
	$(OBJDUMP) -d $(ELF) > $(ELF).asm

doxygen:
	@tail -n +4 README.md > main_page.md
	@doxygen docs/Doxyfile
	@rm -rf main_page.md
	@echo "doxygen docs/Doxyfile"

.PHONY: all check clean gdbauto format size objdump msggen gen_syscalls doxygen
