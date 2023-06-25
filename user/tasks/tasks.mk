PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS+=-I$(PROJ_ROOT)/user/tasks

# by deleting the source file you can disable
# the unwanted features
SRC+=$(PROJ_ROOT)/user/tasks/led_task.c
SRC+=$(PROJ_ROOT)/user/tasks/shell_task.c
SRC+=$(PROJ_ROOT)/user/tasks/debug_link_task.c
SRC+=$(PROJ_ROOT)/user/tasks/mavlink_task.c

#SRC+=$(PROJ_ROOT)/user/tasks/examples/fifo-ex.c
#SRC+=$(PROJ_ROOT)/user/tasks/examples/mqueue-ex.c
#SRC+=$(PROJ_ROOT)/user/tasks/examples/mutex-ex.c
