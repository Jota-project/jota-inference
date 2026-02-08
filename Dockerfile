# --- ETAPA 1: Builder ---
FROM nvidia/cuda:12.2.0-devel-ubuntu22.04 AS builder

# Instalar dependencias de compilación según el README
#
RUN apt-get update && apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    pkg-config \
    zlib1g-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar el código fuente y submódulos (llama.cpp)
COPY . .

# Configurar y compilar con soporte para CUDA
#
RUN mkdir build && cd build && \
    cmake -DUSE_CUDA=ON -DBUILD_TESTS=OFF ..

# Compilar con verbose para ver errores y limitar hilos para evitar OOM
RUN cd build && \
    make VERBOSE=1 -j2

# --- ETAPA 2: Runner ---
FROM nvidia/cuda:12.2.0-runtime-ubuntu22.04

# Instalar dependencias mínimas de ejecución
#
RUN apt-get update && apt-get install -y \
    libgomp1 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar solo el ejecutable desde la etapa de construcción
COPY --from=builder /app/build/InferenceCore .
# Copiar librerías compartidas de llama.cpp
COPY --from=builder /app/build/bin/*.so* /app/lib/
# Copiar el archivo de configuración de clientes por defecto
COPY --from=builder /app/clients.json .

# Exponer el puerto del servidor WebSocket (default 3000)
#
EXPOSE 8001

# Variables de entorno por defecto
ENV LD_LIBRARY_PATH=/app/lib
ENV MODEL_PATH=/models/model.gguf
ENV PORT=8001
ENV GPU_LAYERS=-1

# Comando para ejecutar el servidor
#
ENTRYPOINT ./InferenceCore --model $MODEL_PATH --port $PORT --gpu-layers $GPU_LAYERS
