PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS += -I $(PROJ_ROOT)/drivers/devices
CFLAGS += -I $(PROJ_ROOT)/drivers/periph

# The drivers written against the Standard Peripheral Library, which every
# board but the newest has, along with the parts that only the board that flies
# carries. A board that has neither says so and names its own instead
PERIPH_DRV ?= uart.c pwm.c spi.c
DEVICE_DRV ?= mpu6500.c sbus.c

# The line discipline every serial driver hands its bytes to, whichever
# library the driver itself is written against
SRC += $(PROJ_ROOT)/drivers/periph/serial_tty.c

SRC += $(addprefix $(PROJ_ROOT)/drivers/periph/,$(PERIPH_DRV))
SRC += $(addprefix $(PROJ_ROOT)/drivers/devices/,$(DEVICE_DRV))
