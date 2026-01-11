# Instance-Specific Property Editing System

A comprehensive system for editing per-instance properties with archetype inheritance, JSON persistence, and an intuitive UI.

## Overview

The Instance Property System allows you to:
- Define base configurations in **archetype JSON files**
- Create **instances** that inherit from archetypes
- **Override** specific properties per instance
- Add **custom data** unique to each instance
- **Auto-save** changes with debouncing
- View **read-only archetype properties** and **editable instance overrides** side-by-side

## Architecture

### Core Components

#### 1. InstanceData (`engine/scene/InstanceData.hpp`)
Stores per-instance data including:
- Archetype reference
- Transform (position, rotation, scale)
- Property overrides (nested JSON)
- Custom data (instance-specific fields)
- Dirty flag for auto-save

#### 2. InstanceManager (`engine/scene/InstanceManager.hpp`)
Manages instances and archetypes:
- Loads/saves instance JSON files
- Loads archetype configurations
- Merges instance overrides with base configs
- Tracks dirty instances for auto-save
- Maps instances to files: `assets/maps/{mapName}/instances/{instanceId}.json`

#### 3. PropertyEditor (`examples/PropertyEditor.hpp`)
UI components for property editing:
- Renders nested JSON as ImGui tree
- Type-specific widgets (DragFloat, ColorEdit, InputText, etc.)
- "Override" buttons for read-only properties
- "Reset to Default" buttons for overridden properties
- Search/filter functionality

#### 4. InstancePropertyEditor (`examples/InstancePropertyEditor.hpp`)
High-level editor integrating all components:
- Renders complete property panel UI
- Shows archetype properties (read-only)
- Shows instance overrides (editable)
- Shows custom data section
- Handles auto-save with 2-second debounce

## File Structure

```
Old3DEngine/
├── engine/
│   └── scene/
│       ├── InstanceData.hpp          # Instance data structure
│       ├── InstanceData.cpp          # Serialization implementation
│       ├── InstanceManager.hpp       # Instance management
│       └── InstanceManager.cpp       # Load/save/merge logic
│
├── examples/
│   ├── PropertyEditor.hpp            # UI property rendering
│   ├── PropertyEditor.cpp
│   ├── InstancePropertyEditor.hpp    # High-level property editor
│   ├── InstancePropertyEditor.cpp
│   └── InstanceSystem_IntegrationExample.cpp  # Integration guide
│
└── assets/
    ├── config/                       # Archetype definitions
    │   └── humans/
    │       ├── units/
    │       │   ├── footman.json
    │       │   └── archer.json
    │       └── buildings/
    │           └── barracks.json
    │
    └── maps/                         # Instance data per map
        └── test_map/
            └── instances/
                └── {instanceId}.json
```

## JSON File Formats

### Archetype JSON (`assets/config/humans/units/footman.json`)

```json
{
  "name": "Footman",
  "type": "unit",
  "race": "humans",
  "stats": {
    "health": 100,
    "damage": 10,
    "armor": 5,
    "moveSpeed": 3.5
  },
  "costs": {
    "gold": 50,
    "food": 1
  },
  "model": {
    "mesh": "models/humans/footman.obj",
    "texture": "textures/humans/footman_diffuse.png"
  }
}
```

### Instance JSON (`assets/maps/test_map/instances/{id}.json`)

```json
{
  "archetype": "humans.units.footman",
  "instanceId": "12345678-1234-1234-1234-123456789abc",
  "name": "Captain Marcus",
  "transform": {
    "position": [10.0, 0.0, 5.0],
    "rotation": [1.0, 0.0, 0.707, 0.0],
    "scale": [1.0, 1.0, 1.0]
  },
  "overrides": {
    "stats": {
      "health": 150,
      "damage": 15
    }
  },
  "customData": {
    "quest_giver": true,
    "dialog_id": "quest_001"
  }
}
```

## Usage

### 1. Initialize System

```cpp
// In StandaloneEditor::Initialize()
m_instanceManager = std::make_unique<Nova::InstanceManager>();
m_instanceManager->Initialize("assets/config/", "assets/maps/");

m_propertyEditor = std::make_unique<InstancePropertyEditor>();
m_propertyEditor->Initialize(m_instanceManager.get());
m_propertyEditor->SetAutoSaveEnabled(true);
m_propertyEditor->SetAutoSaveDelay(2.0f);
```

### 2. Create Instance from Archetype

