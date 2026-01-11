#include "BuildingComponentSystem.hpp"
#include <algorithm>
#include <random>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace Nova {
namespace Buildings {

// =============================================================================
// ComponentPlacementRule
// =============================================================================

nlohmann::json ComponentPlacementRule::Serialize() const {
    nlohmann::json json;
    json["mode"] = static_cast<int>(mode);
    json["allowRotation"] = allowRotation;
    json["lockToCardinalDirections"] = lockToCardinalDirections;
    json["rotationAxis"] = {rotationAxis.x, rotationAxis.y, rotationAxis.z};
    json["maxStackHeight"] = maxStackHeight;
    json["maxStackOffset"] = maxStackOffset;
    json["requiresSupport"] = requiresSupport;
    json["minFootprint"] = {minFootprint.x, minFootprint.y};
    json["maxFootprint"] = {maxFootprint.x, maxFootprint.y};
    json["minHeight"] = minHeight;
    json["maxHeight"] = maxHeight;
    json["minBuildingLevel"] = minBuildingLevel;
    json["maxBuildingLevel"] = maxBuildingLevel;

    json["snapRule"] = {
        {"enabled", snapRule.enabled},
        {"snapDistance", snapRule.snapDistance},
        {"snapAngle", snapRule.snapAngle},
        {"snapToTags", snapRule.snapToTags},
        {"alignNormals", snapRule.alignNormals}
    };

    json["intersectionRule"] = {
        {"maxIntersectionDepth", intersectionRule.maxIntersectionDepth},
        {"minClearance", intersectionRule.minClearance},
        {"allowSelfIntersection", intersectionRule.allowSelfIntersection},
        {"allowIntersectionWith", intersectionRule.allowIntersectionWith},
        {"forbidIntersectionWith", intersectionRule.forbidIntersectionWith}
    };

    return json;
}

ComponentPlacementRule ComponentPlacementRule::Deserialize(const nlohmann::json& json) {
    ComponentPlacementRule rule;

    if (json.contains("mode")) rule.mode = static_cast<PlacementMode>(json["mode"].get<int>());
    if (json.contains("allowRotation")) rule.allowRotation = json["allowRotation"];
    if (json.contains("lockToCardinalDirections")) rule.lockToCardinalDirections = json["lockToCardinalDirections"];
    if (json.contains("maxStackHeight")) rule.maxStackHeight = json["maxStackHeight"];
    if (json.contains("maxStackOffset")) rule.maxStackOffset = json["maxStackOffset"];
    if (json.contains("requiresSupport")) rule.requiresSupport = json["requiresSupport"];
    if (json.contains("minBuildingLevel")) rule.minBuildingLevel = json["minBuildingLevel"];
    if (json.contains("maxBuildingLevel")) rule.maxBuildingLevel = json["maxBuildingLevel"];

    if (json.contains("snapRule")) {
        auto& sr = json["snapRule"];
        if (sr.contains("enabled")) rule.snapRule.enabled = sr["enabled"];
        if (sr.contains("snapDistance")) rule.snapRule.snapDistance = sr["snapDistance"];
        if (sr.contains("snapAngle")) rule.snapRule.snapAngle = sr["snapAngle"];
        if (sr.contains("snapToTags")) rule.snapRule.snapToTags = sr["snapToTags"];
        if (sr.contains("alignNormals")) rule.snapRule.alignNormals = sr["alignNormals"];
    }

    if (json.contains("intersectionRule")) {
        auto& ir = json["intersectionRule"];
        if (ir.contains("maxIntersectionDepth")) rule.intersectionRule.maxIntersectionDepth = ir["maxIntersectionDepth"];
        if (ir.contains("minClearance")) rule.intersectionRule.minClearance = ir["minClearance"];
        if (ir.contains("allowSelfIntersection")) rule.intersectionRule.allowSelfIntersection = ir["allowSelfIntersection"];
        if (ir.contains("allowIntersectionWith")) rule.intersectionRule.allowIntersectionWith = ir["allowIntersectionWith"];
        if (ir.contains("forbidIntersectionWith")) rule.intersectionRule.forbidIntersectionWith = ir["forbidIntersectionWith"];
    }

    return rule;
}

// =============================================================================
// BuildingComponent
// =============================================================================

BuildingComponent::BuildingComponent(const std::string& id, const std::string& name)
    : m_id(id), m_name(name) {
}

BuildingComponent::Variant BuildingComponent::GetRandomVariant(int buildingLevel, int seed) const {
    // Filter variants by level
    std::vector<const Variant*> validVariants;
    for (const auto& variant : m_variants) {
        if (buildingLevel >= variant.minLevel && buildingLevel <= variant.maxLevel) {
            validVariants.push_back(&variant);
        }
    }

    if (validVariants.empty()) {
        // Return default if no variants match
        Variant defaultVariant;
        defaultVariant.id = m_id + "_default";
        defaultVariant.name = m_name;
        defaultVariant.sdfModel = m_sdfModel;
        return defaultVariant;
    }

    // Calculate total weight
    float totalWeight = 0.0f;
    for (const auto* variant : validVariants) {
        totalWeight += variant->weight;
    }

    // Random selection based on weight
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float randomValue = dist(rng);

    float cumulative = 0.0f;
    for (const auto* variant : validVariants) {
        cumulative += variant->weight;
        if (randomValue <= cumulative) {
            return *variant;
        }
    }

    return *validVariants.back();
}

nlohmann::json BuildingComponent::Serialize() const {
    nlohmann::json json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["category"] = m_category;
    json["tags"] = m_tags;
    json["sdfModel"] = m_sdfModel;
    json["bounds"] = {
        {"min", {m_boundsMin.x, m_boundsMin.y, m_boundsMin.z}},
        {"max", {m_boundsMax.x, m_boundsMax.y, m_boundsMax.z}}
    };
    json["placementRule"] = m_placementRule.Serialize();
    json["cost"] = {
        {"gold", m_cost.gold},
        {"wood", m_cost.wood},
        {"stone", m_cost.stone}
    };

    nlohmann::json variantsJson = nlohmann::json::array();
    for (const auto& variant : m_variants) {
        variantsJson.push_back({
            {"id", variant.id},
            {"name", variant.name},
            {"sdfModel", variant.sdfModel},
            {"weight", variant.weight},
            {"minLevel", variant.minLevel},
            {"maxLevel", variant.maxLevel}
        });
    }
    json["variants"] = variantsJson;

    return json;
}

ComponentPtr BuildingComponent::Deserialize(const nlohmann::json& json) {
    auto component = std::make_shared<BuildingComponent>(
        json["id"].get<std::string>(),
        json["name"].get<std::string>()
    );

    if (json.contains("category")) component->m_category = json["category"];
    if (json.contains("tags")) component->m_tags = json["tags"].get<std::vector<std::string>>();
    if (json.contains("sdfModel")) component->m_sdfModel = json["sdfModel"];

    if (json.contains("bounds")) {
        auto& bounds = json["bounds"];
        auto min = bounds["min"];
        auto max = bounds["max"];
        component->m_boundsMin = glm::vec3(min[0], min[1], min[2]);
        component->m_boundsMax = glm::vec3(max[0], max[1], max[2]);
    }

    if (json.contains("placementRule")) {
        component->m_placementRule = ComponentPlacementRule::Deserialize(json["placementRule"]);
    }

    if (json.contains("cost")) {
        auto& cost = json["cost"];
        component->m_cost.gold = cost.value("gold", 0);
        component->m_cost.wood = cost.value("wood", 0);
        component->m_cost.stone = cost.value("stone", 0);
    }

    if (json.contains("variants")) {
        for (const auto& variantJson : json["variants"]) {
            Variant variant;
            variant.id = variantJson["id"];
            variant.name = variantJson["name"];
            variant.sdfModel = variantJson["sdfModel"];
            variant.weight = variantJson.value("weight", 1.0f);
            variant.minLevel = variantJson.value("minLevel", 1);
            variant.maxLevel = variantJson.value("maxLevel", 10);
            component->AddVariant(variant);
        }
    }

    return component;
}

// =============================================================================
// PlacedComponent
// =============================================================================

glm::mat4 PlacedComponent::GetTransformMatrix() const {
    glm::mat4 mat = glm::mat4(1.0f);
    mat = glm::translate(mat, position);
    mat = mat * glm::mat4_cast(rotation);
    mat = glm::scale(mat, scale);
    return mat;
}

glm::vec3 PlacedComponent::GetWorldBoundsMin() const {
    if (!component) return position;

    glm::mat4 transform = GetTransformMatrix();
    glm::vec3 localMin = component->GetBoundsMin();
    glm::vec4 worldMin = transform * glm::vec4(localMin, 1.0f);
    return glm::vec3(worldMin);
}

glm::vec3 PlacedComponent::GetWorldBoundsMax() const {
    if (!component) return position;

    glm::mat4 transform = GetTransformMatrix();
    glm::vec3 localMax = component->GetBoundsMax();
    glm::vec4 worldMax = transform * glm::vec4(localMax, 1.0f);
    return glm::vec3(worldMax);
}

nlohmann::json PlacedComponent::Serialize() const {
    nlohmann::json json;
    json["componentId"] = component ? component->GetId() : "";
    json["variantId"] = variantId;
    json["position"] = {position.x, position.y, position.z};
    json["rotation"] = {rotation.w, rotation.x, rotation.y, rotation.z};
    json["scale"] = {scale.x, scale.y, scale.z};
    json["randomSeed"] = randomSeed;
    json["attachedToId"] = attachedToId;
    json["supportingIds"] = supportingIds;
    json["isValid"] = isValid;
    json["validationErrors"] = validationErrors;
    return json;
}

PlacedComponent PlacedComponent::Deserialize(const nlohmann::json& json) {
    PlacedComponent pc;
    // Note: component pointer needs to be resolved from ComponentLibrary after deserialization
    pc.variantId = json.value("variantId", "");

    if (json.contains("position")) {
        auto& p = json["position"];
        pc.position = glm::vec3(p[0], p[1], p[2]);
    }
    if (json.contains("rotation")) {
        auto& r = json["rotation"];
        pc.rotation = glm::quat(r[0], r[1], r[2], r[3]);
    }
    if (json.contains("scale")) {
        auto& s = json["scale"];
        pc.scale = glm::vec3(s[0], s[1], s[2]);
    }

    pc.randomSeed = json.value("randomSeed", 0);
    pc.attachedToId = json.value("attachedToId", "");
    if (json.contains("supportingIds")) {
        pc.supportingIds = json["supportingIds"].get<std::vector<std::string>>();
    }
    pc.isValid = json.value("isValid", true);
    if (json.contains("validationErrors")) {
        pc.validationErrors = json["validationErrors"].get<std::vector<std::string>>();
    }

    return pc;
}

// =============================================================================
// BuildingTemplate
// =============================================================================

BuildingTemplate::BuildingTemplate(const std::string& id, const std::string& name)
    : m_id(id), m_name(name) {
}

void BuildingTemplate::AddAvailableComponent(ComponentPtr component, int minLevel, int maxLevel) {
    ComponentEntry entry;
    entry.component = component;
    entry.minLevel = minLevel;
    entry.maxLevel = maxLevel;
    m_availableComponents.push_back(entry);
}

std::vector<ComponentPtr> BuildingTemplate::GetAvailableComponents(int level) const {
    std::vector<ComponentPtr> result;
    for (const auto& entry : m_availableComponents) {
        if (level >= entry.minLevel && level <= entry.maxLevel) {
            result.push_back(entry.component);
        }
    }
    return result;
}

void BuildingTemplate::AddLevelRequirement(const LevelRequirement& req) {
    m_levelRequirements[req.level] = req;
}

std::optional<BuildingTemplate::LevelRequirement> BuildingTemplate::GetLevelRequirement(int level) const {
    auto it = m_levelRequirements.find(level);
    if (it != m_levelRequirements.end()) {
        return it->second;
    }
    return std::nullopt;
}

void BuildingTemplate::AddUpgradePath(const UpgradeInfo& upgrade) {
    m_upgradePaths.push_back(upgrade);
}

std::optional<BuildingTemplate::UpgradeInfo> BuildingTemplate::GetUpgradePath(int fromLevel, int toLevel) const {
    for (const auto& upgrade : m_upgradePaths) {
        if (upgrade.fromLevel == fromLevel && upgrade.toLevel == toLevel) {
            return upgrade;
        }
    }
    return std::nullopt;
}

void BuildingTemplate::AddPresetLayout(const ComponentLayout& layout) {
    m_presetLayouts.push_back(layout);
}

std::vector<BuildingTemplate::ComponentLayout> BuildingTemplate::GetPresetLayouts(int level) const {
    std::vector<ComponentLayout> result;
    for (const auto& layout : m_presetLayouts) {
        if (layout.level == level) {
            result.push_back(layout);
        }
    }
    return result;
}

nlohmann::json BuildingTemplate::Serialize() const {
    nlohmann::json json;
    json["id"] = m_id;
    json["name"] = m_name;
    json["race"] = m_race;
    // Additional serialization...
    return json;
}

BuildingTemplatePtr BuildingTemplate::Deserialize(const nlohmann::json& json) {
    auto template_ = std::make_shared<BuildingTemplate>(
        json["id"].get<std::string>(),
        json["name"].get<std::string>()
    );
    if (json.contains("race")) template_->m_race = json["race"];
    // Additional deserialization...
    return template_;
}

// =============================================================================
// BuildingInstance
// =============================================================================

size_t BuildingInstance::s_nextId = 0;

BuildingInstance::BuildingInstance(BuildingTemplatePtr template_)
    : m_template(template_), m_level(1) {
    m_id = "building_" + std::to_string(s_nextId++);
}

bool BuildingInstance::CanUpgradeTo(int targetLevel) const {
    if (targetLevel <= m_level) return false;

    auto upgradePath = m_template->GetUpgradePath(m_level, targetLevel);
    if (!upgradePath) return false;

    // Check if we have enough merged area
    if (m_totalMergedArea < upgradePath->requiredTotalArea) {
        return false;
    }

    // Check level requirements
    auto requirements = m_template->GetLevelRequirement(targetLevel);
    if (requirements) {
        float currentArea = GetFootprintArea();
        if (currentArea < requirements->minFootprintArea) {
            return false;
        }
    }

    return true;
}

bool BuildingInstance::Upgrade(int targetLevel) {
    if (!CanUpgradeTo(targetLevel)) return false;

    m_level = targetLevel;
    return true;
}

bool BuildingInstance::CanMergeWith(const BuildingInstance& other) const {
    // Must be same building type
    if (m_template->GetId() != other.m_template->GetId()) return false;

    // Must be adjacent (check distance)
    float distance = glm::length(m_worldPosition - other.m_worldPosition);
    float maxMergeDistance = 10.0f; // Configurable
    return distance <= maxMergeDistance;
}

void BuildingInstance::MergeWith(BuildingInstancePtr other) {
    m_mergedBuildingIds.push_back(other->GetId());
    m_totalMergedArea += other->GetFootprintArea();

    // Transfer components (with position offset)
    glm::vec3 offset = other->GetWorldPosition() - m_worldPosition;
    for (auto component : other->GetAllComponents()) {
        component.position += offset;
        AddComponent(component);
    }
}

std::string BuildingInstance::AddComponent(PlacedComponent component) {
    std::string componentId = m_id + "_comp_" + std::to_string(m_placedComponents.size());

    m_placedComponents.push_back(component);
    m_componentIdToIndex[componentId] = m_placedComponents.size() - 1;

    return componentId;
}

bool BuildingInstance::RemoveComponent(const std::string& componentId) {
    auto it = m_componentIdToIndex.find(componentId);
    if (it == m_componentIdToIndex.end()) return false;

    m_placedComponents.erase(m_placedComponents.begin() + it->second);
    m_componentIdToIndex.erase(it);

    // Rebuild index
    m_componentIdToIndex.clear();
    for (size_t i = 0; i < m_placedComponents.size(); ++i) {
        std::string id = m_id + "_comp_" + std::to_string(i);
        m_componentIdToIndex[id] = i;
    }

    return true;
}

void BuildingInstance::UpdateComponent(const std::string& componentId, const PlacedComponent& component) {
    auto it = m_componentIdToIndex.find(componentId);
    if (it != m_componentIdToIndex.end()) {
        m_placedComponents[it->second] = component;
    }
}

PlacedComponent* BuildingInstance::GetComponent(const std::string& componentId) {
    auto it = m_componentIdToIndex.find(componentId);
    if (it != m_componentIdToIndex.end()) {
        return &m_placedComponents[it->second];
    }
    return nullptr;
}

glm::vec2 BuildingInstance::GetFootprintSize() const {
    if (m_placedComponents.empty()) return glm::vec2(0, 0);

    glm::vec3 min = m_placedComponents[0].GetWorldBoundsMin();
    glm::vec3 max = m_placedComponents[0].GetWorldBoundsMax();

    for (const auto& comp : m_placedComponents) {
        glm::vec3 compMin = comp.GetWorldBoundsMin();
        glm::vec3 compMax = comp.GetWorldBoundsMax();
        min = glm::min(min, compMin);
        max = glm::max(max, compMax);
    }

    return glm::vec2(max.x - min.x, max.z - min.z);
}

float BuildingInstance::GetFootprintArea() const {
    glm::vec2 size = GetFootprintSize();
    return size.x * size.y + m_totalMergedArea;
}

glm::vec3 BuildingInstance::GetTotalBoundsMin() const {
    if (m_placedComponents.empty()) return glm::vec3(0);

    glm::vec3 min = m_placedComponents[0].GetWorldBoundsMin();
    for (const auto& comp : m_placedComponents) {
        min = glm::min(min, comp.GetWorldBoundsMin());
    }
    return min;
}

glm::vec3 BuildingInstance::GetTotalBoundsMax() const {
    if (m_placedComponents.empty()) return glm::vec3(0);

    glm::vec3 max = m_placedComponents[0].GetWorldBoundsMax();
    for (const auto& comp : m_placedComponents) {
        max = glm::max(max, comp.GetWorldBoundsMax());
    }
    return max;
}

bool BuildingInstance::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    // Check level requirements
    auto requirements = m_template->GetLevelRequirement(m_level);
    if (requirements) {
        if (m_placedComponents.size() < static_cast<size_t>(requirements->minComponents)) {
            errors.push_back("Not enough components for level " + std::to_string(m_level));
            valid = false;
        }

        float area = GetFootprintArea();
        if (area < requirements->minFootprintArea) {
            errors.push_back("Footprint too small for level " + std::to_string(m_level));
            valid = false;
        }
    }

