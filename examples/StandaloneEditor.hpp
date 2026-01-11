#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <imgui.h>

namespace Nova {
    class Renderer;
    class Camera;
    class Mesh;
    class DebugDraw;
}

class WorldMapEditor;
class LocalMapEditor;
class PCGGraphEditor;
class CommandHistory;
class AssetBrowser;

#include "SettingsMenu.hpp"

/**
 * @brief Standalone editor for RTSApplication
 *
 * Supports two types of maps:
 * 1. World Maps - Global maps using lat/long coordinates with PCG-based generation
 * 2. Local Maps - Small instance maps (arenas, dungeons, etc.)
 *
 * Features:
 * - PCG node graph system for procedural generation
 * - Real-world data integration (elevation, roads, buildings, biomes)
 * - Terrain painting and sculpting
 * - Asset browser and placement
 * - Material editor
 */
class StandaloneEditor {
public:
    enum class EditorType {
        WorldMap,   // Global world using lat/long
        LocalMap,   // Local instance map
        PCGGraph    // PCG node graph editor
    };

    enum class EditMode {
        None,
        TerrainPaint,    // Paint terrain tiles
        TerrainSculpt,   // Raise/lower terrain
        ObjectPlace,     // Place objects/units/buildings
        ObjectSelect,    // Select and transform objects
        MaterialEdit,    // Edit materials
        PCGEdit          // Edit PCG graphs
    };

    enum class TerrainBrush {
        Grass,
        Dirt,
        Stone,
        Sand,
        Water,
        Raise,   // Raise terrain height
        Lower    // Lower terrain height
    };

    enum class TransformTool {
        None,
        Move,
        Rotate,
        Scale
    };

    enum class GizmoAxis {
        None,
        X,
        Y,
        Z,
        XY,      // For plane handles
        XZ,
        YZ,
        Center   // For uniform scale or center handle
    };

    enum class BrushFalloff {
        Linear,
        Smooth,
        Spherical
    };

    StandaloneEditor();
    ~StandaloneEditor();

    /**
     * @brief Initialize the editor
     */
    bool Initialize();

    /**
     * @brief Apply custom editor theme
     */
    void ApplyEditorTheme();

    /**
     * @brief Panel docking zones
     */
    enum class DockZone {
        None,
        Left,
        Right,
        Top,
        Bottom,
        Center,
        Floating
    };

    /**
     * @brief Panel identifiers
     */
    enum class PanelID {
        Viewport,
        Tools,
        ContentBrowser,
        Details,
        MaterialEditor,
        EngineStats
    };

    /**
     * @brief Panel layout configuration
     */
    struct PanelLayout {
        PanelID id;
        DockZone zone;
        float splitRatio = 0.5f;  // For multiple panels in same zone
        bool isVisible = true;
        glm::vec2 position{0.0f, 0.0f};  // Position for floating panels
        glm::vec2 size{400.0f, 300.0f};  // Size for floating panels and docked zones
    };

    /**
     * @brief Setup default panel layout
     */
    void SetupDefaultLayout();

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
     * @brief Render 3D scene
     */
    void Render3D(Nova::Renderer& renderer, const Nova::Camera& camera);

    /**
     * @brief Process input
     */
    void ProcessInput();

    /**
     * @brief Check if editor is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Get current edit mode
     */
    EditMode GetEditMode() const { return m_editMode; }

    /**
     * @brief Set edit mode
     */
    void SetEditMode(EditMode mode);

    /**
     * @brief Set transform tool
     */
    void SetTransformTool(TransformTool tool);

    /**
     * @brief Get current editor type
     */
    EditorType GetEditorType() const { return m_editorType; }

    /**
     * @brief Switch editor type
     */
    void SwitchEditorType(EditorType type);

    /**
     * @brief New map (simple version)
     */
    void NewMap(int width, int height);

    /**
     * @brief New world map
     */
    void NewWorldMap();

    /**
     * @brief New local map
     */
    void NewLocalMap(int width, int height);

    /**
     * @brief New PCG graph
     */
    void NewPCGGraph();

    /**
     * @brief Load map from file
     */
    bool LoadMap(const std::string& path);

