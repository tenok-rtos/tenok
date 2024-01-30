PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../../..

CFLAGS += -DHAS_STDIO -DHAS_PRINTF -DCORE_DEBUG=0 -DMAIN_HAS_NOARGC=0 -DCOMPILER_REQUIRES_SORT_RETURN=0
CFLAGS += -DITERATIONS=2500 -DPERFORMANCE_RUN=1 -DVALIDATION_RUN=0 -DPROFILE_RUN=0 -DFLAGS_STR=\"\"

CFLAGS += -I $(PROJ_ROOT)/benchmarks/coremark

SRC += $(PROJ_ROOT)/user/benchmarks/coremark/core_list_join.c
SRC += $(PROJ_ROOT)/user/benchmarks/coremark/core_main.c
SRC += $(PROJ_ROOT)/user/benchmarks/coremark/core_matrix.c
SRC += $(PROJ_ROOT)/user/benchmarks/coremark/core_portme.c
SRC += $(PROJ_ROOT)/user/benchmarks/coremark/core_state.c
SRC += $(PROJ_ROOT)/user/benchmarks/coremark/core_util.c