```cpp
Nova::InstanceData instance = m_instanceManager->CreateInstance(
    "humans.units.footman",
    glm::vec3(10.0f, 0.0f, 5.0f)  // position
);

// Instance now has:
// - Unique instanceId
// - Reference to "humans.units.footman" archetype
// - Transform set to specified position
// - Empty overrides (inherits all from archetype)
```

### 3. Load Instance from File

```cpp
Nova::InstanceData instance = m_instanceManager->LoadInstance(
    "assets/maps/test_map/instances/12345678-1234-1234-1234-123456789abc.json"
);

// Automatically registers in memory
```

### 4. Get Effective Configuration

```cpp
// Get merged config (archetype + overrides)
nlohmann::json effectiveConfig = m_instanceManager->GetEffectiveConfig(instance);

// Access properties
int health = effectiveConfig["stats"]["health"];  // Returns 150 (overridden)
int gold = effectiveConfig["costs"]["gold"];       // Returns 50 (from archetype)
```

### 5. Override Properties

```cpp
// Programmatically override a property
instance.SetOverride("stats.health", 150);
instance.SetOverride("stats.damage", 15);

// Check if property is overridden
bool hasOverride = instance.HasOverride("stats.health");  // true

// Get override value
int health = instance.GetOverride("stats.health", 100);  // Returns 150
```

### 6. Custom Data

```cpp
// Add custom instance data
instance.SetCustomData("quest_giver", true);
instance.SetCustomData("dialog_id", std::string("quest_001"));

// Retrieve custom data
bool isQuestGiver = instance.GetCustomData("quest_giver", false);
std::string dialogId = instance.GetCustomData("dialog_id", std::string(""));
```

### 7. Render Property Editor UI

```cpp
// In RenderDetailsPanel()
if (m_selectedObjectIndex >= 0) {
    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    m_propertyEditor->RenderPanel(
        obj.instanceId,
        m_selectedObjectPosition,
        m_selectedObjectRotation,
        m_selectedObjectScale
    );
}
```

### 8. Save Instances

```cpp
// Save single instance
m_instanceManager->SaveInstanceToMap("test_map", instance);

// Save all dirty instances
int savedCount = m_instanceManager->SaveDirtyInstances("test_map");

// Or use property editor's SaveAll (recommended)
m_propertyEditor->SaveAll("test_map");
```

### 9. Auto-Save

```cpp
// In Update loop
m_propertyEditor->Update(deltaTime);
// Automatically saves dirty instances after 2 seconds of no changes
```

## UI Features

### Property Display

The Details panel shows three sections:

#### 1. **Archetype Properties** (Read-Only, Gray)
- Base configuration from archetype JSON
- Cannot be edited directly
- Shows "Override" button next to each property
- Clicking "Override" copies property to Instance Overrides

#### 2. **Instance Overrides** (Editable, White/Bold)
- Properties that differ from archetype
- Fully editable
- Shows "Reset to Default" button
- Changes trigger auto-save

#### 3. **Custom Data** (Editable, Purple)
- Instance-specific data not in archetype
- Add new properties on the fly
- Useful for quest data, AI parameters, etc.

### Transform Section

Always editable, shows:
- Position (X, Y, Z)
- Rotation (Euler angles in degrees)
- Scale (X, Y, Z)

### Property Types

Automatically detects and renders:
- **Boolean**: Checkbox
- **Integer**: DragInt
- **Float**: DragFloat
- **String**: InputText
- **Vec3**: DragFloat3
- **Vec4**: DragFloat4 (or ColorEdit4 for colors)
- **Object**: Nested tree node
- **Array**: Expandable list

### Search/Filter

- Search bar at top of panel
- Filters properties by name
- Case-insensitive
- Searches nested paths

## Best Practices

### 1. Archetype Design

