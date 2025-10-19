# Dockerfile para HTTP Server
FROM gcc:15.2

# Instalar herramientas necesarias
RUN apt-get update && apt-get install -y \
    make \
    gcovr \
    lcov \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Establecer directorio de trabajo
WORKDIR /app

# Copiar todo el proyecto
COPY . .

# Crear directorio build
RUN mkdir -p build

# Por defecto, ejecutar bash
CMD ["/bin/bash"]