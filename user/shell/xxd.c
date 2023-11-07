#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "shell.h"

#define XXD_N_BYTES     16 //byte numbers for xxd to display per one line
#define XXD_PAUSE_LINES 10 //the number of lines that xxd can print at once

extern struct inode *shell_dir_curr;

static void byte2hex(char byte, char *hex_str)
{
    hex_str[2] = '\0';
    snprintf(hex_str, 4, "%02x", byte);
}

static void byte2char(char byte, char *char_str)
{
    char_str[1] = '\0';

    if(byte >= 32 && byte <= 126) {
        snprintf(char_str, 2, "%c", byte);
    } else {
        strncpy(char_str, ".", 2);
    }
}

static void xxd_print_line(unsigned int addr, char *buf, int nbytes)
{
    /* append address */
    char s[200] = {0};
    sprintf(s, "%08x: ", addr);

    /* hex code */
    for(int i = 0; i < XXD_N_BYTES; i++) {
        if(i < nbytes) {
            /* append hex code of the byte */
            char hex_str[3] = {'\0'};
            byte2hex(buf[i], hex_str);
            strncat(s, hex_str, 3);
        } else {
            strncat(s, "  ", 3);
        }

        if(i % 2) {
            /* add a new space after every two bytes */
            strncat(s, " ", 2);
        }
    }

    /* append delimiter */
    strncat(s, "  ", 3);

    /* plain text */
    for(int i = 0; i < nbytes; i++) {
        /* append plain text of the byte */
        char char_str[2] = {'\0'};
        byte2char(buf[i], char_str);
        strncat(s, char_str, 2);
    }

    /* append new line */
    strncat(s, "\n\r", 3);

    /* print the xxd line out */
    shell_puts(s);
}

static char xxd_wait_key(void)
{
    char c;
    while(1) {
        c = shell_getc();
        if(c == 'q' || c == ENTER) {
            return c;
        }
    }
}

void shell_cmd_xxd(int argc, char *argv[])
{
    if(argc != 2) {
        shell_puts("usage: xxd file\n\r");
        return;
    }

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

    /* open file */
    FILE *file = fopen(path, "");
    if(!file) {
        shell_puts("failed to open the file.\n\r");
        return;
    }

    /* get file size */
    struct stat stat;
    fstat(fileno(file), &stat);
    fseek(file, 0, SEEK_SET);
    int fsize = stat.st_size;

    /* read file */
    unsigned int addr = 0;
    char buf[100];

    int line = 0;

    while(fsize > 0) {
        int rsize;
        if(fsize >= XXD_N_BYTES) {
            /* full line */
            rsize = XXD_N_BYTES;
            fsize -= XXD_N_BYTES;
        } else {
            /* remainers */
            rsize = fsize;
            fsize = 0;
        }

        /* read file */
        fread(buf, rsize, 1, file);

        /* print one line of xxd message */
        xxd_print_line(addr, buf, rsize);

        /* address of next line to read */
        addr += XXD_N_BYTES;

        if(fsize == 0) {
            return;
        }

        /* pause the printing and wait for the key */
        line++;
        if(line == XXD_PAUSE_LINES) {
            shell_puts("press [enter] to show more, [q] to leave.\n\r");

            char key = xxd_wait_key();
            if(key == ENTER) {
                line = 0;
            } else if(key == 'q') {
                return;
            }
        }
    }

    fclose(file);
}

HOOK_SHELL_CMD(xxd);
