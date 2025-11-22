# API Reference

This document provides an overview of the Nova3D Engine API, including core namespaces, key classes, common patterns, and usage examples.

## Namespaces

### `Nova`

The primary namespace containing all engine systems.

```cpp
namespace Nova {
    class Engine;           // Core engine singleton
    class Window;           // Window management
    class Time;             // Timing utilities
    class Renderer;         // Graphics rendering
    class InputManager;     // Input handling
    class Scene;            // Scene management
    class Animation;        // Animation clips
    class AnimationController;  // Animation playback
}
```

### `Nova::Reflect`

Runtime reflection and type information system.

```cpp
namespace Nova::Reflect {
    class TypeRegistry;     // Global type registry
    class TypeInfo;         // Type metadata
    class PropertyInfo;     // Property metadata
    class EventBus;         // Pub/sub event system
}
```

### `Nova::Events`

Event binding system for data-driven behaviors.

```cpp
namespace Nova::Events {
    struct EventBinding;        // Event-to-action binding
    struct EventCondition;      // Condition for triggering
    class EventBindingManager;  // Binding management
}
```

### `Nova::Scripting`

Python scripting integration.

```cpp
namespace Nova::Scripting {
    class PythonEngine;     // Python interpreter wrapper
    class ScriptContext;    // Game state access
    class EventDispatcher;  // Script event handling
}
```

### `Vehement`

Game-specific systems for the Vehement2 game.

```cpp
namespace Vehement {
    class Game;             // Main game class
    class World;            // Game world
    class EntityManager;    // Entity management
    class Editor;           // Level editor
}

namespace Vehement::Lifecycle {
    class LifecycleManager; // Object lifecycle
    class ObjectPool;       // Object pooling
    class TickScheduler;    // Update scheduling
}
```

## Core Classes

### Engine

The main engine singleton that orchestrates all subsystems.

```cpp
#include <engine/core/Engine.hpp>

// Get the singleton instance
auto& engine = Nova::Engine::Instance();

// Initialize with custom parameters
Nova::Engine::InitParams params;
params.configPath = "config/engine.json";
params.enableImGui = true;
params.enableDebugDraw = true;

if (!engine.Initialize(params)) {
    // Handle initialization failure
    return -1;
}

// Create application callbacks
Nova::Engine::ApplicationCallbacks callbacks;

callbacks.onStartup = []() -> bool {
    // Initialize game resources
    return true;
};

callbacks.onUpdate = [](float deltaTime) {
    // Game logic update
};

callbacks.onRender = []() {
    // Custom rendering
};

callbacks.onImGui = []() {
    // ImGui debug UI
};

callbacks.onShutdown = []() {
    // Cleanup resources
};

// Run the main loop
int exitCode = engine.Run(std::move(callbacks));

// Access subsystems
auto& window = engine.GetWindow();
auto& time = engine.GetTime();
auto& renderer = engine.GetRenderer();
auto& input = engine.GetInput();
```

### Window

Platform-independent window management.

```cpp
#include <engine/core/Window.hpp>

auto& window = engine.GetWindow();

// Get window size
int width = window.GetWidth();
int height = window.GetHeight();
float aspect = window.GetAspectRatio();

// Check window state
bool focused = window.IsFocused();
bool minimized = window.IsMinimized();
bool fullscreen = window.IsFullscreen();

// Toggle fullscreen
window.SetFullscreen(!fullscreen);

// Set window title
window.SetTitle("My Game");
```

### Time

High-precision timing system.

```cpp
#include <engine/core/Time.hpp>

auto& time = engine.GetTime();

// Get delta time (seconds since last frame)
float dt = time.GetDeltaTime();

// Get total elapsed time
float total = time.GetTotalTime();

// Get frame count
uint64_t frame = time.GetFrameCount();

// Get FPS
float fps = time.GetFPS();

// Set time scale (for slow-motion)
time.SetTimeScale(0.5f);
```

### InputManager

Unified input handling for keyboard, mouse, and gamepads.

```cpp
#include <engine/input/InputManager.hpp>

auto& input = engine.GetInput();

// Keyboard input
if (input.IsKeyPressed(Key::Space)) {
    // Space just pressed this frame
}

if (input.IsKeyDown(Key::W)) {
    // W is held down
}

if (input.IsKeyReleased(Key::Escape)) {
    // Escape just released
}

// Mouse input
glm::vec2 mousePos = input.GetMousePosition();
glm::vec2 mouseDelta = input.GetMouseDelta();
float scroll = input.GetMouseScrollDelta();

if (input.IsMouseButtonDown(MouseButton::Left)) {
    // Left mouse button held
}

// Cursor control
input.SetCursorMode(CursorMode::Disabled);  // FPS camera
input.SetCursorMode(CursorMode::Normal);    // Normal cursor
```

