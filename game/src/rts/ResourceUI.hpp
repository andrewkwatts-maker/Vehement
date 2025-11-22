#pragma once

#include "Resource.hpp"
#include "Upkeep.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace Nova {
    class Renderer;
    class Graph;
}

namespace Vehement {
namespace RTS {

// Forward declarations
class GatheringSystem;
class ProductionSystem;
class TradingSystem;
class UpkeepSystem;

// ============================================================================
// Resource Bar Configuration
// ============================================================================

/**
 * @brief Configuration for how a resource is displayed in the UI
 */
struct ResourceDisplayConfig {
    ResourceType type = ResourceType::Food;

    /// Whether to show this resource in the bar
    bool visible = true;

    /// Whether to show income/expense rate
    bool showRate = true;

    /// Whether to show capacity bar
    bool showCapacity = true;

    /// Whether to pulse when low
    bool pulseWhenLow = true;

    /// Low threshold for pulsing (0-1)
    float lowThreshold = 0.2f;

    /// Position in the resource bar (0 = leftmost)
    int displayOrder = 0;

    /// Custom icon path (empty = use default)
    std::string customIcon;
};

// ============================================================================
// Resource Alert
// ============================================================================

/**
 * @brief An alert message for resource events
 */
struct ResourceAlert {
    /// Message text
    std::string message;

    /// Associated resource type
    ResourceType resourceType = ResourceType::Food;

    /// Severity level (0 = info, 1 = warning, 2 = critical)
    int severity = 0;

    /// Time remaining for this alert
    float duration = 5.0f;

    /// Current alpha for fade effect
    float alpha = 1.0f;

    /// World position for localized alerts
    glm::vec2 position{0.0f};

    /// Whether this is a localized (world-space) alert
    bool isLocalized = false;
};

// ============================================================================
// Resource Tooltip
// ============================================================================

/**
 * @brief Tooltip information for a resource
 */
struct ResourceTooltip {
    ResourceType type = ResourceType::Food;

    /// Resource name
    std::string name;

    /// Current amount
    int amount = 0;

    /// Maximum capacity
    int capacity = 0;

    /// Income rate per second
    float incomeRate = 0.0f;

    /// Expense rate per second
    float expenseRate = 0.0f;

    /// Net rate per second
    float netRate = 0.0f;

    /// Time until full/empty
    float timeUntilChange = 0.0f;

    /// Whether depleting or filling
    bool isDepleting = false;

    /// Breakdown of income sources
    std::vector<std::pair<std::string, float>> incomeSources;

    /// Breakdown of expense sources
    std::vector<std::pair<std::string, float>> expenseSources;
};

// ============================================================================
// Resource Bar
// ============================================================================

/**
 * @brief Main resource display bar at top of screen
 */
class ResourceBar {
public:
    ResourceBar();
    ~ResourceBar();

    /**
     * @brief Initialize the resource bar
     * @param position Screen position (top-left corner)
     * @param width Total width in pixels
     * @param height Height in pixels
     */
    void Initialize(const glm::vec2& position, float width, float height);

    /**
     * @brief Update animations and alerts
     */
    void Update(float deltaTime);

    /**
     * @brief Render the resource bar
     */
    void Render(Nova::Renderer& renderer);

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Set display configuration for a resource type
     */
    void SetDisplayConfig(ResourceType type, const ResourceDisplayConfig& config);

    /**
     * @brief Get display configuration for a resource type
     */
    [[nodiscard]] const ResourceDisplayConfig& GetDisplayConfig(ResourceType type) const;

    /**
     * @brief Set which resources are visible
     */
    void SetVisibleResources(const std::vector<ResourceType>& types);

    /**
     * @brief Set position
     */
    void SetPosition(const glm::vec2& pos) { m_position = pos; }

    /**
     * @brief Set size
     */
    void SetSize(float width, float height) { m_width = width; m_height = height; }

    // -------------------------------------------------------------------------
    // Data Binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind to a resource stock for display
     */
    void BindResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Bind to upkeep system for rate display
     */
    void BindUpkeepSystem(UpkeepSystem* upkeep) { m_upkeepSystem = upkeep; }

    /**
     * @brief Bind to gathering system for income display
     */
    void BindGatheringSystem(GatheringSystem* gathering) { m_gatheringSystem = gathering; }

    /**
     * @brief Bind to production system for income display
     */
    void BindProductionSystem(ProductionSystem* production) { m_productionSystem = production; }

    // -------------------------------------------------------------------------
    // Interaction
    // -------------------------------------------------------------------------

    /**
     * @brief Check if mouse is over a resource
     * @param mousePos Mouse position in screen coordinates
     * @return ResourceType under mouse, or COUNT if none
     */
    [[nodiscard]] ResourceType GetResourceAtPosition(const glm::vec2& mousePos) const;

