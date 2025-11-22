#pragma once

#include "ImportSettings.hpp"
#include "ImportProgress.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class Mesh;
class Material;
class Skeleton;
class Animation;

// ============================================================================
// Model Data Structures
// ============================================================================

/**
 * @brief Imported vertex data
 */
struct ImportedVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    glm::vec3 tangent{1.0f, 0.0f, 0.0f};
    glm::vec3 bitangent{0.0f, 0.0f, 1.0f};
    glm::ivec4 boneIds{-1};
    glm::vec4 boneWeights{0.0f};
    glm::vec4 color{1.0f};
};

/**
 * @brief Imported mesh data
 */
struct ImportedMesh {
    std::string name;
    std::vector<ImportedVertex> vertices;
    std::vector<uint32_t> indices;

    // Bounds
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    glm::vec3 boundsCenter{0.0f};
    float boundsSphereRadius = 0.0f;

    // Material
    int materialIndex = -1;

    // Statistics
    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    bool hasTangents = false;
    bool hasBoneWeights = false;
    bool hasVertexColors = false;
};

/**
 * @brief LOD level mesh
 */
struct LODMesh {
    ImportedMesh mesh;
    float screenSize = 1.0f;     ///< Screen percentage for switching
    float distance = 0.0f;       ///< Distance for switching
    float reductionRatio = 1.0f; ///< Vertex reduction from LOD0
};

/**
 * @brief Material texture reference
 */
struct MaterialTexture {
    std::string path;
    std::string type;        ///< diffuse, normal, specular, etc.
    int uvChannel = 0;
    glm::vec2 uvScale{1.0f};
    glm::vec2 uvOffset{0.0f};
    bool embedded = false;
    std::vector<uint8_t> embeddedData;
};

/**
 * @brief Imported material data
 */
struct ImportedMaterial {
    std::string name;

    // Colors
    glm::vec4 diffuseColor{1.0f};
    glm::vec4 specularColor{1.0f};
    glm::vec4 emissiveColor{0.0f};

    // PBR properties
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;

    // Legacy properties
    float shininess = 32.0f;
    float opacity = 1.0f;

    // Textures
    std::vector<MaterialTexture> textures;

    // Flags
    bool doubleSided = false;
    bool transparent = false;
    std::string blendMode = "opaque";
};

/**
 * @brief Bone data for skeleton
 */
struct ImportedBone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 offsetMatrix{1.0f};
    glm::mat4 localTransform{1.0f};
};

/**
 * @brief Collision shape types
 */
enum class CollisionShapeType : uint8_t {
    Box,
    Sphere,
    Capsule,
    ConvexHull,
    TriangleMesh
};

/**
 * @brief Collision shape
 */
struct CollisionShape {
    CollisionShapeType type = CollisionShapeType::ConvexHull;
    std::string name;

    // For primitive shapes
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    float height = 1.0f;

    // For mesh shapes
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
};

/**
 * @brief Imported model result
 */
struct ImportedModel {
    std::string sourcePath;
    std::string outputPath;
    std::string assetId;

    // Meshes
    std::vector<ImportedMesh> meshes;
    std::vector<std::vector<LODMesh>> lodChains;  ///< LOD chains per mesh

    // Materials
    std::vector<ImportedMaterial> materials;

    // Skeleton
    std::vector<ImportedBone> bones;
    glm::mat4 globalInverseTransform{1.0f};
    bool hasSkeleton = false;

    // Animations (references)
    std::vector<std::string> animationNames;

    // Collision
    std::vector<CollisionShape> collisionShapes;

    // Bounds
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};

    // Statistics
    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;
    uint32_t totalBones = 0;
    uint32_t totalMaterials = 0;

    // Import info
    bool success = false;
    std::string errorMessage;
    std::vector<std::string> warnings;
};

// ============================================================================
// Model Importer
// ============================================================================

