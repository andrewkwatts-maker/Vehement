# ============================================================================
# Nova3D Linux Platform CMake Configuration
# ============================================================================
#
# This CMakeLists.txt builds the Linux platform layer for Nova3D Engine.
#
# Supports:
#   - X11 window system
#   - Wayland window system (via GLFW)
#   - OpenGL 4.6 rendering
#   - Vulkan rendering (optional)
#   - PulseAudio/ALSA audio
#
# Usage:
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Release ..
#   cmake --build .
#
# ============================================================================

cmake_minimum_required(VERSION 3.18)

# ============================================================================
# Platform Detection
# ============================================================================

if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "This CMakeLists.txt is for Linux builds only!")
endif()

# ============================================================================
# Options
# ============================================================================

option(NOVA3D_LINUX_VULKAN "Enable Vulkan support" ON)
option(NOVA3D_LINUX_WAYLAND "Enable Wayland support" ON)
option(NOVA3D_LINUX_X11 "Enable X11 support" ON)
option(NOVA3D_LINUX_PULSEAUDIO "Enable PulseAudio support" ON)
option(NOVA3D_LINUX_ALSA "Enable ALSA support" ON)

# ============================================================================
# Platform Sources
# ============================================================================

set(NOVA3D_LINUX_SOURCES
    LinuxPlatform.cpp
)

set(NOVA3D_LINUX_HEADERS
    LinuxPlatform.hpp
)

# Optional Vulkan support
if(NOVA3D_LINUX_VULKAN)
    list(APPEND NOVA3D_LINUX_SOURCES LinuxVulkan.cpp)
    list(APPEND NOVA3D_LINUX_HEADERS LinuxVulkan.hpp)
endif()

# Optional Audio support
if(NOVA3D_LINUX_PULSEAUDIO OR NOVA3D_LINUX_ALSA)
    list(APPEND NOVA3D_LINUX_SOURCES LinuxAudio.cpp)
    list(APPEND NOVA3D_LINUX_HEADERS LinuxAudio.hpp)
endif()

# ============================================================================
# Find Required Packages
# ============================================================================

# GLFW for windowing
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW not found, will be fetched via FetchContent")
endif()

# X11 support
if(NOVA3D_LINUX_X11)
    find_package(X11)
    if(X11_FOUND)
        message(STATUS "X11 found: ${X11_LIBRARIES}")
    else()
        message(WARNING "X11 not found, X11 support disabled")
        set(NOVA3D_LINUX_X11 OFF)
    endif()
endif()

# Wayland support
if(NOVA3D_LINUX_WAYLAND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND wayland-client wayland-egl)
        if(WAYLAND_FOUND)
            message(STATUS "Wayland found")
        else()
            message(STATUS "Wayland not found, Wayland support disabled")
            set(NOVA3D_LINUX_WAYLAND OFF)
        endif()
    endif()
endif()

# Vulkan support
if(NOVA3D_LINUX_VULKAN)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        message(STATUS "Vulkan found: ${Vulkan_LIBRARIES}")
    else()
        message(STATUS "Vulkan not found, Vulkan support disabled")
        set(NOVA3D_LINUX_VULKAN OFF)
    endif()
endif()

# OpenGL support
find_package(OpenGL REQUIRED)
message(STATUS "OpenGL found: ${OPENGL_LIBRARIES}")

# Threading
find_package(Threads REQUIRED)

# ============================================================================
# Create Linux Platform Library
# ============================================================================

add_library(nova3d_linux STATIC
    ${NOVA3D_LINUX_SOURCES}
    ${NOVA3D_LINUX_HEADERS}
)

target_compile_features(nova3d_linux PUBLIC cxx_std_17)

