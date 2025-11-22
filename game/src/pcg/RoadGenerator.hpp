#pragma once

#include "PCGPipeline.hpp"
#include <vector>

namespace Vehement {

/**
 * @brief Road generation parameters
 */
struct RoadParams {
    // Road appearance
    TileType highwayTile = TileType::ConcreteAsphalt2;
    TileType mainRoadTile = TileType::ConcreteAsphalt1;
    TileType secondaryRoadTile = TileType::ConcreteAsphalt1;
    TileType residentialTile = TileType::ConcreteAsphalt1;
    TileType pathTile = TileType::GroundDirt;

    // Road widths (in tiles)
    int highwayWidth = 4;
    int mainRoadWidth = 3;
    int secondaryRoadWidth = 2;
    int residentialWidth = 2;
    int pathWidth = 1;

    // Generation
    bool useRealData = true;            // Use GeoRoad data
    bool generateProcedural = false;    // Generate roads procedurally
    bool connectDisconnected = true;    // Connect isolated road segments
    bool addSidewalks = true;
    TileType sidewalkTile = TileType::ConcreteTiles1;

    // Intersection handling
    bool smoothIntersections = true;
    bool addCrosswalks = false;

    // Pathfinding for connections
    float roadCost = 0.5f;              // Cost to follow existing road
    float terrainCost = 1.0f;           // Cost to create new road
    float waterCost = 100.0f;           // High cost to cross water
};

/**
 * @brief Road segment for internal processing
 */
struct RoadSegment {
    glm::ivec2 start;
    glm::ivec2 end;
    RoadType type = RoadType::None;
    int width = 1;
    bool processed = false;
};

/**
 * @brief Road intersection
 */
struct RoadIntersection {
    glm::ivec2 position;
    std::vector<int> connectedSegments;  // Indices into segment list
    int degree = 0;                       // Number of connections
};

/**
 * @brief Road placement from real data
 *
 * Processes:
 * - Convert GeoRoad data to tile roads
 * - Path-finding for road connections
 * - Intersection handling
 * - Road surface tile assignment
 *
 * Python script hook: road_*.py
 */
class RoadGenerator : public PCGStageGenerator {
public:
    RoadGenerator();
    ~RoadGenerator() override = default;

    // PCGStageGenerator interface
    PCGStageResult Generate(PCGContext& context, PCGMode mode) override;
    PCGStage GetStage() const override { return PCGStage::Roads; }
    const char* GetName() const override { return "RoadGenerator"; }

    // Configuration
    void SetParams(const RoadParams& params) { m_params = params; }
    RoadParams& GetParams() { return m_params; }
    const RoadParams& GetParams() const { return m_params; }

    // Individual generation steps

    /**
     * @brief Convert GeoRoad data to segments
     */
    void ConvertGeoRoads(PCGContext& context, std::vector<RoadSegment>& segments);

    /**
     * @brief Generate procedural road network
     */
    void GenerateProceduralRoads(PCGContext& context, std::vector<RoadSegment>& segments);

    /**
     * @brief Find and process intersections
     */
    void FindIntersections(const std::vector<RoadSegment>& segments,
                           std::vector<RoadIntersection>& intersections);

    /**
     * @brief Rasterize roads to tiles
     */
    void RasterizeRoads(PCGContext& context, const std::vector<RoadSegment>& segments);

    /**
     * @brief Connect disconnected road segments
     */
    void ConnectRoads(PCGContext& context, std::vector<RoadSegment>& segments);

    /**
     * @brief Add sidewalks along roads
     */
    void AddSidewalks(PCGContext& context);

    /**
     * @brief Smooth intersection tiles
     */
    void SmoothIntersections(PCGContext& context,
                              const std::vector<RoadIntersection>& intersections);

private:
    RoadParams m_params;

    // Helpers
    TileType GetTileForRoadType(RoadType type) const;
    int GetWidthForRoadType(RoadType type) const;
    void RasterizeLine(PCGContext& context, glm::ivec2 start, glm::ivec2 end,
                       TileType tile, int width);
    std::vector<glm::ivec2> FindPath(PCGContext& context, glm::ivec2 start, glm::ivec2 end);
};

} // namespace Vehement
