# Instance Property System - Architecture Overview

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        StandaloneEditor                              │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │                    RenderDetailsPanel()                         │ │
│  │  - Displays selected object properties                          │ │
│  │  - Integrates InstancePropertyEditor                            │ │
│  └────────────────────┬───────────────────────────────────────────┘ │
│                       │                                              │
│                       │ uses                                         │
│                       ▼                                              │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │            InstancePropertyEditor                               │ │
│  │  ┌──────────────────────────────────────────────────────────┐  │ │
│  │  │ RenderPanel()                                             │  │ │
│  │  │ • Archetype Properties (read-only)                        │  │ │
│  │  │ • Instance Overrides (editable)                           │  │ │
│  │  │ • Custom Data                                             │  │ │
│  │  │ • Transform                                               │  │ │
│  │  └──────────────────────────────────────────────────────────┘  │ │
│  │                                                                  │ │
│  │  Auto-Save Logic:                                               │ │
│  │  - Tracks last change time                                      │ │
│  │  - Debounces saves (2 second delay)                             │ │
│  │  - Calls InstanceManager::SaveDirtyInstances()                  │ │
│  └──────────────────────┬──────────────────────────────────────────┘ │
│                         │                                            │
└─────────────────────────┼────────────────────────────────────────────┘
                          │ uses
                          ▼
         ┌────────────────────────────────────────┐
         │        InstanceManager                  │
         │                                         │
         │  ┌───────────────────────────────────┐ │
         │  │ Core Responsibilities:            │ │
         │  │ • Load/save instances             │ │
         │  │ • Load archetypes                 │ │
         │  │ • Merge overrides                 │ │
         │  │ • Track dirty instances           │ │
         │  │ • Generate instance IDs           │ │
         │  └───────────────────────────────────┘ │
         │                                         │
         │  In-Memory Storage:                     │
         │  • m_instances: map<id, InstanceData>   │
         │  • m_archetypeCache: map<id, json>      │
         │  • m_dirtyInstances: set<id>            │
         └──────────┬─────────────┬────────────────┘
                    │             │
        creates     │             │ loads/saves
        manages     │             │
                    ▼             ▼
         ┌──────────────┐   ┌─────────────────┐
         │ InstanceData │   │  File System    │
         │              │   │                 │
         │ • archetype  │   │ Archetypes:     │
         │ • instanceId │   │ assets/config/  │
         │ • transform  │   │   └── *.json    │
         │ • overrides  │   │                 │
         │ • customData │   │ Instances:      │
         │ • isDirty    │   │ assets/maps/    │
         └──────────────┘   │   └── {map}/    │
                            │       └── inst/ │
                            │           └─*.json
                            └─────────────────┘
```

## Data Flow

### 1. Loading an Instance

```
User Opens Map
      │
      ▼
