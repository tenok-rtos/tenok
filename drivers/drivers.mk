PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS+=-I$(PROJ_ROOT)/drivers/serial

SRC+=$(PROJ_ROOT)/drivers/serial/uart.c \
	$(PROJ_ROOT)/drivers/serial/serial0.c \
	$(PROJ_ROOT)/drivers/serial/serial1.c
