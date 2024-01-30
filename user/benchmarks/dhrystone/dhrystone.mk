PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../../..

CFLAGS += -DTIME
CFLAGS += -I $(PROJ_ROOT)/benchmarks/dhrystone

SRC += $(PROJ_ROOT)/user/benchmarks/dhrystone/dhrystone.c
