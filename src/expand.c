#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "expand.h"

static int expand_string(const char *input,
                         char *output,
                         size_t output_size)
{
    size_t i = 0;
    size_t j = 0;

    if (input == NULL || output == NULL || output_size == 0)
    {
        return -1;
    }

    while (input[i] != '\0')
    {
        if (input[i] == '$')
        {
            char variable[128];
            size_t v = 0;
            const char *value;

            i++;

            if (input[i] == '{')
            {
                i++;

                while (input[i] != '\0' &&
                       input[i] != '}' &&
                       v < sizeof(variable) - 1)
                {
                    variable[v++] = input[i++];
                }

                if (input[i] == '}')
                {
                    i++;
                }
            }
            else
            {
                while (input[i] != '\0' &&
                       (isalnum((unsigned char)input[i]) ||
                        input[i] == '_') &&
                       v < sizeof(variable) - 1)
                {
                    variable[v++] = input[i++];
                }
            }

            variable[v] = '\0';

            if (v == 0)
            {
                if (j >= output_size - 1)
                {
                    return -1;
                }

                output[j++] = '$';
                continue;
            }

            value = getenv(variable);

            if (value == NULL)
            {
                value = "";
            }

            while (*value != '\0')
            {
                if (j >= output_size - 1)
                {
                    return -1;
                }

                output[j++] = *value++;
            }
        }
        else
        {
            if (j >= output_size - 1)
            {
                return -1;
            }

            output[j++] = input[i++];
        }
    }

    output[j] = '\0';

    return 0;
}

int expand_variables(pipeline_t *pipeline)
{
    if (pipeline == NULL)
    {
        return -1;
    }

    for (int i = 0; i < pipeline->command_count; i++)
    {
        command_t *command = &pipeline->commands[i];

        for (int j = 0; j < command->argc; j++)
        {
            char expanded[MAX_TOKEN_LEN];

            if (expand_string(command->argv[j],
                              expanded,
                              sizeof(expanded)) != 0)
            {
                fprintf(stderr,
                        "Expand Error: Variable expansion too long\n");
                return -1;
            }

            char *new_argument = strdup(expanded);

            if (new_argument == NULL)
            {
                perror("strdup");
                return -1;
            }

            free(command->argv[j]);
            command->argv[j] = new_argument;
        }

        if (command->input[0] != '\0')
        {
            char expanded[MAX_TOKEN_LEN];

            if (expand_string(command->input,
                              expanded,
                              sizeof(expanded)) != 0)
            {
                fprintf(stderr,
                        "Expand Error: Input filename too long\n");
                return -1;
            }

            strncpy(command->input,
                    expanded,
                    MAX_TOKEN_LEN - 1);

            command->input[MAX_TOKEN_LEN - 1] = '\0';
        }

        if (command->output[0] != '\0')
        {
            char expanded[MAX_TOKEN_LEN];

            if (expand_string(command->output,
                              expanded,
                              sizeof(expanded)) != 0)
            {
                fprintf(stderr,
                        "Expand Error: Output filename too long\n");
                return -1;
            }

            strncpy(command->output,
                    expanded,
                    MAX_TOKEN_LEN - 1);

            command->output[MAX_TOKEN_LEN - 1] = '\0';
        }
    }

    return 0;
}
