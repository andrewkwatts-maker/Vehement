#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include "../math/Vector3.hpp"
#include "../math/Matrix4.hpp"
#include "ParallelCullingSystem.hpp"

namespace Engine {
namespace Graphics {

/**
 * @brief GPU indirect draw command structure (OpenGL layout)
 */
struct DrawElementsIndirectCommand {
    uint32_t vertexCount;      // Number of vertices to draw
    uint32_t instanceCount;    // Number of instances to draw
    uint32_t firstVertex;      // Index of first vertex
    uint32_t baseVertex;       // Base vertex offset
    uint32_t baseInstance;     // Base instance offset

    DrawElementsIndirectCommand()
        : vertexCount(0), instanceCount(0)
        , firstVertex(0), baseVertex(0), baseInstance(0) {}
};

/**
 * @brief GPU instance data structure
 */
struct GPUInstanceData {
    Matrix4 transform;           // 64 bytes
    Vector4 boundingSphere;      // center.xyz + radius (16 bytes)
    uint32_t materialID;         // 4 bytes
    uint32_t lodLevel;           // 4 bytes
    uint32_t instanceID;         // 4 bytes
    uint32_t flags;              // 4 bytes (padding)

    GPUInstanceData()
        : materialID(0), lodLevel(0), instanceID(0), flags(0) {}
};

/**
 * @brief GPU buffer object wrapper
 */
class GPUBuffer {
public:
    enum class Type {
        Vertex,
        Index,
        Uniform,
        ShaderStorage,
        Indirect,
        AtomicCounter
    };

    enum class Usage {
        Static,        // Data modified once, used many times
        Dynamic,       // Data modified repeatedly, used many times
        Stream         // Data modified once, used a few times
    };

    GPUBuffer(Type type, Usage usage);
    ~GPUBuffer();

    void Allocate(size_t size);
    void Upload(const void* data, size_t size, size_t offset = 0);
    void* Map(size_t offset = 0, size_t size = 0);
    void Unmap();
    void Bind() const;
    void BindBase(uint32_t bindingPoint) const;

    uint32_t GetHandle() const { return m_handle; }
    size_t GetSize() const { return m_size; }
    Type GetType() const { return m_type; }

private:
    uint32_t m_handle;
    Type m_type;
    Usage m_usage;
    size_t m_size;
    void* m_mappedPointer;
};

/**
 * @brief Compute shader wrapper
 */
class ComputeShader {
public:
    ComputeShader();
    ~ComputeShader();

    bool LoadFromFile(const char* path);
    bool LoadFromSource(const char* source);
    void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
    void DispatchIndirect(const GPUBuffer& commandBuffer, size_t offset = 0);

    void SetUniform(const char* name, int value);
    void SetUniform(const char* name, float value);
    void SetUniform(const char* name, const Vector3& value);
    void SetUniform(const char* name, const Matrix4& value);

    void Bind() const;
    uint32_t GetHandle() const { return m_program; }

private:
    uint32_t m_program;
    uint32_t m_shader;
};

/**
 * @brief GPU-driven rendering system
 * Performs culling and rendering entirely on GPU
 * Zero CPU-GPU synchronization per frame
 */
class GPUDrivenRenderer {
public:
    /**
     * @brief Configuration for GPU-driven rendering
     */
    struct Config {
        uint32_t maxInstances;           // Maximum number of instances
        uint32_t maxDrawCommands;        // Maximum number of draw commands
        bool enablePersistentMapping;    // Use persistent mapped buffers
        bool enableGPUCulling;           // Enable GPU-side culling
        bool enableOcclusionCulling;     // Enable occlusion culling
        uint32_t cullingThreadGroupSize; // Compute shader thread group size

        Config()
            : maxInstances(100000)
            , maxDrawCommands(1000)
            , enablePersistentMapping(true)
            , enableGPUCulling(true)
            , enableOcclusionCulling(false)
            , cullingThreadGroupSize(256) {}
    };

    explicit GPUDrivenRenderer(const Config& config = Config());
    ~GPUDrivenRenderer();

    /**
     * @brief Initialize GPU resources
     */
    bool Initialize();

    /**
     * @brief Update instance data
     * @param instances Array of instances to render
     */
    void UpdateInstances(const std::vector<SDFInstance>& instances);

    /**
     * @brief Perform GPU culling and generate draw commands
     * @param camera Camera for frustum culling
     */
    void CullAndGenerateDrawCommands(const CullingCamera& camera);

    /**
     * @brief Execute indirect draw calls
     */
    void ExecuteIndirectDraws();

    /**
     * @brief Execute indirect draw calls for specific material batch
     */
    void ExecuteIndirectDrawBatch(uint32_t batchIndex);

    /**
     * @brief Get number of draw commands generated
     */
    uint32_t GetDrawCommandCount() const { return m_drawCommandCount; }

