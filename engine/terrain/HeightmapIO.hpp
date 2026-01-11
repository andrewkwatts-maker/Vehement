#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Container for heightmap data
 *
 * Stores height values as normalized floats [0, 1] that can be scaled
 * to world-space heights using the heightScale parameter.
 */
struct HeightmapData {
    int width = 0;                      ///< Width in pixels
    int height = 0;                     ///< Height in pixels
    std::vector<float> data;            ///< Height values (row-major, normalized [0,1])
    float minHeight = 0.0f;             ///< Minimum height in world units (for denormalization)
    float maxHeight = 1.0f;             ///< Maximum height in world units (for denormalization)

    /**
     * @brief Check if heightmap data is valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return width > 0 && height > 0 &&
               data.size() == static_cast<size_t>(width * height);
    }

    /**
     * @brief Get height at pixel coordinates (clamped)
     * @param x X coordinate (0 to width-1)
     * @param y Y coordinate (0 to height-1)
     * @return Normalized height value [0, 1]
     */
    [[nodiscard]] float GetHeight(int x, int y) const noexcept {
        x = glm::clamp(x, 0, width - 1);
        y = glm::clamp(y, 0, height - 1);
        return data[y * width + x];
    }

    /**
     * @brief Get bilinearly interpolated height at UV coordinates
     * @param u U coordinate [0, 1]
     * @param v V coordinate [0, 1]
     * @return Normalized height value [0, 1]
     */
    [[nodiscard]] float SampleBilinear(float u, float v) const noexcept;

    /**
     * @brief Get world-space height at pixel coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @return Height in world units
     */
    [[nodiscard]] float GetWorldHeight(int x, int y) const noexcept {
        return minHeight + GetHeight(x, y) * (maxHeight - minHeight);
    }

    /**
     * @brief Set height at pixel coordinates
     * @param x X coordinate
     * @param y Y coordinate
     * @param value Normalized height value [0, 1]
     */
    void SetHeight(int x, int y, float value) noexcept {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            data[y * width + x] = glm::clamp(value, 0.0f, 1.0f);
        }
    }

    /**
     * @brief Allocate storage for given dimensions
     * @param w Width
     * @param h Height
     * @param initialValue Initial height value (default 0)
     */
    void Allocate(int w, int h, float initialValue = 0.0f) {
        width = w;
        height = h;
        data.resize(static_cast<size_t>(w * h), initialValue);
    }

    /**
     * @brief Clear all data
     */
    void Clear() noexcept {
        width = 0;
        height = 0;
        data.clear();
        minHeight = 0.0f;
        maxHeight = 1.0f;
    }

    /**
     * @brief Calculate actual min/max values in the data
     * @return Pair of (min, max) normalized values
     */
    [[nodiscard]] std::pair<float, float> CalculateMinMax() const noexcept;

    /**
     * @brief Normalize data to [0, 1] range
     */
    void Normalize();

    /**
     * @brief Get raw pointer to data
     */
    [[nodiscard]] float* GetData() noexcept { return data.data(); }
    [[nodiscard]] const float* GetData() const noexcept { return data.data(); }

    /**
     * @brief Get total size in bytes
     */
    [[nodiscard]] size_t GetSizeBytes() const noexcept { return data.size() * sizeof(float); }
};

/**
 * @brief Options for importing heightmaps
 */
struct HeightmapImportOptions {
    bool normalizeHeight = true;        ///< Normalize imported heights to [0, 1]
    float heightScale = 1.0f;           ///< Scale factor applied after normalization
    bool invertY = false;               ///< Flip the heightmap vertically
    bool invertHeight = false;          ///< Invert height values (1 - h)

    // For RAW format
    bool littleEndian = true;           ///< RAW file byte order (true = little endian)
    bool signedFormat = false;          ///< RAW file uses signed 16-bit values

