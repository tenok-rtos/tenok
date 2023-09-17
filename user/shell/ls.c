#include <string.h>

#include <fs/fs.h>

#include <tenok/stdio.h>
#include <tenok/dirent.h>
#include <tenok/sys/stat.h>

#include "shell.h"

extern struct inode *shell_dir_curr;

void shell_cmd_ls(int argc, char *argv[])
{
    char str[PRINT_SIZE_MAX] = {0};

    /* no file is under this directory */
    if(shell_dir_curr->i_size == 0)
        return;

    /* get current directoory path */
    char path[PATH_LEN_MAX] = {0};
    fs_get_pwd(path, shell_dir_curr);

    if(argc == 2) {
        if(argv[1][0] != '/') {
            /* user feed a relative path */
            int pos = strlen(path);
            snprintf(&path[pos], PRINT_SIZE_MAX, "/%s", argv[1]);
        } else {
            /* user feed a absolute path */
            strncpy(path, argv[1], PATH_LEN_MAX);
        }
    } else if(argc > 2) {
        shell_puts("ls: too many arguments\n\r");
        return;
    }

    /* open the directory */
    DIR dir;
    int retval = opendir(path, &dir);

    /* check if the directory is successfull opened */
    if(retval != 0) {
        snprintf(str, PRINT_SIZE_MAX,
                 "ls: cannot access '%s': No such file or directory\n\r", argv[1]);
        shell_puts(str);
        return;
    }

    /* enumerate the directory */
    int pos = 0;
    struct dirent dirent;
    while((readdir(&dir, &dirent)) != -1) {
        if(dirent.d_type == S_IFDIR) {
            pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s/  ", dirent.d_name);
        } else {
            pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s  ", dirent.d_name);
        }
    }

    snprintf(&str[pos], PRINT_SIZE_MAX, "\n\r");

    shell_puts(str);
}

HOOK_SHELL_CMD(ls);
