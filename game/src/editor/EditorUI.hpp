#pragma once

#include "LevelEditor.hpp"
#include "TilePalette.hpp"
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <memory>

namespace Nova {
class Texture;
class TextureManager;
class Renderer;
}

namespace Vehement {

// Forward declarations
class TileMap;
class ProceduralTown;

/**
 * @brief Full Editor UI for the Vehement2 Level Editor
 *
 * Provides a complete UI for level editing including:
 * - Tool bar with all editing tools
 * - Tile palette for tile selection
 * - Properties panel for wall height, brush settings, etc.
 * - Mini-map for navigation
 * - Save/Load buttons
 * - Coin display (editing costs coins)
 * - Undo/Redo buttons
 * - Status bar with helpful information
 */
class EditorUI {
public:
    /**
     * @brief UI Theme configuration
     */
    struct Theme {
        glm::vec4 backgroundColor{0.15f, 0.15f, 0.18f, 0.95f};
        glm::vec4 panelColor{0.2f, 0.2f, 0.25f, 1.0f};
        glm::vec4 buttonColor{0.3f, 0.3f, 0.35f, 1.0f};
        glm::vec4 buttonHoverColor{0.4f, 0.4f, 0.45f, 1.0f};
        glm::vec4 buttonActiveColor{0.5f, 0.4f, 0.2f, 1.0f};
        glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 accentColor{1.0f, 0.7f, 0.2f, 1.0f};
        glm::vec4 errorColor{1.0f, 0.3f, 0.3f, 1.0f};
        glm::vec4 successColor{0.3f, 1.0f, 0.3f, 1.0f};
        float cornerRadius = 4.0f;
        float padding = 8.0f;
    };

    /**
     * @brief UI Configuration
     */
    struct Config {
        int windowWidth = 1920;
        int windowHeight = 1080;
        float toolBarHeight = 50.0f;
        float paletteWidth = 280.0f;
        float propertiesWidth = 250.0f;
        float statusBarHeight = 30.0f;
        float miniMapSize = 200.0f;
        bool showMiniMap = true;
        bool showProperties = true;
        bool showPalette = true;
        Theme theme;
    };

    /**
     * @brief Cost configuration for different tile types
     */
    struct CostConfig {
        int baseTileCost = 10;          ///< Base cost per tile
        int wallMultiplier = 2;          ///< Walls cost this much more
        int foliageMultiplier = 3;       ///< Trees/plants cost this much more
        int objectMultiplier = 5;        ///< Objects cost this much more
        int eraseCost = 0;               ///< Cost to erase (refund partial?)
    };

public:
    EditorUI();
    ~EditorUI();

    // Delete copy, allow move
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;
    EditorUI(EditorUI&&) noexcept = default;
    EditorUI& operator=(EditorUI&&) noexcept = default;

    /**
     * @brief Initialize the editor UI
     * @param editor Reference to the level editor
     * @param atlas Reference to the tile atlas
     * @param config Optional configuration
     */
    void Initialize(LevelEditor& editor, TileAtlas& atlas, const Config& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if UI is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Render the editor UI
     * Call this each frame after clearing the screen.
     */
    void Render();

    /**
     * @brief Update UI state
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Player Resources
    // -------------------------------------------------------------------------

    /**
     * @brief Set the player's current coin count
     * @param coins Number of coins the player has
     */
    void SetPlayerCoins(int coins);

    /**
     * @brief Get the player's current coin count
     */
    [[nodiscard]] int GetPlayerCoins() const noexcept { return m_playerCoins; }

    /**
     * @brief Get the cost of pending edits
     */
    [[nodiscard]] int GetEditCost() const;

    /**
     * @brief Check if player can afford pending edits
     */
    [[nodiscard]] bool CanAffordEdits() const;

    /**
     * @brief Set cost configuration
     */
    void SetCostConfig(const CostConfig& config);

    // -------------------------------------------------------------------------
    // Visibility
    // -------------------------------------------------------------------------

    /**
     * @brief Show or hide the entire editor UI
     */
    void SetVisible(bool visible);

    /**
     * @brief Check if UI is visible
     */
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }

    /**
     * @brief Toggle specific UI elements
     */
    void SetPaletteVisible(bool visible);
    void SetPropertiesVisible(bool visible);
    void SetMiniMapVisible(bool visible);

    [[nodiscard]] bool IsPaletteVisible() const noexcept { return m_config.showPalette; }
    [[nodiscard]] bool IsPropertiesVisible() const noexcept { return m_config.showProperties; }
    [[nodiscard]] bool IsMiniMapVisible() const noexcept { return m_config.showMiniMap; }

    // -------------------------------------------------------------------------
    // Mini Map
    // -------------------------------------------------------------------------

    /**
     * @brief Set the tile map for mini-map rendering
     */
    void SetTileMap(TileMap* map);

    /**
     * @brief Set the camera view bounds for mini-map viewport indicator
     */
    void SetCameraView(const glm::vec2& center, const glm::vec2& size);

