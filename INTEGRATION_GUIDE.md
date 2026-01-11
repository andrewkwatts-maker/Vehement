# Unified Rendering Pipeline Integration Guide

## Quick Start

### 1. Include Headers
```cpp
#include "graphics/RenderBackend.hpp"
#include "graphics/SDFRasterizer.hpp"
#include "graphics/PolygonRasterizer.hpp"
#include "graphics/HybridRasterizer.hpp"
```

### 2. Create Renderer in Your Application Class
```cpp
class GameApplication {
private:
    std::unique_ptr<Nova::IRenderBackend> m_renderer;
    Nova::QualitySettings m_renderSettings;

public:
    bool Initialize() {
        // Load settings from JSON
        LoadRenderSettings("assets/config/render_settings.json");

        // Create renderer based on settings
        auto backendType = Nova::RenderBackendFactory::BackendType::Hybrid;
        m_renderer = Nova::RenderBackendFactory::Create(backendType);

        if (!m_renderer->Initialize(m_windowWidth, m_windowHeight)) {
            spdlog::error("Failed to initialize renderer");
            return false;
        }

        m_renderer->SetQualitySettings(m_renderSettings);
        return true;
    }

    void Render() {
        m_renderer->BeginFrame(m_camera);
        m_renderer->Render(m_scene, m_camera);
        m_renderer->EndFrame();

        // Display output
        auto colorTexture = m_renderer->GetOutputColor();
        // Blit to screen or use in post-processing
    }
};
```

### 3. Load Settings from JSON
```cpp
void LoadRenderSettings(const std::string& path) {
    // Use your JSON library (e.g., nlohmann/json)
    std::ifstream file(path);
    json config = json::parse(file);

    // Parse render mode
    std::string mode = config["renderMode"];
    if (mode == "sdf_first") {
        m_renderSettings.renderOrder = Nova::QualitySettings::RenderOrder::SDFFirst;
    } else if (mode == "polygon_first") {
        m_renderSettings.renderOrder = Nova::QualitySettings::RenderOrder::PolygonFirst;
    } else if (mode == "hybrid") {
        m_renderSettings.renderOrder = Nova::QualitySettings::RenderOrder::Auto;
    }

    // Parse SDF settings
    m_renderSettings.sdfTileSize = config["sdfRasterizer"]["tileSize"];
    m_renderSettings.maxRaymarchSteps = config["sdfRasterizer"]["maxRaymarchSteps"];
    m_renderSettings.sdfEnableShadows = config["sdfRasterizer"]["enableShadows"];
    m_renderSettings.sdfEnableAO = config["sdfRasterizer"]["enableAO"];
    m_renderSettings.sdfAOSamples = config["sdfRasterizer"]["aoSamples"];

    // Parse polygon settings
    m_renderSettings.shadowMapSize = config["polygonRasterizer"]["shadowMapSize"];
    m_renderSettings.cascadeCount = config["polygonRasterizer"]["cascadeCount"];

    // Parse hybrid settings
    m_renderSettings.enableDepthInterleaving =
        config["hybridSettings"]["enableDepthInterleaving"];
}
```

### 4. Integrate with Scene System

#### Option A: Automatic Classification
```cpp
// In your Scene class
void Scene::ClassifyObjectsForRendering(Nova::IRenderBackend* renderer) {
    // Check if it's a hybrid renderer
    if (renderer->SupportsFeature(Nova::RenderFeature::HybridRendering)) {
        auto* hybridRenderer = dynamic_cast<Nova::HybridRasterizer*>(renderer);

        for (auto& node : m_sceneNodes) {
            if (node->HasSDFComponent()) {
                // Submit to SDF rasterizer
                auto& sdfRasterizer = hybridRenderer->GetSDFRasterizer();
                Nova::SDFObjectGPU sdfObj = node->GetSDFData();
                sdfRasterizer.AddSDFObject(sdfObj);
            }

            if (node->HasMeshComponent()) {
                // Submit to polygon rasterizer
                auto& polygonRasterizer = hybridRenderer->GetPolygonRasterizer();
                auto mesh = node->GetMesh();
                auto material = node->GetMaterial();
                auto transform = node->GetWorldTransform();
                polygonRasterizer.SubmitMesh(mesh, material, transform);
            }
        }
    }
}
```