## Animation System

### Animation

Animation clip containing keyframe data.

```cpp
#include <engine/animation/Animation.hpp>

// Create animation
Nova::Animation walkAnim("Walk");
walkAnim.SetDuration(1.0f);
walkAnim.SetTicksPerSecond(30.0f);
walkAnim.SetLooping(true);

// Create animation channel for a bone
Nova::AnimationChannel channel;
channel.nodeName = "LeftLeg";
channel.interpolationMode = Nova::InterpolationMode::Linear;

// Add keyframes
Nova::Keyframe kf1;
kf1.time = 0.0f;
kf1.position = glm::vec3(0, 0, 0);
kf1.rotation = glm::quat(1, 0, 0, 0);
kf1.scale = glm::vec3(1);
channel.keyframes.push_back(kf1);

Nova::Keyframe kf2;
kf2.time = 0.5f;
kf2.position = glm::vec3(0, 1, 0);
channel.keyframes.push_back(kf2);

walkAnim.AddChannel(std::move(channel));

// Evaluate animation at a time
auto transforms = walkAnim.Evaluate(0.25f);
```

### AnimationController

Animation playback and blending.

```cpp
#include <engine/animation/AnimationController.hpp>

// Create controller
Nova::AnimationController controller;
controller.SetSkeleton(&skeleton);

// Add animations
controller.AddAnimation("idle", idleAnimation);
controller.AddAnimation("walk", walkAnimation);
controller.AddAnimation("run", runAnimation);

// Play with crossfade
controller.Play("idle");
controller.CrossFade("walk", 0.3f);  // 0.3s blend

// Update each frame
controller.Update(deltaTime);

// Get bone matrices for skinning
std::vector<glm::mat4> boneMatrices = controller.GetBoneMatrices();

// Control playback
controller.SetPlaybackSpeed(1.5f);
controller.Pause();
controller.Resume();
controller.Stop(0.2f);  // Blend out over 0.2s
```

### AnimationStateMachine

Transition-based animation control.

```cpp
Nova::AnimationStateMachine stateMachine(&controller);

// Add states
stateMachine.AddState("idle", "idle_anim", true);
stateMachine.AddState("walk", "walk_anim", true);
stateMachine.AddState("run", "run_anim", true);
stateMachine.AddState("jump", "jump_anim", false);

// Add transitions
stateMachine.AddTransition("idle", "walk",
    [&]() { return speed > 0.1f; }, 0.2f);
stateMachine.AddTransition("walk", "run",
    [&]() { return speed > 5.0f; }, 0.3f);
stateMachine.AddTransition("walk", "idle",
    [&]() { return speed < 0.1f; }, 0.2f);

// Any state can transition to jump
stateMachine.AddAnyStateTransition("jump",
    [&]() { return isJumping; }, 0.1f);

stateMachine.SetInitialState("idle");

// Update each frame
stateMachine.Update(deltaTime);
```

## Spatial Systems

### ISpatialIndex

Unified interface for spatial queries.

```cpp
#include <engine/spatial/SpatialIndex.hpp>

// Create a spatial index
auto spatialIndex = Nova::CreateSpatialIndex(
    Nova::SpatialIndexType::Octree,
    Nova::AABB(glm::vec3(-500), glm::vec3(500))
);

// Insert objects
spatialIndex->Insert(objectId, objectBounds, layer);

// Update object position
spatialIndex->Update(objectId, newBounds);

// Remove object
spatialIndex->Remove(objectId);

// Query AABB intersection
auto hits = spatialIndex->QueryAABB(queryBox);

// Query sphere
auto nearby = spatialIndex->QuerySphere(center, radius);

// Frustum culling
auto visible = spatialIndex->QueryFrustum(camera.GetFrustum());

// Ray casting
auto rayHits = spatialIndex->QueryRay(ray, maxDistance);

// K-nearest neighbors
auto nearest = spatialIndex->QueryKNearest(point, k);

// Callback-based query (avoids allocation)
spatialIndex->QueryAABB(queryBox, [](uint64_t id, const AABB& bounds) {
    // Process each hit
    return true;  // Continue searching
});
```

