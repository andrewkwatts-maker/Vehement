# Vehement SDF Engine - SOLID Refactor Plan

## Overview

This document outlines a section-by-section refactoring plan to ensure SOLID compliance throughout the 305,000+ line codebase. Each section can be completed independently without breaking the build.

---

## Section 1: Graphics System (Priority: HIGH)

### Current Issues:
1. **Multiple Renderer implementations without unified interface**
   - `Renderer`, `DeferredRenderer`, `GPUDrivenRenderer`, `SDFRenderer`
   - No `IRenderer` interface for dependency inversion

2. **Duplicate rendering code**
   - `TemplateRenderer`, `BuildingUIRenderer`, `MaterialGraphPreviewRenderer`
   - Similar Draw/Render patterns

3. **Missing plugin points**
   - Render passes hardcoded
   - No render pipeline plugins

### Refactor Tasks:

#### 1.1 Create IRenderer Interface
```cpp
// engine/graphics/IRenderer.hpp
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Submit(const RenderCommand& cmd) = 0;
    virtual void SetViewport(const Viewport& vp) = 0;
    virtual RenderCapabilities GetCapabilities() const = 0;
};
```

**Files to modify:**
- [ ] Create `engine/graphics/IRenderer.hpp`
- [ ] Update `engine/graphics/Renderer.hpp` - implement IRenderer
- [ ] Update `engine/graphics/DeferredRenderer.hpp` - implement IRenderer
- [ ] Update `engine/graphics/GPUDrivenRenderer.hpp` - implement IRenderer

#### 1.2 Create IRenderPass Plugin System
```cpp
// engine/graphics/IRenderPass.hpp
class IRenderPass {
public:
    virtual ~IRenderPass() = default;
    virtual void Setup(RenderContext& ctx) = 0;
    virtual void Execute(RenderContext& ctx, const RenderData& data) = 0;
    virtual void Cleanup(RenderContext& ctx) = 0;
    virtual RenderPassPriority GetPriority() const = 0;
    virtual std::string_view GetName() const = 0;
};
```

**Files to create:**
- [ ] `engine/graphics/IRenderPass.hpp`
- [ ] `engine/graphics/RenderPassRegistry.hpp`
- [ ] `engine/graphics/passes/ShadowPass.hpp`
- [ ] `engine/graphics/passes/GBufferPass.hpp`
- [ ] `engine/graphics/passes/LightingPass.hpp`
- [ ] `engine/graphics/passes/SDFPass.hpp`
- [ ] `engine/graphics/passes/PostProcessPass.hpp`

#### 1.3 Consolidate Preview Renderers
**Merge into single PreviewRenderer class:**
- `MaterialGraphPreviewRenderer` -> PreviewRenderer
- `BuildingUIRenderer` -> PreviewRenderer (with mode)
- `TemplateRenderer` -> PreviewRenderer (with mode)

**Files to modify:**
- [ ] Create `engine/graphics/PreviewRenderer.hpp`
- [ ] Deprecate `engine/materials/MaterialGraphEditor.hpp` (MaterialGraphPreviewRenderer)
- [ ] Update `engine/game/BuildingUI.hpp` to use PreviewRenderer

---

## Section 2: Editor System (Priority: HIGH)

### Current Issues:
1. **EditorApplication is a God Class** (1,147 lines header)
   - Handles menus, selection, tools, layout, undo

2. **Duplicate panel patterns**
   - Similar initialization/cleanup in each panel
   - Copy-paste toolbar code

3. **Missing abstraction for tools**
   - SDFSculptTool, TransformGizmo have similar input handling

### Refactor Tasks:

#### 2.1 Decompose EditorApplication
Split into:
- [ ] `EditorMenuSystem` - Menu bar and commands
- [ ] `EditorSelectionManager` - Selection handling
- [ ] `EditorToolManager` - Active tool management
- [ ] `EditorLayoutManager` - Panel layout persistence
- [ ] `EditorApplication` - Thin coordinator only

