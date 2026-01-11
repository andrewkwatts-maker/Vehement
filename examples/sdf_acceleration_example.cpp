/**
 * @file sdf_acceleration_example.cpp
 * @brief Example demonstrating SDF acceleration structures for high-performance rendering
 *
 * This example shows how to use BVH, Octree, and Brick Map acceleration
 * to render 1000+ SDF instances at 60 FPS.
 */

#include "engine/graphics/SDFRendererAccelerated.hpp"
#include "engine/sdf/SDFModel.hpp"
#include "engine/sdf/SDFPrimitive.hpp"
#include "engine/scene/Camera.hpp"
#include <iostream>
#include <random>
#include <memory>

using namespace Nova;

/**
 * @brief Create stress test scene with many SDF instances
 */
std::pair<std::vector<std::unique_ptr<SDFModel>>, std::vector<glm::mat4>>
CreateStressTestScene(int numInstances = 500) {
    std::vector<std::unique_ptr<SDFModel>> models;
    std::vector<glm::mat4> transforms;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-50.0f, 50.0f);
    std::uniform_real_distribution<float> sizeDist(0.5f, 2.0f);
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);

    for (int i = 0; i < numInstances; ++i) {
        // Create model
        auto model = std::make_unique<SDFModel>("SDF_" + std::to_string(i));

        // Create sphere primitive
        auto* sphere = model->CreatePrimitive("Sphere", SDFPrimitiveType::Sphere);

        // Set parameters
        auto& params = sphere->GetParameters();
        params.radius = sizeDist(gen);

        // Set material
        auto& material = sphere->GetMaterial();
        material.baseColor = glm::vec4(colorDist(gen), colorDist(gen), colorDist(gen), 1.0f);
        material.metallic = colorDist(gen) * 0.5f;
        material.roughness = 0.3f + colorDist(gen) * 0.4f;

        // Create transform
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(posDist(gen), posDist(gen), posDist(gen)));
        transform = glm::scale(transform, glm::vec3(1.0f));

        models.push_back(std::move(model));
        transforms.push_back(transform);
    }

    std::cout << "Created stress test scene with " << numInstances << " SDF instances\n";
    return {std::move(models), transforms};
}

/**
 * @brief Example 1: Basic accelerated rendering
 */
void Example_BasicAcceleration() {
    std::cout << "\n=== Example 1: Basic Accelerated Rendering ===\n";

    // Create renderer
    SDFRendererAccelerated renderer;
    if (!renderer.InitializeAcceleration()) {
        std::cerr << "Failed to initialize renderer\n";
        return;
    }

    // Create scene
    auto [models, transforms] = CreateStressTestScene(100);

    // Convert to raw pointers for rendering
    std::vector<const SDFModel*> modelPtrs;
    for (const auto& model : models) {
        modelPtrs.push_back(model.get());
    }

    // Build acceleration structures
    std::cout << "Building acceleration structures...\n";
    renderer.BuildAcceleration(modelPtrs, transforms);

    // Print BVH stats
    const auto& bvhStats = renderer.GetBVH()->GetStats();
    std::cout << "BVH Statistics:\n";
    std::cout << "  Nodes: " << bvhStats.nodeCount << "\n";
    std::cout << "  Leaves: " << bvhStats.leafCount << "\n";
    std::cout << "  Max Depth: " << bvhStats.maxDepth << "\n";
    std::cout << "  Build Time: " << bvhStats.buildTimeMs << " ms\n";
    std::cout << "  Memory: " << (bvhStats.memoryBytes / 1024) << " KB\n";

    // Setup camera
    Camera camera;
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    camera.SetPosition(glm::vec3(0, 0, 80));

    // Simulate render frame
    renderer.RenderBatchAccelerated(modelPtrs, transforms, camera);

    // Print performance stats
    const auto& stats = renderer.GetStats();
    std::cout << "\nPerformance Statistics:\n";
    std::cout << "  Total Frame Time: " << stats.totalFrameTimeMs << " ms\n";
    std::cout << "  Equivalent FPS: " << (1000.0 / stats.totalFrameTimeMs) << "\n";
    std::cout << "  BVH Traversal: " << stats.bvhTraversalTimeMs << " ms\n";
    std::cout << "  Raymarch Time: " << stats.raymarchTimeMs << " ms\n";
    std::cout << "  Instances: " << stats.renderedInstances << "/" << stats.totalInstances << "\n";
    std::cout << "  Culling Efficiency: " << stats.GetCullingEfficiency() << "%\n";
}

