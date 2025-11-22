#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class ImportSettings;
class TextureImportSettings;
class ModelImportSettings;
class AnimationImportSettings;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Target platform for asset processing
 */
enum class TargetPlatform : uint8_t {
    Desktop,        ///< Windows/Linux/macOS
    Mobile,         ///< iOS/Android
    WebGL,          ///< Browser
    Console         ///< PS5/Xbox/Switch
};

/**
 * @brief Import preset quality levels
 */
enum class ImportPreset : uint8_t {
    Custom,         ///< User-defined settings
    Mobile,         ///< Optimized for mobile (smaller sizes, ETC2/ASTC)
    Desktop,        ///< Standard desktop quality
    HighQuality,    ///< Maximum quality for high-end systems
    WebGL           ///< Optimized for web deployment
};

/**
 * @brief Texture compression format
 */
enum class TextureCompression : uint8_t {
    None,           ///< Uncompressed
    BC1,            ///< RGB, 4:1 compression (DXT1)
    BC3,            ///< RGBA, 4:1 compression (DXT5)
    BC4,            ///< Single channel (ATI1)
    BC5,            ///< Two channel (ATI2) - normal maps
    BC6H,           ///< HDR compression
    BC7,            ///< High quality RGBA
    ETC1,           ///< OpenGL ES 2.0
    ETC2_RGB,       ///< OpenGL ES 3.0 RGB
    ETC2_RGBA,      ///< OpenGL ES 3.0 RGBA
    ASTC_4x4,       ///< ASTC highest quality
    ASTC_6x6,       ///< ASTC medium quality
    ASTC_8x8,       ///< ASTC lower quality, better compression
    PVRTC_RGB,      ///< iOS
    PVRTC_RGBA      ///< iOS with alpha
};

/**
 * @brief Texture usage type for automatic settings
 */
enum class TextureType : uint8_t {
    Default,        ///< Generic texture
    Diffuse,        ///< Color/albedo map
    Normal,         ///< Normal map (special processing)
    Specular,       ///< Specular/glossiness map
    Metallic,       ///< Metallic map
    Roughness,      ///< Roughness map
    AO,             ///< Ambient occlusion
    Emissive,       ///< Emission map
    Height,         ///< Height/displacement map
    Mask,           ///< Mask texture (linear)
    HDR,            ///< HDR environment map
    LUT,            ///< Look-up table
    UI,             ///< UI sprite
    Sprite,         ///< Game sprite
    Lightmap        ///< Baked lighting
};

/**
 * @brief Mipmap generation quality
 */
enum class MipmapQuality : uint8_t {
    Fast,           ///< Box filter (fastest)
    Standard,       ///< Lanczos filter
    HighQuality,    ///< Kaiser filter with sharpening
    Custom          ///< User-defined filter
};

/**
 * @brief Model import scale units
 */
enum class ModelUnits : uint8_t {
    Meters,
    Centimeters,
    Millimeters,
    Inches,
    Feet
};

/**
 * @brief Animation compression quality
 */
enum class AnimationCompression : uint8_t {
    None,           ///< No compression
    Lossy,          ///< Keyframe reduction
    HighQuality,    ///< Minimal loss
    Aggressive      ///< Maximum compression
};

// ============================================================================
// Base Import Settings
// ============================================================================

/**
 * @brief Base class for all import settings
 */
class ImportSettingsBase {
public:
    virtual ~ImportSettingsBase() = default;

    // Identification
    std::string assetPath;          ///< Original asset path
    std::string outputPath;         ///< Output asset path
    std::string assetId;            ///< Unique asset identifier

    // Versioning
    uint32_t settingsVersion = 1;   ///< Settings format version
    uint64_t sourceFileHash = 0;    ///< Hash for change detection
    uint64_t lastImportTime = 0;    ///< Timestamp of last import

    // Common options
    ImportPreset preset = ImportPreset::Desktop;
    TargetPlatform targetPlatform = TargetPlatform::Desktop;
    bool enabled = true;            ///< Whether to process this asset

    /**
     * @brief Serialize settings to JSON
     */
    [[nodiscard]] virtual std::string ToJson() const;

    /**
     * @brief Deserialize settings from JSON
     */
    virtual bool FromJson(const std::string& json);

    /**
     * @brief Apply preset settings
     */
    virtual void ApplyPreset(ImportPreset preset);

    /**
     * @brief Clone settings
     */
    [[nodiscard]] virtual std::unique_ptr<ImportSettingsBase> Clone() const = 0;

    /**
     * @brief Get asset type name
     */
    [[nodiscard]] virtual std::string GetTypeName() const = 0;

protected:
    ImportSettingsBase() = default;
    ImportSettingsBase(const ImportSettingsBase&) = default;
    ImportSettingsBase& operator=(const ImportSettingsBase&) = default;
};

// ============================================================================
// Texture Import Settings
// ============================================================================

