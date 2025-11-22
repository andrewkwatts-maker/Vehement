# Scripting API

The scripting system provides embedded Python support for game logic, AI behaviors, and runtime customization with hot-reload capability.

## PythonEngine Class

**Header**: `engine/scripting/PythonEngine.hpp`

Core Python interpreter wrapper.

### Instance

```cpp
[[nodiscard]] static PythonEngine& Instance();
```

Get singleton instance.

### Lifecycle

#### Initialize

```cpp
[[nodiscard]] bool Initialize(const PythonEngineConfig& config = {});
```

Initialize Python interpreter with configuration.

#### Shutdown

```cpp
void Shutdown();
```

Shutdown interpreter and cleanup.

#### IsInitialized

```cpp
[[nodiscard]] bool IsInitialized() const noexcept;
```

#### Update

```cpp
void Update(float deltaTime);
```

Update engine for hot-reload checks. Call each frame.

### Script Execution

#### ExecuteFile

```cpp
[[nodiscard]] ScriptResult ExecuteFile(const std::string& filePath);
```

Execute a Python script file.

#### ExecuteString

```cpp
[[nodiscard]] ScriptResult ExecuteString(const std::string& code,
                                         const std::string& name = "<string>");
```

Execute Python code from a string.

#### ImportModule

```cpp
[[nodiscard]] bool ImportModule(const std::string& moduleName);
```

Import a Python module.

#### ReloadModule

```cpp
[[nodiscard]] bool ReloadModule(const std::string& moduleName);
```

Reload a previously loaded module.

### Function Calling

#### CallFunction

```cpp
template<typename... Args>
[[nodiscard]] ScriptResult CallFunction(const std::string& moduleName,
                                        const std::string& functionName,
                                        Args&&... args);
```

Call Python function with variadic arguments.

#### CallFunctionV

```cpp
[[nodiscard]] ScriptResult CallFunctionV(
    const std::string& moduleName,
    const std::string& functionName,
    const std::vector<std::variant<bool, int, float, double, std::string>>& args);
```

Call Python function with vector of arguments.

#### CallMethod

```cpp
[[nodiscard]] ScriptResult CallMethod(
    const std::string& objectName,
    const std::string& methodName,
    const std::vector<std::variant<bool, int, float, double, std::string>>& args = {});
```

Call method on a Python object.

### Variable Access

#### GetGlobal

```cpp
template<typename T>
[[nodiscard]] std::optional<T> GetGlobal(const std::string& moduleName,
                                          const std::string& varName);
```

Get global variable from module.

#### SetGlobal

```cpp
template<typename T>
bool SetGlobal(const std::string& moduleName,
               const std::string& varName,
               const T& value);
```

Set global variable in module.

### Caching and Hot-Reload

```cpp
[[nodiscard]] bool PreloadScript(const std::string& filePath);
void ClearCache();
void CheckHotReload();
[[nodiscard]] std::vector<std::string> GetCachedScripts() const;
[[nodiscard]] bool IsScriptModified(const std::string& filePath) const;
```

### Error Handling

```cpp
[[nodiscard]] const std::string& GetLastError() const noexcept;
void ClearError();

using ErrorCallback = std::function<void(const std::string& error, const std::string& traceback)>;
void SetErrorCallback(ErrorCallback callback);
```

### Thread Safety

#### AcquireGIL / ReleaseGIL

```cpp
void AcquireGIL();
void ReleaseGIL();
```

Manual GIL management.

#### GILGuard

```cpp
class GILGuard {
public:
    GILGuard();
    ~GILGuard();
};
```

RAII wrapper for GIL.

### Metrics

```cpp
[[nodiscard]] const ScriptMetrics& GetMetrics() const noexcept;
void ResetMetrics();
[[nodiscard]] std::string GetPythonVersion() const;
[[nodiscard]] std::vector<std::string> GetLoadedModules() const;
```

