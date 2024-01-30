PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

# Enable the benchmark by uncommenting the line and execute them with shell
#include $(PROJ_ROOT)/user/benchmarks/dhrystone/dhrystone.mk
#include $(PROJ_ROOT)/user/benchmarks/coremark/coremark.mk
