# Engine Core API

The engine core provides the fundamental systems for running Nova3D applications, including window management, timing, input handling, and resource loading.

## Engine Class

**Header**: `engine/core/Engine.hpp`

The main engine singleton that orchestrates all subsystems.

### Types

#### InitParams

```cpp
struct InitParams {
    std::string configPath = "config/engine.json";
    bool enableImGui = true;
    bool enableDebugDraw = true;
};
```

Configuration passed at initialization.

#### ApplicationCallbacks

```cpp
struct ApplicationCallbacks {
    std::function<bool()> onStartup;
    std::function<void(float deltaTime)> onUpdate;
    std::function<void()> onRender;
    std::function<void()> onImGui;
    std::function<void()> onShutdown;
};
```

User callbacks for application lifecycle events.

### Methods

#### Instance

```cpp
[[nodiscard]] static Engine& Instance() noexcept;
```

Get the singleton instance.

#### Initialize

```cpp
[[nodiscard]] bool Initialize(const InitParams& params = {});
```

Initialize the engine with given parameters. Returns true if successful.

#### Run

```cpp
[[nodiscard]] int Run(ApplicationCallbacks callbacks);
```

Run the main engine loop. Returns exit code (0 = success).

#### RequestShutdown

```cpp
void RequestShutdown() noexcept;
```

Request engine shutdown. The engine will exit at the end of the current frame.

#### IsRunning / IsInitialized

```cpp
[[nodiscard]] bool IsRunning() const noexcept;
[[nodiscard]] bool IsInitialized() const noexcept;
```

Check engine state.

#### Subsystem Access

```cpp
[[nodiscard]] Window& GetWindow() noexcept;
[[nodiscard]] Time& GetTime() noexcept;
[[nodiscard]] Renderer& GetRenderer() noexcept;
[[nodiscard]] InputManager& GetInput() noexcept;
[[nodiscard]] Scene* GetActiveScene() noexcept;
```

Access engine subsystems. These assume the engine is initialized.

#### Scene Management

```cpp
void SetActiveScene(std::unique_ptr<Scene> scene);
```

Set the currently active scene.

#### Engine Info

```cpp
[[nodiscard]] static consteval std::string_view GetVersion() noexcept;
[[nodiscard]] static consteval std::string_view GetName() noexcept;
```

Get engine version ("1.0.0") and name ("Nova3D").

### Example

```cpp
#include <engine/core/Engine.hpp>

int main() {
    auto& engine = Nova::Engine::Instance();

    Nova::Engine::InitParams params;
    params.enableImGui = true;

    if (!engine.Initialize(params)) {
        return -1;
    }

    Nova::Engine::ApplicationCallbacks callbacks;
    callbacks.onUpdate = [](float dt) {
        // Game logic
    };
    callbacks.onRender = []() {
        // Rendering
    };

    return engine.Run(std::move(callbacks));
}
```

---

## Window Class

**Header**: `engine/core/Window.hpp`

Platform-independent window management.

### Methods

#### GetWidth / GetHeight

```cpp
[[nodiscard]] int GetWidth() const noexcept;
[[nodiscard]] int GetHeight() const noexcept;
```

Get window dimensions in pixels.

#### GetAspectRatio

```cpp
[[nodiscard]] float GetAspectRatio() const noexcept;
```

Get window aspect ratio (width / height).

#### IsFocused / IsMinimized

```cpp
[[nodiscard]] bool IsFocused() const noexcept;
[[nodiscard]] bool IsMinimized() const noexcept;
```

Check window state.

#### IsFullscreen / SetFullscreen

```cpp
[[nodiscard]] bool IsFullscreen() const noexcept;
void SetFullscreen(bool fullscreen);
```

Fullscreen mode control.

#### SetTitle

```cpp
void SetTitle(const std::string& title);
```

Set window title.

---

## Time Class

**Header**: `engine/core/Time.hpp`

High-precision timing system.

### Methods

#### GetDeltaTime

```cpp
[[nodiscard]] float GetDeltaTime() const noexcept;
```

Get time since last frame in seconds.

#### GetTotalTime

```cpp
[[nodiscard]] float GetTotalTime() const noexcept;
```

Get total elapsed time since engine start.

#### GetFrameCount

```cpp
[[nodiscard]] uint64_t GetFrameCount() const noexcept;
```

Get total number of frames processed.

#### GetFPS

```cpp
[[nodiscard]] float GetFPS() const noexcept;
```

Get current frames per second.

#### GetTimeScale / SetTimeScale

```cpp
[[nodiscard]] float GetTimeScale() const noexcept;
void SetTimeScale(float scale);
```

Control time scaling for slow-motion effects.

---

## InputManager Class

**Header**: `engine/input/InputManager.hpp`

Unified input handling for keyboard, mouse, and gamepads.

### Keyboard Methods

#### IsKeyPressed

```cpp
[[nodiscard]] bool IsKeyPressed(Key key) const;
```

Returns true if key was pressed this frame.

#### IsKeyDown

```cpp
[[nodiscard]] bool IsKeyDown(Key key) const;
```

Returns true if key is currently held down.

#### IsKeyReleased

```cpp
[[nodiscard]] bool IsKeyReleased(Key key) const;
```

Returns true if key was released this frame.

### Mouse Methods

#### GetMousePosition

```cpp
[[nodiscard]] glm::vec2 GetMousePosition() const;
```

Get mouse position in screen coordinates.

#### GetMouseDelta

