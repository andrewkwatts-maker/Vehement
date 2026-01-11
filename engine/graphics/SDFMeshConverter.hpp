#pragma once

/**
 * @file SDFMeshConverter.hpp
 * @brief Convert SDF models to triangle meshes for RTX acceleration
 *
 * Uses marching cubes or dual contouring to extract triangle meshes
 * from signed distance fields. Required for hardware ray tracing since
 * RTX works on triangle primitives, not procedural SDFs.
 *
 * Features:
 * - Marching cubes for smooth surfaces
 * - Dual contouring for sharp features
 * - Adaptive resolution based on SDF gradient
 * - Mesh optimization (decimation, smoothing)
 * - Caching for performance
 */

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Nova {

// Forward declarations
class SDFModel;
class Mesh;

/**
 * @brief Mesh extraction algorithm
 */
enum class MeshExtractionAlgorithm {
    MarchingCubes,      // Smooth surfaces, more triangles
    DualContouring,     // Sharp features, fewer triangles
    SurfaceNets,        // Balance between smooth and sharp
};

/**
 * @brief Mesh extraction settings
 */
struct MeshExtractionSettings {
    // Algorithm
    MeshExtractionAlgorithm algorithm = MeshExtractionAlgorithm::MarchingCubes;

    // Resolution
    float voxelSize = 0.1f;             // Smaller = higher detail
    bool adaptiveResolution = true;     // Use higher res near detail
    float adaptiveThreshold = 0.5f;     // Gradient threshold for subdivision

    // Bounds
    glm::vec3 boundsMin{-10.0f};
    glm::vec3 boundsMax{10.0f};
    bool autoComputeBounds = true;      // Calculate from SDF

    // Post-processing
    bool generateNormals = true;
    bool generateTexCoords = true;
    bool smoothNormals = true;
    bool optimizeMesh = true;           // Decimate unnecessary triangles
    float decimationRatio = 0.7f;       // Target: 70% of original triangles

    // Quality
    float isoValue = 0.0f;              // Surface threshold
    int maxTriangles = 1000000;         // Safety limit
};

/**
 * @brief Mesh extraction statistics
 */
struct MeshExtractionStats {
    // Timing
    double extractionTimeMs = 0.0;
    double optimizationTimeMs = 0.0;
    double totalTimeMs = 0.0;

    // Mesh stats
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    uint32_t originalTriangleCount = 0;

    // Voxel grid stats
    glm::ivec3 gridResolution{0};
    uint32_t voxelsProcessed = 0;
    uint32_t voxelsSkipped = 0;          // Empty space skipping

    // Memory
    size_t meshMemoryBytes = 0;

    [[nodiscard]] double GetDecimationRatio() const {
        if (originalTriangleCount == 0) return 1.0;
        return static_cast<double>(triangleCount) / static_cast<double>(originalTriangleCount);
    }

    [[nodiscard]] std::string ToString() const;
};

/**
 * @brief Vertex structure for mesh extraction
 */
struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    bool operator==(const MeshVertex& other) const {
        return position == other.position &&
               normal == other.normal &&
               texCoord == other.texCoord;
    }
};

/**
 * @brief SDF to Mesh Converter
 *
 * Extracts triangle meshes from SDF models for hardware ray tracing.
 * Caches results for reuse.
 */
class SDFMeshConverter {
public:
    SDFMeshConverter();
    ~SDFMeshConverter();

    // Non-copyable
    SDFMeshConverter(const SDFMeshConverter&) = delete;
    SDFMeshConverter& operator=(const SDFMeshConverter&) = delete;

    // =========================================================================
    // Mesh Extraction
    // =========================================================================

    /**
     * @brief Convert SDF model to triangle mesh
     * @param model SDF model to convert
     * @param settings Extraction settings
     * @return Generated mesh, or nullptr on failure
     */
    std::shared_ptr<Mesh> ExtractMesh(const SDFModel& model,
                                      const MeshExtractionSettings& settings = {});