#### 2.2 Create IEditorTool Interface
```cpp
// engine/editor/IEditorTool.hpp
class IEditorTool {
public:
    virtual ~IEditorTool() = default;
    virtual void Activate() = 0;
    virtual void Deactivate() = 0;
    virtual void OnMouseDown(const MouseEvent& e) = 0;
    virtual void OnMouseMove(const MouseEvent& e) = 0;
    virtual void OnMouseUp(const MouseEvent& e) = 0;
    virtual void OnKeyDown(const KeyEvent& e) = 0;
    virtual void Render(RenderContext& ctx) = 0;
    virtual std::string_view GetName() const = 0;
    virtual std::string_view GetIcon() const = 0;
};
```

**Files to modify:**
- [ ] Create `engine/editor/IEditorTool.hpp`
- [ ] Update `engine/editor/TransformGizmo.hpp` - implement IEditorTool
- [ ] Update `engine/editor/SDFSculptTool.hpp` - implement IEditorTool
- [ ] Create `engine/editor/EditorToolRegistry.hpp`

#### 2.3 Create Panel Factory
```cpp
// engine/editor/EditorPanelFactory.hpp
class EditorPanelFactory {
public:
    template<typename T>
    void Register(const std::string& name);

    std::unique_ptr<EditorPanel> Create(const std::string& name);
    std::vector<std::string> GetRegisteredPanels() const;
};
```

---

## Section 3: Platform Abstraction (Priority: MEDIUM)

### Current Issues:
1. **Platform implementations lack unified interface**
   - LinuxVulkan, VulkanRenderer, AndroidGLES, WindowsGL
   - No IPlatformRenderer interface

2. **Duplicated platform detection code**
   - PlatformDetect.hpp has magic numbers
   - Similar code in each platform file

3. **No plugin system for platform backends**

### Refactor Tasks:

#### 3.1 Create IPlatformBackend Interface
```cpp
// engine/platform/IPlatformBackend.hpp
class IPlatformBackend {
public:
    virtual ~IPlatformBackend() = default;
    virtual bool Initialize(const PlatformConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual void* GetNativeHandle() const = 0;
    virtual PlatformCapabilities GetCapabilities() const = 0;
    virtual std::string_view GetName() const = 0;
};
```

**Files to modify:**
- [ ] Create `engine/platform/IPlatformBackend.hpp`
- [ ] Update `engine/platform/linux/LinuxVulkan.hpp`
- [ ] Update `engine/platform/android/VulkanRenderer.hpp`
- [ ] Update `engine/platform/android/AndroidGLES.hpp`
- [ ] Update `engine/platform/windows/WindowsGL.hpp`

#### 3.2 Platform Backend Registry
```cpp
// engine/platform/PlatformBackendRegistry.hpp
class PlatformBackendRegistry {
public:
    void Register(std::unique_ptr<IPlatformBackend> backend);
    IPlatformBackend* GetBest(const PlatformRequirements& req);
    std::vector<IPlatformBackend*> GetAll();
};
```

---

## Section 4: Core Services (Priority: MEDIUM)

### Current Issues:
1. **Manager classes are singletons**
   - LogManager, SettingsManager, AssetDatabaseManager
   - Hard to test, tight coupling

2. **No dependency injection container**
   - Manual wiring throughout

3. **Service access patterns inconsistent**

### Refactor Tasks:

#### 4.1 Create Service Locator
```cpp
// engine/core/ServiceLocator.hpp
class ServiceLocator {
public:
    template<typename T>
    static void Register(std::shared_ptr<T> service);

    template<typename T>
    static T& Get();

    template<typename T>
    static bool Has();

    static void Clear();
};
```

#### 4.2 Create Service Interfaces
- [ ] `ILogService` (from LogManager)
- [ ] `ISettingsService` (from SettingsManager)
- [ ] `IAssetService` (from AssetDatabaseManager)
- [ ] `IInputService` (from InputManager)
- [ ] `IJobService` (from JobSystem)

