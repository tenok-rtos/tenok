#include <stdio.h>
#include <string.h>
#include "shell.h"
#include "kernel.h"
#include "fs.h"
#include "syscall.h"
#include "reg_file.h"
#include "file.h"

extern struct shell shell;
extern struct memory_pool mem_pool;

struct inode *shell_dir_curr = NULL;

void shell_path_init(void)
{
    DIR dir;
    opendir("/", &dir);
    shell_dir_curr = dir.inode_dir;
}

void shell_cmd_help(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char *s = "supported commands:\n\r"
              "help, clear, history, ps, echo, ls, cd, pwd, cat, file, mpool\n\r";
    shell_puts(s);
}

void shell_cmd_clear(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    shell_cls();
}

void shell_cmd_history(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    shell_print_history(&shell);
}

void shell_cmd_ps(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX];
    sprint_tasks(str, PRINT_SIZE_MAX);
    shell_puts(str);
}

void shell_cmd_echo(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX] = {0};
    int pos = 0;

    int i;
    for(i = 1; i < argc - 1; i++) {
        pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s ", argv[i]);
    }

    pos += snprintf(&str[pos], PRINT_SIZE_MAX, "%s\n\r", argv[argc-1]);

    shell_puts(str);
}

void shell_cmd_ls(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
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

void shell_cmd_cd(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
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

            if(strncmp("..", argv[1], SHELL_ARG_LEN) == 0) {
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
            shell_puts("cd: too many  arguments\n\r");
            return;
    }
}

void shell_cmd_pwd(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX] = {0};
    char path[PATH_LEN_MAX] = {'/'};

    fs_get_pwd(path, shell_dir_curr);

    snprintf(str, PATH_LEN_MAX, "%s\n\r", path);
    shell_puts(str);
}

/* print a string by the same time replace \n with \n\r */
static void shell_print_lf_cr(char *str, int size)
{
    int i;
    for(i = 0; i < size; i++) {
        /* end of the string */
        if(str[i] == '\0')
            return;

        if(str[i] == '\n') {
            shell_puts("\n\r");
        } else {
            char s[2] = {str[i], '\0'};
            shell_puts(s);
        }
    }
}

void shell_cmd_cat(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char path[PATH_LEN_MAX] = {0};

    if(argv[1][0] != '/') {
        /* input is a relative path */
        char pwd[PATH_LEN_MAX] = {0};
        fs_get_pwd(pwd, shell_dir_curr);

        snprintf(path, PATH_LEN_MAX, "%s%s", pwd, argv[1]);
    } else {
        /* input is a absolute path */
        strncpy(path, argv[1], PATH_LEN_MAX);
    }

    char str[PRINT_SIZE_MAX] = {0};

    /* open the file */
    FILE file;
    if(fopen(path, 0, &file)) {
        snprintf(str, PRINT_SIZE_MAX, "cat: cannot open `%s'\n\r", path);
        shell_puts(str);
        return;
    }

    struct stat stat;
    fstat(fileno(&file), &stat);

    /* reset the read position of the file */
    fseek(&file, 0, SEEK_SET);

    /* calculate the iteration times to print the whole file */
    int size = stat.st_size / PRINT_SIZE_MAX;
    if((stat.st_size % PRINT_SIZE_MAX) > 0)
        size++;

    /* read and print the file */
    int i;
    for(i = 0; i < size; i++) {
        int recvd = fread(str, PRINT_SIZE_MAX - 1, 1, &file);
        str[recvd] = '\0';

        shell_print_lf_cr(str, recvd);
    }
}

void shell_cmd_file(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX] = {0};

    /* no file is under this directory */
    if(shell_dir_curr->i_size == 0)
        return;

    /* get current directoory path */
    char path[PATH_LEN_MAX] = {0};
    fs_get_pwd(path, shell_dir_curr);

    if(argc == 1) {
        shell_puts("Usage: file <file>\n\r");
        return;
    } else if(argc > 2) {
        shell_puts("file: too many arguments\n\r");
        return;
    }

    if(argv[1][0] != '/') {
        /* user feed a relative path */
        int pos = strlen(path);
        snprintf(&path[pos], PRINT_SIZE_MAX, "%s", argv[1]);
    } else {
        /* user feed a absolute path */
        strncpy(path, argv[1], PATH_LEN_MAX);
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

void shell_cmd_mpool(int argc, char argv[SHELL_ARG_CNT][SHELL_ARG_LEN])
{
    char str[PRINT_SIZE_MAX] = {0};

    snprintf(str, PRINT_SIZE_MAX, "[kernel memory pool] size: %d bytes, used: %d bytes\n\r",
             mem_pool.size, mem_pool.offset);
    shell_puts(str);
}
