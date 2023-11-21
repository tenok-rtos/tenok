#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

/* clang-format off */
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
/* clang-format on */

struct msg_var_entry {
    char *var_name;
    char *c_type;
    char *array_size;
    char *description;

    struct list_head list;
};

int msg_cnt = 0;

int split_tokens(char *token[3],
                 char *line,
                 int size,
                 char *file_name,
                 int line_num)
{
    enum {
        SPLIT_TYPE = 0,
        SPLIT_VAR_NAME = 1,
        SPLIT_DESCRIPTION = 2,
        SPLIT_OVER_LENGTH = 3
    } SplitSteps;

    int step = SPLIT_TYPE;
    int token_cnt = 0;
    int quote_cnt = 0; /* For handling the third token */

    int i = 0, j = 0;
    for (i = 0; i < size; i++) {
        char c = line[i];

        switch (step) {
        case SPLIT_TYPE:
        case SPLIT_VAR_NAME:
            /* Skip spaces before the start of the token */
            if (c == ' ') {
                if (j != 0) {
                    /* End of the current token */
                    token[token_cnt][j] = '\0';
                    token_cnt++;
                    j = 0;

                    /* Switch to next step */
                    step++;
                }
            } else if (c == '"') {
                printf("[msggen] %s:%d:%d: error, read unexpected syntax: \"\n",
                       file_name, line_num, i + 1);
                return -1;
            } else {
                /* Copy data for current token */
                token[token_cnt][j] = c;
                j++;

                /* Is this the last character? */
                if (i == (size - 1)) {
                    token_cnt++;
                }
            }

            break;
        case SPLIT_DESCRIPTION:
            if (quote_cnt == 0) {
                if (c == ' ') {
                    /* Skip spaces before the quote symbol */
                    continue;
                } else if (c == '"') {
                    /* First quote symbol is caught */
                    quote_cnt++;
                } else {
                    /* The first symbol of the third token should
                     * start with the qoute symbol */
                    printf(
                        "[msggen] %s:%d:%d: error, the third argument must be "
                        "quoted\n",
                        file_name, line_num, i + 1);
                    return -1;
                }
            } else if (quote_cnt == 1) {
                if (c == '"') {
                    /* The second quote symbol is caught */
                    quote_cnt++;

                    /* End of the current token */
                    token[token_cnt][j] = '\0';
                    token_cnt++;
                    j = 0;

                    step = SPLIT_OVER_LENGTH;
                } else {
                    /* Copy data for current token */
                    token[token_cnt][j] = c;
                    j++;
                }
            }

            break;
        case SPLIT_OVER_LENGTH:
            if (c != ' ') {
                printf("[msggen] %s:%d:%d: error, too many arguments\n",
                       file_name, line_num, i + 1);
                return -1;
            }
        }
    }

    if (token_cnt == 1) {
        printf("[msggen] %s:%d: error, variable name is missing\n", file_name,
               line_num);
        return -1;
    }

    if ((step == SPLIT_DESCRIPTION) && (quote_cnt == 1)) {
        printf("msggen: %s:%d: error, missing one \" symbol\n", file_name,
               line_num);
        return -1;
    }

    return token_cnt;
}

int type_check(char *type)
{
    int len = strlen(type);
    int type_list_size = sizeof(support_types) / sizeof(char *);

    int i;
    for (i = 0; i < type_list_size; i++) {
        if (strncmp(type, support_types[i], len) == 0)
            return 0;
    }

    return -1;
}