#### 4.3 Convert Singletons to Services
**Files to modify:**
- [ ] `engine/core/Logger.hpp` - Add ILogService interface
- [ ] `engine/core/SettingsManager.hpp` - Add ISettingsService interface
- [ ] `engine/assets/AssetDatabase.hpp` - Add IAssetService interface
- [ ] Update all call sites to use ServiceLocator

---

## Section 5: Physics System (Priority: LOW)

### Current Issues:
1. **SDFCollisionSystem is monolithic**
   - Handles detection, resolution, events

2. **Duplicate collision code**
   - SDFCollision.cpp and CollisionPrimitives.cpp overlap

### Refactor Tasks:

#### 5.1 Decompose SDFCollisionSystem
Split into:
- [ ] `CollisionDetector` - Broad/narrow phase
- [ ] `CollisionResolver` - Response calculation
- [ ] `CollisionEventDispatcher` - Event handling

#### 5.2 Consolidate Collision Code
**Merge:**
- [ ] `SDFCollision.hpp` collision shapes -> `CollisionPrimitives.hpp`
- [ ] Remove duplicates, keep SDF-specific in SDFCollision

---

## Section 6: Animation System (Priority: LOW)

### Current Issues:
1. **Multiple similar blend classes**
   - BlendSpace1D, BlendSpace2D, BlendNode, BlendMask
   - Could use template/strategy pattern

2. **AnimationConfigManager has 290 lines**
   - Could be split

### Refactor Tasks:

#### 6.1 Create IBlendStrategy Interface
```cpp
// engine/animation/IBlendStrategy.hpp
class IBlendStrategy {
public:
    virtual ~IBlendStrategy() = default;
    virtual AnimationPose Blend(
        const std::vector<AnimationPose>& poses,
        const std::vector<float>& weights) = 0;
};
```

#### 6.2 Consolidate Blend Classes
- [ ] `BlendSpace1D` -> `BlendSpace<1>`
- [ ] `BlendSpace2D` -> `BlendSpace<2>`
- [ ] Use template for dimensionality

---

## Section 7: Networking (Priority: LOW)

### Current Issues:
1. **ReplicationSystem does too much**
   - Serialization, transport, state sync

2. **FirebaseClient has cloud-specific code**
   - Should abstract cloud provider

### Refactor Tasks:

#### 7.1 Create ICloudProvider Interface
```cpp
// engine/networking/ICloudProvider.hpp
class ICloudProvider {
public:
    virtual ~ICloudProvider() = default;
    virtual void Connect(const CloudConfig& config) = 0;
    virtual void Disconnect() = 0;
    virtual void Upload(const std::string& key, const std::vector<uint8_t>& data) = 0;
    virtual std::vector<uint8_t> Download(const std::string& key) = 0;
};
```

- [ ] `FirebaseProvider : ICloudProvider`
- [ ] Future: `AWSProvider`, `AzureProvider`

---

## Execution Order

### Phase 1: Foundation (No breaking changes)
1. Create all interface headers (IRenderer, IEditorTool, IPlatformBackend, etc.)
2. Create ServiceLocator
3. Create Registry classes

### Phase 2: Graphics Refactor
1. Implement IRenderer in existing renderers
2. Create IRenderPass system
3. Migrate to render pass plugins

### Phase 3: Editor Refactor
1. Decompose EditorApplication
2. Implement IEditorTool in tools
3. Create panel factory

### Phase 4: Platform Refactor
1. Implement IPlatformBackend
2. Create platform registry
3. Clean up PlatformDetect

### Phase 5: Core Services
1. Create service interfaces
2. Convert singletons
3. Update call sites

### Phase 6: Cleanup
1. Physics consolidation
2. Animation templates
3. Networking abstraction

---

## Success Criteria

- [ ] All major classes have interfaces
- [ ] No singleton patterns in new code
- [ ] ServiceLocator used for all cross-cutting concerns
- [ ] Render passes are plugins
- [ ] Editor tools are plugins
- [ ] Platform backends are plugins
- [ ] 0 duplicate behaviors across codebase
- [ ] Each class has single responsibility
- [ ] Build still compiles after each phase

---

*Document Version: 1.0*
*Created: 2026-01-14*
