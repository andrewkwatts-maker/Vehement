# ============================================================================
# Android NDK Toolchain Configuration for Nova3D Engine
# ============================================================================
#
# This toolchain file configures CMake to cross-compile for Android using
# the Android NDK. It should be used with the NDK's built-in toolchain file
# or as a standalone configuration.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/android.toolchain.cmake \
#         -DANDROID_ABI=arm64-v8a \
#         -DANDROID_PLATFORM=android-24 \
#         ..
#
# Or use with NDK's toolchain:
#   cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
#         -DNOVA3D_ANDROID_CONFIG=ON \
#         ..
# ============================================================================

cmake_minimum_required(VERSION 3.18)

# ============================================================================
# Check if using NDK's built-in toolchain
# ============================================================================

if(DEFINED ANDROID_NDK AND EXISTS "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    # Use NDK's toolchain as base
    include("${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    set(NOVA3D_USING_NDK_TOOLCHAIN TRUE)
else()
    set(NOVA3D_USING_NDK_TOOLCHAIN FALSE)
endif()

# ============================================================================
# Nova3D Android Configuration
# ============================================================================

# Default configuration values
if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI")
endif()

if(NOT DEFINED ANDROID_PLATFORM)
    set(ANDROID_PLATFORM "android-24" CACHE STRING "Android API level")
endif()

if(NOT DEFINED ANDROID_STL)
    set(ANDROID_STL "c++_shared" CACHE STRING "Android STL")
endif()

# Extract API level number
string(REGEX REPLACE "android-" "" ANDROID_API_LEVEL "${ANDROID_PLATFORM}")

# ============================================================================
# Compiler and Linker Flags
# ============================================================================

# Common flags for all builds
set(NOVA3D_ANDROID_COMMON_FLAGS
    "-ffunction-sections"
    "-fdata-sections"
    "-fvisibility=hidden"
    "-fvisibility-inlines-hidden"
)

# Debug flags
set(NOVA3D_ANDROID_DEBUG_FLAGS
    "-O0"
    "-g"
    "-DDEBUG"
    "-D_DEBUG"
)

# Release flags
set(NOVA3D_ANDROID_RELEASE_FLAGS
    "-O3"
    "-DNDEBUG"
    "-flto"
)

# Linker flags
set(NOVA3D_ANDROID_LINKER_FLAGS
    "-Wl,--gc-sections"
    "-Wl,--build-id=sha1"
)

# Convert lists to strings
string(REPLACE ";" " " NOVA3D_ANDROID_COMMON_FLAGS_STR "${NOVA3D_ANDROID_COMMON_FLAGS}")
string(REPLACE ";" " " NOVA3D_ANDROID_DEBUG_FLAGS_STR "${NOVA3D_ANDROID_DEBUG_FLAGS}")
string(REPLACE ";" " " NOVA3D_ANDROID_RELEASE_FLAGS_STR "${NOVA3D_ANDROID_RELEASE_FLAGS}")
string(REPLACE ";" " " NOVA3D_ANDROID_LINKER_FLAGS_STR "${NOVA3D_ANDROID_LINKER_FLAGS}")

# Apply flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NOVA3D_ANDROID_COMMON_FLAGS_STR}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NOVA3D_ANDROID_COMMON_FLAGS_STR}")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${NOVA3D_ANDROID_DEBUG_FLAGS_STR}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${NOVA3D_ANDROID_DEBUG_FLAGS_STR}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${NOVA3D_ANDROID_RELEASE_FLAGS_STR}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${NOVA3D_ANDROID_RELEASE_FLAGS_STR}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${NOVA3D_ANDROID_LINKER_FLAGS_STR}")

# ============================================================================
# Feature Detection
# ============================================================================

# Check for Vulkan support (API 24+)
if(ANDROID_API_LEVEL GREATER_EQUAL 24)
    set(NOVA3D_ANDROID_VULKAN_SUPPORT ON)
else()
    set(NOVA3D_ANDROID_VULKAN_SUPPORT OFF)
endif()

# Check for GLES 3.2 support (API 24+)
if(ANDROID_API_LEVEL GREATER_EQUAL 24)
    set(NOVA3D_ANDROID_GLES32_SUPPORT ON)
else()
    set(NOVA3D_ANDROID_GLES32_SUPPORT OFF)
endif()

# ============================================================================
# Find Required Libraries
# ============================================================================

function(nova3d_find_android_libraries)
    # Android system libraries
    find_library(ANDROID_LOG_LIB log)
    find_library(ANDROID_LIB android)
    find_library(ANDROID_EGL_LIB EGL)
    find_library(ANDROID_GLES3_LIB GLESv3)
    find_library(ANDROID_GLES2_LIB GLESv2)

    # Vulkan (if supported)
    if(NOVA3D_ANDROID_VULKAN_SUPPORT)
        find_library(ANDROID_VULKAN_LIB vulkan)
    endif()

    # Export to parent scope
    set(ANDROID_LOG_LIB ${ANDROID_LOG_LIB} PARENT_SCOPE)
    set(ANDROID_LIB ${ANDROID_LIB} PARENT_SCOPE)
    set(ANDROID_EGL_LIB ${ANDROID_EGL_LIB} PARENT_SCOPE)
    set(ANDROID_GLES3_LIB ${ANDROID_GLES3_LIB} PARENT_SCOPE)
    set(ANDROID_GLES2_LIB ${ANDROID_GLES2_LIB} PARENT_SCOPE)
    set(ANDROID_VULKAN_LIB ${ANDROID_VULKAN_LIB} PARENT_SCOPE)
endfunction()

# ============================================================================
# Nova3D Android Library Target
# ============================================================================

function(nova3d_add_android_library TARGET_NAME)
    # Parse arguments
    cmake_parse_arguments(
        NOVA3D_LIB
        "ENABLE_VULKAN;ENABLE_GLES"
        ""
        "SOURCES;INCLUDES;DEPENDENCIES"
        ${ARGN}
    )

    # Find Android libraries
    nova3d_find_android_libraries()

    # Create shared library
    add_library(${TARGET_NAME} SHARED ${NOVA3D_LIB_SOURCES})

    # Set C++ standard
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)

    # Include directories
    target_include_directories(${TARGET_NAME} PRIVATE
        ${NOVA3D_LIB_INCLUDES}
        ${CMAKE_CURRENT_SOURCE_DIR}/engine
    )

    # Link Android libraries
    target_link_libraries(${TARGET_NAME} PRIVATE
        ${ANDROID_LOG_LIB}
        ${ANDROID_LIB}
    )

    # Link GLES if enabled
    if(NOVA3D_LIB_ENABLE_GLES OR NOT NOVA3D_LIB_ENABLE_VULKAN)
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${ANDROID_EGL_LIB}
            ${ANDROID_GLES3_LIB}
        )
        target_compile_definitions(${TARGET_NAME} PRIVATE NOVA3D_GLES_ENABLED)
    endif()

    # Link Vulkan if enabled and supported
    if(NOVA3D_LIB_ENABLE_VULKAN AND NOVA3D_ANDROID_VULKAN_SUPPORT)
        target_link_libraries(${TARGET_NAME} PRIVATE
            ${ANDROID_VULKAN_LIB}
        )
        target_compile_definitions(${TARGET_NAME} PRIVATE NOVA3D_VULKAN_ENABLED)
    endif()

    # Link additional dependencies
    if(NOVA3D_LIB_DEPENDENCIES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${NOVA3D_LIB_DEPENDENCIES})
    endif()

    # Set library properties
    set_target_properties(${TARGET_NAME} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/libs/${ANDROID_ABI}"
    )
