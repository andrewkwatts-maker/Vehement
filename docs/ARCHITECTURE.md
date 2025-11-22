# Nova3D Engine Architecture

This document provides an overview of the Nova3D engine architecture, including the module structure, data flow, threading model, and memory management strategies.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Application Layer                               │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │    Game     │  │    Editor    │  │   Examples   │  │   User Code     │  │
│  └──────┬──────┘  └──────┬───────┘  └──────┬───────┘  └────────┬────────┘  │
└─────────┼────────────────┼─────────────────┼───────────────────┼───────────┘
          │                │                 │                   │
          ▼                ▼                 ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Game Systems Layer                              │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │  Lifecycle  │  │  Scripting   │  │    Events    │  │  Config-Driven  │  │
│  │  Manager    │  │  (Python)    │  │   Binding    │  │    Entities     │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
          │                │                 │                   │
          ▼                ▼                 ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Engine Core Layer                               │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │   Engine    │  │   Renderer   │  │   Animation  │  │    Physics      │  │
│  │  (Singleton)│  │              │  │              │  │                 │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘  │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │    Scene    │  │   Spatial    │  │  Pathfinding │  │    Particles    │  │
│  │    Graph    │  │   Indexing   │  │              │  │                 │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
          │                │                 │                   │
          ▼                ▼                 ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                             Subsystems Layer                                 │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │   Window    │  │    Input     │  │     Time     │  │   Job System    │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘  │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │   Logger    │  │   Profiler   │  │   Reflection │  │    EventBus     │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
          │                │                 │                   │
          ▼                ▼                 ▼                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Platform Layer                                    │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │   Windows   │  │    macOS     │  │    Linux     │  │   Android/iOS   │  │
│  └─────────────┘  └──────────────┘  └──────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Module Descriptions

### Core Modules

#### Engine (`engine/core/Engine.hpp`)

The central orchestrator for all engine subsystems. Implemented as a singleton with:

- **Initialization**: Configures and starts all subsystems
- **Main Loop**: Manages frame timing, input processing, updates, and rendering
- **Shutdown**: Orderly cleanup of all resources

```cpp
// Singleton access
auto& engine = Nova::Engine::Instance();

// Lifecycle
engine.Initialize(params);
engine.Run(callbacks);
engine.RequestShutdown();
```

#### Window (`engine/core/Window.hpp`)

Platform-independent window management via GLFW:

- Window creation and destruction
- Event handling (resize, focus, close)
- OpenGL context management
- Fullscreen and windowed modes

#### Time (`engine/core/Time.hpp`)

High-precision timing system:

- Delta time calculation
- Fixed timestep for physics
- Frame rate limiting
- Time scaling for slow-motion effects

#### Job System (`engine/core/JobSystem.hpp`)

Thread pool with work stealing for parallel task execution:

- Lock-free job queues
- Dependency-aware scheduling
- Worker thread management
- Parallel-for utilities

### Graphics Modules

#### Renderer (`engine/graphics/Renderer.hpp`)

OpenGL 4.6 rendering system:

- Forward rendering pipeline
- Render queue with sorting
- Frustum culling
- LOD management
- Debug visualization

#### Shader System (`engine/graphics/Shader.hpp`)

GLSL shader management:

- Shader compilation and linking
- Uniform management
- Shader hot-reloading
- Preprocessor support

#### Materials (`engine/graphics/Material.hpp`)

PBR material system:

- Texture binding
- Uniform buffer objects
- Material instancing
- Render state management

### Animation System

#### Animation (`engine/animation/Animation.hpp`)

Keyframe animation system:

- Position, rotation, scale keyframes
- Multiple interpolation modes (Linear, Cubic, CatmullRom)
- Animation channels per bone/node
- Keyframe optimization utilities

#### AnimationController (`engine/animation/AnimationController.hpp`)

Animation playback and blending:

- Multiple concurrent animations
- Crossfade transitions
- Animation layers
- Blend masks

#### Skeleton (`engine/animation/Skeleton.hpp`)

Skeletal hierarchy management:

- Bone hierarchy
- Inverse bind matrices
- Skinning transforms

### Spatial Systems

#### Spatial Index (`engine/spatial/SpatialIndex.hpp`)

Unified interface for spatial acceleration structures:

