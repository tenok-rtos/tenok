PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS += -I $(PROJ_ROOT)/drivers/device
CFLAGS += -I $(PROJ_ROOT)/drivers/periph
CFLAGS += -I $(PROJ_ROOT)/drivers/periph/serial

SRC += $(PROJ_ROOT)/drivers/periph/serial/uart.c \
       $(PROJ_ROOT)/drivers/periph/serial/console.c \
       $(PROJ_ROOT)/drivers/periph/serial/mavlink.c \
       $(PROJ_ROOT)/drivers/periph/serial/debug_link.c \
       $(PROJ_ROOT)/drivers/periph/pwm.c
