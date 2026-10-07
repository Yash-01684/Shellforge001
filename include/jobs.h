#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64

typedef enum
{
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct job
{
    int id;
    pid_t pgid;
    job_state_t state;
    char *command;
} job_t;

/*
 * Job table management
 */
void jobs_init(void);

job_t *job_add(pid_t pgid, const char *command, job_state_t state);

job_t *job_find(int id);

job_t *job_find_by_pgid(pid_t pgid);

int job_remove(int id);

/*
 * Job state management
 */
void job_stop(job_t *job);

void job_continue(job_t *job);

void job_done(job_t *job);

/*
 * Job status and display
 */
void jobs_update_status(void);

void jobs_print(void);

/*
 * Most-recent job helpers
 */
int jobs_get_most_recent_id(void);

int jobs_get_most_recent_stopped_id(void);

/*
 * Foreground/background job control
 */
int fg_command(int job_id);

int bg_command(int job_id);

/*
 * Foreground process-group tracking
 */
void set_foreground_pgid(pid_t pgid);

pid_t get_foreground_pgid(void);

#endif /* JOBS_H */
