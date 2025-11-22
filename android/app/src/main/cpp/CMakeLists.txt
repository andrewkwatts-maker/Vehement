# ============================================================================
# Nova3D Android Native Library CMakeLists.txt
# ============================================================================

cmake_minimum_required(VERSION 3.18)

project(nova3d VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard and Compiler Flags
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Source Files
# ============================================================================

# Engine source directory (relative to android/app/src/main/cpp)
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine")

# Platform-specific sources
set(ANDROID_PLATFORM_SOURCES
    ${ENGINE_DIR}/platform/android/AndroidPlatform.cpp
    ${ENGINE_DIR}/platform/android/AndroidGLES.cpp
    ${ENGINE_DIR}/platform/android/VulkanRenderer.cpp
    ${ENGINE_DIR}/platform/android/AndroidTouchInput.cpp
    ${ENGINE_DIR}/platform/android/AndroidJNI.cpp
)

# Core engine sources (add your engine sources here)
set(ENGINE_CORE_SOURCES
    # ${ENGINE_DIR}/core/Engine.cpp
    # ${ENGINE_DIR}/core/Window.cpp
    # ${ENGINE_DIR}/core/Time.cpp
    # ${ENGINE_DIR}/graphics/Renderer.cpp
    # Add more as needed
)

# ============================================================================
# Find Libraries
# ============================================================================

find_library(log-lib log)
find_library(android-lib android)
find_library(EGL-lib EGL)
find_library(GLESv3-lib GLESv3)
find_library(vulkan-lib vulkan)

# ============================================================================
# Create Native Library
# ============================================================================

add_library(nova3d SHARED
    ${ANDROID_PLATFORM_SOURCES}
    ${ENGINE_CORE_SOURCES}
)

# ============================================================================
# Include Directories
# ============================================================================

target_include_directories(nova3d PRIVATE
    ${ENGINE_DIR}
    ${ENGINE_DIR}/platform/android
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# GLM (math library)
# Option 1: If GLM is included in your project
# target_include_directories(nova3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/glm)

# Option 2: Use a pre-built GLM or fetch it
# include(FetchContent)
# FetchContent_Declare(
#     glm
#     GIT_REPOSITORY https://github.com/g-truc/glm.git
#     GIT_TAG 0.9.9.8
# )
# FetchContent_MakeAvailable(glm)
# target_link_libraries(nova3d PRIVATE glm::glm)

# ============================================================================
# Link Libraries
# ============================================================================

target_link_libraries(nova3d
    ${log-lib}
    ${android-lib}
    ${EGL-lib}
    ${GLESv3-lib}
)

# Link Vulkan if available
if(vulkan-lib)
    target_link_libraries(nova3d ${vulkan-lib})
    target_compile_definitions(nova3d PRIVATE NOVA3D_VULKAN_AVAILABLE)
endif()

# ============================================================================
# Compile Definitions
# ============================================================================

target_compile_definitions(nova3d PRIVATE
    NOVA3D_PLATFORM_ANDROID
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(nova3d PRIVATE
        NOVA3D_DEBUG
        NOVA3D_VALIDATION_LAYERS
    )
endif()

# ============================================================================
# Compile Options
# ============================================================================

target_compile_options(nova3d PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -fvisibility=hidden
)

# Release optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(nova3d PRIVATE
        -O3
        -flto
    )
endif()

# ============================================================================
# Strip Debug Symbols in Release
# ============================================================================

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET nova3d POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:nova3d>
    )
endif()