#### Option B: Manual Submission
```cpp
void GameApplication::SubmitRenderObjects() {
    // For SDF objects
    if (m_renderer->SupportsFeature(Nova::RenderFeature::SDFRendering)) {
        for (auto& sdfShape : m_sdfShapes) {
            Nova::SDFObjectGPU obj;
            obj.transform = sdfShape.transform;
            obj.inverseTransform = glm::inverse(obj.transform);
            obj.bounds = glm::vec4(sdfShape.center, sdfShape.radius);
            obj.material = glm::vec4(sdfShape.color, sdfShape.roughness);
            obj.params = glm::vec4(sdfShape.type, 0, sdfShape.metallic, 0);

            auto* sdfRasterizer = GetSDFRasterizer(m_renderer.get());
            sdfRasterizer->AddSDFObject(obj);
        }
    }

    // For polygon objects
    if (m_renderer->SupportsFeature(Nova::RenderFeature::PolygonRendering)) {
        for (auto& entity : m_entities) {
            auto* polygonRasterizer = GetPolygonRasterizer(m_renderer.get());
            polygonRasterizer->SubmitMesh(
                entity.mesh,
                entity.material,
                entity.transform
            );
        }
    }
}
```

### 5. Hotkey Backend Switching (F5)
```cpp
void GameApplication::Update(float deltaTime) {
    // Check for F5 key press
    if (m_input->IsKeyPressed(Key::F5)) {
        CycleRenderBackend();
    }

    // Update settings menu
    if (m_settingsMenu.IsOpen()) {
        m_settingsMenu.Render(&m_showSettings);

        // Check if backend changed
        if (m_settingsMenu.RenderBackendChanged()) {
            SwitchRenderBackend(m_settingsMenu.GetSelectedBackend());
        }
    }
}

void GameApplication::CycleRenderBackend() {
    static int currentBackend = 0;
    currentBackend = (currentBackend + 1) % 3;

    Nova::RenderBackendFactory::BackendType types[] = {
        Nova::RenderBackendFactory::BackendType::SDF,
        Nova::RenderBackendFactory::BackendType::Polygon,
        Nova::RenderBackendFactory::BackendType::Hybrid
    };

    SwitchRenderBackend(types[currentBackend]);
}

void GameApplication::SwitchRenderBackend(
    Nova::RenderBackendFactory::BackendType type
) {
    spdlog::info("Switching to backend: {}",
        Nova::RenderBackendFactory::GetBackendName(type));

    // Create new renderer
    auto newRenderer = Nova::RenderBackendFactory::Create(type);
    if (!newRenderer->Initialize(m_windowWidth, m_windowHeight)) {
        spdlog::error("Failed to initialize new renderer");
        return;
    }

    // Transfer settings
    newRenderer->SetQualitySettings(m_renderSettings);

    // Swap renderers
    m_renderer.reset();
    m_renderer = std::move(newRenderer);

    spdlog::info("Backend switched successfully");
}
```

### 6. Display Performance Stats
```cpp
void GameApplication::RenderUI() {
    // Get stats from renderer
    const auto& stats = m_renderer->GetStats();

    ImGui::Begin("Performance");
    ImGui::Text("Renderer: %s", m_renderer->GetName());
    ImGui::Separator();

    ImGui::Text("FPS: %d", stats.fps);
    ImGui::Text("Frame Time: %.2f ms", stats.frameTimeMs);
    ImGui::Text("CPU Time: %.2f ms", stats.cpuTimeMs);
    ImGui::Text("GPU Time: %.2f ms", stats.gpuTimeMs);

    ImGui::Separator();
    ImGui::Text("Draw Calls: %u", stats.drawCalls);
    ImGui::Text("Compute Dispatches: %u", stats.computeDispatches);
    ImGui::Text("Triangles: %u", stats.trianglesRendered);
    ImGui::Text("SDF Objects: %u", stats.sdfObjectsRendered);

    if (m_renderer->SupportsFeature(Nova::RenderFeature::TileBasedCulling)) {
        ImGui::Separator();
        ImGui::Text("Tiles Processed: %u", stats.tilesProcessed);
        ImGui::Text("Tiles Culled: %u", stats.tilesCulled);
        float cullingEfficiency =
            100.0f * stats.tilesCulled / std::max(1u, stats.tilesProcessed);
        ImGui::Text("Culling Efficiency: %.1f%%", cullingEfficiency);
    }

    ImGui::End();
}
```

