# ============================================================================
# Nova3D Android Platform CMake Configuration
# ============================================================================
#
# This CMakeLists.txt builds the Android platform layer for Nova3D Engine.
#
# Supported architectures:
#   - arm64-v8a (ARM64, recommended)
#   - armeabi-v7a (ARM32, legacy support)
#   - x86_64 (Emulator)
#   - x86 (Emulator, legacy)
#
# Minimum API Level: 24 (Android 7.0)
# Target API Level: 34 (Android 14)
#
# Usage with Android NDK:
#   cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
#         -DANDROID_ABI=arm64-v8a \
#         -DANDROID_PLATFORM=android-24 \
#         -DCMAKE_BUILD_TYPE=Release \
#         ..
#
# ============================================================================

cmake_minimum_required(VERSION 3.18)

# ============================================================================
# Android NDK Configuration
# ============================================================================

# Ensure we're building for Android
if(NOT ANDROID)
    message(FATAL_ERROR "This CMakeLists.txt is for Android builds only!")
endif()

# Minimum and target API levels
set(NOVA3D_ANDROID_MIN_SDK_VERSION 24)
set(NOVA3D_ANDROID_TARGET_SDK_VERSION 34)

# Verify API level meets minimum requirement
if(ANDROID_NATIVE_API_LEVEL LESS NOVA3D_ANDROID_MIN_SDK_VERSION)
    message(WARNING "Android API level ${ANDROID_NATIVE_API_LEVEL} is below minimum ${NOVA3D_ANDROID_MIN_SDK_VERSION}. Vulkan support may be limited.")
endif()

# Architecture-specific optimizations
if(ANDROID_ABI STREQUAL "arm64-v8a")
    message(STATUS "Building for ARM64 (arm64-v8a)")
    set(NOVA3D_ARM64 TRUE)
    # Enable NEON by default on ARM64
    add_compile_options(-march=armv8-a)
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    message(STATUS "Building for ARM32 (armeabi-v7a)")
    set(NOVA3D_ARM32 TRUE)
    # Enable NEON for ARM32
    add_compile_options(-march=armv7-a -mfpu=neon -mfloat-abi=softfp)
elseif(ANDROID_ABI STREQUAL "x86_64")
    message(STATUS "Building for x86_64 (emulator)")
    set(NOVA3D_X86_64 TRUE)
elseif(ANDROID_ABI STREQUAL "x86")
    message(STATUS "Building for x86 (emulator)")
    set(NOVA3D_X86 TRUE)
else()
    message(WARNING "Unknown ABI: ${ANDROID_ABI}")
endif()

# ============================================================================
# Platform Sources
# ============================================================================

set(NOVA3D_ANDROID_SOURCES
    AndroidPlatform.cpp
    AndroidGLES.cpp
    VulkanRenderer.cpp
    AndroidTouchInput.cpp
    AndroidJNI.cpp
)

set(NOVA3D_ANDROID_HEADERS
    AndroidPlatform.hpp
    AndroidGLES.hpp
    VulkanRenderer.hpp
    AndroidTouchInput.hpp
    AndroidLocation.hpp
)

# ============================================================================
# Find Required Libraries
# ============================================================================

find_library(LOG_LIB log)
find_library(ANDROID_LIB android)
find_library(EGL_LIB EGL)
find_library(GLESV3_LIB GLESv3)
find_library(JNIGRAPHICS_LIB jnigraphics)
find_library(MEDIANDK_LIB mediandk)

# Optional Vulkan support (API 24+)
find_library(VULKAN_LIB vulkan)

# Optional Camera support
find_library(CAMERA2NDK_LIB camera2ndk)

# Optional neural networks (API 27+)
find_library(NEURALNETWORKS_LIB neuralnetworks)

# ============================================================================
# Create Android Platform Library
# ============================================================================

add_library(nova3d_android STATIC
    ${NOVA3D_ANDROID_SOURCES}
    ${NOVA3D_ANDROID_HEADERS}
)

target_compile_features(nova3d_android PUBLIC cxx_std_17)

