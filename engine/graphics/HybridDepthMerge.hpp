#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <cstdint>

namespace Nova {

// Forward declarations
class Shader;
class Texture;

/**
 * @brief Depth merge mode
 */
enum class DepthMergeMode {
    SDFFirst,       // Render SDFs first, then polygons with SDF depth test
    PolygonFirst,   // Render polygons first, then SDFs with polygon depth test
    Interleaved     // Merge both depth buffers atomically
};

/**
 * @brief Hybrid depth merge for Z-buffer interleaving
 *
 * Handles merging depth buffers from SDF raymarching and polygon rasterization.
 * Supports multiple modes:
 * - SDF-first: SDF depth acts as early-Z for polygon pass
 * - Polygon-first: Polygon depth limits SDF raymarch distance
 * - Interleaved: Atomic min operations merge both depths
 *
 * Key features:
 * - Compute shader-based depth merge
 * - Atomic min for depth writes
 * - Conservative depth testing
 * - Proper Z-fighting resolution
 */
class HybridDepthMerge {
public:
    HybridDepthMerge();
    ~HybridDepthMerge();

    // Non-copyable
    HybridDepthMerge(const HybridDepthMerge&) = delete;
    HybridDepthMerge& operator=(const HybridDepthMerge&) = delete;

    /**
     * @brief Initialize the depth merge system
     */
    bool Initialize(int width, int height);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Resize depth buffers
     */
    void Resize(int width, int height);

    /**
     * @brief Prepare depth buffer for SDF pass
     * @param mode Depth merge mode
     */
    void PrepareSDFPass(DepthMergeMode mode);

    /**
     * @brief Prepare depth buffer for polygon pass
     * @param mode Depth merge mode
     */
    void PreparePolygonPass(DepthMergeMode mode);

    /**
     * @brief Merge SDF and polygon depth buffers
     * @param sdfDepth Depth texture from SDF pass
     * @param polygonDepth Depth texture from polygon pass
     * @param output Output merged depth texture
     */
    void MergeDepthBuffers(const std::shared_ptr<Texture>& sdfDepth,
                          const std::shared_ptr<Texture>& polygonDepth,
                          const std::shared_ptr<Texture>& output);

    /**
     * @brief Copy depth buffer with optional comparison
     * @param source Source depth texture
     * @param dest Destination depth texture
     * @param useMin If true, take minimum of source and dest
     */
    void CopyDepth(const std::shared_ptr<Texture>& source,
                   const std::shared_ptr<Texture>& dest,
                   bool useMin = false);

    /**
     * @brief Clear depth buffer to far plane
     * @param depth Depth texture to clear
     */
    void ClearDepth(const std::shared_ptr<Texture>& depth);

    /**
     * @brief Initialize depth buffer to far plane for raymarching
     * @param depth Depth texture to initialize
     * @param farPlane Camera far plane distance
     */
    void InitializeDepthForRaymarch(const std::shared_ptr<Texture>& depth, float farPlane);

    /**
     * @brief Set depth merge mode
     */
    void SetMode(DepthMergeMode mode) { m_mode = mode; }

    /**
     * @brief Get current depth merge mode
     */
    DepthMergeMode GetMode() const { return m_mode; }

    /**
     * @brief Enable/disable conservative depth testing
     */
    void SetConservativeDepth(bool enabled) { m_conservativeDepth = enabled; }

    /**
     * @brief Get depth bias for Z-fighting resolution
     */
    float GetDepthBias() const { return m_depthBias; }

    /**
     * @brief Set depth bias (default: 0.0001)
     */
    void SetDepthBias(float bias) { m_depthBias = bias; }

private:
    /**
     * @brief Create compute shaders
     */
    bool CreateShaders();

    /**
     * @brief Create GPU buffers
     */
    bool CreateBuffers();

    // State
    bool m_initialized = false;
    int m_width = 0;
    int m_height = 0;
    DepthMergeMode m_mode = DepthMergeMode::SDFFirst;
    bool m_conservativeDepth = true;
    float m_depthBias = 0.0001f;

    // Compute shaders
    std::shared_ptr<Shader> m_depthMergeShader;
    std::shared_ptr<Shader> m_depthCopyShader;
    std::shared_ptr<Shader> m_depthClearShader;
    std::shared_ptr<Shader> m_depthInitShader;

    // Temporary depth buffers for intermediate results
    std::shared_ptr<Texture> m_tempDepth;
};

} // namespace Nova
