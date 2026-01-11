#pragma once

#include <memory>
#include <string>

namespace Vehement {

// Forward declarations
class InGameEditor;
class MapEditor;
class AssetPalette;
class PlacementSystem;
class TransformGizmo;
class SelectionSystem;
class LayerSystem;
class LayerPanel;
class Hierarchy;

/**
 * @brief Editor Integration - Connects all editor systems together
 *
 * This class manages the integration of:
 * - Asset palette for browsing and selecting assets
 * - Placement system for previewing and placing assets
 * - Transform gizmo for moving/rotating/scaling objects
 * - Selection system for multi-select and box select
 * - Layer system for organizing objects
 * - Hierarchy panel integration
 *
 * Workflow:
 * 1. User selects asset from AssetPalette
 * 2. PlacementSystem shows ghost preview
 * 3. User clicks to place, object added to scene
 * 4. SelectionSystem allows selecting placed objects
 * 5. TransformGizmo allows manipulating selected objects
 * 6. LayerSystem organizes objects into layers
 * 7. Hierarchy shows object tree with layer grouping
 */
class EditorIntegration {
public:
    EditorIntegration();
    ~EditorIntegration();

    // Delete copy
    EditorIntegration(const EditorIntegration&) = delete;
    EditorIntegration& operator=(const EditorIntegration&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize editor integration
     * @param inGameEditor Parent editor
     * @return true if successful
     */
    bool Initialize(InGameEditor& inGameEditor);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update all systems
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render all systems
     */
    void Render();

    /**
     * @brief Process input for all systems
     */
    void ProcessInput();

    // =========================================================================
    // System Access
    // =========================================================================

    [[nodiscard]] AssetPalette* GetAssetPalette() { return m_assetPalette.get(); }
    [[nodiscard]] PlacementSystem* GetPlacementSystem() { return m_placementSystem.get(); }
    [[nodiscard]] TransformGizmo* GetTransformGizmo() { return m_transformGizmo.get(); }
    [[nodiscard]] SelectionSystem* GetSelectionSystem() { return m_selectionSystem.get(); }
    [[nodiscard]] LayerSystem* GetLayerSystem() { return m_layerSystem.get(); }
    [[nodiscard]] LayerPanel* GetLayerPanel() { return m_layerPanel.get(); }

    // =========================================================================
    // Workflow Integration
    // =========================================================================

    /**
     * @brief Handle asset selected from palette (start placement)
     */
    void OnAssetSelected(const std::string& assetId);

    /**
     * @brief Handle asset placed in scene
     */
    void OnAssetPlaced(uint64_t entityId);

    /**
     * @brief Handle object selected
     */
    void OnObjectSelected(uint64_t entityId);

    /**
     * @brief Handle objects transformed
     */
    void OnObjectsTransformed();

    // =========================================================================
    // Editor Mode
    // =========================================================================

    /**
     * @brief Set editor tool mode
     */
    void SetToolMode(const std::string& mode);

    /**
     * @brief Get current tool mode
     */
    [[nodiscard]] const std::string& GetToolMode() const noexcept { return m_currentToolMode; }

    // Common tool modes:
    // - "select" - Selection mode
    // - "place" - Asset placement mode
    // - "paint" - Paint placement mode
    // - "move" - Move tool (gizmo)
    // - "rotate" - Rotate tool (gizmo)
    // - "scale" - Scale tool (gizmo)

private:
    // Setup system connections
    void ConnectSystems();

    // Event handlers
    void HandleAssetPaletteEvents();
    void HandlePlacementEvents();
    void HandleSelectionEvents();
    void HandleTransformEvents();
    void HandleLayerEvents();

    // State
    bool m_initialized = false;
    InGameEditor* m_editor = nullptr;
    MapEditor* m_mapEditor = nullptr;

    // Systems
    std::unique_ptr<AssetPalette> m_assetPalette;
    std::unique_ptr<PlacementSystem> m_placementSystem;
    std::unique_ptr<TransformGizmo> m_transformGizmo;
    std::unique_ptr<SelectionSystem> m_selectionSystem;
    std::unique_ptr<LayerSystem> m_layerSystem;
    std::unique_ptr<LayerPanel> m_layerPanel;

    // State
    std::string m_currentToolMode = "select";
    bool m_isPlacing = false;
};

} // namespace Vehement
