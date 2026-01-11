#pragma once

/**
 * @file FBXImporter.hpp
 * @brief FBX model import system using Assimp
 *
 * Provides comprehensive FBX file import with support for:
 * - Static meshes with full vertex attributes
 * - PBR materials with texture references
 * - Skeletal hierarchies for animation
 * - Animation clips with position/rotation/scale keyframes
 * - Automatic mesh-to-SDF conversion option
 * - Multi-mesh support per file
 *
 * @see ModelImporter for the general import pipeline
 * @see SDFMeshConverter for SDF conversion
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

// Forward declarations
class Mesh;
class Material;
class Texture;
class Skeleton;
class Animation;
class SDFModel;

// ============================================================================
// Import Options
// ============================================================================

/**
 * @brief Coordinate system up axis
 */
enum class UpAxis : uint8_t {
    Y_Up,       ///< OpenGL/DirectX convention (default)
    Z_Up        ///< Blender/3ds Max convention
};

/**
 * @brief Front axis direction
 */
enum class FrontAxis : uint8_t {
    NegativeZ,  ///< OpenGL convention (default)
    PositiveZ,  ///< Some DCC tools
    NegativeY,
    PositiveY
};

/**
 * @brief Mesh optimization level
 */
enum class MeshOptimization : uint8_t {
    None,           ///< No optimization
    Standard,       ///< Join vertices, optimize indices
    Aggressive      ///< Full optimization with mesh merging
};

/**
 * @brief Texture loading mode
 */
enum class TextureLoadMode : uint8_t {
    Skip,           ///< Don't load textures
    PathOnly,       ///< Store paths only, don't load
    LoadImmediate,  ///< Load textures immediately
    LoadDeferred    ///< Mark for deferred loading
};

/**
 * @brief Options for FBX import
 */
struct FBXImportOptions {
    // -------------------------------------------------------------------------
    // Transform Settings
    // -------------------------------------------------------------------------

    /// Scale factor applied to all geometry
    float scaleFactor = 1.0f;

    /// Source file up axis (auto-detected if possible)
    UpAxis sourceUpAxis = UpAxis::Y_Up;

    /// Target up axis
    UpAxis targetUpAxis = UpAxis::Y_Up;

    /// Source front axis
    FrontAxis sourceFrontAxis = FrontAxis::NegativeZ;

    /// Target front axis
    FrontAxis targetFrontAxis = FrontAxis::NegativeZ;

    /// Flip triangle winding order
    bool flipWindingOrder = false;

    /// Flip UV coordinates vertically
    bool flipUVs = true;

    // -------------------------------------------------------------------------
    // Mesh Settings
    // -------------------------------------------------------------------------

    /// Import mesh geometry
    bool importMeshes = true;

    /// Optimization level
    MeshOptimization optimization = MeshOptimization::Standard;

    /// Generate normals if missing
    bool generateNormals = true;

    /// Use smooth normals (vs flat)
    bool smoothNormals = true;

    /// Generate tangents and bitangents
    bool generateTangents = true;

    /// Calculate bounding boxes
    bool calculateBounds = true;

    /// Maximum bones per vertex for skinning
    int maxBonesPerVertex = 4;

    /// Minimum bone weight threshold
    float boneWeightThreshold = 0.01f;

    // -------------------------------------------------------------------------
    // Material Settings
    // -------------------------------------------------------------------------

    /// Import materials
    bool importMaterials = true;

    /// Texture loading mode
    TextureLoadMode textureMode = TextureLoadMode::PathOnly;

    /// Directory to search for textures (relative to model)
    std::string textureSearchPath;

    /// Extract embedded textures
    bool extractEmbeddedTextures = true;

    /// Output directory for embedded textures
    std::string embeddedTextureOutputDir;

    // -------------------------------------------------------------------------
    // Skeleton Settings
    // -------------------------------------------------------------------------

    /// Import skeleton/bone hierarchy
    bool importSkeleton = true;

    /// Import skin weights
    bool importSkinWeights = true;

    /// Remove leaf bones (often IK targets)
    bool removeLeafBones = false;

    /// Maximum skeleton depth
    int maxBoneDepth = 64;

    // -------------------------------------------------------------------------
    // Animation Settings
    // -------------------------------------------------------------------------

    /// Import animations
    bool importAnimations = true;

    /// Resample animations to fixed framerate
    bool resampleAnimations = false;

