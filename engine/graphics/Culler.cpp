#include "graphics/Culler.hpp"
#include "graphics/Mesh.hpp"
#include "scene/Camera.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace Nova {

// ============================================================================
// AABB Implementation
// ============================================================================

// Note: AABB::Transform and AABB::Merge are now defined inline in Culler.hpp
// to avoid duplicate symbol definitions across translation units.

// ============================================================================
// Frustum Implementation
// ============================================================================

// Note: All Frustum methods are now defined inline in Culler.hpp
// to avoid duplicate symbol definitions across translation units.

// ============================================================================
// HiZBuffer Implementation
// ============================================================================

HiZBuffer::HiZBuffer() = default;

HiZBuffer::~HiZBuffer() {
    Shutdown();
}

bool HiZBuffer::Initialize(int width, int height, int levels) {
    if (m_initialized) {
        return true;
    }

    m_width = width;
    m_height = height;
    m_levelCount = levels;

    // Create Hi-Z texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glTexStorage2D(GL_TEXTURE_2D, m_levelCount, GL_R32F, m_width, m_height);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Create framebuffer for downsample passes
    glGenFramebuffers(1, &m_framebuffer);

    m_initialized = true;
    spdlog::info("Hi-Z buffer initialized: {}x{}, {} levels", width, height, levels);

    return true;
}

void HiZBuffer::Shutdown() {
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }

    if (m_framebuffer) {
        glDeleteFramebuffers(1, &m_framebuffer);
        m_framebuffer = 0;
    }

    m_initialized = false;
}

void HiZBuffer::Update(uint32_t depthTexture) {
    if (!m_initialized) {
        return;
    }

    // Copy depth to level 0
    glCopyImageSubData(depthTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
                       m_texture, GL_TEXTURE_2D, 0, 0, 0, 0,
                       m_width, m_height, 1);

    // Generate mip levels by taking max of 4 pixels
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    int levelWidth = m_width;
    int levelHeight = m_height;

    for (int level = 1; level < m_levelCount; level++) {
        levelWidth = std::max(1, levelWidth / 2);
        levelHeight = std::max(1, levelHeight / 2);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_texture, level);

        glViewport(0, 0, levelWidth, levelHeight);

        // In a real implementation, we'd use a compute shader or
        // downsample shader here to take max of 4 texels
        // For now, we rely on manual mipmap generation
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool HiZBuffer::TestAABB(const AABB& aabb, const glm::mat4& viewProjection) const {
    if (!m_initialized) {
        return true; // Assume visible if no Hi-Z buffer
    }

    // Project AABB corners to screen space and find screen-space bounds
    glm::vec3 corners[8] = {
        glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z),
        glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z),
        glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
        glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
        glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
        glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z),
        glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)
    };

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();

    for (int i = 0; i < 8; i++) {
        glm::vec4 clip = viewProjection * glm::vec4(corners[i], 1.0f);

        if (clip.w <= 0.0f) {
            return true; // Behind camera, might be partially visible
        }

        glm::vec3 ndc = glm::vec3(clip) / clip.w;

        minX = std::min(minX, ndc.x);
        maxX = std::max(maxX, ndc.x);
        minY = std::min(minY, ndc.y);
        maxY = std::max(maxY, ndc.y);
        minZ = std::min(minZ, ndc.z);
    }

    // Clamp to viewport
    minX = std::clamp(minX, -1.0f, 1.0f);
    maxX = std::clamp(maxX, -1.0f, 1.0f);
    minY = std::clamp(minY, -1.0f, 1.0f);
    maxY = std::clamp(maxY, -1.0f, 1.0f);

    // Calculate appropriate mip level based on screen-space size
    float screenWidth = (maxX - minX) * 0.5f * static_cast<float>(m_width);
    float screenHeight = (maxY - minY) * 0.5f * static_cast<float>(m_height);
    float maxDim = std::max(screenWidth, screenHeight);

    int mipLevel = static_cast<int>(std::log2(maxDim + 1.0f));
    mipLevel = std::clamp(mipLevel, 0, m_levelCount - 1);

    // Convert to texture coordinates
    float u = (minX + 1.0f) * 0.5f;
    float v = (minY + 1.0f) * 0.5f;

    int levelWidth = m_width >> mipLevel;
    int levelHeight = m_height >> mipLevel;

    // Note: texX and texY calculated for future per-pixel depth query optimization
    [[maybe_unused]] int texX = static_cast<int>(u * static_cast<float>(levelWidth));
    [[maybe_unused]] int texY = static_cast<int>(v * static_cast<float>(levelHeight));

    // Read depth from Hi-Z buffer at calculated mip level
    glBindTexture(GL_TEXTURE_2D, m_texture);

    float hiZDepth = 0.0f;
    glGetTexImage(GL_TEXTURE_2D, mipLevel, GL_RED, GL_FLOAT, &hiZDepth);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Object is occluded if its nearest depth is further than Hi-Z depth
    return minZ <= hiZDepth;
}

