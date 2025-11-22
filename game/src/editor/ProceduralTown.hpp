#pragma once

#include "LevelEditor.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <random>
#include <string>
#include <memory>

namespace Vehement {

// Forward declarations
class TileMap;

/**
 * @brief Procedural Town Generator for Vehement2
 *
 * Generates random towns with varied structures including:
 * - Roads in grid or organic patterns
 * - Buildings with walls and interior floors
 * - Parks with trees and landscaping
 * - Town center/plaza areas
 * - Residential neighborhoods
 * - Commercial/industrial areas
 * - Decorative elements and details
 *
 * Uses the existing Vehement2 tile types to create varied environments.
 */
class ProceduralTown {
public:
    /**
     * @brief Zone types for town layout
     */
    enum class ZoneType {
        Empty,
        Residential,
        Commercial,
        Industrial,
        Park,
        Plaza,
        Road,
        Water
    };

    /**
     * @brief Building style presets
     */
    enum class BuildingStyle {
        Brick,      ///< Classic brick buildings
        Stone,      ///< Stone/marble buildings
        Wood,       ///< Wooden structures
        Metal,      ///< Industrial metal buildings
        Mixed       ///< Mix of materials
    };

    /**
     * @brief Road layout patterns
     */
    enum class RoadPattern {
        Grid,       ///< Regular grid pattern
        Organic,    ///< Curved, organic roads
        Radial,     ///< Roads radiating from center
        Mixed       ///< Combination of patterns
    };

    /**
     * @brief Town generation parameters
     */
    struct TownParams {
        // Size
        int width = 100;                    ///< Town width in tiles
        int height = 100;                   ///< Town height in tiles

        // Randomization
        int seed = 0;                       ///< Random seed (0 = random)

        // Density settings (0.0 - 1.0)
        float roadDensity = 0.3f;           ///< Amount of road coverage
        float buildingDensity = 0.4f;       ///< Amount of building coverage
        float parkDensity = 0.1f;           ///< Amount of park coverage
        float waterDensity = 0.05f;         ///< Amount of water features

        // Road settings
        RoadPattern roadPattern = RoadPattern::Grid;
        int mainRoadWidth = 3;              ///< Width of main roads
        int sideRoadWidth = 2;              ///< Width of side roads
        int blockSizeMin = 8;               ///< Minimum block size
        int blockSizeMax = 16;              ///< Maximum block size

        // Building settings
        BuildingStyle defaultStyle = BuildingStyle::Brick;
        int buildingMinSize = 4;            ///< Minimum building dimension
        int buildingMaxSize = 12;           ///< Maximum building dimension
        float wallHeight = 2.5f;            ///< Default wall height
        float tallBuildingChance = 0.1f;    ///< Chance of taller buildings

        // Zone settings
        bool hasTownCenter = true;          ///< Create central plaza
        float residentialRatio = 0.5f;      ///< Ratio of residential zones
        float commercialRatio = 0.3f;       ///< Ratio of commercial zones
        float industrialRatio = 0.2f;       ///< Ratio of industrial zones

        // Detail settings
        float foliageDensity = 0.3f;        ///< Amount of trees/plants
        float decorationDensity = 0.2f;     ///< Amount of decorative objects
        bool addStreetLights = true;        ///< Add street lights
        bool addBenches = true;             ///< Add park benches
        bool addGarbage = false;            ///< Add garbage/debris

        // Validation
        [[nodiscard]] bool IsValid() const noexcept;
        void Clamp();                       ///< Clamp values to valid ranges
    };

    /**
     * @brief Result of town generation
     */
    struct GenerationResult {
        bool success = false;
        std::string errorMessage;

        // Statistics
        int totalTiles = 0;
        int roadTiles = 0;
        int buildingTiles = 0;
        int parkTiles = 0;
        int waterTiles = 0;
        int objectTiles = 0;

        int buildingCount = 0;
        int treeCount = 0;
        int decorationCount = 0;
    };

    /**
     * @brief Building definition for generation
     */
    struct Building {
        int x = 0, y = 0;
        int width = 0, height = 0;
        BuildingStyle style = BuildingStyle::Brick;
        float wallHeight = 2.5f;
        bool hasEntrance = true;
        int entranceSide = 0;  // 0=south, 1=east, 2=north, 3=west
        ZoneType zone = ZoneType::Residential;
    };

    /**
     * @brief Park/green space definition
     */
    struct Park {
        int x = 0, y = 0;
        int width = 0, height = 0;
        bool hasFountain = false;
        bool hasPaths = true;
        float treeDensity = 0.3f;
    };

public:
    ProceduralTown();
    ~ProceduralTown();

    /**
     * @brief Generate a new town
     * @param params Generation parameters
     * @return Generated tile map (caller takes ownership)
     */
    [[nodiscard]] static std::unique_ptr<TileMap> Generate(const TownParams& params);

    /**
     * @brief Generate with result information
     * @param params Generation parameters
     * @param outResult Output generation statistics
     * @return Generated tile map
     */
    [[nodiscard]] static std::unique_ptr<TileMap> Generate(const TownParams& params, GenerationResult& outResult);

    /**
     * @brief Apply generation to existing tile map
     * @param map Tile map to modify
     * @param params Generation parameters
     * @param offsetX X offset in the map
     * @param offsetY Y offset in the map
     * @return Generation result
     */
    static GenerationResult ApplyToMap(TileMap& map, const TownParams& params, int offsetX = 0, int offsetY = 0);

    // -------------------------------------------------------------------------
    // Individual Generation Functions (for custom generation pipelines)
    // -------------------------------------------------------------------------

