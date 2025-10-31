[![Ver video en YouTube](https://img.youtube.com/vi/cECXk18jrXY/0.jpg)](https://youtu.be/cECXk18jrXY)

[Ver video en YouTube](https://youtu.be/cECXk18jrXY)

# HTTP Server - Proyecto de Sistemas Operativos

Servidor HTTP/1.0 implementado en C con soporte para múltiples clientes concurrentes, workers especializados por tipo de comando y un sistema de jobs asíncronos. Esta README completa describe cómo compilar y ejecutar el servidor tanto dentro de Docker como en Ubuntu/WSL, documenta todos los endpoints, muestra ejemplos reales y detalla cómo ejecutar las pruebas.

## Índice

- Requisitos
- Compilación y ejecución (Docker)
- Compilación y ejecución (Ubuntu / WSL)
- Estructura del proyecto
- Endpoints (descripción + ejemplos)
- Jobs: uso (submit / status / result / cancel) y ejemplos de polling
- Tests: ejecutar tests individuales y la suite completa
- Generar cobertura
- Troubleshooting y notas de implementación

---

## 📋 Requisitos

- GCC (>= 7)
- make
- curl
- bash (para scripts de test y WSL)
- jq (para formatear JSON en ejemplos)
- (Opcional para benchmarking) wrk, apachebench (ab), bc

En sistemas Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential make curl ca-certificates pkg-config jq
```

Si quieres trabajar dentro de Docker (recomendado para entornos reproducibles) necesitas Docker & docker-compose instalados.

---

## Compilación y ejecución (Docker)

Estos pasos crean un contenedor reproducible que contiene las herramientas necesarias y ejecuta el servidor.

1) Construir la imagen:

```bash
docker compose build
```

2) Levantar el contenedor en background:

```bash
docker compose up -d
```

3) Entrar al contenedor (si quieres compilar/ejecutar allí):

```bash
docker compose exec http-server bash
```

Dentro del contenedor puedes compilar y ejecutar exactamente igual que en Ubuntu:

```bash
# Dentro del contenedor
make clean
make server
./build/http_server 8080
```

Nota: el `docker-compose.yml` del repositorio expone el puerto 8080 por defecto.

---

## Compilación y ejecución (Ubuntu / WSL)

En tu máquina local Ubuntu o en WSL (recomendado para desarrollo):

```bash
# Desde la raíz del repo
make clean
make server
./build/http_server 8080
```

O puedes usar el objetivo Make para facilitar:

```bash
make run   # compila y ejecuta
```

Para pruebas desde la misma máquina (WSL) usa `curl` normal:

```bash
curl "http://localhost:8080/status"
```

Si trabajas en PowerShell y quieres forzar que el cliente curl se ejecute en WSL, antepone `wsl` (No recomendado):

```powershell
wsl curl "http://localhost:8080/status"
```

---

## Estructura del proyecto (resumen)

- `src/` - código fuente (server, router, comandos, core)
- `data/` - datos de prueba y jobs persistidos
- `scripts/` - scripts de test y utilidades
- `build/` - binarios compilados
- `tests/` - tests unitarios
- `Makefile` - objetivos de compilación y test

---

## Endpoints principales (descripción y ejemplos)

Todos los ejemplos usan `curl`. Reemplaza `localhost:8080` por la dirección donde corre tu servidor (opcional).

### 1) Básicos

- `/fibonacci?num=N`
  - Descripción: Calcula el N-ésimo número de la secuencia de Fibonacci. Útil para demostraciones con números pequeños.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/fibonacci?num=10" | jq '.'
```

- `/createfile?name=filename&content=text&repeat=x`
  - Descripción: Crea un archivo con el contenido especificado, repetido x veces.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/createfile?name=test.txt&content=hello&repeat=3" | jq '.'
```

- `/deletefile?name=filename`
  - Descripción: Elimina un archivo especificado del servidor.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/deletefile?name=test.txt" | jq '.'
```

- `/status`
  - Descripción: Muestra el estado general del servidor incluyendo uptime, PID y estadísticas.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/status" | jq '.'
```

- `/reverse?text=abcdef`
  - Descripción: Invierte el texto proporcionado.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/reverse?text=abcdef" | jq '.'
```

- `/toupper?text=abcd`
  - Descripción: Convierte el texto a mayúsculas.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/toupper?text=hello%20world" | jq '.'
```

- `/random?count=n&min=a&max=b`
  - Descripción: Genera n números aleatorios entre min y max.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/random?count=5&min=1&max=100"
```

- `/timestamp`
  - Descripción: Devuelve la marca de tiempo actual del servidor.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/timestamp" | jq '.'
```

- `/hash?text=someinput`
  - Descripción: Calcula el hash SHA-256 del texto proporcionado.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/hash?text=hello" | jq '.'
```

- `/simulate?seconds=s&task=name`
  - Descripción: Simula una tarea que toma s segundos en completarse.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/simulate?seconds=5&task=cpu" | jq '.'
```

- `/sleep?seconds=s`
  - Descripción: Hace que el servidor espere s segundos antes de responder.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/sleep?seconds=3" | jq '.'
```

- `/loadtest?tasks=n&sleep=x`
  - Descripción: Ejecuta n tareas simultáneas, cada una durmiendo x segundos.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/loadtest?tasks=5&sleep=2" | jq '.'
```

- `/help`
  - Descripción: Muestra la lista de todos los endpoints disponibles.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/help" | jq "."
```

### 2) CPU-bound 

- `/isprime?n=NUM`
  - Descripción: Comprueba si NUM es primo. Implementación por división hasta √n o (configurable) Miller–Rabin para números grandes.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/isprime?n=1000003" | jq '.'
```

- `/factor?n=NUM`
  - Descripción: Factoriza NUM en primos. Devuelve un arreglo con los factores y sus multiplicidades: [{"prime":p, "count":k}, ...].
  - Ejemplo:
```bash
curl -s "http://localhost:8080/factor?n=360"
```

- `/pi?digits=D`
  - Descripción: Calcula π con un algoritmo tipo Spigot o Chudnovsky (versión iterativa y con control de tiempo para evitar bloqueos largos). No usar D excesivamente grande en entornos limitados.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/pi?digits=10" | jq '.'
```

- `/mandelbrot?width=W&height=H&max_iter=I`
  - Descripción: Genera un mapa de iteraciones del conjunto de Mandelbrot de tamaño W×H; devuelve una matriz de enteros (número de iteraciones por píxel) en JSON. Opcionalmente el servidor puede volcar una imagen PGM/PPM a disco y devolver el nombre del archivo.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/mandelbrot?width=80&height=60&max_iter=100" | jq '.'
```

- `/matrixmul?size=N&seed=S`
  - Descripción: Genera dos matrices N×N pseudoaleatorias (con seed S), calcula su multiplicación y devuelve el hash SHA-256 del resultado para verificación (evita enviar matrices grandes por la red).
  - Ejemplo:
```bash
curl -s "http://localhost:8080/matrixmul?size=128&seed=42" | jq '.'
```

### 3) IO-bound (archivos)

- `/sortfile?name=FILE&algo=merge|quick`
  - Descripción: Ordena un archivo en disco que contiene enteros (uno por línea). Diseñado para manejar archivos grandes (>= 50MB). Retorna HTTP 200 con métricas de tiempo y ruta al archivo ordenado.
  - Ejemplo:
```bash
curl 'http://127.0.0.1:8080/createfile?name=numbers.txt&content=42%0A15%0A8%0A23%0A4%0A&repeat=1' | jq '.'
curl -i 'http://127.0.0.1:8080/sortfile?name=numbers.txt&algo=merge'
curl -i 'http://127.0.0.1:8080/sortfile?name=numbers.txt&algo=quick'
```

- `/wordcount?name=FILE`
  - Descripción: Similar a wc: cuenta líneas, palabras y bytes del archivo especificado. Soporta procesamiento en streaming para archivos grandes.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/wordcount?name=test.txt" | jq '.' 
```

- `/grep?name=FILE&pattern=REGEX`
  - Descripción: Busca coincidencias de la expresión regular en el archivo; devuelve el número total de coincidencias y las primeras 10 líneas que coinciden (si existen).
  - Ejemplo:
```bash
curl -s "http://localhost:8080/grep?name=data/test.txt&pattern=ERROR" | jq '.'
```

- `/compress?name=FILE&codec=gzip|xz`
  - Descripción: Comprime el archivo indicado usando gzip o xz. Devuelve el nombre del archivo comprimido y el tamaño en bytes.
  - Ejemplo:
```bash
curl -s "http://localhost:8080/compress?name=test.txt&codec=gzip" | jq '.'

# O si prefieres usar xz:
curl -s "http://localhost:8080/compress?name=test.txt&codec=xz" | jq '.'
```

- `/hashfile?name=FILE&algo=sha256`
  - Descripción: Calcula el hash del archivo y retorna el resultado en hexadecimal. Diseñado para calcular el hash en streaming para archivos grandes.
  - Ejemplo:
```bash
curl -s "http://127.0.0.1:8080/hashfile?name=test.txt&algo=sha256" | jq '.' 
```

---

## Jobs (tareas largas)

Las rutas CPU/IO soportan ejecución directa y también vía Jobs para tareas largas. Use `/jobs/*` para manejo asíncrono.

- `/jobs/submit?task=TASK&<params>`
  - Encola un job y retorna `{ "job_id": "...", "status": "queued" }`.
  - Ejemplo (simulate 10s CPU):

```bash
curl -s "http://localhost:8080/jobs/submit?task=simulate&seconds=10&task=cpu" | jq '.'
```

> Nota: el parámetro `task` en `/jobs/submit` indica la ruta que se ejecutará (por ejemplo `simulate`, `isprime`, `sortfile`, etc.). Los parámetros dependientes del comando deben incluirse también (p. ej. `seconds=10`).

- `/jobs/status?id=JOBID` → `{ "status": "queued|running|done|error|canceled", "progress":0..100, "eta_ms":... }`

Ejemplo de polling (bash):

```bash
JOB_JSON=$(curl -s "http://localhost:8080/jobs/submit?task=simulate&seconds=10&task=cpu")
JOBID=$(echo "$JOB_JSON" | sed -E 's/.*"job_id":"([^"]+)".*/\1/')
for i in $(seq 1 12); do
  curl -s "http://localhost:8080/jobs/status?id=$JOBID" | jq '.'
  sleep 1
done
```

- `/jobs/result?id=JOBID` → resultado JSON del comando si `done`, o `{ "error": "..." }` si falló.

```bash
curl -s "http://localhost:8080/jobs/result?id=$JOBID" | jq '.'
```

- `/jobs/cancel?id=JOBID` → intenta cancelar; retorna `{"status":"canceled"}` o `{"status":"not_cancelable"}`.

```bash
curl -s "http://localhost:8080/jobs/cancel?id=$JOBID" | jq '.'
```

---

## Tests (unitarios & integración)

El proyecto incluye tests unitarios (carpeta `tests/`) y scripts para integrar pruebas.

### Compilar y ejecutar tests individuales

```bash
make test_queue        # Tests de cola thread-safe
make test_string       # Tests de string utils
make test_job_manager
make test_worker_pool
make test_http         # Tests de HTTP parser
make test_metrics
make test_race_conditions
```

Cada target compila y ejecuta el test correspondiente. Si falla alguno, el `Makefile` devuelve código de error y muestra el log en consola.

### Ejecutar todos los tests (suite completa)

```bash
make test   # compila y ejecuta los tests unitarios listados en el Makefile
```

O bien usar el orquestador:

```bash
make test-all
# o
bash scripts/run_all_tests.sh
```

### Tests de integración y carga

- `scripts/test_cpu_bound_auto.sh` - ejecuta pruebas CPU-bound.
- `scripts/test_io_bound.sh` - genera archivos grandes y prueba endpoints IO-bound (`/sortfile`, `/compress`, `/hashfile`).
- `scripts/test_race_conditions_integration.sh` - tests de concurrencia y race conditions.

### Cobertura

Generar cobertura con gcov (Makefile ya tiene objetivos):

```bash
make coverage
make coverage-html
```

Los artefactos de cobertura se colocarán en `coverage/`.

---

## Comprobación de `progress` y `eta_ms` en Jobs

1. `/jobs/submit` devuelve `job_id`.
2. Haz polling con `/jobs/status?id=JOBID` cada segundo. Debes ver `progress` incrementando (0 → 100) y `eta_ms` decreciendo hasta 0.

Si `eta_ms` queda en `-1` o `progress` permanece en 100 inmediatamente, revisa:

- Que el `task` que ejecutaste reciba correctamente sus parámetros (`/jobs/submit?task=simulate&seconds=10&task=cpu`). Evita usar el mismo key `task` dos veces en el query string (p. ej. `task=simulate&task=cpu` causará conflicto).
- Que el handler del comando invoque `job_update_progress(job_id, progress, eta_ms)` periódicamente (p.ej. `simulate.c`).

---

## Debugging y troubleshooting

- Puerto ocupado

```bash
lsof -ti:8080 | xargs kill -9
```

- Logs del servidor

El servidor imprime logs en stderr; si lo ejecutas desde systemd/container redirige la salida a archivo.

- Tests fallan

Recompila y ejecuta el test problemático en modo verbose. Usa Valgrind si sospechas fugas de memoria.

```bash
make clean
make server
./build/test_job_manager
valgrind --leak-check=full ./build/test_job_manager
```