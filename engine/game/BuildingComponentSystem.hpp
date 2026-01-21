#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace Nova {
namespace Buildings {

// Forward declarations
class BuildingComponent;
class BuildingInstance;
class BuildingTemplate;
class ComponentPlacementRule;

using ComponentPtr = std::shared_ptr<BuildingComponent>;
using BuildingInstancePtr = std::shared_ptr<BuildingInstance>;
using BuildingTemplatePtr = std::shared_ptr<BuildingTemplate>;

// =============================================================================
// Component Placement Rules
// =============================================================================

/**
 * @brief Defines how a component can be placed relative to others
 */
enum class PlacementMode {
    Ground,          // Must be on ground plane
    Stacked,         // Can stack on top of other components
    Attached,        // Must attach to another component's surface
    Floating,        // Can float (supports, pillars underneath)
    Merged           // Can merge/blend with other components
};

/**
 * @brief Defines snapping behavior
 */
struct SnapRule {
    bool enabled = true;
    float snapDistance = 0.5f;        // Distance at which snapping occurs
    float snapAngle = 15.0f;          // Angle in degrees for rotation snapping
    std::vector<std::string> snapToTags; // Only snap to components with these tags
    bool alignNormals = false;        // Align to surface normal when snapping
};

/**
 * @brief Intersection/collision rules for component placement
 */
struct IntersectionRule {
    float maxIntersectionDepth = 0.1f;     // Max allowed penetration (for blending)
    float minClearance = 0.0f;             // Min distance from other components
    bool allowSelfIntersection = true;     // Can intersect with same component type
    std::vector<std::string> allowIntersectionWith; // Specific component types allowed to intersect
    std::vector<std::string> forbidIntersectionWith; // Never intersect with these
};

/**
 * @brief Defines valid placement constraints for a component
 */
class ComponentPlacementRule {
public:
    PlacementMode mode = PlacementMode::Ground;
    SnapRule snapRule;
    IntersectionRule intersectionRule;

    // Rotation constraints
    bool allowRotation = true;
    bool lockToCardinalDirections = false;  // Only 0, 90, 180, 270 degrees
    glm::vec3 rotationAxis = glm::vec3(0, 1, 0); // Allowed rotation axis (usually Y for buildings)

    // Stacking constraints
    int maxStackHeight = 1;                 // How many can stack vertically
    float maxStackOffset = 0.2f;            // Max XZ offset when stacking
    bool requiresSupport = true;            // Must have component below

    // Area requirements
    glm::vec2 minFootprint{1.0f, 1.0f};    // Minimum ground footprint needed
    glm::vec2 maxFootprint{10.0f, 10.0f};  // Maximum ground footprint allowed
    float minHeight = 0.0f;
    float maxHeight = 20.0f;

    // Level requirements
    int minBuildingLevel = 1;               // Minimum building level to place
    int maxBuildingLevel = 10;              // Maximum building level can appear

    nlohmann::json Serialize() const;
    static ComponentPlacementRule Deserialize(const nlohmann::json& json);
};

// =============================================================================
// Building Component Definition
// =============================================================================

/**
 * @brief Represents a single placeable component of a building
 * Components are SDF models that can be positioned, rotated, and combined
 */
class BuildingComponent {
public:
    BuildingComponent() = default;
    BuildingComponent(const std::string& id, const std::string& name);
    virtual ~BuildingComponent() = default;

    // Identity
    std::string GetId() const { return m_id; }
    std::string GetName() const { return m_name; }
    std::string GetCategory() const { return m_category; }
    std::vector<std::string> GetTags() const { return m_tags; }

    void SetId(const std::string& id) { m_id = id; }
    void SetName(const std::string& name) { m_name = name; }
    void SetCategory(const std::string& category) { m_category = category; }
    void AddTag(const std::string& tag) { m_tags.push_back(tag); }

    // SDF Model
    void SetSdfModel(const nlohmann::json& sdfModel) { m_sdfModel = sdfModel; }
    const nlohmann::json& GetSdfModel() const { return m_sdfModel; }

    // Bounds
    void SetBounds(const glm::vec3& min, const glm::vec3& max) {
        m_boundsMin = min;
        m_boundsMax = max;
    }
    glm::vec3 GetBoundsMin() const { return m_boundsMin; }
    glm::vec3 GetBoundsMax() const { return m_boundsMax; }
    glm::vec3 GetBoundsSize() const { return m_boundsMax - m_boundsMin; }
    glm::vec3 GetBoundsCenter() const { return (m_boundsMin + m_boundsMax) * 0.5f; }

    // Placement rules
    void SetPlacementRule(const ComponentPlacementRule& rule) { m_placementRule = rule; }
    const ComponentPlacementRule& GetPlacementRule() const { return m_placementRule; }

    // Variants (different visual styles of same component)
    struct Variant {
        std::string id;
        std::string name;
        nlohmann::json sdfModel;
        float weight = 1.0f;  // Probability weight for random selection
        int minLevel = 1;
        int maxLevel = 10;
    };