target_include_directories(nova3d_android PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# ============================================================================
# Link Required Libraries
# ============================================================================

target_link_libraries(nova3d_android PUBLIC
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${EGL_LIB}
    ${GLESV3_LIB}
)

# Link JNI graphics for bitmap support
if(JNIGRAPHICS_LIB)
    target_link_libraries(nova3d_android PUBLIC ${JNIGRAPHICS_LIB})
endif()

# Link Vulkan if available (API 24+)
if(VULKAN_LIB)
    target_link_libraries(nova3d_android PUBLIC ${VULKAN_LIB})
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_VULKAN_AVAILABLE)
    message(STATUS "Vulkan support: ENABLED")
else()
    message(STATUS "Vulkan support: DISABLED (library not found)")
endif()

# Link Media NDK for hardware codecs
if(MEDIANDK_LIB)
    target_link_libraries(nova3d_android PUBLIC ${MEDIANDK_LIB})
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_MEDIANDK_AVAILABLE)
endif()

# Link Camera2 NDK for camera support
if(CAMERA2NDK_LIB)
    target_link_libraries(nova3d_android PUBLIC ${CAMERA2NDK_LIB})
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_CAMERA2_AVAILABLE)
endif()

# Link Neural Networks API (TensorFlow Lite delegate)
if(NEURALNETWORKS_LIB AND ANDROID_NATIVE_API_LEVEL GREATER_EQUAL 27)
    target_link_libraries(nova3d_android PUBLIC ${NEURALNETWORKS_LIB})
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_NNAPI_AVAILABLE)
endif()

# ============================================================================
# Compile Definitions
# ============================================================================

target_compile_definitions(nova3d_android PUBLIC
    NOVA3D_PLATFORM_ANDROID
    NOVA3D_ANDROID_API_LEVEL=${ANDROID_NATIVE_API_LEVEL}
    NOVA3D_ANDROID_MIN_SDK=${NOVA3D_ANDROID_MIN_SDK_VERSION}
    NOVA3D_ANDROID_TARGET_SDK=${NOVA3D_ANDROID_TARGET_SDK_VERSION}
)

# Architecture-specific definitions
if(NOVA3D_ARM64)
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_ARCH_ARM64)
elseif(NOVA3D_ARM32)
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_ARCH_ARM32)
elseif(NOVA3D_X86_64)
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_ARCH_X86_64)
elseif(NOVA3D_X86)
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_ARCH_X86)
endif()

# Debug/Release configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(nova3d_android PUBLIC
        NOVA3D_DEBUG
        NOVA3D_ENABLE_VALIDATION
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_RELEASE)
endif()

# ============================================================================
# Compiler Options
# ============================================================================

target_compile_options(nova3d_android PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

# Release optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(nova3d_android PRIVATE
        -O3
        -flto
        -fno-exceptions
    )
    target_link_options(nova3d_android PRIVATE
        -Wl,--gc-sections
        -Wl,--strip-all
    )
endif()

# Debug symbols
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(nova3d_android PRIVATE
        -g3
        -O0
    )
endif()

# ============================================================================
# STL Configuration
# ============================================================================

# Use c++_shared for smaller APK size (requires packaging libstdc++.so)
# Or use c++_static for self-contained library
if(ANDROID_STL STREQUAL "c++_shared")
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_STL_SHARED)
elseif(ANDROID_STL STREQUAL "c++_static")
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_STL_STATIC)
endif()

# ============================================================================
# Export Headers
# ============================================================================

set_target_properties(nova3d_android PROPERTIES
    PUBLIC_HEADER "${NOVA3D_ANDROID_HEADERS}"
)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS nova3d_android
    ARCHIVE DESTINATION lib/${ANDROID_ABI}
    LIBRARY DESTINATION lib/${ANDROID_ABI}
    PUBLIC_HEADER DESTINATION include/nova3d/platform/android
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Nova3D Android Platform Configuration:")
message(STATUS "  ABI:              ${ANDROID_ABI}")
message(STATUS "  API Level:        ${ANDROID_NATIVE_API_LEVEL}")
message(STATUS "  Min SDK:          ${NOVA3D_ANDROID_MIN_SDK_VERSION}")
message(STATUS "  Target SDK:       ${NOVA3D_ANDROID_TARGET_SDK_VERSION}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  STL:              ${ANDROID_STL}")
if(VULKAN_LIB)
    message(STATUS "  Vulkan:           YES")
else()
    message(STATUS "  Vulkan:           NO")
endif()
message(STATUS "")
