# HTTP Server - Proyecto de Sistemas Operativos

Servidor HTTP/1.0 con soporte para múltiples clientes concurrentes, workers especializados y sistema de jobs asíncronos.

## 📋 Requisitos

- GCC >= 7.0
- Make
- pthreads (incluido en sistemas Unix-like)
- gcov (para cobertura de código)
- curl (para pruebas)

```bash
# Verificar instalación
gcc --version
make --version
curl --version
```

## 🚀 Compilación y Ejecución


```bash
# 1. Construir imagen (solo la primera vez)
docker compose build

# 2. Iniciar contenedor
docker compose up -d

# 3. Entrar al contenedor
docker compose exec http-server bash

# 4. Compilar el servidor
make server

# 5. Ejecutar el servidor (puerto 8080 por defecto)
make run

# O manualmente:
./build/http_server 8080

# 6. En otra terminal, probar con curl
curl http://localhost:8080/status
curl http://localhost:8080/help

Tambien se puede probar 

# BASIC

status
curl -i 'http://127.0.0.1:8080/status'

fibonacci
curl -i 'http://127.0.0.1:8080/fibonacci?num=3'

hash
curl -i 'http://127.0.0.1:8080/hash?text=someinput'

random
curl -i 'http://127.0.0.1:8080/random?count=55&min=1&max=10'

reverse
curl -i 'http://127.0.0.1:8080/reverse?text=abcdef'

toupper (percent-encoded space)
curl -i 'http://127.0.0.1:8080/toupper?text=hola%20mundo'

timestamp
curl -i 'http://127.0.0.1:8080/timestamp'

submit job (example)
curl -i 'http://127.0.0.1:8080/jobs/submit?task=isprime&n=97'

# CPU BOUND

isprime

factor
curl -i 'http://127.0.0.1:8080/factor?n=60'

# IO BOUND

sortfile
curl 'http://127.0.0.1:8080/createfile?name=numbers.txt&content=42%0A15%0A8%0A23%0A4%0A&repeat=1'
curl -i 'http://127.0.0.1:8080/sortfile?name=numbers.txt&algo=merge'
curl -i 'http://127.0.0.1:8080/sortfile?name=numbers.txt&algo=quick'

wordcount
curl -i 'http://127.0.0.1:8080/wordcount?name=test.txt'

hashfile
curl -i 'http://127.0.0.1:8080/hashfile?name=test.txt&algo=sha256'

# FILES

createfile
curl -i 'http://127.0.0.1:8080/createfile?name=test.txt&content=Hello&repeat=3'

deletefile
curl -i 'http://127.0.0.1:8080/deletefile?name=test.txt'


```

### Salida Esperada del Servidor

```
[2025-01-15 10:30:00] [INFO ] [main.c:45] ===========================================
[2025-01-15 10:30:00] [INFO ] [main.c:46]   HTTP Server v1.0 - Phase 3
[2025-01-15 10:30:00] [INFO ] [main.c:47] ===========================================
[2025-01-15 10:30:00] [INFO ] [main.c:48] PID: 12345
[2025-01-15 10:30:00] [INFO ] [server.c:89] Server initialized on port 8080
[2025-01-15 10:30:00] [INFO ] [server.c:102] Server starting... PID: 12345
[2025-01-15 10:30:00] [INFO ] [main.c:68] Server ready. Press Ctrl+C to stop.
[2025-01-15 10:30:00] [INFO ] [main.c:69] Try: curl http://localhost:8080/status
```

### Endpoints Disponibles (Fase 3)

| Endpoint | Descripción | Ejemplo |
|----------|-------------|---------|
| `/status` | Estado del servidor | `curl http://localhost:8080/status` |
| `/help` | Lista de endpoints | `curl http://localhost:8080/help` |

Respuesta de `/status`:
```json
{
  "status": "running",
  "pid": 12345,
  "uptime_seconds": 42,
  "connections_served": 10,
  "requests_ok": 8,
  "requests_error": 2
}
```


## 🧪 Tests Unitarios

```bash
# 1. Ejecutar tests individuales
make test_queue         # Tests de cola thread-safe
make test_string        # Tests de string utils
make test_job_manager
make test_worker_pool
make test_http          # Tests de HTTP parser
make test_metrics

# 2. Ejecutar TODOS los tests
make test

# 3. Generar reporte de cobertura
make coverage

# 4. Limpiar archivos compilados
make clean
```



## 🔧 Pruebas Manuales con curl

```bash
# Test básico
curl http://localhost:8080/status

# Ver headers de respuesta
curl -v http://localhost:8080/status

# Test con query string
curl "http://localhost:8080/status?test=1"

# Ver solo headers
curl -I http://localhost:8080/status

# Test de error 404
curl http://localhost:8080/notfound

# Test de path inseguro (debe retornar 400)
curl "http://localhost:8080/../etc/passwd"

# Múltiples requests concurrentes
for i in {1..10}; do curl -s http://localhost:8080/status & done
```

