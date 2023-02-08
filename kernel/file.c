#include <string.h>
#include "util.h"
#include "file.h"
#include "kernel.h"
#include "syscall.h"
#include "fifo.h"

#define PATH_SERVER_FD 0 //reserve file descriptor 0 for path server
#define PATH_LEN_MAX   128

extern struct file *files;

/*----------------------------------------------------------*
 |               file descriptors list layout               |
 *---------*----------------*--------------*----------------*
 |  usage  | path server fd |   task fds   |    file fds    |
 |---------*----------------*--------------*----------------*
 | numbers |       1        | TASK_NUM_MAX | FILE_CNT_LIMIT |
 *---------*----------------*--------------*----------------*/

char path_list[FILE_CNT_LIMIT][PATH_LEN_MAX];
int  path_cnt = 0;

void request_path_register(int reply_fd, char *path)
{
	int file_cmd = PATH_CMD_REGISTER_PATH;
	int path_len = strlen(path) + 1;

	const int overhead = sizeof(file_cmd) + sizeof(reply_fd) + sizeof(path_len);
	char buf[PATH_LEN_MAX + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &file_cmd, sizeof(file_cmd));
	buf_size += sizeof(file_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &path_len, sizeof(path_len));
	buf_size += sizeof(path_len);

	memcpy(&buf[buf_size], path, path_len);
	buf_size += path_len;

	fifo_write(&files[PATH_SERVER_FD], buf, buf_size, 0);
}

void request_file_open(int reply_fd, char *path)
{
	int file_cmd = PATH_CMD_OPEN;
	int path_len = strlen(path) + 1;

	const int overhead = sizeof(file_cmd) + sizeof(reply_fd) + sizeof(path_len);
	char buf[PATH_LEN_MAX + overhead];

	int buf_size = 0;

	memcpy(&buf[buf_size], &file_cmd, sizeof(file_cmd));
	buf_size += sizeof(file_cmd);

	memcpy(&buf[buf_size], &reply_fd, sizeof(reply_fd));
	buf_size += sizeof(reply_fd);

	memcpy(&buf[buf_size], &path_len, sizeof(path_len));
	buf_size += sizeof(path_len);

	memcpy(&buf[buf_size], path, path_len);
	buf_size += path_len;

	fifo_write(&files[PATH_SERVER_FD], buf, buf_size, 0);
}

void path_server(void)
{
	set_program_name("path server");
	setpriority(PRIO_PROCESS, getpid(), 0);

	//mknod("/sys/path_server", 0, S_IFIFO);

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

				new_fd = path_cnt + TASK_NUM_MAX + 1;
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
					open_fd = i + TASK_NUM_MAX + 1;
					break;
				}
			}

			/* path not found */
			if(i >= path_cnt) {
				open_fd = -1;
			}

			write(reply_fd, &open_fd, sizeof(open_fd));

			break;
		}
	}
}
