#include "terrain/SDFTerrain.hpp"
#include <glad/gl.h>
#include "terrain/TerrainGenerator.hpp"
#include "terrain/VoxelTerrain.hpp"
#include "graphics/Shader.hpp"
#include "core/Logger.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <execution>
#include <chrono>
#include <cmath>

#ifdef _WIN32
#include <glad/gl.h>
#else
#include <OpenGL/gl3.h>
#endif

namespace Nova {

// =============================================================================
// OctreeNode Implementation
// =============================================================================

int SDFTerrain::OctreeNode::GetChildIndex(const glm::vec3& pos) const {
    int index = 0;
    if (pos.x >= center.x) index |= 1;
    if (pos.y >= center.y) index |= 2;
    if (pos.z >= center.z) index |= 4;
    return index;
}

bool SDFTerrain::OctreeNode::Contains(const glm::vec3& pos) const {
    return std::abs(pos.x - center.x) <= halfSize &&
           std::abs(pos.y - center.y) <= halfSize &&
           std::abs(pos.z - center.z) <= halfSize;
}

// =============================================================================
// SDFTerrain Implementation
// =============================================================================

SDFTerrain::SDFTerrain() {
    // Initialize default materials
    m_materials.resize(8);

    // Grass
    m_materials[0].albedo = glm::vec3(0.3f, 0.5f, 0.2f);
    m_materials[0].roughness = 0.9f;

    // Rock
    m_materials[1].albedo = glm::vec3(0.4f, 0.4f, 0.4f);
    m_materials[1].roughness = 0.8f;

    // Sand
    m_materials[2].albedo = glm::vec3(0.76f, 0.7f, 0.5f);
    m_materials[2].roughness = 0.85f;

    // Snow
    m_materials[3].albedo = glm::vec3(0.9f, 0.9f, 0.95f);
    m_materials[3].roughness = 0.6f;

    // Dirt
    m_materials[4].albedo = glm::vec3(0.4f, 0.3f, 0.2f);
    m_materials[4].roughness = 0.9f;

    // Water (should use transparency)
    m_materials[5].albedo = glm::vec3(0.1f, 0.3f, 0.5f);
    m_materials[5].roughness = 0.1f;

    // Ice
    m_materials[6].albedo = glm::vec3(0.7f, 0.8f, 0.9f);
    m_materials[6].roughness = 0.2f;

    // Lava (emissive)
    m_materials[7].albedo = glm::vec3(0.9f, 0.3f, 0.1f);
    m_materials[7].roughness = 0.5f;
    m_materials[7].emissive = glm::vec3(5.0f, 1.0f, 0.2f);
}

SDFTerrain::~SDFTerrain() {
    Shutdown();
}

bool SDFTerrain::Initialize(const Config& config) {
    if (m_initialized) {
        spdlog::warn("SDFTerrain already initialized");
        return true;
    }

    m_config = config;

    // Calculate world bounds
    m_worldMin = glm::vec3(-config.worldSize * 0.5f, 0.0f, -config.worldSize * 0.5f);
    m_worldMax = glm::vec3(config.worldSize * 0.5f, config.maxHeight, config.worldSize * 0.5f);

    // Allocate SDF data
    size_t totalVoxels = static_cast<size_t>(config.resolution) * config.resolution * config.resolution;
    m_sdfData.resize(totalVoxels, 1.0f);  // Initialize to "outside"

    if (config.storeMaterialPerVoxel) {
        m_materialIDs.resize(totalVoxels, 0);
    }

    // Create GPU resources
    CreateGPUTextures();

    m_initialized = true;
    spdlog::info("SDFTerrain initialized: resolution={}, worldSize={}, maxHeight={}",
                 config.resolution, config.worldSize, config.maxHeight);

    return true;
}

void SDFTerrain::Shutdown() {
    if (!m_initialized) return;

    // Delete GPU resources
    if (m_sdfTexture) {
        glDeleteTextures(1, &m_sdfTexture);
        m_sdfTexture = 0;
    }

    if (m_octreeSSBO) {
        glDeleteBuffers(1, &m_octreeSSBO);
        m_octreeSSBO = 0;
    }

    if (m_materialSSBO) {
        glDeleteBuffers(1, &m_materialSSBO);
        m_materialSSBO = 0;
    }

    for (auto tex : m_lodTextures) {
        if (tex) glDeleteTextures(1, &tex);
    }
    m_lodTextures.clear();

    // Clear data
    m_sdfData.clear();
    m_materialIDs.clear();
    m_octree.clear();
    m_heightmap.clear();

    m_initialized = false;
    spdlog::info("SDFTerrain shutdown");
}

// =============================================================================
// Building SDF from Heightmap
// =============================================================================

void SDFTerrain::BuildFromHeightmap(const float* heightData, int width, int height) {
    if (!m_initialized) {
        spdlog::error("SDFTerrain not initialized");
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    m_building.store(true);
    m_buildProgress.store(0.0f);

    // Store heightmap
    m_heightmapWidth = width;
    m_heightmapHeight = height;
    m_heightmap.assign(heightData, heightData + width * height);

    spdlog::info("Building SDF from heightmap ({}x{})", width, height);

    // Voxelize terrain
    VoxelizeTerrain();

    // Calculate distance field
    CalculateDistanceField();

    // Build octree for acceleration
    if (m_config.useOctree) {
        BuildOctree();
    }

    // Upload to GPU
    UploadToGPU();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    m_building.store(false);
    m_buildProgress.store(1.0f);

    spdlog::info("SDF build complete: {:.2f}ms, {} non-empty voxels, {} octree nodes",
                 m_stats.buildTimeMs, m_stats.nonEmptyVoxels, m_stats.octreeNodes);
}

void SDFTerrain::BuildFromTerrainGenerator(TerrainGenerator& terrain) {
    if (!m_initialized) {
        spdlog::error("SDFTerrain not initialized");
        return;
    }

    // Sample heightmap from terrain generator
    const int sampleRes = 256;
    std::vector<float> heightData(sampleRes * sampleRes);

    float worldSize = m_config.worldSize;
    float step = worldSize / float(sampleRes - 1);

    for (int z = 0; z < sampleRes; ++z) {
        for (int x = 0; x < sampleRes; ++x) {
            float worldX = -worldSize * 0.5f + x * step;
            float worldZ = -worldSize * 0.5f + z * step;
            heightData[z * sampleRes + x] = terrain.GetHeightAt(worldX, worldZ);
        }
    }

    BuildFromHeightmap(heightData.data(), sampleRes, sampleRes);
}

void SDFTerrain::BuildFromVoxelTerrain(VoxelTerrain& terrain) {
    // Convert voxel terrain density to SDF
    // This is a placeholder - actual implementation would traverse voxel chunks
    spdlog::warn("BuildFromVoxelTerrain not fully implemented yet");
}

void SDFTerrain::BuildFromFunction(std::function<float(const glm::vec3&)> generator) {
    if (!m_initialized) {
        spdlog::error("SDFTerrain not initialized");
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    m_building.store(true);
    m_buildProgress.store(0.0f);

    const int res = m_config.resolution;
    const size_t totalVoxels = static_cast<size_t>(res) * res * res;

    spdlog::info("Building SDF from function ({} voxels)", totalVoxels);

    // Parallel voxelization
    std::vector<int> indices(totalVoxels);
    std::iota(indices.begin(), indices.end(), 0);

    std::atomic<int> completed{0};

    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
        [&](int idx) {
            int z = idx / (res * res);
            int y = (idx / res) % res;
            int x = idx % res;

            glm::ivec3 voxel(x, y, z);
            glm::vec3 worldPos = VoxelToWorld(voxel, m_worldMin, m_worldMax, res);

            float dist = generator(worldPos);
            m_sdfData[idx] = dist;

            if (dist < 0.1f) {
                m_stats.nonEmptyVoxels++;
            }

            // Update progress
            int count = completed.fetch_add(1) + 1;
            if (count % 10000 == 0) {
                m_buildProgress.store(float(count) / float(totalVoxels));
            }
        });

    m_stats.totalVoxels = totalVoxels;

    // Build octree
    if (m_config.useOctree) {
        BuildOctree();
    }

    // Upload to GPU
    UploadToGPU();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    m_building.store(false);
    m_buildProgress.store(1.0f);

    spdlog::info("SDF build complete: {:.2f}ms", m_stats.buildTimeMs);
}

bool SDFTerrain::IsBuilding() const {
    return m_building.load();
}

float SDFTerrain::GetBuildProgress() const {
    return m_buildProgress.load();
}

// =============================================================================
// SDF Queries
// =============================================================================

float SDFTerrain::QueryDistance(const glm::vec3& pos) const {
    if (!m_initialized) return 1000.0f;

    glm::ivec3 voxel = WorldToVoxel(pos, m_worldMin, m_worldMax, m_config.resolution);

    // Bounds check
    if (voxel.x < 0 || voxel.x >= m_config.resolution ||
        voxel.y < 0 || voxel.y >= m_config.resolution ||
        voxel.z < 0 || voxel.z >= m_config.resolution) {
        return 1000.0f;  // Far outside
    }

    int idx = GetVoxelIndex(voxel.x, voxel.y, voxel.z, m_config.resolution);
    return m_sdfData[idx];
}

glm::vec3 SDFTerrain::QueryNormal(const glm::vec3& pos) const {
    const float eps = 0.5f;

    float dx = QueryDistance(pos + glm::vec3(eps, 0, 0)) - QueryDistance(pos - glm::vec3(eps, 0, 0));
    float dy = QueryDistance(pos + glm::vec3(0, eps, 0)) - QueryDistance(pos - glm::vec3(0, eps, 0));
    float dz = QueryDistance(pos + glm::vec3(0, 0, eps)) - QueryDistance(pos - glm::vec3(0, 0, eps));

    glm::vec3 normal(dx, dy, dz);
    float len = glm::length(normal);
    if (len > 0.0001f) {
        return normal / len;
    }
    return glm::vec3(0, 1, 0);
}

int SDFTerrain::QueryMaterialID(const glm::vec3& pos) const {
    if (!m_initialized || m_materialIDs.empty()) return 0;

    glm::ivec3 voxel = WorldToVoxel(pos, m_worldMin, m_worldMax, m_config.resolution);

    if (voxel.x < 0 || voxel.x >= m_config.resolution ||
        voxel.y < 0 || voxel.y >= m_config.resolution ||
        voxel.z < 0 || voxel.z >= m_config.resolution) {
        return 0;
    }

    int idx = GetVoxelIndex(voxel.x, voxel.y, voxel.z, m_config.resolution);
    return m_materialIDs[idx];
}

float SDFTerrain::SampleDistance(const glm::vec3& pos) const {
    // Trilinear interpolation
    glm::vec3 normalized = (pos - m_worldMin) / (m_worldMax - m_worldMin);
    glm::vec3 voxelF = normalized * float(m_config.resolution - 1);

    glm::ivec3 v0 = glm::ivec3(glm::floor(voxelF));
    glm::ivec3 v1 = glm::min(v0 + glm::ivec3(1), glm::ivec3(m_config.resolution - 1));

    glm::vec3 t = voxelF - glm::vec3(v0);

    const int res = m_config.resolution;

    auto sample = [&](int x, int y, int z) -> float {
        if (x < 0 || x >= res || y < 0 || y >= res || z < 0 || z >= res) {
            return 1000.0f;
        }
        return m_sdfData[GetVoxelIndex(x, y, z, res)];
    };

    // Trilinear interpolation
    float c000 = sample(v0.x, v0.y, v0.z);
    float c100 = sample(v1.x, v0.y, v0.z);
    float c010 = sample(v0.x, v1.y, v0.z);
    float c110 = sample(v1.x, v1.y, v0.z);
    float c001 = sample(v0.x, v0.y, v1.z);
    float c101 = sample(v1.x, v0.y, v1.z);
    float c011 = sample(v0.x, v1.y, v1.z);
    float c111 = sample(v1.x, v1.y, v1.z);

    float c00 = c000 * (1 - t.x) + c100 * t.x;
    float c01 = c001 * (1 - t.x) + c101 * t.x;
    float c10 = c010 * (1 - t.x) + c110 * t.x;
    float c11 = c011 * (1 - t.x) + c111 * t.x;

    float c0 = c00 * (1 - t.y) + c10 * t.y;
    float c1 = c01 * (1 - t.y) + c11 * t.y;

    return c0 * (1 - t.z) + c1 * t.z;
}

float SDFTerrain::GetHeightAt(float x, float z) const {
    // Raycast down from above
    glm::vec3 origin(x, m_config.maxHeight + 10.0f, z);
    glm::vec3 direction(0, -1, 0);

    glm::vec3 hitPoint, hitNormal;
    if (Raymarch(origin, direction, m_config.maxHeight + 20.0f, hitPoint, hitNormal)) {
        return hitPoint.y;
    }

    return 0.0f;
}

// =============================================================================
// LOD Management
// =============================================================================

void SDFTerrain::UpdateLOD(const glm::vec3& cameraPos) {
    // LOD update logic - not fully implemented
    // Would generate lower resolution SDF textures for distant regions
}

int SDFTerrain::GetLODLevel(const glm::vec3& pos, const glm::vec3& cameraPos) const {
    float dist = glm::distance(pos, cameraPos);

    for (int i = 0; i < static_cast<int>(m_config.lodDistances.size()); ++i) {
        if (dist < m_config.lodDistances[i]) {
            return i;
        }
    }

    return static_cast<int>(m_config.lodDistances.size()) - 1;
}

unsigned int SDFTerrain::GetLODTexture(int lodLevel) const {
    if (lodLevel >= 0 && lodLevel < static_cast<int>(m_lodTextures.size())) {
        return m_lodTextures[lodLevel];
    }
    return m_sdfTexture;
}

// =============================================================================
// GPU Resources
// =============================================================================

void SDFTerrain::CreateGPUTextures() {
    // Create 3D texture for SDF
    glGenTextures(1, &m_sdfTexture);
    glBindTexture(GL_TEXTURE_3D, m_sdfTexture);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    const int res = m_config.resolution;
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, res, res, res, 0, GL_RED, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_3D, 0);

    // Create octree SSBO
    glGenBuffers(1, &m_octreeSSBO);

    // Create material SSBO
    glGenBuffers(1, &m_materialSSBO);

    spdlog::info("Created GPU resources for SDF terrain");
}

void SDFTerrain::UploadToGPU() {
    if (!m_initialized || m_sdfData.empty()) return;

    // Upload SDF texture
    glBindTexture(GL_TEXTURE_3D, m_sdfTexture);
    const int res = m_config.resolution;
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, res, res, res, GL_RED, GL_FLOAT, m_sdfData.data());
    glBindTexture(GL_TEXTURE_3D, 0);

