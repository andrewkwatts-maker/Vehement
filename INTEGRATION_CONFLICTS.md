# Integration Conflicts & Resolution Guide

**Document Version:** 1.0
**Date:** November 30, 2025
**Purpose:** Identify and resolve potential conflicts during integration

---

## Table of Contents

1. [Overview](#overview)
2. [Variable Name Conflicts](#variable-name-conflicts)
3. [Function Definition Conflicts](#function-definition-conflicts)
4. [Include Order Issues](#include-order-issues)
5. [Circular Dependencies](#circular-dependencies)
6. [Type Mismatches](#type-mismatches)
7. [Platform-Specific Conflicts](#platform-specific-conflicts)
8. [ImGui Context Conflicts](#imgui-context-conflicts)
9. [Static Variable Conflicts](#static-variable-conflicts)
10. [Namespace Conflicts](#namespace-conflicts)

---

## Overview

This document catalogues all potential conflicts that may arise during integration and provides specific solutions for each.

### Conflict Categories

- **High Priority**: Must be resolved or build will fail
- **Medium Priority**: May cause runtime issues or warnings
- **Low Priority**: Code style or optimization issues

### General Resolution Strategy

1. **Identify** conflict through compiler/linker error
2. **Locate** conflicting code sections
3. **Apply** solution from this guide
4. **Verify** fix with compilation
5. **Test** affected functionality

---

## Variable Name Conflicts

### Conflict 1: Static bool vs Member Variable for Debug Overlays

**Priority:** HIGH
**File:** StandaloneEditor.cpp
**Lines:** ~311-325 (Stats menu)

**Problem:**
```cpp
// OLD CODE (existing)
static bool showDebugOverlay = false;
static bool showProfiler = false;

// NEW CODE (from agents)
// Uses member variables instead:
m_showDebugOverlay
m_showProfiler
```

**Error Message:**
```
error: 'showDebugOverlay' undeclared (first use in this function)
```

**Solution:**

Remove ALL static bool declarations in Stats menu:
```cpp
// DELETE THESE LINES:
static bool showDebugOverlay = false;
static bool showProfiler = false;
static bool showMemoryStats = false;
static bool showRenderTime = false;
static bool showUpdateTime = false;
static bool showPhysicsTime = false;

// REPLACE WITH:
ImGui::Checkbox("Show Debug Overlay", &m_showDebugOverlay);
ImGui::Checkbox("Show Profiler", &m_showProfiler);
ImGui::Checkbox("Show Memory Stats", &m_showMemoryStats);
ImGui::Checkbox("Show Render Time", &m_showRenderTime);
ImGui::Checkbox("Show Update Time", &m_showUpdateTime);
ImGui::Checkbox("Show Physics Time", &m_showPhysicsTime);
```

**Verification:**
```bash
# Search for any remaining static bool declarations
grep -n "static bool show" examples/StandaloneEditor.cpp
# Should return no results
```

---

### Conflict 2: m_currentMapPath Variable

**Priority:** MEDIUM
**File:** StandaloneEditor.hpp, StandaloneEditor.cpp

**Problem:**
May already exist in codebase for map management.

**Check:**
```bash
grep -n "m_currentMapPath" examples/StandaloneEditor.hpp
```

**If exists:**
- No conflict, variable is reused
- Ensure type is `std::string`

**If doesn't exist:**
- Add to header as specified in Phase 2

**Solution:**
```cpp
// In StandaloneEditor.hpp
std::string m_currentMapPath;  // Add if missing
```

---

### Conflict 3: Recent Files Vector

**Priority:** LOW
**File:** StandaloneEditor.hpp

**Problem:**
Unlikely, but recent files list may conflict with existing code.

**Check:**
```bash
grep -n "m_recentFiles" examples/StandaloneEditor.hpp
```

**Solution:**
If exists, ensure it's `std::vector<std::string>`. If different type, rename new variable to `m_recentFilesList`.

---

## Function Definition Conflicts

### Conflict 4: LoadMap() / SaveMap() Signature Mismatch

**Priority:** HIGH
**Files:** StandaloneEditor.cpp

**Problem:**
New code calls `LoadMap(path)` and `SaveMap(path)` but existing functions may have different signatures.

**Check existing signatures:**
```bash
grep -n "void.*LoadMap" examples/StandaloneEditor.cpp
grep -n "void.*SaveMap" examples/StandaloneEditor.cpp
```

**Possible conflicts:**
```cpp
// If existing is:
void LoadMap();  // No parameters

// But new code expects:
void LoadMap(const std::string& path);
```

**Solution A: Overload**
```cpp
// Keep existing
void LoadMap();

// Add new overload
void LoadMap(const std::string& path);
```

**Solution B: Modify Calls**
```cpp
// If LoadMap() exists without parameters, modify new code to:
m_currentMapPath = path;
LoadMap();  // Use existing signature
```

---

### Conflict 5: Duplicate RenderDebugOverlay() Declaration

**Priority:** HIGH
**Files:** StandaloneEditor.hpp

**Problem:**
Debug overlay functions may already be declared if previous agents ran.

**Check:**
```bash
grep -n "RenderDebugOverlay\|RenderProfiler\|RenderMemoryStats" examples/StandaloneEditor.hpp
```

**Solution:**
If already declared:
- **Don't add again** - skip Phase 2.1
- Verify declarations match:
```cpp
void RenderDebugOverlay();
void RenderProfiler();
void RenderMemoryStats();
void RenderTimeDistribution();
```

If declarations differ, use the version from Phase 2.

---

### Conflict 6: File Dialog Function Names

**Priority:** MEDIUM
**Files:** StandaloneEditor.hpp

**Problem:**
Functions `OpenNativeFileDialog` / `SaveNativeFileDialog` may conflict with similar existing functions.

**Check:**
```bash
grep -n "FileDialog" examples/StandaloneEditor.hpp
```

**Solution:**
If conflicts exist, rename to:
```cpp
std::string OpenNativeFileDialogEx(const char* filter, const char* title);
std::string SaveNativeFileDialogEx(const char* filter, const char* title, const char* defaultExt);
```

Update all calls in StandaloneEditor.cpp accordingly.

---

## Include Order Issues

### Conflict 7: Windows.h Include Order

**Priority:** HIGH
**Files:** StandaloneEditor.cpp

**Problem:**
Windows.h must be included before certain other headers, or after, depending on defines.

**Error Message:**
```
error: macro redefinition 'NOMINMAX'
warning: 'APIENTRY' macro redefined
```

**Solution:**
Place Windows includes in specific order:

```cpp
// At top of file, before other includes
#ifdef _WIN32
#define NOMINMAX           // Prevent min/max macro conflicts
#define WIN32_LEAN_AND_MEAN  // Reduce Windows.h bloat
#include <Windows.h>
#include <commdlg.h>
#undef near  // Prevent conflicts with math functions
#undef far
#endif

// Then other includes
#include <imgui.h>
#include <glm/glm.hpp>
// etc.
```

---

### Conflict 8: std::min/max Conflicts with Windows Macros

**Priority:** HIGH
**Files:** StandaloneEditor.cpp

**Problem:**
Windows.h defines `min` and `max` macros that conflict with `std::min`/`std::max`.

**Error Message:**
```
error: expected unqualified-id before '(' token
std::min(...)
```

**Solution A: Define NOMINMAX (Preferred)**
```cpp
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
```

**Solution B: Use Parentheses**
```cpp
// Change:
std::min(a, b)
// To:
(std::min)(a, b)
```

**Solution C: Undef Macros**
```cpp
#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif
```

---

### Conflict 9: ImGui Before ModernUI

**Priority:** MEDIUM
**Files:** StandaloneEditor.cpp, ModernUI.cpp

**Problem:**
ModernUI.hpp depends on ImGui, so ImGui must be included first.

**Solution:**
Ensure include order:
```cpp
#include <imgui.h>          // First
#include "ModernUI.hpp"     // After ImGui
```

In ModernUI.hpp, ensure it includes ImGui:
```cpp
#pragma once
#include <imgui.h>  // Required
```

---

## Circular Dependencies

### Conflict 10: EditorCommand Circular Reference

**Priority:** LOW
**Files:** EditorCommand.hpp, StandaloneEditor.hpp

**Problem:**
EditorCommand may need StandaloneEditor types, but StandaloneEditor includes EditorCommand.

**Current State:**
EditorCommand is self-contained (uses glm types, std::vector), no circular dependency.

**Prevention:**
- Don't include StandaloneEditor.hpp in EditorCommand.hpp
- Use forward declarations if needed
- Keep EditorCommand generic

**Solution if occurs:**
```cpp
// In EditorCommand.hpp
class StandaloneEditor;  // Forward declaration only
```

---

## Type Mismatches

### Conflict 11: Nova::Key Enum vs int

**Priority:** HIGH
**Files:** StandaloneEditor.cpp (keyboard shortcuts)

**Problem:**
`Nova::Key::Q` may not be defined or may be different type.

**Error Message:**
```
error: 'Q' is not a member of 'Nova::Key'
```

**Solution A: Check Nova Engine Key Codes**
```bash
grep -rn "enum.*Key" engine/
```

Find correct enum name (might be `Nova::KeyCode` or `Nova::Input::Key`).

**Solution B: Use Raw Key Codes**
```cpp
// Instead of:
if (input.IsKeyPressed(Nova::Key::Q))

// Use:
if (input.IsKeyPressed('Q'))  // ASCII code
// Or
if (input.IsKeyPressed(81))   // Q key code
```

**Solution C: Define Missing Keys**
```cpp
// In StandaloneEditor.cpp
#ifndef NOVA_KEY_Q
#define NOVA_KEY_Q 'Q'
#endif
```

---

### Conflict 12: ImVec2 vs glm::vec2

**Priority:** MEDIUM
**Files:** Multiple

**Problem:**
Mixing ImGui and GLM vector types.

**Error Message:**
```
error: cannot convert 'glm::vec2' to 'ImVec2'
```

**Solution:**
Explicit conversion:
```cpp
// GLM to ImGui
glm::vec2 glmVec(x, y);
ImVec2 imVec(glmVec.x, glmVec.y);

// ImGui to GLM
ImVec2 imVec(x, y);
glm::vec2 glmVec(imVec.x, imVec.y);
```

---

## Platform-Specific Conflicts

### Conflict 13: Windows-Only Code on Linux/Mac

**Priority:** HIGH
**Files:** StandaloneEditor.cpp (file dialogs)

**Problem:**
Windows-specific code will fail on other platforms.

**Error Message:**
```
error: 'GetOpenFileNameA' was not declared in this scope
```

**Solution:**
Already handled with `#ifdef _WIN32`, but verify:

```cpp
#ifdef _WIN32
std::string StandaloneEditor::OpenNativeFileDialog(...) {
    // Windows implementation
}
#else
std::string StandaloneEditor::OpenNativeFileDialog(...) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}
#endif
```

**Test on target platforms:**
```bash
# Linux/Mac
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

---

### Conflict 14: Path Separators

**Priority:** MEDIUM
**Files:** StandaloneEditor.cpp (recent files)

**Problem:**
Windows uses backslashes, Unix uses forward slashes.

**Solution:**
Use `std::filesystem` for cross-platform paths:

```cpp
#include <filesystem>
namespace fs = std::filesystem;

// Instead of:
std::string path = "assets\\config\\editor.json";

// Use:
fs::path configPath = fs::path("assets") / "config" / "editor.json";
std::string path = configPath.string();
```

---

## ImGui Context Conflicts

### Conflict 15: Multiple ImGui Contexts

**Priority:** LOW
**Files:** All ImGui rendering code

**Problem:**
If multiple ImGui contexts exist, calls may operate on wrong context.

**Symptoms:**
- Widgets don't render
- Crashes in ImGui::Render()
- Assertion failures

**Solution:**
Ensure single context:
```cpp
// In main.cpp or initialization
ImGui::CreateContext();

// In shutdown
ImGui::DestroyContext();
```

Don't create multiple contexts unless explicitly needed.

---

### Conflict 16: ImGui State Pollution

**Priority:** MEDIUM
**Files:** ModernUI.cpp

**Problem:**
ModernUI functions push styles but don't pop them.

**Check ModernUI functions:**
```cpp
bool ModernUI::GlowButton(...) {
    ImGui::PushStyleColor(...);
    bool result = ImGui::Button(...);
    ImGui::PopStyleColor();  // Must pop!
    return result;
}
```

**Solution:**
Audit all ModernUI functions for balanced Push/Pop:
```bash
grep -n "PushStyle" examples/ModernUI.cpp > push.txt
grep -n "PopStyle" examples/ModernUI.cpp > pop.txt
# Counts should match
```

---

## Static Variable Conflicts

### Conflict 17: Static Animation Timers

**Priority:** LOW
**Files:** ModernUI.cpp

**Problem:**
Static variables in ModernUI::GetHoverAnimation() may cause issues with multiple instances.

**Current Code:**
```cpp
float ModernUI::GetHoverAnimation(const char* id, bool isHovered) {
    static std::unordered_map<std::string, float> animations;
    // ...
}
```

**Potential Issue:**
Multiple buttons with same ID will share animation state.

**Solution:**
Use ImGui ID stack:
```cpp
ImGui::PushID(uniqueID);
ModernUI::GlowButton("Button");
ImGui::PopID();
```

Or ensure unique labels:
```cpp
ModernUI::GlowButton("Button##UniqueID1");
ModernUI::GlowButton("Button##UniqueID2");
```

---

## Namespace Conflicts

### Conflict 18: Global `using namespace`

**Priority:** MEDIUM
**Files:** Multiple

**Problem:**
`using namespace std;` or `using namespace Nova;` in headers causes conflicts.

**Check:**
```bash
grep -n "using namespace" examples/*.hpp
```

**Solution:**
Remove from headers:
```cpp
// In .hpp files - DON'T DO THIS:
using namespace std;

// Instead, use explicit namespaces:
std::vector<std::string> m_recentFiles;
```

In .cpp files, it's acceptable but discouraged:
```cpp
// In .cpp files - OK but not ideal:
using namespace std;

// Better - use explicit or local using:
using std::string;
using std::vector;
```

---

### Conflict 19: GLM Namespace

**Priority:** LOW
**Files:** StandaloneEditor.cpp, EditorCommand.cpp

**Problem:**
GLM types like `vec3` may be unqualified.

**Solution:**
Always use `glm::` prefix:
```cpp
// Instead of:
vec3 position;

// Use:
glm::vec3 position;
```

Or local using:
```cpp
using glm::vec3;
using glm::mat4;

vec3 position;  // Now OK
```

---

## Resolution Checklist

After applying fixes, verify:

### Compilation
```bash
cd H:/Github/Old3DEngine/build
cmake --build . --clean-first
# Should complete with 0 errors
```

### Warnings
```bash
# Build with all warnings
cmake .. -DCMAKE_CXX_FLAGS="/W4"
cmake --build .
# Review and fix any warnings
```

### Runtime
```bash
# Run application
cd bin/Debug
./StandaloneEditor.exe
# Should launch without crashes
```

### Functionality
- [ ] All menus render correctly
- [ ] Keyboard shortcuts work
- [ ] File dialogs open
- [ ] Recent files persist
- [ ] Debug overlays display
- [ ] No console errors

---

## Conflict Prevention

### Best Practices

1. **Always use explicit namespaces** in headers
2. **Guard platform-specific code** with `#ifdef`
3. **Balance ImGui Push/Pop** calls
4. **Use unique ImGui IDs** for widgets
5. **Include guards** in all headers
6. **Forward declarations** to break circular deps
7. **Const correctness** to prevent type issues
8. **nullptr instead of NULL** for C++
9. **Modern C++ features** (auto, range-for)
10. **Consistent code style** across files

### Pre-Integration Checks

```bash
# Check for common issues
grep -rn "using namespace" examples/*.hpp  # Should be empty
grep -rn "NULL" examples/*.cpp  # Should use nullptr
grep -rn "^\s*#include.*\.hpp\"" examples/*.cpp  # Check include order
```

---

## Quick Reference: Common Error Messages

| Error Message | Conflict # | Solution |
|---------------|-----------|----------|
| 'showDebugOverlay' undeclared | 1 | Change static bool to member variable |
| macro redefinition 'NOMINMAX' | 7 | Add `#define NOMINMAX` before Windows.h |
| 'Q' is not a member of 'Nova::Key' | 11 | Check Nova engine key enum name |
| cannot convert 'glm::vec2' to 'ImVec2' | 12 | Explicit conversion |
| 'GetOpenFileNameA' not declared | 13 | Verify `#ifdef _WIN32` |
| ImGui assertion failure | 15,16 | Check context/state management |

---

## Advanced Conflicts

### Link-Time Conflicts

If you get linker errors about multiple definitions:

```bash
# Check for duplicate symbols
nm -C StandaloneEditor.o | grep "duplicate_symbol"
# Or on Windows
dumpbin /SYMBOLS StandaloneEditor.obj | findstr "duplicate"
```

**Common causes:**
- Function defined in header without `inline`
- Static variable in header without `inline`
- Missing `#pragma once` or include guards

**Solution:**
```cpp
// In .hpp file
#pragma once  // Or include guards

inline void MyFunction() {  // Use inline for definitions in headers
    // ...
}
```

---

## Document Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-11-30 | Final Integration Agent | Initial conflict documentation |

---

**END OF INTEGRATION CONFLICTS GUIDE**

For build issues, see TROUBLESHOOTING.md
For step-by-step integration, see MASTER_INTEGRATION_GUIDE.md
