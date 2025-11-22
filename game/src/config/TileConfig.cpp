#include "TileConfig.hpp"
#include <nlohmann/json.hpp>
#include <random>

namespace Vehement {
namespace Config {

using json = nlohmann::json;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

glm::vec2 ParseVec2(const json& j) {
    if (j.is_array() && j.size() >= 2) {
        return glm::vec2(j[0].get<float>(), j[1].get<float>());
    }
    return glm::vec2(0.0f);
}

glm::vec3 ParseVec3(const json& j) {
    if (j.is_array() && j.size() >= 3) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
    return glm::vec3(1.0f);
}

glm::vec4 ParseVec4(const json& j) {
    if (j.is_array() && j.size() >= 4) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(),
                        j[2].get<float>(), j[3].get<float>());
    }
    if (j.is_array() && j.size() >= 3) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(),
                        j[2].get<float>(), 1.0f);
    }
    return glm::vec4(1.0f);
}

TileVariant ParseTileVariant(const json& j) {
    TileVariant variant;

    if (j.contains("id")) variant.id = j["id"].get<std::string>();
    if (j.contains("model")) variant.modelPath = j["model"].get<std::string>();
    if (j.contains("texture")) variant.texturePath = j["texture"].get<std::string>();
    if (j.contains("weight")) variant.weight = j["weight"].get<float>();
    if (j.contains("tint")) variant.tintColor = ParseVec4(j["tint"]);
    if (j.contains("rotationVariance")) variant.rotationVariance = j["rotationVariance"].get<float>();

    return variant;
}

TileTransitionRule ParseTransitionRule(const json& j) {
    TileTransitionRule rule;

    if (j.contains("adjacentType")) rule.adjacentTileType = j["adjacentType"].get<std::string>();
    if (j.contains("model")) rule.transitionModel = j["model"].get<std::string>();
    if (j.contains("texture")) rule.transitionTexture = j["texture"].get<std::string>();
    if (j.contains("priority")) rule.priority = j["priority"].get<int>();

    if (j.contains("directions")) {
        const auto& dir = j["directions"];
        if (dir.contains("north")) rule.applyNorth = dir["north"].get<bool>();
        if (dir.contains("south")) rule.applySouth = dir["south"].get<bool>();
        if (dir.contains("east")) rule.applyEast = dir["east"].get<bool>();
        if (dir.contains("west")) rule.applyWest = dir["west"].get<bool>();
        if (dir.contains("northEast")) rule.applyNorthEast = dir["northEast"].get<bool>();
        if (dir.contains("northWest")) rule.applyNorthWest = dir["northWest"].get<bool>();
        if (dir.contains("southEast")) rule.applySouthEast = dir["southEast"].get<bool>();
        if (dir.contains("southWest")) rule.applySouthWest = dir["southWest"].get<bool>();
    }

    return rule;
}

TileResourceYield ParseResourceYield(const json& j) {
    TileResourceYield yield;

    if (j.contains("type")) {
        yield.resourceType = StringToResourceType(j["type"].get<std::string>());
    }
    if (j.contains("resourceType")) {
        yield.resourceType = StringToResourceType(j["resourceType"].get<std::string>());
    }
    if (j.contains("baseAmount")) yield.baseAmount = j["baseAmount"].get<int>();
    if (j.contains("amount")) yield.baseAmount = j["amount"].get<int>();
    if (j.contains("regenRate")) yield.regenRate = j["regenRate"].get<float>();
    if (j.contains("maxAmount")) yield.maxAmount = j["maxAmount"].get<int>();
    if (j.contains("depletes")) yield.depletes = j["depletes"].get<bool>();
    if (j.contains("depletedModel")) yield.depletedModelPath = j["depletedModel"].get<std::string>();

    return yield;
}

TileAnimationConfig ParseTileAnimation(const json& j) {
    TileAnimationConfig anim;

    if (j.contains("type")) {
        std::string typeStr = j["type"].get<std::string>();
        if (typeStr == "uv_scroll" || typeStr == "scroll") {
            anim.type = TileAnimationConfig::AnimationType::UVScroll;
        } else if (typeStr == "sprite_sheet" || typeStr == "frames") {
            anim.type = TileAnimationConfig::AnimationType::SpriteSheet;
        } else if (typeStr == "vertex_wave" || typeStr == "wave") {
            anim.type = TileAnimationConfig::AnimationType::VertexWave;
        } else if (typeStr == "color_cycle" || typeStr == "color") {
            anim.type = TileAnimationConfig::AnimationType::ColorCycle;
        }
    }

    if (j.contains("speed")) anim.speed = j["speed"].get<float>();
    if (j.contains("scrollDirection")) anim.scrollDirection = ParseVec2(j["scrollDirection"]);
    if (j.contains("frameCount")) anim.frameCount = j["frameCount"].get<int>();
    if (j.contains("frameDuration")) anim.frameDuration = j["frameDuration"].get<float>();
    if (j.contains("waveAmplitude")) anim.waveAmplitude = j["waveAmplitude"].get<float>();
    if (j.contains("waveFrequency")) anim.waveFrequency = j["waveFrequency"].get<float>();

    return anim;
}

} // anonymous namespace

