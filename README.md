# HTTP Server - Proyecto de Sistemas Operativos

Servidor HTTP/1.0 con soporte para múltiples clientes concurrentes, workers especializados y sistema de jobs asíncronos.

## 📋 Requisitos

- GCC >= 7.0
- Make
- pthreads (incluido en sistemas Unix-like)
- gcov (para cobertura de código)

```bash
# Verificar instalación
gcc --version
make --version
```

## 🚀 Compilación y Ejecución

### Fase 1: Tests de Queue (Estado Actual)
``` bash
# 1. Construir imagen (solo la primera vez)
docker compose build

# 2. Iniciar contenedor
docker compose up -d

# 3. Entrar al contenedor
docker compose exec http-server bash

# 4. Dentro del contenedor, ejecutar como siempre:
make clean
make test_queue
make test_string
make test
make coverage
```

## 📊 Salida Esperada

```
$ make test_queue

=========================================
  Ejecutando Tests de Queue
=========================================

📦 Test Suite: Queue (Thread-Safe)
==========================================
  Running: test_queue_create ... ✅ PASS
  Running: test_enqueue_dequeue_single ... ✅ PASS
  Running: test_concurrent_producer_consumer ... ✅ PASS
  ...

📊 TEST SUMMARY
==========================================
Total Assertions: 87
✅ Passed: 87
❌ Failed: 0
Pass Rate: 100.00%
🎉 ALL TESTS PASSED!

✅ Tests de Queue: PASSED
```

## 📁 Estructura Actual del Proyecto

```
http-server/
├── src/
│   ├── core/
│   │   ├── queue.h          ✅ Implementado
│   │   └── queue.c          ✅ Implementado
│   └── utils/
│       ├── utils.h          ✅ Implementado
│       ├── logger.c         ✅ Implementado
│       ├── string_utils.c   ✅ Implementado
│       ├── timer.c          ✅ Implementado
│       └── uuid.c           ✅ Implementado
├── tests/
│   ├── test_utils.h         ✅ Implementado
│   ├── test_queue.c         ✅ Implementado
│   ├── test_string_utils.c  ✅ Implementado
│   └── test_main.c          ✅ Implementado
├── Makefile                 ✅ Implementado
└── README.md                ✅ Este archivo
```

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

## 🎯 Próximos Pasos

### Fase 2: Worker Pool
- [ ] `src/core/worker_pool.h`
- [ ] `src/core/worker_pool.c`
- [ ] `tests/test_worker_pool.c`

### Fase 3: HTTP Server Base
- [ ] `src/server/http.h`
- [ ] `src/server/http.c`
- [ ] `src/server/server.h`
- [ ] `src/server/server.c`

### Fase 4: Router y Comandos
- [ ] `src/router/router.c`
- [ ] `src/commands/basic_commands.c`
- [ ] `src/commands/cpu_bound/isprime.c`

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

## 📈 Cobertura de Código

Meta del proyecto: **≥ 90%**

```bash
make coverage
```

Ejemplo de salida:
```
Generando Reporte de Cobertura
==========================================

Archivos de cobertura:
  ✅ queue.c: 94%
  ✅ string_utils.c: 92%
  ✅ logger.c: 88%
  ✅ timer.c: 100%
  ✅ uuid.c: 100%
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

