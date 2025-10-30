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
make run   # compila y ejecuta (foreground)
```

Para pruebas desde la misma máquina (WSL) usa `curl` normal:

```bash
curl "http://localhost:8080/status"
```

Si trabajas en PowerShell y quieres forzar que el cliente curl se ejecute en WSL, antepone `wsl`:

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

- `/status`
  - Descripción: estado general del servidor (uptime, PID, stats)
  - Ejemplo:

```bash
curl -s "http://localhost:8080/status" | jq '.'
```

- `/help`
  - Descripción: listado de endpoints.

```bash
curl -s "http://localhost:8080/help"
```

- `/fibonacci?num=N`
  - Calcula N-ésimo número de Fibonacci (pequeños N para demo).

```bash
curl -s "http://localhost:8080/fibonacci?num=10" | jq '.'
```

- `/reverse?text=...` y `/toupper?text=...`

```bash
curl -s "http://localhost:8080/reverse?text=abcdef" | jq '.'
curl -s "http://localhost:8080/toupper?text=hola%20mundo" | jq '.'
```

### 2) Utilidades

- `/hash?text=...` → SHA-256 of text

```bash
curl -s "http://localhost:8080/hash?text=hello" | jq '.'
```

- `/random?count=n&min=a&max=b`

### 3) CPU-bound (intenso)

- `/isprime?n=NUM`

```bash
curl -s "http://localhost:8080/isprime?n=1000003" | jq '.'
```

- `/factor?n=NUM`
- `/pi?digits=D` (intenso; cuidado con valores grandes)
- `/mandelbrot?width=W&height=H&max_iter=I`
- `/matrixmul?size=N&seed=S` → devuelve SHA-256 del resultado

### 4) IO-bound (archivos)

- `/sortfile?name=FILE&algo=merge|quick`
  - Ordena un archivo con números (uno por línea). Debe soportar archivos grandes (>=50MB).

```bash
curl -s "http://localhost:8080/sortfile?name=data/numbers_50mb.txt&algo=merge" | jq '.'
```

- `/wordcount?name=FILE`
- `/grep?name=FILE&pattern=REGEX`

```bash
curl -s "http://localhost:8080/grep?name=data/test.txt&pattern=ERROR" | jq '.'
```

- `/compress?name=FILE&codec=gzip|xz`
- `/hashfile?name=FILE&algo=sha256`

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