# Claude Development Notes

## Core Development Philosophy

**CRITICAL: Always push forward with C++ solutions in the editor/game. Never create Python/script workarounds when encountering C++ build bugs.**

When we encounter compilation errors or build issues:
1. Fix the C++ code properly
2. Debug and resolve the root cause
3. Never create "hacky workarounds" with Python or external scripts
4. The game engine must be self-contained in C++

## Recent Work: Icon Generation System

### Build Status (2025-12-07)

**Fixes Applied:**
- ✅ Added `#include <string>` to [Renderer.hpp](engine/graphics/Renderer.hpp:5)
- ✅ Added `#include <string>` to [LODManager.hpp](engine/graphics/LODManager.hpp)
- ✅ Added `#include <string>` to [Batching.hpp](engine/graphics/Batching.hpp)
- ✅ Added `#include <glm/gtc/constants.hpp>` to [Mesh.cpp](engine/graphics/Mesh.cpp:6)
- ✅ Fixed Texture API calls in [AssetThumbnailCache.cpp](engine/editor/AssetThumbnailCache.cpp) (Load instead of LoadFromFile, Create instead of LoadFromMemory)
- ✅ Cleaned all stale .obj files

**Remaining Build Issues:**
- ❌ [EventBinding.cpp](engine/events/EventBinding.cpp:108-120) - JSON parsing errors with nlohmann::json undefined type

**Completed C++ Implementation:**
- ✅ [AssetThumbnailCache.hpp](engine/editor/AssetThumbnailCache.hpp) (280 lines) - Full thumbnail cache API
- ✅ [AssetThumbnailCache.cpp](engine/editor/AssetThumbnailCache.cpp) (650+ lines) - Background thumbnail generation with priority queue
- ✅ [asset_media_renderer.cpp](tools/asset_media_renderer.cpp) (850+ lines) - Standalone CLI tool for batch icon/video generation
- ✅ Added to [CMakeLists.txt](CMakeLists.txt:1169-1174)

### Next Steps

1. Fix EventBinding.cpp JSON include issues
2. Build nova3d library successfully
3. Build asset_media_renderer tool
4. Generate hero icons using C++ rendering pipeline with full GI support
5. Generate animated video previews for units/buildings using idle animations

### Icon System Architecture

The system uses the **same C++ rendering pipeline** that the editor and game use:
- [SDFRenderer](engine/graphics/SDFRenderer.hpp) - SDF raymarching with GI
- [RadianceCascade](engine/graphics/RadianceCascade.hpp) - Global illumination
- [SpectralRenderer](engine/graphics/SpectralRenderer.hpp) - Wavelength-dependent rendering

**No Python rendering** - Python is only for build scripts and asset pipeline automation, never for core rendering functionality.

## Coding Standards

- Always use proper C++ solutions
- Fix bugs at their source, don't work around them
- Keep the engine self-contained
- Python is for tooling only, not core features
- When stuck: debug, fix, document - never hack around

---

*This file tracks development philosophy and critical reminders for AI assistance.*

### Build Session Summary (2025-12-07)

**Work Completed:**
1. ✅ Removed Python workaround (generate_hero_icons.py)
2. ✅ Fixed missing `#include <string>` in Renderer.hpp, LODManager.hpp, Batching.hpp
3. ✅ Fixed GLM constants in Mesh.cpp
4. ✅ Fixed Texture API in AssetThumbnailCache.cpp
5. ✅ Added nlohmann/json includes to EventBinding.cpp, EventCondition.hpp, TypeInfo.hpp
6. ✅ Created [CLAUDE.md](CLAUDE.md) with development philosophy
7. ✅ Cleaned stale object files

**Critical Blocker:**
The build is blocked by nlohmann/json library namespace pollution. The library is injecting symbols into `std::ranges` namespace causing 100+ template compilation errors in MSVC's `<algorithm>` header. This is a fundamental compatibility issue that requires:
- Investigation of JSON library version (likely needs update or different configuration)
- Potential namespace isolation fixes
- May require JSON library replacement or version downgrade

**C++ Implementation Status:**
- ✅ AssetThumbnailCache - Fully implemented (930+ lines)
- ✅ asset_media_renderer tool - Fully implemented (850+ lines)
- ✅ Added to build system (CMakeLists.txt)
- ❌ Build blocked - cannot compile due to JSON library issues

