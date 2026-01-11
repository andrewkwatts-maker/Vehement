#include "SDFConfigLoader.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Engine {

// =============================================================================
// Generic Asset Loading
// =============================================================================

AssetConfigVariant SDFConfigLoader::LoadAssetFromFile(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        m_lastError = "File not found: " + filepath.string();
        throw std::runtime_error(m_lastError);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        m_lastError = "Failed to open file: " + filepath.string();
        throw std::runtime_error(m_lastError);
    }

    try {
        nlohmann::json json;
        file >> json;
        return LoadAssetFromString(json.dump());
    } catch (const nlohmann::json::parse_error& e) {
        m_lastError = "JSON parse error in " + filepath.string() + ": " + e.what();
        throw std::runtime_error(m_lastError);
    }
}

AssetConfigVariant SDFConfigLoader::LoadAssetFromString(const std::string& jsonString) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonString);

        // Determine asset type from the "type" field
        std::string typeStr = json.value("type", "asset");
        AssetType type = StringToAssetType(typeStr);

        // Dispatch to appropriate parser based on type
        switch (type) {
            case AssetType::Texture:
                return ParseTexture(json);

            case AssetType::Material:
                return ParseMaterial(json);

            case AssetType::SDFModel:
                return ParseSDFModel(json);

            case AssetType::Skeleton:
                return ParseSkeleton(json);

            case AssetType::Animation:
                return ParseAnimation(json.value("name", ""), json);

            case AssetType::AnimationSet:
                return ParseAnimationSet(json);

            case AssetType::Effect:
                return ParseEffect(json.value("name", ""), json);

            case AssetType::Ability:
                return ParseAbility(json);

            case AssetType::Behavior:
                return ParseBehaviors(json);

            case AssetType::Unit:
                return ParseUnit(json);

            case AssetType::Hero:
                return ParseHero(json);

            case AssetType::Building:
                return ParseBuilding(json);

            case AssetType::ResourceNode:
                return ParseResourceNode(json);

            case AssetType::Projectile:
                return ParseProjectile(json);

            case AssetType::Decoration:
                return ParseDecoration(json);

            case AssetType::Entity:
            default:
                // Default to entity parsing for backward compatibility
                return ParseEntity(json);
        }
    } catch (const nlohmann::json::parse_error& e) {
        m_lastError = std::string("JSON parse error: ") + e.what();
        throw std::runtime_error(m_lastError);
    }
}

std::unordered_map<std::string, AssetConfigVariant> SDFConfigLoader::LoadAssetsFromDirectory(
    const std::filesystem::path& directory,
    bool recursive) {

    std::unordered_map<std::string, AssetConfigVariant> assets;

    if (!std::filesystem::exists(directory)) {
        m_lastError = "Directory not found: " + directory.string();
        return assets;
    }

    auto processEntry = [&](const std::filesystem::directory_entry& entry) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".json") {
                try {
                    auto asset = LoadAssetFromFile(entry.path());

                    // Extract ID from the variant
                    std::string id;
                    std::visit([&id](const auto& config) {
                        id = config.id;
                    }, asset);

                    if (!id.empty()) {
                        assets[id] = std::move(asset);
                    }
                } catch (const std::exception& e) {
                    m_lastError = e.what();
                }
            }
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            processEntry(entry);
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            processEntry(entry);
        }
    }

    return assets;
}

// =============================================================================
// Legacy Entity Loading (backward compatibility)
// =============================================================================

EntityConfig SDFConfigLoader::LoadFromFile(const std::filesystem::path& filepath) {
    if (!std::filesystem::exists(filepath)) {
        m_lastError = "File not found: " + filepath.string();
        throw std::runtime_error(m_lastError);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        m_lastError = "Failed to open file: " + filepath.string();
        throw std::runtime_error(m_lastError);
    }

    try {
        nlohmann::json json;
        file >> json;
        return ParseEntity(json);
    } catch (const nlohmann::json::parse_error& e) {
        m_lastError = "JSON parse error in " + filepath.string() + ": " + e.what();
        throw std::runtime_error(m_lastError);
    }
}

EntityConfig SDFConfigLoader::LoadFromString(const std::string& jsonString) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonString);
        return ParseEntity(json);
    } catch (const nlohmann::json::parse_error& e) {
        m_lastError = std::string("JSON parse error: ") + e.what();
        throw std::runtime_error(m_lastError);
    }
}

