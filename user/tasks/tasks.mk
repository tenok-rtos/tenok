PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

include $(PROJ_ROOT)/user/navigation/navigation.mk

CFLAGS += -I $(PROJ_ROOT)/user/tasks

# By deleting the source file you can disable
# the unwanted features
SRC += $(PROJ_ROOT)/user/tasks/led_task.c
SRC += $(PROJ_ROOT)/user/tasks/shell_task.c
#SRC += $(PROJ_ROOT)/user/tasks/debug_task.c # Run `scripts/download-examples.sh` first
SRC += $(PROJ_ROOT)/user/tasks/mavlink_task.c

# Demo projects
SRC += $(PROJ_ROOT)/user/tasks/examples/quadrotor.c

# Examples
#SRC += $(PROJ_ROOT)/user/tasks/examples/fifo-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/mqueue-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/semaphore.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/mutex-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/priority-inversion.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/signal-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/timer-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/poll-ex.c
#SRC += $(PROJ_ROOT)/user/tasks/examples/pthread-ex.c
