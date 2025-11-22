# =============================================================================
# Linux ARM64 (aarch64) Toolchain
# =============================================================================
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Linux-aarch64.cmake ..
# For cross-compilation, set AARCH64_TOOLCHAIN_PATH environment variable

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Find toolchain
if(DEFINED ENV{AARCH64_TOOLCHAIN_PATH})
    set(TOOLCHAIN_PATH $ENV{AARCH64_TOOLCHAIN_PATH})
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/bin/aarch64-linux-gnu-g++)
    set(CMAKE_SYSROOT ${TOOLCHAIN_PATH}/sysroot)
else()
    # Native compilation or system toolchain
    set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find root path mode
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform definition
add_definitions(-DNOVA_PLATFORM_LINUX)

message(STATUS "Using Linux aarch64 toolchain")
