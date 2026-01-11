#include "SDFRendererAccelerated.hpp"
#include "../sdf/SDFModel.hpp"
#include "../scene/Camera.hpp"
#include <glad/glad.h>
#include <chrono>

namespace Nova {

SDFRendererAccelerated::SDFRendererAccelerated()
    : m_bvh(std::make_unique<SDFAccelerationStructure>()),
      m_octree(std::make_unique<SDFSparseVoxelOctree>()),
      m_brickMap(std::make_unique<SDFBrickMap>()) {
}

SDFRendererAccelerated::~SDFRendererAccelerated() {
    ShutdownAcceleration();
}

bool SDFRendererAccelerated::InitializeAcceleration() {
    if (!Initialize()) {
        return false;
    }

    m_accelerationEnabled = true;
    return true;
}

void SDFRendererAccelerated::ShutdownAcceleration() {
    m_bvh.reset();
    m_octree.reset();
    m_brickMap.reset();
    m_accelerationEnabled = false;
    m_accelerationBuilt = false;
}

void SDFRendererAccelerated::BuildAcceleration(const std::vector<const SDFModel*>& models,
                                                const std::vector<glm::mat4>& transforms) {
    if (!m_accelerationEnabled || models.empty()) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Build BVH for frustum culling
    if (m_accelSettings.useBVH && models.size() >= m_accelSettings.maxInstancesBeforeAcceleration) {
        BVHBuildSettings bvhSettings;
        bvhSettings.strategy = m_accelSettings.bvhStrategy;
        bvhSettings.parallelBuild = true;
        bvhSettings.maxPrimitivesPerLeaf = 4;

        m_bvh->Build(models, transforms, bvhSettings);
    }

    // Build octree for first model (or combined scene)
    if (m_accelSettings.useOctree && !models.empty() && models[0]) {
        VoxelizationSettings octreeSettings;
        octreeSettings.maxDepth = m_accelSettings.octreeDepth;
        octreeSettings.voxelSize = m_accelSettings.octreeVoxelSize;
        octreeSettings.adaptiveDepth = true;
        octreeSettings.storeDistances = true;

        m_octree->Voxelize(*models[0], octreeSettings);
    }

    // Build brick map for static caching
    if (m_accelSettings.useBrickMap && m_accelSettings.enableDistanceCache && !models.empty() && models[0]) {
        BrickMapSettings brickSettings;
        brickSettings.brickResolution = m_accelSettings.brickResolution;
        brickSettings.worldVoxelSize = m_accelSettings.brickVoxelSize;
        brickSettings.enableCompression = true;

        m_brickMap->Build(*models[0], brickSettings);
    }

    m_accelerationBuilt = true;

    auto endTime = std::chrono::high_resolution_clock::now();
    double buildTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Update stats
    m_stats.bvhMemoryBytes = m_bvh->GetMemoryUsage();
    m_stats.octreeMemoryBytes = m_octree->GetMemoryUsage();
    m_stats.brickMapMemoryBytes = m_brickMap->GetMemoryUsage();
}

void SDFRendererAccelerated::UpdateAcceleration(const std::vector<int>& changedIndices,
                                                 const std::vector<glm::mat4>& newTransforms) {
    if (!m_accelerationEnabled || !m_accelerationBuilt) {
        return;
    }

    if (m_accelSettings.useBVH) {
        m_bvh->UpdateDynamic(changedIndices, newTransforms);
    }
}

void SDFRendererAccelerated::RefitAcceleration() {
    if (!m_accelerationEnabled || !m_accelerationBuilt) {
        return;
    }

    if (m_accelSettings.useBVH && m_accelSettings.refitBVH) {
        m_bvh->Refit();
    }
}

void SDFRendererAccelerated::RenderBatchAccelerated(const std::vector<const SDFModel*>& models,
                                                     const std::vector<glm::mat4>& transforms,
                                                     const Camera& camera) {
    if (!IsInitialized() || models.empty()) {
        return;
    }

    auto frameStart = std::chrono::high_resolution_clock::now();

    // Build or update acceleration if needed
    if (!m_accelerationBuilt || m_accelSettings.rebuildAccelerationEachFrame) {
        BuildAcceleration(models, transforms);
    }

    m_stats.totalInstances = static_cast<int>(models.size());

    // Perform frustum culling using BVH
    std::vector<int> visibleIndices;
    if (m_accelSettings.useBVH && m_accelSettings.enableFrustumCulling && m_bvh->IsBuilt()) {
        auto cullStart = std::chrono::high_resolution_clock::now();

        visibleIndices = PerformFrustumCulling(camera);

        auto cullEnd = std::chrono::high_resolution_clock::now();
        m_stats.bvhTraversalTimeMs = std::chrono::duration<double, std::milli>(cullEnd - cullStart).count();

        m_stats.culledInstances = static_cast<int>(models.size()) - static_cast<int>(visibleIndices.size());
        m_stats.renderedInstances = static_cast<int>(visibleIndices.size());
    } else {
        // No culling - render all
        visibleIndices.resize(models.size());
        for (size_t i = 0; i < models.size(); ++i) {
            visibleIndices[i] = static_cast<int>(i);
        }
        m_stats.renderedInstances = static_cast<int>(models.size());
        m_stats.culledInstances = 0;
    }

    // Upload acceleration structures to GPU
    UploadAccelerationToGPU();

    // Setup acceleration uniforms
    SetupAccelerationUniforms(camera);

    // Render visible instances
    auto renderStart = std::chrono::high_resolution_clock::now();

    for (int idx : visibleIndices) {
        if (idx >= 0 && idx < models.size() && models[idx]) {
            Render(*models[idx], camera, transforms[idx]);
        }
    }

    auto renderEnd = std::chrono::high_resolution_clock::now();
    m_stats.raymarchTimeMs = std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();

    auto frameEnd = std::chrono::high_resolution_clock::now();
    m_stats.totalFrameTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
}

void SDFRendererAccelerated::RenderWithOctree(const SDFModel& model, const Camera& camera,
                                                const glm::mat4& modelTransform) {
    if (!IsInitialized()) {
        return;
    }

    // Build octree if not built
    if (!m_octree->IsBuilt()) {
        VoxelizationSettings settings;
        settings.maxDepth = m_accelSettings.octreeDepth;
        settings.voxelSize = m_accelSettings.octreeVoxelSize;
        m_octree->Voxelize(model, settings);
    }

    // Upload octree to GPU
    if (!m_octree->IsGPUValid()) {
        m_octree->UploadToGPU();
    }

    // Bind octree texture
    auto shader = GetShader();
    if (shader) {
        shader->Use();
        shader->SetBool("u_useOctree", true);
        shader->SetInt("u_octreeTexture", 1);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, m_octree->GetGPUTexture());

        shader->SetVec3("u_octreeBoundsMin", m_octree->GetBoundsMin());
        shader->SetVec3("u_octreeBoundsMax", m_octree->GetBoundsMax());
        shader->SetFloat("u_octreeVoxelSize", m_accelSettings.octreeVoxelSize);
    }