// ============================================================================
// TileConfig Implementation
// ============================================================================

void TileConfig::ParseTypeSpecificFields(const std::string& jsonContent) {
    try {
        json j = json::parse(StripComments(jsonContent));

        // Parse identity
        if (j.contains("tileTypeId")) m_tileTypeId = j["tileTypeId"].get<int>();
        if (j.contains("typeId")) m_tileTypeId = j["typeId"].get<int>();
        if (j.contains("displayName")) m_displayName = j["displayName"].get<std::string>();
        if (j.contains("category")) m_category = j["category"].get<std::string>();

        // Parse rendering
        if (j.contains("procedural")) {
            if (j["procedural"].is_bool()) {
                m_isProcedural = j["procedural"].get<bool>();
            } else if (j["procedural"].is_string()) {
                m_isProcedural = true;
                m_proceduralType = j["procedural"].get<std::string>();
            }
        }
        if (j.contains("proceduralType")) m_proceduralType = j["proceduralType"].get<std::string>();
        if (j.contains("height")) m_tileHeight = j["height"].get<float>();
        if (j.contains("tileHeight")) m_tileHeight = j["tileHeight"].get<float>();

        if (j.contains("wall")) {
            if (j["wall"].is_bool()) {
                m_isWall = j["wall"].get<bool>();
            } else if (j["wall"].is_object()) {
                m_isWall = true;
                if (j["wall"].contains("height")) {
                    m_wallHeight = j["wall"]["height"].get<float>();
                }
            }
        }
        if (j.contains("wallHeight")) m_wallHeight = j["wallHeight"].get<float>();

        // Parse movement
        if (j.contains("walkable")) m_isWalkable = j["walkable"].get<bool>();
        if (j.contains("buildable")) m_isBuildable = j["buildable"].get<bool>();
        if (j.contains("blocksSight")) m_blocksSight = j["blocksSight"].get<bool>();
        if (j.contains("blocksProjectiles")) m_blocksProjectiles = j["blocksProjectiles"].get<bool>();
        if (j.contains("movementCost")) m_movementCost = j["movementCost"].get<float>();

        if (j.contains("movementCosts") && j["movementCosts"].is_object()) {
            for (auto& [unitClass, cost] : j["movementCosts"].items()) {
                m_unitClassMovementCosts[unitClass] = cost.get<float>();
            }
        }

        // Parse environment
        if (j.contains("damage")) {
            if (j["damage"].is_number()) {
                m_damagePerSecond = j["damage"].get<float>();
            } else if (j["damage"].is_object()) {
                if (j["damage"].contains("perSecond")) {
                    m_damagePerSecond = j["damage"]["perSecond"].get<float>();
                }
                if (j["damage"].contains("type")) {
                    m_damageType = j["damage"]["type"].get<std::string>();
                }
            }
        }
        if (j.contains("damagePerSecond")) m_damagePerSecond = j["damagePerSecond"].get<float>();
        if (j.contains("damageType")) m_damageType = j["damageType"].get<std::string>();
        if (j.contains("speedModifier")) m_speedModifier = j["speedModifier"].get<float>();

        if (j.contains("concealment")) {
            if (j["concealment"].is_bool()) {
                m_providesConcealment = j["concealment"].get<bool>();
            } else if (j["concealment"].is_number()) {
                m_providesConcealment = true;
                m_concealmentBonus = j["concealment"].get<float>();
            }
        }

        // Parse resources
        if (j.contains("resource") && j["resource"].is_object()) {
            m_resourceYield = ParseResourceYield(j["resource"]);
        }
        if (j.contains("resourceYield") && j["resourceYield"].is_object()) {
            m_resourceYield = ParseResourceYield(j["resourceYield"]);
        }

        // Parse variants
        if (j.contains("variants") && j["variants"].is_array()) {
            m_variants.clear();
            for (const auto& variantJson : j["variants"]) {
                m_variants.push_back(ParseTileVariant(variantJson));
            }
        }

        // Parse transitions
        if (j.contains("transitions") && j["transitions"].is_array()) {
            m_transitionRules.clear();
            for (const auto& transJson : j["transitions"]) {
                m_transitionRules.push_back(ParseTransitionRule(transJson));
            }
        }

        // Parse animation
        if (j.contains("animation") && j["animation"].is_object()) {
            m_animation = ParseTileAnimation(j["animation"]);
        }

        // Parse lighting
        if (j.contains("light")) {
            const auto& light = j["light"];
            if (light.contains("emission")) m_lightEmission = light["emission"].get<float>();
            if (light.contains("color")) m_lightColor = ParseVec3(light["color"]);
        }
        if (j.contains("lightEmission")) m_lightEmission = j["lightEmission"].get<float>();

        // Parse audio
        if (j.contains("sounds")) {
            const auto& sounds = j["sounds"];
            if (sounds.contains("footstep")) m_footstepSound = sounds["footstep"].get<std::string>();
            if (sounds.contains("ambient")) m_ambientSound = sounds["ambient"].get<std::string>();
            if (sounds.contains("ambientVolume")) m_ambientVolume = sounds["ambientVolume"].get<float>();
        }
        if (j.contains("footstepSound")) m_footstepSound = j["footstepSound"].get<std::string>();

        // Parse script hooks
        if (j.contains("scripts") && j["scripts"].is_object()) {
            for (auto& [hook, path] : j["scripts"].items()) {
                m_scriptHooks[hook] = path.get<std::string>();
            }
        }

    } catch (const json::exception&) {
        // Error parsing tile-specific fields
    }
}

