#include "RadianceCascade.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "debug/DebugDraw.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <chrono>

namespace Nova {

RadianceCascade::RadianceCascade() = default;

RadianceCascade::~RadianceCascade() {
    Shutdown();
}

bool RadianceCascade::Initialize(const Config& config) {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing Radiance Cascade system");
    m_config = config;

    // Create cascade levels
    m_cascades.resize(m_config.numCascades);
    m_cascadeTextures.resize(m_config.numCascades);

    for (int i = 0; i < m_config.numCascades; ++i) {
        int resolution = m_config.baseResolution / static_cast<int>(std::pow(m_config.cascadeScale, i));
        resolution = std::max(resolution, 4); // Minimum 4x4x4

        float spacing = m_config.baseSpacing * static_cast<float>(std::pow(m_config.cascadeScale, i));

        InitializeCascade(m_cascades[i], resolution, spacing);
        spdlog::info("Cascade {}: resolution={}, spacing={}", i, resolution, spacing);
    }

    // Load propagation compute shader
    if (!LoadPropagationShader()) {
        spdlog::warn("Failed to load radiance propagation shader - GPU propagation disabled");
    }

    m_initialized = true;
    spdlog::info("Radiance Cascade system initialized");
    return true;
}

void RadianceCascade::InitializeCascade(CascadeLevel& cascade, int resolution, float spacing) {
    cascade.resolution = resolution;
    cascade.spacing = spacing;
    cascade.origin = m_config.origin;

    // Create 3D textures for radiance storage
    glGenTextures(1, &cascade.radianceTexture);
    glBindTexture(GL_TEXTURE_3D, cascade.radianceTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, resolution, resolution, resolution,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &cascade.radianceTextureHistory);
    glBindTexture(GL_TEXTURE_3D, cascade.radianceTextureHistory);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, resolution, resolution, resolution,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);

    // Initialize probe data
    int totalProbes = resolution * resolution * resolution;
    cascade.probePositions.resize(totalProbes);
    cascade.probeValidity.resize(totalProbes, 0.0f);
    cascade.needsUpdate.resize(totalProbes, true);

    // Calculate initial probe positions
    for (int z = 0; z < resolution; ++z) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                int idx = x + y * resolution + z * resolution * resolution;
                cascade.probePositions[idx] = GridToWorld(glm::ivec3(x, y, z), cascade);
            }
        }
    }
}

void RadianceCascade::Shutdown() {
    if (!m_initialized) {
        return;
    }

    for (auto& cascade : m_cascades) {
        if (cascade.radianceTexture) {
            glDeleteTextures(1, &cascade.radianceTexture);
            cascade.radianceTexture = 0;
        }
        if (cascade.radianceTextureHistory) {
            glDeleteTextures(1, &cascade.radianceTextureHistory);
            cascade.radianceTextureHistory = 0;
        }
    }

    m_cascades.clear();
    m_cascadeTextures.clear();
    m_initialized = false;
}

void RadianceCascade::Update(const glm::vec3& cameraPosition, float deltaTime) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    m_time += deltaTime;

    // Update cascade origins to follow camera
    UpdateCascadeOrigin(cameraPosition);

    // Update probes in each cascade
    m_stats.probesUpdatedThisFrame = 0;
    for (auto& cascade : m_cascades) {
        int remainingBudget = m_config.maxProbesPerFrame - m_stats.probesUpdatedThisFrame;
        if (remainingBudget > 0) {
            UpdateProbes(cascade, remainingBudget);
        }
    }
}

void RadianceCascade::UpdateCascadeOrigin(const glm::vec3& cameraPosition) {
    for (auto& cascade : m_cascades) {
        // Snap origin to grid to avoid swimming artifacts
        glm::vec3 snappedOrigin = glm::floor(cameraPosition / cascade.spacing) * cascade.spacing;

        if (glm::distance(snappedOrigin, cascade.origin) > cascade.spacing * 0.5f) {
            cascade.origin = snappedOrigin;

            // Mark all probes for update when cascade moves
            for (size_t i = 0; i < cascade.needsUpdate.size(); ++i) {
                cascade.needsUpdate[i] = true;
            }
        }
    }
}

