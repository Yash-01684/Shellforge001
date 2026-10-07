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
#include "builtin.h"
#include "jobs.h"

static int apply_redirections(command_t *command)
{
    int fd;

    if (command->input[0] != '\0')
    {
        fd = open(command->input, O_RDONLY);

        if (fd < 0)
        {
            perror(command->input);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            return -1;
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
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

static int execute_external(command_t *command)
{
    execvp(command->argv[0], command->argv);

    fprintf(
        stderr,
        "shellforge: %s: %s\n",
        command->argv[0],
        strerror(errno)
    );

    return 127;
}

static int execute_child(command_t *command)
{
    int result;

    if (apply_redirections(command) != 0)
    {
        return 1;
    }

    result = builtin_execute(command);

    if (result == BUILTIN_HANDLED)
    {
        return 0;
    }

    if (result == BUILTIN_EXIT)
    {
        return 0;
    }

    return execute_external(command);
}

static int execute_single_builtin(command_t *command)
{
    int saved_stdin = -1;
    int saved_stdout = -1;
    int result;

    if (strcmp(command->argv[0], "cd") != 0 &&
        strcmp(command->argv[0], "pwd") != 0 &&
        strcmp(command->argv[0], "echo") != 0 &&
        strcmp(command->argv[0], "exit") != 0)
    {
        return BUILTIN_NOT_FOUND;
    }

    saved_stdin = dup(STDIN_FILENO);
    saved_stdout = dup(STDOUT_FILENO);

    if (saved_stdin < 0 || saved_stdout < 0)
    {
        perror("dup");
        return 1;
    }

    if (apply_redirections(command) != 0)
    {
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);

        close(saved_stdin);
        close(saved_stdout);

        return 1;
    }

    result = builtin_execute(command);

    fflush(stdout);
    fflush(stderr);

    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);

    close(saved_stdin);
    close(saved_stdout);

    return result;
}

static void build_job_command(
    pipeline_t *pipeline,
    char *buffer,
    size_t buffer_size
)
{
    if (buffer == NULL || buffer_size == 0)
    {
        return;
    }

    buffer[0] = '\0';

    size_t used = 0;

    for (int i = 0; i < pipeline->command_count; i++)
    {
        command_t *command = &pipeline->commands[i];

        for (int j = 0; command->argv[j] != NULL; j++)
        {
            if (used > 0)
            {
                if (used + 1 >= buffer_size)
                {
                    return;
                }

                buffer[used++] = ' ';
                buffer[used] = '\0';
            }

            size_t remaining = buffer_size - used;

            int written = snprintf(
                buffer + used,
                remaining,
                "%s",
                command->argv[j]
            );

            if (written < 0)
            {
                return;
            }

            if ((size_t)written >= remaining)
            {
                buffer[buffer_size - 1] = '\0';
                return;
            }

            used += (size_t)written;
        }

        if (i < pipeline->command_count - 1)
        {
            if (used + 3 >= buffer_size)
            {
                return;
            }

            buffer[used++] = ' ';
            buffer[used++] = '|';
            buffer[used++] = ' ';
            buffer[used] = '\0';
        }
    }

    if (used + 2 < buffer_size)
    {
        buffer[used++] = ' ';
        buffer[used++] = '&';
        buffer[used] = '\0';
    }
}

static int execute_one_command(command_t *command)
{
    pid_t pid;
    int status;

    if (!command->background)
    {
        int builtin_result = execute_single_builtin(command);

        if (builtin_result != BUILTIN_NOT_FOUND)
        {
            if (builtin_result == BUILTIN_EXIT)
            {
                return EXECUTOR_EXIT;
            }

            return 0;
        }
    }

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        if (command->background)
        {
            if (setpgid(0, 0) < 0)
            {
                perror("setpgid");
                _exit(1);
            }
        }

        int result = execute_child(command);
        _exit(result);
    }

    if (command->background)
    {
        if (setpgid(pid, pid) < 0 && errno != EACCES)
        {
            perror("setpgid");
        }

        char job_command[1024];

        snprintf(
            job_command,
            sizeof(job_command),
            "%s &",
            command->argv[0]
        );

        job_t *job = job_add(
            pid,
            job_command,
            JOB_RUNNING
        );

        if (job != NULL)
        {
            printf(
                "[%d] %d %s\n",
                job->id,
                pid,
                job_command
            );
        }
        else
        {
            printf(
                "[background pid %d]\n",
                pid
            );
        }

        return 0;
    }

    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }

    return 1;
}

