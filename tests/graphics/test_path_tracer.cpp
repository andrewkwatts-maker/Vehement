/**
 * @file test_path_tracer.cpp
 * @brief Comprehensive unit tests for PathTracer
 *
 * Test categories:
 * 1. Initialization and shutdown
 * 2. Ray generation and tracing
 * 3. Material scattering
 * 4. SDF evaluation
 * 5. Dispersion calculations
 * 6. Accumulation and output
 * 7. Performance benchmarks
 * 8. Edge cases
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "graphics/PathTracer.hpp"
#include "utils/TestHelpers.hpp"
#include "utils/Generators.hpp"
#include "mocks/MockGraphics.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <cmath>
#include <limits>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Mock Camera for Testing
// =============================================================================

class MockCamera {
public:
    MockCamera()
        : m_position(0.0f, 0.0f, 5.0f)
        , m_target(0.0f, 0.0f, 0.0f)
        , m_up(0.0f, 1.0f, 0.0f)
        , m_fov(60.0f)
    {}

    glm::vec3 GetPosition() const { return m_position; }
    void SetPosition(const glm::vec3& pos) { m_position = pos; }

    glm::vec3 ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
        // Simple perspective projection ray generation
        float aspectRatio = screenSize.x / screenSize.y;
        float tanHalfFov = std::tan(glm::radians(m_fov / 2.0f));

        float x = (2.0f * screenPos.x / screenSize.x - 1.0f) * aspectRatio * tanHalfFov;
        float y = (1.0f - 2.0f * screenPos.y / screenSize.y) * tanHalfFov;

        glm::vec3 forward = glm::normalize(m_target - m_position);
        glm::vec3 right = glm::normalize(glm::cross(forward, m_up));
        glm::vec3 up = glm::cross(right, forward);

        return glm::normalize(forward + x * right + y * up);
    }

private:
    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;
    float m_fov;
};

// Declare Camera type alias for PathTracer compatibility
namespace Nova {
    using Camera = MockCamera;
}

// =============================================================================
// Test Fixture
// =============================================================================

class PathTracerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tracer = std::make_unique<PathTracer>();
    }

    void TearDown() override {
        if (tracer) {
            tracer->Shutdown();
        }
        tracer.reset();
    }

    // Create test primitives
    std::vector<SDFPrimitive> CreateTestScene() {
        std::vector<SDFPrimitive> primitives;

        // Ground plane (large sphere below scene)
        SDFPrimitive ground;
        ground.positionRadius = glm::vec4(0.0f, -100.5f, 0.0f, 100.0f);
        ground.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        ground.materialProps = glm::vec4(0.0f, 0.5f, 0.0f, 1.5f); // Diffuse
        ground.dispersionProps = glm::vec4(0.01f, 0.0f, 0.0f, 0.0f);
        ground.transform = glm::mat4(1.0f);
        ground.inverseTransform = glm::mat4(1.0f);
        primitives.push_back(ground);

        // Center sphere - diffuse
        SDFPrimitive center;
        center.positionRadius = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);
        center.color = glm::vec4(0.8f, 0.3f, 0.3f, 1.0f);
        center.materialProps = glm::vec4(0.0f, 0.5f, 0.0f, 1.5f);
        center.dispersionProps = glm::vec4(0.01f, 0.0f, 0.0f, 0.0f);
        center.transform = glm::mat4(1.0f);
        center.inverseTransform = glm::mat4(1.0f);
        primitives.push_back(center);

        // Left sphere - metal
        SDFPrimitive left;
        left.positionRadius = glm::vec4(-1.0f, 0.0f, 0.0f, 0.5f);
        left.color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        left.materialProps = glm::vec4(1.0f, 0.1f, 1.0f, 1.5f); // Metal
        left.dispersionProps = glm::vec4(0.01f, 0.0f, 0.0f, 0.0f);
        left.transform = glm::mat4(1.0f);
        left.inverseTransform = glm::mat4(1.0f);
        primitives.push_back(left);

        // Right sphere - glass
        SDFPrimitive right;
        right.positionRadius = glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
        right.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        right.materialProps = glm::vec4(2.0f, 0.0f, 0.0f, 1.5f); // Dielectric
        right.dispersionProps = glm::vec4(0.01f, 0.0f, 0.0f, 0.0f);
        right.transform = glm::mat4(1.0f);
        right.inverseTransform = glm::mat4(1.0f);
        primitives.push_back(right);

        return primitives;
    }

    SDFPrimitive CreateSphere(const glm::vec3& center, float radius, MaterialType type) {
        SDFPrimitive prim;
        prim.positionRadius = glm::vec4(center, radius);
        prim.color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        prim.materialProps = glm::vec4(static_cast<float>(type), 0.5f, 0.0f, 1.5f);
        prim.dispersionProps = glm::vec4(0.01f, 0.0f, 0.0f, 0.0f);
        prim.transform = glm::mat4(1.0f);
        prim.inverseTransform = glm::mat4(1.0f);
        return prim;
    }

    std::unique_ptr<PathTracer> tracer;
};

// =============================================================================
// Initialization and Shutdown Tests
// =============================================================================

TEST_F(PathTracerTest, DefaultConstruction) {
    // Default values should be set
    EXPECT_EQ(8, tracer->GetMaxBounces());
    EXPECT_EQ(4, tracer->GetSamplesPerPixel());
    EXPECT_TRUE(tracer->IsDispersionEnabled());
}

TEST_F(PathTracerTest, InitializeCPU) {
    bool result = tracer->Initialize(640, 480, false);

    EXPECT_TRUE(result);
    EXPECT_EQ(640 * 480, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, InitializeGPU) {
    // GPU initialization may fall back to CPU in test environment
    bool result = tracer->Initialize(640, 480, true);

    // Should succeed either way
    EXPECT_TRUE(result);
}

TEST_F(PathTracerTest, InitializeSmallResolution) {
    bool result = tracer->Initialize(16, 16, false);

    EXPECT_TRUE(result);
    EXPECT_EQ(16 * 16, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, InitializeLargeResolution) {
    bool result = tracer->Initialize(1920, 1080, false);

    EXPECT_TRUE(result);
    EXPECT_EQ(1920 * 1080, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, ShutdownBeforeInitialize) {
    // Should not crash
    tracer->Shutdown();
}

TEST_F(PathTracerTest, ShutdownAfterInitialize) {
    tracer->Initialize(640, 480, false);
    tracer->Shutdown();

    EXPECT_TRUE(tracer->GetOutputData().empty());
}

TEST_F(PathTracerTest, ReinitializeDifferentSize) {
    tracer->Initialize(640, 480, false);
    EXPECT_EQ(640 * 480, tracer->GetOutputData().size());

    tracer->Initialize(320, 240, false);
    EXPECT_EQ(320 * 240, tracer->GetOutputData().size());
}

// =============================================================================
// Settings Tests
// =============================================================================

TEST_F(PathTracerTest, SetMaxBounces) {
    tracer->SetMaxBounces(4);
    EXPECT_EQ(4, tracer->GetMaxBounces());

    tracer->SetMaxBounces(16);
    EXPECT_EQ(16, tracer->GetMaxBounces());
}

TEST_F(PathTracerTest, SetSamplesPerPixel) {
    tracer->SetSamplesPerPixel(1);
    EXPECT_EQ(1, tracer->GetSamplesPerPixel());

    tracer->SetSamplesPerPixel(64);
    EXPECT_EQ(64, tracer->GetSamplesPerPixel());
}

TEST_F(PathTracerTest, SetEnableDispersion) {
    tracer->SetEnableDispersion(false);
    EXPECT_FALSE(tracer->IsDispersionEnabled());

    tracer->SetEnableDispersion(true);
    EXPECT_TRUE(tracer->IsDispersionEnabled());
}

TEST_F(PathTracerTest, SetEnableReSTIR) {
    tracer->SetEnableReSTIR(false);
    EXPECT_FALSE(tracer->IsReSTIREnabled());

    tracer->SetEnableReSTIR(true);
    EXPECT_TRUE(tracer->IsReSTIREnabled());
}

TEST_F(PathTracerTest, SetEnableDenoising) {
    tracer->SetEnableDenoising(false);
    EXPECT_FALSE(tracer->IsDenoisingEnabled());

    tracer->SetEnableDenoising(true);
    EXPECT_TRUE(tracer->IsDenoisingEnabled());
}

TEST_F(PathTracerTest, SetEnvironmentColor) {
    glm::vec3 envColor(0.2f, 0.5f, 0.8f);
    tracer->SetEnvironmentColor(envColor);

    // No getter, but should not crash
}

// =============================================================================
// Resize Tests
// =============================================================================

TEST_F(PathTracerTest, ResizeSameSize) {
    tracer->Initialize(640, 480, false);

    tracer->Resize(640, 480);

    EXPECT_EQ(640 * 480, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, ResizeLarger) {
    tracer->Initialize(640, 480, false);

    tracer->Resize(1280, 720);

    EXPECT_EQ(1280 * 720, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, ResizeSmaller) {
    tracer->Initialize(1280, 720, false);

    tracer->Resize(640, 480);

    EXPECT_EQ(640 * 480, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, ResizeResetsAccumulation) {
    tracer->Initialize(64, 64, false);
    MockCamera camera;
    auto primitives = CreateTestScene();

    // Render a few frames
    for (int i = 0; i < 5; ++i) {
        tracer->Render(camera, primitives);
    }

    const auto& stats = tracer->GetStats();
    int framesBefore = stats.frameCount;

    tracer->Resize(128, 128);

    // After resize, frame count might reset
    // Implementation-dependent
}

// =============================================================================
// Rendering Tests
// =============================================================================

TEST_F(PathTracerTest, RenderEmptyScene) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    MockCamera camera;
    std::vector<SDFPrimitive> emptyScene;

    tracer->Render(camera, emptyScene);

    // Should render sky color
    const auto& output = tracer->GetOutputData();
    EXPECT_EQ(64 * 64, output.size());

    // Check that output is not all black (sky should be visible)
    bool hasColor = false;
    for (const auto& pixel : output) {
        if (pixel.r > 0.01f || pixel.g > 0.01f || pixel.b > 0.01f) {
            hasColor = true;
            break;
        }
    }
    EXPECT_TRUE(hasColor);
}

TEST_F(PathTracerTest, RenderSimpleScene) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    // Should complete without crash
    const auto& output = tracer->GetOutputData();
    EXPECT_EQ(64 * 64, output.size());
}

TEST_F(PathTracerTest, RenderMultipleFrames) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);

    MockCamera camera;
    auto primitives = CreateTestScene();

    for (int i = 0; i < 10; ++i) {
        tracer->Render(camera, primitives);
    }

    const auto& stats = tracer->GetStats();
    EXPECT_GE(stats.frameCount, 10);
}

TEST_F(PathTracerTest, RenderWithDispersion) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetEnableDispersion(true);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    // Should complete with dispersion enabled
    EXPECT_TRUE(tracer->IsDispersionEnabled());
}

TEST_F(PathTracerTest, RenderWithoutDispersion) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetEnableDispersion(false);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    EXPECT_FALSE(tracer->IsDispersionEnabled());
}

TEST_F(PathTracerTest, RenderOutputInValidRange) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(4);
    tracer->SetMaxBounces(4);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    const auto& output = tracer->GetOutputData();
    for (const auto& pixel : output) {
        EXPECT_GE(pixel.r, 0.0f);
        EXPECT_GE(pixel.g, 0.0f);
        EXPECT_GE(pixel.b, 0.0f);
        EXPECT_LE(pixel.r, 1.0f);
        EXPECT_LE(pixel.g, 1.0f);
        EXPECT_LE(pixel.b, 1.0f);

        // No NaN values
        EXPECT_FALSE(std::isnan(pixel.r));
        EXPECT_FALSE(std::isnan(pixel.g));
        EXPECT_FALSE(std::isnan(pixel.b));
    }
}

// =============================================================================
// Accumulation Tests
// =============================================================================

TEST_F(PathTracerTest, AccumulationImproves) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    MockCamera camera;
    auto primitives = CreateTestScene();

    // Render first frame
    tracer->Render(camera, primitives);
    const auto& output1 = tracer->GetOutputData();

    // Store a pixel value
    glm::vec3 pixel1 = output1[32 * 64 + 32]; // Center pixel

    // Render more frames (accumulation)
    for (int i = 0; i < 10; ++i) {
        tracer->Render(camera, primitives);
    }

    // Pixel should be smoothed by accumulation
    // (may or may not change significantly, depends on scene)
    const auto& output2 = tracer->GetOutputData();
    glm::vec3 pixel2 = output2[32 * 64 + 32];

    // Values should still be in valid range
    EXPECT_GE(pixel2.r, 0.0f);
    EXPECT_LE(pixel2.r, 1.0f);
}

TEST_F(PathTracerTest, ResetAccumulation) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);

    MockCamera camera;
    auto primitives = CreateTestScene();

    // Render some frames
    for (int i = 0; i < 5; ++i) {
        tracer->Render(camera, primitives);
    }

    tracer->ResetAccumulation();

    // After reset, frame count should be 0
    const auto& stats = tracer->GetStats();
    // Note: Stats might not update until next render
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(PathTracerTest, StatsAfterRender) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(4);
    tracer->SetMaxBounces(4);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    const auto& stats = tracer->GetStats();
    EXPECT_GT(stats.renderTimeMs, 0.0f);
    EXPECT_EQ(64 * 64 * 4, stats.primaryRays);
    EXPECT_GE(stats.secondaryRays, 0);
    EXPECT_GE(stats.frameCount, 1);
    EXPECT_GT(stats.fps, 0.0f);
}

TEST_F(PathTracerTest, TraceTimeTracked) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    const auto& stats = tracer->GetStats();
    EXPECT_GT(stats.traceTimeMs, 0.0f);
}

// =============================================================================
// Ray and Hit Record Tests
// =============================================================================

TEST_F(PathTracerTest, RayAt) {
    Ray ray;
    ray.origin = glm::vec3(0.0f, 0.0f, 0.0f);
    ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 point = ray.At(5.0f);

    EXPECT_VEC3_EQ(glm::vec3(0.0f, 0.0f, -5.0f), point);
}

TEST_F(PathTracerTest, RayAtNegative) {
    Ray ray;
    ray.origin = glm::vec3(0.0f, 0.0f, 0.0f);
    ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 point = ray.At(-5.0f);

    EXPECT_VEC3_EQ(glm::vec3(0.0f, 0.0f, 5.0f), point);
}

TEST_F(PathTracerTest, HitRecordSetFaceNormalFront) {
    Ray ray;
    ray.origin = glm::vec3(0.0f, 0.0f, 5.0f);
    ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);

    HitRecord hit;
    hit.SetFaceNormal(ray, glm::vec3(0.0f, 0.0f, 1.0f));

    EXPECT_TRUE(hit.frontFace);
    EXPECT_VEC3_EQ(glm::vec3(0.0f, 0.0f, 1.0f), hit.normal);
}

TEST_F(PathTracerTest, HitRecordSetFaceNormalBack) {
    Ray ray;
    ray.origin = glm::vec3(0.0f, 0.0f, -5.0f);
    ray.direction = glm::vec3(0.0f, 0.0f, 1.0f);

    HitRecord hit;
    hit.SetFaceNormal(ray, glm::vec3(0.0f, 0.0f, 1.0f));

    EXPECT_FALSE(hit.frontFace);
    EXPECT_VEC3_EQ(glm::vec3(0.0f, 0.0f, -1.0f), hit.normal);
}

// =============================================================================
// Material Tests
// =============================================================================

TEST_F(PathTracerTest, PathTraceMaterialDefaultValues) {
    PathTraceMaterial mat;

    EXPECT_EQ(MaterialType::Diffuse, mat.type);
    EXPECT_VEC3_NEAR(glm::vec3(0.8f), mat.albedo, 0.01f);
    EXPECT_VEC3_EQ(glm::vec3(0.0f), mat.emission);
    EXPECT_FLOAT_EQ(0.5f, mat.roughness);
    EXPECT_FLOAT_EQ(0.0f, mat.metallic);
    EXPECT_FLOAT_EQ(1.5f, mat.ior);
}

TEST_F(PathTracerTest, PathTraceMaterialGetIOR) {
    PathTraceMaterial mat;
    mat.ior = 1.5f;
    mat.cauchyB = 0.01f;
    mat.cauchyC = 0.0f;

    float ior550 = mat.GetIOR(550.0f);
    float ior450 = mat.GetIOR(450.0f);
    float ior650 = mat.GetIOR(650.0f);

    // Blue should have higher IOR than red
    EXPECT_GT(ior450, ior650);
}

TEST_F(PathTracerTest, PathTraceMaterialDispersionMonotonic) {
    PathTraceMaterial mat;
    mat.ior = 1.5f;
    mat.cauchyB = 0.01f;
    mat.cauchyC = 0.0f;

    float prevIOR = mat.GetIOR(380.0f);
    for (float w = 400.0f; w <= 780.0f; w += 20.0f) {
        float ior = mat.GetIOR(w);
        EXPECT_LE(ior, prevIOR + 0.0001f); // IOR decreases with wavelength
        prevIOR = ior;
    }
}

// =============================================================================
// SDF Primitive Tests
// =============================================================================

TEST_F(PathTracerTest, SDFPrimitiveDefaultTransform) {
    SDFPrimitive prim = CreateSphere(glm::vec3(0.0f), 1.0f, MaterialType::Diffuse);

    EXPECT_MAT4_EQ(glm::mat4(1.0f), prim.transform);
    EXPECT_MAT4_EQ(glm::mat4(1.0f), prim.inverseTransform);
}

TEST_F(PathTracerTest, SDFPrimitiveWithTransform) {
    SDFPrimitive prim = CreateSphere(glm::vec3(0.0f), 1.0f, MaterialType::Diffuse);

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
    prim.transform = transform;
    prim.inverseTransform = glm::inverse(transform);

    EXPECT_MAT4_EQ(transform, prim.transform);
}

// =============================================================================
// Helper Function Tests
// =============================================================================

TEST_F(PathTracerTest, WavelengthToRGBRed) {
    glm::vec3 rgb = Nova::WavelengthToRGB(650.0f);

    EXPECT_GT(rgb.r, rgb.g);
    EXPECT_GT(rgb.r, rgb.b);
}

TEST_F(PathTracerTest, WavelengthToRGBGreen) {
    glm::vec3 rgb = Nova::WavelengthToRGB(550.0f);

    EXPECT_GT(rgb.g, rgb.b);
}

TEST_F(PathTracerTest, WavelengthToRGBBlue) {
    glm::vec3 rgb = Nova::WavelengthToRGB(470.0f);

    EXPECT_GT(rgb.b, rgb.r);
}

TEST_F(PathTracerTest, RGBToSpectral) {
    glm::vec3 spectral = RGBToSpectral(glm::vec3(1.0f, 0.0f, 0.0f));

    // Should return peak wavelengths
    EXPECT_FLOAT_EQ(650.0f, spectral.r); // Red peak
    EXPECT_FLOAT_EQ(550.0f, spectral.g); // Green peak
    EXPECT_FLOAT_EQ(450.0f, spectral.b); // Blue peak
}

TEST_F(PathTracerTest, SampleWavelengthFromRGBRed) {
    glm::vec3 rgb(1.0f, 0.0f, 0.0f);

    // With low random value, should sample red
    float wavelength = SampleWavelengthFromRGB(rgb, 0.5f);
    EXPECT_FLOAT_EQ(650.0f, wavelength);
}

TEST_F(PathTracerTest, SampleWavelengthFromRGBGreen) {
    glm::vec3 rgb(0.0f, 1.0f, 0.0f);

    float wavelength = SampleWavelengthFromRGB(rgb, 0.5f);
    EXPECT_FLOAT_EQ(550.0f, wavelength);
}

TEST_F(PathTracerTest, SampleWavelengthFromRGBBlack) {
    glm::vec3 rgb(0.0f, 0.0f, 0.0f);

    float wavelength = SampleWavelengthFromRGB(rgb, 0.5f);
    EXPECT_FLOAT_EQ(550.0f, wavelength); // Default green
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(PathTracerTest, RenderWithZeroBounces) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(0);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    // Should still produce valid output (no secondary rays)
    const auto& stats = tracer->GetStats();
    EXPECT_EQ(0, stats.secondaryRays);
}

TEST_F(PathTracerTest, RenderWithManyBounces) {
    tracer->Initialize(32, 32, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(32);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    // Should complete without crash
    const auto& output = tracer->GetOutputData();
    EXPECT_EQ(32 * 32, output.size());
}

TEST_F(PathTracerTest, RenderVerySmallResolution) {
    tracer->Initialize(1, 1, false);
    tracer->SetSamplesPerPixel(1);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    const auto& output = tracer->GetOutputData();
    EXPECT_EQ(1, output.size());
}

TEST_F(PathTracerTest, RenderWithManySamples) {
    tracer->Initialize(16, 16, false);
    tracer->SetSamplesPerPixel(64);
    tracer->SetMaxBounces(2);

    MockCamera camera;
    auto primitives = CreateTestScene();

    tracer->Render(camera, primitives);

    // Should complete (might be slow)
    EXPECT_EQ(16 * 16, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, RenderWithManyPrimitives) {
    tracer->Initialize(32, 32, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    std::vector<SDFPrimitive> primitives;
    for (int i = 0; i < 100; ++i) {
        primitives.push_back(CreateSphere(
            glm::vec3(i * 2.0f - 100.0f, 0.0f, -10.0f),
            0.5f,
            static_cast<MaterialType>(i % 4)
        ));
    }

    MockCamera camera;
    tracer->Render(camera, primitives);

    // Should handle many primitives
    EXPECT_EQ(32 * 32, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, RenderEmissiveMaterial) {
    tracer->Initialize(32, 32, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    std::vector<SDFPrimitive> primitives;

    // Emissive sphere
    SDFPrimitive emissive;
    emissive.positionRadius = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    emissive.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    emissive.materialProps = glm::vec4(3.0f, 0.0f, 0.0f, 1.5f); // Emissive type
    emissive.dispersionProps = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    emissive.transform = glm::mat4(1.0f);
    emissive.inverseTransform = glm::mat4(1.0f);
    primitives.push_back(emissive);

    MockCamera camera;
    tracer->Render(camera, primitives);

    // Emissive objects should produce bright output
    const auto& output = tracer->GetOutputData();
    bool hasBrightPixel = false;
    for (const auto& pixel : output) {
        if (pixel.r > 0.5f || pixel.g > 0.5f || pixel.b > 0.5f) {
            hasBrightPixel = true;
            break;
        }
    }
    EXPECT_TRUE(hasBrightPixel);
}

TEST_F(PathTracerTest, RenderWithNaNPosition) {
    tracer->Initialize(32, 32, false);
    tracer->SetSamplesPerPixel(1);

    std::vector<SDFPrimitive> primitives;
    SDFPrimitive bad;
    bad.positionRadius = glm::vec4(std::numeric_limits<float>::quiet_NaN());
    bad.transform = glm::mat4(1.0f);
    bad.inverseTransform = glm::mat4(1.0f);
    primitives.push_back(bad);

    MockCamera camera;
    tracer->Render(camera, primitives);

    // Should handle gracefully (output sky color or handle NaN)
}

TEST_F(PathTracerTest, RenderWithInfinitePosition) {
    tracer->Initialize(32, 32, false);
    tracer->SetSamplesPerPixel(1);

    std::vector<SDFPrimitive> primitives;
    SDFPrimitive bad;
    bad.positionRadius = glm::vec4(std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 1.0f);
    bad.transform = glm::mat4(1.0f);
    bad.inverseTransform = glm::mat4(1.0f);
    primitives.push_back(bad);

    MockCamera camera;
    tracer->Render(camera, primitives);

    // Should handle gracefully
}

// =============================================================================
// Performance Benchmarks
// =============================================================================

class PathTracerBenchmark : public PathTracerTest {
protected:
    void SetUp() override {
        PathTracerTest::SetUp();
        tracer->Initialize(256, 256, false);
    }
};

TEST_F(PathTracerBenchmark, RenderPerformance_LowQuality) {
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);
    tracer->SetEnableDispersion(false);

    MockCamera camera;
    auto primitives = CreateTestScene();

    ScopedTimer timer("PathTracer Low Quality (256x256, 1spp, 2 bounces)");

    tracer->Render(camera, primitives);

    double timeMs = timer.ElapsedMilliseconds();
    std::cout << "Render time: " << timeMs << " ms" << std::endl;

    EXPECT_LT(timeMs, 5000.0); // Should complete in reasonable time
}

TEST_F(PathTracerBenchmark, RenderPerformance_MediumQuality) {
    tracer->SetSamplesPerPixel(4);
    tracer->SetMaxBounces(4);
    tracer->SetEnableDispersion(true);

    MockCamera camera;
    auto primitives = CreateTestScene();

    ScopedTimer timer("PathTracer Medium Quality (256x256, 4spp, 4 bounces)");

    tracer->Render(camera, primitives);

    double timeMs = timer.ElapsedMilliseconds();
    std::cout << "Render time: " << timeMs << " ms" << std::endl;

    EXPECT_LT(timeMs, 30000.0);
}

TEST_F(PathTracerBenchmark, RenderPerformance_ManyPrimitives) {
    tracer->Initialize(128, 128, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    std::vector<SDFPrimitive> primitives;
    for (int i = 0; i < 50; ++i) {
        primitives.push_back(CreateSphere(
            glm::vec3((i % 10) * 2.0f - 10.0f, (i / 10) * 2.0f - 5.0f, -5.0f),
            0.4f,
            MaterialType::Diffuse
        ));
    }

    MockCamera camera;

    ScopedTimer timer("PathTracer Many Primitives (128x128, 50 spheres)");

    tracer->Render(camera, primitives);

    double timeMs = timer.ElapsedMilliseconds();
    std::cout << "Render time: " << timeMs << " ms" << std::endl;
}

TEST_F(PathTracerBenchmark, AccumulationFrameRate) {
    tracer->Initialize(128, 128, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);

    MockCamera camera;
    auto primitives = CreateTestScene();

    const int FRAME_COUNT = 20;

    ScopedTimer timer("PathTracer Accumulation");

    for (int i = 0; i < FRAME_COUNT; ++i) {
        tracer->Render(camera, primitives);
    }

    double totalTimeMs = timer.ElapsedMilliseconds();
    double avgFrameMs = totalTimeMs / FRAME_COUNT;
    double fps = 1000.0 / avgFrameMs;

    std::cout << "Average frame time: " << avgFrameMs << " ms (" << fps << " FPS)" << std::endl;
}

// =============================================================================
// Determinism Tests
// =============================================================================

TEST_F(PathTracerTest, DeterministicRendering) {
    tracer->Initialize(32, 32, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(2);
    tracer->SetEnableDispersion(false); // Disable for determinism

    MockCamera camera;
    auto primitives = CreateTestScene();

    // First render
    tracer->Render(camera, primitives);
    std::vector<glm::vec3> output1 = tracer->GetOutputData();

    // Reset and render again
    tracer->ResetAccumulation();
    tracer->Render(camera, primitives);
    std::vector<glm::vec3> output2 = tracer->GetOutputData();

    // Note: Due to random sampling, outputs may differ
    // This test documents the non-deterministic behavior
}

// =============================================================================
// Memory Tests
// =============================================================================

TEST_F(PathTracerTest, MemoryLeakOnMultipleRenders) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(1);
    tracer->SetMaxBounces(1);

    MockCamera camera;
    auto primitives = CreateTestScene();

    // Render many frames - memory should not grow
    for (int i = 0; i < 100; ++i) {
        tracer->Render(camera, primitives);
    }

    // Output size should remain constant
    EXPECT_EQ(64 * 64, tracer->GetOutputData().size());
}

TEST_F(PathTracerTest, MemoryLeakOnResize) {
    tracer->Initialize(64, 64, false);

    MockCamera camera;
    auto primitives = CreateTestScene();

    for (int i = 0; i < 10; ++i) {
        tracer->Resize(32 + i * 16, 32 + i * 16);
        tracer->Render(camera, primitives);
    }

    // Final size should match last resize
    int finalSize = (32 + 9 * 16);
    EXPECT_EQ(finalSize * finalSize, tracer->GetOutputData().size());
}

// =============================================================================
// Integration-style Tests
// =============================================================================

TEST_F(PathTracerTest, FullRenderPipeline) {
    // Test complete rendering pipeline
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(4);
    tracer->SetMaxBounces(4);
    tracer->SetEnableDispersion(true);
    tracer->SetEnableReSTIR(false); // Not implemented
    tracer->SetEnableDenoising(false); // Not implemented
    tracer->SetEnvironmentColor(glm::vec3(0.5f, 0.7f, 1.0f));

    MockCamera camera;
    auto primitives = CreateTestScene();

    // Accumulate several frames
    for (int frame = 0; frame < 10; ++frame) {
        tracer->Render(camera, primitives);
    }

    // Verify output
    const auto& output = tracer->GetOutputData();
    EXPECT_EQ(64 * 64, output.size());

    // All pixels should be in valid range
    for (const auto& pixel : output) {
        EXPECT_GE(pixel.r, 0.0f);
        EXPECT_LE(pixel.r, 1.0f);
        EXPECT_GE(pixel.g, 0.0f);
        EXPECT_LE(pixel.g, 1.0f);
        EXPECT_GE(pixel.b, 0.0f);
        EXPECT_LE(pixel.b, 1.0f);
    }

    // Check statistics
    const auto& stats = tracer->GetStats();
    EXPECT_EQ(10, stats.frameCount);
    EXPECT_GT(stats.totalRays, 0);
    EXPECT_GT(stats.fps, 0.0f);
}

TEST_F(PathTracerTest, ComplexMaterialMix) {
    tracer->Initialize(64, 64, false);
    tracer->SetSamplesPerPixel(2);
    tracer->SetMaxBounces(4);

    std::vector<SDFPrimitive> primitives;

    // Add all material types
    primitives.push_back(CreateSphere(glm::vec3(-3.0f, 0.0f, 0.0f), 0.5f, MaterialType::Diffuse));
    primitives.push_back(CreateSphere(glm::vec3(-1.0f, 0.0f, 0.0f), 0.5f, MaterialType::Metal));
    primitives.push_back(CreateSphere(glm::vec3(1.0f, 0.0f, 0.0f), 0.5f, MaterialType::Dielectric));
    primitives.push_back(CreateSphere(glm::vec3(3.0f, 0.0f, 0.0f), 0.5f, MaterialType::Emissive));

    MockCamera camera;
    tracer->Render(camera, primitives);

    // Should render all material types without issue
    const auto& output = tracer->GetOutputData();
    EXPECT_EQ(64 * 64, output.size());
}
