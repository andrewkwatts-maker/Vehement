#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <random>
#include <memory>
#include <cstdint>

namespace Nova {

class Camera;
class Scene;
class Shader;
class Framebuffer;
class Texture;
class RadianceCascade;

// ============================================================================
// Material and Ray Structures
// ============================================================================

enum class MaterialType {
    Diffuse = 0,
    Metal = 1,
    Dielectric = 2,  // Glass, water, etc.
    Emissive = 3
};

/**
 * @brief Material properties for path tracing
 */
struct PathTraceMaterial {
    MaterialType type = MaterialType::Diffuse;
    glm::vec3 albedo{0.8f};
    glm::vec3 emission{0.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ior = 1.5f;  // Index of refraction

    // Dispersion - Cauchy coefficients for wavelength-dependent IOR
    // IOR(λ) = baseIOR + cauchyB / λ² + cauchyC / λ⁴
    float cauchyB = 0.01f;  // Dispersion coefficient (glass ~0.01)
    float cauchyC = 0.0f;   // Higher order dispersion

    /**
     * @brief Get IOR for specific wavelength (nm)
     * Uses Cauchy's equation for dispersion
     */
    float GetIOR(float wavelength) const;
};

/**
 * @brief Ray structure for path tracing
 */
struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    float wavelength = 550.0f;  // nm (green light default)
    int depth = 0;

    glm::vec3 At(float t) const { return origin + t * direction; }
};

/**
 * @brief Hit record for ray-scene intersection
 */
struct HitRecord {
    float t = FLT_MAX;
    glm::vec3 point;
    glm::vec3 normal;
    PathTraceMaterial material;
    bool frontFace = true;

    void SetFaceNormal(const Ray& ray, const glm::vec3& outwardNormal);
};

/**
 * @brief SDF primitive for GPU path tracing
 */
struct SDFPrimitive {
    glm::vec4 positionRadius;  // xyz = position, w = radius/size
    glm::vec4 color;
    glm::vec4 materialProps;   // x = type, y = roughness, z = metallic, w = ior
    glm::vec4 dispersionProps; // x = cauchyB, y = cauchyC, z = unused, w = unused
    glm::mat4 transform;
    glm::mat4 inverseTransform;
};

// ============================================================================
// Path Tracer Core
// ============================================================================

/**
 * @brief High-performance physically-based path tracer for SDF-first engine
 *
 * Architecture (SOLID principles):
 * - PathTracerConfig: Separate configuration data class
 * - ISampler: Interface for pluggable sampling strategies
 * - IIntegrator: Interface for path integration methods
 * - RayGenerator: Separate ray generation system
 * - SDFHitResolver: SDF raymarching and hit detection
 * - MaterialShader: Material evaluation and scattering
 * - AccumulationBuffer: Frame accumulation with temporal reprojection
 *
 * Features:
 * - Multiple bounce path tracing (diffuse, specular, refraction)
 * - Physically accurate refraction using Snell's law
 * - Chromatic dispersion (wavelength-dependent IOR)
 * - Caustics rendering
 * - GPU compute shader acceleration
 * - ReSTIR importance resampling for 1SPP quality
 * - SVGF temporal denoising
 * - RadianceCascade GI integration
 * - Adaptive sampling based on variance
 * - Temporal reprojection with motion vectors
 * - Blue noise sampling for improved spatial distribution
 * - Support for SDF geometry (primary) and polygons
 *
 * Performance Target: 145+ FPS at 1080p with GPU acceleration
 */
class PathTracer {
public:
    PathTracer();
    ~PathTracer();