    /// Target sample rate for resampling
    float targetSampleRate = 30.0f;

    /// Remove redundant keyframes
    bool optimizeKeyframes = true;

    /// Tolerance for keyframe optimization
    float keyframeOptimizationTolerance = 0.0001f;

    /// Import animation events/markers
    bool importAnimationEvents = true;

    // -------------------------------------------------------------------------
    // LOD Settings
    // -------------------------------------------------------------------------

    /// Generate LOD meshes
    bool generateLODs = false;

    /// Number of LOD levels to generate
    int lodLevelCount = 4;

    /// LOD reduction ratios (0.0-1.0)
    std::vector<float> lodReductions = {0.5f, 0.25f, 0.125f, 0.0625f};

    /// LOD switching distances
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};

    // -------------------------------------------------------------------------
    // SDF Conversion Settings
    // -------------------------------------------------------------------------

    /// Convert meshes to SDF representations
    bool convertToSDF = false;

    /// SDF voxel resolution
    int sdfResolution = 64;

    /// SDF bounds padding factor
    float sdfBoundsPadding = 0.1f;

    /// Generate LODs for SDF mesh
    bool sdfGenerateLODs = false;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /// Progress callback (0.0-1.0)
    std::function<void(float progress, const std::string& status)> progressCallback;

    /// Warning callback
    std::function<void(const std::string& warning)> warningCallback;
};

// ============================================================================
// Import Result Structures
// ============================================================================

/**
 * @brief Imported texture reference
 */
struct FBXTextureRef {
    std::string path;           ///< Texture file path
    std::string type;           ///< Texture type (diffuse, normal, etc.)
    int uvChannel = 0;          ///< UV channel index
    glm::vec2 uvScale{1.0f};    ///< UV scale
    glm::vec2 uvOffset{0.0f};   ///< UV offset
    bool embedded = false;      ///< True if texture was embedded in FBX

    /// Loaded texture (if TextureLoadMode::LoadImmediate)
    std::shared_ptr<Texture> texture;
};

/**
 * @brief Imported material data
 */
struct FBXMaterialData {
    std::string name;

    // PBR properties
    glm::vec4 albedoColor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissiveColor{0.0f};
    float emissiveIntensity = 0.0f;

    // Legacy properties (for non-PBR content)
    glm::vec3 diffuseColor{1.0f};
    glm::vec3 specularColor{1.0f};
    float shininess = 32.0f;
    float opacity = 1.0f;

    // Textures
    std::optional<FBXTextureRef> albedoMap;
    std::optional<FBXTextureRef> normalMap;
    std::optional<FBXTextureRef> metallicMap;
    std::optional<FBXTextureRef> roughnessMap;
    std::optional<FBXTextureRef> aoMap;
    std::optional<FBXTextureRef> emissiveMap;
    std::optional<FBXTextureRef> heightMap;
    std::optional<FBXTextureRef> opacityMap;

    // Rendering flags
    bool doubleSided = false;
    bool transparent = false;
    std::string blendMode = "opaque";

    /// Convert to engine Material
    [[nodiscard]] std::shared_ptr<Material> ToMaterial() const;
};

/**
 * @brief Imported mesh data
 */
struct FBXMeshData {
    std::string name;

    // Geometry
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<glm::vec4> colors;
    std::vector<uint32_t> indices;

    // Skinning
    std::vector<glm::ivec4> boneIds;
    std::vector<glm::vec4> boneWeights;

    // Material
    int materialIndex = -1;

    // Bounds
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    glm::vec3 boundsCenter{0.0f};
    float boundsSphereRadius = 0.0f;

    // Stats
    bool hasTangents = false;
    bool hasBoneWeights = false;
    bool hasVertexColors = false;

    /// Convert to engine Mesh
    [[nodiscard]] std::unique_ptr<Mesh> ToMesh() const;

    /// Get vertex count
    [[nodiscard]] size_t GetVertexCount() const { return positions.size(); }

    /// Get triangle count
    [[nodiscard]] size_t GetTriangleCount() const { return indices.size() / 3; }
};

/**
 * @brief Imported bone data
 */
struct FBXBoneData {
    std::string name;
    int parentIndex = -1;
    glm::mat4 offsetMatrix{1.0f};       ///< Inverse bind pose matrix
    glm::mat4 localTransform{1.0f};     ///< Local transform
    glm::mat4 globalTransform{1.0f};    ///< Global transform at bind pose

