PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS += -DTIME
CFLAGS += -I $(PROJ_ROOT)/benchmarks/dhrystone

# Enable the feature by uncommenting the line

# To execute the Dhrystone benchmarking program, type `dhrystone'
# command to the shell
# SRC += $(PROJ_ROOT)/user/benchmarks/dhrystone/dhrystone.c