    // Non-copyable but movable
    PathTracer(const PathTracer&) = delete;
    PathTracer& operator=(const PathTracer&) = delete;
    PathTracer(PathTracer&&) noexcept = default;
    PathTracer& operator=(PathTracer&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize path tracer
     * @param width Output resolution width
     * @param height Output resolution height
     * @param useGPU Use GPU compute shader (recommended)
     */
    bool Initialize(int width, int height, bool useGPU = true);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Resize output buffer
     */
    void Resize(int width, int height);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render a single frame
     * @param camera Camera for view
     * @param primitives SDF primitives to render
     */
    void Render(const Camera& camera, const std::vector<SDFPrimitive>& primitives);

    /**
     * @brief Reset accumulation buffer (call when camera moves)
     */
    void ResetAccumulation();

    /**
     * @brief Get output texture
     */
    std::shared_ptr<Texture> GetOutputTexture() const { return m_outputTexture; }

    /**
     * @brief Get raw output data (for CPU path tracer)
     */
    const std::vector<glm::vec3>& GetOutputData() const { return m_outputData; }

    // =========================================================================
    // Settings
    // =========================================================================

    void SetMaxBounces(int bounces) { m_maxBounces = bounces; }
    void SetSamplesPerPixel(int samples) { m_samplesPerPixel = samples; }
    void SetEnableDispersion(bool enable) { m_enableDispersion = enable; }
    void SetEnableReSTIR(bool enable) { m_enableReSTIR = enable; }
    void SetEnableDenoising(bool enable) { m_enableDenoising = enable; }
    void SetEnvironmentColor(const glm::vec3& color) { m_envColor = color; }

    int GetMaxBounces() const { return m_maxBounces; }
    int GetSamplesPerPixel() const { return m_samplesPerPixel; }
    bool IsDispersionEnabled() const { return m_enableDispersion; }
    bool IsReSTIREnabled() const { return m_enableReSTIR; }
    bool IsDenoisingEnabled() const { return m_enableDenoising; }

    // =========================================================================
    // Performance Stats
    // =========================================================================

    struct Stats {
        float renderTimeMs = 0.0f;
        float traceTimeMs = 0.0f;
        float restirTimeMs = 0.0f;
        float denoiseTimeMs = 0.0f;
        int totalRays = 0;
        int primaryRays = 0;
        int secondaryRays = 0;
        float averageBounces = 0.0f;
        int frameCount = 0;
        float fps = 0.0f;
    };

    const Stats& GetStats() const { return m_stats; }

private:
    // =========================================================================
    // CPU Path Tracing (Reference Implementation)
    // =========================================================================

    glm::vec3 TraceRay(const Ray& ray, const std::vector<SDFPrimitive>& primitives, int depth);
    bool ScatterRay(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation);

    // Material-specific scattering
    bool ScatterDiffuse(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation);
    bool ScatterMetal(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation);
    bool ScatterDielectric(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation);

    // SDF evaluation and intersection
    float EvaluateSDF(const glm::vec3& p, const std::vector<SDFPrimitive>& primitives,
                      int& hitIndex, PathTraceMaterial& hitMaterial);
    bool RaymarchSDF(const Ray& ray, const std::vector<SDFPrimitive>& primitives, HitRecord& hit);
    glm::vec3 CalculateNormal(const glm::vec3& p, const std::vector<SDFPrimitive>& primitives);

    // Refraction with dispersion
    glm::vec3 Refract(const glm::vec3& v, const glm::vec3& n, float etai_over_etat);
    float Reflectance(float cosine, float refIdx);  // Schlick's approximation

    // Wavelength to RGB conversion
    glm::vec3 WavelengthToRGB(float wavelength);

    // Random number generation
    float Random01();
    glm::vec3 RandomInUnitSphere();
    glm::vec3 RandomUnitVector();
    glm::vec3 RandomInHemisphere(const glm::vec3& normal);
    float RandomWavelength();  // 380-780nm

    // =========================================================================
    // GPU Path Tracing (High Performance)
    // =========================================================================

    void RenderGPU(const Camera& camera, const std::vector<SDFPrimitive>& primitives);
    void RenderCPU(const Camera& camera, const std::vector<SDFPrimitive>& primitives);

    void CreateGPUResources();
    void UpdateGPUBuffers(const std::vector<SDFPrimitive>& primitives);
    void BindGPUResources();
    void DispatchGPUCompute();

    // =========================================================================
    // Post-Processing
    // =========================================================================

    void ApplyReSTIR();     // Reservoir-based importance resampling
    void ApplyDenoising();  // SVGF temporal denoising
    void ApplyToneMapping(); // ACES filmic tone mapping

    // Tone mapping helper
    glm::vec3 ACESToneMap(const glm::vec3& color);

    // =========================================================================
    // RadianceCascade GI Integration
    // =========================================================================

    /**
     * @brief Set radiance cascade for global illumination
     */
    void SetRadianceCascade(std::shared_ptr<RadianceCascade> cascade);

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Resolution
    int m_width = 1920;
    int m_height = 1080;

    // Settings
    int m_maxBounces = 8;
    int m_samplesPerPixel = 4;
    bool m_enableDispersion = true;
    bool m_enableReSTIR = true;
    bool m_enableDenoising = true;
    bool m_useGPU = true;
    glm::vec3 m_envColor{0.5f, 0.7f, 1.0f};

    // GPU Resources
    std::unique_ptr<Shader> m_pathTraceShader;
    std::unique_ptr<Shader> m_restirShader;
    std::unique_ptr<Shader> m_denoiseShader;
    std::unique_ptr<Shader> m_toneMapShader;

    std::shared_ptr<Texture> m_outputTexture;
    std::shared_ptr<Texture> m_accumulationTexture;
    std::shared_ptr<Texture> m_albedoTexture;    // For denoising
    std::shared_ptr<Texture> m_normalTexture;    // For denoising
    std::shared_ptr<Texture> m_depthTexture;     // For denoising
    std::shared_ptr<Texture> m_reservoirTexture; // For ReSTIR

    uint32_t m_sdfBuffer = 0;  // SSBO for SDF primitives
    uint32_t m_screenQuadVAO = 0;
    uint32_t m_screenQuadVBO = 0;

    // CPU Resources
    std::vector<glm::vec3> m_outputData;
    std::vector<glm::vec3> m_accumulationBuffer;

    // State
    int m_frameCount = 0;
    Stats m_stats;

    // Random number generation
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist{0.0f, 1.0f};

    bool m_initialized = false;

    // =========================================================================
    // Pimpl for internal implementation details
    // =========================================================================

    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert wavelength (nm) to RGB color
 * Uses CIE 1931 color matching approximation
 */
glm::vec3 WavelengthToRGB(float wavelength);

/**
 * @brief Convert RGB wavelengths to spectral for dispersion
 */
glm::vec3 RGBToSpectral(const glm::vec3& rgb);

/**
 * @brief Sample wavelength from RGB color
 */
float SampleWavelengthFromRGB(const glm::vec3& rgb, float random);

} // namespace Nova
