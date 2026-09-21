#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"

static void apply_redirections(command_t *command)
{
    int fd;

    if (command->input[0] != '\0')
    {
        fd = open(command->input, O_RDONLY);

        if (fd < 0)
        {
            perror(command->input);
            exit(EXIT_FAILURE);
        }

        if (dup2(fd, STDIN_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            exit(EXIT_FAILURE);
        }

        close(fd);
    }

    if (command->output[0] != '\0')
    {
        int flags = O_WRONLY | O_CREAT;

        if (command->append)
        {
            flags |= O_APPEND;
        }
        else
        {
            flags |= O_TRUNC;
        }

        fd = open(command->output, flags, 0644);

        if (fd < 0)
        {
            perror(command->output);
            exit(EXIT_FAILURE);
        }

        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            exit(EXIT_FAILURE);
        }

        close(fd);
    }
}

static void execute_command(command_t *command)
{
    apply_redirections(command);

    execvp(command->argv[0], command->argv);

    fprintf(stderr,
            "shellforge: %s: %s\n",
            command->argv[0],
            strerror(errno));

    exit(127);
}

int execute_pipeline(pipeline_t *pipeline)
{
    if (pipeline == NULL || pipeline->command_count <= 0)
    {
        return -1;
    }

    int previous_pipe_read = -1;
    pid_t pids[MAX_COMMANDS];

    for (int i = 0; i < pipeline->command_count; i++)
    {
        int pipefd[2] = {-1, -1};

        if (i < pipeline->command_count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");

                if (previous_pipe_read != -1)
                {
                    close(previous_pipe_read);
                }

                return -1;
            }
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (previous_pipe_read != -1)
            {
                close(previous_pipe_read);
            }

            if (pipefd[0] != -1)
            {
                close(pipefd[0]);
            }

            if (pipefd[1] != -1)
            {
                close(pipefd[1]);
            }

            return -1;
        }

        if (pid == 0)
        {
            if (previous_pipe_read != -1)
            {
                if (dup2(previous_pipe_read, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                close(previous_pipe_read);
            }

            if (pipefd[1] != -1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                close(pipefd[1]);
            }

            if (pipefd[0] != -1)
            {
                close(pipefd[0]);
            }

            execute_command(&pipeline->commands[i]);
        }

        pids[i] = pid;

        if (previous_pipe_read != -1)
        {
            close(previous_pipe_read);
        }

        if (pipefd[1] != -1)
        {
            close(pipefd[1]);
        }

        previous_pipe_read = pipefd[0];
    }

    if (previous_pipe_read != -1)
    {
        close(previous_pipe_read);
    }

    if (!pipeline->commands[pipeline->command_count - 1].background)
    {
        for (int i = 0; i < pipeline->command_count; i++)
        {
            waitpid(pids[i], NULL, 0);
        }
    }

    return 0;
}
