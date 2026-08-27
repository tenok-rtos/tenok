include ./makefiles/config.mk

.DEFAULT_GOAL := all

LDFLAGS :=
CFLAGS :=
SRC :=
LD_SCRIPT :=
LD_GENERATED := generated.ld

ST_LIB := ./lib/STM32F4xx_StdPeriph_Driver

include ./makefiles/$(PLATFORM).mk

MSG_DIR   := ./msg
MSG_BUILD := ./build/msg

LDFLAGS += -Wl,--no-warn-rwx-segments
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -lm

CFLAGS += -ffunction-sections -fdata-sections

# The compiler turns a printf() with no conversions into a puts() and one with
# a single string into an fputs(), naming the functions of the C library it
# assumes. Tenok has its own, reached through names of its own, so the rewrite
# would step past them into newlib.
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf
CFLAGS += -O2 -g -mlittle-endian -mthumb \
          -fcommon \
          -mcpu=cortex-m4 \
          -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
          --specs=nano.specs \
          --specs=nosys.specs

CFLAGS += -Wall \
          -Werror=undef \
          -Wno-unused-function \
          -Wno-format-truncation \
          -Wno-address-of-packed-member \
          -Wno-array-bounds # FIXME

CFLAGS += -D USE_STDPERIPH_DRIVER \
          -D STM32F4xx \
          -D ARM_MATH_CM4 \
          -D __FPU_PRESENT=1 \
          -D __FPU_USED=1

CFLAGS += -Wl,-T,$(LD_GENERATED)


CFLAGS += -I./lib/CMSIS/ST/STM32F4xx/Include
CFLAGS += -I./lib/CMSIS/Include
CFLAGS += -I$(ST_LIB)/inc

CFLAGS += -I./lib/mavlink
CFLAGS += -I./lib/mavlink/common

CFLAGS += -I./
CFLAGS += -I./platform
CFLAGS += -I./include
CFLAGS += -I./include/common
CFLAGS += -I./include/tenok
CFLAGS += -I./include/tenok/sys
CFLAGS += -I./include/kernel
CFLAGS += -I./include/kernel/arch
CFLAGS += -I./include/kernel/fs
CFLAGS += -I./include/filters
CFLAGS += -I./user
CFLAGS += -I./user/debug-link
CFLAGS += -I./build/msg

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
       $(ST_LIB)/src/stm32f4xx_i2c.c \
       $(ST_LIB)/src/stm32f4xx_syscfg.c \
       $(ST_LIB)/src/stm32f4xx_exti.c

SRC += ./kernel/arch/v7m_port.c \
       ./kernel/fs/fs.c \
       ./kernel/fs/vfs.c \
       ./kernel/fs/wrapper.c \
       ./kernel/fs/reg_file.c \
       ./kernel/fs/rom_dev.c \
       ./kernel/fs/null_dev.c \
       ./kernel/pwd.c \
       ./kernel/socket.c \
       ./kernel/resource.c \
       ./kernel/sysconf.c \
       ./kernel/signal_posix.c \
       ./kernel/utsname.c \
       ./kernel/termios.c \
       ./kernel/fnmatch.c \
       ./kernel/libgen.c \
       ./kernel/mm/mpool.c \
       ./kernel/mm/mm.c \
       ./kernel/mm/page.c \
       ./kernel/mm/slab.c \
       ./kernel/kfifo.c \
       ./kernel/kernel.c \
       ./kernel/task.c \
       ./kernel/sched.c \
       ./kernel/file.c \
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
       ./main.c

SRC += ./user/debug-link/debug_link.c 

-include ./drivers/drivers.mk
-include ./user/shell/shell.mk
-include ./user/busybox/busybox.mk
-include ./user/mavlink/mavlink.mk
-include ./user/benchmarks/benchmarks.mk

OBJS := $(SRC:.c=.o)
OBJS += ./tools/mkromfs/romfs.o

DEPEND = $(SRC:.c=.d)

SYSCALL_HDR := include/kernel/syscall.h

# Every platform compiles the same sources with different pins, clocks and
# board names, and the object files it makes look alike. Building one platform
# on top of what another left gives firmware that is half of each, and it fails
# in ways that point nowhere near the build.
#
# The name of the platform is written down in a file that every object depends
# on. The file is only written when the name in it is not the name being built,
# so building the same platform again leaves it alone, and building a different
# one makes every object out of date the way an edited header would.
PLATFORM_STAMP := .platform

$(PLATFORM_STAMP): FORCE
	@last=$$(cat $@ 2>/dev/null); \
	 if [ "$$last" != "$(PLATFORM)" ]; then \
	     echo "PLATFORM $${last:+$$last -> }$(PLATFORM)"; \
	     echo "$(PLATFORM)" > $@; \
	 fi

FORCE:

.PHONY: FORCE

$(OBJS): $(PLATFORM_STAMP) $(SYSCALL_HDR)

# What the build knows and the kernel does not: which revision this is, when it
# was made, and by whom. Only what reads these is made to depend on them, so
# that a new commit does not rebuild the whole system
VERSION_HDR := include/kernel/version.h

$(VERSION_HDR): FORCE
	@./scripts/gen-version.sh $@

./kernel/utsname.o ./user/tasks/shell_task.o: $(VERSION_HDR)

ASM := ./kernel/arch/v7m_entry.S \
       ./platform/startup_stm32f4xx.s

all: $(SYSCALL_HDR) msggen $(LD_GENERATED) $(ELF)
	@$(MAKE) -C ./tools/mkromfs/ -f Makefile

$(ELF): $(ASM) $(OBJS)
	@echo "LD" $@
	@$(CC) $(CFLAGS) $(OBJS) $(ASM) $(LDFLAGS) -o $@
	@rm $(LD_GENERATED)

$(BIN): $(ELF)
	@echo "OBJCPY" $@
	@$(OBJCOPY) -O binary $(PROJECT).elf $(PROJECT).bin

# A file and not a recipe of its own, so that what reads it is rebuilt after it
# is written and not on the build after that. Written only when what it says
# has changed, so that building again does not rebuild what reads it at all
$(SYSCALL_HDR): scripts/gen-syscalls.py
	@echo "GEN" $@
	@./scripts/gen-syscalls.py > $@.new
	@cmp -s $@.new $@ || mv $@.new $@
	@rm -f $@.new

gen_syscalls: $(SYSCALL_HDR)

-include $(DEPEND)

# The image is linked into the firmware, so it has to be packed again before
# the link and not only in the "all" recipe, which runs after it. The contents
# come from the directory HOST_INPUT_DIR of tools/mkromfs/mkromfs.c names.
ROM_SRC := $(shell find ./rom -type f 2>/dev/null)

tools/mkromfs/romfs.o: tools/mkromfs/mkromfs.c $(ROM_SRC)
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
	rm -rf $(PLATFORM_STAMP)
	rm -rf $(VERSION_HDR)
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
