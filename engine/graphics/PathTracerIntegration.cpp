#include "PathTracerIntegration.hpp"
#include "../core/Logger.hpp"
#include "../scene/SceneNode.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <functional>

namespace Nova {

PathTracerIntegration::PathTracerIntegration()
    : m_pathTracer(std::make_unique<PathTracer>())
{
}

PathTracerIntegration::~PathTracerIntegration() {
    Shutdown();
}

bool PathTracerIntegration::Initialize(int width, int height, bool useGPU) {
    Logger::Info("Initializing PathTracerIntegration (%dx%d)", width, height);

    if (!m_pathTracer->Initialize(width, height, useGPU)) {
        Logger::Error("Failed to initialize PathTracer");
        return false;
    }

    // Default to realtime preset
    SetQualityPreset(QualityPreset::Realtime);

    return true;
}

void PathTracerIntegration::Shutdown() {
    if (m_pathTracer) {
        m_pathTracer->Shutdown();
    }
}

void PathTracerIntegration::Resize(int width, int height) {
    m_pathTracer->Resize(width, height);
}

void PathTracerIntegration::RenderScene(const Scene& scene) {
    // Convert scene to primitives
    auto primitives = ConvertSceneToPrimitives(scene);

    // Get camera
    const Camera* camera = scene.GetCamera();
    if (!camera) {
        Logger::Warn("Scene has no camera for path tracing");
        return;
    }

    // Render
    Render(*camera, primitives);
}

void PathTracerIntegration::Render(const Camera& camera, const std::vector<SDFPrimitive>& primitives) {
    m_pathTracer->Render(camera, primitives);

    // Update adaptive quality
    if (m_adaptiveQuality) {
        UpdateAdaptiveQuality();
    }
}

std::shared_ptr<Texture> PathTracerIntegration::GetOutputTexture() const {
    return m_pathTracer->GetOutputTexture();
}

void PathTracerIntegration::ResetAccumulation() {
    m_pathTracer->ResetAccumulation();
}

// ============================================================================
// Quality Presets
// ============================================================================

void PathTracerIntegration::SetQualityPreset(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Low:
            m_pathTracer->SetSamplesPerPixel(1);
            m_pathTracer->SetMaxBounces(4);
            m_pathTracer->SetEnableDispersion(false);
            m_pathTracer->SetEnableReSTIR(true);
            m_pathTracer->SetEnableDenoising(true);
            Logger::Info("Path tracer quality: Low (1 SPP, 4 bounces)");
            break;

        case QualityPreset::Medium:
            m_pathTracer->SetSamplesPerPixel(2);
            m_pathTracer->SetMaxBounces(6);
            m_pathTracer->SetEnableDispersion(true);
            m_pathTracer->SetEnableReSTIR(true);
            m_pathTracer->SetEnableDenoising(true);
            Logger::Info("Path tracer quality: Medium (2 SPP, 6 bounces)");
            break;

        case QualityPreset::High:
            m_pathTracer->SetSamplesPerPixel(4);
            m_pathTracer->SetMaxBounces(8);
            m_pathTracer->SetEnableDispersion(true);
            m_pathTracer->SetEnableReSTIR(true);
            m_pathTracer->SetEnableDenoising(true);
            Logger::Info("Path tracer quality: High (4 SPP, 8 bounces)");
            break;

        case QualityPreset::Ultra:
            m_pathTracer->SetSamplesPerPixel(8);
            m_pathTracer->SetMaxBounces(12);
            m_pathTracer->SetEnableDispersion(true);
            m_pathTracer->SetEnableReSTIR(true);
            m_pathTracer->SetEnableDenoising(true);
            Logger::Info("Path tracer quality: Ultra (8 SPP, 12 bounces)");
            break;

        case QualityPreset::Realtime:
            m_pathTracer->SetSamplesPerPixel(1);
            m_pathTracer->SetMaxBounces(4);
            m_pathTracer->SetEnableDispersion(true);
            m_pathTracer->SetEnableReSTIR(true);
            m_pathTracer->SetEnableDenoising(true);
            Logger::Info("Path tracer quality: Realtime (1 SPP, 4 bounces, optimized for 120 FPS)");
            break;
    }
}

void PathTracerIntegration::SetEnableDispersion(bool enable) {
    m_pathTracer->SetEnableDispersion(enable);
}

void PathTracerIntegration::SetEnableReSTIR(bool enable) {
    m_pathTracer->SetEnableReSTIR(enable);
}

void PathTracerIntegration::SetEnableDenoising(bool enable) {
    m_pathTracer->SetEnableDenoising(enable);
}

void PathTracerIntegration::SetMaxBounces(int bounces) {
    m_pathTracer->SetMaxBounces(bounces);
}

