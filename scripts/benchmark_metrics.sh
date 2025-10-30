#!/bin/bash

SERVER_HOST="localhost"
SERVER_PORT="8080"
RESULTS_DIR="benchmark_results"

mkdir -p "$RESULTS_DIR"

echo ""
echo "=========================================="
echo "  BENCHMARK DE METRICAS Y LATENCIAS"
echo "=========================================="
echo ""

# Verificar servidor
if ! curl -s -m 2 "http://$SERVER_HOST:$SERVER_PORT/status" > /dev/null 2>&1; then
    echo "❌ ERROR: Servidor no disponible"
    exit 1
fi

echo "✓ Servidor disponible"
echo ""

# Función para ejecutar benchmark
run_benchmark() {
    local profile_name=$1
    local num_requests=$2
    local concurrency=$3
    local endpoint=$4
    local description=$5
    
    echo "=========================================="
    echo "PERFIL: $profile_name"
    echo "=========================================="
    echo "Descripción: $description"
    echo "Requests totales: $num_requests"
    echo "Concurrencia: $concurrency"
    echo "Endpoint: $endpoint"
    echo ""
    
    output_file="$RESULTS_DIR/${profile_name}_$(date +%Y%m%d_%H%M%S).txt"
    
    # Ejecutar benchmark con Apache Bench (ab)
    if command -v ab > /dev/null 2>&1; then
        echo "Ejecutando con Apache Bench (ab)..."
        ab -n "$num_requests" -c "$concurrency" \
           "http://$SERVER_HOST:$SERVER_PORT$endpoint" \
           > "$output_file" 2>&1
        
        # Parsear resultados
        parse_ab_results "$output_file" "$profile_name"
        
    # Fallback: usar curl con mediciones manuales
    else
        echo "Apache Bench no disponible, usando curl..."
        run_curl_benchmark "$profile_name" "$num_requests" "$concurrency" "$endpoint"
    fi
    
    echo ""
}

# Parsear resultados de Apache Bench
parse_ab_results() {
    local file=$1
    local profile=$2
    
    if [ ! -f "$file" ]; then
        echo "⚠️  Archivo de resultados no encontrado"
        return
    fi
    
    echo "RESULTADOS:"
    echo ""
    
    # Requests por segundo (throughput)
    rps=$(grep "Requests per second:" "$file" | awk '{print $4}')
    echo "  Throughput: $rps req/s"
    
    # Tiempo promedio por request
    time_per_req=$(grep "Time per request:" "$file" | head -1 | awk '{print $4}')
    echo "  Tiempo promedio: ${time_per_req}ms"
    
    # Latencias (percentiles)
    echo ""
    echo "  Latencias (ms):"
    p50=$(grep "50%" "$file" | awk '{print $2}')
    p95=$(grep "95%" "$file" | awk '{print $2}')
    p99=$(grep "99%" "$file" | awk '{print $2}')
    
    echo "    p50 (mediana): ${p50}ms"
    echo "    p95:           ${p95}ms"
    echo "    p99:           ${p99}ms"
    
    # Requests completados vs fallidos
    complete=$(grep "Complete requests:" "$file" | awk '{print $3}')
    failed=$(grep "Failed requests:" "$file" | awk '{print $3}')
    echo ""
    echo "  Requests completados: $complete"
    echo "  Requests fallidos: $failed"
    
    # Guardar resumen
    {
        echo "Profile: $profile"
        echo "Timestamp: $(date)"
        echo "Throughput: $rps req/s"
        echo "Latency p50: ${p50}ms"
        echo "Latency p95: ${p95}ms"
        echo "Latency p99: ${p99}ms"
        echo "Completed: $complete"
        echo "Failed: $failed"
        echo "---"
    } >> "$RESULTS_DIR/summary.txt"
}

