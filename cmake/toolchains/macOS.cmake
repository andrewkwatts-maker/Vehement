# =============================================================================
# macOS Toolchain
# =============================================================================
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macOS.cmake ..

set(CMAKE_SYSTEM_NAME Darwin)

# Detect architecture (arm64 for Apple Silicon, x86_64 for Intel)
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CMAKE_OSX_ARCHITECTURES ${ARCH})
endif()

set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_OSX_ARCHITECTURES})

# Use Clang (default on macOS)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Minimum deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform definition
add_definitions(-DNOVA_PLATFORM_MACOS)

# Universal binary support (optional)
# set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

# Framework paths
set(CMAKE_FRAMEWORK_PATH /Library/Frameworks)

message(STATUS "Using macOS toolchain (${CMAKE_OSX_ARCHITECTURES})")
message(STATUS "Deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
