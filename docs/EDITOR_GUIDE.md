# Editor User Guide

The Nova3D Editor is a comprehensive ImGui-based development environment for creating and editing game content. This guide covers all editor features and workflows.

## Overview

The editor provides:

- **Dockable Windows**: Flexible layout with saveable positions
- **Content Browser**: Asset management with thumbnails
- **Config Editor**: Visual JSON editing with schema validation
- **World View**: 3D scene viewport
- **Hierarchy**: Entity tree view
- **Inspector**: Property editing
- **Script Editor**: Python code editing
- **Console**: Debug output and commands
- **PCG Panel**: Procedural content generation

## Getting Started

### Launching the Editor

```bash
# Run with editor enabled
./nova_demo --editor

# Or in integrated mode (editor + game)
./nova_demo --editor --integrated
```

### Initial Layout

On first launch, the editor creates a default layout:

```
┌─────────────────────────────────────────────────────────────┐
│  File  Edit  View  Tools  Help                              │
├──────────┬────────────────────────────────┬─────────────────┤
│          │                                │                 │
│ Hierarchy│         World View             │   Inspector     │
│          │                                │                 │
│          │                                │                 │
├──────────┤                                ├─────────────────┤
│          │                                │                 │
│  Asset   │                                │  Config Editor  │
│  Browser │                                │                 │
│          ├────────────────────────────────┴─────────────────┤
│          │                Console                           │
└──────────┴──────────────────────────────────────────────────┘
```

### Saving and Loading Layouts

- **Save Layout**: `File > Save Layout` or `Ctrl+Shift+S`
- **Load Layout**: `File > Load Layout`
- **Reset Layout**: `View > Reset Layout`

## Main Menu

### File Menu

| Action | Shortcut | Description |
|--------|----------|-------------|
| New Project | `Ctrl+N` | Create a new project |
| Open Project | `Ctrl+O` | Open an existing project |
| Save | `Ctrl+S` | Save current project |
| Save As | `Ctrl+Shift+S` | Save to new location |
| Close Project | - | Close current project |
| Exit | `Alt+F4` | Exit the editor |

### Edit Menu

| Action | Shortcut | Description |
|--------|----------|-------------|
| Undo | `Ctrl+Z` | Undo last action |
| Redo | `Ctrl+Y` | Redo last undone action |
| Cut | `Ctrl+X` | Cut selection |
| Copy | `Ctrl+C` | Copy selection |
| Paste | `Ctrl+V` | Paste from clipboard |
| Delete | `Del` | Delete selection |
| Select All | `Ctrl+A` | Select all entities |

### View Menu

| Action | Shortcut | Description |
|--------|----------|-------------|
| Hierarchy | `F1` | Toggle Hierarchy panel |
| Inspector | `F2` | Toggle Inspector panel |
| Console | `F3` | Toggle Console panel |
| Asset Browser | `F4` | Toggle Asset Browser |
| Config Editor | `F5` | Toggle Config Editor |
| Script Editor | `F6` | Toggle Script Editor |
| PCG Panel | `F7` | Toggle PCG panel |
| World View | `F8` | Toggle World View |

### Tools Menu

| Action | Shortcut | Description |
|--------|----------|-------------|
| Location Crafter | - | Open location editor |
| Procedural Town | - | Generate procedural town |
| Import Asset | `Ctrl+I` | Import external asset |
| Build Configs | - | Validate all configs |

## UI Overview

### World View

The 3D scene viewport for viewing and editing the game world.

**Camera Controls**:
- **Right Mouse + WASD**: Fly camera movement
- **Right Mouse + Mouse**: Look around
- **Scroll Wheel**: Adjust movement speed
- **F**: Focus on selected entity
- **Home**: Reset camera position

**Selection**:
- **Left Click**: Select entity
- **Ctrl+Click**: Add to selection
- **Shift+Click**: Select range
- **Drag Box**: Box selection

**Transform Gizmos**:
- **W**: Translate mode
- **E**: Rotate mode
- **R**: Scale mode
- **Q**: No gizmo (selection only)
- **Space**: Toggle local/world space

**View Options**:
- **Grid**: Toggle grid display
- **Wireframe**: Toggle wireframe rendering
- **Bounds**: Show entity bounding boxes
- **Debug**: Show debug visualizations

### Hierarchy Panel

Tree view of all entities in the scene.

**Features**:
- Drag-and-drop parenting
- Search/filter entities
- Context menu actions
- Multi-select support

**Context Menu** (Right-click):
- Create Child Entity
- Duplicate
- Delete
- Rename
- Copy/Paste

**Filtering**:
```
# Filter by name
enemy

# Filter by type
type:Zombie

# Filter by component
has:HealthComponent

# Multiple filters
enemy type:Zombie
```

### Inspector Panel

Property editor for selected entities.

**Property Types**:
- **Numbers**: Slider or input field with range
- **Strings**: Text input
- **Booleans**: Checkbox
- **Vectors**: XYZ inputs with color coding
- **Colors**: Color picker
- **Enums**: Dropdown selection
- **References**: Entity/asset picker

**Property Attributes**:
- **Editable**: Can be modified in editor
- **ReadOnly**: Display only
- **Range**: Min/max constraints
- **Category**: Grouped display

### Console Panel

Debug output and command interface.

**Log Levels**:
- **Info** (white): General information
- **Warning** (yellow): Potential issues
- **Error** (red): Errors and exceptions
- **Debug** (gray): Verbose debug output

**Commands**:
```
# Clear console
clear

# Execute Python
python print("Hello")

# Spawn entity
spawn Zombie 10 0 5

# Set property
set player.health 100

# Query entities
list type:Enemy
```

**Filtering**:
- Click log level buttons to filter
- Use search box for text filtering

