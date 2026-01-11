#include "graphics/ClusteredLighting.hpp"
#include "graphics/Shader.hpp"
#include "scene/Camera.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>

namespace Nova {

ClusteredLightManager::ClusteredLightManager() = default;

ClusteredLightManager::~ClusteredLightManager() {
    Shutdown();
}

bool ClusteredLightManager::Initialize(int width, int height, int gridX, int gridY, int gridZ) {
    if (m_initialized) {
        spdlog::warn("ClusteredLightManager already initialized");
        return true;
    }

    spdlog::info("Initializing Clustered Lighting: viewport={}x{}, grid={}x{}x{}",
                 width, height, gridX, gridY, gridZ);

    m_viewportWidth = width;
    m_viewportHeight = height;
    m_gridDim = glm::ivec3(gridX, gridY, gridZ);

    // Calculate total clusters
    uint32_t totalClusters = gridX * gridY * gridZ;
    m_clusters.resize(totalClusters);

    // Create cluster SSBO
    // Each cluster stores: uvec2(lightCount, lightIndexOffset)
    glGenBuffers(1, &m_clusterSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, totalClusters * sizeof(glm::uvec2),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create light SSBO
    glGenBuffers(1, &m_lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GPULight) * 10000, // Support up to 10k lights
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create light index SSBO (compact index list)
    glGenBuffers(1, &m_lightIndexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightIndexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_TOTAL_LIGHT_INDICES * sizeof(uint32_t),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create atomic counter buffer
    glGenBuffers(1, &m_atomicCounterBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    // Load compute shader for light culling
    m_cullingShader = std::make_unique<Shader>();
    if (!m_cullingShader->LoadCompute("assets/shaders/clustered_light_culling.comp")) {
        spdlog::error("Failed to load clustered light culling compute shader");
        Shutdown();
        return false;
    }

    m_initialized = true;
    spdlog::info("Clustered Lighting initialized: {} clusters", totalClusters);

    return true;
}

void ClusteredLightManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_clusterSSBO) {
        glDeleteBuffers(1, &m_clusterSSBO);
        m_clusterSSBO = 0;
    }
    if (m_lightSSBO) {
        glDeleteBuffers(1, &m_lightSSBO);
        m_lightSSBO = 0;
    }
    if (m_lightIndexSSBO) {
        glDeleteBuffers(1, &m_lightIndexSSBO);
        m_lightIndexSSBO = 0;
    }
    if (m_atomicCounterBuffer) {
        glDeleteBuffers(1, &m_atomicCounterBuffer);
        m_atomicCounterBuffer = 0;
    }

    m_cullingShader.reset();
    m_lights.clear();
    m_clusters.clear();

    m_initialized = false;
    spdlog::info("Clustered Lighting shut down");
}

void ClusteredLightManager::Resize(int width, int height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
}

// =============================================================================
// Light Management
// =============================================================================

void ClusteredLightManager::ClearLights() {
    m_lights.clear();
    m_freeLightIndices.clear();
}

uint32_t ClusteredLightManager::AddPointLight(const glm::vec3& position, const glm::vec3& color,
                                               float intensity, float range) {
    GPULight light{};
    light.position = glm::vec4(position, range);
    light.direction = glm::vec4(0.0f);
    light.color = glm::vec4(color, intensity);
    light.params = glm::vec4(0.0f, static_cast<float>(LightType::Point), 1.0f, 0.0f);

    uint32_t index;
    if (!m_freeLightIndices.empty()) {
        index = m_freeLightIndices.back();
        m_freeLightIndices.pop_back();
        m_lights[index] = light;
    } else {
        index = static_cast<uint32_t>(m_lights.size());
        m_lights.push_back(light);
    }

    return index;
}

uint32_t ClusteredLightManager::AddSpotLight(const glm::vec3& position, const glm::vec3& direction,
                                              const glm::vec3& color, float intensity, float range,
                                              float innerAngle, float outerAngle) {
    GPULight light{};
    light.position = glm::vec4(position, range);
    light.direction = glm::vec4(glm::normalize(direction), glm::cos(glm::radians(innerAngle)));
    light.color = glm::vec4(color, intensity);
    light.params = glm::vec4(glm::cos(glm::radians(outerAngle)),
                            static_cast<float>(LightType::Spot), 1.0f, 0.0f);

    uint32_t index;
    if (!m_freeLightIndices.empty()) {
        index = m_freeLightIndices.back();
        m_freeLightIndices.pop_back();
        m_lights[index] = light;
    } else {
        index = static_cast<uint32_t>(m_lights.size());
        m_lights.push_back(light);
    }

    return index;
}

uint32_t ClusteredLightManager::AddDirectionalLight(const glm::vec3& direction,
                                                     const glm::vec3& color, float intensity) {
    GPULight light{};
    light.position = glm::vec4(0.0f);
    light.direction = glm::vec4(glm::normalize(direction), 0.0f);
    light.color = glm::vec4(color, intensity);
    light.params = glm::vec4(0.0f, static_cast<float>(LightType::Directional), 1.0f, 0.0f);

    uint32_t index;
    if (!m_freeLightIndices.empty()) {
        index = m_freeLightIndices.back();
        m_freeLightIndices.pop_back();
        m_lights[index] = light;
    } else {
        index = static_cast<uint32_t>(m_lights.size());
        m_lights.push_back(light);
    }

    return index;
}

void ClusteredLightManager::UpdateLight(uint32_t index, const GPULight& light) {
    if (index < m_lights.size()) {
        m_lights[index] = light;
    }
}

void ClusteredLightManager::RemoveLight(uint32_t index) {
    if (index < m_lights.size()) {
        m_lights[index].params.z = 0.0f; // Disable light
        m_freeLightIndices.push_back(index);
    }
}

// =============================================================================
// Rendering
// =============================================================================

void ClusteredLightManager::UpdateClusters(const Camera& camera) {
    if (!m_initialized || m_lights.empty()) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Update near/far from camera
    m_nearPlane = camera.GetNearPlane();
    m_farPlane = camera.GetFarPlane();

    // Build cluster grid
    BuildClusterGrid(camera);

    // Upload lights to GPU
    UploadLights();

    // Cull lights using compute shader
    CullLights(camera);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.cullingTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    // Update statistics
    m_stats.totalLights = static_cast<uint32_t>(m_lights.size());
    m_stats.totalClusters = m_gridDim.x * m_gridDim.y * m_gridDim.z;
}

void ClusteredLightManager::BuildClusterGrid(const Camera& camera) {
    const glm::mat4& viewMatrix = camera.GetView();
    const glm::mat4& projMatrix = camera.GetProjection();

    // Calculate cluster dimensions in screen space
    float clusterWidthScreen = static_cast<float>(m_viewportWidth) / m_gridDim.x;
    float clusterHeightScreen = static_cast<float>(m_viewportHeight) / m_gridDim.y;

    // Calculate depth slice scale and bias for exponential depth distribution
    float depthSliceScale = static_cast<float>(m_gridDim.z) / std::log2(m_farPlane / m_nearPlane);
    float depthSliceBias = -(static_cast<float>(m_gridDim.z) * std::log2(m_nearPlane) /
                            std::log2(m_farPlane / m_nearPlane));

    // Build clusters (we'll do actual culling in compute shader)
    uint32_t clusterIndex = 0;
    for (int z = 0; z < m_gridDim.z; ++z) {
        for (int y = 0; y < m_gridDim.y; ++y) {
            for (int x = 0; x < m_gridDim.x; ++x) {
                CalculateClusterAABB(x, y, z, m_clusters[clusterIndex].minAABB,
                                    m_clusters[clusterIndex].maxAABB);
                clusterIndex++;
            }
        }
    }
}

void ClusteredLightManager::CalculateClusterAABB(int x, int y, int z,
                                                 glm::vec3& minAABB, glm::vec3& maxAABB) {
    // Calculate cluster bounds in screen space
    float minX = (static_cast<float>(x) / m_gridDim.x) * 2.0f - 1.0f;
    float maxX = (static_cast<float>(x + 1) / m_gridDim.x) * 2.0f - 1.0f;
    float minY = (static_cast<float>(y) / m_gridDim.y) * 2.0f - 1.0f;
    float maxY = (static_cast<float>(y + 1) / m_gridDim.y) * 2.0f - 1.0f;

    // Calculate depth using exponential distribution
    float depthRatio = static_cast<float>(z) / m_gridDim.z;
    float minZ = m_nearPlane * std::pow(m_farPlane / m_nearPlane, depthRatio);
    float maxZ = m_nearPlane * std::pow(m_farPlane / m_nearPlane, (z + 1.0f) / m_gridDim.z);

    minAABB = glm::vec3(minX, minY, -maxZ); // Negative because view space looks down -Z
    maxAABB = glm::vec3(maxX, maxY, -minZ);
}

void ClusteredLightManager::UploadLights() {
    if (m_lights.empty()) {
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_lights.size() * sizeof(GPULight),
                   m_lights.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ClusteredLightManager::CullLights(const Camera& camera) {
    if (!m_cullingShader || m_lights.empty()) {
        return;
    }

    // Reset atomic counter
    uint32_t zero = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_atomicCounterBuffer);
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &zero);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_clusterSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_lightSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_lightIndexSSBO);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, m_atomicCounterBuffer);