    /**
     * @brief Generate the zone layout
     * @param map Output zone map (ZoneType values)
     * @param params Town parameters
     */
    static void GenerateZoneLayout(std::vector<ZoneType>& zoneMap, int width, int height, const TownParams& params, std::mt19937& rng);

    /**
     * @brief Generate roads based on zone layout
     */
    static void GenerateRoads(TileMap& map, const TownParams& params, std::mt19937& rng);

    /**
     * @brief Generate buildings in building zones
     */
    static void GenerateBuildings(TileMap& map, const TownParams& params, std::mt19937& rng);

    /**
     * @brief Generate parks and green spaces
     */
    static void GenerateParks(TileMap& map, const TownParams& params, std::mt19937& rng);

    /**
     * @brief Generate water features
     */
    static void GenerateWater(TileMap& map, const TownParams& params, std::mt19937& rng);

    /**
     * @brief Add details like trees, decorations, etc.
     */
    static void GenerateDetails(TileMap& map, const TownParams& params, std::mt19937& rng);

    /**
     * @brief Generate town center/plaza
     */
    static void GenerateTownCenter(TileMap& map, const TownParams& params, std::mt19937& rng);

private:
    // Internal generation helpers
    static void FillBackground(TileMap& map, const TownParams& params, std::mt19937& rng);
    static void CreateGridRoads(TileMap& map, const TownParams& params, std::mt19937& rng);
    static void CreateOrganicRoads(TileMap& map, const TownParams& params, std::mt19937& rng);
    static void CreateRadialRoads(TileMap& map, const TownParams& params, std::mt19937& rng);

    static Building PlaceBuilding(TileMap& map, int x, int y, int maxW, int maxH,
                                  BuildingStyle style, const TownParams& params, std::mt19937& rng);
    static void RenderBuilding(TileMap& map, const Building& building, std::mt19937& rng);
    static void RenderBuildingWalls(TileMap& map, const Building& building, std::mt19937& rng);
    static void RenderBuildingFloor(TileMap& map, const Building& building, std::mt19937& rng);
    static void RenderBuildingEntrance(TileMap& map, const Building& building, std::mt19937& rng);

    static void RenderPark(TileMap& map, const Park& park, std::mt19937& rng);
    static void PlaceTrees(TileMap& map, int x, int y, int width, int height, float density, std::mt19937& rng);
    static void PlaceDecorations(TileMap& map, int x, int y, int width, int height, float density, std::mt19937& rng);

    static void CreatePath(TileMap& map, int x1, int y1, int x2, int y2, int width, TileType tileType, std::mt19937& rng);
    static void CreateRoad(TileMap& map, int x, int y, int length, bool horizontal, int width, std::mt19937& rng);

    // Tile selection helpers
    static TileType GetGroundTile(std::mt19937& rng);
    static TileType GetRoadTile(std::mt19937& rng);
    static TileType GetWallTile(BuildingStyle style, std::mt19937& rng);
    static TileType GetFloorTile(BuildingStyle style, std::mt19937& rng);
    static TileType GetTreeTile(std::mt19937& rng);
    static TileType GetParkGroundTile(std::mt19937& rng);
    static TileType GetDecorationTile(ZoneType zone, std::mt19937& rng);
    static TileType GetCornerTile(int cornerType, std::mt19937& rng);

    // Area checking
    static bool IsAreaEmpty(const TileMap& map, int x, int y, int width, int height);
    static bool IsAreaType(const TileMap& map, int x, int y, int width, int height, TileType type);
    static bool HasAdjacentRoad(const TileMap& map, int x, int y, int width, int height);
};

/**
 * @brief Simple TileMap implementation for generation
 *
 * This is a standalone implementation for the procedural generator.
 * In a real game, this would be replaced with the actual game's TileMap class.
 */
class TileMap {
public:
    struct Tile {
        TileType type = TileType::Empty;
        uint8_t variant = 0;
        bool isWall = false;
        float wallHeight = 0.0f;
    };

    TileMap(int width, int height);
    ~TileMap() = default;

    // Tile access
    [[nodiscard]] TileType GetTile(int x, int y) const;
    [[nodiscard]] uint8_t GetVariant(int x, int y) const;
    [[nodiscard]] bool IsWall(int x, int y) const;
    [[nodiscard]] float GetWallHeight(int x, int y) const;
    [[nodiscard]] const Tile* GetTileData(int x, int y) const;

    void SetTile(int x, int y, TileType type, uint8_t variant = 0);
    void SetWall(int x, int y, bool isWall, float height = 0.0f);

    // Dimensions
    [[nodiscard]] int GetWidth() const noexcept { return m_width; }
    [[nodiscard]] int GetHeight() const noexcept { return m_height; }

    // Bounds checking
    [[nodiscard]] bool InBounds(int x, int y) const noexcept {
        return x >= 0 && y >= 0 && x < m_width && y < m_height;
    }

    // Fill operations
    void Fill(TileType type, uint8_t variant = 0);
    void FillRect(int x, int y, int width, int height, TileType type, uint8_t variant = 0);

    // Raw access for serialization
    [[nodiscard]] const std::vector<Tile>& GetTiles() const noexcept { return m_tiles; }
    std::vector<Tile>& GetTiles() noexcept { return m_tiles; }

private:
    int m_width;
    int m_height;
    std::vector<Tile> m_tiles;

    [[nodiscard]] size_t GetIndex(int x, int y) const noexcept {
        return static_cast<size_t>(y) * m_width + x;
    }
};

} // namespace Vehement
