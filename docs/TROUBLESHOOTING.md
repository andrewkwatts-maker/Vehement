# Nova3D Engine Troubleshooting Guide

This guide provides solutions for common issues encountered when building and running Nova3D Engine.

## Table of Contents

1. [Build Issues](#build-issues)
   - [nlohmann/json C++20 Ranges Conflict](#nlohmannjson-c20-ranges-conflict-msvc)
   - [Missing Standard Headers](#missing-standard-headers)
   - [GLM Issues](#glm-issues)
   - [Compiler Requirements](#compiler-requirements)
   - [Linker Errors](#linker-errors)
2. [Platform-Specific Issues](#platform-specific-issues)
   - [Windows](#windows)
   - [Linux](#linux)
   - [macOS](#macos)
3. [Runtime Issues](#runtime-issues)
   - [OpenGL Errors](#opengl-errors)
   - [Python Scripting](#python-scripting)
   - [Audio System](#audio-system)
4. [Editor Issues](#editor-issues)
5. [Performance Issues](#performance-issues)

---

## Build Issues

### nlohmann/json C++20 Ranges Conflict (MSVC)

**Status:** Known critical blocker

**Symptoms:**
- 100+ template compilation errors
- Errors reference `<algorithm>` header
- Errors mention `std::ranges` namespace
- Error messages like: `'ranges' is not a member of 'std'`

**Root Cause:**
nlohmann/json v3.11.x creates a `nlohmann::std::ranges` namespace which pollutes MSVC's `std::ranges` namespace, causing widespread compilation failures when any standard library header uses C++20 ranges.

**Attempted Fixes (Not Working):**

1. **Compile definitions (too late)**
   ```cmake
   target_compile_definitions(nova3d PUBLIC JSON_HAS_CPP_20=0 JSON_HAS_RANGES=0)
   ```
   Issue: Definitions are set after nlohmann/json headers are already processed.

2. **Defines before FetchContent**
   ```cmake
   set(JSON_HAS_CPP_20 OFF CACHE BOOL "" FORCE)
   set(JSON_HAS_RANGES OFF CACHE BOOL "" FORCE)
   ```
   Issue: CMake cache variables don't affect header-level feature detection.

3. **Downgrading nlohmann/json**
   - v3.10.5: CMake 3.5 compatibility issues
   - v3.11.2: Same ranges conflict

4. **Switching to C++17**
   Issue: Codebase requires C++20 features (`std::source_location`, `std::format`, concepts).

**Recommended Workarounds:**

1. **Use single-include json.hpp with manual defines**
   ```cpp
   // Create a wrapper header: json_wrapper.hpp
   #define JSON_HAS_CPP_20 0
   #define JSON_HAS_RANGES 0
   #include <nlohmann/json.hpp>
   ```
   Then use `json_wrapper.hpp` instead of direct include.

2. **Fork and patch nlohmann/json**
   - Clone the library
   - Modify the ranges detection macros
   - Use local copy in FetchContent

3. **Use alternative JSON library**
   - RapidJSON (header-only, no C++20 issues)
   - simdjson (high performance)
   - Boost.JSON

4. **Update MSVC to latest version**
   Check for newer MSVC updates that may have better ranges support.

5. **Force-include workaround**
   ```cmake
   target_compile_options(nova3d PRIVATE /FI"${PROJECT_SOURCE_DIR}/include/json_defines.hpp")
   ```
   Where `json_defines.hpp` contains the necessary defines.

**Reference:** See [CLAUDE.md](../CLAUDE.md) for detailed development notes on this issue.

---

### Missing Standard Headers

**Symptom:** `error: 'string' is not a member of 'std'`

**Cause:** Some headers don't include all necessary standard library headers.

**Solution:** Add the missing include to the affected header file.

**Known Fixed Files:**
```cpp
// engine/graphics/Renderer.hpp - line 5
#include <string>

// engine/graphics/LODManager.hpp
#include <string>

// engine/graphics/Batching.hpp
#include <string>
```

**General Fix:**
```cpp
// Add to any header that uses std::string without including <string>
#include <string>
```

---

### GLM Issues

#### Constants Not Found

**Symptom:** `error: 'pi' is not a member of 'glm'`

**Solution:**
```cpp
// Add this include where GLM constants are used
#include <glm/gtc/constants.hpp>

// Usage
float pi = glm::pi<float>();
float tau = glm::two_pi<float>();
```

**Known Fixed Files:**
- `engine/graphics/Mesh.cpp` - line 6

#### Extension Functions Not Found

**Symptom:** Undefined reference to `glm::rotate`, `glm::translate`, etc.

**Solution:**
```cpp
#include <glm/gtc/matrix_transform.hpp>
```

#### Type Traits Issues

**Symptom:** `glm::value_ptr` not found

**Solution:**
```cpp
#include <glm/gtc/type_ptr.hpp>
```

---

### Compiler Requirements

**Minimum Versions:**
| Compiler | Minimum Version | C++ Standard |
|----------|-----------------|--------------|
| MSVC | 2022 (19.30+) | C++23 |
| GCC | 13+ | C++23 |
| Clang | 16+ | C++23 |

**Check Compiler Version:**

```bash
# GCC
g++ --version

# Clang
clang++ --version

# MSVC (in Developer Command Prompt)
cl
```

**CMake Configuration:**
```cmake
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

---

### Linker Errors

#### Undefined Reference to Engine Functions

**Symptom:** `undefined reference to 'Nova::Engine::Instance()'`

**Solution:** Ensure you're linking against `nova3d` library:
```cmake
target_link_libraries(your_target PRIVATE nova3d)
```

#### Multiple Definition Errors

**Symptom:** `multiple definition of 'SomeClass::Method()'`

**Cause:** Function defined in header without `inline` keyword.

**Solution:**
```cpp
// In header file, use inline for definitions
inline void SomeClass::Method() {
    // implementation
}

// Or move to .cpp file
```

#### Texture API Mismatch

**Symptom:** `undefined reference to 'Texture::LoadFromFile'`

**Cause:** API changed from `LoadFromFile` to `Load`.

**Solution:** Update calls:
```cpp
// Old (incorrect)
texture.LoadFromFile(path);

// New (correct)
texture.Load(path);

// Old (incorrect)
texture.LoadFromMemory(data, size);

// New (correct)
texture.Create(width, height, format, data);
```

---

## Platform-Specific Issues

### Windows

#### Visual Studio Project Issues

**Symptom:** Project won't open or configure fails.

**Solution:**
1. Ensure Visual Studio 2022 is installed with "Desktop development with C++"
2. Delete `build` folder and reconfigure:
   ```powershell
   Remove-Item -Recurse -Force build
   cmake -B build -G "Visual Studio 17 2022" -A x64
   ```

#### Missing Windows SDK

**Symptom:** `windows.h not found`

**Solution:** Install Windows SDK via Visual Studio Installer.

#### DLL Not Found at Runtime

**Symptom:** Missing DLL errors when running.

**Solution:**
- Run from correct working directory
- Add DLL paths to PATH
- Copy DLLs to executable directory

---

### Linux

#### X11 Development Headers Missing

**Symptom:** GLFW build fails with X11 errors.

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Fedora
sudo dnf install libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel

# Arch
sudo pacman -S libx11 libxrandr libxinerama libxcursor libxi
```

#### OpenGL Development Headers Missing

**Symptom:** `GL/gl.h not found`

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libgl1-mesa-dev libglu1-mesa-dev

# Fedora
sudo dnf install mesa-libGL-devel mesa-libGLU-devel

# Arch
sudo pacman -S mesa glu
```

#### GCC Version Too Old

**Symptom:** C++23 features not recognized.

**Solution:**
```bash
# Ubuntu - add toolchain PPA
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install g++-13

# Configure CMake to use it
cmake -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++-13
```

---

### macOS

#### Xcode Command Line Tools Missing

**Symptom:** `xcrun: error: invalid active developer path`

**Solution:**
```bash
xcode-select --install
```

#### Homebrew Dependencies

**Solution:**
```bash
brew install cmake python
```

#### OpenGL Deprecation Warnings

**Note:** OpenGL is deprecated on macOS but still functional. Warnings can be suppressed:
```cmake
target_compile_definitions(nova3d PRIVATE GL_SILENCE_DEPRECATION)
```

---

## Runtime Issues

### OpenGL Errors

#### Context Creation Failed

**Symptom:** Window doesn't open, OpenGL context errors.

**Solutions:**
1. Update graphics drivers
2. Check OpenGL version support:
   ```bash
   # Linux
   glxinfo | grep "OpenGL version"
   ```
3. Try different GLFW hints:
   ```cpp
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);  // Try lower version
   ```

#### Shader Compilation Failed

**Symptom:** Black screen, shader errors in console.

**Solutions:**
1. Check shader file paths
2. Validate GLSL version compatibility
3. Enable shader debug output:
   ```cpp
   Nova::Renderer::EnableDebugOutput(true);
   ```

---

### Python Scripting

#### Python Not Found

**Symptom:** CMake can't find Python.

**Solution:**
```bash
cmake -B build -DPython3_EXECUTABLE=/path/to/python3
```

#### pybind11 Errors

**Symptom:** pybind11 compilation errors.

**Solution:** Ensure Python development headers are installed:
```bash
# Ubuntu/Debian
sudo apt install python3-dev

# Fedora
sudo dnf install python3-devel

# macOS
brew install python
```

#### Script Hot-Reload Not Working

**Solutions:**
1. Check file watching is enabled
2. Verify script file permissions
3. Check console for errors

---

### Audio System

#### Audio Not Playing

**Note:** Audio is disabled by default.

**Enable Audio:**
```bash
cmake -B build -DNOVA_ENABLE_AUDIO=ON
```

**When Disabled:**
The stub implementation provides the same API but logs warnings:
```
[WARNING] AudioEngine: Audio system is disabled (stub implementation)
```

#### OpenAL Initialization Failed

**Solutions:**
1. Install OpenAL:
   ```bash
   # Ubuntu
   sudo apt install libopenal-dev
   ```
2. Check audio device availability
3. Try different audio backend

---

## Editor Issues

#### ImGui Docking Not Working

**Symptom:** Windows can't be docked.

**Cause:** ImGui not using docking branch.

**Solution:** CMakeLists.txt should use docking branch:
```cmake
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
```

#### Layout Not Saving

**Solutions:**
1. Check write permissions to config directory
2. Verify imgui.ini path
3. Call `ImGui::SaveIniSettingsToDisk()` manually

#### Asset Thumbnails Not Generating

**Solutions:**
1. Check AssetThumbnailCache initialization
2. Verify OpenGL context is current
3. Check background thread is running

---

## Performance Issues

### Low Frame Rate

**Diagnostic Steps:**
1. Check GPU utilization
2. Profile with `PROFILE_SCOPE()` macros
3. Check renderer statistics:
   ```cpp
   auto stats = renderer.GetStats();
   std::cout << "Draw calls: " << stats.drawCalls << std::endl;
   ```

**Common Causes:**
- Too many SDF primitives (limit to 256 per model)
- BVH not being used for large scenes
- GI settings too high

### GI Performance

**Optimization:**
```cpp
RadianceCascade::Config config;
config.numCascades = 3;        // Reduce from 4
config.baseResolution = 16;    // Reduce from 32
config.raysPerProbe = 32;      // Reduce from 64
config.maxProbesPerFrame = 512; // Reduce from 1024
```

### Memory Usage

**Check Memory:**
```cpp
auto stats = accel.GetStats();
std::cout << "BVH memory: " << stats.memoryBytes / 1024 << " KB" << std::endl;
```

**Reduce Memory:**
- Lower SDF mesh resolution
- Reduce GI cascade count
- Enable mesh simplification

---

## Getting Help

If your issue isn't covered here:

1. **Check existing documentation:**
   - [CLAUDE.md](../CLAUDE.md) - Development notes and known issues
   - [BUILDING.md](BUILDING.md) - Build instructions

2. **Search GitHub Issues** for similar problems

3. **Create a new issue** with:
   - Nova3D version
   - Operating system and version
   - Compiler version
   - Complete error message
   - Steps to reproduce
   - Minimal reproduction code

---

*Last Updated: 2026-01-21*
