# Makefile para HTTP Server Project
# Fase 1: Queue + Utils

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
CORE_SRC = $(SRC_DIR)/core/queue.c

# Todos los sources (sin main.c por ahora)
ALL_SRC = $(UTILS_SRC) $(CORE_SRC)

# ============================================================================
# TESTS
# ============================================================================

TEST_QUEUE_SRC = $(TEST_DIR)/test_queue.c
TEST_STRING_SRC = $(TEST_DIR)/test_string_utils.c

# ============================================================================
# TARGETS PRINCIPALES
# ============================================================================

.PHONY: all clean test coverage help

all: help

help:
	@echo "=========================================="
	@echo "  HTTP Server - Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Targets disponibles:"
	@echo "  make test_queue       - Compilar y ejecutar tests de queue"
	@echo "  make test_string      - Compilar y ejecutar tests de string_utils"
	@echo "  make test             - Ejecutar TODOS los tests"
	@echo "  make coverage         - Generar reporte de cobertura"
	@echo "  make clean            - Limpiar archivos compilados"
	@echo ""

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

# ============================================================================
# EJECUTAR TODOS LOS TESTS
# ============================================================================

test: test_string test_queue
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
	@gcov -r $(CORE_SRC) $(UTILS_SRC) > /dev/null 2>&1
	@echo "Archivos de cobertura:"
	@echo ""
	@for file in $(CORE_SRC) $(UTILS_SRC); do \
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