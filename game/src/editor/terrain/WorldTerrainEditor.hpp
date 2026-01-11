#pragma once

#include "TerrainEditor.hpp"
#include "../../world/World.hpp"
#include "../../../../engine/terrain/VoxelTerrain.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Vehement {

// Forward declarations
class Editor;

/**
 * @brief Biome type enumeration
 */
enum class BiomeType : uint8_t {
    Plains,
    Forest,
    Desert,
    Tundra,
    Mountains,
    Ocean,
    Beach,
    Swamp,
    Jungle,
    Volcanic,
    Custom
};

/**
 * @brief Get biome name
 */
inline const char* GetBiomeName(BiomeType type) {
    switch (type) {
        case BiomeType::Plains:    return "Plains";
        case BiomeType::Forest:    return "Forest";
        case BiomeType::Desert:    return "Desert";
        case BiomeType::Tundra:    return "Tundra";
        case BiomeType::Mountains: return "Mountains";
        case BiomeType::Ocean:     return "Ocean";
        case BiomeType::Beach:     return "Beach";
        case BiomeType::Swamp:     return "Swamp";
        case BiomeType::Jungle:    return "Jungle";
        case BiomeType::Volcanic:  return "Volcanic";
        case BiomeType::Custom:    return "Custom";
        default:                   return "Unknown";
    }
}

/**
 * @brief Biome preset with material and color settings
 */
struct BiomePreset {
    BiomeType type = BiomeType::Plains;
    std::string name;
    std::string description;

    // Primary materials
    Nova::VoxelMaterial surfaceMaterial = Nova::VoxelMaterial::Grass;
    Nova::VoxelMaterial subsurfaceMaterial = Nova::VoxelMaterial::Dirt;
    Nova::VoxelMaterial deepMaterial = Nova::VoxelMaterial::Stone;

    // Colors
    glm::vec3 surfaceColor{0.3f, 0.6f, 0.2f};
    glm::vec3 subsurfaceColor{0.5f, 0.4f, 0.3f};
    glm::vec3 deepColor{0.5f, 0.5f, 0.5f};

    // Height ranges
    float surfaceDepth = 2.0f;
    float subsurfaceDepth = 5.0f;

    // Visual effects
    glm::vec3 fogColor{0.7f, 0.8f, 0.9f};
    float fogDensity = 0.01f;
    std::string particleEffect;
};

/**
 * @brief Terrain region for zone-based editing
 */
struct TerrainRegion {
    std::string name;
    glm::vec2 minXZ{0.0f};
    glm::vec2 maxXZ{0.0f};
    float minHeight = -100.0f;
    float maxHeight = 100.0f;
    BiomeType biome = BiomeType::Plains;
    bool isProtected = false;  // Prevent editing if true
};

/**
 * @brief Terrain layer system for texture blending
 */
struct TerrainLayer {
    std::string name;
    std::string textureId;
    Nova::VoxelMaterial material = Nova::VoxelMaterial::Dirt;
    glm::vec3 color{0.5f, 0.5f, 0.5f};
    float tilingScale = 1.0f;
    float blendSharpness = 0.5f;

    // Auto-paint rules
    float heightMin = -1000.0f;
    float heightMax = 1000.0f;
    float slopeMin = 0.0f;    // 0 = flat, 1 = vertical
    float slopeMax = 1.0f;
    bool autoApply = false;
};

/**
 * @brief Brush stroke for painting operations
 */
struct BrushStroke {
    std::vector<glm::vec3> positions;
    TerrainToolType tool;
    TerrainBrush brush;
    float timestamp = 0.0f;
};

/**
 * @brief World Terrain Editor - Global persistent world editing
 *
 * Extends TerrainEditor to support:
 * - Editing the global persistent world
 * - Multiple biome/texture layer support
 * - Region-based protection
 * - Networked multi-user editing
 * - Automatic material/color based on height/slope
 * - Large-scale terrain modifications
 * - Real-time preview for other players
 */
class WorldTerrainEditor : public TerrainEditor {
public:
    struct WorldConfig {
        bool enableNetworkedEditing = false;
        bool enableRegionProtection = true;
        bool enableAutoSave = true;
        float autoSaveInterval = 300.0f;  // 5 minutes
        int maxUndoHistory = 100;
        bool enablePreviewForOthers = true;
        float previewUpdateRate = 10.0f;  // Updates per second
    };

    WorldTerrainEditor();
    ~WorldTerrainEditor();

