#include "UnitLogicConfig.hpp"
#include <fstream>
#include <filesystem>

namespace Vehement {

// ============================================================================
// AnimationMapping
// ============================================================================

json AnimationMapping::ToJson() const {
    json j = {
        {"id", id},
        {"clip", clipPath},
        {"speed", speed},
        {"loop", loop}
    };

    if (mirror) j["mirror"] = true;
    if (!variants.empty()) j["variants"] = variants;

    return j;
}

AnimationMapping AnimationMapping::FromJson(const json& j) {
    AnimationMapping mapping;
    mapping.id = j.value("id", "");
    mapping.clipPath = j.value("clip", "");
    mapping.speed = j.value("speed", 1.0f);
    mapping.loop = j.value("loop", true);
    mapping.mirror = j.value("mirror", false);

    if (j.contains("variants") && j["variants"].is_array()) {
        mapping.variants = j["variants"].get<std::vector<std::string>>();
    }

    return mapping;
}

// ============================================================================
// SoundMapping
// ============================================================================

json SoundMapping::ToJson() const {
    json j = {
        {"id", id},
        {"sound", soundPath},
        {"volume", volume}
    };

    if (pitchMin != 1.0f || pitchMax != 1.0f) {
        j["pitchRange"] = {pitchMin, pitchMax};
    }
    if (!variants.empty()) j["variants"] = variants;

    return j;
}

SoundMapping SoundMapping::FromJson(const json& j) {
    SoundMapping mapping;
    mapping.id = j.value("id", "");
    mapping.soundPath = j.value("sound", "");
    mapping.volume = j.value("volume", 1.0f);

    if (j.contains("pitchRange") && j["pitchRange"].is_array()) {
        mapping.pitchMin = j["pitchRange"][0].get<float>();
        mapping.pitchMax = j["pitchRange"][1].get<float>();
    }

    if (j.contains("variants") && j["variants"].is_array()) {
        mapping.variants = j["variants"].get<std::vector<std::string>>();
    }

    return mapping;
}

// ============================================================================
// VFXMapping
// ============================================================================

json VFXMapping::ToJson() const {
    json j = {
        {"id", id},
        {"vfx", vfxPath},
        {"scale", scale},
        {"attachToUnit", attachToUnit}
    };

    if (!attachBone.empty()) j["bone"] = attachBone;
    if (offset != glm::vec3(0.0f)) {
        j["offset"] = {offset.x, offset.y, offset.z};
    }

    return j;
}

VFXMapping VFXMapping::FromJson(const json& j) {
    VFXMapping mapping;
    mapping.id = j.value("id", "");
    mapping.vfxPath = j.value("vfx", "");
    mapping.attachBone = j.value("bone", "");
    mapping.scale = j.value("scale", 1.0f);
    mapping.attachToUnit = j.value("attachToUnit", true);

    if (j.contains("offset") && j["offset"].is_array()) {
        mapping.offset = glm::vec3(
            j["offset"][0].get<float>(),
            j["offset"][1].get<float>(),
            j["offset"][2].get<float>()
        );
    }

    return mapping;
}

// ============================================================================
// UnitLogicConfig
// ============================================================================

json UnitLogicConfig::ToJson() const {
    json j = {
        {"id", id},
        {"name", name},
        {"type", type}
    };

    if (!basedOn.empty()) j["basedOn"] = basedOn;
    if (!stateMachineConfig.empty()) j["stateMachine"] = stateMachineConfig;
    if (!locomotionBlendTreeConfig.empty()) j["locomotionBlendTree"] = locomotionBlendTreeConfig;
    if (!combatStateMachineConfig.empty()) j["combatStateMachine"] = combatStateMachineConfig;
    if (!abilityStateMachineConfig.empty()) j["abilityStateMachine"] = abilityStateMachineConfig;
    if (!eventBindingsConfig.empty()) j["eventBindings"] = eventBindingsConfig;

    // Animation mappings
    if (!animationMappings.empty()) {
        j["animations"] = json::array();
        for (const auto& anim : animationMappings) {
            j["animations"].push_back(anim.ToJson());
        }
    }

    // Sound mappings
    if (!soundMappings.empty()) {
        j["sounds"] = json::array();
        for (const auto& sound : soundMappings) {
            j["sounds"].push_back(sound.ToJson());
        }
    }

    // VFX mappings
    if (!vfxMappings.empty()) {
        j["vfx"] = json::array();
        for (const auto& vfx : vfxMappings) {
            j["vfx"].push_back(vfx.ToJson());
        }
    }

    // Masks
    j["masks"] = {
        {"upperBody", masks.upperBody},
        {"lowerBody", masks.lowerBody},
        {"fullBody", masks.fullBody},
        {"headOnly", masks.headOnly},
        {"handsOnly", masks.handsOnly}
    };

    // Timing
    j["timing"] = {
        {"locomotionBlendSpeed", timing.locomotionBlendSpeed},
        {"combatBlendSpeed", timing.combatBlendSpeed},
        {"transitionBlendTime", timing.transitionBlendTime},
        {"hitReactionDuration", timing.hitReactionDuration},
        {"stunRecoveryTime", timing.stunRecoveryTime}
    };

    // Features
    j["features"] = {
        {"useRootMotion", features.useRootMotion},
        {"useFootIK", features.useFootIK},
        {"useLookAt", features.useLookAt},
        {"useLayeredAnimation", features.useLayeredAnimation}
    };

    return j;
}

UnitLogicConfig UnitLogicConfig::FromJson(const json& j) {
    UnitLogicConfig config;

    config.id = j.value("id", "");
    config.name = j.value("name", "");
    config.type = j.value("type", "humanoid");
    config.basedOn = j.value("basedOn", "");

    config.stateMachineConfig = j.value("stateMachine", "");
    config.locomotionBlendTreeConfig = j.value("locomotionBlendTree", "");
    config.combatStateMachineConfig = j.value("combatStateMachine", "");
    config.abilityStateMachineConfig = j.value("abilityStateMachine", "");
    config.eventBindingsConfig = j.value("eventBindings", "");

    // Animation mappings
    if (j.contains("animations") && j["animations"].is_array()) {
        for (const auto& anim : j["animations"]) {
            config.animationMappings.push_back(AnimationMapping::FromJson(anim));
        }
    }

    // Sound mappings
    if (j.contains("sounds") && j["sounds"].is_array()) {
        for (const auto& sound : j["sounds"]) {
            config.soundMappings.push_back(SoundMapping::FromJson(sound));
        }
    }

    // VFX mappings
    if (j.contains("vfx") && j["vfx"].is_array()) {
        for (const auto& vfx : j["vfx"]) {
            config.vfxMappings.push_back(VFXMapping::FromJson(vfx));
        }
    }

    // Masks
    if (j.contains("masks")) {
        config.masks.upperBody = j["masks"].value("upperBody", "");
        config.masks.lowerBody = j["masks"].value("lowerBody", "");
        config.masks.fullBody = j["masks"].value("fullBody", "");
        config.masks.headOnly = j["masks"].value("headOnly", "");
        config.masks.handsOnly = j["masks"].value("handsOnly", "");
    }

    // Timing
    if (j.contains("timing")) {
        config.timing.locomotionBlendSpeed = j["timing"].value("locomotionBlendSpeed", 5.0f);
        config.timing.combatBlendSpeed = j["timing"].value("combatBlendSpeed", 8.0f);
        config.timing.transitionBlendTime = j["timing"].value("transitionBlendTime", 0.2f);
        config.timing.hitReactionDuration = j["timing"].value("hitReactionDuration", 0.3f);
        config.timing.stunRecoveryTime = j["timing"].value("stunRecoveryTime", 0.5f);
    }

    // Features
    if (j.contains("features")) {
        config.features.useRootMotion = j["features"].value("useRootMotion", false);
        config.features.useFootIK = j["features"].value("useFootIK", false);
        config.features.useLookAt = j["features"].value("useLookAt", false);
        config.features.useLayeredAnimation = j["features"].value("useLayeredAnimation", true);
    }

    return config;
}

// ============================================================================
// UnitLogicConfigManager
// ============================================================================

UnitLogicConfig* UnitLogicConfigManager::Load(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return nullptr;
        }