    // Height range mapping
    float sourceMinHeight = 0.0f;       ///< Expected minimum height in source
    float sourceMaxHeight = 1.0f;       ///< Expected maximum height in source
    float targetMinHeight = 0.0f;       ///< Target minimum world height
    float targetMaxHeight = 100.0f;     ///< Target maximum world height
};

/**
 * @brief Options for exporting heightmaps
 */
struct HeightmapExportOptions {
    bool normalize = true;              ///< Normalize to full bit range before export
    bool invertY = false;               ///< Flip the heightmap vertically
    int jpegQuality = 95;               ///< JPEG quality (1-100) if exporting to JPEG
    bool littleEndian = true;           ///< RAW file byte order
};

/**
 * @brief Result of heightmap operations with error information
 */
struct HeightmapResult {
    bool success = false;
    std::string errorMessage;
    HeightmapData heightmap;

    [[nodiscard]] operator bool() const noexcept { return success; }

    static HeightmapResult Success(HeightmapData&& data) {
        return {true, "", std::move(data)};
    }

    static HeightmapResult Error(const std::string& message) {
        return {false, message, {}};
    }
};

/**
 * @brief Heightmap import/export utility for Nova3D/Vehement terrain system
 *
 * Supports:
 * - PNG heightmaps (8-bit and 16-bit grayscale)
 * - RAW heightmaps (16-bit unsigned, commonly used in terrain editors)
 * - Height range normalization and scaling
 * - Terrain scaling options
 *
 * Uses stb_image for PNG loading and stb_image_write for saving.
 *
 * Usage example:
 * @code
 * HeightmapImportOptions options;
 * options.normalizeHeight = true;
 * options.targetMaxHeight = 500.0f;
 *
 * auto result = HeightmapIO::LoadFromPNG("terrain.png", options);
 * if (result.success) {
 *     // Use result.heightmap
 * }
 * @endcode
 */
class HeightmapIO {
public:
    // =========================================================================
    // Import Functions
    // =========================================================================

    /**
     * @brief Load heightmap from PNG file
     *
     * Supports 8-bit and 16-bit grayscale PNG files. RGB/RGBA images are
     * converted to grayscale using luminance formula.
     *
     * @param path Path to PNG file
     * @param options Import options
     * @return HeightmapResult containing data or error message
     */
    [[nodiscard]] static HeightmapResult LoadFromPNG(
        const std::string& path,
        const HeightmapImportOptions& options = {});

    /**
     * @brief Load heightmap from RAW file
     *
     * RAW files are headerless 16-bit heightmaps commonly used by terrain
     * editors. The dimensions must be specified since they're not stored
     * in the file.
     *
     * @param path Path to RAW file
     * @param width Width in pixels
     * @param height Height in pixels (if 0, assumes square with width)
     * @param options Import options
     * @return HeightmapResult containing data or error message
     */
    [[nodiscard]] static HeightmapResult LoadFromRAW(
        const std::string& path,
        int width,
        int height = 0,
        const HeightmapImportOptions& options = {});

    /**
     * @brief Auto-detect format and load heightmap
     *
     * Attempts to detect format from file extension:
     * - .png -> LoadFromPNG
     * - .raw, .r16 -> LoadFromRAW (requires width/height)
     *
     * @param path Path to heightmap file
     * @param options Import options
     * @param rawWidth Width for RAW files (required if loading RAW)
     * @param rawHeight Height for RAW files (optional, defaults to width)
     * @return HeightmapResult containing data or error message
     */
    [[nodiscard]] static HeightmapResult Load(
        const std::string& path,
        const HeightmapImportOptions& options = {},
        int rawWidth = 0,
        int rawHeight = 0);

    // =========================================================================
    // Export Functions
    // =========================================================================

    /**
     * @brief Save heightmap to PNG file
     *
     * @param heightmap Heightmap data to save
     * @param path Output file path
     * @param bits Bit depth (8 or 16)
     * @param options Export options
     * @return true on success, false on failure
     */
    [[nodiscard]] static bool SaveToPNG(
        const HeightmapData& heightmap,
        const std::string& path,
        int bits = 16,
        const HeightmapExportOptions& options = {});

