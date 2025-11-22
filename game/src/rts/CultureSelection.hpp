#pragma once

#include "Culture.hpp"
#include <functional>
#include <memory>
#include <string>
#include <optional>

namespace Nova {
    class Texture;
    class Renderer;
}

namespace Vehement {
namespace RTS {

/**
 * @brief Selection state for the culture selection UI
 */
enum class SelectionState : uint8_t {
    Browsing,       ///< Player is looking through options
    Previewing,     ///< Player is viewing details of a culture
    Confirming,     ///< Player is confirming their selection
    Confirmed,      ///< Selection has been confirmed
    Cancelled       ///< Selection was cancelled
};

/**
 * @brief UI element for displaying a single culture option
 */
struct CultureUICard {
    CultureType type;
    std::string name;
    std::string description;
    std::string shortDescription;
    std::shared_ptr<Nova::Texture> previewTexture;
    std::shared_ptr<Nova::Texture> bannerTexture;

    // Bonus summary strings for display
    std::vector<std::string> bonusSummary;
    std::vector<std::string> penaltySummary;
    std::vector<std::string> uniqueAbilities;
    std::vector<std::string> uniqueBuildings;

    // UI state
    bool isHovered = false;
    bool isSelected = false;
    float hoverAnimationProgress = 0.0f;
    float selectAnimationProgress = 0.0f;
};

/**
 * @brief Callback types for culture selection events
 */
using CultureSelectedCallback = std::function<void(CultureType culture)>;
using SelectionCancelledCallback = std::function<void()>;

/**
 * @brief Culture selection UI manager
 *
 * Displays an interactive selection screen at game start where players
 * choose their culture/faction. The selection is stored and cannot be
 * changed for the duration of the game session.
 *
 * UI Layout:
 * - Grid of culture cards (4x2 or carousel)
 * - Each card shows preview image, name, and brief description
 * - Clicking a card expands it to show full details
 * - Confirm button finalizes selection
 *
 * Firebase Integration:
 * - Stores selected culture with player data
 * - Retrieves previously selected culture for returning players
 */
class CultureSelectionUI {
public:
    CultureSelectionUI();
    ~CultureSelectionUI();

    // Prevent copying
    CultureSelectionUI(const CultureSelectionUI&) = delete;
    CultureSelectionUI& operator=(const CultureSelectionUI&) = delete;

    /**
     * @brief Initialize the selection UI
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Show the selection UI
     */
    void Show();

    /**
     * @brief Hide the selection UI
     */
    void Hide();

    /**
     * @brief Check if UI is currently visible
     */
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }

    /**
     * @brief Check if a culture has been confirmed
     */
    [[nodiscard]] bool HasSelection() const noexcept {
        return m_state == SelectionState::Confirmed;
    }

    /**
     * @brief Get the confirmed culture selection
     * @return Selected culture type or nullopt if not yet confirmed
     */
    [[nodiscard]] std::optional<CultureType> GetSelectedCulture() const;

    /**
     * @brief Update the selection UI
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Render the selection UI
     * @param renderer The renderer to use
     */
    void Render(Nova::Renderer& renderer);

    /**
     * @brief Render ImGui debug/dev UI
     */
    void RenderImGui();

    // =========================================================================
    // Input Handling
    // =========================================================================

    /**
     * @brief Handle mouse movement
     * @param x Mouse X position
     * @param y Mouse Y position
     */
    void OnMouseMove(float x, float y);

    /**
     * @brief Handle mouse click
     * @param x Mouse X position
     * @param y Mouse Y position
     * @param button Mouse button (0 = left, 1 = right)
     */
    void OnMouseClick(float x, float y, int button);

    /**
     * @brief Handle keyboard input
     * @param key Key code
     * @param pressed true if pressed, false if released
     */
    void OnKeyPress(int key, bool pressed);

    /**
     * @brief Handle scroll wheel
     * @param delta Scroll delta
     */
    void OnScroll(float delta);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for when a culture is confirmed
     */
    void SetOnCultureSelected(CultureSelectedCallback callback) {
        m_onCultureSelected = std::move(callback);
    }

    /**
     * @brief Set callback for when selection is cancelled
     */
    void SetOnCancelled(SelectionCancelledCallback callback) {
        m_onCancelled = std::move(callback);
    }

    // =========================================================================
    // Firebase Integration
    // =========================================================================

    /**
     * @brief Save the selected culture to Firebase
     * @param userId The user's Firebase ID
     */
    void SaveSelectionToFirebase(const std::string& userId);

    /**
     * @brief Load previously selected culture from Firebase
     * @param userId The user's Firebase ID
     * @param callback Function to call with loaded culture (or nullopt)
     */
    void LoadSelectionFromFirebase(
        const std::string& userId,
        std::function<void(std::optional<CultureType>)> callback);

