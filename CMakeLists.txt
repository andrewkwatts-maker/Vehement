cmake_minimum_required(VERSION 3.20)
project(Nova3DEngine VERSION 1.0.0 LANGUAGES CXX C)

# Require C++23 for reflection and modern features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(NOVA_BUILD_EXAMPLES "Build example applications" ON)
option(NOVA_BUILD_TESTS "Build unit tests" OFF)
option(NOVA_USE_ASAN "Enable Address Sanitizer" OFF)
option(NOVA_ENABLE_SCRIPTING "Enable Python scripting support" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =============================================================================
# Platform Detection and Configuration
# =============================================================================
set(NOVA_PLATFORM_SOURCES "")
set(NOVA_PLATFORM_LIBS "")

if(WIN32)
    set(NOVA_PLATFORM "Windows")
    add_definitions(-DNOVA_PLATFORM_WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    set(NOVA_PLATFORM_SOURCES
        engine/platform/windows/WindowsPlatform.cpp
    )
elseif(ANDROID)
    set(NOVA_PLATFORM "Android")
    add_definitions(-DNOVA_PLATFORM_ANDROID)
    set(NOVA_PLATFORM_SOURCES
        engine/platform/android/AndroidPlatform.cpp
        engine/platform/android/AndroidGLES.cpp
        engine/platform/android/AndroidJNI.cpp
        engine/platform/android/AndroidTouchInput.cpp
        engine/platform/android/VulkanRenderer.cpp
        engine/platform/mobile/MobileInput.cpp
    )
    set(NOVA_PLATFORM_LIBS
        android
        log
        EGL
        GLESv3
    )
elseif(APPLE)
    if(IOS)
        set(NOVA_PLATFORM "iOS")
        add_definitions(-DNOVA_PLATFORM_IOS)
        set(NOVA_PLATFORM_SOURCES
            engine/platform/ios/iOSPlatform.mm
            engine/platform/mobile/MobileInput.cpp
        )
        set(NOVA_PLATFORM_LIBS
            "-framework UIKit"
            "-framework Foundation"
            "-framework Metal"
            "-framework MetalKit"
            "-framework QuartzCore"
            "-framework CoreLocation"
            "-framework CoreMotion"
        )
    else()
        set(NOVA_PLATFORM "macOS")
        add_definitions(-DNOVA_PLATFORM_MACOS)
        set(NOVA_PLATFORM_SOURCES
            engine/platform/macos/MacOSPlatform.mm
            engine/platform/desktop/DesktopInput.cpp
        )
        set(NOVA_PLATFORM_LIBS
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework CoreLocation"
            "-framework IOKit"
        )
    endif()
elseif(EMSCRIPTEN)
    set(NOVA_PLATFORM "Web")
    add_definitions(-DNOVA_PLATFORM_WEB)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(NOVA_PLATFORM_SOURCES
        engine/platform/web/WebPlatform.cpp
    )
elseif(UNIX)
    set(NOVA_PLATFORM "Linux")
    add_definitions(-DNOVA_PLATFORM_LINUX)
    set(NOVA_PLATFORM_SOURCES
        engine/platform/linux/LinuxPlatform.cpp
        engine/platform/desktop/DesktopInput.cpp
    )
    # Find X11 for native window access
    find_package(X11)
    if(X11_FOUND)
        add_definitions(-DGLFW_EXPOSE_NATIVE_X11)
        list(APPEND NOVA_PLATFORM_LIBS ${X11_LIBRARIES})
    endif()
endif()

message(STATUS "Target Platform: ${NOVA_PLATFORM}")

# Include FetchContent for dependency management
include(FetchContent)

# Set FetchContent to use quiet mode
set(FETCHCONTENT_QUIET OFF)

# =============================================================================
# External Dependencies via FetchContent
# =============================================================================

# GLFW - Window and Input
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# GLM - Mathematics
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# GLAD - OpenGL Loader (modern replacement for LoadGen)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.6
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake
)

# Assimp - Model Loading (replaces AIE FBXFile)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v5.4.3
    GIT_SHALLOW    TRUE
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

# stb - Single header libraries (image loading, etc.)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# nlohmann/json - JSON configuration
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# spdlog - Logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# ImGui - Debug UI (replaces AntTweakBar)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)

# FastNoise2 - Modern noise generation for terrain
FetchContent_Declare(
    fastnoise2
    GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
    GIT_TAG        v0.10.0-alpha
    GIT_SHALLOW    TRUE
)
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_TESTS OFF CACHE BOOL "" FORCE)

