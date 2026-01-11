# StandaloneEditor Menu Reference Guide

Quick reference for all menu items and their actions.

## File Menu

| Menu Item | Shortcut | Action | Status |
|-----------|----------|--------|--------|
| New → World Map | Ctrl+Shift+N | Creates a new spherical world map | ✅ Implemented |
| New → Local Map | Ctrl+N | Shows new local map dialog | ✅ Implemented |
| Open Map | Ctrl+O | Shows load map dialog | ✅ Implemented |
| Save Map | Ctrl+S | Saves map to current path, or shows save dialog if no path | ✅ Implemented |
| Save Map As | Ctrl+Shift+S | Shows save map dialog | ✅ Implemented |
| Import → Heightmap... | - | Opens file dialog and imports heightmap | ⚠️ Stub (logs warning) |
| Export → Heightmap... | - | Opens file dialog and exports heightmap | ⚠️ Stub (logs warning) |
| Recent Files → (file) | - | Loads the selected recent file | ✅ Implemented |
| Recent Files → Clear | - | Clears the recent files list | ✅ Implemented |
| Exit Editor | Alt+F4 | Shuts down the engine and exits | ✅ Implemented |

## Edit Menu

| Menu Item | Shortcut | Action | Status |
|-----------|----------|--------|--------|
| Undo | Ctrl+Z | Undoes the last command | ✅ Implemented (enabled when CanUndo()) |
| Redo | Ctrl+Y | Redoes the last undone command | ✅ Implemented (enabled when CanRedo()) |
| Cut | Ctrl+X | Copies and deletes selected object | ✅ Implemented (enabled when object selected) |
| Copy | Ctrl+C | Copies selected object to clipboard | ⚠️ Stub (logs action, enabled when object selected) |
| Paste | Ctrl+V | Pastes object from clipboard | ⚠️ Stub (logs warning, always disabled) |
| Delete | Del | Deletes selected objects | ✅ Implemented (enabled when object selected) |
| Select All | Ctrl+A | Selects all objects in the scene | ✅ Implemented |
| Map Properties | - | Shows map properties dialog | ✅ Implemented |
| Preferences... | - | Shows settings/preferences dialog | ✅ Implemented |

## View Menu

| Menu Item | Shortcut | Action | Status |
|-----------|----------|--------|--------|
| **Panels** | | | |
| Viewport | - | (Always visible, grayed out) | ✅ Always on |
| Details Panel | - | Toggles Details panel visibility | ✅ Implemented |
| Tools Panel | - | Toggles Tools panel visibility | ✅ Implemented |
| Content Browser | - | Toggles Content Browser visibility | ✅ Implemented |
| Material Editor | - | Toggles Material Editor visibility | ✅ Implemented |
| World Map Editor | - | Toggles World Map Editor (spherical worlds only) | ✅ Implemented |
| PCG Graph Editor | - | Toggles PCG Graph Editor | ✅ Implemented |
| **Rendering Options** | | | |
| Show Grid | - | Toggles grid visibility | ✅ Implemented |
| Show Gizmos | - | Toggles transform gizmos | ✅ Implemented |
| Show Wireframe | - | Toggles wireframe overlay | ✅ Implemented |
| Show Spherical Grid | - | Toggles spherical grid (spherical worlds only) | ✅ Implemented |
| Show Normals | - | Toggles normal vector visualization | ✅ Implemented |
| Snap to Grid | - | Toggles grid snapping | ✅ Implemented |
| **Camera** | | | |
| Reset Camera | - | Resets camera to default position | ✅ Implemented |
| Top View | - | Sets orthographic top-down view | ✅ Implemented |
| Front View | - | Sets orthographic front view | ✅ Implemented |
| Free Camera | - | Sets perspective free-look camera | ✅ Implemented |

## Tools Menu

| Menu Item | Shortcut | Action | Status |
|-----------|----------|--------|--------|
| Object Select | Q | Enters object selection mode | ✅ Implemented |
| **Transform Tools** | | | |
| Move | W | Activates move tool | ✅ Implemented |
| Rotate | E | Activates rotate tool | ✅ Implemented |
| Scale | R | Activates scale tool | ✅ Implemented |
| **Terrain Tools** | | | |
| Terrain Paint | 1 | Enters terrain painting mode | ✅ Implemented |
| Terrain Sculpt | 2 | Enters terrain sculpting mode | ✅ Implemented |
| **Tool Settings** | | | |
| Brush Size | - | Adjusts brush size (1-100) | ✅ Implemented |
| Brush Strength | - | Adjusts brush strength (0.1-10.0) | ✅ Implemented |
| Brush Falloff → Linear | - | Sets linear brush falloff | ✅ Implemented |
| Brush Falloff → Smooth | - | Sets smooth brush falloff | ✅ Implemented |
| Brush Falloff → Spherical | - | Sets spherical brush falloff | ✅ Implemented |
| Material Editor | - | Opens material editor and sets mode | ✅ Implemented |

## Help Menu

| Menu Item | Shortcut | Action | Status |
|-----------|----------|--------|--------|
| Controls | F1 | Shows controls help dialog | ✅ Implemented |
| About | - | Shows about dialog | ✅ Implemented |

## Additional Keyboard Shortcuts

| Shortcut | Context | Action |
|----------|---------|--------|
| Delete | Object Select mode | Deletes selected objects |
| Escape | Object Select mode | Clears object selection |
| [ | Terrain Paint/Sculpt | Decreases brush size |
| ] | Terrain Paint/Sculpt | Increases brush size |
| Page Up | Always | Zoom camera in |
| Page Down | Always | Zoom camera out |
| Left Arrow | Always | Rotate camera left |
| Right Arrow | Always | Rotate camera right |

## Status Legend

- ✅ **Implemented** - Fully functional
- ⚠️ **Stub** - Has placeholder implementation, logs message
- 🔒 **Disabled** - Always disabled (planned feature)

## Notes

### Recent Files
- Maximum of 10 recent files tracked
- Files are added to the top of the list
- Duplicates are automatically removed
- Persistence (load/save to config) is not yet implemented

### Command History
- Undo/Redo integrated with CommandHistory system
- Menu items are automatically enabled/disabled based on availability
- Maximum history size defaults to 100 commands

### Clipboard Operations
- Copy operation logs the selected object but doesn't yet implement full clipboard functionality
- Paste is always disabled until clipboard system is implemented
- Cut performs Copy + Delete

### Import/Export
- File dialogs are shown
- Functions log informational and warning messages
- Actual image loading/saving needs to be implemented

### Object Selection
- Select All selects all objects in m_sceneObjects
- Multi-selection mode is activated when Select All is used
- First object becomes the "primary" selection for properties display

---

**Last Updated:** 2025-12-03
