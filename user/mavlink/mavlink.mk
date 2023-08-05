PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..

CFLAGS+=-I$(PROJ_ROOT)/user/mavlink

# by deleting the source file you can disable
# the unwanted features
SRC+=$(PROJ_ROOT)/user/mavlink/parser.c \
	$(PROJ_ROOT)/user/mavlink/publisher.c \
	$(PROJ_ROOT)/user/mavlink/hil.c