    /**
     * @brief Get tooltip for a resource
     */
    [[nodiscard]] ResourceTooltip GetTooltip(ResourceType type) const;

    /**
     * @brief Check if mouse is over the resource bar
     */
    [[nodiscard]] bool IsMouseOver(const glm::vec2& mousePos) const;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ResourceClickCallback = std::function<void(ResourceType)>;
    void SetOnResourceClick(ResourceClickCallback cb) { m_onResourceClick = std::move(cb); }

private:
    void RenderResourceSlot(Nova::Renderer& renderer, ResourceType type,
                            const glm::vec2& position, float width);
    void UpdatePulseEffects(float deltaTime);

    glm::vec2 m_position{0.0f};
    float m_width = 800.0f;
    float m_height = 40.0f;

    std::vector<ResourceDisplayConfig> m_displayConfigs;
    std::vector<ResourceType> m_visibleResources;

    ResourceStock* m_resourceStock = nullptr;
    UpkeepSystem* m_upkeepSystem = nullptr;
    GatheringSystem* m_gatheringSystem = nullptr;
    ProductionSystem* m_productionSystem = nullptr;

    // Pulse animation state
    std::unordered_map<ResourceType, float> m_pulseTimers;
    float m_globalPulseTime = 0.0f;

    ResourceClickCallback m_onResourceClick;

    bool m_initialized = false;
};

// ============================================================================
// Alert Manager
// ============================================================================

/**
 * @brief Manages resource-related alerts and notifications
 */
class ResourceAlertManager {
public:
    ResourceAlertManager();
    ~ResourceAlertManager();

    /**
     * @brief Initialize the alert manager
     */
    void Initialize();

    /**
     * @brief Update alerts
     */
    void Update(float deltaTime);

    /**
     * @brief Render active alerts
     */
    void Render(Nova::Renderer& renderer);

    // -------------------------------------------------------------------------
    // Alert Creation
    // -------------------------------------------------------------------------

    /**
     * @brief Show an info alert
     */
    void ShowInfo(const std::string& message, ResourceType type, float duration = 3.0f);

    /**
     * @brief Show a warning alert
     */
    void ShowWarning(const std::string& message, ResourceType type, float duration = 5.0f);

    /**
     * @brief Show a critical alert
     */
    void ShowCritical(const std::string& message, ResourceType type, float duration = 7.0f);

    /**
     * @brief Show a localized alert at a world position
     */
    void ShowLocalized(const std::string& message, const glm::vec2& worldPos,
                       ResourceType type, int severity, float duration = 3.0f);

    /**
     * @brief Clear all alerts
     */
    void ClearAll();

    /**
     * @brief Clear alerts for a specific resource
     */
    void ClearForResource(ResourceType type);

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Set maximum number of visible alerts
     */
    void SetMaxAlerts(int max) { m_maxAlerts = max; }

    /**
     * @brief Set alert position on screen
     */
    void SetAlertPosition(const glm::vec2& pos) { m_alertPosition = pos; }

    /**
     * @brief Enable/disable sound effects
     */
    void SetSoundEnabled(bool enabled) { m_soundEnabled = enabled; }

    // -------------------------------------------------------------------------
    // Automatic Alerts
    // -------------------------------------------------------------------------

    /**
     * @brief Bind to upkeep system for automatic alerts
     */
    void BindUpkeepSystem(UpkeepSystem* upkeep);

    /**
     * @brief Bind to resource stock for capacity alerts
     */
    void BindResourceStock(ResourceStock* stock);

private:
    void AddAlert(const ResourceAlert& alert);
    void OnUpkeepWarning(const UpkeepWarning& warning);
    void OnLowResource(ResourceType type, int amount, int threshold);

    std::vector<ResourceAlert> m_alerts;
    glm::vec2 m_alertPosition{10.0f, 100.0f};
    int m_maxAlerts = 5;
    bool m_soundEnabled = true;

    UpkeepSystem* m_upkeepSystem = nullptr;
    ResourceStock* m_resourceStock = nullptr;

    bool m_initialized = false;
};

// ============================================================================
// Storage Capacity Widget
// ============================================================================

/**
 * @brief Widget showing storage capacity status
 */
class StorageCapacityWidget {
public:
    StorageCapacityWidget();
    ~StorageCapacityWidget();

    /**
     * @brief Initialize the widget
     */
    void Initialize(const glm::vec2& position, float width, float height);

    /**
     * @brief Update animations
     */
    void Update(float deltaTime);

    /**
     * @brief Render the widget
     */
    void Render(Nova::Renderer& renderer);

    /**
     * @brief Bind to resource stock
     */
    void BindResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Set position
     */
    void SetPosition(const glm::vec2& pos) { m_position = pos; }