### Context Access

```cpp
[[nodiscard]] ScriptContext* GetContext();
[[nodiscard]] EventDispatcher* GetEventDispatcher();
```

---

## PythonEngineConfig Struct

Configuration for engine initialization.

```cpp
struct PythonEngineConfig {
    std::vector<std::string> scriptPaths;     // Script search paths
    std::string mainModuleName = "nova_game"; // Main module name
    bool enableHotReload = true;              // Enable script hot-reloading
    float hotReloadCheckInterval = 1.0f;      // Seconds between checks
    bool enableSandbox = true;                // Enable sandbox restrictions
    size_t maxExecutionTimeMs = 100;          // Max script execution time
    size_t maxMemoryMB = 256;                 // Max memory for scripts
    bool verboseErrors = true;                // Detailed error messages
};
```

---

## ScriptResult Struct

Result of script execution.

```cpp
struct ScriptResult {
    bool success = false;
    std::string errorMessage;
    std::variant<std::monostate, bool, int, float, double, std::string> returnValue;

    operator bool() const;

    template<typename T>
    std::optional<T> GetValue() const;
};
```

---

## ScriptMetrics Struct

Performance metrics.

```cpp
struct ScriptMetrics {
    size_t totalExecutions = 0;
    size_t failedExecutions = 0;
    double totalExecutionTimeMs = 0.0;
    double avgExecutionTimeMs = 0.0;
    double maxExecutionTimeMs = 0.0;
    size_t hotReloads = 0;
    std::chrono::system_clock::time_point lastExecution;

    void RecordExecution(double timeMs, bool success);
    void Reset();
};
```

---

## Python Game API

Available modules and functions in Python scripts.

### entities Module

```python
from nova import entities

entity = entities.get(handle)           # Get by handle
entity = entities.find("Player")        # Find by name
enemies = entities.find_all("Enemy")    # Find all of type
entity = entities.create("Zombie", {...}) # Create entity
entities.destroy(handle)                # Destroy entity
exists = entities.exists(handle)        # Check existence
```

### components Module

```python
from nova import components

health = components.get(entity, "HealthComponent")
components.add(entity, "Effect", {...})
components.remove(entity, "Effect")
has = components.has(entity, "HealthComponent")
```

### transform Module

```python
from nova import transform

pos = transform.get_position(entity)
transform.set_position(entity, [x, y, z])
rot = transform.get_rotation(entity)
transform.set_rotation_euler(entity, [rx, ry, rz])
forward = transform.get_forward(entity)
transform.look_at(entity, target_pos)
transform.translate(entity, [dx, dy, dz])
transform.rotate(entity, [rx, ry, rz])
```

### input Module

```python
from nova import input

pressed = input.is_key_pressed("space")
down = input.is_key_down("w")
released = input.is_key_released("escape")
mouse_pos = input.get_mouse_position()
mouse_delta = input.get_mouse_delta()
scroll = input.get_scroll_delta()
button_down = input.is_mouse_button_down(0)
world_pos = input.screen_to_world(mouse_pos)
```

### events Module

```python
from nova import events

events.subscribe("OnDamage", handler)
events.subscribe("OnDamage", handler, priority=100)
events.publish("OnDamage", {"damage": 50, "target": entity_id})
events.unsubscribe("OnDamage", handler)
events.queue("OnExplosion", data, delay=2.0)
```

### time Module

```python
from nova import time

dt = time.delta_time()
total = time.total_time()
frame = time.frame_count()
time.set_scale(0.5)
scale = time.get_scale()
```

### math Module

```python
from nova import math

length = math.length(vec)
normalized = math.normalize(vec)
dot = math.dot(v1, v2)
cross = math.cross(v1, v2)
lerped = math.lerp(a, b, t)
dist = math.distance(p1, p2)
rand = math.random()
rand = math.random_range(min, max)
point = math.random_point_in_sphere(radius)
```