**Do:**
- Define common properties in archetypes
- Use nested structure for organization
- Keep archetypes DRY (Don't Repeat Yourself)
- Use clear, hierarchical naming

**Don't:**
- Put instance-specific data in archetypes
- Duplicate data across similar archetypes
- Use flat structure for complex data

### 2. Instance Overrides

**Do:**
- Override only what's different
- Use meaningful override names
- Document why overrides exist (in custom data if needed)

**Don't:**
- Override everything (defeats purpose of archetypes)
- Create deeply nested override structures
- Override properties that should be archetype-level

### 3. Custom Data

**Do:**
- Use for truly instance-unique data
- Namespace custom properties (e.g., "quest_001_data")
- Document custom data structure

**Don't:**
- Put archetype data in custom data
- Use generic names like "data" or "value"
- Store large binary data

### 4. Performance

**Do:**
- Let auto-save handle persistence
- Load instances on map load, not on-demand
- Cache archetype configs (done automatically)

**Don't:**
- Save on every property change
- Reload archetypes repeatedly
- Load all instances at startup

## Advanced Features

### Nested Property Paths

Access nested properties using dot notation:

```cpp
instance.SetOverride("stats.combat.damage", 15);
instance.SetOverride("model.animations.idle", "idle_captain.anim");
```

### Batch Operations

```cpp
// Get all dirty instances
std::vector<std::string> dirtyIds = m_instanceManager->GetDirtyInstances();

// Check dirty count
int count = m_propertyEditor->GetDirtyCount();

// Force save all
m_propertyEditor->SaveAll(mapName);
```

### Property Reset

```cpp
// Remove single override
instance.RemoveOverride("stats.health");

// Clear all overrides
instance.ClearOverrides();
```

### Custom Widgets

Extend `PropertyEditor::RenderJsonValue()` to add custom widgets for specific types:

```cpp
// In PropertyEditor.cpp
if (value.is_object() && value.contains("r") && value.contains("g") && value.contains("b")) {
    // Render as color picker
    float color[3] = {value["r"], value["g"], value["b"]};
    if (ImGui::ColorEdit3(key, color)) {
        value["r"] = color[0];
        value["g"] = color[1];
        value["b"] = color[2];
        modified = true;
    }
}
```

## Troubleshooting

### Instances Not Saving

**Check:**
1. Auto-save enabled? `SetAutoSaveEnabled(true)`
2. Delay too long? `SetAutoSaveDelay(2.0f)`
3. Update() called? `m_propertyEditor->Update(deltaTime)`
4. Directory exists? Check `assets/maps/{mapName}/instances/`
5. Write permissions?

### Properties Not Overriding

**Check:**
1. Property path correct? Use exact path from archetype
2. Type matches? Override must match archetype type
3. Archetype loaded? Check `LoadArchetype()` succeeds
4. Case sensitivity? Keys are case-sensitive

### UI Not Showing

**Check:**
1. InstancePropertyEditor initialized? `Initialize(instanceManager)`
2. Valid instanceId? Check not empty
3. Instance registered? `RegisterInstance()`
4. Panel visible? Check ImGui::Begin() called

## Examples

See `InstanceSystem_IntegrationExample.cpp` for complete integration guide.

## API Reference

### InstanceData

```cpp
// Create
InstanceData instance("humans.units.footman");

// Serialize
nlohmann::json json = instance.ToJson();
InstanceData loaded = InstanceData::FromJson(json);

// File I/O
instance.SaveToFile("path/to/instance.json");
InstanceData loaded = InstanceData::LoadFromFile("path/to/instance.json");

// Overrides
instance.SetOverride("stats.health", 150);
int health = instance.GetOverride("stats.health", 100);
bool has = instance.HasOverride("stats.health");
instance.RemoveOverride("stats.health");
instance.ClearOverrides();

// Custom data
instance.SetCustomData("quest_giver", true);
bool isGiver = instance.GetCustomData("quest_giver", false);
```

### InstanceManager

```cpp
// Initialize
manager.Initialize("assets/config/", "assets/maps/");

// Create instance
InstanceData instance = manager.CreateInstance("humans.units.footman", position);

// Load/Save
InstanceData instance = manager.LoadInstance("path/to/instance.json");
manager.SaveInstance("path/to/instance.json", instance);
manager.SaveInstanceToMap("test_map", instance);

// Archetypes
nlohmann::json archetype = manager.LoadArchetype("humans.units.footman");
std::vector<std::string> archetypes = manager.ListArchetypes();

// Effective config
nlohmann::json config = manager.GetEffectiveConfig(instance);

// Registration
manager.RegisterInstance(instance);
manager.UnregisterInstance(instanceId);
InstanceData* ptr = manager.GetInstance(instanceId);

// Dirty tracking
manager.MarkDirty(instanceId);
bool dirty = manager.IsDirty(instanceId);
std::vector<std::string> dirtyIds = manager.GetDirtyInstances();
int saved = manager.SaveDirtyInstances("test_map");
```

### InstancePropertyEditor

```cpp
// Initialize
editor.Initialize(instanceManager);
editor.SetAutoSaveEnabled(true);
editor.SetAutoSaveDelay(2.0f);

// Render
editor.RenderPanel(instanceId, position, rotation, scale);

// Update
editor.Update(deltaTime);  // Call every frame

// Save
editor.SaveAll("test_map");

// Status
bool hasChanges = editor.HasUnsavedChanges();
int count = editor.GetDirtyCount();
```

## License

Part of the Old3DEngine project.
