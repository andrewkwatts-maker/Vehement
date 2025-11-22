#pragma once

#include "GeoTypes.hpp"
#include <functional>
#include <unordered_map>

namespace Vehement {
namespace Geo {

/**
 * @brief Processed building in game coordinates
 */
struct ProcessedBuilding {
    int64_t id = 0;
    std::string name;
    BuildingType type = BuildingType::Unknown;

    std::vector<glm::vec2> outline;          // Footprint polygon in game coords
    std::vector<std::vector<glm::vec2>> holes;  // Interior holes

    float height = 10.0f;                    // Building height in game units
    float minHeight = 0.0f;                  // Ground offset
    int levels = 3;
    int minLevel = 0;

    BuildingMaterial material = BuildingMaterial::Unknown;
    RoofType roofType = RoofType::Flat;
    float roofHeight = 0.0f;

    glm::vec3 wallColor = glm::vec3(0.8f);
    glm::vec3 roofColor = glm::vec3(0.5f);

    glm::vec2 centroid{0.0f};                // Pre-calculated centroid
    glm::vec2 boundsMin{0.0f};
    glm::vec2 boundsMax{0.0f};
    float area = 0.0f;                       // Pre-calculated area

    /**
     * @brief Check if point is inside building footprint
     */
    bool Contains(const glm::vec2& point) const;

    /**
     * @brief Get footprint perimeter
     */
    float GetPerimeter() const;
};

/**
 * @brief Building lot (plot of land)
 */
struct BuildingLot {
    int64_t id = 0;
    std::vector<glm::vec2> outline;
    glm::vec2 centroid{0.0f};
    float area = 0.0f;

    int64_t buildingId = -1;                 // Building on this lot (-1 if empty)
    LandUseType zoning = LandUseType::Unknown;

    // For procedural building placement
    float suitability = 1.0f;                // How suitable for building (0-1)
    bool canBuild = true;
};

/**
 * @brief Building placement parameters
 */
struct BuildingPlacementParams {
    float minDistanceBetweenBuildings = 2.0f;
    float roadSetback = 5.0f;               // Distance from road edge
    float minLotArea = 100.0f;              // Minimum lot area (sq meters)
    float maxBuildingCoverage = 0.6f;       // Max building footprint / lot area
    bool alignToRoads = true;
    bool alignToNeighbors = true;
};

/**
 * @brief Building height estimator
 */
class BuildingHeightEstimator {
public:
    /**
     * @brief Estimate height from building type
     */
    static float EstimateFromType(BuildingType type);

    /**
     * @brief Estimate height from area (larger area = taller building)
     */
    static float EstimateFromArea(float area, BuildingType type);

    /**
     * @brief Estimate height from context (surrounding buildings)
     */
    static float EstimateFromContext(
        const GeoBuilding& building,
        const std::vector<GeoBuilding>& neighbors);

    /**
     * @brief Get average floor height for building type
     */
    static float GetFloorHeight(BuildingType type);

    /**
     * @brief Estimate levels from height
     */
    static int HeightToLevels(float height, BuildingType type);
};

/**
 * @brief Building footprint processor
 *
 * Processes geographic building data into game-ready format:
 * - Coordinate transformation
 * - Height estimation
 * - Mesh generation
 * - LOD support
 */
class BuildingFootprints {
public:
    /**
     * @brief Coordinate transform callback
     */
    using CoordTransform = std::function<glm::vec2(const GeoCoordinate&)>;

    BuildingFootprints();
    ~BuildingFootprints();

    /**
     * @brief Set coordinate transformation
     */
    void SetCoordinateTransform(CoordTransform transform) { m_transform = transform; }

    /**
     * @brief Set default transform (meters from origin)
     */
    void SetDefaultTransform(const GeoCoordinate& origin, float scale = 1.0f);

    // ==========================================================================
    // Processing
    // ==========================================================================

    /**
     * @brief Process buildings from geographic data
     */
    int ProcessBuildings(const std::vector<GeoBuilding>& buildings);

    /**
     * @brief Process single building
     */
    void ProcessBuilding(const GeoBuilding& building);

    /**
     * @brief Estimate heights for buildings without explicit heights
     */
    void EstimateHeights();

    /**
     * @brief Full processing pipeline
     */
    void ProcessAll(const std::vector<GeoBuilding>& buildings);

    /**
     * @brief Clear all data
     */
    void Clear();

    // ==========================================================================
    // Access
    // ==========================================================================

    /**
     * @brief Get processed buildings
     */
    const std::vector<ProcessedBuilding>& GetBuildings() const { return m_buildings; }

    /**
     * @brief Get building by ID
     */
    const ProcessedBuilding* GetBuilding(int64_t id) const;