    void AddVariant(const Variant& variant) { m_variants.push_back(variant); }
    const std::vector<Variant>& GetVariants() const { return m_variants; }
    Variant GetRandomVariant(int buildingLevel, int seed = 0) const;

    // Cost (if component has individual cost)
    struct Cost {
        int gold = 0;
        int wood = 0;
        int stone = 0;
    };
    void SetCost(const Cost& cost) { m_cost = cost; }
    const Cost& GetCost() const { return m_cost; }

    // Serialization
    virtual nlohmann::json Serialize() const;
    static ComponentPtr Deserialize(const nlohmann::json& json);

protected:
    std::string m_id;
    std::string m_name;
    std::string m_category;      // e.g., "structure", "decoration", "functional"
    std::vector<std::string> m_tags;

    nlohmann::json m_sdfModel;
    glm::vec3 m_boundsMin{-1, 0, -1};
    glm::vec3 m_boundsMax{1, 2, 1};

    ComponentPlacementRule m_placementRule;
    std::vector<Variant> m_variants;
    Cost m_cost;
};

// =============================================================================
// Placed Component Instance
// =============================================================================

/**
 * @brief An instance of a component placed in a building
 */
struct PlacedComponent {
    ComponentPtr component;
    std::string variantId;

    glm::vec3 position{0, 0, 0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1, 1, 1};

    // Randomization seed for procedural variation
    int randomSeed = 0;

    // Relationships
    std::string attachedToId;        // ID of component this is attached to
    std::vector<std::string> supportingIds; // IDs of components this is supporting

    // Validation state
    bool isValid = true;
    std::vector<std::string> validationErrors;

    glm::mat4 GetTransformMatrix() const;
    glm::vec3 GetWorldBoundsMin() const;
    glm::vec3 GetWorldBoundsMax() const;

    nlohmann::json Serialize() const;
    static PlacedComponent Deserialize(const nlohmann::json& json);
};

// =============================================================================
// Building Template (Base Definition)
// =============================================================================

/**
 * @brief Defines a building type with its allowed components and upgrade path
 */
class BuildingTemplate {
public:
    BuildingTemplate(const std::string& id, const std::string& name);

    std::string GetId() const { return m_id; }
    std::string GetName() const { return m_name; }
    std::string GetRace() const { return m_race; }
    void SetRace(const std::string& race) { m_race = race; }

    // Available components for this building type
    void AddAvailableComponent(ComponentPtr component, int minLevel = 1, int maxLevel = 10);
    std::vector<ComponentPtr> GetAvailableComponents(int level) const;

    // Required components per level
    struct LevelRequirement {
        int level;
        int minComponents;
        int maxComponents;
        std::vector<std::string> requiredComponentCategories; // Must have at least one from each category
        float minFootprintArea;        // Total ground area needed
        float maxFootprintArea;
        glm::vec3 minTotalBounds;      // Minimum total building size
        glm::vec3 maxTotalBounds;
    };

    void AddLevelRequirement(const LevelRequirement& req);
    std::optional<LevelRequirement> GetLevelRequirement(int level) const;

    // Upgrade path
    struct UpgradeInfo {
        int fromLevel;
        int toLevel;
        int goldCost;
        int woodCost;
        int stoneCost;
        float buildTime;
        int requiredMergedBuildings;   // How many buildings need to merge for this level
        float requiredTotalArea;       // Combined footprint area needed
    };

    void AddUpgradePath(const UpgradeInfo& upgrade);
    std::optional<UpgradeInfo> GetUpgradePath(int fromLevel, int toLevel) const;

    // Default component layouts (suggestions/presets)
    struct ComponentLayout {
        std::string name;
        int level;
        std::vector<PlacedComponent> components;
    };

    void AddPresetLayout(const ComponentLayout& layout);
    std::vector<ComponentLayout> GetPresetLayouts(int level) const;

    nlohmann::json Serialize() const;
    static BuildingTemplatePtr Deserialize(const nlohmann::json& json);

private:
    std::string m_id;
    std::string m_name;
    std::string m_race;

    struct ComponentEntry {
        ComponentPtr component;
        int minLevel;
        int maxLevel;
    };
    std::vector<ComponentEntry> m_availableComponents;

    std::unordered_map<int, LevelRequirement> m_levelRequirements;
    std::vector<UpgradeInfo> m_upgradePaths;
    std::vector<ComponentLayout> m_presetLayouts;
};

// =============================================================================
// Building Instance (Actual Building in Game)
// =============================================================================

/**
 * @brief A constructed building instance in the game world
 */
class BuildingInstance {
public:
    BuildingInstance(BuildingTemplatePtr template_);

    std::string GetId() const { return m_id; }
    BuildingTemplatePtr GetTemplate() const { return m_template; }

    // Level and upgrade
    int GetLevel() const { return m_level; }
    bool CanUpgradeTo(int targetLevel) const;
    bool Upgrade(int targetLevel);

    // Merging buildings
    bool CanMergeWith(const BuildingInstance& other) const;
    void MergeWith(BuildingInstancePtr other);
    std::vector<std::string> GetMergedBuildingIds() const { return m_mergedBuildingIds; }

