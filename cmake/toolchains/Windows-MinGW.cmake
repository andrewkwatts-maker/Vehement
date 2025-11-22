# =============================================================================
# Windows MinGW Toolchain (Cross-compilation from Linux)
# =============================================================================
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Windows-MinGW.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MinGW toolchain (adjust paths as needed)
set(MINGW_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER ${MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${MINGW_PREFIX}-windres)

# Root path
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX})

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform definitions
add_definitions(-DNOVA_PLATFORM_WINDOWS)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DNOMINMAX)
add_definitions(-DWIN32_LEAN_AND_MEAN)

# Link statically for better portability
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

message(STATUS "Using Windows MinGW cross-compilation toolchain")
