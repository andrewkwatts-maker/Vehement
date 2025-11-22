# UI System API

The UI system provides ImGui-based debug interfaces and editor panels for the Nova3D engine.

## Editor Class

**Header**: `game/src/editor/Editor.hpp`

Main editor application class.

### Mode Enum

```cpp
enum class Mode {
    Standalone,  // Editor runs independently
    Integrated   // Editor runs alongside game
};
```

### Initialization

#### Initialize

```cpp
bool Initialize(Nova::Engine& engine, const EditorConfig& config = {});
```

Initialize editor with engine reference.

#### InitializeStandalone

```cpp
bool InitializeStandalone(const EditorConfig& config = {});
```

Initialize in standalone mode with own window.

#### Shutdown

```cpp
void Shutdown();
```

### Lifecycle

```cpp
[[nodiscard]] bool IsInitialized() const noexcept;
int Run();  // Standalone mode main loop
void Update(float deltaTime);  // Integrated mode update
void Render();
void ProcessInput();
```

### Panel Access

```cpp
[[nodiscard]] ConfigEditor* GetConfigEditor();
[[nodiscard]] WorldView* GetWorldView();
[[nodiscard]] TileInspector* GetTileInspector();
[[nodiscard]] PCGPanel* GetPCGPanel();
[[nodiscard]] LocationCrafter* GetLocationCrafter();
[[nodiscard]] ScriptEditor* GetScriptEditor();
[[nodiscard]] AssetBrowser* GetAssetBrowser();
[[nodiscard]] Hierarchy* GetHierarchy();
[[nodiscard]] Inspector* GetInspector();
[[nodiscard]] Console* GetConsole();
[[nodiscard]] Toolbar* GetToolbar();
```

### Undo/Redo

```cpp
void ExecuteCommand(std::unique_ptr<EditorCommand> command);
void Undo();
void Redo();
[[nodiscard]] bool CanUndo() const;
[[nodiscard]] bool CanRedo() const;
void ClearHistory();
```

### Project Management

```cpp
bool NewProject(const std::string& path);
bool OpenProject(const std::string& path);
bool SaveProject();
bool SaveProjectAs(const std::string& path);
void CloseProject();
[[nodiscard]] bool HasOpenProject() const;
[[nodiscard]] bool HasUnsavedChanges() const;
void MarkDirty();
```

### Layout

```cpp
bool SaveLayout(const std::string& path);
bool LoadLayout(const std::string& path);
void ResetLayout();
```

### Keyboard Shortcuts

```cpp
void RegisterShortcut(const KeyboardShortcut& shortcut);
void UnregisterShortcut(const std::string& action);
[[nodiscard]] const std::vector<KeyboardShortcut>& GetShortcuts() const;
```

### Theme

```cpp
void ApplyTheme(const EditorTheme& theme);
[[nodiscard]] const EditorTheme& GetTheme() const;
```

### Panel Visibility

```cpp
void SetConfigEditorVisible(bool visible);
void SetWorldViewVisible(bool visible);
// ... more setters

[[nodiscard]] bool IsConfigEditorVisible() const;
[[nodiscard]] bool IsWorldViewVisible() const;
// ... more getters
```

### Callbacks

```cpp
std::function<void()> OnProjectNew;
std::function<void(const std::string&)> OnProjectOpen;
std::function<void()> OnProjectSave;
std::function<void()> OnProjectClose;
std::function<void()> OnPlay;
std::function<void()> OnPause;
std::function<void()> OnStop;
```

---

## EditorConfig Struct

```cpp
struct EditorConfig {
    std::string projectPath;
    std::string layoutPath = "config/editor_layout.json";
    bool showDemoWindow = false;
    bool enableVsync = true;
    int targetFPS = 60;
    float autosaveInterval = 300.0f;  // 5 minutes
    EditorTheme theme;
};
```

---

## EditorTheme Struct