    // Hierarchy info
    std::vector<int> childIndices;
    int depth = 0;
};

/**
 * @brief Imported skeleton data
 */
struct FBXSkeletonData {
    std::string name;
    std::vector<FBXBoneData> bones;
    glm::mat4 globalInverseTransform{1.0f};

    /// Find bone index by name
    [[nodiscard]] int FindBoneIndex(const std::string& boneName) const;

    /// Convert to engine Skeleton
    [[nodiscard]] std::unique_ptr<Skeleton> ToSkeleton() const;

    /// Get bone count
    [[nodiscard]] size_t GetBoneCount() const { return bones.size(); }
};

/**
 * @brief Animation keyframe
 */
struct FBXKeyframe {
    float time = 0.0f;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

/**
 * @brief Animation channel (keyframes for one bone)
 */
struct FBXAnimationChannel {
    std::string boneName;
    std::vector<FBXKeyframe> keyframes;

    /// Interpolate transform at time
    [[nodiscard]] FBXKeyframe Interpolate(float time) const;

    /// Get duration
    [[nodiscard]] float GetDuration() const;
};

/**
 * @brief Imported animation data
 */
struct FBXAnimationData {
    std::string name;
    float duration = 0.0f;
    float ticksPerSecond = 25.0f;
    bool looping = false;

    std::vector<FBXAnimationChannel> channels;

    /// Find channel by bone name
    [[nodiscard]] const FBXAnimationChannel* FindChannel(const std::string& boneName) const;

    /// Convert to engine Animation
    [[nodiscard]] std::unique_ptr<Animation> ToAnimation() const;

    /// Get channel count
    [[nodiscard]] size_t GetChannelCount() const { return channels.size(); }
};

/**
 * @brief Animation event/marker
 */
struct FBXAnimationEvent {
    std::string name;
    float time = 0.0f;
    std::string data;
};

/**
 * @brief LOD mesh chain
 */
struct FBXLODChain {
    std::vector<FBXMeshData> levels;
    std::vector<float> distances;
    std::vector<float> reductionRatios;
};

/**
 * @brief Complete FBX import result
 */
struct FBXImportResult {
    // Source info
    std::string sourcePath;
    std::string sourceFileName;

    // Meshes
    std::vector<FBXMeshData> meshes;
    std::vector<FBXLODChain> lodChains;

    // Materials
    std::vector<FBXMaterialData> materials;

    // Skeleton
    std::optional<FBXSkeletonData> skeleton;

    // Animations
    std::vector<FBXAnimationData> animations;
    std::vector<FBXAnimationEvent> animationEvents;

    // SDF (if conversion enabled)
    std::vector<std::unique_ptr<SDFModel>> sdfModels;

    // Scene hierarchy
    glm::mat4 rootTransform{1.0f};

    // Bounds
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};

    // Statistics
    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;
    uint32_t totalBones = 0;
    uint32_t totalAnimationClips = 0;
    double importTimeMs = 0.0;

    // Status
    bool success = false;
    std::string errorMessage;
    std::vector<std::string> warnings;

    // -------------------------------------------------------------------------
    // Convenience Methods
    // -------------------------------------------------------------------------

    /// Check if import was successful
    [[nodiscard]] bool IsValid() const { return success && !meshes.empty(); }

    /// Check if model has skeleton
    [[nodiscard]] bool HasSkeleton() const { return skeleton.has_value(); }

    /// Check if model has animations
    [[nodiscard]] bool HasAnimations() const { return !animations.empty(); }

    /// Get all engine meshes
    [[nodiscard]] std::vector<std::unique_ptr<Mesh>> GetMeshes() const;

    /// Get all engine materials
    [[nodiscard]] std::vector<std::shared_ptr<Material>> GetMaterials() const;

    /// Get engine skeleton
    [[nodiscard]] std::unique_ptr<Skeleton> GetSkeleton() const;

    /// Get all engine animations
    [[nodiscard]] std::vector<std::unique_ptr<Animation>> GetAnimations() const;

    /// Get summary string
    [[nodiscard]] std::string GetSummary() const;
};

// ============================================================================
// FBX Importer Class
// ============================================================================

