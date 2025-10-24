# Makefile para HTTP Server Project

CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2 -g -Isrc
LDFLAGS = -lpthread -lm 
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

# Core (Queue)
CORE_SRC = $(SRC_DIR)/core/queue.c \
		   $(SRC_DIR)/core/worker_pool.c \
		   $(SRC_DIR)/core/job_manager.c

# Server (HTTP + TCP)
SERVER_SRC = $(SRC_DIR)/server/http.c \
			 $(SRC_DIR)/server/server.c \
			 $(SRC_DIR)/router/router.c

# Todos los sources (sin main.c por ahora)
ALL_SRC = $(UTILS_SRC) $(CORE_SRC) $(SERVER_SRC)

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
	@echo "  $(GREEN)make server$(NC)          - Compilar servidor HTTP"
	@echo "  $(GREEN)make run$(NC)             - Compilar y ejecutar servidor"
	@echo "  $(GREEN)make test_queue$(NC)      - Tests de queue"
	@echo "  $(GREEN)make test_string$(NC)     - Tests de string_utils"
	@echo "  $(GREEN)make test_http$(NC)       - Tests de HTTP parser"
	@echo "  $(GREEN)make test$(NC)            - Ejecutar TODOS los tests"
	@echo "  $(GREEN)make coverage$(NC)        - Generar reporte de cobertura"
	@echo "  $(GREEN)make clean$(NC)           - Limpiar archivos compilados"
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
	@if [ $? -eq 0 ]; then \
		echo "$(GREEN)✅ Tests de HTTP Parser: PASSED$(NC)"; \
	else \
		echo "$(RED)❌ Tests de HTTP Parser: FAILED$(NC)"; \
		exit 1; \
	fi

# ============================================================================
# COMPILACIÓN DE TESTS
# ============================================================================

$(BUILD_DIR)/test_queue: $(TEST_QUEUE_SRC) $(CORE_SRC) $(UTILS_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_queue..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_string_utils: $(TEST_STRING_SRC) $(UTILS_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_string_utils..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_worker_pool: $(TEST_WORKER_POOL_SRC) $(WORKER_POOL_SRC) $(CORE_SRC) $(UTILS_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_worker_pool..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_job_manager: $(TEST_JOB_MANAGER_SRC) $(CORE_SRC) $(UTILS_SRC) $(SRC_DIR)/core/job_manager.c
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_job_manager..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov 

$(BUILD_DIR)/test_http_parser: $(TEST_HTTP_SRC) $(SERVER_SRC) $(UTILS_SRC)
	@mkdir -p $(BUILD_DIR)
	@echo "Compilando test_http_parser..."
	@$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -o $@ $^ $(LDFLAGS) -lgcov
# ============================================================================
# EJECUTAR TODOS LOS TESTS
# ============================================================================

test: test_string test_queue test_worker_pool test_job_manager test_http
	@echo ""
	@echo "$(GREEN)=========================================$(NC)"
	@echo "$(GREEN)  🎉 TODOS LOS TESTS PASARON$(NC)"
	@echo "$(GREEN)=========================================$(NC)"
	@echo ""

# ============================================================================
# COBERTURA DE CÓDIGO
# ============================================================================

coverage: test
	@echo ""
	@echo "=========================================="
	@echo "  Generando Reporte de Cobertura"
	@echo "=========================================="
	@echo ""
	@gcov -r $(CORE_SRC) $(UTILS_SRC) $(SERVER_SRC) > /dev/null 2>&1
	@echo "Archivos de cobertura:"
	@echo ""
	@for file in $(CORE_SRC) $(UTILS_SRC) $(SERVER_SRC); do \
		base=$$(basename $$file .c); \
		if [ -f $$base.c.gcov ]; then \
			lines=$$(grep -c "####" $$base.c.gcov 2>/dev/null || echo 0); \
			total=$$(grep -cE "^ *[0-9]+" $$base.c.gcov 2>/dev/null || echo 1); \
			covered=$$((total - lines)); \
			percent=$$((covered * 100 / total)); \
			if [ $$percent -ge 90 ]; then \
				echo "  $(GREEN)✅ $$base.c: $$percent%$(NC)"; \
			elif [ $$percent -ge 70 ]; then \
				echo "  ⚠️  $$base.c: $$percent%"; \
			else \
				echo "  $(RED)❌ $$base.c: $$percent%$(NC)"; \
			fi; \
		fi; \
	done
	@echo ""
	@echo "Archivos .gcov generados en el directorio actual"
	@echo "Ver detalles: cat <archivo>.c.gcov"
	@echo ""

# ============================================================================
# LIMPIEZA
# ============================================================================

clean:
	@echo "Limpiando archivos compilados..."
	@rm -rf $(BUILD_DIR)/*
	@rm -f *.gcda *.gcno *.gcov
	@rm -f $(SRC_DIR)/**/*.gcda $(SRC_DIR)/**/*.gcno
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