target_include_directories(nova3d_linux PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# ============================================================================
# Link Libraries
# ============================================================================

# Base libraries
target_link_libraries(nova3d_linux PUBLIC
    ${OPENGL_LIBRARIES}
    Threads::Threads
    dl
)

# GLFW
if(glfw3_FOUND)
    target_link_libraries(nova3d_linux PUBLIC glfw)
endif()

# X11 support
if(NOVA3D_LINUX_X11 AND X11_FOUND)
    target_link_libraries(nova3d_linux PUBLIC ${X11_LIBRARIES})
    target_compile_definitions(nova3d_linux PUBLIC
        GLFW_EXPOSE_NATIVE_X11
        NOVA3D_HAS_X11
    )

    if(X11_Xrandr_FOUND)
        target_link_libraries(nova3d_linux PUBLIC ${X11_Xrandr_LIB})
    endif()

    if(X11_Xcursor_FOUND)
        target_link_libraries(nova3d_linux PUBLIC ${X11_Xcursor_LIB})
    endif()

    if(X11_Xi_FOUND)
        target_link_libraries(nova3d_linux PUBLIC ${X11_Xi_LIB})
    endif()
endif()

# Wayland support
if(NOVA3D_LINUX_WAYLAND AND WAYLAND_FOUND)
    target_link_libraries(nova3d_linux PUBLIC ${WAYLAND_LIBRARIES})
    target_include_directories(nova3d_linux PUBLIC ${WAYLAND_INCLUDE_DIRS})
    target_compile_definitions(nova3d_linux PUBLIC
        GLFW_EXPOSE_NATIVE_WAYLAND
        NOVA3D_HAS_WAYLAND
    )
endif()

# Vulkan support
if(NOVA3D_LINUX_VULKAN AND Vulkan_FOUND)
    target_link_libraries(nova3d_linux PUBLIC Vulkan::Vulkan)
    target_compile_definitions(nova3d_linux PUBLIC
        NOVA3D_HAS_VULKAN
        VK_USE_PLATFORM_XLIB_KHR
        VK_USE_PLATFORM_WAYLAND_KHR
    )
endif()

# ============================================================================
# Compile Definitions
# ============================================================================

target_compile_definitions(nova3d_linux PUBLIC
    NOVA3D_PLATFORM_LINUX
    NOVA_PLATFORM_LINUX
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(nova3d_linux PUBLIC
        NOVA3D_DEBUG
        NOVA3D_ENABLE_VALIDATION
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(nova3d_linux PUBLIC NOVA3D_RELEASE)
endif()

# Audio backend definitions
if(NOVA3D_LINUX_PULSEAUDIO)
    target_compile_definitions(nova3d_linux PUBLIC NOVA3D_HAS_PULSEAUDIO)
endif()

if(NOVA3D_LINUX_ALSA)
    target_compile_definitions(nova3d_linux PUBLIC NOVA3D_HAS_ALSA)
endif()

# ============================================================================
# Compiler Options
# ============================================================================

target_compile_options(nova3d_linux PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

# Release optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(nova3d_linux PRIVATE
        -O3
        -flto
        -fno-rtti
    )
endif()

# Debug symbols
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(nova3d_linux PRIVATE
        -g3
        -O0
        -fno-omit-frame-pointer
    )
endif()

# ============================================================================
# Export Headers
# ============================================================================

set_target_properties(nova3d_linux PROPERTIES
    PUBLIC_HEADER "${NOVA3D_LINUX_HEADERS}"
)

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS nova3d_linux
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nova3d/platform/linux
)

# ============================================================================
# pkg-config file
# ============================================================================

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/nova3d-linux.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/nova3d-linux.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/nova3d-linux.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Nova3D Linux Platform Configuration:")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  X11 Support:      ${NOVA3D_LINUX_X11}")
message(STATUS "  Wayland Support:  ${NOVA3D_LINUX_WAYLAND}")
message(STATUS "  Vulkan Support:   ${NOVA3D_LINUX_VULKAN}")
message(STATUS "  PulseAudio:       ${NOVA3D_LINUX_PULSEAUDIO}")
message(STATUS "  ALSA:             ${NOVA3D_LINUX_ALSA}")
message(STATUS "")
