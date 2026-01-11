#pragma once

/**
 * @file MeshToSDFConverter.hpp
 * @brief Convert triangle meshes to SDF primitive representations
 *
 * This system analyzes triangle meshes and converts them into efficient SDF
 * primitive approximations using multiple strategies:
 * - Primitive fitting (spheres, boxes, capsules, etc.)
 * - Convex decomposition
 * - Voxelization fallback for complex geometry
 * - Hierarchical LOD generation
 *
 * Features:
 * - Automatic primitive type detection
 * - Error metric calculation
 * - CSG tree construction
 * - Skeletal animation binding
 * - Multi-threaded processing
 *
 * Example usage:
 * @code
 * MeshToSDFConverter converter;
 * ConversionSettings settings;
 * settings.maxPrimitives = 40;
 * settings.strategy = ConversionStrategy::PrimitiveFitting;
 *
 * auto result = converter.Convert(mesh, settings);
 * if (result.success) {
 *     sdfModel.SetRoot(std::move(result.rootPrimitive));
 * }
 * @endcode
 */

#include "../sdf/SDFPrimitive.hpp"
#include "../sdf/SDFModel.hpp"
#include "Mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace Nova {

// Forward declarations
class Skeleton;

/**
 * @brief Conversion strategy for mesh to SDF
 */
enum class ConversionStrategy {
    PrimitiveFitting,      // Fit best-matching primitives (default)
    ConvexDecomposition,   // Decompose into convex hulls, then primitives
    Voxelization,          // Grid-based voxel approximation
    Hybrid,                // Combine strategies based on mesh complexity
    Auto                   // Automatically select best strategy
};

/**
 * @brief Primitive fitting quality
 */
enum class FittingQuality {
    Fast,       // Quick fitting, lower quality
    Balanced,   // Good balance of speed and quality (default)
    High,       // High quality, slower
    Perfect     // Best possible fit, very slow
};

/**
 * @brief Settings for mesh to SDF conversion
 */
struct ConversionSettings {
    // Strategy
    ConversionStrategy strategy = ConversionStrategy::Auto;
    FittingQuality quality = FittingQuality::Balanced;

    // Primitive limits
    int maxPrimitives = 40;           // Maximum number of primitives
    int minPrimitives = 1;            // Minimum number of primitives
    float errorThreshold = 0.05f;     // Stop when error < this (0-1)

    // Primitive fitting
    bool allowSpheres = true;
    bool allowBoxes = true;
    bool allowCapsules = true;
    bool allowCylinders = true;
    bool allowCones = true;
    bool allowTorus = false;          // Usually not needed

    // CSG operations
    bool useCSG = true;               // Enable CSG tree generation
    float smoothFactor = 0.1f;        // Smooth blend factor for CSG

    // Voxelization (fallback)
    int voxelResolution = 32;         // Grid resolution
    float voxelThreshold = 0.5f;      // Occupancy threshold

    // LOD generation
    bool generateLODs = true;
    std::vector<int> lodPrimitiveCounts = {40, 12, 6, 3};  // Primitives per LOD
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};

    // Skeletal animation
    bool bindToSkeleton = false;      // Compute bone weights
    const Skeleton* skeleton = nullptr;

    // Performance
    bool useMultiThreading = true;
    int numThreads = 0;               // 0 = auto-detect

    // Output
    bool verbose = false;             // Print detailed progress
    std::function<void(float)> progressCallback = nullptr;  // Progress 0-1
};

/**
 * @brief Result of primitive fitting
 */
struct PrimitiveFitResult {
    SDFPrimitiveType type = SDFPrimitiveType::Sphere;
    glm::vec3 position{0.0f};
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    SDFParameters parameters;

    float error = 0.0f;               // RMS error (lower is better)
    float coverage = 0.0f;            // Percentage of triangles covered (0-1)
    float importance = 1.0f;          // Importance score for LOD sorting

    std::vector<int> triangleIndices; // Which triangles this primitive covers

    // Convert to SDFPrimitive
    std::unique_ptr<SDFPrimitive> ToPrimitive(const std::string& name = "") const;
};

/**
 * @brief Result of mesh to SDF conversion
 */
struct ConversionResult {
    bool success = false;
    std::string errorMessage;

    // Output primitives
    std::unique_ptr<SDFPrimitive> rootPrimitive;
    std::vector<PrimitiveFitResult> allPrimitives;  // All fitted primitives

    // LOD levels (indices into allPrimitives, sorted by importance)
    std::vector<std::vector<int>> lodLevels;

    // Statistics
    float totalError = 0.0f;          // Overall approximation error
    float avgError = 0.0f;            // Average per-triangle error
    float maxError = 0.0f;            // Maximum per-triangle error
    int primitiveCount = 0;
    float conversionTimeMs = 0.0f;

    // Memory usage
    size_t originalTriangleCount = 0;
    size_t originalVertexCount = 0;
    size_t estimatedMemorySavings = 0;  // Bytes saved vs triangle mesh
};

/**
 * @brief Triangle data for fitting
 */
