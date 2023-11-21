#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shell.h"

#define XXD_N_BYTES 16     /* The Byte number for xxd to print per line */
#define XXD_PAUSE_LINES 10 /* The line number that xxd can print at once */

static void byte2hex(char byte, char *hex_str)
{
    hex_str[2] = '\0';
    snprintf(hex_str, 4, "%02x", byte);
}

static void byte2char(char byte, char *char_str)
{
    char_str[1] = '\0';

    if (byte >= 32 && byte <= 126) {
        snprintf(char_str, 2, "%c", byte);
    } else {
        strncpy(char_str, ".", 2);
    }
}

static void xxd_print_line(unsigned int addr, char *buf, int nbytes)
{
    /* Append address */
    char s[PRINT_SIZE_MAX] = {0};
    snprintf(s, PRINT_SIZE_MAX, "%08x: ", addr);

    /* Hex code */
    for (int i = 0; i < XXD_N_BYTES; i++) {
        if (i < nbytes) {
            /* Append hex code of the byte */
            char hex_str[3] = {'\0'};
            byte2hex(buf[i], hex_str);
            strncat(s, hex_str, 3);
        } else {
            strncat(s, "  ", 3);
        }

        if (i % 2) {
            /* Add a new space after every two bytes */
            strncat(s, " ", 2);
        }
    }

    /* Append delimiter */
    strncat(s, "  ", 3);

    /* Plain text */
    for (int i = 0; i < nbytes; i++) {
        /* Append plain text of the byte */
        char char_str[2] = {'\0'};
        byte2char(buf[i], char_str);
        strncat(s, char_str, 2);
    }

    /* Append new line */
    strncat(s, "\n\r", 3);

    /* Print the xxd line out */
    shell_puts(s);
}

static char xxd_wait_key(void)
{
    char c;
    while (1) {
        c = shell_getc();
        if (c == 'q' || c == ENTER) {
            return c;
        }
    }
}

void shell_cmd_xxd(int argc, char *argv[])
{
    if (argc != 2) {
        shell_puts("usage: xxd file\n\r");
        return;
    }

    char path[PATH_LEN_MAX] = {0};

    if (argv[1][0] != '/') {
        /* Input is a relative path */
        char pwd[PATH_LEN_MAX] = {0};
        getcwd(pwd, PATH_LEN_MAX);

        snprintf(path, PATH_LEN_MAX, "%s%s", pwd, argv[1]);
    } else {
        /* Input is a absolute path */
        strncpy(path, argv[1], PATH_LEN_MAX);
    }

    /* Open file */
    FILE *file = fopen(path, "");
    if (!file) {
        shell_puts("failed to open the file.\n\r");
        return;
    }

    /* Get file size */
    struct stat stat;
    fstat(fileno(file), &stat);
    fseek(file, 0, SEEK_SET);
    int fsize = stat.st_size;

    /* Read file */
    unsigned int addr = 0;
    char buf[100];

    int line = 0;

    while (fsize > 0) {
        int rsize;
        if (fsize >= XXD_N_BYTES) {
            /* Full line */
            rsize = XXD_N_BYTES;
            fsize -= XXD_N_BYTES;
        } else {
            /* Remainers */
            rsize = fsize;
            fsize = 0;
        }

        /* Read file */
        fread(buf, rsize, 1, file);

        /* Print one line of xxd message */
        xxd_print_line(addr, buf, rsize);

        /* Address of the next line to read */
        addr += XXD_N_BYTES;

        if (fsize == 0) {
            return;
        }

        /* Pause the printing and wait for the key */
        line++;
        if (line == XXD_PAUSE_LINES) {
            shell_puts("press [enter] to show more, [q] to leave.\n\r");

            char key = xxd_wait_key();
            if (key == ENTER) {
                line = 0;
            } else if (key == 'q') {
                return;
            }
        }
    }

    fclose(file);
}

HOOK_SHELL_CMD(xxd);
