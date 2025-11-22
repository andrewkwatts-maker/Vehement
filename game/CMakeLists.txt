cmake_minimum_required(VERSION 3.20)
project(Vehement2 VERSION 1.0.0 LANGUAGES CXX)

# Require C++23 for modern features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(VEHEMENT_ENABLE_EDITOR "Build with level editor support" ON)
option(VEHEMENT_ENABLE_DEBUG "Enable debug features" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include FetchContent for dependency management
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# =============================================================================
# Nova3D Engine (from parent directory)
# =============================================================================
# Check if we're being built as part of the main project or standalone
if(NOT TARGET nova3d)
    # Standalone build - add the engine directory
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_BINARY_DIR}/engine)
endif()

# =============================================================================
# Firebase C++ SDK
# =============================================================================
FetchContent_Declare(
    firebase_cpp_sdk
    GIT_REPOSITORY https://github.com/firebase/firebase-cpp-sdk.git
    GIT_TAG        v12.1.0
    GIT_SHALLOW    TRUE
)

# Firebase configuration - only build what we need
set(FIREBASE_CPP_SDK_DIR "" CACHE PATH "Path to Firebase C++ SDK")

# Note: Firebase SDK requires additional setup for different platforms
# For now, we provide a mock interface that can be replaced with actual Firebase
# when the SDK is properly configured for the target platform.

# =============================================================================
# nlohmann/json (may already be available from engine)
# =============================================================================
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# =============================================================================
# Game Source Files
# =============================================================================
set(VEHEMENT_CORE_SOURCES
    src/core/Game.cpp
    src/core/GameConfig.cpp
)

set(VEHEMENT_CORE_HEADERS
    src/core/Game.hpp
    src/core/GameConfig.hpp
)

set(VEHEMENT_ENTITY_SOURCES
    # Placeholder for entity sources
)

set(VEHEMENT_WORLD_SOURCES
    # Placeholder for world sources
)

set(VEHEMENT_COMBAT_SOURCES
    # Placeholder for combat sources
)

set(VEHEMENT_NETWORK_SOURCES
    # Placeholder for network sources
)

set(VEHEMENT_UI_SOURCES
    # Placeholder for UI sources
)

set(VEHEMENT_EDITOR_SOURCES
    # Placeholder for editor sources
)

# =============================================================================
# Vehement2 Executable
# =============================================================================
add_executable(vehement2
    src/main.cpp
    ${VEHEMENT_CORE_SOURCES}
    ${VEHEMENT_CORE_HEADERS}
    ${VEHEMENT_ENTITY_SOURCES}
    ${VEHEMENT_WORLD_SOURCES}
    ${VEHEMENT_COMBAT_SOURCES}
    ${VEHEMENT_NETWORK_SOURCES}
    ${VEHEMENT_UI_SOURCES}
    ${VEHEMENT_EDITOR_SOURCES}
)

target_include_directories(vehement2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine
)

target_link_libraries(vehement2 PRIVATE
    nova3d
    nlohmann_json::nlohmann_json
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(vehement2 PRIVATE
        VEHEMENT_PLATFORM_WINDOWS
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(vehement2 PRIVATE
        VEHEMENT_PLATFORM_LINUX
    )
    target_link_libraries(vehement2 PRIVATE pthread)
elseif(APPLE)
    target_compile_definitions(vehement2 PRIVATE
        VEHEMENT_PLATFORM_MACOS
    )
endif()

# Debug/Editor features
if(VEHEMENT_ENABLE_DEBUG)
    target_compile_definitions(vehement2 PRIVATE VEHEMENT_DEBUG)
endif()

if(VEHEMENT_ENABLE_EDITOR)
    target_compile_definitions(vehement2 PRIVATE VEHEMENT_EDITOR)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(vehement2 PRIVATE /W4)
else()
    target_compile_options(vehement2 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# =============================================================================
# Asset handling
# =============================================================================
# Create symlink to Vehement2 assets or copy them
set(VEHEMENT_ASSETS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../Vehement2")
set(VEHEMENT_ASSETS_DEST "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")

add_custom_command(TARGET vehement2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VEHEMENT_ASSETS_SOURCE}" "${VEHEMENT_ASSETS_DEST}/vehement2"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets" "${VEHEMENT_ASSETS_DEST}/engine"
    COMMENT "Copying game assets..."
)

# =============================================================================
# Configuration Summary
# =============================================================================
message(STATUS "")
message(STATUS "Vehement2 Game Configuration:")
message(STATUS "  Version:       ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  Editor:        ${VEHEMENT_ENABLE_EDITOR}")
message(STATUS "  Debug:         ${VEHEMENT_ENABLE_DEBUG}")
message(STATUS "")