    // Validate each component
    for (const auto& comp : m_placedComponents) {
        std::vector<std::string> compErrors;
        if (!ValidateComponentPlacement(comp, compErrors)) {
            valid = false;
            errors.insert(errors.end(), compErrors.begin(), compErrors.end());
        }
    }

    return valid;
}

bool BuildingInstance::ValidateComponentPlacement(const PlacedComponent& component, std::vector<std::string>& errors) const {
    if (!component.component) {
        errors.push_back("Invalid component (null)");
        return false;
    }

    bool valid = true;
    const auto& rule = component.component->GetPlacementRule();

    // Check level requirements
    if (m_level < rule.minBuildingLevel || m_level > rule.maxBuildingLevel) {
        errors.push_back("Component not available at building level " + std::to_string(m_level));
        valid = false;
    }

    // Check support if required
    if (rule.requiresSupport && !HasSupport(component)) {
        errors.push_back("Component requires support but none found");
        valid = false;
    }

    // Check intersections
    for (const auto& other : m_placedComponents) {
        if (&other == &component) continue;
        if (CheckIntersection(component, other)) {
            errors.push_back("Component intersects with another component");
            valid = false;
            break;
        }
    }

    return valid;
}

bool BuildingInstance::CheckIntersection(const PlacedComponent& newComp, const PlacedComponent& existing) const {
    glm::vec3 newMin = newComp.GetWorldBoundsMin();
    glm::vec3 newMax = newComp.GetWorldBoundsMax();
    glm::vec3 existMin = existing.GetWorldBoundsMin();
    glm::vec3 existMax = existing.GetWorldBoundsMax();

    // AABB intersection test
    bool intersects = (newMin.x <= existMax.x && newMax.x >= existMin.x) &&
                     (newMin.y <= existMax.y && newMax.y >= existMin.y) &&
                     (newMin.z <= existMax.z && newMax.z >= existMin.z);

    if (!intersects) return false;

    // Check if intersection is allowed
    const auto& rule = newComp.component->GetPlacementRule().intersectionRule;

    // Calculate intersection depth
    float depthX = std::min(newMax.x - existMin.x, existMax.x - newMin.x);
    float depthY = std::min(newMax.y - existMin.y, existMax.y - newMin.y);
    float depthZ = std::min(newMax.z - existMin.z, existMax.z - newMin.z);
    float depth = std::min({depthX, depthY, depthZ});

    if (depth > rule.maxIntersectionDepth) {
        return true; // Invalid intersection
    }

    return false;
}