// ============================================================================
// Culler Implementation
// ============================================================================

Culler::Culler() = default;

Culler::~Culler() {
    Shutdown();
}

bool Culler::Initialize(const CullingConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Initialize Hi-Z buffer for occlusion culling
    if (m_config.occlusionCullingEnabled) {
        m_hiZBuffer = std::make_unique<HiZBuffer>();
        m_hiZBuffer->Initialize(m_config.occlusionResolution,
                                 m_config.occlusionResolution,
                                 m_config.occlusionHierarchyDepth);
    }

    m_initialized = true;
    spdlog::info("Culling system initialized");

    return true;
}

void Culler::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_objects.clear();
    m_freeIndices.clear();
    m_occlusionQueries.clear();
    m_hiZBuffer.reset();

    m_initialized = false;
}

void Culler::BeginFrame(const Camera& camera) {
    m_stats.Reset();

    // Update camera data
    m_cameraPosition = camera.GetPosition();
    m_viewProjection = camera.GetProjectionView();
    m_fov = camera.GetFOV();
    m_aspectRatio = camera.GetAspectRatio();
    m_nearPlane = camera.GetNearPlane();
    m_farPlane = camera.GetFarPlane();

    // Extract frustum planes
    m_frustum.ExtractFromMatrix(m_viewProjection);

    // Reset visibility flags
    for (auto& obj : m_objects) {
        obj.visible = true;
        obj.occluded = false;
    }
}

void Culler::EndFrame() {
    // Process pending occlusion queries
    if (m_config.occlusionCullingEnabled) {
        ProcessOcclusionQueries();
    }

    // Calculate efficiency
    if (m_stats.totalObjects > 0) {
        m_stats.cullingEfficiency =
            (1.0f - static_cast<float>(m_stats.visibleObjects) / m_stats.totalObjects) * 100.0f;
    }
}

uint32_t Culler::RegisterObject(const AABB& bounds, void* userData) {
    CullableObject obj;
    obj.id = m_nextObjectID++;
    obj.worldBounds = bounds;
    obj.boundingSphere = BoundingSphere::FromAABB(bounds);
    obj.userData = userData;

    if (!m_freeIndices.empty()) {
        uint32_t index = m_freeIndices.back();
        m_freeIndices.pop_back();
        m_objects[index] = obj;
    } else {
        m_objects.push_back(obj);
    }

    return obj.id;
}

void Culler::UpdateObjectBounds(uint32_t objectID, const AABB& newBounds) {
    for (auto& obj : m_objects) {
        if (obj.id == objectID) {
            obj.worldBounds = newBounds;
            obj.boundingSphere = BoundingSphere::FromAABB(newBounds);
            return;
        }
    }
}

void Culler::RemoveObject(uint32_t objectID) {
    for (size_t i = 0; i < m_objects.size(); i++) {
        if (m_objects[i].id == objectID) {
            m_objects[i].id = 0;  // Mark as free
            m_freeIndices.push_back(static_cast<uint32_t>(i));
            return;
        }
    }
}

void Culler::ClearObjects() {
    m_objects.clear();
    m_freeIndices.clear();
    m_nextObjectID = 1;
}