```cpp
struct EditorTheme {
    glm::vec4 windowBg{0.1f, 0.1f, 0.12f, 1.0f};
    glm::vec4 titleBg{0.15f, 0.15f, 0.18f, 1.0f};
    glm::vec4 titleBgActive{0.2f, 0.2f, 0.25f, 1.0f};
    glm::vec4 frameBg{0.18f, 0.18f, 0.22f, 1.0f};
    glm::vec4 button{0.25f, 0.25f, 0.3f, 1.0f};
    glm::vec4 text{0.95f, 0.95f, 0.95f, 1.0f};
    glm::vec4 accent{0.4f, 0.6f, 1.0f, 1.0f};
    glm::vec4 success{0.2f, 0.8f, 0.3f, 1.0f};
    glm::vec4 warning{1.0f, 0.8f, 0.2f, 1.0f};
    glm::vec4 error{1.0f, 0.3f, 0.3f, 1.0f};
    float windowRounding = 4.0f;
    float frameRounding = 2.0f;
};
```

---

## KeyboardShortcut Struct

```cpp
struct KeyboardShortcut {
    int key = 0;
    int modifiers = 0;  // GLFW modifier bits
    std::string action;
    std::string description;
    std::function<void()> callback;
};
```

---

## EditorCommand Class

Base class for undo/redo commands.

```cpp
class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
};
```

### Example Command

```cpp
class MoveEntityCommand : public EditorCommand {
    uint64_t entityId;
    glm::vec3 oldPosition;
    glm::vec3 newPosition;

public:
    MoveEntityCommand(uint64_t id, const glm::vec3& from, const glm::vec3& to)
        : entityId(id), oldPosition(from), newPosition(to) {}

    void Execute() override {
        SetEntityPosition(entityId, newPosition);
    }

    void Undo() override {
        SetEntityPosition(entityId, oldPosition);
    }

    std::string GetDescription() const override {
        return "Move Entity";
    }
};

// Usage
editor.ExecuteCommand(std::make_unique<MoveEntityCommand>(
    entityId, oldPos, newPos));
```

---

## ImGui Integration

Nova3D uses Dear ImGui for debug UI and editor panels.

### Basic Usage

```cpp
// In onImGui callback
void RenderDebugUI() {
    ImGui::Begin("Debug");

    ImGui::Text("FPS: %.1f", engine.GetTime().GetFPS());
    ImGui::Text("Entities: %zu", entityManager.GetCount());

    if (ImGui::Button("Spawn Enemy")) {
        SpawnEnemy();
    }

    static float speed = 1.0f;
    ImGui::SliderFloat("Speed", &speed, 0.0f, 10.0f);

    ImGui::End();
}
```

### Custom Windows

```cpp
class MyPanel {
    bool visible = true;

public:
    void Render() {
        if (!visible) return;

        if (ImGui::Begin("My Panel", &visible)) {
            // Panel content
            ImGui::Text("Custom panel content");

            if (ImGui::TreeNode("Section")) {
                ImGui::Text("Section content");
                ImGui::TreePop();
            }
        }
        ImGui::End();
    }
};
```

### Property Editor Pattern

```cpp
void DrawPropertyEditor(void* object, const TypeInfo& typeInfo) {
    for (const auto& prop : typeInfo.properties) {
        if (prop.attributes & PropertyAttribute::Hidden) continue;

        ImGui::PushID(prop.name.c_str());

        // Get current value
        std::any value = prop.getterAny(object);

        // Draw appropriate editor based on type
        if (prop.typeName == "float") {
            float v = std::any_cast<float>(value);
            if (prop.minValue < prop.maxValue) {
                if (ImGui::SliderFloat(prop.displayName.c_str(), &v,
                                       prop.minValue, prop.maxValue)) {
                    prop.setterAny(object, v);
                }
            } else {
                if (ImGui::DragFloat(prop.displayName.c_str(), &v, 0.1f)) {
                    prop.setterAny(object, v);
                }
            }
        } else if (prop.typeName == "bool") {
            bool v = std::any_cast<bool>(value);
            if (ImGui::Checkbox(prop.displayName.c_str(), &v)) {
                prop.setterAny(object, v);
            }
        }
        // ... more types

        ImGui::PopID();
    }
}
```

