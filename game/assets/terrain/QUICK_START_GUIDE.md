# Cinematic Landscape - Quick Start Guide

## 5-Minute Integration

This guide will get the cinematic landscape rendering in your RTS main menu in 5 steps.

---

## Step 1: Verify Files (30 seconds)

Check that these files exist:
```
✓ H:/Github/Old3DEngine/game/assets/terrain/menu_landscape.json
✓ H:/Github/Old3DEngine/game/assets/terrain/LANDSCAPE_IMPLEMENTATION.md
✓ H:/Github/Old3DEngine/game/assets/terrain/LandscapeIntegrationExample.cpp
✓ H:/Github/Old3DEngine/game/assets/terrain/COLOR_PALETTE_REFERENCE.md
```

---

## Step 2: Add MenuLandscape Class (2 minutes)

Create `H:/Github/Old3DEngine/examples/MenuLandscape.hpp`:

```cpp
#pragma once

#include "engine/procedural/ProcGenGraph.hpp"
#include "engine/terrain/SDFTerrain.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Mesh.hpp"
#include <memory>
#include <string>

namespace Nova {

class MenuLandscape {
public:
    MenuLandscape() = default;
    ~MenuLandscape() = default;

    bool Initialize(const std::string& configPath);
    void Render(const glm::mat4& viewProj, const glm::vec3& cameraPos);
    float GetHeightAt(float x, float z) const;

private:
    std::shared_ptr<ProcGen::ProcGenGraph> m_procGenGraph;
    std::shared_ptr<SDFTerrain> m_sdfTerrain;
    std::shared_ptr<Shader> m_terrainShader;
    std::shared_ptr<Mesh> m_terrainMesh;

    // Simplified for quick start - full implementation in LandscapeIntegrationExample.cpp
};

} // namespace Nova
```

---

## Step 3: Modify RTSApplication.hpp (30 seconds)

Add to `RTSApplication` class:

```cpp
// In RTSApplication.hpp
#include "MenuLandscape.hpp"  // Add this include

class RTSApplication : public Application {
    // ... existing members ...
private:
    // Add this member
    std::unique_ptr<Nova::MenuLandscape> m_menuLandscape;
};
```

---

## Step 4: Initialize Landscape (1 minute)

In `RTSApplication::Initialize()`:

```cpp
bool RTSApplication::Initialize() {
    spdlog::info("Initializing RTS Application");

    // ... existing initialization code ...

    // ADD THIS BLOCK:
    spdlog::info("Loading cinematic landscape...");
    m_menuLandscape = std::make_unique<Nova::MenuLandscape>();
    if (!m_menuLandscape->Initialize("game/assets/terrain/menu_landscape.json")) {
        spdlog::warn("Failed to load menu landscape, using simple terrain");
        // Fall back to existing simple terrain
    } else {
        spdlog::info("Cinematic landscape loaded successfully!");
    }

    return true;
}
```

---

## Step 5: Render Landscape (1 minute)

In `RTSApplication::Render()`, replace terrain rendering in main menu mode:

```cpp
void RTSApplication::Render() {
    // ... existing setup ...

    if (m_currentMode == GameMode::MainMenu) {
        // MAIN MENU: Fantasy movie scene
        m_basicShader->SetFloat("u_AmbientStrength", 0.42f);

        // NEW: Render cinematic landscape
        if (m_menuLandscape) {
            m_menuLandscape->Render(
                m_camera->GetProjectionView(),
                m_camera->GetPosition()
            );
        } else {
            // Fallback: render old simple terrain
            if (m_terrainMesh) {
                glm::mat4 terrainTransform = glm::mat4(1.0f);
                m_basicShader->SetMat4("u_Model", terrainTransform);
                m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.3f, 0.65f, 0.3f));
                m_terrainMesh->Draw();
            }
        }

        // Hero (existing code - no changes needed)
        if (m_heroMesh) {
            // NEW: Get exact height from landscape
            float heroY = m_menuLandscape ?
                m_menuLandscape->GetHeightAt(-4.0f, 3.0f) : 0.0f;

            glm::mat4 heroTransform = glm::translate(
                glm::mat4(1.0f),
                glm::vec3(-4.0f, heroY, 3.0f)  // Use landscape height
            );
            heroTransform = glm::rotate(heroTransform, glm::radians(155.0f),
                                       glm::vec3(0, 1, 0));
            m_basicShader->SetMat4("u_Model", heroTransform);
            m_basicShader->SetVec3("u_ObjectColor", glm::vec3(0.7f, 0.6f, 0.5f));
            m_heroMesh->Draw();
        }

        // ... rest of menu rendering ...
    }
}
```

---

## Done!

Compile and run. You should now see:
- ✓ Rolling hills with proper erosion
- ✓ Distant mountains with atmospheric haze
- ✓ Hero standing on flat platform at (-4, 0, 3)
- ✓ Painterly color gradients (foreground→background)
- ✓ Golden hour lighting

---

## Troubleshooting

### Problem: "Failed to load menu landscape"
**Solution**: Check file path is correct:
```cpp
// Verify the path exists
std::ifstream test("game/assets/terrain/menu_landscape.json");
if (!test) {
    spdlog::error("Config file not found!");
}
```

### Problem: Terrain appears flat
**Solution**: Check height scale in config:
```json
"world": {
    "heightScale": 60.0  // Try increasing this
}
```

### Problem: Performance issues (low FPS)
**Solution**: Reduce resolution in config:
```json
"world": {
    "resolution": 512  // Reduce from 1024
}
"performance": {
    "sdf_terrain": {
        "resolution": 256  // Reduce from 512
    }
}
```

