#include "UnitConfig.hpp"
#include <nlohmann/json.hpp>

namespace Vehement {
namespace Config {

using json = nlohmann::json;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

glm::vec3 ParseVec3(const json& j) {
    if (j.is_array() && j.size() >= 3) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
    return glm::vec3(0.0f);
}

MovementConfig ParseMovementConfig(const json& j) {
    MovementConfig config;

    if (j.contains("speed")) config.moveSpeed = j["speed"].get<float>();
    if (j.contains("moveSpeed")) config.moveSpeed = j["moveSpeed"].get<float>();
    if (j.contains("turnRate")) config.turnRate = j["turnRate"].get<float>();
    if (j.contains("acceleration")) config.acceleration = j["acceleration"].get<float>();
    if (j.contains("deceleration")) config.deceleration = j["deceleration"].get<float>();

    if (j.contains("canFly")) config.canFly = j["canFly"].get<bool>();
    if (j.contains("canSwim")) config.canSwim = j["canSwim"].get<bool>();
    if (j.contains("canClimb")) config.canClimb = j["canClimb"].get<bool>();
    if (j.contains("canBurrow")) config.canBurrow = j["canBurrow"].get<bool>();

    if (j.contains("flyHeight")) config.flyHeight = j["flyHeight"].get<float>();
    if (j.contains("jumpHeight")) config.jumpHeight = j["jumpHeight"].get<float>();

    if (j.contains("terrainModifiers") && j["terrainModifiers"].is_object()) {
        for (auto& [terrain, modifier] : j["terrainModifiers"].items()) {
            config.terrainSpeedModifiers[terrain] = modifier.get<float>();
        }
    }

    return config;
}

CombatStats ParseCombatStats(const json& j) {
    CombatStats stats;

    if (j.contains("health")) stats.health = j["health"].get<float>();
    if (j.contains("maxHealth")) stats.maxHealth = j["maxHealth"].get<float>();
    else if (j.contains("health")) stats.maxHealth = stats.health;

    if (j.contains("armor")) stats.armor = j["armor"].get<float>();
    if (j.contains("magicResist")) stats.magicResist = j["magicResist"].get<float>();

    if (j.contains("attackDamage")) stats.attackDamage = j["attackDamage"].get<float>();
    if (j.contains("damage")) stats.attackDamage = j["damage"].get<float>();
    if (j.contains("attackSpeed")) stats.attackSpeed = j["attackSpeed"].get<float>();
    if (j.contains("attackRange")) stats.attackRange = j["attackRange"].get<float>();
    if (j.contains("range")) stats.attackRange = j["range"].get<float>();

    if (j.contains("critChance")) stats.critChance = j["critChance"].get<float>();
    if (j.contains("critMultiplier")) stats.critMultiplier = j["critMultiplier"].get<float>();

    if (j.contains("physicalDamage")) stats.physicalDamage = j["physicalDamage"].get<bool>();
    if (j.contains("magicalDamage")) stats.magicalDamage = j["magicalDamage"].get<bool>();
    if (j.contains("trueDamage")) stats.trueDamage = j["trueDamage"].get<bool>();

    return stats;
}

ProjectileConfig ParseProjectileConfig(const json& j) {
    ProjectileConfig config;

    if (j.contains("id")) config.projectileId = j["id"].get<std::string>();
    if (j.contains("projectileId")) config.projectileId = j["projectileId"].get<std::string>();
    if (j.contains("model")) config.modelPath = j["model"].get<std::string>();
    if (j.contains("speed")) config.speed = j["speed"].get<float>();
    if (j.contains("lifetime")) config.lifetime = j["lifetime"].get<float>();
    if (j.contains("gravity")) config.gravity = j["gravity"].get<float>();

    if (j.contains("homing")) config.homing = j["homing"].get<bool>();
    if (j.contains("homingStrength")) config.homingStrength = j["homingStrength"].get<float>();

    if (j.contains("trailEffect")) config.trailEffect = j["trailEffect"].get<std::string>();
    if (j.contains("impactEffect")) config.impactEffect = j["impactEffect"].get<std::string>();
    if (j.contains("soundOnFire")) config.soundOnFire = j["soundOnFire"].get<std::string>();
    if (j.contains("soundOnImpact")) config.soundOnImpact = j["soundOnImpact"].get<std::string>();

    return config;
}

AbilityConfig ParseAbilityConfig(const json& j) {
    AbilityConfig ability;

    if (j.contains("id")) ability.id = j["id"].get<std::string>();
    if (j.contains("name")) ability.name = j["name"].get<std::string>();
    if (j.contains("description")) ability.description = j["description"].get<std::string>();
    if (j.contains("icon")) ability.iconPath = j["icon"].get<std::string>();

    if (j.contains("cooldown")) ability.cooldown = j["cooldown"].get<float>();
    if (j.contains("manaCost")) ability.manaCost = j["manaCost"].get<float>();
    if (j.contains("castTime")) ability.castTime = j["castTime"].get<float>();
    if (j.contains("range")) ability.range = j["range"].get<float>();
    if (j.contains("radius")) ability.radius = j["radius"].get<float>();

    if (j.contains("targetType")) {
        std::string targetStr = j["targetType"].get<std::string>();
        if (targetStr == "none") ability.targetType = AbilityConfig::TargetType::None;
        else if (targetStr == "self") ability.targetType = AbilityConfig::TargetType::Self;
        else if (targetStr == "ground") ability.targetType = AbilityConfig::TargetType::GroundPoint;
        else if (targetStr == "unit") ability.targetType = AbilityConfig::TargetType::Unit;
        else if (targetStr == "unit_or_ground") ability.targetType = AbilityConfig::TargetType::UnitOrGround;
        else if (targetStr == "direction") ability.targetType = AbilityConfig::TargetType::Direction;
    }

    if (j.contains("targetsFriendly")) ability.targetsFriendly = j["targetsFriendly"].get<bool>();
    if (j.contains("targetsEnemy")) ability.targetsEnemy = j["targetsEnemy"].get<bool>();
    if (j.contains("targetsSelf")) ability.targetsSelf = j["targetsSelf"].get<bool>();

    if (j.contains("script")) ability.scriptPath = j["script"].get<std::string>();
    if (j.contains("function")) ability.scriptFunction = j["function"].get<std::string>();

    if (j.contains("castAnimation")) ability.castAnimation = j["castAnimation"].get<std::string>();
    if (j.contains("castEffect")) ability.castEffect = j["castEffect"].get<std::string>();
    if (j.contains("castSound")) ability.castSound = j["castSound"].get<std::string>();

    return ability;
}

AnimationMapping ParseAnimationMapping(const json& j, const std::string& stateName) {
    AnimationMapping anim;
    anim.stateName = stateName;

    if (j.is_string()) {
        anim.animationPath = j.get<std::string>();
    } else if (j.is_object()) {
        if (j.contains("path")) anim.animationPath = j["path"].get<std::string>();
        if (j.contains("speed")) anim.speed = j["speed"].get<float>();
        if (j.contains("loop")) anim.loop = j["loop"].get<bool>();
        if (j.contains("blendIn")) anim.blendInTime = j["blendIn"].get<float>();
        if (j.contains("blendOut")) anim.blendOutTime = j["blendOut"].get<float>();

        if (j.contains("events") && j["events"].is_array()) {
            for (const auto& event : j["events"]) {
                if (event.is_array() && event.size() >= 2) {
                    anim.animationEvents.emplace_back(
                        event[0].get<float>(),
                        event[1].get<std::string>()
                    );
                }
            }
        }
    }

    return anim;
}

SoundMapping ParseSoundMapping(const json& j, const std::string& eventName) {
    SoundMapping sound;
    sound.eventName = eventName;

    if (j.is_string()) {
        sound.soundPaths.push_back(j.get<std::string>());
    } else if (j.is_array()) {
        for (const auto& path : j) {
            sound.soundPaths.push_back(path.get<std::string>());
        }
    } else if (j.is_object()) {
        if (j.contains("path")) {
            sound.soundPaths.push_back(j["path"].get<std::string>());
        }
        if (j.contains("paths") && j["paths"].is_array()) {
            for (const auto& path : j["paths"]) {
                sound.soundPaths.push_back(path.get<std::string>());
            }
        }
        if (j.contains("volume")) sound.volume = j["volume"].get<float>();
        if (j.contains("pitchVariation")) sound.pitchVariation = j["pitchVariation"].get<float>();
        if (j.contains("minDistance")) sound.minDistance = j["minDistance"].get<float>();
        if (j.contains("maxDistance")) sound.maxDistance = j["maxDistance"].get<float>();
        if (j.contains("is3D")) sound.is3D = j["is3D"].get<bool>();
    }

    return sound;
}

} // anonymous namespace

// ============================================================================
// UnitConfig Implementation
// ============================================================================

void UnitConfig::ParseTypeSpecificFields(const std::string& jsonContent) {
    try {
        json j = json::parse(StripComments(jsonContent));

        // Parse movement
        if (j.contains("movement")) {
            m_movement = ParseMovementConfig(j["movement"]);
        }

        // Parse combat stats
        if (j.contains("combat")) {
            m_combat = ParseCombatStats(j["combat"]);
        } else if (j.contains("stats")) {
            m_combat = ParseCombatStats(j["stats"]);
        }

        // Parse projectile
        if (j.contains("projectile")) {
            m_projectile = ParseProjectileConfig(j["projectile"]);
        }

        // Parse AI
        if (j.contains("ai")) {
            const auto& ai = j["ai"];
            if (ai.contains("behaviorTree")) m_behaviorTreePath = ai["behaviorTree"].get<std::string>();
            if (ai.contains("profile")) m_aiProfile = ai["profile"].get<std::string>();
            if (ai.contains("aggroRange")) m_aggroRange = ai["aggroRange"].get<float>();
            if (ai.contains("leashRange")) m_leashRange = ai["leashRange"].get<float>();
        }

        // Parse abilities
        if (j.contains("abilities") && j["abilities"].is_array()) {
            m_abilities.clear();
            for (const auto& abilityJson : j["abilities"]) {
                m_abilities.push_back(ParseAbilityConfig(abilityJson));
            }
        }

        // Parse animations
        if (j.contains("animations") && j["animations"].is_object()) {
            m_animations.clear();
            for (auto& [state, animJson] : j["animations"].items()) {
                m_animations.push_back(ParseAnimationMapping(animJson, state));
            }
        }

        // Parse sounds
        if (j.contains("sounds") && j["sounds"].is_object()) {
            m_sounds.clear();
            for (auto& [event, soundJson] : j["sounds"].items()) {
                m_sounds.push_back(ParseSoundMapping(soundJson, event));
            }
        }

        // Parse script hooks
        if (j.contains("scripts") && j["scripts"].is_object()) {
            for (auto& [hook, path] : j["scripts"].items()) {
                m_scriptHooks[hook] = path.get<std::string>();
            }
        }

        // Parse classification
        if (j.contains("class")) m_unitClass = j["class"].get<std::string>();
        if (j.contains("unitClass")) m_unitClass = j["unitClass"].get<std::string>();
        if (j.contains("faction")) m_faction = j["faction"].get<std::string>();
        if (j.contains("tier")) m_tier = j["tier"].get<int>();
        if (j.contains("isHero")) m_isHero = j["isHero"].get<bool>();

    } catch (const json::exception&) {
        // Error parsing unit-specific fields
    }
}

std::string UnitConfig::SerializeTypeSpecificFields() const {
    json j;

    // Movement
    json movement;
    movement["speed"] = m_movement.moveSpeed;
    movement["turnRate"] = m_movement.turnRate;
    movement["acceleration"] = m_movement.acceleration;
    if (m_movement.canFly) movement["canFly"] = true;
    if (m_movement.canSwim) movement["canSwim"] = true;
    j["movement"] = movement;

    // Combat
    json combat;
    combat["health"] = m_combat.health;
    combat["maxHealth"] = m_combat.maxHealth;
    combat["armor"] = m_combat.armor;
    combat["attackDamage"] = m_combat.attackDamage;
    combat["attackSpeed"] = m_combat.attackSpeed;
    combat["attackRange"] = m_combat.attackRange;
    j["combat"] = combat;

    // AI
    if (!m_behaviorTreePath.empty()) {
        json ai;
        ai["behaviorTree"] = m_behaviorTreePath;
        ai["aggroRange"] = m_aggroRange;
        ai["leashRange"] = m_leashRange;
        j["ai"] = ai;
    }

    // Abilities
    if (!m_abilities.empty()) {
        json abilities = json::array();
        for (const auto& ability : m_abilities) {
            json ab;
            ab["id"] = ability.id;
            ab["name"] = ability.name;
            ab["cooldown"] = ability.cooldown;
            abilities.push_back(ab);
        }
        j["abilities"] = abilities;
    }

    // Scripts
    if (!m_scriptHooks.empty()) {
        j["scripts"] = m_scriptHooks;
    }

    return j.dump(2);
}

ValidationResult UnitConfig::Validate() const {
    ValidationResult result = EntityConfig::Validate();

    // Validate combat stats
    if (m_combat.maxHealth <= 0) {
        result.AddError("combat.maxHealth", "Max health must be positive");
    }

    if (m_combat.health > m_combat.maxHealth) {
        result.AddWarning("combat.health", "Health exceeds max health");
    }

    if (m_combat.attackSpeed <= 0) {
        result.AddError("combat.attackSpeed", "Attack speed must be positive");
    }

    // Validate movement
    if (m_movement.moveSpeed < 0) {
        result.AddError("movement.speed", "Move speed cannot be negative");
    }

    // Validate abilities
    for (const auto& ability : m_abilities) {
        if (ability.id.empty()) {
            result.AddError("abilities", "Ability ID is required");
        }
        if (ability.cooldown < 0) {
            result.AddError("abilities." + ability.id, "Cooldown cannot be negative");
        }
    }

    return result;
}

void UnitConfig::ApplyBaseConfig(const EntityConfig& baseConfig) {
    EntityConfig::ApplyBaseConfig(baseConfig);

    // If base is also a UnitConfig, inherit unit-specific values
    if (const auto* baseUnit = dynamic_cast<const UnitConfig*>(&baseConfig)) {
        // Inherit movement if using defaults
        if (m_movement.moveSpeed == 5.0f) {
            m_movement = baseUnit->m_movement;
        }

        // Inherit combat stats if using defaults
        if (m_combat.maxHealth == 100.0f && m_combat.attackDamage == 10.0f) {
            m_combat = baseUnit->m_combat;
        }

        // Inherit AI settings
        if (m_behaviorTreePath.empty()) {
            m_behaviorTreePath = baseUnit->m_behaviorTreePath;
        }
        if (m_aiProfile.empty()) {
            m_aiProfile = baseUnit->m_aiProfile;
        }

        // Merge abilities (don't override existing)
        for (const auto& ability : baseUnit->m_abilities) {
            if (!GetAbility(ability.id)) {
                m_abilities.push_back(ability);
            }
        }

        // Merge animations
        for (const auto& anim : baseUnit->m_animations) {
            if (!GetAnimation(anim.stateName)) {
                m_animations.push_back(anim);
            }
        }

        // Merge sounds
        for (const auto& sound : baseUnit->m_sounds) {
            if (!GetSound(sound.eventName)) {
                m_sounds.push_back(sound);
            }
        }

        // Merge script hooks
        for (const auto& [hook, path] : baseUnit->m_scriptHooks) {
            if (m_scriptHooks.find(hook) == m_scriptHooks.end()) {
                m_scriptHooks[hook] = path;
            }
        }

        // Inherit classification
        if (m_unitClass.empty()) m_unitClass = baseUnit->m_unitClass;
        if (m_faction.empty()) m_faction = baseUnit->m_faction;
    }
}

const AbilityConfig* UnitConfig::GetAbility(const std::string& id) const {
    for (const auto& ability : m_abilities) {
        if (ability.id == id) {
            return &ability;
        }
    }
    return nullptr;
}

const AnimationMapping* UnitConfig::GetAnimation(const std::string& state) const {
    for (const auto& anim : m_animations) {
        if (anim.stateName == state) {
            return &anim;
        }
    }
    return nullptr;
}

const SoundMapping* UnitConfig::GetSound(const std::string& event) const {
    for (const auto& sound : m_sounds) {
        if (sound.eventName == event) {
            return &sound;
        }
    }
    return nullptr;
}

std::string UnitConfig::GetScriptHook(const std::string& hookName) const {
    auto it = m_scriptHooks.find(hookName);
    return (it != m_scriptHooks.end()) ? it->second : "";
}

void UnitConfig::SetScriptHook(const std::string& hookName, const std::string& path) {
    if (path.empty()) {
        m_scriptHooks.erase(hookName);
    } else {
        m_scriptHooks[hookName] = path;
    }
}

} // namespace Config
} // namespace Vehement
