#!/bin/bash

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

RESULTS_DIR="test_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_FILE="$RESULTS_DIR/test_suite_${TIMESTAMP}.log"

mkdir -p "$RESULTS_DIR"

# Variables para tracking
total_tests=0
passed_tests=0
failed_tests=0
skipped_tests=0

# Arrays para almacenar resultados
declare -a test_names
declare -a test_results
declare -a test_times

echo "" | tee -a "$LOG_FILE"
echo "╔════════════════════════════════════════════════════════╗" | tee -a "$LOG_FILE"
echo "║                                                        ║" | tee -a "$LOG_FILE"
echo "║        SUITE COMPLETA DE PRUEBAS - HTTP SERVER        ║" | tee -a "$LOG_FILE"
echo "║                                                        ║" | tee -a "$LOG_FILE"
echo "╚════════════════════════════════════════════════════════╝" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"
echo "Timestamp: $(date)" | tee -a "$LOG_FILE"
echo "Log file: $LOG_FILE" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Función para ejecutar una fase de tests
run_test_phase() {
    local phase_name=$1
    local test_command=$2
    local required=${3:-true}
    
    total_tests=$((total_tests + 1))
    test_names+=("$phase_name")
    
    echo "════════════════════════════════════════════════════════" | tee -a "$LOG_FILE"
    echo "FASE $total_tests: $phase_name" | tee -a "$LOG_FILE"
    echo "════════════════════════════════════════════════════════" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    
    start_time=$(date +%s)
    
    # Ejecutar comando
    eval "$test_command" >> "$LOG_FILE" 2>&1
    exit_code=$?
    
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    test_times+=("${elapsed}s")
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✅ PASSED${NC} (${elapsed}s)" | tee -a "$LOG_FILE"
        test_results+=("PASSED")
        passed_tests=$((passed_tests + 1))
    else
        if [ "$required" = "true" ]; then
            echo -e "${RED}❌ FAILED${NC} (${elapsed}s)" | tee -a "$LOG_FILE"
            test_results+=("FAILED")
            failed_tests=$((failed_tests + 1))
        else
            echo -e "${YELLOW}⊘ SKIPPED${NC} (no crítico)" | tee -a "$LOG_FILE"
            test_results+=("SKIPPED")
            skipped_tests=$((skipped_tests + 1))
        fi
    fi
    
    echo "" | tee -a "$LOG_FILE"
}

# ============================================================================
# FASE 1: COMPILACIÓN
# ============================================================================

run_test_phase \
    "Compilación del servidor" \
    "make clean && make server"

# ============================================================================
# FASE 2: TESTS UNITARIOS
# ============================================================================

run_test_phase \
    "Tests unitarios - String Utils" \
    "make test_string"

run_test_phase \
    "Tests unitarios - Queue" \
    "make test_queue"

run_test_phase \
    "Tests unitarios - Worker Pool" \
    "make test_worker_pool"

run_test_phase \
    "Tests unitarios - Job Manager" \
    "make test_job_manager"

run_test_phase \
    "Tests unitarios - HTTP Parser" \
    "make test_http"

run_test_phase \
    "Tests unitarios - Metrics" \
    "make test_metrics"

# ============================================================================
# FASE 3: COBERTURA DE CÓDIGO
# ============================================================================

run_test_phase \
    "Análisis de cobertura de código" \
    "make coverage"

# ============================================================================
# FASE 4: TESTS DE INTEGRACIÓN (requieren servidor corriendo)
# ============================================================================

echo "════════════════════════════════════════════════════════" | tee -a "$LOG_FILE"
echo "FASE: Tests de Integración (servidor en background)" | tee -a "$LOG_FILE"
echo "════════════════════════════════════════════════════════" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Iniciar servidor en background
echo "Iniciando servidor..." | tee -a "$LOG_FILE"
./build/http_server 8080 >> "$LOG_FILE" 2>&1 &
SERVER_PID=$!

# Esperar a que el servidor inicie
sleep 3

# Verificar que el servidor esté corriendo
if ! curl -s -m 2 "http://localhost:8080/status" > /dev/null 2>&1; then
    echo -e "${RED}❌ Servidor no pudo iniciar${NC}" | tee -a "$LOG_FILE"
    kill $SERVER_PID 2>/dev/null
    failed_tests=$((failed_tests + 1))
