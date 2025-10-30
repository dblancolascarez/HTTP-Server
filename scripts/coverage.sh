#!/bin/bash

echo ""
echo "=========================================="
echo "  Generando Reporte de Cobertura"
echo "=========================================="
echo ""

# Generar archivos .gcov
gcov -r src/utils/*.c src/core/*.c src/server/*.c > /dev/null 2>&1 || true

echo "Archivos de cobertura generados: $(ls -1 *.c.gcov 2>/dev/null | wc -l)"
echo ""

for file in *.c.gcov; do
    [ -f "$file" ] || continue
    
    total=$(grep -cE "^ +[0-9#]+" "$file" 2>/dev/null || echo 1)
    covered=$(grep -cE "^ +[1-9][0-9]*:" "$file" 2>/dev/null || echo 0)
    
    if [ $total -gt 0 ]; then
        percent=$((covered * 100 / total))
        printf "%-40s %3d%%\n" "$file" $percent
    fi
done

echo ""
echo "Reporte de cobertura generado."
echo ""# Limpiar archivos .gcov
rm -f *.c.gcov