#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "kernel.h"
#include "fs.h"
#include "syscall.h"

extern struct shell shell;
extern struct memory_pool mem_pool;

struct inode *shell_dir_curr = NULL;

void shell_path_init(void)
{
    DIR dir;
    opendir("/", &dir);
    shell_dir_curr = dir.inode_dir;
}

void shell_cmd_help(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char *s = "supported commands:\n\r"
              "help, clear, history, ps, echo, ls, cd, pwd, cat, file, mpool\n\r";
    shell_puts(s);
}

void shell_cmd_clear(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    shell_cls();
}

void shell_cmd_history(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    shell_print_history(&shell);
}

void shell_cmd_ps(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX];
    sprint_tasks(str, PRINT_SIZE_MAX);
    shell_puts(str);
}

void shell_cmd_echo(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX] = {0};

    int i;
    for(i = 1; i < param_cnt - 1; i++) {
        snprintf(str, PRINT_SIZE_MAX, "%s%s ", str, param_list[i]);
    }
    snprintf(str, PRINT_SIZE_MAX, "%s%s\n\r", str, param_list[param_cnt-1]);

    shell_puts(str);
}

void shell_cmd_ls(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX] = {0};

    /* no file is under this directory */
    if(shell_dir_curr->i_size == 0)
        return;

    /* get current directoory path */
    char path[PATH_LEN_MAX] = {0};
    fs_get_pwd(path, shell_dir_curr);

    if(param_cnt == 2) {
        if(param_list[1][0] != '/') {
            /* user feed a relative path */
            sprintf(path, "%s/%s", path, param_list[1]);
        } else {
            /* user feed a absolute path */
            strncpy(path, param_list[1], PATH_LEN_MAX);
        }
    } else if(param_cnt > 2) {
        shell_puts("ls: too many arguments\n\r");
        return;
    }

    /* open the directory */
    DIR dir;
    int retval = opendir(path, &dir);

    /* check if the directory is successfull opened */
    if(retval != 0) {
        snprintf(str, PRINT_SIZE_MAX,
                 "ls: cannot access '%s': No such file or directory\n\r", param_list[1]);
        shell_puts(str);
        return;
    }

    /* enumerate the directory */
    struct dirent dirent;
    while((readdir(&dir, &dirent)) != -1) {
        if(dirent.d_type == S_IFDIR) {
            snprintf(str, PRINT_SIZE_MAX, "%s%s/  ", str, dirent.d_name);
        } else {
            snprintf(str, PRINT_SIZE_MAX, "%s%s  ", str, dirent.d_name);
        }
    }

    snprintf(str, PRINT_SIZE_MAX, "%s\n\r", str);

    shell_puts(str);
}

void shell_cmd_cd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX] = {0};

    DIR dir;

    if(param_cnt == 1) {
        opendir("/", &dir);
        shell_dir_curr = dir.inode_dir;
    } else if(param_cnt == 2) {
        char path[PATH_LEN_MAX] = {0};

        if(strncmp("..", param_list[1], PARAM_LEN_MAX) == 0) {
            /* handle cd .. */
            fs_get_pwd(path, shell_dir_curr);
            snprintf(path, PATH_LEN_MAX, "%s..");
        } else {
            /* handle regular path */
            if(param_list[1][0] == '/') {
                /* handle the absolute path */
                strncpy(path, param_list[1], PATH_LEN_MAX);
            } else {
                /* handle the relative path */
                fs_get_pwd(path, shell_dir_curr);
                snprintf(path, PATH_LEN_MAX, "%s%s", path, param_list[1]);
            }
        }

        /* open the directory */
        int retval = opendir(path, &dir);

        /* directory not found */
        if(retval != 0) {
            snprintf(str, PRINT_SIZE_MAX, "cd: %s: No such file or directory\n\r", param_list[1]);
            shell_puts(str);
            return;
        }

        /* not a directory */
        if(dir.inode_dir->i_mode != S_IFDIR) {
            snprintf(str, PRINT_SIZE_MAX, "cd: %s: Not a directory\n\r", param_list[1]);
            shell_puts(str);
            return;
        }

        shell_dir_curr = dir.inode_dir;
    } else {
        shell_puts("cd: too many  arguments\n\r");
        return;
    }
}