/**
 * @brief Comprehensive 3D model import pipeline
 *
 * Supports: OBJ, FBX, GLTF/GLB, DAE (Collada), 3DS
 *
 * Features:
 * - Mesh optimization (vertex cache, overdraw)
 * - LOD generation
 * - Material extraction
 * - Embedded texture extraction
 * - Skeleton extraction
 * - Coordinate system conversion
 * - Collision mesh generation
 * - Batch import
 */
class ModelImporter {
public:
    ModelImporter();
    ~ModelImporter();

    // Non-copyable
    ModelImporter(const ModelImporter&) = delete;
    ModelImporter& operator=(const ModelImporter&) = delete;

    // -------------------------------------------------------------------------
    // Single Model Import
    // -------------------------------------------------------------------------

    /**
     * @brief Import a single model
     */
    ImportedModel Import(const std::string& path, const ModelImportSettings& settings,
                         ImportProgress* progress = nullptr);

    /**
     * @brief Import with default settings
     */
    ImportedModel Import(const std::string& path);

    // -------------------------------------------------------------------------
    // Batch Import
    // -------------------------------------------------------------------------

    /**
     * @brief Import multiple models
     */
    std::vector<ImportedModel> ImportBatch(const std::vector<std::string>& paths,
                                            const ModelImportSettings& settings,
                                            ImportProgressTracker* tracker = nullptr);

    // -------------------------------------------------------------------------
    // Format-Specific Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load OBJ file
     */
    ImportedModel LoadOBJ(const std::string& path, ImportProgress* progress = nullptr);

    /**
     * @brief Load GLTF/GLB file
     */
    ImportedModel LoadGLTF(const std::string& path, ImportProgress* progress = nullptr);

    /**
     * @brief Load FBX file (simplified parser)
     */
    ImportedModel LoadFBX(const std::string& path, ImportProgress* progress = nullptr);

    /**
     * @brief Load DAE (Collada) file
     */
    ImportedModel LoadDAE(const std::string& path, ImportProgress* progress = nullptr);

    /**
     * @brief Load 3DS file
     */
    ImportedModel Load3DS(const std::string& path, ImportProgress* progress = nullptr);

    // -------------------------------------------------------------------------
    // Mesh Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Optimize mesh for GPU
     */
    void OptimizeMesh(ImportedMesh& mesh);

    /**
     * @brief Optimize vertex cache
     */
    void OptimizeVertexCache(ImportedMesh& mesh);

    /**
     * @brief Optimize overdraw
     */
    void OptimizeOverdraw(ImportedMesh& mesh);

    /**
     * @brief Calculate mesh bounds
     */
    void CalculateBounds(ImportedMesh& mesh);

    /**
     * @brief Calculate model bounds from all meshes
     */
    void CalculateModelBounds(ImportedModel& model);

    /**
     * @brief Generate normals
     */
    void GenerateNormals(ImportedMesh& mesh, bool smooth = true);

    /**
     * @brief Generate tangents
     */
    void GenerateTangents(ImportedMesh& mesh);

    /**
     * @brief Merge duplicate vertices
     */
    void MergeVertices(ImportedMesh& mesh, float threshold = 0.0001f);

    /**
     * @brief Apply transform to mesh
     */
    void TransformMesh(ImportedMesh& mesh, const glm::mat4& transform);

    // -------------------------------------------------------------------------
    // LOD Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate LOD chain for mesh
     */
    std::vector<LODMesh> GenerateLODs(const ImportedMesh& mesh,
                                       const std::vector<float>& reductions,
                                       const std::vector<float>& distances);

    /**
     * @brief Simplify mesh to target triangle count
     */
    ImportedMesh SimplifyMesh(const ImportedMesh& mesh, float targetRatio);

    /**
     * @brief Calculate screen size for LOD switching
     */
    static float CalculateScreenSize(float distance, float boundsSphereRadius, float fov);

    // -------------------------------------------------------------------------
    // Material Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Extract embedded textures
     */
    std::vector<std::pair<std::string, std::vector<uint8_t>>>
        ExtractEmbeddedTextures(const ImportedModel& model);

