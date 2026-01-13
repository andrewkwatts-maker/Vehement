#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "../graphics/PreviewRenderer.hpp"

namespace Nova {
namespace Buildings {

// Forward declarations
class BuildingInstance;
class TechTreeNode;
class ProductionQueue;

using BuildingInstancePtr = std::shared_ptr<BuildingInstance>;
using TechTreeNodePtr = std::shared_ptr<TechTreeNode>;

// =============================================================================
// Tech Tree System
// =============================================================================

/**
 * @brief Types of tech tree upgrades
 */
enum class UpgradeType {
    BuildingLevel,      // Unlock higher building levels
    UnitProduction,     // Unlock new unit types
    ResourceBonus,      // Increase resource production
    ComponentUnlock,    // Unlock new building components
    AbilityUnlock,      // Unlock special abilities
    PassiveBonus        // Passive stat improvements
};

/**
 * @brief Represents a single research/upgrade node in tech tree
 */
class TechTreeNode {
public:
    TechTreeNode() = default;
    TechTreeNode(const std::string& id, const std::string& name, UpgradeType type);

    // Identification
    std::string GetId() const { return m_id; }
    std::string GetName() const { return m_name; }
    std::string GetDescription() const { return m_description; }
    UpgradeType GetType() const { return m_type; }

    // Icon/visual
    std::string GetIconPath() const { return m_iconPath; }
    glm::vec2 GetTreePosition() const { return m_treePosition; }

    // Requirements
    int GetRequiredBuildingLevel() const { return m_requiredBuildingLevel; }
    const std::vector<std::string>& GetPrerequisites() const { return m_prerequisites; }
    bool CanResearch(int buildingLevel, const std::vector<std::string>& completedResearch) const;

    // Cost
    const std::unordered_map<std::string, float>& GetCost() const { return m_cost; }
    float GetResearchTime() const { return m_researchTime; }

    // Effects
    struct Effect {
        std::string target;        // What it affects (e.g., "unit:chicken", "production:food", "component:barn")
        std::string effectType;    // "unlock", "multiply", "add", "enable"
        float value = 1.0f;        // Numeric value for the effect
        nlohmann::json data;       // Additional data
    };
    const std::vector<Effect>& GetEffects() const { return m_effects; }

    // Serialization
    nlohmann::json Serialize() const;
    static TechTreeNodePtr Deserialize(const nlohmann::json& json);

private:
    std::string m_id;
    std::string m_name;
    std::string m_description;
    UpgradeType m_type;
    std::string m_iconPath;
    glm::vec2 m_treePosition{0.0f, 0.0f}; // Position in tech tree UI

    // Requirements
    int m_requiredBuildingLevel = 1;
    std::vector<std::string> m_prerequisites;  // IDs of required techs

    // Cost
    std::unordered_map<std::string, float> m_cost; // Resource costs
    float m_researchTime = 10.0f;                  // Time in seconds

    // Effects
    std::vector<Effect> m_effects;

    friend class TechTree;
};

/**
 * @brief Manages tech tree for a building type
 */
class TechTree {
public:
    TechTree() = default;

    void AddNode(TechTreeNodePtr node);
    TechTreeNodePtr GetNode(const std::string& id) const;
    std::vector<TechTreeNodePtr> GetAvailableNodes(int buildingLevel,
                                                    const std::vector<std::string>& completedResearch) const;
    std::vector<TechTreeNodePtr> GetAllNodes() const;

    // Check if specific tech is researched
    bool IsResearched(const std::string& techId, const std::vector<std::string>& completedResearch) const;

    // Serialization
    nlohmann::json Serialize() const;
    static std::shared_ptr<TechTree> Deserialize(const nlohmann::json& json);

private:
    std::unordered_map<std::string, TechTreeNodePtr> m_nodes;
};

// =============================================================================
// Production Queue System
// =============================================================================

/**
 * @brief Represents a unit/item in production
 */
struct ProductionItem {
    std::string unitId;            // ID of unit being produced
    std::string unitName;          // Display name
    std::string iconPath;          // Icon to display
    float productionTime;          // Total time to produce
    float elapsedTime = 0.0f;      // Time already spent
    std::unordered_map<std::string, float> cost; // Resource costs
    int priority = 0;              // Higher priority = processed first
    bool paused = false;

