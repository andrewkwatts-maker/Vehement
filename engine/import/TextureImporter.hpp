#pragma once

#include "ImportSettings.hpp"
#include "ImportProgress.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class Texture;

// ============================================================================
// Texture Data Structures
// ============================================================================

/**
 * @brief Raw image data
 */
struct ImageData {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 0;          ///< 1=Gray, 2=GrayAlpha, 3=RGB, 4=RGBA
    bool isHDR = false;
    bool is16Bit = false;
    std::vector<float> hdrPixels;  ///< For HDR images
};

/**
 * @brief Mipmap level data
 */
struct MipmapLevel {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    size_t dataSize = 0;
};

/**
 * @brief Compressed texture data
 */
struct CompressedTextureData {
    std::vector<MipmapLevel> mipmaps;
    TextureCompression format = TextureCompression::None;
    int width = 0;
    int height = 0;
    int channels = 0;
    bool sRGB = true;
};

/**
 * @brief Sprite slice information
 */
struct SpriteSlice {
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int pivotX = 0;
    int pivotY = 0;
    glm::vec4 border{0};  ///< 9-slice borders (left, top, right, bottom)
};

/**
 * @brief Texture atlas entry
 */
struct AtlasEntry {
    std::string texturePath;
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool rotated = false;
    glm::vec2 uvMin{0};
    glm::vec2 uvMax{1};
};

/**
 * @brief Texture atlas packing result
 * Note: Named AtlasPackResult to avoid conflict with Nova::TextureAtlas class
 */
struct AtlasPackResult {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<AtlasEntry> entries;
    std::vector<uint8_t> imageData;
};

/**
 * @brief Imported texture result
 */
struct ImportedTexture {
    std::string sourcePath;
    std::string outputPath;
    std::string assetId;

    // Image data
    int width = 0;
    int height = 0;
    int channels = 0;
    TextureCompression compression = TextureCompression::None;
    bool sRGB = true;
    bool hasMipmaps = false;
    int mipmapCount = 0;

    // Compressed data
    CompressedTextureData compressedData;

    // Metadata
    size_t originalSize = 0;
    size_t compressedSize = 0;
    float compressionRatio = 1.0f;

    // Sprite slices (if any)
    std::vector<SpriteSlice> sprites;

    // Thumbnail
    std::vector<uint8_t> thumbnail;
    int thumbnailWidth = 0;
    int thumbnailHeight = 0;

    // Import info
    TextureType detectedType = TextureType::Default;
    bool success = false;
    std::string errorMessage;
};

// ============================================================================
// Texture Importer
// ============================================================================

/**
 * @brief Comprehensive texture import pipeline
 *
 * Supports: PNG, JPG, TGA, BMP, DDS, KTX, EXR, HDR
 *
 * Features:
 * - Mipmap generation with quality options
 * - Compression (BC1-BC7, ETC2, ASTC)
 * - Normal map detection and processing
 * - sRGB vs linear color space
 * - Texture atlas generation
 * - Sprite sheet slicing
 * - Thumbnail generation
 * - Batch import
 */
class TextureImporter {
public:
    TextureImporter();
    ~TextureImporter();

    // Non-copyable
    TextureImporter(const TextureImporter&) = delete;
    TextureImporter& operator=(const TextureImporter&) = delete;

    // -------------------------------------------------------------------------
    // Single Texture Import
    // -------------------------------------------------------------------------

    /**
     * @brief Import a single texture
     */
    ImportedTexture Import(const std::string& path, const TextureImportSettings& settings,
                           ImportProgress* progress = nullptr);

    /**
     * @brief Import with default settings
     */
    ImportedTexture Import(const std::string& path);

    /**
     * @brief Quick import with auto-detection
     */
    std::unique_ptr<Texture> QuickImport(const std::string& path);

    // -------------------------------------------------------------------------
    // Batch Import
    // -------------------------------------------------------------------------

    /**
     * @brief Import multiple textures
     */
    std::vector<ImportedTexture> ImportBatch(const std::vector<std::string>& paths,
                                              const TextureImportSettings& settings,
                                              ImportProgressTracker* tracker = nullptr);