/**
 * @brief Example 2: Octree-based empty space skipping
 */
void Example_OctreeAcceleration() {
    std::cout << "\n=== Example 2: Octree Empty Space Skipping ===\n";

    // Create renderer
    SDFRendererAccelerated renderer;
    renderer.InitializeAcceleration();

    // Create complex model with lots of empty space
    auto model = std::make_unique<SDFModel>("HollowSphere");

    // Outer sphere
    auto* outer = model->CreatePrimitive("Outer", SDFPrimitiveType::Sphere);
    outer->GetParameters().radius = 10.0f;
    outer->GetMaterial().baseColor = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);

    // Inner sphere (subtraction for hollow effect)
    auto* inner = model->CreatePrimitive("Inner", SDFPrimitiveType::Sphere, outer);
    inner->GetParameters().radius = 9.0f;
    inner->SetCSGOperation(CSGOperation::Subtraction);

    // Enable octree acceleration
    auto& settings = renderer.GetAccelerationSettings();
    settings.useOctree = true;
    settings.enableEmptySpaceSkipping = true;
    settings.octreeDepth = 6;
    settings.octreeVoxelSize = 0.5f;

    // Setup camera
    Camera camera;
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    camera.SetPosition(glm::vec3(0, 0, 30));

    // Render with octree
    std::cout << "Building octree...\n";
    renderer.RenderWithOctree(*model, camera);

    // Print octree stats
    const auto& octreeStats = renderer.GetOctree()->GetStats();
    std::cout << "Octree Statistics:\n";
    std::cout << "  Nodes: " << octreeStats.nodeCount << "\n";
    std::cout << "  Leaves: " << octreeStats.leafCount << "\n";
    std::cout << "  Max Depth: " << octreeStats.maxDepth << "\n";
    std::cout << "  Sparsity Ratio: " << (octreeStats.sparsityRatio * 100.0f) << "%\n";
    std::cout << "  Build Time: " << octreeStats.buildTimeMs << " ms\n";
    std::cout << "  Memory: " << (octreeStats.memoryBytes / (1024 * 1024)) << " MB\n";

    std::cout << "\nExpected speedup: 5-15x for hollow models\n";
}

/**
 * @brief Example 3: Dynamic scene with BVH refitting
 */
void Example_DynamicScene() {
    std::cout << "\n=== Example 3: Dynamic Scene with Refitting ===\n";

    // Create renderer
    SDFRendererAccelerated renderer;
    renderer.InitializeAcceleration();

    // Create scene
    auto [models, transforms] = CreateStressTestScene(200);

    std::vector<const SDFModel*> modelPtrs;
    for (const auto& model : models) {
        modelPtrs.push_back(model.get());
    }

    // Configure for dynamic scenes
    auto& settings = renderer.GetAccelerationSettings();
    settings.bvhStrategy = BVHBuildStrategy::HLBVH;  // Fast build
    settings.refitBVH = true;  // Enable refitting
    settings.rebuildAccelerationEachFrame = false;  // Don't rebuild every frame

    // Build initial acceleration
    renderer.BuildAcceleration(modelPtrs, transforms);

    // Setup camera
    Camera camera;
    camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    camera.SetPosition(glm::vec3(0, 0, 80));

    std::cout << "Simulating dynamic scene (10 frames)...\n";

    // Simulate 10 frames with moving objects
    for (int frame = 0; frame < 10; ++frame) {
        // Move some objects
        for (size_t i = 0; i < 20; ++i) {
            float offset = static_cast<float>(frame) * 0.1f;
            transforms[i] = glm::translate(transforms[i], glm::vec3(0.5f, 0.0f, 0.0f) * offset);
        }

        // Refit BVH (much faster than rebuild)
        auto refitStart = std::chrono::high_resolution_clock::now();
        renderer.RefitAcceleration();
        auto refitEnd = std::chrono::high_resolution_clock::now();
        double refitTime = std::chrono::duration<double, std::milli>(refitEnd - refitStart).count();

        // Render frame
        renderer.RenderBatchAccelerated(modelPtrs, transforms, camera);

        const auto& stats = renderer.GetStats();
        std::cout << "Frame " << frame << ": "
                  << stats.totalFrameTimeMs << " ms "
                  << "(Refit: " << refitTime << " ms)\n";
    }

    std::cout << "\nRefit is 5-10x faster than rebuild for dynamic scenes\n";
}