int parse_variable_name(char *input, char *var_name, char *array_size)
{
    enum {
        PARSER_WAIT_NAME = 0,
        PARSER_WAIT_NAME_OR_LB = 1,
        PARSER_WAIT_INDEX = 2,
        PARSER_WAIT_INDEX_OR_RB = 3,
        PARSER_OVER_LENGTH = 4
    } VarNameParserStates;

    int var_name_len = 0;
    int array_size_len = 0;

    int state = PARSER_WAIT_NAME;
    bool bracket_not_closed = false;

    int i;
    for (i = 0; i < strlen(input); i++) {
        char c = input[i];

        switch (state) {
        case PARSER_WAIT_NAME: {
            /* 1. Handle the first character of the variable name to come in */
            bool legal =
                (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c == '_'); /* Variable name should not start with numbers */

            if (legal) {
                state = PARSER_WAIT_NAME_OR_LB;
                var_name[var_name_len] = c;
                var_name_len++;
            } else {
                return -1;
            }

            break;
        }
        case PARSER_WAIT_NAME_OR_LB: {
            /* 2. Handle for more characters of variable name or a left
             * bracket to come in */
            if (c == '[') {
                state = PARSER_WAIT_INDEX; /* left bracket detected! */

                /* As long as the left bracket is read, it is known that this
                 * is a array declaration, so we need to track the income
                 * character until the right bracket is read. */
                bracket_not_closed = true;
            } else {
                bool legal = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                             (c >= 'a' && c <= 'z') || (c == '_');
                if (legal == true) {
                    var_name[var_name_len] = c;
                    var_name_len++;
                } else {
                    return -1;
                }
            }

            break;
        }
        case PARSER_WAIT_INDEX: {
            /* 3. Handle for the first index number to come in */
            if (c >= '0' && c <= '9') {
                state = PARSER_WAIT_INDEX_OR_RB;
                array_size[array_size_len] = c;
                array_size_len++;
            } else {
                /* Only accept numbers */
                return -1;
            }

            break;
        }
        case PARSER_WAIT_INDEX_OR_RB: {
            /* 4. Handle for more index numbers or a right bracket to come in */
            if (c == ']') {
                /* ready to be done */
                bracket_not_closed = false;

                /* If everything is fine, this assigned new state should not be
                 * happened */
                state = PARSER_OVER_LENGTH;
            } else {
                if ((c >= '0' && c <= '9') == true) {
                    array_size[array_size_len] = c;
                    array_size_len++;
                } else {
                    /* Only numbers are accepted */
                    return -1;
                }
            }

            break;
        }
        case PARSER_OVER_LENGTH: {
            /* Bracket is closed, no more characters are accepted! */
            return -1;
        }
        }
    }

    if (bracket_not_closed) {
        return -1;
    }

    return 0;
}

int msg_name_rule_check(char *msg_name)
{
    int len = strlen(msg_name);

    int i;
    for (i = 0; i < len; i++) {
        char c = msg_name[i];

        bool legal = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z') || (c == '_');

        /* Message name should not starts with a number */
        if (i == 0 && (c >= '0' && c <= '9'))
            legal = false;

        /* Bad message name */
        if (legal == false)
            return -1;
    }

    return 0;
}

char *get_message_name(char *file_name)
{
    int len = strlen(file_name);
    char *msg_name = malloc(sizeof(char) * len);

    if (msg_name == NULL)
        return NULL;

    /* Copy and omit the ".msg" part of the file name */
    strncpy(msg_name, file_name, len - 4);
    msg_name[len - 4] = '\0';

    if (msg_name_rule_check(msg_name) != 0) {
        printf("[msggen] error, bad message name \"%s\"\n", msg_name);
        free(msg_name);
        return NULL;
    }

    return msg_name;
}