# pybind11 - Python bindings for C++ (for scripting support)
if(NOVA_ENABLE_SCRIPTING)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.12.0
        GIT_SHALLOW    TRUE
    )
endif()

# Make dependencies available
message(STATUS "Fetching dependencies...")
FetchContent_MakeAvailable(glfw glm json spdlog)

# Handle stb separately (header-only, no CMakeLists)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

# Handle ImGui separately (needs custom setup)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# Handle GLAD separately with generator
FetchContent_GetProperties(glad)
if(NOT glad_POPULATED)
    FetchContent_Populate(glad)
endif()

# Handle Assimp
FetchContent_MakeAvailable(assimp)

# Handle FastNoise2
FetchContent_GetProperties(fastnoise2)
if(NOT fastnoise2_POPULATED)
    FetchContent_Populate(fastnoise2)
    add_subdirectory(${fastnoise2_SOURCE_DIR} ${fastnoise2_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

# Handle pybind11 and Python (for scripting)
if(NOVA_ENABLE_SCRIPTING)
    # Find Python first
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    message(STATUS "Found Python ${Python3_VERSION}: ${Python3_EXECUTABLE}")
    message(STATUS "Python include dirs: ${Python3_INCLUDE_DIRS}")
    message(STATUS "Python libraries: ${Python3_LIBRARIES}")

    # Make pybind11 available
    FetchContent_MakeAvailable(pybind11)
    add_definitions(-DNOVA_SCRIPTING_ENABLED)
endif()

# =============================================================================
# GLAD OpenGL Loader Setup
# =============================================================================
add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR})
glad_add_library(glad_gl_core_46 REPRODUCIBLE API gl:core=4.6)

# =============================================================================
# STB Library Setup (header-only wrapper)
# =============================================================================
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# =============================================================================
# ImGui Library Setup
# =============================================================================
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

# =============================================================================
# Nova3D Engine Library
# =============================================================================
add_library(nova3d STATIC
    # Core
    engine/core/Engine.cpp
    engine/core/Window.cpp
    engine/core/Time.cpp
    engine/core/Logger.cpp
    engine/core/JobSystem.cpp

    # Configuration
    engine/config/Config.cpp

    # Reflection
    engine/reflection/TypeRegistry.cpp
    engine/reflection/Property.cpp

    # Graphics
    engine/graphics/Renderer.cpp
    engine/graphics/ShaderManager.cpp
    engine/graphics/Shader.cpp
    engine/graphics/Texture.cpp
    engine/graphics/TextureManager.cpp
    engine/graphics/Mesh.cpp
    engine/graphics/Material.cpp
    engine/graphics/Framebuffer.cpp
    engine/graphics/ModelLoader.cpp

    # Debug Visualization (replaces AIE Gizmos)
    engine/graphics/debug/DebugDraw.cpp
    engine/graphics/debug/DebugShapes.cpp

    # Input
    engine/input/InputManager.cpp
    engine/input/Keyboard.cpp
    engine/input/Mouse.cpp

    # Math utilities
    engine/math/Transform.cpp
    engine/math/Geometry.cpp
    engine/math/Random.cpp

    # Camera
    engine/scene/Camera.cpp
    engine/scene/FlyCamera.cpp
    engine/scene/SceneNode.cpp
    engine/scene/Scene.cpp

    # Animation
    engine/animation/Animation.cpp
    engine/animation/Keyframe.cpp
    engine/animation/AnimationController.cpp
    engine/animation/Skeleton.cpp

    # Particles
    engine/particles/ParticleSystem.cpp
    engine/particles/ParticleEmitter.cpp
    engine/particles/GPUParticleSystem.cpp

    # Terrain
    engine/terrain/TerrainGenerator.cpp
    engine/terrain/TerrainChunk.cpp
    engine/terrain/NoiseGenerator.cpp

    # Pathfinding
    engine/pathfinding/Graph.cpp
    engine/pathfinding/Node.cpp
    engine/pathfinding/Pathfinder.cpp
    engine/pathfinding/AStar.cpp

    # Utils
    engine/utils/FileSystem.cpp
    engine/utils/StringUtils.cpp

    # Platform-specific sources (populated by platform detection above)
    ${NOVA_PLATFORM_SOURCES}
)

