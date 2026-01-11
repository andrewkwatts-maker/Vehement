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
option(NOVA_ENABLE_SCRIPTING "Enable Python scripting support" OFF) # Temporarily disabled

# Feature flags for optional subsystems (OFF by default if external dependencies required)
option(NOVA_ENABLE_PHYSICS "Enable physics system" ON)
option(NOVA_ENABLE_AUDIO "Enable audio system (requires OpenAL, libsndfile, libvorbis)" OFF)
option(NOVA_ENABLE_PERSISTENCE "Enable persistence/database systems (requires SQLite3)" OFF)
option(NOVA_ENABLE_NETWORKING "Enable networking and Firebase integration" OFF)
option(NOVA_ENABLE_UI "Enable advanced UI system" ON)
option(NOVA_ENABLE_EVENTS "Enable event binding system" OFF) # Disabled: has JSON API issues
option(NOVA_ENABLE_MODDING "Enable modding system" ON)
option(NOVA_ENABLE_LOCATION "Enable location services" OFF)
option(NOVA_ENABLE_PROFILING "Enable performance profiling (requires SQLite3)" OFF)
option(NOVA_ENABLE_ADVANCED_GRAPHICS "Enable advanced rendering features" OFF) # Disabled: orphaned files with include errors
option(NOVA_ENABLE_RTS_GAME "Enable RTS game systems" OFF) # Disabled: has constructor/access errors

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
    add_definitions(-DGLM_ENABLE_EXPERIMENTAL)
    set(NOVA_PLATFORM_SOURCES
        engine/platform/windows/WindowsPlatform.cpp
        engine/platform/windows/WindowsInput.cpp
        engine/platform/windows/WindowsGL.cpp
        engine/platform/windows/WindowsAudio.cpp
        engine/platform/desktop/DesktopInput.cpp
    )
    set(NOVA_PLATFORM_LIBS
        winmm           # Multimedia API (audio, joystick)
        xinput          # Xbox controller support
        ws2_32          # Winsock (networking)
        ole32           # COM (audio, shell)
        oleaut32        # COM automation
        uuid            # GUID support
        user32          # Window management
        gdi32           # Graphics Device Interface
        dwmapi          # Desktop Window Manager
        imm32           # Input Method Manager
        shell32         # Shell functions
        shlwapi         # Shell lightweight utilities
        version         # Version info
        opengl32        # OpenGL
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
        enable_language(OBJCXX)
        set(CMAKE_OBJCXX_STANDARD 23)
        set(NOVA_PLATFORM_SOURCES
            engine/platform/macos/MacOSPlatform.mm
            engine/platform/macos/MacOSMetal.mm
            engine/platform/desktop/DesktopInput.cpp
        )
        set(NOVA_PLATFORM_LIBS
            "-framework Cocoa"
            "-framework Metal"
            "-framework MetalKit"
            "-framework OpenGL"
            "-framework CoreLocation"
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework AudioUnit"
            "-framework IOKit"
            "-framework GameController"
            "-framework SystemConfiguration"
            "-framework Security"
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

    # Cross-compilation options for Linux
    option(NOVA_LINUX_VULKAN "Enable Vulkan support on Linux" ON)
    option(NOVA_LINUX_WAYLAND "Enable Wayland support on Linux" ON)
    option(NOVA_LINUX_X11 "Enable X11 support on Linux" ON)
    option(NOVA_LINUX_PULSEAUDIO "Enable PulseAudio support" ON)
    option(NOVA_LINUX_ALSA "Enable ALSA support" ON)

    set(NOVA_PLATFORM_SOURCES
        engine/platform/linux/LinuxPlatform.cpp
        engine/platform/linux/LinuxVulkan.cpp
        engine/platform/linux/LinuxAudio.cpp
        engine/platform/desktop/DesktopInput.cpp
    )

    # Find X11 for native window access
    if(NOVA_LINUX_X11)
        find_package(X11)
        if(X11_FOUND)
            add_definitions(-DGLFW_EXPOSE_NATIVE_X11 -DNOVA_HAS_X11)
            list(APPEND NOVA_PLATFORM_LIBS ${X11_LIBRARIES})
            message(STATUS "X11 support: ENABLED")

            # X11 extensions
            if(X11_Xrandr_FOUND)
                list(APPEND NOVA_PLATFORM_LIBS ${X11_Xrandr_LIB})
            endif()
            if(X11_Xcursor_FOUND)
                list(APPEND NOVA_PLATFORM_LIBS ${X11_Xcursor_LIB})
            endif()
            if(X11_Xi_FOUND)
                list(APPEND NOVA_PLATFORM_LIBS ${X11_Xi_LIB})
            endif()
        else()
            message(STATUS "X11 support: DISABLED (not found)")
        endif()
    endif()

    # Find Wayland
    if(NOVA_LINUX_WAYLAND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(WAYLAND wayland-client wayland-egl)
            if(WAYLAND_FOUND)
                add_definitions(-DGLFW_EXPOSE_NATIVE_WAYLAND -DNOVA_HAS_WAYLAND)
                list(APPEND NOVA_PLATFORM_LIBS ${WAYLAND_LIBRARIES})
                message(STATUS "Wayland support: ENABLED")
            else()
                message(STATUS "Wayland support: DISABLED (not found)")
            endif()
        endif()
    endif()

    # Find Vulkan
    if(NOVA_LINUX_VULKAN)
        find_package(Vulkan)
        if(Vulkan_FOUND)
            add_definitions(-DNOVA_HAS_VULKAN -DVK_USE_PLATFORM_XLIB_KHR)
            if(WAYLAND_FOUND)
                add_definitions(-DVK_USE_PLATFORM_WAYLAND_KHR)
            endif()
            list(APPEND NOVA_PLATFORM_LIBS Vulkan::Vulkan)
            message(STATUS "Vulkan support: ENABLED")
        else()
            message(STATUS "Vulkan support: DISABLED (not found)")
        endif()
    endif()

    # Audio backend definitions
    if(NOVA_LINUX_PULSEAUDIO)
        add_definitions(-DNOVA_HAS_PULSEAUDIO)
    endif()
    if(NOVA_LINUX_ALSA)
        add_definitions(-DNOVA_HAS_ALSA)
    endif()

    # Threading and dynamic loading
    find_package(Threads REQUIRED)
    list(APPEND NOVA_PLATFORM_LIBS Threads::Threads dl)
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
    engine/core/Profiler.cpp

    # Configuration
    engine/config/Config.cpp

    # Reflection
    engine/reflection/TypeRegistry.cpp
    engine/reflection/Property.cpp

    # Graphics
    engine/graphics/Renderer.cpp
    engine/graphics/OptimizedRenderer.cpp
    engine/graphics/Batching.cpp
    engine/graphics/Culler.cpp
    engine/graphics/LODManager.cpp
    engine/graphics/TextureAtlas.cpp
    engine/graphics/RenderQueue.cpp
    engine/graphics/ShaderManager.cpp
    engine/graphics/Shader.cpp
    engine/graphics/Texture.cpp
    engine/graphics/TextureManager.cpp
    engine/graphics/Mesh.cpp
    engine/graphics/Material.cpp
    engine/graphics/Framebuffer.cpp
    engine/graphics/ModelLoader.cpp
    engine/graphics/SDFRenderer.cpp
    engine/graphics/RadianceCascade.cpp
    engine/graphics/SpectralRenderer.cpp

    # SDF System
    engine/sdf/SDFModel.cpp
    engine/sdf/SDFPrimitive.cpp
    engine/sdf/SDFSerializer.cpp
    engine/sdf/SDFAnimation.cpp
    engine/sdf/SDFCache.cpp

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
    # Procedural Generation System
    engine/procedural/ProcGenNodes.cpp
    engine/procedural/ProcGenGraph.cpp
    engine/procedural/WorldTemplate.cpp

    # Asset Serialization System
    engine/assets/JsonAssetSerializer.cpp

    # ==========================================================================
    # ORPHANED FILES - Now included in build!
    # ==========================================================================

    # Core System Extensions
    engine/core/PropertySystem.cpp
    engine/core/SettingsManager.cpp

    # Extended Reflection System
    engine/reflection/TypeRegistryImpl.cpp
    engine/reflection/TypeInfo.cpp
    engine/reflection/EventBus.cpp
    engine/reflection/Observable.cpp

    # Spatial Data Structures
    engine/spatial/AABB.cpp
    engine/spatial/CollisionPrimitives.cpp
    engine/spatial/BVH.cpp
    engine/spatial/SDFBVH.cpp
    engine/spatial/SpatialManager.cpp
    engine/spatial/SpatialIndex.cpp
    engine/spatial/SpatialHash3D.cpp
    engine/spatial/OBB.cpp
    engine/spatial/Octree.cpp
    engine/spatial/Frustum.cpp

    # Scene Extensions
    engine/scene/InstanceData.cpp
    engine/scene/InstanceManager.cpp

    # Advanced Animation System
    engine/animation/AnimationStateMachine.cpp
    engine/animation/AnimationEventSystem.cpp
    engine/animation/AnimationTrigger.cpp
    engine/animation/AnimationConfig.cpp
    engine/animation/AnimationBlendTree.cpp
    engine/animation/blending/BlendTreeRuntime.cpp
    engine/animation/blending/BlendSpace2D.cpp
    engine/animation/blending/BlendSpace1D.cpp
    engine/animation/blending/BlendNode.cpp
    engine/animation/blending/BlendTreeBuilder.cpp
    engine/animation/blending/BlendMask.cpp
    engine/animation/blending/AnimationLayer.cpp
    engine/animation/editor/AnimationClipboard.cpp
    engine/animation/editor/EditableAnimation.cpp
    engine/animation/editor/EditableSkeleton.cpp

    # Extended Terrain System
    engine/terrain/LandscapeSDFIntegration.cpp
    engine/terrain/SDFTerrain.cpp
    engine/terrain/TerrainSDFDemo.cpp
    engine/terrain/VoxelTerrain.cpp
    engine/terrain/HeightmapIO.cpp

    # Import Pipeline
    engine/import/ModelImporter.cpp
    engine/import/TextureImporter.cpp
    engine/import/ImportSettings.cpp
    engine/import/AssetProcessor.cpp
    engine/import/AnimationImporter.cpp
    engine/import/ImportProgress.cpp

    # Materials System
    engine/materials/AdvancedMaterial.cpp
    engine/materials/MaterialGraphEditor.cpp
    engine/materials/MaterialNode.cpp
    engine/materials/NoiseNodes.cpp
    engine/materials/ShaderGraph.cpp
    engine/materials/ShaderGraphEditor.cpp
    engine/materials/ShaderNodes.cpp

    # Lighting System
    engine/lighting/EmissiveGeometryLight.cpp
    engine/lighting/LightMaterialFunction.cpp
    engine/lighting/PhysicalLight.cpp

    # Post-Processing
    engine/postprocess/PostProcess.cpp

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

target_compile_definitions(nova3d PUBLIC JSON_HAS_CPP_20=0 JSON_HAS_RANGES=0 GLM_ENABLE_EXPERIMENTAL)

if(MSVC)
    target_compile_options(nova3d PRIVATE /W4 /FS)
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
# Tests
# =============================================================================
if(NOVA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
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

# Install shaders
install(DIRECTORY engine/shaders/ DESTINATION share/nova3d/shaders
    FILES_MATCHING
    PATTERN "*.glsl"
    PATTERN "*.vert"
    PATTERN "*.frag"
    PATTERN "*.comp"
    PATTERN "*.metal"
    PATTERN "*.hlsl"
    PATTERN "*.hlsli"
)

# =============================================================================
# Shader Compilation (optional)
# =============================================================================
option(NOVA_PRECOMPILE_SHADERS "Precompile shaders for Metal/D3D12" OFF)

if(NOVA_PRECOMPILE_SHADERS)
    # Metal shader compilation (macOS/iOS)
    if(APPLE)
        find_program(METAL_COMPILER xcrun)
        if(METAL_COMPILER)
            file(GLOB METAL_SHADERS "${CMAKE_SOURCE_DIR}/engine/shaders/metal/*.metal")
            foreach(SHADER ${METAL_SHADERS})
                get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
                set(AIR_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.air")
                set(METALLIB_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.metallib")
                add_custom_command(
                    OUTPUT ${METALLIB_OUTPUT}
                    COMMAND ${METAL_COMPILER} -sdk macosx metal -c ${SHADER} -o ${AIR_OUTPUT}
                    COMMAND ${METAL_COMPILER} -sdk macosx metallib ${AIR_OUTPUT} -o ${METALLIB_OUTPUT}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling Metal shader: ${SHADER_NAME}"
                )
                list(APPEND COMPILED_METAL_SHADERS ${METALLIB_OUTPUT})
            endforeach()
            add_custom_target(metal_shaders ALL DEPENDS ${COMPILED_METAL_SHADERS})
        endif()
    endif()

    # HLSL shader compilation (Windows with D3D12)
    if(WIN32)
        find_program(DXC_COMPILER dxc)
        if(DXC_COMPILER)
            file(GLOB HLSL_VS_SHADERS "${CMAKE_SOURCE_DIR}/engine/shaders/hlsl/*_vs.hlsl")
            file(GLOB HLSL_PS_SHADERS "${CMAKE_SOURCE_DIR}/engine/shaders/hlsl/*_ps.hlsl")

            foreach(SHADER ${HLSL_VS_SHADERS})
                get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
                set(CSO_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.cso")
                add_custom_command(
                    OUTPUT ${CSO_OUTPUT}
                    COMMAND ${DXC_COMPILER} -T vs_6_0 -Fo ${CSO_OUTPUT} ${SHADER}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling HLSL vertex shader: ${SHADER_NAME}"
                )
                list(APPEND COMPILED_HLSL_SHADERS ${CSO_OUTPUT})
            endforeach()

            foreach(SHADER ${HLSL_PS_SHADERS})
                get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
                set(CSO_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.cso")
                add_custom_command(
                    OUTPUT ${CSO_OUTPUT}
                    COMMAND ${DXC_COMPILER} -T ps_6_0 -Fo ${CSO_OUTPUT} ${SHADER}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling HLSL pixel shader: ${SHADER_NAME}"
                )
                list(APPEND COMPILED_HLSL_SHADERS ${CSO_OUTPUT})
            endforeach()

            add_custom_target(hlsl_shaders ALL DEPENDS ${COMPILED_HLSL_SHADERS})
        endif()
    endif()
endif()

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

# RTS Demo with fantasy menu scene
add_executable(nova_rts_demo
    examples/RTSApplication.cpp
    examples/MenuSceneMeshes.cpp
    examples/SettingsMenu.cpp
    examples/game_main.cpp
)
target_link_libraries(nova_rts_demo PRIVATE nova3d)

# Copy assets
add_custom_command(TARGET nova_rts_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:nova_rts_demo>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/game/assets $<TARGET_FILE_DIR:nova_rts_demo>/game_assets
)


# Asset Icon Renderer Tool
add_executable(asset_icon_renderer
    tools/asset_icon_renderer.cpp
)
target_link_libraries(asset_icon_renderer PRIVATE nova3d)
target_include_directories(asset_icon_renderer PRIVATE ${CMAKE_SOURCE_DIR})

# Asset Icon Renderer with GI Pipeline
add_executable(asset_icon_renderer_gi
    tools/asset_icon_renderer_gi.cpp
)
target_link_libraries(asset_icon_renderer_gi PRIVATE nova3d)
target_include_directories(asset_icon_renderer_gi PRIVATE ${CMAKE_SOURCE_DIR})
# ========================================================================
# Physics System
# ========================================================================
if(NOVA_ENABLE_PHYSICS)
    target_sources(nova3d PRIVATE
        engine/physics/PhysicsDebugDraw.cpp
        engine/physics/CollisionShape.cpp
        engine/physics/CollisionBody.cpp
        engine/physics/CollisionConfig.cpp
        engine/physics/CollisionEvents.cpp
        engine/physics/PhysicsWorld.cpp
        engine/physics/Triggers.cpp
        engine/physics/BlackbodyRadiation.cpp
        engine/physics/SDFCollision.cpp
    )
    message(STATUS "Physics System: ENABLED")
else()
    message(STATUS "Physics System: DISABLED")
endif()

# ========================================================================
# Audio System
# ========================================================================
if(NOVA_ENABLE_AUDIO)
    target_sources(nova3d PRIVATE
        engine/audio/AudioEngine.cpp
    )
    message(STATUS "Audio System: ENABLED")
else()
    message(STATUS "Audio System: DISABLED")
endif()

# ========================================================================
# Persistence System
# ========================================================================
if(NOVA_ENABLE_PERSISTENCE)
    target_sources(nova3d PRIVATE
        engine/persistence/ChunkStreamer.cpp
        engine/persistence/EntitySerializer.cpp
        engine/persistence/FirebaseBackend.cpp
        engine/persistence/PersistenceManager.cpp
        engine/persistence/PlayerDatabase.cpp
        engine/persistence/SQLiteBackend.cpp
        engine/persistence/WorldDatabase.cpp
    )
    message(STATUS "Persistence System: ENABLED")
else()
    message(STATUS "Persistence System: DISABLED")
endif()

# ========================================================================
# Networking System
# ========================================================================
if(NOVA_ENABLE_NETWORKING)
    target_sources(nova3d PRIVATE
        engine/networking/FirebaseClient.cpp
        engine/networking/FirebasePersistence.cpp
        engine/networking/ReplicationSystem.cpp
    )
    message(STATUS "Networking System: ENABLED")
else()
    message(STATUS "Networking System: DISABLED")
endif()

# ========================================================================
# Advanced UI System
# ========================================================================
if(NOVA_ENABLE_UI)
    target_sources(nova3d PRIVATE
        engine/ui/editor/UIEditor.cpp
        engine/ui/EditorPanel.cpp
        engine/ui/EditorTheme.cpp
        engine/ui/EditorWidgets.cpp
        engine/ui/runtime/HTMLRenderer.cpp
        engine/ui/runtime/RuntimeUIManager.cpp
        engine/ui/runtime/UIAnimation.cpp
        engine/ui/runtime/UIBinding.cpp
        engine/ui/runtime/UIDataBinding.cpp
        engine/ui/runtime/UIEventRouter.cpp
        engine/ui/runtime/UIWindow.cpp
        engine/ui/widgets/UIParser.cpp
        engine/ui/widgets/UIWidget.cpp
    )
    message(STATUS "Advanced UI System: ENABLED")
else()
    message(STATUS "Advanced UI System: DISABLED")
endif()

# ========================================================================
# Event System
# ========================================================================
if(NOVA_ENABLE_EVENTS)
    target_sources(nova3d PRIVATE
        engine/events/EventCondition.cpp
        engine/events/EventBindingManager.cpp
        engine/events/PropertyWatcher.cpp
        engine/events/EventBinding.cpp
    )
    message(STATUS "Event System: ENABLED")
else()
    message(STATUS "Event System: DISABLED")
endif()

# ========================================================================
# Modding System
# ========================================================================
if(NOVA_ENABLE_MODDING)
    target_sources(nova3d PRIVATE
        engine/modding/BehaviorSystem.cpp
        engine/modding/EntityTypes.cpp
        engine/modding/JsonSchema.cpp
        engine/modding/ModManager.cpp
        engine/modding/UIComponents.cpp
        engine/modding/UITemplateEngine.cpp
    )
    message(STATUS "Modding System: ENABLED")
else()
    message(STATUS "Modding System: DISABLED")
endif()

# ========================================================================
# Location Services
# ========================================================================
if(NOVA_ENABLE_LOCATION)
    target_sources(nova3d PRIVATE
        engine/location/LocationSimulator.cpp
        engine/location/LocationManager.cpp
        engine/location/Geofence.cpp
        engine/location/Geocoding.cpp
    )
    message(STATUS "Location Services: ENABLED")
else()
    message(STATUS "Location Services: DISABLED")
endif()

# ========================================================================
# Performance Profiling
# ========================================================================
if(NOVA_ENABLE_PROFILING)
    target_sources(nova3d PRIVATE
        engine/profiling/DetailedFrameProfiler.cpp
        engine/profiling/PerformanceAnalyzer.cpp
        engine/profiling/PerformanceDatabase.cpp
    )
    message(STATUS "Performance Profiling: ENABLED")
else()
    message(STATUS "Performance Profiling: DISABLED")
endif()

# ========================================================================
# Advanced Graphics
# ========================================================================
if(NOVA_ENABLE_ADVANCED_GRAPHICS)
    target_sources(nova3d PRIVATE
        engine/graphics/AsyncComputePipeline.cpp
        engine/graphics/ClusteredLighting.cpp
        engine/graphics/ClusteredLightingExpanded.cpp
        engine/graphics/GPUDrivenRenderer.cpp
        engine/graphics/HybridDepthMerge.cpp
        engine/graphics/HybridPathTracer.cpp
        engine/graphics/HybridRasterizer.cpp
        engine/graphics/MassiveSceneProfiler.cpp
        engine/graphics/MeshToSDFConverter.cpp
        engine/graphics/ParallelCullingSystem.cpp
        engine/graphics/PathTracer.cpp
        engine/graphics/PathTracerIntegration.cpp
        engine/graphics/PolygonRasterizer.cpp
        engine/graphics/RadianceCascadeTest.cpp
        engine/graphics/ReSTIR.cpp
        engine/graphics/RTGIPipeline.cpp
        engine/graphics/RTXAccelerationStructure.cpp
        engine/graphics/RTXPathTracer.cpp
        engine/graphics/RTXSupport.cpp
        engine/graphics/SDFAcceleration.cpp
        engine/graphics/SDFBatchRenderer.cpp
        engine/graphics/SDFBrickCache.cpp
        engine/graphics/SDFBrickMap.cpp
        engine/graphics/SDFDeferredLighting.cpp
        engine/graphics/SDFGPUEvaluator.cpp
        engine/graphics/SDFInstanceManager.cpp
        engine/graphics/SDFLODSystem.cpp
        engine/graphics/SDFPerformanceProfiler.cpp
        engine/graphics/SDFRasterizer.cpp
        engine/graphics/SDFRendererAccelerated.cpp
        engine/graphics/SDFSkeletalAnimation.cpp
        engine/graphics/SDFSparseOctree.cpp
        engine/graphics/SVGF.cpp
    )
    message(STATUS "Advanced Graphics: ENABLED")
else()
    message(STATUS "Advanced Graphics: DISABLED")
endif()

# ========================================================================
# RTS Game Systems
# ========================================================================
if(NOVA_ENABLE_RTS_GAME)
    target_sources(nova3d PRIVATE
        engine/game/BuildingComponentSystem.cpp
        engine/game/BuildingInstanceExtensions.cpp
        engine/game/BuildingUI.cpp
        engine/game/ComponentPlacementController.cpp
        engine/game/DefenseStructures.cpp
        engine/game/SDFConfigLoader.cpp
        engine/game/WallPlacementSystem.cpp
    )
    message(STATUS "RTS Game Systems: ENABLED")
else()
    message(STATUS "RTS Game Systems: DISABLED")
endif()

if(NOVA_ENABLE_SCRIPTING)
    target_sources(nova3d PRIVATE
        engine/scripting/ScriptValidator.cpp
        engine/scripting/ScriptStorage.cpp
        engine/scripting/ScriptTemplate.cpp
        engine/scripting/GameAPI.cpp
        engine/scripting/EventNodes.cpp
        engine/scripting/visual/VisualScriptEditor.cpp
        engine/scripting/visual/VisualScriptingCore.cpp
    )
    message(STATUS "Extended scripting features: ENABLED")
endif()

# Asset Media Renderer (Icons + Animated Videos)
add_executable(asset_media_renderer
    tools/asset_media_renderer.cpp
)
target_link_libraries(asset_media_renderer PRIVATE nova3d)
target_include_directories(asset_media_renderer PRIVATE ${CMAKE_SOURCE_DIR})

# Asset Thumbnail Cache System (Editor)
if(NOVA_ENABLE_UI)
    target_sources(nova3d PRIVATE
        engine/editor/AssetThumbnailCache.cpp
    )
endif()

# Generic Asset Editor System (Editor)
if(NOVA_ENABLE_UI)
    target_sources(nova3d PRIVATE
        engine/editor/GenericAssetEditor.cpp
    )
endif()

# Ray Picker System (Editor)
target_sources(nova3d PRIVATE
    engine/editor/RayPicker.cpp
)

# Transform Gizmo System (Editor)
target_sources(nova3d PRIVATE
    engine/editor/TransformGizmo.cpp
)

# Editor Command System (Undo/Redo)
target_sources(nova3d PRIVATE
    engine/editor/EditorCommand.cpp
    engine/editor/CommandHistory.cpp
)

# Material Editor System (Multi-object editing)
target_sources(nova3d PRIVATE
    engine/editor/MaterialEditor.cpp
)