/**
 * @brief FBX file importer using Assimp
 *
 * Example usage:
 * @code{.cpp}
 * Nova::FBXImporter importer;
 *
 * // Configure options
 * Nova::FBXImportOptions options;
 * options.importAnimations = true;
 * options.generateLODs = true;
 * options.convertToSDF = false;
 *
 * // Import
 * auto result = importer.Import("character.fbx", options);
 *
 * if (result.success) {
 *     auto meshes = result.GetMeshes();
 *     auto materials = result.GetMaterials();
 *     auto skeleton = result.GetSkeleton();
 *     auto animations = result.GetAnimations();
 * }
 * @endcode
 */
class FBXImporter {
public:
    FBXImporter();
    ~FBXImporter();

    // Non-copyable
    FBXImporter(const FBXImporter&) = delete;
    FBXImporter& operator=(const FBXImporter&) = delete;

    // -------------------------------------------------------------------------
    // Import Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Import FBX file with options
     * @param path Path to FBX file
     * @param options Import options
     * @return Import result with all data
     */
    [[nodiscard]] FBXImportResult Import(const std::string& path,
                                          const FBXImportOptions& options = {});

    /**
     * @brief Import FBX from memory
     * @param data File data
     * @param size Data size
     * @param hint Format hint (e.g., "fbx")
     * @param options Import options
     * @return Import result
     */
    [[nodiscard]] FBXImportResult ImportFromMemory(const void* data,
                                                    size_t size,
                                                    const std::string& hint,
                                                    const FBXImportOptions& options = {});

    /**
     * @brief Quick import with default options
     * @param path Path to FBX file
     * @return Import result
     */
    [[nodiscard]] FBXImportResult Import(const std::string& path) {
        return Import(path, FBXImportOptions{});
    }

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    /**
     * @brief Check if file is a valid FBX
     * @param path File path
     * @return True if file can be imported
     */
    [[nodiscard]] bool CanImport(const std::string& path) const;

    /**
     * @brief Get file info without full import
     * @param path File path
     * @return Basic file information
     */
    struct FileInfo {
        bool valid = false;
        size_t meshCount = 0;
        size_t materialCount = 0;
        size_t boneCount = 0;
        size_t animationCount = 0;
        std::string formatVersion;
        std::string creator;
    };
    [[nodiscard]] FileInfo GetFileInfo(const std::string& path) const;

    // -------------------------------------------------------------------------
    // Utility Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Get supported file extensions
     */
    [[nodiscard]] static std::vector<std::string> GetSupportedExtensions();

    /**
     * @brief Check if extension is supported
     */
    [[nodiscard]] static bool IsExtensionSupported(const std::string& extension);

    /**
     * @brief Get default import options
     */
    [[nodiscard]] static FBXImportOptions GetDefaultOptions();

    /**
     * @brief Get optimized options for static meshes
     */
    [[nodiscard]] static FBXImportOptions GetStaticMeshOptions();

    /**
     * @brief Get optimized options for skeletal meshes
     */
    [[nodiscard]] static FBXImportOptions GetSkeletalMeshOptions();

    /**
     * @brief Get optimized options for animation-only import
     */
    [[nodiscard]] static FBXImportOptions GetAnimationOnlyOptions();

    // -------------------------------------------------------------------------
    // Error Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& GetLastError() const { return m_lastError; }

    /**
     * @brief Clear error state
     */
    void ClearError() { m_lastError.clear(); }

private:
    // Assimp importer (forward declared, impl in cpp)
    class Impl;
    std::unique_ptr<Impl> m_impl;

    std::string m_lastError;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert Assimp matrix to GLM
 */
glm::mat4 ConvertAssimpMatrix(const struct aiMatrix4x4& matrix);

/**
 * @brief Convert Assimp vector to GLM
 */
glm::vec3 ConvertAssimpVector(const struct aiVector3D& vector);

/**
 * @brief Convert Assimp quaternion to GLM
 */
glm::quat ConvertAssimpQuaternion(const struct aiQuaternion& quat);

/**
 * @brief Convert Assimp color to GLM
 */
glm::vec4 ConvertAssimpColor(const struct aiColor4D& color);

/**
 * @brief Calculate coordinate system transformation matrix
 */
glm::mat4 CalculateCoordinateSystemTransform(UpAxis sourceUp, UpAxis targetUp,
                                              FrontAxis sourceFront, FrontAxis targetFront);

} // namespace Nova
