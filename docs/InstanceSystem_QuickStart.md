# Instance Property System - Quick Start Guide

Get up and running with the instance property system in 5 minutes.

## Step 1: Add Files to Your Project

Copy these files to your project:

**Engine Files:**
- `engine/scene/InstanceData.hpp`
- `engine/scene/InstanceData.cpp`
- `engine/scene/InstanceManager.hpp`
- `engine/scene/InstanceManager.cpp`

**Editor Files:**
- `examples/PropertyEditor.hpp`
- `examples/PropertyEditor.cpp`
- `examples/InstancePropertyEditor.hpp`
- `examples/InstancePropertyEditor.cpp`

**Optional:**
- `examples/InstanceSystem_IntegrationExample.cpp` (reference only)

## Step 2: Update CMakeLists.txt

Add to your engine library:
```cmake
add_library(engine STATIC
    # ... existing sources ...
    engine/scene/InstanceData.cpp
    engine/scene/InstanceManager.cpp
)

target_link_libraries(engine PUBLIC
    nlohmann_json::nlohmann_json
)
```

Add to your editor executable:
```cmake
add_executable(StandaloneEditor
    # ... existing sources ...
    examples/PropertyEditor.cpp
    examples/InstancePropertyEditor.cpp
)
```

## Step 3: Create Archetype JSON

Create `assets/config/test/unit.json`:
```json
{
  "name": "Test Unit",
  "stats": {
    "health": 100,
    "damage": 10
  },
  "model": {
    "mesh": "test.obj",
    "scale": 1.0
  }
}
```

## Step 4: Add to StandaloneEditor.hpp

Add these includes at the top:
```cpp
#include "scene/InstanceManager.hpp"
#include "InstancePropertyEditor.hpp"
```

Add these members to the private section:
```cpp
private:
    std::unique_ptr<Nova::InstanceManager> m_instanceManager;
    std::unique_ptr<InstancePropertyEditor> m_propertyEditor;
    std::string m_currentMapName = "test_map";
```

## Step 5: Initialize in StandaloneEditor.cpp

In `Initialize()`:
```cpp
bool StandaloneEditor::Initialize() {
    // ... existing code ...

    // Initialize instance system
    m_instanceManager = std::make_unique<Nova::InstanceManager>();
    m_instanceManager->Initialize("assets/config/", "assets/maps/");

    m_propertyEditor = std::make_unique<InstancePropertyEditor>();
    m_propertyEditor->Initialize(m_instanceManager.get());

    return true;
}
```

In `Update()`:
```cpp
void StandaloneEditor::Update(float deltaTime) {
    // ... existing code ...

    // Update property editor (handles auto-save)
    if (m_propertyEditor) {
        m_propertyEditor->Update(deltaTime);
    }
}
```

## Step 6: Update RenderDetailsPanel()

Replace the existing implementation:
```cpp
void StandaloneEditor::RenderDetailsPanel() {
    ImGui::Begin("Details");

    if (m_selectedObjectIndex >= 0 &&
        m_selectedObjectIndex < static_cast<int>(m_sceneObjects.size())) {

        auto& obj = m_sceneObjects[m_selectedObjectIndex];

        // Create instance if needed
        if (obj.instanceId.empty() && !obj.archetypeId.empty()) {
            Nova::InstanceData instance =
                m_instanceManager->CreateInstance(obj.archetypeId, obj.position);
            obj.instanceId = instance.instanceId;
        }

        // Render property editor
        if (m_propertyEditor && !obj.instanceId.empty()) {
            m_propertyEditor->RenderPanel(
                obj.instanceId,
                m_selectedObjectPosition,
                m_selectedObjectRotation,
                m_selectedObjectScale
            );
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
    }

    ImGui::End();
}
```

## Step 7: Update SceneObject Structure

In `StandaloneEditor.hpp`, update the SceneObject struct:
```cpp
struct SceneObject {
    std::string name = "Object";
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec3 boundingBoxMin{-0.5f, -0.5f, -0.5f};
    glm::vec3 boundingBoxMax{0.5f, 0.5f, 0.5f};
    std::string instanceId;   // ADD THIS
    std::string archetypeId;  // ADD THIS
};
```

## Step 8: Test It!

1. Build your project
2. Run StandaloneEditor
3. Create a test object:
   ```cpp
   SceneObject obj;
   obj.name = "Test Object";
   obj.archetypeId = "test.unit";
   obj.position = glm::vec3(0, 0, 0);
   m_sceneObjects.push_back(obj);
   ```
4. Select the object in the viewport
5. View properties in the Details panel
6. Click "Override" next to a property
7. Edit the value
8. Watch auto-save trigger after 2 seconds

## Common Issues

### "nlohmann/json.hpp not found"

Add nlohmann_json dependency:
```cmake
find_package(nlohmann_json REQUIRED)
# or use FetchContent (see CMake_Integration.txt)
```

### "Instance not found"

Make sure you call `CreateInstance()` and set `obj.instanceId`:
```cpp
Nova::InstanceData instance = m_instanceManager->CreateInstance(
    "test.unit",
    glm::vec3(0, 0, 0)
);
obj.instanceId = instance.instanceId;
```

### Properties not saving

Check:
1. `Update(deltaTime)` is called every frame
2. Auto-save is enabled: `SetAutoSaveEnabled(true)`
3. Directory exists: `assets/maps/test_map/instances/`

### Archetype not loading

Verify:
1. JSON file exists at correct path
2. Use dot notation: `"test.unit"` → `assets/config/test/unit.json`
3. JSON is valid (check with online validator)

## Next Steps

- Read `InstancePropertySystem.md` for full documentation
- See `InstanceSystem_IntegrationExample.cpp` for advanced usage
- Create your own archetypes in `assets/config/`
- Add custom property widgets in `PropertyEditor.cpp`

## Example Workflow

1. **Create Archetype:**
   - Make `assets/config/orcs/units/grunt.json`
   - Define base stats: health, damage, etc.

2. **Place Instance:**
   ```cpp
   Nova::InstanceData grunt = m_instanceManager->CreateInstance(
       "orcs.units.grunt",
       glm::vec3(10, 0, 5)
   );
   ```

3. **Override Properties:**
   - Select unit in viewport
   - Click "Override" next to "stats.health"
   - Change from 120 to 200
   - Unit now has custom health

4. **Add Custom Data:**
   - Add property "boss_unit" = true
   - Add property "aggro_range" = 20.0
   - This data is unique to this instance

5. **Save & Load:**
   - Properties auto-save to `assets/maps/{mapName}/instances/{id}.json`
   - On map load, instances restore with overrides applied

## That's It!

You now have a fully functional instance property editing system with:
- Archetype inheritance
- Per-instance overrides
- Custom data support
- Auto-save with debouncing
- Beautiful UI with read-only/editable distinction

For more advanced features, see the full documentation.
