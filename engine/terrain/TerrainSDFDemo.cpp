/**
 * @file TerrainSDFDemo.cpp
 * @brief Demonstration of SDF terrain with full global illumination
 *
 * This example shows how to:
 * 1. Convert traditional heightmap terrain to SDF representation
 * 2. Integrate with radiance cascade global illumination
 * 3. Achieve 120 FPS with full GI on terrain
 *
 * Performance targets:
 * - Primary pass (rasterization): ~0.5-1ms
 * - GI pass (SDF raymarching): ~2-4ms
 * - Composite + TAA: ~0.5ms
 * - Total: ~3-6ms per frame (166-333 FPS, capped at 120)
 */

#include "terrain/SDFTerrain.hpp"
#include "terrain/HybridTerrainRenderer.hpp"
#include "terrain/TerrainGenerator.hpp"
#include "graphics/RadianceCascade.hpp"
#include "scene/Camera.hpp"
#include "scene/FlyCamera.hpp"
#include "core/Engine.hpp"
#include "core/Logger.hpp"
#include "core/Time.hpp"
#include "graphics/Shader.hpp"

#include <spdlog/spdlog.h>
#include <memory>

using namespace Nova;

class TerrainSDFDemo {
public:
    TerrainSDFDemo() = default;

    bool Initialize() {
        spdlog::info("=== Terrain SDF + GI Demo ===");
        spdlog::info("Initializing demo...");

        // Create camera
        m_camera = std::make_unique<FlyCamera>();
        m_camera->SetPosition(glm::vec3(0, 100, 0));
        m_camera->SetPerspective(60.0f, 1920.0f / 1080.0f, 0.1f, 5000.0f);
        m_camera->SetMoveSpeed(50.0f);

        // Create traditional terrain generator
        spdlog::info("Creating terrain generator...");
        m_terrainGen = std::make_unique<TerrainGenerator>();
        if (!m_terrainGen->Initialize()) {
            spdlog::error("Failed to initialize terrain generator");
            return false;
        }

        // Configure terrain parameters
        m_terrainGen->SetChunkSize(64);
        m_terrainGen->SetViewDistance(8);
        m_terrainGen->SetHeightScale(100.0f);
        m_terrainGen->SetNoiseParams(0.01f, 6, 0.5f, 2.0f);

        // Create SDF terrain representation
        spdlog::info("Creating SDF terrain...");
        m_sdfTerrain = std::make_unique<SDFTerrain>();

        SDFTerrain::Config sdfConfig;
        sdfConfig.resolution = 512;             // 512^3 voxels
        sdfConfig.worldSize = 1000.0f;          // 1km x 1km
        sdfConfig.maxHeight = 150.0f;           // 150m max height
        sdfConfig.octreeLevels = 6;             // 6-level octree
        sdfConfig.useOctree = true;             // Enable acceleration
        sdfConfig.highPrecision = false;        // 8-bit is enough
        sdfConfig.compressGPU = true;           // BC4 compression
        sdfConfig.supportCaves = false;         // Simple heightfield
        sdfConfig.storeMaterialPerVoxel = true; // Material per voxel

        if (!m_sdfTerrain->Initialize(sdfConfig)) {
            spdlog::error("Failed to initialize SDF terrain");
            return false;
        }

        // Build SDF from terrain generator
        spdlog::info("Building SDF from terrain...");
        m_sdfTerrain->BuildFromTerrainGenerator(*m_terrainGen);

        // Wait for build to complete
        while (m_sdfTerrain->IsBuilding()) {
            float progress = m_sdfTerrain->GetBuildProgress();
            spdlog::info("Building SDF: {:.1f}%", progress * 100.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        auto stats = m_sdfTerrain->GetStats();
        spdlog::info("SDF built: {:.2f}ms, {} voxels, {} octree nodes",
                     stats.buildTimeMs, stats.nonEmptyVoxels, stats.octreeNodes);

        // Create radiance cascade for GI
        spdlog::info("Creating radiance cascade GI...");
        m_radianceCascade = std::make_unique<RadianceCascade>();

        RadianceCascade::Config giConfig;
        giConfig.numCascades = 4;
        giConfig.baseResolution = 32;
        giConfig.cascadeScale = 2.0f;
        giConfig.origin = glm::vec3(0, 0, 0);
        giConfig.baseSpacing = 2.0f;
        giConfig.updateRadius = 500.0f;
        giConfig.raysPerProbe = 64;
        giConfig.bounces = 2;
        giConfig.asyncUpdate = true;
        giConfig.maxProbesPerFrame = 512;
        giConfig.temporalBlend = 0.9f;

        if (!m_radianceCascade->Initialize(giConfig)) {
            spdlog::error("Failed to initialize radiance cascade");
            return false;
        }

        // Create hybrid renderer
        spdlog::info("Creating hybrid terrain renderer...");
        m_renderer = std::make_unique<HybridTerrainRenderer>();

        HybridTerrainRenderer::Config renderConfig;
        renderConfig.usePrimaryRasterization = true;    // Hybrid mode
        renderConfig.enableGI = true;
        renderConfig.enableShadows = true;
        renderConfig.enableReflections = false;         // Not needed for terrain
        renderConfig.enableAO = true;
        renderConfig.giSamples = 1;                     // 1 SPP for 120 FPS
        renderConfig.shadowSamples = 1;
        renderConfig.aoSamples = 4;
        renderConfig.giIntensity = 1.0f;
        renderConfig.shadowSoftness = 2.0f;
        renderConfig.useTemporalAccumulation = true;    // Reduce noise
        renderConfig.maxRaySteps = 64;
        renderConfig.maxRayDistance = 500.0f;
        renderConfig.useTriplanarMapping = true;
        renderConfig.blendMaterials = true;

        if (!m_renderer->Initialize(1920, 1080, renderConfig)) {
            spdlog::error("Failed to initialize hybrid renderer");
            return false;
        }

        spdlog::info("=== Initialization Complete ===\n");
        PrintControls();
        return true;
    }

    void Update(float deltaTime) {
        // Update camera
        m_camera->Update(deltaTime);

        // Update terrain LOD
        m_terrainGen->Update(m_camera->GetPosition());

        // Process pending terrain meshes (limit per frame)
        m_terrainGen->ProcessPendingMeshes(2);

        // Update radiance cascade
        m_radianceCascade->Update(m_camera->GetPosition(), deltaTime);

        // Inject sunlight into radiance cascade
        // (In a full implementation, would inject from all light sources)
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> radiance;

        // Sample terrain surface points and inject direct light
        const int numSamples = 100;
        for (int i = 0; i < numSamples; ++i) {
            float x = (rand() % 1000) - 500.0f;
            float z = (rand() % 1000) - 500.0f;
            float y = m_sdfTerrain->GetHeightAt(x, z);

            positions.push_back(glm::vec3(x, y, z));

            // Direct sunlight
            glm::vec3 sunColor = glm::vec3(1.0f, 0.95f, 0.8f) * 2.0f;
            radiance.push_back(sunColor);
        }

        m_radianceCascade->InjectDirectLighting(positions, radiance);
        m_radianceCascade->PropagateLighting();

        // Update FPS
        m_frameCount++;
        m_accumulatedTime += deltaTime;

        if (m_accumulatedTime >= 1.0f) {
            m_fps = m_frameCount / m_accumulatedTime;
            m_frameTime = (m_accumulatedTime / m_frameCount) * 1000.0f;
            m_frameCount = 0;
            m_accumulatedTime = 0.0f;

            // Log performance
            auto renderStats = m_renderer->GetStats();
            spdlog::info("FPS: {:.1f} | Frame: {:.2f}ms | Primary: {:.2f}ms | GI: {:.2f}ms | Tris: {}",
                         m_fps, m_frameTime,
                         renderStats.primaryPassMs,
                         renderStats.secondaryPassMs,
                         renderStats.trianglesRendered);
        }
    }

    void Render() {
        // Render terrain with full GI
        m_renderer->Render(*m_terrainGen, *m_sdfTerrain, *m_camera, m_radianceCascade.get());
    }

    void Shutdown() {
        spdlog::info("Shutting down demo...");

        if (m_renderer) m_renderer->Shutdown();
        if (m_radianceCascade) m_radianceCascade->Shutdown();
        if (m_sdfTerrain) m_sdfTerrain->Shutdown();
        if (m_terrainGen) m_terrainGen->Shutdown();

        spdlog::info("Demo shutdown complete");
    }

    void PrintControls() const {
        spdlog::info("=== Controls ===");
        spdlog::info("WASD - Move camera");
        spdlog::info("Mouse - Look around");
        spdlog::info("Space - Move up");
        spdlog::info("Shift - Move down");
        spdlog::info("1 - Toggle GI");
        spdlog::info("2 - Toggle Shadows");
        spdlog::info("3 - Toggle AO");
        spdlog::info("4 - Increase GI samples");
        spdlog::info("5 - Decrease GI samples");
        spdlog::info("R - Reset camera");
        spdlog::info("P - Print performance stats");
        spdlog::info("================\n");
    }

    void HandleKeyPress(int key) {
        auto& config = m_renderer->GetConfig();

        switch (key) {
            case '1':
                config.enableGI = !config.enableGI;
                spdlog::info("GI: {}", config.enableGI ? "ON" : "OFF");
                break;

            case '2':
                config.enableShadows = !config.enableShadows;
                spdlog::info("Shadows: {}", config.enableShadows ? "ON" : "OFF");
                break;

            case '3':
                config.enableAO = !config.enableAO;
                spdlog::info("AO: {}", config.enableAO ? "ON" : "OFF");
                break;

            case '4':
                config.giSamples = std::min(config.giSamples + 1, 16);
                spdlog::info("GI Samples: {}", config.giSamples);
                m_renderer->ResetAccumulation();
                break;

            case '5':
                config.giSamples = std::max(config.giSamples - 1, 1);
                spdlog::info("GI Samples: {}", config.giSamples);
                m_renderer->ResetAccumulation();
                break;

            case 'R':
            case 'r':
                m_camera->SetPosition(glm::vec3(0, 100, 0));
                spdlog::info("Camera reset");
                break;

            case 'P':
            case 'p':
                PrintPerformanceStats();
                break;
        }

        m_renderer->SetConfig(config);
    }

    void PrintPerformanceStats() const {
        auto renderStats = m_renderer->GetStats();
        auto terrainStats = m_terrainGen->GetStats();
        auto sdfStats = m_sdfTerrain->GetStats();
        auto giStats = m_radianceCascade->GetStats();

        spdlog::info("\n=== Performance Statistics ===");
        spdlog::info("FPS: {:.1f}", m_fps);
        spdlog::info("Frame Time: {:.2f}ms", m_frameTime);
        spdlog::info("\nRendering:");
        spdlog::info("  Primary Pass: {:.2f}ms", renderStats.primaryPassMs);
        spdlog::info("  Secondary Pass: {:.2f}ms", renderStats.secondaryPassMs);
        spdlog::info("  Total: {:.2f}ms", renderStats.totalFrameMs);
        spdlog::info("  Triangles: {}", renderStats.trianglesRendered);
        spdlog::info("  Avg Ray Steps: {}", renderStats.avgRaySteps);
        spdlog::info("\nTerrain:");
        spdlog::info("  Total Chunks: {}", terrainStats.totalChunks);
        spdlog::info("  Visible Chunks: {}", terrainStats.visibleChunks);
        spdlog::info("  Pending: {}", terrainStats.pendingChunks);
        spdlog::info("\nSDF:");
        spdlog::info("  Total Voxels: {}", sdfStats.totalVoxels);
        spdlog::info("  Non-Empty: {}", sdfStats.nonEmptyVoxels);
        spdlog::info("  Octree Nodes: {}", sdfStats.octreeNodes);
        spdlog::info("  Memory: {:.2f} MB", sdfStats.memoryBytes / (1024.0f * 1024.0f));
        spdlog::info("\nGlobal Illumination:");
        spdlog::info("  Total Probes: {}", giStats.totalProbes);
        spdlog::info("  Active Probes: {}", giStats.activeProbes);
        spdlog::info("  Update Time: {:.2f}ms", giStats.updateTimeMs);
        spdlog::info("  Propagation Time: {:.2f}ms", giStats.propagationTimeMs);
        spdlog::info("==============================\n");
    }

    FlyCamera* GetCamera() { return m_camera.get(); }

private:
    std::unique_ptr<FlyCamera> m_camera;
    std::unique_ptr<TerrainGenerator> m_terrainGen;
    std::unique_ptr<SDFTerrain> m_sdfTerrain;
    std::unique_ptr<RadianceCascade> m_radianceCascade;
    std::unique_ptr<HybridTerrainRenderer> m_renderer;

    // Performance tracking
    float m_fps = 0.0f;
    float m_frameTime = 0.0f;
    int m_frameCount = 0;
    float m_accumulatedTime = 0.0f;
};

// =============================================================================
// Main Function
// =============================================================================

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting Terrain SDF + GI Demo");

    TerrainSDFDemo demo;

    if (!demo.Initialize()) {
        spdlog::error("Failed to initialize demo");
        return -1;
    }

    // Main loop would go here
    // In a full application, this would be integrated with the engine's game loop

    spdlog::info("Demo initialized successfully");
    spdlog::info("Press any key to exit...");

    // Cleanup
    demo.Shutdown();

    return 0;
}