void RadianceCascade::UpdateProbes(CascadeLevel& cascade, int maxProbes) {
    int probesUpdated = 0;

    for (size_t i = 0; i < cascade.needsUpdate.size() && probesUpdated < maxProbes; ++i) {
        if (!cascade.needsUpdate[i]) {
            continue;
        }

        // Update probe position
        int resolution = cascade.resolution;
        int z = static_cast<int>(i) / (resolution * resolution);
        int y = (static_cast<int>(i) / resolution) % resolution;
        int x = static_cast<int>(i) % resolution;

        cascade.probePositions[i] = GridToWorld(glm::ivec3(x, y, z), cascade);

        // Mark as updated
        cascade.needsUpdate[i] = false;
        cascade.probeValidity[i] = 1.0f;
        probesUpdated++;
    }

    m_stats.probesUpdatedThisFrame += probesUpdated;
}

void RadianceCascade::InjectDirectLighting(const std::vector<glm::vec3>& positions,
                                           [[maybe_unused]] const std::vector<glm::vec3>& radiance) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Inject lighting into appropriate cascade levels
    for (size_t i = 0; i < positions.size(); ++i) {
        for (auto& cascade : m_cascades) {
            glm::ivec3 gridPos = WorldToGrid(positions[i], cascade);

            // Check if position is within cascade bounds
            if (gridPos.x >= 0 && gridPos.x < cascade.resolution &&
                gridPos.y >= 0 && gridPos.y < cascade.resolution &&
                gridPos.z >= 0 && gridPos.z < cascade.resolution) {

                int idx = GetProbeIndex(gridPos, cascade);
                if (idx >= 0 && idx < static_cast<int>(cascade.needsUpdate.size())) {
                    cascade.needsUpdate[idx] = true;
                }
            }
        }
    }
}

void RadianceCascade::InjectEmissive(const glm::vec3& position, [[maybe_unused]] const glm::vec3& radiance, float radius) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Inject emissive light into cascades
    for (auto& cascade : m_cascades) {
        glm::ivec3 centerGrid = WorldToGrid(position, cascade);
        int radiusInProbes = static_cast<int>(radius / cascade.spacing) + 1;

        for (int dz = -radiusInProbes; dz <= radiusInProbes; ++dz) {
            for (int dy = -radiusInProbes; dy <= radiusInProbes; ++dy) {
                for (int dx = -radiusInProbes; dx <= radiusInProbes; ++dx) {
                    glm::ivec3 gridPos = centerGrid + glm::ivec3(dx, dy, dz);

                    if (gridPos.x >= 0 && gridPos.x < cascade.resolution &&
                        gridPos.y >= 0 && gridPos.y < cascade.resolution &&
                        gridPos.z >= 0 && gridPos.z < cascade.resolution) {

                        int idx = GetProbeIndex(gridPos, cascade);
                        if (idx >= 0 && idx < static_cast<int>(cascade.needsUpdate.size())) {
                            cascade.needsUpdate[idx] = true;
                        }
                    }
                }
            }
        }
    }
}

void RadianceCascade::PropagateLighting() {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Propagate light from fine to coarse cascades
    for (int i = 0; i < static_cast<int>(m_cascades.size()) - 1; ++i) {
        PropagateLevel(i);
    }
}