struct Triangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
    glm::vec3 centroid;
    float area;

    Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

    float DistanceToPoint(const glm::vec3& point) const;
    bool Contains(const glm::vec3& point) const;
};

/**
 * @brief Mesh to SDF Converter
 *
 * Converts triangle meshes into efficient SDF primitive representations.
 * Supports multiple conversion strategies and automatic LOD generation.
 */
class MeshToSDFConverter {
public:
    MeshToSDFConverter();
    ~MeshToSDFConverter();

    // Non-copyable
    MeshToSDFConverter(const MeshToSDFConverter&) = delete;
    MeshToSDFConverter& operator=(const MeshToSDFConverter&) = delete;

    // =========================================================================
    // Conversion
    // =========================================================================

    /**
     * @brief Convert mesh to SDF primitives
     * @param mesh Input triangle mesh
     * @param settings Conversion settings
     * @return Conversion result with primitives and statistics
     */
    ConversionResult Convert(const Mesh& mesh, const ConversionSettings& settings = {});

    /**
     * @brief Convert mesh with vertex data
     */
    ConversionResult Convert(const std::vector<Vertex>& vertices,
                            const std::vector<uint32_t>& indices,
                            const ConversionSettings& settings = {});

    /**
     * @brief Fit primitives to mesh (primitive fitting strategy)
     */
    std::vector<PrimitiveFitResult> FitPrimitives(
        const std::vector<Triangle>& triangles,
        int maxPrimitives,
        const ConversionSettings& settings);

    // =========================================================================
    // Primitive Fitting Algorithms
    // =========================================================================

    /**
     * @brief Fit best primitive to triangle set
     */
    PrimitiveFitResult FitBestPrimitive(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& triangleIndices,
        const ConversionSettings& settings);

    /**
     * @brief Fit sphere to triangles
     */
    PrimitiveFitResult FitSphere(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices);

    /**
     * @brief Fit box (OBB) to triangles
     */
    PrimitiveFitResult FitBox(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices);

    /**
     * @brief Fit capsule to triangles
     */
    PrimitiveFitResult FitCapsule(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices);

    /**
     * @brief Fit cylinder to triangles
     */
    PrimitiveFitResult FitCylinder(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices);

    /**
     * @brief Fit cone to triangles
     */
    PrimitiveFitResult FitCone(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices);

    // =========================================================================
    // Error Metrics
    // =========================================================================

    /**
     * @brief Calculate RMS error between primitive and triangles
     */
    float CalculateError(
        const PrimitiveFitResult& primitive,
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices);

    /**
     * @brief Calculate per-triangle coverage
     */
    void CalculateCoverage(
        const std::vector<PrimitiveFitResult>& primitives,
        const std::vector<Triangle>& triangles,
        std::vector<bool>& outCovered);

    // =========================================================================
    // LOD Generation
    // =========================================================================

    /**
     * @brief Generate LOD levels from primitives
     * @return LOD levels (indices into primitives, sorted by importance)
     */
    std::vector<std::vector<int>> GenerateLODs(
        const std::vector<PrimitiveFitResult>& primitives,
        const std::vector<int>& primitiveCounts);

    /**
     * @brief Sort primitives by importance for LOD
     */
    void SortByImportance(std::vector<PrimitiveFitResult>& primitives);

    // =========================================================================
    // Skeletal Binding
    // =========================================================================

    /**
     * @brief Compute bone weights for primitives
     */
    void ComputeBoneWeights(
        std::vector<PrimitiveFitResult>& primitives,
        const Skeleton& skeleton);

private:
    // Strategy implementations
    ConversionResult ConvertPrimitiveFitting(
        const std::vector<Triangle>& triangles,
        const ConversionSettings& settings);

    ConversionResult ConvertConvexDecomposition(
        const std::vector<Triangle>& triangles,
        const ConversionSettings& settings);

    ConversionResult ConvertVoxelization(
        const std::vector<Triangle>& triangles,
        const ConversionSettings& settings);

    // Helper methods
    std::vector<Triangle> BuildTriangleList(
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices);

    void BuildCSGTree(
        std::vector<PrimitiveFitResult>& primitives,
        const ConversionSettings& settings);

    ConversionStrategy SelectBestStrategy(
        const std::vector<Triangle>& triangles,
        const ConversionSettings& settings);

    // Progressive fitting
    void FindHighestErrorRegion(
        const std::vector<Triangle>& triangles,
        const std::vector<bool>& covered,
        std::vector<int>& outRegion);

    // PCA and geometric analysis
    void ComputePCA(
        const std::vector<Triangle>& triangles,
        const std::vector<int>& indices,
        glm::vec3& outCenter,
        glm::vec3& outAxis1,
        glm::vec3& outAxis2,
        glm::vec3& outAxis3);

    // Statistics
    struct ConversionStats {
        int totalTriangles = 0;
        int primitivesGenerated = 0;
        float avgError = 0.0f;
        float maxError = 0.0f;
    } m_stats;
};

} // namespace Nova
