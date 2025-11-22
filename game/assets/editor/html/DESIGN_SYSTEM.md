# Nova3D Editor Design System

A comprehensive design system for the Nova3D Editor HTML-based UI. This system provides consistent styling, reusable components, and shared utilities across all editor interfaces.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Design Tokens](#design-tokens)
3. [Components](#components)
4. [Layout Utilities](#layout-utilities)
5. [Icons](#icons)
6. [Themes](#themes)
7. [JavaScript Utilities](#javascript-utilities)
8. [Migration Guide](#migration-guide)

---

## Getting Started

### Basic Setup

Include the design system files in your HTML editor:

```html
<head>
    <!-- Design System CSS -->
    <link rel="stylesheet" href="design/design_tokens.css">
    <link rel="stylesheet" href="design/theme_dark.css">
    <link rel="stylesheet" href="design/components.css">
    <link rel="stylesheet" href="design/layout.css">
    <link rel="stylesheet" href="design/icons.css">
</head>
<body>
    <!-- Your content -->

    <!-- Design System JS -->
    <script src="design/design_tokens.js"></script>
    <script src="design/icons.js"></script>
    <script src="design/components.js"></script>
    <script src="js/editor_core.js"></script>
    <script src="js/editor_layout.js"></script>
    <script src="js/editor_3d.js"></script>

    <script>
        // Initialize icons
        document.addEventListener('DOMContentLoaded', () => {
            NovaIcons.replaceIcons();
        });
    </script>
</body>
```

---

## Design Tokens

Design tokens are CSS custom properties that define the visual foundation of the design system.

### Colors

#### Primitives
```css
/* Gray scale */
--color-gray-50: #fafafa;
--color-gray-100: #f5f5f5;
--color-gray-200: #e5e5e5;
--color-gray-300: #d4d4d4;
--color-gray-400: #a3a3a3;
--color-gray-500: #737373;
--color-gray-600: #525252;
--color-gray-700: #404040;
--color-gray-800: #262626;
--color-gray-900: #171717;
--color-gray-950: #0a0a0a;

/* Accent colors available: blue, green, yellow, red, cyan, purple, orange */
--color-blue-400: #60a5fa;
--color-blue-500: #3b82f6;
--color-blue-600: #2563eb;
```

#### Semantic Tokens
```css
/* Backgrounds */
--bg-primary: var(--color-gray-900);
--bg-secondary: var(--color-gray-800);
--bg-tertiary: var(--color-gray-700);
--bg-hover: var(--color-gray-600);
--bg-active: var(--color-gray-500);

/* Text */
--text-primary: var(--color-gray-100);
--text-secondary: var(--color-gray-400);
--text-muted: var(--color-gray-500);
--text-inverse: var(--color-gray-900);

/* Borders */
--border-default: var(--color-gray-700);
--border-subtle: var(--color-gray-800);
--border-strong: var(--color-gray-600);
--border-focus: var(--color-blue-500);

/* Accents */
--accent-primary: var(--color-blue-500);
--accent-success: var(--color-green-500);
--accent-warning: var(--color-yellow-500);
--accent-danger: var(--color-red-500);
```

### Typography

```css
/* Font families */
--font-sans: -apple-system, BlinkMacSystemFont, 'Segoe UI', ...;
--font-mono: 'JetBrains Mono', 'Fira Code', monospace;

/* Font sizes */
--text-xs: 0.75rem;    /* 12px */
--text-sm: 0.875rem;   /* 14px */
--text-md: 1rem;       /* 16px */
--text-lg: 1.125rem;   /* 18px */
--text-xl: 1.25rem;    /* 20px */
--text-2xl: 1.5rem;    /* 24px */

/* Font weights */
--font-normal: 400;
--font-medium: 500;
--font-semibold: 600;
--font-bold: 700;
```

### Spacing

```css
--space-0: 0;
--space-1: 0.25rem;   /* 4px */
--space-2: 0.5rem;    /* 8px */
--space-3: 0.75rem;   /* 12px */
--space-4: 1rem;      /* 16px */
--space-5: 1.25rem;   /* 20px */
--space-6: 1.5rem;    /* 24px */
--space-8: 2rem;      /* 32px */
--space-10: 2.5rem;   /* 40px */
--space-12: 3rem;     /* 48px */
```

### Border Radius

```css
--radius-none: 0;
--radius-sm: 0.125rem;  /* 2px */
--radius-md: 0.25rem;   /* 4px */
--radius-lg: 0.5rem;    /* 8px */
--radius-xl: 0.75rem;   /* 12px */
--radius-2xl: 1rem;     /* 16px */
--radius-full: 9999px;
```

### Shadows

```css
--shadow-sm: 0 1px 2px rgba(0, 0, 0, 0.3);
--shadow-md: 0 4px 6px -1px rgba(0, 0, 0, 0.3);
--shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.3);
--shadow-xl: 0 20px 25px -5px rgba(0, 0, 0, 0.3);
```

### JavaScript Access

```javascript
// Get token values
const primary = NovaDesignTokens.colors.blue[500];
const spacing = NovaDesignTokens.spacing[4];

// Theme management
NovaDesignTokens.setTheme('dark');  // 'dark', 'light', 'highContrast'
NovaDesignTokens.getTheme();
NovaDesignTokens.onThemeChange(theme => console.log(theme));

// Color utilities
NovaDesignTokens.hexToRgb('#3b82f6');
NovaDesignTokens.withAlpha('#3b82f6', 0.5);
```

---

## Components

### Buttons

```html
<!-- Primary button -->
<button class="btn btn-primary">Save</button>

<!-- Secondary button -->
<button class="btn btn-secondary">Cancel</button>

<!-- Ghost button -->
<button class="btn btn-ghost">More Options</button>

<!-- Danger button -->
<button class="btn btn-danger">Delete</button>

<!-- Button sizes -->
<button class="btn btn-xs btn-primary">XS</button>
<button class="btn btn-sm btn-primary">SM</button>
<button class="btn btn-primary">Default</button>
<button class="btn btn-lg btn-primary">LG</button>

<!-- Icon button -->
<button class="btn btn-icon btn-ghost">
    <span data-icon="settings"></span>
</button>

<!-- Button with icon -->
<button class="btn btn-primary">
    <span data-icon="save"></span>
    Save Changes
</button>

<!-- Button group -->
<div class="btn-group">
    <button class="btn btn-secondary active">Grid</button>
    <button class="btn btn-secondary">List</button>
</div>
```

### Inputs

```html
<!-- Text input -->
<input type="text" class="input" placeholder="Enter text...">

<!-- With size -->
<input type="text" class="input input-sm">
<input type="text" class="input input-lg">

<!-- Select -->
<select class="select">
    <option>Option 1</option>
    <option>Option 2</option>
</select>

<!-- Textarea -->
<textarea class="input" rows="4"></textarea>

<!-- Checkbox -->
<label class="checkbox">
    <input type="checkbox">
    <span class="checkbox-mark"></span>
    Enable feature
</label>

<!-- Radio -->
<label class="radio">
    <input type="radio" name="option">
    <span class="radio-mark"></span>
    Option A
</label>

<!-- Range slider -->
<input type="range" class="range" min="0" max="100">
```

### Tabs

```html
<div class="tabs">
    <button class="tab active" data-tab="general">General</button>
    <button class="tab" data-tab="settings">Settings</button>
    <button class="tab" data-tab="advanced">Advanced</button>
</div>

<div class="tab-content active" id="tab-general">
    <!-- Content -->
</div>
```

### Cards & Panels

```html
<!-- Card -->
<div class="card">
    <div class="card-header">
        <h3>Card Title</h3>
    </div>
    <div class="card-body">
        Content here
    </div>
    <div class="card-footer">
        <button class="btn btn-primary">Action</button>
    </div>
</div>

<!-- Panel -->
<div class="panel">
    <div class="panel-header">
        <span class="panel-title">Properties</span>
        <button class="btn btn-icon btn-ghost btn-xs panel-collapse">
            <span data-icon="chevron-down"></span>
        </button>
    </div>
    <div class="panel-body">
        Content
    </div>
</div>
```

### Modal

```html
<div class="modal-backdrop active">
    <div class="modal modal-md">
        <div class="modal-header">
            <h3 class="modal-title">Dialog Title</h3>
            <button class="btn btn-icon btn-ghost btn-sm modal-close">
                <span data-icon="x"></span>
            </button>
        </div>
        <div class="modal-body">
            Modal content here
        </div>
        <div class="modal-footer">
            <button class="btn btn-secondary">Cancel</button>
            <button class="btn btn-primary">Confirm</button>
        </div>
    </div>
</div>
```

### Badges & Tags

```html
<!-- Badges -->
<span class="badge">Default</span>
<span class="badge badge-success">Success</span>
<span class="badge badge-warning">Warning</span>
<span class="badge badge-danger">Danger</span>

<!-- Tags -->
<span class="tag">
    Label
    <button class="tag-remove">&times;</button>
</span>
```

### Notifications (Toast)

```javascript
// Using NovaToast
NovaToast.success('Changes saved successfully');
NovaToast.error('Failed to save changes');
NovaToast.warning('Please review before continuing');
NovaToast.info('New update available');
```

### Context Menu

```javascript
// Using NovaContextMenu
NovaContextMenu.show([
    { label: 'Edit', icon: 'edit', action: () => handleEdit() },
    { label: 'Duplicate', icon: 'copy', action: () => handleDuplicate() },
    { type: 'separator' },
    { label: 'Delete', icon: 'trash', action: () => handleDelete(), danger: true }
], event.clientX, event.clientY);
```

### Web Components

```html
<!-- Panel component -->
<nova-panel title="Properties" collapsible>
    Content here
</nova-panel>

<!-- Tab container -->
<nova-tab-container>
    <nova-tab label="General">Content 1</nova-tab>
    <nova-tab label="Settings">Content 2</nova-tab>
</nova-tab-container>

<!-- Tree view -->
<nova-tree-view id="file-tree"></nova-tree-view>

<!-- Property grid -->
<nova-property-grid id="props"></nova-property-grid>

<!-- Color picker -->
<nova-color-picker value="#ff0000"></nova-color-picker>

<!-- Number slider -->
<nova-number-slider min="0" max="100" value="50"></nova-number-slider>

<!-- Vector input -->
<nova-vector-input dimensions="3" value="1,2,3"></nova-vector-input>

<!-- Search box -->
<nova-search-box placeholder="Search..."></nova-search-box>
```

---

## Layout Utilities

### Flexbox

```html
<div class="flex-row gap-2">...</div>
<div class="flex-col gap-4">...</div>
<div class="flex-row justify-between items-center">...</div>
<div class="flex-1">...</div>  <!-- flex: 1 -->
<div class="flex-shrink-0">...</div>
```

### Grid

```html
<div class="grid grid-cols-2 gap-4">...</div>
<div class="grid grid-cols-3 gap-2">...</div>
<div class="col-span-2">...</div>
```

### Spacing

```html
<!-- Padding -->
<div class="p-4">...</div>
<div class="px-2 py-4">...</div>
<div class="pt-2 pb-4">...</div>

<!-- Margin -->
<div class="m-4">...</div>
<div class="mx-auto">...</div>
<div class="mt-2 mb-4">...</div>

<!-- Gap -->
<div class="gap-2">...</div>
<div class="gap-x-4 gap-y-2">...</div>
```

### Width & Height

```html
<div class="w-full">...</div>
<div class="w-1/2">...</div>
<div class="h-screen">...</div>
<div class="min-h-0 max-w-md">...</div>
```

### Editor Layout Patterns

```html
<!-- Standard editor container -->
<div class="editor-container">
    <div class="editor-sidebar">
        <!-- Sidebar content -->
    </div>
    <div class="editor-main-view">
        <!-- Main content -->
    </div>
    <div class="editor-properties-panel">
        <!-- Properties -->
    </div>
</div>
```

---

## Icons

### Using Icons

```html
<!-- Icon in element (auto-replaced) -->
<span data-icon="save"></span>
<span data-icon="folder-open"></span>

<!-- With explicit size -->
<span data-icon="settings" class="icon icon-lg"></span>
```

### JavaScript API

```javascript
// Replace all data-icon elements
NovaIcons.replaceIcons();

// Replace within container
NovaIcons.replaceIcons(document.getElementById('my-container'));

// Create icon element
const icon = NovaIcons.create('save');
element.appendChild(icon);

// Get SVG string
const svg = NovaIcons.getSVG('save');

// Register custom icon
NovaIcons.register('my-icon', 'M10 10 L20 20...');
```

### Available Icons

**General:** `save`, `folder`, `folder-open`, `file`, `copy`, `paste`, `cut`, `trash`, `edit`, `settings`, `search`, `refresh`, `x`, `check`

**Navigation:** `arrow-left`, `arrow-right`, `arrow-up`, `arrow-down`, `chevron-left`, `chevron-right`, `chevron-up`, `chevron-down`, `home`, `menu`

**Actions:** `play`, `pause`, `stop`, `undo`, `redo`, `plus`, `minus`, `maximize`, `minimize`, `grid-view`, `list-view`, `zoom-in`, `zoom-out`

**Editor:** `link`, `unlink`, `layers`, `eye`, `eye-off`, `lock`, `unlock`, `warning`, `info`, `error`

**Game Assets:** `unit`, `building`, `spell`, `effect`, `tile`, `hero`, `ability`, `techtree`, `projectile`

---

## Themes

### Switching Themes

```javascript
// Via JavaScript
NovaDesignTokens.setTheme('light');    // Light theme
NovaDesignTokens.setTheme('dark');     // Dark theme (default)
NovaDesignTokens.setTheme('highContrast'); // High contrast

// Via CSS class
document.body.dataset.theme = 'light';
```

### Theme Files

- `theme_dark.css` - Default dark theme
- `theme_light.css` - Light theme for bright environments
- `theme_high_contrast.css` - Accessibility-focused high contrast theme

### Creating Custom Themes

```css
/* custom_theme.css */
:root,
[data-theme="custom"] {
    --bg-primary: #your-color;
    --bg-secondary: #your-color;
    /* Override all semantic tokens */
}
```

---

## JavaScript Utilities

### EditorCore.Bridge

Communication with the C++ backend:

```javascript
// Invoke backend method
EditorCore.Bridge.invoke('method.name', [args]).then(result => {
    console.log(result);
});

// File operations
EditorCore.Bridge.browseFile('*.json').then(path => {});
EditorCore.Bridge.loadConfig(path).then(data => {});
EditorCore.Bridge.saveConfig(path, data).then(() => {});
EditorCore.Bridge.executePython(script).then(result => {});
```

### EditorCore.Events

Pub/sub event system:

```javascript
// Subscribe
const unsubscribe = EditorCore.Events.on('config.changed', data => {
    console.log('Config changed:', data);
});

// Emit
EditorCore.Events.emit('config.changed', { path: 'name', value: 'New Name' });

// Unsubscribe
unsubscribe();
```

### EditorCore.State

Path-based state management:

```javascript
// Set state
EditorCore.State.set('editor.selectedId', 'unit_001');

// Get state
const id = EditorCore.State.get('editor.selectedId');

// Subscribe to changes
EditorCore.State.subscribe('editor.selectedId', (value, path) => {
    console.log('Selection changed:', value);
});
```

### EditorCore.Shortcuts

Keyboard shortcut registration:

```javascript
EditorCore.Shortcuts.register('ctrl+s', () => saveConfig());
EditorCore.Shortcuts.register('ctrl+z', () => undo());
EditorCore.Shortcuts.register('delete', () => deleteSelected());
```

### EditorCore.UndoRedo

Undo/redo management:

```javascript
// Execute action with undo support
EditorCore.UndoRedo.execute({
    execute: () => { /* do action */ },
    undo: () => { /* reverse action */ },
    description: 'Changed property'
});

// Undo/redo
EditorCore.UndoRedo.undo();
EditorCore.UndoRedo.redo();
```

### EditorCore.Notifications

Toast notifications:

```javascript
EditorCore.Notifications.success('Saved successfully');
EditorCore.Notifications.error('Failed to save');
EditorCore.Notifications.warning('Unsaved changes');
EditorCore.Notifications.info('Processing...');
```

### EditorCore.Utils

Utility functions:

```javascript
EditorCore.Utils.debounce(fn, 300);
EditorCore.Utils.throttle(fn, 100);
EditorCore.Utils.generateId();
EditorCore.Utils.deepClone(obj);
EditorCore.Utils.deepMerge(target, source);
```

### EditorLayout

Panel and layout management:

```javascript
// Register panel
EditorLayout.registerPanel('properties', {
    element: document.getElementById('properties-panel'),
    defaultWidth: 300,
    minWidth: 200,
    collapsible: true
});

// Save/restore layout
EditorLayout.saveLayout();
EditorLayout.restoreLayout();

// Create split pane
const split = new EditorLayout.SplitPane(container, {
    direction: 'horizontal',
    sizes: [30, 70]
});
```

### Editor3D

Three.js scene utilities:

```javascript
// Create preview scene
const scene = new Editor3D.PreviewScene(container, {
    cameraPosition: { x: 5, y: 5, z: 5 },
    enableGrid: true,
    enableAxes: true
});

// Selection manager
const selection = new Editor3D.SelectionManager(scene);
selection.select(object);
selection.onSelectionChange(objects => {});

// Transform gizmo
const gizmo = new Editor3D.TransformGizmo(scene, camera, renderer);
gizmo.setMode('translate'); // 'translate', 'rotate', 'scale'

// Object picker
const picker = new Editor3D.ObjectPicker(scene, camera);
picker.onClick(object => {});
```

---

## Migration Guide

### Updating Existing Editors

1. **Add design system imports** in `<head>`:
   ```html
   <link rel="stylesheet" href="design/design_tokens.css">
   <link rel="stylesheet" href="design/theme_dark.css">
   <link rel="stylesheet" href="design/components.css">
   <link rel="stylesheet" href="design/layout.css">
   <link rel="stylesheet" href="design/icons.css">
   ```

2. **Remove inline `:root` CSS variables** - they're now in `design_tokens.css`

3. **Replace inline styles with design tokens**:
   ```css
   /* Before */
   padding: 8px 12px;
   background: #252526;

   /* After */
   padding: var(--space-2) var(--space-3);
   background: var(--bg-secondary);
   ```

4. **Use component classes**:
   ```html
   <!-- Before -->
   <button style="padding: 6px 12px; background: #007acc;">Save</button>

   <!-- After -->
   <button class="btn btn-primary">Save</button>
   ```

5. **Update icons** to use `data-icon` attribute:
   ```html
   <!-- Before -->
   <span>&#x25BC;</span>

   <!-- After -->
   <span data-icon="chevron-down"></span>
   ```

6. **Add JavaScript imports** before `</body>`:
   ```html
   <script src="design/design_tokens.js"></script>
   <script src="design/icons.js"></script>
   <script src="design/components.js"></script>
   <script src="js/editor_core.js"></script>
   ```

7. **Initialize icons** in DOMContentLoaded:
   ```javascript
   document.addEventListener('DOMContentLoaded', () => {
       NovaIcons.replaceIcons();
   });
   ```

8. **Use EditorCore utilities** instead of inline implementations:
   ```javascript
   // Before
   function debounce(fn, delay) { ... }

   // After
   EditorCore.Utils.debounce(fn, delay);
   ```

---

## File Structure

```
game/assets/editor/html/
├── design/
│   ├── design_tokens.css      # Core CSS custom properties
│   ├── design_tokens.js       # JS access to design tokens
│   ├── components.css         # Reusable component styles
│   ├── components.js          # Web components
│   ├── layout.css             # Layout utilities
│   ├── icons.css              # Icon styles
│   ├── icons.js               # Icon system
│   ├── theme_dark.css         # Dark theme
│   ├── theme_light.css        # Light theme
│   └── theme_high_contrast.css # High contrast theme
├── js/
│   ├── editor_core.js         # Core utilities
│   ├── editor_layout.js       # Layout management
│   └── editor_3d.js           # 3D preview utilities
├── components/
│   ├── vector_editor.html     # Vector editing component
│   ├── color_picker.html      # Color picker component
│   ├── curve_editor.html      # Animation curve editor
│   ├── file_picker.html       # File browser dialog
│   └── asset_reference.html   # Asset reference picker
├── viewers/
│   ├── unit_viewer.html       # Unit editor
│   ├── building_viewer.html   # Building editor
│   └── ...                    # Other viewers
└── DESIGN_SYSTEM.md           # This documentation
```

---

## Best Practices

1. **Always use design tokens** instead of hardcoded values
2. **Use semantic color tokens** (`--bg-primary`) over primitive tokens (`--color-gray-900`)
3. **Leverage component classes** for consistent UI elements
4. **Use layout utilities** for spacing and alignment
5. **Initialize icons** after dynamic content is added
6. **Use EditorCore.Events** for component communication
7. **Save layout state** using EditorLayout for user preferences
8. **Follow the viewer template** pattern for new editors

---

## Contributing

When adding new components or utilities:

1. Add CSS to appropriate file (`components.css`, `layout.css`)
2. Use existing design tokens
3. Document in this file
4. Add examples to a demo HTML file
5. Test across all three themes
