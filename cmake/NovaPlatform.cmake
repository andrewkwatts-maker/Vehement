# =============================================================================
# NovaPlatform.cmake
# Nova3D Engine Platform Detection and Configuration
# =============================================================================
# Include this file to detect and configure the target platform
# Usage: include(cmake/NovaPlatform.cmake)

# =============================================================================
# Platform Detection
# =============================================================================

# Detect platform
if(WIN32)
    set(NOVA_PLATFORM "Windows")
    set(NOVA_PLATFORM_WINDOWS TRUE)
    set(NOVA_PLATFORM_DESKTOP TRUE)
elseif(ANDROID)
    set(NOVA_PLATFORM "Android")
    set(NOVA_PLATFORM_ANDROID TRUE)
    set(NOVA_PLATFORM_MOBILE TRUE)
elseif(IOS)
    set(NOVA_PLATFORM "iOS")
    set(NOVA_PLATFORM_IOS TRUE)
    set(NOVA_PLATFORM_MOBILE TRUE)
    set(NOVA_PLATFORM_APPLE TRUE)
elseif(APPLE)
    set(NOVA_PLATFORM "macOS")
    set(NOVA_PLATFORM_MACOS TRUE)
    set(NOVA_PLATFORM_DESKTOP TRUE)
    set(NOVA_PLATFORM_APPLE TRUE)
elseif(EMSCRIPTEN)
    set(NOVA_PLATFORM "Web")
    set(NOVA_PLATFORM_WEB TRUE)
elseif(UNIX)
    set(NOVA_PLATFORM "Linux")
    set(NOVA_PLATFORM_LINUX TRUE)
    set(NOVA_PLATFORM_DESKTOP TRUE)
endif()

# =============================================================================
# Architecture Detection
# =============================================================================

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(NOVA_ARCH_64BIT TRUE)
else()
    set(NOVA_ARCH_32BIT TRUE)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|aarch64|ARM64")
    set(NOVA_ARCH_ARM TRUE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(NOVA_ARCH_ARM64 TRUE)
    else()
        set(NOVA_ARCH_ARM32 TRUE)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x64")
    set(NOVA_ARCH_X86 TRUE)
    set(NOVA_ARCH_X64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86")
    set(NOVA_ARCH_X86 TRUE)
    set(NOVA_ARCH_X32 TRUE)
endif()

# =============================================================================
# Graphics API Detection
# =============================================================================

# Function to check for Vulkan support
function(nova_check_vulkan RESULT_VAR)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Function to check for Metal support (Apple only)
function(nova_check_metal RESULT_VAR)
    if(NOVA_PLATFORM_APPLE)
        set(${RESULT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${RESULT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# =============================================================================
# Compiler Detection
# =============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(NOVA_COMPILER_CLANG TRUE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        set(NOVA_COMPILER_APPLECLANG TRUE)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(NOVA_COMPILER_GCC TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(NOVA_COMPILER_MSVC TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set(NOVA_COMPILER_INTEL TRUE)
endif()

# =============================================================================
# Platform Configuration Function
# =============================================================================

# Function to configure platform-specific settings
function(nova_configure_platform TARGET)
    # Add platform definition
    target_compile_definitions(${TARGET} PRIVATE NOVA_PLATFORM_${NOVA_PLATFORM})

    # Platform-specific compile options
    if(NOVA_COMPILER_MSVC)
        target_compile_options(${TARGET} PRIVATE
            /W4           # Warning level 4
            /permissive-  # Strict conformance
            /Zc:__cplusplus  # Correct __cplusplus macro
        )
    elseif(NOVA_COMPILER_GCC OR NOVA_COMPILER_CLANG)
        target_compile_options(${TARGET} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    endif()

    # Architecture-specific options
    if(NOVA_ARCH_X64 AND NOT NOVA_COMPILER_MSVC)
        target_compile_options(${TARGET} PRIVATE -march=x86-64)
    endif()

    # Enable SIMD where appropriate
    if(NOVA_ARCH_X86 AND NOT EMSCRIPTEN)
        if(NOVA_COMPILER_GCC OR NOVA_COMPILER_CLANG)
            target_compile_options(${TARGET} PRIVATE -msse4.1 -mavx)
        elseif(NOVA_COMPILER_MSVC)
            target_compile_options(${TARGET} PRIVATE /arch:AVX)
        endif()
    endif()
endfunction()

# =============================================================================
# Print Configuration
# =============================================================================

function(nova_print_platform_info)
    message(STATUS "")
    message(STATUS "Nova3D Platform Configuration:")
    message(STATUS "  Platform:     ${NOVA_PLATFORM}")
    message(STATUS "  Processor:    ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "  Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  Architecture: ${CMAKE_SIZEOF_VOID_P}-bit")

    if(NOVA_PLATFORM_DESKTOP)
        message(STATUS "  Type:         Desktop")
    elseif(NOVA_PLATFORM_MOBILE)
        message(STATUS "  Type:         Mobile")
    elseif(NOVA_PLATFORM_WEB)
        message(STATUS "  Type:         Web")
    endif()

    message(STATUS "")
endfunction()
