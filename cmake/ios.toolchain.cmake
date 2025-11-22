# =============================================================================
# iOS CMake Toolchain File for Nova3D Engine
# =============================================================================
#
# Usage:
#   cmake -G Xcode \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.cmake \
#         -DPLATFORM=OS64 \
#         -DDEPLOYMENT_TARGET=14.0 \
#         ..
#
# Platforms:
#   OS64        - iOS (arm64)
#   SIMULATOR64 - iOS Simulator (x86_64)
#   SIMULATORARM64 - iOS Simulator (arm64, for Apple Silicon Macs)
#   OS64COMBINED - iOS + Simulator (fat library)
#
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# Platform selection
if(NOT DEFINED PLATFORM)
    set(PLATFORM "OS64" CACHE STRING "Target platform")
endif()

# Deployment target
if(NOT DEFINED DEPLOYMENT_TARGET)
    set(DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum iOS version")
endif()

# Enable bitcode (optional, deprecated in Xcode 14+)
if(NOT DEFINED ENABLE_BITCODE)
    set(ENABLE_BITCODE OFF CACHE BOOL "Enable Bitcode")
endif()

# ARC (Automatic Reference Counting)
if(NOT DEFINED ENABLE_ARC)
    set(ENABLE_ARC ON CACHE BOOL "Enable ARC")
endif()

# Set system name
set(CMAKE_SYSTEM_NAME iOS CACHE STRING "")
set(CMAKE_SYSTEM_VERSION ${DEPLOYMENT_TARGET} CACHE STRING "")

# Set platform flags
if(PLATFORM STREQUAL "OS64")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT "iphoneos" CACHE STRING "")
    set(IOS_PLATFORM "OS")
    message(STATUS "Configuring for iOS device (arm64)")
elseif(PLATFORM STREQUAL "SIMULATOR64")
    set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT "iphonesimulator" CACHE STRING "")
    set(IOS_PLATFORM "SIMULATOR")
    message(STATUS "Configuring for iOS Simulator (x86_64)")
elseif(PLATFORM STREQUAL "SIMULATORARM64")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT "iphonesimulator" CACHE STRING "")
    set(IOS_PLATFORM "SIMULATOR")
    message(STATUS "Configuring for iOS Simulator (arm64)")
elseif(PLATFORM STREQUAL "OS64COMBINED")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT "iphoneos" CACHE STRING "")
    set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO")
    set(IOS_PLATFORM "OS")
    message(STATUS "Configuring for iOS (combined)")
else()
    message(FATAL_ERROR "Unknown platform: ${PLATFORM}")
endif()

# Set deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET ${DEPLOYMENT_TARGET} CACHE STRING "")

# Platform defines
add_definitions(-DNOVA_PLATFORM_IOS)
add_definitions(-DGLES_SILENCE_DEPRECATION)

# Find SDK path
execute_process(
    COMMAND xcrun --sdk ${CMAKE_OSX_SYSROOT} --show-sdk-path
    OUTPUT_VARIABLE CMAKE_OSX_SYSROOT_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_OSX_SYSROOT "${CMAKE_OSX_SYSROOT_PATH}" CACHE PATH "iOS SDK path")

