#include "ClusteredLightingExpanded.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace Engine {
namespace Graphics {

// ============================================================================
// Light Implementation
// ============================================================================

GPULight Light::ToGPULight() const {
    GPULight gpu;

    gpu.position = Vector4(position.x, position.y, position.z, range);
    gpu.direction = Vector4(direction.x, direction.y, direction.z, spotAngle);
    gpu.color = Vector4(color.x * intensity, color.y * intensity, color.z * intensity, intensity);
    gpu.attenuation = Vector4(constantAttenuation, linearAttenuation, quadraticAttenuation,
                             static_cast<float>(static_cast<uint32_t>(type)));

    if (type == LightType::Area) {
        gpu.extra = Vector4(areaSize.x, areaSize.y, areaSize.z, innerSpotAngle);
    } else if (type == LightType::EmissiveMesh) {
        gpu.extra = Vector4(static_cast<float>(meshID), 0, 0, 0);
    } else {
        gpu.extra = Vector4(innerSpotAngle, castsShadows ? 1.0f : 0.0f,
                           static_cast<float>(shadowMapIndex), 0);
    }

    return gpu;
}

// ============================================================================
// ClusterAABB Implementation
// ============================================================================

bool ClusterAABB::Intersects(const Light& light) const {
    switch (light.type) {
        case LightType::Point: {
            // Sphere-AABB intersection
            Vector3 closest;
            closest.x = std::max(min.x, std::min(light.position.x, max.x));
            closest.y = std::max(min.y, std::min(light.position.y, max.y));
            closest.z = std::max(min.z, std::min(light.position.z, max.z));

            Vector3 diff = closest - light.position;
            float distSquared = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            return distSquared <= (light.range * light.range);
        }

        case LightType::Spot: {
            // Simplified: treat as sphere first
            Vector3 closest;
            closest.x = std::max(min.x, std::min(light.position.x, max.x));
            closest.y = std::max(min.y, std::min(light.position.y, max.y));
            closest.z = std::max(min.z, std::min(light.position.z, max.z));

            Vector3 diff = closest - light.position;
            float distSquared = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

            if (distSquared > light.range * light.range) {
                return false;
            }

            // TODO: More accurate cone-AABB test
            return true;
        }

        case LightType::Directional:
            // Directional lights affect all clusters
            return true;

        case LightType::Area: {
            // Simplified: treat as point light
            Vector3 closest;
            closest.x = std::max(min.x, std::min(light.position.x, max.x));
            closest.y = std::max(min.y, std::min(light.position.y, max.y));
            closest.z = std::max(min.z, std::min(light.position.z, max.z));

            Vector3 diff = closest - light.position;
            float distSquared = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            return distSquared <= (light.range * light.range);
        }

        default:
            return true;
    }
}

// ============================================================================
// ShadowMapAtlas Implementation
// ============================================================================

ShadowMapAtlas::ShadowMapAtlas(uint32_t size)
    : m_texture(0)
    , m_fbo(0)
    , m_size(size)
    , m_slotSize(0)
    , m_maxSlots(0) {

    // Calculate slot layout (16x16 grid for 256 slots)
    m_slotSize = size / 16;
    m_maxSlots = 256;

    // Create depth texture array
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, size, size);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // Create framebuffer
    glGenFramebuffers(1, &m_fbo);

    m_allocatedSlots.resize(m_maxSlots, false);
}

ShadowMapAtlas::~ShadowMapAtlas() {
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
    }
}

int32_t ShadowMapAtlas::AllocateSlot() {
    for (size_t i = 0; i < m_allocatedSlots.size(); i++) {
        if (!m_allocatedSlots[i]) {
            m_allocatedSlots[i] = true;
            return static_cast<int32_t>(i);
        }
    }
    return -1; // Full
}

void ShadowMapAtlas::FreeSlot(uint32_t index) {
    if (index < m_allocatedSlots.size()) {
        m_allocatedSlots[index] = false;
    }
}

ShadowMapAtlas::SlotViewport ShadowMapAtlas::GetSlotViewport(uint32_t index) const {
    SlotViewport vp;
    uint32_t row = index / 16;
    uint32_t col = index % 16;
    vp.x = col * m_slotSize;
    vp.y = row * m_slotSize;
    vp.width = m_slotSize;
    vp.height = m_slotSize;
    return vp;
}

// ============================================================================
// ClusteredLightingExpanded Implementation
// ============================================================================

ClusteredLightingExpanded::ClusteredLightingExpanded(const Config& config)
    : m_config(config)
    , m_overflowCounter(0)
    , m_queryObject(0)
    , m_nearPlane(0.1f)
    , m_farPlane(1000.0f)
    , m_fov(60.0f) {

    m_lights.reserve(config.maxLights);
    m_gpuLights.reserve(config.maxLights);

    // Pre-allocate cluster data
    uint32_t clusterCount = GetClusterCount();
    m_clusters.resize(clusterCount);
    m_overflowPool.resize(config.overflowPoolSize);
}

