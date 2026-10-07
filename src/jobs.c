#define _POSIX_C_SOURCE 200809L

#include "jobs.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static job_t job_table[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;
static pid_t current_fg_pgid = 0;

static void handle_sigint(int sig)
{
    (void)sig;

    if (current_fg_pgid > 0)
    {
        kill(-current_fg_pgid, SIGINT);
    }
}

static void handle_sigtstp(int sig)
{
    (void)sig;

    if (current_fg_pgid > 0)
    {
        kill(-current_fg_pgid, SIGTSTP);
    }
}

void jobs_init(void)
{
    memset(job_table, 0, sizeof(job_table));

    job_count = 0;
    next_job_id = 1;
    current_fg_pgid = 0;

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
}

job_t *job_add(pid_t pgid, const char *command, job_state_t state)
{
    if (job_count >= MAX_JOBS)
    {
        fprintf(
            stderr,
            "shellforge: maximum number of jobs reached\n"
        );

        return NULL;
    }

    job_t *job = &job_table[job_count];

    job->id = next_job_id++;
    job->pgid = pgid;
    job->state = state;

    if (command != NULL)
    {
        job->command = strdup(command);
    }
    else
    {
        job->command = strdup("");
    }

    if (job->command == NULL)
    {
        perror("strdup");
        return NULL;
    }

    job_count++;

    return job;
}

job_t *job_find(int id)
{
    for (int i = 0; i < job_count; i++)
    {
        if (job_table[i].id == id)
        {
            return &job_table[i];
        }
    }

    return NULL;
}

job_t *job_find_by_pgid(pid_t pgid)
{
    for (int i = 0; i < job_count; i++)
    {
        if (job_table[i].pgid == pgid)
        {
            return &job_table[i];
        }
    }

    return NULL;
}

int job_remove(int id)
{
    for (int i = 0; i < job_count; i++)
    {
        if (job_table[i].id == id)
        {
            free(job_table[i].command);

            for (int j = i; j < job_count - 1; j++)
            {
                job_table[j] = job_table[j + 1];
            }

            job_count--;

            return 0;
        }
    }

    return -1;
}

void job_stop(job_t *job)
{
    if (job != NULL)
    {
        job->state = JOB_STOPPED;
    }
}

void job_continue(job_t *job)
{
    if (job != NULL)
    {
        job->state = JOB_RUNNING;

        if (job->pgid > 0)
        {
            kill(-job->pgid, SIGCONT);
        }
    }
}

void job_done(job_t *job)
{
    if (job != NULL)
    {
        job->state = JOB_DONE;
    }
}

void jobs_update_status(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(
                -1,
                &status,
                WNOHANG | WUNTRACED | WCONTINUED
            )) > 0)
    {
        job_t *job = job_find_by_pgid(pid);

        if (job == NULL)
        {
            continue;
        }

        if (WIFSTOPPED(status))
        {
            job_stop(job);
        }
        else if (WIFCONTINUED(status))
        {
            job->state = JOB_RUNNING;
        }
        else if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            job_done(job);
        }
    }
}

void jobs_print(void)
{
    printf("\n----------- Jobs -----------\n");

    for (int i = 0; i < job_count;)
    {
        job_t *job = &job_table[i];

        if (job->state == JOB_DONE)
        {
            printf(
                "[%d]  Done      %s\n",
                job->id,
                job->command
            );

            free(job->command);

            for (int j = i; j < job_count - 1; j++)
            {
                job_table[j] = job_table[j + 1];
            }

            job_count--;

            continue;
        }

        if (job->state == JOB_RUNNING)
        {
            printf(
                "[%d]  Running   %s\n",
                job->id,
                job->command
            );
        }
        else if (job->state == JOB_STOPPED)
        {
            printf(
                "[%d]  Stopped   %s\n",
                job->id,
                job->command
            );
        }

        i++;
    }

    if (job_count == 0)
    {
        printf("No active jobs.\n");
    }

    printf("----------------------------\n");
}

int jobs_get_most_recent_id(void)
{
    if (job_count == 0)
    {
        return -1;
    }

    for (int i = job_count - 1; i >= 0; i--)
    {
        if (job_table[i].state != JOB_DONE)
        {
            return job_table[i].id;
        }
    }

    return -1;
}

int jobs_get_most_recent_stopped_id(void)
{
    for (int i = job_count - 1; i >= 0; i--)
    {
        if (job_table[i].state == JOB_STOPPED)
        {
            return job_table[i].id;
        }
    }

    return -1;
}

void set_foreground_pgid(pid_t pgid)
{
    current_fg_pgid = pgid;
}

pid_t get_foreground_pgid(void)
{
    return current_fg_pgid;
}

int fg_command(int job_id)
{
    job_t *job;

    if (job_id <= 0)
    {
        job_id = jobs_get_most_recent_id();
    }

    if (job_id < 0)
    {
        fprintf(stderr, "fg: no current job\n");
        return 1;
    }

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(
            stderr,
            "fg: no such job: %d\n",
            job_id
        );

        return 1;
    }

    if (job->pgid <= 0)
    {
        fprintf(stderr, "fg: invalid process group\n");
        return 1;
    }

    if (tcsetpgrp(STDIN_FILENO, job->pgid) < 0)
    {
        perror("tcsetpgrp");
        return 1;
    }

    set_foreground_pgid(job->pgid);

    if (job->state == JOB_STOPPED)
    {
        if (kill(-job->pgid, SIGCONT) < 0)
        {
            perror("fg: SIGCONT");
        }

        job->state = JOB_RUNNING;
    }

    int status;

    while (waitpid(
               -job->pgid,
               &status,
               WUNTRACED
           ) > 0)
    {
        if (WIFSTOPPED(status))
        {
            job->state = JOB_STOPPED;
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            job->state = JOB_DONE;
            break;
        }
    }

    set_foreground_pgid(0);

    if (tcsetpgrp(
            STDIN_FILENO,
            getpgrp()
        ) < 0)
    {
        perror("tcsetpgrp");
    }

    if (job->state == JOB_DONE)
    {
        job_remove(job->id);
    }

    return 0;
}

int bg_command(int job_id)
{
    job_t *job;

    if (job_id <= 0)
    {
        job_id = jobs_get_most_recent_stopped_id();

        if (job_id < 0)
        {
            job_id = jobs_get_most_recent_id();
        }
    }

    if (job_id < 0)
    {
        fprintf(stderr, "bg: no current job\n");
        return 1;
    }

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(
            stderr,
            "bg: no such job: %d\n",
            job_id
        );

        return 1;
    }

    if (job->pgid <= 0)
    {
        fprintf(stderr, "bg: invalid process group\n");
        return 1;
    }

    if (kill(-job->pgid, SIGCONT) < 0)
    {
        perror("bg: SIGCONT");
        return 1;
    }

    job->state = JOB_RUNNING;

    printf(
        "[%d] %s &\n",
        job->id,
        job->command
    );

    return 0;
}