    // -------------------------------------------------------------------------
    // Status Messages
    // -------------------------------------------------------------------------

    /**
     * @brief Show a status message in the status bar
     * @param message Message to display
     * @param duration How long to show (0 = until replaced)
     * @param isError Whether this is an error message
     */
    void ShowStatus(const std::string& message, float duration = 3.0f, bool isError = false);

    /**
     * @brief Clear status message
     */
    void ClearStatus();

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a screen position is over the UI
     * @param screenPos Position in screen coordinates
     * @return true if the position is over any UI element
     */
    [[nodiscard]] bool IsOverUI(const glm::vec2& screenPos) const;

    /**
     * @brief Handle mouse click
     * @param screenPos Click position
     * @param button Mouse button
     * @return true if click was handled by UI
     */
    bool OnMouseClick(const glm::vec2& screenPos, int button);

    /**
     * @brief Handle mouse move
     * @param screenPos Mouse position
     */
    void OnMouseMove(const glm::vec2& screenPos);

    /**
     * @brief Handle key press
     * @param key Key code
     * @param mods Modifier keys
     * @return true if key was handled
     */
    bool OnKeyPress(int key, int mods);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /** @brief Called when Save button is clicked */
    std::function<void()> OnSave;

    /** @brief Called when Load button is clicked */
    std::function<void()> OnLoad;

    /** @brief Called when Exit button is clicked */
    std::function<void()> OnExit;

    /** @brief Called when Apply Changes button is clicked */
    std::function<void()> OnApplyChanges;

    /** @brief Called when Discard Changes button is clicked */
    std::function<void()> OnDiscardChanges;

    /** @brief Called when Generate Town button is clicked */
    std::function<void()> OnGenerateTown;

    /** @brief Called when a mini-map location is clicked */
    std::function<void(const glm::vec2&)> OnMiniMapClick;

private:
    // Render individual UI components
    void RenderToolBar();
    void RenderTilePalette();
    void RenderPropertiesPanel();
    void RenderMiniMap();
    void RenderStatusBar();
    void RenderCoinDisplay();
    void RenderConfirmDialog();

    // Tool bar helpers
    void RenderToolButton(LevelEditor::Tool tool, const char* label, const char* tooltip);
    void RenderUndoRedoButtons();
    void RenderBrushSizeSlider();

    // Properties panel helpers
    void RenderWallSettings();
    void RenderTileInfo();
    void RenderCostBreakdown();

    // Mini map helpers
    void UpdateMiniMapTexture();
    glm::vec2 MiniMapToWorld(const glm::vec2& miniMapPos) const;
    glm::vec2 WorldToMiniMap(const glm::vec2& worldPos) const;

    // Calculate edit cost for a tile type
    int CalculateTileCost(TileType type, bool isWall) const;

    // Layout calculations
    void UpdateLayout();
    glm::vec2 GetToolBarBounds() const;
    glm::vec2 GetPaletteBounds() const;
    glm::vec2 GetPropertiesBounds() const;
    glm::vec2 GetMiniMapBounds() const;
    glm::vec2 GetStatusBarBounds() const;

private:
    bool m_initialized = false;
    bool m_visible = true;

    // Configuration
    Config m_config;
    CostConfig m_costConfig;

    // References
    LevelEditor* m_editor = nullptr;
    TileAtlas* m_atlas = nullptr;
    TileMap* m_tileMap = nullptr;

    // Sub-components
    TilePalette m_palette;

    // Player resources
    int m_playerCoins = 1000;

    // Camera view for mini-map
    glm::vec2 m_cameraCenter{0.0f};
    glm::vec2 m_cameraSize{20.0f, 15.0f};

    // Status message
    std::string m_statusMessage;
    float m_statusTimer = 0.0f;
    bool m_statusIsError = false;

    // Confirmation dialog
    bool m_showConfirmDialog = false;
    std::string m_confirmMessage;
    std::function<void()> m_confirmAction;

    // Mini-map texture (rendered dynamically)
    std::shared_ptr<Nova::Texture> m_miniMapTexture;
    bool m_miniMapDirty = true;

    // Layout cache
    struct LayoutCache {
        glm::vec2 toolBarPos{0.0f};
        glm::vec2 toolBarSize{0.0f};
        glm::vec2 palettePos{0.0f};
        glm::vec2 paletteSize{0.0f};
        glm::vec2 propertiesPos{0.0f};
        glm::vec2 propertiesSize{0.0f};
        glm::vec2 miniMapPos{0.0f};
        glm::vec2 miniMapSize{0.0f};
        glm::vec2 statusBarPos{0.0f};
        glm::vec2 statusBarSize{0.0f};
    };
    LayoutCache m_layout;

    // Animation state
    float m_paletteSlideAnim = 1.0f;
    float m_propertiesSlideAnim = 1.0f;

    // Hover state
    glm::vec2 m_mousePos{0.0f};
    LevelEditor::Tool m_hoveredTool = LevelEditor::Tool::Select;
    bool m_isHoveringTool = false;
};

} // namespace Vehement
