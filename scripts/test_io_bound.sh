#!/bin/bash

# Colores para output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Iniciando pruebas IO-bound...${NC}"

# Directorio para archivos de prueba
TEST_DIR="files"
mkdir -p "$TEST_DIR"

# Generar archivo grande (50MB) con números aleatorios para sortfile
echo -e "${BLUE}Generando archivo de prueba para sortfile (50MB)...${NC}"
dd if=/dev/urandom bs=1M count=50 | od -An -td1 | tr -s ' ' '\n' | grep -v '^$' > "$TEST_DIR/numbers_50mb.txt"
echo -e "${GREEN}✓ Archivo generado: numbers_50mb.txt${NC}"

# Generar archivo de texto para compress/hashfile
echo -e "${BLUE}Generando archivo de texto para compress/hashfile (50MB)...${NC}"
dd if=/dev/urandom bs=1M count=50 | base64 > "$TEST_DIR/data_50mb.txt"
echo -e "${GREEN}✓ Archivo generado: data_50mb.txt${NC}"

echo -e "\n${BLUE}Ejecutando pruebas IO-bound...${NC}\n"

# Test 1: sortfile (con ambos algoritmos)
echo -e "${BLUE}Test 1: /sortfile${NC}"
for algo in merge quick; do
    echo -e "\nProbando sortfile con algoritmo $algo..."
    time curl -s "http://localhost:8080/jobs/submit?task=sortfile&name=numbers_50mb.txt&algo=$algo" | tee /tmp/job_id.txt
    job_id=$(grep -o '"job_id":"[^"]*"' /tmp/job_id.txt | cut -d'"' -f4)
    
    echo -e "\nEsperando completación del job $job_id..."
    for i in $(seq 1 60); do
        sleep 1
        status=$(curl -s "http://localhost:8080/jobs/status?id=$job_id")
        echo -n "."
        if echo "$status" | grep -q '"status":"done"'; then
            echo -e "\n${GREEN}✓ Job completado${NC}"
            curl -s "http://localhost:8080/jobs/result?id=$job_id" | jq .
            break
        fi
    done
done

# Test 2: compress (con ambos codecs)
echo -e "\n${BLUE}Test 2: /compress${NC}"
for codec in gzip xz; do
    echo -e "\nProbando compress con codec $codec..."
    time curl -s "http://localhost:8080/jobs/submit?task=compress&name=data_50mb.txt&codec=$codec" | tee /tmp/job_id.txt
    job_id=$(grep -o '"job_id":"[^"]*"' /tmp/job_id.txt | cut -d'"' -f4)
    
    echo -e "\nEsperando completación del job $job_id..."
    for i in $(seq 1 60); do
        sleep 1
        status=$(curl -s "http://localhost:8080/jobs/status?id=$job_id")
        echo -n "."
        if echo "$status" | grep -q '"status":"done"'; then
            echo -e "\n${GREEN}✓ Job completado${NC}"
            curl -s "http://localhost:8080/jobs/result?id=$job_id" | jq .
            break
        fi
    done
done

# Test 3: hashfile
echo -e "\n${BLUE}Test 3: /hashfile${NC}"
echo -e "\nCalculando SHA-256 de archivo de 50MB..."
time curl -s "http://localhost:8080/jobs/submit?task=hashfile&name=data_50mb.txt&algo=sha256" | tee /tmp/job_id.txt
job_id=$(grep -o '"job_id":"[^"]*"' /tmp/job_id.txt | cut -d'"' -f4)

echo -e "\nEsperando completación del job $job_id..."
for i in $(seq 1 60); do
    sleep 1
    status=$(curl -s "http://localhost:8080/jobs/status?id=$job_id")
    echo -n "."
    if echo "$status" | grep -q '"status":"done"'; then
        echo -e "\n${GREEN}✓ Job completado${NC}"
        curl -s "http://localhost:8080/jobs/result?id=$job_id" | jq .
        break
    fi
done

# Comparar resultados
echo -e "\n${BLUE}Verificación de resultados:${NC}"

# Verificar sort
if [ -f "$TEST_DIR/numbers_50mb.txt.sorted" ]; then
    echo -e "${GREEN}✓ Archivo ordenado generado correctamente${NC}"
    head -n 5 "$TEST_DIR/numbers_50mb.txt.sorted"
else
    echo -e "${RED}✗ Error: Archivo ordenado no encontrado${NC}"
fi

# Verificar compress
for ext in gz xz; do
    if [ -f "$TEST_DIR/data_50mb.txt.$ext" ]; then
        original_size=$(stat --format="%s" "$TEST_DIR/data_50mb.txt")
        compressed_size=$(stat --format="%s" "$TEST_DIR/data_50mb.txt.$ext")
        ratio=$(echo "scale=2; $compressed_size * 100 / $original_size" | bc)
        echo -e "${GREEN}✓ Compresión $ext: $ratio% del tamaño original${NC}"
    else
        echo -e "${RED}✗ Error: Archivo comprimido .$ext no encontrado${NC}"
    fi
done

# Verificar hash contra sha256sum
echo -e "\nVerificando hash SHA-256..."
expected=$(sha256sum "$TEST_DIR/data_50mb.txt" | cut -d' ' -f1)
actual=$(curl -s "http://localhost:8080/jobs/result?id=$job_id" | jq -r .hash)
if [ "$expected" = "$actual" ]; then
    echo -e "${GREEN}✓ Hash SHA-256 verificado correctamente${NC}"
else
    echo -e "${RED}✗ Error: Hash SHA-256 no coincide${NC}"
    echo "Esperado: $expected"
    echo "Recibido: $actual"
fi

echo -e "\n${BLUE}Pruebas IO-bound completadas.${NC}"