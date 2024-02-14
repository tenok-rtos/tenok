PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS += -I $(PROJ_ROOT)/drivers/device
CFLAGS += -I $(PROJ_ROOT)/drivers/periph
CFLAGS += -I $(PROJ_ROOT)/drivers/periph/serial

SRC += $(PROJ_ROOT)/drivers/periph/serial/uart.c
SRC += $(PROJ_ROOT)/drivers/periph/serial/uart1.c
SRC += $(PROJ_ROOT)/drivers/periph/serial/uart2.c
SRC += $(PROJ_ROOT)/drivers/periph/serial/uart3.c
SRC += $(PROJ_ROOT)/drivers/periph/pwm.c
SRC += $(PROJ_ROOT)/drivers/periph/spi.c
SRC += $(PROJ_ROOT)/drivers/devices/mpu6500.c
