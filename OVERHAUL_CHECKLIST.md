# Vehement SDF Engine - Overhaul Checklist

## Status: Active Rewrite in Progress

### Priority 1: Critical Files (20+ TODOs)
| File | TODOs | Status | Agent |
|------|-------|--------|-------|
| `engine/graphics/Renderer.cpp` | 20 | Pending | - |
| `engine/materials/ShaderGraphEditor.cpp` | 20 | Pending | - |

### Priority 2: High-Priority Files (8-19 TODOs)
| File | TODOs | Status | Agent |
|------|-------|--------|-------|
| `engine/platform/linux/LinuxVulkan.cpp` | 19 | Pending | - |
| `engine/platform/android/VulkanRenderer.cpp` | 10 | Pending | - |
| `engine/networking/ReplicationSystem.cpp` | 9 | Pending | - |
| `engine/graphics/RTXPathTracer.cpp` | 9 | Pending | - |
| `engine/graphics/RTXAccelerationStructure.cpp` | 8 | Pending | - |
| `engine/graphics/PathTracer.cpp` | 8 | Pending | - |
| `engine/procedural/ProcGenNodes.cpp` | 8 | Pending | - |
| `engine/platform/PlatformDetect.hpp` | 8 | Pending | - |

### Priority 3: Medium-Priority Files (5-7 TODOs)
| File | TODOs | Status | Agent |
|------|-------|--------|-------|
| `engine/core/Logger.hpp` | 7 | Pending | - |
| `engine/networking/FirebaseClient.cpp` | 6 | Pending | - |
| `engine/ui/widgets/UIWidget.cpp` | 6 | Pending | - |
| `engine/graphics/HybridDepthMerge.cpp` | 5 | Pending | - |
| `engine/graphics/RTXSupport.cpp` | 5 | Pending | - |

### Priority 4: Lower-Priority Files (1-4 TODOs)
- `engine/graphics/MeshToSDFConverter.cpp` (4)
- `engine/graphics/PolygonRasterizer.cpp` (4)
- `engine/scripting/EventNodes.cpp` (4)
- `engine/graphics/debug/DebugDraw.cpp` (3)
- `engine/platform/windows/WindowsGL.cpp` (3)
- `engine/platform/PlatformConfig.hpp` (3)
- `engine/platform/android/AndroidGLES.cpp` (3)
- And 27 more files...

---

## Legacy Patterns to Eliminate

### 1. Global Variables/Singletons
- [ ] Replace with dependency injection
- [ ] Create interfaces for services
- [ ] Use service locator pattern sparingly

### 2. God Classes
- [ ] Decompose large classes
- [ ] Single Responsibility per class
- [ ] Create class diagrams for proposed splits

### 3. Tight Coupling
- [ ] Introduce interfaces
- [ ] Use dependency injection
- [ ] Decouple concrete implementations

### 4. Duplicated Code
- [ ] Extract to helper functions
- [ ] Create reusable components
- [ ] DRY principle enforcement

### 5. Magic Numbers/Strings
- [ ] Replace with named constants
- [ ] Use configuration files
- [ ] Document all constants

### 6. Misspellings
- [ ] Fix "Shaddows" -> "Shadows"
- [ ] Enforce naming conventions
- [ ] Use consistent terminology

---

## Acceptance Criteria for Modern Code

1. **SOLID Compliance** - All 5 principles followed
2. **Testability** - Dependency injection, no static methods
3. **Readability** - Clear, concise, well-commented
4. **Performance** - Optimized critical paths
5. **Documentation** - API docs and design docs
6. **Unit Tests** - 80%+ coverage target
7. **No Warnings** - Clean compilation

---

## Sprint Integration

- Sprint 2 agents completing new features
- Overhaul agents will rewrite legacy files
- All rewrites follow new patterns from Sprint 1+2

---

*Document Version: 1.0*
*Last Updated: 2026-01-12*