        json config;
        file >> config;

        auto unitConfig = std::make_unique<UnitLogicConfig>(UnitLogicConfig::FromJson(config));
        std::string id = unitConfig->id.empty() ? filepath : unitConfig->id;

        auto* ptr = unitConfig.get();
        m_configs[id] = std::move(unitConfig);
        m_pathToId[filepath] = id;

        return ptr;
    } catch (...) {
        return nullptr;
    }
}

void UnitLogicConfigManager::LoadDirectory(const std::string& directory, bool recursive) {
    try {
        namespace fs = std::filesystem;

        auto processFile = [this](const fs::path& path) {
            if (path.extension() == ".json") {
                Load(path.string());
            }
        };

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    processFile(entry.path());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    processFile(entry.path());
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }
}

UnitLogicConfig* UnitLogicConfigManager::Get(const std::string& id) {
    auto it = m_configs.find(id);
    return it != m_configs.end() ? it->second.get() : nullptr;
}

const UnitLogicConfig* UnitLogicConfigManager::Get(const std::string& id) const {
    auto it = m_configs.find(id);
    return it != m_configs.end() ? it->second.get() : nullptr;
}

UnitLogicConfig* UnitLogicConfigManager::CreateFromJson(const std::string& id, const json& config) {
    auto unitConfig = std::make_unique<UnitLogicConfig>(UnitLogicConfig::FromJson(config));
    if (unitConfig->id.empty()) {
        unitConfig->id = id;
    }

    auto* ptr = unitConfig.get();
    m_configs[id] = std::move(unitConfig);
    return ptr;
}