### debug Module

```python
from nova import debug

debug.log("Info message")
debug.warn("Warning")
debug.error("Error")
debug.draw_line(start, end, color)
debug.draw_sphere(center, radius, color)
debug.draw_box(center, size, color)
debug.draw_text_3d("Text", position)
debug.draw_text_2d("Text", screen_pos)
```

---

## Usage Examples

### Basic Setup

```cpp
auto& python = PythonEngine::Instance();

PythonEngineConfig config;
config.scriptPaths = {"scripts/", "game/scripts/"};
config.enableHotReload = true;

if (!python.Initialize(config)) {
    Logger::Error("Failed to initialize Python");
    return false;
}
```

### Execute Scripts

```cpp
// Execute file
auto result = python.ExecuteFile("ai/enemy_ai.py");
if (!result) {
    Logger::Error("Script error: {}", result.errorMessage);
}

// Execute string
python.ExecuteString(R"(
    def hello(name):
        return f"Hello, {name}!"
)");
```

### Call Functions

```cpp
// Call with arguments
auto result = python.CallFunction("game", "on_damage",
    targetId, damage, std::string("fire"));

if (result) {
    if (auto killed = result.GetValue<bool>()) {
        if (*killed) {
            HandleDeath(targetId);
        }
    }
}

// Call with vector args
std::vector<std::variant<bool, int, float, double, std::string>> args = {
    42, 3.14, std::string("test")
};
auto result = python.CallFunctionV("module", "function", args);
```

### Access Variables

```cpp
// Get variable
auto health = python.GetGlobal<int>("player", "health");
if (health) {
    std::cout << "Health: " << *health << "\n";
}

// Set variable
python.SetGlobal("player", "health", 100);
```

### Hot Reload

```cpp
// In game loop
void Update(float dt) {
    python.Update(dt);  // Checks for file changes
}

// Manual reload
if (python.IsScriptModified("ai/enemy_ai.py")) {
    python.ReloadModule("ai.enemy_ai");
}
```

### Error Handling

```cpp
python.SetErrorCallback([](const std::string& error, const std::string& traceback) {
    Logger::Error("Python Error: {}", error);
    Logger::Debug("Traceback:\n{}", traceback);
});

auto result = python.ExecuteFile("buggy_script.py");
if (!result) {
    // Error callback was already called
    // result.errorMessage contains error details
}
```

### Thread Safety

```cpp
// From non-main thread
void WorkerThread() {
    PythonEngine::GILGuard guard;  // Acquire GIL

    auto result = python.ExecuteString("compute_something()");

    // GIL released when guard goes out of scope
}
```

### Script Example (Python)

```python
# scripts/ai/enemy_ai.py

from nova import entities, transform, events, time, math

class EnemyAI:
    def __init__(self, entity_id):
        self.entity = entity_id
        self.target = None
        self.attack_cooldown = 0

    def update(self, dt):
        self.attack_cooldown = max(0, self.attack_cooldown - dt)

        if not self.target:
            self.target = self.find_target()
            return

        self.move_toward_target(dt)

        if self.in_attack_range():
            self.attack()

    def find_target(self):
        players = entities.find_all("Player")
        if not players:
            return None

        my_pos = transform.get_position(self.entity)
        return min(players, key=lambda p:
            math.distance(my_pos, transform.get_position(p)))

    def attack(self):
        if self.attack_cooldown > 0:
            return

        events.publish("OnAttack", {
            "attacker": self.entity,
            "target": self.target,
            "damage": 10
        })
        self.attack_cooldown = 1.0

# Global AI instances
_ais = {}

def on_spawn(entity_id):
    _ais[entity_id] = EnemyAI(entity_id)

def on_update(entity_id, dt):
    if entity_id in _ais:
        _ais[entity_id].update(dt)

def on_destroy(entity_id):
    _ais.pop(entity_id, None)
```
