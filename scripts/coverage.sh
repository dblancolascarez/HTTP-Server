#!/bin/bash

COVERAGE_DIR="coverage"

echo ""
echo "=========================================="
echo "  Generando Reporte de Cobertura"
echo "=========================================="
echo ""

mkdir -p "$COVERAGE_DIR"
rm -rf "$COVERAGE_DIR"/*

cp build/*.gcda "$COVERAGE_DIR/" 2>/dev/null
cp build/*.gcno "$COVERAGE_DIR/" 2>/dev/null

cd "$COVERAGE_DIR" || exit 1

# Renombrar archivos
for file in *.gcda *.gcno; do
    [ -f "$file" ] || continue
    newname=$(echo "$file" | sed 's/^test_[a-z_]*-//')
    if [ "$file" != "$newname" ] && [ ! -f "$newname" ]; then
        mv "$file" "$newname" 2>/dev/null
    fi
done

cd ..

echo "Generando reportes con gcov..."
echo ""

# Ejecutar gcov DESDE LA RAIZ con -o apuntando a coverage/
for src_file in src/utils/*.c src/core/*.c src/server/*.c src/router/*.c; do
    [ -f "$src_file" ] || continue
    base=$(basename "$src_file" .c)
    
    if [ -f "$COVERAGE_DIR/${base}.gcda" ]; then
        # Ejecutar gcov desde raíz, especificando donde están los .gcda/.gcno
        gcov -o "$COVERAGE_DIR" "$src_file" >/dev/null 2>&1
        
        # Mover el .gcov generado al directorio coverage/
        if [ -f "${base}.c.gcov" ]; then
            mv "${base}.c.gcov" "$COVERAGE_DIR/"
        fi
    fi
done

gcov_count=$(ls -1 "$COVERAGE_DIR"/*.c.gcov 2>/dev/null | wc -l)
echo "Archivos .gcov generados: $gcov_count"

if [ "$gcov_count" -eq 0 ]; then
    echo "ERROR: No se generaron archivos .gcov"
    exit 1
fi

echo ""
echo "=========================================================="
echo "          REPORTE DE COBERTURA"
echo "=========================================================="
echo ""

total_all_lines=0
total_all_covered=0

for module in utils core server router; do
    module_dir="src/$module"
    [ -d "$module_dir" ] || continue
    
    echo "=== $(echo $module | tr '[:lower:]' '[:upper:]') ==="
    
    module_total=0
    module_covered=0
    file_count=0
    
    for src_file in "$module_dir"/*.c; do
        [ -f "$src_file" ] || continue
        
        base=$(basename "$src_file" .c)
        gcov_file="$COVERAGE_DIR/${base}.c.gcov"
        
        if [ -f "$gcov_file" ]; then
            # Contar líneas ejecutadas (las que empiezan con número)
            executed=$(grep -E "^[ ]*[0-9]+" "$gcov_file" | grep -c ":")
            # Contar líneas NO ejecutadas (las marcadas con #####)
            not_executed=$(grep -cE "^[ ]*#####:" "$gcov_file")
            
            executed=${executed:-0}
            not_executed=${not_executed:-0}
            
            total_lines=$((executed + not_executed))
            
            if [ "$total_lines" -gt 0 ]; then
                # Calcular líneas realmente ejecutadas (excluyendo las que tienen 0)
                really_executed=$(grep -E "^[ ]*[1-9][0-9]*:" "$gcov_file" | wc -l)
                really_executed=${really_executed:-0}
                
                percent=$((really_executed * 100 / total_lines))
                module_total=$((module_total + total_lines))
                module_covered=$((module_covered + really_executed))
                file_count=$((file_count + 1))
                
                if [ "$percent" -ge 90 ]; then mark="[+]"
                elif [ "$percent" -ge 70 ]; then mark="[~]"
                elif [ "$percent" -ge 50 ]; then mark="[.]"
                else mark="[-]"; fi
                
                printf "  %s %-30s %3d%% (%d/%d)\n" "$mark" "$base.c" "$percent" "$really_executed" "$total_lines"
            fi
        fi
    done
    
    if [ "$file_count" -gt 0 ] && [ "$module_total" -gt 0 ]; then
        module_percent=$((module_covered * 100 / module_total))
        echo "  ----------------------------------------------------------"
        printf "  Total: %d%% (%d/%d lines, %d archivos)\n" "$module_percent" "$module_covered" "$module_total" "$file_count"
        total_all_lines=$((total_all_lines + module_total))
        total_all_covered=$((total_all_covered + module_covered))
    else
        echo "  (Sin cobertura)"
    fi
    
    echo ""
done

if [ "$total_all_lines" -gt 0 ]; then
    overall=$((total_all_covered * 100 / total_all_lines))
    echo "=========================================================="
    printf "COBERTURA TOTAL: %d%% (%d/%d lines)\n" "$overall" "$total_all_covered" "$total_all_lines"
    echo "=========================================================="
fi

echo ""
echo "Leyenda: [+] 90%+  [~] 70-89%  [.] 50-69%  [-] <50%"
echo ""
echo "Ver detalles: cat $COVERAGE_DIR/<archivo>.c.gcov | less"
echo ""