int execute_pipeline(pipeline_t *pipeline)
{
    if (pipeline == NULL || pipeline->command_count <= 0)
    {
        return 1;
    }

    /*
     * The parser stores '&' on the final command of a pipeline.
     * Treat the entire pipeline as a background job when the
     * final command has the background flag.
     */
    int pipeline_background =
        pipeline->commands[pipeline->command_count - 1].background;

    if (pipeline->command_count == 1)
    {
        return execute_one_command(
            &pipeline->commands[0]
        );
    }

    int previous_read = -1;
    pid_t pids[MAX_COMMANDS];
    int last_status = 0;
    pid_t pipeline_pgid = 0;

    for (int i = 0; i < pipeline->command_count; i++)
    {
        int pipefd[2] = {-1, -1};

        if (i < pipeline->command_count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");

                if (previous_read != -1)
                {
                    close(previous_read);
                }

                return 1;
            }
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (previous_read != -1)
            {
                close(previous_read);
            }

            if (pipefd[0] != -1)
            {
                close(pipefd[0]);
            }

            if (pipefd[1] != -1)
            {
                close(pipefd[1]);
            }

            return 1;
        }

        if (pid == 0)
        {
            if (pipeline_background)
            {
                if (pipeline_pgid == 0)
                {
                    if (setpgid(0, 0) < 0)
                    {
                        perror("setpgid");
                        _exit(1);
                    }
                }
                else
                {
                    if (setpgid(0, pipeline_pgid) < 0)
                    {
                        perror("setpgid");
                        _exit(1);
                    }
                }
            }

            if (previous_read != -1)
            {
                if (dup2(previous_read, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(1);
                }

                close(previous_read);
            }

            if (pipefd[1] != -1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(1);
                }

                close(pipefd[1]);
            }

            if (pipefd[0] != -1)
            {
                close(pipefd[0]);
            }

            int result = execute_child(
                &pipeline->commands[i]
            );

            _exit(result);
        }

        pids[i] = pid;

        if (pipeline_background)
        {
            if (pipeline_pgid == 0)
            {
                pipeline_pgid = pid;
            }

            if (setpgid(pid, pipeline_pgid) < 0 &&
                errno != EACCES)
            {
                perror("setpgid");
            }
        }

        if (previous_read != -1)
        {
            close(previous_read);
        }

        if (pipefd[1] != -1)
        {
            close(pipefd[1]);
        }

        previous_read = pipefd[0];
    }

    if (previous_read != -1)
    {
        close(previous_read);
    }

    /*
     * Background pipeline:
     * register the entire pipeline as one job and return
     * immediately to the Shellforge prompt.
     */
    if (pipeline_background)
    {
        char job_command[1024];

        build_job_command(
            pipeline,
            job_command,
            sizeof(job_command)
        );

        job_t *job = job_add(
            pipeline_pgid,
            job_command,
            JOB_RUNNING
        );

        if (job != NULL)
        {
            printf(
                "[%d] %d %s\n",
                job->id,
                pipeline_pgid,
                job_command
            );
        }
        else
        {
            printf(
                "[background pid %d]\n",
                pipeline_pgid
            );
        }

        return 0;
    }

    /*
     * Foreground pipeline.
     */
    for (int i = 0; i < pipeline->command_count; i++)
    {
        int status;

        if (waitpid(pids[i], &status, 0) < 0)
        {
            perror("waitpid");
            continue;
        }

        if (i == pipeline->command_count - 1)
        {
            if (WIFEXITED(status))
            {
                last_status = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status))
            {
                last_status = 128 + WTERMSIG(status);
            }
        }
    }

    return last_status;
}