void shell_cmd_pwd(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX] = {0};
    char path[PATH_LEN_MAX] = {'/'};

    fs_get_pwd(path, shell_dir_curr);

    snprintf(str, PATH_LEN_MAX, "%s\n\r", path);
    shell_puts(str);
}

void shell_cmd_cat(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char path[PATH_LEN_MAX] = {0};

    if(param_list[1][0] != '/') {
        /* input is a relative path */
        char pwd[PATH_LEN_MAX] = {0};
        fs_get_pwd(pwd, shell_dir_curr);

        snprintf(path, PATH_LEN_MAX, "%s%s", pwd, param_list[1]);
    } else {
        /* input is a absolute path */
        strncpy(path, param_list[1], PATH_LEN_MAX);
    }

    char str[PRINT_SIZE_MAX] = {0};

    /* open the file */
    int fd = open(path, 0, 0);
    if(fd == -1) {
        snprintf(str, PRINT_SIZE_MAX, "cat: %s: No such file or directory\n\r", path);
        shell_puts(str);
        return;
    }

    /* check if the file mode is set as regular file */
    struct stat stat;
    fstat(fd, &stat);
    if(stat.st_mode != S_IFREG) {
        snprintf(str, PRINT_SIZE_MAX, "cat: %s: Invalid argument\n\r", path);
        shell_puts(str);
        return;
    }

    /* reset the start position of the file */
    lseek(fd, 0, SEEK_SET);

    /* read file content until EOF is detected */
    signed char c;

    int i;
    for(i = 0; i < stat.st_size; i++) {
        if(i >= (PRINT_SIZE_MAX))
            break;

        read(fd, &c, 1);

        if(c == EOF) {
            str[i] = '\0';
            break;
        } else {
            str[i] = c;
        }
    }

    shell_puts(str);
}

void shell_cmd_file(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX] = {0};

    /* no file is under this directory */
    if(shell_dir_curr->i_size == 0)
        return;

    /* get current directoory path */
    char path[PATH_LEN_MAX] = {0};
    fs_get_pwd(path, shell_dir_curr);

    if(param_cnt == 1) {
        shell_puts("Usage: file <file>\n\r");
        return;
    } else if(param_cnt > 2) {
        shell_puts("file: too many arguments\n\r");
        return;
    }

    if(param_list[1][0] != '/') {
        /* user feed a relative path */
        sprintf(path, "%s%s", path, param_list[1]);
    } else {
        /* user feed a absolute path */
        strncpy(path, param_list[1], PATH_LEN_MAX);
    }

    /* open the file */
    int fd = open(path, 0, 0);
    if(fd == -1) {
        /* maybe it is a direcotry */
        DIR dir;
        if(opendir(path, &dir) == 0) {
            snprintf(str, PRINT_SIZE_MAX, "%s: directory\n\r", path);
            shell_puts(str);
            return;
        }

        /* the given path is neither a file or direcotry */
        snprintf(str, PRINT_SIZE_MAX,
                 "%s: cannot open `%s' (No such file or directory)\n\r", path, path);
        shell_puts(str);
        return;
    }

    /* read and print the file type */
    struct stat stat;
    fstat(fd, &stat);

    switch(stat.st_mode) {
        case S_IFIFO:
            snprintf(str, PRINT_SIZE_MAX, "%s: fifo (named pipe)\n\r", path);
            break;
        case S_IFCHR:
            snprintf(str, PRINT_SIZE_MAX, "%s: character device\n\r", path);
            break;
        case S_IFBLK:
            snprintf(str, PRINT_SIZE_MAX, "%s: block device\n\r", path);
            break;
        case S_IFREG:
            snprintf(str, PRINT_SIZE_MAX, "%s: regular file\n\r", path);
            break;
        default:
            snprintf(str, PRINT_SIZE_MAX, "%s: unknown\n\r", path);
            break;
    }

    shell_puts(str);
    return;
}

void shell_cmd_mpool(char param_list[PARAM_LIST_SIZE_MAX][PARAM_LEN_MAX], int param_cnt)
{
    char str[PRINT_SIZE_MAX] = {0};

    snprintf(str, PRINT_SIZE_MAX, "[kernel memory pool] size: %d bytes, used: %d bytes\n\r",
             mem_pool.size, mem_pool.offset);
    shell_puts(str);
}