# Set compilers
execute_process(
    COMMAND xcrun --sdk ${CMAKE_OSX_SYSROOT} --find clang
    OUTPUT_VARIABLE CMAKE_C_COMPILER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND xcrun --sdk ${CMAKE_OSX_SYSROOT} --find clang++
    OUTPUT_VARIABLE CMAKE_CXX_COMPILER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Compiler flags
set(CMAKE_C_FLAGS_INIT "-arch ${CMAKE_OSX_ARCHITECTURES}")
set(CMAKE_CXX_FLAGS_INIT "-arch ${CMAKE_OSX_ARCHITECTURES}")

# Objective-C++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc" CACHE STRING "" FORCE)

# Enable ARC
if(ENABLE_ARC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fobjc-arc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fobjc-arc")
endif()

# Bitcode
if(ENABLE_BITCODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fembed-bitcode")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fembed-bitcode")
endif()

# iOS-specific settings
set(CMAKE_MACOSX_BUNDLE YES)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "YES")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-")

# Skip RPATH settings (not used on iOS)
set(CMAKE_SKIP_RPATH TRUE CACHE BOOL "")

# Set Xcode attributes
set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET ${DEPLOYMENT_TARGET} CACHE STRING "")
set(CMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" CACHE STRING "") # iPhone and iPad
set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES" CACHE STRING "")
set(CMAKE_XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN "YES" CACHE STRING "")

# Metal support
set(CMAKE_XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO "INCLUDE_SOURCE" CACHE STRING "")

# Try compile settings
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Find required iOS frameworks
macro(find_ios_framework framework_name)
    find_library(${framework_name}_FRAMEWORK ${framework_name}
        PATHS ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks
        NO_DEFAULT_PATH
    )
    if(${framework_name}_FRAMEWORK)
        message(STATUS "Found ${framework_name}: ${${framework_name}_FRAMEWORK}")
    else()
        message(WARNING "Could not find ${framework_name} framework")
    endif()
endmacro()

# Standard iOS frameworks used by Nova3D
set(IOS_FRAMEWORKS
    Foundation
    UIKit
    QuartzCore
    Metal
    MetalKit
    OpenGLES
    CoreGraphics
    CoreLocation
    CoreMotion
    CoreHaptics
    GameKit
    AVFoundation
    AudioToolbox
)

# =============================================================================
# Helper function to create iOS app bundle
# =============================================================================

function(nova_create_ios_app target_name)
    cmake_parse_arguments(
        APP
        ""
        "BUNDLE_ID;VERSION;BUILD;DISPLAY_NAME;TEAM_ID"
        "SOURCES;RESOURCES;FRAMEWORKS;STORYBOARDS"
        ${ARGN}
    )

    # Set defaults
    if(NOT APP_BUNDLE_ID)
        set(APP_BUNDLE_ID "com.nova3d.${target_name}")
    endif()
    if(NOT APP_VERSION)
        set(APP_VERSION "1.0.0")
    endif()
    if(NOT APP_BUILD)
        set(APP_BUILD "1")
    endif()
    if(NOT APP_DISPLAY_NAME)
        set(APP_DISPLAY_NAME "${target_name}")
    endif()

    # Create executable
    add_executable(${target_name} MACOSX_BUNDLE ${APP_SOURCES})

    # Set bundle properties
    set_target_properties(${target_name} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/${target_name}/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${APP_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_BUILD}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
        MACOSX_BUNDLE_BUNDLE_NAME "${APP_DISPLAY_NAME}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${APP_BUNDLE_ID}"
        XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName "${APP_DISPLAY_NAME}"
    )

    # Set team ID if provided
    if(APP_TEAM_ID)
        set_target_properties(${target_name} PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${APP_TEAM_ID}"
        )
    endif()

    # Add resources
    if(APP_RESOURCES)
        target_sources(${target_name} PRIVATE ${APP_RESOURCES})
        set_source_files_properties(${APP_RESOURCES} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
        )
    endif()

    # Add storyboards
    if(APP_STORYBOARDS)
        target_sources(${target_name} PRIVATE ${APP_STORYBOARDS})
        set_source_files_properties(${APP_STORYBOARDS} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources
        )
    endif()

    # Link frameworks
    target_link_libraries(${target_name} PRIVATE
        "-framework Foundation"
        "-framework UIKit"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework MetalKit"
        "-framework OpenGLES"
        "-framework CoreGraphics"
        "-framework CoreLocation"
    )

    # Link additional frameworks
    if(APP_FRAMEWORKS)
        foreach(framework ${APP_FRAMEWORKS})
            target_link_libraries(${target_name} PRIVATE "-framework ${framework}")
        endforeach()
    endif()

    # Link Nova3D engine
    target_link_libraries(${target_name} PRIVATE nova3d_ios)

endfunction()

# =============================================================================
# Message summary
# =============================================================================

message(STATUS "")
message(STATUS "iOS Toolchain Configuration:")
message(STATUS "  Platform:          ${PLATFORM}")
message(STATUS "  Architecture:      ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "  SDK:               ${CMAKE_OSX_SYSROOT}")
message(STATUS "  Deployment Target: ${DEPLOYMENT_TARGET}")
message(STATUS "  Enable Bitcode:    ${ENABLE_BITCODE}")
message(STATUS "  Enable ARC:        ${ENABLE_ARC}")
message(STATUS "")