std::unordered_map<std::string, EntityConfig> SDFConfigLoader::LoadFromDirectory(
    const std::filesystem::path& directory,
    bool recursive) {

    std::unordered_map<std::string, EntityConfig> configs;

    if (!std::filesystem::exists(directory)) {
        m_lastError = "Directory not found: " + directory.string();
        return configs;
    }

    auto processEntry = [&](const std::filesystem::directory_entry& entry) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".json") {
                try {
                    auto config = LoadFromFile(entry.path());
                    configs[config.id] = std::move(config);
                } catch (const std::exception& e) {
                    m_lastError = e.what();
                }
            }
        }
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            processEntry(entry);
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            processEntry(entry);
        }
    }

    return configs;
}

// =============================================================================
// Type-Specific Public Loading Methods
// =============================================================================

SDFModelConfig SDFConfigLoader::LoadSDFModel(const nlohmann::json& json) {
    return ParseSDFModel(json);
}

SkeletonConfig SDFConfigLoader::LoadSkeleton(const nlohmann::json& json) {
    return ParseSkeleton(json);
}

AnimationConfig SDFConfigLoader::LoadAnimation(const nlohmann::json& json) {
    return ParseAnimation(json.value("name", ""), json);
}

AnimationSetConfig SDFConfigLoader::LoadAnimationSet(const nlohmann::json& json) {
    return ParseAnimationSet(json);
}

AbilityConfig SDFConfigLoader::LoadAbility(const nlohmann::json& json) {
    return ParseAbility(json);
}

BehaviorConfig SDFConfigLoader::LoadBehavior(const nlohmann::json& json) {
    return ParseBehaviors(json);
}

EffectConfig SDFConfigLoader::LoadEffect(const nlohmann::json& json) {
    return ParseEffect(json.value("name", ""), json);
}

// =============================================================================
// Validation
// =============================================================================

std::vector<std::string> SDFConfigLoader::Validate(const EntityConfig& config) {
    std::vector<std::string> errors;

    // Required fields
    if (config.id.empty()) {
        errors.push_back("Entity ID is required");
    }
    if (config.name.empty()) {
        errors.push_back("Entity name is required");
    }

    // Validate type
    if (config.type != AssetType::Entity &&
        config.type != AssetType::Unit &&
        config.type != AssetType::Hero &&
        config.type != AssetType::Building &&
        config.type != AssetType::ResourceNode &&
        config.type != AssetType::Projectile &&
        config.type != AssetType::Decoration) {
        errors.push_back("Invalid entity type");
    }

    // Validate stats
    if (config.stats.health <= 0) {
        errors.push_back("Health must be positive");
    }

    // Validate SDF model if present
    if (config.sdfModel) {
        auto modelErrors = ValidateSDFModel(*config.sdfModel,
            config.skeleton ? &(*config.skeleton) : nullptr);
        errors.insert(errors.end(), modelErrors.begin(), modelErrors.end());
    }

    // Validate skeleton bone hierarchy
    if (config.skeleton) {
        for (const auto& bone : config.skeleton->bones) {
            if (bone.name.empty()) {
                errors.push_back("Bone name is required");
            }
            if (!bone.parent.empty() && bone.parent != "null") {
                bool parentFound = false;
                for (const auto& otherBone : config.skeleton->bones) {
                    if (otherBone.name == bone.parent) {
                        parentFound = true;
                        break;
                    }
                }
                if (!parentFound) {
                    errors.push_back("Bone " + bone.name + " references non-existent parent: " + bone.parent);
                }
            }
        }
    }

    // Validate animation set if present
    if (config.animationSet) {
        const auto& animSet = *config.animationSet;

        // Validate animation state machine
        if (!animSet.stateMachine.initialState.empty()) {
            if (animSet.stateMachine.states.find(animSet.stateMachine.initialState)
                == animSet.stateMachine.states.end()) {
                errors.push_back("Initial animation state not found: " + animSet.stateMachine.initialState);
            }

            for (const auto& [stateName, state] : animSet.stateMachine.states) {
                // Check transition targets
                for (const auto& transition : state.transitions) {
                    if (animSet.stateMachine.states.find(transition.to)
                        == animSet.stateMachine.states.end()) {
                        errors.push_back("State " + stateName + " has transition to non-existent state: " + transition.to);
                    }
                }
            }
        }
    }

    // Validate abilities
    for (const auto& ability : config.abilities) {
        if (ability.id.empty()) {
            errors.push_back("Ability ID is required");
        }
        if (ability.cooldown < 0) {
            errors.push_back("Ability " + ability.id + " cooldown cannot be negative");
        }
    }

    return errors;
}