bool BuildingInstance::HasSupport(const PlacedComponent& component) const {
    // Check if component is on ground
    if (component.position.y < 0.1f) return true;

    // Check if there's a component below
    for (const auto& other : m_placedComponents) {
        if (&other == &component) continue;

        // Check if other is below and overlaps in XZ
        if (other.position.y < component.position.y) {
            glm::vec3 otherMax = other.GetWorldBoundsMax();
            glm::vec3 compMin = component.GetWorldBoundsMin();

            if (otherMax.y >= compMin.y - 0.1f) { // Within tolerance
                // Check XZ overlap
                glm::vec3 otherMin = other.GetWorldBoundsMin();
                glm::vec3 compMax = component.GetWorldBoundsMax();

                bool xOverlap = (compMin.x <= otherMax.x && compMax.x >= otherMin.x);
                bool zOverlap = (compMin.z <= otherMax.z && compMax.z >= otherMin.z);

                if (xOverlap && zOverlap) {
                    return true;
                }
            }
        }
    }

    return false;
}

nlohmann::json BuildingInstance::Serialize() const {
    nlohmann::json json;
    json["id"] = m_id;
    json["templateId"] = m_template ? m_template->GetId() : "";
    json["level"] = m_level;
    json["worldPosition"] = {m_worldPosition.x, m_worldPosition.y, m_worldPosition.z};
    json["worldRotation"] = {m_worldRotation.w, m_worldRotation.x, m_worldRotation.y, m_worldRotation.z};
    json["mergedBuildingIds"] = m_mergedBuildingIds;
    json["totalMergedArea"] = m_totalMergedArea;

    nlohmann::json componentsJson = nlohmann::json::array();
    for (const auto& comp : m_placedComponents) {
        componentsJson.push_back(comp.Serialize());
    }
    json["components"] = componentsJson;

    return json;
}