**Philosophy Applied:**
Deleted Python workaround immediately upon user feedback. Focused on fixing C++ build issues properly rather than creating shortcuts. Documented all issues in CLAUDE.md for future reference.

## Current Build Fix Status (2025-12-07 continued)

**JSON Library Issue Resolution:**

Added compile definitions to disable nlohmann/json C++20 ranges support:
```cmake
# Disable nlohmann/json C++20 ranges support (causes MSVC conflicts)
target_compile_definitions(nova3d PUBLIC JSON_HAS_CPP_20=0 JSON_HAS_RANGES=0)
```

This was added to [CMakeLists.txt](CMakeLists.txt) after the "# Compiler warnings" section.

**Status**: BLOCKED - nlohmann/json C++20 ranges namespace pollution on MSVC.

### Build Blocker (2026-01-14)

**Issue**: nlohmann/json v3.11.x creates `nlohmann::std::ranges` namespace which pollutes MSVC's `std::ranges` causing 100+ compilation errors in `<algorithm>`.

**Attempted Fixes**:
- ❌ `JSON_HAS_CPP_20=0 JSON_HAS_RANGES=0` compile definitions (set too late)
- ❌ Setting defines before FetchContent (still pollutes)
- ❌ Setting defines at CMake project start (still pollutes)
- ❌ Downgrading to v3.10.5 (CMake 3.5 compatibility issues)
- ❌ Downgrading to v3.11.2 (same issue)
- ❌ Switching to C++17 (codebase needs C++20 features: source_location, format)

**Workarounds to Try**:
1. Use single-include json.hpp with manual defines before include
2. Fork and patch nlohmann/json to disable ranges support
3. Use a different JSON library (RapidJSON, simdjson)
4. Update MSVC to latest version that may have fix
5. Force-include a header with JSON defines before all compilation

**Other Fixes Applied (2026-01-14)**:
- ✅ Fixed AnimationLayer redefinition (renamed to TimelineAnimationLayer)
- ✅ Fixed Logger.hpp ios::openmode operator
- ✅ Switched ImGui to docking branch for DockSpace/SetNextWindowViewport

## Audio System (2026-01-18)

### Overview

The audio system uses OpenAL-soft for 3D positional audio, libsndfile for audio file loading, and libvorbis for OGG support.

### Enabling Audio

Audio is **disabled by default** to avoid dependency complexity. To enable:

```bash
cmake -DNOVA_ENABLE_AUDIO=ON ..
```

### Dependencies (via FetchContent)

When `NOVA_ENABLE_AUDIO=ON`, the following are automatically fetched:
- **OpenAL-soft 1.23.1** - Cross-platform 3D audio API
- **libsndfile 1.2.2** - Audio file loading (WAV, OGG, FLAC, etc.)
- **libvorbis 1.3.7** - OGG Vorbis decoding
- **libogg 1.3.5** - OGG container format (vorbis dependency)

### Files

- `engine/audio/AudioEngine.hpp` - Full audio API (AudioEngine, AudioSource, AudioBuffer, etc.)
- `engine/audio/AudioEngine.cpp` - OpenAL implementation (used when NOVA_ENABLE_AUDIO=ON)
- `engine/audio/AudioEngineStub.cpp` - Stub implementation (used when NOVA_ENABLE_AUDIO=OFF)
- `engine/audio/SoundBank.hpp` - Sound variation system with JSON configuration

### Stub Implementation

When audio is disabled, a stub implementation is compiled that:
- Provides the same API as the real implementation
- Logs warnings when audio functions are called
- Returns appropriate default values
- Allows the engine to compile and run without audio support

### Usage Example

```cpp
#include "audio/AudioEngine.hpp"

auto& audio = Nova::AudioEngine::Instance();
audio.Initialize();

// Load a sound
auto buffer = audio.LoadSound("explosion.ogg");

// Play 3D sound
auto source = audio.Play3D(buffer, position);

// Update listener (from camera)
audio.SetListenerTransform(cameraPos, cameraForward, cameraUp);

// Update each frame
audio.Update(deltaTime);
```
