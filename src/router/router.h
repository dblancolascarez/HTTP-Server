// Router: mapeo de paths -> handlers
#ifndef ROUTER_H
#define ROUTER_H

#include <sys/types.h>
#include "../server/http.h"
#include "../server/server.h"
#include "../utils/utils.h"  // Para query_params_t

// Maneja una petición HTTP parseada. Debe escribir la respuesta al socket
// client_fd y retornar el número de bytes enviados o -1 en caso de error.
// bytes_received se pasa para que el router pueda reportar métricas si lo desea.
ssize_t router_handle_request(const http_request_t *req, int client_fd,
                              const char *request_id,
                              server_state_t *server,
                              size_t bytes_received);

ssize_t handle_jobs_request(const char *subpath, int client_fd,
                          const char *request_id, query_params_t *qp);


#endif // ROUTER_H
