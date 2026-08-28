DirectFB2 on Tenok
==================

`DirectFB2` is built into the firmware of the board that has a display. Each
example is a shell command that draws until the board is reset:

```
USER@stm32f429:/$ fire
fire is drawing in thread 4
USER@stm32f429:/$ ps
```

### 1. What is there

`dfb` draws a blue field with a red bar and a white frame, which is enough to
see that the display works. Beside it are the examples of DirectFB2:

| Command | What it draws |
| --- | --- |
| `gears` | The turning gears, through OpenGL |
| `window` | Windows stacked on the display layer, with an image and a font |
| `fire` | A fire, written straight into the pixels of a surface |
| `palette` | A palette surface, drawn by changing the palette rather than the pixels |
| `particle` | A field of particles |
| `matrix` | A surface drawn through a transformation matrix |
| `vkcolor` | The colour keying of a surface |

Which of them are built is chosen in `make menuconfig`, under `User space`.
None of them can be stopped: an example draws until it is told to, and there is
no input device on the board to tell it. Each therefore runs in a thread of its
own, below everything that has work to do and above the idle thread, so the
shell goes on being a shell while one draws.

### 2. The path it takes

DirectFB2 already has a path for a system with no operating system underneath
it, which NuttX uses. Tenok is built through the same one:

```
systems/nuttxfb           the display, reached through /dev/fb0
lib/direct/os/nuttx       threads, locks, time and the file system
```

`drivers/display/fb.c` is what answers it, in ninety three lines. NuttX asks a
frame buffer two questions and Tenok answers both:

| Request | What it answers |
| --- | --- |
| `FBIOGET_VIDEOINFO` | The format, the width and the height |
| `FBIOGET_PLANEINFO` | Where the pixels are, how many bytes a row takes, and how many bytes there are |

`mmap()` on the device gives back the address the pixels already live at.
Tenok has no address space to map anything into, so nothing is moved and the
call is the way a program asks where the display is.

### 3. What it costs

```
                              text        bss
DirectFB2 off              226,440  4,301,164
DirectFB2 with seven     1,334,988  4,305,420
```

DirectFB2 is one megabyte of the two the flash holds. Most of it is the
software rasteriser and the OpenGL of `gears`; the four smallest examples come
to 5,720 bytes between them.

The `bss` is the heap of the user space, which is four megabytes of the SDRAM
the board carries. `USER_STACK_SIZE` in `platform/stm32f429.ld` is what says
so, and `__board_memory_init()` is what brings the SDRAM up before anything is
laid out in it.

### 4. How it is built

Eight repositories are involved. Every one of them is pinned and none is
modified in git:

```
lib/directfb2               DirectFB2 itself, 143 sources
lib/directfb2-examples      the examples and the data they draw
lib/directfbgl-portablegl   the OpenGL interface of DirectFB2, as a header
lib/portablegl              the OpenGL that draws with the processor
lib/flux                    fluxcomp, which writes the core interfaces
lib/directfb-csource        turns a data file into C
lib/directfb-tools          mkdgiff, which renders a font
lib/package-patches         the three patches Tenok needs
```

What Tenok changes lives in `lib/package-patches` as a patch series, because a
patch carries the surrounding lines of the file it changes and is therefore
covered by the LGPL of DirectFB2, while Tenok itself is under the BSD 2-Clause
licence. To apply them to a fresh checkout:

```
./scripts/package-prepare.sh directfb2
```

The three are the whole of what DirectFB2 needs, and none of them is a change
Tenok asked for:

| Patch | What it changes |
| --- | --- |
| `0001-reach-the-headers-nuttx-supplies-for-free.patch` | The headers the NuttX layer had from its platform and Tenok does not give away, and the names `u8` through `u64` are given, so that PortableGL and DirectFB2 agree what a `uint32_t` is |
| `0002-include-what-each-header-uses.patch` | Four headers that use `errno`, `INT_MIN`, `DIR` and `struct dirent` without asking for them |
| `0003-include-what-each-source-uses.patch` | Five sources that use `atexit`, `getenv`, `rand`, `errno` and the file calls without asking for them |

Every one of them is a file that uses something it never included. The NuttX
headers pull those in for it; a system whose headers do not leaves it to say so
itself. All three would apply to DirectFB2 upstream without changing anything
for NuttX.

### 5. What the build writes

Three parts of DirectFB2 are not in the repository and are written by the build.

`src/core/Core*.{c,h}` are the core interfaces, written once as interface
descriptions and turned into C by `fluxcomp`:

```
./scripts/directfb-fluxcomp.sh
```

Eight headers hold what the upstream build derives from its own configuration.
`user/directfb/directfb.mk` writes them, and the only value that differs from
what upstream writes is `SIZEOF_LONG`, which is four here and eight there.

The data the examples draw is compiled in, because there is no file system on
the board to read a picture or a font from:

```
decker.ttf  --mkdgiff-->  decker.tenok.dgiff  --csource-->  decker.h
cursor_red.dfiff          ----------------------csource-->  cursor_red.h
cursor_yellow.dfiff       ----------------------csource-->  cursor_yellow.h
dfblogo.dfiff             ----------------------csource-->  dfblogo.h
```

The font is rendered again rather than taken as it is shipped: the `decker.dgiff`
of the examples holds every size and comes to four megabytes, where the whole
flash is two. Both examples that draw text ask for a height of eight on a screen
this wide, and one size of the same font comes to fifteen kilobytes.

### 6. Registered rather than constructed

DirectFB2 registers its modules from constructors, which run before `main()` on
a system that has a loader to run them. Tenok runs nothing before a program
starts, so what a constructor would have registered is registered by
`user/directfb/modules.c` instead, once, however many programs go on to use it:

```c
DirectFBCoreSystemInit(nuttxfb);
DirectFBWindowManagerInit(default);
DirectFBFontProviderInit(DGIFF);
DirectFBImageProviderInit(DFIFF);
DirectFBVideoProviderInit(DFVFF);
DirectFBGLInit(PGL);            /* only when gears is in the build */
```

### 7. Add an example

Turn it on in `make menuconfig`, name it in `user/Kconfig`, and add two lines:

```make
ifdef CONFIG_DIRECTFB2_NEO
$(eval $(call DFB_EXAMPLE,neo,neo,))
endif
```

```c
#ifdef CONFIG_DIRECTFB2_NEO
DFB_EXAMPLE(neo, dfb_neo_main);
#endif
```

The build renames the `main()` of the example so that the shell command can
call it, and `user/directfb/examples_shell.c` is what calls it, in a thread.

An example whose loop waits for input will not come back: there is no input
device to wake it. `df_dok`, `df_porter`, `df_spacedream` and `df_input` are of
that kind. `df_texture` and `df_video` want a video decoder, which Tenok has
none of.
