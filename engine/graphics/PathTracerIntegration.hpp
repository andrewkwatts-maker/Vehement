#pragma once

#include "PathTracer.hpp"
#include "../scene/Scene.hpp"
#include "../scene/Camera.hpp"
#include <memory>
#include <vector>

namespace Nova {

/**
 * @brief High-level integration of path tracer with engine
 *
 * Provides easy-to-use interface for path traced rendering in the Nova engine.
 * Handles scene conversion, camera setup, and performance optimization.
 */
class PathTracerIntegration {
public:
    PathTracerIntegration();
    ~PathTracerIntegration();

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize path tracer integration
     * @param width Output resolution width
     * @param height Output resolution height
     * @param useGPU Use GPU acceleration (highly recommended)
     */
    bool Initialize(int width, int height, bool useGPU = true);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Resize output
     */
    void Resize(int width, int height);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render scene with path tracing
     * @param scene Scene to render
     */
    void RenderScene(const Scene& scene);

    /**
     * @brief Render with explicit camera
     * @param camera Camera to use
     * @param primitives SDF primitives to render
     */
    void Render(const Camera& camera, const std::vector<SDFPrimitive>& primitives);

    /**
     * @brief Get output texture
     */
    std::shared_ptr<Texture> GetOutputTexture() const;

    /**
     * @brief Reset accumulation (call when camera moves)
     */
    void ResetAccumulation();

    // =========================================================================
    // Configuration Presets
    // =========================================================================

    /**
     * @brief Set quality preset
     */
    enum class QualityPreset {
        Low,        // 1 SPP, 4 bounces, fast denoising
        Medium,     // 2 SPP, 6 bounces, standard denoising
        High,       // 4 SPP, 8 bounces, high quality denoising
        Ultra,      // 8 SPP, 12 bounces, maximum quality
        Realtime    // 1 SPP, 4 bounces, aggressive optimizations for 120 FPS
    };

    void SetQualityPreset(QualityPreset preset);

    /**
     * @brief Enable/disable features
     */
    void SetEnableDispersion(bool enable);
    void SetEnableReSTIR(bool enable);
    void SetEnableDenoising(bool enable);

    /**
     * @brief Fine-tune settings
     */
    void SetMaxBounces(int bounces);
    void SetSamplesPerPixel(int samples);
    void SetEnvironmentColor(const glm::vec3& color);

    // =========================================================================
    // Performance
    // =========================================================================

    /**
     * @brief Get performance statistics
     */
    const PathTracer::Stats& GetStats() const;

    /**
     * @brief Enable automatic quality adjustment for target FPS
     * @param targetFPS Target frame rate (e.g., 60, 120)
     */
    void SetAdaptiveQuality(bool enable, float targetFPS = 120.0f);

    // =========================================================================
    // Scene Conversion
    // =========================================================================

    /**
     * @brief Convert scene nodes to SDF primitives
     * Extracts renderable geometry from scene graph
     */
    std::vector<SDFPrimitive> ConvertSceneToPrimitives(const Scene& scene);

    /**
     * @brief Create SDF primitive from parameters
     */
    static SDFPrimitive CreateSpherePrimitive(
        const glm::vec3& position,
        float radius,
        const glm::vec3& color,
        MaterialType materialType = MaterialType::Diffuse,
        float roughness = 0.5f,
        float metallic = 0.0f,
        float ior = 1.5f
    );

    static SDFPrimitive CreateGlassSphere(
        const glm::vec3& position,
        float radius,
        float ior = 1.5f,
        float dispersionStrength = 0.01f
    );

    static SDFPrimitive CreateMetalSphere(
        const glm::vec3& position,
        float radius,
        const glm::vec3& color,
        float roughness = 0.1f
    );

    static SDFPrimitive CreateLightSphere(
        const glm::vec3& position,
        float radius,
        const glm::vec3& emission,
        float intensity = 10.0f
    );

    // =========================================================================
    // Demo Scenes
    // =========================================================================

    /**
     * @brief Create Cornell Box test scene
     */
    std::vector<SDFPrimitive> CreateCornellBox();

    /**
     * @brief Create glass refraction test scene
     */
    std::vector<SDFPrimitive> CreateRefractionScene();

    /**
     * @brief Create caustics demonstration scene
     */
    std::vector<SDFPrimitive> CreateCausticsScene();

