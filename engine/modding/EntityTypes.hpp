#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <variant>
#include <optional>

namespace Nova {

/**
 * @brief Property value types for entity attributes
 */
using PropertyValue = std::variant<
    bool,
    int,
    float,
    std::string,
    std::vector<int>,
    std::vector<float>,
    std::vector<std::string>
>;

/**
 * @brief Property definition for entity attributes
 */
struct PropertyDef {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    PropertyValue defaultValue;

    // Constraints
    std::optional<float> minValue;
    std::optional<float> maxValue;
    std::optional<std::vector<std::string>> allowedValues;  // For enums

    // Balance
    float pointCostPerUnit = 0.0f;  // Cost per unit increase
    float basePointCost = 0.0f;     // Base cost to have this property

    // UI hints
    std::string uiWidget = "default";  // default, slider, dropdown, color, etc.
    std::string uiGroup;
    int uiOrder = 0;
    bool hidden = false;
    bool readOnly = false;
};

/**
 * @brief Behavior slot definition
 */
struct BehaviorSlot {
    std::string id;
    std::string name;
    std::string description;
    std::string category;

    // What behaviors can be assigned
    std::vector<std::string> allowedBehaviorTypes;

    // Multiplicity
    int minCount = 0;
    int maxCount = 1;

    // Balance
    float pointCostPerBehavior = 0.0f;
};

/**
 * @brief Entity archetype (base type definition)
 */
struct EntityArchetype {
    std::string id;
    std::string name;
    std::string description;
    std::string category;  // unit, building, hero, projectile, effect, etc.
    std::string parentArchetype;  // Inheritance

    // Properties this archetype has
    std::vector<PropertyDef> properties;

    // Behavior slots
    std::vector<BehaviorSlot> behaviorSlots;

    // Balance constraints
    float minPoints = 0.0f;
    float maxPoints = 100.0f;
    float basePointCost = 0.0f;

    // Visual defaults
    std::string defaultModel;
    std::string defaultIcon;

    // Tags for filtering/categorization
    std::vector<std::string> tags;
};

/**
 * @brief Point allocation entry
 */
struct PointAllocation {
    std::string propertyId;
    float points;
    std::string reason;
};

/**
 * @brief Balance report for an entity
 */
struct BalanceReport {
    float totalPoints = 0.0f;
    float minAllowed = 0.0f;
    float maxAllowed = 0.0f;
    bool isValid = true;
    std::vector<PointAllocation> allocations;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/**
 * @brief Concrete entity instance created from archetype
 */
class EntityDefinition {
public:
    EntityDefinition() = default;
    explicit EntityDefinition(const std::string& archetypeId);

    // Identity
    [[nodiscard]] const std::string& GetId() const { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] const std::string& GetArchetypeId() const { return m_archetypeId; }

    // Properties
    void SetProperty(const std::string& id, const PropertyValue& value);
    [[nodiscard]] PropertyValue GetProperty(const std::string& id) const;
    [[nodiscard]] bool HasProperty(const std::string& id) const;
    [[nodiscard]] const std::unordered_map<std::string, PropertyValue>& GetAllProperties() const { return m_properties; }

    // Convenience getters
    [[nodiscard]] int GetInt(const std::string& id, int defaultVal = 0) const;
    [[nodiscard]] float GetFloat(const std::string& id, float defaultVal = 0.0f) const;
    [[nodiscard]] bool GetBool(const std::string& id, bool defaultVal = false) const;
    [[nodiscard]] std::string GetString(const std::string& id, const std::string& defaultVal = "") const;

    // Behaviors
    void AddBehavior(const std::string& slotId, const std::string& behaviorId);
    void RemoveBehavior(const std::string& slotId, const std::string& behaviorId);
    [[nodiscard]] std::vector<std::string> GetBehaviors(const std::string& slotId) const;
    [[nodiscard]] const std::unordered_map<std::string, std::vector<std::string>>& GetAllBehaviors() const { return m_behaviors; }

    // Balance
    [[nodiscard]] BalanceReport CalculateBalance() const;
    [[nodiscard]] float GetTotalPoints() const;

    // Serialization
    [[nodiscard]] std::string ToJson() const;
    bool FromJson(const std::string& json);

