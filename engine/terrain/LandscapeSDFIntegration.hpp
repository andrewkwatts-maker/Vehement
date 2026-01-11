#pragma once

#include <memory>
#include <vector>
#include "../math/Vector3.hpp"
#include "../math/Matrix4.hpp"
#include "../graphics/GPUDrivenRenderer.hpp"

namespace Engine {
namespace Terrain {

/**
 * @brief Hierarchical Z-buffer for occlusion culling
 */
class HiZBuffer {
public:
    HiZBuffer(uint32_t width, uint32_t height);
    ~HiZBuffer();

    void GenerateFromDepth(uint32_t depthTexture);
    uint32_t GetTexture() const { return m_texture; }
    uint32_t GetMipLevels() const { return m_mipLevels; }

private:
    uint32_t m_texture;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevels;
};

/**
 * @brief Landscape/terrain SDF integration
 * Renders terrain with heightmap, SDFs on top with proper depth testing
 */
class LandscapeSDFIntegration {
public:
    struct Config {
        uint32_t terrainResolution;
        float terrainSize;
        bool enableOcclusionCulling;
        bool enableShadows;
        uint32_t hiZResolution;

        Config()
            : terrainResolution(1024)
            , terrainSize(1000.0f)
            , enableOcclusionCulling(true)
            , enableShadows(true)
            , hiZResolution(1024) {}
    };

    explicit LandscapeSDFIntegration(const Config& config = Config());
    ~LandscapeSDFIntegration();

    bool Initialize();

    /**
     * @brief Render terrain (heightmap-based)
     */
    void RenderTerrain(const Matrix4& viewMatrix, const Matrix4& projMatrix);

    /**
     * @brief Generate Hi-Z buffer from terrain depth
     */
    void GenerateHiZ();

    /**
     * @brief Cull SDFs using terrain occlusion
     */
    void CullSDFsWithTerrain(
        const std::vector<Graphics::SDFInstance>& instances,
        std::vector<uint32_t>& visibleIndices
    );

    /**
     * @brief Render SDFs with terrain depth testing
     */
    void RenderSDFsWithDepthTest(
        uint32_t sdfShader,
        const Matrix4& viewMatrix,
        const Matrix4& projMatrix
    );

    /**
     * @brief Get terrain depth texture
     */
    uint32_t GetTerrainDepth() const { return m_terrainDepthTexture; }

    /**
     * @brief Get Hi-Z buffer
     */
    HiZBuffer* GetHiZ() { return m_hiZ.get(); }

    /**
     * @brief Performance statistics
     */
    struct Stats {
        float terrainRenderTimeMs;
        float hiZGenerationTimeMs;
        float occlusionCullingTimeMs;
        uint32_t sdfsCulledByTerrain;

        Stats() : terrainRenderTimeMs(0), hiZGenerationTimeMs(0),
                  occlusionCullingTimeMs(0), sdfsCulledByTerrain(0) {}
    };

    Stats GetStats() const { return m_stats; }

private:
    Config m_config;

    // Terrain rendering
    uint32_t m_terrainVAO;
    uint32_t m_terrainVBO;
    uint32_t m_terrainIBO;
    uint32_t m_terrainShader;
    uint32_t m_heightmapTexture;

    // Terrain depth
    uint32_t m_terrainFBO;
    uint32_t m_terrainDepthTexture;
    uint32_t m_terrainColorTexture;

    // Hi-Z buffer
    std::unique_ptr<HiZBuffer> m_hiZ;

    // Occlusion culling
    std::unique_ptr<Graphics::ComputeShader> m_occlusionCullShader;

    Stats m_stats;
};

} // namespace Terrain
} // namespace Engine