StandaloneEditor::LoadMap()
      │
      ├─> InstanceManager::LoadMapInstances("test_map")
      │        │
      │        ├─> Read assets/maps/test_map/instances/*.json
      │        │
      │        ├─> For each file:
      │        │     └─> InstanceData::FromJson(json)
      │        │
      │        └─> RegisterInstance() for each
      │
      └─> Create SceneObjects from InstanceData
            │
            └─> m_sceneObjects.push_back(obj)
```

### 2. Displaying Properties

```
User Selects Object
      │
      ▼
StandaloneEditor::RenderDetailsPanel()
      │
      ├─> Get selected SceneObject
      │     └─> instanceId = obj.instanceId
      │
      └─> InstancePropertyEditor::RenderPanel(instanceId, ...)
            │
            ├─> InstanceManager::GetInstance(instanceId)
            │     └─> Returns InstanceData*
            │
            ├─> InstanceManager::LoadArchetype(archetypeId)
            │     └─> Returns JSON archetype config
            │
            ├─> RenderArchetypeProperties(archetype, instance)
            │     └─> PropertyEditor::RenderPropertyTree(json, READ_ONLY)
            │           └─> Shows gray, read-only properties
            │
            ├─> RenderInstanceOverrides(instance)
            │     └─> PropertyEditor::RenderPropertyTree(overrides, EDITABLE)
            │           └─> Shows white, editable properties
            │
            └─> RenderCustomData(instance)
                  └─> PropertyEditor::RenderPropertyTree(customData, EDITABLE)
```

### 3. Editing Properties

```
User Clicks "Override" Button
      │
      ▼
PropertyEditor::RenderOverrideButton() returns true
      │
      ▼
InstanceData::SetOverride("stats.health", 150)
      │
      ├─> overrides["stats"]["health"] = 150
      │
      └─> isDirty = true
            │
            ▼
InstancePropertyEditor::MarkDirty()
      │
      ├─> m_hasPendingChanges = true
      │
      ├─> m_lastChangeTime = now()
      │
      └─> InstanceManager::MarkDirty(instanceId)
            └─> m_dirtyInstances.insert(instanceId)
```

### 4. Auto-Save

```
Every Frame: Update(deltaTime)
      │
      ▼
InstancePropertyEditor::Update(deltaTime)
      │
      ├─> Check: ShouldAutoSave()?
      │     └─> hasPendingChanges && (now - lastChange) >= 2.0s
      │
      └─> If yes:
            │
            └─> InstanceManager::SaveDirtyInstances("test_map")
                  │
                  ├─> For each dirty instance ID:
                  │     │
                  │     ├─> Get instance from m_instances
                  │     │
                  │     ├─> json = instance.ToJson()
                  │     │
                  │     ├─> path = "assets/maps/test_map/instances/{id}.json"
                  │     │
                  │     └─> Save json to file
                  │
                  └─> Clear m_dirtyInstances
```

## Component Relationships

```
┌─────────────────────────────────────────────────────────────┐
│                       Editor Layer                           │
│  ┌─────────────────────────────────────────────────────┐    │
│  │         InstancePropertyEditor                       │    │
│  │         (High-level UI integration)                  │    │
│  └─────────────┬───────────────────────────────────────┘    │
│                │ uses                                        │
│  ┌─────────────▼───────────────────────────────────────┐    │
│  │         PropertyEditor                               │    │
│  │         (Low-level widget rendering)                 │    │
│  └──────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                │ uses
┌───────────────▼─────────────────────────────────────────────┐
│                       Engine Layer                           │
│  ┌─────────────────────────────────────────────────────┐    │
│  │         InstanceManager                              │    │
│  │         (Business logic & persistence)               │    │
│  └─────────────┬───────────────────────────────────────┘    │
│                │ manages                                     │
│  ┌─────────────▼───────────────────────────────────────┐    │
│  │         InstanceData                                 │    │
│  │         (Data structure & serialization)             │    │
│  └──────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

## State Machine: Property Editing

```
┌─────────────┐
│  ARCHETYPE  │  (Base property from archetype)
│  PROPERTY   │
└──────┬──────┘
       │
       │ User clicks "Override"
       │
       ▼
┌─────────────┐
│  OVERRIDDEN │  (Instance-specific value)
│  PROPERTY   │
└──────┬──────┘
       │
       │ User edits value
       │
       ▼
┌─────────────┐
│    DIRTY    │  (Needs saving)
│  PROPERTY   │
└──────┬──────┘
       │
       │ Auto-save triggers (2s delay)
       │
       ▼
┌─────────────┐
│    SAVED    │  (Written to disk)
│  PROPERTY   │
└──────┬──────┘
       │
       │ User clicks "Reset to Default"
       │
       ▼
┌─────────────┐
│  ARCHETYPE  │  (Back to base value)
│  PROPERTY   │
└─────────────┘
```

## JSON Structure Hierarchy

```
Archetype JSON (assets/config/humans/units/footman.json)
│
├── name: "Footman"
├── type: "unit"
├── stats:
│   ├── health: 100
│   ├── damage: 10
│   └── armor: 5
├── costs:
│   ├── gold: 50
│   └── food: 1
└── model:
    ├── mesh: "..."
    └── scale: 1.0

Instance JSON (assets/maps/test_map/instances/{id}.json)
│
├── archetype: "humans.units.footman"  ───┐ (references archetype)
├── instanceId: "abc-123-..."             │
├── name: "Captain Marcus"                │
├── transform:                            │
│   ├── position: [10, 0, 5]              │
│   ├── rotation: [1, 0, 0.707, 0]        │
│   └── scale: [1, 1, 1]                  │
├── overrides:                            │ (overrides archetype)
│   └── stats:                            │
│       ├── health: 150  ─────────────────┼─── overrides 100
│       └── damage: 15   ─────────────────┼─── overrides 10
└── customData:                           │
    ├── quest_giver: true                 │ (instance-unique)
    └── dialog_id: "quest_001"            │

Effective Configuration (merged at runtime)
│
├── name: "Captain Marcus"         (instance name)
├── type: "unit"                   (from archetype)
├── stats:
│   ├── health: 150                (overridden)
│   ├── damage: 15                 (overridden)
│   └── armor: 5                   (from archetype)
├── costs:
│   ├── gold: 50                   (from archetype)
│   └── food: 1                    (from archetype)
├── model:
│   ├── mesh: "..."                (from archetype)
│   └── scale: 1.0                 (from archetype)
└── customData:
    ├── quest_giver: true          (instance custom)
    └── dialog_id: "quest_001"     (instance custom)
```

## Memory Management

```
┌────────────────────────────────────────────────────┐
│              InstanceManager                       │
│                                                    │
│  m_instances:                                      │
│  ┌────────────────────────────────────────────┐   │
│  │  "abc-123" -> InstanceData { ... }         │   │
│  │  "def-456" -> InstanceData { ... }         │   │
│  │  "ghi-789" -> InstanceData { ... }         │   │
│  └────────────────────────────────────────────┘   │
│                                                    │
│  m_archetypeCache:                                 │
│  ┌────────────────────────────────────────────┐   │
│  │  "humans.units.footman" -> JSON { ... }    │   │
│  │  "humans.units.archer"  -> JSON { ... }    │   │
│  └────────────────────────────────────────────┘   │
│                                                    │
│  m_dirtyInstances:                                 │
│  ┌────────────────────────────────────────────┐   │
│  │  { "abc-123", "ghi-789" }                  │   │
│  └────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────┘
```

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Load Instance | O(1) | Direct file read + JSON parse |
| Get Instance | O(1) | HashMap lookup |
| Load Archetype | O(1) | Cached after first load |
| Apply Overrides | O(n) | Where n = number of override keys |
| Save Instance | O(1) | Direct file write |
| Mark Dirty | O(1) | Set insertion |
| Auto-Save | O(m) | Where m = number of dirty instances |

## Thread Safety

**Current Implementation:**
- **Not thread-safe** - designed for single-threaded editor use

**To Make Thread-Safe:**
1. Add mutex to InstanceManager
2. Lock in all public methods
3. Use atomic for dirty flags
4. Consider read-write locks for read-heavy operations

## Extension Points

### Custom Property Widgets

Extend `PropertyEditor::RenderJsonValue()`:

```cpp
// Add custom widget for color properties
if (key.find("color") != std::string::npos && value.is_array()) {
    // Render as color picker
}
```

### Custom Serialization

Override `InstanceData::ToJson()` / `FromJson()`:

```cpp
// Add binary data support
json["binary_data"] = base64_encode(binaryData);
```

### Custom Merge Logic

Override `InstanceManager::ApplyOverrides()`:

```cpp
// Add special handling for arrays
if (value.is_array()) {
    // Append instead of replace
}
```

## Future Enhancements

1. **Undo/Redo System**
   - Track property change history
   - Store before/after states
   - Integrate with command pattern

2. **Multi-Instance Editing**
   - Select multiple objects
   - Edit properties in batch
   - Show "mixed values" for differences

3. **Property Templates**
   - Save common override sets
   - Apply to multiple instances
   - Share between maps

4. **Diff Visualization**
   - Highlight what changed
   - Show history of overrides
   - Compare instances

5. **Hot Reload**
   - Watch archetype files
   - Reload on change
   - Update instances automatically

6. **Validation**
   - Type checking
   - Range validation
   - Required fields
   - Custom validators

## Conclusion

The Instance Property System provides a robust, flexible architecture for managing per-instance properties with archetype inheritance. The separation of concerns between data (InstanceData), management (InstanceManager), and presentation (PropertyEditor) makes it easy to extend and maintain.