    /**
     * @brief Initialize world terrain editor
     */
    void Initialize(Editor* editor, World* world, const Config& config = {}, const WorldConfig& worldConfig = {});

    /**
     * @brief Set world reference
     */
    void SetWorld(World* world) { m_world = world; }
    [[nodiscard]] World* GetWorld() const { return m_world; }

    // =========================================================================
    // Editing Mode
    // =========================================================================

    /**
     * @brief Set editing mode (local map vs global world)
     */
    void SetEditingGlobalWorld(bool global);
    [[nodiscard]] bool IsEditingGlobalWorld() const { return m_editingGlobalWorld; }

    /**
     * @brief Check if position can be edited (respects region protection)
     */
    [[nodiscard]] bool CanEdit(const glm::vec3& position) const;

    // =========================================================================
    // Biome System
    // =========================================================================

    /**
     * @brief Get biome presets
     */
    [[nodiscard]] const std::vector<BiomePreset>& GetBiomePresets() const { return m_biomePresets; }

    /**
     * @brief Add biome preset
     */
    void AddBiomePreset(const BiomePreset& preset);

    /**
     * @brief Select current biome
     */
    void SelectBiome(size_t index);
    [[nodiscard]] size_t GetSelectedBiomeIndex() const { return m_selectedBiomeIndex; }
    [[nodiscard]] const BiomePreset* GetSelectedBiome() const;

    /**
     * @brief Paint biome at position
     */
    void PaintBiome(const glm::vec3& position);

    /**
     * @brief Get biome at position
     */
    [[nodiscard]] BiomeType GetBiomeAt(const glm::vec3& position) const;

    // =========================================================================
    // Texture Layer System
    // =========================================================================

    /**
     * @brief Get terrain layers
     */
    [[nodiscard]] const std::vector<TerrainLayer>& GetTerrainLayers() const { return m_terrainLayers; }

    /**
     * @brief Add terrain layer
     */
    void AddTerrainLayer(const TerrainLayer& layer);

    /**
     * @brief Remove terrain layer
     */
    void RemoveTerrainLayer(size_t index);

    /**
     * @brief Select current layer for painting
     */
    void SelectTerrainLayer(size_t index);
    [[nodiscard]] size_t GetSelectedLayerIndex() const { return m_selectedLayerIndex; }

    /**
     * @brief Paint terrain layer at position
     */
    void PaintTerrainLayer(const glm::vec3& position);

    /**
     * @brief Auto-apply layers based on height/slope
     */
    void AutoApplyLayers(const glm::vec3& minBounds, const glm::vec3& maxBounds);

    // =========================================================================
    // Region System
    // =========================================================================

    /**
     * @brief Get terrain regions
     */
    [[nodiscard]] const std::vector<TerrainRegion>& GetRegions() const { return m_regions; }

    /**
     * @brief Add terrain region
     */
    void AddRegion(const TerrainRegion& region);

    /**
     * @brief Remove region
     */
    void RemoveRegion(size_t index);

    /**
     * @brief Check if position is in protected region
     */
    [[nodiscard]] bool IsInProtectedRegion(const glm::vec3& position) const;

    /**
     * @brief Get region at position
     */
    [[nodiscard]] const TerrainRegion* GetRegionAt(const glm::vec3& position) const;

    // =========================================================================
    // Large-Scale Operations
    // =========================================================================

    /**
     * @brief Fill region with flat terrain
     */
    void FillRegionFlat(const glm::vec3& minBounds, const glm::vec3& maxBounds, float height);

    /**
     * @brief Generate noise terrain in region
     */
    void GenerateNoiseInRegion(const glm::vec3& minBounds, const glm::vec3& maxBounds, int seed, float scale, int octaves);

    /**
     * @brief Copy terrain from one region to another
     */
    void CopyRegion(const glm::vec3& srcMin, const glm::vec3& srcMax, const glm::vec3& dstMin);

    /**
     * @brief Mirror terrain across axis
     */
    void MirrorTerrain(const glm::vec3& center, bool mirrorX, bool mirrorZ);

    /**
     * @brief Rotate terrain 90 degrees
     */
    void RotateTerrain(const glm::vec3& center, float radius, int quarterTurns);

    /**
     * @brief Scale terrain height in region
     */
    void ScaleHeight(const glm::vec3& minBounds, const glm::vec3& maxBounds, float scale);

    // =========================================================================
    // Advanced Tools
    // =========================================================================

    /**
     * @brief Hydraulic erosion simulation
     */
    void SimulateErosion(const glm::vec3& minBounds, const glm::vec3& maxBounds, int iterations, float strength);

