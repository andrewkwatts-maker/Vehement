#!/bin/bash
# =============================================================================
# Nova3D Engine - iOS Build Script
# =============================================================================
#
# This script builds the Nova3D Engine for iOS platforms.
#
# Prerequisites:
#   - macOS with Xcode installed
#   - CMake 3.20+
#   - Xcode Command Line Tools
#
# Usage:
#   ./build_ios.sh                        # Build for iOS device (Release)
#   ./build_ios.sh --simulator            # Build for iOS Simulator
#   ./build_ios.sh --debug                # Build Debug configuration
#   ./build_ios.sh --xcode                # Generate Xcode project only
#   ./build_ios.sh --clean                # Clean build directory
#
# =============================================================================

set -e  # Exit on error

# =============================================================================
# Configuration
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build-ios"

# Default settings
BUILD_TYPE="Release"
TARGET_PLATFORM="OS64"  # OS64 = iOS device (arm64)
GENERATE_XCODE_ONLY=false
CLEAN_BUILD=false
IOS_DEPLOYMENT_TARGET="14.0"
PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# =============================================================================
# Helper Functions
# =============================================================================

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --debug           Build in Debug mode"
    echo "  --release         Build in Release mode (default)"
    echo "  --simulator       Build for iOS Simulator"
    echo "  --device          Build for iOS Device (default)"
    echo "  --universal       Build universal binary (device + simulator)"
    echo "  --xcode           Generate Xcode project only (don't build)"
    echo "  --target <ver>    Set iOS deployment target (default: 14.0)"
    echo "  --clean           Clean build directory before building"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Build for iOS device, Release"
    echo "  $0 --simulator --debug       # Build for simulator, Debug"
    echo "  $0 --xcode                   # Generate Xcode project"
}

check_prerequisites() {
    # Check we're on macOS
    if [[ "$(uname)" != "Darwin" ]]; then
        print_error "This script must be run on macOS"
        exit 1
    fi

    # Check for Xcode
    if ! command -v xcodebuild &> /dev/null; then
        print_error "Xcode is not installed. Please install Xcode from the App Store."
        exit 1
    fi

    XCODE_VERSION=$(xcodebuild -version | head -n1)
    print_info "Using $XCODE_VERSION"

    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Install via: brew install cmake"
        exit 1
    fi

    CMAKE_VERSION=$(cmake --version | head -n1)
    print_info "$CMAKE_VERSION"

    # Check for iOS toolchain (we'll create one if needed)
    TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/ios.toolchain.cmake"
    if [ ! -f "$TOOLCHAIN_FILE" ]; then
        print_warning "iOS toolchain not found, will use CMake's iOS support"
        TOOLCHAIN_FILE=""
    fi
}

create_toolchain_if_needed() {
    # Create cmake directory if it doesn't exist
    mkdir -p "$PROJECT_ROOT/cmake"

    # If toolchain doesn't exist, create a minimal one
    if [ ! -f "$PROJECT_ROOT/cmake/ios.toolchain.cmake" ]; then
        print_info "Creating iOS toolchain file..."
        cat > "$PROJECT_ROOT/cmake/ios.toolchain.cmake" << 'TOOLCHAIN_EOF'
# iOS CMake Toolchain File
# Based on cmake-ios-toolchain

# Platform selection
if(NOT DEFINED PLATFORM)
    set(PLATFORM "OS64")
endif()

# Set system name
set(CMAKE_SYSTEM_NAME iOS)

# Determine SDK and architectures
if(PLATFORM STREQUAL "OS64")
    set(CMAKE_OSX_SYSROOT iphoneos)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
elseif(PLATFORM STREQUAL "SIMULATOR64")
    set(CMAKE_OSX_SYSROOT iphonesimulator)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
elseif(PLATFORM STREQUAL "SIMULATORARM64")
    set(CMAKE_OSX_SYSROOT iphonesimulator)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

# Deployment target
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
endif()

# Find developer directory
execute_process(
    COMMAND xcode-select -print-path
    OUTPUT_VARIABLE XCODE_DEVELOPER_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set compiler
set(CMAKE_C_COMPILER "${XCODE_DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang")
set(CMAKE_CXX_COMPILER "${XCODE_DEVELOPER_DIR}/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++")

# Enable ARC
set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)

# Enable bitcode (optional, disabled by default now)
set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE NO)

