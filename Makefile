# Makefile para HTTP Server Project

CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2 -g -Isrc
LDFLAGS = -lpthread -lm -lssl -lcrypto
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage

# Directorios
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = tests

# Colores para output
GREEN = \033[0;32m
RED = \033[0;31m
BLUE = \033[0;34m
NC = \033[0m # No Color

# ============================================================================
# ARCHIVOS FUENTE
# ============================================================================

# Utilidades
UTILS_SRC = $(SRC_DIR)/utils/logger.c \
            $(SRC_DIR)/utils/string_utils.c \
            $(SRC_DIR)/utils/timer.c \
            $(SRC_DIR)/utils/uuid.c

# Basic Commands
BASIC_COMMANDS_SRC = $(SRC_DIR)/commands/basic/fibonacci.c \
					 $(SRC_DIR)/commands/basic/reverse.c \
					 $(SRC_DIR)/commands/basic/hash.c \
					 $(SRC_DIR)/commands/basic/random.c \
					 $(SRC_DIR)/commands/basic/reverse.c \
                     $(SRC_DIR)/commands/basic/simulate.c \
                     $(SRC_DIR)/commands/basic/sleep_cmd.c \
                     $(SRC_DIR)/commands/basic/loadtest.c \
                     $(SRC_DIR)/commands/basic/help.c

# CPU-bound Commands
CPU_COMMANDS_SRC =  $(SRC_DIR)/commands/cpu_bound/isprime.c \
					$(SRC_DIR)/commands/cpu_bound/factor.c \
					$(SRC_DIR)/commands/cpu_bound/pi.c \
                	$(SRC_DIR)/commands/cpu_bound/mandelbrot.c \
                	$(SRC_DIR)/commands/cpu_bound/matrixmul.c

# IO-bound Commands
IO_COMMANDS_SRC = $(SRC_DIR)/commands/io_bound/sortfile.c \
                 $(SRC_DIR)/commands/io_bound/wordcount.c \
                 $(SRC_DIR)/commands/io_bound/hashfile.c \
                 $(SRC_DIR)/commands/io_bound/compress.c \
                 $(SRC_DIR)/commands/io_bound/grep.c

# File Commands
FILE_COMMANDS_SRC = $(SRC_DIR)/commands/files/createfile.c \
                   $(SRC_DIR)/commands/files/deletefile.c

# Core (Queue)
CORE_SRC = $(SRC_DIR)/core/queue.c \
		   $(SRC_DIR)/core/worker_pool.c \
		   $(SRC_DIR)/core/job_manager.c \
		   $(SRC_DIR)/core/job_executor.c \
		   $(SRC_DIR)/core/metrics.c

# Server (HTTP + TCP)
SERVER_SRC = $(SRC_DIR)/server/http.c \
			 $(SRC_DIR)/server/server.c \
			 $(SRC_DIR)/router/router.c

# Todos los sources (sin main.c por ahora)
ALL_SRC = $(UTILS_SRC) $(CORE_SRC) $(SERVER_SRC) $(BASIC_COMMANDS_SRC) $(CPU_COMMANDS_SRC) $(IO_COMMANDS_SRC) $(FILE_COMMANDS_SRC)

# Main
MAIN_SRC = $(SRC_DIR)/main.c

# ============================================================================
# TESTS
# ============================================================================

TEST_QUEUE_SRC = $(TEST_DIR)/test_queue.c
TEST_STRING_SRC = $(TEST_DIR)/test_string_utils.c
TEST_JOB_MANAGER_SRC = $(TEST_DIR)/test_job_manager.c
TEST_WORKER_POOL_SRC = $(TEST_DIR)/test_worker_pool.c
TEST_HTTP_SRC = $(TEST_DIR)/test_http_parser.c
TEST_METRICS_SRC = $(TEST_DIR)/test_metrics.c
TEST_RACE_CONDITIONS_SRC = $(TEST_DIR)/test_race_conditions.c 

# ============================================================================
# TARGETS PRINCIPALES
# ============================================================================

.PHONY: all clean test coverage help server

all: server

help:
	@echo "=========================================="
	@echo "  HTTP Server - Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Targets disponibles:"
	@echo "  $(GREEN)make server$(NC)             - Compilar servidor HTTP"
	@echo "  $(GREEN)make run$(NC)                - Compilar y ejecutar servidor"
	@echo ""
	@echo "  $(BLUE)Tests:$(NC)"
	@echo "  $(GREEN)make test_queue$(NC)         - Tests de queue"
	@echo "  $(GREEN)make test_string$(NC)        - Tests de string_utils"
	@echo "  $(GREEN)make test_worker_pool$(NC)   - Tests de worker pool"
	@echo "  $(GREEN)make test_job_manager$(NC)   - Tests de job manager"
	@echo "  $(GREEN)make test_http$(NC)          - Tests de HTTP parser"
	@echo "  $(GREEN)make test_metrics$(NC)       - Tests de sistema de métricas"
	@echo "  $(GREEN)make test_race_conditions$(NC) - Tests de concurrencia"
	@echo "  $(GREEN)make test$(NC)               - Ejecutar TODOS los tests"
	@echo ""
	@echo "  $(BLUE)Cobertura:$(NC)"
	@echo "  $(GREEN)make coverage$(NC)           - Reporte de cobertura (texto)"
	@echo "  $(GREEN)make coverage-html$(NC)      - Reporte de cobertura (HTML)"
	@echo "  $(GREEN)make clean-coverage$(NC)     - Limpiar archivos de cobertura"
	@echo ""
	@echo "  $(GREEN)make clean$(NC)              - Limpiar archivos compilados"
	@echo ""


# ============================================================================
# SERVIDOR PRINCIPAL
# ============================================================================

server: $(BUILD_DIR)/http_server

$(BUILD_DIR)/http_server: $(MAIN_SRC) $(ALL_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "$(BLUE)Compilando servidor HTTP...$(NC)"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "$(GREEN)✅ Servidor compilado: ./$(BUILD_DIR)/http_server$(NC)"
	@echo ""

run: server
	@echo "$(BLUE)=========================================$(NC)"
	@echo "$(BLUE)  Iniciando Servidor HTTP$(NC)"
	@echo "$(BLUE)=========================================$(NC)"
	@./$(BUILD_DIR)/http_server 8080

# ============================================================================
# TESTS INDIVIDUALES
# ============================================================================

test_queue: $(BUILD_DIR)/test_queue
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Ejecutando Tests de Queue$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_queue
	@if [ $$? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de Queue: PASSED$(NC)"; \
	else \
		echo "$(RED)❌ Tests de Queue: FAILED$(NC)"; \
		exit 1; \
	fi

test_string: $(BUILD_DIR)/test_string_utils
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Ejecutando Tests de String Utils$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_string_utils
	@if [ $$? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de String Utils: PASSED$(NC)"; \
	else \
		echo "$(RED)❌ Tests de String Utils: FAILED$(NC)"; \
		exit 1; \
	fi

test_worker_pool: $(BUILD_DIR)/test_worker_pool
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Ejecutando Tests de Worker Pool$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_worker_pool
	@if [ $$? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de Worker Pool: PASSED$(NC)"; \
	else \
		echo "$(RED)❌ Tests de Worker Pool: FAILED$(NC)"; \
		exit 1; \
	fi

test_job_manager: $(BUILD_DIR)/test_job_manager
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Ejecutando Tests de Job Manager$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_job_manager
	@if [ $$? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de Job Manager: PASSED$(NC)"; \
	else \
		echo "$(RED)❌ Tests de Job Manager: FAILED$(NC)"; \
		exit 1; \
	fi

test_http: $(BUILD_DIR)/test_http_parser
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Ejecutando Tests de HTTP Parser$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_http_parser
	@if [ $$? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de HTTP Parser: PASSED$(NC)"; \
	else \
		echo "$(RED)❌ Tests de HTTP Parser: FAILED$(NC)"; \
		exit 1; \
	fi

test_metrics: $(BUILD_DIR)/test_metrics
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Ejecutando Tests de Metrics$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_metrics
	@if [ $$? -eq 0 ]; then \
	    echo "$(GREEN)✅ Tests de Metrics: PASSED$(NC)"; \
	else \
	    echo "$(RED)❌ Tests de Metrics: FAILED$(NC)"; \
	    exit 1; \
	fi

test_race_conditions: $(BUILD_DIR)/test_race_conditions
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  Test de Race Conditions$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@./$(BUILD_DIR)/test_race_conditions
	@if [ $$? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de Race Conditions: PASSED$(NC)"; \
	else \
	    echo "$(RED)❌ Tests de Race Conditions: FAILED$(NC)"; \
	    exit 1; \
	fi




# ============================================================================
# COMPILACIÓN DE TESTS
# ============================================================================

# Dependencias completas para tests que usan el sistema completo
FULL_TEST_DEPS = $(UTILS_SRC) $(CORE_SRC) $(SERVER_SRC) $(BASIC_COMMANDS_SRC) $(CPU_COMMANDS_SRC) $(IO_COMMANDS_SRC) $(FILE_COMMANDS_SRC)

$(BUILD_DIR)/test_queue: $(TEST_QUEUE_SRC) $(FULL_TEST_DEPS)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_queue..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_string_utils: $(TEST_STRING_SRC) $(UTILS_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_string_utils..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_worker_pool: $(TEST_WORKER_POOL_SRC) $(FULL_TEST_DEPS)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_worker_pool..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_job_manager: $(TEST_JOB_MANAGER_SRC) $(FULL_TEST_DEPS)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_job_manager..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_http_parser: $(TEST_HTTP_SRC) $(FULL_TEST_DEPS)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_http_parser..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov

$(BUILD_DIR)/test_metrics: $(TEST_METRICS_SRC) $(FULL_TEST_DEPS)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_metrics..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov
$(BUILD_DIR)/test_race_conditions: $(TEST_RACE_CONDITIONS_SRC) $(FULL_TEST_DEPS)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_race_conditions..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov

# ============================================================================
# EJECUTAR TODOS LOS TESTS
# ============================================================================

test: test_string test_queue test_worker_pool test_job_manager test_http test_metrics test_race_conditions
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  🎉 TODOS LOS TESTS PASARON$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@echo ""

# ============================================================================
# COBERTURA DE CÓDIGO
# ============================================================================

coverage: test
	@bash scripts/coverage.sh

coverage-html: test
	@bash scripts/coverage_html.sh

clean-coverage:
	@echo "Limpiando archivos de cobertura..."
	@rm -rf coverage/
	@rm -f *.gcda *.gcno *.gcov
	@echo "$(GREEN)✅ Archivos de cobertura eliminados$(NC)"

# ============================================================================
# PRUEBAS DE CARGA
# ============================================================================

test_cpu: server
	@echo ""
	@echo "$(BLUE)=========================================$(NC)"
	@echo "$(BLUE)  Pruebas de CPU-Bound$(NC)"
	@echo "$(BLUE)=========================================$(NC)"
	@echo ""
	@echo "Asegúrate de que el servidor esté corriendo:"
	@echo "  Terminal 1: make run"
	@echo "  Terminal 2: make test_cpu"
	@echo ""
	@bash scripts/test_cpu_bound_auto.sh

.PHONY: test_cpu

# ============================================================================
# BENCHMARKING Y METRICAS
# ============================================================================

benchmark: server
	@echo ""
	@echo "$(BLUE)=========================================$(NC)"
	@echo "$(BLUE)  Benchmark de Metricas$(NC)"
	@echo "$(BLUE)=========================================$(NC)"
	@echo ""
	@bash scripts/benchmark_metrics.sh

install-bench-tools:
	@echo "Instalando herramientas de benchmark..."
	@apt-get update
	@apt-get install -y apache2-utils bc wrk
	@echo "$(GREEN)✓ Herramientas instaladas$(NC)"

.PHONY: benchmark install-bench-tools




# ============================================================================
# LIMPIEZA
# ============================================================================

clean:
	@echo "Limpiando archivos compilados..."
	@rm -rf $(BUILD_DIR)/*
	@rm -rf coverage/
	@rm -f *.gcda *.gcno *.gcov
#	@rm -f $(SRC_DIR)/**/*.gcda $(SRC_DIR)/**/*.gcno
	@echo "$(GREEN)✅ Limpieza completada$(NC)"

# ============================================================================
# TARGETS DE DEBUG
# ============================================================================

# Mostrar variables del Makefile
debug:
	@echo "UTILS_SRC: $(UTILS_SRC)"
	@echo "CORE_SRC: $(CORE_SRC)"
	@echo "TEST_QUEUE_SRC: $(TEST_QUEUE_SRC)"
	@echo "SERVER_SRC: $(SERVER_SRC)"
	@echo "ALL_SRC: $(ALL_SRC)"