    // Tags
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);
    [[nodiscard]] bool HasTag(const std::string& tag) const;
    [[nodiscard]] const std::vector<std::string>& GetTags() const { return m_tags; }

private:
    std::string m_id;
    std::string m_name;
    std::string m_archetypeId;
    std::unordered_map<std::string, PropertyValue> m_properties;
    std::unordered_map<std::string, std::vector<std::string>> m_behaviors;
    std::vector<std::string> m_tags;
};

/**
 * @brief Registry for entity archetypes and definitions
 */
class EntityTypeRegistry {
public:
    static EntityTypeRegistry& Instance();

    // Archetype management
    void RegisterArchetype(const EntityArchetype& archetype);
    void UnregisterArchetype(const std::string& id);
    [[nodiscard]] const EntityArchetype* GetArchetype(const std::string& id) const;
    [[nodiscard]] std::vector<const EntityArchetype*> GetArchetypesByCategory(const std::string& category) const;
    [[nodiscard]] std::vector<const EntityArchetype*> GetAllArchetypes() const;

    // Definition management
    void RegisterDefinition(std::shared_ptr<EntityDefinition> definition);
    void UnregisterDefinition(const std::string& id);
    [[nodiscard]] std::shared_ptr<EntityDefinition> GetDefinition(const std::string& id) const;
    [[nodiscard]] std::vector<std::shared_ptr<EntityDefinition>> GetDefinitionsByArchetype(const std::string& archetypeId) const;

    // Create new definition from archetype
    [[nodiscard]] std::shared_ptr<EntityDefinition> CreateDefinition(const std::string& archetypeId, const std::string& definitionId);

    // Inheritance resolution
    [[nodiscard]] EntityArchetype GetResolvedArchetype(const std::string& id) const;

    // Load/Save
    bool LoadArchetypesFromFile(const std::string& path);
    bool SaveArchetypesToFile(const std::string& path) const;
    bool LoadDefinitionsFromFile(const std::string& path);
    bool SaveDefinitionsToFile(const std::string& path) const;

    // Validation
    [[nodiscard]] std::vector<std::string> ValidateDefinition(const EntityDefinition& def) const;
    [[nodiscard]] bool IsDefinitionValid(const EntityDefinition& def) const;

private:
    EntityTypeRegistry() = default;

    std::unordered_map<std::string, EntityArchetype> m_archetypes;
    std::unordered_map<std::string, std::shared_ptr<EntityDefinition>> m_definitions;
};

/**
 * @brief Helper to build archetypes fluently
 */
class ArchetypeBuilder {
public:
    explicit ArchetypeBuilder(const std::string& id);

    ArchetypeBuilder& Name(const std::string& name);
    ArchetypeBuilder& Description(const std::string& desc);
    ArchetypeBuilder& Category(const std::string& category);
    ArchetypeBuilder& Parent(const std::string& parentId);
    ArchetypeBuilder& Tag(const std::string& tag);
    ArchetypeBuilder& PointRange(float min, float max);
    ArchetypeBuilder& BaseCost(float cost);

    // Property builders
    ArchetypeBuilder& IntProperty(const std::string& id, const std::string& name, int defaultVal,
                                   int min = 0, int max = 100, float pointCost = 0.0f);
    ArchetypeBuilder& FloatProperty(const std::string& id, const std::string& name, float defaultVal,
                                     float min = 0.0f, float max = 1.0f, float pointCost = 0.0f);
    ArchetypeBuilder& BoolProperty(const std::string& id, const std::string& name, bool defaultVal,
                                    float pointCost = 0.0f);
    ArchetypeBuilder& StringProperty(const std::string& id, const std::string& name,
                                      const std::string& defaultVal);
    ArchetypeBuilder& EnumProperty(const std::string& id, const std::string& name,
                                    const std::vector<std::string>& options, const std::string& defaultVal);

    // Behavior slot builder
    ArchetypeBuilder& BehaviorSlot(const std::string& id, const std::string& name,
                                    const std::vector<std::string>& allowedTypes,
                                    int minCount = 0, int maxCount = 1, float pointCost = 0.0f);

    [[nodiscard]] EntityArchetype Build() const;
    void Register();

private:
    EntityArchetype m_archetype;
};

} // namespace Nova
