#!/bin/bash

echo "═══════════════════════════════════════════════════"
echo "  DETECCIÓN DE RACE CONDITIONS CON VALGRIND"
echo "═══════════════════════════════════════════════════"
echo ""

# Compilar servidor con símbolos de debug
echo "Compilando servidor con símbolos de debug..."
make clean
make server CFLAGS="-Wall -Wextra -pthread -g -O0 -Isrc"

echo ""
echo "--- Iniciando servidor con Helgrind ---"
echo "Esto detectará:"
echo "  - Race conditions (data races)"
echo "  - Deadlocks potenciales"
echo "  - Uso incorrecto de mutexes"
echo ""

# Ejecutar con Helgrind
valgrind --tool=helgrind \
         --log-file=helgrind_report.txt \
         --read-var-info=yes \
         --track-lockorders=yes \
         ./build/http_server 8080 &

SERVER_PID=$!

# Esperar a que el servidor inicie
sleep 2

echo ""
echo "Ejecutando tests de concurrencia..."
./build/test_race_conditions

# Terminar servidor
echo ""
echo "Terminando servidor..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo ""
echo "═══════════════════════════════════════════════════"
echo "  REPORTE GENERADO: helgrind_report.txt"
echo "═══════════════════════════════════════════════════"
echo ""
echo "Resumen de errores encontrados:"
grep -A 2 "^==[0-9]*== Possible data race" helgrind_report.txt | head -20
echo ""
echo "Ver reporte completo: cat helgrind_report.txt"
