#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace Nova {

class Shader;
class Texture;
class Camera;
class RadianceCascade;

/**
 * @brief SDF-based terrain representation
 *
 * Converts heightmap-based terrain into Signed Distance Field representation
 * for integration with global illumination and raymarching pipelines.
 *
 * Features:
 * - Heightmap to SDF conversion
 * - Sparse octree acceleration
 * - Multi-resolution LOD
 * - Support for caves and overhangs (full 3D SDF)
 * - GPU-friendly 3D texture format
 * - Material ID per voxel
 * - Optimized for real-time raytracing
 */
class SDFTerrain {
public:
    /**
     * @brief Configuration for SDF terrain
     */
    struct Config {
        // Voxelization resolution
        int resolution = 512;                   // Voxel grid resolution per axis
        float worldSize = 1000.0f;              // World space size
        float maxHeight = 100.0f;               // Maximum terrain height

        // Octree acceleration
        int octreeLevels = 6;                   // Depth of octree
        bool useOctree = true;                  // Enable octree acceleration
        bool sparseStorage = true;              // Only store non-empty octree nodes

        // LOD configuration
        int numLODLevels = 4;                   // Number of LOD levels
        std::vector<float> lodDistances = {     // Distance thresholds for each LOD
            100.0f, 250.0f, 500.0f, 1000.0f
        };

        // Quality settings
        bool supportCaves = false;              // Full 3D voxelization (slower)
        bool highPrecision = false;             // Use 16-bit floats instead of 8-bit
        bool compressGPU = true;                // BC4 compression for GPU storage

        // Material support
        int numMaterials = 8;                   // Max material types
        bool storeMaterialPerVoxel = true;      // Material ID per voxel

        // Performance
        bool asyncBuild = true;                 // Build SDF on worker thread
        int maxVoxelsPerFrame = 65536;          // Max voxels to process per frame
    };

    /**
     * @brief Sparse octree node for acceleration
     */
    struct OctreeNode {
        glm::vec3 center{0.0f};                 // Node center in world space
        float halfSize = 0.0f;                  // Half-size of node

        float minDist = -1.0f;                  // Minimum distance in this node
        float maxDist = 1.0f;                   // Maximum distance in this node

        int children[8] = {-1, -1, -1, -1, -1, -1, -1, -1};  // Child indices (-1 = none)

        int voxelDataStart = -1;                // For leaf nodes: index into voxel data
        int voxelDataCount = 0;                 // Number of voxels in this leaf

        bool isEmpty = false;                   // Skip during traversal
        bool isSolid = false;                   // Solid throughout (minDist < 0)

        // Get child index for position
        [[nodiscard]] int GetChildIndex(const glm::vec3& pos) const;

        // Check if node contains position
        [[nodiscard]] bool Contains(const glm::vec3& pos) const;
    };

    /**
     * @brief Material properties for terrain
     */
    struct TerrainMaterial {
        glm::vec3 albedo{0.5f};
        float roughness = 0.8f;
        float metallic = 0.0f;
        glm::vec3 emissive{0.0f};

        // Texture IDs (optional)
        int albedoTextureID = -1;
        int normalTextureID = -1;
        int roughnessTextureID = -1;
    };

    /**
     * @brief Statistics for debugging
     */
    struct Stats {
        size_t totalVoxels = 0;
        size_t nonEmptyVoxels = 0;
        size_t octreeNodes = 0;
        size_t memoryBytes = 0;
        float buildTimeMs = 0.0f;
        float lastQueryTimeUs = 0.0f;
    };

    SDFTerrain();
    ~SDFTerrain();