    /**
     * @brief Get GPU culling time (query result)
     */
    float GetGPUCullingTimeMs() const { return m_gpuCullingTimeMs; }

    /**
     * @brief Get instance buffer (for custom rendering)
     */
    GPUBuffer* GetInstanceBuffer() { return m_instanceBuffer.get(); }

    /**
     * @brief Get visible instance buffer (post-culling)
     */
    GPUBuffer* GetVisibleInstanceBuffer() { return m_visibleInstanceBuffer.get(); }

    /**
     * @brief Get draw command buffer
     */
    GPUBuffer* GetDrawCommandBuffer() { return m_drawCommandBuffer.get(); }

    /**
     * @brief Performance statistics
     */
    struct Stats {
        uint32_t totalInstances;
        uint32_t visibleInstances;
        uint32_t drawCallCount;
        float gpuCullingTimeMs;
        float uploadTimeMs;
        size_t instanceBufferSize;
        size_t commandBufferSize;

        Stats()
            : totalInstances(0), visibleInstances(0), drawCallCount(0)
            , gpuCullingTimeMs(0), uploadTimeMs(0)
            , instanceBufferSize(0), commandBufferSize(0) {}
    };

    Stats GetStats() const { return m_stats; }
    void ResetStats();

    /**
     * @brief Compact instance buffer (remove invisible instances)
     * Called automatically after GPU culling
     */
    void CompactInstanceBuffer();

    /**
     * @brief Clear all instances
     */
    void Clear();

private:
    /**
     * @brief Create GPU buffers
     */
    bool CreateBuffers();

    /**
     * @brief Load compute shaders
     */
    bool LoadShaders();

    /**
     * @brief Update frustum planes uniform buffer
     */
    void UpdateFrustumPlanes(const Frustum& frustum);

    /**
     * @brief Read back GPU query results
     */
    void ReadQueryResults();

    Config m_config;

    // GPU Buffers
    std::unique_ptr<GPUBuffer> m_instanceBuffer;
    std::unique_ptr<GPUBuffer> m_visibleInstanceBuffer;
    std::unique_ptr<GPUBuffer> m_drawCommandBuffer;
    std::unique_ptr<GPUBuffer> m_frustumPlaneBuffer;
    std::unique_ptr<GPUBuffer> m_counterBuffer;

    // Compute Shaders
    std::unique_ptr<ComputeShader> m_cullingShader;
    std::unique_ptr<ComputeShader> m_compactionShader;

    // State
    uint32_t m_instanceCount;
    uint32_t m_drawCommandCount;
    std::vector<GPUInstanceData> m_instanceData;

    // Performance tracking
    Stats m_stats;
    uint32_t m_queryObject;
    float m_gpuCullingTimeMs;

    // Persistent mapping support
    void* m_persistentInstancePtr;
    void* m_persistentCommandPtr;

    // Frame synchronization
    uint32_t m_frameIndex;
    static constexpr uint32_t FRAME_BUFFER_COUNT = 3;
};

/**
 * @brief Multi-draw indirect renderer
 * Batches multiple indirect draws into single call
 */
class MultiDrawIndirectRenderer {
public:
    MultiDrawIndirectRenderer();
    ~MultiDrawIndirectRenderer();

    /**
     * @brief Add draw command to batch
     */
    void AddDrawCommand(const DrawElementsIndirectCommand& command);

    /**
     * @brief Execute all batched draw commands in single call
     */
    void ExecuteMultiDraw();

    /**
     * @brief Clear all draw commands
     */
    void Clear();

    /**
     * @brief Get draw command count
     */
    size_t GetDrawCommandCount() const { return m_drawCommands.size(); }

private:
    std::vector<DrawElementsIndirectCommand> m_drawCommands;
    std::unique_ptr<GPUBuffer> m_commandBuffer;
};

/**
 * @brief Occlusion culling support (Hi-Z buffer)
 */
class OcclusionCuller {
public:
    OcclusionCuller();
    ~OcclusionCuller();

    /**
     * @brief Initialize Hi-Z buffer
     */
    bool Initialize(uint32_t width, uint32_t height);

    /**
     * @brief Generate Hi-Z mipmap chain from depth buffer
     */
    void GenerateHiZ(uint32_t depthTexture);

    /**
     * @brief Perform occlusion culling on GPU
     */
    void CullOccluded(GPUBuffer* instanceBuffer,
                     GPUBuffer* visibleBuffer,
                     uint32_t instanceCount);

    /**
     * @brief Get Hi-Z texture handle
     */
    uint32_t GetHiZTexture() const { return m_hiZTexture; }

private:
    uint32_t m_hiZTexture;
    uint32_t m_hiZFBO;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;
    std::unique_ptr<ComputeShader> m_hiZShader;
    std::unique_ptr<ComputeShader> m_occlusionShader;
};

} // namespace Graphics
} // namespace Engine
