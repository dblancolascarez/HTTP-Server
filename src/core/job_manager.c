#include "job_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

// Implementación simple en memoria con persistencia por archivo
typedef struct job_entry {
    char *job_id;
    char *task_name;
    char *payload_json;
    char *result_json;
    char *error_msg;
    job_status_t status;
    int progress;
    long eta_ms;
    int cancel_requested;
    struct timeval created_at;
    struct timeval started_at;
    struct timeval finished_at;
    struct job_entry *next;
} job_entry_t;

static job_entry_t *jobs_head = NULL;
static pthread_mutex_t jobs_mutex = PTHREAD_MUTEX_INITIALIZER;
static char storage_path[1024] = "data/jobs";

// Helpers
static void ensure_storage_dir() {
    struct stat st = {0};
    if (stat(storage_path, &st) == -1) {
        mkdir(storage_path, 0755);
    }
}

static char* strdup_safe(const char *s) {
    if (!s) return NULL;
    char *r = strdup(s);
    return r;
}

static job_entry_t* find_job_locked(const char *job_id) {
    job_entry_t *it = jobs_head;
    while (it) {
        if (strcmp(it->job_id, job_id) == 0) return it;
        it = it->next;
    }
    return NULL;
}

static void persist_job_locked(job_entry_t *job) {
    ensure_storage_dir();
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s.json", storage_path, job->job_id);

    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "{\n");
    fprintf(f, "  \"job_id\": \"%s\",\n", job->job_id);
    fprintf(f, "  \"task_name\": \"%s\",\n", job->task_name ? job->task_name : "");
    fprintf(f, "  \"status\": %d,\n", job->status);
    fprintf(f, "  \"progress\": %d,\n", job->progress);
    fprintf(f, "  \"eta_ms\": %ld,\n", job->eta_ms);
    if (job->payload_json) fprintf(f, "  \"payload\": %s,\n", job->payload_json);
    if (job->result_json) fprintf(f, "  \"result\": %s,\n", job->result_json);
    if (job->error_msg) fprintf(f, "  \"error\": \"%s\",\n", job->error_msg);
    fprintf(f, "  \"created_at\": %ld\n", job->created_at.tv_sec);
    fprintf(f, "}\n");
    fclose(f);
}

int job_manager_init(const char *storage_dir) {
    if (storage_dir && storage_dir[0]) {
        strncpy(storage_path, storage_dir, sizeof(storage_path)-1);
        storage_path[sizeof(storage_path)-1] = '\0';
    }
    ensure_storage_dir();
    return 0;
}

void job_manager_shutdown() {
    // Persist all jobs
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *it = jobs_head;
    while (it) {
        persist_job_locked(it);
        it = it->next;
    }
    pthread_mutex_unlock(&jobs_mutex);
}

char* job_submit(const char *task_name, const char *payload_json, int priority_ms_timeout) {
    char idbuf[128];
    generate_request_id(idbuf, sizeof(idbuf));

    job_entry_t *job = calloc(1, sizeof(job_entry_t));
    job->job_id = strdup_safe(idbuf);
    job->task_name = strdup_safe(task_name ? task_name : "");
    job->payload_json = strdup_safe(payload_json);
    job->result_json = NULL;
    job->error_msg = NULL;
    job->status = JOB_STATUS_QUEUED;
    job->progress = 0;
    job->eta_ms = -1;
    job->cancel_requested = 0;
    gettimeofday(&job->created_at, NULL);

    pthread_mutex_lock(&jobs_mutex);
    job->next = jobs_head;
    jobs_head = job;
    persist_job_locked(job);
    pthread_mutex_unlock(&jobs_mutex);

    return strdup_safe(job->job_id);
}

int job_get_status(const char *job_id, job_status_info_t *out) {
    if (!job_id || !out) return -1;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) {
        pthread_mutex_unlock(&jobs_mutex);
        return -1;
    }
    out->status = job->status;
    out->progress = job->progress;
    out->eta_ms = job->eta_ms;
    pthread_mutex_unlock(&jobs_mutex);
    return 0;
}

char* job_get_result(const char *job_id) {
    if (!job_id) return NULL;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) { pthread_mutex_unlock(&jobs_mutex); return NULL; }
    char *res = NULL;
    if (job->status == JOB_STATUS_DONE && job->result_json) {
        res = strdup_safe(job->result_json);
    } else if (job->status == JOB_STATUS_ERROR && job->error_msg) {
        // Return an error JSON
        size_t n = strlen(job->error_msg) + 64;
        res = malloc(n);
        snprintf(res, n, "{\"error\": \"%s\"}", job->error_msg);
    }
    pthread_mutex_unlock(&jobs_mutex);
    return res;
}

int job_cancel(const char *job_id) {
    if (!job_id) return -1;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) { pthread_mutex_unlock(&jobs_mutex); return -1; }
    if (job->status == JOB_STATUS_DONE || job->status == JOB_STATUS_ERROR || job->status == JOB_STATUS_CANCELED) {
        pthread_mutex_unlock(&jobs_mutex);
        return 1; // not cancelable
    }
    job->cancel_requested = 1;
    job->status = JOB_STATUS_CANCELED;
    gettimeofday(&job->finished_at, NULL);
    persist_job_locked(job);
    pthread_mutex_unlock(&jobs_mutex);
    return 0;
}

int job_mark_running(const char *job_id) {
    if (!job_id) return -1;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) { pthread_mutex_unlock(&jobs_mutex); return -1; }
    job->status = JOB_STATUS_RUNNING;
    gettimeofday(&job->started_at, NULL);
    persist_job_locked(job);
    pthread_mutex_unlock(&jobs_mutex);
    return 0;
}

int job_update_progress(const char *job_id, int progress, long eta_ms) {
    if (!job_id) return -1;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) { pthread_mutex_unlock(&jobs_mutex); return -1; }
    job->progress = progress;
    job->eta_ms = eta_ms;
    persist_job_locked(job);
    pthread_mutex_unlock(&jobs_mutex);
    return 0;
}

int job_mark_done(const char *job_id, const char *result_json) {
    if (!job_id) return -1;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) { pthread_mutex_unlock(&jobs_mutex); return -1; }
    job->status = JOB_STATUS_DONE;
    if (job->result_json) free(job->result_json);
    job->result_json = strdup_safe(result_json);
    gettimeofday(&job->finished_at, NULL);
    job->progress = 100;
    persist_job_locked(job);
    pthread_mutex_unlock(&jobs_mutex);
    return 0;
}

int job_mark_error(const char *job_id, const char *error_msg) {
    if (!job_id) return -1;
    pthread_mutex_lock(&jobs_mutex);
    job_entry_t *job = find_job_locked(job_id);
    if (!job) { pthread_mutex_unlock(&jobs_mutex); return -1; }
    job->status = JOB_STATUS_ERROR;
    if (job->error_msg) free(job->error_msg);
    job->error_msg = strdup_safe(error_msg);
    gettimeofday(&job->finished_at, NULL);
    persist_job_locked(job);
    pthread_mutex_unlock(&jobs_mutex);
    return 0;
}
// Sistema de jobs asíncronos completo