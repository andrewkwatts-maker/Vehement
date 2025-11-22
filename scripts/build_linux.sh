#!/bin/bash
# =============================================================================
# Nova3D Engine - Linux Build Script
# =============================================================================
#
# This script builds the Nova3D Engine for Linux platforms.
#
# Prerequisites:
#   - CMake 3.18+
#   - GCC 11+ or Clang 14+
#   - Development libraries: X11, GLFW, OpenGL, Vulkan (optional)
#
# Installation of dependencies (Ubuntu/Debian):
#   sudo apt install build-essential cmake ninja-build \
#       libx11-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev \
#       libgl1-mesa-dev libglu1-mesa-dev \
#       libwayland-dev libxkbcommon-dev \
#       libvulkan-dev vulkan-tools \
#       libpulse-dev libasound2-dev \
#       libglfw3-dev
#
# Usage:
#   ./build_linux.sh                    # Build Release
#   ./build_linux.sh --debug            # Build Debug
#   ./build_linux.sh --vulkan           # Enable Vulkan support
#   ./build_linux.sh --clean            # Clean build directory
#   ./build_linux.sh --install          # Install to system
#
# =============================================================================

set -e  # Exit on error

# =============================================================================
# Configuration
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build-linux"

# Default settings
BUILD_TYPE="Release"
CLEAN_BUILD=false
ENABLE_VULKAN=true
ENABLE_SCRIPTING=true
DO_INSTALL=false
INSTALL_PREFIX="/usr/local"
PARALLEL_JOBS=$(nproc 2>/dev/null || echo 4)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --debug           Build in Debug mode"
    echo "  --release         Build in Release mode (default)"
    echo "  --relwithdebinfo  Build RelWithDebInfo (release + debug symbols)"
    echo "  --vulkan          Enable Vulkan support (default: on)"
    echo "  --no-vulkan       Disable Vulkan support"
    echo "  --scripting       Enable Python scripting (default: on)"
    echo "  --no-scripting    Disable Python scripting"
    echo "  --clean           Clean build directory before building"
    echo "  --install         Install to system after build"
    echo "  --prefix <path>   Installation prefix (default: /usr/local)"
    echo "  --clang           Use Clang compiler"
    echo "  --gcc             Use GCC compiler (default)"
    echo "  --jobs <n>        Number of parallel jobs (default: auto)"
    echo "  --help            Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Build Release with GCC"
    echo "  $0 --debug --clang           # Build Debug with Clang"
    echo "  $0 --release --install       # Build and install"
}

check_prerequisites() {
    print_step "Checking prerequisites..."

    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Install via: sudo apt install cmake"
        exit 1
    fi
    CMAKE_VERSION=$(cmake --version | head -n1)
    print_info "Found $CMAKE_VERSION"

    # Check for compiler
    if [ -n "$USE_CLANG" ]; then
        if ! command -v clang++ &> /dev/null; then
            print_error "Clang not found. Install via: sudo apt install clang"
            exit 1
        fi
        CXX_COMPILER=$(clang++ --version | head -n1)
    else
        if ! command -v g++ &> /dev/null; then
            print_error "G++ not found. Install via: sudo apt install g++"
            exit 1
        fi
        CXX_COMPILER=$(g++ --version | head -n1)
    fi
    print_info "Using compiler: $CXX_COMPILER"

    # Check for Ninja (optional but recommended)
    if command -v ninja &> /dev/null; then
        CMAKE_GENERATOR="Ninja"
        print_info "Using Ninja build system"
    else
        CMAKE_GENERATOR="Unix Makefiles"
        print_info "Using Unix Makefiles (install ninja-build for faster builds)"
    fi

    # Check for required development libraries
    print_step "Checking development libraries..."

    MISSING_DEPS=""

    # Check X11
    if ! pkg-config --exists x11 2>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS libx11-dev"
    fi

    # Check OpenGL
    if ! pkg-config --exists gl 2>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS libgl1-mesa-dev"
    fi

    # Check GLFW
    if ! pkg-config --exists glfw3 2>/dev/null; then
        print_warning "GLFW not found (will be fetched via FetchContent)"
    else
        GLFW_VERSION=$(pkg-config --modversion glfw3)
        print_info "Found GLFW $GLFW_VERSION"
    fi

    # Check Vulkan (optional)
    if [ "$ENABLE_VULKAN" = true ]; then
        if ! pkg-config --exists vulkan 2>/dev/null; then
            print_warning "Vulkan not found. Install libvulkan-dev for Vulkan support."
            print_warning "Building without Vulkan support..."
            ENABLE_VULKAN=false
        else
            VULKAN_VERSION=$(pkg-config --modversion vulkan)
            print_info "Found Vulkan $VULKAN_VERSION"
        fi
    fi

    # Check PulseAudio (optional)
    if pkg-config --exists libpulse-simple 2>/dev/null; then
        print_info "Found PulseAudio"
    else
        print_warning "PulseAudio not found. Audio will use ALSA fallback."
    fi

    # Check Python (for scripting)
    if [ "$ENABLE_SCRIPTING" = true ]; then
        if ! python3 --version &> /dev/null; then
            print_warning "Python 3 not found. Disabling scripting support."
            ENABLE_SCRIPTING=false
        else
            PYTHON_VERSION=$(python3 --version)
            print_info "Found $PYTHON_VERSION"
        fi
    fi

    if [ -n "$MISSING_DEPS" ]; then
        print_error "Missing dependencies:$MISSING_DEPS"
        print_error "Install with: sudo apt install$MISSING_DEPS"
        exit 1
    fi

    print_info "All prerequisites satisfied"
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
        --relwithdebinfo)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        --vulkan)
            ENABLE_VULKAN=true
            shift
            ;;
        --no-vulkan)
            ENABLE_VULKAN=false
            shift
            ;;
        --scripting)
            ENABLE_SCRIPTING=true
            shift
            ;;
        --no-scripting)
            ENABLE_SCRIPTING=false
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --install)
            DO_INSTALL=true
            shift
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --clang)
            USE_CLANG=true
            shift
            ;;
        --gcc)
            USE_CLANG=false
            shift
            ;;
        --jobs)
            PARALLEL_JOBS="$2"
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