```cpp
// Available implementations
enum class SpatialIndexType {
    Octree,       // Good for static scenes
    LooseOctree,  // Better for dynamic objects
    BVH,          // Optimal for ray queries
    SpatialHash   // Best for uniform distributions
};
```

Query types:
- AABB intersection
- Sphere intersection
- Frustum culling
- Ray casting
- K-nearest neighbors

### Reflection System

#### TypeRegistry (`engine/reflection/TypeRegistry.hpp`)

Runtime type information:

```cpp
// Registration
REFLECT_TYPE(MyClass)
REFLECT_TYPE_BEGIN(MyClass)
    REFLECT_PROPERTY(health, REFLECT_ATTR_EDITABLE REFLECT_ATTR_RANGE(0, 100))
    REFLECT_PROPERTY(name, REFLECT_ATTR_SERIALIZED)
REFLECT_TYPE_END()

// Usage
auto* info = TypeRegistry::Instance().GetType("MyClass");
auto instance = info->factory();
```

#### EventBus (`engine/reflection/EventBus.hpp`)

Publish/subscribe event system:

- Priority-based handler ordering
- Event filtering
- Async event queue
- Event history for debugging

### Scripting System

#### PythonEngine (`engine/scripting/PythonEngine.hpp`)

Embedded Python interpreter:

- Script execution (file and string)
- Function calling with type conversion
- Hot-reload support
- Sandboxed execution

### Event Binding System

#### EventBinding (`engine/events/EventBinding.hpp`)

Data-driven event handlers:

- JSON-configurable event responses
- Python, native, or event callbacks
- Conditions with property watching
- Execution controls (cooldown, max executions)

## Data Flow

### Frame Update Flow

```
┌─────────────┐
│ Begin Frame │
└──────┬──────┘
       │
       ▼
┌──────────────┐    ┌───────────────┐    ┌──────────────┐
│ Process Input│───►│ Update Events │───►│ Update Scripts│
└──────────────┘    └───────────────┘    └──────────────┘
       │                                         │
       ▼                                         ▼
┌──────────────┐    ┌───────────────┐    ┌──────────────┐
│ Update Physics│◄──│ Update Objects │◄──│ Update AI    │
└──────────────┘    └───────────────┘    └──────────────┘
       │
       ▼
┌──────────────┐    ┌───────────────┐    ┌──────────────┐
│ Cull Scene   │───►│ Sort Render Q │───►│ Submit Draws │
└──────────────┘    └───────────────┘    └──────────────┘
       │
       ▼
┌──────────────┐    ┌───────────────┐
│ Render ImGui │───►│  Swap Buffers │
└──────────────┘    └───────────────┘
```

### Event Flow

```
┌─────────────────┐
│   Event Source  │
│ (Game, System)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐     ┌───────────────────┐
│    EventBus     │────►│ Subscription Check │
└─────────────────┘     └─────────┬─────────┘
                                  │
         ┌────────────────────────┼────────────────────────┐
         │                        │                        │
         ▼                        ▼                        ▼
┌────────────────┐    ┌───────────────────┐    ┌──────────────────┐
│ Native Handler │    │ Python Handler    │    │ Event Emission   │
└────────────────┘    └───────────────────┘    └──────────────────┘
```

## Threading Model

### Thread Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Main Thread                               │
│  - Window management                                             │
│  - Input processing                                              │
│  - OpenGL rendering                                              │
│  - ImGui                                                         │
│  - Script execution (GIL)                                        │
└─────────────────────────────────────────────────────────────────┘
         │
         │ Job dispatch
         ▼