int codegen(char *file_name, char *msgs, char *output_dir)
{
    char *msg_name = get_message_name(file_name);
    if (msg_name == NULL)
        return -1;

    /* Create C header file */
    char *c_header_name =
        calloc(sizeof(char), strlen(msg_name) + strlen(output_dir) + 50);
    sprintf(c_header_name, "%s/tenok_%s_msg.h", output_dir, msg_name);

    FILE *output_c_header = fopen(c_header_name, "wb");
    if (output_c_header == NULL) {
        printf("[msggen] error, failed to write to %s\n", c_header_name);
        free(c_header_name);
        return -1;
    }

    /* Create YAML file  */
    char *yaml_name =
        calloc(sizeof(char), strlen(msg_name) + strlen(output_dir) + 50);
    sprintf(yaml_name, "%s/tenok_%s_msg.yaml", output_dir, msg_name);

    FILE *output_yaml = fopen(yaml_name, "wb");
    if (output_yaml == NULL) {
        printf("[msggen] error, failed to write to %s\n", yaml_name);
        free(yaml_name);
        return -1;
    }

    /* Parse the message declaration*/
    char *line_start = msgs;
    char *line_end = msgs;

    int var_cnt = 0;

    struct list_head msg_var_list;
    INIT_LIST_HEAD(&msg_var_list);

    bool error = false;
    int type_len = 0;
    int var_name_len = 0;
    int desc_len = 0;

    int line_num = 0;

    while (1) {
        line_num++;

        /* Search the end of the current line */
        char *line_end = strchr(line_start, '\n');

        /* Skip empty lines */
        if ((line_end - line_start) == 0) {
            line_start++;
            continue;
        }

        /* EOF detection */
        if (line_end == NULL) {
            /* Leave the loop */
            break;
        }

        *line_end = '\0';

        /* Allocate memory space for storing tokens */
        char *tokens[3];
        tokens[0] =
            calloc(sizeof(char), line_end - line_start + 1); /* Field type */
        tokens[1] =
            calloc(sizeof(char), line_end - line_start + 1); /* Field name */
        tokens[2] =
            calloc(sizeof(char), line_end - line_start + 1); /* Description */

        /* Split tokens of current line */
        int token_cnt = split_tokens(tokens, line_start, line_end - line_start,
                                     file_name, line_num);

        /* token[1] can be further decomposed into variable name and index
         * number */
        char *var_name = calloc(sizeof(char), line_end - line_start + 1);
        char *array_size = calloc(sizeof(char), line_end - line_start + 1);

        if (token_cnt == -1) {
            /* Syntax error */
            error = true;
        } else if (token_cnt == 0) {
            /* Read nothing, skip current line */
            free(tokens[0]);
            free(tokens[1]);
            free(tokens[2]);
            free(var_name);
            free(array_size);

            /* Read next line */
            line_start = line_end + 1;
            continue;
        } else if (token_cnt >= 2) {
            type_len = strlen(tokens[0]);
            var_name_len = strlen(tokens[1]);
            desc_len = strlen(tokens[2]);

            /* Check data type of current message data field */
            if (type_check(tokens[0]) != 0) {
                printf("[msggen] %s:%d: error, unknown type \"%s\"\n",
                       file_name, line_num, tokens[0]);
                error = true;
            }

            /* Check variable name of current message data field */
            if (parse_variable_name(tokens[1], var_name, array_size) != 0) {
                printf("[msggen] %s:%d: error, illegal variable name: \"%s\"\n",
                       file_name, line_num, tokens[1]);
                error = true;
            }
        }

        /* Clean up then abort */
        if (error) {
            fclose(output_c_header);
            fclose(output_yaml);

            struct list_head *curr, *next;
            list_for_each_safe (curr, next, &msg_var_list) {
                struct msg_var_entry *msg_var =
                    list_first_entry(curr, struct msg_var_entry, list);
                list_del(curr);

                free(msg_var->c_type);
                free(msg_var->var_name);
                free(msg_var->description);
                free(msg_var);
            }

            free(tokens[0]);
            free(tokens[1]);
            free(tokens[2]);
            free(var_name);
            free(array_size);
            free(msg_name);
            free(c_header_name);
            free(yaml_name);

            return -1;
        }

        /* Append new variable declaration to the list */
        struct msg_var_entry *new_var = malloc(sizeof(struct msg_var_entry));
        new_var->var_name = var_name;
        new_var->c_type = calloc(sizeof(char), var_name_len + 1);
        strncpy(new_var->c_type, tokens[0], type_len);
        new_var->array_size = array_size;
        new_var->description = calloc(sizeof(char), desc_len + 1);
        strncpy(new_var->description, tokens[2], desc_len);

        list_add(&new_var->list, &msg_var_list);

        var_cnt++;

        line_start = line_end + 1;

        /* Clean up */
        free(tokens[0]);
        free(tokens[1]);
        free(tokens[2]);
    }

    /* Check if variable with duplicated name exists */
    bool var_duplicated = false;

    struct list_head *cmp1;
    list_for_each (cmp1, &msg_var_list) {
        /* Counter for recording how many time the variable name appears */
        int cnt = 0;

        /* Pick the name of each variable in the list */
        struct msg_var_entry *cmp_var1 =
            list_entry(cmp1, struct msg_var_entry, list);

        /* Compare with all variable in list */
        struct list_head *cmp2;
        list_for_each (cmp2, &msg_var_list) {
            struct msg_var_entry *cmp_var2 =
                list_entry(cmp2, struct msg_var_entry, list);

            if (strcmp(cmp_var1->var_name, cmp_var2->var_name) == 0)
                cnt++;
        }

        /* Duplicated, variable with same name appeared */
        if (cnt > 1) {
            printf(
                "[msggen] %s:%d: error, variable name \"%s\" is duplicated\n",
                file_name, line_num, cmp_var1->var_name);

            var_duplicated = true;
            break;
        }
    }

    if (var_duplicated == false) {
        /*===================*
         * C code generation *
         *===================*/

        /* Generate preprocessing code */
        fprintf(output_c_header,
                "#pragma once\n\n"
                "#include \"tenok_link.h\"\n\n"
                "#define TENOK_MSG_ID_%s %d\n\n",
                msg_name, msg_cnt);

        /* Generarte message structure */
        fprintf(output_c_header, "typedef struct __tenok_msg_%s_t {\n",
                msg_name);

        struct list_head *curr;
        list_for_each (curr, &msg_var_list) {
            struct msg_var_entry *msg_var =
                list_entry(curr, struct msg_var_entry, list);

            /* Data type and variable name */
            fprintf(output_c_header, "    %s %s", msg_var->c_type,
                    msg_var->var_name);

            /* Array size */
            if (strlen(msg_var->array_size) > 0) {
                fprintf(output_c_header, "[%s];", msg_var->array_size);
            } else {
                fprintf(output_c_header, ";");
            }

            /* Description */
            if (desc_len > 0) {
                fprintf(output_c_header, " //%s\n", msg_var->description);
            } else {
                fprintf(output_c_header, "\n");
            }
        }

        fprintf(output_c_header, "} tenok_msg_%s_t;\n\n", msg_name);

        /* Generate message functions */
        fprintf(output_c_header,
                "static inline size_t pack_tenok_%s_msg(tenok_msg_%s_t *msg, "
                "uint8_t *data)\n{\n"
                "    tenok_payload_t payload = {.data = data};\n"
                "    pack_tenok_msg_header(&payload, TENOK_MSG_ID_%s);\n",
                msg_name, msg_name, msg_name);

        list_for_each (curr, &msg_var_list) {
            struct msg_var_entry *msg_var =
                list_entry(curr, struct msg_var_entry, list);

            /* Generate functions to pack data fields */
            if (strlen(msg_var->array_size) > 0) {
                fprintf(output_c_header,
                        "    pack_tenok_msg_field_%s(msg->%s, &payload, %s);\n",
                        msg_var->c_type, msg_var->var_name,
                        msg_var->array_size);
            } else {
                fprintf(output_c_header,
                        "    pack_tenok_msg_field_%s(&msg->%s, &payload, 1);\n",
                        msg_var->c_type, msg_var->var_name);
            }
        }

        fprintf(output_c_header,
                "    generate_tenok_msg_checksum(&payload);\n");

        fprintf(output_c_header, "return payload.size;\n}\n");

        /*======================*
         * YAML file generation *
         *======================*/
        fprintf(output_yaml, "msg_id: %d\n\npayload:\n", msg_cnt);

        list_for_each (curr, &msg_var_list) {
            struct msg_var_entry *msg_var =
                list_entry(curr, struct msg_var_entry, list);

            if (strlen(msg_var->array_size) > 0) {
                fprintf(output_yaml,
                        "  -\n    c_type: %s\n"
                        "    var_name: %s\n"
                        "    array_size: %s\n"
                        "    description: \"%s\"\n",
                        msg_var->c_type, msg_var->var_name, msg_var->array_size,
                        msg_var->description);
            } else {
                fprintf(output_yaml,
                        "  -\n    c_type: %s\n"
                        "    var_name: %s\n"
                        "    array_size: 0\n"
                        "    description: \"%s\"\n",
                        msg_var->c_type, msg_var->var_name,
                        msg_var->description);
            }
        }

        printf("[msggen] generate %s\n[msggen] generate %s\n", c_header_name,
               yaml_name);
    }

    /* Clean up */
    fclose(output_c_header);
    fclose(output_yaml);

    struct list_head *curr, *next;
    list_for_each_safe (curr, next, &msg_var_list) {
        struct msg_var_entry *msg_var =
            list_entry(curr, struct msg_var_entry, list);
        list_del(curr);

        free(msg_var->c_type);
        free(msg_var->var_name);
        free(msg_var->array_size);
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
    /* Open msg file */
    FILE *msg_file = fopen(file_name, "r");
    if (msg_file == NULL) {
        printf("msggen: error, no such file\n");
        return NULL;
    }

    /* Get file size */
    fseek(msg_file, 0, SEEK_END);  /* Move to the end of the file */
    long size = ftell(msg_file);   /* Get size */
    fseek(msg_file, 0L, SEEK_SET); /* Move the start of the file */

    /* Read msg file */
    char *file_content = (char *) malloc(sizeof(char) * size);

    if (file_content != NULL)
        fread(file_content, sizeof(char), size, msg_file);

    /* Close file */
    fclose(msg_file);

    return file_content;
}

int main(int argc, char **argv)
{
    /* Incorrect argument number */
    if (argc != 3) {
        printf("msggen [input directory] [output directory]\n");
        return -1;
    }

    char *input_dir = argv[1];
    char *output_dir = argv[2];

    /* Open directory */
    DIR *dir = opendir(input_dir);
    if (dir == NULL) {
        printf("msggen: failed to open the given directory.\n");
        return -1;
    }

    bool failed = false;

    /* Enumerate all the files under the given directory path */
    struct dirent *dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_type != DT_REG)
            continue;

        char *msg_file_name = dirent->d_name;
        int len = strlen(msg_file_name);

        /* Find all msg files, i.e., file names end with ".msg" */
        if ((strncmp(".msg", msg_file_name + len - 4, 4) == 0)) {
            /* Load the msg file */
            char *file_path = calloc(
                sizeof(char), strlen(input_dir) + strlen(msg_file_name) + 50);
            sprintf(file_path, "%s/%s", input_dir, msg_file_name);
            char *msgs = load_msg_file(file_path);

            /* Run code generation */
            int retval = codegen(msg_file_name, msgs, output_dir);

            /* Clean up */
            free(msgs);

            /* Failed for some reason */
            if (retval != 0) {
                failed = true;
                break;
            }

            msg_cnt++;
        }
    }

    /* Close directory */
    closedir(dir);

    return failed == true ? -1 : 0;
}