std::vector<std::string> SDFConfigLoader::ValidateSDFModel(
    const SDFModelConfig& config,
    const SkeletonConfig* skeleton) {

    std::vector<std::string> errors;

    if (config.primitives.empty()) {
        errors.push_back("SDF model must have at least one primitive");
    }

    for (const auto& primitive : config.primitives) {
        if (primitive.id.empty()) {
            errors.push_back("Primitive ID is required");
        }
        if (primitive.type.empty()) {
            errors.push_back("Primitive type is required for: " + primitive.id);
        }

        // Validate operation
        static const std::vector<std::string> validOps = {
            "Union", "Subtraction", "Intersection",
            "SmoothUnion", "SmoothSubtraction", "SmoothIntersection"
        };
        bool validOp = false;
        for (const auto& op : validOps) {
            if (primitive.operation == op) {
                validOp = true;
                break;
            }
        }
        if (!validOp) {
            errors.push_back("Invalid operation for primitive " + primitive.id + ": " + primitive.operation);
        }

        // Validate bone reference if present
        if (!primitive.bone.empty() && skeleton != nullptr) {
            bool boneFound = false;
            for (const auto& bone : skeleton->bones) {
                if (bone.name == primitive.bone) {
                    boneFound = true;
                    break;
                }
            }
            if (!boneFound) {
                errors.push_back("Primitive " + primitive.id + " references non-existent bone: " + primitive.bone);
            }
        }
    }

    return errors;
}

// =============================================================================
// Base Asset Parsing
// =============================================================================

void SDFConfigLoader::ParseBaseAsset(const nlohmann::json& json, AssetConfig& config) {
    config.id = json.value("id", "");
    config.name = json.value("name", "");
    config.description = json.value("description", "");

    // Parse type
    std::string typeStr = json.value("type", "asset");
    config.type = StringToAssetType(typeStr);

    // Parse tags
    if (json.contains("tags")) {
        for (const auto& tag : json["tags"]) {
            config.tags.push_back(tag.get<std::string>());
        }
    }

    // Store metadata
    if (json.contains("metadata")) {
        config.metadata = json["metadata"];
    }
}

// =============================================================================
// Entity Parsing
// =============================================================================

