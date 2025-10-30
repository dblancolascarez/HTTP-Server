#!/bin/bash

echo "Instalando Apache Bench (ab)..."
apt-get update
apt-get install -y apache2-utils bc
echo "✓ Apache Bench instalado"
ab -V
