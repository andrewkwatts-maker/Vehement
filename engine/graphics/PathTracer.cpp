#include "PathTracer.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"
#include "../scene/Camera.hpp"
#include "../core/Logger.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Nova {

// ============================================================================
// PathTraceMaterial Implementation
// ============================================================================

float PathTraceMaterial::GetIOR(float wavelength) const {
    // Cauchy's equation: n(λ) = A + B/λ² + C/λ⁴
    // wavelength in micrometers
    float lambda = wavelength / 1000.0f;
    float lambda2 = lambda * lambda;
    float lambda4 = lambda2 * lambda2;
    return ior + cauchyB / lambda2 + cauchyC / lambda4;
}

// ============================================================================
// HitRecord Implementation
// ============================================================================

void HitRecord::SetFaceNormal(const Ray& ray, const glm::vec3& outwardNormal) {
    frontFace = glm::dot(ray.direction, outwardNormal) < 0.0f;
    normal = frontFace ? outwardNormal : -outwardNormal;
}

// ============================================================================
// Helper Functions
// ============================================================================

glm::vec3 WavelengthToRGB(float wavelength) {
    // Wavelength in nm (380-780)
    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (wavelength >= 380.0f && wavelength < 440.0f) {
        r = -(wavelength - 440.0f) / (440.0f - 380.0f);
        b = 1.0f;
    } else if (wavelength >= 440.0f && wavelength < 490.0f) {
        g = (wavelength - 440.0f) / (490.0f - 440.0f);
        b = 1.0f;
    } else if (wavelength >= 490.0f && wavelength < 510.0f) {
        g = 1.0f;
        b = -(wavelength - 510.0f) / (510.0f - 490.0f);
    } else if (wavelength >= 510.0f && wavelength < 580.0f) {
        r = (wavelength - 510.0f) / (580.0f - 510.0f);
        g = 1.0f;
    } else if (wavelength >= 580.0f && wavelength < 645.0f) {
        r = 1.0f;
        g = -(wavelength - 645.0f) / (645.0f - 580.0f);
    } else if (wavelength >= 645.0f && wavelength <= 780.0f) {
        r = 1.0f;
    }

    // Let the intensity fall off near the vision limits
    float factor = 1.0f;
    if (wavelength >= 380.0f && wavelength < 420.0f) {
        factor = 0.3f + 0.7f * (wavelength - 380.0f) / (420.0f - 380.0f);
    } else if (wavelength >= 700.0f && wavelength <= 780.0f) {
        factor = 0.3f + 0.7f * (780.0f - wavelength) / (780.0f - 700.0f);
    }

    return glm::vec3(r, g, b) * factor;
}

glm::vec3 RGBToSpectral(const glm::vec3& rgb) {
    // Simplified RGB to spectral conversion
    // Returns dominant wavelengths for R, G, B channels
    return glm::vec3(650.0f, 550.0f, 450.0f); // Red, Green, Blue peaks
}

float SampleWavelengthFromRGB(const glm::vec3& rgb, float random) {
    // Sample wavelength based on RGB intensity
    float total = rgb.r + rgb.g + rgb.b;
    if (total < 0.001f) return 550.0f; // Default to green

    float r = random * total;
    if (r < rgb.r) return 650.0f;      // Red
    if (r < rgb.r + rgb.g) return 550.0f; // Green
    return 450.0f;                      // Blue
}

// ============================================================================
// PathTracer Implementation
// ============================================================================

PathTracer::PathTracer()
    : m_rng(std::random_device{}())
{
}

PathTracer::~PathTracer() {
    Shutdown();
}

bool PathTracer::Initialize(int width, int height, bool useGPU) {
    if (m_initialized) {
        Shutdown();
    }

    m_width = width;
    m_height = height;
    m_useGPU = useGPU;

    Logger::Info("Initializing PathTracer (%dx%d, %s)", width, height,
                 useGPU ? "GPU" : "CPU");

    if (m_useGPU) {
        CreateGPUResources();
    } else {
        // Allocate CPU buffers
        m_outputData.resize(width * height);
        m_accumulationBuffer.resize(width * height);
    }

    m_initialized = true;
    return true;
}

