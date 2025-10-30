#!/bin/bash

SERVER_HOST="localhost"
SERVER_PORT="8080"

echo ""
echo "=========================================="
echo "  PRUEBAS AUTOMATICAS CPU-BOUND"
echo "=========================================="
echo ""

if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
    echo "❌ ERROR: Servidor no disponible"
    exit 1
fi

echo "✓ Servidor disponible"
echo ""

tests_passed=0
tests_failed=0

run_test() {
    local name=$1
    local url=$2
    
    printf "%-50s" "$name..."
    
    start=$(date +%s%3N)
    response=$(curl -s -m 30 "http://$SERVER_HOST:$SERVER_PORT$url")
    end=$(date +%s%3N)
    elapsed=$((end - start))
    
    if echo "$response" | grep -q '"error"'; then
        echo " ❌ FAILED (${elapsed}ms)"
        echo "   Error: $(echo $response | grep -o '"error":"[^"]*"')"
        tests_failed=$((tests_failed + 1))
        return 1
    else
        echo " ✓ PASSED (${elapsed}ms)"
        tests_passed=$((tests_passed + 1))
        return 0
    fi
}

echo "Ejecutando pruebas..."
echo ""

# Tests de PI
run_test "PI: 10 dígitos" "/pi?digits=10"
run_test "PI: 15 dígitos" "/pi?digits=15"

# Tests de Mandelbrot
run_test "MANDELBROT: 50x50, 100 iter" "/mandelbrot?width=50&height=50&max_iter=100"
run_test "MANDELBROT: 100x100, 100 iter" "/mandelbrot?width=100&height=100&max_iter=100"
run_test "MANDELBROT: 200x200, 500 iter" "/mandelbrot?width=200&height=200&max_iter=500"
run_test "MANDELBROT: 300x300, 500 iter" "/mandelbrot?width=300&height=300&max_iter=500"

# Tests de MatrixMul
run_test "MATRIXMUL: 50x50" "/matrixmul?size=50&seed=1"
run_test "MATRIXMUL: 100x100" "/matrixmul?size=100&seed=42"
run_test "MATRIXMUL: 150x150" "/matrixmul?size=150&seed=123"
run_test "MATRIXMUL: 200x200" "/matrixmul?size=200&seed=999"

# Test de carga pesada (opcional)
echo ""
echo "Tests de carga pesada (pueden tardar varios segundos):"
run_test "MANDELBROT: 400x400, 1000 iter" "/mandelbrot?width=400&height=400&max_iter=1000"
run_test "MATRIXMUL: 300x300" "/matrixmul?size=300&seed=777"

echo ""
echo "=========================================="
echo "  RESULTADOS"
echo "=========================================="
echo "Tests exitosos: $tests_passed"
echo "Tests fallidos: $tests_failed"
echo "Total: $((tests_passed + tests_failed))"
echo ""

if [ "$tests_failed" -eq 0 ]; then
    echo "✓ Todos los tests pasaron"
    exit 0
else
    echo "⚠️  Algunos tests fallaron"
    exit 1
fi
