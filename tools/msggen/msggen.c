#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include "list.h"

char *support_types[] = {
    "bool",
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
    int  array_size;
    char *description;

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
        if(line[i] == ' ') {
            /* if j is not 0 while a space is read means the token is cut */
            if(j != 0) {
                /* end of the current token */
                token[token_cnt][j] = '\0';
                token_cnt++;
            }

            j = 0; //reset token string index
        } else {
            /* error, too many tokens */
            if(token_cnt >= 3) {
                printf("msggen: too many arguments in one line\n");
                return -1; //grammer rule: [fieldtype] [fieldname] [description]
            }

            /* copy the content for current token */
            token[token_cnt][j] = line[i];
            j++;
        }
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

int parse_variable_name(char *var_name)
{
    enum {
        PARSER_WAIT_NAME = 0,
        PARSER_WAIT_NAME_OR_LB = 1,
        PARSER_WAIT_INDEX = 2,
        PARSER_WAIT_INDEX_OR_RB = 3,
        PARSER_OVER_LENGTH = 4
    } VarNameParserStates;

    //TODO: identify the array size

    int len = strlen(var_name);

    int var_name_len = 0;
    int state = PARSER_WAIT_NAME;

    bool bracket_not_closed = false;

    int i;
    for(i = 0; i < len; i++) {
        char c = var_name[i];

        switch(state) {
            case PARSER_WAIT_NAME: {
                /* 1. handle the first character of the variable name to come in */
                bool legal = (c >= 'A' && c <= 'Z') ||
                             (c >= 'a' && c <= 'z') ||
                             (c == '_'); //variable name should not starts with numbers

                if(legal) {
                    state = PARSER_WAIT_NAME_OR_LB;
                } else {
                    return -1;
                }

                break;
            }
            case PARSER_WAIT_NAME_OR_LB: {
                /* 2. handle for more characters of variable name or a left
                 * bracket to come in */
                if(c == '[') {
                    state = PARSER_WAIT_INDEX; //left bracket detected!

                    /* as long as the left bracket is read, it is known that this
                     * is a array declaration, so we need to track the income
                     * character until the right bracket is read */
                    bracket_not_closed = true;
                } else {
                    bool legal = (c >= '0' && c <= '9') ||
                                 (c >= 'A' && c <= 'Z') ||
                                 (c >= 'a' && c <= 'z') ||
                                 (c == '_');
                    if(legal == false)
                        return -1;
                }

                break;
            }
            case PARSER_WAIT_INDEX: {
                /* 3. handle for the first index number to come in */
                if(c >= '0' && c <= '9') {
                    state = PARSER_WAIT_INDEX_OR_RB;
                } else {
                    return -1; //only numbers are accepted
                }

                break;
            }
            case PARSER_WAIT_INDEX_OR_RB: {
                /* 4. handle for more index numbers or a right bracket to come in */
                if(c == ']') {
                    /* ready to be done */
                    bracket_not_closed = false;

                    /* if everything is fine, this assigned new state should not be
                     * happened */
                    state = PARSER_OVER_LENGTH;
                } else {
                    if((c >= '0' && c <= '9') == false)
                        return -1; //only numbers are accepted
                }

                break;
            }
            case PARSER_OVER_LENGTH: {
                return -1; //bracket is closed, no more characters are accepted!
            }
        }
    }

    if(bracket_not_closed) {
        return -1;
    }

    return 0;
}

char *get_message_name(char *file_name)
{
    int len = strlen(file_name);
    char *msg_name = malloc(sizeof(char) * len);

    if(msg_name == NULL)
        return NULL;

    /* copy and omit the ".msg" part of the file name */
    strncpy(msg_name, file_name, len - 4);
    msg_name[len - 4] = '\0';

    if(msg_name_rule_check(msg_name) != 0) {
        printf("msggen: error, bad message name \"%s\"\n", msg_name);
        free(msg_name);
        return NULL;
    }

    return msg_name;
}

int codegen(char *file_name, char *msgs, char *output_dir)
{
    char *msg_name = get_message_name(file_name);
    if(msg_name == NULL)
        return -1;

    /* create c header file */
    char *c_header_name = calloc(sizeof(char), strlen(msg_name) + strlen(output_dir) + 50);
    sprintf(c_header_name, "%s/tenok_%s_msg.h", output_dir, msg_name);

    FILE *output_c_header = fopen(c_header_name, "wb");
    if(output_c_header == NULL) {
        printf("msggen: error, failed to write to %s\n", c_header_name);
        free(c_header_name);
        return -1;
    }

    /* create yaml file  */
    char *yaml_name = calloc(sizeof(char), strlen(msg_name) + strlen(output_dir) + 50);
    sprintf(yaml_name, "%s/tenok_%s_msg.yaml", output_dir, msg_name);

    FILE *output_yaml = fopen(yaml_name, "wb");
    if(output_yaml == NULL) {
        printf("msggen: error, failed to write to %s\n", yaml_name);
        free(yaml_name);
        return -1;
    }

    /* parse the message declaration*/
    char *line_start = msgs;
    char *line_end = msgs;

    int var_cnt = 0;

    struct list msg_var_list;
    list_init(&msg_var_list);

    bool error = false;
    int type_len = 0;
    int var_name_len = 0;
    int desc_len = 0;

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
        char *tokens[3];
        tokens[0] = calloc(sizeof(char), line_end - line_start + 1);
        tokens[1] = calloc(sizeof(char), line_end - line_start + 1);
        tokens[2] = calloc(sizeof(char), line_end - line_start + 1);
        int result = split_tokens(tokens, line_start, line_end - line_start);
        //printf("type:%s, name:%s\n\r", tokens[0], tokens[1]);

        if(result == 0) {
            type_len = strlen(tokens[0]);
            var_name_len = strlen(tokens[1]);
            desc_len = strlen(tokens[2]);

            /* check data type of current message data field */
            if(type_check(tokens[0]) != 0) {
                printf("msggen: error, unknown type \"%s\" in %s\n", tokens[0], file_name);
                error = true;
            }

            /* check variable name of current message data field */
            if(parse_variable_name(tokens[1]) != 0) {
                printf("msggen: error, bad variable name \"%s\" in %s\n", tokens[1], file_name);
                error = true;
            }
        } else {
            error = true; //grammer error
        }

        if(error) {
            /* clean up */
            fclose(output_c_header);
            fclose(output_yaml);

            while(!list_is_empty(&msg_var_list)) {
                struct list *curr = list_pop(&msg_var_list);
                struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);

                free(msg_var->c_type);
                free(msg_var->var_name);
                free(msg_var->description);
                free(msg_var);
            }

            free(tokens[0]);
            free(tokens[1]);
            free(tokens[2]);
            free(msg_name);
            free(c_header_name);
            free(yaml_name);

            return -1;
        }

        /* append new variable declaration to the list */
        struct msg_var_entry *new_var = malloc(sizeof(struct msg_var_entry));
        new_var->var_name = calloc(sizeof(char), type_len + 1);
        new_var->c_type = calloc(sizeof(char), var_name_len + 1);
        new_var->array_size = 0; //TODO
        new_var->description = calloc(sizeof(char), desc_len + 1);
        strncpy(new_var->c_type, tokens[0], type_len);
        strncpy(new_var->var_name, tokens[1], var_name_len);
        strncpy(new_var->description, tokens[2], desc_len);

        list_push(&msg_var_list, &new_var->list);

        var_cnt++;

        line_start = line_end + 1;

        /* clean up */
        free(tokens[0]);
        free(tokens[1]);
        free(tokens[2]);
    }

    /* check if variable with duplicated name exists */
    bool var_duplicated = false;

    struct list *cmp1;
    list_for_each(cmp1, &msg_var_list) {
        /* counter for recording how many time the variable name appears */
        int cnt = 0;

        /* pick the name of each variable in the list */
        struct msg_var_entry *cmp_var1 = list_entry(cmp1, struct msg_var_entry, list);

        /* compare with all variable in list */
        struct list *cmp2;
        list_for_each(cmp2, &msg_var_list) {
            struct msg_var_entry *cmp_var2 = list_entry(cmp2, struct msg_var_entry, list);

            if(strcmp(cmp_var1->var_name, cmp_var2->var_name) == 0)
                cnt++;
        }

        /* duplicated, variable with same name appeared */
        if(cnt > 1) {
            printf("msggen: variable name \"%s\" in %s is duplicated\n",
                   cmp_var1->var_name, file_name);

            var_duplicated = true;
            break;
        }
    }

    if(var_duplicated == false) {
        /*===================*
         * c code generation *
         *===================*/

        /* generate preprocessing code */
        fprintf(output_c_header,
                "#pragma once\n\n"
                "#define TENOK_MSG_ID_%s %d\n\n", msg_name, msg_cnt);

        /* generarte message structure */
        fprintf(output_c_header, "typedef struct __tenok_msg_%s_t {\n", msg_name);

        struct list *curr;
        list_for_each(curr, &msg_var_list) {
            struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);

            if(desc_len > 0) {
                fprintf(output_c_header, "    %s %s; //%s\n",
                        msg_var->c_type, msg_var->var_name, msg_var->description);
            } else {
                fprintf(output_c_header, "    %s %s;\n",
                        msg_var->c_type, msg_var->var_name);
            }
        }

        fprintf(output_c_header, "} tenok_msg_%s_t;\n\n", msg_name);

        /* generation message function */
        fprintf(output_c_header,
                "inline void pack_%s_tenok_msg(tenok_msg_%s_t *msg, debug_msg_t *payload)\n{\n"
                "    pack_tenok_msg_header(payload, MSG_ID_%s);\n",
                msg_name, msg_name, msg_name);

        list_for_each(curr, &msg_var_list) {
            struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);

            fprintf(output_c_header, "    pack_tenok_msg_field_%s(&msg->%s, payload);\n",
                    msg_var->c_type, msg_var->var_name);
        }

        fprintf(output_c_header, "}\n");

        /*======================*
         * yaml file generation *
         *======================*/
        fprintf(output_yaml, "msg_id: %d\n\npayload:\n", msg_cnt);

        list_for_each(curr, &msg_var_list) {
            struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);
            fprintf(output_yaml,
                    "  -\n    c_type: %s\n"
                    "    var_name: %s\n"
                    "    array_size: %d\n"
                    "    description: \"%s\"\n",
                    msg_var->c_type, msg_var->var_name,
                    msg_var->array_size, msg_var->description);
        }


        printf("[msggen] generate %s\n[msggen] generate %s\n", c_header_name, yaml_name);
    }

    /* clean up */
    fclose(output_c_header);
    fclose(output_yaml);

    while(!list_is_empty(&msg_var_list)) {
        struct list *curr = list_pop(&msg_var_list);
        struct msg_var_entry *msg_var = list_entry(curr, struct msg_var_entry, list);

        free(msg_var->c_type);
        free(msg_var->var_name);
        free(msg_var->description);
        free(msg_var);
    }

    free(msg_name);
    free(c_header_name);
    free(yaml_name);

    return (var_duplicated == false) ? 0 : -1;
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