std::string TileConfig::SerializeTypeSpecificFields() const {
    json j;

    // Identity
    j["tileTypeId"] = m_tileTypeId;
    if (!m_displayName.empty()) j["displayName"] = m_displayName;
    if (!m_category.empty()) j["category"] = m_category;

    // Rendering
    if (m_isProcedural) {
        j["procedural"] = m_proceduralType.empty() ? json(true) : json(m_proceduralType);
    }
    if (m_tileHeight != 0.0f) j["height"] = m_tileHeight;
    if (m_isWall) {
        json wall;
        wall["height"] = m_wallHeight;
        j["wall"] = wall;
    }

    // Movement
    j["walkable"] = m_isWalkable;
    j["buildable"] = m_isBuildable;
    if (m_blocksSight) j["blocksSight"] = true;
    if (m_movementCost != 1.0f) j["movementCost"] = m_movementCost;

    // Environment
    if (m_damagePerSecond > 0) {
        json damage;
        damage["perSecond"] = m_damagePerSecond;
        if (!m_damageType.empty()) damage["type"] = m_damageType;
        j["damage"] = damage;
    }

    // Resources
    if (m_resourceYield.resourceType != ResourceType::None) {
        json resource;
        resource["type"] = ResourceTypeToString(m_resourceYield.resourceType);
        resource["amount"] = m_resourceYield.baseAmount;
        j["resource"] = resource;
    }

    // Variants
    if (!m_variants.empty()) {
        json variants = json::array();
        for (const auto& variant : m_variants) {
            json v;
            v["id"] = variant.id;
            if (!variant.modelPath.empty()) v["model"] = variant.modelPath;
            if (!variant.texturePath.empty()) v["texture"] = variant.texturePath;
            v["weight"] = variant.weight;
            variants.push_back(v);
        }
        j["variants"] = variants;
    }

    // Scripts
    if (!m_scriptHooks.empty()) {
        j["scripts"] = m_scriptHooks;
    }

    return j.dump(2);
}

ValidationResult TileConfig::Validate() const {
    ValidationResult result = EntityConfig::Validate();

    // Validate movement cost
    if (m_movementCost < 0) {
        result.AddError("movementCost", "Movement cost cannot be negative");
    }

    if (m_isWalkable && m_movementCost >= 999) {
        result.AddWarning("movementCost", "Walkable tile has very high movement cost");
    }

    // Validate wall
    if (m_isWall && m_wallHeight <= 0) {
        result.AddWarning("wall.height", "Wall has non-positive height");
    }

    // Validate variants
    float totalWeight = 0.0f;
    for (const auto& variant : m_variants) {
        if (variant.weight < 0) {
            result.AddError("variants", "Variant weight cannot be negative");
        }
        totalWeight += variant.weight;
    }
    if (!m_variants.empty() && totalWeight <= 0) {
        result.AddWarning("variants", "Total variant weight is zero or negative");
    }

    // Validate resources
    if (m_resourceYield.resourceType != ResourceType::None) {
        if (m_resourceYield.baseAmount <= 0) {
            result.AddWarning("resource.amount", "Harvestable tile has no yield");
        }
    }

    return result;
}