    /**
     * @brief Save map to file
     */
    bool SaveMap(const std::string& path);

private:
    // UI rendering
    void RenderMenuBar();
    void RenderToolbar();
    void RenderAssetBrowser();
    void RenderTerrainPanel();
    void RenderObjectPanel();
    void RenderMaterialPanel();
    void RenderPropertiesPanel();
    void RenderStatusBar();
    void RenderToolsPanel();
    void RenderContentBrowser();
    void RenderDetailsPanel();
    void RenderViewportControls();

    // ImGui DockSpace system
    void SetupDefaultDockLayout();
    void RenderPanelWindow(PanelLayout& layout);
    const char* GetPanelName(PanelID panel);
    void RenderPanelContent(PanelID panel);

    // Unified panel rendering (merged from separate panels)
    void RenderUnifiedToolsPanel();
    void RenderUnifiedContentBrowser();
    void RenderDetailsContent();
    void RenderMaterialEditorContent();
    void RenderEngineStatsContent();

    // Debug overlays
    void RenderDebugOverlay();
    void RenderProfiler();
    void RenderMemoryStats();
    void RenderTimeDistribution();

    // Dialogs
    void ShowNewMapDialog();
    void ShowLoadMapDialog();
    void ShowSaveMapDialog();
    void ShowAboutDialog();
    void ShowControlsDialog();
    void ShowMapPropertiesDialog();