EntityConfig SDFConfigLoader::ParseEntity(const nlohmann::json& json) {
    EntityConfig config;

    // Parse base asset fields
    ParseBaseAsset(json, config);

    config.race = json.value("race", "");
    config.category = json.value("category", "");

    // Parse modular references
    config.sdfModelRef = json.value("sdfModelRef", "");
    config.skeletonRef = json.value("skeletonRef", "");
    config.animationSetRef = json.value("animationSetRef", "");
    config.behaviorRef = json.value("behaviorRef", "");

    if (json.contains("stats")) {
        config.stats = ParseStats(json["stats"]);
    }

    if (json.contains("costs")) {
        config.costs = ParseCosts(json["costs"]);
    }

    if (json.contains("requirements")) {
        for (const auto& req : json["requirements"]) {
            config.requirements.push_back(req.get<std::string>());
        }
    }

    // Parse inline SDF model
    if (json.contains("sdfModel")) {
        config.sdfModel = ParseSDFModel(json["sdfModel"]);
    }

    // Parse inline skeleton
    if (json.contains("skeleton")) {
        config.skeleton = ParseSkeleton(json["skeleton"]);
    }

    // Parse inline animation set or legacy animations format
    if (json.contains("animationSet")) {
        config.animationSet = ParseAnimationSet(json["animationSet"]);
    } else if (json.contains("animations")) {
        // Legacy format: convert animations map to animation set
        AnimationSetConfig animSet;
        ParseBaseAsset(json, animSet);
        animSet.type = AssetType::AnimationSet;

        for (const auto& [name, animJson] : json["animations"].items()) {
            // Store animation references (in real usage, these would be separate files)
            animSet.animationRefs.push_back(name);
        }

        if (json.contains("animationStateMachine")) {
            animSet.stateMachine = ParseAnimationStateMachine(json["animationStateMachine"]);
        }

        config.animationSet = animSet;
    }

    // Parse inline behavior
    if (json.contains("behaviors")) {
        config.behavior = ParseBehaviors(json["behaviors"]);
    }

    // Parse ability references
    if (json.contains("abilityRefs")) {
        for (const auto& ref : json["abilityRefs"]) {
            config.abilityRefs.push_back(ref.get<std::string>());
        }
    }

    // Parse inline abilities
    if (json.contains("abilities")) {
        for (const auto& abilityJson : json["abilities"]) {
            config.abilities.push_back(ParseAbility(abilityJson));
        }
    }

    // Parse effect references
    if (json.contains("effectRefs")) {
        for (const auto& ref : json["effectRefs"]) {
            config.effectRefs.push_back(ref.get<std::string>());
        }
    }

    // Parse inline effects
    if (json.contains("effects")) {
        for (const auto& [name, effectJson] : json["effects"].items()) {
            config.effects[name] = ParseEffect(name, effectJson);
        }
    }

    // Transform defaults
    if (json.contains("spawnOffset")) {
        config.spawnOffset = ParseVec3(json["spawnOffset"]);
    }
    config.collisionRadius = json.value("collisionRadius", 0.5f);
    config.selectionRadius = json.value("selectionRadius", 1.0f);

    return config;
}

UnitConfig SDFConfigLoader::ParseUnit(const nlohmann::json& json) {
    UnitConfig config;

    // Parse base entity fields (manual copy since we can't do proper inheritance parsing)
    EntityConfig baseEntity = ParseEntity(json);

    // Copy base fields
    static_cast<EntityConfig&>(config) = baseEntity;
    config.type = AssetType::Unit;

    // Unit-specific fields
    config.unitClass = json.value("unitClass", "melee");
    config.armorType = json.value("armorType", "medium");
    config.attackType = json.value("attackType", "normal");
    config.squadSize = json.value("squadSize", 1);

    return config;
}

HeroConfig SDFConfigLoader::ParseHero(const nlohmann::json& json) {
    HeroConfig config;

    // Parse unit fields first
    UnitConfig baseUnit = ParseUnit(json);
    static_cast<UnitConfig&>(config) = baseUnit;
    config.type = AssetType::Hero;

    // Hero-specific fields
    config.heroClass = json.value("heroClass", "warrior");
    config.startingLevel = json.value("startingLevel", 1);

    config.healthPerLevel = json.value("healthPerLevel", 50);
    config.manaPerLevel = json.value("manaPerLevel", 25);
    config.damagePerLevel = json.value("damagePerLevel", 3);
    config.strPerLevel = json.value("strPerLevel", 2.0f);
    config.agiPerLevel = json.value("agiPerLevel", 1.5f);
    config.intPerLevel = json.value("intPerLevel", 2.0f);

    // Hero ability references
    if (json.contains("heroAbilityRefs")) {
        for (const auto& ref : json["heroAbilityRefs"]) {
            config.heroAbilityRefs.push_back(ref.get<std::string>());
        }
    }
    config.ultimateAbilityRef = json.value("ultimateAbilityRef", "");

    return config;
}

BuildingConfig SDFConfigLoader::ParseBuilding(const nlohmann::json& json) {
    BuildingConfig config;

    // Parse base entity fields
    EntityConfig baseEntity = ParseEntity(json);
    static_cast<EntityConfig&>(config) = baseEntity;
    config.type = AssetType::Building;

    // Building-specific fields
    if (json.contains("trains")) {
        for (const auto& unit : json["trains"]) {
            config.trains.push_back(unit.get<std::string>());
        }
    }

    if (json.contains("upgrades")) {
        for (const auto& upgrade : json["upgrades"]) {
            config.upgrades.push_back(upgrade.get<std::string>());
        }
    }

    if (json.contains("researches")) {
        for (const auto& research : json["researches"]) {
            config.researches.push_back(research.get<std::string>());
        }
    }

    config.isDefensive = json.value("isDefensive", false);
    config.isMainBuilding = json.value("isMainBuilding", false);
    config.providesDropOff = json.value("providesDropOff", false);

    if (json.contains("footprint")) {
        config.footprint = ParseVec2(json["footprint"], glm::vec2(2.0f, 2.0f));
    }

    return config;
}