void PathTracer::Shutdown() {
    if (!m_initialized) return;

    if (m_sdfBuffer != 0) {
        glDeleteBuffers(1, &m_sdfBuffer);
        m_sdfBuffer = 0;
    }

    if (m_screenQuadVAO != 0) {
        glDeleteVertexArrays(1, &m_screenQuadVAO);
        glDeleteBuffers(1, &m_screenQuadVBO);
        m_screenQuadVAO = 0;
        m_screenQuadVBO = 0;
    }

    m_pathTraceShader.reset();
    m_restirShader.reset();
    m_denoiseShader.reset();
    m_toneMapShader.reset();

    m_outputTexture.reset();
    m_accumulationTexture.reset();
    m_albedoTexture.reset();
    m_normalTexture.reset();
    m_depthTexture.reset();
    m_reservoirTexture.reset();

    m_outputData.clear();
    m_accumulationBuffer.clear();

    m_initialized = false;
}

void PathTracer::Resize(int width, int height) {
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    ResetAccumulation();

    if (m_useGPU) {
        CreateGPUResources();
    } else {
        m_outputData.resize(width * height);
        m_accumulationBuffer.resize(width * height);
    }
}

void PathTracer::ResetAccumulation() {
    m_frameCount = 0;

    if (!m_useGPU) {
        std::fill(m_accumulationBuffer.begin(), m_accumulationBuffer.end(), glm::vec3(0.0f));
    }
}

void PathTracer::Render(const Camera& camera, const std::vector<SDFPrimitive>& primitives) {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (m_useGPU) {
        RenderGPU(camera, primitives);
    } else {
        RenderCPU(camera, primitives);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.renderTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_stats.fps = 1000.0f / m_stats.renderTimeMs;
    m_stats.frameCount = m_frameCount;

    m_frameCount++;
}

// ============================================================================
// CPU Path Tracing
// ============================================================================

void PathTracer::RenderCPU(const Camera& camera, const std::vector<SDFPrimitive>& primitives) {
    auto traceStart = std::chrono::high_resolution_clock::now();

    const int totalPixels = m_width * m_height;
    m_stats.primaryRays = totalPixels * m_samplesPerPixel;
    m_stats.secondaryRays = 0;

    #pragma omp parallel for
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            glm::vec3 pixelColor(0.0f);

            for (int s = 0; s < m_samplesPerPixel; s++) {
                // Jittered sampling
                float u = (x + Random01()) / float(m_width);
                float v = (y + Random01()) / float(m_height);

                // Convert to NDC
                glm::vec2 ndc(u * 2.0f - 1.0f, v * 2.0f - 1.0f);

                // Generate ray
                Ray ray;
                ray.origin = camera.GetPosition();
                ray.direction = camera.ScreenToWorldRay(
                    glm::vec2(x, m_height - y),
                    glm::vec2(m_width, m_height)
                );
                ray.wavelength = m_enableDispersion ? RandomWavelength() : 550.0f;
                ray.depth = 0;

                glm::vec3 radiance = TraceRay(ray, primitives, 0);

                if (m_enableDispersion) {
                    radiance *= WavelengthToRGB(ray.wavelength);
                }

                pixelColor += radiance;
            }

            pixelColor /= float(m_samplesPerPixel);

            // Accumulate
            int idx = y * m_width + x;
            if (m_frameCount > 0) {
                float blend = 1.0f / (m_frameCount + 1.0f);
                pixelColor = glm::mix(m_accumulationBuffer[idx], pixelColor, blend);
            }
            m_accumulationBuffer[idx] = pixelColor;

            // Tone mapping and gamma correction
            glm::vec3 finalColor = pixelColor / (pixelColor + glm::vec3(1.0f)); // Reinhard
            finalColor = glm::pow(finalColor, glm::vec3(1.0f / 2.2f));         // Gamma
            m_outputData[idx] = glm::clamp(finalColor, 0.0f, 1.0f);
        }
    }

    auto traceEnd = std::chrono::high_resolution_clock::now();
    m_stats.traceTimeMs = std::chrono::duration<float, std::milli>(traceEnd - traceStart).count();
}

glm::vec3 PathTracer::TraceRay(const Ray& ray, const std::vector<SDFPrimitive>& primitives, int depth) {
    if (depth >= m_maxBounces) {
        return glm::vec3(0.0f);
    }

    HitRecord hit;
    if (!RaymarchSDF(ray, primitives, hit)) {
        // Sky color
        float t = 0.5f * (ray.direction.y + 1.0f);
        return glm::mix(glm::vec3(1.0f), m_envColor, t);
    }

    // Emissive materials
    if (hit.material.type == MaterialType::Emissive) {
        return hit.material.emission;
    }

    // Scatter ray
    Ray scattered;
    glm::vec3 attenuation;
    if (ScatterRay(ray, hit, scattered, attenuation)) {
        m_stats.secondaryRays++;
        return attenuation * TraceRay(scattered, primitives, depth + 1);
    }

    return glm::vec3(0.0f);
}