# Code signing (development)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "")
TOOLCHAIN_EOF
        print_info "Toolchain created at $PROJECT_ROOT/cmake/ios.toolchain.cmake"
    fi
}

# =============================================================================
# Parse Arguments
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --simulator)
            TARGET_PLATFORM="SIMULATOR64"
            shift
            ;;
        --device)
            TARGET_PLATFORM="OS64"
            shift
            ;;
        --universal)
            TARGET_PLATFORM="COMBINED"
            shift
            ;;
        --xcode)
            GENERATE_XCODE_ONLY=true
            shift
            ;;
        --target)
            IOS_DEPLOYMENT_TARGET="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# =============================================================================
# Main Build Process
# =============================================================================

print_info "Nova3D iOS Build"
print_info "================"

check_prerequisites
create_toolchain_if_needed

TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/ios.toolchain.cmake"

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_ROOT"
fi

# Determine build directory name
if [ "$TARGET_PLATFORM" = "SIMULATOR64" ]; then
    BUILD_DIR="$BUILD_ROOT/simulator"
    PLATFORM_NAME="iOS Simulator"
elif [ "$TARGET_PLATFORM" = "COMBINED" ]; then
    BUILD_DIR="$BUILD_ROOT/universal"
    PLATFORM_NAME="iOS Universal"
else
    BUILD_DIR="$BUILD_ROOT/device"
    PLATFORM_NAME="iOS Device"
fi

mkdir -p "$BUILD_DIR"

print_info ""
print_info "Building for: $PLATFORM_NAME"
print_info "Build Type: $BUILD_TYPE"
print_info "Deployment Target: iOS $IOS_DEPLOYMENT_TARGET"
print_info "-------------------------------------------"

# Configure
print_info "Configuring CMake..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -G "Xcode" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DPLATFORM="$TARGET_PLATFORM" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$IOS_DEPLOYMENT_TARGET" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DNOVA_BUILD_EXAMPLES=OFF \
    -DNOVA_BUILD_TESTS=OFF \
    -DNOVA_ENABLE_SCRIPTING=OFF

# Generate Xcode project only?
if [ "$GENERATE_XCODE_ONLY" = true ]; then
    print_info ""
    print_info "Xcode project generated at: $BUILD_DIR"
    print_info "Open with: open $BUILD_DIR/*.xcodeproj"
    exit 0
fi

# Build
print_info "Building..."

# Determine SDK
if [ "$TARGET_PLATFORM" = "SIMULATOR64" ] || [ "$TARGET_PLATFORM" = "SIMULATORARM64" ]; then
    SDK="iphonesimulator"
else
    SDK="iphoneos"
fi

cmake --build "$BUILD_DIR" \
    --config "$BUILD_TYPE" \
    -- \
    -sdk "$SDK" \
    -allowProvisioningUpdates \
    -jobs "$PARALLEL_JOBS"

# =============================================================================
# Summary
# =============================================================================

print_info ""
print_info "==================================="
print_info "Build Summary"
print_info "==================================="
print_info "Platform:      $PLATFORM_NAME"
print_info "Build Type:    $BUILD_TYPE"
print_info "iOS Target:    $IOS_DEPLOYMENT_TARGET"
print_info "Output:        $BUILD_DIR"
print_info ""

# List built frameworks/libraries
PRODUCTS_DIR="$BUILD_DIR/$BUILD_TYPE-$SDK"
if [ -d "$PRODUCTS_DIR" ]; then
    print_info "Built products:"
    ls -la "$PRODUCTS_DIR" 2>/dev/null || echo "  (checking alternative paths)"
fi

print_info ""
print_info "Build completed successfully!"
print_info ""
print_info "To open in Xcode:"
print_info "  open $BUILD_DIR/*.xcodeproj"