    // Render with octree acceleration
    Render(model, camera, modelTransform);

    // Cleanup
    if (shader) {
        shader->SetBool("u_useOctree", false);
    }
}

void SDFRendererAccelerated::RenderWithBrickMap(const SDFModel& model, const Camera& camera,
                                                 const glm::mat4& modelTransform) {
    if (!IsInitialized()) {
        return;
    }

    // Build brick map if not built
    if (!m_brickMap->IsBuilt()) {
        BrickMapSettings settings;
        settings.brickResolution = m_accelSettings.brickResolution;
        settings.worldVoxelSize = m_accelSettings.brickVoxelSize;
        m_brickMap->Build(model, settings);
    }

    // Upload to GPU
    if (!m_brickMap->IsGPUValid()) {
        m_brickMap->UploadToGPU();
    }

    // Bind brick map
    auto shader = GetShader();
    if (shader) {
        shader->Use();
        shader->SetBool("u_useBrickMap", true);
        shader->SetInt("u_brickMapTexture", 2);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_brickMap->GetGPUTexture());

        shader->SetVec3("u_brickMapBoundsMin", m_brickMap->GetBoundsMin());
        shader->SetVec3("u_brickMapBoundsMax", m_brickMap->GetBoundsMax());
    }

    // Render with brick map
    Render(model, camera, modelTransform);

    // Cleanup
    if (shader) {
        shader->SetBool("u_useBrickMap", false);
    }
}

void SDFRendererAccelerated::SetAccelerationSettings(const SDFAccelerationSettings& settings) {
    m_accelSettings = settings;

    // If settings changed significantly, rebuild acceleration
    if (m_accelerationBuilt) {
        m_accelerationBuilt = false;
    }
}

std::vector<int> SDFRendererAccelerated::PerformFrustumCulling(const Camera& camera) {
    if (!m_bvh->IsBuilt()) {
        return {};
    }

    // Create frustum from camera
    glm::mat4 projView = camera.GetProjection() * camera.GetView();
    Frustum frustum(projView);

    // Query BVH for visible instances
    return m_bvh->QueryFrustum(frustum);
}

void SDFRendererAccelerated::UploadAccelerationToGPU() {
    // Upload BVH
    if (m_accelSettings.useBVH && m_bvh->IsBuilt() && !m_bvh->IsGPUValid()) {
        m_bvh->UploadToGPU();
    }

    // Upload Octree
    if (m_accelSettings.useOctree && m_octree->IsBuilt() && !m_octree->IsGPUValid()) {
        m_octree->UploadToGPU();
    }

    // Upload Brick Map
    if (m_accelSettings.useBrickMap && m_brickMap->IsBuilt() && !m_brickMap->IsGPUValid()) {
        m_brickMap->UploadToGPU();
    }
}

void SDFRendererAccelerated::SetupAccelerationUniforms(const Camera& camera) {
    auto shader = GetShader();
    if (!shader) {
        return;
    }

    shader->Use();

    // BVH uniforms
    if (m_accelSettings.useBVH && m_bvh->IsBuilt()) {
        shader->SetBool("u_useBVH", true);
        shader->SetInt("u_bvhNodeCount", static_cast<int>(m_bvh->GetNodes().size()));

        // Bind BVH buffer
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_bvh->GetGPUBuffer());
    } else {
        shader->SetBool("u_useBVH", false);
    }

    // Octree uniforms
    if (m_accelSettings.useOctree && m_octree->IsBuilt()) {
        shader->SetBool("u_useOctree", true);
        shader->SetBool("u_enableEmptySpaceSkipping", m_accelSettings.enableEmptySpaceSkipping);
    } else {
        shader->SetBool("u_useOctree", false);
    }

    // Brick map uniforms
    if (m_accelSettings.useBrickMap && m_brickMap->IsBuilt()) {
        shader->SetBool("u_useBrickMap", true);
        shader->SetBool("u_enableDistanceCache", m_accelSettings.enableDistanceCache);
    } else {
        shader->SetBool("u_useBrickMap", false);
    }
}

void SDFRendererAccelerated::UpdateStats() {
    m_stats.bvhMemoryBytes = m_bvh ? m_bvh->GetMemoryUsage() : 0;
    m_stats.octreeMemoryBytes = m_octree ? m_octree->GetMemoryUsage() : 0;
    m_stats.brickMapMemoryBytes = m_brickMap ? m_brickMap->GetMemoryUsage() : 0;
}

} // namespace Nova
