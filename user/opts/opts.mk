# The Open POSIX Test Suite, which is what Tenok is measured against for
# conformance. It is fetched rather than kept in the tree, because it is needed
# only when the tests are built:
#
#     ./scripts/download-posix-tests.sh
#
# Which of its tests can be run is worked out by the build rather than listed
# here: a test that names something Tenok does not have will not compile, and a
# test that wants a second process cannot be run at all. Both are left out, so
# that what is measured follows what the system gains.

OPTS_DIR := $(PROJ_ROOT)/lib/open-posix-testsuite
OPTS_BUILD := $(PROJ_ROOT)/build/opts
OPTS_LIST := $(OPTS_BUILD)/opts_tests

OPTS_CFLAGS := -I$(OPTS_DIR)/include \
               -I$(PROJ_ROOT)/include/tenok \
               -I$(PROJ_ROOT)/include/tenok/sys \
               -I$(PROJ_ROOT)/include \
               -I$(PROJ_ROOT) \
               -mcpu=cortex-m4 -mthumb --specs=nano.specs

$(OPTS_LIST).mk $(OPTS_LIST).h: $(PROJ_ROOT)/scripts/gen-posix-tests.py \
                                $(OPTS_DIR)/.tenok-revision
	@echo "GEN" $(OPTS_LIST).h
	@mkdir -p $(OPTS_BUILD)
	@CC="$(CC)" $(PROJ_ROOT)/scripts/gen-posix-tests.py $(OPTS_LIST) \
	    $(OPTS_CFLAGS)

-include $(OPTS_LIST).mk

# A test says how it went by calling exit(), which would end the task it runs
# in. The name is pointed at the shell command instead, which carries the
# answer back to the loop that called the test
$(OPTS_SRC:.c=.o): CFLAGS := $(CFLAGS) -I$(OPTS_DIR)/include -w -Dexit=opts_exit
$(OPTS_SRC:.c=.o): $(OPTS_LIST).h

SRC += $(OPTS_SRC)

OPTS_SHELL := $(PROJ_ROOT)/user/opts/opts_shell.c

$(OPTS_SHELL:.c=.o): $(OPTS_LIST).h
$(OPTS_SHELL:.c=.o): CFLAGS += -I$(OPTS_BUILD) -I$(OPTS_DIR)/include

SRC += $(OPTS_SHELL)