# Benchmark manual con curl (fallback)
run_curl_benchmark() {
    local profile=$1
    local num_requests=$2
    local concurrency=$3
    local endpoint=$4
    
    echo "Ejecutando $num_requests requests con concurrencia $concurrency..."
    
    temp_file="$RESULTS_DIR/${profile}_latencies.txt"
    > "$temp_file"
    
    # Ejecutar requests y medir tiempos
    for i in $(seq 1 $num_requests); do
        start=$(date +%s%3N)
        curl -s -o /dev/null "http://$SERVER_HOST:$SERVER_PORT$endpoint" 2>/dev/null
        end=$(date +%s%3N)
        latency=$((end - start))
        echo "$latency" >> "$temp_file"
    done
    
    # Calcular estadísticas
    calculate_percentiles "$temp_file" "$profile"
}

# Calcular percentiles manualmente
calculate_percentiles() {
    local file=$1
    local profile=$2
    
    if [ ! -f "$file" ]; then
        return
    fi
    
    # Ordenar latencias
    sort -n "$file" > "${file}.sorted"
    
    total=$(wc -l < "${file}.sorted")
    
    # Calcular índices de percentiles
    idx_50=$((total / 2))
    idx_95=$((total * 95 / 100))
    idx_99=$((total * 99 / 100))
    
    # Obtener valores
    p50=$(sed -n "${idx_50}p" "${file}.sorted")
    p95=$(sed -n "${idx_95}p" "${file}.sorted")
    p99=$(sed -n "${idx_99}p" "${file}.sorted")
    
    # Calcular throughput
    total_time=$(awk '{sum+=$1} END {print sum}' "$file")
    rps=$(echo "scale=2; $total * 1000 / $total_time" | bc)
    
    echo "RESULTADOS:"
    echo ""
    echo "  Throughput: ${rps} req/s"
    echo ""
    echo "  Latencias (ms):"
    echo "    p50 (mediana): ${p50}ms"
    echo "    p95:           ${p95}ms"
    echo "    p99:           ${p99}ms"
    
    # Guardar resumen
    {
        echo "Profile: $profile"
        echo "Timestamp: $(date)"
        echo "Throughput: ${rps} req/s"
        echo "Latency p50: ${p50}ms"
        echo "Latency p95: ${p95}ms"
        echo "Latency p99: ${p99}ms"
        echo "---"
    } >> "$RESULTS_DIR/summary.txt"
    
    rm -f "${file}.sorted"
}

# =============================================================================
# PERFIL 1: CARGA LIGERA
# Simula tráfico normal con requests simples
# =============================================================================

echo "╔════════════════════════════════════════════════════════╗"
echo "║  PERFIL 1: CARGA LIGERA                                ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

run_benchmark \
    "perfil1_ligera" \
    100 \
    10 \
    "/random?min=1&max=100" \
    "Carga ligera: 100 requests, concurrencia 10"

sleep 2

# =============================================================================
# PERFIL 2: CARGA MEDIA
# Simula tráfico moderado con mix de operaciones
# =============================================================================

echo ""
echo "╔════════════════════════════════════════════════════════╗"
echo "║  PERFIL 2: CARGA MEDIA                                 ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

run_benchmark \
    "perfil2_media" \
    500 \
    50 \
    "/isprime?n=97" \
    "Carga media: 500 requests, concurrencia 50"

sleep 2

# =============================================================================
# PERFIL 3: CARGA ALTA (STRESS TEST)
# Simula tráfico intenso con alta concurrencia
# =============================================================================

echo ""
echo "╔════════════════════════════════════════════════════════╗"
echo "║  PERFIL 3: CARGA ALTA (STRESS)                         ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

run_benchmark \
    "perfil3_alta" \
    1000 \
    100 \
    "/factor?n=123456" \
    "Carga alta: 1000 requests, concurrencia 100"

# =============================================================================
# RESUMEN FINAL
# =============================================================================

echo ""
echo "=========================================="
echo "  RESUMEN DE TODOS LOS PERFILES"
echo "=========================================="
echo ""

if [ -f "$RESULTS_DIR/summary.txt" ]; then
    cat "$RESULTS_DIR/summary.txt"
else
    echo "No se generó archivo de resumen"
fi

echo ""
echo "Resultados guardados en: $RESULTS_DIR/"
echo ""
echo "Ver métricas del servidor:"
echo "  curl http://$SERVER_HOST:$SERVER_PORT/metrics"
echo ""
