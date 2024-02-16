PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS += -I $(PROJ_ROOT)/user/navigation

SRC += $(PROJ_ROOT)/user/navigation/madgwick_filter.c