bool UnitLogicConfigManager::Remove(const std::string& id) {
    return m_configs.erase(id) > 0;
}

void UnitLogicConfigManager::Clear() {
    m_configs.clear();
    m_pathToId.clear();
}

std::vector<std::string> UnitLogicConfigManager::GetAllIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_configs.size());
    for (const auto& [id, config] : m_configs) {
        ids.push_back(id);
    }
    return ids;
}

void UnitLogicConfigManager::ApplyInheritance() {
    for (auto& [id, config] : m_configs) {
        ApplyInheritance(*config);
    }
}

void UnitLogicConfigManager::ApplyInheritance(UnitLogicConfig& config) {
    if (config.basedOn.empty()) {
        return;
    }

    auto* base = Get(config.basedOn);
    if (!base) {
        return;
    }

    // Apply base's inheritance first
    ApplyInheritance(*const_cast<UnitLogicConfig*>(base));

    // Merge base into config
    MergeConfig(config, *base);
}

void UnitLogicConfigManager::MergeConfig(UnitLogicConfig& target, const UnitLogicConfig& base) {
    // Only merge if target doesn't have a value
    if (target.stateMachineConfig.empty()) {
        target.stateMachineConfig = base.stateMachineConfig;
    }
    if (target.locomotionBlendTreeConfig.empty()) {
        target.locomotionBlendTreeConfig = base.locomotionBlendTreeConfig;
    }
    if (target.combatStateMachineConfig.empty()) {
        target.combatStateMachineConfig = base.combatStateMachineConfig;
    }
    if (target.abilityStateMachineConfig.empty()) {
        target.abilityStateMachineConfig = base.abilityStateMachineConfig;
    }

    // Merge animation mappings (add base animations if not overridden)
    for (const auto& baseAnim : base.animationMappings) {
        bool found = false;
        for (const auto& targetAnim : target.animationMappings) {
            if (targetAnim.id == baseAnim.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            target.animationMappings.push_back(baseAnim);
        }
    }

    // Similar for sounds and VFX
    for (const auto& baseSound : base.soundMappings) {
        bool found = false;
        for (const auto& targetSound : target.soundMappings) {
            if (targetSound.id == baseSound.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            target.soundMappings.push_back(baseSound);
        }
    }

    for (const auto& baseVfx : base.vfxMappings) {
        bool found = false;
        for (const auto& targetVfx : target.vfxMappings) {
            if (targetVfx.id == baseVfx.id) {
                found = true;
                break;
            }
        }
        if (!found) {
            target.vfxMappings.push_back(baseVfx);
        }
    }
}

void UnitLogicConfigManager::ReloadAll() {
    for (const auto& [path, id] : m_pathToId) {
        Load(path);
    }
    ApplyInheritance();
}

std::unique_ptr<UnitAnimationController> UnitLogicConfigManager::CreateAnimationController(
    const std::string& configId) const {

    auto* config = Get(configId);
    if (!config) {
        return nullptr;
    }

    UnitAnimationConfig animConfig;
    animConfig.stateMachineConfig = config->stateMachineConfig;
    animConfig.locomotionBlendTree = config->locomotionBlendTreeConfig;
    animConfig.combatConfig = config->combatStateMachineConfig;
    animConfig.abilityConfig = config->abilityStateMachineConfig;
    animConfig.upperBodyMask = config->masks.upperBody;
    animConfig.lowerBodyMask = config->masks.lowerBody;
    animConfig.fullBodyMask = config->masks.fullBody;
    animConfig.locomotionBlendSpeed = config->timing.locomotionBlendSpeed;
    animConfig.combatBlendSpeed = config->timing.combatBlendSpeed;
    animConfig.transitionBlendTime = config->timing.transitionBlendTime;

    // Build clip mappings
    for (const auto& anim : config->animationMappings) {
        animConfig.clipMappings[anim.id] = anim.clipPath;
    }

    auto controller = std::make_unique<UnitAnimationController>();
    if (!controller->Initialize(animConfig)) {
        return nullptr;
    }

    return controller;
}

std::unique_ptr<UnitEventHandler> UnitLogicConfigManager::CreateEventHandler(
    const std::string& configId,
    Nova::AnimationEventSystem* eventSystem) const {

    auto* config = Get(configId);

    std::unique_ptr<UnitEventHandler> handler;

    if (config) {
        if (config->type == "humanoid") {
            handler = UnitEventHandlerFactory::CreateHumanoid(eventSystem);
        } else if (config->type == "creature") {
            handler = UnitEventHandlerFactory::CreateCreature(eventSystem);
        } else if (config->type == "vehicle") {
            handler = UnitEventHandlerFactory::CreateVehicle(eventSystem);
        } else {
            handler = std::make_unique<UnitEventHandler>();
            handler->Initialize(eventSystem);
        }

        // Load additional bindings if specified
        if (!config->eventBindingsConfig.empty()) {
            handler->LoadBindings(config->eventBindingsConfig);
        }

        // Add sound mappings
        for (const auto& sound : config->soundMappings) {
            if (sound.id.find("footstep") != std::string::npos) {
                handler->SetFootstepSound(sound.id, sound.soundPath);
            }
        }

        // Add VFX mappings
        for (const auto& vfx : config->vfxMappings) {
            handler->SetEventVFX(vfx.id, vfx.vfxPath);
        }
    } else {
        handler = std::make_unique<UnitEventHandler>();
        handler->Initialize(eventSystem);
    }

    return handler;
}

// ============================================================================
// UnitLogicConfigBuilder
// ============================================================================

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetId(const std::string& id) {
    m_config.id = id;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetName(const std::string& name) {
    m_config.name = name;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetType(const std::string& type) {
    m_config.type = type;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetBasedOn(const std::string& basedOn) {
    m_config.basedOn = basedOn;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetStateMachine(const std::string& path) {
    m_config.stateMachineConfig = path;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetLocomotionBlendTree(const std::string& path) {
    m_config.locomotionBlendTreeConfig = path;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetCombatStateMachine(const std::string& path) {
    m_config.combatStateMachineConfig = path;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::AddAnimation(const std::string& id,
                                                              const std::string& clipPath,
                                                              float speed, bool loop) {
    AnimationMapping mapping;
    mapping.id = id;
    mapping.clipPath = clipPath;
    mapping.speed = speed;
    mapping.loop = loop;
    m_config.animationMappings.push_back(mapping);
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::AddSound(const std::string& id,
                                                          const std::string& soundPath,
                                                          float volume) {
    SoundMapping mapping;
    mapping.id = id;
    mapping.soundPath = soundPath;
    mapping.volume = volume;
    m_config.soundMappings.push_back(mapping);
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::AddVFX(const std::string& id,
                                                        const std::string& vfxPath,
                                                        const std::string& bone) {
    VFXMapping mapping;
    mapping.id = id;
    mapping.vfxPath = vfxPath;
    mapping.attachBone = bone;
    m_config.vfxMappings.push_back(mapping);
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetMasks(const std::string& upperBody,
                                                          const std::string& lowerBody,
                                                          const std::string& fullBody) {
    m_config.masks.upperBody = upperBody;
    m_config.masks.lowerBody = lowerBody;
    m_config.masks.fullBody = fullBody;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::SetTiming(float locomotionBlendSpeed,
                                                           float combatBlendSpeed,
                                                           float transitionTime) {
    m_config.timing.locomotionBlendSpeed = locomotionBlendSpeed;
    m_config.timing.combatBlendSpeed = combatBlendSpeed;
    m_config.timing.transitionBlendTime = transitionTime;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::EnableRootMotion(bool enable) {
    m_config.features.useRootMotion = enable;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::EnableFootIK(bool enable) {
    m_config.features.useFootIK = enable;
    return *this;
}

UnitLogicConfigBuilder& UnitLogicConfigBuilder::EnableLookAt(bool enable) {
    m_config.features.useLookAt = enable;
    return *this;
}

UnitLogicConfig UnitLogicConfigBuilder::Build() const {
    return m_config;
}

json UnitLogicConfigBuilder::ToJson() const {
    return m_config.ToJson();
}

} // namespace Vehement
