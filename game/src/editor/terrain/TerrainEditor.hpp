#pragma once

#include "../../../../engine/terrain/VoxelTerrain.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Vehement {

// Forward declarations
class Editor;

/**
 * @brief Terrain editing tool type
 */
enum class TerrainToolType {
    Sculpt,         // Add/remove terrain
    Smooth,         // Smooth terrain
    Flatten,        // Flatten to target height
    Raise,          // Raise terrain
    Lower,          // Lower terrain
    Paint,          // Paint material/color
    Tunnel,         // Dig tunnels
    Cave,           // Create caves
    Stamp,          // Stamp SDF shapes
    Erode,          // Erosion simulation
    Path,           // Create paths/roads
    Cliff,          // Create cliffs
    Noise           // Apply noise
};

/**
 * @brief Brush shape for terrain editing
 */
enum class TerrainBrushShape {
    Sphere,
    Cube,
    Cylinder,
    Cone,
    Custom
};

/**
 * @brief Terrain brush settings
 */
struct TerrainBrush {
    TerrainToolType tool = TerrainToolType::Sculpt;
    TerrainBrushShape shape = TerrainBrushShape::Sphere;
    float radius = 5.0f;
    float strength = 0.5f;
    float falloff = 0.5f;    // 0 = hard edge, 1 = smooth falloff
    float smoothness = 0.3f;  // SDF smooth blending
    Nova::VoxelMaterial material = Nova::VoxelMaterial::Dirt;
    glm::vec3 color{0.5f, 0.4f, 0.3f};

    // Additional settings per tool
    float targetHeight = 0.0f;      // For flatten
    float noiseScale = 1.0f;        // For noise/cave
    int noiseOctaves = 4;           // For noise
    float erosionStrength = 0.5f;   // For erode
    float pathWidth = 2.0f;         // For path
};

/**
 * @brief Stamp template for stamping predefined shapes
 */
struct TerrainStamp {
    std::string name;
    std::string description;
    Nova::SDFBrushShape shape;
    glm::vec3 size{1.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float smoothness = 0.3f;
    std::string thumbnailPath;

    // Custom SDF function for complex stamps
    std::function<float(const glm::vec3&)> customSDF;
};

/**
 * @brief Material preset
 */
struct MaterialPreset {
    std::string name;
    Nova::VoxelMaterial material;
    glm::vec3 color;
    std::string texturePath;
};

/**
 * @brief Terrain Editor - 3D landscaping tool with caves/tunnels support
 *
 * Features:
 * - Sculpt tools (raise, lower, smooth, flatten)
 * - Tunnel and cave creation
 * - SDF boolean operations
 * - Material painting
 * - Stamp system for predefined shapes
 * - Undo/redo support
 * - Brush preview
 * - Height painting mode
 * - Erosion simulation
 */
class TerrainEditor {
public:
    struct Config {
        float minBrushRadius = 0.5f;
        float maxBrushRadius = 50.0f;
        float brushRadiusStep = 0.5f;
        float minStrength = 0.01f;
        float maxStrength = 1.0f;
        bool showBrushPreview = true;
        bool realTimePreview = true;
        int previewResolution = 32;
    };

    TerrainEditor();
    ~TerrainEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(Editor* editor, Nova::VoxelTerrain* terrain, const Config& config = {});

    /**
     * @brief Set terrain reference
     */
    void SetTerrain(Nova::VoxelTerrain* terrain) { m_terrain = terrain; }

    // =========================================================================
    // Tool Selection
    // =========================================================================

    /**
     * @brief Set current tool
     */
    void SetTool(TerrainToolType tool);

    /**
     * @brief Get current tool
     */
    [[nodiscard]] TerrainToolType GetTool() const { return m_brush.tool; }

    /**
     * @brief Get brush settings
     */
    [[nodiscard]] TerrainBrush& GetBrush() { return m_brush; }
    [[nodiscard]] const TerrainBrush& GetBrush() const { return m_brush; }

    // =========================================================================
    // Brush Operations
    // =========================================================================

    /**
     * @brief Apply brush at world position
     */
    void ApplyBrush(const glm::vec3& position);

    /**
     * @brief Apply brush stroke from start to end
     */
    void ApplyBrushStroke(const glm::vec3& start, const glm::vec3& end);

    /**
     * @brief Start brush stroke (for continuous painting)
     */
    void BeginStroke(const glm::vec3& position);

    /**
     * @brief Continue brush stroke
     */
    void ContinueStroke(const glm::vec3& position);

    /**
     * @brief End brush stroke
     */
    void EndStroke();

    /**
     * @brief Check if currently stroking
     */
    [[nodiscard]] bool IsStroking() const { return m_isStroking; }

    // =========================================================================
    // Tunnel/Cave Tools
    // =========================================================================

    /**
     * @brief Begin tunnel mode
     */
    void BeginTunnel(const glm::vec3& start);

    /**
     * @brief Set tunnel end point and preview
     */
    void PreviewTunnel(const glm::vec3& end);

    /**
     * @brief Complete tunnel creation
     */
    void CompleteTunnel(const glm::vec3& end);

    /**
     * @brief Cancel tunnel creation
     */
    void CancelTunnel();

    /**
     * @brief Check if in tunnel mode
     */
    [[nodiscard]] bool IsTunnelMode() const { return m_isTunnelMode; }

    /**
     * @brief Create cave at position
     */
    void CreateCave(const glm::vec3& center, const glm::vec3& size);

    // =========================================================================
    // Stamp System
    // =========================================================================

    /**
     * @brief Get available stamps
     */
    [[nodiscard]] const std::vector<TerrainStamp>& GetStamps() const { return m_stamps; }