bool PathTracer::RaymarchSDF(const Ray& ray, const std::vector<SDFPrimitive>& primitives, HitRecord& hit) {
    const float maxDist = 100.0f;
    const int maxSteps = 128;
    const float hitThreshold = 0.001f;

    float t = 0.0f;
    for (int step = 0; step < maxSteps; step++) {
        glm::vec3 p = ray.At(t);

        int hitIndex = -1;
        PathTraceMaterial material;
        float d = EvaluateSDF(p, primitives, hitIndex, material);

        if (d < hitThreshold) {
            hit.t = t;
            hit.point = p;
            hit.normal = CalculateNormal(p, primitives);
            hit.material = material;
            hit.SetFaceNormal(ray, hit.normal);
            return true;
        }

        if (t > maxDist) break;
        t += d;
    }

    return false;
}

float PathTracer::EvaluateSDF(const glm::vec3& p, const std::vector<SDFPrimitive>& primitives,
                               int& hitIndex, PathTraceMaterial& hitMaterial) {
    float minDist = 1e10f;
    hitIndex = -1;

    for (size_t i = 0; i < primitives.size(); i++) {
        const auto& prim = primitives[i];

        // Transform point to local space
        glm::vec4 localP = prim.inverseTransform * glm::vec4(p, 1.0f);

        // Sphere SDF
        float dist = glm::length(glm::vec3(localP)) - prim.positionRadius.w;

        if (dist < minDist) {
            minDist = dist;
            hitIndex = static_cast<int>(i);

            // Extract material properties
            hitMaterial.type = static_cast<MaterialType>(static_cast<int>(prim.materialProps.x));
            hitMaterial.roughness = prim.materialProps.y;
            hitMaterial.metallic = prim.materialProps.z;
            hitMaterial.ior = prim.materialProps.w;
            hitMaterial.albedo = glm::vec3(prim.color);
            hitMaterial.cauchyB = prim.dispersionProps.x;
            hitMaterial.cauchyC = prim.dispersionProps.y;
        }
    }

    return minDist;
}

glm::vec3 PathTracer::CalculateNormal(const glm::vec3& p, const std::vector<SDFPrimitive>& primitives) {
    const float h = 0.001f;
    int dummy;
    PathTraceMaterial dummyMat;

    glm::vec3 n;
    n.x = EvaluateSDF(p + glm::vec3(h, 0, 0), primitives, dummy, dummyMat) -
          EvaluateSDF(p - glm::vec3(h, 0, 0), primitives, dummy, dummyMat);
    n.y = EvaluateSDF(p + glm::vec3(0, h, 0), primitives, dummy, dummyMat) -
          EvaluateSDF(p - glm::vec3(0, h, 0), primitives, dummy, dummyMat);
    n.z = EvaluateSDF(p + glm::vec3(0, 0, h), primitives, dummy, dummyMat) -
          EvaluateSDF(p - glm::vec3(0, 0, h), primitives, dummy, dummyMat);

    return glm::normalize(n);
}

bool PathTracer::ScatterRay(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation) {
    switch (hit.material.type) {
        case MaterialType::Diffuse:
            return ScatterDiffuse(rayIn, hit, scattered, attenuation);
        case MaterialType::Metal:
            return ScatterMetal(rayIn, hit, scattered, attenuation);
        case MaterialType::Dielectric:
            return ScatterDielectric(rayIn, hit, scattered, attenuation);
        default:
            return false;
    }
}

bool PathTracer::ScatterDiffuse(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation) {
    scattered.origin = hit.point + hit.normal * 0.001f;
    scattered.direction = glm::normalize(hit.normal + RandomUnitVector());
    scattered.wavelength = rayIn.wavelength;
    scattered.depth = rayIn.depth + 1;

    attenuation = hit.material.albedo;
    return true;
}

bool PathTracer::ScatterMetal(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation) {
    glm::vec3 reflected = glm::reflect(rayIn.direction, hit.normal);
    scattered.origin = hit.point + hit.normal * 0.001f;
    scattered.direction = glm::normalize(reflected + hit.material.roughness * RandomInUnitSphere());
    scattered.wavelength = rayIn.wavelength;
    scattered.depth = rayIn.depth + 1;

    attenuation = hit.material.albedo * (1.0f - hit.material.roughness * 0.5f);
    return glm::dot(scattered.direction, hit.normal) > 0.0f;
}

