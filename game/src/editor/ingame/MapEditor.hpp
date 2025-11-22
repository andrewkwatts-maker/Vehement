#pragma once

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Texture;
}

namespace Vehement {

// Forward declarations
class InGameEditor;
class World;
class MapFile;

/**
 * @brief Map editing tool types
 */
enum class MapTool : uint8_t {
    Select,         // Select objects
    Terrain,        // Paint terrain height
    Texture,        // Paint terrain textures
    Water,          // Set water level
    Cliff,          // Create cliffs
    PlaceObject,    // Place objects (units, buildings, doodads)
    Region,         // Define regions
    TriggerZone,    // Create trigger zones
    SpawnPoint,     // Place spawn points
    Resource,       // Place resource nodes
    Pathing,        // Edit pathfinding data
    Eraser          // Remove objects
};

/**
 * @brief Terrain brush shape
 */
enum class BrushShape : uint8_t {
    Circle,
    Square,
    Diamond
};

/**
 * @brief Terrain brush settings
 */
struct TerrainBrush {
    BrushShape shape = BrushShape::Circle;
    float size = 4.0f;
    float strength = 0.5f;
    float falloff = 0.3f;
    bool smooth = true;
};

/**
 * @brief Height tool mode
 */
enum class HeightMode : uint8_t {
    Raise,
    Lower,
    Smooth,
    Plateau,
    Noise,
    Flatten
};

/**
 * @brief Texture layer for terrain
 */
struct TerrainTextureLayer {
    std::string textureId;
    std::string texturePath;
    float tilingScale = 1.0f;
    glm::vec4 tint = glm::vec4(1.0f);
    std::shared_ptr<Nova::Texture> texture;
};

/**
 * @brief Placed object on map
 */
struct PlacedObject {
    uint32_t id = 0;
    std::string objectType;         // "unit", "building", "doodad", "item"
    std::string objectId;           // Reference to object definition
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    int player = 0;                 // Owner player (0 = neutral)
    std::unordered_map<std::string, std::string> properties;
    bool isSelected = false;
};

/**
 * @brief Map region definition
 */
struct MapRegion {
    uint32_t id = 0;
    std::string name;
    std::string type;               // "playable", "camera_bounds", "custom"
    glm::vec2 min = glm::vec2(0.0f);
    glm::vec2 max = glm::vec2(0.0f);
    glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 0.3f);
    std::vector<glm::vec2> polygon;  // For non-rectangular regions
    bool isRectangle = true;
};

/**
 * @brief Trigger zone for events
 */
struct TriggerZone {
    uint32_t id = 0;
    std::string name;
    glm::vec2 center = glm::vec2(0.0f);
    float radius = 5.0f;
    bool isCircle = true;
    std::vector<glm::vec2> polygon;  // For polygon zones
    std::string triggerId;           // Associated trigger
    glm::vec4 color = glm::vec4(0.0f, 0.0f, 1.0f, 0.3f);
};

/**
 * @brief Spawn point definition
 */
struct SpawnPoint {
    uint32_t id = 0;
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    float rotation = 0.0f;
    int player = 0;                 // Which player spawns here
    std::string spawnType;          // "player_start", "creep", "boss", etc.
    float respawnTime = 0.0f;       // 0 = no respawn
};

/**
 * @brief Resource node on map
 */
struct ResourceNode {
    uint32_t id = 0;
    std::string resourceType;       // "gold", "wood", "food", etc.
    glm::vec3 position = glm::vec3(0.0f);
    int amount = 1000;
    int maxAmount = 1000;
    float respawnTime = 0.0f;
};

/**
 * @brief Map layer for organization
 */
struct MapLayer {
    std::string name;
    bool visible = true;
    bool locked = false;
    float opacity = 1.0f;
    std::vector<uint32_t> objectIds;
};

/**
 * @brief Map editor command for undo/redo
 */
class MapEditorCommand {
public:
    virtual ~MapEditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
};

/**
 * @brief Map Editor for creating game maps
 *
 * Provides comprehensive map creation tools including:
 * - Terrain painting (height, textures, water)
 * - Object placement (units, buildings, doodads)
 * - Region definition
 * - Trigger zones
 * - Spawn points
 * - Resource placement
 * - Pathfinding preview
 */
class MapEditor {
public:
    MapEditor();
    ~MapEditor();