BuildingInstancePtr BuildingInstance::Deserialize(const nlohmann::json& json) {
    // Template needs to be resolved from ComponentLibrary
    std::string templateId = json.value("templateId", "");
    auto template_ = ComponentLibrary::Instance().GetTemplate(templateId);

    if (!template_) return nullptr;

    auto building = std::make_shared<BuildingInstance>(template_);
    building->m_id = json.value("id", "building_0");
    building->m_level = json.value("level", 1);

    if (json.contains("worldPosition")) {
        auto& p = json["worldPosition"];
        building->m_worldPosition = glm::vec3(p[0], p[1], p[2]);
    }
    if (json.contains("worldRotation")) {
        auto& r = json["worldRotation"];
        building->m_worldRotation = glm::quat(r[0], r[1], r[2], r[3]);
    }

    if (json.contains("mergedBuildingIds")) {
        building->m_mergedBuildingIds = json["mergedBuildingIds"].get<std::vector<std::string>>();
    }
    building->m_totalMergedArea = json.value("totalMergedArea", 0.0f);

    if (json.contains("components")) {
        for (const auto& compJson : json["components"]) {
            auto comp = PlacedComponent::Deserialize(compJson);
            std::string componentId = compJson.value("componentId", "");
            comp.component = ComponentLibrary::Instance().GetComponent(componentId);
            building->AddComponent(comp);
        }
    }

    return building;
}