    /**
     * @brief Thermal erosion (slope-based)
     */
    void SimulateThermalErosion(const glm::vec3& minBounds, const glm::vec3& maxBounds, float talusAngle, int iterations);

    /**
     * @brief Create river/stream path
     */
    void CreateRiver(const std::vector<glm::vec3>& path, float width, float depth);

    /**
     * @brief Create road path (flattened)
     */
    void CreateRoad(const std::vector<glm::vec3>& path, float width, float smoothness);

    /**
     * @brief Create plateau with cliffs
     */
    void CreatePlateau(const glm::vec3& center, float radius, float height, float cliffSteepness);

    /**
     * @brief Create valley
     */
    void CreateValley(const glm::vec3& start, const glm::vec3& end, float width, float depth);

    // =========================================================================
    // Brush Stroke Recording
    // =========================================================================

    /**
     * @brief Begin recording brush stroke
     */
    void BeginBrushStroke();

    /**
     * @brief Add position to current stroke
     */
    void AddStrokePosition(const glm::vec3& position);

    /**
     * @brief End and apply brush stroke
     */
    void EndBrushStroke();

    /**
     * @brief Get current stroke
     */
    [[nodiscard]] const BrushStroke* GetCurrentStroke() const { return m_isRecordingStroke ? &m_currentStroke : nullptr; }

    // =========================================================================
    // Auto-Save
    // =========================================================================

    /**
     * @brief Enable/disable auto-save
     */
    void SetAutoSaveEnabled(bool enabled);
    [[nodiscard]] bool IsAutoSaveEnabled() const { return m_worldConfig.enableAutoSave; }

    /**
     * @brief Manually trigger save
     */
    void SaveWorldTerrain();

    /**
     * @brief Load world terrain
     */
    bool LoadWorldTerrain();

    // =========================================================================
    // Networked Editing
    // =========================================================================

    /**
     * @brief Broadcast edit to other players
     */
    void BroadcastEdit(const glm::vec3& position);

    /**
     * @brief Receive edit from another player
     */
    void ReceiveEdit(uint32_t playerId, const glm::vec3& position, const TerrainBrush& brush);

    /**
     * @brief Get active editors (other players editing)
     */
    [[nodiscard]] const std::unordered_map<uint32_t, glm::vec3>& GetActiveEditors() const { return m_activeEditors; }

    // =========================================================================
    // UI
    // =========================================================================

    /**
     * @brief Render world terrain editor UI
     */
    void RenderWorldUI();

    /**
     * @brief Render biome panel
     */
    void RenderBiomePanel();

    /**
     * @brief Render layer panel
     */
    void RenderLayerPanel();

    /**
     * @brief Render region panel
     */
    void RenderRegionPanel();

    /**
     * @brief Render advanced tools panel
     */
    void RenderAdvancedToolsPanel();

    /**
     * @brief Update world editor
     */
    void UpdateWorld(float deltaTime);

private:
    // Initialization
    void InitializeBiomePresets();
    void InitializeDefaultLayers();

    // Tool implementations
    void ApplyBiomePaint(const glm::vec3& position);
    void ApplyLayerPaint(const glm::vec3& position);

    // Helper functions
    float CalculateSlope(const glm::vec3& position) const;
    void ApplyLayerToVoxel(const glm::vec3& position, const TerrainLayer& layer, float weight);

    // Auto-save
    float m_autoSaveTimer = 0.0f;

    // World reference
    World* m_world = nullptr;

    // Config
    WorldConfig m_worldConfig;

    // Editing mode
    bool m_editingGlobalWorld = false;

    // Biome system
    std::vector<BiomePreset> m_biomePresets;
    size_t m_selectedBiomeIndex = 0;
    std::unordered_map<uint64_t, BiomeType> m_biomeMap;  // Position hash -> biome type

    // Layer system
    std::vector<TerrainLayer> m_terrainLayers;
    size_t m_selectedLayerIndex = 0;

    // Region system
    std::vector<TerrainRegion> m_regions;

    // Brush stroke recording
    bool m_isRecordingStroke = false;
    BrushStroke m_currentStroke;

    // Networked editing
    std::unordered_map<uint32_t, glm::vec3> m_activeEditors;  // PlayerId -> brush position
    float m_previewBroadcastTimer = 0.0f;

    // UI state
    bool m_showBiomePanel = false;
    bool m_showLayerPanel = false;
    bool m_showRegionPanel = false;
    bool m_showAdvancedTools = false;
};

} // namespace Vehement