void PathTracerIntegration::SetSamplesPerPixel(int samples) {
    m_pathTracer->SetSamplesPerPixel(samples);
}

void PathTracerIntegration::SetEnvironmentColor(const glm::vec3& color) {
    m_pathTracer->SetEnvironmentColor(color);
}

// ============================================================================
// Performance
// ============================================================================

const PathTracer::Stats& PathTracerIntegration::GetStats() const {
    return m_pathTracer->GetStats();
}

void PathTracerIntegration::SetAdaptiveQuality(bool enable, float targetFPS) {
    m_adaptiveQuality = enable;
    m_targetFPS = targetFPS;

    if (enable) {
        Logger::Info("Adaptive quality enabled, target: %.0f FPS", targetFPS);
    }
}

void PathTracerIntegration::UpdateAdaptiveQuality() {
    const auto& stats = m_pathTracer->GetStats();
    m_framesSinceFPSCheck++;

    // Check every 30 frames
    if (m_framesSinceFPSCheck < 30) {
        return;
    }

    m_framesSinceFPSCheck = 0;
    m_currentFPS = stats.fps;

    // Adjust quality based on FPS
    if (m_currentFPS < m_targetFPS * 0.9f) {
        // Too slow, reduce quality
        int currentSPP = m_pathTracer->GetSamplesPerPixel();
        if (currentSPP > 1) {
            m_pathTracer->SetSamplesPerPixel(currentSPP - 1);
            Logger::Info("Adaptive quality: Reduced SPP to %d (FPS: %.1f)", currentSPP - 1, m_currentFPS);
        }
    } else if (m_currentFPS > m_targetFPS * 1.1f) {
        // Room to improve, increase quality
        int currentSPP = m_pathTracer->GetSamplesPerPixel();
        if (currentSPP < 4) {
            m_pathTracer->SetSamplesPerPixel(currentSPP + 1);
            Logger::Info("Adaptive quality: Increased SPP to %d (FPS: %.1f)", currentSPP + 1, m_currentFPS);
        }
    }
}

// ============================================================================
// Scene Conversion
// ============================================================================

std::vector<SDFPrimitive> PathTracerIntegration::ConvertSceneToPrimitives(const Scene& scene) {
    std::vector<SDFPrimitive> primitives;

    // Get root node
    const SceneNode* root = scene.GetRoot();
    if (!root) {
        Logger::Warn("Scene has no root node");
        return primitives;
    }

    // Reserve space for efficiency
    primitives.reserve(64);

    // Recursive lambda to traverse scene graph
    std::function<void(const SceneNode&, const glm::mat4&)> traverseNode;
    traverseNode = [&](const SceneNode& node, const glm::mat4& parentTransform) {
        // Skip invisible nodes
        if (!node.IsVisible()) {
            return;
        }

        // Calculate world transform
        glm::mat4 worldTransform = parentTransform * node.GetLocalTransform();

        // Check if node has renderable content
        if (node.HasMesh()) {
            const auto& mesh = node.GetMesh();

            // For now, convert meshes to sphere primitives based on bounding box
            // A full implementation would use the mesh's SDF representation if available
            if (mesh) {
                glm::vec3 boundsMin = mesh->GetBoundsMin();
                glm::vec3 boundsMax = mesh->GetBoundsMax();

                // Calculate center and radius
                glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
                glm::vec3 halfExtent = (boundsMax - boundsMin) * 0.5f;
                float radius = glm::length(halfExtent);

                // Transform center to world space
                glm::vec4 worldCenter = worldTransform * glm::vec4(center, 1.0f);

                // Extract scale from transform for radius adjustment
                glm::vec3 scale(
                    glm::length(glm::vec3(worldTransform[0])),
                    glm::length(glm::vec3(worldTransform[1])),
                    glm::length(glm::vec3(worldTransform[2]))
                );
                float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
                radius *= avgScale;

                // Get material properties
                glm::vec3 color(0.7f, 0.7f, 0.7f);
                float roughness = 0.5f;
                float metallic = 0.0f;
                MaterialType matType = MaterialType::Diffuse;

                if (node.HasMaterial()) {
                    const auto& material = node.GetMaterial();
                    if (material) {
                        // Extract material properties
                        // Note: Actual implementation depends on Material class API
                        // Using defaults for now
                    }
                }

                // Create sphere primitive as approximation
                SDFPrimitive prim = CreateSpherePrimitive(
                    glm::vec3(worldCenter),
                    radius,
                    color,
                    matType,
                    roughness,
                    metallic,
                    1.5f
                );

                primitives.push_back(prim);
            }
        }

        // Traverse children
        for (const auto& child : node.GetChildren()) {
            if (child) {
                traverseNode(*child, worldTransform);
            }
        }
    };

    // Start traversal from root with identity transform
    traverseNode(*root, glm::mat4(1.0f));

    Logger::Info("Converted scene to %zu SDF primitives", primitives.size());
    return primitives;
}

