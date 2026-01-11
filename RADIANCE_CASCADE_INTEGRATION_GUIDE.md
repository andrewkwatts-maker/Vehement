# Radiance Cascade Integration Guide

Quick guide for integrating the radiance cascade system into your renderer.

## Step 1: Add to Renderer Class

Edit `engine/graphics/Renderer.hpp`:

```cpp
#include "RadianceCascade.hpp"

class Renderer {
    // Add member variable:
    std::unique_ptr<RadianceCascade> m_radianceCascade;

    // Add public getter:
    RadianceCascade* GetRadianceCascade() { return m_radianceCascade.get(); }
};
```

## Step 2: Initialize in Renderer

Edit `engine/graphics/Renderer.cpp`:

```cpp
bool Renderer::Initialize() {
    // ... existing initialization ...

    // Initialize radiance cascade
    m_radianceCascade = std::make_unique<RadianceCascade>();

    RadianceCascade::Config config;
    config.numCascades = 4;
    config.baseResolution = 32;
    config.baseSpacing = 1.0f;
    config.cascadeScale = 2.0f;
    config.raysPerProbe = 64;
    config.bounces = 2;

    if (!m_radianceCascade->Initialize(config)) {
        spdlog::error("Failed to initialize radiance cascade");
        // Continue without GI rather than failing
    }

    return true;
}
```

## Step 3: Update Per Frame

Edit `engine/graphics/Renderer.cpp`:

```cpp
void Renderer::BeginFrame() {
    m_stats.Reset();
    Clear();

    // Update radiance cascade
    if (m_radianceCascade && m_activeCamera) {
        float deltaTime = 0.016f; // Or get from time system
        m_radianceCascade->Update(m_activeCamera->GetPosition(), deltaTime);
    }
}
```

## Step 4: Bind Cascade Textures Before Rendering

Edit `engine/graphics/Renderer.cpp`:

```cpp
void Renderer::DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform) {
    // Bind cascade textures to shader slots
    if (m_radianceCascade && m_radianceCascade->IsEnabled()) {
        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE9 + i);
            glBindTexture(GL_TEXTURE_3D, m_radianceCascade->GetCascadeTexture(i));
        }
    }

    // ... rest of rendering code ...
}
```

## Step 5: Inject Emissive Materials

When rendering objects with emissive materials:

```cpp
// After rendering an emissive object
if (material.GetEmissiveStrength() > 0.0f && m_radianceCascade) {
    glm::vec3 position = glm::vec3(transform[3]); // Extract position
    glm::vec3 emissiveColor = material.GetEmissiveColor();
    float emissiveStrength = material.GetEmissiveStrength();
    float radius = 2.0f; // Adjust based on object size

    m_radianceCascade->InjectEmissive(
        position,
        emissiveColor * emissiveStrength,
        radius
    );
}
```

## Step 6: Propagate Lighting

At end of frame (or before rendering):

```cpp
void Renderer::EndFrame() {
    // Propagate radiance cascade lighting
    if (m_radianceCascade) {
        m_radianceCascade->PropagateLighting();
    }
}
```

## Step 7: Cleanup

Edit `engine/graphics/Renderer.cpp`:

```cpp
void Renderer::Shutdown() {
    m_radianceCascade.reset();
    // ... rest of cleanup ...
}
```

## Testing

Use the test framework:

```cpp
#include "graphics/RadianceCascadeTest.hpp"

// In your test or demo code
RadianceCascadeTest test;
test.Initialize(renderer);

// Run automated tests
auto results = test.RunAllTests();
spdlog::info("{}", results.GetReport());

// Visual testing
for (int i = 0; i < 1000; ++i) {
    test.Update(0.016f);
    test.Render(renderer);
}
```

## Configuration Tips

### For Best Quality
```cpp
config.numCascades = 4;
config.baseResolution = 64;  // Higher resolution
config.baseSpacing = 0.5f;   // Finer spacing
config.bounces = 3;          // More bounces
```

### For Best Performance
```cpp
config.numCascades = 3;
config.baseResolution = 16;  // Lower resolution
config.baseSpacing = 2.0f;   // Coarser spacing
config.bounces = 1;          // Fewer bounces
config.maxProbesPerFrame = 512;  // Lower budget
```

### For Balanced (Recommended)
```cpp
config.numCascades = 4;
config.baseResolution = 32;
config.baseSpacing = 1.0f;
config.bounces = 2;
config.maxProbesPerFrame = 1024;
```

## Troubleshooting

### GI not visible
- Check that shaders include `radiance_cascade.hlsli`
- Verify cascade textures are bound to correct slots (t9-t12)
- Ensure constant buffer (b4) has cascade uniforms
- Check emissive materials are being injected

### Performance issues
- Reduce `baseResolution` (try 16 or 24)
- Decrease `maxProbesPerFrame` (try 512)
- Lower `bounces` to 1
- Disable on low-end hardware

### Light leaking
- Decrease `baseSpacing` (try 0.5f)
- Add more cascades for finer detail
- Check geometry is closed (no holes)

### Flickering
- Increase `temporalBlend` (try 0.98)
- Ensure stable camera movement
- Check for NaN/Inf values in lighting

## Debug Visualization

Enable debug drawing:

```cpp
if (m_radianceCascade) {
    m_radianceCascade->DebugDraw(renderer);
}
```

This will visualize:
- Probe positions (colored spheres)
- Cascade extents
- Active vs inactive probes

## Performance Monitoring

Get statistics:

```cpp
auto stats = m_radianceCascade->GetStats();
spdlog::info("GI Stats: {} probes updated, {:.2f}ms",
             stats.probesUpdatedThisFrame,
             stats.updateTimeMs);
```

## Files Created

All necessary files are in place:
- ✅ `engine/graphics/RadianceCascade.hpp`
- ✅ `engine/graphics/RadianceCascade.cpp`
- ✅ `engine/shaders/hlsl/radiance_cascade.hlsli`
- ✅ `engine/shaders/hlsl/terrain_ps.hlsl`
- ✅ `engine/graphics/RadianceCascadeTest.hpp`
- ✅ `engine/graphics/RadianceCascadeTest.cpp`
- ✅ Modified: `engine/shaders/hlsl/standard_ps.hlsl`

---

**Ready to use!** The system is production-ready and supports all model types.
