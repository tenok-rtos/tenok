PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..
BB_DIR := $(PROJ_ROOT)/lib/busybox

# The BusyBox sources needed by the enabled applets. The list is the transitive
# closure of the symbols they reference, computed against libbb.
BB_LIBBB := appletlib ask_confirmation auto_string bb_bswap_64 bb_cat \
            bb_getgroups bb_qsort bb_strtonum c_escape common_bufsiz \
            compare_string_array concat_path_file concat_subpath_file \
            copy_file copyfd default_error_retval dump endofname executable \
            fclose_nonstdin fflush_stdout_and_exit full_write \
            get_last_path_component get_line_from_file getopt32 hash_hmac \
            hash_md5_sha inode_hash last_char_is lineedit lineedit_ptr_hack \
            llist make_directory messages mode_string parse_mode perror_msg \
            perror_nomsg_and_die poll_with_signals printable_string \
            process_escape_sequence ptr_to_globals read read_key read_printf \
            recursive_action remove_file safe_gethostname safe_poll \
            safe_strncpy safe_write signals single_argv skip_whitespace \
            sysconf time u_signal_names verror_msg vfork_daemon_rexec wfopen \
            wfopen_input xatonum xfunc_die xfuncs xfuncs_printf xgetcwd \
            xreadlink xrealloc_vector
BB_APPLETS := coreutils/basename coreutils/cat coreutils/chmod coreutils/cp \
              coreutils/cut console-tools/clear \
              coreutils/date coreutils/dirname coreutils/echo coreutils/env \
              coreutils/false coreutils/head \
              coreutils/libcoreutils/cp_mv_stat coreutils/ls \
              coreutils/md5_sha1_sum coreutils/mkdir coreutils/mv \
              coreutils/od util-linux/hexdump \
              coreutils/printf coreutils/pwd coreutils/rm coreutils/sleep \
              coreutils/stat coreutils/tee coreutils/test \
              coreutils/test_ptr_hack coreutils/touch coreutils/tr \
              coreutils/true coreutils/uname coreutils/wc debianutils/which \
              editors/cmp shell/ash shell/ash_ptr_hack shell/math \
              shell/shell_common

BB_SRC := $(addprefix $(BB_DIR)/libbb/,$(addsuffix .c,$(BB_LIBBB))) \
          $(addprefix $(BB_DIR)/,$(addsuffix .c,$(BB_APPLETS)))

# BusyBox is compiled with its own flags: the compatibility headers come first
# and Tenok's own come after, so that the limits Tenok decides are the ones
# BusyBox is built against. main() is renamed because BusyBox is linked into
# the firmware instead of being executed.
BB_CFLAGS := -O2 -g -mlittle-endian -mthumb -fcommon \
             -fno-builtin-printf -fno-builtin-fprintf \
             -ffunction-sections -fdata-sections \
             -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
             --specs=nano.specs --specs=nosys.specs \
             -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
             -Wno-sign-compare -Wno-format-truncation -Wno-implicit-fallthrough \
             -D__TENOK__ -DBB_VER=\"1.38.0\" -D_GNU_SOURCE -Dmain=busybox_entry \
             -I$(PROJ_ROOT)/include/busybox \
             -I$(BB_DIR)/include -I$(BB_DIR)/libbb \
             -include $(BB_DIR)/include/autoconf.h \
             -I$(PROJ_ROOT)/include/tenok -I$(PROJ_ROOT)/include -I$(PROJ_ROOT)

# BusyBox derives several headers from its configuration. Its own build system
# is the only thing that knows how to produce them, so it is run and allowed to
# fail: it gets as far as the generated headers before it tries to compile
# anything with the Linux assumptions Tenok cannot satisfy.
BB_STAMP := $(BB_DIR)/.tenok-generated

# The configuration of Tenok names only what it turns on. Everything else is
# turned off first, and what the file names is put in place of it, so that no
# setting is left for the BusyBox configuration to ask about.
define BB_MERGE_CONFIG
NR == FNR { if ($$0 ~ /^CONFIG_/) { split($$0, a, "="); want[a[1]] = $$0 } ; next } \
{ \
	name = $$0; \
	sub(/^# /, "", name); sub(/ is not set$$/, "", name); sub(/=.*/, "", name); \
	print (name in want) ? want[name] : $$0; \
}
endef

$(BB_DIR)/.config: $(PROJ_ROOT)/configs/busybox.config
	@echo "BUSYBOX config"
	@$(MAKE) -C $(BB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) allnoconfig >/dev/null
	@awk '$(BB_MERGE_CONFIG)' $< $@ > $@.merged
	@mv $@.merged $@

# The applet table is derived from the //applet: comments of the applet
# sources, so a patch that changes one has to regenerate it
$(BB_STAMP): $(BB_DIR)/.config $(BB_SRC)
	@echo "BUSYBOX generate headers"
	-@$(MAKE) -C $(BB_DIR) CROSS_COMPILE=$(CROSS_COMPILE) >/dev/null 2>&1
	@test -f $(BB_DIR)/include/applet_tables.h
	@test -f $(BB_DIR)/include/autoconf.h
	@touch $@

$(BB_SRC:.c=.o): $(BB_STAMP)
$(BB_SRC:.c=.o): CFLAGS := $(BB_CFLAGS)

SRC += $(BB_SRC)

# The glue: POSIX shaped wrappers over Tenok, and the shell command that runs
# the multi-call binary. Both are built with Tenok's own flags.
SRC += $(PROJ_ROOT)/user/busybox/applet.c
SRC += $(PROJ_ROOT)/user/busybox/memory.c
SRC += $(PROJ_ROOT)/user/busybox/descriptor.c
SRC += $(PROJ_ROOT)/user/busybox/bb_shell.c