    // Set uniforms
    m_cullingShader->Bind();
    m_cullingShader->SetMat4("u_viewMatrix", camera.GetView());
    m_cullingShader->SetMat4("u_projectionMatrix", camera.GetProjection());
    m_cullingShader->SetVec3("u_cameraPos", camera.GetPosition());
    m_cullingShader->SetInt("u_numLights", static_cast<int>(m_lights.size()));
    m_cullingShader->SetIvec3("u_gridDim", m_gridDim);
    m_cullingShader->SetVec2("u_screenDim", glm::vec2(m_viewportWidth, m_viewportHeight));
    m_cullingShader->SetFloat("u_nearPlane", m_nearPlane);
    m_cullingShader->SetFloat("u_farPlane", m_farPlane);

    // Dispatch compute shader
    glDispatchCompute(m_gridDim.x, m_gridDim.y, m_gridDim.z);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Unbind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, 0);
}

void ClusteredLightManager::BindForRendering(uint32_t clusterBinding, uint32_t lightBinding,
                                             uint32_t lightIndexBinding) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, clusterBinding, m_clusterSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, lightBinding, m_lightSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, lightIndexBinding, m_lightIndexSSBO);
}

void ClusteredLightManager::SetShaderUniforms(Shader& shader) {
    shader.SetIvec3("u_gridDim", m_gridDim);
    shader.SetVec2("u_screenDim", glm::vec2(m_viewportWidth, m_viewportHeight));
    shader.SetFloat("u_nearPlane", m_nearPlane);
    shader.SetFloat("u_farPlane", m_farPlane);
    shader.SetInt("u_numLights", static_cast<int>(m_lights.size()));
}

