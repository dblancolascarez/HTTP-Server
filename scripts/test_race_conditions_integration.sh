#!/bin/bash

# Verificar servidor
if ! curl -s -m 2 "http://localhost:8080/status" > /dev/null 2>&1; then
    echo "ERROR: Servidor no disponible"
    exit 1
fi

echo "Ejecutando tests de concurrencia básicos..."

# Test simple de concurrencia
for i in {1..50}; do
    curl -s "http://localhost:8080/random?min=1&max=100" > /dev/null &
done

wait

echo "✓ Tests de concurrencia completados"
exit 0
