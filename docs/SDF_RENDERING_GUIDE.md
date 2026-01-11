# SDF Rendering Guide

This comprehensive guide covers the Signed Distance Field (SDF) rendering system in Nova3D, including primitives, CSG operations, raymarching, and integration with the global illumination system.

## Table of Contents

1. [Introduction to SDF](#introduction-to-sdf)
2. [SDF Primitives](#sdf-primitives)
3. [CSG Operations](#csg-operations)
4. [SDFModel and Hierarchy](#sdfmodel-and-hierarchy)
5. [SDFRenderer](#sdfrenderer)
6. [Acceleration Structures](#acceleration-structures)
7. [Global Illumination Integration](#global-illumination-integration)
8. [Spectral Rendering](#spectral-rendering)
9. [Performance Optimization](#performance-optimization)
10. [Shader Reference](#shader-reference)

---

## Introduction to SDF

### What is a Signed Distance Field?

A Signed Distance Field (SDF) represents geometry by storing the shortest distance from any point in space to the surface. The sign indicates whether the point is inside (negative) or outside (positive) the surface.

```
     Distance Field Visualization
     ============================

     Outside (+)     Surface (0)     Inside (-)
          |              |               |
          v              v               v

        +2.0  +1.0  +0.5  0  -0.5  -1.0  -2.0
          |     |     |   |    |     |     |
     -----+-----+-----+===+----+-----+-----+----> distance
                      ^
                      |
                 Surface boundary
```

### Advantages of SDF Rendering

| Feature | Benefit |
|---------|---------|
| **Smooth Blending** | Organic CSG operations without mesh artifacts |
| **Infinite Resolution** | Detail limited only by raymarching precision |
| **Soft Shadows** | Natural soft shadow calculation |
| **Ambient Occlusion** | Analytically computed AO |
| **Easy Animation** | Smooth morphing between shapes |

---

## SDF Primitives

### Primitive Types

Nova3D supports the following SDF primitive types:

```cpp
enum class SDFPrimitiveType : uint8_t {
    Sphere,       // Perfect sphere
    Box,          // Axis-aligned box
    Cylinder,     // Vertical cylinder
    Capsule,      // Cylinder with hemispherical caps
    Cone,         // Circular cone
    Torus,        // Donut shape
    Plane,        // Infinite plane
    RoundedBox,   // Box with rounded edges
    Ellipsoid,    // Stretched sphere
    Pyramid,      // Square-based pyramid
    Prism,        // N-sided prism
    Custom        // User-defined function
};
```

### Creating Primitives

```cpp
#include <engine/sdf/SDFPrimitive.hpp>

// Create a sphere
Nova::SDFPrimitive sphere("MySphere", Nova::SDFPrimitiveType::Sphere);
sphere.GetParameters().radius = 1.0f;

// Create a box
Nova::SDFPrimitive box("MyBox", Nova::SDFPrimitiveType::Box);
box.GetParameters().dimensions = glm::vec3(2.0f, 1.0f, 1.5f);

// Create a rounded box
Nova::SDFPrimitive roundedBox("RoundBox", Nova::SDFPrimitiveType::RoundedBox);
roundedBox.GetParameters().dimensions = glm::vec3(1.0f, 1.0f, 1.0f);
roundedBox.GetParameters().cornerRadius = 0.1f;

// Create a cylinder
Nova::SDFPrimitive cylinder("MyCylinder", Nova::SDFPrimitiveType::Cylinder);
cylinder.GetParameters().height = 2.0f;
cylinder.GetParameters().bottomRadius = 0.5f;

// Create a torus
Nova::SDFPrimitive torus("MyTorus", Nova::SDFPrimitiveType::Torus);
torus.GetParameters().majorRadius = 1.0f;
torus.GetParameters().minorRadius = 0.3f;

// Create an N-sided prism
Nova::SDFPrimitive hexPrism("HexPrism", Nova::SDFPrimitiveType::Prism);
hexPrism.GetParameters().sides = 6;
hexPrism.GetParameters().height = 1.0f;
```

### Primitive Parameters Reference

| Primitive | Key Parameters |
|-----------|----------------|
| Sphere | `radius` |
| Box | `dimensions` (vec3) |
| RoundedBox | `dimensions`, `cornerRadius` |
| Cylinder | `height`, `bottomRadius` |
| Capsule | `height`, `bottomRadius` |
| Cone | `height`, `bottomRadius`, `topRadius` |
| Torus | `majorRadius`, `minorRadius` |
| Ellipsoid | `radii` (vec3) |
| Prism | `sides`, `height` |
| Pyramid | `height`, base implied by dimensions |

### Transform System

```cpp
// Local transform
Nova::SDFTransform transform;
transform.position = glm::vec3(1.0f, 0.0f, 0.0f);
transform.rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
transform.scale = glm::vec3(1.0f, 2.0f, 1.0f);

primitive.SetLocalTransform(transform);

// Get world transform (accumulates parent transforms)
Nova::SDFTransform worldTransform = primitive.GetWorldTransform();
glm::mat4 matrix = worldTransform.ToMatrix();
```

### Material Properties

```cpp
Nova::SDFMaterial& mat = primitive.GetMaterial();

// PBR properties
mat.baseColor = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);  // Red
mat.metallic = 0.0f;    // 0 = dielectric, 1 = metal
mat.roughness = 0.5f;   // 0 = mirror, 1 = diffuse

// Emission
mat.emissive = 2.0f;    // Emission intensity
mat.emissiveColor = glm::vec3(1.0f, 0.5f, 0.0f);  // Orange glow

// Texturing (optional)
mat.texturePath = "textures/rock_albedo.png";
mat.normalMapPath = "textures/rock_normal.png";
mat.uvScale = glm::vec2(2.0f);  // Tile texture 2x
```

---

## CSG Operations

### Operation Types

CSG (Constructive Solid Geometry) combines primitives:

```cpp
enum class CSGOperation : uint8_t {
    Union,               // A + B (combine)
    Subtraction,         // A - B (carve)
    Intersection,        // A & B (overlap only)
    SmoothUnion,         // Smooth A + B
    SmoothSubtraction,   // Smooth A - B
    SmoothIntersection   // Smooth A & B
};
```

### Visual CSG Reference

```
  UNION                SUBTRACTION           INTERSECTION
  =====                ===========           ============

  +---+                  +---+                  +---+
  | A +---+              | A  \--+              |   +---+
  |   | B |    ==>       |      B|    ==>       | X |   |
  +---+   |              +---\   |              +---+   |
      +---+                  +---+                  +---+

  Result: A+B          Result: A-B           Result: A&B
```

### Using CSG Operations

```cpp
// Create a box with a hole
Nova::SDFModel model("BoxWithHole");

// Main box
auto* box = model.CreatePrimitive("Box", Nova::SDFPrimitiveType::Box);
box->GetParameters().dimensions = glm::vec3(2.0f, 2.0f, 2.0f);

// Hole (cylinder to subtract)
auto* hole = model.CreatePrimitive("Hole", Nova::SDFPrimitiveType::Cylinder, box);
hole->GetParameters().height = 3.0f;  // Extend through box
hole->GetParameters().bottomRadius = 0.5f;
hole->SetCSGOperation(Nova::CSGOperation::Subtraction);
```

### Smooth Blending

Smooth CSG creates organic transitions:

```cpp
// Create smooth blob union
auto* sphere1 = model.CreatePrimitive("Sphere1", Nova::SDFPrimitiveType::Sphere);
sphere1->GetParameters().radius = 1.0f;

auto* sphere2 = model.CreatePrimitive("Sphere2", Nova::SDFPrimitiveType::Sphere);
sphere2->GetLocalTransform().position = glm::vec3(1.2f, 0, 0);
sphere2->GetParameters().radius = 0.8f;

// Enable smooth union with blend factor
sphere2->SetCSGOperation(Nova::CSGOperation::SmoothUnion);
sphere2->GetParameters().smoothness = 0.3f;  // Larger = smoother blend
```

### SDF Evaluation Functions

The engine provides optimized SDF functions:

```cpp
namespace Nova::SDFEval {
    // Basic primitives
    float Sphere(const glm::vec3& p, float radius);
    float Box(const glm::vec3& p, const glm::vec3& dimensions);
    float RoundedBox(const glm::vec3& p, const glm::vec3& dim, float radius);
    float Cylinder(const glm::vec3& p, float height, float radius);
    float Capsule(const glm::vec3& p, float height, float radius);
    float Cone(const glm::vec3& p, float height, float radius);
    float Torus(const glm::vec3& p, float majorRadius, float minorRadius);
    float Ellipsoid(const glm::vec3& p, const glm::vec3& radii);
    float Prism(const glm::vec3& p, int sides, float radius, float height);

    // CSG operations
    float Union(float d1, float d2);
    float Subtraction(float d1, float d2);
    float Intersection(float d1, float d2);
    float SmoothUnion(float d1, float d2, float k);
    float SmoothSubtraction(float d1, float d2, float k);
    float SmoothIntersection(float d1, float d2, float k);

    // Advanced smooth blending
    float ExponentialSmoothUnion(float d1, float d2, float k);
    float PowerSmoothUnion(float d1, float d2, float k);
    float CubicSmoothUnion(float d1, float d2, float k);
    float DistanceAwareSmoothUnion(float d1, float d2, float k, float minDist);
}
```

---

## SDFModel and Hierarchy

### Creating Models

```cpp
#include <engine/sdf/SDFModel.hpp>

// Create a model
Nova::SDFModel character("RobotCharacter");

// Create primitive hierarchy
auto* body = character.CreatePrimitive("Body", Nova::SDFPrimitiveType::Capsule);
auto* head = character.CreatePrimitive("Head", Nova::SDFPrimitiveType::Sphere, body);
auto* leftArm = character.CreatePrimitive("LeftArm", Nova::SDFPrimitiveType::Capsule, body);
auto* rightArm = character.CreatePrimitive("RightArm", Nova::SDFPrimitiveType::Capsule, body);

// Position children relative to parent
head->GetLocalTransform().position = glm::vec3(0, 1.5f, 0);
leftArm->GetLocalTransform().position = glm::vec3(-0.8f, 0.5f, 0);
rightArm->GetLocalTransform().position = glm::vec3(0.8f, 0.5f, 0);
```

### Hierarchy Operations

```cpp
// Find primitive by name
Nova::SDFPrimitive* arm = character.FindPrimitive("LeftArm");

// Get all primitives
std::vector<Nova::SDFPrimitive*> allPrimitives = character.GetAllPrimitives();

// Delete primitive (and children)
character.DeletePrimitive(arm);

// Evaluate combined SDF at a point
float distance = character.EvaluateSDF(glm::vec3(0, 0, 0));

// Get model bounds
auto [min, max] = character.GetBounds();
```

### Mesh Generation

Convert SDF to polygon mesh using Marching Cubes:

```cpp
// Configure mesh generation
Nova::SDFMeshSettings settings;
settings.resolution = 128;           // Voxel grid resolution
settings.isoLevel = 0.0f;           // Surface threshold
settings.smoothNormals = true;       // Smooth vertex normals
settings.generateUVs = true;         // Generate texture coordinates
settings.generateTangents = true;    // For normal mapping
settings.simplifyMesh = true;        // Reduce triangle count
settings.simplifyRatio = 0.5f;       // Target 50% of triangles

// Generate mesh
auto mesh = character.GenerateMesh(settings);

// Generate with LODs
settings.generateLODs = true;
settings.lodLevels = 4;
settings.lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
auto meshLODs = character.GenerateMeshLODs(settings);
```

### Serialization

```cpp
// Save to file
character.SaveToFile("assets/characters/robot.sdfmodel");

// Load from file
Nova::SDFModel loaded;
loaded.LoadFromFile("assets/characters/robot.sdfmodel");

// JSON serialization
std::string json = character.ToJson();
character.FromJson(json);
```

---

## SDFRenderer

### Initialization

```cpp
#include <engine/graphics/SDFRenderer.hpp>

Nova::SDFRenderer renderer;

if (!renderer.Initialize()) {
    std::cerr << "Failed to initialize SDF renderer" << std::endl;
    return;
}
```

### Render Settings

```cpp
Nova::SDFRenderSettings& settings = renderer.GetSettings();

// Raymarching quality
settings.maxSteps = 128;           // Max raymarch iterations
settings.maxDistance = 100.0f;     // Max ray travel distance
settings.hitThreshold = 0.001f;    // Surface hit precision

// Features
settings.enableShadows = true;
settings.enableAO = true;
settings.enableReflections = false;

// Shadow settings
settings.shadowSoftness = 8.0f;
settings.shadowSteps = 32;

// Ambient occlusion
settings.aoSteps = 5;
settings.aoDistance = 0.5f;
settings.aoIntensity = 0.5f;

// Lighting
settings.lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
settings.lightColor = glm::vec3(1.0f, 0.98f, 0.95f);
settings.lightIntensity = 1.0f;

// Background
settings.backgroundColor = glm::vec3(0.1f, 0.1f, 0.15f);
settings.useEnvironmentMap = true;
```

### Rendering

```cpp
// Basic render
renderer.Render(model, camera);

// Render with custom transform
glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), glm::vec3(5, 0, 0));
renderer.Render(model, camera, modelTransform);

// Render to texture
auto framebuffer = std::make_shared<Nova::Framebuffer>(1920, 1080);
renderer.RenderToTexture(model, camera, framebuffer);

// Batch rendering (instanced)
std::vector<const Nova::SDFModel*> models = {&model1, &model2, &model3};
std::vector<glm::mat4> transforms = {transform1, transform2, transform3};
renderer.RenderBatch(models, transforms, camera);
```

### Environment Mapping

```cpp
// Load and set environment map
auto envMap = std::make_shared<Nova::Texture>();
envMap->LoadHDR("assets/skyboxes/sunset.hdr");

renderer.SetEnvironmentMap(envMap);
renderer.GetSettings().useEnvironmentMap = true;
```

---

## Acceleration Structures

### BVH (Bounding Volume Hierarchy)

For scenes with many SDF instances:

```cpp
#include <engine/graphics/SDFAcceleration.hpp>

// Create acceleration structure
Nova::SDFAccelerationStructure accel;

// Prepare instances
std::vector<Nova::SDFInstance> instances;
for (int i = 0; i < 1000; i++) {
    Nova::SDFInstance inst;
    inst.model = &myModel;
    inst.transform = GetRandomTransform();
    inst.instanceId = i;
    inst.dynamic = false;  // Static instance
    instances.push_back(inst);
}

// Build BVH
Nova::BVHBuildSettings bvhSettings;
bvhSettings.strategy = Nova::BVHBuildStrategy::SAH;  // Best quality
bvhSettings.maxPrimitivesPerLeaf = 4;
bvhSettings.parallelBuild = true;

accel.Build(instances, bvhSettings);

// Get build stats
const auto& stats = accel.GetStats();
std::cout << "BVH nodes: " << stats.nodeCount << std::endl;
std::cout << "Build time: " << stats.buildTimeMs << "ms" << std::endl;
```

### Spatial Queries

```cpp
// Frustum culling
Nova::Frustum frustum(camera.GetProjectionView());
std::vector<int> visible = accel.QueryFrustum(frustum);

// Ray intersection
Nova::Ray ray(camera.GetPosition(), camera.GetForward());
std::vector<int> hits = accel.QueryRay(ray, 100.0f);

// Find closest
float distance;
int closest = accel.FindClosestInstance(ray, distance);

// AABB query
Nova::AABB queryBox(glm::vec3(-10), glm::vec3(10));
std::vector<int> inBox = accel.QueryAABB(queryBox);

// Sphere query
std::vector<int> nearby = accel.QuerySphere(playerPos, 50.0f);
```

### GPU Upload

```cpp
// Upload BVH to GPU for shader access
unsigned int ssbo = accel.UploadToGPU();

// In shader, bind the SSBO
glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, accel.GetGPUBuffer());
```

### Dynamic Updates

```cpp
// Mark instances as dynamic during creation
instance.dynamic = true;

// Update transforms
std::vector<int> movedIds = {5, 12, 23};
std::vector<glm::mat4> newTransforms = {...};
accel.UpdateDynamic(movedIds, newTransforms);

// Refit BVH (faster than rebuild)
accel.Refit();

// Or full rebuild for best quality
accel.Build(instances, bvhSettings);
```

---

## Global Illumination Integration

### RadianceCascade Setup

```cpp
#include <engine/graphics/RadianceCascade.hpp>

Nova::RadianceCascade gi;

// Configure
Nova::RadianceCascade::Config config;
config.numCascades = 4;           // Number of LOD levels
config.baseResolution = 32;       // Finest cascade resolution
config.cascadeScale = 2.0f;       // Scale between cascades
config.baseSpacing = 1.0f;        // Probe spacing (world units)
config.updateRadius = 100.0f;     // Update radius from camera

// Quality
config.raysPerProbe = 64;         // Rays per probe
config.bounces = 2;               // Light bounces
config.useInterpolation = true;   // Trilinear interpolation

// Performance
config.asyncUpdate = true;
config.maxProbesPerFrame = 1024;
config.temporalBlend = 0.95f;     // Temporal stability

gi.Initialize(config);
```

### Connecting to Renderer

```cpp
// Share RadianceCascade with SDFRenderer
auto sharedGI = std::make_shared<Nova::RadianceCascade>();
sharedGI->Initialize(config);

sdfRenderer.SetRadianceCascade(sharedGI);
sdfRenderer.SetGlobalIlluminationEnabled(true);
```

### Update Loop

```cpp
// In your update function
void Update(float deltaTime) {
    // Update GI from camera position
    gi.Update(camera.GetPosition(), deltaTime);

    // Inject emissive objects
    for (auto& emitter : emissiveObjects) {
        gi.InjectEmissive(emitter.position, emitter.radiance, emitter.radius);
    }

    // Propagate lighting
    gi.PropagateLighting();
}
```

### Sampling Radiance

```cpp
// Sample indirect lighting at a point
glm::vec3 indirect = gi.SampleRadiance(worldPosition, surfaceNormal);

// In shader, bind cascade textures
for (int i = 0; i < config.numCascades; i++) {
    glBindTextureUnit(i, gi.GetCascadeTexture(i));
}
```

---

## Spectral Rendering

### Overview

Nova3D supports wavelength-dependent rendering for accurate chromatic effects:

```cpp
#include <engine/graphics/SpectralRenderer.hpp>

Engine::SpectralRenderer spectral;

// Rendering modes
spectral.mode = Engine::SpectralRenderer::Mode::RGB;           // Fast, standard
spectral.mode = Engine::SpectralRenderer::Mode::Spectral;      // Full spectral
spectral.mode = Engine::SpectralRenderer::Mode::HeroWavelength; // Balanced (default)
```

### Chromatic Dispersion

```cpp
// Calculate dispersed IOR for glass
float baseIOR = 1.5f;
float abbeNumber = 60.0f;  // Crown glass

float iorRed = Engine::SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, 700.0f);
float iorGreen = Engine::SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, 550.0f);
float iorBlue = Engine::SpectralRenderer::GetDispersedIOR(baseIOR, abbeNumber, 450.0f);

// Calculate refracted rays
glm::vec3 refractedR, refractedG, refractedB;
Engine::ChromaticDispersion::CalculateRGB(
    incident, normal, baseIOR, abbeNumber,
    refractedR, refractedG, refractedB
);
```

### Wavelength Utilities

```cpp
// Convert wavelength to RGB
glm::vec3 color = Engine::SpectralRenderer::WavelengthToRGB(550.0f);  // Green

// Get CIE color matching function
auto cmf = Engine::SpectralRenderer::GetCIE_CMF(580.0f);  // Yellow region

// Convert spectrum to XYZ then RGB
std::vector<float> spectrum = {...};
std::vector<float> wavelengths = {...};
glm::vec3 xyz = Engine::SpectralRenderer::SpectralToXYZ(spectrum, wavelengths);
glm::vec3 rgb = Engine::SpectralRenderer::XYZ_to_RGB(xyz);
```

### SDFRenderer Integration

```cpp
// Enable spectral features
sdfRenderer.SetDispersionEnabled(true);
sdfRenderer.SetBlackbodyEnabled(true);
sdfRenderer.SetDiffractionEnabled(false);  // Expensive

// Set spectral mode (maps to SpectralRenderer::Mode)
sdfRenderer.SetSpectralMode(2);  // HeroWavelength
```

---

## Performance Optimization

### Raymarching Optimization

```
  Raymarching Steps vs Quality
  ============================

  Steps   Quality          Performance
  -----   -------          -----------
   32     Low/Fast         Mobile, previews
   64     Medium           General use
  128     High             Default
  256     Very High        Hero shots
  512     Maximum          Offline rendering

  Tip: Use lower steps during camera movement,
       increase when stationary.
```

### Best Practices

1. **Limit Primitive Count**
   - Target < 256 primitives per model
   - Use mesh conversion for complex static geometry

2. **Optimize CSG Hierarchy**
   ```cpp
   // Good: Flat structure
   model.CreatePrimitive("A", type);
   model.CreatePrimitive("B", type);
   model.CreatePrimitive("C", type);

   // Avoid: Deep nesting
   auto* a = model.CreatePrimitive("A", type);
   auto* b = model.CreatePrimitive("B", type, a);
   auto* c = model.CreatePrimitive("C", type, b);  // 3 levels deep
   ```

3. **Use Acceleration Structures**
   ```cpp
   // For 100+ instances, always use BVH
   if (instances.size() > 100) {
       accel.Build(instances);
       auto visible = accel.QueryFrustum(camera.GetFrustum());
       // Only render visible instances
   }
   ```

4. **LOD System**
   ```cpp
   // Distance-based quality
   float distance = glm::length(objectPos - cameraPos);
   if (distance > 100.0f) {
       settings.maxSteps = 32;
       settings.enableReflections = false;
   } else {
       settings.maxSteps = 128;
       settings.enableReflections = true;
   }
   ```

5. **Temporal Stability**
   ```cpp
   // Enable temporal blending for GI
   giConfig.temporalBlend = 0.95f;  // Smooth, stable

   // Lower for fast-moving scenes
   giConfig.temporalBlend = 0.8f;
   ```

### Performance Metrics

```cpp
// Get renderer statistics
const auto& stats = renderer.GetStats();
std::cout << "Draw calls: " << stats.drawCalls << std::endl;

// GI statistics
const auto& giStats = gi.GetStats();
std::cout << "Active probes: " << giStats.activeProbes << std::endl;
std::cout << "Update time: " << giStats.updateTimeMs << "ms" << std::endl;

// BVH statistics
const auto& bvhStats = accel.GetStats();
std::cout << "Memory: " << bvhStats.memoryBytes / 1024 << "KB" << std::endl;
```

---

## Shader Reference

### GPU Data Structures

```glsl
// SDF primitive data (matches SDFPrimitiveData in C++)
struct SDFPrimitive {
    mat4 transform;
    mat4 inverseTransform;
    vec4 parameters;    // radius, dim.x, dim.y, dim.z
    vec4 parameters2;   // height, topRadius, bottomRadius, cornerRadius
    vec4 parameters3;   // majorRadius, minorRadius, smoothness, sides
    vec4 material;      // metallic, roughness, emissive, unused
    vec4 baseColor;
    vec4 emissiveColor;
    int type;
    int csgOperation;
    int visible;
    int padding;
};
```

### GLSL SDF Functions

```glsl
// Basic SDF primitives
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdTorus(vec3 p, float R, float r) {
    vec2 q = vec2(length(p.xz) - R, p.y);
    return length(q) - r;
}

// CSG operations
float opUnion(float d1, float d2) {
    return min(d1, d2);
}

float opSubtraction(float d1, float d2) {
    return max(d1, -d2);
}

float opIntersection(float d1, float d2) {
    return max(d1, d2);
}

float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}
```

### Raymarching Template

```glsl
vec3 raymarch(vec3 ro, vec3 rd, float maxDist, int maxSteps) {
    float t = 0.0;

    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = sceneSDF(p);

        if (d < HIT_THRESHOLD) {
            return p;  // Hit
        }

        t += d;

        if (t > maxDist) {
            break;  // Miss
        }
    }

    return vec3(-1.0);  // No hit
}

// Calculate normal via gradient
vec3 calcNormal(vec3 p) {
    const float eps = 0.001;
    return normalize(vec3(
        sceneSDF(p + vec3(eps, 0, 0)) - sceneSDF(p - vec3(eps, 0, 0)),
        sceneSDF(p + vec3(0, eps, 0)) - sceneSDF(p - vec3(0, eps, 0)),
        sceneSDF(p + vec3(0, 0, eps)) - sceneSDF(p - vec3(0, 0, eps))
    ));
}

// Soft shadows
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;

    for (int i = 0; i < 32; i++) {
        float h = sceneSDF(ro + rd * t);
        res = min(res, k * h / t);
        t += clamp(h, 0.02, 0.10);
        if (h < 0.001 || t > maxt) break;
    }

    return clamp(res, 0.0, 1.0);
}

// Ambient occlusion
float calcAO(vec3 pos, vec3 nor) {
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 0; i < 5; i++) {
        float h = 0.01 + 0.12 * float(i) / 4.0;
        float d = sceneSDF(pos + h * nor);
        occ += (h - d) * sca;
        sca *= 0.95;
    }

    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}
```

---

## See Also

- **[API_REFERENCE.md](API_REFERENCE.md)** - Complete API reference
- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Quick start guide
- **[TERRAIN_SDF_GI.md](TERRAIN_SDF_GI.md)** - Terrain SDF integration
- **[GI_INTEGRATION_COMPLETE.md](GI_INTEGRATION_COMPLETE.md)** - GI implementation details

---

*Nova3D Engine - High-Performance SDF Rendering with Real-Time Global Illumination*