// =============================================================================
// Configuration
// =============================================================================

void ClusteredLightManager::SetDepthRange(float nearPlane, float farPlane) {
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

// =============================================================================
// Utility Functions
// =============================================================================

void ClusteredLightManager::TransformAABB(const glm::mat4& transform, const glm::vec3& minAABB,
                                         const glm::vec3& maxAABB, glm::vec3& outMin,
                                         glm::vec3& outMax) {
    // Transform all 8 corners and find new min/max
    glm::vec3 corners[8] = {
        glm::vec3(minAABB.x, minAABB.y, minAABB.z),
        glm::vec3(maxAABB.x, minAABB.y, minAABB.z),
        glm::vec3(minAABB.x, maxAABB.y, minAABB.z),
        glm::vec3(maxAABB.x, maxAABB.y, minAABB.z),
        glm::vec3(minAABB.x, minAABB.y, maxAABB.z),
        glm::vec3(maxAABB.x, minAABB.y, maxAABB.z),
        glm::vec3(minAABB.x, maxAABB.y, maxAABB.z),
        glm::vec3(maxAABB.x, maxAABB.y, maxAABB.z)
    };

    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& corner : corners) {
        glm::vec3 transformed = glm::vec3(transform * glm::vec4(corner, 1.0f));
        outMin = glm::min(outMin, transformed);
        outMax = glm::max(outMax, transformed);
    }
}

bool ClusteredLightManager::SphereAABBIntersect(const glm::vec3& center, float radius,
                                               const glm::vec3& aabbMin, const glm::vec3& aabbMax) {
    float sqDist = 0.0f;

    for (int i = 0; i < 3; ++i) {
        float v = center[i];
        if (v < aabbMin[i]) sqDist += (aabbMin[i] - v) * (aabbMin[i] - v);
        if (v > aabbMax[i]) sqDist += (v - aabbMax[i]) * (v - aabbMax[i]);
    }

    return sqDist <= radius * radius;
}

} // namespace Nova