bool PathTracer::ScatterDielectric(const Ray& rayIn, const HitRecord& hit, Ray& scattered, glm::vec3& attenuation) {
    float ior = hit.material.ior;

    // Apply dispersion
    if (m_enableDispersion) {
        ior = hit.material.GetIOR(rayIn.wavelength);
    }

    float etai_over_etat = hit.frontFace ? (1.0f / ior) : ior;

    glm::vec3 unitDir = glm::normalize(rayIn.direction);
    float cos_theta = std::min(glm::dot(-unitDir, hit.normal), 1.0f);
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    bool cannotRefract = etai_over_etat * sin_theta > 1.0f;

    glm::vec3 direction;
    if (cannotRefract || Reflectance(cos_theta, etai_over_etat) > Random01()) {
        direction = glm::reflect(unitDir, hit.normal);
    } else {
        direction = Refract(unitDir, hit.normal, etai_over_etat);
    }

    scattered.origin = hit.point - hit.normal * 0.001f * (hit.frontFace ? 1.0f : -1.0f);
    scattered.direction = direction;
    scattered.wavelength = rayIn.wavelength;
    scattered.depth = rayIn.depth + 1;

    attenuation = glm::vec3(0.95f); // Slight absorption
    return true;
}

glm::vec3 PathTracer::Refract(const glm::vec3& v, const glm::vec3& n, float etai_over_etat) {
    float cos_theta = std::min(glm::dot(-v, n), 1.0f);
    glm::vec3 r_out_perp = etai_over_etat * (v + cos_theta * n);
    glm::vec3 r_out_parallel = -std::sqrt(std::abs(1.0f - glm::dot(r_out_perp, r_out_perp))) * n;
    return r_out_perp + r_out_parallel;
}

float PathTracer::Reflectance(float cosine, float refIdx) {
    // Schlick's approximation
    float r0 = (1.0f - refIdx) / (1.0f + refIdx);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * std::pow(1.0f - cosine, 5.0f);
}

glm::vec3 PathTracer::WavelengthToRGB(float wavelength) {
    return ::Nova::WavelengthToRGB(wavelength);
}

// ============================================================================
// Random Number Generation
// ============================================================================

float PathTracer::Random01() {
    return m_dist(m_rng);
}

glm::vec3 PathTracer::RandomInUnitSphere() {
    while (true) {
        glm::vec3 p(Random01() * 2.0f - 1.0f,
                    Random01() * 2.0f - 1.0f,
                    Random01() * 2.0f - 1.0f);
        if (glm::dot(p, p) < 1.0f) return p;
    }
}

glm::vec3 PathTracer::RandomUnitVector() {
    return glm::normalize(RandomInUnitSphere());
}

glm::vec3 PathTracer::RandomInHemisphere(const glm::vec3& normal) {
    glm::vec3 inUnitSphere = RandomInUnitSphere();
    if (glm::dot(inUnitSphere, normal) > 0.0f) {
        return inUnitSphere;
    } else {
        return -inUnitSphere;
    }
}

float PathTracer::RandomWavelength() {
    // Sample visible spectrum (380-780nm)
    return 380.0f + Random01() * 400.0f;
}

// ============================================================================
// GPU Path Tracing (Stubs - implemented in compute shader)
// ============================================================================

void PathTracer::CreateGPUResources() {
    // TODO: Implement GPU resource creation
    // - Create compute shaders
    // - Create textures
    // - Create SSBO for primitives
    Logger::Warn("GPU path tracing not yet implemented");
}

void PathTracer::RenderGPU(const Camera& camera, const std::vector<SDFPrimitive>& primitives) {
    // TODO: Implement GPU rendering
    Logger::Warn("GPU path tracing not yet implemented - falling back to CPU");
    RenderCPU(camera, primitives);
}

void PathTracer::UpdateGPUBuffers(const std::vector<SDFPrimitive>& primitives) {
    // TODO: Upload primitive data to GPU
}

void PathTracer::BindGPUResources() {
    // TODO: Bind textures and buffers
}

void PathTracer::DispatchGPUCompute() {
    // TODO: Dispatch compute shader
}

void PathTracer::ApplyReSTIR() {
    // TODO: Implement ReSTIR
}

void PathTracer::ApplyDenoising() {
    // TODO: Implement SVGF denoising
}

void PathTracer::ApplyToneMapping() {
    // TODO: Apply tone mapping
}

} // namespace Nova
