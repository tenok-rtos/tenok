# The configuration, which says which board is built and what is built into it.
#
# Kconfig is the language the choices are written in and Kconfiglib is what
# reads it. The file it writes is .config, which is not kept in the repository:
# what is kept is a defconfig for each board, holding only what differs from
# the defaults.

KCONFIG_DIR := ./lib/kconfiglib
KCONFIG_CONFIG ?= .config
AUTOCONF := include/generated/autoconf.h

PYTHON ?= python3
KCONFIG_ENV := srctree=. PYTHONPATH=$(KCONFIG_DIR) KCONFIG_CONFIG=$(KCONFIG_CONFIG)

DEFCONFIG_DIR := configs

# Only the targets that make a configuration are allowed to run without one
KCONFIG_TARGETS := menuconfig defconfig savedefconfig oldconfig olddefconfig \
                   alldefconfig allnoconfig listnewconfig
NOCONFIG_GOALS := $(KCONFIG_TARGETS) $(notdir $(basename $(wildcard $(DEFCONFIG_DIR)/*_defconfig))) \
                  clean distclean help

ifeq ($(filter $(NOCONFIG_GOALS),$(MAKECMDGOALS)),)
ifeq ($(wildcard $(KCONFIG_CONFIG)),)
$(error No $(KCONFIG_CONFIG). Run "make menuconfig" or "make <board>_defconfig")
endif

# What the makefiles need to know before anything is read: which board this is
include $(KCONFIG_CONFIG)
PLATFORM := $(patsubst "%",%,$(CONFIG_PLATFORM))
endif

menuconfig:
	@$(KCONFIG_ENV) $(PYTHON) $(KCONFIG_DIR)/menuconfig.py Kconfig

oldconfig olddefconfig alldefconfig allnoconfig listnewconfig:
	@$(KCONFIG_ENV) $(PYTHON) $(KCONFIG_DIR)/$@.py Kconfig

# What differs from the defaults, which is what a defconfig of a board holds
savedefconfig:
	@$(KCONFIG_ENV) $(PYTHON) $(KCONFIG_DIR)/savedefconfig.py \
	    --kconfig Kconfig --out defconfig
	@echo "SAVED defconfig"

%_defconfig:
	@echo "CONFIG" $@
	@$(KCONFIG_ENV) $(PYTHON) $(KCONFIG_DIR)/defconfig.py \
	    --kconfig Kconfig $(DEFCONFIG_DIR)/$@

# Written only when what it says has changed, so that a configuration saved
# again does not rebuild the whole system
$(AUTOCONF): $(KCONFIG_CONFIG)
	@echo "GEN" $@
	@mkdir -p $(dir $@)
	@$(KCONFIG_ENV) $(PYTHON) $(KCONFIG_DIR)/genconfig.py \
	    --header-path $@.new Kconfig
	@cmp -s $@.new $@ || mv $@.new $@
	@rm -f $@.new

.PHONY: menuconfig oldconfig olddefconfig alldefconfig allnoconfig \
        listnewconfig savedefconfig
