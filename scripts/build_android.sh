#!/bin/bash
# =============================================================================
# Nova3D Engine - Android Build Script
# =============================================================================
#
# This script builds the Nova3D Engine for Android platforms.
#
# Prerequisites:
#   - Android NDK (r25 or later recommended)
#   - CMake 3.18+
#   - Ninja (optional, for faster builds)
#
# Environment Variables:
#   ANDROID_NDK_HOME - Path to Android NDK
#   ANDROID_SDK_HOME - Path to Android SDK (optional)
#
# Usage:
#   ./build_android.sh                    # Build all ABIs, Release
#   ./build_android.sh --debug            # Build all ABIs, Debug
#   ./build_android.sh --abi arm64-v8a    # Build specific ABI
#   ./build_android.sh --clean            # Clean build directory
#
# =============================================================================

set -e  # Exit on error

# =============================================================================
# Configuration
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build-android"

# Default settings
BUILD_TYPE="Release"
ANDROID_API_LEVEL="24"
TARGET_ABIS=("arm64-v8a" "armeabi-v7a")  # Default to ARM architectures
CLEAN_BUILD=false
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

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
    echo "  --abi <abi>       Build specific ABI (arm64-v8a, armeabi-v7a, x86_64, x86)"
    echo "  --all-abis        Build all supported ABIs"
    echo "  --api <level>     Set Android API level (default: 24)"
    echo "  --clean           Clean build directory before building"
    echo "  --ndk <path>      Override NDK path"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Build arm64-v8a and armeabi-v7a, Release"
    echo "  $0 --abi arm64-v8a --debug   # Build arm64-v8a only, Debug"
    echo "  $0 --all-abis --release      # Build all ABIs, Release"
}

check_prerequisites() {
    # Check for NDK
    if [ -z "$ANDROID_NDK_HOME" ]; then
        # Try common locations
        if [ -d "$HOME/Android/Sdk/ndk" ]; then
            # Find latest NDK version
            ANDROID_NDK_HOME=$(ls -d "$HOME/Android/Sdk/ndk/"* 2>/dev/null | sort -V | tail -n1)
        elif [ -d "/opt/android-ndk" ]; then
            ANDROID_NDK_HOME="/opt/android-ndk"
        fi
    fi

    if [ -z "$ANDROID_NDK_HOME" ] || [ ! -d "$ANDROID_NDK_HOME" ]; then
        print_error "Android NDK not found. Set ANDROID_NDK_HOME environment variable."
        exit 1
    fi

    print_info "Using NDK: $ANDROID_NDK_HOME"

    # Check NDK version
    if [ -f "$ANDROID_NDK_HOME/source.properties" ]; then
        NDK_VERSION=$(grep "Pkg.Revision" "$ANDROID_NDK_HOME/source.properties" | cut -d= -f2 | tr -d ' ')
        print_info "NDK version: $NDK_VERSION"
    fi

    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Please install CMake 3.18 or later."
        exit 1
    fi

    CMAKE_VERSION=$(cmake --version | head -n1 | grep -oP '\d+\.\d+')
    print_info "CMake version: $CMAKE_VERSION"

    # Check for Ninja (optional)
    if command -v ninja &> /dev/null; then
        CMAKE_GENERATOR="Ninja"
        print_info "Using Ninja build system"
    else
        CMAKE_GENERATOR="Unix Makefiles"
        print_info "Using Unix Makefiles (install Ninja for faster builds)"
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
        --abi)
            TARGET_ABIS=("$2")
            shift 2
            ;;
        --all-abis)
            TARGET_ABIS=("arm64-v8a" "armeabi-v7a" "x86_64" "x86")
            shift
            ;;
        --api)
            ANDROID_API_LEVEL="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --ndk)
            ANDROID_NDK_HOME="$2"
            shift 2
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

print_info "Nova3D Android Build"
print_info "===================="

check_prerequisites

# Toolchain file
TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    print_error "Toolchain file not found: $TOOLCHAIN_FILE"
    exit 1
fi

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_ROOT"
fi

# Create build root
mkdir -p "$BUILD_ROOT"

# Build each ABI
for ABI in "${TARGET_ABIS[@]}"; do
    print_info ""
    print_info "Building for ABI: $ABI"
    print_info "-------------------------------------------"

    BUILD_DIR="$BUILD_ROOT/$ABI"
    mkdir -p "$BUILD_DIR"

    # Configure
    print_info "Configuring CMake..."
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
        -G "$CMAKE_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DANDROID_STL="c++_shared" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DNOVA_BUILD_EXAMPLES=OFF \
        -DNOVA_BUILD_TESTS=OFF \
        -DNOVA_ENABLE_SCRIPTING=OFF

    # Build
    print_info "Building..."
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$PARALLEL_JOBS"

    print_info "Build complete for $ABI"
done

# =============================================================================
# Summary
# =============================================================================

print_info ""
print_info "==================================="
print_info "Build Summary"
print_info "==================================="
print_info "Build Type:    $BUILD_TYPE"
print_info "API Level:     $ANDROID_API_LEVEL"
print_info "ABIs:          ${TARGET_ABIS[*]}"
print_info "Output:        $BUILD_ROOT"
print_info ""

# List built libraries
for ABI in "${TARGET_ABIS[@]}"; do
    LIB_PATH="$BUILD_ROOT/$ABI/lib"
    if [ -d "$LIB_PATH" ]; then
        print_info "Libraries for $ABI:"
        ls -la "$LIB_PATH"/*.a 2>/dev/null || ls -la "$LIB_PATH"/*.so 2>/dev/null || echo "  (none found)"
    fi
done

print_info ""
print_info "Build completed successfully!"
