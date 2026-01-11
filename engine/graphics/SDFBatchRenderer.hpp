#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "GPUDrivenRenderer.hpp"
#include "ParallelCullingSystem.hpp"

namespace Engine {
namespace Graphics {

/**
 * @brief Material batch for SDF rendering
 */
struct SDFMaterialBatch {
    uint32_t materialID;
    uint32_t shaderProgram;
    std::vector<uint32_t> instanceIndices;
    DrawElementsIndirectCommand drawCommand;

    SDFMaterialBatch() : materialID(0), shaderProgram(0) {}
};

/**
 * @brief Shader state for rendering
 */
struct ShaderState {
    uint32_t program;
    uint32_t blendMode;
    uint32_t depthMode;
    bool alphaTesting;

    ShaderState() : program(0), blendMode(0), depthMode(0), alphaTesting(false) {}

    bool operator==(const ShaderState& other) const {
        return program == other.program && blendMode == other.blendMode &&
               depthMode == other.depthMode && alphaTesting == other.alphaTesting;
    }
};

/**
 * @brief SDF batch renderer for 10,000+ instances
 * Minimizes draw calls through batching and state sorting
 */
class SDFBatchRenderer {
public:
    struct Config {
        uint32_t maxBatches;
        uint32_t maxInstancesPerBatch;
        bool enableStateSorting;
        bool enableFrustumCulling;
        bool enableInstancing;

        Config()
            : maxBatches(1000)
            , maxInstancesPerBatch(10000)
            , enableStateSorting(true)
            , enableFrustumCulling(true)
            , enableInstancing(true) {}
    };

    explicit SDFBatchRenderer(const Config& config = Config());
    ~SDFBatchRenderer();

    bool Initialize();

    /**
     * @brief Add SDF instance to batch
     */
    void AddInstance(const SDFInstance& instance);

    /**
     * @brief Build batches from instances
     */
    void BuildBatches();

    /**
     * @brief Render all batches
     */
    void RenderBatches(const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Render specific batch
     */
    void RenderBatch(uint32_t batchIndex, const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Clear all instances and batches
     */
    void Clear();

    /**
     * @brief Get batch count
     */
    uint32_t GetBatchCount() const { return static_cast<uint32_t>(m_batches.size()); }

    /**
     * @brief Performance statistics
     */
    struct Stats {
        uint32_t totalInstances;
        uint32_t totalBatches;
        uint32_t drawCallCount;
        uint32_t stateChanges;
        float batchingTimeMs;
        float renderTimeMs;

        Stats() : totalInstances(0), totalBatches(0), drawCallCount(0),
                  stateChanges(0), batchingTimeMs(0), renderTimeMs(0) {}
    };

    Stats GetStats() const { return m_stats; }
    void ResetStats();

private:
    void SortBatches();
    void ApplyShaderState(const ShaderState& state);

    Config m_config;
    std::vector<SDFInstance> m_instances;
    std::vector<SDFMaterialBatch> m_batches;
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_materialGroups;

    std::unique_ptr<GPUBuffer> m_batchedInstanceBuffer;
    std::unique_ptr<MultiDrawIndirectRenderer> m_multiDrawRenderer;

    Stats m_stats;
    ShaderState m_currentState;
};

} // namespace Graphics
} // namespace Engine
