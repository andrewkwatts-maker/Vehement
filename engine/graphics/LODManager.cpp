#include "graphics/LODManager.hpp"
#include "graphics/Mesh.hpp"
#include "scene/Camera.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace Nova {

// Static null mesh
std::shared_ptr<Mesh> LODManager::s_nullMesh = nullptr;

// ============================================================================
// LODGroup Implementation
// ============================================================================

bool LODGroup::AddLevel(std::shared_ptr<Mesh> mesh, float maxDistance) {
    if (numLevels >= MAX_LOD_LEVELS) {
        return false;
    }

    LODLevel& level = levels[numLevels];
    level.mesh = std::move(mesh);
    level.maxDistance = maxDistance;

    if (level.mesh) {
        level.triangleCount = level.mesh->GetIndexCount() / 3;

        if (numLevels > 0 && levels[0].triangleCount > 0) {
            level.reductionRatio = static_cast<float>(level.triangleCount) /
                                   static_cast<float>(levels[0].triangleCount);
        }
    }

    numLevels++;
    dirty = true;

    return true;
}

int LODGroup::GetLevelForDistance(float distance) const {
    for (int i = 0; i < numLevels - 1; i++) {
        if (distance < levels[i].maxDistance) {
            return i;
        }
    }
    return std::max(0, numLevels - 1);
}

int LODGroup::GetLevelForScreenSize(float screenSize) const {
    for (int i = 0; i < numLevels - 1; i++) {
        if (screenSize > levels[i].screenSizeThreshold) {
            return i;
        }
    }
    return std::max(0, numLevels - 1);
}

const std::shared_ptr<Mesh>& LODGroup::GetCurrentMesh() const {
    if (currentLevel >= 0 && currentLevel < numLevels) {
        return levels[currentLevel].mesh;
    }
    return LODManager::s_nullMesh;
}

const std::shared_ptr<Mesh>& LODGroup::GetMesh(int level) const {
    if (level >= 0 && level < numLevels) {
        return levels[level].mesh;
    }
    return LODManager::s_nullMesh;
}

float LODGroup::GetFadeFactor(float distance) const {
    if (!enableFading || numLevels < 2) {
        return -1.0f;
    }

    int level = GetLevelForDistance(distance);
    if (level >= numLevels - 1) {
        return -1.0f;
    }

    float levelDistance = levels[level].maxDistance;
    float fadeStart = levelDistance - fadeRange;

    if (distance > fadeStart) {
        return (distance - fadeStart) / fadeRange;
    }

    return -1.0f;
}

// ============================================================================
// LODManager Implementation
// ============================================================================

LODManager::LODManager() = default;

LODManager::~LODManager() {
    Shutdown();
}

bool LODManager::Initialize(const LODConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_initialized = true;

    spdlog::info("LOD Manager initialized with {} distance levels",
                 std::count_if(m_config.distances.begin(), m_config.distances.end(),
                              [](float d) { return d > 0.0f; }));

    return true;
}

void LODManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_groups.clear();
    m_initialized = false;
}

void LODManager::Update(const Camera& camera) {
    m_stats.Reset();

    m_cameraPosition = camera.GetPosition();
    m_fov = camera.GetFOV();
    m_aspectRatio = camera.GetAspectRatio();

    if (!m_config.enabled) {
        return;
    }

    uint32_t totalLODSum = 0;

    for (auto& [id, group] : m_groups) {
        m_stats.totalGroups++;

        // Calculate distance
        float distance = glm::distance(m_cameraPosition, group.center);
        group.lastDistance = distance;

        // Calculate LOD level
        int newLevel;
        if (m_config.useScreenSizeMetric) {
            float screenSize = CalculateScreenSize(group.radius, distance);
            newLevel = CalculateLODLevelFromScreenSize(screenSize);
        } else {
            newLevel = CalculateLODLevel(distance);
        }

        // Clamp to available levels
        newLevel = std::clamp(newLevel, 0, group.numLevels - 1);

        // Apply hysteresis
        newLevel = ApplyHysteresis(group, newLevel, distance);

        // Update current level
        if (newLevel != group.currentLevel) {
            group.currentLevel = newLevel;
            group.dirty = true;
        }

        // Update stats
        m_stats.objectsPerLevel[group.currentLevel]++;
        totalLODSum += group.currentLevel;

        // Calculate triangles saved
        if (group.numLevels > 0 && group.levels[0].triangleCount > 0) {
            uint32_t fullDetail = group.levels[0].triangleCount;
            uint32_t currentDetail = group.levels[group.currentLevel].triangleCount;
            m_stats.trianglesSaved += fullDetail - currentDetail;
            m_stats.trianglesRendered += currentDetail;
        }
    }

    // Calculate average LOD level
    if (m_stats.totalGroups > 0) {
        m_stats.avgLODLevel = static_cast<float>(totalLODSum) / m_stats.totalGroups;

        uint32_t totalTriangles = m_stats.trianglesSaved + m_stats.trianglesRendered;
        if (totalTriangles > 0) {
            m_stats.lodEfficiency =
                (static_cast<float>(m_stats.trianglesSaved) / totalTriangles) * 100.0f;
        }
    }
}