// =============================================================================
// ComponentLibrary
// =============================================================================

ComponentLibrary& ComponentLibrary::Instance() {
    static ComponentLibrary instance;
    return instance;
}

void ComponentLibrary::RegisterComponent(ComponentPtr component) {
    m_components[component->GetId()] = component;
}

ComponentPtr ComponentLibrary::GetComponent(const std::string& id) const {
    auto it = m_components.find(id);
    return (it != m_components.end()) ? it->second : nullptr;
}

std::vector<ComponentPtr> ComponentLibrary::GetComponentsByCategory(const std::string& category) const {
    std::vector<ComponentPtr> result;
    for (const auto& [id, component] : m_components) {
        if (component->GetCategory() == category) {
            result.push_back(component);
        }
    }
    return result;
}

std::vector<ComponentPtr> ComponentLibrary::GetComponentsByTags(const std::vector<std::string>& tags) const {
    std::vector<ComponentPtr> result;
    for (const auto& [id, component] : m_components) {
        auto compTags = component->GetTags();
        bool hasAllTags = true;
        for (const auto& tag : tags) {
            if (std::find(compTags.begin(), compTags.end(), tag) == compTags.end()) {
                hasAllTags = false;
                break;
            }
        }
        if (hasAllTags) {
            result.push_back(component);
        }
    }
    return result;
}

