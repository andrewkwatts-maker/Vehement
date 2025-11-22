# =============================================================================
# Android Toolchain
# =============================================================================
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Android.cmake \
#              -DANDROID_NDK=/path/to/ndk \
#              -DANDROID_ABI=arm64-v8a \
#              -DANDROID_PLATFORM=android-26 ..
#
# Alternatively, use the NDK's built-in toolchain:
#   cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake ...

# Check for NDK path
if(NOT DEFINED ANDROID_NDK)
    if(DEFINED ENV{ANDROID_NDK_HOME})
        set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
    elseif(DEFINED ENV{ANDROID_NDK})
        set(ANDROID_NDK $ENV{ANDROID_NDK})
    else()
        message(FATAL_ERROR "ANDROID_NDK must be set to the Android NDK path")
    endif()
endif()

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_ANDROID_NDK ${ANDROID_NDK})

# Default ABI if not specified
if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI")
endif()
set(CMAKE_ANDROID_ARCH_ABI ${ANDROID_ABI})

# Minimum API level
if(NOT DEFINED ANDROID_PLATFORM)
    set(ANDROID_PLATFORM "android-26" CACHE STRING "Android API level")
endif()
set(CMAKE_ANDROID_API ${ANDROID_PLATFORM})
set(CMAKE_SYSTEM_VERSION ${CMAKE_ANDROID_API})

# Use clang
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)

# STL - use libc++
set(CMAKE_ANDROID_STL_TYPE c++_shared)

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform definition
add_definitions(-DNOVA_PLATFORM_ANDROID)

# Recommended: Use NDK's toolchain file for better compatibility
# This file is a simplified version; for production, use:
# include(${ANDROID_NDK}/build/cmake/android.toolchain.cmake)

message(STATUS "Using Android toolchain")
message(STATUS "NDK path: ${ANDROID_NDK}")
message(STATUS "ABI: ${ANDROID_ABI}")
message(STATUS "Platform: ${ANDROID_PLATFORM}")
