// Tipos y constantes HTTP
#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>


// ============================================================================
// HTTP STATUS CODES
// ============================================================================

#define HTTP_OK                    200
#define HTTP_BAD_REQUEST           400
#define HTTP_NOT_FOUND             404
#define HTTP_CONFLICT              409
#define HTTP_TOO_MANY_REQUESTS     429
#define HTTP_INTERNAL_ERROR        500
#define HTTP_SERVICE_UNAVAILABLE   503

// ============================================================================
// HTTP STRUCTURES
// ============================================================================

// Estructura de un HTTP request parseado
typedef struct {
    char method[16];           // GET, HEAD, etc.
    char path[256];            // /isprime
    char query[2048];          // n=97&max=100
    char version[16];          // HTTP/1.0
    char host[256];            // Host header (opcional)
    int content_length;        // Content-Length header
    bool connection_close;     // Connection: close
} http_request_t;

// Estructura de un HTTP response
typedef struct {
    int status_code;           // 200, 404, etc.
    const char *content_type;  // application/json, text/plain
    const char *body;          // Response body
    size_t body_length;        // Longitud del body
    const char *request_id;    // X-Request-Id
    int worker_pid;            // X-Worker-Pid
    const char *extra_headers; // Headers adicionales (opcional)
} http_response_t;

// ============================================================================
// HTTP PARSING
// ============================================================================

/**
 * Parsear un HTTP request desde raw bytes
 * 
 * Formato esperado:
 *   GET /isprime?n=97 HTTP/1.0\r\n
 *   Host: localhost:8080\r\n
 *   Connection: close\r\n
 *   \r\n
 * 
 * @param raw_request Buffer con el request completo
 * @param request Estructura donde guardar el resultado
 * @return 0 si éxito, -1 si error de parsing
 */
int http_parse_request(const char *raw_request, http_request_t *request);

/**
 * Validar que el método HTTP es soportado
 * 
 * @param method Método HTTP (GET, HEAD, etc.)
 * @return true si es soportado
 */
bool http_is_method_supported(const char *method);

/**
 * Extraer query string de una URI completa
 * 
 * Ejemplo: "/isprime?n=97" -> path="/isprime", query="n=97"
 * 
 * @param uri URI completa
 * @param path Buffer donde guardar el path (mínimo 256 bytes)
 * @param query Buffer donde guardar el query (mínimo 2048 bytes)
 */
void http_split_uri(const char *uri, char *path, char *query);

// ============================================================================
// HTTP RESPONSE BUILDING
// ============================================================================

/**
 * Enviar respuesta HTTP completa
 * 
 * Construye y envía:
 *   HTTP/1.0 200 OK\r\n
 *   Content-Type: application/json\r\n
 *   Content-Length: 42\r\n
 *   X-Request-Id: ...\r\n
 *   X-Worker-Pid: 1234\r\n
 *   Connection: close\r\n
 *   \r\n
 *   {"result": "ok"}
 * 
 * @param client_fd File descriptor del socket del cliente
 * @param response Estructura con los datos de la respuesta
 * @return Número de bytes enviados, o -1 si error
 */
int http_send_response(int client_fd, const http_response_t *response);

/**
 * Enviar respuesta JSON simple (wrapper conveniente)
 * 
 * @param client_fd File descriptor del cliente
 * @param status_code Código HTTP (200, 404, etc.)
 * @param json_body Cuerpo JSON como string
 * @param request_id Request ID único
 * @return Bytes enviados o -1 si error
 */
int http_send_json(int client_fd, int status_code, const char *json_body, 
                   const char *request_id);

/**
 * Enviar respuesta de error JSON
 * 
 * Formato: {"error": "mensaje"}
 * 
 * @param client_fd File descriptor del cliente
 * @param status_code Código de error (400, 404, 500, etc.)
 * @param error_message Mensaje de error
 * @param request_id Request ID único
 * @return Bytes enviados o -1 si error
 */
int http_send_error(int client_fd, int status_code, const char *error_message,
                    const char *request_id);

/**
 * Enviar respuesta 503 Service Unavailable con retry_after
 * 
 * Usado para backpressure cuando las colas están llenas
 * 
 * @param client_fd File descriptor del cliente
 * @param retry_after_ms Milisegundos sugeridos para reintentar
 * @param request_id Request ID único
 * @return Bytes enviados o -1 si error
 */
int http_send_503_backpressure(int client_fd, int retry_after_ms, 
                                const char *request_id);

// ============================================================================
// HTTP UTILITIES
// ============================================================================

/**
 * Convertir código de status a texto descriptivo
 * 
 * @param status_code Código HTTP (200, 404, etc.)
 * @return String con el texto (ej: "OK", "Not Found")
 */
const char* http_status_text(int status_code);

/**
 * Obtener Content-Type por extensión de archivo
 * 
 * @param filename Nombre del archivo
 * @return Content-Type apropiado
 */
const char* http_content_type_from_file(const char *filename);

/**
 * Validar que un path es seguro (no tiene ../ ni otras vulnerabilidades)
 * 
 * @param path Path a validar
 * @return true si es seguro
 */
bool http_is_path_safe(const char *path);

#endif // HTTP_H