void RadianceCascade::PropagateLevel(int level) {
    if (level < 0 || level >= static_cast<int>(m_cascades.size())) {
        return;
    }

    if (!m_propagationShader || !m_propagationShader->IsValid()) {
        // Fallback to CPU propagation if shader not available
        PropagateLevelCPU(level);
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    const auto& cascade = m_cascades[level];
    int resolution = cascade.resolution;

    // Bind the propagation compute shader
    m_propagationShader->Bind();

    // Bind current cascade texture for read/write
    glBindImageTexture(0, cascade.radianceTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);

    // Bind finer cascade for reading (if exists)
    int finerResolution = 0;
    float finerSpacing = 0.0f;
    if (level > 0) {
        const auto& finerCascade = m_cascades[level - 1];
        glBindImageTexture(1, finerCascade.radianceTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
        finerResolution = finerCascade.resolution;
        finerSpacing = finerCascade.spacing;
    } else {
        // No finer cascade - bind a dummy or the same texture
        glBindImageTexture(1, cascade.radianceTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
    }

    // Bind history texture for temporal blending
    glBindImageTexture(2, cascade.radianceTextureHistory, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

    // Set uniforms
    m_propagationShader->SetInt("u_cascadeLevel", level);
    m_propagationShader->SetInt("u_resolution", resolution);
    m_propagationShader->SetInt("u_finerResolution", finerResolution);
    m_propagationShader->SetFloat("u_spacing", cascade.spacing);
    m_propagationShader->SetFloat("u_finerSpacing", finerSpacing);
    m_propagationShader->SetVec3("u_cascadeOrigin", cascade.origin);

    // Ray tracing parameters
    m_propagationShader->SetInt("u_raysPerProbe", m_config.raysPerProbe);
    m_propagationShader->SetInt("u_maxSteps", 64);
    m_propagationShader->SetFloat("u_maxDistance", cascade.spacing * 4.0f);

    // Temporal blending
    m_propagationShader->SetFloat("u_temporalBlend", m_config.temporalBlend);
    m_propagationShader->SetInt("u_frameIndex", m_frameIndex);

    // Scene bounds (use cascade bounds as default)
    float cascadeExtent = cascade.spacing * resolution;
    glm::vec3 sceneMin = cascade.origin - glm::vec3(cascadeExtent * 0.1f);
    glm::vec3 sceneMax = cascade.origin + glm::vec3(cascadeExtent * 1.1f);
    m_propagationShader->SetVec3("u_sceneMin", sceneMin);
    m_propagationShader->SetVec3("u_sceneMax", sceneMax);
    m_propagationShader->SetBool("u_hasSDF", false); // SDF binding handled externally if available

    // Lighting
    m_propagationShader->SetInt("u_numLights", 0); // Lights injected via InjectDirectLighting
    m_propagationShader->SetVec3("u_ambientLight", glm::vec3(0.1f, 0.12f, 0.15f)); // Default ambient

    // Dispatch compute shader
    // Work group size is 4x4x4, so calculate number of groups needed
    int groupsX = (resolution + 3) / 4;
    int groupsY = (resolution + 3) / 4;
    int groupsZ = (resolution + 3) / 4;

    glDispatchCompute(groupsX, groupsY, groupsZ);

    // Memory barrier to ensure writes are complete before next level
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // Swap current and history textures for next frame
    std::swap(const_cast<CascadeLevel&>(cascade).radianceTexture,
              const_cast<CascadeLevel&>(cascade).radianceTextureHistory);

    Shader::Unbind();

    auto endTime = std::chrono::high_resolution_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_stats.propagationTimeMs += elapsedMs;

    m_frameIndex++;
}

void RadianceCascade::PropagateLevelCPU(int level) {
    // CPU fallback for when compute shader is not available
    // This is a simple placeholder that could be expanded for debugging
    if (level < 0 || level >= static_cast<int>(m_cascades.size())) {
        return;
    }

    const auto& cascade = m_cascades[level];
    int resolution = cascade.resolution;
    int totalProbes = resolution * resolution * resolution;

    // Simple ambient fill for CPU fallback
    std::vector<float> radianceData(totalProbes * 4);
    glm::vec3 ambient(0.1f, 0.12f, 0.15f);

    for (int i = 0; i < totalProbes; ++i) {
        radianceData[i * 4 + 0] = ambient.r;
        radianceData[i * 4 + 1] = ambient.g;
        radianceData[i * 4 + 2] = ambient.b;
        radianceData[i * 4 + 3] = 1.0f; // Valid
    }

    glBindTexture(GL_TEXTURE_3D, cascade.radianceTexture);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0,
                    resolution, resolution, resolution,
                    GL_RGBA, GL_FLOAT, radianceData.data());
    glBindTexture(GL_TEXTURE_3D, 0);
}

bool RadianceCascade::LoadPropagationShader() {
    // Try to load the propagation compute shader
    std::string shaderPath = "assets/shaders/radiance_propagate.comp";

    std::ifstream file(shaderPath);
    if (!file.is_open()) {
        spdlog::error("Failed to open radiance propagation shader: {}", shaderPath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string shaderSource = buffer.str();
    file.close();

    m_propagationShader = std::make_shared<Shader>();
    if (!m_propagationShader->LoadComputeShader(shaderSource)) {
        spdlog::error("Failed to compile radiance propagation shader");
        m_propagationShader.reset();
        return false;
    }

    spdlog::info("Radiance propagation compute shader loaded successfully");
    return true;
}

glm::vec3 RadianceCascade::SampleRadiance(const glm::vec3& worldPos, [[maybe_unused]] const glm::vec3& normal) const {
    if (!m_initialized || !m_enabled) {
        return glm::vec3(0.0f);
    }

    // Sample from finest cascade that contains the position
    for (const auto& cascade : m_cascades) {
        glm::ivec3 gridPos = WorldToGrid(worldPos, cascade);

        if (gridPos.x >= 0 && gridPos.x < cascade.resolution &&
            gridPos.y >= 0 && gridPos.y < cascade.resolution &&
            gridPos.z >= 0 && gridPos.z < cascade.resolution) {

            return SampleCascade(worldPos, 0);
        }
    }

    return glm::vec3(0.0f);
}

glm::vec3 RadianceCascade::SampleCascade(const glm::vec3& worldPos, int cascadeLevel) const {
    if (cascadeLevel < 0 || cascadeLevel >= static_cast<int>(m_cascades.size())) {
        return glm::vec3(0.0f);
    }

    const auto& cascade = m_cascades[cascadeLevel];

    // Convert world position to texture coordinates [0, 1]
    glm::vec3 localPos = (worldPos - cascade.origin) / (cascade.spacing * static_cast<float>(cascade.resolution));
    // Note: texCoord reserved for future GPU texture sampling implementation
    [[maybe_unused]] glm::vec3 texCoord = localPos + 0.5f; // Center in texture

    // Sample using hardware trilinear filtering
    // This would be done in shader, here we just return a placeholder
    return glm::vec3(0.0f);
}

uint32_t RadianceCascade::GetCascadeTexture(int level) const {
    if (level < 0 || level >= static_cast<int>(m_cascades.size())) {
        return 0;
    }
    return m_cascades[level].radianceTexture;
}

void RadianceCascade::SetConfig(const Config& config) {
    if (m_initialized) {
        Shutdown();
    }
    m_config = config;
    if (m_initialized) {
        Initialize(config);
    }
}

void RadianceCascade::Clear() {
    for (auto& cascade : m_cascades) {
        // Clear textures to black
        if (cascade.radianceTexture) {
            std::vector<float> zeros(cascade.resolution * cascade.resolution * cascade.resolution * 4, 0.0f);
            glBindTexture(GL_TEXTURE_3D, cascade.radianceTexture);
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0,
                           cascade.resolution, cascade.resolution, cascade.resolution,
                           GL_RGBA, GL_FLOAT, zeros.data());
        }

        // Reset probe validity
        std::fill(cascade.probeValidity.begin(), cascade.probeValidity.end(), 0.0f);
        std::fill(cascade.needsUpdate.begin(), cascade.needsUpdate.end(), true);
    }
}

void RadianceCascade::DebugDraw(Renderer* renderer) {
    if (!m_initialized || !renderer) {
        return;
    }

    auto& debugDraw = renderer->GetDebugDraw();

    // Draw probes as small spheres
    for (size_t cascadeIdx = 0; cascadeIdx < m_cascades.size(); ++cascadeIdx) {
        const auto& cascade = m_cascades[cascadeIdx];

        // Color based on cascade level
        glm::vec4 color = glm::vec4(1.0f, 1.0f - cascadeIdx * 0.25f, 0.0f, 1.0f);

        // Only draw a subset of probes to avoid clutter
        int step = std::max(1, cascade.resolution / 8);

        for (int z = 0; z < cascade.resolution; z += step) {
            for (int y = 0; y < cascade.resolution; y += step) {
                for (int x = 0; x < cascade.resolution; x += step) {
                    int idx = GetProbeIndex(glm::ivec3(x, y, z), cascade);
                    if (idx >= 0 && cascade.probeValidity[idx] > 0.5f) {
                        debugDraw.AddSphere(cascade.probePositions[idx],
                                           cascade.spacing * 0.1f, color);
                    }
                }
            }
        }
    }
}

glm::ivec3 RadianceCascade::WorldToGrid(const glm::vec3& worldPos, const CascadeLevel& cascade) const {
    glm::vec3 localPos = (worldPos - cascade.origin) / cascade.spacing;
    return glm::ivec3(glm::floor(localPos));
}

glm::vec3 RadianceCascade::GridToWorld(const glm::ivec3& gridPos, const CascadeLevel& cascade) const {
    return cascade.origin + glm::vec3(gridPos) * cascade.spacing;
}

int RadianceCascade::GetProbeIndex(const glm::ivec3& gridPos, const CascadeLevel& cascade) const {
    if (gridPos.x < 0 || gridPos.x >= cascade.resolution ||
        gridPos.y < 0 || gridPos.y >= cascade.resolution ||
        gridPos.z < 0 || gridPos.z >= cascade.resolution) {
        return -1;
    }
    return gridPos.x + gridPos.y * cascade.resolution + gridPos.z * cascade.resolution * cascade.resolution;
}

} // namespace Nova