ResourceNodeConfig SDFConfigLoader::ParseResourceNode(const nlohmann::json& json) {
    ResourceNodeConfig config;

    // Parse base entity fields
    EntityConfig baseEntity = ParseEntity(json);
    static_cast<EntityConfig&>(config) = baseEntity;
    config.type = AssetType::ResourceNode;

    // Resource node-specific fields
    config.resourceType = json.value("resourceType", "gold");
    config.resourceAmount = json.value("resourceAmount", 1000);
    config.harvestRate = json.value("harvestRate", 10);
    config.harvestTime = json.value("harvestTime", 1.0f);
    config.depletes = json.value("depletes", true);
    config.respawns = json.value("respawns", false);
    config.respawnTime = json.value("respawnTime", 0.0f);

    return config;
}

ProjectileConfig SDFConfigLoader::ParseProjectile(const nlohmann::json& json) {
    ProjectileConfig config;

    // Parse base entity fields
    EntityConfig baseEntity = ParseEntity(json);
    static_cast<EntityConfig&>(config) = baseEntity;
    config.type = AssetType::Projectile;

    // Projectile-specific fields
    config.speed = json.value("speed", 500.0f);
    config.arcHeight = json.value("arcHeight", 0.0f);
    config.homing = json.value("homing", false);
    config.turnRate = json.value("turnRate", 0.0f);
    config.impactEffectRef = json.value("impactEffectRef", "");
    config.damage = json.value("damage", 0);
    config.splashRadius = json.value("splashRadius", 0.0f);

    return config;
}

DecorationConfig SDFConfigLoader::ParseDecoration(const nlohmann::json& json) {
    DecorationConfig config;

    // Parse base entity fields
    EntityConfig baseEntity = ParseEntity(json);
    static_cast<EntityConfig&>(config) = baseEntity;
    config.type = AssetType::Decoration;

    // Decoration-specific fields
    config.blocksPathing = json.value("blocksPathing", false);
    config.blocksBuilding = json.value("blocksBuilding", true);
    config.fadeDistance = json.value("fadeDistance", 100.0f);

    return config;
}

// =============================================================================
// Component Parsing
// =============================================================================

StatsConfig SDFConfigLoader::ParseStats(const nlohmann::json& json) {
    StatsConfig stats;

    stats.health = json.value("health", 100);
    stats.maxHealth = json.value("maxHealth", stats.health);
    stats.mana = json.value("mana", 0);
    stats.maxMana = json.value("maxMana", stats.mana);
    stats.armor = json.value("armor", 0);
    stats.damage = json.value("damage", 10);
    stats.attackSpeed = json.value("attackSpeed", 1.0f);
    stats.moveSpeed = json.value("moveSpeed", 200.0f);
    stats.attackRange = json.value("attackRange", 100.0f);
    stats.healthRegen = json.value("healthRegen", 0.0f);
    stats.manaRegen = json.value("manaRegen", 0.0f);
    stats.sightRange = json.value("sightRange", 800.0f);
    stats.flying = json.value("flying", false);

    // Building-specific
    stats.foodProvided = json.value("foodProvided", 0);
    stats.buildTime = json.value("buildTime", 0.0f);

    // Hero-specific
    stats.level = json.value("level", 1);
    stats.maxLevel = json.value("maxLevel", 10);
    stats.experience = json.value("experience", 0);
    stats.strength = json.value("strength", 0);
    stats.agility = json.value("agility", 0);
    stats.intelligence = json.value("intelligence", 0);

    return stats;
}

CostConfig SDFConfigLoader::ParseCosts(const nlohmann::json& json) {
    CostConfig costs;

    costs.gold = json.value("gold", 0);
    costs.lumber = json.value("lumber", 0);
    costs.food = json.value("food", 0);
    costs.mana = json.value("mana", 0);
    costs.buildTime = json.value("buildTime", 0.0f);

    return costs;
}

