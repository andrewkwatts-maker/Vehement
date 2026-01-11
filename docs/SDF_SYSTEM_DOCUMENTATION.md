# SDF (Signed Distance Field) System Documentation

## Overview

The Nova Engine SDF System provides a complete pipeline for creating, editing, animating, and rendering 3D models using Signed Distance Fields. This document covers the entire workflow from model creation to in-game rendering.

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Core Components](#core-components)
3. [Creating SDF Models](#creating-sdf-models)
4. [Animation System](#animation-system)
5. [Rendering Pipeline](#rendering-pipeline)
6. [Editor Integration](#editor-integration)
7. [Performance Considerations](#performance-considerations)
8. [Examples and Best Practices](#examples-and-best-practices)

---

## System Architecture

### Component Overview

```
┌──────────────────────────────────────────────────────────────┐
│                    SDF Model Hierarchy                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │SDFPrimitive │──│SDFPrimitive │──│SDFPrimitive │          │
│  │   (Root)    │  │  (Child 1)  │  │  (Child 2)  │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
└──────────────────────────────────────────────────────────────┘
                           │
                           ├──────────────────────────────┐
                           ▼                              ▼
              ┌────────────────────────┐    ┌────────────────────────┐
              │   SDFAnimation         │    │    SDFRenderer         │
              │   - Keyframes          │    │    - Raymarching       │
              │   - Pose Library       │    │    - GPU Compute       │
              │   - State Machine      │    │    - CSG Operations    │
              └────────────────────────┘    └────────────────────────┘
                           │                              │
                           └──────────┬───────────────────┘
                                      ▼
                          ┌────────────────────────┐
                          │   SDFModelEditor       │
                          │   - Hierarchy Panel    │
                          │   - Inspector Panel    │
                          │   - Timeline Editor    │
                          └────────────────────────┘
```

### File Structure

```
engine/
├── sdf/
│   ├── SDFModel.hpp/cpp              # Model container
│   ├── SDFPrimitive.hpp/cpp          # Primitive types & evaluation
│   ├── SDFAnimation.hpp/cpp          # Animation system
│   └── SDFSerializer.hpp/cpp         # JSON serialization
├── graphics/
│   ├── SDFRenderer.hpp/cpp           # GPU raymarching renderer
│   └── shaders/
│       ├── sdf_raymarching.vert      # Fullscreen quad vertex shader
│       └── sdf_raymarching.frag      # Raymarching fragment shader
game/
├── src/editor/sdf/
│   └── SDFModelEditor.hpp/cpp        # In-editor SDF model editor
└── assets/
    └── sdf_models/
        ├── example_soldier_unit.json
        ├── example_tower_building.json
        └── example_magic_effect.json
```

---

## Core Components

### SDFPrimitive

Represents a single geometric primitive in the SDF hierarchy.

**Supported Primitive Types:**
- **Sphere** - Uniform radius
- **Box** - Rectangular prism
- **RoundedBox** - Box with rounded edges
- **Cylinder** - Cylindrical shape
- **Capsule** - Pill shape (cylinder with spherical caps)
- **Cone** - Conical shape
- **Torus** - Donut shape
- **Plane** - Infinite plane
- **Ellipsoid** - Stretched sphere
- **Pyramid** - Four-sided pyramid
- **Prism** - N-sided prism
- **Custom** - Custom SDF function

**CSG Operations:**
- **Union** - Combines shapes (additive)
- **Subtraction** - Carves out shape (subtractive)
- **Intersection** - Keeps only overlapping region
- **Smooth Union** - Smooth blend between shapes
- **Smooth Subtraction** - Smooth carving
- **Smooth Intersection** - Smooth overlap

**Code Example:**
```cpp
// Create a primitive
auto primitive = std::make_unique<SDFPrimitive>("Body", SDFPrimitiveType::Capsule);

// Set transform
SDFTransform transform;
transform.position = glm::vec3(0, 0, 0);
transform.rotation = glm::quat(1, 0, 0, 0);
transform.scale = glm::vec3(1, 1, 1);
primitive->SetLocalTransform(transform);

// Set parameters
SDFParameters params;
params.radius = 0.3f;
params.height = 1.5f;
primitive->SetParameters(params);

// Set material
SDFMaterial material;
material.baseColor = glm::vec4(0.3f, 0.5f, 0.3f, 1.0f);
material.metallic = 0.1f;
material.roughness = 0.8f;
primitive->SetMaterial(material);
```

### SDFModel

Container for an entire SDF model with hierarchy support.

**Key Features:**
- Hierarchical primitive tree
- Marching Cubes mesh generation
- Texture painting layers
- Pose management
- Bind pose storage

**Code Example:**
```cpp
// Create model
auto model = std::make_unique<SDFModel>("MyCharacter");

// Add primitives
SDFPrimitive* root = model->CreatePrimitive("Body", SDFPrimitiveType::Capsule);
SDFPrimitive* head = model->CreatePrimitive("Head", SDFPrimitiveType::Sphere, root);
SDFPrimitive* arm = model->CreatePrimitive("Arm", SDFPrimitiveType::Capsule, root);

// Generate mesh for rendering
auto mesh = model->GenerateMesh();

// Save to file
model->SaveToFile("assets/models/my_character.sdf.json");
```

### SDFAnimation

Complete animation system with keyframes, poses, and state machines.

**Components:**
- **SDFAnimationClip** - Keyframed animation sequence
- **SDFPoseLibrary** - Named pose storage and blending
- **SDFAnimationStateMachine** - State-based animation control
- **SDFAnimationController** - High-level playback control

**Code Example:**
```cpp
// Create animation clip
auto clip = std::make_shared<SDFAnimationClip>("Walk");
clip->SetDuration(1.0f);
clip->SetLooping(true);

// Add keyframes
auto* kf1 = clip->AddKeyframe(0.0f);
kf1->transforms["LeftLeg"] = leftLegPose1;
kf1->transforms["RightLeg"] = rightLegPose1;

auto* kf2 = clip->AddKeyframe(0.5f);
kf2->transforms["LeftLeg"] = leftLegPose2;
kf2->transforms["RightLeg"] = rightLegPose2;

// Play animation
SDFAnimationController controller;
controller.SetModel(model.get());
controller.PlayClip(clip);

// Update in game loop
controller.Update(deltaTime);
```

### SDFRenderer

GPU-accelerated raymarching renderer for real-time SDF visualization.

**Features:**
- Fragment shader raymarching
- All primitive types supported
- CSG operations in shader
- PBR materials
- Soft shadows
- Ambient occlusion
- Reflections (optional)

**Code Example:**
```cpp
// Initialize renderer
SDFRenderer renderer;
renderer.Initialize();

// Configure settings
SDFRenderSettings settings;
settings.maxSteps = 128;
settings.enableShadows = true;
settings.enableAO = true;
settings.shadowSoftness = 8.0f;
renderer.SetSettings(settings);

// Render model
renderer.Render(*model, camera);
```

---

## Creating SDF Models

### Workflow Overview

1. **Plan Your Model** - Sketch out primitive hierarchy
2. **Create Base Shape** - Start with root primitive
3. **Add Details** - Build hierarchy with CSG operations
4. **Material Assignment** - Set colors and material properties
5. **Test and Iterate** - Preview in editor
6. **Save Configuration** - Export to JSON

### Step-by-Step Example: Creating a Character

#### 1. Create the Body (Root Primitive)

```cpp
auto model = std::make_unique<SDFModel>("Soldier");
auto* body = model->CreatePrimitive("Body", SDFPrimitiveType::Capsule);

SDFParameters bodyParams;
bodyParams.radius = 0.3f;
bodyParams.height = 1.5f;
body->SetParameters(bodyParams);
```

#### 2. Add Head

```cpp
auto* head = model->CreatePrimitive("Head", SDFPrimitiveType::Sphere, body);

SDFTransform headTransform;
headTransform.position = glm::vec3(0, 0.95f, 0);
head->SetLocalTransform(headTransform);

SDFParameters headParams;
headParams.radius = 0.25f;
head->SetParameters(headParams);

// Smooth union with body
head->SetCSGOperation(CSGOperation::SmoothUnion);
headParams.smoothness = 0.05f;
```

#### 3. Add Arms

```cpp
auto* leftArm = model->CreatePrimitive("LeftArm", SDFPrimitiveType::Capsule, body);
SDFTransform leftArmTransform;
leftArmTransform.position = glm::vec3(-0.45f, 0.3f, 0);
leftArmTransform.rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(0, 0, 1));
leftArm->SetLocalTransform(leftArmTransform);

SDFParameters armParams;
armParams.radius = 0.12f;
armParams.height = 0.8f;
leftArm->SetParameters(armParams);
```

#### 4. Add Materials

```cpp
// Body material
SDFMaterial bodyMat;
bodyMat.baseColor = glm::vec4(0.3f, 0.5f, 0.3f, 1.0f);
bodyMat.metallic = 0.1f;
bodyMat.roughness = 0.8f;
body->SetMaterial(bodyMat);

// Head material (skin tone)
SDFMaterial headMat;
headMat.baseColor = glm::vec4(0.85f, 0.7f, 0.6f, 1.0f);
headMat.metallic = 0.0f;
headMat.roughness = 0.9f;
head->SetMaterial(headMat);
```

#### 5. Save Model

```cpp
SDFSerializer::SaveModel(*model, "assets/models/soldier.sdf.json");
```

### Using CSG Operations

**Union (Additive):**
```cpp
// Combine shapes - default operation
primitive->SetCSGOperation(CSGOperation::Union);
```

**Subtraction (Carving):**
```cpp
// Create window by subtracting box from building
auto* window = model->CreatePrimitive("Window", SDFPrimitiveType::Box, wall);
window->SetCSGOperation(CSGOperation::Subtraction);
```

**Smooth Union (Organic Blending):**
```cpp
// Smooth blend between body parts
head->SetCSGOperation(CSGOperation::SmoothUnion);
SDFParameters params = head->GetParameters();
params.smoothness = 0.1f;  // Higher = smoother blend
head->SetParameters(params);
```

---

## Animation System

### Creating Poses

**Save Current Pose:**
```cpp
SDFPoseLibrary library;

// Capture current model pose
library.SavePoseFromModel("Idle", *model, "Character");
library.SavePoseFromModel("Walking", *model, "Character");
library.SavePoseFromModel("Attacking", *model, "Combat");
```

**Apply Pose:**
```cpp
const SDFPose* pose = library.GetPose("Idle");
if (pose) {
    model->ApplyPose(pose->transforms);
}
```

**Blend Poses:**
```cpp
// Blend between two poses
auto blendedPose = library.BlendPoses("Idle", "Walking", 0.5f);
model->ApplyPose(blendedPose);
```

### Creating Animation Clips

```cpp
// Create clip
auto walkClip = std::make_shared<SDFAnimationClip>("Walk");
walkClip->SetDuration(1.2f);
walkClip->SetLooping(true);
walkClip->SetFrameRate(30.0f);

// Add keyframes at specific times
auto* kf0 = walkClip->AddKeyframe(0.0f);
kf0->transforms["LeftLeg"] = leftLegForward;
kf0->transforms["RightLeg"] = rightLegBack;
kf0->easing = "ease_in_out";

auto* kf1 = walkClip->AddKeyframe(0.6f);
kf1->transforms["LeftLeg"] = leftLegBack;
kf1->transforms["RightLeg"] = rightLegForward;
kf1->easing = "ease_in_out";

// Save clip
SDFSerializer::SaveAnimationClip(*walkClip, "assets/animations/walk.anim.json");
```

### State Machine

```cpp
// Create state machine
auto stateMachine = std::make_unique<SDFAnimationStateMachine>();

// Add states
stateMachine->AddState("Idle", idleClip);
stateMachine->AddState("Walk", walkClip);
stateMachine->AddState("Attack", attackClip);

// Add transitions
auto* transition = stateMachine->AddTransition("Idle", "Walk", 0.2f);
transition->condition = "speed > 0.1";

// Use in controller
SDFAnimationController controller;
controller.SetStateMachine(stateMachine);
controller.Start();

// Update
controller.Update(deltaTime);
controller.ApplyToModel(*model);
```

---

## Rendering Pipeline

### Raymarching Process

1. **Ray Generation** - Create rays from camera through screen pixels
2. **Ray Marching** - Step along ray, evaluating SDF at each point
3. **Hit Detection** - Stop when distance < threshold
4. **Normal Calculation** - Compute surface normal using gradient
5. **Lighting** - Apply PBR or Phong shading
6. **Shadows** - Cast shadow rays to light
7. **Ambient Occlusion** - Sample surrounding points
8. **Final Color** - Combine all lighting terms

### Shader Configuration

**Quality Presets:**

```cpp
// High Quality
SDFRenderSettings highQuality;
highQuality.maxSteps = 256;
highQuality.enableShadows = true;
highQuality.enableAO = true;
highQuality.shadowSteps = 64;
highQuality.aoSteps = 8;

// Performance
SDFRenderSettings performance;
performance.maxSteps = 64;
performance.enableShadows = false;
performance.enableAO = false;
performance.shadowSteps = 16;
performance.aoSteps = 3;

renderer.SetSettings(highQuality);
```

### Lighting Setup

```cpp
SDFRenderSettings settings;

// Directional light
settings.lightDirection = glm::vec3(0.5f, -1.0f, 0.5f);
settings.lightColor = glm::vec3(1.0f, 0.95f, 0.9f);
settings.lightIntensity = 1.2f;

// Shadows
settings.shadowSoftness = 8.0f;  // Softer shadows

// Ambient occlusion
settings.aoIntensity = 0.5f;
settings.aoDistance = 0.3f;

renderer.SetSettings(settings);
```

---

## Editor Integration

### SDFModelEditor Usage

**Initialization:**
```cpp
SDFModelEditor editor;
editor.Initialize(mainEditor);
```

**Main Functions:**
- **Hierarchy Panel** - Tree view of primitives
- **Inspector Panel** - Edit selected primitive properties
- **Timeline Panel** - Animation keyframe editor
- **Pose Library** - Save and apply poses
- **Toolbar** - Quick tool access
- **3D Viewport** - Live preview with gizmos

**Keyboard Shortcuts:**
- `Ctrl+N` - New model
- `Ctrl+S` - Save model
- `Ctrl+O` - Open model
- `W` - Translate gizmo
- `E` - Rotate gizmo
- `R` - Scale gizmo
- `Del` - Delete selected
- `Ctrl+D` - Duplicate selected

### Creating Models in Editor

1. **Start New Model** - File → New Model
2. **Add Primitives** - Right-click in Hierarchy → Add Primitive
3. **Edit Properties** - Select primitive → Use Inspector
4. **Position with Gizmos** - Select primitive → Use transform tools
5. **Test Animation** - Timeline → Add Keyframes → Play
6. **Export** - File → Export to Entity JSON

---

## Performance Considerations

### Optimization Tips

**1. Primitive Count:**
- Keep hierarchy shallow when possible
- Combine primitives using CSG instead of many separate pieces
- Target: < 50 primitives per model for real-time rendering

**2. Render Settings:**
- Lower `maxSteps` for distant objects
- Disable shadows/AO for background objects
- Use LOD system to switch between detailed/simple versions

**3. Mesh Generation:**
- Use lower resolution for preview (32-64)
- Use higher resolution for final export (128-256)
- Cache generated meshes when not animating

**4. Animation:**
- Limit keyframe count
- Use pose blending instead of full keyframe evaluation
- Batch animation updates per frame

### LOD Strategy

```cpp
// Create LOD versions
SDFMeshSettings highDetail;
highDetail.resolution = 128;

SDFMeshSettings mediumDetail;
mediumDetail.resolution = 64;

SDFMeshSettings lowDetail;
lowDetail.resolution = 32;

// Generate LOD meshes
auto lod0 = model->GenerateMesh(highDetail);
auto lod1 = model->GenerateMesh(mediumDetail);
auto lod2 = model->GenerateMesh(lowDetail);

// Use based on distance
float distance = glm::distance(camera.GetPosition(), model WorldPos);
if (distance < 10.0f) {
    // Render with SDF raymarching
    sdfRenderer.Render(*model, camera);
} else if (distance < 50.0f) {
    // Use medium LOD mesh
    renderer.DrawMesh(*lod1);
} else {
    // Use low LOD mesh
    renderer.DrawMesh(*lod2);
}
```

---

## Examples and Best Practices

### Example 1: Simple Unit

See `game/assets/sdf_models/example_soldier_unit.json` for complete example.

**Key Points:**
- Use capsules for limbs
- Smooth union for organic connections
- Separate materials per body part
- Keep hierarchy organized

### Example 2: Building

See `game/assets/sdf_models/example_tower_building.json` for complete example.

**Key Points:**
- Use cylinders/boxes for structure
- Subtraction for windows/doors
- Torus for decorative elements
- Emissive materials for glowing windows

### Example 3: Visual Effects

See `game/assets/sdf_models/example_magic_effect.json` for complete example.

**Key Points:**
- Use sphere as core
- Multiple torus rings
- High emissive values
- Animated rotation for spinning effect

### Best Practices

**DO:**
- ✅ Plan hierarchy before building
- ✅ Use descriptive names
- ✅ Test early and often
- ✅ Save multiple versions
- ✅ Use smooth operations for organic shapes
- ✅ Keep materials consistent

**DON'T:**
- ❌ Create overly deep hierarchies (> 5 levels)
- ❌ Use too many primitives (> 100)
- ❌ Forget to set smoothness for smooth CSG
- ❌ Make all primitives visible=true (use hierarchy visibility)
- ❌ Use subtraction without understanding hollowing

---

## Troubleshooting

### Common Issues

**1. Model not rendering:**
- Check primitive visibility flags
- Verify root primitive exists
- Ensure shader compiled successfully

**2. Holes or artifacts:**
- Increase `maxSteps` in render settings
- Decrease `hitThreshold`
- Check CSG operations are correct

**3. Performance issues:**
- Reduce primitive count
- Lower resolution
- Disable shadows/AO
- Use mesh fallback for distant objects

**4. Animation not playing:**
- Verify keyframes are sorted by time
- Check animation controller is updated
- Ensure model is set in controller

---

## API Reference

### Key Classes

- `SDFModel` - Model container and hierarchy
- `SDFPrimitive` - Individual geometric primitive
- `SDFAnimation` - Animation system
- `SDFRenderer` - GPU raymarching renderer
- `SDFModelEditor` - In-editor model editor
- `SDFSerializer` - JSON import/export

### Full Documentation

See header files for complete API documentation:
- `engine/sdf/SDFModel.hpp`
- `engine/sdf/SDFPrimitive.hpp`
- `engine/sdf/SDFAnimation.hpp`
- `engine/graphics/SDFRenderer.hpp`

---

## Further Reading

- [Inigo Quilez SDF Functions](https://iquilezles.org/www/articles/distfunctions/distfunctions.htm)
- [Raymarching Tutorial](https://michaelwalczyk.com/blog-ray-marching.html)
- [CSG Operations](https://en.wikipedia.org/wiki/Constructive_solid_geometry)
- [PBR Shading](https://learnopengl.com/PBR/Theory)

---

**Version:** 1.0
**Last Updated:** 2025-12-03
**Engine:** Nova Engine (Old3DEngine)
