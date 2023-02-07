#include <string.h>
#include "util.h"
#include "file.h"
#include "kernel.h"
#include "syscall.h"

#define PATH_SERVER_FD 0 //reserve file descriptor 0 for path server
#define PATH_LEN_MAX   128

char path_list[FILE_CNT_LIMIT][PATH_LEN_MAX];
int  path_cnt = 0;

void path_server(void)
{
	set_program_name("path server");
	setpriority(PRIO_PROCESS, getpid(), 0);

	mknod("/sys/path_server", 0, S_IFIFO);

	int  path_len;
	char path[PATH_LEN_MAX];

	int  dev;

	while(1) {
		int file_cmd;
		read(PATH_SERVER_FD, &file_cmd, sizeof(file_cmd));

		int reply_fd;
		read(PATH_SERVER_FD, &reply_fd, sizeof(reply_fd));

		switch(file_cmd) {
		case PATH_CMD_REGISTER_PATH:
			read(PATH_SERVER_FD, &path_len, sizeof(path_len));
			read(PATH_SERVER_FD, path, path_len);

			int new_fd;

			if(path_cnt < FILE_CNT_LIMIT) {
				memcpy(path_list[path_cnt], path, path_len);

				new_fd = path_cnt + 1;
				path_cnt++;
			} else {
				new_fd = -1;
			}

			write(reply_fd, &new_fd, sizeof(new_fd));

			break;
		case PATH_CMD_OPEN:
			read(PATH_SERVER_FD, &path_len, sizeof(path_len));
			read(PATH_SERVER_FD, path, path_len);

			int open_fd;

			/* invalid empty path */
			if(path[0] == '\0') {
				open_fd = -1;
			}

			/* path finding */
			int i;
			for(i = 0; i < path_cnt; i++) {
				if(strcmp(path_list[i], path) == 0) {
					open_fd = i + 1;
				}
			}

			write(reply_fd, &open_fd, sizeof(open_fd));

			break;
		}
	}
}