    /**
     * @brief Create dispersion (rainbow) demonstration scene
     */
    std::vector<SDFPrimitive> CreateDispersionScene();

private:
    std::unique_ptr<PathTracer> m_pathTracer;

    // Adaptive quality
    bool m_adaptiveQuality = false;
    float m_targetFPS = 120.0f;
    float m_currentFPS = 60.0f;
    int m_framesSinceFPSCheck = 0;

    void UpdateAdaptiveQuality();
};

// ============================================================================
// Inline Helper Functions
// ============================================================================

inline SDFPrimitive PathTracerIntegration::CreateSpherePrimitive(
    const glm::vec3& position,
    float radius,
    const glm::vec3& color,
    MaterialType materialType,
    float roughness,
    float metallic,
    float ior)
{
    SDFPrimitive prim;

    // Position and size
    prim.positionRadius = glm::vec4(position, radius);

    // Color
    prim.color = glm::vec4(color, 1.0f);

    // Material properties
    prim.materialProps = glm::vec4(
        static_cast<float>(materialType),
        roughness,
        metallic,
        ior
    );

    // Dispersion properties (default)
    prim.dispersionProps = glm::vec4(0.01f, 0.0f, 0.0f, 0.0f);

    // Transform matrices (identity for sphere at origin)
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
    transform = glm::scale(transform, glm::vec3(radius));
    prim.transform = transform;
    prim.inverseTransform = glm::inverse(transform);

    return prim;
}

inline SDFPrimitive PathTracerIntegration::CreateGlassSphere(
    const glm::vec3& position,
    float radius,
    float ior,
    float dispersionStrength)
{
    SDFPrimitive prim = CreateSpherePrimitive(
        position, radius, glm::vec3(1.0f),
        MaterialType::Dielectric, 0.0f, 0.0f, ior
    );

    prim.dispersionProps.x = dispersionStrength;
    return prim;
}

inline SDFPrimitive PathTracerIntegration::CreateMetalSphere(
    const glm::vec3& position,
    float radius,
    const glm::vec3& color,
    float roughness)
{
    return CreateSpherePrimitive(
        position, radius, color,
        MaterialType::Metal, roughness, 1.0f, 1.5f
    );
}

inline SDFPrimitive PathTracerIntegration::CreateLightSphere(
    const glm::vec3& position,
    float radius,
    const glm::vec3& emission,
    float intensity)
{
    SDFPrimitive prim = CreateSpherePrimitive(
        position, radius, emission * intensity,
        MaterialType::Emissive, 0.0f, 0.0f, 1.0f
    );
    return prim;
}

// ============================================================================
// Example Usage
// ============================================================================

/**
 * Example: Basic path traced rendering
 *
 * ```cpp
 * // Create path tracer
 * PathTracerIntegration pathTracer;
 * pathTracer.Initialize(1920, 1080, true);
 * pathTracer.SetQualityPreset(PathTracerIntegration::QualityPreset::Realtime);
 *
 * // Create scene
 * std::vector<SDFPrimitive> primitives;
 * primitives.push_back(pathTracer.CreateGlassSphere(glm::vec3(0, 0, 0), 1.0f, 1.5f, 0.02f));
 * primitives.push_back(pathTracer.CreateMetalSphere(glm::vec3(-2, 0, 0), 0.8f, glm::vec3(1, 0.8, 0.2), 0.1f));
 * primitives.push_back(pathTracer.CreateLightSphere(glm::vec3(0, 5, 0), 0.5f, glm::vec3(1, 1, 1), 20.0f));
 *
 * // Setup camera
 * Camera camera;
 * camera.SetPerspective(45.0f, 16.0f/9.0f, 0.1f, 100.0f);
 * camera.LookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0));
 *
 * // Render loop
 * while (running) {
 *     if (cameraMoved) {
 *         pathTracer.ResetAccumulation();
 *     }
 *
 *     pathTracer.Render(camera, primitives);
 *
 *     // Display output
 *     auto texture = pathTracer.GetOutputTexture();
 *     // ... render texture to screen
 *
 *     // Check performance
 *     const auto& stats = pathTracer.GetStats();
 *     printf("FPS: %.1f, Bounces: %.1f\n", stats.fps, stats.averageBounces);
 * }
 * ```
 */

} // namespace Nova