# =============================================================================
# Python Scripting Support (conditionally compiled)
# =============================================================================
if(NOVA_ENABLE_SCRIPTING)
    target_sources(nova3d PRIVATE
        engine/scripting/PythonEngine.cpp
        engine/scripting/ScriptContext.cpp
        engine/scripting/EventDispatcher.cpp
        engine/scripting/AIBehavior.cpp
        engine/scripting/ScriptBindings.cpp
        engine/scripting/ScriptableComponent.cpp
    )

    target_include_directories(nova3d PUBLIC
        ${Python3_INCLUDE_DIRS}
    )

    target_link_libraries(nova3d PUBLIC
        pybind11::embed
        ${Python3_LIBRARIES}
    )

    message(STATUS "Python scripting support: ENABLED")
else()
    message(STATUS "Python scripting support: DISABLED")
endif()

target_include_directories(nova3d PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/platform
)

# Base libraries (not for Android which has its own graphics stack)
if(NOT ANDROID)
    target_link_libraries(nova3d PUBLIC
        glad_gl_core_46
        glfw
        glm::glm
        assimp
        stb
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        imgui
        FastNoise
    )
else()
    # Android uses its own graphics setup
    target_link_libraries(nova3d PUBLIC
        glm::glm
        nlohmann_json::nlohmann_json
        spdlog::spdlog
    )
endif()

# Platform-specific libraries
if(NOVA_PLATFORM_LIBS)
    target_link_libraries(nova3d PUBLIC ${NOVA_PLATFORM_LIBS})
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(nova3d PRIVATE /W4)
else()
    target_compile_options(nova3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Address Sanitizer
if(NOVA_USE_ASAN)
    target_compile_options(nova3d PUBLIC -fsanitize=address)
    target_link_options(nova3d PUBLIC -fsanitize=address)
endif()

# =============================================================================
# Examples
# =============================================================================
if(NOVA_BUILD_EXAMPLES)
    add_executable(nova_demo
        examples/main.cpp
        examples/DemoApplication.cpp
    )
    target_link_libraries(nova_demo PRIVATE nova3d)

    # Copy assets to build directory
    add_custom_command(TARGET nova_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:nova_demo>/assets
    )
endif()

# =============================================================================
# Installation
# =============================================================================
install(TARGETS nova3d
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY engine/ DESTINATION include/nova3d
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

# Print configuration summary
message(STATUS "")
message(STATUS "Nova3D Engine Configuration:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Examples: ${NOVA_BUILD_EXAMPLES}")
message(STATUS "  Build Tests:    ${NOVA_BUILD_TESTS}")
message(STATUS "  Scripting:      ${NOVA_ENABLE_SCRIPTING}")
if(NOVA_ENABLE_SCRIPTING)
    message(STATUS "  Python Version: ${Python3_VERSION}")
endif()
message(STATUS "")
