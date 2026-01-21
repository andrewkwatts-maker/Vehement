#pragma once

#include "PCGNodeGraph.hpp"
#include "PCGGraphEditor.hpp"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

/**
 * @brief Editor for global world maps
 *
 * Edits entire world using lat/long coordinates.
 * Supports:
 * - PCG graph-based terrain generation
 * - Real-world data integration (elevation, roads, buildings)
 * - Biome placement
 * - Global resource distribution
 * - LOD management for streaming
 */
class WorldMapEditor {
public:
    struct WorldConfig {
        // World bounds (in degrees)
        double minLatitude = -90.0;
        double maxLatitude = 90.0;
        double minLongitude = -180.0;
        double maxLongitude = 180.0;

        // Tile resolution
        int tilesPerDegree = 100;  // 100 tiles per degree = ~1km resolution at equator

        // Height range (meters)
        float minElevation = -500.0f;  // Below sea level
        float maxElevation = 8848.0f;  // Mount Everest

        // PCG settings
        std::string pcgGraphPath;
        uint64_t worldSeed = 12345;

        // Real-world data sources
        std::string elevationDataPath;
        std::string roadDataPath;
        std::string buildingDataPath;
        std::string biomeDataPath;
    };

    WorldMapEditor();
    ~WorldMapEditor();

    /**
     * @brief Initialize the world editor
     */
    bool Initialize();

    /**
     * @brief Shutdown the editor
     */
    void Shutdown();

    /**
     * @brief Update editor state
     */
    void Update(float deltaTime);

    /**
     * @brief Render editor UI
     */
    void RenderUI();

    /**
     * @brief Render 3D world view
     */
    void Render3D();

    /**
     * @brief Create new world
     */
    void NewWorld(const WorldConfig& config);

    /**
     * @brief Load world from file
     */
    bool LoadWorld(const std::string& path);

    /**
     * @brief Save world to file
     */
    bool SaveWorld(const std::string& path);

    /**
     * @brief Generate terrain for a lat/long region
     */
    void GenerateRegion(double lat, double lon, int radiusTiles);

    /**
     * @brief Get terrain height at lat/long
     */
    float GetHeightAt(double latitude, double longitude);

    /**
     * @brief Set terrain height at lat/long
     */
    void SetHeightAt(double latitude, double longitude, float height);

    /**
     * @brief Navigate to lat/long position
     */
    void NavigateTo(double latitude, double longitude);

private:
    // UI panels
    void RenderMenuBar();
    void RenderToolbar();
    void RenderNavigator();
    void RenderPCGPanel();
    void RenderDataSourcesPanel();
    void RenderGenerationPanel();
    void RenderPropertiesPanel();
    void RenderFileDialogs();

    // Generation
    void GenerateEntireWorld();

    // Terrain data (chunked/streaming) - defined early for use in function declarations
    struct TerrainChunk {
        double centerLat;
        double centerLon;
        int tileSize;
        std::vector<float> heights;
        bool isGenerated = false;
        bool needsRegeneration = false;
    };

    // Terrain generation
    void GenerateTerrain();
    void ApplyPCGGraph();

    // Real-world data loading
    bool LoadElevationData();
    bool LoadRoadData();
    bool LoadBuildingData();
    bool LoadBiomeData();

    // Coordinate conversion
    glm::vec3 LatLongToWorld(double latitude, double longitude, float elevation = 0.0f);
    void WorldToLatLong(const glm::vec3& world, double& latitude, double& longitude);

    // Chunk management
    void GenerateChunkTerrain(TerrainChunk& chunk);
    TerrainChunk* FindChunkAt(double latitude, double longitude);

    // Export
    void ExportRegion(double lat, double lon, int radiusTiles, const std::string& path);

    // Real-world data queries
    float GetRealWorldElevation(double latitude, double longitude);
    float GetRoadDistance(double latitude, double longitude);
    float GetBuildingDistance(double latitude, double longitude);

    // State
    bool m_initialized = false;
    WorldConfig m_config;
    std::string m_currentWorldPath;

    // PCG system
    std::unique_ptr<PCGGraphEditor> m_pcgEditor;
    std::unique_ptr<PCG::PCGGraph> m_pcgGraph;

    // Camera/navigation
    double m_currentLatitude = 0.0;
    double m_currentLongitude = 0.0;
    float m_cameraAltitude = 1000.0f;  // Meters above terrain
    float m_cameraZoom = 1.0f;

    // Loaded chunks
    std::vector<TerrainChunk> m_loadedChunks;

    // Real-world data flags
    bool m_hasElevationData = false;
    bool m_hasRoadData = false;
    bool m_hasBuildingData = false;
    bool m_hasBiomeData = false;

    // Elevation data
    std::vector<float> m_elevationData;
    int m_elevationWidth = 0;
    int m_elevationHeight = 0;
    struct { double minLat, maxLat, minLon, maxLon; } m_elevationBounds = {0, 0, 0, 0};

    // Road data
    struct RoadSegment {
        double startLat, startLon;
        double endLat, endLon;
        int roadType;  // 0=path, 1=residential, 2=highway
    };
    std::vector<RoadSegment> m_roadSegments;

    // Building data
    struct BuildingFootprint {
        double centerLat, centerLon;
        float radius;
        float height;
    };
    std::vector<BuildingFootprint> m_buildingFootprints;

    // Biome data
    std::vector<uint8_t> m_biomeData;
    int m_biomeWidth = 0;
    int m_biomeHeight = 0;
    struct { double minLat, maxLat, minLon, maxLon; } m_biomeBounds = {0, 0, 0, 0};

    // UI state
    bool m_showPCGEditor = false;
    bool m_showNavigator = true;
    bool m_showDataSources = true;
    bool m_showGeneration = true;
    bool m_showProperties = true;

    // Dialog state
    bool m_showOpenDialog = false;
    bool m_showSaveDialog = false;
    bool m_showExportDialog = false;
    bool m_showImportDialog = false;
    bool m_showWorldPropertiesDialog = false;
    bool m_showLoadPCGDialog = false;
    bool m_showSavePCGDialog = false;
    char m_dialogPathBuffer[512] = "";
    char m_exportPathBuffer[512] = "";

    // Generation settings
    bool m_useRealWorldElevation = false;
    bool m_avoidRoads = true;
    bool m_avoidBuildings = true;
    float m_generationScale = 1.0f;
};
