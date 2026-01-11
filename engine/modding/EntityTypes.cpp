#include "EntityTypes.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Nova {

// ============================================================================
// EntityDefinition Implementation
// ============================================================================

EntityDefinition::EntityDefinition(const std::string& archetypeId)
    : m_archetypeId(archetypeId) {

    // Initialize with archetype defaults
    const auto* archetype = EntityTypeRegistry::Instance().GetArchetype(archetypeId);
    if (archetype) {
        for (const auto& prop : archetype->properties) {
            m_properties[prop.id] = prop.defaultValue;
        }
    }
}

void EntityDefinition::SetProperty(const std::string& id, const PropertyValue& value) {
    m_properties[id] = value;
}

PropertyValue EntityDefinition::GetProperty(const std::string& id) const {
    auto it = m_properties.find(id);
    if (it != m_properties.end()) {
        return it->second;
    }
    return PropertyValue{};
}

bool EntityDefinition::HasProperty(const std::string& id) const {
    return m_properties.count(id) > 0;
}

int EntityDefinition::GetInt(const std::string& id, int defaultVal) const {
    auto it = m_properties.find(id);
    if (it != m_properties.end()) {
        if (auto* val = std::get_if<int>(&it->second)) {
            return *val;
        }
    }
    return defaultVal;
}

float EntityDefinition::GetFloat(const std::string& id, float defaultVal) const {
    auto it = m_properties.find(id);
    if (it != m_properties.end()) {
        if (auto* val = std::get_if<float>(&it->second)) {
            return *val;
        }
    }
    return defaultVal;
}

bool EntityDefinition::GetBool(const std::string& id, bool defaultVal) const {
    auto it = m_properties.find(id);
    if (it != m_properties.end()) {
        if (auto* val = std::get_if<bool>(&it->second)) {
            return *val;
        }
    }
    return defaultVal;
}

std::string EntityDefinition::GetString(const std::string& id, const std::string& defaultVal) const {
    auto it = m_properties.find(id);
    if (it != m_properties.end()) {
        if (auto* val = std::get_if<std::string>(&it->second)) {
            return *val;
        }
    }
    return defaultVal;
}

void EntityDefinition::AddBehavior(const std::string& slotId, const std::string& behaviorId) {
    auto& behaviors = m_behaviors[slotId];
    if (std::find(behaviors.begin(), behaviors.end(), behaviorId) == behaviors.end()) {
        behaviors.push_back(behaviorId);
    }
}

void EntityDefinition::RemoveBehavior(const std::string& slotId, const std::string& behaviorId) {
    auto it = m_behaviors.find(slotId);
    if (it != m_behaviors.end()) {
        auto& behaviors = it->second;
        behaviors.erase(std::remove(behaviors.begin(), behaviors.end(), behaviorId), behaviors.end());
    }
}

std::vector<std::string> EntityDefinition::GetBehaviors(const std::string& slotId) const {
    auto it = m_behaviors.find(slotId);
    if (it != m_behaviors.end()) {
        return it->second;
    }
    return {};
}

