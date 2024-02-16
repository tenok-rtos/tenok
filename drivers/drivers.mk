PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS += -I $(PROJ_ROOT)/drivers/devices
CFLAGS += -I $(PROJ_ROOT)/drivers/periph

SRC += $(PROJ_ROOT)/drivers/periph/uart.c
SRC += $(PROJ_ROOT)/drivers/periph/pwm.c
SRC += $(PROJ_ROOT)/drivers/periph/spi.c
SRC += $(PROJ_ROOT)/drivers/devices/mpu6500.c
SRC += $(PROJ_ROOT)/drivers/devices/sbus.c
