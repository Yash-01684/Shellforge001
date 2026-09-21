#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "lexer.h"
#include "token.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"

static void print_history(void)
{
    HIST_ENTRY **entries = history_list();

    printf("\n----------- Command History -----------\n");

    if (entries != NULL)
    {
        for (int i = 0; entries[i] != NULL; i++)
        {
            printf("%d  %s\n", i + 1, entries[i]->line);
        }
    }

    printf("---------------------------------------\n");
}

int main(void)
{
    printf("=====================================\n");
    printf("             Shellforge\n");
    printf("      A Unix Style Shell written in C\n");
    printf("=====================================\n");

    while (1)
    {
        char *line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }

        add_history(line);

        if (strcmp(line, "history") == 0)
        {
            print_history();
            free(line);
            continue;
        }

        token_list_t tokens;

        if (lexer(line, &tokens) != 0)
        {
            free(line);
            continue;
        }

        pipeline_t pipeline;

        if (parse(&tokens, &pipeline) != 0)
        {
            free(line);
            continue;
        }

        if (expand_variables(&pipeline) != 0)
        {
            pipeline_free(&pipeline);
            free(line);
            continue;
        }

        if (pipeline.command_count == 1)
        {
            int result = builtin_execute(&pipeline.commands[0]);

            if (result == BUILTIN_EXIT)
            {
                pipeline_free(&pipeline);
                free(line);
                break;
            }

            if (result == BUILTIN_HANDLED)
            {
                pipeline_free(&pipeline);
                free(line);
                continue;
            }
        }

        execute_pipeline(&pipeline);

        pipeline_free(&pipeline);
        free(line);
    }

    return 0;
}
