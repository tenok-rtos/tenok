PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS += -I $(PROJ_ROOT)/drivers/device
CFLAGS += -I $(PROJ_ROOT)/drivers/serial
CFLAGS += -I $(PROJ_ROOT)/drivers/periph

SRC += $(PROJ_ROOT)/drivers/serial/uart.c \
       $(PROJ_ROOT)/drivers/serial/console.c \
       $(PROJ_ROOT)/drivers/serial/mavlink.c \
       $(PROJ_ROOT)/drivers/serial/debug_link.c \
       $(PROJ_ROOT)/drivers/periph/pwm.c