```cpp
[[nodiscard]] glm::vec2 GetMouseDelta() const;
```

Get mouse movement since last frame.

#### GetMouseScrollDelta

```cpp
[[nodiscard]] float GetMouseScrollDelta() const;
```

Get scroll wheel delta.

#### IsMouseButtonDown

```cpp
[[nodiscard]] bool IsMouseButtonDown(MouseButton button) const;
```

Check if mouse button is held.

#### SetCursorMode

```cpp
void SetCursorMode(CursorMode mode);
```

Set cursor visibility and capture mode.

### Key Enumeration

```cpp
enum class Key {
    // Letters
    A, B, C, ... Z,
    // Numbers
    Num0, Num1, ... Num9,
    // Function keys
    F1, F2, ... F12,
    // Special keys
    Space, Enter, Escape, Tab, Backspace,
    Left, Right, Up, Down,
    LeftShift, RightShift, LeftCtrl, RightCtrl,
    LeftAlt, RightAlt,
    // ...
};
```

### MouseButton Enumeration

```cpp
enum class MouseButton {
    Left,
    Right,
    Middle,
    Button4,
    Button5
};
```

### CursorMode Enumeration

```cpp
enum class CursorMode {
    Normal,    // Visible, free movement
    Hidden,    // Hidden but free movement
    Disabled   // Hidden and captured (FPS camera)
};
```

---

## Logger Class

**Header**: `engine/core/Logger.hpp`

Logging system with multiple severity levels.

### Static Methods

```cpp
static void Info(const std::string& message);
static void Warn(const std::string& message);
static void Error(const std::string& message);
static void Debug(const std::string& message);

// Format string versions
template<typename... Args>
static void Info(const std::string& fmt, Args&&... args);
```

### Usage

```cpp
Logger::Info("Starting engine...");
Logger::Warn("Config file not found, using defaults");
Logger::Error("Failed to load shader: {}", shaderPath);
Logger::Debug("Frame time: {}ms", frameTime * 1000);
```

---

## JobSystem Class

**Header**: `engine/core/JobSystem.hpp`

Thread pool with work stealing for parallel task execution.

### Methods

#### Initialize

```cpp
bool Initialize(size_t numThreads = 0);
```

Initialize with specified threads (0 = auto-detect).

#### Shutdown

```cpp
void Shutdown();
```

Stop all workers and wait for completion.

#### Submit

```cpp
template<typename F>
JobHandle Submit(F&& task);
```

Submit a task for execution.

#### Wait

```cpp
void Wait(JobHandle handle);
```

Block until job completes.

#### ParallelFor

```cpp
template<typename F>
void ParallelFor(size_t count, F&& task);
```

Execute task for indices 0 to count-1 in parallel.

### Example

```cpp
auto& jobs = JobSystem::Instance();

// Submit individual tasks
auto handle = jobs.Submit([]() {
    // Work
});
jobs.Wait(handle);

// Parallel loop
jobs.ParallelFor(1000, [&](size_t i) {
    ProcessItem(items[i]);
});
```

---

## Profiler Class

**Header**: `engine/core/Profiler.hpp`

Performance profiling utilities.

### Macros

```cpp
PROFILE_SCOPE("ScopeName")     // Profile a scope
PROFILE_FUNCTION()             // Profile current function
```

### Methods

```cpp
static void BeginFrame();
static void EndFrame();
static void BeginSample(const char* name);
static void EndSample();
static ProfileData GetData();
```

### Usage

```cpp
void Update(float dt) {
    PROFILE_FUNCTION();

    {
        PROFILE_SCOPE("Physics");
        UpdatePhysics(dt);
    }

    {
        PROFILE_SCOPE("AI");
        UpdateAI(dt);
    }
}
```

---

## Resource Loading

### ConfigLoader

**Header**: `engine/config/Config.hpp`

```cpp
template<typename T>
static T Load(const std::string& path);

template<typename T>
static std::vector<T> LoadAll(const std::string& directory);

static void Save(const std::string& path, const json& config);

static void Watch(const std::string& path, std::function<void(const std::string&)> callback);
```

### FileSystem

**Header**: `engine/utils/FileSystem.hpp`

```cpp
static std::string ReadText(const std::string& path);
static std::vector<uint8_t> ReadBinary(const std::string& path);
static bool WriteText(const std::string& path, const std::string& content);
static bool WriteBinary(const std::string& path, const std::vector<uint8_t>& data);
static bool Exists(const std::string& path);
static std::vector<std::string> ListFiles(const std::string& directory, const std::string& extension = "");
```

---

## Platform Abstraction

### Platform Detection

```cpp
#if defined(NOVA_PLATFORM_WINDOWS)
    // Windows code
#elif defined(NOVA_PLATFORM_MACOS)
    // macOS code
#elif defined(NOVA_PLATFORM_LINUX)
    // Linux code
#elif defined(NOVA_PLATFORM_ANDROID)
    // Android code
#elif defined(NOVA_PLATFORM_IOS)
    // iOS code
#endif
```

### Platform-Specific Headers

| Platform | Header |
|----------|--------|
| Windows | `engine/platform/windows/WindowsPlatform.hpp` |
| macOS | `engine/platform/macos/MacOSPlatform.hpp` |
| Linux | `engine/platform/linux/LinuxPlatform.hpp` |
| Android | `engine/platform/android/AndroidPlatform.hpp` |
| iOS | `engine/platform/ios/IOSPlatform.hpp` |
