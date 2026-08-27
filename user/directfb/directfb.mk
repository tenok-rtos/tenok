PROJ_ROOT := $(dir $(lastword $(MAKEFILE_LIST)))/../..
DFB_DIR := $(PROJ_ROOT)/lib/directfb2

# The DirectFB2 submodule stays exactly as upstream left it, so what Tenok
# changes is put on before anything is built. Applying it resets the submodule
# first, which is why it happens once from a stamp and not on every build.
DFB_PATCHES := $(wildcard $(PROJ_ROOT)/lib/busybox-patches/directfb2/patches/*.patch)
DFB_PATCHED := $(DFB_DIR)/.tenok-patched

$(DFB_PATCHED): $(DFB_PATCHES)
	@$(PROJ_ROOT)/scripts/package-prepare.sh directfb2
	@touch $@
FLUX_DIR := $(PROJ_ROOT)/lib/flux

# The sources the NuttX build of DirectFB2 names, which is the build made for a
# system without an operating system underneath it. The input driver is left
# out: it talks to the NuttX button, keyboard and touchscreen devices, and
# Tenok has none of them.
DFB_SRC :=
DFB_SRC += $(addprefix $(DFB_DIR)/interfaces/IDirectFBFont/,\
                  idirectfbfont_dgiff.c)
DFB_SRC += $(addprefix $(DFB_DIR)/interfaces/IDirectFBImageProvider/,\
                  idirectfbimageprovider_dfiff.c)
DFB_SRC += $(addprefix $(DFB_DIR)/interfaces/IDirectFBVideoProvider/,\
                  idirectfbvideoprovider_dfvff.c)
DFB_SRC += $(addprefix $(DFB_DIR)/lib/direct/,\
                  clock.c \
                  conf.c \
                  debug.c \
                  direct.c \
                  direct_result.c \
                  hash.c \
                  init.c \
                  interface.c \
                  log.c \
                  log_domain.c \
                  map.c \
                  mem.c \
                  memcpy.c \
                  messages.c \
                  modules.c \
                  result.c \
                  stream.c \
                  system.c \
                  thread.c \
                  trace.c \
                  util.c)
DFB_SRC += $(addprefix $(DFB_DIR)/lib/direct/os/nuttx/,\
                  clock.c \
                  filesystem.c \
                  log.c \
                  mem.c \
                  mutex.c \
                  signals.c \
                  system.c \
                  thread.c)
DFB_SRC += $(addprefix $(DFB_DIR)/lib/fusion/,\
                  arena.c \
                  call.c \
                  conf.c \
                  fusion.c \
                  hash.c \
                  init.c \
                  lock.c \
                  object.c \
                  reactor.c \
                  ref.c \
                  shmalloc.c \
                  vector.c)
DFB_SRC += $(addprefix $(DFB_DIR)/lib/fusion/shm/,\
                  fake.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/core/,\
                  CoreDFB.c \
                  CoreDFB_real.c \
                  CoreGraphicsState.c \
                  CoreGraphicsStateClient.c \
                  CoreGraphicsState_real.c \
                  CoreInputDevice.c \
                  CoreInputDevice_real.c \
                  CoreLayer.c \
                  CoreLayerContext.c \
                  CoreLayerContext_real.c \
                  CoreLayerRegion.c \
                  CoreLayerRegion_real.c \
                  CoreLayer_real.c \
                  CorePalette.c \
                  CorePalette_real.c \
                  CoreScreen.c \
                  CoreScreen_real.c \
                  CoreSlave.c \
                  CoreSlave_real.c \
                  CoreSurface.c \
                  CoreSurfaceAllocation.c \
                  CoreSurfaceAllocation_real.c \
                  CoreSurfaceClient.c \
                  CoreSurfaceClient_real.c \
                  CoreSurface_real.c \
                  CoreWindow.c \
                  CoreWindowStack.c \
                  CoreWindowStack_real.c \
                  CoreWindow_real.c \
                  clipboard.c \
                  colorhash.c \
                  core.c \
                  core_parts.c \
                  fonts.c \
                  gfxcard.c \
                  graphics_state.c \
                  input.c \
                  layer_context.c \
                  layer_control.c \
                  layer_region.c \
                  layers.c \
                  local_surface_pool.c \
                  palette.c \
                  prealloc_surface_pool.c \
                  prealloc_surface_pool_bridge.c \
                  screen.c \
                  screens.c \
                  state.c \
                  surface.c \
                  surface_allocation.c \
                  surface_buffer.c \
                  surface_client.c \
                  surface_core.c \
                  surface_pool.c \
                  surface_pool_bridge.c \
                  system.c \
                  windows.c \
                  windowstack.c \
                  wm.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/,\
                  directfb.c \
                  directfb_result.c \
                  idirectfb.c \
                  init.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/display/,\
                  idirectfbdisplaylayer.c \
                  idirectfbpalette.c \
                  idirectfbscreen.c \
                  idirectfbsurface.c \
                  idirectfbsurface_layer.c \
                  idirectfbsurface_window.c \
                  idirectfbsurfaceallocation.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/gfx/,\
                  clip.c \
                  convert.c \
                  util.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/gfx/generic/,\
                  generic.c \
                  generic_blit.c \
                  generic_draw_line.c \
                  generic_fill_rectangle.c \
                  generic_stretch_blit.c \
                  generic_texture_triangles.c \
                  generic_util.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/input/,\
                  idirectfbeventbuffer.c \
                  idirectfbinputdevice.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/media/,\
                  idirectfbdatabuffer.c \
                  idirectfbdatabuffer_file.c \
                  idirectfbdatabuffer_memory.c \
                  idirectfbdatabuffer_streamed.c \
                  idirectfbfont.c \
                  idirectfbimageprovider.c \
                  idirectfbvideoprovider.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/misc/,\
                  conf.c \
                  gfx_util.c \
                  util.c)
DFB_SRC += $(addprefix $(DFB_DIR)/src/windows/,\
                  idirectfbwindow.c)
DFB_SRC += $(addprefix $(DFB_DIR)/systems/nuttxfb/,\
                  nuttxfb_layer.c \
                  nuttxfb_screen.c \
                  nuttxfb_surface_pool.c \
                  nuttxfb_system.c)
DFB_SRC += $(addprefix $(DFB_DIR)/wm/default/,\
                  default.c)

# DirectFB2 is compiled with its own flags. Its own headers come first, then
# Tenok's. CONFIG_TASK_NAME_SIZE is what NuttX calls the room a thread name
# needs, and Tenok says the same thing in <sys/prctl.h>.
DFB_CFLAGS := -O2 -g -mlittle-endian -mthumb \
              -ffunction-sections -fdata-sections \
              -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
              --specs=nano.specs --specs=nosys.specs \
              -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
              -Wno-sign-compare -Wno-implicit-fallthrough \
              -DBUILDTIME=\"tenok\" -DSYSCONFDIR=\"/etc\" \
              -DCONFIG_TASK_NAME_SIZE=PR_NAME_MAX \
              -I$(DFB_DIR) -I$(DFB_DIR)/include -I$(DFB_DIR)/src \
              -I$(DFB_DIR)/lib -I$(DFB_DIR)/systems \
              -I$(PROJ_ROOT)/include/tenok -I$(PROJ_ROOT)/include -I$(PROJ_ROOT)

# What DirectFB2 derives from its build configuration. Upstream writes these
# out of the Makefile it ships for NuttX; the values here are the same except
# for the size of a long, which is four on this machine and not eight.
DFB_BUILD_HDRS := $(DFB_DIR)/config.h \
                  $(DFB_DIR)/include/directfb_build.h \
                  $(DFB_DIR)/include/directfb_version.h \
                  $(DFB_DIR)/include/directfb_keynames.h \
                  $(DFB_DIR)/include/directfb_strings.h \
                  $(DFB_DIR)/lib/direct/build.h \
                  $(DFB_DIR)/lib/fusion/build.h \
                  $(DFB_DIR)/src/build.h

# The core interfaces are written once in an interface description and turned
# into C by fluxcomp, which is built from the sources next to DirectFB2.
FLUXCOMP := $(PROJ_ROOT)/lib/flux/fluxcomp
FLUX_ARGS := --call-mode --dispatch-error-abort --identity --include-prefix=core \
             --object-ptrs --static-args-bytes=FLUXED_ARGS_BYTES
DFB_FLUX := $(wildcard $(DFB_DIR)/src/core/*.flux)
DFB_FLUX_STAMP := $(DFB_DIR)/.tenok-fluxed

$(FLUXCOMP):
	@echo "FLUXCOMP"
	@$(PROJ_ROOT)/scripts/directfb-fluxcomp.sh

$(DFB_FLUX_STAMP): $(FLUXCOMP) $(DFB_FLUX)
	@echo "DIRECTFB flux"
	@for f in $(DFB_FLUX); do \
		$(FLUXCOMP) $(FLUX_ARGS) -c $$f -o=$(DFB_DIR)/src/core >/dev/null; \
	done
	@touch $@

# What fluxcomp writes are sources like any other, and the build has to know
# that they can be made. Without this make sees a source that is not there,
# finds no rule for it, and quietly leaves the object out of the link
DFB_FLUX_SRC := $(DFB_FLUX:.flux=.c)
DFB_FLUX_HDR := $(DFB_FLUX:.flux=.h)

$(DFB_FLUX_SRC) $(DFB_FLUX_HDR): $(DFB_FLUX_STAMP)

$(DFB_DIR)/config.h:
	@echo "DIRECTFB $(@F)"
	@echo "#pragma once" > $@
	@echo "#define SIZEOF_LONG 4" >> $@

$(DFB_DIR)/include/directfb_build.h:
	@echo "DIRECTFB $(@F)"
	@echo "#pragma once" > $@
	@echo "#define FLUXED_ARGS_BYTES 1024" >> $@

$(DFB_DIR)/include/directfb_version.h:
	@echo "DIRECTFB $(@F)"
	@echo "#pragma once" > $@
	@echo "#define DIRECTFB_MAJOR_VERSION 2" >> $@
	@echo "#define DIRECTFB_MINOR_VERSION 0" >> $@
	@echo "#define DIRECTFB_MICRO_VERSION 0" >> $@

$(DFB_DIR)/include/directfb_keynames.h: $(DFB_DIR)/include/directfb_keyboard.h
	@echo "DIRECTFB $(@F)"
	@$(DFB_DIR)/include/gen_directfb_keynames.sh $< > $@

$(DFB_DIR)/include/directfb_strings.h: $(DFB_DIR)/include/directfb.h
	@echo "DIRECTFB $(@F)"
	@$(DFB_DIR)/include/gen_directfb_strings.sh $< > $@

$(DFB_DIR)/lib/direct/build.h:
	@echo "DIRECTFB $(@F)"
	@echo "#pragma once" > $@
	@echo "#define DIRECT_BUILD_CTORS 0" >> $@
	@echo "#define DIRECT_BUILD_DEBUG 0" >> $@
	@echo "#define DIRECT_BUILD_DEBUGS 0" >> $@
	@echo "#define DIRECT_BUILD_DYNLOAD 0" >> $@
	@echo "#define DIRECT_BUILD_MEMCPY_PROBING 0" >> $@
	@echo "#define DIRECT_BUILD_NETWORK 0" >> $@
	@echo "#define DIRECT_BUILD_OS_NUTTX 1" >> $@
	@echo "#define DIRECT_BUILD_PIPED_STREAM 0" >> $@
	@echo "#define DIRECT_BUILD_SENTINELS 0" >> $@
	@echo "#define DIRECT_BUILD_TEXT 1" >> $@
	@echo "#define DIRECT_BUILD_TRACE 0" >> $@

$(DFB_DIR)/lib/fusion/build.h:
	@echo "DIRECTFB $(@F)"
	@echo "#pragma once" > $@
	@echo "#define FUSION_BUILD_MULTI 0" >> $@

$(DFB_DIR)/src/build.h:
	@echo "DIRECTFB $(@F)"
	@echo "#pragma once" > $@
	@echo "#define DFB_SMOOTH_SCALING 1" >> $@
	@echo "#define DIRECTFB_VERSION_VENDOR \"\"" >> $@

$(DFB_SRC:.c=.o): $(DFB_PATCHED) $(DFB_FLUX_STAMP) $(DFB_BUILD_HDRS)
$(DFB_SRC:.c=.o): CFLAGS := $(DFB_CFLAGS)

SRC += $(DFB_SRC)

# The shell command that runs a DirectFB2 program. It is built with Tenok's own
# flags plus the headers of DirectFB2, since it calls into both.
DFB_SHELL := $(PROJ_ROOT)/user/directfb/dfb_shell.c
DFB_SHELL += $(PROJ_ROOT)/user/directfb/modules.c

$(DFB_SHELL:.c=.o): $(DFB_PATCHED) $(DFB_FLUX_STAMP) $(DFB_BUILD_HDRS)
$(DFB_SHELL:.c=.o): CFLAGS += -I$(DFB_DIR) -I$(DFB_DIR)/include -I$(DFB_DIR)/lib \
    -I$(PROJ_ROOT)/user/directfb

SRC += $(DFB_SHELL)