ClusteredLightingExpanded::~ClusteredLightingExpanded() {
    if (m_queryObject) {
        glDeleteQueries(1, &m_queryObject);
    }
}

bool ClusteredLightingExpanded::Initialize() {
    if (!CreateBuffers()) {
        return false;
    }

    if (!LoadShaders()) {
        return false;
    }

    // Create shadow atlas if enabled
    if (m_config.enableShadows) {
        m_shadowAtlas = std::make_unique<ShadowMapAtlas>(m_config.shadowAtlasSize);
    }

    // Create GPU query
    glGenQueries(1, &m_queryObject);

    ResetClusters();

    return true;
}

bool ClusteredLightingExpanded::CreateBuffers() {
    // Light buffer
    m_lightBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_lightBuffer->Allocate(m_config.maxLights * sizeof(GPULight));

    // Cluster buffer
    m_clusterBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_clusterBuffer->Allocate(GetClusterCount() * sizeof(LightCluster));

    // Overflow buffer
    m_overflowBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_overflowBuffer->Allocate(m_config.overflowPoolSize * sizeof(LightOverflowNode));

    // Cluster bounds buffer (for GPU assignment)
    m_clusterBoundsBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_clusterBoundsBuffer->Allocate(GetClusterCount() * sizeof(ClusterAABB));

    return true;
}

bool ClusteredLightingExpanded::LoadShaders() {
    // Load cluster assignment shader
    m_clusterAssignShader = std::make_unique<ComputeShader>();
    if (!m_clusterAssignShader->LoadFromFile("assets/shaders/light_cluster_assign.comp")) {
        // Fallback to embedded shader
        const char* fallbackSource = R"(
            #version 450 core
            layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

            struct Light {
                vec4 position;
                vec4 direction;
                vec4 color;
                vec4 attenuation;
                vec4 extra;
            };

            struct Cluster {
                uint lightCount;
                uint lightIndices[256];
                uint overflowHead;
                uint padding;
            };

            layout(std430, binding = 0) readonly buffer LightBuffer {
                Light lights[];
            };

            layout(std430, binding = 1) buffer ClusterBuffer {
                Cluster clusters[];
            };

            uniform uint u_lightCount;
            uniform uint u_clusterGridX;
            uniform uint u_clusterGridY;
            uniform uint u_clusterGridZ;

            void main() {
                uvec3 clusterID = gl_WorkGroupID.xyz;
                if (clusterID.x >= u_clusterGridX || clusterID.y >= u_clusterGridY || clusterID.z >= u_clusterGridZ)
                    return;

                uint clusterIndex = clusterID.z * (u_clusterGridX * u_clusterGridY) +
                                   clusterID.y * u_clusterGridX + clusterID.x;

                // Reset cluster
                clusters[clusterIndex].lightCount = 0;
                clusters[clusterIndex].overflowHead = 0;

                // Test all lights (simplified)
                for (uint i = 0; i < u_lightCount; i++) {
                    // Simple distance test
                    uint slot = clusters[clusterIndex].lightCount;
                    if (slot < 256) {
                        clusters[clusterIndex].lightIndices[slot] = i;
                        clusters[clusterIndex].lightCount++;
                    }
                }
            }
        )";
        m_clusterAssignShader->LoadFromSource(fallbackSource);
    }

    // Load cluster reset shader
    m_clusterResetShader = std::make_unique<ComputeShader>();

    return true;
}

int32_t ClusteredLightingExpanded::AddLight(const Light& light) {
    uint32_t index;

    // Reuse freed indices first
    if (!m_freeLightIndices.empty()) {
        index = m_freeLightIndices.back();
        m_freeLightIndices.pop_back();

        if (index < m_lights.size()) {
            m_lights[index] = light;
            m_gpuLights[index] = light.ToGPULight();
        }
    } else {
        if (m_lights.size() >= m_config.maxLights) {
            return -1; // Full
        }

        index = static_cast<uint32_t>(m_lights.size());
        m_lights.push_back(light);
        m_gpuLights.push_back(light.ToGPULight());
    }

    m_stats.totalLights = static_cast<uint32_t>(m_lights.size());
    m_stats.activeLights++;

    return static_cast<int32_t>(index);
}

void ClusteredLightingExpanded::UpdateLight(uint32_t index, const Light& light) {
    if (index >= m_lights.size()) {
        return;
    }

    m_lights[index] = light;
    m_gpuLights[index] = light.ToGPULight();
}

void ClusteredLightingExpanded::RemoveLight(uint32_t index) {
    if (index >= m_lights.size()) {
        return;
    }

    // Mark as inactive
    m_freeLightIndices.push_back(index);
    m_stats.activeLights--;
}

