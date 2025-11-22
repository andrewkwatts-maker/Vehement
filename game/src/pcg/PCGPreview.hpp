#pragma once

#include "PCGPipeline.hpp"
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace Vehement {

/**
 * @brief Preview quality levels
 */
enum class PreviewQuality {
    VeryLow,    // 1/8 resolution, minimal features
    Low,        // 1/4 resolution, basic features
    Medium,     // 1/2 resolution, most features
    High        // Full resolution, all features
};

/**
 * @brief Preview image format
 */
struct PreviewImage {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;       // RGBA format

    bool IsValid() const { return width > 0 && height > 0 && !pixels.empty(); }
    void Clear() { width = height = 0; pixels.clear(); }
    void Resize(int w, int h) {
        width = w;
        height = h;
        pixels.resize(w * h, 0xFF000000);
    }

    void SetPixel(int x, int y, uint32_t color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            pixels[y * width + x] = color;
        }
    }

    uint32_t GetPixel(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return pixels[y * width + x];
        }
        return 0;
    }
};

/**
 * @brief Preview generation result
 */
struct PreviewResult {
    bool success = false;
    bool cancelled = false;
    std::string errorMessage;
    float generationTime = 0.0f;
    PreviewImage image;
};

/**
 * @brief Fast preview generation for editor
 *
 * Features:
 * - Generate low-res preview for editor
 * - Async generation with progress callback
 * - Cancel support
 * - Multiple quality levels
 * - Color-coded tile visualization
 */
class PCGPreview {
public:
    PCGPreview();
    ~PCGPreview();

    // Non-copyable
    PCGPreview(const PCGPreview&) = delete;
    PCGPreview& operator=(const PCGPreview&) = delete;

    /**
     * @brief Set preview quality
     */
    void SetQuality(PreviewQuality quality);
    PreviewQuality GetQuality() const { return m_quality; }

    /**
     * @brief Set preview dimensions (overrides quality-based scaling)
     */
    void SetDimensions(int width, int height);

    /**
     * @brief Enable/disable specific features in preview
     */
    void SetFeatureEnabled(PCGStage stage, bool enabled);
    bool IsFeatureEnabled(PCGStage stage) const;

    // ========== Synchronous Generation ==========

    /**
     * @brief Generate preview synchronously
     * @param pipeline Pipeline to generate from
     * @return Preview result
     */
    PreviewResult Generate(PCGPipeline& pipeline);

    /**
     * @brief Generate preview from context
     * @param context Existing context to visualize
     * @return Preview result
     */
    PreviewResult GenerateFromContext(const PCGContext& context);

    // ========== Asynchronous Generation ==========

    /**
     * @brief Progress callback type
     */
    using ProgressCallback = std::function<void(float progress, const std::string& stage)>;

    /**
     * @brief Completion callback type
     */
    using CompletionCallback = std::function<void(const PreviewResult& result)>;

    /**
     * @brief Set progress callback
     */
    void SetProgressCallback(ProgressCallback callback);

    /**
     * @brief Set completion callback
     */
    void SetCompletionCallback(CompletionCallback callback);

    /**
     * @brief Start async preview generation
     * @param pipeline Pipeline to generate from
     * @return true if started successfully
     */
    bool StartAsync(PCGPipeline& pipeline);

    /**
     * @brief Check if async generation is running
     */
    bool IsRunning() const { return m_running.load(); }

    /**
     * @brief Cancel async generation
     */
    void Cancel();

    /**
     * @brief Wait for completion
     * @param timeoutMs Timeout in milliseconds (-1 for infinite)
     * @return true if completed, false if timeout
     */
    bool Wait(int timeoutMs = -1);

    /**
     * @brief Get last generated preview
     */
    const PreviewResult& GetLastResult() const { return m_lastResult; }

    // ========== Visualization ==========

    /**
     * @brief Get color for tile type
     */
    static uint32_t GetTileColor(TileType type);

    /**
     * @brief Get color for biome type
     */
    static uint32_t GetBiomeColor(BiomeType biome);

    /**
     * @brief Get color for road type
     */
    static uint32_t GetRoadColor(RoadType type);

    /**
     * @brief Visualization mode
     */
    enum class VisualizationMode {
        Tiles,          // Show tile types
        Biomes,         // Show biome data
        Elevation,      // Show height map
        Roads,          // Highlight roads
        Buildings,      // Highlight buildings
        Zones,          // Show zone types
        Occupancy       // Show occupied areas
    };

    /**
     * @brief Set visualization mode
     */
    void SetVisualizationMode(VisualizationMode mode);
    VisualizationMode GetVisualizationMode() const { return m_vizMode; }

    // ========== Image Utilities ==========

    /**
     * @brief Convert preview to PNG data
     * @return PNG encoded bytes
     */
    std::vector<uint8_t> EncodeToPNG() const;

    /**
     * @brief Save preview to file
     * @param filepath Output file path
     * @return true if saved successfully
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * @brief Upscale preview image
     * @param targetWidth Target width
     * @param targetHeight Target height
     * @return Upscaled image
     */
    PreviewImage Upscale(int targetWidth, int targetHeight) const;

private:
    PreviewQuality m_quality = PreviewQuality::Medium;
    int m_customWidth = 0;
    int m_customHeight = 0;
    VisualizationMode m_vizMode = VisualizationMode::Tiles;
    bool m_stageEnabled[static_cast<int>(PCGStage::COUNT)] = {true, true, true, true, true, true};

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelled{false};
    PreviewResult m_lastResult;

    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    std::mutex m_mutex;
    std::thread m_thread;

    // Internal helpers
    int GetScaleFactor() const;
    void RenderContext(const PCGContext& context, PreviewImage& image);
    void RenderTiles(const PCGContext& context, PreviewImage& image);
    void RenderElevation(const PCGContext& context, PreviewImage& image);
    void RenderBiomes(const PCGContext& context, PreviewImage& image);
    void RenderRoads(const PCGContext& context, PreviewImage& image);
    void RenderBuildings(const PCGContext& context, PreviewImage& image);
    void RenderZones(const PCGContext& context, PreviewImage& image);
    void RenderOccupancy(const PCGContext& context, PreviewImage& image);
    void RenderEntities(const PCGContext& context, PreviewImage& image);
    void RenderFoliage(const PCGContext& context, PreviewImage& image);
};

} // namespace Vehement