## Reflection System

### TypeRegistry

Runtime type registration and lookup.

```cpp
#include <engine/reflection/TypeRegistry.hpp>

auto& registry = Nova::Reflect::TypeRegistry::Instance();

// Register a type
registry.RegisterType<MyClass>("MyClass");

// Register derived type
registry.RegisterDerivedType<DerivedClass, BaseClass>("DerivedClass");

// Query type information
const auto* info = registry.GetType("MyClass");
if (info) {
    // Create instance
    void* instance = registry.CreateInstance("MyClass");

    // Get properties
    for (const auto& prop : info->properties) {
        std::cout << prop.name << ": " << prop.typeName << "\n";
    }
}

// Iterate all types
registry.ForEachType([](const Nova::Reflect::TypeInfo& info) {
    std::cout << info.name << "\n";
});
```

### Reflection Macros

Convenient macros for type registration.

```cpp
// Simple type registration
REFLECT_TYPE(MyComponent)

// With properties
REFLECT_TYPE_BEGIN(Enemy)
    REFLECT_PROPERTY(health, REFLECT_ATTR_EDITABLE REFLECT_ATTR_RANGE(0, 100))
    REFLECT_PROPERTY(damage, REFLECT_ATTR_EDITABLE)
    REFLECT_PROPERTY(name, REFLECT_ATTR_SERIALIZED)
    REFLECT_EVENT(OnDamaged)
REFLECT_TYPE_END()
```

### EventBus

Publish/subscribe event system.

```cpp
#include <engine/reflection/EventBus.hpp>

auto& bus = Nova::Reflect::EventBus::Instance();

// Subscribe to events
size_t subId = bus.Subscribe("OnDamage", [](Nova::Reflect::BusEvent& evt) {
    float damage = evt.GetDataOr<float>("damage", 0.0f);
    uint64_t targetId = evt.GetDataOr<uint64_t>("targetId", 0);
    // Handle damage event
});

// Subscribe with priority
bus.Subscribe("OnDamage", handler, Nova::Reflect::EventPriority::High);

// Subscribe with source filter
bus.SubscribeFiltered("OnDamage", "Enemy", enemyDamageHandler);

// Publish event
Nova::Reflect::BusEvent evt("OnDamage", "Player", playerId);
evt.SetData("damage", 50.0f);
evt.SetData("targetId", enemyId);
bus.Publish(evt);

// Queue delayed event
bus.QueueDelayed(evt, 1.0f);  // Fire after 1 second

// Process queued events (call each frame)
bus.ProcessQueue(deltaTime);

// Unsubscribe
bus.Unsubscribe(subId);

// RAII subscription
Nova::Reflect::EventSubscriptionGuard guard(
    bus.Subscribe("OnDamage", handler)
);
// Automatically unsubscribes when guard goes out of scope
```

## Scripting System

### PythonEngine

Embedded Python interpreter.

```cpp
#include <engine/scripting/PythonEngine.hpp>

auto& python = Nova::Scripting::PythonEngine::Instance();

// Initialize with configuration
Nova::Scripting::PythonEngineConfig config;
config.scriptPaths = {"scripts/", "game/scripts/"};
config.enableHotReload = true;
config.hotReloadCheckInterval = 1.0f;

if (!python.Initialize(config)) {
    // Handle initialization failure
}

// Execute script file
auto result = python.ExecuteFile("ai/zombie_ai.py");
if (!result) {
    std::cerr << "Script error: " << result.errorMessage << "\n";
}

// Execute inline Python
python.ExecuteString("print('Hello from Python!')");

// Call Python function
auto result = python.CallFunction("game_logic", "on_player_attack",
    playerId, targetId, damage);

// Get global variable
auto health = python.GetGlobal<int>("player", "health");
if (health) {
    std::cout << "Player health: " << *health << "\n";
}

// Set global variable
python.SetGlobal("player", "health", 100);

// Hot reload check (call each frame)
python.Update(deltaTime);

// Shutdown
python.Shutdown();
```

## Lifecycle System

### LifecycleManager

Object creation, destruction, and update management.

