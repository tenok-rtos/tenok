PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS+=-I./user/shell

# by deleting the source file you can disable
# the unwanted features
SRC+=$(PROJ_ROOT)/user/shell/shell.c \
	$(PROJ_ROOT)/user/shell/cat.c \
	$(PROJ_ROOT)/user/shell/clear.c \
	$(PROJ_ROOT)/user/shell/file.c \
	$(PROJ_ROOT)/user/shell/history.c \
	$(PROJ_ROOT)/user/shell/mpool.c \
	$(PROJ_ROOT)/user/shell/pwd.c \
	$(PROJ_ROOT)/user/shell/cd.c \
	$(PROJ_ROOT)/user/shell/echo.c \
	$(PROJ_ROOT)/user/shell/help.c \
	$(PROJ_ROOT)/user/shell/ls.c \
	$(PROJ_ROOT)/user/shell/ps.c \
