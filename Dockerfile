# ─── FASE A: Dependencias pesadas (cache contra llama.cpp + CMakeLists.txt) ───
FROM nvidia/cuda:13.1.1-devel-ubuntu22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    git \
    pkg-config \
    zlib1g-dev \
    libssl-dev \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Solo lo que define las dependencias pesadas.
# Cambios en src/ NO invalidan esta capa.
COPY llama.cpp/ llama.cpp/
COPY CMakeLists.txt .

# Stubs vacíos para que cmake configure no falle con add_executable().
# No se compilan en esta fase — solo necesitan existir.
RUN mkdir -p src/core src/server/services src/server/handlers \
             src/config src/logging src/hardware && \
    touch src/main.cpp \
          src/core/Engine.cpp \
          src/core/Session.cpp \
          src/core/SessionManager.cpp \
          src/config/EnvLoader.cpp \
          src/config/AppConfig.cpp \
          src/logging/Logger.cpp \
          src/logging/LlamaLogger.cpp \
          src/server/WsServer.cpp \
          src/server/ClientAuth.cpp \
          src/server/services/InferenceService.cpp \
          src/server/services/MetricsService.cpp \
          src/server/services/ModelResolver.cpp \
          src/hardware/Monitor.cpp

# Configure: resuelve FetchContent, detecta CUDA, define todos los targets.
RUN cmake -G Ninja -DUSE_CUDA=ON -DBUILD_TESTS=OFF -S . -B build

# Compilar solo los targets pesados: llama.cpp + CUDA kernels + uSockets.
# InferenceCore NO se compila aquí.
RUN ninja -C build llama ggml uSockets -j2

# ─── FASE B: Código propio (invalidada solo si src/ cambia) ──────────────────
# Sobreescribe los stubs con el código real.
COPY src/ src/

# CMake detecta los .cpp modificados y recompila únicamente InferenceCore.
# llama/ggml/uSockets ya están compilados y cacheados.
RUN ninja -C build -j$(nproc)

# ─── RUNTIME ─────────────────────────────────────────────────────────────────
FROM nvidia/cuda:13.1.1-runtime-ubuntu22.04

RUN apt-get update && apt-get install -y \
    libgomp1 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN mkdir -p /app/logs && chmod 777 /app/logs

COPY --from=builder /app/build/InferenceCore .
COPY --from=builder /app/build/bin/*.so* /app/lib/
COPY --from=builder /app/.env .

EXPOSE 8001

ENV LD_LIBRARY_PATH=/app/lib
ENV PORT=8001
ENV GPU_LAYERS=-1

ENTRYPOINT ./InferenceCore --port $PORT --gpu-layers $GPU_LAYERS