BalanceReport EntityDefinition::CalculateBalance() const {
    BalanceReport report;
    const auto* archetype = EntityTypeRegistry::Instance().GetArchetype(m_archetypeId);

    if (!archetype) {
        report.isValid = false;
        report.errors.push_back("Unknown archetype: " + m_archetypeId);
        return report;
    }

    report.minAllowed = archetype->minPoints;
    report.maxAllowed = archetype->maxPoints;

    // Base cost
    report.totalPoints += archetype->basePointCost;
    report.allocations.push_back({"_base", archetype->basePointCost, "Base archetype cost"});

    // Property costs
    for (const auto& propDef : archetype->properties) {
        auto it = m_properties.find(propDef.id);
        if (it == m_properties.end()) continue;

        float cost = propDef.basePointCost;

        // Calculate per-unit cost based on deviation from default
        if (propDef.pointCostPerUnit > 0.0f) {
            if (auto* intVal = std::get_if<int>(&it->second)) {
                if (auto* defaultInt = std::get_if<int>(&propDef.defaultValue)) {
                    cost += (*intVal - *defaultInt) * propDef.pointCostPerUnit;
                }
            } else if (auto* floatVal = std::get_if<float>(&it->second)) {
                if (auto* defaultFloat = std::get_if<float>(&propDef.defaultValue)) {
                    cost += (*floatVal - *defaultFloat) * propDef.pointCostPerUnit;
                }
            }
        }

        if (cost != 0.0f) {
            report.totalPoints += cost;
            report.allocations.push_back({propDef.id, cost, propDef.name});
        }
    }

    // Behavior costs
    for (const auto& slot : archetype->behaviorSlots) {
        auto it = m_behaviors.find(slot.id);
        if (it == m_behaviors.end()) continue;

        float cost = static_cast<float>(it->second.size()) * slot.pointCostPerBehavior;
        if (cost > 0.0f) {
            report.totalPoints += cost;
            report.allocations.push_back({slot.id, cost, slot.name + " behaviors"});
        }

        // Check slot constraints
        int count = static_cast<int>(it->second.size());
        if (count < slot.minCount) {
            report.errors.push_back("Slot '" + slot.name + "' requires at least " +
                                    std::to_string(slot.minCount) + " behaviors");
        }
        if (count > slot.maxCount) {
            report.errors.push_back("Slot '" + slot.name + "' allows at most " +
                                    std::to_string(slot.maxCount) + " behaviors");
        }
    }

    // Validate point range
    if (report.totalPoints < report.minAllowed) {
        report.warnings.push_back("Total points (" + std::to_string(report.totalPoints) +
                                  ") below minimum (" + std::to_string(report.minAllowed) + ")");
    }
    if (report.totalPoints > report.maxAllowed) {
        report.errors.push_back("Total points (" + std::to_string(report.totalPoints) +
                                ") exceeds maximum (" + std::to_string(report.maxAllowed) + ")");
        report.isValid = false;
    }

    if (!report.errors.empty()) {
        report.isValid = false;
    }

    return report;
}

float EntityDefinition::GetTotalPoints() const {
    return CalculateBalance().totalPoints;
}

void EntityDefinition::AddTag(const std::string& tag) {
    if (std::find(m_tags.begin(), m_tags.end(), tag) == m_tags.end()) {
        m_tags.push_back(tag);
    }
}

void EntityDefinition::RemoveTag(const std::string& tag) {
    m_tags.erase(std::remove(m_tags.begin(), m_tags.end(), tag), m_tags.end());
}

bool EntityDefinition::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

std::string EntityDefinition::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << m_id << "\",\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"archetype\": \"" << m_archetypeId << "\",\n";

    // Properties
    ss << "  \"properties\": {\n";
    size_t propIndex = 0;
    for (const auto& [key, value] : m_properties) {
        ss << "    \"" << key << "\": ";
        std::visit([&ss](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, bool>) {
                ss << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, float>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                ss << "\"" << arg << "\"";
            } else if constexpr (std::is_same_v<T, std::vector<int>>) {
                ss << "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    ss << arg[i];
                    if (i < arg.size() - 1) ss << ", ";
                }
                ss << "]";
            } else if constexpr (std::is_same_v<T, std::vector<float>>) {
                ss << "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    ss << arg[i];
                    if (i < arg.size() - 1) ss << ", ";
                }
                ss << "]";
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                ss << "[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    ss << "\"" << arg[i] << "\"";
                    if (i < arg.size() - 1) ss << ", ";
                }
                ss << "]";
            }
        }, value);
        if (++propIndex < m_properties.size()) ss << ",";
        ss << "\n";
    }
    ss << "  },\n";

    // Behaviors
    ss << "  \"behaviors\": {\n";
    size_t behaviorIndex = 0;
    for (const auto& [slot, behaviors] : m_behaviors) {
        ss << "    \"" << slot << "\": [";
        for (size_t i = 0; i < behaviors.size(); ++i) {
            ss << "\"" << behaviors[i] << "\"";
            if (i < behaviors.size() - 1) ss << ", ";
        }
        ss << "]";
        if (++behaviorIndex < m_behaviors.size()) ss << ",";
        ss << "\n";
    }
    ss << "  },\n";

    // Tags
    ss << "  \"tags\": [";
    for (size_t i = 0; i < m_tags.size(); ++i) {
        ss << "\"" << m_tags[i] << "\"";
        if (i < m_tags.size() - 1) ss << ", ";
    }
    ss << "]\n";

    ss << "}";
    return ss.str();
}

// ============================================================================
// EntityTypeRegistry Implementation
// ============================================================================

EntityTypeRegistry& EntityTypeRegistry::Instance() {
    static EntityTypeRegistry instance;
    return instance;
}

void EntityTypeRegistry::RegisterArchetype(const EntityArchetype& archetype) {
    m_archetypes[archetype.id] = archetype;
}