uint32_t LODManager::CreateLODGroup(const std::string& name) {
    LODGroup group;
    group.id = m_nextGroupID++;
    group.name = name.empty() ? "LODGroup_" + std::to_string(group.id) : name;

    m_groups[group.id] = std::move(group);

    return group.id;
}

void LODManager::RemoveLODGroup(uint32_t groupID) {
    m_groups.erase(groupID);
}

LODGroup* LODManager::GetLODGroup(uint32_t groupID) {
    auto it = m_groups.find(groupID);
    return it != m_groups.end() ? &it->second : nullptr;
}

const LODGroup* LODManager::GetLODGroup(uint32_t groupID) const {
    auto it = m_groups.find(groupID);
    return it != m_groups.end() ? &it->second : nullptr;
}

bool LODManager::AddLODLevel(uint32_t groupID, std::shared_ptr<Mesh> mesh, float maxDistance) {
    LODGroup* group = GetLODGroup(groupID);
    if (!group) {
        return false;
    }

    // Use config default distance if not specified
    if (maxDistance <= 0.0f && group->numLevels < MAX_LOD_LEVELS) {
        maxDistance = m_config.distances[group->numLevels];
    }

    return group->AddLevel(std::move(mesh), maxDistance);
}

void LODManager::SetGroupBounds(uint32_t groupID, const glm::vec3& center, float radius) {
    LODGroup* group = GetLODGroup(groupID);
    if (group) {
        group->center = center;
        group->radius = radius;
    }
}

void LODManager::UpdateGroupPosition(uint32_t groupID, const glm::vec3& worldPosition) {
    LODGroup* group = GetLODGroup(groupID);
    if (group) {
        group->center = worldPosition;
    }
}

int LODManager::CalculateLODLevel(float distance) const {
    // Apply LOD bias
    float adjustedDistance = distance * std::exp(m_config.lodBias * 0.1f);

    for (int i = 0; i < MAX_LOD_LEVELS - 1; i++) {
        if (adjustedDistance < m_config.distances[i]) {
            return i;
        }
    }

    return MAX_LOD_LEVELS - 1;
}

int LODManager::CalculateLODLevelFromScreenSize(float screenSize) const {
    for (int i = 0; i < MAX_LOD_LEVELS - 1; i++) {
        if (screenSize > m_config.screenSizes[i]) {
            return i;
        }
    }

    return MAX_LOD_LEVELS - 1;
}

const std::shared_ptr<Mesh>& LODManager::GetMeshForDistance(uint32_t groupID, float distance) {
    LODGroup* group = GetLODGroup(groupID);
    if (!group) {
        return s_nullMesh;
    }

    if (distance >= 0.0f) {
        group->currentLevel = CalculateLODLevel(distance);
        group->currentLevel = std::clamp(group->currentLevel, 0, group->numLevels - 1);
    }

    return group->GetCurrentMesh();
}

void LODManager::ForceLODLevel(uint32_t groupID, int level) {
    LODGroup* group = GetLODGroup(groupID);
    if (group) {
        group->currentLevel = std::clamp(level, 0, group->numLevels - 1);
    }
}

void LODManager::ClearForcedLODLevel(uint32_t groupID) {
    LODGroup* group = GetLODGroup(groupID);
    if (group) {
        group->dirty = true;
    }
}

void LODManager::SetConfig(const LODConfig& config) {
    m_config = config;

    // Mark all groups as dirty
    for (auto& [id, group] : m_groups) {
        group.dirty = true;
    }
}

float LODManager::CalculateScreenSize(float objectRadius, float distance) const {
    if (distance < 0.001f) {
        return 1.0f;
    }

    // Calculate angular size
    float angularSize = 2.0f * std::atan(objectRadius / distance);

    // Convert to screen ratio based on FOV
    float fovRadians = glm::radians(m_fov);
    return angularSize / fovRadians;
}

int LODManager::ApplyHysteresis(const LODGroup& group, int newLevel, float distance) const {
    if (m_config.hysteresis <= 1.0f) {
        return newLevel;
    }

    int currentLevel = group.currentLevel;

    // Going to lower detail
    if (newLevel > currentLevel) {
        // Need to be further than threshold * hysteresis to switch
        float threshold = m_config.distances[currentLevel];
        if (distance < threshold * m_config.hysteresis) {
            return currentLevel;
        }
    }
    // Going to higher detail
    else if (newLevel < currentLevel) {
        // Need to be closer than threshold / hysteresis to switch
        float threshold = m_config.distances[newLevel];
        if (distance > threshold / m_config.hysteresis) {
            return currentLevel;
        }
    }

    return newLevel;
}