    // Component placement
    std::string AddComponent(PlacedComponent component);
    bool RemoveComponent(const std::string& componentId);
    void UpdateComponent(const std::string& componentId, const PlacedComponent& component);
    PlacedComponent* GetComponent(const std::string& componentId);
    const std::vector<PlacedComponent>& GetAllComponents() const { return m_placedComponents; }

    // Footprint/bounds
    glm::vec2 GetFootprintSize() const;
    float GetFootprintArea() const;
    glm::vec3 GetTotalBoundsMin() const;
    glm::vec3 GetTotalBoundsMax() const;

    // Validation
    bool Validate(std::vector<std::string>& errors) const;
    bool ValidateComponentPlacement(const PlacedComponent& component, std::vector<std::string>& errors) const;

    // Position in world
    void SetWorldPosition(const glm::vec3& pos) { m_worldPosition = pos; }
    glm::vec3 GetWorldPosition() const { return m_worldPosition; }

    void SetWorldRotation(const glm::quat& rot) { m_worldRotation = rot; }
    glm::quat GetWorldRotation() const { return m_worldRotation; }

    // Serialization
    nlohmann::json Serialize() const;
    static BuildingInstancePtr Deserialize(const nlohmann::json& json);

private:
    std::string m_id;
    BuildingTemplatePtr m_template;
    int m_level = 1;

    std::vector<PlacedComponent> m_placedComponents;
    std::unordered_map<std::string, int> m_componentIdToIndex;

    std::vector<std::string> m_mergedBuildingIds;
    float m_totalMergedArea = 0.0f;

    glm::vec3 m_worldPosition{0, 0, 0};
    glm::quat m_worldRotation{1, 0, 0, 0};

    static size_t s_nextId;

    bool CheckIntersection(const PlacedComponent& newComp, const PlacedComponent& existing) const;
    bool HasSupport(const PlacedComponent& component) const;
};

// =============================================================================
// Component Library Manager
// =============================================================================

/**
 * @brief Manages all available building components and templates
 */
class ComponentLibrary {
public:
    static ComponentLibrary& Instance();

    // Components
    void RegisterComponent(ComponentPtr component);
    ComponentPtr GetComponent(const std::string& id) const;
    std::vector<ComponentPtr> GetComponentsByCategory(const std::string& category) const;
    std::vector<ComponentPtr> GetComponentsByTags(const std::vector<std::string>& tags) const;
    std::vector<ComponentPtr> GetComponentsForRace(const std::string& race) const;

    // Templates
    void RegisterTemplate(BuildingTemplatePtr template_);
    BuildingTemplatePtr GetTemplate(const std::string& id) const;
    std::vector<BuildingTemplatePtr> GetTemplatesForRace(const std::string& race) const;

    // Loading from files
    void LoadComponentsFromDirectory(const std::string& directory, const std::string& race);
    void LoadTemplatesFromDirectory(const std::string& directory, const std::string& race);

    // Clear
    void Clear();

private:
    ComponentLibrary() = default;

    std::unordered_map<std::string, ComponentPtr> m_components;
    std::unordered_map<std::string, BuildingTemplatePtr> m_templates;
    std::unordered_map<std::string, std::vector<std::string>> m_componentsByRace;
    std::unordered_map<std::string, std::vector<std::string>> m_templatesByRace;
};

// =============================================================================
// Interactive Building Placer (for in-game editor)
// =============================================================================

/**
 * @brief Handles interactive placement of components during building construction
 */
class BuildingPlacer {
public:
    BuildingPlacer(BuildingInstancePtr building);

    // Selection
    void SelectComponent(ComponentPtr component);
    void CycleVariant(); // Cycle through variants of selected component
    ComponentPtr GetSelectedComponent() const { return m_selectedComponent; }
    int GetCurrentVariantIndex() const { return m_currentVariantIndex; }

    // Preview placement
    void SetPreviewPosition(const glm::vec3& pos);
    void SetPreviewRotation(float angleY);
    void ApplySnapToGrid(float gridSize);
    void ApplySnapToNearbyComponents();

    PlacedComponent GetPreviewComponent() const { return m_preview; }
    bool IsPreviewValid() const { return m_previewValid; }
    std::vector<std::string> GetPreviewErrors() const { return m_previewErrors; }

    // Commit placement
    bool PlaceComponent();
    void CancelPlacement();

    // Edit existing
    void SelectExistingComponent(const std::string& componentId);
    void MoveSelectedComponent(const glm::vec3& offset);
    void RotateSelectedComponent(float deltaAngle);
    void DeleteSelectedComponent();

    // Randomization
    void RandomizePreviewSeed();
    int GetPreviewSeed() const { return m_previewSeed; }

private:
    BuildingInstancePtr m_building;
    ComponentPtr m_selectedComponent;
    int m_currentVariantIndex = 0;

    PlacedComponent m_preview;
    bool m_previewValid = false;
    std::vector<std::string> m_previewErrors;
    int m_previewSeed = 0;

    std::string m_editingComponentId;

    void UpdatePreviewValidity();
};

} // namespace Buildings
} // namespace Nova
