PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS += -I $(PROJ_ROOT)/user/shell

# by deleting the source file you can disable
# the unwanted features
SRC += $(PROJ_ROOT)/user/shell/shell.c
SRC += $(PROJ_ROOT)/user/shell/cat.c
SRC += $(PROJ_ROOT)/user/shell/clear.c
SRC += $(PROJ_ROOT)/user/shell/file.c
SRC += $(PROJ_ROOT)/user/shell/history.c
SRC += $(PROJ_ROOT)/user/shell/mpool.c
SRC += $(PROJ_ROOT)/user/shell/pwd.c
SRC += $(PROJ_ROOT)/user/shell/cd.c
SRC += $(PROJ_ROOT)/user/shell/echo.c
SRC += $(PROJ_ROOT)/user/shell/help.c
SRC += $(PROJ_ROOT)/user/shell/ls.c
SRC += $(PROJ_ROOT)/user/shell/ps.c
SRC += $(PROJ_ROOT)/user/shell/xxd.c
