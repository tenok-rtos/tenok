PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../

CFLAGS+=-I$(PROJ_ROOT)/drivers/serial

# by deleting the source file you can disable
# the unwanted features
SRC+=$(PROJ_ROOT)/drivers/serial/uart.c
