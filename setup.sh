#!/bin/bash
# Nova3D Engine - Linux/macOS Setup Script
# This script automates the build process and dependency fetching

set -e

echo ""
echo "============================================"
echo "  Nova3D Engine - Build Setup"
echo "============================================"
echo ""

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=0
GENERATOR=""
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Parse arguments
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
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --ninja)
            GENERATOR="-G Ninja"
            shift
            ;;
        --help)
            echo "Usage: ./setup.sh [options]"
            echo ""
            echo "Options:"
            echo "  --debug     Build in Debug mode"
            echo "  --release   Build in Release mode (default)"
            echo "  --clean     Clean build directory before building"
            echo "  --ninja     Use Ninja generator (faster builds)"
            echo "  --help      Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check for required tools
check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo "[ERROR] $1 not found in PATH"
        echo "Please install $1"
        exit 1
    fi
}

check_tool cmake
check_tool git
check_tool g++ || check_tool clang++

# Check for required system libraries
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "[INFO] Checking for required system libraries..."

    # Check for X11/Wayland development libraries
    if ! pkg-config --exists x11 2>/dev/null && ! pkg-config --exists wayland-client 2>/dev/null; then
        echo "[WARNING] X11 or Wayland development libraries may be missing"
        echo "  Ubuntu/Debian: sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev"
        echo "  Fedora: sudo dnf install libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel"
        echo ""
    fi
fi

BUILD_DIR="build"

# Clean if requested
if [[ $CLEAN_BUILD -eq 1 ]]; then
    echo "[INFO] Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

echo "[INFO] Build Type: $BUILD_TYPE"
echo "[INFO] Build Directory: $BUILD_DIR"
echo "[INFO] Parallel Jobs: $JOBS"
echo ""

# Configure with CMake
echo "[INFO] Configuring project with CMake..."
echo "[INFO] This will automatically fetch all dependencies..."
echo ""

cd "$BUILD_DIR"

cmake .. $GENERATOR \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DNOVA_BUILD_EXAMPLES=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo ""
echo "[INFO] Configuration complete!"
echo ""

# Build the project
echo "[INFO] Building project with $JOBS parallel jobs..."
cmake --build . --config "$BUILD_TYPE" --parallel "$JOBS"

cd ..

echo ""
echo "============================================"
echo "  Build Complete!"
echo "============================================"
echo ""
echo "Executable location: $BUILD_DIR/bin/nova_demo"
echo ""
echo "To run: ./$BUILD_DIR/bin/nova_demo"
echo ""
