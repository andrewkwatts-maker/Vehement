# Building Nova3D Engine

This guide covers building Nova3D Engine on all supported platforms.

## Prerequisites

### All Platforms

- **CMake**: 3.20 or higher
- **Git**: For fetching dependencies
- **Python 3.8+**: Required for scripting support (optional)

### Windows

- **Visual Studio 2022** with C++ Desktop Development workload
- Or **MinGW-w64** with GCC 13+
- **Windows SDK** (usually included with VS)

### macOS

- **Xcode 15+** or Command Line Tools
- **Homebrew** (recommended for dependencies)

```bash
xcode-select --install
brew install cmake python
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake git python3 python3-dev
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
```

### Linux (Fedora)

```bash
sudo dnf install gcc-c++ cmake git python3 python3-devel
sudo dnf install libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
sudo dnf install mesa-libGL-devel mesa-libGLU-devel
```

### Linux (Arch)

```bash
sudo pacman -S base-devel cmake git python
sudo pacman -S libx11 libxrandr libxinerama libxcursor libxi
sudo pacman -S mesa glu
```

## CMake Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `NOVA_BUILD_EXAMPLES` | ON | Build example applications |
| `NOVA_BUILD_TESTS` | OFF | Build unit tests |
| `NOVA_USE_ASAN` | OFF | Enable Address Sanitizer |
| `NOVA_ENABLE_SCRIPTING` | ON | Enable Python scripting support |
| `CMAKE_BUILD_TYPE` | - | Debug, Release, RelWithDebInfo, MinSizeRel |

## Building for Desktop

### Windows with Visual Studio

```powershell
# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Debug
cmake --build build --config Debug --parallel

# Build Release
cmake --build build --config Release --parallel

# Run demo
.\build\bin\Debug\nova_demo.exe
```

### Windows with MinGW

```powershell
# Configure
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run demo
.\build\bin\nova_demo.exe
```

### macOS

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run demo
./build/bin/nova_demo
```

### macOS with Xcode

```bash
# Configure
cmake -B build -G Xcode

# Open in Xcode
open build/Nova3DEngine.xcodeproj

# Or build from command line
cmake --build build --config Release --parallel
```

### Linux

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run demo
./build/bin/nova_demo
```

## Building for Mobile

### Android

#### Prerequisites

1. **Android Studio** with NDK installed (r25+)
2. **Android SDK** (API level 24+)
3. Set environment variables:

```bash
export ANDROID_NDK=/path/to/ndk
export ANDROID_SDK=/path/to/sdk
```

#### Building

```bash
# Navigate to Android project
cd android

# Build with Gradle
./gradlew assembleDebug

# Or for release
./gradlew assembleRelease
```

#### CMake Cross-Compilation

```bash
# Configure for Android
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-android --parallel
```

### iOS

#### Prerequisites

1. **Xcode 15+**
2. **iOS SDK** (included with Xcode)
3. Apple Developer account for device deployment

#### Building

```bash
# Configure for iOS
cmake -B build-ios \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DIOS=ON

# Open in Xcode
open build-ios/Nova3DEngine.xcodeproj
```

#### Simulator Build

```bash
cmake -B build-ios-sim \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DIOS=ON
```

## IDE Setup

### Visual Studio Code

1. Install extensions:
   - C/C++ (Microsoft)
   - CMake Tools (Microsoft)
   - CMake (twxs)

2. Create `.vscode/settings.json`:

```json
{
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.generator": "Ninja",
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

3. Create `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Nova Demo",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/bin/nova_demo",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/build/bin",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "preLaunchTask": "cmake-build"
        }
    ]
}
```

### Visual Studio 2022

1. Open Visual Studio
2. Select "Open a local folder"
3. Choose the Nova3D root directory
4. CMake configuration starts automatically
5. Select build configuration (Debug/Release)
6. Build with Ctrl+Shift+B
7. Debug with F5

### CLion

1. Open CLion
2. Select "Open" and choose the Nova3D root directory
3. CLion auto-detects CMakeLists.txt
4. Configure toolchains in Settings > Build > Toolchains
5. Build with Ctrl+F9
6. Run/Debug with Shift+F10/F9

### Xcode

```bash
# Generate Xcode project
cmake -B build -G Xcode

# Open project
open build/Nova3DEngine.xcodeproj
```

## Build Configurations

### Debug Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Features:
- Full debug symbols
- No optimization
- Assertions enabled
- Debug logging

### Release Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Features:
- Full optimization (-O3)
- No debug symbols
- Assertions disabled
- Release logging

### RelWithDebInfo Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Features:
- Optimization with debug info
- Useful for profiling
- Assertions disabled

## Advanced Configuration

### Disabling Python Scripting

```bash
cmake -B build -DNOVA_ENABLE_SCRIPTING=OFF
```

### Enabling Address Sanitizer

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNOVA_USE_ASAN=ON
```

### Custom Install Location

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/nova3d
cmake --build build
cmake --install build
```

### Using Ninja

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

## Troubleshooting

### Common Issues

#### CMake can't find Python

```bash
# Specify Python path explicitly
cmake -B build -DPython3_EXECUTABLE=/usr/bin/python3
```

#### OpenGL headers not found

**Linux**:
```bash
sudo apt install libgl1-mesa-dev
```

**macOS**:
```bash
# OpenGL is part of system frameworks, no action needed
```

#### GLFW build fails on Linux

```bash
# Install X11 development packages
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

#### Linker errors with undefined references

Ensure you're using a C++23-compatible compiler:
```bash
g++ --version  # Should be 13+
clang++ --version  # Should be 16+
```

#### Out of memory during build

Reduce parallel jobs:
```bash
cmake --build build --parallel 2
```

### Clean Build

```bash
# Remove build directory
rm -rf build

# Reconfigure and rebuild
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Verbose Build

```bash
cmake --build build --verbose
```

### CMake Configuration Debug

```bash
cmake -B build --debug-output
```

## Installation

### System Installation

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Default install locations:
- **Libraries**: `/usr/local/lib`
- **Headers**: `/usr/local/include/nova3d`
- **Binaries**: `/usr/local/bin`

### Custom Installation

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/nova3d
cmake --build build
cmake --install build
```

## Dependencies

Dependencies are automatically fetched via CMake FetchContent. If you prefer pre-installed dependencies:

```bash
cmake -B build \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -Dglfw3_DIR=/path/to/glfw/cmake \
    -Dglm_DIR=/path/to/glm/cmake
```

### Pre-installing Dependencies (Optional)

```bash
# GLFW
git clone https://github.com/glfw/glfw.git
cd glfw && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/deps
make install

# GLM
git clone https://github.com/g-truc/glm.git
cd glm && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/deps
make install

# Then build Nova3D with:
cmake -B build -DCMAKE_PREFIX_PATH=/opt/deps
```

## Continuous Integration

### GitHub Actions Example

```yaml
name: Build
on: [push, pull_request]

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]

    steps:
    - uses: actions/checkout@v4

    - name: Install dependencies (Linux)
      if: runner.os == 'Linux'
      run: |
        sudo apt-get update
        sudo apt-get install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev

    - name: Configure
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --parallel
```
