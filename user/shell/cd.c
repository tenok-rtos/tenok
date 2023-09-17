#include <string.h>

#include <fs/fs.h>

#include <tenok/stdio.h>
#include <tenok/dirent.h>
#include <tenok/sys/stat.h>

#include "shell.h"

extern struct inode *shell_dir_curr;

void shell_cmd_cd(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    DIR dir;

    switch(argc) {
        case 1:
            opendir("/", &dir);
            shell_dir_curr = dir.inode_dir;
            return;
        case 2: {
            char path[PATH_LEN_MAX] = {0};

            if(strncmp("..", argv[1], SHELL_CMD_LEN_MAX) == 0) {
                /* handle cd .. */
                fs_get_pwd(path, shell_dir_curr);
                int pos = strlen(path);
                snprintf(&path[pos], PATH_LEN_MAX, "..");
            } else {
                /* handle regular path */
                if(argv[1][0] == '/') {
                    /* handle the absolute path */
                    strncpy(path, argv[1], PATH_LEN_MAX);
                } else {
                    /* handle the relative path */
                    fs_get_pwd(path, shell_dir_curr);
                    int pos = strlen(path);
                    snprintf(&path[pos], PATH_LEN_MAX, "%s", argv[1]);
                }
            }

            /* open the directory */
            int retval = opendir(path, &dir);

            /* directory not found */
            if(retval != 0) {
                snprintf(str, PRINT_SIZE_MAX, "cd: %s: No such file or directory\n\r", argv[1]);
                shell_puts(str);
                return;
            }

            /* not a directory */
            if(dir.inode_dir->i_mode != S_IFDIR) {
                snprintf(str, PRINT_SIZE_MAX, "cd: %s: Not a directory\n\r", argv[1]);
                shell_puts(str);
                return;
            }

            shell_dir_curr = dir.inode_dir;
            return;
        }
        default:
            shell_puts("cd: too many arguments\n\r");
            return;
    }
}

HOOK_SHELL_CMD(cd);