    /**
     * @brief Force a specific culture selection (for testing/loading)
     * @param culture Culture to select
     */
    void ForceSelection(CultureType culture);

    // =========================================================================
    // UI Customization
    // =========================================================================

    /**
     * @brief Set the screen dimensions for layout
     */
    void SetScreenSize(float width, float height);

    /**
     * @brief Set UI scale factor
     */
    void SetUIScale(float scale) { m_uiScale = scale; }

    /**
     * @brief Enable/disable animations
     */
    void SetAnimationsEnabled(bool enabled) { m_animationsEnabled = enabled; }

private:
    // UI card management
    void InitializeCultureCards();
    void UpdateCardAnimations(float deltaTime);
    void UpdateHoverStates(float mouseX, float mouseY);

    // Selection logic
    void SelectCulture(int cardIndex);
    void ConfirmSelection();
    void CancelSelection();
    void BackToGrid();

    // Rendering helpers
    void RenderGrid(Nova::Renderer& renderer);
    void RenderDetailView(Nova::Renderer& renderer);
    void RenderConfirmDialog(Nova::Renderer& renderer);
    void RenderCard(Nova::Renderer& renderer, const CultureUICard& card,
                    float x, float y, float width, float height);
    void RenderBonusList(Nova::Renderer& renderer, const std::vector<std::string>& items,
                         float x, float y, bool positive);

    // Layout calculations
    void CalculateLayout();
    int GetCardAtPosition(float x, float y) const;

    // State
    bool m_initialized = false;
    bool m_visible = false;
    SelectionState m_state = SelectionState::Browsing;

    // Culture cards
    std::vector<CultureUICard> m_cards;
    int m_hoveredCard = -1;
    int m_selectedCard = -1;
    std::optional<CultureType> m_confirmedCulture;

    // Animation state
    float m_fadeInProgress = 0.0f;
    float m_detailViewProgress = 0.0f;
    float m_confirmDialogProgress = 0.0f;
    bool m_animationsEnabled = true;

    // Layout
    float m_screenWidth = 1920.0f;
    float m_screenHeight = 1080.0f;
    float m_uiScale = 1.0f;

    // Card grid layout
    static constexpr int CARDS_PER_ROW = 4;
    static constexpr int NUM_ROWS = 2;
    static constexpr float CARD_PADDING = 20.0f;
    static constexpr float CARD_ASPECT_RATIO = 0.75f; // height/width

    // Calculated layout values
    float m_cardWidth = 0.0f;
    float m_cardHeight = 0.0f;
    float m_gridStartX = 0.0f;
    float m_gridStartY = 0.0f;

    // Scroll state (for mobile/small screens)
    float m_scrollOffset = 0.0f;
    float m_maxScroll = 0.0f;

    // Callbacks
    CultureSelectedCallback m_onCultureSelected;
    SelectionCancelledCallback m_onCancelled;

    // Textures
    std::shared_ptr<Nova::Texture> m_backgroundTexture;
    std::shared_ptr<Nova::Texture> m_cardFrameTexture;
    std::shared_ptr<Nova::Texture> m_buttonTexture;
};

/**
 * @brief Player culture state for in-game use
 *
 * Stores the selected culture and provides easy access to bonuses
 * and cultural data during gameplay.
 */
class PlayerCulture {
public:
    PlayerCulture() = default;
    explicit PlayerCulture(CultureType type);

    /**
     * @brief Get the culture type
     */
    [[nodiscard]] CultureType GetType() const noexcept { return m_type; }

    /**
     * @brief Get the culture data
     */
    [[nodiscard]] const CultureData* GetData() const;

    /**
     * @brief Check if a culture has been set
     */
    [[nodiscard]] bool IsSet() const noexcept { return m_isSet; }

    /**
     * @brief Set the culture type
     */
    void SetCulture(CultureType type);

    /**
     * @brief Apply a bonus to a base value
     * @param baseValue The base value to modify
     * @param bonusType The type of bonus to apply
     * @return Modified value
     */
    [[nodiscard]] float ApplyBonus(float baseValue, const std::string& bonusType) const;

    /**
     * @brief Apply cost modifier to resource cost
     */
    [[nodiscard]] ResourceCost ApplyCostModifier(const ResourceCost& baseCost) const;

    /**
     * @brief Check if a building can be constructed
     */
    [[nodiscard]] bool CanBuild(BuildingType building) const;

    /**
     * @brief Get texture path for a building
     */
    [[nodiscard]] std::string GetBuildingTexture(BuildingType building) const;

    /**
     * @brief Serialize to JSON for Firebase storage
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static PlayerCulture FromJson(const std::string& json);

private:
    CultureType m_type = CultureType::Fortress;
    bool m_isSet = false;
};

} // namespace RTS
} // namespace Vehement
