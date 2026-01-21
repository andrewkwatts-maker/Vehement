#pragma once

#include "PCGNodeGraph.hpp"
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

/**
 * @brief Editor for local instance maps
 *
 * Creates small, isolated maps for specific gameplay scenarios:
 * - Battle arenas
 * - Dungeons/caves
 * - Player housing instances
 * - Custom scenarios
 *
 * Local maps are NOT part of the global world and exist in their own coordinate space.
 * They can optionally inherit biome/theme from a global world location.
 */
class LocalMapEditor {
public:
    struct LocalMapConfig {
        // Map dimensions (in world units, e.g., meters)
        int width = 256;
        int height = 256;
        float tileSize = 1.0f;

        // Map type
        enum class MapType {
            Arena,      // PvP/PvE battle arena
            Dungeon,    // PvE dungeon/cave
            Housing,    // Player housing instance
            Scenario,   // Custom scenario map
            Tutorial    // Tutorial map
        };
        MapType type = MapType::Arena;

        // Optional: Inherit from global world location
        bool inheritFromWorld = false;
        double sourceLatitude = 0.0;
        double sourceLongitude = 0.0;

        // PCG settings
        std::string pcgGraphPath;
        uint64_t seed = 12345;

        // Height range
        float minHeight = 0.0f;
        float maxHeight = 50.0f;

        // Theme
        std::string theme = "default";  // grass, desert, snow, lava, etc.
    };

    LocalMapEditor();
    ~LocalMapEditor();

    /**
     * @brief Initialize the local map editor
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
     * @brief Render 3D map view
     */
    void Render3D();

    /**
     * @brief Create new local map
     */
    void NewMap(const LocalMapConfig& config);

    /**
     * @brief Load map from file
     */
    bool LoadMap(const std::string& path);

    /**
     * @brief Save map to file
     */
    bool SaveMap(const std::string& path);

    /**
     * @brief Generate terrain using PCG graph
     */
    void GenerateTerrain();

    /**
     * @brief Paint terrain at position
     */
    void PaintTerrain(int x, int y, const std::string& terrainType);

    /**
     * @brief Sculpt terrain height
     */
    void SculptTerrain(int x, int y, float strength, bool raise);

    /**
     * @brief Place object/asset
     */
    void PlaceAsset(const glm::vec3& position, const std::string& assetType);

    /**
     * @brief Get height at position
     */
    float GetHeightAt(int x, int y);

private:
    // UI panels
    void RenderMenuBar();
    void RenderToolbar();
    void RenderTerrainPanel();
    void RenderAssetPanel();
    void RenderPCGPanel();
    void RenderSpawnPointsPanel();
    void RenderObjectivesPanel();
    void RenderPropertiesPanel();

    // Terrain operations
    void FlattenTerrain();
    void SmoothTerrain();
    void NoiseTerrain();

    // Asset operations
    void PlaceTrees(int count);
    void PlaceRocks(int count);
    void PlaceResources(int count);

    // Spawn points
    void AddPlayerSpawn(const glm::vec3& position);
    void AddEnemySpawn(const glm::vec3& position);
    void RemoveSpawn(int index);

    // Objectives (for scenario maps)
    void AddObjective(const std::string& type, const glm::vec3& position);

    // Dialog rendering
    void RenderOpenDialog();
    void RenderSaveDialog();
    void RenderExportDialog();
    void RenderMapPropertiesDialog();
    void RenderLoadPCGDialog();

    // State
    bool m_initialized = false;
    LocalMapConfig m_config;
    std::string m_currentMapPath;

    // Terrain data
    std::vector<float> m_heights;
    std::vector<int> m_terrainTypes;

    // Assets/objects
    struct PlacedAsset {
        std::string type;
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
    };
    std::vector<PlacedAsset> m_assets;

    // Spawn points
    struct SpawnPoint {
        enum class Type { Player, Enemy, NPC, Boss };
        Type type;
        glm::vec3 position;
        std::string faction;
    };
    std::vector<SpawnPoint> m_spawnPoints;

    // Objectives
    struct Objective {
        std::string type;  // "capture", "defend", "escort", etc.
        glm::vec3 position;
        std::string description;
    };
    std::vector<Objective> m_objectives;

    // PCG system
    std::unique_ptr<PCG::PCGGraph> m_pcgGraph;

    // Camera
    glm::vec3 m_cameraPos{0.0f, 20.0f, 20.0f};
    glm::vec3 m_cameraTarget{0.0f};
    float m_cameraDistance = 30.0f;

    // Tool state
    enum class EditMode {
        TerrainPaint,
        TerrainSculpt,
        AssetPlace,
        SpawnPlace,
        ObjectivePlace
    };
    EditMode m_editMode = EditMode::TerrainPaint;

    int m_brushSize = 3;
    float m_brushStrength = 1.0f;
    std::string m_selectedTerrainType = "grass";
    std::string m_selectedAssetType = "tree";

    // UI state
    bool m_showTerrainPanel = true;
    bool m_showAssetPanel = false;
    bool m_showPCGPanel = false;
    bool m_showSpawnPoints = false;
    bool m_showObjectives = false;
    bool m_showProperties = true;

    // Selection
    int m_selectedAssetIndex = -1;
    int m_selectedSpawnIndex = -1;

    // Dialog states
    bool m_showOpenDialog = false;
    bool m_showSaveDialog = false;
    bool m_showExportDialog = false;
    bool m_showMapPropertiesDialog = false;
    bool m_showLoadPCGDialog = false;
    char m_filePathBuffer[256] = "";
    char m_exportPathBuffer[256] = "";
    char m_pcgGraphPathBuffer[256] = "";
};