### Script de Prueba Automatizado

```bash
# Hacer el script ejecutable
chmod +x scripts/test_server.sh

# Ejecutar (el servidor debe estar corriendo)
./scripts/test_server.sh 8080
```

## 📊 Cobertura de Código

Meta del proyecto: **≥ 90%**

```bash
# Limpiar
make clean

# Ejecutar tests
make test

# Generar cobertura
make coverage
```

## 📊 Pruebas de Rendimiento
Meta del proyecto: **≥ 90%**

```bash
#1. El servidor debe estar corriendo
#2. Instalar lo siguiente en terminal
apt-get update && apt-get install -y netcat-openbsd

# Limpiar
make clean

# Ejecutar tests
make test_cpu


```

## 🐛 Troubleshooting

### El servidor no inicia

```bash
# Error: Address already in use
# Solución: Matar proceso que usa el puerto
lsof -ti:8080 | xargs kill -9

# O usar otro puerto
./build/http_server 8081
```

### Tests fallan

```bash
# Ver detalles del error
./build/test_http_parser

# Ejecutar con valgrind (memory leaks)
valgrind --leak-check=full ./build/test_http_parser
```

### No se puede compilar

```bash
# Limpiar y recompilar
make clean
make server

# Ver errores detallados
make server 2>&1 | less
```

## 📖 Características Implementadas (Fase 3)

### ✅ Cola Thread-Safe
- FIFO con mutex + condition variables
- Backpressure (rechaza si está llena)
- Timeouts configurables
- Graceful shutdown
- Métricas (encolado/desencolado/rechazado)

### ✅ Parser HTTP/1.0
- Parsea request line (método, path, query, versión)
- Parsea headers (Host, Content-Length, Connection)
- Separa path y query string
- Validación de paths seguros (no permite ../)

### ✅ Servidor TCP
- Accept loop con threads por conexión
- Timeout en lectura de requests
- Graceful shutdown (SIGINT, SIGTERM)
- Estadísticas (conexiones, bytes, uptime)
- Logging estructurado

### ✅ Response Builder
- Construye responses HTTP/1.0 correctas
- Headers automáticos (Content-Length, X-Request-Id, etc.)
- Soporte para JSON
- Manejo de errores (400, 404, 500, 503)

## 📝 Notas de Implementación

### Concurrencia
- **1 thread por conexión** (pthread_create + detached)
- **No bloquea el accept loop**
- **Thread-safe** en estadísticas (mutex)

### HTTP/1.0
- **Connection: close** por defecto
- **No keep-alive** (simplifica implementación)
- **Máximo 8KB** por request

### Logging
- **Thread-safe** con mutex
- **Timestamps automáticos**
- **Niveles**: DEBUG, INFO, WARN, ERROR
- **Salida**: stderr o archivo

## 🧪 Tests Implementados

### Queue (test_queue.c)
- ✅ Crear y destruir cola
- ✅ Encolar y desencolar tareas
- ✅ Backpressure (cola llena)
- ✅ Timeout en enqueue/dequeue
- ✅ Shutdown graceful
- ✅ Múltiples productores concurrentes
- ✅ Productores y consumidores concurrentes
- ✅ Test de stress (10 prod + 10 cons, 500 tareas)

### String Utils (test_string_utils.c)
- ✅ URL decode (%20, +, caracteres especiales)
- ✅ Trim whitespace
- ✅ Split strings
- ✅ Parse query strings
- ✅ Obtener parámetros como int/long


## 🐛 Debugging

### Si los tests fallan:

```bash
# Ver detalles de errores
./build/test_queue

# Ejecutar con valgrind (memory leaks)
valgrind --leak-check=full ./build/test_queue

# Ver archivos de cobertura
make coverage
cat queue.c.gcov
```

### Errores comunes:

**Error: `pthread_create` undefined**
```bash
# Asegúrate de compilar con -lpthread
gcc ... -lpthread
```

**Error: `clock_gettime` undefined**
```bash
# Asegúrate de compilar con -lrt (algunas distribuciones)
gcc ... -lrt
```



## 📝 Notas de Implementación

### Queue Thread-Safe

La cola implementa:
- **FIFO** (First In, First Out)
- **Backpressure**: Rechaza tareas si está llena
- **Timeouts**: Espera configurable en enqueue/dequeue
- **Graceful shutdown**: Señaliza a workers que terminen
- **Métricas**: Cuenta tareas encoladas/desencoladas/rechazadas

Primitivas de sincronización:
- `pthread_mutex_t`: Protege estructura
- `pthread_cond_t not_empty`: Señal para consumidores
- `pthread_cond_t not_full`: Señal para productores

### String Utils

Parser de query strings HTTP:
```c
// Entrada: "n=97&max=100&text=hello+world"
query_params_t *params = parse_query_string(query);

int n = get_query_param_int(params, "n", -1);        // 97
const char *text = get_query_param(params, "text");  // "hello world"

free_query_params(params);
```

