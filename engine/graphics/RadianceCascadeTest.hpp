#pragma once

#include "RadianceCascade.hpp"
#include "../scene/Scene.hpp"
#include <memory>
#include <vector>

namespace Nova {

class Renderer;
class Mesh;
class Material;
class SDFModel;
class VoxelTerrain;

/**
 * @brief Test scene for radiance cascade system
 *
 * Creates a comprehensive test environment with:
 * - Standard mesh models (buildings, props)
 * - SDF models (units, procedural objects)
 * - Voxel terrain
 * - Various light sources
 * - Emissive materials
 *
 * Tests all radiance cascade features:
 * - Indirect diffuse lighting
 * - Indirect specular reflections
 * - Color bleeding between surfaces
 * - Emissive material contribution
 * - Multi-bounce light transport
 * - Dynamic updates
 */
class RadianceCascadeTest {
public:
    RadianceCascadeTest();
    ~RadianceCascadeTest();

    /**
     * @brief Initialize test scene
     */
    bool Initialize(Renderer* renderer);

    /**
     * @brief Update test scene
     */
    void Update(float deltaTime);

    /**
     * @brief Render test scene
     */
    void Render(Renderer* renderer);

    /**
     * @brief Get radiance cascade instance
     */
    RadianceCascade* GetRadianceCascade() { return m_radianceCascade.get(); }

    /**
     * @brief Test scenarios
     */
    void TestScenario1_BasicIndirectLighting();
    void TestScenario2_ColorBleeding();
    void TestScenario3_EmissiveSurfaces();
    void TestScenario4_DynamicObjects();
    void TestScenario5_ComplexGeometry();

    /**
     * @brief Validate results
     */
    struct TestResults {
        bool meshesReceiveGI = false;
        bool sdfReceivesGI = false;
        bool terrainReceivesGI = false;
        bool emissiveContributes = false;
        bool colorBleedingWorks = false;
        bool dynamicUpdateWorks = false;

        std::string GetReport() const;
    };

    TestResults RunAllTests();

private:
    void CreateTestMeshes();
    void CreateTestSDFs();
    void CreateTestTerrain();
    void CreateTestLights();
    void CreateEmissiveMaterials();

    std::unique_ptr<RadianceCascade> m_radianceCascade;
    Renderer* m_renderer = nullptr;

    // Test objects
    struct TestObject {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        glm::mat4 transform;
        std::string name;
        bool isEmissive = false;
    };

    std::vector<TestObject> m_meshObjects;
    std::vector<std::shared_ptr<SDFModel>> m_sdfObjects;
    std::shared_ptr<VoxelTerrain> m_terrain;

    // Test lights
    struct TestLight {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
        float radius;
    };

    std::vector<TestLight> m_lights;

    bool m_initialized = false;
};

/**
 * @brief Example usage of radiance cascade test
 */
inline void ExampleRadianceCascadeUsage(Renderer* renderer) {
    // Create test scene
    RadianceCascadeTest test;
    test.Initialize(renderer);

    // Run comprehensive tests
    auto results = test.RunAllTests();
    spdlog::info("Radiance Cascade Test Results:\n{}", results.GetReport());

    // Example: Test basic indirect lighting
    test.TestScenario1_BasicIndirectLighting();

    // Example: Test color bleeding
    test.TestScenario2_ColorBleeding();

    // Example: Test emissive surfaces
    test.TestScenario3_EmissiveSurfaces();

    // Render loop
    float deltaTime = 0.016f;
    for (int frame = 0; frame < 100; ++frame) {
        test.Update(deltaTime);
        test.Render(renderer);
    }
}

} // namespace Nova