void TileConfig::ApplyBaseConfig(const EntityConfig& baseConfig) {
    EntityConfig::ApplyBaseConfig(baseConfig);

    if (const auto* baseTile = dynamic_cast<const TileConfig*>(&baseConfig)) {
        // Inherit rendering
        if (!m_isProcedural && m_proceduralType.empty()) {
            m_isProcedural = baseTile->m_isProcedural;
            m_proceduralType = baseTile->m_proceduralType;
        }

        // Inherit movement defaults
        if (m_movementCost == 1.0f) {
            m_movementCost = baseTile->m_movementCost;
        }

        // Merge unit class movement costs
        for (const auto& [unitClass, cost] : baseTile->m_unitClassMovementCosts) {
            if (m_unitClassMovementCosts.find(unitClass) == m_unitClassMovementCosts.end()) {
                m_unitClassMovementCosts[unitClass] = cost;
            }
        }

        // Inherit resource yield if not set
        if (m_resourceYield.resourceType == ResourceType::None) {
            m_resourceYield = baseTile->m_resourceYield;
        }

        // Merge variants
        for (const auto& variant : baseTile->m_variants) {
            if (!GetVariant(variant.id)) {
                m_variants.push_back(variant);
            }
        }

        // Merge transitions
        for (const auto& rule : baseTile->m_transitionRules) {
            if (!GetTransitionTo(rule.adjacentTileType)) {
                m_transitionRules.push_back(rule);
            }
        }

        // Inherit animation if not set
        if (m_animation.type == TileAnimationConfig::AnimationType::None) {
            m_animation = baseTile->m_animation;
        }

        // Inherit audio
        if (m_footstepSound.empty()) m_footstepSound = baseTile->m_footstepSound;
        if (m_ambientSound.empty()) m_ambientSound = baseTile->m_ambientSound;

        // Merge script hooks
        for (const auto& [hook, path] : baseTile->m_scriptHooks) {
            if (m_scriptHooks.find(hook) == m_scriptHooks.end()) {
                m_scriptHooks[hook] = path;
            }
        }

        // Inherit category
        if (m_category.empty()) m_category = baseTile->m_category;
    }
}

float TileConfig::GetMovementCostFor(const std::string& unitClass) const {
    auto it = m_unitClassMovementCosts.find(unitClass);
    return (it != m_unitClassMovementCosts.end()) ? it->second : m_movementCost;
}

void TileConfig::SetMovementCostFor(const std::string& unitClass, float cost) {
    m_unitClassMovementCosts[unitClass] = cost;
}

const TileVariant* TileConfig::GetVariant(const std::string& id) const {
    for (const auto& variant : m_variants) {
        if (variant.id == id) {
            return &variant;
        }
    }
    return nullptr;
}

const TileVariant* TileConfig::GetRandomVariant() const {
    if (m_variants.empty()) {
        return nullptr;
    }

    // Calculate total weight
    float totalWeight = 0.0f;
    for (const auto& variant : m_variants) {
        totalWeight += variant.weight;
    }

    if (totalWeight <= 0) {
        return &m_variants[0];
    }

    // Random selection
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float roll = dist(rng);

    float cumulative = 0.0f;
    for (const auto& variant : m_variants) {
        cumulative += variant.weight;
        if (roll <= cumulative) {
            return &variant;
        }
    }

    return &m_variants.back();
}

const TileTransitionRule* TileConfig::GetTransitionTo(const std::string& adjacentType) const {
    const TileTransitionRule* best = nullptr;
    for (const auto& rule : m_transitionRules) {
        if (rule.adjacentTileType == adjacentType) {
            if (!best || rule.priority > best->priority) {
                best = &rule;
            }
        }
    }
    return best;
}

std::string TileConfig::GetScriptHook(const std::string& hookName) const {
    auto it = m_scriptHooks.find(hookName);
    return (it != m_scriptHooks.end()) ? it->second : "";
}

void TileConfig::SetScriptHook(const std::string& hookName, const std::string& path) {
    if (path.empty()) {
        m_scriptHooks.erase(hookName);
    } else {
        m_scriptHooks[hookName] = path;
    }
}

} // namespace Config
} // namespace Vehement
