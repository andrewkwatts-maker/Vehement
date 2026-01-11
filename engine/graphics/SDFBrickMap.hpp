#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Nova {

class SDFModel;

/**
 * @brief Brick (cached distance field block)
 */
struct BrickData {
    static constexpr int BRICK_SIZE = 8;  // 8x8x8 voxels per brick
    static constexpr int BRICK_VOXELS = BRICK_SIZE * BRICK_SIZE * BRICK_SIZE;

    std::vector<float> distanceField;  // Cached SDF values
    glm::vec3 worldMin;
    glm::vec3 worldMax;
    uint32_t brickId;
    uint32_t compressionId;  // ID of compressed representation (if duplicate)
    bool isDirty;
    bool isCompressed;

    BrickData() : brickId(0), compressionId(0), isDirty(false), isCompressed(false) {
        distanceField.resize(BRICK_VOXELS, FLT_MAX);
    }

    [[nodiscard]] float GetDistance(int x, int y, int z) const {
        int index = x + y * BRICK_SIZE + z * BRICK_SIZE * BRICK_SIZE;
        return distanceField[index];
    }

    void SetDistance(int x, int y, int z, float value) {
        int index = x + y * BRICK_SIZE + z * BRICK_SIZE * BRICK_SIZE;
        distanceField[index] = value;
        isDirty = true;
    }

    [[nodiscard]] float Sample(const glm::vec3& localPos) const;
    [[nodiscard]] uint64_t ComputeHash() const;
};

/**
 * @brief Brick map settings
 */
struct BrickMapSettings {
    int brickResolution = 8;        // Voxels per brick dimension
    float worldVoxelSize = 0.1f;    // World space size of each voxel
    bool enableCompression = true;   // Compress duplicate bricks
    bool enableStreaming = false;    // Stream bricks on demand
    int maxCachedBricks = 4096;     // Maximum bricks to keep in memory
    float updateThreshold = 0.01f;  // Distance change threshold for updates
};

/**
 * @brief Brick map statistics
 */
struct BrickMapStats {
    int totalBricks = 0;
    int uniqueBricks = 0;          // After compression
    int compressedBricks = 0;
    int dirtyBricks = 0;
    size_t memoryBytes = 0;
    size_t memoryBytesCompressed = 0;
    float compressionRatio = 1.0f;
    double buildTimeMs = 0.0;
};

/**
 * @brief Brick index for spatial lookup
 */
struct BrickIndex {
    int x, y, z;