void EntityTypeRegistry::UnregisterArchetype(const std::string& id) {
    m_archetypes.erase(id);
}

const EntityArchetype* EntityTypeRegistry::GetArchetype(const std::string& id) const {
    auto it = m_archetypes.find(id);
    return it != m_archetypes.end() ? &it->second : nullptr;
}

std::vector<const EntityArchetype*> EntityTypeRegistry::GetArchetypesByCategory(const std::string& category) const {
    std::vector<const EntityArchetype*> result;
    for (const auto& [id, archetype] : m_archetypes) {
        if (archetype.category == category) {
            result.push_back(&archetype);
        }
    }
    return result;
}

std::vector<const EntityArchetype*> EntityTypeRegistry::GetAllArchetypes() const {
    std::vector<const EntityArchetype*> result;
    for (const auto& [id, archetype] : m_archetypes) {
        result.push_back(&archetype);
    }
    return result;
}

void EntityTypeRegistry::RegisterDefinition(std::shared_ptr<EntityDefinition> definition) {
    if (definition) {
        m_definitions[definition->GetId()] = definition;
    }
}

void EntityTypeRegistry::UnregisterDefinition(const std::string& id) {
    m_definitions.erase(id);
}

std::shared_ptr<EntityDefinition> EntityTypeRegistry::GetDefinition(const std::string& id) const {
    auto it = m_definitions.find(id);
    return it != m_definitions.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<EntityDefinition>> EntityTypeRegistry::GetDefinitionsByArchetype(const std::string& archetypeId) const {
    std::vector<std::shared_ptr<EntityDefinition>> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->GetArchetypeId() == archetypeId) {
            result.push_back(def);
        }
    }
    return result;
}

std::shared_ptr<EntityDefinition> EntityTypeRegistry::CreateDefinition(const std::string& archetypeId, const std::string& definitionId) {
    auto def = std::make_shared<EntityDefinition>(archetypeId);
    def->SetId(definitionId);
    RegisterDefinition(def);
    return def;
}

