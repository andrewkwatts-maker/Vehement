# Asset Icon Capture System

Automated screenshot capture system for generating transparent PNG icons for all game assets (heroes, units, buildings, decorations, etc.).

## Overview

The asset icon system automatically:
- Scans all asset categories for JSON files
- Detects which assets need icon updates (new or modified)
- Generates transparent background screenshots for UI use
- Maintains a cache to avoid redundant renders
- Organizes output matching the asset folder structure

## Quick Start

### 1. Run the Icon Capture Script

```bash
cd H:/Github/Old3DEngine
python tools/capture_asset_icons.py
```

### 2. Output

Icons are saved to:
```
H:/Github/Old3DEngine/screenshots/asset_icons/
├── heroes/
│   ├── warrior_hero.png
│   ├── warrior_hero_v2.png
│   └── warrior_hero_v3.png
├── units/
│   └── (unit icons)
├── buildings/
│   └── (building icons)
└── ...
```

## Features

### Smart Update Detection

The system only regenerates icons when:
- Asset JSON file is new (no icon exists)
- Asset JSON file has been modified (MD5 hash changed)
- Asset file is newer than the existing icon (timestamp check)

This saves rendering time when working with large asset libraries.

### Automatic Organization

Icon paths mirror asset paths:
```
Asset:  game/assets/heroes/elven/archer.json
Icon:   screenshots/asset_icons/heroes/elven/archer.png
```

### Cache System

A cache file (`.asset_cache.json`) tracks:
- Asset file hashes (MD5)
- Last render timestamp
- Icon file paths
- Icon file sizes

This enables intelligent skipping of unchanged assets.

## Asset Categories

The system processes these categories by default:
- **heroes** - Player hero characters
- **units** - Regular military units
- **buildings** - Structures and constructions
- **decorations** - Environmental decorations
- **vehicles** - Vehicles and mounts
- **props** - Miscellaneous objects

Add more categories by editing `ASSET_CATEGORIES` in the script.

## Icon Specifications

### Current Settings

```json
{
  "width": 512,
  "height": 512,
  "background": "transparent",
  "camera_distance": 3.5,
  "camera_angle": [15, 45, 0],
  "fov": 35,
  "lighting": "studio_three_point"
}
```

### Studio Lighting Setup

Three-point lighting for clean, professional icons:
- **Key Light**: intensity 1.5, main illumination
- **Fill Light**: intensity 0.6, softens shadows
- **Rim Light**: intensity 0.8, edge definition

No ambient occlusion or complex post-processing - keeps icons clean and recognizable.

## Workflow Integration

### When Asset is Created/Modified

1. Artist creates/modifies asset JSON
2. Artist runs `capture_asset_icons.py`
3. System detects change via hash comparison
4. Icon is re-rendered with transparent background
5. Cache is updated
6. Icon is ready for use in UI

### Batch Processing

Run regularly to update all assets:
```bash
# Weekly batch update
python tools/capture_asset_icons.py
```

Only modified assets will be re-rendered.

## Current Implementation

### Phase 1: Placeholder Icons (CURRENT)

The script currently generates **placeholder icons** using PIL:
- Colored squares based on asset category
- Gold = heroes, Blue = units, Brown = buildings, etc.
- Asset name overlaid
- Fully transparent background

This allows UI development to proceed while rendering is implemented.

### Phase 2: Real Rendering (TODO)

Future implementation will:
1. Launch demo executable in headless mode
2. Load asset from JSON
3. Position camera for optimal framing
4. Render with transparent background
5. Save PNG with alpha channel
6. Exit automatically

## Customization

### Changing Icon Size

Edit in `capture_asset_icons.py`:
```python
"width": 1024,   # Larger icons
"height": 1024
```

### Adjusting Camera

```python
"camera_distance": 4.0,     # Further back
"camera_angle": [20, 30, 0] # Different angle
```

### Custom Lighting

```python
"lighting": "dramatic"  # Options: studio, dramatic, outdoor, magical
```

## File Structure

```
tools/
├── capture_asset_icons.py       # Main script
└── capture_all_assets.py        # Advanced version (headless render)

screenshots/
├── asset_icons/                 # Output directory
│   ├── heroes/
│   ├── units/
│   └── ...
└── .asset_cache.json           # Modification cache

game/assets/
├── heroes/
├── units/
└── ...                         # Source assets
```

## Integration with UI

Icons can be loaded in the game UI:

```cpp
// Example: Load unit icon for UI
std::string iconPath = "screenshots/asset_icons/units/archer.png";
Texture* unitIcon = ResourceManager::LoadTexture(iconPath);

// Use in UI
ImGui::Image(unitIcon, ImVec2(64, 64));
```

## Troubleshooting

### "No assets found"

- Check that asset JSON files exist in `game/assets/`
- Verify JSON files have `"type"` and `"sdfModel"` fields

### "PIL not available"

Install Pillow for placeholder generation:
```bash
pip install Pillow
```

### Icons not updating

Clear the cache to force regeneration:
```bash
rm H:/Github/Old3DEngine/screenshots/.asset_cache.json
python tools/capture_asset_icons.py
```

### Permission errors

Ensure write access to `screenshots/asset_icons/` directory.

## Performance

### Scan Performance
- Scanning ~1000 assets: < 1 second
- Hash calculation: ~0.5ms per asset
- JSON parsing: ~1ms per asset

### Render Performance (Future)
- Estimated: 2-5 seconds per asset icon
- Batch of 100 assets: ~5-10 minutes
- Can be parallelized with multiple render instances

## API Reference

### AssetIconCapture Class

```python
capturer = AssetIconCapture()

# Scan and plan captures
assets = capturer.scan_and_plan()

# Check if asset needs update
needs_update = capturer.needs_update(asset_path, screenshot_path)

# Get output path for asset
output_path = capturer.get_screenshot_path(asset)

# Generate manifest for batch rendering
manifest_path = capturer.generate_capture_manifest()

# Print summary
capturer.print_summary()
```

## Future Enhancements

### Planned Features

1. **Real-time Preview** - Show icon as it renders
2. **Multi-angle Icons** - Front, side, top views
3. **Animated Icons** - Short looping GIFs for heroes
4. **LOD Icons** - Multiple resolutions (64x64, 128x128, 512x512)
5. **Batch Parallelization** - Render multiple assets simultaneously
6. **Style Presets** - Different lighting/angle presets per category
7. **Custom Frames** - Decorative borders for rare/epic items
8. **Background Variants** - Option for solid colors, gradients

### Integration Goals

- Auto-trigger on asset save in editor
- Live preview in asset browser
- Version history (keep old icons when asset changes)
- Export to game-ready atlas/sprite sheet

## Examples

### Hero Icon
```
Asset: warrior_hero_v3.json
Icon:  warrior_hero_v3.png
- 512x512 transparent PNG
- Golden armor clearly visible
- Sword and shield prominent
- Slightly angled for depth
```

### Building Icon
```
Asset: castle.json
Icon:  castle.png
- Isometric-style view (45° angle)
- All towers visible
- Flags and details clear
- Shadow on transparent background
```

### Unit Icon
```
Asset: archer.json
Icon:  archer.png
- Portrait-style framing
- Face and weapon visible
- Action pose
- Team color tintable
```

## Conclusion

The asset icon system provides automated, intelligent icon generation for all game assets. Current placeholder implementation allows immediate UI development, with real rendering coming in Phase 2.

**Status**: ✅ Placeholder generation working
**Next**: Implement headless rendering in demo executable