    /**
     * @brief Show/hide the widget
     */
    void SetVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Check if visible
     */
    [[nodiscard]] bool IsVisible() const { return m_visible; }

private:
    glm::vec2 m_position{0.0f};
    float m_width = 200.0f;
    float m_height = 150.0f;
    bool m_visible = true;

    ResourceStock* m_resourceStock = nullptr;
};

// ============================================================================
// Income/Expense Summary
// ============================================================================

/**
 * @brief Widget showing income and expense breakdown
 */
class IncomeExpenseSummary {
public:
    IncomeExpenseSummary();
    ~IncomeExpenseSummary();

    /**
     * @brief Initialize the widget
     */
    void Initialize(const glm::vec2& position, float width, float height);

    /**
     * @brief Update calculations
     */
    void Update(float deltaTime);

    /**
     * @brief Render the widget
     */
    void Render(Nova::Renderer& renderer);

    /**
     * @brief Bind systems
     */
    void BindGatheringSystem(GatheringSystem* gathering) { m_gatheringSystem = gathering; }
    void BindProductionSystem(ProductionSystem* production) { m_productionSystem = production; }
    void BindUpkeepSystem(UpkeepSystem* upkeep) { m_upkeepSystem = upkeep; }
    void BindResourceStock(ResourceStock* stock) { m_resourceStock = stock; }

    /**
     * @brief Set position
     */
    void SetPosition(const glm::vec2& pos) { m_position = pos; }

    /**
     * @brief Show/hide the widget
     */
    void SetVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Toggle visibility
     */
    void Toggle() { m_visible = !m_visible; }

    /**
     * @brief Check if visible
     */
    [[nodiscard]] bool IsVisible() const { return m_visible; }

private:
    glm::vec2 m_position{0.0f};
    float m_width = 300.0f;
    float m_height = 200.0f;
    bool m_visible = false;

    GatheringSystem* m_gatheringSystem = nullptr;
    ProductionSystem* m_productionSystem = nullptr;
    UpkeepSystem* m_upkeepSystem = nullptr;
    ResourceStock* m_resourceStock = nullptr;
};

// ============================================================================
// Resource UI Manager
// ============================================================================

/**
 * @brief Main manager for all resource UI elements
 */
class ResourceUIManager {
public:
    ResourceUIManager();
    ~ResourceUIManager();

    /**
     * @brief Initialize all UI elements
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     */
    void Initialize(float screenWidth, float screenHeight);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update all UI elements
     */
    void Update(float deltaTime);

    /**
     * @brief Render all UI elements
     */
    void Render(Nova::Renderer& renderer);

    /**
     * @brief Handle mouse input
     */
    void HandleMouseInput(const glm::vec2& mousePos, bool clicked);

    // -------------------------------------------------------------------------
    // Binding
    // -------------------------------------------------------------------------

    /**
     * @brief Bind all systems at once
     */
    void BindSystems(
        ResourceStock* stock,
        GatheringSystem* gathering,
        ProductionSystem* production,
        TradingSystem* trading,
        UpkeepSystem* upkeep
    );

    // -------------------------------------------------------------------------
    // Access to Components
    // -------------------------------------------------------------------------

    [[nodiscard]] ResourceBar& GetResourceBar() { return m_resourceBar; }
    [[nodiscard]] ResourceAlertManager& GetAlertManager() { return m_alertManager; }
    [[nodiscard]] StorageCapacityWidget& GetStorageWidget() { return m_storageWidget; }
    [[nodiscard]] IncomeExpenseSummary& GetIncomeSummary() { return m_incomeSummary; }

    // -------------------------------------------------------------------------
    // Visibility
    // -------------------------------------------------------------------------

    /**
     * @brief Show/hide the entire resource UI
     */
    void SetVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Check if visible
     */
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    /**
     * @brief Toggle detailed view
     */
    void ToggleDetailedView();

private:
    ResourceBar m_resourceBar;
    ResourceAlertManager m_alertManager;
    StorageCapacityWidget m_storageWidget;
    IncomeExpenseSummary m_incomeSummary;

    float m_screenWidth = 1280.0f;
    float m_screenHeight = 720.0f;
    bool m_visible = true;
    bool m_detailedView = false;
    bool m_initialized = false;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Format a resource amount for display
 * @param amount The amount to format
 * @param abbreviated Use K/M abbreviations for large numbers
 */
[[nodiscard]] std::string FormatResourceAmount(int amount, bool abbreviated = true);

/**
 * @brief Format a rate for display (per second)
 * @param rate The rate to format
 */
[[nodiscard]] std::string FormatResourceRate(float rate);

/**
 * @brief Format time until depletion/fill
 * @param seconds Time in seconds
 */
[[nodiscard]] std::string FormatTimeRemaining(float seconds);

/**
 * @brief Get color for a rate (green for positive, red for negative)
 */
[[nodiscard]] uint32_t GetRateColor(float rate);

} // namespace RTS
} // namespace Vehement