    /**
     * @brief Get buildings in bounds
     */
    std::vector<int64_t> GetBuildingsInBounds(
        const glm::vec2& min, const glm::vec2& max) const;

    /**
     * @brief Get buildings by type
     */
    std::vector<int64_t> GetBuildingsByType(BuildingType type) const;

    // ==========================================================================
    // Queries
    // ==========================================================================

    /**
     * @brief Find building at point
     */
    int64_t FindBuildingAt(const glm::vec2& point) const;

    /**
     * @brief Find nearest building to point
     */
    std::pair<int64_t, float> FindNearestBuilding(const glm::vec2& point) const;

    /**
     * @brief Get building coverage in area
     */
    float GetBuildingCoverage(const glm::vec2& min, const glm::vec2& max) const;

    /**
     * @brief Get average building height in area
     */
    float GetAverageHeight(const glm::vec2& min, const glm::vec2& max) const;

    // ==========================================================================
    // Mesh Generation
    // ==========================================================================

    /**
     * @brief Building mesh vertex
     */
    struct BuildingVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec3 color;
    };

    /**
     * @brief Building mesh data
     */
    struct BuildingMesh {
        std::vector<BuildingVertex> vertices;
        std::vector<uint32_t> indices;
        int64_t buildingId = 0;
    };

    /**
     * @brief Generate mesh for a building
     */
    BuildingMesh GenerateBuildingMesh(int64_t buildingId) const;

    /**
     * @brief Generate mesh for a building (direct)
     */
    BuildingMesh GenerateBuildingMesh(const ProcessedBuilding& building) const;

    /**
     * @brief Generate simplified mesh for LOD
     */
    BuildingMesh GenerateLODMesh(const ProcessedBuilding& building, int lodLevel) const;

    /**
     * @brief Generate meshes for buildings in bounds
     */
    std::vector<BuildingMesh> GenerateMeshesInBounds(
        const glm::vec2& min, const glm::vec2& max) const;

    /**
     * @brief Generate combined mesh for all buildings in bounds
     */
    BuildingMesh GenerateCombinedMesh(
        const glm::vec2& min, const glm::vec2& max) const;

    // ==========================================================================
    // Placement Data
    // ==========================================================================

    /**
     * @brief Generate placement data for procedural generation
     */
    struct PlacementData {
        glm::vec2 position;
        float rotation;                      // Rotation angle (radians)
        glm::vec2 footprintSize;             // Approximate bounding box
        float height;
        BuildingType suggestedType;
    };

    /**
     * @brief Get building placement data for area
     */
    std::vector<PlacementData> GetPlacementData(
        const glm::vec2& min, const glm::vec2& max) const;

    // ==========================================================================
    // Utilities
    // ==========================================================================

    /**
     * @brief Calculate polygon area
     */
    static float CalculatePolygonArea(const std::vector<glm::vec2>& polygon);

    /**
     * @brief Calculate polygon centroid
     */
    static glm::vec2 CalculateCentroid(const std::vector<glm::vec2>& polygon);

    /**
     * @brief Check if point is inside polygon
     */
    static bool PointInPolygon(const glm::vec2& point,
                                const std::vector<glm::vec2>& polygon);

    /**
     * @brief Simplify polygon (reduce vertices)
     */
    static std::vector<glm::vec2> SimplifyPolygon(
        const std::vector<glm::vec2>& polygon, float tolerance);

    /**
     * @brief Get oriented bounding box
     */
    static void GetOrientedBoundingBox(
        const std::vector<glm::vec2>& polygon,
        glm::vec2& center, glm::vec2& halfExtents, float& rotation);

private:
    /**
     * @brief Transform geographic coordinates
     */
    glm::vec2 TransformCoord(const GeoCoordinate& coord) const;

    /**
     * @brief Generate wall mesh for building
     */
    void GenerateWalls(const ProcessedBuilding& building,
                       std::vector<BuildingVertex>& vertices,
                       std::vector<uint32_t>& indices) const;

    /**
     * @brief Generate roof mesh
     */
    void GenerateRoof(const ProcessedBuilding& building,
                      std::vector<BuildingVertex>& vertices,
                      std::vector<uint32_t>& indices) const;

    /**
     * @brief Triangulate polygon for roof
     */
    std::vector<uint32_t> TriangulatePolygon(
        const std::vector<glm::vec2>& polygon) const;

    CoordTransform m_transform;
    GeoCoordinate m_origin;
    float m_scale = 1.0f;

    std::vector<ProcessedBuilding> m_buildings;
    std::unordered_map<int64_t, size_t> m_buildingIndex;
};

} // namespace Geo
} // namespace Vehement