SDFModelConfig SDFConfigLoader::ParseSDFModel(const nlohmann::json& json) {
    SDFModelConfig model;

    // Parse base asset fields
    ParseBaseAsset(json, model);
    model.type = AssetType::SDFModel;
    model.name = json.value("name", "");

    if (json.contains("bounds")) {
        const auto& bounds = json["bounds"];
        if (bounds.contains("min")) {
            model.boundsMin = ParseVec3(bounds["min"], glm::vec3(-1.0f));
        }
        if (bounds.contains("max")) {
            model.boundsMax = ParseVec3(bounds["max"], glm::vec3(1.0f));
        }
    }

    if (json.contains("primitives")) {
        for (const auto& primJson : json["primitives"]) {
            model.primitives.push_back(ParsePrimitive(primJson));
        }
    }

    // LOD models
    if (json.contains("lodModels")) {
        for (const auto& lod : json["lodModels"]) {
            model.lodModels.push_back(lod.get<std::string>());
        }
    }

    return model;
}

SDFPrimitiveConfig SDFConfigLoader::ParsePrimitive(const nlohmann::json& json) {
    SDFPrimitiveConfig primitive;

    primitive.id = json.value("id", "");
    primitive.type = json.value("type", "");

    if (json.contains("params")) {
        primitive.params = json["params"];
    }

    if (json.contains("transform")) {
        const auto& transform = json["transform"];
        if (transform.contains("position")) {
            primitive.position = ParseVec3(transform["position"]);
        }
        if (transform.contains("rotation")) {
            primitive.rotation = ParseQuat(transform["rotation"]);
        }
        if (transform.contains("scale")) {
            primitive.scale = ParseVec3(transform["scale"], glm::vec3(1.0f));
        }
    }

    // Material reference or inline material
    if (json.contains("materialRef")) {
        primitive.materialRef = json["materialRef"].get<std::string>();
    }

    if (json.contains("material")) {
        const auto& material = json["material"];
        if (material.contains("baseColor")) {
            primitive.baseColor = ParseVec4(material["baseColor"]);
        }
        primitive.metallic = material.value("metallic", 0.0f);
        primitive.roughness = material.value("roughness", 0.5f);
        if (material.contains("emissive")) {
            primitive.emissive = ParseVec3(material["emissive"]);
        }
    }

    primitive.operation = json.value("operation", "Union");
    primitive.smoothness = json.value("smoothness", 0.0f);
    primitive.bone = json.value("bone", "");

    return primitive;
}

SkeletonConfig SDFConfigLoader::ParseSkeleton(const nlohmann::json& json) {
    SkeletonConfig skeleton;

    // Parse base asset fields
    ParseBaseAsset(json, skeleton);
    skeleton.type = AssetType::Skeleton;

    if (json.contains("bones")) {
        for (const auto& boneJson : json["bones"]) {
            skeleton.bones.push_back(ParseBone(boneJson));
        }
    }

    return skeleton;
}

BoneConfig SDFConfigLoader::ParseBone(const nlohmann::json& json) {
    BoneConfig bone;

    bone.name = json.value("name", "");

    if (json.contains("parent") && !json["parent"].is_null()) {
        bone.parent = json["parent"].get<std::string>();
    }

    if (json.contains("position")) {
        bone.position = ParseVec3(json["position"]);
    }

    if (json.contains("rotation")) {
        bone.rotation = ParseQuat(json["rotation"]);
    }

    if (json.contains("scale")) {
        bone.scale = ParseVec3(json["scale"], glm::vec3(1.0f));
    }

    return bone;
}

AnimationConfig SDFConfigLoader::ParseAnimation(const std::string& name, const nlohmann::json& json) {
    AnimationConfig anim;

    // Parse base asset fields
    ParseBaseAsset(json, anim);
    anim.type = AssetType::Animation;
    anim.name = name.empty() ? json.value("name", "") : name;

    anim.duration = json.value("duration", 1.0f);
    anim.loop = json.value("loop", false);
    anim.skeletonRef = json.value("skeletonRef", "");

    if (json.contains("keyframes")) {
        for (const auto& kfJson : json["keyframes"]) {
            anim.keyframes.push_back(ParseKeyframe(kfJson));
        }
    }

    return anim;
}

