# NUCLEO-F746ZG board (https://www.st.com/en/evaluation-tools/nucleo-f746zg.html)
#
# The core is a Cortex-M7 and the part is of the F7 family, for which ST never
# wrote a Standard Peripheral Library. What it wrote instead is STM32Cube, of
# which the Low Layer is the part that speaks to the registers the way the
# older library did. That is what is used here.

LD_SCRIPT += platform/stm32f746.ld

# A Cortex-M7 with a single precision floating point unit, an instruction cache
# and a data cache. The caches are left as the reset leaves them, which is off
CPU_FLAGS := -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard

ST_LIB := ./lib/stm32f7xx-ll
CMSIS_DEVICE := ./lib/cmsis-device-f7
CMSIS_CORE := ./lib/cmsis-core/CMSIS/Core

ST_DEFINES := -D USE_FULL_LL_DRIVER -D STM32F746xx -D ARM_MATH_CM7
FPU_DEFINES :=

ST_LIB_INC := $(ST_LIB)/Inc $(CMSIS_DEVICE)/Include
CMSIS_DEVICE_INC := $(CMSIS_DEVICE)/Include

# The core this part has came after the CMSIS the older boards are built
# against, so its definitions are taken from the one ST keeps up to date. The
# maths library of the older CMSIS is left behind with it: it knows nothing of
# a Cortex-M7, and the only code that asks anything of it is the code that
# flies a board this one is not
CMSIS_CORE_INC := $(CMSIS_CORE)/Include
CMSIS_DSP_SRC :=
# The vendor's own copy of this brings the part up on the clock its reset
# leaves it at and goes no further, and what runs before the kernel is the only
# place the rest of the way can be gone. So the tree keeps its own, the way it
# already does for the older family
CMSIS_SYSTEM := ./platform/system_stm32f746.c
STARTUP := $(CMSIS_DEVICE)/Source/Templates/gcc/startup_stm32f746xx.s

# The same peripherals the older boards drive, named the way the Low Layer
# names them. There is no module of its own for SYSCONFIG or the NVIC: the
# first is reached through the external interrupt module and the second is
# what CMSIS itself provides
ST_LIB_SRC := $(ST_LIB)/Src/stm32f7xx_ll_rcc.c \
              $(ST_LIB)/Src/stm32f7xx_ll_dma.c \
              $(ST_LIB)/Src/stm32f7xx_ll_gpio.c \
              $(ST_LIB)/Src/stm32f7xx_ll_usart.c \
              $(ST_LIB)/Src/stm32f7xx_ll_tim.c \
              $(ST_LIB)/Src/stm32f7xx_ll_spi.c \
              $(ST_LIB)/Src/stm32f7xx_ll_i2c.c \
              $(ST_LIB)/Src/stm32f7xx_ll_exti.c \
              $(ST_LIB)/Src/stm32f7xx_ll_pwr.c \
              $(ST_LIB)/Src/stm32f7xx_ll_utils.c

CFLAGS += -D SYSTEM_CORE_CLOCK=216000000 \
          -D HSE_VALUE=8000000 \
          -D __ARCH__=\"armv7m\" \
          -D __BOARD_NAME__=\"nucleo-f746zg\"

# Only the serial port and the LEDs this board has, driven the way the newer
# library drives them. The parts that only the board that flies carries are
# left behind with the library that knew them
PERIPH_DRV := uart_ll.c
DEVICE_DRV :=

# The one peripheral of this part the Low Layer was never written for, and the
# half of the vendor's library that was written for it
CFLAGS += -I./drivers/net
SRC += ./drivers/net/eth.c
SRC += $(ST_LIB)/Src/stm32f7xx_hal_eth.c

ifdef CONFIG_NET
include ./net/net.mk
endif

CFLAGS += -I./drivers/boards
CFLAGS += -I./user/tasks

# Board specific driver
SRC += ./drivers/boards/nucleo_f746zg.c

# Example tasks
SRC += ./user/tasks/led_task.c
SRC += ./user/tasks/shell_task.c

flash:
	openocd -f interface/stlink.cfg \
	-f target/stm32f7x.cfg \
	-c "init" \
	-c "reset init" \
	-c "halt" \
	-c "flash write_image erase $(ELF)" \
	-c "verify_image $(ELF)" \
	-c "reset run" -c shutdown

openocd:
	openocd -s /opt/openocd/share/openocd/scripts/ -f ./gdb/openocd.cfg

.PHONY: flash openocd