    /**
     * @brief Import with per-file settings
     */
    std::vector<ImportedTexture> ImportBatch(
        const std::vector<std::pair<std::string, TextureImportSettings>>& imports,
        ImportProgressTracker* tracker = nullptr);

    // -------------------------------------------------------------------------
    // Image Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load raw image data from file
     */
    ImageData LoadImage(const std::string& path);

    /**
     * @brief Load HDR image
     */
    ImageData LoadHDRImage(const std::string& path);

    /**
     * @brief Detect if image has alpha
     */
    bool HasAlpha(const ImageData& image);

    /**
     * @brief Get image info without loading full data
     */
    bool GetImageInfo(const std::string& path, int& width, int& height, int& channels);

    // -------------------------------------------------------------------------
    // Mipmap Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate mipmaps for image
     */
    std::vector<MipmapLevel> GenerateMipmaps(const ImageData& image,
                                              MipmapQuality quality = MipmapQuality::Standard,
                                              int maxLevels = 0);

    /**
     * @brief Calculate number of mip levels for dimensions
     */
    static int CalculateMipLevels(int width, int height);

    // -------------------------------------------------------------------------
    // Compression
    // -------------------------------------------------------------------------

    /**
     * @brief Compress texture data
     */
    CompressedTextureData Compress(const ImageData& image,
                                    TextureCompression format,
                                    int quality = 75,
                                    bool generateMipmaps = true);

    /**
     * @brief Compress with settings
     */
    CompressedTextureData Compress(const ImageData& image,
                                    const TextureImportSettings& settings);

    /**
     * @brief Estimate compressed size
     */
    size_t EstimateCompressedSize(int width, int height, TextureCompression format,
                                   bool withMipmaps = true);

    // -------------------------------------------------------------------------
    // Normal Map Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Detect if image is a normal map
     */
    bool DetectNormalMap(const ImageData& image);

    /**
     * @brief Convert height map to normal map
     */
    ImageData HeightToNormal(const ImageData& heightMap, float strength = 1.0f);

    /**
     * @brief Normalize normal map
     */
    void NormalizeNormalMap(ImageData& normalMap);

    /**
     * @brief Reconstruct Z channel from XY
     */
    void ReconstructNormalZ(ImageData& normalMap);

    /**
     * @brief Swizzle normal map channels (for different conventions)
     */
    void SwizzleNormalMap(ImageData& normalMap, int xChannel, int yChannel, int zChannel);

    // -------------------------------------------------------------------------
    // Image Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Resize image
     */
    ImageData Resize(const ImageData& image, int newWidth, int newHeight);

    /**
     * @brief Resize to power of two
     */
    ImageData ResizeToPowerOfTwo(const ImageData& image, bool roundUp = true);

    /**
     * @brief Flip image vertically
     */
    void FlipVertical(ImageData& image);

    /**
     * @brief Flip image horizontally
     */
    void FlipHorizontal(ImageData& image);

    /**
     * @brief Premultiply alpha
     */
    void PremultiplyAlpha(ImageData& image);

    /**
     * @brief Convert sRGB to linear
     */
    void SRGBToLinear(ImageData& image);

    /**
     * @brief Convert linear to sRGB
     */
    void LinearToSRGB(ImageData& image);

    /**
     * @brief Adjust gamma
     */
    void AdjustGamma(ImageData& image, float gamma);

    // -------------------------------------------------------------------------
    // Atlas Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate texture atlas from multiple images
     */
    AtlasPackResult GenerateAtlas(const std::vector<std::string>& imagePaths,
                                   int maxSize = 4096,
                                   int padding = 2,
                                   bool trimWhitespace = true);

    /**
     * @brief Pack images into atlas
     */
    AtlasPackResult PackImages(const std::vector<ImageData>& images,
                                const std::vector<std::string>& names,
                                int maxSize = 4096,
                                int padding = 2);

    // -------------------------------------------------------------------------
    // Sprite Sheet Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Slice sprite sheet into sprites
     */
    std::vector<SpriteSlice> SliceSpriteSheet(const ImageData& image,
                                               int sliceWidth, int sliceHeight,
                                               int columns = 0, int rows = 0);

