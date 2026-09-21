#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static void command_init(command_t *command)
{
    if (command == NULL)
    {
        return;
    }

    command->argc = 0;
    command->input[0] = '\0';
    command->output[0] = '\0';
    command->append = 0;
    command->background = 0;

    for (int i = 0; i < MAX_ARGS; i++)
    {
        command->argv[i] = NULL;
    }
}

void pipeline_init(pipeline_t *pipeline)
{
    if (pipeline == NULL)
    {
        return;
    }

    pipeline->command_count = 1;

    for (int i = 0; i < MAX_COMMANDS; i++)
    {
        command_init(&pipeline->commands[i]);
    }
}

static int add_argument(command_t *command, const char *text)
{
    if (command == NULL || text == NULL)
    {
        return -1;
    }

    if (command->argc >= MAX_ARGS - 1)
    {
        fprintf(stderr, "Parser Error: Too many arguments\n");
        return -1;
    }

    command->argv[command->argc] = strdup(text);

    if (command->argv[command->argc] == NULL)
    {
        perror("strdup");
        return -1;
    }

    command->argc++;

    return 0;
}

int parse(const token_list_t *tokens, pipeline_t *pipeline)
{
    if (tokens == NULL || pipeline == NULL)
    {
        return -1;
    }

    pipeline_init(pipeline);

    int current = 0;

    for (int i = 0; i < tokens->count; i++)
    {
        const token_t *t = &tokens->tokens[i];
        command_t *command = &pipeline->commands[current];

        switch (t->type)
        {
            case TOKEN_WORD:
                if (add_argument(command, t->text) != 0)
                {
                    pipeline_free(pipeline);
                    return -1;
                }
                break;

            case TOKEN_INPUT:
                if (i + 1 >= tokens->count ||
                    tokens->tokens[i + 1].type != TOKEN_WORD)
                {
                    fprintf(stderr,
                            "Parser Error: Missing input filename\n");
                    pipeline_free(pipeline);
                    return -1;
                }

                strncpy(command->input,
                        tokens->tokens[++i].text,
                        MAX_TOKEN_LEN - 1);

                command->input[MAX_TOKEN_LEN - 1] = '\0';
                break;

            case TOKEN_OUTPUT:
                if (i + 1 >= tokens->count ||
                    tokens->tokens[i + 1].type != TOKEN_WORD)
                {
                    fprintf(stderr,
                            "Parser Error: Missing output filename\n");
                    pipeline_free(pipeline);
                    return -1;
                }

                strncpy(command->output,
                        tokens->tokens[++i].text,
                        MAX_TOKEN_LEN - 1);

                command->output[MAX_TOKEN_LEN - 1] = '\0';
                command->append = 0;
                break;

            case TOKEN_APPEND:
                if (i + 1 >= tokens->count ||
                    tokens->tokens[i + 1].type != TOKEN_WORD)
                {
                    fprintf(stderr,
                            "Parser Error: Missing append filename\n");
                    pipeline_free(pipeline);
                    return -1;
                }

                strncpy(command->output,
                        tokens->tokens[++i].text,
                        MAX_TOKEN_LEN - 1);

                command->output[MAX_TOKEN_LEN - 1] = '\0';
                command->append = 1;
                break;

            case TOKEN_PIPE:
                if (current >= MAX_COMMANDS - 1)
                {
                    fprintf(stderr,
                            "Parser Error: Too many pipeline commands\n");
                    pipeline_free(pipeline);
                    return -1;
                }

                if (command->argc == 0)
                {
                    fprintf(stderr,
                            "Parser Error: Empty pipeline command\n");
                    pipeline_free(pipeline);
                    return -1;
                }

                current++;
                pipeline->command_count++;

                command_init(&pipeline->commands[current]);
                break;

            case TOKEN_BACKGROUND:
                command->background = 1;
                break;

            case TOKEN_END:
                break;

            default:
                fprintf(stderr,
                        "Parser Error: Unknown token\n");
                pipeline_free(pipeline);
                return -1;
        }
    }

    for (int i = 0; i < pipeline->command_count; i++)
    {
        command_t *command = &pipeline->commands[i];

        if (command->argc == 0)
        {
            fprintf(stderr,
                    "Parser Error: Command has no arguments\n");
            pipeline_free(pipeline);
            return -1;
        }

        command->argv[command->argc] = NULL;
    }

    return 0;
}

void pipeline_print(const pipeline_t *pipeline)
{
    if (pipeline == NULL)
    {
        return;
    }

    printf("\n================ PIPELINE ================\n");

    for (int i = 0; i < pipeline->command_count; i++)
    {
        const command_t *command = &pipeline->commands[i];

        printf("\nCommand %d\n", i + 1);
        printf("Arguments\n");

        for (int j = 0; j < command->argc; j++)
        {
            printf("argv[%d] = %s\n",
                   j,
                   command->argv[j]);
        }

        printf("Input : %s\n",
               command->input[0] != '\0'
                   ? command->input
                   : "None");

        printf("Output : %s\n",
               command->output[0] != '\0'
                   ? command->output
                   : "None");

        printf("Append : %s\n",
               command->append ? "Yes" : "No");

        printf("Background : %s\n",
               command->background ? "Yes" : "No");
    }

    printf("===========================================\n");
}

void pipeline_free(pipeline_t *pipeline)
{
    if (pipeline == NULL)
    {
        return;
    }

    for (int i = 0; i < pipeline->command_count; i++)
    {
        for (int j = 0; j < pipeline->commands[i].argc; j++)
        {
            free(pipeline->commands[i].argv[j]);
            pipeline->commands[i].argv[j] = NULL;
        }

        pipeline->commands[i].argc = 0;
    }

    pipeline->command_count = 0;
}