    /**
     * @brief Select stamp by index
     */
    void SelectStamp(size_t index);

    /**
     * @brief Get selected stamp
     */
    [[nodiscard]] const TerrainStamp* GetSelectedStamp() const {
        return m_selectedStampIndex < m_stamps.size() ? &m_stamps[m_selectedStampIndex] : nullptr;
    }

    /**
     * @brief Apply selected stamp at position
     */
    void ApplyStamp(const glm::vec3& position, bool subtract = false);

    /**
     * @brief Add custom stamp
     */
    void AddStamp(const TerrainStamp& stamp);

    /**
     * @brief Load stamps from file
     */
    bool LoadStamps(const std::string& path);

    // =========================================================================
    // Material System
    // =========================================================================

    /**
     * @brief Get material presets
     */
    [[nodiscard]] const std::vector<MaterialPreset>& GetMaterialPresets() const { return m_materialPresets; }

    /**
     * @brief Select material preset
     */
    void SelectMaterial(size_t index);

    /**
     * @brief Add material preset
     */
    void AddMaterialPreset(const MaterialPreset& preset);

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Update brush preview position
     */
    void UpdatePreview(const glm::vec3& position);

    /**
     * @brief Get brush preview mesh data
     */
    void GetPreviewMesh(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) const;

    /**
     * @brief Get brush preview position
     */
    [[nodiscard]] glm::vec3 GetPreviewPosition() const { return m_previewPosition; }

    /**
     * @brief Check if preview is valid
     */
    [[nodiscard]] bool HasValidPreview() const { return m_hasValidPreview; }

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Undo last operation
     */
    void Undo();

    /**
     * @brief Redo last undone operation
     */
    void Redo();

    /**
     * @brief Check if undo available
     */
    [[nodiscard]] bool CanUndo() const { return m_terrain && m_terrain->CanUndo(); }

    /**
     * @brief Check if redo available
     */
    [[nodiscard]] bool CanRedo() const { return m_terrain && m_terrain->CanRedo(); }

    // =========================================================================
    // Utility Tools
    // =========================================================================

    /**
     * @brief Sample height at position
     */
    [[nodiscard]] float SampleHeight(float x, float z) const;

    /**
     * @brief Raycast to find terrain hit
     */
    bool RaycastTerrain(const glm::vec3& origin, const glm::vec3& direction, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

    /**
     * @brief Generate terrain from heightmap
     */
    void ImportHeightmap(const std::string& path, float heightScale);

    /**
     * @brief Export terrain to heightmap
     */
    void ExportHeightmap(const std::string& path, int resolution) const;

    /**
     * @brief Fill terrain with flat plane
     */
    void FillFlat(float height);

    /**
     * @brief Generate procedural terrain
     */
    void GenerateProcedural(int seed, float scale, int octaves);

    // =========================================================================
    // UI
    // =========================================================================

    /**
     * @brief Render editor UI
     */
    void RenderUI();

    /**
     * @brief Process input
     */
    void ProcessInput();

    /**
     * @brief Update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(TerrainToolType)> OnToolChanged;
    std::function<void()> OnBrushApplied;
    std::function<void()> OnTerrainModified;

private:
    // Internal tool implementations
    void ApplySculptTool(const glm::vec3& position);
    void ApplySmoothTool(const glm::vec3& position);
    void ApplyFlattenTool(const glm::vec3& position);
    void ApplyRaiseTool(const glm::vec3& position);
    void ApplyLowerTool(const glm::vec3& position);
    void ApplyPaintTool(const glm::vec3& position);
    void ApplyNoiseTool(const glm::vec3& position);
    void ApplyErodeTool(const glm::vec3& position);
    void ApplyPathTool(const glm::vec3& start, const glm::vec3& end);
    void ApplyCliffTool(const glm::vec3& position);

    // Brush shape evaluation
    float EvaluateBrush(const glm::vec3& worldPos, const glm::vec3& brushCenter) const;

    // Preview generation
    void GenerateSpherePreview();
    void GenerateCubePreview();
    void GenerateCylinderPreview();

    // UI panels
    void RenderToolPanel();
    void RenderBrushPanel();
    void RenderMaterialPanel();
    void RenderStampPanel();
    void RenderTerrainInfoPanel();
    void RenderHistoryPanel();

    // Initialize default stamps and materials
    void InitializeDefaults();

    Config m_config;
    Editor* m_editor = nullptr;
    Nova::VoxelTerrain* m_terrain = nullptr;

    // Current brush settings
    TerrainBrush m_brush;

    // Stroke state
    bool m_isStroking = false;
    glm::vec3 m_lastStrokePosition{0.0f};
    float m_strokeSpacing = 0.5f;

    // Tunnel mode
    bool m_isTunnelMode = false;
    glm::vec3 m_tunnelStart{0.0f};
    glm::vec3 m_tunnelEnd{0.0f};

    // Preview
    glm::vec3 m_previewPosition{0.0f};
    bool m_hasValidPreview = false;
    std::vector<glm::vec3> m_previewVertices;
    std::vector<uint32_t> m_previewIndices;

    // Stamps
    std::vector<TerrainStamp> m_stamps;
    size_t m_selectedStampIndex = 0;

    // Materials
    std::vector<MaterialPreset> m_materialPresets;
    size_t m_selectedMaterialIndex = 0;

    // Path tool state
    std::vector<glm::vec3> m_pathPoints;
    bool m_isDrawingPath = false;

    // UI state
    bool m_showToolPanel = true;
    bool m_showBrushPanel = true;
    bool m_showMaterialPanel = true;
    bool m_showStampPanel = false;
    bool m_showInfoPanel = true;
    bool m_showHistoryPanel = false;

    bool m_initialized = false;
};

} // namespace Vehement
