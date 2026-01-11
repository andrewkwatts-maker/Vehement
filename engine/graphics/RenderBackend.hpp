#pragma once

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Camera;
class Scene;
class Mesh;
class Material;
class Framebuffer;
class Texture;

/**
 * @brief Supported rendering features
 */
enum class RenderFeature {
    SDFRendering,           // Signed Distance Field rendering
    PolygonRendering,       // Traditional polygon rasterization
    HybridRendering,        // Combined SDF + polygon
    ComputeShaders,         // Compute shader support
    RTXRaytracing,          // Hardware raytracing (RTX)
    DepthInterleaving,      // Z-buffer merge between render passes
    TileBasedCulling,       // Tile-based frustum culling
    ClusteredLighting,      // Clustered forward lighting
    PBRShading,             // Physically Based Rendering
    ShadowMapping,          // Shadow maps
    AmbientOcclusion        // Screen-space AO
};

/**
 * @brief Quality settings for rendering
 */
struct QualitySettings {
    // Resolution
    int renderWidth = 1920;
    int renderHeight = 1080;

    // SDF settings
    int maxRaymarchSteps = 128;
    float sdfRayEpsilon = 0.001f;
    int sdfTileSize = 16;
    bool sdfEnableShadows = true;
    bool sdfEnableAO = true;
    float sdfAORadius = 0.5f;
    int sdfAOSamples = 4;

    // Polygon settings
    int shadowMapSize = 2048;
    int cascadeCount = 4;
    bool enableMSAA = false;
    int msaaSamples = 4;

    // Hybrid settings
    bool enableDepthInterleaving = true;
    enum class RenderOrder {
        SDFFirst,
        PolygonFirst,
        Auto
    } renderOrder = RenderOrder::SDFFirst;

    // Performance
    bool enableFrustumCulling = true;
    bool enableOcclusionCulling = false;
    bool enableLOD = true;

    // Debug
    bool showTiles = false;
    bool showDepthBuffer = false;
    bool showPerformanceOverlay = false;
};

/**
 * @brief Performance statistics for a render backend
 */
struct RenderStats {
    // Frame timing
    float frameTimeMs = 0.0f;
    float cpuTimeMs = 0.0f;
    float gpuTimeMs = 0.0f;
    int fps = 0;

    // Rendering stats
    uint32_t drawCalls = 0;
    uint32_t computeDispatches = 0;
    uint32_t trianglesRendered = 0;
    uint32_t sdfObjectsRendered = 0;
    uint32_t polygonObjectsRendered = 0;

    // Culling stats
    uint32_t tilesProcessed = 0;
    uint32_t tilesCulled = 0;
    uint32_t objectsCulled = 0;

    // Memory
    uint64_t vramUsedBytes = 0;
    uint64_t bufferMemoryBytes = 0;

    // Per-pass timing
    float sdfPassMs = 0.0f;
    float polygonPassMs = 0.0f;
    float depthMergeMs = 0.0f;
    float lightingMs = 0.0f;
    float postProcessMs = 0.0f;

    void Reset() {
        frameTimeMs = cpuTimeMs = gpuTimeMs = 0.0f;
        fps = 0;
        drawCalls = computeDispatches = trianglesRendered = 0;
        sdfObjectsRendered = polygonObjectsRendered = 0;
        tilesProcessed = tilesCulled = objectsCulled = 0;
        vramUsedBytes = bufferMemoryBytes = 0;
        sdfPassMs = polygonPassMs = depthMergeMs = 0.0f;
        lightingMs = postProcessMs = 0.0f;
    }
};

/**
 * @brief Abstract interface for different rendering backends
 *
 * Provides a unified interface for SDF-first, polygon-based, and hybrid
 * rendering approaches. Implementations can use compute shaders, RTX,
 * or traditional rasterization.
 */
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    /**
     * @brief Initialize the render backend
     * @param width Initial render target width
     * @param height Initial render target height
     * @return true on success
     */
    virtual bool Initialize(int width, int height) = 0;

    /**
     * @brief Shutdown and cleanup resources
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Resize render targets
     */
    virtual void Resize(int width, int height) = 0;

    /**
     * @brief Begin a new frame
     * @param camera Active camera for rendering
     */
    virtual void BeginFrame(const Camera& camera) = 0;

    /**
     * @brief End the current frame
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Render the scene
     * @param scene Scene to render
     * @param camera Camera view
     */
    virtual void Render(const Scene& scene, const Camera& camera) = 0;

    /**
     * @brief Set quality settings
     * @param settings Quality configuration
     */
    virtual void SetQualitySettings(const QualitySettings& settings) = 0;

    /**
     * @brief Get current quality settings
     */
    virtual const QualitySettings& GetQualitySettings() const = 0;

    /**
     * @brief Get performance statistics
     */
    virtual const RenderStats& GetStats() const = 0;

    /**
     * @brief Check if a feature is supported
     */
    virtual bool SupportsFeature(RenderFeature feature) const = 0;

    /**
     * @brief Get backend name for UI display
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief Get output color texture
     */
    virtual std::shared_ptr<Texture> GetOutputColor() const = 0;

    /**
     * @brief Get output depth texture
     */
    virtual std::shared_ptr<Texture> GetOutputDepth() const = 0;

    /**
     * @brief Enable/disable debug visualization
     */
    virtual void SetDebugMode(bool enabled) = 0;

protected:
    // Helper for timing
    class ScopedTimer {
    public:
        ScopedTimer(float& target) : m_target(target) {
            m_start = std::chrono::high_resolution_clock::now();
        }

        ~ScopedTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float, std::milli> duration = end - m_start;
            m_target = duration.count();
        }

    private:
        float& m_target;
        std::chrono::high_resolution_clock::time_point m_start;
    };
};

/**
 * @brief Factory for creating render backends
 */
class RenderBackendFactory {
public:
    enum class BackendType {
        SDF,        // Pure SDF raymarching
        Polygon,    // Traditional polygon rasterization
        Hybrid      // Combined SDF + polygon
    };

    /**
     * @brief Create a render backend of specified type
     * @param type Backend type to create
     * @return Unique pointer to backend, or nullptr on failure
     */
    static std::unique_ptr<IRenderBackend> Create(BackendType type);

    /**
     * @brief Get available backend types on this system
     * @return List of supported backends
     */
    static std::vector<BackendType> GetAvailableBackends();

    /**
     * @brief Get backend type name for UI
     */
    static const char* GetBackendName(BackendType type);

    /**
     * @brief Check if a backend type is available
     */
    static bool IsBackendAvailable(BackendType type);
};

} // namespace Nova
