# =============================================================================
# Windows MSVC Toolchain
# =============================================================================
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Windows-MSVC.cmake ..
# Note: Usually not needed when building natively on Windows with MSVC

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC-specific flags
set(CMAKE_CXX_FLAGS_INIT "/EHsc /W4")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "/O2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "/Od /Zi /RTC1")

# Platform definitions
add_definitions(-DNOVA_PLATFORM_WINDOWS)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DNOMINMAX)
add_definitions(-DWIN32_LEAN_AND_MEAN)

# Use static runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

message(STATUS "Using Windows MSVC toolchain")