    // Non-copyable
    MapEditor(const MapEditor&) = delete;
    MapEditor& operator=(const MapEditor&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(InGameEditor& parent);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Map Creation/Loading
    // =========================================================================

    bool CreateNew(int width, int height);
    bool LoadFromFile(const MapFile& file);
    void SaveToFile(MapFile& file) const;
    void ApplyToWorld(World& world);
    void RestoreFromWorld(World& world);

    // =========================================================================
    // Update and Render
    // =========================================================================

    void Update(float deltaTime);
    void Render();
    void RenderMinimap();
    void ProcessInput();

    // =========================================================================
    // Tool Selection
    // =========================================================================

    void SetTool(MapTool tool);
    [[nodiscard]] MapTool GetTool() const noexcept { return m_currentTool; }

    void SetHeightMode(HeightMode mode);
    [[nodiscard]] HeightMode GetHeightMode() const noexcept { return m_heightMode; }

    // =========================================================================
    // Brush Settings
    // =========================================================================

    void SetBrush(const TerrainBrush& brush);
    [[nodiscard]] const TerrainBrush& GetBrush() const noexcept { return m_brush; }

    void SetBrushSize(float size);
    void SetBrushStrength(float strength);
    void SetBrushShape(BrushShape shape);

    // =========================================================================
    // Terrain Textures
    // =========================================================================

    void AddTextureLayer(const TerrainTextureLayer& layer);
    void RemoveTextureLayer(int index);
    void SetCurrentTextureLayer(int index);
    [[nodiscard]] int GetCurrentTextureLayer() const noexcept { return m_currentTextureLayer; }
    [[nodiscard]] const std::vector<TerrainTextureLayer>& GetTextureLayers() const {
        return m_textureLayers;
    }

    // =========================================================================
    // Water
    // =========================================================================

    void SetWaterLevel(float level);
    [[nodiscard]] float GetWaterLevel() const noexcept { return m_waterLevel; }
    void SetWaterEnabled(bool enabled);
    [[nodiscard]] bool IsWaterEnabled() const noexcept { return m_waterEnabled; }

    // =========================================================================
    // Object Management
    // =========================================================================

    uint32_t PlaceObject(const PlacedObject& obj);
    void RemoveObject(uint32_t id);
    void UpdateObject(uint32_t id, const PlacedObject& obj);
    [[nodiscard]] PlacedObject* GetObject(uint32_t id);
    [[nodiscard]] const std::vector<PlacedObject>& GetObjects() const { return m_objects; }

    void SetCurrentObjectType(const std::string& type, const std::string& id);
    [[nodiscard]] const std::string& GetCurrentObjectType() const { return m_currentObjectType; }
    [[nodiscard]] const std::string& GetCurrentObjectId() const { return m_currentObjectId; }

    // =========================================================================
    // Selection
    // =========================================================================

    void SelectObject(uint32_t id);
    void DeselectObject(uint32_t id);
    void SelectAll();
    void DeselectAll();
    void DeleteSelected();
    [[nodiscard]] const std::vector<uint32_t>& GetSelectedObjects() const { return m_selectedObjects; }

    void SetSelectionRect(const glm::vec2& start, const glm::vec2& end);
    void SelectInRect(const glm::vec2& min, const glm::vec2& max);

    // =========================================================================
    // Regions
    // =========================================================================

    uint32_t CreateRegion(const MapRegion& region);
    void UpdateRegion(uint32_t id, const MapRegion& region);
    void RemoveRegion(uint32_t id);
    [[nodiscard]] MapRegion* GetRegion(uint32_t id);
    [[nodiscard]] const std::vector<MapRegion>& GetRegions() const { return m_regions; }

    // =========================================================================
    // Trigger Zones
    // =========================================================================

    uint32_t CreateTriggerZone(const TriggerZone& zone);
    void UpdateTriggerZone(uint32_t id, const TriggerZone& zone);
    void RemoveTriggerZone(uint32_t id);
    [[nodiscard]] TriggerZone* GetTriggerZone(uint32_t id);
    [[nodiscard]] const std::vector<TriggerZone>& GetTriggerZones() const { return m_triggerZones; }

    // =========================================================================
    // Spawn Points
    // =========================================================================

    uint32_t CreateSpawnPoint(const SpawnPoint& spawn);
    void UpdateSpawnPoint(uint32_t id, const SpawnPoint& spawn);
    void RemoveSpawnPoint(uint32_t id);
    [[nodiscard]] SpawnPoint* GetSpawnPoint(uint32_t id);
    [[nodiscard]] const std::vector<SpawnPoint>& GetSpawnPoints() const { return m_spawnPoints; }

    // =========================================================================
    // Resources
    // =========================================================================

    uint32_t CreateResourceNode(const ResourceNode& node);
    void UpdateResourceNode(uint32_t id, const ResourceNode& node);
    void RemoveResourceNode(uint32_t id);
    [[nodiscard]] ResourceNode* GetResourceNode(uint32_t id);
    [[nodiscard]] const std::vector<ResourceNode>& GetResourceNodes() const { return m_resourceNodes; }

    // =========================================================================
    // Layers
    // =========================================================================

    int CreateLayer(const std::string& name);
    void RemoveLayer(int index);
    void SetLayerVisible(int index, bool visible);
    void SetLayerLocked(int index, bool locked);
    void SetCurrentLayer(int index);
    [[nodiscard]] int GetCurrentLayer() const noexcept { return m_currentLayer; }
    [[nodiscard]] const std::vector<MapLayer>& GetLayers() const { return m_layers; }

    // =========================================================================
    // Pathfinding Preview
    // =========================================================================

    void SetShowPathfinding(bool show);
    [[nodiscard]] bool IsShowingPathfinding() const noexcept { return m_showPathfinding; }
    void UpdatePathfindingPreview();

    // =========================================================================
    // Grid and Snapping
    // =========================================================================

    void SetShowGrid(bool show);
    [[nodiscard]] bool IsShowingGrid() const noexcept { return m_showGrid; }
    void SetGridSize(float size);
    [[nodiscard]] float GetGridSize() const noexcept { return m_gridSize; }
    void SetSnapToGrid(bool snap);
    [[nodiscard]] bool IsSnappingToGrid() const noexcept { return m_snapToGrid; }
    glm::vec3 SnapPosition(const glm::vec3& pos) const;

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    void ExecuteCommand(std::unique_ptr<MapEditorCommand> command);
    void Undo();
    void Redo();
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    void ClearHistory();

    // =========================================================================
    // Map Properties
    // =========================================================================

    [[nodiscard]] int GetWidth() const noexcept { return m_width; }
    [[nodiscard]] int GetHeight() const noexcept { return m_height; }
    [[nodiscard]] const std::vector<float>& GetHeightmap() const { return m_heightmap; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(uint32_t)> OnObjectPlaced;
    std::function<void(uint32_t)> OnObjectRemoved;
    std::function<void(uint32_t)> OnObjectSelected;
    std::function<void()> OnMapModified;

private:
    // Input handling
    void HandleTerrainPaint(const glm::vec2& worldPos);
    void HandleTexturePaint(const glm::vec2& worldPos);
    void HandleWaterPaint(const glm::vec2& worldPos);
    void HandleObjectPlacement(const glm::vec3& worldPos);
    void HandleSelection(const glm::vec2& screenPos);

    // Terrain painting helpers
    void ApplyBrush(const glm::vec2& center, std::function<void(int, int, float)> operation);
    float GetBrushWeight(const glm::vec2& brushCenter, int x, int y) const;
    void RaiseHeight(int x, int y, float weight);
    void LowerHeight(int x, int y, float weight);
    void SmoothHeight(int x, int y, float weight);
    void PlateauHeight(int x, int y, float weight, float targetHeight);
    void FlattenHeight(int x, int y, float weight, float targetHeight);

    // ID generation
    uint32_t GenerateObjectId();
    uint32_t GenerateRegionId();
    uint32_t GenerateZoneId();
    uint32_t GenerateSpawnId();
    uint32_t GenerateResourceId();

    // Rendering helpers
    void RenderGrid();
    void RenderObjects();
    void RenderRegions();
    void RenderTriggerZones();
    void RenderSpawnPoints();
    void RenderResourceNodes();
    void RenderSelectionRect();
    void RenderBrushPreview();
    void RenderPathfindingOverlay();

    // State
    bool m_initialized = false;
    InGameEditor* m_parent = nullptr;

    // Map dimensions
    int m_width = 128;
    int m_height = 128;

    // Terrain data
    std::vector<float> m_heightmap;
    std::vector<uint8_t> m_textureBlendMap;
    std::vector<TerrainTextureLayer> m_textureLayers;
    int m_currentTextureLayer = 0;

    // Water
    float m_waterLevel = 0.0f;
    bool m_waterEnabled = false;

    // Current tool state
    MapTool m_currentTool = MapTool::Select;
    HeightMode m_heightMode = HeightMode::Raise;
    TerrainBrush m_brush;
    float m_plateauTarget = 0.0f;

    // Object placement
    std::string m_currentObjectType;
    std::string m_currentObjectId;
    int m_currentPlayer = 0;

    // Map data
    std::vector<PlacedObject> m_objects;
    std::vector<MapRegion> m_regions;
    std::vector<TriggerZone> m_triggerZones;
    std::vector<SpawnPoint> m_spawnPoints;
    std::vector<ResourceNode> m_resourceNodes;
    std::vector<MapLayer> m_layers;

    // Selection
    std::vector<uint32_t> m_selectedObjects;
    glm::vec2 m_selectionStart = glm::vec2(0.0f);
    glm::vec2 m_selectionEnd = glm::vec2(0.0f);
    bool m_isSelecting = false;

    // Layer state
    int m_currentLayer = 0;

    // Grid and snapping
    bool m_showGrid = true;
    float m_gridSize = 1.0f;
    bool m_snapToGrid = true;

    // Pathfinding preview
    bool m_showPathfinding = false;
    std::vector<uint8_t> m_pathfindingPreview;

    // Undo/Redo
    std::deque<std::unique_ptr<MapEditorCommand>> m_undoStack;
    std::deque<std::unique_ptr<MapEditorCommand>> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    // ID counters
    uint32_t m_nextObjectId = 1;
    uint32_t m_nextRegionId = 1;
    uint32_t m_nextZoneId = 1;
    uint32_t m_nextSpawnId = 1;
    uint32_t m_nextResourceId = 1;

    // Input state
    bool m_isPainting = false;
    glm::vec2 m_lastPaintPos = glm::vec2(0.0f);
};

} // namespace Vehement
