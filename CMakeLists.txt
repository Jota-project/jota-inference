cmake_minimum_required(VERSION 3.18)
project(InferenceCore LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable/disable CUDA
option(USE_CUDA "Enable CUDA support" OFF)

if(USE_CUDA)
    # 1. Buscar CUDA
    find_package(CUDAToolkit REQUIRED)
    add_compile_definitions(USE_CUDA)
    message(STATUS "CUDA enabled")
else()
    message(STATUS "Building CPU-only version")
endif()

find_package(ZLIB REQUIRED) # Needed by uWebSockets

# 2. IMPORTAR llama.cpp
if(USE_CUDA)
    set(GGML_CUDA ON CACHE BOOL "Enable CUDA" FORCE)
else()
    set(GGML_CUDA OFF CACHE BOOL "Disable CUDA" FORCE)
endif()
add_subdirectory(llama.cpp)

find_package(Threads REQUIRED)

# --- DEPENDENCIES (FetchContent) ---
include(FetchContent)

# 1. JSON (nlohmann/json)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# 2. uSockets (Base layer for uWebSockets)
FetchContent_Declare(
    usockets
    GIT_REPOSITORY https://github.com/uNetworking/uSockets.git
    GIT_TAG        v0.8.8
)

FetchContent_Declare(
    uwebsockets
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
    GIT_TAG        v20.64.0
)

# 2.1 Manually configure uSockets
FetchContent_GetProperties(usockets)
if(NOT usockets_POPULATED)
  FetchContent_MakeAvailable(usockets)
endif()

# Create uSockets library from source
file(GLOB_RECURSE USOCKETS_SOURCES "${usockets_SOURCE_DIR}/src/*.c" "${usockets_SOURCE_DIR}/src/*.cpp")

# Exclude main files or examples if any (usually src is clean)
add_library(uSockets STATIC ${USOCKETS_SOURCES})

# Disable SSL for simplicity (avoids OpenSSL dependency hell for now)
target_compile_definitions(uSockets PUBLIC LIBUS_NO_SSL)
target_include_directories(uSockets PUBLIC "${usockets_SOURCE_DIR}/src")

# 2.2 Configure uWebSockets
FetchContent_GetProperties(uwebsockets)
if(NOT uwebsockets_POPULATED)
  FetchContent_MakeAvailable(uwebsockets)
endif()
# uWebSockets is header only, just needs the includes (handled below)

# --- END DEPENDENCIES ---

# 3. Tu ejecutable
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    # Core
    src/core/Engine.cpp
    src/core/Session.cpp
    src/core/SessionManager.cpp
    # Server - Core
    src/server/Protocol.h
    src/server/WsServer.h
    src/server/WsServer.cpp
    src/server/ClientAuth.h
    src/server/ClientAuth.cpp
    src/server/RequestContext.h
    src/server/MessageDispatcher.h
    # Server - Services
    src/server/services/InferenceService.h
    src/server/services/InferenceService.cpp
    src/server/services/MetricsService.h
    src/server/services/MetricsService.cpp
    # Server - Handlers (header-only)
    src/server/handlers/AuthHandler.h
    src/server/handlers/SessionHandler.h
    src/server/handlers/InferenceHandler.h
    src/server/handlers/MetricsHandler.h
    # Hardware
    src/hardware/Monitor.h
    src/hardware/Monitor.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src/core src/server src/hardware)

# 4. Enlazar usando los nombres de los objetivos, no rutas a archivos
target_link_libraries(${PROJECT_NAME} PRIVATE 
    llama 
    ggml 
    uSockets
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

if(USE_CUDA)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        CUDA::cudart
        CUDA::cublas
        CUDA::nvml
    )
endif()

# uWebSockets is header only, so just include directory
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${uwebsockets_SOURCE_DIR}/src
    ${usockets_SOURCE_DIR}/src
)

# 5. Flags
if (NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O3 -march=native)
endif()