KeyframeConfig SDFConfigLoader::ParseKeyframe(const nlohmann::json& json) {
    KeyframeConfig keyframe;

    keyframe.time = json.value("time", 0.0f);

    if (json.contains("bones")) {
        for (const auto& [boneName, boneData] : json["bones"].items()) {
            keyframe.boneTransforms[boneName] = boneData;
        }
    }

    if (json.contains("events")) {
        for (const auto& event : json["events"]) {
            keyframe.events.push_back(event.get<std::string>());
        }
    }

    if (json.contains("constructionProgress")) {
        keyframe.constructionProgress = json["constructionProgress"].get<float>();
    }

    return keyframe;
}

AnimationStateMachineConfig SDFConfigLoader::ParseAnimationStateMachine(const nlohmann::json& json) {
    AnimationStateMachineConfig stateMachine;

    stateMachine.initialState = json.value("initialState", "");

    if (json.contains("states")) {
        for (const auto& [stateName, stateJson] : json["states"].items()) {
            AnimationStateConfig state;

            // Support both "animation" (legacy) and "animationRef" (new)
            state.animationRef = stateJson.value("animationRef",
                stateJson.value("animation", ""));
            state.playbackSpeed = stateJson.value("playbackSpeed", 1.0f);

            if (stateJson.contains("transitions")) {
                for (const auto& transJson : stateJson["transitions"]) {
                    StateTransitionConfig transition;
                    transition.to = transJson.value("to", "");
                    transition.condition = transJson.value("condition", "");
                    transition.blendTime = transJson.value("blendTime", 0.2f);
                    state.transitions.push_back(transition);
                }
            }

            stateMachine.states[stateName] = state;
        }
    }

    return stateMachine;
}

AnimationSetConfig SDFConfigLoader::ParseAnimationSet(const nlohmann::json& json) {
    AnimationSetConfig animSet;

    // Parse base asset fields
    ParseBaseAsset(json, animSet);
    animSet.type = AssetType::AnimationSet;

    animSet.skeletonRef = json.value("skeletonRef", "");

    // Animation references
    if (json.contains("animationRefs")) {
        for (const auto& ref : json["animationRefs"]) {
            animSet.animationRefs.push_back(ref.get<std::string>());
        }
    }

    // Animation state machine
    if (json.contains("stateMachine")) {
        animSet.stateMachine = ParseAnimationStateMachine(json["stateMachine"]);
    }

    return animSet;
}

AbilityConfig SDFConfigLoader::ParseAbility(const nlohmann::json& json) {
    AbilityConfig ability;

    // Parse base asset fields
    ParseBaseAsset(json, ability);
    ability.type = AssetType::Ability;

    ability.hotkey = json.value("hotkey", "");
    ability.targetType = json.value("targetType", "");
    ability.icon = json.value("icon", "");

    ability.cooldown = json.value("cooldown", 0.0f);
    ability.manaCost = json.value("manaCost", 0);
    ability.range = json.value("range", 0.0f);
    ability.castTime = json.value("castTime", 0.0f);
    ability.duration = json.value("duration", 0.0f);
    ability.radius = json.value("radius", 0.0f);

    // Effect references
    if (json.contains("effectRefs")) {
        for (const auto& ref : json["effectRefs"]) {
            ability.effectRefs.push_back(ref.get<std::string>());
        }
    }

    // Store all params for flexibility
    ability.params = json;

    return ability;
}

BehaviorConfig SDFConfigLoader::ParseBehaviors(const nlohmann::json& json) {
    BehaviorConfig behavior;

    // Parse base asset fields if present
    ParseBaseAsset(json, behavior);
    behavior.type = AssetType::Behavior;

    // Parse triggers - support both "triggers" wrapper and direct trigger definitions
    const nlohmann::json& triggersJson = json.contains("triggers") ? json["triggers"] : json;

    for (const auto& [triggerName, triggerJson] : triggersJson.items()) {
        // Skip base asset fields
        if (triggerName == "id" || triggerName == "name" || triggerName == "type" ||
            triggerName == "description" || triggerName == "tags" || triggerName == "metadata") {
            continue;
        }
        behavior.triggers[triggerName] = ParseBehaviorTrigger(triggerJson);
    }

    return behavior;
}