```cpp
#include <game/src/systems/lifecycle/LifecycleManager.hpp>

auto& manager = Vehement::Lifecycle::GetGlobalLifecycleManager();

// Register types
manager.RegisterType<Enemy>("Enemy");
manager.RegisterType<Projectile>("Projectile");

// Enable object pooling for frequently spawned objects
manager.EnablePooling<Projectile>(100);

// Create objects
auto handle = manager.Create<Enemy>({
    {"type", "zombie"},
    {"health", 100},
    {"position", {10.0f, 0.0f, 5.0f}}
});

// Create from config file
auto handle2 = manager.CreateFromFile("configs/units/boss.json");

// Access objects
if (auto* enemy = manager.GetAs<Enemy>(handle)) {
    enemy->TakeDamage(25);
}

// Destroy (deferred by default)
manager.Destroy(handle);

// Immediate destruction
manager.Destroy(handle, true);

// Update all objects (call each frame)
manager.Update(deltaTime);

// Process deferred actions
manager.ProcessDeferredActions();

// Send events
Vehement::Lifecycle::GameEvent evt(EventType::Damaged, handle);
evt.SetData("damage", 50.0f);
manager.SendEvent(targetHandle, evt);

// Broadcast to all
manager.BroadcastEvent(evt);
```

## Common Patterns

### Initialization Pattern

```cpp
// RAII-style engine usage
int main() {
    auto& engine = Nova::Engine::Instance();

    if (!engine.Initialize()) {
        return -1;
    }

    // Setup callbacks
    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onStartup = []() {
        // Load resources
        return true;
    };

    // Engine cleanup is automatic
    return engine.Run(std::move(callbacks));
}
```

### Observer Pattern (EventBus)

```cpp
class HealthComponent {
public:
    void TakeDamage(float amount) {
        health -= amount;

        // Publish event
        Nova::Reflect::BusEvent evt("OnHealthChanged", "HealthComponent");
        evt.SetData("health", health);
        evt.SetData("maxHealth", maxHealth);
        Nova::Reflect::EventBus::Instance().Publish(evt);
    }
};

// Subscriber
bus.Subscribe("OnHealthChanged", [&ui](Nova::Reflect::BusEvent& evt) {
    float health = evt.GetDataOr<float>("health", 0.0f);
    float maxHealth = evt.GetDataOr<float>("maxHealth", 100.0f);
    ui.UpdateHealthBar(health / maxHealth);
});
```

### Factory Pattern (TypeRegistry)

```cpp
// Registration
REFLECT_TYPE(Zombie)
REFLECT_TYPE(Skeleton)
REFLECT_TYPE(Ghost)

// Factory usage
std::unique_ptr<Enemy> CreateEnemy(const std::string& type) {
    auto& registry = Nova::Reflect::TypeRegistry::Instance();
    return registry.Create<Enemy>(type);
}

auto enemy = CreateEnemy("Zombie");
```

### Builder Pattern (EventBinding)

```cpp
Nova::Events::EventBinding binding;
binding
    .WithId("damage_flash")
    .WithName("Damage Flash Effect")
    .WithCondition(Nova::Events::EventCondition("OnDamaged")
        .WithPropertyCondition("health", CompareOp::LessThan, 50.0f))
    .WithPythonFunction("effects", "play_damage_flash")
    .WithCooldown(0.5f)
    .WithPriority(10);
```

## Error Handling

### Script Errors

```cpp
auto result = python.ExecuteFile("script.py");
if (!result) {
    std::cerr << "Script error: " << result.errorMessage << "\n";

    // Get detailed error
    const auto& error = python.GetLastError();
}

// Set error callback
python.SetErrorCallback([](const std::string& error, const std::string& traceback) {
    Logger::Error("Python: {}\n{}", error, traceback);
});
```

### Type-Safe Queries

```cpp
// Optional return for potentially missing data
auto health = python.GetGlobal<int>("player", "health");
if (health.has_value()) {
    // Use health value
}

// With default value
auto damage = evt.GetDataOr<float>("damage", 0.0f);
```

## Thread Safety

### GIL Management

```cpp
// Acquire GIL for Python operations from non-main thread
{
    Nova::Scripting::PythonEngine::GILGuard guard;
    python.ExecuteString("...");
}  // GIL released automatically
```

### Thread-Safe Events

```cpp
// EventBus is thread-safe
// Queue events from any thread
bus.QueueEvent(evt);

// Process on main thread
bus.ProcessQueue(deltaTime);
```

## See Also

- [Engine API](api/Engine.md) - Detailed engine core documentation
- [Animation API](api/Animation.md) - Full animation system reference
- [Spatial API](api/Spatial.md) - Spatial indexing details
- [Reflection API](api/Reflection.md) - Type system reference
- [Scripting API](api/Scripting.md) - Python integration details