EntityArchetype EntityTypeRegistry::GetResolvedArchetype(const std::string& id) const {
    const auto* base = GetArchetype(id);
    if (!base) return {};

    EntityArchetype resolved = *base;

    // Resolve inheritance
    if (!base->parentArchetype.empty()) {
        EntityArchetype parent = GetResolvedArchetype(base->parentArchetype);

        // Merge properties (child overrides parent)
        for (const auto& parentProp : parent.properties) {
            bool found = false;
            for (const auto& prop : resolved.properties) {
                if (prop.id == parentProp.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                resolved.properties.insert(resolved.properties.begin(), parentProp);
            }
        }

        // Merge behavior slots
        for (const auto& parentSlot : parent.behaviorSlots) {
            bool found = false;
            for (const auto& slot : resolved.behaviorSlots) {
                if (slot.id == parentSlot.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                resolved.behaviorSlots.insert(resolved.behaviorSlots.begin(), parentSlot);
            }
        }

        // Merge tags
        for (const auto& tag : parent.tags) {
            if (std::find(resolved.tags.begin(), resolved.tags.end(), tag) == resolved.tags.end()) {
                resolved.tags.push_back(tag);
            }
        }
    }

    return resolved;
}

std::vector<std::string> EntityTypeRegistry::ValidateDefinition(const EntityDefinition& def) const {
    std::vector<std::string> errors;

    const auto* archetype = GetArchetype(def.GetArchetypeId());
    if (!archetype) {
        errors.push_back("Unknown archetype: " + def.GetArchetypeId());
        return errors;
    }

    EntityArchetype resolved = GetResolvedArchetype(def.GetArchetypeId());

    // Validate properties
    for (const auto& propDef : resolved.properties) {
        if (!def.HasProperty(propDef.id)) continue;

        PropertyValue value = def.GetProperty(propDef.id);

        // Check constraints
        if (propDef.minValue.has_value() || propDef.maxValue.has_value()) {
            float numValue = 0.0f;
            if (auto* intVal = std::get_if<int>(&value)) {
                numValue = static_cast<float>(*intVal);
            } else if (auto* floatVal = std::get_if<float>(&value)) {
                numValue = *floatVal;
            }

            if (propDef.minValue.has_value() && numValue < *propDef.minValue) {
                errors.push_back("Property '" + propDef.name + "' below minimum value");
            }
            if (propDef.maxValue.has_value() && numValue > *propDef.maxValue) {
                errors.push_back("Property '" + propDef.name + "' exceeds maximum value");
            }
        }

        // Check enum values
        if (propDef.allowedValues.has_value()) {
            if (auto* strVal = std::get_if<std::string>(&value)) {
                const auto& allowed = *propDef.allowedValues;
                if (std::find(allowed.begin(), allowed.end(), *strVal) == allowed.end()) {
                    errors.push_back("Property '" + propDef.name + "' has invalid value: " + *strVal);
                }
            }
        }
    }

    // Validate balance
    BalanceReport balance = def.CalculateBalance();
    for (const auto& error : balance.errors) {
        errors.push_back(error);
    }

    return errors;
}

bool EntityTypeRegistry::IsDefinitionValid(const EntityDefinition& def) const {
    return ValidateDefinition(def).empty();
}

// ============================================================================
// ArchetypeBuilder Implementation
// ============================================================================

ArchetypeBuilder::ArchetypeBuilder(const std::string& id) {
    m_archetype.id = id;
}

ArchetypeBuilder& ArchetypeBuilder::Name(const std::string& name) {
    m_archetype.name = name;
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::Description(const std::string& desc) {
    m_archetype.description = desc;
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::Category(const std::string& category) {
    m_archetype.category = category;
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::Parent(const std::string& parentId) {
    m_archetype.parentArchetype = parentId;
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::Tag(const std::string& tag) {
    m_archetype.tags.push_back(tag);
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::PointRange(float min, float max) {
    m_archetype.minPoints = min;
    m_archetype.maxPoints = max;
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::BaseCost(float cost) {
    m_archetype.basePointCost = cost;
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::IntProperty(const std::string& id, const std::string& name, int defaultVal,
                                                 int min, int max, float pointCost) {
    PropertyDef prop;
    prop.id = id;
    prop.name = name;
    prop.defaultValue = defaultVal;
    prop.minValue = static_cast<float>(min);
    prop.maxValue = static_cast<float>(max);
    prop.pointCostPerUnit = pointCost;
    prop.uiWidget = "slider";
    m_archetype.properties.push_back(prop);
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::FloatProperty(const std::string& id, const std::string& name, float defaultVal,
                                                   float min, float max, float pointCost) {
    PropertyDef prop;
    prop.id = id;
    prop.name = name;
    prop.defaultValue = defaultVal;
    prop.minValue = min;
    prop.maxValue = max;
    prop.pointCostPerUnit = pointCost;
    prop.uiWidget = "slider";
    m_archetype.properties.push_back(prop);
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::BoolProperty(const std::string& id, const std::string& name, bool defaultVal,
                                                  float pointCost) {
    PropertyDef prop;
    prop.id = id;
    prop.name = name;
    prop.defaultValue = defaultVal;
    prop.basePointCost = pointCost;
    prop.uiWidget = "checkbox";
    m_archetype.properties.push_back(prop);
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::StringProperty(const std::string& id, const std::string& name,
                                                    const std::string& defaultVal) {
    PropertyDef prop;
    prop.id = id;
    prop.name = name;
    prop.defaultValue = defaultVal;
    prop.uiWidget = "text";
    m_archetype.properties.push_back(prop);
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::EnumProperty(const std::string& id, const std::string& name,
                                                  const std::vector<std::string>& options, const std::string& defaultVal) {
    PropertyDef prop;
    prop.id = id;
    prop.name = name;
    prop.defaultValue = defaultVal;
    prop.allowedValues = options;
    prop.uiWidget = "dropdown";
    m_archetype.properties.push_back(prop);
    return *this;
}

ArchetypeBuilder& ArchetypeBuilder::BehaviorSlot(const std::string& id, const std::string& name,
                                                  const std::vector<std::string>& allowedTypes,
                                                  int minCount, int maxCount, float pointCost) {
    Nova::BehaviorSlot slot;
    slot.id = id;
    slot.name = name;
    slot.allowedBehaviorTypes = allowedTypes;
    slot.minCount = minCount;
    slot.maxCount = maxCount;
    slot.pointCostPerBehavior = pointCost;
    m_archetype.behaviorSlots.push_back(slot);
    return *this;
}

EntityArchetype ArchetypeBuilder::Build() const {
    return m_archetype;
}

void ArchetypeBuilder::Register() {
    EntityTypeRegistry::Instance().RegisterArchetype(m_archetype);
}

} // namespace Nova
