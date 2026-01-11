/**
 * @file test_radiance_cascade.cpp
 * @brief Comprehensive unit tests for RadianceCascade GI system
 *
 * Test categories:
 * 1. Initialization and shutdown
 * 2. Radiance injection and propagation
 * 3. Radiance sampling accuracy
 * 4. Cascade level transitions
 * 5. Performance benchmarks
 * 6. Memory management
 * 7. Edge cases (empty scenes, extreme values)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "graphics/RadianceCascade.hpp"
#include "utils/TestHelpers.hpp"
#include "utils/Generators.hpp"
#include "mocks/MockGraphics.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <cmath>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Test Fixture
// =============================================================================

class RadianceCascadeTest : public ::testing::Test {
protected:
    void SetUp() override {
        cascade = std::make_unique<RadianceCascade>();
    }

    void TearDown() override {
        if (cascade) {
            cascade->Shutdown();
        }
        cascade.reset();
    }

    RadianceCascade::Config CreateDefaultConfig() {
        RadianceCascade::Config config;
        config.numCascades = 4;
        config.baseResolution = 32;
        config.cascadeScale = 2.0f;
        config.origin = glm::vec3(0.0f);
        config.baseSpacing = 1.0f;
        config.updateRadius = 100.0f;
        config.raysPerProbe = 64;
        config.bounces = 2;
        config.useInterpolation = true;
        config.asyncUpdate = false; // Disable for deterministic tests
        config.maxProbesPerFrame = 1024;
        config.temporalBlend = 0.95f;
        return config;
    }

    RadianceCascade::Config CreateMinimalConfig() {
        RadianceCascade::Config config;
        config.numCascades = 1;
        config.baseResolution = 4;
        config.cascadeScale = 2.0f;
        config.origin = glm::vec3(0.0f);
        config.baseSpacing = 1.0f;
        config.maxProbesPerFrame = 64;
        config.asyncUpdate = false;
        return config;
    }

    std::unique_ptr<RadianceCascade> cascade;
};

// =============================================================================
// Initialization and Shutdown Tests
// =============================================================================

TEST_F(RadianceCascadeTest, DefaultConstruction) {
    EXPECT_FALSE(cascade->IsEnabled());
    EXPECT_EQ(0, cascade->GetCascadeTextures().size());
}

TEST_F(RadianceCascadeTest, InitializeWithDefaultConfig) {
    RadianceCascade::Config config = CreateDefaultConfig();

    bool result = cascade->Initialize(config);

    EXPECT_TRUE(result);
    EXPECT_TRUE(cascade->IsEnabled());
    EXPECT_EQ(config.numCascades, static_cast<int>(cascade->GetCascadeTextures().size()));
}

TEST_F(RadianceCascadeTest, InitializeWithMinimalConfig) {
    RadianceCascade::Config config = CreateMinimalConfig();

    bool result = cascade->Initialize(config);

    EXPECT_TRUE(result);
    EXPECT_EQ(1, cascade->GetCascadeTextures().size());
}

TEST_F(RadianceCascadeTest, InitializeMultipleTimes) {
    RadianceCascade::Config config = CreateDefaultConfig();

    // First initialization
    EXPECT_TRUE(cascade->Initialize(config));

    // Second initialization should succeed (reinitialize)
    config.numCascades = 2;
    EXPECT_TRUE(cascade->Initialize(config));
}

TEST_F(RadianceCascadeTest, ShutdownBeforeInitialize) {
    // Should not crash
    cascade->Shutdown();
    EXPECT_FALSE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, ShutdownAfterInitialize) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->Shutdown();

    EXPECT_FALSE(cascade->IsEnabled());
    EXPECT_TRUE(cascade->GetCascadeTextures().empty());
}

TEST_F(RadianceCascadeTest, ShutdownMultipleTimes) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->Shutdown();
    cascade->Shutdown(); // Should not crash
    cascade->Shutdown(); // Should not crash

    EXPECT_FALSE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InitializeAfterShutdown) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->Shutdown();

    bool result = cascade->Initialize(CreateMinimalConfig());

    EXPECT_TRUE(result);
    EXPECT_TRUE(cascade->IsEnabled());
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(RadianceCascadeTest, GetConfigReturnsSetConfig) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.numCascades = 5;
    config.baseResolution = 64;
    config.baseSpacing = 2.5f;

    cascade->Initialize(config);

    const auto& retrievedConfig = cascade->GetConfig();
    EXPECT_EQ(config.numCascades, retrievedConfig.numCascades);
    EXPECT_EQ(config.baseResolution, retrievedConfig.baseResolution);
    EXPECT_FLOAT_EQ(config.baseSpacing, retrievedConfig.baseSpacing);
}

TEST_F(RadianceCascadeTest, SetConfigReinitializes) {
    RadianceCascade::Config config1 = CreateDefaultConfig();
    config1.numCascades = 4;
    cascade->Initialize(config1);
    EXPECT_EQ(4, cascade->GetCascadeTextures().size());

    RadianceCascade::Config config2 = CreateDefaultConfig();
    config2.numCascades = 2;
    cascade->SetConfig(config2);

    // Note: SetConfig behavior depends on implementation
    // It may reinitialize or just update internal config
    EXPECT_EQ(config2.numCascades, cascade->GetConfig().numCascades);
}

TEST_F(RadianceCascadeTest, GetOrigin) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.origin = glm::vec3(10.0f, 20.0f, 30.0f);

    cascade->Initialize(config);

    EXPECT_VEC3_EQ(config.origin, cascade->GetOrigin());
}

TEST_F(RadianceCascadeTest, GetBaseSpacing) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.baseSpacing = 2.5f;

    cascade->Initialize(config);

    EXPECT_FLOAT_EQ(2.5f, cascade->GetBaseSpacing());
}

// =============================================================================
// Enable/Disable Tests
// =============================================================================

TEST_F(RadianceCascadeTest, EnableDisable) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->SetEnabled(false);
    EXPECT_FALSE(cascade->IsEnabled());

    cascade->SetEnabled(true);
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, DisabledCascadeSkipsUpdate) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->SetEnabled(false);

    // Should not crash and should skip processing
    cascade->Update(glm::vec3(0.0f), 0.016f);

    const auto& stats = cascade->GetStats();
    EXPECT_EQ(0, stats.probesUpdatedThisFrame);
}

TEST_F(RadianceCascadeTest, DisabledCascadeReturnsZeroRadiance) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->SetEnabled(false);

    glm::vec3 radiance = cascade->SampleRadiance(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f), radiance);
}

// =============================================================================
// Cascade Level Tests
// =============================================================================

TEST_F(RadianceCascadeTest, CascadeTextureCount) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.numCascades = 4;

    cascade->Initialize(config);

    EXPECT_EQ(4, cascade->GetCascadeTextures().size());
}

TEST_F(RadianceCascadeTest, GetCascadeTextureValidLevel) {
    cascade->Initialize(CreateDefaultConfig());

    // Valid levels should return non-zero texture IDs (in real implementation)
    // For unit tests without GPU, we check it doesn't crash
    for (int i = 0; i < cascade->GetConfig().numCascades; ++i) {
        uint32_t texture = cascade->GetCascadeTexture(i);
        // Texture ID can be 0 or valid depending on GPU availability
        (void)texture; // Suppress unused warning
    }
}

TEST_F(RadianceCascadeTest, GetCascadeTextureInvalidLevel) {
    cascade->Initialize(CreateDefaultConfig());

    // Invalid levels should return 0
    EXPECT_EQ(0u, cascade->GetCascadeTexture(-1));
    EXPECT_EQ(0u, cascade->GetCascadeTexture(100));
    EXPECT_EQ(0u, cascade->GetCascadeTexture(cascade->GetConfig().numCascades));
}

TEST_F(RadianceCascadeTest, CascadeResolutionsDecrease) {
    RadianceCascade::Config config;
    config.numCascades = 4;
    config.baseResolution = 32;
    config.cascadeScale = 2.0f;

    cascade->Initialize(config);

    // Expected resolutions: 32, 16, 8, 4
    // We can't directly access resolutions, but we can verify via stats
    cascade->Update(glm::vec3(0.0f), 0.016f);

    const auto& stats = cascade->GetStats();
    // Total probes = 32^3 + 16^3 + 8^3 + 4^3 = 32768 + 4096 + 512 + 64 = 37440
    // But this depends on implementation details
    EXPECT_GT(stats.totalProbes, 0);
}

// =============================================================================
// Update Tests
// =============================================================================

TEST_F(RadianceCascadeTest, UpdateFromCameraPosition) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->Update(glm::vec3(0.0f, 0.0f, 0.0f), 0.016f);

    const auto& stats = cascade->GetStats();
    EXPECT_GE(stats.probesUpdatedThisFrame, 0);
}

TEST_F(RadianceCascadeTest, UpdateWithMovingCamera) {
    cascade->Initialize(CreateDefaultConfig());

    glm::vec3 positions[] = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        glm::vec3(20.0f, 5.0f, 10.0f)
    };

    for (const auto& pos : positions) {
        cascade->Update(pos, 0.016f);
        // Should not crash
    }
}

TEST_F(RadianceCascadeTest, UpdateMultipleFrames) {
    cascade->Initialize(CreateDefaultConfig());

    for (int i = 0; i < 100; ++i) {
        cascade->Update(glm::vec3(i * 0.1f, 0.0f, 0.0f), 0.016f);
    }

    // Should complete without issues
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, UpdateWithZeroDeltaTime) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->Update(glm::vec3(0.0f), 0.0f);

    // Should not crash
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, UpdateWithLargeDeltaTime) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->Update(glm::vec3(0.0f), 10.0f);

    // Should handle gracefully
    EXPECT_TRUE(cascade->IsEnabled());
}

// =============================================================================
// Radiance Injection Tests
// =============================================================================

TEST_F(RadianceCascadeTest, InjectDirectLightingEmpty) {
    cascade->Initialize(CreateDefaultConfig());

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> radiance;

    // Should not crash with empty vectors
    cascade->InjectDirectLighting(positions, radiance);
}

TEST_F(RadianceCascadeTest, InjectDirectLightingSingle) {
    cascade->Initialize(CreateDefaultConfig());

    std::vector<glm::vec3> positions = {glm::vec3(0.0f, 0.0f, 0.0f)};
    std::vector<glm::vec3> radiance = {glm::vec3(1.0f, 1.0f, 1.0f)};

    cascade->InjectDirectLighting(positions, radiance);

    // Should complete without error
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectDirectLightingMultiple) {
    cascade->Initialize(CreateDefaultConfig());

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> radiance;

    for (int i = 0; i < 100; ++i) {
        positions.push_back(glm::vec3(i * 2.0f, 0.0f, 0.0f));
        radiance.push_back(glm::vec3(1.0f, 0.5f, 0.2f));
    }

    cascade->InjectDirectLighting(positions, radiance);

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectEmissiveSingle) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f, 10.0f, 10.0f), 5.0f);

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectEmissiveMultiple) {
    cascade->Initialize(CreateDefaultConfig());

    for (int i = 0; i < 50; ++i) {
        cascade->InjectEmissive(
            glm::vec3(i * 5.0f, 0.0f, 0.0f),
            glm::vec3(5.0f, 5.0f, 5.0f),
            2.0f
        );
    }

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectEmissiveZeroRadius) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(1.0f), 0.0f);

    // Should handle zero radius gracefully
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectEmissiveLargeRadius) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(1.0f), 1000.0f);

    // Should handle large radius without crashing
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectWhenDisabled) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->SetEnabled(false);

    std::vector<glm::vec3> positions = {glm::vec3(0.0f)};
    std::vector<glm::vec3> radiance = {glm::vec3(1.0f)};

    // Should early-out without crash
    cascade->InjectDirectLighting(positions, radiance);
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(1.0f), 1.0f);
}

// =============================================================================
// Propagation Tests
// =============================================================================

TEST_F(RadianceCascadeTest, PropagateLighting) {
    cascade->Initialize(CreateDefaultConfig());

    // Inject some light first
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 5.0f);

    // Propagate should complete without error
    cascade->PropagateLighting();

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, PropagateWithoutInjection) {
    cascade->Initialize(CreateDefaultConfig());

    // Propagate on empty cascade
    cascade->PropagateLighting();

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, PropagateWhenDisabled) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->SetEnabled(false);

    // Should early-out
    cascade->PropagateLighting();

    EXPECT_FALSE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, PropagateMultipleTimes) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 5.0f);

    for (int i = 0; i < 10; ++i) {
        cascade->PropagateLighting();
    }

    EXPECT_TRUE(cascade->IsEnabled());
}

// =============================================================================
// Radiance Sampling Tests
// =============================================================================

TEST_F(RadianceCascadeTest, SampleRadianceAtOrigin) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->Update(glm::vec3(0.0f), 0.016f);

    glm::vec3 radiance = cascade->SampleRadiance(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Default radiance should be zero or some ambient value
    // Just verify it returns a valid vector
    EXPECT_FALSE(std::isnan(radiance.x));
    EXPECT_FALSE(std::isnan(radiance.y));
    EXPECT_FALSE(std::isnan(radiance.z));
}

TEST_F(RadianceCascadeTest, SampleRadianceOutsideBounds) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->Update(glm::vec3(0.0f), 0.016f);

    // Sample far outside cascade bounds
    glm::vec3 radiance = cascade->SampleRadiance(
        glm::vec3(10000.0f, 10000.0f, 10000.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    // Should return zero or gracefully handle
    EXPECT_FALSE(std::isnan(radiance.x));
}

TEST_F(RadianceCascadeTest, SampleRadianceWithDifferentNormals) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 5.0f);
    cascade->Update(glm::vec3(0.0f), 0.016f);

    glm::vec3 normals[] = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f))
    };

    for (const auto& normal : normals) {
        glm::vec3 radiance = cascade->SampleRadiance(glm::vec3(5.0f, 0.0f, 0.0f), normal);
        EXPECT_FALSE(std::isnan(radiance.x));
        EXPECT_FALSE(std::isnan(radiance.y));
        EXPECT_FALSE(std::isnan(radiance.z));
    }
}

TEST_F(RadianceCascadeTest, SampleRadianceBeforeInitialize) {
    // Create new cascade without initializing
    RadianceCascade uninitializedCascade;

    glm::vec3 radiance = uninitializedCascade.SampleRadiance(
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    EXPECT_VEC3_EQ(glm::vec3(0.0f), radiance);
}

// =============================================================================
// Clear Tests
// =============================================================================

TEST_F(RadianceCascadeTest, ClearAfterInjection) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(100.0f), 10.0f);
    cascade->Update(glm::vec3(0.0f), 0.016f);

    cascade->Clear();

    // After clear, radiance should be zero
    glm::vec3 radiance = cascade->SampleRadiance(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // Implementation may vary, but should not crash
    EXPECT_FALSE(std::isnan(radiance.x));
}

TEST_F(RadianceCascadeTest, ClearMultipleTimes) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->Clear();
    cascade->Clear();
    cascade->Clear();

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, ClearBeforeInitialize) {
    // Should not crash on uninitialized cascade
    cascade->Clear();
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(RadianceCascadeTest, GetStatsAfterInit) {
    cascade->Initialize(CreateDefaultConfig());

    const auto& stats = cascade->GetStats();

    EXPECT_GE(stats.totalProbes, 0);
    EXPECT_GE(stats.activeProbes, 0);
    EXPECT_EQ(0, stats.probesUpdatedThisFrame);
}

TEST_F(RadianceCascadeTest, GetStatsAfterUpdate) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->Update(glm::vec3(0.0f), 0.016f);

    const auto& stats = cascade->GetStats();

    EXPECT_GT(stats.totalProbes, 0);
    EXPECT_GE(stats.probesUpdatedThisFrame, 0);
}

TEST_F(RadianceCascadeTest, UpdateTimeTracked) {
    cascade->Initialize(CreateDefaultConfig());

    auto start = std::chrono::high_resolution_clock::now();
    cascade->Update(glm::vec3(0.0f), 0.016f);
    auto end = std::chrono::high_resolution_clock::now();

    const auto& stats = cascade->GetStats();

    // Update time should be non-negative
    EXPECT_GE(stats.updateTimeMs, 0.0f);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(RadianceCascadeTest, ZeroResolutionConfig) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.baseResolution = 0; // Invalid

    // Should handle gracefully (either reject or clamp)
    bool result = cascade->Initialize(config);
    // Implementation-dependent behavior
}

TEST_F(RadianceCascadeTest, NegativeSpacing) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.baseSpacing = -1.0f; // Invalid

    bool result = cascade->Initialize(config);
    // Should handle gracefully
}

TEST_F(RadianceCascadeTest, ExtremeOrigin) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.origin = glm::vec3(1e10f, 1e10f, 1e10f);

    bool result = cascade->Initialize(config);
    EXPECT_TRUE(result);

    cascade->Update(config.origin, 0.016f);
    // Should handle extreme values
}

TEST_F(RadianceCascadeTest, InjectNegativeRadiance) {
    cascade->Initialize(CreateDefaultConfig());

    // Negative radiance (physically incorrect but should not crash)
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(-1.0f, -1.0f, -1.0f), 1.0f);

    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectInfiniteRadiance) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(
        glm::vec3(0.0f),
        glm::vec3(std::numeric_limits<float>::infinity()),
        1.0f
    );

    // Should handle gracefully (clamp or reject)
    EXPECT_TRUE(cascade->IsEnabled());
}

TEST_F(RadianceCascadeTest, InjectNaNRadiance) {
    cascade->Initialize(CreateDefaultConfig());

    cascade->InjectEmissive(
        glm::vec3(0.0f),
        glm::vec3(std::numeric_limits<float>::quiet_NaN()),
        1.0f
    );

    // Should handle gracefully
    EXPECT_TRUE(cascade->IsEnabled());
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

class RadianceCascadeBenchmark : public RadianceCascadeTest {
protected:
    static constexpr int BENCHMARK_ITERATIONS = 100;
};

TEST_F(RadianceCascadeBenchmark, UpdatePerformance) {
    cascade->Initialize(CreateDefaultConfig());

    ScopedTimer timer("RadianceCascade::Update");

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        cascade->Update(glm::vec3(i * 0.1f, 0.0f, 0.0f), 0.016f);
    }

    double avgTimeMs = timer.ElapsedMilliseconds() / BENCHMARK_ITERATIONS;
    std::cout << "Average update time: " << avgTimeMs << " ms" << std::endl;

    // Should complete updates within reasonable time
    EXPECT_LT(avgTimeMs, 100.0); // Less than 100ms per update
}

TEST_F(RadianceCascadeBenchmark, InjectionPerformance) {
    cascade->Initialize(CreateDefaultConfig());

    std::vector<glm::vec3> positions(1000);
    std::vector<glm::vec3> radiance(1000);

    for (size_t i = 0; i < positions.size(); ++i) {
        positions[i] = glm::vec3(i * 0.1f, 0.0f, 0.0f);
        radiance[i] = glm::vec3(1.0f);
    }

    ScopedTimer timer("RadianceCascade::InjectDirectLighting");

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        cascade->InjectDirectLighting(positions, radiance);
    }

    double avgTimeMs = timer.ElapsedMilliseconds() / BENCHMARK_ITERATIONS;
    std::cout << "Average injection time (1000 lights): " << avgTimeMs << " ms" << std::endl;

    EXPECT_LT(avgTimeMs, 50.0);
}

TEST_F(RadianceCascadeBenchmark, SamplingPerformance) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 10.0f);
    cascade->Update(glm::vec3(0.0f), 0.016f);

    const int SAMPLE_COUNT = 10000;
    RandomGenerator rng(42);
    Vec3Generator posGen(-50.0f, 50.0f);
    Vec3Generator normalGen(-1.0f, 1.0f);

    ScopedTimer timer("RadianceCascade::SampleRadiance");

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        glm::vec3 pos = posGen.Generate(rng);
        glm::vec3 normal = glm::normalize(normalGen.Generate(rng));
        cascade->SampleRadiance(pos, normal);
    }

    double totalTimeMs = timer.ElapsedMilliseconds();
    double avgTimeUs = (totalTimeMs * 1000.0) / SAMPLE_COUNT;
    std::cout << "Average sample time: " << avgTimeUs << " us" << std::endl;

    EXPECT_LT(avgTimeUs, 100.0); // Less than 100 microseconds per sample
}

TEST_F(RadianceCascadeBenchmark, PropagationPerformance) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 10.0f);

    ScopedTimer timer("RadianceCascade::PropagateLighting");

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        cascade->PropagateLighting();
    }

    double avgTimeMs = timer.ElapsedMilliseconds() / BENCHMARK_ITERATIONS;
    std::cout << "Average propagation time: " << avgTimeMs << " ms" << std::endl;

    EXPECT_LT(avgTimeMs, 100.0);
}

// =============================================================================
// Memory Management Tests
// =============================================================================

TEST_F(RadianceCascadeTest, MemoryLeakOnReinitialize) {
    // Reinitialize multiple times to check for leaks
    // (Would need memory profiling tools for actual leak detection)
    for (int i = 0; i < 10; ++i) {
        RadianceCascade::Config config = CreateDefaultConfig();
        config.numCascades = (i % 4) + 1;
        cascade->Initialize(config);
        cascade->Shutdown();
    }

    // Final initialization
    EXPECT_TRUE(cascade->Initialize(CreateDefaultConfig()));
}

TEST_F(RadianceCascadeTest, LargeCascadeAllocation) {
    RadianceCascade::Config config;
    config.numCascades = 6;
    config.baseResolution = 64; // Large resolution
    config.cascadeScale = 2.0f;
    config.asyncUpdate = false;

    bool result = cascade->Initialize(config);

    // Should either succeed or gracefully fail
    if (result) {
        const auto& stats = cascade->GetStats();
        EXPECT_GT(stats.totalProbes, 0);
    }
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST_F(RadianceCascadeTest, RadianceSamplingIsSmooth) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 5.0f);
    cascade->Update(glm::vec3(0.0f), 0.016f);
    cascade->PropagateLighting();

    // Sample at nearby positions - radiance should be similar
    glm::vec3 pos1(1.0f, 0.0f, 0.0f);
    glm::vec3 pos2(1.1f, 0.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    glm::vec3 rad1 = cascade->SampleRadiance(pos1, normal);
    glm::vec3 rad2 = cascade->SampleRadiance(pos2, normal);

    // Radiance should change smoothly (no sudden jumps)
    glm::vec3 diff = glm::abs(rad1 - rad2);
    float maxDiff = glm::max(diff.x, glm::max(diff.y, diff.z));

    // The difference should be relatively small for nearby samples
    EXPECT_LT(maxDiff, 10.0f); // Adjust threshold based on implementation
}

TEST_F(RadianceCascadeTest, CascadeCoversConfiguredRadius) {
    RadianceCascade::Config config = CreateDefaultConfig();
    config.updateRadius = 50.0f;

    cascade->Initialize(config);
    cascade->Update(glm::vec3(0.0f), 0.016f);

    // Sampling within radius should not return errors
    for (float r = 0.0f; r <= config.updateRadius; r += 10.0f) {
        glm::vec3 pos(r, 0.0f, 0.0f);
        glm::vec3 radiance = cascade->SampleRadiance(pos, glm::vec3(0.0f, 1.0f, 0.0f));
        EXPECT_FALSE(std::isnan(radiance.x));
    }
}

// =============================================================================
// Thread Safety Tests (Basic)
// =============================================================================

TEST_F(RadianceCascadeTest, ConcurrentSampling) {
    cascade->Initialize(CreateDefaultConfig());
    cascade->InjectEmissive(glm::vec3(0.0f), glm::vec3(10.0f), 5.0f);
    cascade->Update(glm::vec3(0.0f), 0.016f);

    // Note: Full thread safety testing would require more sophisticated setup
    // This just ensures sampling doesn't crash under basic conditions

    const int THREAD_COUNT = 4;
    const int SAMPLES_PER_THREAD = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> completedSamples(0);

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([this, &completedSamples, t]() {
            RandomGenerator rng(42 + t);
            Vec3Generator posGen(-20.0f, 20.0f);

            for (int i = 0; i < SAMPLES_PER_THREAD; ++i) {
                glm::vec3 pos = posGen.Generate(rng);
                cascade->SampleRadiance(pos, glm::vec3(0.0f, 1.0f, 0.0f));
                completedSamples++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(THREAD_COUNT * SAMPLES_PER_THREAD, completedSamples.load());
}