void ClusteredLightingExpanded::ClearLights() {
    m_lights.clear();
    m_gpuLights.clear();
    m_freeLightIndices.clear();
    m_stats.totalLights = 0;
    m_stats.activeLights = 0;
}

void ClusteredLightingExpanded::ResetClusters() {
    for (auto& cluster : m_clusters) {
        cluster.lightCount = 0;
        cluster.overflowHead = 0;
    }
    m_overflowCounter = 0;
}

void ClusteredLightingExpanded::UpdateClusters(const Matrix4& viewMatrix, const Matrix4& projMatrix) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Upload light data to GPU
    if (!m_gpuLights.empty()) {
        m_lightBuffer->Upload(m_gpuLights.data(), m_gpuLights.size() * sizeof(GPULight));
    }

    auto uploadEndTime = std::chrono::high_resolution_clock::now();
    m_stats.lightUploadTimeMs = std::chrono::duration<float, std::milli>(uploadEndTime - startTime).count();

    // Run GPU cluster assignment
    if (m_clusterAssignShader) {
        m_lightBuffer->BindBase(0);
        m_clusterBuffer->BindBase(1);
        m_overflowBuffer->BindBase(2);

        m_clusterAssignShader->SetUniform("u_lightCount", static_cast<int>(m_gpuLights.size()));
        m_clusterAssignShader->SetUniform("u_clusterGridX", static_cast<int>(m_config.clusterGridX));
        m_clusterAssignShader->SetUniform("u_clusterGridY", static_cast<int>(m_config.clusterGridY));
        m_clusterAssignShader->SetUniform("u_clusterGridZ", static_cast<int>(m_config.clusterGridZ));

        glBeginQuery(GL_TIME_ELAPSED, m_queryObject);

        m_clusterAssignShader->Dispatch(
            m_config.clusterGridX,
            m_config.clusterGridY,
            m_config.clusterGridZ
        );

        glEndQuery(GL_TIME_ELAPSED);

        // Read query result
        GLint available = 0;
        glGetQueryObjectiv(m_queryObject, GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            GLuint64 timeElapsed = 0;
            glGetQueryObjectui64v(m_queryObject, GL_QUERY_RESULT, &timeElapsed);
            m_stats.clusterUpdateTimeMs = timeElapsed / 1000000.0f;
        }
    } else {
        // CPU fallback
        UpdateClustersCPU(viewMatrix, projMatrix);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    float totalTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void ClusteredLightingExpanded::UpdateClustersCPU(const Matrix4& viewMatrix, const Matrix4& projMatrix) {
    ResetClusters();

    // For each cluster
    for (uint32_t z = 0; z < m_config.clusterGridZ; z++) {
        for (uint32_t y = 0; y < m_config.clusterGridY; y++) {
            for (uint32_t x = 0; x < m_config.clusterGridX; x++) {
                uint32_t clusterIndex = z * (m_config.clusterGridX * m_config.clusterGridY) +
                                       y * m_config.clusterGridX + x;

                ClusterAABB bounds = CalculateClusterBounds(x, y, z, viewMatrix, projMatrix);
                LightCluster& cluster = m_clusters[clusterIndex];

                // Test all lights
                for (size_t i = 0; i < m_lights.size(); i++) {
                    if (bounds.Intersects(m_lights[i])) {
                        if (cluster.lightCount < m_config.maxLightsPerCluster) {
                            cluster.lightIndices[cluster.lightCount] = static_cast<uint32_t>(i);
                            cluster.lightCount++;
                        } else {
                            // Overflow
                            if (m_overflowCounter < m_config.overflowPoolSize) {
                                uint32_t nodeIndex = m_overflowCounter++;
                                m_overflowPool[nodeIndex].lightIndex = static_cast<uint32_t>(i);
                                m_overflowPool[nodeIndex].next = cluster.overflowHead;
                                cluster.overflowHead = nodeIndex;
                                m_stats.clustersWithOverflow++;
                            }
                        }

                        m_stats.maxLightsInCluster = std::max(m_stats.maxLightsInCluster, cluster.lightCount);
                    }
                }
            }
        }
    }

    // Upload to GPU
    m_clusterBuffer->Upload(m_clusters.data(), m_clusters.size() * sizeof(LightCluster));
    if (m_overflowCounter > 0) {
        m_overflowBuffer->Upload(m_overflowPool.data(), m_overflowCounter * sizeof(LightOverflowNode));
    }
}

ClusterAABB ClusteredLightingExpanded::CalculateClusterBounds(
    uint32_t x, uint32_t y, uint32_t z,
    const Matrix4& viewMatrix,
    const Matrix4& projMatrix) const {

    ClusterAABB bounds;

    // Calculate cluster position in grid space [0,1]
    float xMin = static_cast<float>(x) / m_config.clusterGridX;
    float xMax = static_cast<float>(x + 1) / m_config.clusterGridX;
    float yMin = static_cast<float>(y) / m_config.clusterGridY;
    float yMax = static_cast<float>(y + 1) / m_config.clusterGridY;

    // Exponential depth slicing
    float zNear = m_nearPlane;
    float zFar = m_farPlane;
    float zMin = zNear * std::pow(zFar / zNear, static_cast<float>(z) / m_config.clusterGridZ);
    float zMax = zNear * std::pow(zFar / zNear, static_cast<float>(z + 1) / m_config.clusterGridZ);

    // Reconstruct world-space bounds (simplified)
    bounds.min = Vector3(xMin * 100.0f - 50.0f, yMin * 100.0f - 50.0f, -zMax);
    bounds.max = Vector3(xMax * 100.0f - 50.0f, yMax * 100.0f - 50.0f, -zMin);

    return bounds;
}

uint32_t ClusteredLightingExpanded::GetClusterIndex(const Vector3& worldPos) const {
    // Simplified: would need proper view-space transformation
    uint32_t x = static_cast<uint32_t>(std::max(0.0f, std::min(
        static_cast<float>(m_config.clusterGridX - 1),
        (worldPos.x + 50.0f) / 100.0f * m_config.clusterGridX
    )));

    uint32_t y = static_cast<uint32_t>(std::max(0.0f, std::min(
        static_cast<float>(m_config.clusterGridY - 1),
        (worldPos.y + 50.0f) / 100.0f * m_config.clusterGridY
    )));

    uint32_t z = static_cast<uint32_t>(std::max(0.0f, std::min(
        static_cast<float>(m_config.clusterGridZ - 1),
        worldPos.z / m_farPlane * m_config.clusterGridZ
    )));

    return z * (m_config.clusterGridX * m_config.clusterGridY) +
           y * m_config.clusterGridX + x;
}

ClusterAABB ClusteredLightingExpanded::GetClusterAABB(uint32_t clusterIndex) const {
    uint32_t z = clusterIndex / (m_config.clusterGridX * m_config.clusterGridY);
    uint32_t rem = clusterIndex % (m_config.clusterGridX * m_config.clusterGridY);
    uint32_t y = rem / m_config.clusterGridX;
    uint32_t x = rem % m_config.clusterGridX;

    return GetClusterAABBFromGrid(x, y, z);
}

ClusterAABB ClusteredLightingExpanded::GetClusterAABBFromGrid(uint32_t x, uint32_t y, uint32_t z) const {
    // Would normally use proper view-proj transform
    Matrix4 identity;
    return CalculateClusterBounds(x, y, z, identity, identity);
}

void ClusteredLightingExpanded::BindLightingBuffers() {
    m_lightBuffer->BindBase(0);
    m_clusterBuffer->BindBase(1);
    m_overflowBuffer->BindBase(2);
}

void ClusteredLightingExpanded::ResetStats() {
    m_stats = Stats();
}

// ============================================================================
// LightImportanceSampler Implementation
// ============================================================================

float LightImportanceSampler::CalculateImportance(const Light& light, const Vector3& viewPos) {
    Vector3 toLight = light.position - viewPos;
    float distance = toLight.Length();

    // Importance based on intensity and distance
    float importance = light.intensity / (1.0f + distance * distance * 0.01f);

    // Boost importance for shadow casters
    if (light.castsShadows) {
        importance *= 2.0f;
    }

    // Boost for directional lights
    if (light.type == LightType::Directional) {
        importance *= 3.0f;
    }

    return importance;
}

void LightImportanceSampler::SortByImportance(std::vector<Light>& lights, const Vector3& viewPos) {
    std::sort(lights.begin(), lights.end(), [&viewPos](const Light& a, const Light& b) {
        return CalculateImportance(a, viewPos) > CalculateImportance(b, viewPos);
    });
}

std::vector<uint32_t> LightImportanceSampler::SelectTopLights(
    const std::vector<Light>& lights,
    const Vector3& viewPos,
    uint32_t count) {

    std::vector<std::pair<float, uint32_t>> importanceList;
    importanceList.reserve(lights.size());

    for (size_t i = 0; i < lights.size(); i++) {
        float importance = CalculateImportance(lights[i], viewPos);
        importanceList.push_back({importance, static_cast<uint32_t>(i)});
    }

    std::sort(importanceList.begin(), importanceList.end(),
             [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<uint32_t> result;
    result.reserve(std::min(count, static_cast<uint32_t>(importanceList.size())));

    for (size_t i = 0; i < std::min(static_cast<size_t>(count), importanceList.size()); i++) {
        result.push_back(importanceList[i].second);
    }

    return result;
}

} // namespace Graphics
} // namespace Engine
