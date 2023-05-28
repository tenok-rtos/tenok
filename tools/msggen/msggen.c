#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include "list.h"

char *support_types[] = {
    "bool",
    "char",
    "uint8_t",
    "int8_t",
    "uint16_t",
    "int16_t",
    "uint32_t",
    "int32_t",
    "uint64_t",
    "int64_t",
    "float",
    "double",
};

struct msg_var_entry {
    char *var_name;
    char *c_type;

    struct list list;
};

int msg_cnt = 0;

int type_check(char *type)
{
    int len = strlen(type);
    int type_list_size = sizeof(support_types) / sizeof(char *);

    int i;
    for(i = 0; i < type_list_size; i++) {
        if(strncmp(type, support_types[i], len) == 0)
            return 0;
    }

    return -1;
}

int split_tokens(char *token[2], char *line, int size)
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

    if(msg_name_rule_check(msg_name)) {
        printf("abort, bad message name.\n");
    }

    return msg_name;
}

int codegen(char *file_name, char *msgs, char *output_dir)
{
    char *msg_name = get_message_name(file_name);
    if(msg_name == NULL)
        return -1;

    char *output_name = calloc(sizeof(char), strlen(msg_name) + strlen(output_dir) + 50);
    sprintf(output_name, "%s/tenok_%s_msg.h", output_dir, msg_name);

    FILE *output_file = fopen(output_name, "wb");
    if(output_file == NULL) {
        printf("msggen: error, failed to write to %s\n", output_name);
        free(output_name);
        return -1;
    }

    /* parse the message declaration*/
    char *line_start = msgs;
    char *line_end = msgs;

    int var_cnt = 0;

    struct list msg_var_list;
    list_init(&msg_var_list);

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
        char *tokens[2];
        tokens[0] = calloc(sizeof(char), line_end - line_start + 1);
        tokens[1] = calloc(sizeof(char), line_end - line_start + 1);
        split_tokens(tokens, line_start, line_end - line_start);
        //printf("type:%s, name:%s\n\r", tokens[0], tokens[1]);

        int type_len = strlen(tokens[0]) + 1;
        int var_name_len = strlen(tokens[1]) + 1;

        if(type_check(tokens[0]) != 0) {
            printf("msggen: error, unknown type %s from %s\n", tokens[0], file_name);

            /* clean up */
            fclose(output_file);

            while(!list_is_empty(&msg_var_list)) {
                struct list *curr = list_pop(&msg_var_list);
                struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);

                free(msg_var->c_type);
                free(msg_var->var_name);
                free(msg_var);
            }

            free(tokens[0]);
            free(tokens[1]);
            free(msg_name);
            free(output_name);

            return -1;
        }

        /* append new variable declaration to the list */
        struct msg_var_entry *new_var = malloc(sizeof(struct msg_var_entry));
        new_var->var_name = calloc(sizeof(char), type_len);
        new_var->c_type = calloc(sizeof(char), var_name_len);
        strncpy(new_var->c_type, tokens[0], type_len);
        strncpy(new_var->var_name, tokens[1], var_name_len);

        list_push(&msg_var_list, &new_var->list);

        var_cnt++;

        line_start = line_end + 1;

        /* clean up */
        free(tokens[0]);
        free(tokens[1]);
    }

    /* generate preprocessing code */
    fprintf(output_file,
            "#pragma once\n\n"
            "#define TENOK_MSG_ID_%s %d\n\n", msg_name, msg_cnt);

    /* generarte message structure */
    fprintf(output_file, "typedef struct __tenok_msg_%s_t {\n", msg_name);

    struct list *curr;
    list_for_each(curr, &msg_var_list) {
        struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);
        fprintf(output_file, "    %s %s;\n", msg_var->c_type, msg_var->var_name);
    }

    fprintf(output_file, "} tenok_msg_%s_t;\n\n", msg_name);

    /* generation message function */
    fprintf(output_file,
            "inline void pack_%s_tenok_msg(tenok_msg_%s_t *msg, debug_msg_t *payload)\n{\n"
            "    pack_tenok_msg_header(payload, MSG_ID_%s);\n",
            msg_name, msg_name, msg_name);

    list_for_each(curr, &msg_var_list) {
        struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);
        fprintf(output_file, "    pack_tenok_msg_field_%s(&msg->%s, payload);\n",
                msg_var->c_type, msg_var->var_name);
    }

    fprintf(output_file, "}\n");

    printf("[msggen] generate %s\n", output_name);

    /* clean up */
    fclose(output_file);

    while(!list_is_empty(&msg_var_list)) {
        struct list *curr = list_pop(&msg_var_list);
        struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);

        free(msg_var->c_type);
        free(msg_var->var_name);
        free(msg_var);
    }

    free(msg_name);
    free(output_name);

    return 0;
}

char *load_msg_file(char *file_name)
{
    /* open msg file */
    FILE *msg_file = fopen(file_name, "r");
    if(msg_file == NULL) {
        printf("msggen: error, no such file\n");
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
    if(argc != 3) {
        printf("msggen [input directory] [output directory]\n");
        return -1;
    }

    char *input_dir = argv[1];
    char *output_dir = argv[2];

    /* open directory */
    DIR *dir = opendir(input_dir);
    if(dir == NULL) {
        printf("msggen: failed to open the given directory.\n");
        return -1;
    }

    bool failed = false;

    /* enumerate all the files under the given directory path */
    struct dirent* dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        if(dirent->d_type != DT_REG)
            continue;

        char *msg_file_name = dirent->d_name;
        int len = strlen(msg_file_name);

        /* find all msg files, i.e., file names end with ".msg" */
        if((strncmp(".msg", msg_file_name + len - 4, 4) == 0)) {
            /* load the msg file */
            char *file_path = calloc(sizeof(char), strlen(input_dir) + strlen(msg_file_name) + 50);
            sprintf(file_path, "%s/%s", input_dir, msg_file_name);
            char *msgs = load_msg_file(file_path);

            /* run code generation */
            int retval = codegen(msg_file_name, msgs, output_dir);

            /* clean up */
            free(msgs);

            /* failed for some reason */
            if(retval != 0) {
                failed = true;
                break;
            }

            msg_cnt++;
        }
    }

    /* close directory */
    closedir(dir);

    return failed == true ? -1 : 0;
}
