#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>

char *support_types[] = {
    "bool",
    "char",
    "uint8",
    "int8",
    "uint16",
    "int16",
    "uint32",
    "int32",
    "uint64",
    "int64",
    "float",
    "double",
};

int split_tokens(char token[2][1000], char *line, int size)
{
    int i = 0, j = 0;
    int token_cnt = 0;

    /* itterate through the whole line */
    for(i = 0; i < size; i++) {
        /* ignore spaces before first token */
        if(line[i] == ' ') {
            /* end of the current token */
            if(j != 0) {
                token_cnt++;
                token[token_cnt][j] = '\0';

                if(token_cnt > 2)
                    return -1; //only two tokens are allowed for declaration semantics in the msg file
            }

            j = 0; //reset token string index

            continue;
        }

        /* copy the content for current token */
        token[token_cnt][j] = line[i];
        j++;
    }

    return 0;
}

int msg_name_rule_check(char *msg_name)
{
    int len = strlen(msg_name);

    int i;
    for(i = 0; i < len; i++) {
        char c = msg_name[i];

        bool legal = (c >= '0' && c <= '9') ||
                     (c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z') ||
                     (c == '_');

        /* message name should not starts with a number */
        if(i == 0 && (c >= '0' && c <= '9'))
            legal = false;

        /* bad message name */
        if(legal == false)
            return -1;
    }

    return 0;
}

char *get_message_name(char *file_name)
{
    int len = strlen(file_name);
    char *msg_name = malloc(sizeof(char) * len);

    /* copy and omit the ".msg" part of the file name */
    strncpy(msg_name, file_name, len - 4);
    msg_name[len - 4] = '\0';

    if(msg_name_rule_check(msg_name) == 0) {
        printf("message name: %s\n", msg_name);
    } else {
        printf("abort, bad message name.\n");
    }

    return msg_name;
}

int codegen(char *file_name, char *msgs, FILE *output_src, FILE *output_header)
{
    char *msg_name = get_message_name(file_name);
    if(msg_name == NULL)
        return -1;

    /* parse the message declaration*/
    char *line_start = msgs;
    char *line_end = msgs;

    while(1) {
        /* search the end of the current line */
        char *line_end = strchr(line_start, '\n');

        /* skip empty lines */
        if((line_end - line_start) == 0) {
            line_start++;
            continue;
        }

        /* EOF detection */
        if(line_end == NULL) {
            break; //leave from the loop
        }

        *line_end = '\0';

        /* split tokens of current line */
        char tokens[2][1000] = {0};
        split_tokens(tokens, line_start, line_end - line_start);
        printf("type:%s, name:%s\n\r", tokens[0], tokens[1]);

        line_start = line_end + 1;
    }

    /* header file */
    fprintf(output_header,
            "#ifndef __DEBUG_MSG_H__\n#define __DEBUG_MSG_H\n\n"
            "#include \"debug_link.h\"\n\n");
    fprintf(output_header, "void pack_%s_tenok_debug_msg(debug_msg_t *payload);\n", msg_name);
    fprintf(output_header, "\n#endif");

    /* source file */
    fprintf(output_src, "#include \"debug_msg.h\"\n\n");
    fprintf(output_src,
            "void pack_%s_tenok_debug_msg(debug_msg_t *payload)\n{\n"
            "    pack_debug_debug_message_header(payload, MSG_ID_%s);\n"
            "}", msg_name, msg_name);

    free(msg_name);

    return 0;
}

char *load_msg_file(char *file_name)
{
    /* open msg file */
    FILE *msg_file = fopen(file_name, "r");
    if(msg_file == NULL) {
        printf("msggen: error: no such file\n");
        return NULL;
    }

    /* get file size */
    fseek(msg_file, 0, SEEK_END);  //move to the end of the file
    long size = ftell(msg_file);   //get size
    fseek(msg_file, 0L, SEEK_SET); //move the start of the file

    /* read msg file */
    char *file_content = (char *)malloc(sizeof(char) * size);

    if(file_content != NULL)
        fread(file_content, sizeof(char), size, msg_file);

    /* close file */
    fclose(msg_file);

    return file_content;
}

int main(int argc, char **argv)
{
    /* incorrect argument counts */
    if(argc != 2) {
        printf("msggen [directory path]\n");
        return -1;
    }

    /* open directory */
    DIR *dir = opendir(argv[1]);
    if(dir == NULL) {
        printf("msggen: failed to open the given directory.\n");
        return -1;
    }

    FILE *output_src = fopen("./debug_msg.c", "wb");
    FILE *output_header = fopen("./debug_msg.h", "wb");

    /* enumerate all the files under the given directory path */
    struct dirent* dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        if(dirent->d_type != DT_REG)
            continue;

        char *msg_file_name = dirent->d_name;
        int len = strlen(msg_file_name);

        /* find all msg files, i.e., file names end with ".msg" */
        if((strncmp(".msg", msg_file_name + len - 4, 4) == 0)) {
            /* load the msg file and generate body-part code */
            char *msgs = load_msg_file(msg_file_name);
            codegen(msg_file_name, msgs, output_src, output_header);

            /* clean up */
            free(msgs);
        }
    }

    /* close directory */
    closedir(dir);

    fclose(output_src);
    fclose(output_header);

    return 0;
}
