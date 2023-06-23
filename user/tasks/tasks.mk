PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS+=-I$(PROJ_ROOT)/user/tasks

# by deleting the source file you can disable
# the unwanted features
SRC+=$(PROJ_ROOT)/user/tasks/led_task.c \
	$(PROJ_ROOT)/user/tasks/shell_task.c \
	$(PROJ_ROOT)/user/tasks/debug_link_task.c