    // Upload octree
    if (m_config.useOctree && !m_octree.empty()) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_octreeSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_octree.size() * sizeof(OctreeNode),
                     m_octree.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    // Upload materials
    UploadMaterialsToGPU();

    spdlog::info("Uploaded SDF terrain to GPU");
}

void SDFTerrain::BindForRendering(Shader* shader) {
    if (!shader) return;

    // Bind SDF texture
    glActiveTexture(GL_TEXTURE0 + 10);  // Use texture unit 10
    glBindTexture(GL_TEXTURE_3D, m_sdfTexture);
    shader->SetInt("u_terrainSDF", 10);

    // Bind octree
    if (m_octreeSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_octreeSSBO);
    }

    // Bind materials
    if (m_materialSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_materialSSBO);
    }

    // Set uniforms
    shader->SetVec3("u_terrainWorldMin", m_worldMin);
    shader->SetVec3("u_terrainWorldMax", m_worldMax);
    shader->SetInt("u_terrainResolution", m_config.resolution);
    shader->SetBool("u_terrainUseOctree", m_config.useOctree);
}

// =============================================================================
// Raymarching
// =============================================================================

bool SDFTerrain::Raymarch(const glm::vec3& origin, const glm::vec3& direction,
                          float maxDist, glm::vec3& hitPoint, glm::vec3& hitNormal) const {
    const float threshold = 0.01f;
    const int maxSteps = 128;

    float t = 0.0f;
    glm::vec3 pos = origin;

    for (int i = 0; i < maxSteps; ++i) {
        pos = origin + direction * t;

        float dist = SampleDistance(pos);

        if (dist < threshold) {
            hitPoint = pos;
            hitNormal = QueryNormal(pos);
            return true;
        }

        t += std::max(dist * 0.9f, 0.01f);  // Use 90% of distance for safety

        if (t > maxDist) {
            break;
        }
    }

    return false;
}

