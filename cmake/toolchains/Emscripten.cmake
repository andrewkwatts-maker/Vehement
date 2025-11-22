# =============================================================================
# Emscripten (WebAssembly) Toolchain
# =============================================================================
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Emscripten.cmake ..
#
# Requires Emscripten SDK to be installed and activated:
#   source /path/to/emsdk/emsdk_env.sh

# Check for Emscripten
if(NOT DEFINED ENV{EMSDK})
    message(FATAL_ERROR "Emscripten SDK not found. Please activate emsdk_env.sh")
endif()

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# Emscripten compilers
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)
set(CMAKE_AR emar)
set(CMAKE_RANLIB emranlib)

# Enable modern C++ features (limited by Emscripten support)
set(CMAKE_CXX_STANDARD 20)  # C++23 support may be limited
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform definition
add_definitions(-DNOVA_PLATFORM_WEB)

# Emscripten compile flags
set(EMSCRIPTEN_FLAGS "-s USE_WEBGL2=1 -s FULL_ES3=1 -s WASM=1")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s MAX_WEBGL_VERSION=2")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s MIN_WEBGL_VERSION=2")

# Debug flags
set(EMSCRIPTEN_DEBUG_FLAGS "-s ASSERTIONS=1 -s DEMANGLE_SUPPORT=1")

# Release flags
set(EMSCRIPTEN_RELEASE_FLAGS "-O3 --closure 1")

set(CMAKE_C_FLAGS_INIT "${EMSCRIPTEN_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${EMSCRIPTEN_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG_INIT "${EMSCRIPTEN_DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "${EMSCRIPTEN_DEBUG_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE_INIT "${EMSCRIPTEN_RELEASE_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "${EMSCRIPTEN_RELEASE_FLAGS}")

# Link flags for final executable
set(CMAKE_EXE_LINKER_FLAGS_INIT "${EMSCRIPTEN_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT "${EMSCRIPTEN_DEBUG_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT "${EMSCRIPTEN_RELEASE_FLAGS}")

# Output HTML instead of just .wasm
set(CMAKE_EXECUTABLE_SUFFIX ".html")

# Find root path mode
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

message(STATUS "Using Emscripten toolchain")
message(STATUS "EMSDK: $ENV{EMSDK}")
