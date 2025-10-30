#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include <pthread.h>
#include <sys/time.h>

#include "../utils/utils.h"

// Estado de un job
typedef enum {
    JOB_STATUS_QUEUED,
    JOB_STATUS_RUNNING,
    JOB_STATUS_DONE,
    JOB_STATUS_ERROR,
    JOB_STATUS_CANCELED
} job_status_t;

// Estructura de estadísticas/estado retornable
typedef struct {
    job_status_t status;
    int progress;       // 0..100
    long eta_ms;        // estimación en ms (puede ser -1 si desconocido)
} job_status_info_t;

// Inicializar el job manager (opcionalmente con directorio para persistencia)
// Devuelve 0 on success
int job_manager_init(const char *storage_dir);

// Shutdown: libera recursos (no cancela trabajos en curso necesariamente)
void job_manager_shutdown();

// Crear un job; 'payload_json' puede ser NULL. Retorna job_id (malloc) que el caller debe liberar.
// El job se registra con estado QUEUED.
char* job_submit(const char *task_name, const char *payload_json, int priority_ms_timeout);

// Obtener estado (status/progress/eta). Retorna 0 si encontrado, -1 si no existe.
int job_get_status(const char *job_id, job_status_info_t *out);

// Obtener resultado: si el job está DONE devuelve un strdup del JSON resultado (caller libera).
// Si el job está en ERROR devuelve strdup del mensaje de error. Si no está listo retorna NULL.
char* job_get_result(const char *job_id);

// Intentar cancelar un job. Retorna 0 si fue marcado canceled, 1 si no cancelable (ya done/running no cancelable), -1 si no encontrado.
int job_cancel(const char *job_id);

// Funciones para workers (marcar estado y resultado)
int job_mark_running(const char *job_id);
int job_update_progress(const char *job_id, int progress, long eta_ms);
int job_mark_done(const char *job_id, const char *result_json);
int job_mark_error(const char *job_id, const char *error_msg);

#endif // JOB_MANAGER_H