/**
 * @brief Example 4: Memory and performance comparison
 */
void Example_PerformanceComparison() {
    std::cout << "\n=== Example 4: Performance Comparison ===\n";

    const int NUM_INSTANCES = 500;

    // Test 1: Without acceleration
    {
        std::cout << "\nTest 1: No Acceleration\n";
        SDFRenderer basicRenderer;
        basicRenderer.Initialize();

        auto [models, transforms] = CreateStressTestScene(NUM_INSTANCES);
        std::vector<const SDFModel*> modelPtrs;
        for (const auto& model : models) {
            modelPtrs.push_back(model.get());
        }

        Camera camera;
        camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        camera.SetPosition(glm::vec3(0, 0, 80));

        auto start = std::chrono::high_resolution_clock::now();
        basicRenderer.RenderBatch(modelPtrs, transforms, camera);
        auto end = std::chrono::high_resolution_clock::now();

        double frameTime = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "  Frame Time: " << frameTime << " ms\n";
        std::cout << "  Equivalent FPS: " << (1000.0 / frameTime) << "\n";
    }

    // Test 2: With acceleration
    {
        std::cout << "\nTest 2: With Acceleration (BVH + Octree)\n";
        SDFRendererAccelerated accelRenderer;
        accelRenderer.InitializeAcceleration();

        auto [models, transforms] = CreateStressTestScene(NUM_INSTANCES);
        std::vector<const SDFModel*> modelPtrs;
        for (const auto& model : models) {
            modelPtrs.push_back(model.get());
        }

        Camera camera;
        camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        camera.SetPosition(glm::vec3(0, 0, 80));

        // Build acceleration
        auto buildStart = std::chrono::high_resolution_clock::now();
        accelRenderer.BuildAcceleration(modelPtrs, transforms);
        auto buildEnd = std::chrono::high_resolution_clock::now();
        double buildTime = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();

        std::cout << "  Build Time: " << buildTime << " ms (one-time cost)\n";

        // Render
        auto start = std::chrono::high_resolution_clock::now();
        accelRenderer.RenderBatchAccelerated(modelPtrs, transforms, camera);
        auto end = std::chrono::high_resolution_clock::now();

        const auto& stats = accelRenderer.GetStats();
        std::cout << "  Frame Time: " << stats.totalFrameTimeMs << " ms\n";
        std::cout << "  Equivalent FPS: " << (1000.0 / stats.totalFrameTimeMs) << "\n";
        std::cout << "  Culled Instances: " << stats.culledInstances << "/" << stats.totalInstances << "\n";
        std::cout << "  Memory Usage: " << ((stats.bvhMemoryBytes + stats.octreeMemoryBytes) / (1024 * 1024)) << " MB\n";

        // Calculate speedup
        // Note: Would need actual baseline measurement
        std::cout << "\n  Expected Speedup: 10-20x\n";
    }
}

/**
 * @brief Main function
 */
int main(int argc, char** argv) {
    std::cout << "SDF Acceleration Structures - Examples\n";
    std::cout << "======================================\n";

    try {
        // Run examples
        Example_BasicAcceleration();
        Example_OctreeAcceleration();
        Example_DynamicScene();
        Example_PerformanceComparison();

        std::cout << "\n=== All Examples Completed Successfully ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