else
    echo -e "${GREEN}✓ Servidor iniciado correctamente (PID: $SERVER_PID)${NC}" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    
    # Tests de concurrencia
    run_test_phase \
        "Tests de concurrencia y race conditions" \
        "./scripts/test_race_conditions_integration.sh" \
        false
    
    # Tests de CPU
    run_test_phase \
        "Tests de comandos CPU-bound" \
        "./scripts/test_cpu_bound_auto.sh" \
        false
    
    # Benchmark y métricas
    run_test_phase \
        "Benchmark y análisis de métricas" \
        "./scripts/benchmark_metrics.sh" \
        false
    
    # Detener servidor
    echo "Deteniendo servidor..." | tee -a "$LOG_FILE"
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    echo "" | tee -a "$LOG_FILE"
fi

# ============================================================================
# RESUMEN FINAL
# ============================================================================

echo "" | tee -a "$LOG_FILE"
echo "╔════════════════════════════════════════════════════════╗" | tee -a "$LOG_FILE"
echo "║                                                        ║" | tee -a "$LOG_FILE"
echo "║                  RESUMEN DE PRUEBAS                    ║" | tee -a "$LOG_FILE"
echo "║                                                        ║" | tee -a "$LOG_FILE"
echo "╚════════════════════════════════════════════════════════╝" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Mostrar tabla de resultados
printf "%-50s %-10s %-10s\n" "Test" "Resultado" "Tiempo" | tee -a "$LOG_FILE"
echo "────────────────────────────────────────────────────────────────────" | tee -a "$LOG_FILE"

for i in "${!test_names[@]}"; do
    result="${test_results[$i]}"
    time="${test_times[$i]}"
    
    if [ "$result" = "PASSED" ]; then
        result_colored="${GREEN}✅ PASSED${NC}"
    elif [ "$result" = "FAILED" ]; then
        result_colored="${RED}❌ FAILED${NC}"
    else
        result_colored="${YELLOW}⊘ SKIPPED${NC}"
    fi
    
    printf "%-50s " "${test_names[$i]}"
    echo -e "$result_colored" | awk '{printf "%-20s", $0}'
    echo "$time"
done | tee -a "$LOG_FILE"

echo "" | tee -a "$LOG_FILE"
echo "────────────────────────────────────────────────────────────────────" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Estadísticas
success_rate=0
if [ $total_tests -gt 0 ]; then
    success_rate=$((passed_tests * 100 / total_tests))
fi

echo "ESTADÍSTICAS:" | tee -a "$LOG_FILE"
echo "  Total de pruebas:    $total_tests" | tee -a "$LOG_FILE"
echo -e "  ${GREEN}Exitosas:            $passed_tests${NC}" | tee -a "$LOG_FILE"
echo -e "  ${RED}Fallidas:            $failed_tests${NC}" | tee -a "$LOG_FILE"
echo -e "  ${YELLOW}Omitidas:            $skipped_tests${NC}" | tee -a "$LOG_FILE"
echo "  Tasa de éxito:       $success_rate%" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Archivos generados
echo "ARCHIVOS GENERADOS:" | tee -a "$LOG_FILE"
echo "  Log completo:        $LOG_FILE" | tee -a "$LOG_FILE"
echo "  Cobertura:           coverage/" | tee -a "$LOG_FILE"
echo "  Benchmarks:          benchmark_results/" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

# Resultado final
if [ $failed_tests -eq 0 ]; then
    echo "╔════════════════════════════════════════════════════════╗" | tee -a "$LOG_FILE"
    echo "║                                                        ║" | tee -a "$LOG_FILE"
    echo -e "║  ${GREEN}✅  TODAS LAS PRUEBAS CRÍTICAS PASARON${NC}              ║" | tee -a "$LOG_FILE"
    echo "║                                                        ║" | tee -a "$LOG_FILE"
    echo "╚════════════════════════════════════════════════════════╝" | tee -a "$LOG_FILE"
    exit 0
else
    echo "╔════════════════════════════════════════════════════════╗" | tee -a "$LOG_FILE"
    echo "║                                                        ║" | tee -a "$LOG_FILE"
    echo -e "║  ${RED}❌  ALGUNAS PRUEBAS FALLARON${NC}                          ║" | tee -a "$LOG_FILE"
    echo "║                                                        ║" | tee -a "$LOG_FILE"
    echo "╚════════════════════════════════════════════════════════╝" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
    echo "Ver detalles en: $LOG_FILE" | tee -a "$LOG_FILE"
    exit 1
fi