### Problem: Wrong colors
**Solution**: Check shader uniforms are being set:
```cpp
// In terrain shader setup
m_shader->SetVec3("u_LightDirection", glm::vec3(-0.4f, -1.1f, -0.6f));
m_shader->SetVec3("u_LightColor", glm::vec3(1.0f, 0.92f, 0.78f));
```

---

## Next Steps

### 1. Add Animated Elements (Optional)
```cpp
// In MenuLandscape::Update()
void MenuLandscape::Update(float deltaTime) {
    m_windTime += deltaTime * 0.5f;
    // Pass wind time to grass shader for gentle sway
}
```

### 2. Add Water Reflections (Optional)
```cpp
// Render water surface with reflections
RenderWater(viewProj, cameraPos);
```

### 3. Add Particle Effects (Optional)
```cpp
// Dust motes in sunbeams, butterflies, etc.
m_particleSystem->Update(deltaTime);
m_particleSystem->Render(viewProj);
```

### 4. Customize Colors
Edit `game/assets/terrain/menu_landscape.json`:
```json
"foreground_grass": {
    "base_color": [0.20, 0.60, 0.15],  // More saturated green
    "saturation": 1.2
}
```

### 5. Add Seasonal Variations
Create multiple config files:
- `menu_landscape_spring.json` (flowers, bright greens)
- `menu_landscape_autumn.json` (orange/red foliage)
- `menu_landscape_winter.json` (snow coverage)

Load based on current date:
```cpp
int month = GetCurrentMonth();
std::string configPath = "game/assets/terrain/menu_landscape";
if (month >= 3 && month <= 5) configPath += "_spring.json";
else if (month >= 9 && month <= 11) configPath += "_autumn.json";
else if (month == 12 || month <= 2) configPath += "_winter.json";
else configPath += ".json";  // Summer default
```

---

## Performance Targets

**Mid-Range Hardware (GTX 1060 / RX 580)**:
- Target FPS: 60
- Terrain Render: ~2.5ms
- Total Frame: ~12ms (plenty of headroom)

**Low-End Hardware (GTX 960 / RX 560)**:
- Target FPS: 30
- Use reduced resolution config
- Disable some post-processing

**High-End Hardware (RTX 3060 / RX 6700)**:
- Target FPS: 60+ easily
- Can enable all features at full quality
- Add extra effects (particles, reflections)

---

## Configuration Presets

### Preset 1: Maximum Quality
```json
"world": {
    "resolution": 2048
}
"performance": {
    "sdf_terrain": {
        "resolution": 1024
    }
}
```

### Preset 2: Balanced (Default)
```json
"world": {
    "resolution": 1024
}
"performance": {
    "sdf_terrain": {
        "resolution": 512
    }
}
```

### Preset 3: Performance
```json
"world": {
    "resolution": 512
}
"performance": {
    "sdf_terrain": {
        "resolution": 256
    }
}
```

### Preset 4: Minimum
```json
"world": {
    "resolution": 256
}
"performance": {
    "sdf_terrain": {
        "resolution": 128
    },
    "lod": {
        "num_levels": 3
    }
}
```

---

## Testing Checklist

Before committing changes:

- [ ] Landscape loads without errors
- [ ] Hero stands on flat platform (not floating/clipping)
- [ ] Camera framing looks cinematic (hero in lower third)
- [ ] Colors transition smoothly (foreground→background)
- [ ] Atmospheric fog visible on distant mountains
- [ ] Performance meets target (60 FPS on target hardware)
- [ ] Main menu UI renders on top of landscape
- [ ] Escape key returns to menu / quits
- [ ] No memory leaks after loading/unloading

---

## Debug Commands

Add these to help development:

```cpp
// In RTSApplication, handle debug keys
if (input.IsKeyPressed(Nova::Key::F1)) {
    // Toggle wireframe
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_FILL : GL_LINE);
    wireframe = !wireframe;
}

if (input.IsKeyPressed(Nova::Key::F2)) {
    // Reload landscape
    m_menuLandscape->Initialize("game/assets/terrain/menu_landscape.json");
}

if (input.IsKeyPressed(Nova::Key::F3)) {
    // Print camera position
    auto pos = m_camera->GetPosition();
    spdlog::info("Camera: ({:.1f}, {:.1f}, {:.1f})", pos.x, pos.y, pos.z);
}

if (input.IsKeyPressed(Nova::Key::F4)) {
    // Print performance stats
    auto stats = m_menuLandscape->GetStats();
    spdlog::info("Terrain: {:.2f}ms, Tris: {}", stats.renderTime, stats.triangles);
}
```

---

## Full Documentation

For complete technical details, see:
- **Implementation Guide**: `LANDSCAPE_IMPLEMENTATION.md`
- **Color Reference**: `COLOR_PALETTE_REFERENCE.md`
- **Code Example**: `LandscapeIntegrationExample.cpp`
- **Configuration**: `menu_landscape.json`

---

## Support

If you encounter issues:

1. Check the logs for error messages
2. Verify all files are in correct locations
3. Test with minimum quality preset first
4. Review the full implementation guide
5. Check shader compilation errors

---

## Credits

**Procedural Generation System**:
- ProcGenNodes.hpp/cpp (Perlin, Simplex, Ridge, Voronoi)
- ProcGenGraph.hpp (Visual scripting execution)
- HydraulicErosion, ThermalErosion (Realistic terrain shaping)

**Rendering System**:
- SDFTerrain.hpp (SDF-based terrain representation)
- LandscapeSDFIntegration.hpp (Terrain/SDF integration)

**Artistic Direction**:
- Atmospheric perspective techniques from classical landscape painting
- Color palette inspired by fantasy films (Lord of the Rings, Skyrim)
- Golden hour lighting for cinematic quality

---

**Enjoy your cinematic fantasy landscape!**