bool SDFTerrain::RaymarchAccelerated(const glm::vec3& origin, const glm::vec3& direction,
                                     float maxDist, glm::vec3& hitPoint, glm::vec3& hitNormal) const {
    // Use octree to skip empty space
    // Simplified version - full implementation would traverse octree
    return Raymarch(origin, direction, maxDist, hitPoint, hitNormal);
}

// =============================================================================
// Material Management
// =============================================================================

void SDFTerrain::SetMaterial(int materialID, const TerrainMaterial& material) {
    if (materialID >= 0 && materialID < static_cast<int>(m_materials.size())) {
        m_materials[materialID] = material;
        UploadMaterialsToGPU();
    }
}

const SDFTerrain::TerrainMaterial& SDFTerrain::GetMaterial(int materialID) const {
    if (materialID >= 0 && materialID < static_cast<int>(m_materials.size())) {
        return m_materials[materialID];
    }
    return m_materials[0];
}

void SDFTerrain::UploadMaterialsToGPU() {
    if (!m_materialSSBO || m_materials.empty()) return;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_materialSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, m_materials.size() * sizeof(TerrainMaterial),
                 m_materials.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// =============================================================================
// Private Helpers
// =============================================================================

void SDFTerrain::VoxelizeTerrain() {
    if (m_heightmap.empty()) return;

    const int res = m_config.resolution;
    const size_t totalVoxels = static_cast<size_t>(res) * res * res;

    spdlog::info("Voxelizing terrain...");

    for (int z = 0; z < res; ++z) {
        for (int y = 0; y < res; ++y) {
            for (int x = 0; x < res; ++x) {
                glm::ivec3 voxel(x, y, z);
                glm::vec3 worldPos = VoxelToWorld(voxel, m_worldMin, m_worldMax, res);

                float terrainHeight = SampleHeightmap(worldPos.x, worldPos.z);
                float signedDist = worldPos.y - terrainHeight;

                int idx = GetVoxelIndex(x, y, z, res);
                m_sdfData[idx] = signedDist;

                if (signedDist < 0.1f) {
                    m_stats.nonEmptyVoxels++;

                    // Determine material
                    if (m_config.storeMaterialPerVoxel) {
                        m_materialIDs[idx] = DetermineMaterialID(worldPos, terrainHeight, 0.0f);
                    }
                }
            }
        }
    }

    m_stats.totalVoxels = totalVoxels;
}

void SDFTerrain::CalculateDistanceField() {
    // For heightfield terrain, signed distance is simply (y - heightmap)
    // For full 3D terrain, would need to use jump flooding or similar
    // Current implementation already has correct signed distance from voxelization
}

float SDFTerrain::SampleHeightmap(float x, float z) const {
    if (m_heightmap.empty()) return 0.0f;

    // Normalize to heightmap coordinates
    float nx = (x - m_worldMin.x) / (m_worldMax.x - m_worldMin.x);
    float nz = (z - m_worldMin.z) / (m_worldMax.z - m_worldMin.z);

    nx = std::clamp(nx, 0.0f, 1.0f);
    nz = std::clamp(nz, 0.0f, 1.0f);

    float fx = nx * (m_heightmapWidth - 1);
    float fz = nz * (m_heightmapHeight - 1);

    int x0 = static_cast<int>(std::floor(fx));
    int z0 = static_cast<int>(std::floor(fz));
    int x1 = std::min(x0 + 1, m_heightmapWidth - 1);
    int z1 = std::min(z0 + 1, m_heightmapHeight - 1);

    float tx = fx - x0;
    float tz = fz - z0;

    float h00 = m_heightmap[z0 * m_heightmapWidth + x0];
    float h10 = m_heightmap[z0 * m_heightmapWidth + x1];
    float h01 = m_heightmap[z1 * m_heightmapWidth + x0];
    float h11 = m_heightmap[z1 * m_heightmapWidth + x1];

    float h0 = h00 * (1 - tx) + h10 * tx;
    float h1 = h01 * (1 - tx) + h11 * tx;

    return h0 * (1 - tz) + h1 * tz;
}

void SDFTerrain::BuildOctree() {
    spdlog::info("Building octree...");

    m_octree.clear();
    m_octree.reserve(10000);  // Reserve space

    // Create root node
    glm::vec3 center = (m_worldMin + m_worldMax) * 0.5f;
    float halfSize = glm::length(m_worldMax - m_worldMin) * 0.5f;

    OctreeNode root;
    root.center = center;
    root.halfSize = halfSize;
    m_octree.push_back(root);

    m_octreeRoot = 0;

    // Build recursively
    BuildOctreeNode(0, center, halfSize, 0);

    m_stats.octreeNodes = m_octree.size();
    spdlog::info("Octree built: {} nodes", m_stats.octreeNodes);
}

void SDFTerrain::BuildOctreeNode(int nodeIndex, const glm::vec3& center,
                                  float halfSize, int depth) {
    if (depth >= m_config.octreeLevels) {
        return;  // Max depth reached
    }

    // Sample SDF at node corners to determine min/max distance
    // Simplified: just mark as non-empty for now
    m_octree[nodeIndex].isEmpty = false;
    m_octree[nodeIndex].isSolid = false;
}

int SDFTerrain::DetermineMaterialID(const glm::vec3& pos, float height, float slope) const {
    float normalizedHeight = height / m_config.maxHeight;

    if (normalizedHeight < 0.1f) return 5;  // Water
    if (normalizedHeight < 0.3f) return 2;  // Sand
    if (normalizedHeight < 0.7f) return 0;  // Grass
    if (normalizedHeight < 0.85f) return 1; // Rock
    return 3;  // Snow
}

} // namespace Nova
