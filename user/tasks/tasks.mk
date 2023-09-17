PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS += -I $(PROJ_ROOT)/user/tasks

# by deleting the source file you can disable
# the unwanted features
SRC += $(PROJ_ROOT)/user/tasks/led_task.c
SRC += $(PROJ_ROOT)/user/tasks/shell_task.c
#SRC += $(PROJ_ROOT)/user/tasks/debug_task.c # run `scripts/download-examples.sh` first
SRC += $(PROJ_ROOT)/user/tasks/mavlink_task.c

#SRC += $(PROJ_ROOT)/user/tasks/examples/fifo-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/mqueue-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/mutex-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/signal-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/timer-ex.c
SRC += $(PROJ_ROOT)/user/tasks/examples/poll-ex.c