endfunction()

# ============================================================================
# Utility Macros
# ============================================================================

# Copy assets to APK
macro(nova3d_copy_assets TARGET_NAME ASSETS_DIR)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ASSETS_DIR}
            ${CMAKE_BINARY_DIR}/assets
        COMMENT "Copying assets to build directory"
    )
endmacro()

# Generate SPIR-V shaders
function(nova3d_compile_shaders TARGET_NAME)
    cmake_parse_arguments(
        SHADERS
        ""
        "OUTPUT_DIR"
        "SOURCES"
        ${ARGN}
    )

    if(NOT SHADERS_OUTPUT_DIR)
        set(SHADERS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    endif()

    foreach(SHADER_SOURCE ${SHADERS_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        set(SHADER_OUTPUT "${SHADERS_OUTPUT_DIR}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND glslc -o ${SHADER_OUTPUT} ${SHADER_SOURCE}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling shader ${SHADER_NAME}"
        )

        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endforeach()

    add_custom_target(${TARGET_NAME}_shaders DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_shaders)
endfunction()

# ============================================================================
# Print Configuration Summary
# ============================================================================

message(STATUS "")
message(STATUS "Nova3D Android Configuration:")
message(STATUS "  ABI: ${ANDROID_ABI}")
message(STATUS "  Platform: ${ANDROID_PLATFORM} (API ${ANDROID_API_LEVEL})")
message(STATUS "  STL: ${ANDROID_STL}")
message(STATUS "  Vulkan Support: ${NOVA3D_ANDROID_VULKAN_SUPPORT}")
message(STATUS "  GLES 3.2 Support: ${NOVA3D_ANDROID_GLES32_SUPPORT}")
message(STATUS "")