    float GetProgress() const { return productionTime > 0 ? elapsedTime / productionTime : 0.0f; }
    bool IsComplete() const { return elapsedTime >= productionTime; }

    nlohmann::json Serialize() const;
    static ProductionItem Deserialize(const nlohmann::json& json);
};

/**
 * @brief Manages production queue for units/items
 */
class ProductionQueue {
public:
    ProductionQueue() = default;

    // Queue management
    void AddToQueue(const ProductionItem& item);
    void RemoveFromQueue(size_t index);
    void ClearQueue();
    void PauseProduction(bool pause);
    void SetPriority(size_t index, int priority);

    // Update
    void Update(float deltaTime);
    ProductionItem* GetCurrentItem();
    const std::vector<ProductionItem>& GetQueue() const { return m_queue; }

    // Callbacks
    using OnProductionCompleteCallback = std::function<void(const ProductionItem&)>;
    void SetOnProductionComplete(OnProductionCompleteCallback callback) { m_onComplete = callback; }

    // Configuration
    void SetProductionSpeedMultiplier(float multiplier) { m_speedMultiplier = multiplier; }
    float GetProductionSpeedMultiplier() const { return m_speedMultiplier; }

    // Serialization
    nlohmann::json Serialize() const;
    static std::shared_ptr<ProductionQueue> Deserialize(const nlohmann::json& json);

private:
    std::vector<ProductionItem> m_queue;
    float m_speedMultiplier = 1.0f;
    bool m_paused = false;
    OnProductionCompleteCallback m_onComplete;

    void SortByPriority();
};

// =============================================================================
// Unit Definition System
// =============================================================================

/**
 * @brief Defines a produceable unit
 */
class UnitDefinition {
public:
    UnitDefinition() = default;

    std::string GetId() const { return m_id; }
    std::string GetName() const { return m_name; }
    std::string GetDescription() const { return m_description; }
    std::string GetCategory() const { return m_category; }
    std::string GetIconPath() const { return m_iconPath; }
    std::string GetModelPath() const { return m_modelPath; }

    // Requirements
    int GetRequiredBuildingLevel() const { return m_requiredLevel; }
    const std::vector<std::string>& GetRequiredTechs() const { return m_requiredTechs; }
    bool CanProduce(int buildingLevel, const std::vector<std::string>& completedResearch) const;

    // Production
    float GetProductionTime() const { return m_productionTime; }
    const std::unordered_map<std::string, float>& GetCost() const { return m_cost; }
    int GetPopulationCost() const { return m_populationCost; }

    // Stats
    const nlohmann::json& GetStats() const { return m_stats; }

    // Create production item from this definition
    ProductionItem CreateProductionItem() const;

    // Serialization
    nlohmann::json Serialize() const;
    static std::shared_ptr<UnitDefinition> Deserialize(const nlohmann::json& json);

private:
    std::string m_id;
    std::string m_name;
    std::string m_description;
    std::string m_category;
    std::string m_iconPath;
    std::string m_modelPath;

    // Requirements
    int m_requiredLevel = 1;
    std::vector<std::string> m_requiredTechs;

    // Production
    float m_productionTime = 10.0f;
    std::unordered_map<std::string, float> m_cost;
    int m_populationCost = 1;

    // Stats
    nlohmann::json m_stats; // Flexible stat storage (health, damage, speed, etc.)
};

// =============================================================================
// Building UI State
// =============================================================================

/**
 * @brief UI state and interaction for buildings
 */
class BuildingUIState {
public:
    BuildingUIState() = default;

    // Selection state
    bool IsSelected() const { return m_selected; }
    void SetSelected(bool selected) { m_selected = selected; }

    // Active tab in building UI
    enum class UITab {
        Overview,       // General building info
        Production,     // Unit production queue
        TechTree,       // Research/upgrades
        Components,     // Building component management
        Stats           // Statistics and info
    };
    UITab GetActiveTab() const { return m_activeTab; }
    void SetActiveTab(UITab tab) { m_activeTab = tab; }