/**
 * @brief Texture-specific import settings
 */
class TextureImportSettings : public ImportSettingsBase {
public:
    TextureImportSettings() = default;

    // Format settings
    TextureType textureType = TextureType::Default;
    TextureCompression compression = TextureCompression::BC7;
    bool sRGB = true;               ///< sRGB color space (false for normal maps, data)
    bool generateMipmaps = true;
    MipmapQuality mipmapQuality = MipmapQuality::Standard;
    int maxMipLevel = 0;            ///< 0 = auto (full chain)

    // Size settings
    int maxWidth = 4096;
    int maxHeight = 4096;
    bool powerOfTwo = false;        ///< Force power-of-two dimensions
    bool allowNonPowerOfTwo = true;

    // Filtering
    bool enableAnisotropic = true;
    int anisotropicLevel = 8;

    // Channels
    bool premultiplyAlpha = false;
    bool flipVertically = false;
    bool flipHorizontally = false;

    // Normal map specific
    bool isNormalMap = false;
    bool normalMapFromHeight = false;
    float normalMapStrength = 1.0f;
    bool reconstructZ = false;      ///< Reconstruct Z from XY

    // Atlas/Sprite settings
    bool createAtlas = false;
    int atlasMaxSize = 4096;
    int atlasPadding = 2;
    bool trimWhitespace = true;

    // Sprite slicing
    bool sliceSprites = false;
    int sliceWidth = 32;
    int sliceHeight = 32;
    int sliceColumns = 0;           ///< 0 = auto-detect
    int sliceRows = 0;

    // Thumbnail
    bool generateThumbnail = true;
    int thumbnailSize = 128;

    // Quality
    int compressionQuality = 75;    ///< 0-100
    bool dithering = true;

    // Streaming
    bool enableStreaming = false;
    int streamingPriority = 0;

    [[nodiscard]] std::unique_ptr<ImportSettingsBase> Clone() const override;
    [[nodiscard]] std::string GetTypeName() const override { return "Texture"; }
    [[nodiscard]] std::string ToJson() const override;
    bool FromJson(const std::string& json) override;
    void ApplyPreset(ImportPreset preset) override;

    /**
     * @brief Auto-detect texture type from filename
     */
    void AutoDetectType(const std::string& filename);

    /**
     * @brief Get recommended compression for platform
     */
    [[nodiscard]] TextureCompression GetRecommendedCompression(TargetPlatform platform) const;
};

// ============================================================================
// Model Import Settings
// ============================================================================

/**
 * @brief 3D model import settings
 */
class ModelImportSettings : public ImportSettingsBase {
public:
    ModelImportSettings() = default;

    // Transform
    float scaleFactor = 1.0f;
    ModelUnits sourceUnits = ModelUnits::Meters;
    ModelUnits targetUnits = ModelUnits::Meters;
    bool swapYZ = false;            ///< Convert from Z-up to Y-up
    bool flipWindingOrder = false;

    // Mesh processing
    bool optimizeMesh = true;
    bool generateNormals = false;
    bool generateTangents = true;
    bool calculateBounds = true;
    bool mergeVertices = true;
    float mergeThreshold = 0.0001f;
    bool removeRedundantMaterials = true;

    // LOD generation
    bool generateLODs = false;
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
    std::vector<float> lodReductions = {0.5f, 0.25f, 0.125f, 0.0625f};
    float lodScreenSize = 0.01f;    ///< Minimum screen percentage

    // Materials
    bool importMaterials = true;
    bool importTextures = true;
    bool embedTextures = false;
    std::string materialSearchPath;

    // Skeleton
    bool importSkeleton = true;
    bool importSkinWeights = true;
    int maxBonesPerVertex = 4;
    float boneWeightThreshold = 0.01f;

    // Collision
    bool generateCollision = false;
    bool convexDecomposition = false;
    int maxConvexHulls = 16;
    int maxVerticesPerHull = 64;
    bool generateSimplifiedCollision = true;
    float collisionSimplification = 0.5f;

    // Animation
    bool importAnimations = true;
    bool splitAnimations = true;

    // Compression
    bool compressVertices = false;
    bool compressIndices = true;
    bool use16BitIndices = true;    ///< When possible

    [[nodiscard]] std::unique_ptr<ImportSettingsBase> Clone() const override;
    [[nodiscard]] std::string GetTypeName() const override { return "Model"; }
    [[nodiscard]] std::string ToJson() const override;
    bool FromJson(const std::string& json) override;
    void ApplyPreset(ImportPreset preset) override;

    /**
     * @brief Calculate scale factor between unit systems
     */
    [[nodiscard]] float CalculateUnitScale() const;
};

// ============================================================================
// Animation Import Settings
// ============================================================================

/**
 * @brief Animation import settings
 */
class AnimationImportSettings : public ImportSettingsBase {
public:
    AnimationImportSettings() = default;

