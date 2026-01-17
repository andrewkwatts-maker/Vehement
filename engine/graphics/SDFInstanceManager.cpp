#include "SDFInstanceManager.hpp"
#include "SDFGPUEvaluator.hpp"
#include "SDFBrickCache.hpp"
#include <glad/gl.h>
#include <chrono>
#include <algorithm>

namespace Nova {

SDFInstanceManager::SDFInstanceManager() {}

SDFInstanceManager::~SDFInstanceManager() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool SDFInstanceManager::Initialize(SDFGPUEvaluator* evaluator, SDFBrickCache* brickCache) {
    if (m_initialized) {
        return true;
    }

    if (!evaluator) {
        return false;
    }

    m_evaluator = evaluator;
    m_brickCache = brickCache;

    // Create GPU buffers
    glGenBuffers(1, &m_instanceSSBO);
    glGenBuffers(1, &m_visibleInstanceSSBO);
    glGenBuffers(1, &m_indirectDrawBuffer);

    if (m_instanceSSBO == 0 || m_visibleInstanceSSBO == 0 || m_indirectDrawBuffer == 0) {
        Shutdown();
        return false;
    }

    // Initialize with empty buffers
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_visibleInstanceSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectDrawBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    m_initialized = true;
    return true;
}

void SDFInstanceManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_instanceSSBO != 0) {
        glDeleteBuffers(1, &m_instanceSSBO);
        m_instanceSSBO = 0;
    }

    if (m_visibleInstanceSSBO != 0) {
        glDeleteBuffers(1, &m_visibleInstanceSSBO);
        m_visibleInstanceSSBO = 0;
    }

    if (m_indirectDrawBuffer != 0) {
        glDeleteBuffers(1, &m_indirectDrawBuffer);
        m_indirectDrawBuffer = 0;
    }

    m_instances.clear();
    m_freeHandles.clear();
    m_handleToIndex.clear();

    m_cullingShader.reset();
    m_initialized = false;
}

// =============================================================================
// Instance Management
// =============================================================================

uint32_t SDFInstanceManager::CreateInstance(
    const glm::mat4& transform,
    uint32_t programOffset,
    uint32_t programLength,
    float boundingRadius)
{
    // Allocate handle
    uint32_t handle;
    if (!m_freeHandles.empty()) {
        handle = m_freeHandles.back();
        m_freeHandles.pop_back();
    } else {
        handle = m_nextHandle++;
    }

    // Create instance data
    SDFInstance instance;
    instance.transform = transform;
    instance.programOffset = programOffset;
    instance.programLength = programLength;
    instance.flags = SDF_INSTANCE_VISIBLE | SDF_INSTANCE_CAST_SHADOW;

    // Calculate bounding sphere center from transform
    instance.boundsCenter = glm::vec4(
        transform[3][0],  // Translation x
        transform[3][1],  // Translation y
        transform[3][2],  // Translation z
        boundingRadius
    );

    // Add to instance list
    size_t index = m_instances.size();
    m_instances.push_back(instance);
    m_handleToIndex[handle] = index;

    m_instancesDirty = true;
    return handle;
}

void SDFInstanceManager::RemoveInstance(uint32_t handle) {
    auto it = m_handleToIndex.find(handle);
    if (it == m_handleToIndex.end()) {
        return;
    }

    size_t index = it->second;

    // Swap with last element
    if (index < m_instances.size() - 1) {
        m_instances[index] = m_instances.back();

        // Update handle mapping for swapped element
        for (auto& pair : m_handleToIndex) {
            if (pair.second == m_instances.size() - 1) {
                pair.second = index;
                break;
            }
        }
    }

    m_instances.pop_back();
    m_handleToIndex.erase(it);
    m_freeHandles.push_back(handle);

    m_instancesDirty = true;
}

void SDFInstanceManager::UpdateInstanceTransform(uint32_t handle, const glm::mat4& transform) {
    SDFInstance* instance = GetInstance(handle);
    if (!instance) {
        return;
    }

    instance->transform = transform;
    instance->boundsCenter.x = transform[3][0];
    instance->boundsCenter.y = transform[3][1];
    instance->boundsCenter.z = transform[3][2];

    m_instancesDirty = true;
}

void SDFInstanceManager::SetInstanceVisible(uint32_t handle, bool visible) {
    SDFInstance* instance = GetInstance(handle);
    if (!instance) {
        return;
    }

    if (visible) {
        instance->flags |= SDF_INSTANCE_VISIBLE;
    } else {
        instance->flags &= ~SDF_INSTANCE_VISIBLE;
    }

    m_instancesDirty = true;
}

void SDFInstanceManager::SetInstanceFlags(uint32_t handle, uint32_t flags) {
    SDFInstance* instance = GetInstance(handle);
    if (!instance) {
        return;
    }

    instance->flags = flags;
    m_instancesDirty = true;
}

SDFInstance* SDFInstanceManager::GetInstance(uint32_t handle) {
    auto it = m_handleToIndex.find(handle);
    if (it == m_handleToIndex.end()) {
        return nullptr;
    }
    return &m_instances[it->second];
}

