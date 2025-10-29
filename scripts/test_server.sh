#!/bin/bash

# Script para probar el servidor HTTP con curl
# Usage: ./scripts/test_server.sh [port]

PORT=${1:-8080}
BASE_URL="http://localhost:$PORT"

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo ""
echo "=========================================="
echo "  Testing HTTP Server on port $PORT"
echo "=========================================="
echo ""

# Función para hacer request y mostrar resultado
test_endpoint() {
    local name=$1
    local path=$2
    local expected_code=$3
    
    echo -n "Testing $name ... "
    
    response=$(curl -s -w "\n%{http_code}" "$BASE_URL$path")
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n-1)
    
    if [ "$http_code" -eq "$expected_code" ]; then
        echo -e "${GREEN}✅ PASS${NC} (HTTP $http_code)"
        echo "   Response: $body"
    else
        echo -e "${RED}❌ FAIL${NC} (Expected $expected_code, got $http_code)"
        echo "   Response: $body"
    fi
    echo ""
}

# Verificar si el servidor está corriendo
echo "Checking if server is running..."
if ! curl -s --connect-timeout 2 "$BASE_URL/status" > /dev/null 2>&1; then
    echo -e "${RED}❌ Server is not running on port $PORT${NC}"
    echo ""
    echo "Start the server first:"
    echo "  make run"
    echo "  # or"
    echo "  ./build/http_server $PORT"
    echo ""
    exit 1
fi

echo -e "${GREEN}✅ Server is running${NC}"
echo ""

# ============================================================================
# TESTS
# ============================================================================

echo "Running endpoint tests..."
echo ""

# Test 1: Status endpoint
test_endpoint "GET /status" "/status" 200

# Test 2: Help endpoint
test_endpoint "GET /help" "/help" 200

# Test 3: Not found
test_endpoint "GET /notfound" "/notfound" 404

# Test 4: Status con query string (ignorado por ahora)
test_endpoint "GET /status?test=1" "/status?test=1" 200

# Test 5: Unsafe path
test_endpoint "GET /../etc/passwd" "/../etc/passwd" 400

# ============================================================================
# CONCURRENT REQUESTS TEST
# ============================================================================

echo "=========================================="
echo "  Concurrent Requests Test"
echo "=========================================="
echo ""

echo "Sending 10 concurrent requests..."
for i in {1..10}; do
    curl -s "$BASE_URL/status" > /dev/null &
done

wait

echo -e "${GREEN}✅ All concurrent requests completed${NC}"
echo ""

# ============================================================================
# PERFORMANCE TEST
# ============================================================================

echo "=========================================="
echo "  Simple Performance Test"
echo "=========================================="
echo ""

echo "Sending 100 requests sequentially..."
start_time=$(date +%s%N)

for i in {1..100}; do
    curl -s "$BASE_URL/status" > /dev/null
done

end_time=$(date +%s%N)
elapsed=$((($end_time - $start_time) / 1000000)) # Convert to milliseconds

echo -e "${GREEN}✅ Completed 100 requests in ${elapsed}ms${NC}"
echo "   Average: $((elapsed / 100))ms per request"
echo ""

# ============================================================================
# SUMMARY
# ============================================================================

echo "=========================================="
echo "  Test Summary"
echo "=========================================="
echo ""
echo "All basic tests completed!"
echo ""
echo "Try these commands manually:"
echo "  curl $BASE_URL/status"
echo "  curl $BASE_URL/help"
echo "  curl -v $BASE_URL/status  # Verbose mode"
echo ""