Interact with Tenok Shell
=========================

### 1. Keys

* **Backspace**, **Delete**: Delete a single character
* **Home**, **Ctrl+A**: Move the cursor to the leftmost
* **End**, **Ctrl+E**: Move the cursor to the rightmost
* **Ctrl+U**: Delete the whole line
* **Left Arrow**, **Ctrl+B**: Move the cursor one to left
* **Right Arrow**, **Ctrl+F**: Move the cursor one to right
* **Up Arrow**, **Down Arrow**: Display previous typings
* **Tab**: Command completion

### 2. Add new shell command

Suppose you want to add a new command called `XXX`. First, create a source file
called  `tenok/user/shell/XXX.c`:

```c
#include "shell.h"

int XXX(int argc, char *argv[])
{
    shell_puts("hello world!\n\r");

    return 0;
}

HOOK_SHELL_CMD("XXX", XXX);
```

The macro is given the name the user types and the function that answers it,
so one file can register more than one command.

Next, append the new source file to the `tenok/user/shell/shell.mk`:

```make
...
SRC += $(PROJ_ROOT)/user/shell/XXX.c
```

Now, recompile the source code:

```
cd tenok/
make
```

Finally, you should able to run your new shell command in the system:

```
USER@stm32f407:/$ XXX
hello world!
```

### 3. The BusyBox shell

BusyBox is built into the firmware alongside the shell of Tenok. `busybox`
runs an applet by name, and `busybox sh` starts `ash`:

```
USER@stm32f407:/$ busybox ls -l /rom_data
USER@stm32f407:/$ busybox sh
/ $ help
Built-in commands:
------------------
	. : [ alias break cd command continue eval exec exit export false
	getopts hash help history let local ...
/ $ exit
```

Tenok has neither `fork()` nor `exec()`, so an applet is reached by calling it
rather than by starting a process. Everything a process exit would have given
back is given back by the run instead: the globals of BusyBox are laid out
again, and the memory and the descriptors it left behind are taken back.

What this leaves out is what needs a second process: `a | b` and `$(cmd)` both
report that a pipe cannot be created. A here-document works, because BusyBox
writes a short one into the pipe without forking.