const SDFInstance* SDFInstanceManager::GetInstance(uint32_t handle) const {
    auto it = m_handleToIndex.find(handle);
    if (it == m_handleToIndex.end()) {
        return nullptr;
    }
    return &m_instances[it->second];
}

void SDFInstanceManager::ClearInstances() {
    m_instances.clear();
    m_freeHandles.clear();
    m_handleToIndex.clear();
    m_nextHandle = 1;
    m_instancesDirty = true;
}

// =============================================================================
// Rendering
// =============================================================================

void SDFInstanceManager::UpdateCullingAndLOD(
    const glm::mat4& viewMatrix,
    const glm::mat4& projMatrix,
    const glm::vec3& cameraPos)
{
    if (m_instances.empty()) {
        m_visibleInstanceCount = 0;
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Reset statistics
    m_stats.totalInstances = static_cast<uint32_t>(m_instances.size());
    m_stats.visibleInstances = 0;
    m_stats.culledByFrustum = 0;
    m_stats.culledByOcclusion = 0;
    m_stats.lod0Count = 0;
    m_stats.lod1Count = 0;
    m_stats.lod2Count = 0;
    m_stats.lod3Count = 0;
    m_stats.cachedInstances = 0;

    // Update LOD levels and perform culling
    glm::mat4 viewProj = projMatrix * viewMatrix;

    for (auto& instance : m_instances) {
        // Skip invisible instances
        if (!(instance.flags & SDF_INSTANCE_VISIBLE)) {
            continue;
        }

        // Frustum culling
        glm::vec3 center(instance.boundsCenter.x, instance.boundsCenter.y, instance.boundsCenter.z);
        float radius = instance.boundsCenter.w;

        if (!FrustumCullSphere(center, radius, viewProj)) {
            m_stats.culledByFrustum++;
            continue;
        }

        // LOD selection
        if (m_lodParams.enableLOD) {
            instance.lodLevel = CalculateLOD(center, cameraPos, radius);
        } else {
            instance.lodLevel = 0;
        }

        // Update LOD statistics
        switch (instance.lodLevel) {
            case 0: m_stats.lod0Count++; break;
            case 1: m_stats.lod1Count++; break;
            case 2: m_stats.lod2Count++; break;
            case 3: m_stats.lod3Count++; break;
        }

        // Check cache
        if (instance.flags & SDF_INSTANCE_USE_CACHE) {
            m_stats.cachedInstances++;
        }

        m_stats.visibleInstances++;
    }

    // Upload instance data if dirty
    if (m_instancesDirty) {
        UploadInstances();
        m_instancesDirty = false;
    }

    m_visibleInstanceCount = m_stats.visibleInstances;

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.cullingTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void SDFInstanceManager::BindInstanceBuffer(uint32_t binding) {
    if (!m_initialized) {
        return;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_instanceSSBO);
}

// =============================================================================
// Private Helper Functions
// =============================================================================

void SDFInstanceManager::UploadInstances() {
    if (m_instances.empty()) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    size_t bufferSize = m_instances.size() * sizeof(SDFInstance);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, m_instances.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

bool SDFInstanceManager::FrustumCullSphere(
    const glm::vec3& center,
    float radius,
    const glm::mat4& viewProj)
{
    // Extract frustum planes from view-projection matrix
    // Plane format: ax + by + cz + d = 0

    glm::vec4 planes[6];

    // Left plane
    planes[0] = glm::vec4(
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]
    );

    // Right plane
    planes[1] = glm::vec4(
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]
    );

    // Bottom plane
    planes[2] = glm::vec4(
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]
    );

    // Top plane
    planes[3] = glm::vec4(
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]
    );

    // Near plane
    planes[4] = glm::vec4(
        viewProj[0][3] + viewProj[0][2],
        viewProj[1][3] + viewProj[1][2],
        viewProj[2][3] + viewProj[2][2],
        viewProj[3][3] + viewProj[3][2]
    );

    // Far plane
    planes[5] = glm::vec4(
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]
    );

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }

    // Test sphere against all planes
    for (int i = 0; i < 6; ++i) {
        float distance = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (distance < -radius) {
            return false;  // Outside frustum
        }
    }

    return true;  // Inside or intersecting frustum
}

uint16_t SDFInstanceManager::CalculateLOD(
    const glm::vec3& instancePos,
    const glm::vec3& cameraPos,
    float boundingRadius)
{
    float distance = glm::length(instancePos - cameraPos);
    distance -= boundingRadius;  // Account for object size
    distance = std::max(distance, 0.0f);

    // Apply LOD bias
    distance *= (1.0f + m_lodParams.lodBias);

    // Select LOD level based on distance
    for (int i = 0; i < 4; ++i) {
        if (distance < m_lodParams.lodDistances[i]) {
            return static_cast<uint16_t>(i);
        }
    }

    return 3;  // Maximum LOD level
}

} // namespace Nova