    // Native file dialogs
    std::string OpenNativeFileDialog(const char* filter, const char* title);
    std::string SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt);

    // Recent files management
    void LoadRecentFiles();
    void SaveRecentFiles();
    void AddToRecentFiles(const std::string& path);
    void ClearRecentFiles();

    // Import/Export
    bool ImportHeightmap(const std::string& path);
    bool ExportHeightmap(const std::string& path);

    // Terrain editing
    void PaintTerrain(int x, int y);
    void SculptTerrain(int x, int y, float strength);
    float CalculateBrushFalloff(float distance, float radius);

    // Object editing
    void PlaceObject(const glm::vec3& position, const std::string& objectType);
    void SelectObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    void SelectObjectAtScreenPos(int x, int y);
    void SelectObjectByIndex(int index, bool addToSelection);
    void ClearSelection();
    void DeleteSelectedObject();
    void DeleteSelectedObjects();
    void SelectAllObjects();
    void CopySelectedObjects();
    void PasteObjects();
    void TransformSelectedObject();
    void RenderSelectionOutline();
    void RenderTransformGizmo(Nova::DebugDraw& debugDraw);
    void RenderMoveGizmo(Nova::DebugDraw& debugDraw, const glm::vec3& position);
    void RenderRotateGizmo(Nova::DebugDraw& debugDraw, const glm::vec3& position, const glm::vec3& rotation);
    void RenderScaleGizmo(Nova::DebugDraw& debugDraw, const glm::vec3& position, const glm::vec3& scale);

    // Gizmo interaction
    void UpdateGizmoInteraction(float deltaTime);
    GizmoAxis RaycastMoveGizmo(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& gizmoPos);
    GizmoAxis RaycastRotateGizmo(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& gizmoPos);
    GizmoAxis RaycastScaleGizmo(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& gizmoPos);
    void ApplyMoveTransform(GizmoAxis axis, const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    void ApplyRotateTransform(GizmoAxis axis, const glm::vec2& mouseDelta);
    void ApplyScaleTransform(GizmoAxis axis, const glm::vec2& mouseDelta);
    glm::vec4 GetGizmoAxisColor(GizmoAxis axis, GizmoAxis hoveredAxis, GizmoAxis draggedAxis);

    glm::vec3 ScreenToWorldRay(int screenX, int screenY);
    bool RayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                           const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                           float& distance);
    bool RayIntersectsCylinder(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                              const glm::vec3& cylinderStart, const glm::vec3& cylinderEnd,
                              float radius, float& distance);
    bool RayIntersectsSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                            const glm::vec3& sphereCenter, float radius, float& distance);
    float RayToLineDistance(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                           const glm::vec3& linePoint, const glm::vec3& lineDir);

    // Material editing
    void RenderTextureSlot(const char* label, std::string& texturePath);
    void SaveMaterial(const std::string& path);
    void LoadMaterial(const std::string& path);
    void ApplyMaterialToSelected();
    void ResetMaterial();
    void LoadGoldPreset();
    void LoadChromePreset();
    void LoadPlasticPreset();
    void LoadWoodPreset();
    void LoadGlassPreset();
    void AddToMaterialHistory();

    // Material data structure
    struct Material {
        std::string name = "New Material";

        // PBR Properties
        glm::vec3 albedoColor{1.0f, 1.0f, 1.0f};
        std::string albedoTexture = "";

        float metallic = 0.0f;
        std::string metallicTexture = "";

        float roughness = 0.5f;
        std::string roughnessTexture = "";

        std::string normalMap = "";
        float normalStrength = 1.0f;

        glm::vec3 emissiveColor{0.0f, 0.0f, 0.0f};
        std::string emissiveTexture = "";
        float emissiveStrength = 1.0f;

        // UV Settings
        glm::vec2 uvTiling{1.0f, 1.0f};
        glm::vec2 uvOffset{0.0f, 0.0f};

        // Shader
        std::string shaderName = "pbr_standard";
    };

    struct MaterialHistoryEntry {
        std::string name;
        std::string path;
    };

    // Scene object data structure
    struct SceneObject {
        std::string name = "Object";
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 rotation{0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 boundingBoxMin{-0.5f, -0.5f, -0.5f};
        glm::vec3 boundingBoxMax{0.5f, 0.5f, 0.5f};
    };

    // State
    bool m_initialized = false;
    EditorType m_editorType = EditorType::LocalMap;  // Default to local map
    EditMode m_editMode = EditMode::None;
    TerrainBrush m_selectedBrush = TerrainBrush::Grass;
    TransformTool m_transformTool = TransformTool::None;
    int m_brushSize = 1;
    float m_brushStrength = 1.0f;
    BrushFalloff m_brushFalloff = BrushFalloff::Linear;

    // Sub-editors
    std::unique_ptr<WorldMapEditor> m_worldMapEditor;
    std::unique_ptr<LocalMapEditor> m_localMapEditor;
    std::unique_ptr<PCGGraphEditor> m_pcgGraphEditor;
    bool m_showWorldMapEditor = false;
    bool m_showLocalMapEditor = false;
    bool m_showPCGEditor = false;

    // Asset browser
    std::unique_ptr<AssetBrowser> m_assetBrowser;

    // Settings menu
    std::unique_ptr<SettingsMenu> m_settingsMenu;

    // Command history for undo/redo
    std::unique_ptr<CommandHistory> m_commandHistory;

    // Map data
    int m_mapWidth = 64;
    int m_mapHeight = 64;
    std::vector<int> m_terrainTiles;
    std::vector<float> m_terrainHeights;
    bool m_terrainMeshDirty = false;  // Flag to indicate terrain needs regeneration

    // File paths
    std::string m_currentMapPath;
    std::string m_assetDirectory = "assets/";
    std::vector<std::string> m_recentFiles;

    // Selection
    int m_selectedObjectIndex = -1;
    glm::vec3 m_selectedObjectPosition{0.0f};
    glm::vec3 m_selectedObjectRotation{0.0f};
    glm::vec3 m_selectedObjectScale{1.0f};
    std::vector<int> m_selectedObjectIndices;
    bool m_isMultiSelectMode = false;

    // Gizmo interaction state
    bool m_gizmoDragging = false;
    GizmoAxis m_dragAxis = GizmoAxis::None;
    GizmoAxis m_hoveredAxis = GizmoAxis::None;
    glm::vec2 m_dragStartMousePos{0.0f};
    glm::vec3 m_dragStartObjectPos{0.0f};
    glm::vec3 m_dragStartObjectRot{0.0f};
    glm::vec3 m_dragStartObjectScale{1.0f};
    glm::vec3 m_dragPlaneNormal{0.0f, 1.0f, 0.0f};  // For constrained movement
    float m_dragStartDistance = 0.0f;  // Distance from camera to object at drag start
    bool m_snapToGridEnabled = false;  // Override for snapping during transform
    float m_snapAngle = 15.0f;  // Degrees for rotation snapping
    float m_snapDistance = 0.5f;  // Units for translation snapping

    // Scene objects
    std::vector<SceneObject> m_sceneObjects;

    // Camera
    enum class CameraMode {
        Free,        // Free-look perspective camera
        Top,         // Orthographic top-down view
        Front,       // Orthographic front view
        Side         // Orthographic side view
    };
    CameraMode m_cameraMode = CameraMode::Free;
    glm::vec3 m_editorCameraPos{0.0f, 20.0f, 20.0f};
    glm::vec3 m_editorCameraTarget{0.0f};
    glm::vec3 m_defaultCameraPos{0.0f, 20.0f, 20.0f};
    glm::vec3 m_defaultCameraTarget{0.0f};
    float m_cameraDistance = 30.0f;
    float m_cameraAngle = 45.0f;
    const Nova::Camera* m_currentCamera = nullptr;  // Camera reference for ray-picking

    // UI state
    bool m_showAssetBrowser = true;
    bool m_showTerrainPanel = true;
    bool m_showObjectPanel = false;
    bool m_showMaterialPanel = false;
    bool m_showPropertiesPanel = true;

    // Dialog state
    bool m_showNewMapDialog = false;
    bool m_showLoadMapDialog = false;
    bool m_showSaveMapDialog = false;
    bool m_showAboutDialog = false;
    bool m_showControlsDialog = false;
    bool m_showMapPropertiesDialog = false;
    bool m_showSettingsDialog = false;

    // Map properties
    std::string m_mapName = "Untitled";
    std::string m_mapDescription = "";
    glm::vec3 m_mapAmbientLight{0.3f, 0.3f, 0.4f};
    glm::vec3 m_mapDirectionalLight{1.0f, 0.9f, 0.8f};
    glm::vec3 m_mapFogColor{0.5f, 0.6f, 0.7f};
    float m_mapFogDensity = 0.01f;
    std::string m_mapSkybox = "default";

    // Spherical world properties
    enum class WorldType {
        Flat,       // Traditional flat map
        Spherical   // Spherical world (planet)
    };
    WorldType m_worldType = WorldType::Flat;
    float m_worldRadius = 6371.0f;  // Default to Earth radius in km
    glm::vec3 m_worldCenter{0.0f};  // Center of spherical world
    bool m_showSphericalGrid = true; // Show wireframe sphere for spherical worlds

    // Terrain height range
    float m_minHeight = -100.0f;   // Minimum terrain height in meters
    float m_maxHeight = 8848.0f;   // Maximum terrain height in meters (default: Mt. Everest)

    // Grid visualization
    bool m_showGrid = true;
    bool m_showGizmos = true;
    bool m_snapToGrid = true;
    float m_gridSize = 1.0f;
    bool m_showWireframe = false;  // Wireframe overlay
    bool m_showNormals = false;     // Debug normal vectors

    // Panel visibility (linked to PanelLayout system)
    bool m_showDetailsPanel = true;
    bool m_showToolsPanel = true;
    bool m_showContentBrowser = true;
    bool m_showMaterialEditor = false;

    // Docking system
    std::vector<PanelLayout> m_panelLayouts;

    // Debug overlay toggles
    bool m_showDebugOverlay = false;
    bool m_showProfiler = false;
    bool m_showMemoryStats = false;
    bool m_showRenderTime = false;
    bool m_showUpdateTime = false;
    bool m_showPhysicsTime = false;

    // Performance tracking data
    std::vector<float> m_fpsHistory;
    std::vector<float> m_frameTimeHistory;
    int m_historyMaxSize = 100;

    // Material editor state
    Material m_currentMaterial;
    Material m_savedMaterial;  // For cancel/revert
    std::vector<std::string> m_availableShaders{"pbr_standard", "unlit", "custom"};
    std::vector<MaterialHistoryEntry> m_materialHistory;
    int m_maxMaterialHistorySize = 5;
    float m_materialPreviewRotation = 0.0f;
};