    /**
     * @brief Save heightmap to RAW file
     *
     * Saves as 16-bit unsigned integers, suitable for terrain editors.
     *
     * @param heightmap Heightmap data to save
     * @param path Output file path
     * @param options Export options
     * @return true on success, false on failure
     */
    [[nodiscard]] static bool SaveToRAW(
        const HeightmapData& heightmap,
        const std::string& path,
        const HeightmapExportOptions& options = {});

    /**
     * @brief Auto-detect format and save heightmap
     *
     * Detects format from file extension:
     * - .png -> SaveToPNG (16-bit by default)
     * - .raw, .r16 -> SaveToRAW
     *
     * @param heightmap Heightmap data to save
     * @param path Output file path
     * @param options Export options
     * @return true on success, false on failure
     */
    [[nodiscard]] static bool Save(
        const HeightmapData& heightmap,
        const std::string& path,
        const HeightmapExportOptions& options = {});

    // =========================================================================
    // Utility Functions
    // =========================================================================

    /**
     * @brief Create empty heightmap with specified dimensions
     *
     * @param width Width in pixels
     * @param height Height in pixels
     * @param initialHeight Initial normalized height value [0, 1]
     * @return HeightmapData initialized with given dimensions
     */
    [[nodiscard]] static HeightmapData CreateEmpty(
        int width,
        int height,
        float initialHeight = 0.0f);

    /**
     * @brief Create heightmap from raw float data
     *
     * @param data Pointer to float height data (row-major)
     * @param width Width in pixels
     * @param height Height in pixels
     * @param normalize Whether to normalize the data to [0, 1]
     * @return HeightmapData containing the data
     */
    [[nodiscard]] static HeightmapData CreateFromFloat(
        const float* data,
        int width,
        int height,
        bool normalize = false);

    /**
     * @brief Resample heightmap to new dimensions
     *
     * Uses bilinear interpolation for smooth resampling.
     *
     * @param heightmap Source heightmap
     * @param newWidth New width
     * @param newHeight New height
     * @return Resampled heightmap
     */
    [[nodiscard]] static HeightmapData Resample(
        const HeightmapData& heightmap,
        int newWidth,
        int newHeight);

    /**
     * @brief Apply Gaussian blur to heightmap
     *
     * Useful for smoothing imported heightmaps.
     *
     * @param heightmap Source heightmap
     * @param radius Blur radius in pixels
     * @return Blurred heightmap
     */
    [[nodiscard]] static HeightmapData GaussianBlur(
        const HeightmapData& heightmap,
        int radius);

    /**
     * @brief Generate normal map from heightmap
     *
     * @param heightmap Source heightmap
     * @param strength Normal strength multiplier
     * @return RGBA normal map data (can be saved with SaveToPNG as 8-bit)
     */
    [[nodiscard]] static std::vector<uint8_t> GenerateNormalMap(
        const HeightmapData& heightmap,
        float strength = 1.0f);

    /**
     * @brief Get file extension in lowercase
     */
    [[nodiscard]] static std::string GetFileExtension(const std::string& path);

    /**
     * @brief Check if file exists
     */
    [[nodiscard]] static bool FileExists(const std::string& path);

    /**
     * @brief Get last error message
     */
    [[nodiscard]] static const std::string& GetLastError() noexcept { return s_lastError; }

private:
    static std::string s_lastError;

    // Internal helpers
    static void SetError(const std::string& error);
    static float ByteToFloat(uint8_t value) noexcept;
    static float UInt16ToFloat(uint16_t value) noexcept;
    static uint8_t FloatToByte(float value) noexcept;
    static uint16_t FloatToUInt16(float value) noexcept;
};

} // namespace Nova