    /**
     * @brief Auto-detect sprites (by color boundaries)
     */
    std::vector<SpriteSlice> AutoDetectSprites(const ImageData& image,
                                                 uint8_t alphaThreshold = 1);

    // -------------------------------------------------------------------------
    // Thumbnail Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate thumbnail
     */
    ImageData GenerateThumbnail(const ImageData& image, int size = 128);

    /**
     * @brief Generate thumbnail from file
     */
    ImageData GenerateThumbnailFromFile(const std::string& path, int size = 128);

    // -------------------------------------------------------------------------
    // File Format Support
    // -------------------------------------------------------------------------

    /**
     * @brief Check if format is supported
     */
    static bool IsFormatSupported(const std::string& extension);

    /**
     * @brief Get all supported extensions
     */
    static std::vector<std::string> GetSupportedExtensions();

    /**
     * @brief Detect format from file header
     */
    static std::string DetectFormat(const std::string& path);

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    /**
     * @brief Save compressed texture to file
     */
    bool SaveCompressed(const CompressedTextureData& data, const std::string& path);

    /**
     * @brief Save as PNG
     */
    bool SavePNG(const ImageData& image, const std::string& path);

    /**
     * @brief Save as engine format
     */
    bool SaveEngineFormat(const ImportedTexture& texture, const std::string& path);

    /**
     * @brief Export texture metadata
     */
    std::string ExportMetadata(const ImportedTexture& texture);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using LoadCallback = std::function<void(const std::string& path)>;
    using ProcessCallback = std::function<void(const std::string& stage, float progress)>;

    void SetLoadCallback(LoadCallback callback) { m_loadCallback = callback; }
    void SetProcessCallback(ProcessCallback callback) { m_processCallback = callback; }

private:
    // Internal helpers
    ImageData LoadPNG(const std::string& path);
    ImageData LoadJPG(const std::string& path);
    ImageData LoadTGA(const std::string& path);
    ImageData LoadBMP(const std::string& path);
    ImageData LoadDDS(const std::string& path);
    ImageData LoadKTX(const std::string& path);
    ImageData LoadEXR(const std::string& path);

    CompressedTextureData CompressBC1(const ImageData& image, int quality);
    CompressedTextureData CompressBC3(const ImageData& image, int quality);
    CompressedTextureData CompressBC4(const ImageData& image, int quality);
    CompressedTextureData CompressBC5(const ImageData& image, int quality);
    CompressedTextureData CompressBC6H(const ImageData& image, int quality);
    CompressedTextureData CompressBC7(const ImageData& image, int quality);
    CompressedTextureData CompressETC(const ImageData& image, TextureCompression format, int quality);
    CompressedTextureData CompressASTC(const ImageData& image, TextureCompression format, int quality);

    MipmapLevel GenerateMipLevel(const uint8_t* srcData, int srcWidth, int srcHeight,
                                  int channels, MipmapQuality quality);

    // Rectangle packing for atlases
    struct PackRect {
        int x = 0, y = 0;
        int width = 0, height = 0;
        int id = 0;
        bool packed = false;
    };
    std::vector<PackRect> PackRectangles(std::vector<PackRect>& rects, int maxWidth, int maxHeight);

    LoadCallback m_loadCallback;
    ProcessCallback m_processCallback;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Calculate mipmap size
 */
inline int CalculateMipSize(int size, int level) {
    return std::max(1, size >> level);
}

/**
 * @brief Check if dimensions are power of two
 */
inline bool IsPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Round up to next power of two
 */
inline int NextPowerOfTwo(int value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

/**
 * @brief sRGB to linear conversion
 */
inline float SRGBToLinear(float srgb) {
    if (srgb <= 0.04045f) {
        return srgb / 12.92f;
    }
    return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
}

/**
 * @brief Linear to sRGB conversion
 */
inline float LinearToSRGB(float linear) {
    if (linear <= 0.0031308f) {
        return linear * 12.92f;
    }
    return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

} // namespace Nova
