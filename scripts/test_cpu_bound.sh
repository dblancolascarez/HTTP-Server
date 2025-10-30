#!/bin/bash

SERVER_HOST="localhost"
SERVER_PORT="8080"

echo ""
echo "=========================================="
echo "  PRUEBAS DE COMANDOS CPU-BOUND"
echo "=========================================="
echo ""

# Verificar que el servidor esté corriendo
if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
    echo "❌ ERROR: El servidor no está corriendo en $SERVER_HOST:$SERVER_PORT"
    echo "   Iniciar con: ./build/http_server $SERVER_PORT"
    exit 1
fi

echo "✓ Servidor detectado en $SERVER_HOST:$SERVER_PORT"
echo ""

# Función para hacer requests y mostrar resultados
test_endpoint() {
    local name=$1
    local url=$2
    local expected_time=$3
    
    echo "=========================================="
    echo "TEST: $name"
    echo "=========================================="
    echo "URL: $url"
    echo "Tiempo esperado: ~$expected_time segundos"
    echo ""
    
    start_time=$(date +%s)
    
    # Hacer request y capturar respuesta
    response=$(curl -s "http://$SERVER_HOST:$SERVER_PORT$url")
    
    end_time=$(date +%s)
    elapsed=$((end_time - start_time))
    
    echo "Tiempo real: $elapsed segundos"
    echo ""
    
    # Parsear JSON para obtener elapsed_ms
    if echo "$response" | grep -q "elapsed_ms"; then
        server_elapsed=$(echo "$response" | grep -o '"elapsed_ms":[0-9]*' | cut -d':' -f2)
        server_elapsed_sec=$((server_elapsed / 1000))
        echo "Tiempo reportado por servidor: ${server_elapsed}ms (${server_elapsed_sec}s)"
    fi
    
    # Mostrar respuesta (truncada si es muy larga)
    response_len=${#response}
    if [ "$response_len" -gt 500 ]; then
        echo "Respuesta (primeros 500 chars):"
        echo "$response" | head -c 500
        echo "..."
        echo "[Total: $response_len caracteres]"
    else
        echo "Respuesta completa:"
        echo "$response"
    fi
    
    echo ""
    
    # Verificar si hubo error
    if echo "$response" | grep -q '"error"'; then
        echo "⚠️  El comando retornó un error"
        return 1
    else
        echo "✓ Comando ejecutado exitosamente"
        return 0
    fi
}

# =============================================================================
# TEST 1: PI - Cálculo de Pi con 15 dígitos
# =============================================================================

test_endpoint \
    "PI - Cálculo de Pi (15 dígitos)" \
    "/pi?digits=15" \
    "< 1"

echo ""
read -p "Presiona Enter para continuar con el siguiente test..."
echo ""

# =============================================================================
# TEST 2: MANDELBROT - Imagen pequeña (100x100)
# =============================================================================

test_endpoint \
    "MANDELBROT - Imagen pequeña (100x100, 100 iteraciones)" \
    "/mandelbrot?width=100&height=100&max_iter=100" \
    "1-2"

echo ""
read -p "Presiona Enter para continuar con el siguiente test..."
echo ""

# =============================================================================
# TEST 3: MANDELBROT - Imagen mediana (300x300)
# =============================================================================

test_endpoint \
    "MANDELBROT - Imagen mediana (300x300, 500 iteraciones)" \
    "/mandelbrot?width=300&height=300&max_iter=500" \
    "3-5"

echo ""
read -p "Presiona Enter para continuar con el siguiente test..."
echo ""

# =============================================================================
# TEST 4: MATRIXMUL - Matriz pequeña (100x100)
# =============================================================================

test_endpoint \
    "MATRIXMUL - Matriz pequeña (100x100)" \
    "/matrixmul?size=100&seed=42" \
    "1-2"

echo ""
read -p "Presiona Enter para continuar con el siguiente test..."
echo ""

# =============================================================================
# TEST 5: MATRIXMUL - Matriz mediana (200x200)
# =============================================================================

test_endpoint \
    "MATRIXMUL - Matriz mediana (200x200)" \
    "/matrixmul?size=200&seed=123" \
    "3-5"

echo ""
read -p "Presiona Enter para continuar con el siguiente test..."
echo ""

# =============================================================================
# TEST 6: MATRIXMUL - Matriz grande (300x300)
# =============================================================================

test_endpoint \
    "MATRIXMUL - Matriz grande (300x300)" \
    "/matrixmul?size=300&seed=999" \
    "8-12"

echo ""
read -p "Presiona Enter para continuar con el siguiente test..."
echo ""

# =============================================================================
# TEST 7: MANDELBROT - Imagen grande con alta resolución
# =============================================================================

test_endpoint \
    "MANDELBROT - Imagen grande (400x400, 1000 iteraciones)" \
    "/mandelbrot?width=400&height=400&max_iter=1000" \
    "10-15"

echo ""
echo "=========================================="
echo "  RESUMEN DE PRUEBAS"
echo "=========================================="
echo ""
echo "Todas las pruebas de CPU completadas."
echo ""
echo "Comandos testeados:"
echo "  ✓ /pi         - Cálculo de Pi"
echo "  ✓ /mandelbrot - Conjunto de Mandelbrot"
echo "  ✓ /matrixmul  - Multiplicación de matrices"
echo ""
echo "Para ver métricas del servidor:"
echo "  curl http://$SERVER_HOST:$SERVER_PORT/metrics"
echo ""
