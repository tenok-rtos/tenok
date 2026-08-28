BusyBox on Tenok
================

`BusyBox` is built into the firmware. `busybox` runs an applet by name and
`busybox sh` starts `ash`:

```
USER@stm32f407:/$ busybox ls -l /rom_data
USER@stm32f407:/$ busybox sh
/ $ echo hello
hello
/ $ exit
```

### 1. What is there

The shell is `ash`, with tab completion, a history of fifteen lines, `$((...))`
arithmetic, aliases and `getopts`. Beside it are thirty four applets:

```
basename  cat     chmod   clear   cmp     cp      cut     date
dirname   echo    env     false   hd      head    hexdump ls
md5sum    mkdir   mv      od      printf  pwd     rm      sha256sum
sleep     stat    tee     test    touch   tr      true    uname
wc        which
```

`configs/busybox.config` is what turns them on. Only what Tenok turns on is
written there; a name that does not appear is off. The build turns everything
off first and then puts the file in place of it, so the ninety lines say what
the firmware holds instead of listing the thousand things it does not.

### 2. Called rather than started

A shell elsewhere runs `ls` by forking a process and letting it `exec` the file
`/bin/ls`. Tenok has neither `fork()` nor `exec()`, and there is no such file:
`ls` is a C function linked into the firmware. The shell therefore calls it and
it returns, on the same stack of the same task.

BusyBox is written for the other arrangement. It expects the globals of an
applet to start out as the linker laid them out, and it leaves memory and
descriptors behind because a process exit would have given them back. What that
exit would have done is done instead by `user/busybox/`:

| File | What it takes back |
| --- | --- |
| `applet.c` | Lays the globals of BusyBox out again before the call, and unwinds an `exit()` back to the caller rather than ending the task |
| `memory.c` | Every block the applet allocated and did not free |
| `descriptor.c` | Every descriptor and stream it left open |

### 3. What a second process would be needed for

A pipeline and a command substitution both want two things running at once:

```
/ $ echo a | wc -c
sh: can't create pipe: Function not implemented
```

A here-document works, because BusyBox writes a short one into the pipe without
forking:

```
/ $ cat <<EOF
> hello
> EOF
hello
```

`Ctrl+C` at the prompt is handled by the line editor. Inside an applet it
unwinds the applet, which is what a shell reports as a signal elsewhere.

### 4. How it is built

The BusyBox submodule stays exactly as its release left it. Everything Tenok
changes lives in a separate repository as a patch series, because a patch
carries the surrounding lines of the file it changes and is therefore covered
by the GPL, while Tenok itself is under the BSD 2-Clause licence.

```
lib/busybox            BusyBox 1.38.0, pinned and never modified in git
lib/package-patches    the two patches Tenok needs
configs/busybox.config what is turned on
user/busybox/          what a process exit would have done
include/busybox/       the headers BusyBox is compiled against
```

To apply the patches to a fresh checkout:

```
./scripts/package-prepare.sh busybox
```

Running it again is safe and always produces the same tree. To turn what you
changed in `lib/busybox` into a patch of the series:

```
./scripts/package-refresh-patch.sh busybox 0003-name-of-the-change
```

The two patches are the whole of what BusyBox needs:

| Patch | What it changes |
| --- | --- |
| `0001-run-the-applets-without-fork.patch` | Every applet is NOFORK, and `run_nofork_applet()` is told when one starts and when it returns |
| `0002-declare-what-tenok-provides.patch` | Tenok does not understand `%m`, and has neither `/dev/fd`, `wait3()` nor `clearenv()` |

### 5. Add an applet

Turn it on in `configs/busybox.config`, and add its source file to `BB_APPLETS`
in `user/busybox/busybox.mk`. The applet table is derived from the `//applet:`
comments of the BusyBox sources, so nothing else has to be told about it.

```make
BB_APPLETS := coreutils/basename coreutils/cat ... coreutils/yes
```

```
CONFIG_YES=y
```

An applet that asks for something Tenok does not have will say so at the link,
naming the function it wanted.