    // Hovered elements (for tooltips)
    std::string GetHoveredElement() const { return m_hoveredElement; }
    void SetHoveredElement(const std::string& element) { m_hoveredElement = element; }

    // Research progress
    struct ActiveResearch {
        std::string techId;
        float startTime = 0.0f;
        float duration = 0.0f;
        float GetProgress(float currentTime) const {
            if (duration <= 0) return 0.0f;
            return std::min(1.0f, (currentTime - startTime) / duration);
        }
    };
    const std::optional<ActiveResearch>& GetActiveResearch() const { return m_activeResearch; }
    void SetActiveResearch(const std::string& techId, float duration, float currentTime);
    void ClearActiveResearch() { m_activeResearch.reset(); }

    // Notifications
    struct Notification {
        std::string message;
        float timestamp;
        float duration = 5.0f;
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    };
    void AddNotification(const std::string& message, float timestamp,
                        const glm::vec4& color = glm::vec4(1.0f));
    std::vector<Notification> GetActiveNotifications(float currentTime) const;

    // Serialization
    nlohmann::json Serialize() const;
    static std::shared_ptr<BuildingUIState> Deserialize(const nlohmann::json& json);

private:
    bool m_selected = false;
    UITab m_activeTab = UITab::Overview;
    std::string m_hoveredElement;
    std::optional<ActiveResearch> m_activeResearch;
    std::vector<Notification> m_notifications;
};

// =============================================================================
// Building UI Renderer (ImGui-based)
// =============================================================================

/**
 * @brief Renders building UI using ImGui
 *
 * This class handles ImGui-based building UI rendering with integrated
 * 3D preview support via PreviewRenderer.
 */
class BuildingUIRenderer {
public:
    BuildingUIRenderer();
    ~BuildingUIRenderer();

    /**
     * @brief Initialize the renderer and its preview component
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    // Main render function
    void RenderBuildingUI(BuildingInstancePtr building, BuildingUIState& uiState,
                         float currentTime, float deltaTime);

    // Individual panel renderers
    void RenderOverviewPanel(BuildingInstancePtr building);
    void RenderProductionPanel(BuildingInstancePtr building, float currentTime);
    void RenderTechTreePanel(BuildingInstancePtr building, BuildingUIState& uiState,
                            float currentTime);
    void RenderComponentsPanel(BuildingInstancePtr building);
    void RenderStatsPanel(BuildingInstancePtr building);

    // Helper renders
    void RenderProductionQueue(ProductionQueue& queue, float currentTime);
    void RenderTechNode(TechTreeNodePtr node, bool available, bool researched,
                       const glm::vec2& position);
    void RenderUnitButton(const UnitDefinition& unit, bool available);
    void RenderProgressBar(float progress, const glm::vec2& size,
                          const std::string& label = "");

    /**
     * @brief Render a 3D preview of a building mesh
     *
     * Uses the internal PreviewRenderer to generate a preview image.
     *
     * @param mesh Building mesh to preview
     * @param material Material to apply
     * @param size Preview size in pixels
     */
    void RenderBuildingPreview(std::shared_ptr<Mesh> mesh,
                               std::shared_ptr<Material> material,
                               const glm::ivec2& size);

    /**
     * @brief Get the preview texture ID for ImGui rendering
     */
    uint32_t GetPreviewTextureID() const;

    /**
     * @brief Access the underlying PreviewRenderer for advanced configuration
     */
    PreviewRenderer* GetPreviewRenderer() { return m_previewRenderer.get(); }
    const PreviewRenderer* GetPreviewRenderer() const { return m_previewRenderer.get(); }

    // Configuration
    void SetUIScale(float scale) { m_uiScale = scale; }
    float GetUIScale() const { return m_uiScale; }

private:
    float m_uiScale = 1.0f;
    std::unique_ptr<PreviewRenderer> m_previewRenderer;
};

} // namespace Buildings
} // namespace Nova
