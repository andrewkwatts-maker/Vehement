# =============================================================================
# Nova3D Engine - iOS Platform Support
# =============================================================================
#
# This CMakeLists.txt builds the iOS platform layer for the Nova3D engine.
#
# Usage (from project root):
#   mkdir build-ios && cd build-ios
#   cmake -G Xcode \
#         -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake \
#         -DPLATFORM=OS64 \
#         ..
#   cmake --build . --config Release
#
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# Check if building for iOS
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "This CMakeLists.txt is for iOS builds only!")
endif()

# iOS Platform Library
add_library(nova3d_ios_platform STATIC
    IOSPlatform.mm
    IOSGLContext.mm
    MetalRenderer.mm
    IOSTouchInput.mm
    IOSAppDelegate.mm
)

target_include_directories(nova3d_ios_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Set Objective-C++ compile options
set_target_properties(nova3d_ios_platform PROPERTIES
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
)

target_compile_options(nova3d_ios_platform PRIVATE
    -fobjc-arc
    -fmodules
)

# Link iOS frameworks
target_link_libraries(nova3d_ios_platform PUBLIC
    "-framework Foundation"
    "-framework UIKit"
    "-framework QuartzCore"
    "-framework Metal"
    "-framework MetalKit"
    "-framework OpenGLES"
    "-framework CoreGraphics"
    "-framework CoreLocation"
    "-framework CoreMotion"
    "-framework CoreHaptics"
    "-framework AVFoundation"
    "-framework AudioToolbox"
    "-framework UserNotifications"
)

# Link GLM (header-only)
target_link_libraries(nova3d_ios_platform PUBLIC
    glm::glm
)

# Export compile definitions
target_compile_definitions(nova3d_ios_platform PUBLIC
    NOVA_PLATFORM_IOS
    GLES_SILENCE_DEPRECATION
)

# =============================================================================
# iOS Engine Library (combines platform with core engine)
# =============================================================================

add_library(nova3d_ios STATIC
    # This would include the main engine sources adapted for iOS
    # For now, it just links to the platform layer
)

target_link_libraries(nova3d_ios PUBLIC
    nova3d_ios_platform
)

# =============================================================================
# Vehement2 iOS App Target
# =============================================================================

if(NOVA_BUILD_VEHEMENT2_IOS)
    # Find all game source files
    file(GLOB_RECURSE VEHEMENT2_SOURCES
        "${CMAKE_SOURCE_DIR}/Vehement2/*.cpp"
        "${CMAKE_SOURCE_DIR}/Vehement2/*.hpp"
    )

    # iOS app sources
    set(VEHEMENT2_IOS_SOURCES
        ${CMAKE_SOURCE_DIR}/ios/Vehement2/AppDelegate.mm
        ${CMAKE_SOURCE_DIR}/ios/Vehement2/ViewController.mm
    )

    # iOS app resources
    set(VEHEMENT2_IOS_RESOURCES
        ${CMAKE_SOURCE_DIR}/ios/Vehement2/Assets.xcassets
        ${CMAKE_SOURCE_DIR}/ios/Vehement2/LaunchScreen.storyboard
        ${CMAKE_SOURCE_DIR}/assets
    )

    # Create iOS app bundle
    add_executable(Vehement2 MACOSX_BUNDLE
        ${VEHEMENT2_SOURCES}
        ${VEHEMENT2_IOS_SOURCES}
        ${VEHEMENT2_IOS_RESOURCES}
    )

    # Set bundle properties
    set_target_properties(Vehement2 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Vehement2/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.nova3d.vehement2"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_BUNDLE_NAME "Vehement 2"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.nova3d.vehement2"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    )

    # Set resource file properties
    set_source_files_properties(${VEHEMENT2_IOS_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )

    # Link libraries
    target_link_libraries(Vehement2 PRIVATE
        nova3d_ios
    )

    # Include directories
    target_include_directories(Vehement2 PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/engine
        ${CMAKE_SOURCE_DIR}/Vehement2
    )

endif()

message(STATUS "")
message(STATUS "Nova3D iOS Platform Configuration:")
message(STATUS "  Platform Library:  nova3d_ios_platform")
message(STATUS "  Engine Library:    nova3d_ios")
if(NOVA_BUILD_VEHEMENT2_IOS)
    message(STATUS "  App Target:        Vehement2")
endif()
message(STATUS "")