std::vector<uint32_t> Culler::Cull() {
    std::vector<uint32_t> visibleObjects;
    visibleObjects.reserve(m_objects.size());

    for (auto& obj : m_objects) {
        if (obj.id == 0) {
            continue; // Skip freed objects
        }

        m_stats.totalObjects++;

        // Calculate distance and screen size
        obj.distanceToCamera = GetDistanceToCamera(obj.worldBounds.GetCenter());
        obj.screenSize = CalculateScreenSize(obj.boundingSphere.radius * 2.0f,
                                              obj.distanceToCamera);

        // Apply culling tests in order of cost (cheapest first)

        // 1. Distance culling (cheapest)
        if (m_config.distanceCullingEnabled && DistanceCull(obj)) {
            m_stats.distanceCulled++;
            obj.visible = false;
            continue;
        }

        // 2. Small object culling
        if (m_config.smallObjectCullingEnabled && SmallObjectCull(obj)) {
            m_stats.smallObjectCulled++;
            obj.visible = false;
            continue;
        }

        // 3. Frustum culling
        if (m_config.frustumCullingEnabled && FrustumCull(obj)) {
            m_stats.frustumCulled++;
            obj.visible = false;
            continue;
        }

        // 4. Occlusion culling (most expensive)
        if (m_config.occlusionCullingEnabled && OcclusionCull(obj)) {
            m_stats.occlusionCulled++;
            obj.occluded = true;
            obj.visible = false;
            continue;
        }

        // Object passed all culling tests
        obj.visible = true;
        visibleObjects.push_back(obj.id);
        m_stats.visibleObjects++;
    }

    return visibleObjects;
}

bool Culler::IsVisible(const AABB& bounds, const glm::mat4& transform) const {
    AABB worldBounds = bounds.Transform(transform);
    return m_frustum.ContainsAABB(worldBounds);
}

bool Culler::IsVisible(const glm::vec3& center, float radius) const {
    return m_frustum.ContainsSphere(center, radius);
}

bool Culler::IsVisible(const glm::vec3& point) const {
    return m_frustum.ContainsPoint(point);
}

float Culler::GetDistanceToCamera(const glm::vec3& point) const {
    return glm::distance(m_cameraPosition, point);
}

float Culler::CalculateScreenSize(float worldSize, float distance) const {
    if (distance < 0.001f) {
        return 1.0f;
    }

    // Calculate angular size
    float angularSize = 2.0f * std::atan(worldSize / (2.0f * distance));

    // Convert to screen ratio based on FOV
    float fovRadians = glm::radians(m_fov);
    return angularSize / fovRadians;
}

void Culler::SetConfig(const CullingConfig& config) {
    m_config = config;

    // Reinitialize Hi-Z buffer if occlusion culling settings changed
    if (m_config.occlusionCullingEnabled && !m_hiZBuffer) {
        m_hiZBuffer = std::make_unique<HiZBuffer>();
        m_hiZBuffer->Initialize(m_config.occlusionResolution,
                                 m_config.occlusionResolution,
                                 m_config.occlusionHierarchyDepth);
    } else if (!m_config.occlusionCullingEnabled && m_hiZBuffer) {
        m_hiZBuffer.reset();
    }
}

void Culler::UpdateOcclusionBuffer(uint32_t depthTexture) {
    if (m_hiZBuffer) {
        m_hiZBuffer->Update(depthTexture);
    }
}

bool Culler::FrustumCull(const CullableObject& object) const {
    // First test bounding sphere (cheaper)
    if (!m_frustum.ContainsSphere(object.boundingSphere.center,
                                   object.boundingSphere.radius)) {
        return true; // Culled
    }

    // Then test AABB for more precision
    return !m_frustum.ContainsAABB(object.worldBounds);
}

bool Culler::OcclusionCull(const CullableObject& object) const {
    if (!m_hiZBuffer) {
        return false;
    }

    return !m_hiZBuffer->TestAABB(object.worldBounds, m_viewProjection);
}

void Culler::ProcessOcclusionQueries() {
    for (auto& query : m_occlusionQueries) {
        if (!query.resultReady) {
            GLuint available = GL_FALSE;
            glGetQueryObjectuiv(query.queryID, GL_QUERY_RESULT_AVAILABLE, &available);

            if (available) {
                GLuint samplesPassed = 0;
                glGetQueryObjectuiv(query.queryID, GL_QUERY_RESULT, &samplesPassed);

                query.visible = (samplesPassed > 0);
                query.resultReady = true;
            }
        }
    }
}

void Culler::IssueOcclusionQuery(CullableObject& object) {
    // Create or reuse a query object
    OcclusionQuery query;
    glGenQueries(1, &query.queryID);
    query.objectID = object.id;
    query.resultReady = false;

    glBeginQuery(GL_ANY_SAMPLES_PASSED, query.queryID);

    // Render bounding box (in actual implementation)
    // This would draw a simple box mesh

    glEndQuery(GL_ANY_SAMPLES_PASSED);

    m_occlusionQueries.push_back(query);
}