    bool operator==(const BrickIndex& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct BrickIndexHash {
    std::size_t operator()(const BrickIndex& idx) const {
        return std::hash<int>()(idx.x) ^
               (std::hash<int>()(idx.y) << 1) ^
               (std::hash<int>()(idx.z) << 2);
    }
};

/**
 * @brief Distance Field Brick Map for SDF caching
 *
 * Features:
 * - Pre-computed distance fields in 3D texture blocks (bricks)
 * - Compression of duplicate bricks
 * - Streaming for large models
 * - Incremental updates for dynamic SDFs
 * - GPU-friendly brick layout
 */
class SDFBrickMap {
public:
    SDFBrickMap();
    ~SDFBrickMap();

    // Non-copyable, movable
    SDFBrickMap(const SDFBrickMap&) = delete;
    SDFBrickMap& operator=(const SDFBrickMap&) = delete;
    SDFBrickMap(SDFBrickMap&&) noexcept = default;
    SDFBrickMap& operator=(SDFBrickMap&&) noexcept = default;

    // =========================================================================
    // Building
    // =========================================================================

    /**
     * @brief Build brick map from SDF model
     */
    void Build(const SDFModel& model, const BrickMapSettings& settings = {});

    /**
     * @brief Build from SDF function
     */
    void Build(const std::function<float(const glm::vec3&)>& sdfFunc,
               const glm::vec3& boundsMin,
               const glm::vec3& boundsMax,
               const BrickMapSettings& settings = {});

    /**
     * @brief Update dirty bricks (after SDF modification)
     */
    void UpdateDirtyBricks(const std::function<float(const glm::vec3&)>& sdfFunc);

    /**
     * @brief Mark region as dirty (needs rebuild)
     */
    void MarkRegionDirty(const glm::vec3& min, const glm::vec3& max);

    /**
     * @brief Compress duplicate bricks
     */
    void CompressBricks();

    /**
     * @brief Clear all data
     */
    void Clear();

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Sample distance field at world position (trilinear interpolation)
     */
    [[nodiscard]] float SampleDistance(const glm::vec3& worldPos) const;

    /**
     * @brief Get brick at world position
     */
    [[nodiscard]] const BrickData* GetBrickAt(const glm::vec3& worldPos) const;

    /**
     * @brief Get brick by index
     */
    [[nodiscard]] const BrickData* GetBrick(const BrickIndex& index) const;

    /**
     * @brief Check if position is cached
     */
    [[nodiscard]] bool IsCached(const glm::vec3& worldPos) const;

    // =========================================================================
    // GPU Synchronization
    // =========================================================================

    /**
     * @brief Upload brick map to GPU (3D texture array)
     */
    unsigned int UploadToGPU();

    /**
     * @brief Upload as buffer (for random access)
     */
    unsigned int UploadToGPUBuffer();

    /**
     * @brief Update GPU data for dirty bricks only
     */
    void UpdateGPU();

    /**
     * @brief Get GPU texture handle
     */
    [[nodiscard]] unsigned int GetGPUTexture() const { return m_gpuTexture; }

    /**
     * @brief Get GPU buffer handle
     */
    [[nodiscard]] unsigned int GetGPUBuffer() const { return m_gpuBuffer; }

    /**
     * @brief Get GPU brick index texture (maps world space to brick IDs)
     */
    [[nodiscard]] unsigned int GetGPUIndexTexture() const { return m_gpuIndexTexture; }

    /**
     * @brief Check if GPU data is valid
     */
    [[nodiscard]] bool IsGPUValid() const { return m_gpuValid; }

    /**
     * @brief Invalidate GPU data
     */
    void InvalidateGPU() { m_gpuValid = false; }

    // =========================================================================
    // Access
    // =========================================================================

    /**
     * @brief Get all bricks
     */
    [[nodiscard]] const std::unordered_map<BrickIndex, BrickData, BrickIndexHash>&
        GetBricks() const { return m_bricks; }

    /**
     * @brief Get bounds
     */
    [[nodiscard]] const glm::vec3& GetBoundsMin() const { return m_boundsMin; }
    [[nodiscard]] const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

    /**
     * @brief Get settings
     */
    [[nodiscard]] const BrickMapSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Get statistics
     */
    [[nodiscard]] const BrickMapStats& GetStats() const { return m_stats; }

    /**
     * @brief Check if built
     */
    [[nodiscard]] bool IsBuilt() const { return !m_bricks.empty(); }

    /**
     * @brief Get memory usage
     */
    [[nodiscard]] size_t GetMemoryUsage() const;

private:
    // Build helpers
    void AllocateBricks(const glm::vec3& boundsMin, const glm::vec3& boundsMax);

    void FillBrick(BrickData& brick,
                   const std::function<float(const glm::vec3&)>& sdfFunc);

    BrickIndex WorldToBrickIndex(const glm::vec3& worldPos) const;
    glm::vec3 BrickIndexToWorld(const BrickIndex& index) const;

    glm::vec3 WorldToLocalBrick(const glm::vec3& worldPos, const BrickData& brick) const;

    // Compression
    uint64_t ComputeBrickHash(const BrickData& brick) const;
    bool BricksAreSimilar(const BrickData& a, const BrickData& b, float threshold) const;

    // Statistics
    void ComputeStats();

    // Data
    std::unordered_map<BrickIndex, BrickData, BrickIndexHash> m_bricks;
    std::unordered_map<uint64_t, uint32_t> m_compressionMap;  // Hash -> canonical brick ID

    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{1.0f};
    glm::ivec3 m_brickGridSize{0};  // Number of bricks along each axis

    BrickMapSettings m_settings;
    BrickMapStats m_stats;

    // GPU data
    unsigned int m_gpuTexture = 0;       // 3D texture array of bricks
    unsigned int m_gpuBuffer = 0;        // Buffer of brick data
    unsigned int m_gpuIndexTexture = 0;  // 3D texture mapping world to brick IDs
    bool m_gpuValid = false;

    // Streaming
    std::vector<BrickIndex> m_streamQueue;
};

/**
 * @brief Utility functions for brick maps
 */
namespace BrickMapUtil {
    /**
     * @brief Trilinear interpolation in brick
     */
    [[nodiscard]] float TrilinearSample(const BrickData& brick,
                                         const glm::vec3& localPos);

    /**
     * @brief Compute brick world bounds
     */
    void ComputeBrickBounds(const BrickIndex& index,
                            float brickWorldSize,
                            const glm::vec3& gridOrigin,
                            glm::vec3& outMin, glm::vec3& outMax);

    /**
     * @brief Check if two bricks are similar (for compression)
     */
    [[nodiscard]] bool CompareBricks(const BrickData& a, const BrickData& b,
                                      float threshold);

    /**
     * @brief Estimate optimal brick size for SDF
     */
    [[nodiscard]] float EstimateOptimalBrickSize(const SDFModel& model);
}

} // namespace Nova
