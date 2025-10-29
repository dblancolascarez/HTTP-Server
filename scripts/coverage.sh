#!/bin/bash

echo ""
echo "=========================================="
echo "  Generando Reporte de Cobertura"
echo "=========================================="
echo ""

# Buscar archivos .gcda en build/ y copiarlos
echo "Preparando archivos de cobertura..."
find build -name "*.gcda" -exec cp {} . \; 2>/dev/null
find build -name "*.gcno" -exec cp {} . \; 2>/dev/null

# Generar archivos .gcov
echo "Generando reportes..."
for src in src/utils/*.c src/core/*.c src/server/*.c src/router/*.c; do
    [ -f "$src" ] || continue
    gcov -o . "$src" > /dev/null 2>&1
done

echo ""
echo "═══════════════════════════════════════"
echo "  REPORTE DE COBERTURA"
echo "═══════════════════════════════════════"
echo ""

# Mostrar reporte
for gcov_file in *.c.gcov; do
    [ -f "$gcov_file" ] || continue
    
    covered=$(grep -cE "^[ ]*[1-9][0-9]*:" "$gcov_file" 2>/dev/null || echo 0)
    uncovered=$(grep -cE "^[ ]*#+:" "$gcov_file" 2>/dev/null || echo 0)
    total=$((covered + uncovered))
    
    if [ $total -gt 0 ]; then
        percent=$((covered * 100 / total))
        printf "%-40s %3d%% (%d/%d lines)\n" "$gcov_file" $percent $covered $total
    fi
done | sort -k2 -rn

total_files=$(ls -1 *.c.gcov 2>/dev/null | wc -l)
echo ""
echo "Total archivos: $total_files"
echo ""