    /**
     * @brief Convert material to PBR
     */
    void ConvertToPBR(ImportedMaterial& material);

    /**
     * @brief Find texture files relative to model
     */
    std::string FindTextureFile(const std::string& modelPath, const std::string& textureName);

    // -------------------------------------------------------------------------
    // Skeleton Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Build skeleton from bones
     */
    std::unique_ptr<Skeleton> BuildSkeleton(const std::vector<ImportedBone>& bones,
                                             const glm::mat4& globalInverse);

    /**
     * @brief Normalize bone weights
     */
    void NormalizeBoneWeights(ImportedMesh& mesh);

    /**
     * @brief Remove bones exceeding max per vertex
     */
    void LimitBonesPerVertex(ImportedMesh& mesh, int maxBones = 4);

    // -------------------------------------------------------------------------
    // Collision Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate convex hull collision
     */
    CollisionShape GenerateConvexHull(const ImportedMesh& mesh);

    /**
     * @brief Generate bounding box collision
     */
    CollisionShape GenerateBoxCollision(const ImportedMesh& mesh);

    /**
     * @brief Generate sphere collision
     */
    CollisionShape GenerateSphereCollision(const ImportedMesh& mesh);

    /**
     * @brief Convex decomposition for complex meshes
     */
    std::vector<CollisionShape> ConvexDecomposition(const ImportedMesh& mesh,
                                                     int maxHulls = 16,
                                                     int maxVerticesPerHull = 64);

    /**
     * @brief Generate simplified triangle mesh collision
     */
    CollisionShape GenerateTriMeshCollision(const ImportedMesh& mesh, float simplification = 0.5f);

    // -------------------------------------------------------------------------
    // Coordinate System
    // -------------------------------------------------------------------------

    /**
     * @brief Convert coordinate system
     */
    void ConvertCoordinateSystem(ImportedModel& model, bool swapYZ, bool flipWindingOrder);

    /**
     * @brief Apply scale normalization
     */
    void NormalizeScale(ImportedModel& model, float targetSize = 1.0f);

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
     * @brief Detect format from file
     */
    static std::string DetectFormat(const std::string& path);

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    /**
     * @brief Save to engine format
     */
    bool SaveEngineFormat(const ImportedModel& model, const std::string& path);

    /**
     * @brief Export mesh as OBJ
     */
    bool ExportOBJ(const ImportedMesh& mesh, const std::string& path);

    /**
     * @brief Export model metadata
     */
    std::string ExportMetadata(const ImportedModel& model);

private:
    // OBJ parsing helpers
    struct OBJData {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        std::vector<std::tuple<int, int, int>> faces;
        std::vector<std::pair<std::string, int>> materialGroups;
    };

    OBJData ParseOBJ(const std::string& path);
    std::vector<ImportedMaterial> ParseMTL(const std::string& path);

    // GLTF parsing helpers
    struct GLTFData {
        std::vector<uint8_t> buffer;
        // Simplified - full implementation would use JSON parsing
    };

    // Mesh simplification using quadric error metrics
    struct QuadricError {
        glm::mat4 quadric{0.0f};
    };

    void ComputeQuadrics(const ImportedMesh& mesh, std::vector<QuadricError>& quadrics);
    float ComputeEdgeCollapseError(const QuadricError& q1, const QuadricError& q2,
                                    const glm::vec3& v1, const glm::vec3& v2, glm::vec3& optimalPos);
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Calculate face normal
 */
glm::vec3 CalculateFaceNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

/**
 * @brief Calculate tangent and bitangent
 */
void CalculateTangentBitangent(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                                const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                                glm::vec3& tangent, glm::vec3& bitangent);

/**
 * @brief Hash function for vertex deduplication
 */
struct VertexHash {
    size_t operator()(const ImportedVertex& v) const;
};

/**
 * @brief Equality for vertex deduplication
 */
struct VertexEqual {
    bool operator()(const ImportedVertex& a, const ImportedVertex& b) const;
};

} // namespace Nova