bool Culler::DistanceCull(const CullableObject& object) const {
    return object.distanceToCamera > m_config.maxRenderDistance;
}

bool Culler::SmallObjectCull(const CullableObject& object) const {
    return object.screenSize < m_config.smallObjectThreshold;
}

// ============================================================================
// BVH Implementation
// ============================================================================

void BVH::Build(const std::vector<CullableObject>& objects) {
    if (objects.empty()) {
        Clear();
        return;
    }

    std::vector<int> indices(objects.size());
    for (size_t i = 0; i < objects.size(); i++) {
        indices[i] = static_cast<int>(i);
    }

    m_nodes.clear();
    m_nodes.reserve(objects.size() * 2);

    BuildRecursive(indices, 0, static_cast<int>(indices.size()), objects);
}

int BVH::BuildRecursive(std::vector<int>& indices, int start, int end,
                         const std::vector<CullableObject>& objects) {
    BVHNode node;

    // Calculate bounds for this node
    node.bounds = objects[indices[start]].worldBounds;
    for (int i = start + 1; i < end; i++) {
        node.bounds = AABB::Merge(node.bounds, objects[indices[i]].worldBounds);
    }

    int numObjects = end - start;

    if (numObjects == 1) {
        // Leaf node
        node.objectIndex = indices[start];
        m_nodes.push_back(node);
        return static_cast<int>(m_nodes.size() - 1);
    }

    // Find longest axis for splitting
    glm::vec3 extent = node.bounds.GetSize();
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent[axis]) axis = 2;

    // Sort objects along axis
    std::sort(indices.begin() + start, indices.begin() + end,
        [&objects, axis](int a, int b) {
            return objects[a].worldBounds.GetCenter()[axis] <
                   objects[b].worldBounds.GetCenter()[axis];
        });

    int mid = start + numObjects / 2;

    int nodeIndex = static_cast<int>(m_nodes.size());
    m_nodes.push_back(node);

    // Build children
    m_nodes[nodeIndex].leftChild = BuildRecursive(indices, start, mid, objects);
    m_nodes[nodeIndex].rightChild = BuildRecursive(indices, mid, end, objects);

    return nodeIndex;
}

void BVH::QueryFrustum(const Frustum& frustum,
                       std::vector<uint32_t>& visibleIndices) const {
    if (m_nodes.empty()) {
        return;
    }

    QueryFrustumRecursive(0, frustum, visibleIndices);
}

void BVH::QueryFrustumRecursive(int nodeIndex, const Frustum& frustum,
                                 std::vector<uint32_t>& results) const {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size())) {
        return;
    }

    const BVHNode& node = m_nodes[nodeIndex];

    // Test this node's bounds against frustum
    int cullResult = frustum.TestAABB(node.bounds);

    if (cullResult == -1) {
        // Completely outside
        return;
    }

    if (node.IsLeaf()) {
        results.push_back(static_cast<uint32_t>(node.objectIndex));
        return;
    }

    // Recurse into children
    QueryFrustumRecursive(node.leftChild, frustum, results);
    QueryFrustumRecursive(node.rightChild, frustum, results);
}

void BVH::QuerySphere(const glm::vec3& center, float radius,
                      std::vector<uint32_t>& indices) const {
    if (m_nodes.empty()) {
        return;
    }

    QuerySphereRecursive(0, center, radius, indices);
}

void BVH::QuerySphereRecursive(int nodeIndex, const glm::vec3& center,
                                float radius, std::vector<uint32_t>& results) const {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size())) {
        return;
    }

    const BVHNode& node = m_nodes[nodeIndex];

    // Test sphere-AABB intersection
    glm::vec3 closest = glm::clamp(center, node.bounds.min, node.bounds.max);
    float distSq = glm::dot(center - closest, center - closest);

    if (distSq > radius * radius) {
        return; // No intersection
    }

    if (node.IsLeaf()) {
        results.push_back(static_cast<uint32_t>(node.objectIndex));
        return;
    }

    QuerySphereRecursive(node.leftChild, center, radius, results);
    QuerySphereRecursive(node.rightChild, center, radius, results);
}

} // namespace Nova