### 7. Debug Visualization
```cpp
void GameApplication::ToggleDebugVisualization() {
    static bool showTiles = false;
    static bool showDepthBuffer = false;

    if (ImGui::BeginMenu("Debug Visualization")) {
        if (ImGui::MenuItem("Show Tiles", "T", &showTiles)) {
            m_renderSettings.showTiles = showTiles;
            m_renderer->SetQualitySettings(m_renderSettings);
        }

        if (ImGui::MenuItem("Show Depth Buffer", "D", &showDepthBuffer)) {
            m_renderSettings.showDepthBuffer = showDepthBuffer;
            m_renderer->SetQualitySettings(m_renderSettings);
        }

        if (ImGui::MenuItem("Toggle Debug Mode", "F3")) {
            m_renderer->SetDebugMode(!m_debugMode);
            m_debugMode = !m_debugMode;
        }

        ImGui::EndMenu();
    }

    // Render depth buffer visualization
    if (showDepthBuffer) {
        auto depthTexture = m_renderer->GetOutputDepth();
        ImGui::Begin("Depth Buffer");
        ImGui::Image(
            reinterpret_cast<ImTextureID>(depthTexture->GetID()),
            ImVec2(512, 512)
        );
        ImGui::End();
    }
}
```

## Advanced Usage

### Custom SDF Shapes
```cpp
// Create a custom SDF shape
Nova::SDFObjectGPU CreateCustomSDFShape() {
    Nova::SDFObjectGPU obj;

    // Set transform
    obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 5, 0));
    obj.transform = glm::scale(obj.transform, glm::vec3(2.0f));
    obj.inverseTransform = glm::inverse(obj.transform);

    // Set bounding sphere
    glm::vec3 center = glm::vec3(0, 5, 0);
    float radius = 2.5f;
    obj.bounds = glm::vec4(center, radius);

    // Set material (PBR)
    glm::vec3 albedo = glm::vec3(0.8f, 0.2f, 0.2f); // Red
    float roughness = 0.5f;
    obj.material = glm::vec4(albedo, roughness);

    // Set parameters
    int type = Nova::SDFObjectGPU::TYPE_SPHERE;
    float metallic = 0.0f;
    float emission = 0.0f;
    obj.params = glm::vec4(type, 0, metallic, emission);

    return obj;
}
```

### Instanced Polygon Rendering
```cpp
// Render many instances of same mesh efficiently
void RenderForest(Nova::PolygonRasterizer* renderer) {
    auto treeMesh = m_assetManager->LoadMesh("tree.obj");
    auto treeMaterial = m_assetManager->LoadMaterial("bark.mat");

    std::vector<glm::mat4> treeTransforms;
    for (int i = 0; i < 1000; i++) {
        glm::vec3 position = GenerateTreePosition(i);
        float rotation = GenerateTreeRotation(i);
        glm::vec3 scale = GenerateTreeScale(i);

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
        transform = glm::rotate(transform, rotation, glm::vec3(0, 1, 0));
        transform = glm::scale(transform, scale);

        treeTransforms.push_back(transform);
    }

    // Submit as single instanced draw call
    renderer->SubmitInstanced(treeMesh, treeMaterial, treeTransforms);
}
```

### Quality Presets
```cpp
void ApplyQualityPreset(const std::string& preset) {
    if (preset == "low") {
        m_renderSettings.sdfTileSize = 32;
        m_renderSettings.maxRaymarchSteps = 64;
        m_renderSettings.shadowMapSize = 1024;
        m_renderSettings.cascadeCount = 2;
        m_renderSettings.sdfEnableShadows = false;
        m_renderSettings.sdfEnableAO = false;
    } else if (preset == "medium") {
        m_renderSettings.sdfTileSize = 16;
        m_renderSettings.maxRaymarchSteps = 96;
        m_renderSettings.shadowMapSize = 2048;
        m_renderSettings.cascadeCount = 3;
        m_renderSettings.sdfEnableShadows = true;
        m_renderSettings.sdfEnableAO = false;
    } else if (preset == "high") {
        m_renderSettings.sdfTileSize = 16;
        m_renderSettings.maxRaymarchSteps = 128;
        m_renderSettings.shadowMapSize = 2048;
        m_renderSettings.cascadeCount = 4;
        m_renderSettings.sdfEnableShadows = true;
        m_renderSettings.sdfEnableAO = true;
        m_renderSettings.sdfAOSamples = 4;
    } else if (preset == "ultra") {
        m_renderSettings.sdfTileSize = 8;
        m_renderSettings.maxRaymarchSteps = 256;
        m_renderSettings.shadowMapSize = 4096;
        m_renderSettings.cascadeCount = 4;
        m_renderSettings.sdfEnableShadows = true;
        m_renderSettings.sdfEnableAO = true;
        m_renderSettings.sdfAOSamples = 8;
    }

    m_renderer->SetQualitySettings(m_renderSettings);
}
```

## CMake Integration