echo ""
echo "============================================="
echo "  Nova3D Engine - Linux Build"
echo "============================================="
echo ""

check_prerequisites

# Set compiler if using Clang
CMAKE_EXTRA_ARGS=""
if [ "$USE_CLANG" = true ]; then
    CMAKE_EXTRA_ARGS="$CMAKE_EXTRA_ARGS -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
fi

# Clean if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_step "Cleaning build directory..."
    rm -rf "$BUILD_ROOT"
fi

# Create build directory
BUILD_DIR="$BUILD_ROOT"
mkdir -p "$BUILD_DIR"

print_step "Configuring CMake..."
print_info "Build Type: $BUILD_TYPE"
print_info "Vulkan: $ENABLE_VULKAN"
print_info "Scripting: $ENABLE_SCRIPTING"
print_info "Install Prefix: $INSTALL_PREFIX"

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -G "$CMAKE_GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DNOVA_BUILD_EXAMPLES=ON \
    -DNOVA_BUILD_TESTS=OFF \
    -DNOVA_ENABLE_SCRIPTING="$ENABLE_SCRIPTING" \
    $CMAKE_EXTRA_ARGS

print_step "Building..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$PARALLEL_JOBS"

# Install if requested
if [ "$DO_INSTALL" = true ]; then
    print_step "Installing..."
    sudo cmake --install "$BUILD_DIR"
    print_info "Installed to $INSTALL_PREFIX"
fi

# =============================================================================
# Summary
# =============================================================================

echo ""
echo "============================================="
echo "  Build Summary"
echo "============================================="
echo ""
print_info "Build Type:     $BUILD_TYPE"
print_info "Generator:      $CMAKE_GENERATOR"
print_info "Vulkan:         $ENABLE_VULKAN"
print_info "Scripting:      $ENABLE_SCRIPTING"
print_info "Parallel Jobs:  $PARALLEL_JOBS"
print_info "Output:         $BUILD_DIR"
echo ""

# Show built binaries
if [ -d "$BUILD_DIR/bin" ]; then
    print_info "Built executables:"
    ls -la "$BUILD_DIR/bin/"* 2>/dev/null || echo "  (none)"
fi

if [ -d "$BUILD_DIR/lib" ]; then
    print_info "Built libraries:"
    ls -la "$BUILD_DIR/lib/"*.a 2>/dev/null || ls -la "$BUILD_DIR/lib/"*.so 2>/dev/null || echo "  (none)"
fi

echo ""
print_info "Build completed successfully!"
echo ""

# Run instructions
if [ -f "$BUILD_DIR/bin/nova_demo" ]; then
    print_info "To run the demo:"
    print_info "  cd $BUILD_DIR/bin && ./nova_demo"
fi