    // Basic settings
    float sampleRate = 30.0f;
    bool resample = false;
    float targetSampleRate = 30.0f;

    // Compression
    AnimationCompression compression = AnimationCompression::Lossy;
    float positionTolerance = 0.001f;
    float rotationTolerance = 0.0001f;
    float scaleTolerance = 0.001f;

    // Root motion
    bool extractRootMotion = true;
    std::string rootBoneName = "root";
    bool lockRootPositionXZ = false;
    bool lockRootRotationY = false;
    bool lockRootHeight = false;

    // Clip settings
    bool splitByMarkers = true;
    bool splitByTakes = true;
    std::vector<std::pair<std::string, std::pair<float, float>>> clipRanges;

    // Looping
    bool detectLoops = true;
    float loopThreshold = 0.01f;
    bool forceLoop = false;

    // Additive
    bool makeAdditive = false;
    std::string additiveReferencePose;
    float additiveReferenceFrame = 0.0f;

    // Retargeting
    bool enableRetargeting = false;
    std::string sourceSkeletonPath;
    std::string targetSkeletonPath;
    std::unordered_map<std::string, std::string> boneMapping;

    // Events
    bool importEvents = true;
    bool importCurves = true;

    // IK
    bool preserveIK = false;
    bool bakeIK = true;

    [[nodiscard]] std::unique_ptr<ImportSettingsBase> Clone() const override;
    [[nodiscard]] std::string GetTypeName() const override { return "Animation"; }
    [[nodiscard]] std::string ToJson() const override;
    bool FromJson(const std::string& json) override;
    void ApplyPreset(ImportPreset preset) override;

    /**
     * @brief Add a clip range
     */
    void AddClipRange(const std::string& name, float startTime, float endTime);

    /**
     * @brief Add bone mapping for retargeting
     */
    void AddBoneMapping(const std::string& sourceBone, const std::string& targetBone);
};

// ============================================================================
// Settings Manager
// ============================================================================

/**
 * @brief Manages import settings for all assets
 */
class ImportSettingsManager {
public:
    static ImportSettingsManager& Instance();

    /**
     * @brief Get settings for an asset (creates default if not exists)
     */
    template<typename T>
    T* GetSettings(const std::string& assetPath);

    /**
     * @brief Get or create settings
     */
    ImportSettingsBase* GetOrCreateSettings(const std::string& assetPath, const std::string& type);

    /**
     * @brief Save settings for an asset
     */
    bool SaveSettings(const std::string& assetPath, const ImportSettingsBase* settings);

    /**
     * @brief Load settings for an asset
     */
    std::unique_ptr<ImportSettingsBase> LoadSettings(const std::string& assetPath);

    /**
     * @brief Check if settings exist
     */
    bool HasSettings(const std::string& assetPath) const;

    /**
     * @brief Remove settings
     */
    void RemoveSettings(const std::string& assetPath);

    /**
     * @brief Get all assets with settings
     */
    [[nodiscard]] std::vector<std::string> GetAllAssetPaths() const;

    /**
     * @brief Apply preset to multiple assets
     */
    void ApplyPresetToAll(ImportPreset preset, const std::string& typeFilter = "");

    /**
     * @brief Get settings file path for an asset
     */
    [[nodiscard]] static std::string GetSettingsPath(const std::string& assetPath);

    /**
     * @brief Detect asset type from extension
     */
    [[nodiscard]] static std::string DetectAssetType(const std::string& path);

    /**
     * @brief Register custom preset
     */
    void RegisterPreset(const std::string& name, std::function<void(ImportSettingsBase*)> applicator);

private:
    ImportSettingsManager() = default;

    std::unordered_map<std::string, std::unique_ptr<ImportSettingsBase>> m_settings;
    std::unordered_map<std::string, std::function<void(ImportSettingsBase*)>> m_customPresets;
};

// ============================================================================
// Template Implementation
// ============================================================================

template<typename T>
T* ImportSettingsManager::GetSettings(const std::string& assetPath) {
    auto it = m_settings.find(assetPath);
    if (it != m_settings.end()) {
        return dynamic_cast<T*>(it->second.get());
    }

    auto settings = std::make_unique<T>();
    settings->assetPath = assetPath;
    T* ptr = settings.get();
    m_settings[assetPath] = std::move(settings);
    return ptr;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get compression format name
 */
[[nodiscard]] const char* GetCompressionName(TextureCompression compression);

/**
 * @brief Get compression bits per pixel
 */
[[nodiscard]] float GetCompressionBPP(TextureCompression compression);

/**
 * @brief Check if compression supports alpha
 */
[[nodiscard]] bool CompressionSupportsAlpha(TextureCompression compression);

/**
 * @brief Get platform-appropriate compression
 */
[[nodiscard]] TextureCompression GetPlatformCompression(TargetPlatform platform, bool hasAlpha, bool isNormalMap);

} // namespace Nova