---

## Debug Draw

**Header**: `engine/graphics/debug/DebugDraw.hpp`

Debug visualization utilities.

### Static Methods

```cpp
static void Line(const glm::vec3& start, const glm::vec3& end,
                 const glm::vec3& color = glm::vec3(1));

static void Sphere(const glm::vec3& center, float radius,
                   const glm::vec3& color = glm::vec3(1), int segments = 16);

static void Box(const glm::vec3& center, const glm::vec3& size,
                const glm::vec3& color = glm::vec3(1));

static void WireBox(const glm::vec3& center, const glm::vec3& size,
                    const glm::vec3& color = glm::vec3(1));

static void Circle(const glm::vec3& center, float radius,
                   const glm::vec3& normal, const glm::vec3& color = glm::vec3(1));

static void Arrow(const glm::vec3& start, const glm::vec3& end,
                  const glm::vec3& color = glm::vec3(1), float headSize = 0.1f);

static void Frustum(const glm::mat4& viewProjection,
                    const glm::vec3& color = glm::vec3(1));

static void Text(const glm::vec3& position, const std::string& text,
                 const glm::vec3& color = glm::vec3(1));

static void Clear();
static void Render(const glm::mat4& viewProjection);
```

### Usage Example

```cpp
void RenderDebug() {
    // Draw coordinate axes
    DebugDraw::Arrow(glm::vec3(0), glm::vec3(1, 0, 0), glm::vec3(1, 0, 0));
    DebugDraw::Arrow(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));
    DebugDraw::Arrow(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, 0, 1));

    // Draw entity bounds
    for (auto& entity : entities) {
        DebugDraw::WireBox(entity.GetPosition(), entity.GetSize(),
                           entity.IsSelected() ? glm::vec3(1, 1, 0) : glm::vec3(0, 1, 0));
    }

    // Draw path
    for (size_t i = 0; i < path.size() - 1; i++) {
        DebugDraw::Line(path[i], path[i + 1], glm::vec3(0, 0, 1));
        DebugDraw::Sphere(path[i], 0.1f, glm::vec3(0, 0, 1));
    }

    // Render all debug draws
    DebugDraw::Render(camera.GetViewProjectionMatrix());
    DebugDraw::Clear();
}
```

---

## Usage Examples

### Basic Editor Setup

```cpp
#include <game/src/editor/Editor.hpp>

int main() {
    Vehement::Editor editor;

    EditorConfig config;
    config.projectPath = "my_project";
    config.enableVsync = true;

    if (!editor.InitializeStandalone(config)) {
        return -1;
    }

    // Setup callbacks
    editor.OnProjectSave = []() {
        std::cout << "Project saved!\n";
    };

    return editor.Run();
}
```

### Integrated Mode

```cpp
// In game initialization
Vehement::Editor editor;
editor.Initialize(engine, config);
editor.SetGame(&game);
editor.SetWorld(&world);

// In game loop
void Update(float dt) {
    game.Update(dt);
    editor.Update(dt);
}

void Render() {
    game.Render();
    editor.Render();
}
```

### Custom Panel

```cpp
class CustomPanel {
    bool m_visible = true;
    std::string m_searchText;
    std::vector<Item> m_filteredItems;

public:
    void Render() {
        if (!m_visible) return;

        if (ImGui::Begin("Custom Panel", &m_visible, ImGuiWindowFlags_MenuBar)) {
            // Menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("Options")) {
                    ImGui::MenuItem("Option 1");
                    ImGui::MenuItem("Option 2");
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            // Search
            if (ImGui::InputText("Search", &m_searchText)) {
                FilterItems();
            }

            // Item list
            if (ImGui::BeginChild("Items", ImVec2(0, 0), true)) {
                for (const auto& item : m_filteredItems) {
                    if (ImGui::Selectable(item.name.c_str(), item.selected)) {
                        OnItemSelected(item);
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
};
```