std::vector<ComponentPtr> ComponentLibrary::GetComponentsForRace(const std::string& race) const {
    std::vector<ComponentPtr> result;
    auto it = m_componentsByRace.find(race);
    if (it != m_componentsByRace.end()) {
        for (const auto& id : it->second) {
            auto component = GetComponent(id);
            if (component) {
                result.push_back(component);
            }
        }
    }
    return result;
}

void ComponentLibrary::RegisterTemplate(BuildingTemplatePtr template_) {
    m_templates[template_->GetId()] = template_;
    m_templatesByRace[template_->GetRace()].push_back(template_->GetId());
}

BuildingTemplatePtr ComponentLibrary::GetTemplate(const std::string& id) const {
    auto it = m_templates.find(id);
    return (it != m_templates.end()) ? it->second : nullptr;
}

std::vector<BuildingTemplatePtr> ComponentLibrary::GetTemplatesForRace(const std::string& race) const {
    std::vector<BuildingTemplatePtr> result;
    auto it = m_templatesByRace.find(race);
    if (it != m_templatesByRace.end()) {
        for (const auto& id : it->second) {
            auto template_ = GetTemplate(id);
            if (template_) {
                result.push_back(template_);
            }
        }
    }
    return result;
}

void ComponentLibrary::LoadComponentsFromDirectory(const std::string& directory, const std::string& race) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".json") {
            try {
                std::ifstream file(entry.path());
                nlohmann::json json;
                file >> json;

                auto component = BuildingComponent::Deserialize(json);
                RegisterComponent(component);
                m_componentsByRace[race].push_back(component->GetId());
            } catch (const std::exception& e) {
                // Log error
            }
        }
    }
}

