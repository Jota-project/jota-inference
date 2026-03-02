#!/bin/bash

# Script para compilar de forma rápida la versión CPU para desarrollo local

BUILD_DIR="build_fast"

echo "=== Preparando compilación rápida (CPU-only) ==="

# Crear y entrar al directorio de build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Opciones de CMake para acelerar el desarrollo:
# - USE_CUDA=OFF: Desactiva CUDA (evita usar nvcc que es muy lento)
# - LLAMA_BUILD_EXAMPLES=OFF: Evita compilar los ~50 binarios/ejemplos de llama.cpp
# - LLAMA_BUILD_TESTS=OFF: Evita compilar los tests propios de llama.cpp
# - LLAMA_TOOLS=OFF: Evita compilar las herramientas extras de llama.cpp
# - BUILD_TESTS=ON: Mantiene los tests de InferenceCore (por defecto desactivados en tu entorno)
CMAKE_ARGS=(
    "-DUSE_CUDA=OFF"
    "-DLLAMA_BUILD_EXAMPLES=OFF"
    "-DLLAMA_BUILD_TESTS=OFF"
    "-DLLAMA_TOOLS=OFF"
    "-DBUILD_TESTS=ON"
)

# 1. Detectar y usar ccache (reutiliza compilaciones previas)
if command -v ccache &> /dev/null; then
    echo "[info] ccache detectado. Usándolo como cache de compilación..."
    CMAKE_ARGS+=("-DCMAKE_C_COMPILER_LAUNCHER=ccache" "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
fi

# 2. Detectar y usar Ninja (gestor de compilación mucho más rápido que Make)
if command -v ninja &> /dev/null; then
    echo "[info] Ninja detectado. Usando el generador Ninja..."
    CMAKE_ARGS+=("-G" "Ninja")
else
    echo "[info] Ninja no detectado. (Recomendación: instala Ninja para mayor velocidad)"
fi

echo "[info] Configurando con CMake..."
cmake .. "${CMAKE_ARGS[@]}"

echo "[info] Compilando en paralelo..."
# 3. --parallel aprovecha todos los núcleos del sistema
cmake --build . --parallel

echo "=== ¡Compilación Terminada! ==="
echo "Puedes encontrar el ejecutable en: $BUILD_DIR/"
