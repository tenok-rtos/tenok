#include "kernel.h"
#include "syscall.h"

void path_server(void)
{
	set_program_name("path server");
	setpriority(PRIO_PROCESS, getpid(), 0);

	while(1) {
	}
}