void ComponentLibrary::LoadTemplatesFromDirectory(const std::string& directory, const std::string& race) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".json") {
            try {
                std::ifstream file(entry.path());
                nlohmann::json json;
                file >> json;

                auto template_ = BuildingTemplate::Deserialize(json);
                template_->SetRace(race);
                RegisterTemplate(template_);
            } catch (const std::exception& e) {
                // Log error
            }
        }
    }
}

void ComponentLibrary::Clear() {
    m_components.clear();
    m_templates.clear();
    m_componentsByRace.clear();
    m_templatesByRace.clear();
}

// =============================================================================
// BuildingPlacer
// =============================================================================

BuildingPlacer::BuildingPlacer(BuildingInstancePtr building)
    : m_building(building) {
}

void BuildingPlacer::SelectComponent(ComponentPtr component) {
    m_selectedComponent = component;
    m_currentVariantIndex = 0;
    m_preview.component = component;
    m_preview.position = glm::vec3(0, 0, 0);
    m_preview.rotation = glm::quat(1, 0, 0, 0);
    m_preview.scale = glm::vec3(1, 1, 1);
    RandomizePreviewSeed();
    UpdatePreviewValidity();
}

void BuildingPlacer::CycleVariant() {
    if (!m_selectedComponent) return;

    const auto& variants = m_selectedComponent->GetVariants();
    if (variants.empty()) return;

    m_currentVariantIndex = (m_currentVariantIndex + 1) % variants.size();
    m_preview.variantId = variants[m_currentVariantIndex].id;
    UpdatePreviewValidity();
}