┌─────────────────────────────────────────────────────────────────┐
│                       Job System Workers                         │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐    │
│  │ Worker 0  │  │ Worker 1  │  │ Worker 2  │  │ Worker N  │    │
│  └───────────┘  └───────────┘  └───────────┘  └───────────┘    │
│  - Physics calculations                                          │
│  - Pathfinding                                                   │
│  - Animation blending                                            │
│  - Spatial queries                                               │
└─────────────────────────────────────────────────────────────────┘
```

### Thread Safety Guidelines

1. **Main Thread Only**:
   - OpenGL calls
   - Window operations
   - ImGui rendering
   - Python script execution

2. **Thread-Safe Systems**:
   - EventBus (with mutex)
   - TypeRegistry (read-write lock)
   - Logger (lock-free queue)
   - Job System

3. **Per-Frame Sync Points**:
   - Job completion before render
   - Event processing on main thread

## Memory Management

### Allocation Strategies

#### Object Pools

```cpp
// Efficient allocation for frequently created/destroyed objects
template<typename T>
class ObjectPool {
    T* Acquire();          // Get object (may be recycled)
    void Release(T* obj);  // Return to pool
};
```

#### Smart Pointers

- `std::unique_ptr` for exclusive ownership
- `std::shared_ptr` for shared resources (textures, shaders)
- Raw pointers for non-owning references

#### Cache-Friendly Data

```cpp
// Structure of Arrays for hot data
namespace SoA {
    struct Transforms {
        std::vector<glm::vec3> positions;
        std::vector<glm::quat> rotations;
        std::vector<glm::vec3> scales;
    };
}
```

### Resource Lifecycle

```
Load Request ──► Check Cache ──► Cache Hit ──► Return Handle
                     │
                     ▼ Cache Miss
               Load from Disk
                     │
                     ▼
               Parse/Compile
                     │
                     ▼
               Add to Cache ──► Return Handle
                     │
              Reference Count
                     │
                     ▼ (ref count = 0)
               Unload/Release
```

## Platform Abstraction

### Platform Detection

```cpp
// Compile-time platform detection
#if defined(NOVA_PLATFORM_WINDOWS)
    // Windows-specific code
#elif defined(NOVA_PLATFORM_MACOS)
    // macOS-specific code
#elif defined(NOVA_PLATFORM_LINUX)
    // Linux-specific code
#elif defined(NOVA_PLATFORM_ANDROID)
    // Android-specific code
#elif defined(NOVA_PLATFORM_IOS)
    // iOS-specific code
#endif
```

### Platform Layer

| Platform | Graphics | Input | Audio |
|----------|----------|-------|-------|
| Windows | OpenGL 4.6 | GLFW | (planned) |
| macOS | OpenGL 4.1 | GLFW | (planned) |
| Linux | OpenGL 4.6 | GLFW | (planned) |
| Android | OpenGL ES 3.0 / Vulkan | Touch | (planned) |
| iOS | Metal | Touch | (planned) |

### Mobile Considerations

- Touch input abstraction
- GPS location services
- Background execution handling
- Battery-aware performance scaling

## Configuration System

### JSON-Based Configuration

```
config/
├── engine.json         # Engine settings
├── graphics_config.json # Graphics options
└── editor_layout.json  # Editor window layout

game/assets/
├── configs/
│   ├── units/          # Unit definitions
│   ├── spells/         # Spell definitions
│   └── buildings/      # Building definitions
└── schemas/            # JSON validation schemas
```

### Schema Validation

All game configurations are validated against JSON schemas:

- `unit.schema.json`
- `spell.schema.json`
- `ability.schema.json`
- `building.schema.json`
- etc.

## Extension Points

### Adding New Systems

1. Create header/source in appropriate module
2. Add to CMakeLists.txt
3. Initialize in Engine (if needed)
4. Document in architecture

### Adding Platform Support

1. Create platform directory (`engine/platform/newplatform/`)
2. Implement platform-specific files
3. Update CMakeLists.txt with platform detection
4. Add to documentation

### Adding New Reflection Types

```cpp
// In your class header
class MyComponent {
    REFLECT_TYPE_BEGIN(MyComponent)
        REFLECT_PROPERTY(value, REFLECT_ATTR_EDITABLE)
    REFLECT_TYPE_END()

    float value = 0.0f;
};
```

## Performance Considerations

### Rendering Optimizations

- **Frustum Culling**: Early rejection of off-screen objects
- **LOD System**: Distance-based detail reduction
- **Batching**: Combine similar draw calls
- **Instancing**: Hardware instanced rendering

### Memory Optimizations

- **Object Pooling**: Reduce allocation overhead
- **Deferred Destruction**: Batch destroys at frame end
- **Cache-Line Alignment**: `alignas(64)` for hot data

### CPU Optimizations

- **SIMD**: `engine/core/SIMD.hpp` for vectorized math
- **Parallel Jobs**: Multi-threaded workloads
- **Spatial Hashing**: O(1) neighbor queries