    /**
     * @brief Extract mesh with specific bounds
     */
    std::shared_ptr<Mesh> ExtractMeshInBounds(const SDFModel& model,
                                              const glm::vec3& boundsMin,
                                              const glm::vec3& boundsMax,
                                              float voxelSize = 0.1f);

    /**
     * @brief Extract mesh using marching cubes
     */
    std::shared_ptr<Mesh> ExtractMarchingCubes(const SDFModel& model,
                                               const MeshExtractionSettings& settings);

    /**
     * @brief Extract mesh using dual contouring
     */
    std::shared_ptr<Mesh> ExtractDualContouring(const SDFModel& model,
                                                const MeshExtractionSettings& settings);

    // =========================================================================
    // Caching
    // =========================================================================

    /**
     * @brief Get cached mesh for model
     * @return Cached mesh, or nullptr if not cached
     */
    std::shared_ptr<Mesh> GetCachedMesh(const SDFModel& model, float voxelSize);

    /**
     * @brief Add mesh to cache
     */
    void CacheMesh(const SDFModel& model, float voxelSize, std::shared_ptr<Mesh> mesh);

    /**
     * @brief Clear mesh cache
     */
    void ClearCache();

    /**
     * @brief Get cache size (number of cached meshes)
     */
    [[nodiscard]] size_t GetCacheSize() const { return m_meshCache.size(); }

    /**
     * @brief Get total cache memory usage
     */
    [[nodiscard]] size_t GetCacheMemoryUsage() const;

    // =========================================================================
    // Settings & Statistics
    // =========================================================================

    [[nodiscard]] const MeshExtractionStats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = MeshExtractionStats{}; }

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Compute bounds for SDF model
     */
    static void ComputeBounds(const SDFModel& model, glm::vec3& outMin, glm::vec3& outMax);

    /**
     * @brief Estimate triangle count for given voxel size
     */
    static uint32_t EstimateTriangleCount(const SDFModel& model, float voxelSize);

private:
    // Marching cubes implementation
    void MarchingCubesImpl(const SDFModel& model,
                          const MeshExtractionSettings& settings,
                          std::vector<MeshVertex>& outVertices,
                          std::vector<uint32_t>& outIndices);

    // Dual contouring implementation
    void DualContouringImpl(const SDFModel& model,
                           const MeshExtractionSettings& settings,
                           std::vector<MeshVertex>& outVertices,
                           std::vector<uint32_t>& outIndices);

    // Sample SDF at position
    float SampleSDF(const SDFModel& model, const glm::vec3& position);

    // Compute SDF gradient (for normal calculation)
    glm::vec3 ComputeGradient(const SDFModel& model, const glm::vec3& position);

    // Post-processing
    void GenerateNormals(std::vector<MeshVertex>& vertices,
                        const std::vector<uint32_t>& indices);
    void SmoothNormals(std::vector<MeshVertex>& vertices,
                      const std::vector<uint32_t>& indices);
    void GenerateTexCoords(std::vector<MeshVertex>& vertices);
    void OptimizeMesh(std::vector<MeshVertex>& vertices,
                     std::vector<uint32_t>& indices,
                     float targetRatio);

    // Create Mesh object from vertices/indices
    std::shared_ptr<Mesh> CreateMesh(const std::vector<MeshVertex>& vertices,
                                     const std::vector<uint32_t>& indices);

    // Cache key generation
    struct CacheKey {
        const SDFModel* model;
        float voxelSize;

        bool operator==(const CacheKey& other) const {
            return model == other.model &&
                   std::abs(voxelSize - other.voxelSize) < 0.001f;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const {
            return std::hash<const void*>{}(key.model) ^
                   (std::hash<float>{}(key.voxelSize) << 1);
        }
    };

    // Cache
    std::unordered_map<CacheKey, std::shared_ptr<Mesh>, CacheKeyHash> m_meshCache;

    // Statistics
    MeshExtractionStats m_stats;

    // Marching cubes lookup tables
    static const int MC_EDGE_TABLE[256];
    static const int MC_TRI_TABLE[256][16];
};

} // namespace Nova