// ============================================================================
// Demo Scenes
// ============================================================================

std::vector<SDFPrimitive> PathTracerIntegration::CreateCornellBox() {
    std::vector<SDFPrimitive> primitives;

    // Floor (white)
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, -1000.5f, 0), 1000.0f,
        glm::vec3(0.8f, 0.8f, 0.8f)
    ));

    // Left wall (red)
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(-1001.0f, 0, 0), 1000.0f,
        glm::vec3(0.8f, 0.1f, 0.1f)
    ));

    // Right wall (green)
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(1001.0f, 0, 0), 1000.0f,
        glm::vec3(0.1f, 0.8f, 0.1f)
    ));

    // Back wall (white)
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, 0, -1001.0f), 1000.0f,
        glm::vec3(0.8f, 0.8f, 0.8f)
    ));

    // Ceiling (white)
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, 1001.0f, 0), 1000.0f,
        glm::vec3(0.8f, 0.8f, 0.8f)
    ));

    // Glass sphere
    primitives.push_back(CreateGlassSphere(
        glm::vec3(-0.5f, -0.2f, 0.5f), 0.3f, 1.5f, 0.01f
    ));

    // Metal sphere
    primitives.push_back(CreateMetalSphere(
        glm::vec3(0.5f, -0.2f, -0.5f), 0.3f,
        glm::vec3(1.0f, 0.85f, 0.6f), 0.05f
    ));

    // Light
    primitives.push_back(CreateLightSphere(
        glm::vec3(0, 0.8f, 0), 0.2f,
        glm::vec3(1, 1, 1), 15.0f
    ));

    return primitives;
}

std::vector<SDFPrimitive> PathTracerIntegration::CreateRefractionScene() {
    std::vector<SDFPrimitive> primitives;

    // Ground
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, -1000.5f, 0), 1000.0f,
        glm::vec3(0.5f, 0.5f, 0.5f)
    ));

    // Multiple glass spheres with different IORs
    primitives.push_back(CreateGlassSphere(glm::vec3(-1.5f, 0, 0), 0.5f, 1.3f, 0.005f)); // Water
    primitives.push_back(CreateGlassSphere(glm::vec3(0.0f, 0, 0), 0.5f, 1.5f, 0.01f));   // Glass
    primitives.push_back(CreateGlassSphere(glm::vec3(1.5f, 0, 0), 0.5f, 2.4f, 0.03f));   // Diamond

    // Lights
    primitives.push_back(CreateLightSphere(
        glm::vec3(-3, 3, 3), 0.5f,
        glm::vec3(1, 0.9, 0.8), 20.0f
    ));

    primitives.push_back(CreateLightSphere(
        glm::vec3(3, 3, -3), 0.5f,
        glm::vec3(0.8, 0.9, 1), 20.0f
    ));

    return primitives;
}

std::vector<SDFPrimitive> PathTracerIntegration::CreateCausticsScene() {
    std::vector<SDFPrimitive> primitives;

    // Ground plane
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, -1.5f, 0), 1000.0f,
        glm::vec3(0.9f, 0.9f, 0.9f)
    ));

    // Glass sphere above ground (creates caustics)
    primitives.push_back(CreateGlassSphere(
        glm::vec3(0, 0.5f, 0), 0.8f, 1.5f, 0.01f
    ));

    // Strong directional light from above
    primitives.push_back(CreateLightSphere(
        glm::vec3(0, 5, 0), 1.0f,
        glm::vec3(1, 1, 1), 30.0f
    ));

    return primitives;
}

std::vector<SDFPrimitive> PathTracerIntegration::CreateDispersionScene() {
    std::vector<SDFPrimitive> primitives;

    // Dark background
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, 0, -1005.0f), 1000.0f,
        glm::vec3(0.1f, 0.1f, 0.1f)
    ));

    // Floor
    primitives.push_back(CreateSpherePrimitive(
        glm::vec3(0, -1001.0f, 0), 1000.0f,
        glm::vec3(0.2f, 0.2f, 0.2f)
    ));

    // Prism (glass sphere with high dispersion)
    primitives.push_back(CreateGlassSphere(
        glm::vec3(0, 0, 0), 1.0f, 1.5f, 0.05f  // High dispersion
    ));

    // White light source
    primitives.push_back(CreateLightSphere(
        glm::vec3(-3, 0, 2), 0.3f,
        glm::vec3(1, 1, 1), 25.0f
    ));

    return primitives;
}

} // namespace Nova