### Asset Browser

Browse and manage project assets.

**Navigation**:
- Double-click folders to navigate
- Breadcrumb path for quick navigation
- Back/Forward buttons

**Asset Types**:
- **Models**: `.fbx`, `.obj`, `.gltf`
- **Textures**: `.png`, `.jpg`, `.tga`
- **Configs**: `.json`
- **Scripts**: `.py`
- **Audio**: `.wav`, `.ogg`, `.mp3`

**Actions**:
- **Drag to Scene**: Create entity from asset
- **Double-click**: Open in appropriate editor
- **Right-click**: Context menu

**Context Menu**:
- Open
- Rename
- Delete
- Show in Explorer
- Copy Path
- Reimport

### Config Editor

Visual JSON editor with schema validation.

**Features**:
- Schema-based validation
- Auto-completion
- Error highlighting
- Property documentation
- Nested object editing

**Config Types**:
- Unit configurations
- Spell definitions
- Building data
- Ability configs
- Effect definitions

**Workflow**:
1. Select config type from dropdown
2. Choose existing config or create new
3. Edit properties in form view
4. Errors shown in real-time
5. Save with `Ctrl+S`

### Script Editor

Python code editing with syntax highlighting.

**Features**:
- Syntax highlighting
- Auto-indentation
- Line numbers
- Error markers
- Hot reload support

**Toolbar**:
- **Run**: Execute script
- **Save**: Save changes
- **Hot Reload**: Toggle hot reload
- **Format**: Auto-format code

**Shortcuts**:
- `Ctrl+S`: Save
- `F5`: Run script
- `Ctrl+/`: Toggle comment
- `Ctrl+F`: Find
- `Ctrl+H`: Find and replace

## Creating Entities

### From Hierarchy

1. Right-click in Hierarchy
2. Select "Create Entity"
3. Choose entity type
4. Entity appears in scene

### From Asset Browser

1. Navigate to asset
2. Drag into World View
3. Drop at desired location
4. Entity created with asset

### From Config

1. Open Config Editor
2. Select entity type
3. Configure properties
4. Click "Spawn in Scene"

## Editing Configs

### Creating a New Config

1. Open Config Editor (`F5`)
2. Click "New Config"
3. Select config type
4. Enter config ID
5. Fill in properties
6. Save (`Ctrl+S`)

### Editing Existing Config

1. Open Asset Browser
2. Navigate to config folder
3. Double-click config file
4. Opens in Config Editor
5. Make changes
6. Save (`Ctrl+S`)

### Config Validation

The editor validates configs against JSON schemas:

- **Green check**: Valid configuration
- **Yellow warning**: Potential issues
- **Red X**: Validation errors

Click error icons to see details.

## Using the Content Browser

### Importing Assets

1. Click "Import" button or `Ctrl+I`
2. Select source file(s)
3. Choose import settings
4. Click "Import"

**Import Settings by Type**:

**Models**:
- Scale factor
- Generate LODs
- Import animations
- Material handling

**Textures**:
- Format conversion
- Compression
- Generate mipmaps
- Max resolution

### Creating Folders

1. Right-click in browser
2. Select "New Folder"
3. Enter name
4. Folder created

### Asset Thumbnails

- Models show 3D preview
- Textures show image preview
- Configs show type icon
- Scripts show code icon

Click refresh button to regenerate thumbnails.

## Python Scripting in Editor

### Running Scripts

1. Open Script Editor (`F6`)
2. Write or open script
3. Press `F5` to run
4. Output appears in Console

### Script Templates

Create common script types:

```python
# AI Behavior Template
def on_spawn(entity):
    pass

def on_update(entity, dt):
    pass

def on_damage(entity, damage, source):
    pass
```

### Hot Reload

When enabled:
1. Save script file
2. Editor detects change
3. Script automatically reloads
4. Changes take effect immediately

## Import Workflow

### Importing 3D Models

1. Click `File > Import Asset`
2. Select `.fbx`, `.obj`, or `.gltf` file
3. Configure import settings:
   - Scale (default: 1.0)
   - Up axis (Y-up or Z-up)
   - Import materials
   - Import animations
4. Click "Import"
5. Asset appears in Content Browser

### Importing Textures

1. Drag image file into Content Browser
2. Or use `File > Import Asset`
3. Configure settings:
   - Format (RGBA, RGB, etc.)
   - Compression (None, DXT, etc.)
   - Generate mipmaps
4. Texture imported and ready to use

### Bulk Import

1. Select multiple files in file dialog
2. Or drag folder into Content Browser
3. Configure shared settings
4. All assets imported

## Keyboard Shortcuts

### Global Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New Project |
| `Ctrl+O` | Open Project |
| `Ctrl+S` | Save |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Del` | Delete |
| `F1-F8` | Toggle panels |

### World View Shortcuts

| Shortcut | Action |
|----------|--------|
| `W` | Translate tool |
| `E` | Rotate tool |
| `R` | Scale tool |
| `Q` | Select tool |
| `F` | Focus selection |
| `Home` | Reset camera |
| `Space` | Local/World toggle |

### Script Editor Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+S` | Save script |
| `F5` | Run script |
| `Ctrl+/` | Toggle comment |
| `Ctrl+F` | Find |
| `Ctrl+H` | Replace |

## Tips and Best Practices

### Performance

- Close unused panels for better performance
- Use LOD preview to optimize models
- Enable culling visualization to verify setup

### Workflow

- Save layouts for different tasks (level design, scripting, etc.)
- Use keyboard shortcuts for common actions
- Keep Console open to catch errors early

### Organization

- Use consistent naming conventions
- Organize assets in logical folders
- Document custom scripts

### Collaboration

- Use version control for configs
- Keep editor layout in `.gitignore`
- Document custom workflows for team
