# Nova3D/Vehement Engine - Complete API Reference

This document provides comprehensive API documentation for the Nova3D engine, covering all major subsystems including SDF rendering, global illumination, terrain generation, and editor systems.

## Table of Contents

1. [Namespaces](#namespaces)
2. [Core Systems](#core-systems)
   - [Engine](#engine)
   - [Window](#window)
   - [Time](#time)
   - [Renderer](#renderer)
3. [SDF System](#sdf-system)
   - [SDFPrimitive](#sdfprimitive)
   - [SDFModel](#sdfmodel)
   - [SDFRenderer](#sdfrenderer)
   - [SDFAccelerationStructure](#sdfaccelerationstructure)
4. [Global Illumination](#global-illumination)
   - [RadianceCascade](#radiancecascade)
   - [SpectralRenderer](#spectralrenderer)
5. [Terrain System](#terrain-system)
   - [TerrainGenerator](#terraingenerator)
   - [NoiseGenerator](#noisegenerator)
   - [SDFTerrain](#sdfterrain)
6. [Scene System](#scene-system)
   - [Camera](#camera)
7. [Editor Systems](#editor-systems)
   - [SDFModelEditor](#sdfmodeleditor)
   - [MaterialEditorAdvanced](#materialeditoradvanced)
8. [Common Patterns](#common-patterns)
9. [Performance Guidelines](#performance-guidelines)

---

## Namespaces

### `Nova`

Primary namespace containing all engine systems.

```cpp
namespace Nova {
    // Core
    class Engine;           // Engine singleton
    class Window;           // Window management
    class Time;             // Timing utilities
    class Renderer;         // Graphics rendering
    class InputManager;     // Input handling

    // Graphics
    class SDFRenderer;      // SDF raymarching
    class RadianceCascade;  // Global illumination
    class Camera;           // View/projection

    // SDF
    class SDFModel;         // SDF model container
    class SDFPrimitive;     // SDF primitive
    enum class SDFPrimitiveType;
    enum class CSGOperation;

    // Terrain
    class TerrainGenerator; // Procedural terrain
    class TerrainChunk;     // Terrain chunk
    class NoiseGenerator;   // Noise functions
    class SDFTerrain;       // SDF terrain

    // Acceleration
    class SDFAccelerationStructure;
    struct AABB;
    struct Ray;
    struct Frustum;
}
```

### `Engine`

Spectral rendering and advanced optics.

```cpp
namespace Engine {
    class SpectralRenderer;     // Wavelength rendering
    class ChromaticDispersion;  // Dispersion effects
}
```

### `Vehement`

Game and editor specific systems.

```cpp
namespace Vehement {
    class Editor;           // Main editor
    class SDFModelEditor;   // SDF model editing
    enum class SDFGizmoMode;
    enum class SDFToolMode;
}
```

---

## Core Systems

### Engine

The main engine singleton that orchestrates all subsystems.

**Header:** `engine/core/Engine.hpp`

#### Class Definition

```cpp
class Engine {
public:
    // Singleton access
    static Engine& Instance() noexcept;

    // Initialization
    bool Initialize(const InitParams& params = {});
    int Run(ApplicationCallbacks callbacks);
    void RequestShutdown() noexcept;

    // State queries
    bool IsRunning() const noexcept;
    bool IsInitialized() const noexcept;

    // Subsystem access
    Window& GetWindow() noexcept;
    Time& GetTime() noexcept;
    Renderer& GetRenderer() noexcept;
    InputManager& GetInput() noexcept;
    Scene* GetActiveScene() noexcept;

    // Scene management
    void SetActiveScene(std::unique_ptr<Scene> scene);

    // Engine info
    static consteval std::string_view GetVersion() noexcept;  // "1.0.0"
    static consteval std::string_view GetName() noexcept;     // "Nova3D"
};
```

#### Structures

```cpp
struct Engine::InitParams {
    std::string configPath = "config/engine.json";
    bool enableImGui = true;
    bool enableDebugDraw = true;
};

struct Engine::ApplicationCallbacks {
    std::function<bool()> onStartup;
    std::function<void(float deltaTime)> onUpdate;
    std::function<void()> onRender;
    std::function<void()> onImGui;
    std::function<void()> onShutdown;
};
```

#### Usage Example

```cpp
#include <engine/core/Engine.hpp>

int main() {
    auto& engine = Nova::Engine::Instance();

    Nova::Engine::InitParams params;
    params.configPath = "config/engine.json";
    params.enableImGui = true;

    if (!engine.Initialize(params)) {
        return -1;
    }

    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onStartup = []() {
        // Load resources
        return true;
    };
    callbacks.onUpdate = [](float dt) {
        // Game logic
    };
    callbacks.onRender = []() {
        // Render scene
    };
    callbacks.onShutdown = []() {
        // Cleanup
    };

    return engine.Run(std::move(callbacks));
}
```

---

### Window

Platform-independent window management using GLFW.

**Header:** `engine/core/Window.hpp`

#### Class Definition

```cpp
class Window {
public:
    // Creation
    bool Create();
    bool Create(const CreateParams& params);
    bool Create(int width, int height, std::string_view title,
                bool fullscreen = false, int samples = 4);
    void Destroy() noexcept;

    // State
    bool ShouldClose() const noexcept;
    bool IsValid() const noexcept;
    void Close() noexcept;
    void SwapBuffers() noexcept;

    // Properties
    void SetTitle(std::string_view title);
    void SetVSync(bool enabled) noexcept;
    void SetFullscreen(bool fullscreen);
    void SetCursorMode(CursorMode mode) noexcept;

    // Getters
    GLFWwindow* GetHandle() const noexcept;
    glm::ivec2 GetSize() const noexcept;
    int GetWidth() const noexcept;
    int GetHeight() const noexcept;
    glm::ivec2 GetFramebufferSize() const noexcept;
    float GetAspectRatio() const noexcept;
    float GetDPIScale() const noexcept;
    bool IsFullscreen() const noexcept;
    bool HasFocus() const noexcept;
    bool IsVSyncEnabled() const noexcept;

    // Callbacks
    void SetCallbacks(Callbacks callbacks) noexcept;
};
```

#### Enumerations

```cpp
enum class CursorMode : int {
    Normal,    // Standard cursor
    Hidden,    // Cursor hidden but not captured
    Disabled   // Cursor hidden and captured (FPS camera)
};
```

#### Structures

```cpp
struct Window::CreateParams {
    int width = 1920;
    int height = 1080;
    std::string title = "Nova3D Engine";
    bool fullscreen = false;
    int samples = 4;         // MSAA samples
    bool vsync = true;
    bool visible = true;
};

struct Window::Callbacks {
    std::function<void(int, int)> onResize;
    std::function<void(bool)> onFocus;
    std::function<void()> onClose;
};
```

---

### Time

High-precision timing system with time scaling support.

**Header:** `engine/core/Time.hpp`

#### Class Definition

```cpp
class Time {
public:
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<float>;
    using TimePoint = Clock::time_point;

    Time() noexcept;

    // Update (call once per frame)
    void Update() noexcept;

    // Delta time
    float GetDeltaTime() const noexcept;
    float GetUnscaledDeltaTime() const noexcept;

    // Total time
    float GetTotalTime() const noexcept;

    // FPS
    float GetFPS() const noexcept;
    float GetAverageFPS() const noexcept;

    // Frame counting
    uint64_t GetFrameCount() const noexcept;

    // Time scale (slow-motion/fast-forward)
    void SetTimeScale(float scale) noexcept;
    float GetTimeScale() const noexcept;

    // Max delta (prevents spiral of death)
    void SetMaxDeltaTime(float maxDelta) noexcept;
    float GetMaxDeltaTime() const noexcept;

    // Fixed timestep (physics)
    float GetFixedDeltaTime() const noexcept;
    void SetFixedDeltaTime(float dt) noexcept;
    float GetFixedAccumulator() const noexcept;
    bool ShouldFixedUpdate() noexcept;
    float GetFixedAlpha() const noexcept;
};
```

#### Utility Classes

```cpp
// RAII scoped timer for profiling
class ScopedTimer {
public:
    explicit ScopedTimer(std::string_view name) noexcept;
    ~ScopedTimer();  // Logs elapsed time
    float GetElapsedMs() const noexcept;
};

// Manual stopwatch
class Stopwatch {
public:
    void Start() noexcept;
    void Stop() noexcept;
    void Reset() noexcept;
    void Restart() noexcept;
    float GetElapsedSeconds() const noexcept;
    float GetElapsedMilliseconds() const noexcept;
    bool IsRunning() const noexcept;
};
```

#### Usage Example

```cpp
auto& time = engine.GetTime();

// Basic timing
float dt = time.GetDeltaTime();
float total = time.GetTotalTime();

// Slow motion
time.SetTimeScale(0.5f);

// Fixed timestep for physics
time.SetFixedDeltaTime(1.0f / 60.0f);
while (time.ShouldFixedUpdate()) {
    PhysicsStep(time.GetFixedDeltaTime());
}

// Profiling
{
    ScopedTimer timer("RenderScene");
    RenderScene();
}  // Logs: "RenderScene: 5.2ms"
```

---

### Renderer

Main rendering class handling OpenGL operations.

**Header:** `engine/graphics/Renderer.hpp`

#### Class Definition

```cpp
class Renderer {
public:
    bool Initialize();
    void Shutdown();

    // Frame management
    void BeginFrame();
    void EndFrame();

    // Camera
    void SetCamera(Camera* camera);
    Camera* GetCamera() const;

    // Render state
    void Clear(const glm::vec4& color = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));
    void SetViewport(int x, int y, int width, int height);
    void SetDepthTest(bool enabled);
    void SetDepthWrite(bool enabled);
    void SetCulling(bool enabled, bool cullBack = true);
    void SetBlending(bool enabled);
    void SetWireframe(bool enabled);

    // Drawing
    void DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform);
    void DrawMesh(const Mesh& mesh, Shader& shader, const glm::mat4& transform);
    void DrawFullscreenQuad(Shader& shader);
    void RenderDebug();

    // Subsystems
    DebugDraw& GetDebugDraw();
    ShaderManager& GetShaderManager();
    TextureManager& GetTextureManager();

    // Statistics
    const Stats& GetStats() const;
    ExtendedStats GetExtendedStats() const;

    // Optimization
    bool InitializeOptimizations(const std::string& configPath = "");
    OptimizedRenderer* GetOptimizedRenderer();
    bool IsOptimizationEnabled() const;
    void SetOptimizationsEnabled(bool enabled);
    void SubmitOptimized(const std::shared_ptr<Mesh>& mesh,
                         const std::shared_ptr<Material>& material,
                         const glm::mat4& transform,
                         uint32_t objectID = 0);
    void FlushOptimized();
    void ApplyQualityPreset(const std::string& preset);

    // Debug
    static bool CheckGLError(const char* location = nullptr);
    static void EnableDebugOutput(bool enabled);
};
```

#### Structures

```cpp
struct Renderer::Stats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t vertices = 0;
    void Reset();
};

struct Renderer::ExtendedStats {
    Stats baseStats;
    uint32_t batchedDrawCalls = 0;
    uint32_t instancedDrawCalls = 0;
    uint32_t drawCallsSaved = 0;
    uint32_t objectsCulled = 0;
    float cullingEfficiency = 0.0f;
    float batchingEfficiency = 0.0f;
    float lodSavings = 0.0f;
    uint32_t stateChanges = 0;
};
```

---

## SDF System

### SDFPrimitive

Single SDF primitive component with transform, parameters, and material.

**Header:** `engine/sdf/SDFPrimitive.hpp`

#### Enumerations

```cpp
enum class SDFPrimitiveType : uint8_t {
    Sphere,
    Box,
    Cylinder,
    Capsule,
    Cone,
    Torus,
    Plane,
    RoundedBox,
    Ellipsoid,
    Pyramid,
    Prism,
    Custom
};

enum class CSGOperation : uint8_t {
    Union,
    Subtraction,
    Intersection,
    SmoothUnion,
    SmoothSubtraction,
    SmoothIntersection
};
```

#### Structures

```cpp
struct SDFTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 ToMatrix() const;
    glm::mat4 ToInverseMatrix() const;
    glm::vec3 TransformPoint(const glm::vec3& point) const;
    glm::vec3 InverseTransformPoint(const glm::vec3& point) const;

    static SDFTransform Identity();
    static SDFTransform Lerp(const SDFTransform& a, const SDFTransform& b, float t);
};

struct SDFParameters {
    float radius = 0.5f;                    // Sphere
    glm::vec3 dimensions{1.0f};             // Box
    float cornerRadius = 0.0f;              // RoundedBox
    float height = 1.0f;                    // Cylinder/Capsule/Cone
    float topRadius = 0.5f;                 // Cone
    float bottomRadius = 0.5f;              // Cylinder/Capsule/Cone
    float majorRadius = 0.5f;               // Torus
    float minorRadius = 0.1f;               // Torus
    glm::vec3 radii{0.5f, 0.3f, 0.4f};      // Ellipsoid
    int sides = 6;                          // Prism
    float smoothness = 0.1f;                // CSG blend factor
    std::string customFunctionId;           // Custom SDF
};

struct SDFMaterial {
    glm::vec4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissive = 0.0f;
    glm::vec3 emissiveColor{0.0f};
    uint32_t textureAtlasIndex = 0;
    glm::vec2 uvOffset{0.0f};
    glm::vec2 uvScale{1.0f};
    std::vector<glm::vec4> vertexColors;
    std::string texturePath;
    std::string normalMapPath;
};
```

#### Class Definition

```cpp
class SDFPrimitive {
public:
    SDFPrimitive();
    explicit SDFPrimitive(SDFPrimitiveType type);
    SDFPrimitive(const std::string& name, SDFPrimitiveType type);

    // Properties
    const std::string& GetName() const;
    void SetName(const std::string& name);
    uint32_t GetId() const;
    SDFPrimitiveType GetType() const;
    void SetType(SDFPrimitiveType type);

    // Transform
    const SDFTransform& GetLocalTransform() const;
    void SetLocalTransform(const SDFTransform& transform);
    SDFTransform GetWorldTransform() const;

    // Parameters
    const SDFParameters& GetParameters() const;
    SDFParameters& GetParameters();
    void SetParameters(const SDFParameters& params);

    // Material
    const SDFMaterial& GetMaterial() const;
    SDFMaterial& GetMaterial();
    void SetMaterial(const SDFMaterial& material);

    // Visibility
    bool IsVisible() const;
    void SetVisible(bool visible);
    bool IsLocked() const;
    void SetLocked(bool locked);

    // SDF evaluation
    float EvaluateSDF(const glm::vec3& point) const;
    glm::vec3 CalculateNormal(const glm::vec3& point, float epsilon = 0.001f) const;
    std::pair<glm::vec3, glm::vec3> GetLocalBounds() const;

    // Hierarchy
    SDFPrimitive* GetParent() const;
    const std::vector<std::unique_ptr<SDFPrimitive>>& GetChildren() const;
    SDFPrimitive* AddChild(std::unique_ptr<SDFPrimitive> child);
    bool RemoveChild(SDFPrimitive* child);
    bool RemoveChild(size_t index);
    SDFPrimitive* FindChild(const std::string& name);
    SDFPrimitive* FindChildById(uint32_t id);

    // CSG
    CSGOperation GetCSGOperation() const;
    void SetCSGOperation(CSGOperation op);

    // Utility
    std::unique_ptr<SDFPrimitive> Clone() const;
    void ForEach(const std::function<void(SDFPrimitive&)>& callback);
    void ForEach(const std::function<void(const SDFPrimitive&)>& callback) const;
};
```

#### SDF Evaluation Functions

```cpp
namespace Nova::SDFEval {
    // Primitives
    float Sphere(const glm::vec3& p, float radius);
    float Box(const glm::vec3& p, const glm::vec3& dimensions);
    float RoundedBox(const glm::vec3& p, const glm::vec3& dim, float radius);
    float Cylinder(const glm::vec3& p, float height, float radius);
    float Capsule(const glm::vec3& p, float height, float radius);
    float Cone(const glm::vec3& p, float height, float radius);
    float Torus(const glm::vec3& p, float majorRadius, float minorRadius);
    float Plane(const glm::vec3& p, const glm::vec3& normal, float offset);
    float Ellipsoid(const glm::vec3& p, const glm::vec3& radii);
    float Pyramid(const glm::vec3& p, float height, float baseSize);
    float Prism(const glm::vec3& p, int sides, float radius, float height);

    // CSG Operations
    float Union(float d1, float d2);
    float Subtraction(float d1, float d2);
    float Intersection(float d1, float d2);
    float SmoothUnion(float d1, float d2, float k);
    float SmoothSubtraction(float d1, float d2, float k);
    float SmoothIntersection(float d1, float d2, float k);

    // Advanced blending
    float ExponentialSmoothUnion(float d1, float d2, float k);
    float PowerSmoothUnion(float d1, float d2, float k);
    float CubicSmoothUnion(float d1, float d2, float k);
    float DistanceAwareSmoothUnion(float d1, float d2, float k, float minDist);
}
```

---

### SDFModel

Complete SDF-based model with hierarchy and animation support.

**Header:** `engine/sdf/SDFModel.hpp`

#### Class Definition

```cpp
class SDFModel {
public:
    SDFModel();
    explicit SDFModel(const std::string& name);

    // Properties
    const std::string& GetName() const;
    void SetName(const std::string& name);
    uint32_t GetId() const;

    // Hierarchy
    SDFPrimitive* GetRoot();
    const SDFPrimitive* GetRoot() const;
    void SetRoot(std::unique_ptr<SDFPrimitive> root);
    SDFPrimitive* CreatePrimitive(const std::string& name, SDFPrimitiveType type,
                                   SDFPrimitive* parent = nullptr);
    SDFPrimitive* FindPrimitive(const std::string& name);
    SDFPrimitive* FindPrimitiveById(uint32_t id);
    bool DeletePrimitive(SDFPrimitive* primitive);
    std::vector<SDFPrimitive*> GetAllPrimitives();
    size_t GetPrimitiveCount() const;

    // SDF Evaluation
    float EvaluateSDF(const glm::vec3& point) const;
    std::pair<glm::vec3, glm::vec3> GetBounds() const;
    glm::vec3 CalculateNormal(const glm::vec3& point, float epsilon = 0.001f) const;

    // Mesh Generation
    std::shared_ptr<Mesh> GenerateMesh(const SDFMeshSettings& settings = {}) const;
    std::vector<std::shared_ptr<Mesh>> GenerateMeshLODs(const SDFMeshSettings& settings = {}) const;
    std::shared_ptr<Mesh> GetMesh();
    void InvalidateMesh();
    const SDFMeshSettings& GetMeshSettings() const;
    void SetMeshSettings(const SDFMeshSettings& settings);

    // Texture Painting
    PaintLayer* AddPaintLayer(const std::string& name, int width = 1024, int height = 1024);
    bool RemovePaintLayer(const std::string& name);
    PaintLayer* GetPaintLayer(const std::string& name);
    const std::vector<PaintLayer>& GetPaintLayers() const;
    void PaintAt(const glm::vec3& worldPos, const glm::vec4& color,
                 float radius, float hardness, const std::string& layer = "");
    std::shared_ptr<Texture> BakePaintTexture() const;

    // Material
    std::shared_ptr<Material> GetMaterial() const;
    void SetMaterial(std::shared_ptr<Material> material);

    // Animation
    std::vector<std::string> GetPrimitiveNames() const;
    void ApplyPose(const std::unordered_map<std::string, SDFTransform>& pose);
    std::unordered_map<std::string, SDFTransform> GetCurrentPose() const;
    void ResetToBindPose();
    void SetBindPose();

    // Serialization
    std::string ToJson() const;
    bool FromJson(const std::string& json);
    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);

    // Callbacks
    std::function<void()> OnModified;
    std::function<void(SDFPrimitive*)> OnPrimitiveAdded;
    std::function<void(uint32_t)> OnPrimitiveRemoved;
};
```

#### Related Structures

```cpp
struct SDFMeshSettings {
    int resolution = 64;
    float boundsPadding = 0.1f;
    float isoLevel = 0.0f;
    bool smoothNormals = true;
    bool generateUVs = true;
    bool generateTangents = true;
    bool simplifyMesh = false;
    float simplifyRatio = 0.5f;
    float simplifyError = 0.01f;
    bool generateLODs = false;
    int lodLevels = 4;
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
};

struct PaintLayer {
    std::string name;
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    float opacity = 1.0f;
    bool visible = true;
};
```

---

### SDFRenderer

GPU raymarching renderer for SDF models.

**Header:** `engine/graphics/SDFRenderer.hpp`

#### Class Definition

```cpp
class SDFRenderer {
public:
    SDFRenderer();
    ~SDFRenderer();

    // Initialization
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    // Rendering
    void Render(const SDFModel& model, const Camera& camera,
                const glm::mat4& modelTransform = glm::mat4(1.0f));
    void RenderToTexture(const SDFModel& model, const Camera& camera,
                         std::shared_ptr<Framebuffer> framebuffer,
                         const glm::mat4& modelTransform = glm::mat4(1.0f));
    void RenderBatch(const std::vector<const SDFModel*>& models,
                     const std::vector<glm::mat4>& transforms,
                     const Camera& camera);

    // Settings
    SDFRenderSettings& GetSettings();
    const SDFRenderSettings& GetSettings() const;
    void SetSettings(const SDFRenderSettings& settings);
    Shader* GetShader();

    // Environment
    void SetEnvironmentMap(std::shared_ptr<Texture> envMap);
    std::shared_ptr<Texture> GetEnvironmentMap() const;

    // Global Illumination
    void SetRadianceCascade(std::shared_ptr<RadianceCascade> cascade);
    void SetGlobalIlluminationEnabled(bool enabled);
    bool IsGlobalIlluminationEnabled() const;

    // Spectral Rendering
    void SetSpectralMode(int mode);  // 0=RGB, 1=Spectral, 2=HeroWavelength
    void SetDispersionEnabled(bool enabled);
    void SetDiffractionEnabled(bool enabled);
    void SetBlackbodyEnabled(bool enabled);
};
```

#### Structures

```cpp
struct SDFRenderSettings {
    // Raymarching
    int maxSteps = 128;
    float maxDistance = 100.0f;
    float hitThreshold = 0.001f;

    // Quality
    bool enableShadows = true;
    bool enableAO = true;
    bool enableReflections = false;

    // Shadows
    float shadowSoftness = 8.0f;
    int shadowSteps = 32;

    // Ambient Occlusion
    int aoSteps = 5;
    float aoDistance = 0.5f;
    float aoIntensity = 0.5f;

    // Lighting
    glm::vec3 lightDirection{0.5f, -1.0f, 0.5f};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;

    // Background
    glm::vec3 backgroundColor{0.1f, 0.1f, 0.15f};
    bool useEnvironmentMap = false;
};

struct SDFPrimitiveData {  // GPU layout
    glm::mat4 transform;
    glm::mat4 inverseTransform;
    glm::vec4 parameters;
    glm::vec4 parameters2;
    glm::vec4 parameters3;
    glm::vec4 material;
    glm::vec4 baseColor;
    glm::vec4 emissiveColor;
    int type;
    int csgOperation;
    int visible;
    int padding;
};
```

---

### SDFAccelerationStructure

BVH-based acceleration for SDF scene traversal.

**Header:** `engine/graphics/SDFAcceleration.hpp`

#### Class Definition

```cpp
class SDFAccelerationStructure {
public:
    // Building
    void Build(const std::vector<SDFInstance>& instances,
               const BVHBuildSettings& settings = {});
    void Build(const std::vector<const SDFModel*>& models,
               const std::vector<glm::mat4>& transforms,
               const BVHBuildSettings& settings = {});
    void UpdateDynamic(const std::vector<int>& instanceIds,
                       const std::vector<glm::mat4>& newTransforms);
    void Refit();
    void Clear();

    // Queries
    std::vector<int> QueryFrustum(const Frustum& frustum) const;
    std::vector<int> QueryRay(const Ray& ray, float maxDistance = FLT_MAX) const;
    std::vector<int> QueryAABB(const AABB& aabb) const;
    std::vector<int> QuerySphere(const glm::vec3& center, float radius) const;
    int FindClosestInstance(const Ray& ray, float& outDistance) const;

    // GPU
    unsigned int UploadToGPU();
    unsigned int GetGPUBuffer() const;
    bool IsGPUValid() const;
    void InvalidateGPU();

    // Access
    const std::vector<SDFBVHNode>& GetNodes() const;
    const std::vector<SDFInstance>& GetInstances() const;
    const BVHBuildStats& GetStats() const;
    const BVHBuildSettings& GetSettings() const;
    bool IsBuilt() const;
    size_t GetMemoryUsage() const;
};
```

#### Structures

```cpp
struct AABB {
    glm::vec3 min{FLT_MAX};
    glm::vec3 max{-FLT_MAX};

    glm::vec3 Center() const;
    glm::vec3 Extents() const;
    glm::vec3 Size() const;
    float SurfaceArea() const;
    float Volume() const;
    bool Contains(const glm::vec3& point) const;
    bool Intersects(const AABB& other) const;
    bool IntersectsRay(const glm::vec3& origin, const glm::vec3& dir,
                       float& tMin, float& tMax) const;
    void Expand(const glm::vec3& point);
    void Expand(const AABB& other);

    static AABB Union(const AABB& a, const AABB& b);
    static AABB Intersection(const AABB& a, const AABB& b);
    static AABB Transform(const AABB& aabb, const glm::mat4& transform);
};

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    glm::vec3 invDirection;
    std::array<int, 3> sign;

    Ray(const glm::vec3& origin, const glm::vec3& direction);
    glm::vec3 At(float t) const;
};

struct Frustum {
    std::array<glm::vec4, 6> planes;

    Frustum();
    explicit Frustum(const glm::mat4& projectionView);
    bool ContainsPoint(const glm::vec3& point) const;
    bool IntersectsSphere(const glm::vec3& center, float radius) const;
    bool IntersectsAABB(const AABB& aabb) const;
};

enum class BVHBuildStrategy {
    SAH,          // Surface Area Heuristic (best quality)
    Middle,       // Midpoint split (fast)
    EqualCounts,  // Equal primitives (balanced)
    HLBVH         // Hierarchical Linear BVH
};

struct BVHBuildSettings {
    BVHBuildStrategy strategy = BVHBuildStrategy::SAH;
    int maxPrimitivesPerLeaf = 4;
    int maxDepth = 32;
    int sahBuckets = 12;
    bool parallelBuild = true;
    bool compactNodes = true;
};

struct BVHBuildStats {
    int nodeCount = 0;
    int leafCount = 0;
    int maxDepth = 0;
    float avgPrimitivesPerLeaf = 0.0f;
    double buildTimeMs = 0.0;
    size_t memoryBytes = 0;
};

struct SDFInstance {
    const SDFModel* model = nullptr;
    glm::mat4 transform{1.0f};
    glm::mat4 inverseTransform{1.0f};
    AABB worldBounds;
    int instanceId = -1;
    bool dynamic = false;
};
```

---

## Global Illumination

### RadianceCascade

Real-time global illumination using radiance cascades.

**Header:** `engine/graphics/RadianceCascade.hpp`

#### Class Definition

```cpp
class RadianceCascade {
public:
    RadianceCascade();
    ~RadianceCascade();

    // Initialization
    bool Initialize(const Config& config = {});
    void Shutdown();

    // Update
    void Update(const glm::vec3& cameraPosition, float deltaTime);

    // Light injection
    void InjectDirectLighting(const std::vector<glm::vec3>& positions,
                              const std::vector<glm::vec3>& radiance);
    void InjectEmissive(const glm::vec3& position, const glm::vec3& radiance, float radius);
    void PropagateLighting();

    // Sampling
    glm::vec3 SampleRadiance(const glm::vec3& worldPos, const glm::vec3& normal) const;

    // GPU resources
    uint32_t GetCascadeTexture(int level) const;
    const std::vector<uint32_t>& GetCascadeTextures() const;
    const glm::vec3& GetOrigin() const;
    float GetBaseSpacing() const;

    // Configuration
    const Config& GetConfig() const;
    void SetConfig(const Config& config);
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void Clear();

    // Debug
    void DebugDraw(Renderer* renderer);
    const Stats& GetStats() const;
};
```

#### Structures

```cpp
struct RadianceCascade::Config {
    // Cascade configuration
    int numCascades = 4;
    int baseResolution = 32;
    float cascadeScale = 2.0f;

    // Spatial
    glm::vec3 origin{0.0f};
    float baseSpacing = 1.0f;
    float updateRadius = 100.0f;

    // Quality
    int raysPerProbe = 64;
    int bounces = 2;
    bool useInterpolation = true;

    // Performance
    bool asyncUpdate = true;
    int maxProbesPerFrame = 1024;
    float temporalBlend = 0.95f;
};

struct RadianceCascade::Stats {
    int totalProbes = 0;
    int activeProbes = 0;
    int probesUpdatedThisFrame = 0;
    float updateTimeMs = 0.0f;
    float propagationTimeMs = 0.0f;
};
```

---

### SpectralRenderer

Wavelength-dependent rendering for chromatic effects.

**Header:** `engine/graphics/SpectralRenderer.hpp`

#### Class Definition

```cpp
class SpectralRenderer {
public:
    enum class Mode {
        RGB,            // Standard RGB
        Spectral,       // Full spectral
        HeroWavelength  // Hero wavelength sampling
    };

    Mode mode = Mode::HeroWavelength;
    int spectralSamples = 16;
    float wavelengthMin = 380.0f;
    float wavelengthMax = 780.0f;

    float SampleWavelength(float u) const;
    float GetWavelengthPDF(float wavelength) const;

    // Color conversion
    static glm::vec3 WavelengthToRGB(float wavelength);
    static glm::vec3 SpectralToXYZ(const std::vector<float>& spectrum,
                                    const std::vector<float>& wavelengths);
    static glm::vec3 XYZ_to_RGB(const glm::vec3& xyz);

    // Dispersion
    static glm::vec3 RefractSpectral(const glm::vec3& incident,
                                      const glm::vec3& normal,
                                      float ior, float wavelength);
    static float GetDispersedIOR(float baseIOR, float abbeNumber, float wavelength);
    static float FresnelSpectral(float cosTheta, float ior);

    // Color matching
    struct CIE_CMF { float x, y, z; };
    static CIE_CMF GetCIE_CMF(float wavelength);
    static std::vector<float> RGB_to_Spectrum(const glm::vec3& rgb);
};

class ChromaticDispersion {
public:
    static void CalculateRGB(const glm::vec3& incident, const glm::vec3& normal,
                             float baseIOR, float abbeNumber,
                             glm::vec3& outRed, glm::vec3& outGreen, glm::vec3& outBlue);
    static glm::vec3 GetChromaticAberration(const glm::vec2& position,
                                             float baseIOR, float abbeNumber);
    static glm::vec3 Rainbow(const glm::vec3& incident, const glm::vec3& normal,
                             float baseIOR, float wavelength);
};
```

---

## Terrain System

### TerrainGenerator

Procedural terrain with async chunk loading and LOD.

**Header:** `engine/terrain/TerrainGenerator.hpp`

#### Class Definition

```cpp
class TerrainGenerator {
public:
    bool Initialize();
    void Shutdown();

    // Update
    void Update(const glm::vec3& viewerPosition);
    void ProcessPendingMeshes(int maxChunksPerFrame = 2);
    void Render(Shader& shader);

    // Queries
    float GetHeightAt(float x, float z) const;
    glm::vec3 GetNormalAt(float x, float z) const;
    Stats GetStats() const;

    // Configuration
    void SetViewDistance(int chunks);
    void SetChunkSize(int size);
    void SetHeightScale(float scale);
    void SetNoiseParams(float frequency, int octaves,
                        float persistence, float lacunarity);
    void SetLODLevels(const std::vector<TerrainLOD>& levels);

    // Management
    void UnloadDistantChunks(float maxDistance);
    void ClearAllChunks();
};
```

#### Structures

```cpp
struct TerrainLOD {
    int resolution;
    float maxDistance;
};

enum class ChunkState {
    Unloaded,
    Generating,
    Generated,
    MeshPending,
    Ready
};

struct TerrainGenerator::Stats {
    size_t totalChunks;
    size_t visibleChunks;
    size_t pendingChunks;
    size_t generatingChunks;
};
```

---

### NoiseGenerator

Thread-safe noise generation utilities.

**Header:** `engine/terrain/NoiseGenerator.hpp`

#### Class Definition

```cpp
class NoiseGenerator {
public:
    // Basic noise
    static float Perlin(float x, float y) noexcept;
    static float Perlin(float x, float y, float z) noexcept;
    static float Simplex(float x, float y) noexcept;
    static float Simplex(float x, float y, float z) noexcept;

    // Fractal noise
    static float FractalNoise(float x, float y,
                              int octaves = 4,
                              float persistence = 0.5f,
                              float lacunarity = 2.0f) noexcept;
    static float FractalNoise(float x, float y, float z,
                              int octaves = 4,
                              float persistence = 0.5f,
                              float lacunarity = 2.0f) noexcept;
    static float RidgedNoise(float x, float y,
                             int octaves = 4,
                             float persistence = 0.5f,
                             float lacunarity = 2.0f) noexcept;
    static float BillowNoise(float x, float y,
                             int octaves = 4,
                             float persistence = 0.5f,
                             float lacunarity = 2.0f) noexcept;

    // Cellular noise
    static float Worley(float x, float y) noexcept;
    static float WorleyF2F1(float x, float y) noexcept;

    // Utility
    static void SetSeed(int seed);
    static int GetSeed() noexcept;
    static void Initialize();
    static bool IsInitialized() noexcept;
};
```

---

### SDFTerrain

SDF-based terrain for GI integration.

**Header:** `engine/terrain/SDFTerrain.hpp`

#### Class Definition

```cpp
class SDFTerrain {
public:
    // Initialization
    bool Initialize(const Config& config = {});
    void Shutdown();
    bool IsInitialized() const;

    // Building
    void BuildFromHeightmap(const float* heightData, int width, int height);
    void BuildFromTerrainGenerator(TerrainGenerator& terrain);
    void BuildFromVoxelTerrain(VoxelTerrain& terrain);
    void BuildFromFunction(std::function<float(const glm::vec3&)> generator);
    bool IsBuilding() const;
    float GetBuildProgress() const;

    // Queries
    float QueryDistance(const glm::vec3& pos) const;
    glm::vec3 QueryNormal(const glm::vec3& pos) const;
    int QueryMaterialID(const glm::vec3& pos) const;
    float SampleDistance(const glm::vec3& pos) const;
    float GetHeightAt(float x, float z) const;
    bool IsInside(const glm::vec3& pos) const;

    // LOD
    void UpdateLOD(const glm::vec3& cameraPos);
    int GetLODLevel(const glm::vec3& pos, const glm::vec3& cameraPos) const;
    unsigned int GetLODTexture(int lodLevel) const;

    // GPU
    void UploadToGPU();
    unsigned int GetSDFTexture() const;
    unsigned int GetOctreeSSBO() const;
    unsigned int GetMaterialSSBO() const;
    void BindForRendering(Shader* shader);

    // Raymarching
    bool Raymarch(const glm::vec3& origin, const glm::vec3& direction,
                  float maxDist, glm::vec3& hitPoint, glm::vec3& hitNormal) const;
    bool RaymarchAccelerated(const glm::vec3& origin, const glm::vec3& direction,
                             float maxDist, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

    // Materials
    void SetMaterial(int materialID, const TerrainMaterial& material);
    const TerrainMaterial& GetMaterial(int materialID) const;
    const std::vector<TerrainMaterial>& GetMaterials() const;

    // Info
    const Config& GetConfig() const;
    const Stats& GetStats() const;
    glm::vec3 GetWorldMin() const;
    glm::vec3 GetWorldMax() const;
};
```

#### Structures

```cpp
struct SDFTerrain::Config {
    int resolution = 512;
    float worldSize = 1000.0f;
    float maxHeight = 100.0f;
    int octreeLevels = 6;
    bool useOctree = true;
    bool sparseStorage = true;
    int numLODLevels = 4;
    std::vector<float> lodDistances = {100.0f, 250.0f, 500.0f, 1000.0f};
    bool supportCaves = false;
    bool highPrecision = false;
    bool compressGPU = true;
    int numMaterials = 8;
    bool storeMaterialPerVoxel = true;
    bool asyncBuild = true;
    int maxVoxelsPerFrame = 65536;
};

struct SDFTerrain::TerrainMaterial {
    glm::vec3 albedo{0.5f};
    float roughness = 0.8f;
    float metallic = 0.0f;
    glm::vec3 emissive{0.0f};
    int albedoTextureID = -1;
    int normalTextureID = -1;
    int roughnessTextureID = -1;
};

struct SDFTerrain::Stats {
    size_t totalVoxels = 0;
    size_t nonEmptyVoxels = 0;
    size_t octreeNodes = 0;
    size_t memoryBytes = 0;
    float buildTimeMs = 0.0f;
    float lastQueryTimeUs = 0.0f;
};
```

---

## Scene System

### Camera

3D camera with view/projection management and frustum culling.

**Header:** `engine/scene/Camera.hpp`

#### Class Definition

```cpp
class Camera {
public:
    Camera();
    virtual ~Camera() = default;

    // Projection
    void SetPerspective(float fovDegrees, float aspectRatio,
                        float nearPlane, float farPlane);
    void SetOrthographic(float left, float right, float bottom, float top,
                         float nearPlane, float farPlane);

    // Position/Orientation
    void LookAt(const glm::vec3& position, const glm::vec3& target,
                const glm::vec3& up = glm::vec3(0, 1, 0));
    void SetPosition(const glm::vec3& position);
    void SetRotation(const glm::vec3& eulerDegrees);
    void SetRotation(float pitchDegrees, float yawDegrees);

    // Getters - Position
    const glm::vec3& GetPosition() const;
    const glm::vec3& GetForward() const;
    const glm::vec3& GetRight() const;
    const glm::vec3& GetUp() const;
    float GetPitch() const;
    float GetYaw() const;

    // Getters - Matrices
    const glm::mat4& GetView() const;
    const glm::mat4& GetProjection() const;
    const glm::mat4& GetProjectionView() const;
    const glm::mat4& GetInverseProjectionView() const;

    // Getters - Projection params
    ProjectionType GetProjectionType() const;
    float GetFOV() const;
    float GetAspectRatio() const;
    float GetNearPlane() const;
    float GetFarPlane() const;

    // Coordinate conversion
    glm::vec3 ScreenToWorldRay(const glm::vec2& screenPos,
                                const glm::vec2& screenSize) const;
    glm::vec2 WorldToScreen(const glm::vec3& worldPos,
                            const glm::vec2& screenSize) const;

    // Frustum culling
    bool IsInFrustum(const glm::vec3& point) const;
    bool IsInFrustum(const glm::vec3& center, float radius) const;
};

enum class ProjectionType {
    Perspective,
    Orthographic
};
```

---

## Editor Systems

### SDFModelEditor

In-game/in-editor SDF model creation and editing.

**Header:** `game/src/editor/sdf/SDFModelEditor.hpp`

#### Class Definition

```cpp
class SDFModelEditor {
public:
    // Initialization
    bool Initialize(Editor* editor);
    void Shutdown();
    bool IsInitialized() const;

    // Model management
    void NewModel(const std::string& name = "NewModel");
    bool LoadModel(const std::string& path);
    bool SaveModel(const std::string& path);
    SDFModel* GetModel();
    void SetModel(std::unique_ptr<SDFModel> model);
    bool HasUnsavedChanges() const;
    void MarkDirty();

    // Update/Render
    void Update(float deltaTime);
    void RenderUI();
    void Render3D(Renderer& renderer, Camera& camera);
    void ProcessInput();

    // Selection
    void SelectPrimitive(SDFPrimitive* primitive);
    void ClearSelection();
    SDFPrimitive* GetSelectedPrimitive();
    SDFPrimitive* PickPrimitive(const glm::vec3& rayOrigin, const glm::vec3& rayDir);

    // Operations
    SDFPrimitive* AddPrimitive(SDFPrimitiveType type, SDFPrimitive* parent = nullptr);
    SDFPrimitive* DuplicateSelected();
    void DeleteSelected();
    void GroupSelected();
    void UngroupSelected();

    // Animation
    SDFAnimationController* GetAnimationController();
    SDFPoseLibrary* GetPoseLibrary();
    void SaveCurrentPose(const std::string& name, const std::string& category = "Default");
    void ApplyPose(const std::string& name);
    void StartRecording();
    void StopRecording();
    bool IsRecording() const;
    void AddKeyframe();
    void SetAnimationTime(float time);
    float GetAnimationTime() const;
    void PlayAnimation();
    void PauseAnimation();
    void StopAnimation();

    // Tool modes
    void SetToolMode(SDFToolMode mode);
    SDFToolMode GetToolMode() const;
    void SetGizmoMode(SDFGizmoMode mode);
    SDFGizmoMode GetGizmoMode() const;
    SDFBrushSettings& GetBrushSettings();

    // Export
    bool ExportToEntityJson(const std::string& jsonPath);
    bool ImportFromEntityJson(const std::string& jsonPath);
    bool ExportMeshOBJ(const std::string& path);

    // Callbacks
    std::function<void(SDFPrimitive*)> OnPrimitiveSelected;
    std::function<void()> OnModelChanged;
    std::function<void(const std::string&)> OnPoseSaved;
    std::function<void(float)> OnAnimationTimeChanged;
};
```

#### Enumerations

```cpp
enum class SDFGizmoMode {
    None,
    Translate,
    Rotate,
    Scale
};

enum class SDFToolMode {
    Select,
    Create,
    Paint,
    Sculpt
};

struct SDFBrushSettings {
    float radius = 0.1f;
    float hardness = 0.5f;
    float opacity = 1.0f;
    glm::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
    std::string currentLayer;
};
```

---

### MaterialEditorAdvanced

Advanced material editing with node graph.

**Header:** `editor/MaterialEditorAdvanced.hpp`

#### Class Definition

```cpp
class MaterialEditorAdvanced {
public:
    // Main interface
    void Render();

    // Material management
    void NewMaterial();
    void LoadMaterial(const std::string& filepath);
    void SaveMaterial(const std::string& filepath);
    void SetMaterial(std::shared_ptr<AdvancedMaterial> material);
    std::shared_ptr<AdvancedMaterial> GetMaterial() const;

    // UI panels
    void RenderMainWindow();
    void RenderGraphEditor();
    void RenderPropertyInspector();
    void RenderPreviewPanel();
    void RenderMaterialLibrary();
    void RenderMenuBar();
    void RenderToolbar();
    void RenderStatusBar();

    // Graph editor
    MaterialGraphEditor& GetGraphEditor();
};
```

---

## Common Patterns

### Initialization Pattern

```cpp
int main() {
    auto& engine = Nova::Engine::Instance();

    if (!engine.Initialize()) {
        return -1;
    }

    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onStartup = []() { return true; };
    callbacks.onUpdate = [](float dt) { /* update */ };
    callbacks.onRender = []() { /* render */ };

    return engine.Run(std::move(callbacks));
}
```

### SDF Scene Setup

```cpp
// Create renderer and GI
Nova::SDFRenderer sdfRenderer;
sdfRenderer.Initialize();

auto gi = std::make_shared<Nova::RadianceCascade>();
gi->Initialize();
sdfRenderer.SetRadianceCascade(gi);
sdfRenderer.SetGlobalIlluminationEnabled(true);

// Create scene with acceleration
Nova::SDFAccelerationStructure accel;
std::vector<Nova::SDFInstance> instances;
// ... populate instances ...
accel.Build(instances);

// Render loop
void Render() {
    gi->Update(camera.GetPosition(), deltaTime);
    gi->PropagateLighting();

    auto visible = accel.QueryFrustum(Frustum(camera.GetProjectionView()));
    for (int id : visible) {
        sdfRenderer.Render(*instances[id].model, camera, instances[id].transform);
    }
}
```

### Terrain Setup

```cpp
Nova::TerrainGenerator terrain;
terrain.Initialize();
terrain.SetViewDistance(8);
terrain.SetChunkSize(64);
terrain.SetHeightScale(100.0f);
terrain.SetNoiseParams(0.02f, 6, 0.5f, 2.0f);

// Convert to SDF for GI
Nova::SDFTerrain sdfTerrain;
sdfTerrain.Initialize();
sdfTerrain.BuildFromTerrainGenerator(terrain);

// Update loop
void Update(float dt) {
    terrain.Update(camera.GetPosition());
    terrain.ProcessPendingMeshes(2);
    sdfTerrain.UpdateLOD(camera.GetPosition());
}
```

---

## Performance Guidelines

### SDF Optimization

| Scenario | Recommendation |
|----------|----------------|
| < 100 instances | Direct rendering |
| 100-1000 instances | Use BVH acceleration |
| > 1000 instances | BVH + LOD + culling |
| Dynamic scenes | Use `Refit()` instead of `Build()` |
| Many primitives/model | Limit to 256, use mesh for complex |

### GI Optimization

| Setting | Performance Impact |
|---------|-------------------|
| `numCascades` | Linear with count |
| `baseResolution` | Cubic (N^3) |
| `raysPerProbe` | Linear |
| `bounces` | Linear per bounce |
| `temporalBlend` | Higher = smoother, less updates |

### Raymarching Quality

| Max Steps | Use Case |
|-----------|----------|
| 32 | Mobile, previews |
| 64 | General use |
| 128 | Default quality |
| 256 | Hero shots |
| 512 | Offline only |

---

## See Also

- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Quick start guide
- **[SDF_RENDERING_GUIDE.md](SDF_RENDERING_GUIDE.md)** - SDF deep dive
- **[ANIMATION_GUIDE.md](ANIMATION_GUIDE.md)** - Animation system
- **[EDITOR_GUIDE.md](EDITOR_GUIDE.md)** - Editor usage

---

*Nova3D Engine v1.0.0 - High-Performance SDF Rendering with Real-Time Global Illumination*