void BuildingPlacer::SetPreviewPosition(const glm::vec3& pos) {
    m_preview.position = pos;
    UpdatePreviewValidity();
}

void BuildingPlacer::SetPreviewRotation(float angleY) {
    m_preview.rotation = glm::angleAxis(glm::radians(angleY), glm::vec3(0, 1, 0));
    UpdatePreviewValidity();
}

void BuildingPlacer::ApplySnapToGrid(float gridSize) {
    m_preview.position.x = std::round(m_preview.position.x / gridSize) * gridSize;
    m_preview.position.z = std::round(m_preview.position.z / gridSize) * gridSize;
    UpdatePreviewValidity();
}

void BuildingPlacer::ApplySnapToNearbyComponents() {
    if (!m_selectedComponent) return;

    const auto& snapRule = m_selectedComponent->GetPlacementRule().snapRule;
    if (!snapRule.enabled) return;

    for (const auto& existing : m_building->GetAllComponents()) {
        float distance = glm::length(m_preview.position - existing.position);
        if (distance < snapRule.snapDistance) {
            // Snap position
            m_preview.position = existing.position;

            // Snap rotation if enabled
            if (snapRule.snapAngle > 0.0f) {
                // Align to existing component's rotation
                m_preview.rotation = existing.rotation;
            }

            break;
        }
    }

    UpdatePreviewValidity();
}

bool BuildingPlacer::PlaceComponent() {
    if (!m_previewValid) return false;

    m_building->AddComponent(m_preview);
    RandomizePreviewSeed();
    UpdatePreviewValidity();

    return true;
}

void BuildingPlacer::CancelPlacement() {
    m_selectedComponent = nullptr;
    m_preview = PlacedComponent();
    m_previewValid = false;
    m_previewErrors.clear();
}

void BuildingPlacer::RandomizePreviewSeed() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    m_previewSeed = dis(gen);
    m_preview.randomSeed = m_previewSeed;
}

void BuildingPlacer::UpdatePreviewValidity() {
    m_previewErrors.clear();
    m_previewValid = m_building->ValidateComponentPlacement(m_preview, m_previewErrors);
}

} // namespace Buildings
} // namespace Nova