    // Non-copyable
    SDFTerrain(const SDFTerrain&) = delete;
    SDFTerrain& operator=(const SDFTerrain&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize SDF terrain
     */
    bool Initialize(const Config& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Building SDF from Heightmap
    // =========================================================================

    /**
     * @brief Build SDF from heightmap data
     * @param heightData Raw heightmap data (row-major)
     * @param width Heightmap width
     * @param height Heightmap height
     */
    void BuildFromHeightmap(const float* heightData, int width, int height);

    /**
     * @brief Build SDF from TerrainGenerator
     */
    void BuildFromTerrainGenerator(class TerrainGenerator& terrain);

    /**
     * @brief Build SDF from VoxelTerrain
     */
    void BuildFromVoxelTerrain(class VoxelTerrain& terrain);

    /**
     * @brief Build SDF using custom generator function
     * @param generator Function that returns signed distance at world position
     */
    void BuildFromFunction(std::function<float(const glm::vec3&)> generator);

    /**
     * @brief Check if SDF is being built
     */
    [[nodiscard]] bool IsBuilding() const;

    /**
     * @brief Get build progress (0.0 to 1.0)
     */
    [[nodiscard]] float GetBuildProgress() const;

    // =========================================================================
    // SDF Queries
    // =========================================================================

    /**
     * @brief Query signed distance at world position
     * @return Signed distance (negative = inside terrain, positive = outside)
     */
    [[nodiscard]] float QueryDistance(const glm::vec3& pos) const;

    /**
     * @brief Query normal at world position
     */
    [[nodiscard]] glm::vec3 QueryNormal(const glm::vec3& pos) const;

    /**
     * @brief Query material ID at world position
     */
    [[nodiscard]] int QueryMaterialID(const glm::vec3& pos) const;

    /**
     * @brief Sample interpolated distance (trilinear)
     */
    [[nodiscard]] float SampleDistance(const glm::vec3& pos) const;

    /**
     * @brief Get height at XZ position (raycast down)
     */
    [[nodiscard]] float GetHeightAt(float x, float z) const;

    /**
     * @brief Check if position is inside terrain
     */
    [[nodiscard]] bool IsInside(const glm::vec3& pos) const {
        return QueryDistance(pos) < 0.0f;
    }

    // =========================================================================
    // LOD Management
    // =========================================================================

    /**
     * @brief Update LOD based on camera position
     */
    void UpdateLOD(const glm::vec3& cameraPos);

    /**
     * @brief Get active LOD level for position
     */
    [[nodiscard]] int GetLODLevel(const glm::vec3& pos, const glm::vec3& cameraPos) const;

    /**
     * @brief Get LOD-specific SDF texture
     */
    [[nodiscard]] unsigned int GetLODTexture(int lodLevel) const;

    // =========================================================================
    // GPU Resources
    // =========================================================================

    /**
     * @brief Upload SDF to GPU
     */
    void UploadToGPU();

    /**
     * @brief Get main SDF 3D texture
     */
    [[nodiscard]] unsigned int GetSDFTexture() const { return m_sdfTexture; }

    /**
     * @brief Get octree SSBO
     */
    [[nodiscard]] unsigned int GetOctreeSSBO() const { return m_octreeSSBO; }

    /**
     * @brief Get material buffer SSBO
     */
    [[nodiscard]] unsigned int GetMaterialSSBO() const { return m_materialSSBO; }

    /**
     * @brief Bind textures and buffers for rendering
     */
    void BindForRendering(Shader* shader);

    // =========================================================================
    // Raymarching Integration
    // =========================================================================

    /**
     * @brief Raymarch against terrain SDF
     */
    bool Raymarch(const glm::vec3& origin, const glm::vec3& direction,
                  float maxDist, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

    /**
     * @brief Raymarch with octree acceleration
     */
    bool RaymarchAccelerated(const glm::vec3& origin, const glm::vec3& direction,
                             float maxDist, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

    // =========================================================================
    // Material Management
    // =========================================================================

    /**
     * @brief Set material for material ID
     */
    void SetMaterial(int materialID, const TerrainMaterial& material);

    /**
     * @brief Get material by ID
     */
    [[nodiscard]] const TerrainMaterial& GetMaterial(int materialID) const;

    /**
     * @brief Get all materials
     */
    [[nodiscard]] const std::vector<TerrainMaterial>& GetMaterials() const { return m_materials; }

    // =========================================================================
    // Configuration & Stats
    // =========================================================================

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const Config& GetConfig() const { return m_config; }

    /**
     * @brief Get statistics
     */
    [[nodiscard]] const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Get world bounds
     */
    [[nodiscard]] glm::vec3 GetWorldMin() const { return m_worldMin; }
    [[nodiscard]] glm::vec3 GetWorldMax() const { return m_worldMax; }

private:
    // Building helpers
    void BuildOctree();
    void VoxelizeTerrain();
    void CalculateDistanceField();
    float SampleHeightmap(float x, float z) const;

    // Octree helpers
    void BuildOctreeNode(int nodeIndex, const glm::vec3& center, float halfSize, int depth);
    int FindOctreeNode(const glm::vec3& pos) const;
    void TraverseOctree(const glm::vec3& pos, std::function<void(const OctreeNode&)> callback) const;

    // Material helpers
    int DetermineMaterialID(const glm::vec3& pos, float height, float slope) const;
    void UploadMaterialsToGPU();

    // GPU upload helpers
    void CreateGPUTextures();
    void UpdateGPUTextures();

    // Configuration
    Config m_config;
    bool m_initialized = false;

    // Heightmap data (source)
    std::vector<float> m_heightmap;
    int m_heightmapWidth = 0;
    int m_heightmapHeight = 0;

    // SDF voxel grid
    std::vector<float> m_sdfData;               // 3D grid of distance values
    std::vector<uint8_t> m_materialIDs;         // Material ID per voxel

    // Sparse octree
    std::vector<OctreeNode> m_octree;
    int m_octreeRoot = 0;

    // LOD levels
    std::vector<std::vector<float>> m_lodSDF;   // SDF data for each LOD
    std::vector<unsigned int> m_lodTextures;     // GPU textures for LOD

    // GPU resources
    unsigned int m_sdfTexture = 0;              // Main 3D texture
    unsigned int m_octreeSSBO = 0;              // Octree structure buffer
    unsigned int m_materialSSBO = 0;            // Material data buffer

    // Materials
    std::vector<TerrainMaterial> m_materials;

    // World bounds
    glm::vec3 m_worldMin{0.0f};
    glm::vec3 m_worldMax{0.0f};

    // Statistics
    Stats m_stats;

    // Thread safety
    mutable std::mutex m_mutex;
    std::atomic<bool> m_building{false};
    std::atomic<float> m_buildProgress{0.0f};
};

/**
 * @brief Helper to convert voxel grid coordinate to array index
 */
inline int GetVoxelIndex(int x, int y, int z, int resolution) {
    return x + y * resolution + z * resolution * resolution;
}

/**
 * @brief Helper to convert world position to voxel grid coordinate
 */
inline glm::ivec3 WorldToVoxel(const glm::vec3& worldPos, const glm::vec3& worldMin,
                                 const glm::vec3& worldMax, int resolution) {
    glm::vec3 normalized = (worldPos - worldMin) / (worldMax - worldMin);
    normalized = glm::clamp(normalized, 0.0f, 1.0f);
    return glm::ivec3(normalized * float(resolution - 1));
}

/**
 * @brief Helper to convert voxel grid coordinate to world position
 */
inline glm::vec3 VoxelToWorld(const glm::ivec3& voxel, const glm::vec3& worldMin,
                                const glm::vec3& worldMax, int resolution) {
    glm::vec3 normalized = glm::vec3(voxel) / float(resolution - 1);
    return worldMin + normalized * (worldMax - worldMin);
}

} // namespace Nova
