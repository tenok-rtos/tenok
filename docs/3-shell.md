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
called  `tenok/shell/XXX.c`:

```c
#include "shell.h"

void shell_XXX_help(int argc, char *argv[])
{
    shell_puts("hello world!\n\r");
}

HOOK_SHELL_CMD(XXX);
```

Next, append the new source file to the `tenok/shell/shell.mk`:

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