std::vector<std::shared_ptr<Mesh>> LODManager::GenerateLODs(
    const std::shared_ptr<Mesh>& baseMesh,
    int numLevels,
    const std::array<float, MAX_LOD_LEVELS>& reductionFactors) {

    std::vector<std::shared_ptr<Mesh>> lods;

    if (!baseMesh || numLevels <= 0) {
        return lods;
    }

    // First level is always the base mesh
    lods.push_back(baseMesh);

    // Generate simplified meshes for each level
    // Note: Actual mesh simplification would require a decimation algorithm
    // like quadric error metrics. This is a placeholder.
    for (int i = 1; i < numLevels && i < MAX_LOD_LEVELS; i++) {
        float ratio = reductionFactors[i];

        // In a real implementation, we would:
        // 1. Get vertex/index data from baseMesh
        // 2. Apply mesh simplification algorithm
        // 3. Create new mesh with reduced data

        // For now, just duplicate the mesh as a placeholder
        // Real implementation would use mesh simplification
        auto lodMesh = std::make_shared<Mesh>();

        // Copy mesh data (placeholder - real implementation would simplify)
        // lodMesh->CreateFromSimplified(baseMesh, ratio);

        lods.push_back(lodMesh);

        spdlog::debug("Generated LOD level {} with {}% reduction",
                      i, static_cast<int>((1.0f - ratio) * 100));
    }

    return lods;
}

std::array<float, MAX_LOD_LEVELS> LODManager::CalculateOptimalDistances(
    uint32_t baseTriangleCount,
    float baseDistance,
    float maxDistance) {

    std::array<float, MAX_LOD_LEVELS> distances;
    distances.fill(0.0f);

    if (baseTriangleCount == 0 || baseDistance >= maxDistance) {
        return distances;
    }

    // Use exponential progression
    float ratio = std::pow(maxDistance / baseDistance, 1.0f / (MAX_LOD_LEVELS - 1));

    float currentDistance = baseDistance;
    for (int i = 0; i < MAX_LOD_LEVELS; i++) {
        distances[i] = currentDistance;
        currentDistance *= ratio;
    }

    return distances;
}

// ============================================================================
// ImpostorSystem Implementation
// ============================================================================

ImpostorSystem::ImpostorSystem() = default;

ImpostorSystem::~ImpostorSystem() {
    if (m_framebuffer) {
        glDeleteFramebuffers(1, &m_framebuffer);
    }
    if (m_billboardVAO) {
        glDeleteVertexArrays(1, &m_billboardVAO);
    }
    if (m_billboardVBO) {
        glDeleteBuffers(1, &m_billboardVBO);
    }
}

ImpostorSystem::ImpostorData ImpostorSystem::GenerateImpostor(
    const Mesh& mesh, int resolution) {

    ImpostorData data;

    // Create framebuffer if needed
    if (m_framebuffer == 0) {
        glGenFramebuffers(1, &m_framebuffer);
    }

    // Create texture for impostor
    glGenTextures(1, &data.textureID);
    glBindTexture(GL_TEXTURE_2D_ARRAY, data.textureID);

    // Create array texture with views from multiple angles
    int numViews = data.viewAngles;
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                 resolution, resolution, numViews,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Calculate impostor size from mesh bounds
    glm::vec3 boundsSize = mesh.GetBoundsMax() - mesh.GetBoundsMin();
    data.size = glm::vec2(std::max(boundsSize.x, boundsSize.z), boundsSize.y);

    // Render mesh from multiple angles into texture array
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, resolution, resolution);

    for (int i = 0; i < numViews; i++) {
        float angle = (2.0f * 3.14159f * i) / numViews;

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  data.textureID, 0, i);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Setup orthographic projection looking at mesh from angle
        // Render mesh here (omitted for brevity)
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    return data;
}

void ImpostorSystem::RenderImpostor(
    const ImpostorData& data,
    const glm::vec3& position,
    const glm::vec3& cameraPosition,
    const glm::mat4& viewProjection) {

    if (data.textureID == 0) {
        return;
    }

    // Calculate which view to use based on camera angle
    glm::vec3 toCamera = glm::normalize(cameraPosition - position);
    float angle = std::atan2(toCamera.x, toCamera.z);
    if (angle < 0) angle += 2.0f * 3.14159f;

    int viewIndex = static_cast<int>((angle / (2.0f * 3.14159f)) * data.viewAngles);
    viewIndex = viewIndex % data.viewAngles;

    // Create billboard that faces camera (y-axis aligned)
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), -toCamera));
    glm::vec3 up(0, 1, 0);

    // Render textured billboard
    // This would use the billboard shader and VAO
    glBindTexture(GL_TEXTURE_2D_ARRAY, data.textureID);

    // Set uniform for texture layer
    // Draw billboard quad

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

} // namespace Nova