BehaviorTriggerConfig SDFConfigLoader::ParseBehaviorTrigger(const nlohmann::json& json) {
    BehaviorTriggerConfig trigger;

    if (json.contains("conditions")) {
        for (const auto& condJson : json["conditions"]) {
            BehaviorConditionConfig condition;
            condition.type = condJson.value("type", "");
            condition.params = condJson;
            trigger.conditions.push_back(condition);
        }
    }

    if (json.contains("actions")) {
        for (const auto& actionJson : json["actions"]) {
            BehaviorActionConfig action;
            action.type = actionJson.value("type", "");
            action.params = actionJson;
            trigger.actions.push_back(action);
        }
    }

    return trigger;
}

EffectConfig SDFConfigLoader::ParseEffect(const std::string& name, const nlohmann::json& json) {
    EffectConfig effect;

    // Parse base asset fields
    ParseBaseAsset(json, effect);
    effect.type = AssetType::Effect;
    effect.name = name.empty() ? json.value("name", "") : name;

    effect.effectType = json.value("effectType", json.value("type", ""));
    effect.params = json;
    effect.duration = json.value("duration", 0.0f);
    effect.attachBone = json.value("attachBone", "");

    if (json.contains("offset")) {
        effect.offset = ParseVec3(json["offset"]);
    }

    return effect;
}

// =============================================================================
// Resource Parsing
// =============================================================================

TextureConfig SDFConfigLoader::ParseTexture(const nlohmann::json& json) {
    TextureConfig texture;

    // Parse base asset fields
    ParseBaseAsset(json, texture);
    texture.type = AssetType::Texture;

    texture.path = json.value("path", "");
    texture.format = json.value("format", "png");
    texture.generateMipmaps = json.value("generateMipmaps", true);
    texture.sRGB = json.value("sRGB", true);

    return texture;
}

MaterialConfig SDFConfigLoader::ParseMaterial(const nlohmann::json& json) {
    MaterialConfig material;

    // Parse base asset fields
    ParseBaseAsset(json, material);
    material.type = AssetType::Material;

    if (json.contains("baseColor")) {
        material.baseColor = ParseVec4(json["baseColor"]);
    }
    material.metallic = json.value("metallic", 0.0f);
    material.roughness = json.value("roughness", 0.5f);

    if (json.contains("emissive")) {
        material.emissive = ParseVec3(json["emissive"]);
    }

    material.albedoTexture = json.value("albedoTexture", "");
    material.normalTexture = json.value("normalTexture", "");
    material.metallicTexture = json.value("metallicTexture", "");
    material.roughnessTexture = json.value("roughnessTexture", "");
    material.aoTexture = json.value("aoTexture", "");
    material.emissiveTexture = json.value("emissiveTexture", "");

    return material;
}

// =============================================================================
// Utility Helpers
// =============================================================================

glm::vec2 SDFConfigLoader::ParseVec2(const nlohmann::json& json, const glm::vec2& defaultValue) {
    if (json.is_array() && json.size() >= 2) {
        return glm::vec2(
            json[0].get<float>(),
            json[1].get<float>()
        );
    }
    return defaultValue;
}

glm::vec3 SDFConfigLoader::ParseVec3(const nlohmann::json& json, const glm::vec3& defaultValue) {
    if (json.is_array() && json.size() >= 3) {
        return glm::vec3(
            json[0].get<float>(),
            json[1].get<float>(),
            json[2].get<float>()
        );
    }
    return defaultValue;
}

glm::vec4 SDFConfigLoader::ParseVec4(const nlohmann::json& json, const glm::vec4& defaultValue) {
    if (json.is_array() && json.size() >= 4) {
        return glm::vec4(
            json[0].get<float>(),
            json[1].get<float>(),
            json[2].get<float>(),
            json[3].get<float>()
        );
    }
    return defaultValue;
}

glm::quat SDFConfigLoader::ParseQuat(const nlohmann::json& json, const glm::quat& defaultValue) {
    if (json.is_array() && json.size() >= 4) {
        // JSON format: [x, y, z, w]
        return glm::quat(
            json[3].get<float>(),  // w
            json[0].get<float>(),  // x
            json[1].get<float>(),  // y
            json[2].get<float>()   // z
        );
    }
    return defaultValue;
}

} // namespace Engine