Add to your CMakeLists.txt:
```cmake
# Rendering pipeline sources
set(RENDER_BACKEND_SOURCES
    engine/graphics/RenderBackend.hpp
    engine/graphics/SDFRasterizer.hpp
    engine/graphics/SDFRasterizer.cpp
    engine/graphics/PolygonRasterizer.hpp
    engine/graphics/PolygonRasterizer.cpp
    engine/graphics/HybridRasterizer.hpp
    engine/graphics/HybridRasterizer.cpp
    engine/graphics/HybridDepthMerge.hpp
    engine/graphics/HybridDepthMerge.cpp
)

add_library(RenderBackend ${RENDER_BACKEND_SOURCES})
target_link_libraries(RenderBackend PUBLIC OpenGL::GL GLEW::GLEW glm::glm)

# Link to main executable
target_link_libraries(YourGame PRIVATE RenderBackend)
```

## Shader Compilation

Make sure compute shaders are compiled and accessible:
```cmake
# Copy shaders to output directory
add_custom_command(TARGET YourGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/shaders
    $<TARGET_FILE_DIR:YourGame>/assets/shaders
)
```

## Testing

```cpp
// Unit test for backend switching
TEST(RenderBackend, SwitchBackends) {
    auto sdfRenderer = Nova::RenderBackendFactory::Create(
        Nova::RenderBackendFactory::BackendType::SDF);
    ASSERT_TRUE(sdfRenderer->Initialize(1280, 720));

    auto polygonRenderer = Nova::RenderBackendFactory::Create(
        Nova::RenderBackendFactory::BackendType::Polygon);
    ASSERT_TRUE(polygonRenderer->Initialize(1280, 720));

    auto hybridRenderer = Nova::RenderBackendFactory::Create(
        Nova::RenderBackendFactory::BackendType::Hybrid);
    ASSERT_TRUE(hybridRenderer->Initialize(1280, 720));
}

// Performance benchmark
TEST(RenderBackend, PerformanceBenchmark) {
    auto renderer = Nova::RenderBackendFactory::Create(
        Nova::RenderBackendFactory::BackendType::Hybrid);
    renderer->Initialize(1920, 1080);

    Scene testScene = CreateTestScene(); // 1000 objects
    Camera camera;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        renderer->BeginFrame(camera);
        renderer->Render(testScene, camera);
        renderer->EndFrame();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start);

    float avgFrameTime = duration.count() / 100.0f;
    EXPECT_LT(avgFrameTime, 16.67f); // Should hit 60 FPS
}
```

## Troubleshooting

### Issue: Black screen with SDF renderer
**Solution:** Check that compute shaders are loading correctly:
```cpp
if (!m_raymarchShader->LoadCompute("assets/shaders/sdf_rasterize_tile.comp")) {
    spdlog::error("Failed to load SDF compute shader");
    spdlog::error("Shader log: {}", m_raymarchShader->GetErrorLog());
}
```

### Issue: Depth fighting between SDF and polygons
**Solution:** Increase depth bias:
```cpp
m_renderSettings.depthBias = 0.001f; // Increase from 0.0001f
m_depthMerge->SetDepthBias(m_renderSettings.depthBias);
```

### Issue: Low FPS with SDF rendering
**Solution:** Reduce tile size or raymarch steps:
```cpp
m_renderSettings.sdfTileSize = 32; // Larger tiles = fewer dispatches
m_renderSettings.maxRaymarchSteps = 64; // Fewer steps = faster
```

### Issue: Tiles not culling properly
**Solution:** Check AABB calculation:
```cpp
// Enable debug visualization
m_renderSettings.showTiles = true;
m_renderer->SetDebugMode(true);
```

## Best Practices

1. **Use Hybrid Mode for Production**
   - Provides maximum flexibility
   - Automatically optimizes render order
   - Seamless Z-buffer integration

2. **Profile Before Optimizing**
   - Use performance overlay
   - Compare backend performance
   - Identify bottlenecks

3. **Quality Presets for Scalability**
   - Start with "High" preset
   - Let users choose based on hardware
   - Auto-detect and recommend preset

4. **Batch Similar Objects**
   - Use instanced rendering for repeated meshes
   - Group SDF objects by type
   - Minimize state changes

5. **Debug Visualization During Development**
   - Show tile boundaries
   - Display depth buffer
   - Visualize culling efficiency

---

## Complete Example Application

See `examples/unified_renderer_demo.cpp` for a complete working example that demonstrates:
- Backend creation and initialization
- Scene object submission
- Hotkey backend switching
- Performance statistics display
- Debug visualization
- Quality preset selection
- Settings persistence

---

## Support and Documentation

For more information:
- See `UNIFIED_RENDERING_PIPELINE_SUMMARY.md` for architecture details
- Check shader comments in `assets/shaders/*.comp`
- Review `render_settings.json` for all configuration options
- Read inline code documentation in header files

## License

This implementation is part of the Old3DEngine project and follows the project's license terms.
