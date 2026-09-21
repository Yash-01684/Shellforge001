#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"

static int builtin_cd(command_t *command)
{
    char *directory;

    if (command->argc == 1)
    {
        directory = getenv("HOME");

        if (directory == NULL)
        {
            fprintf(stderr, "cd: HOME is not set\n");
            return -1;
        }
    }
    else if (command->argc == 2)
    {
        directory = command->argv[1];
    }
    else
    {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
    }

    if (chdir(directory) != 0)
    {
        perror("cd");
        return -1;
    }

    return 0;
}

static int builtin_pwd(command_t *command)
{
    char current_directory[4096];

    if (command->argc != 1)
    {
        fprintf(stderr, "pwd: too many arguments\n");
        return -1;
    }

    if (getcwd(current_directory, sizeof(current_directory)) == NULL)
    {
        perror("pwd");
        return -1;
    }

    printf("%s\n", current_directory);

    return 0;
}

static int builtin_echo(command_t *command)
{
    for (int i = 1; i < command->argc; i++)
    {
        if (i > 1)
        {
            printf(" ");
        }

        printf("%s", command->argv[i]);
    }

    printf("\n");

    return 0;
}

static int builtin_exit(command_t *command)
{
    if (command->argc > 1)
    {
        fprintf(stderr, "exit: too many arguments\n");
        return -1;
    }

    return BUILTIN_EXIT;
}

int builtin_execute(command_t *command)
{
    if (command == NULL || command->argc == 0)
    {
        return BUILTIN_NOT_FOUND;
    }

    if (strcmp(command->argv[0], "cd") == 0)
    {
        builtin_cd(command);
        return BUILTIN_HANDLED;
    }

    if (strcmp(command->argv[0], "pwd") == 0)
    {
        builtin_pwd(command);
        return BUILTIN_HANDLED;
    }

    if (strcmp(command->argv[0], "echo") == 0)
    {
        builtin_echo(command);
        return BUILTIN_HANDLED;
    }

    if (strcmp(command->argv[0], "exit") == 0)
    {
        return builtin_exit(command);
    }

    return BUILTIN_NOT_FOUND;
}
