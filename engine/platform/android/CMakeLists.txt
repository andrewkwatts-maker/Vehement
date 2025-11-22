# ============================================================================
# Nova3D Android Platform CMake Configuration
# ============================================================================

cmake_minimum_required(VERSION 3.18)

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
)

# ============================================================================
# Find Required Libraries
# ============================================================================

find_library(LOG_LIB log)
find_library(ANDROID_LIB android)
find_library(EGL_LIB EGL)
find_library(GLESV3_LIB GLESv3)

# Optional Vulkan support
find_library(VULKAN_LIB vulkan)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Link required libraries
target_link_libraries(nova3d_android PUBLIC
    ${LOG_LIB}
    ${ANDROID_LIB}
    ${EGL_LIB}
    ${GLESV3_LIB}
)

# Link Vulkan if available
if(VULKAN_LIB)
    target_link_libraries(nova3d_android PUBLIC ${VULKAN_LIB})
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_VULKAN_AVAILABLE)
endif()

# GLM for math
# find_package(glm REQUIRED)
# target_link_libraries(nova3d_android PUBLIC glm::glm)

# ============================================================================
# Compile Definitions
# ============================================================================

target_compile_definitions(nova3d_android PUBLIC
    NOVA3D_PLATFORM_ANDROID
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(nova3d_android PUBLIC NOVA3D_DEBUG)
endif()

# ============================================================================
# Compiler Options
# ============================================================================

target_compile_options(nova3d_android PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS nova3d_android
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${NOVA3D_ANDROID_HEADERS}
    DESTINATION include/nova3d/platform/android
)
