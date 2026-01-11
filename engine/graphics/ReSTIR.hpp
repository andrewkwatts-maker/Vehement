#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Nova {

// Forward declarations
class Shader;
class Camera;
class ClusteredLightManager;

/**
 * @brief GPU-aligned Reservoir structure for ReSTIR
 *
 * Stores the result of weighted reservoir sampling for importance resampling.
 * Each pixel maintains a reservoir that tracks the best light sample.
 */
struct alignas(16) Reservoir {
    int lightIndex = -1;     // Index of selected light sample
    float weightSum = 0.0f;  // Sum of weights (used during updates)
    float W = 0.0f;          // Final normalization weight W = (weightSum/M) / p_hat(y)
    int M = 0;               // Number of samples seen/combined

    // Padding to 16-byte alignment
    glm::vec3 padding;
};

/**
 * @brief ReSTIR Global Illumination System
 *
 * Implements Reservoir-based Spatio-Temporal Importance Resampling (ReSTIR)
 * for real-time global illumination. ReSTIR enables rendering with 1 sample
 * per pixel that looks like 1000+ samples through intelligent resampling.
 *
 * Algorithm Overview:
 * 1. Initial Sampling: Generate candidate light samples using RIS (Resampled Importance Sampling)
 * 2. Temporal Reuse: Merge with previous frame's reservoirs (20x sample reuse)
 * 3. Spatial Reuse: Share samples with neighboring pixels (5-10x more samples)
 * 4. Final Shading: Evaluate the selected samples with proper MIS weights
 *
 * Performance Target: <2.0ms for full pipeline at 1920x1080
 *
 * References:
 * - "Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting" (SIGGRAPH 2020)
 * - "ReSTIR GI: Path Resampling for Real-Time Path Tracing" (HPG 2021)
 */
class ReSTIRGI {
public:
    ReSTIRGI();
    ~ReSTIRGI();

    // Non-copyable
    ReSTIRGI(const ReSTIRGI&) = delete;
    ReSTIRGI& operator=(const ReSTIRGI&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize ReSTIR system
     * @param width Viewport width
     * @param height Viewport height
     */
    bool Initialize(int width, int height);

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Resize for new viewport dimensions
     */
    void Resize(int width, int height);

    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Rendering Pipeline
    // =========================================================================

    /**
     * @brief Execute full ReSTIR pipeline
     * @param camera Current frame camera
     * @param lightManager Clustered light manager for light data
     * @param gBufferPosition G-buffer with world positions (RGB32F)
     * @param gBufferNormal G-buffer with world normals (RGB16F)
     * @param gBufferAlbedo G-buffer with albedo (RGBA8)
     * @param gBufferDepth G-buffer with linear depth (R32F)
     * @param motionVectors Motion vectors for temporal reprojection (RG16F)
     * @param outputTexture Output color texture (RGBA16F)
     */
    void Execute(const Camera& camera,
                ClusteredLightManager& lightManager,
                uint32_t gBufferPosition,
                uint32_t gBufferNormal,
                uint32_t gBufferAlbedo,
                uint32_t gBufferDepth,
                uint32_t motionVectors,
                uint32_t outputTexture);

    /**
     * @brief Generate initial light samples using RIS
     */
    void GenerateInitialSamples(const Camera& camera,
                               ClusteredLightManager& lightManager,
                               uint32_t gBufferPosition,
                               uint32_t gBufferNormal,
                               uint32_t gBufferAlbedo);

    /**
     * @brief Temporal reuse - merge with previous frame's reservoirs
     */
    void TemporalReuse(uint32_t gBufferPosition,
                      uint32_t gBufferNormal,
                      uint32_t motionVectors);

    /**
     * @brief Spatial reuse - share samples with neighbors
     */
    void SpatialReuse(uint32_t gBufferPosition,
                     uint32_t gBufferNormal,
                     uint32_t gBufferDepth);

    /**
     * @brief Final shading pass - evaluate selected samples
     */
    void FinalShading(ClusteredLightManager& lightManager,
                     uint32_t gBufferPosition,
                     uint32_t gBufferNormal,
                     uint32_t gBufferAlbedo,
                     uint32_t outputTexture);

    // =========================================================================
    // Configuration
    // =========================================================================

    struct Settings {
        // Initial sampling
        int initialCandidates = 32;         // Number of light candidates to test per pixel
        bool useRIS = true;                 // Use Resampled Importance Sampling

        // Temporal reuse
        bool temporalReuse = true;          // Enable temporal reuse
        float temporalMaxM = 20.0f;         // Maximum M cap for temporal history
        float temporalDepthThreshold = 0.1f; // Depth similarity threshold
        float temporalNormalThreshold = 0.9f; // Normal similarity threshold (dot product)

        // Spatial reuse
        int spatialIterations = 3;          // Number of spatial reuse passes (1-4)
        float spatialRadius = 30.0f;        // Search radius in pixels
        int spatialSamples = 5;             // Samples per spatial iteration
        bool spatialDiscardHistory = false; // Discard M history in spatial reuse

        // Bias reduction
        bool biasCorrection = true;         // Enable visibility/MIS bias correction
        float biasRayOffset = 0.001f;       // Ray offset for shadow rays

        // Performance
        bool halfResolution = false;        // Render at half resolution
        bool checkerboard = false;          // Checkerboard rendering
        bool asyncCompute = false;          // Use async compute queues (if available)
    };

    void SetSettings(const Settings& settings) { m_settings = settings; }
    const Settings& GetSettings() const { return m_settings; }

    // =========================================================================
    // Statistics
    // =========================================================================

    struct Stats {
        float initialSamplingMs = 0.0f;
        float temporalReuseMs = 0.0f;
        float spatialReuseMs = 0.0f;
        float finalShadingMs = 0.0f;
        float totalMs = 0.0f;

        uint32_t avgSamplesPerPixel = 0;
        float temporalReuseRate = 0.0f;  // % of pixels that reused temporal samples
        float avgMValue = 0.0f;          // Average M value across pixels
    };

    const Stats& GetStats() const { return m_stats; }

    /**
     * @brief Enable performance profiling
     */
    void SetProfilingEnabled(bool enabled) { m_profilingEnabled = enabled; }

private:
    // Initialize GPU resources
    bool InitializeBuffers();
    bool InitializeShaders();
    void CleanupBuffers();

    // Helper functions
    void BeginProfile(const char* label);
    void EndProfile(float& outTimeMs);
    void SwapReservoirBuffers();

    bool m_initialized = false;

    // Viewport
    int m_width = 0;
    int m_height = 0;

    // Settings
    Settings m_settings;

    // Frame counter for RNG seed
    uint32_t m_frameCount = 0;

    // GPU Buffers
    uint32_t m_reservoirBuffer[2] = {0, 0};  // Double buffered reservoirs
    int m_currentReservoir = 0;               // Current read buffer (0 or 1)

    // Compute shaders
    std::unique_ptr<Shader> m_initialSamplingShader;
    std::unique_ptr<Shader> m_temporalReuseShader;
    std::unique_ptr<Shader> m_spatialReuseShader;
    std::unique_ptr<Shader> m_finalShadingShader;

    // Performance tracking
    bool m_profilingEnabled = false;
    uint32_t m_queryObjects[8] = {0};  // GPU timer queries
    Stats m_stats;
};

} // namespace Nova
