#pragma once

/**
 * @file RTXAccelerationStructure.hpp
 * @brief Hardware-accelerated ray tracing acceleration structures (BLAS/TLAS)
 *
 * Manages bottom-level (BLAS) and top-level (TLAS) acceleration structures
 * for hardware ray tracing. These structures enable 10-100x faster ray-scene
 * intersection tests compared to software BVH.
 *
 * Key Features:
 * - BLAS for geometry (triangle meshes converted from SDFs)
 * - TLAS for instancing (scene graph with transforms)
 * - Fast updates for dynamic objects (refit vs rebuild)
 * - Compaction to reduce memory usage
 * - Multi-threaded building
 */

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace Nova {

// Forward declarations
class SDFModel;
class Mesh;

/**
 * @brief Acceleration structure build quality
 */
enum class ASBuildQuality {
    Fast,           // Prioritize build speed (for dynamic objects)
    Balanced,       // Balance build speed and trace performance
    HighQuality,    // Prioritize trace performance (for static objects)
};

/**
 * @brief Acceleration structure build flags
 */
enum ASBuildFlags : uint32_t {
    AS_BUILD_FLAG_NONE = 0,
    AS_BUILD_FLAG_ALLOW_UPDATE = 1 << 0,        // Allow future updates
    AS_BUILD_FLAG_ALLOW_COMPACTION = 1 << 1,    // Allow compaction
    AS_BUILD_FLAG_PREFER_FAST_TRACE = 1 << 2,   // Optimize for tracing
    AS_BUILD_FLAG_PREFER_FAST_BUILD = 1 << 3,   // Optimize for building
    AS_BUILD_FLAG_LOW_MEMORY = 1 << 4,          // Minimize memory usage
};

/**
 * @brief Geometry type in acceleration structure
 */
enum class ASGeometryType {
    Triangles,      // Triangle mesh
    AABBs,          // Axis-aligned bounding boxes (for procedural geometry)
};

/**
 * @brief Instance flags
 */
enum ASInstanceFlags : uint32_t {
    AS_INSTANCE_FLAG_NONE = 0,
    AS_INSTANCE_FLAG_DISABLE_CULL = 1 << 0,
    AS_INSTANCE_FLAG_FLIP_FACING = 1 << 1,
    AS_INSTANCE_FLAG_FORCE_OPAQUE = 1 << 2,
    AS_INSTANCE_FLAG_FORCE_NO_OPAQUE = 1 << 3,
};

/**
 * @brief Bottom-Level Acceleration Structure (BLAS) descriptor
 */
struct BLASDescriptor {
    // Geometry data
    uint32_t vertexBuffer = 0;      // OpenGL buffer ID
    uint32_t indexBuffer = 0;       // OpenGL buffer ID (optional)
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    uint32_t vertexStride = 0;      // Bytes between vertices (e.g., 12 for vec3)
    uint32_t vertexOffset = 0;      // Offset in vertex buffer

    ASGeometryType geometryType = ASGeometryType::Triangles;

    // Build settings
    ASBuildQuality buildQuality = ASBuildQuality::Balanced;
    uint32_t buildFlags = AS_BUILD_FLAG_ALLOW_COMPACTION;

    // Optional: material ID
    uint32_t materialID = 0;

    // Debug name
    std::string debugName;
};

/**
 * @brief Top-Level Acceleration Structure (TLAS) instance
 */
struct TLASInstance {
    // Transform (3x4 matrix: row-major, transposed for GPU)
    float transform[12];

    // Instance data
    uint32_t instanceCustomIndex = 0;       // Custom index for shader access
    uint32_t mask = 0xFF;                   // Visibility mask (8 bits)
    uint32_t instanceShaderBindingTableRecordOffset = 0;
    uint32_t flags = AS_INSTANCE_FLAG_NONE;

    // Reference to BLAS
    uint64_t blasHandle = 0;

    /**
     * @brief Set transform from glm::mat4
     */
    void SetTransform(const glm::mat4& mat);

    /**
     * @brief Get transform as glm::mat4
     */
    [[nodiscard]] glm::mat4 GetTransform() const;
};

/**
 * @brief BLAS (Bottom-Level Acceleration Structure) handle
 */
struct BLAS {
    uint64_t handle = 0;                // GPU handle
    uint32_t buffer = 0;                // OpenGL buffer storing the BLAS
    size_t size = 0;                    // Size in bytes
    uint32_t scratchBuffer = 0;         // Temporary buffer for building
    size_t scratchSize = 0;

    // Source geometry
    uint32_t vertexBuffer = 0;
    uint32_t indexBuffer = 0;
    uint32_t triangleCount = 0;

    // Build settings
    ASBuildQuality buildQuality = ASBuildQuality::Balanced;
    bool allowUpdate = false;
    bool compacted = false;

    std::string debugName;

    [[nodiscard]] bool IsValid() const { return handle != 0; }
};

/**
 * @brief TLAS (Top-Level Acceleration Structure) handle
 */
struct TLAS {
    uint64_t handle = 0;                // GPU handle
    uint32_t buffer = 0;                // OpenGL buffer storing the TLAS
    size_t size = 0;                    // Size in bytes
    uint32_t scratchBuffer = 0;         // Temporary buffer for building
    size_t scratchSize = 0;

    uint32_t instanceBuffer = 0;        // Buffer storing instances
    uint32_t instanceCount = 0;

    // Build settings
    bool allowUpdate = false;

    std::string debugName;

    [[nodiscard]] bool IsValid() const { return handle != 0; }
};

/**
 * @brief Acceleration structure build statistics
 */
struct ASBuildStats {
    // Timing
    double buildTimeMs = 0.0;
    double compactionTimeMs = 0.0;
    double updateTimeMs = 0.0;

    // Memory
    size_t originalSize = 0;
    size_t compactedSize = 0;
    size_t scratchSize = 0;

    // Geometry
    uint32_t triangleCount = 0;
    uint32_t instanceCount = 0;

    [[nodiscard]] double GetCompressionRatio() const {
        if (originalSize == 0) return 1.0;
        return static_cast<double>(compactedSize) / static_cast<double>(originalSize);
    }

    [[nodiscard]] std::string ToString() const;
};

/**
 * @brief RTX Acceleration Structure Manager
 *
 * Manages BLAS and TLAS for hardware ray tracing.
 * Handles building, updating, and compacting acceleration structures.
 */
class RTXAccelerationStructure {
public:
    RTXAccelerationStructure();
    ~RTXAccelerationStructure();

    // Non-copyable
    RTXAccelerationStructure(const RTXAccelerationStructure&) = delete;
    RTXAccelerationStructure& operator=(const RTXAccelerationStructure&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize acceleration structure manager
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // BLAS Management
    // =========================================================================

    /**
     * @brief Build BLAS from mesh
     * @return BLAS handle for use in TLAS
     */
    uint64_t BuildBLAS(const BLASDescriptor& desc);

    /**
     * @brief Build BLAS from SDF model (converts to mesh first)
     */
    uint64_t BuildBLASFromSDF(const SDFModel& model, float voxelSize = 0.1f);

    /**
     * @brief Build multiple BLAS in batch (more efficient)
     */
    std::vector<uint64_t> BuildBLASBatch(const std::vector<BLASDescriptor>& descriptors);

    /**
     * @brief Update existing BLAS (for dynamic geometry)
     * Must have been built with AS_BUILD_FLAG_ALLOW_UPDATE
     */
    bool UpdateBLAS(uint64_t blasHandle, const BLASDescriptor& desc);

    /**
     * @brief Refit BLAS (update bounds only, faster than full update)
     * Use for deforming geometry with same topology
     */
    bool RefitBLAS(uint64_t blasHandle);

    /**
     * @brief Compact BLAS to reduce memory usage
     * Returns new handle (old handle becomes invalid)
     */
    uint64_t CompactBLAS(uint64_t blasHandle);

    /**
     * @brief Destroy BLAS and free memory
     */
    void DestroyBLAS(uint64_t blasHandle);

    /**
     * @brief Get BLAS by handle
     */
    [[nodiscard]] const BLAS* GetBLAS(uint64_t handle) const;

    // =========================================================================
    // TLAS Management
    // =========================================================================

    /**
     * @brief Build TLAS from instances
     */
    uint64_t BuildTLAS(const std::vector<TLASInstance>& instances,
                       const std::string& debugName = "TLAS");

    /**
     * @brief Update existing TLAS with new instances
     * Faster than rebuilding from scratch
     */
    bool UpdateTLAS(uint64_t tlasHandle, const std::vector<TLASInstance>& instances);

    /**
     * @brief Update TLAS transforms only (fastest update)
     * Use when only transforms change, not instance count
     */
    bool UpdateTLASTransforms(uint64_t tlasHandle, const std::vector<glm::mat4>& transforms);

    /**
     * @brief Destroy TLAS and free memory
     */
    void DestroyTLAS(uint64_t tlasHandle);

    /**
     * @brief Get TLAS by handle
     */
    [[nodiscard]] const TLAS* GetTLAS(uint64_t handle) const;

    /**
     * @brief Get TLAS GPU buffer ID for shader binding
     */
    [[nodiscard]] uint32_t GetTLASBuffer(uint64_t handle) const;

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Create mesh from SDF model (for BLAS creation)
     * Uses marching cubes or dual contouring
     */
    std::shared_ptr<Mesh> ConvertSDFToMesh(const SDFModel& model, float voxelSize = 0.1f);

    /**
     * @brief Get build statistics
     */
    [[nodiscard]] const ASBuildStats& GetStats() const { return m_stats; }

    /**
     * @brief Reset statistics
     */
    void ResetStats() { m_stats = ASBuildStats{}; }

    /**
     * @brief Get total memory usage
     */
    [[nodiscard]] size_t GetTotalMemoryUsage() const;

    /**
     * @brief Get BLAS count
     */
    [[nodiscard]] size_t GetBLASCount() const { return m_blasList.size(); }

    /**
     * @brief Get TLAS count
     */
    [[nodiscard]] size_t GetTLASCount() const { return m_tlasList.size(); }

    /**
     * @brief Log statistics
     */
    void LogStats() const;

private:
    // Internal helpers
    uint32_t CreateBuffer(size_t size, const void* data = nullptr);
    void DestroyBuffer(uint32_t buffer);
    uint64_t GetNextBLASHandle();
    uint64_t GetNextTLASHandle();

    // Build helpers
    void BuildBLASInternal(BLAS& blas, const BLASDescriptor& desc);
    void BuildTLASInternal(TLAS& tlas, const std::vector<TLASInstance>& instances);
    void CompactAccelerationStructure(BLAS& blas);

    bool m_initialized = false;

    // BLAS storage
    std::vector<BLAS> m_blasList;
    uint64_t m_nextBLASHandle = 1;

    // TLAS storage
    std::vector<TLAS> m_tlasList;
    uint64_t m_nextTLASHandle = 1;

    // Statistics
    ASBuildStats m_stats;

    // Cache for SDF->Mesh conversion
    struct SDFMeshCache {
        const SDFModel* model;
        float voxelSize;
        std::shared_ptr<Mesh> mesh;
    };
    std::vector<SDFMeshCache> m_sdfMeshCache;
};

/**
 * @brief Helper to create TLAS instance from transform
 */
TLASInstance CreateTLASInstance(uint64_t blasHandle,
                                const glm::mat4& transform,
                                uint32_t customIndex = 0,
                                uint32_t mask = 0xFF);

} // namespace Nova
