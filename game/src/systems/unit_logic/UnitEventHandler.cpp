#include "UnitEventHandler.hpp"
#include <fstream>
#include <algorithm>

namespace Vehement {

// ============================================================================
// EventBinding
// ============================================================================

json EventBinding::ToJson() const {
    return json{
        {"eventName", eventName},
        {"handlerType", handlerType},
        {"parameters", parameters},
        {"enabled", enabled}
    };
}

EventBinding EventBinding::FromJson(const json& j) {
    EventBinding binding;
    binding.eventName = j.value("eventName", "");
    binding.handlerType = j.value("handlerType", "custom");
    binding.enabled = j.value("enabled", true);

    if (j.contains("parameters")) {
        binding.parameters = j["parameters"];
    }

    return binding;
}

// ============================================================================
// UnitEventHandler
// ============================================================================

UnitEventHandler::UnitEventHandler() = default;
UnitEventHandler::~UnitEventHandler() {
    if (m_eventSystem && !m_eventHandlerId.empty()) {
        m_eventSystem->UnregisterHandler(m_eventHandlerId);
    }
}

void UnitEventHandler::Initialize(AnimationEventSystem* eventSystem) {
    m_eventSystem = eventSystem;

    if (m_eventSystem) {
        // Register wildcard handler for all animation events
        m_eventHandlerId = m_eventSystem->RegisterHandler("*",
            [this](AnimationEventData& event) {
                OnAnimationEvent(event);
            });
    }
}

bool UnitEventHandler::LoadBindings(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        json config;
        file >> config;
        return LoadBindingsFromJson(config);
    } catch (...) {
        return false;
    }
}

bool UnitEventHandler::LoadBindingsFromJson(const json& config) {
    try {
        m_bindings.clear();

        if (config.contains("bindings") && config["bindings"].is_array()) {
            for (const auto& b : config["bindings"]) {
                m_bindings.push_back(EventBinding::FromJson(b));
            }
        }

        // Load sound mappings
        if (config.contains("footstepSounds") && config["footstepSounds"].is_object()) {
            for (const auto& [surface, sound] : config["footstepSounds"].items()) {
                m_footstepSounds[surface] = sound.get<std::string>();
            }
        }

        // Load VFX mappings
        if (config.contains("eventVFX") && config["eventVFX"].is_object()) {
            for (const auto& [event, vfx] : config["eventVFX"].items()) {
                m_eventVFX[event] = vfx.get<std::string>();
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool UnitEventHandler::SaveBindings(const std::string& configPath) const {
    try {
        std::ofstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        file << ToJson().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

json UnitEventHandler::ToJson() const {
    json j;

    j["bindings"] = json::array();
    for (const auto& binding : m_bindings) {
        j["bindings"].push_back(binding.ToJson());
    }

    j["footstepSounds"] = m_footstepSounds;
    j["eventVFX"] = m_eventVFX;

    return j;
}

void UnitEventHandler::SetCallbacks(const UnitEventCallbacks& callbacks) {
    m_callbacks = callbacks;
}

void UnitEventHandler::SetUnitTransform(const glm::mat4& transform) {
    m_unitTransform = transform;
}

void UnitEventHandler::SetBonePositions(const std::unordered_map<std::string, glm::mat4>& boneTransforms) {
    m_boneTransforms = boneTransforms;
}

void UnitEventHandler::SetSurfaceType(const std::string& surfaceType) {
    m_currentSurfaceType = surfaceType;
}

void UnitEventHandler::AddBinding(const EventBinding& binding) {
    // Check if binding already exists
    auto it = std::find_if(m_bindings.begin(), m_bindings.end(),
                           [&binding](const EventBinding& b) {
                               return b.eventName == binding.eventName;
                           });

    if (it != m_bindings.end()) {
        *it = binding;
    } else {
        m_bindings.push_back(binding);
    }
}

bool UnitEventHandler::RemoveBinding(const std::string& eventName) {
    auto it = std::find_if(m_bindings.begin(), m_bindings.end(),
                           [&eventName](const EventBinding& b) {
                               return b.eventName == eventName;
                           });

    if (it != m_bindings.end()) {
        m_bindings.erase(it);
        return true;
    }
    return false;
}

EventBinding* UnitEventHandler::GetBinding(const std::string& eventName) {
    auto it = std::find_if(m_bindings.begin(), m_bindings.end(),
                           [&eventName](const EventBinding& b) {
                               return b.eventName == eventName;
                           });

    return it != m_bindings.end() ? &(*it) : nullptr;
}

void UnitEventHandler::SetBindingEnabled(const std::string& eventName, bool enabled) {
    if (auto* binding = GetBinding(eventName)) {
        binding->enabled = enabled;
    }
}

void UnitEventHandler::SetFootstepSound(const std::string& surfaceType, const std::string& soundId) {
    m_footstepSounds[surfaceType] = soundId;
}

std::string UnitEventHandler::GetFootstepSound(const std::string& surfaceType) const {
    auto it = m_footstepSounds.find(surfaceType);
    if (it != m_footstepSounds.end()) {
        return it->second;
    }

    // Try default
    it = m_footstepSounds.find("default");
    return it != m_footstepSounds.end() ? it->second : "";
}

void UnitEventHandler::SetEventVFX(const std::string& eventType, const std::string& vfxId) {
    m_eventVFX[eventType] = vfxId;
}

std::string UnitEventHandler::GetEventVFX(const std::string& eventType) const {
    auto it = m_eventVFX.find(eventType);
    return it != m_eventVFX.end() ? it->second : "";
}

void UnitEventHandler::OnAnimationEvent(AnimationEventData& event) {
    // Record event for debug
    if (m_debugLogging) {
        m_lastEvents.push_back(event.eventName);
        while (m_lastEvents.size() > MAX_LAST_EVENTS) {
            m_lastEvents.erase(m_lastEvents.begin());
        }
    }

    // Find binding for this event
    EventBinding* binding = GetBinding(event.eventName);
    if (!binding || !binding->enabled) {
        // Check for pattern matching in standard handlers
        if (event.eventName == "footstep") {
            HandleFootstepEvent(event.data);
        } else if (event.eventName == "attack_hit") {
            HandleHitFrameEvent(event.data);
        } else if (event.eventName == "spawn_projectile") {
            HandleProjectileSpawnEvent(event.data);
        } else if (event.eventName == "spawn_vfx") {
            HandleVFXSpawnEvent(event.data);
        } else if (event.eventName == "play_sound") {
            HandleSoundEvent(event.data);
        } else if (event.eventName == "equipment_visibility") {
            HandleEquipmentVisibilityEvent(event.data);
        } else {
            HandleCustomEvent(event.eventName, event.data);
        }
        return;
    }

    // Merge binding parameters with event data
    json mergedData = binding->parameters;
    if (event.data.is_object()) {
        for (const auto& [key, value] : event.data.items()) {
            mergedData[key] = value;
        }
    }

    // Dispatch based on handler type
    if (binding->handlerType == "footstep") {
        HandleFootstepEvent(mergedData);
    } else if (binding->handlerType == "hit") {
        HandleHitFrameEvent(mergedData);
    } else if (binding->handlerType == "projectile") {
        HandleProjectileSpawnEvent(mergedData);
    } else if (binding->handlerType == "vfx") {
        HandleVFXSpawnEvent(mergedData);
    } else if (binding->handlerType == "sound") {
        HandleSoundEvent(mergedData);
    } else if (binding->handlerType == "equipment") {
        HandleEquipmentVisibilityEvent(mergedData);
    } else {
        HandleCustomEvent(event.eventName, mergedData);
    }
}

void UnitEventHandler::HandleFootstepEvent(const json& data) {
    if (!m_callbacks.onFootstep) {
        return;
    }

    FootstepEvent event;
    event.foot = data.value("foot", "left");
    event.surfaceType = m_currentSurfaceType;
    event.intensity = data.value("intensity", 1.0f);

    // Get position from bone or unit transform
    std::string bone = data.value("bone", event.foot == "left" ? "foot_l" : "foot_r");
    event.position = GetBonePosition(bone);

    m_callbacks.onFootstep(event);
}

void UnitEventHandler::HandleHitFrameEvent(const json& data) {
    if (!m_callbacks.onAttackHit) {
        return;
    }

    AttackHitEvent event;
    event.attackId = data.value("attackId", "");
    event.attackIndex = data.value("attackIndex", 0);
    event.damageMultiplier = data.value("damageMultiplier", 1.0f);
    event.hitEffect = data.value("hitEffect", "");

    if (data.contains("hitboxOffset")) {
        event.hitboxOffset = glm::vec3(
            data["hitboxOffset"].value("x", 0.0f),
            data["hitboxOffset"].value("y", 0.0f),
            data["hitboxOffset"].value("z", 0.0f)
        );
    }

    if (data.contains("hitboxSize")) {
        event.hitboxSize = glm::vec3(
            data["hitboxSize"].value("x", 1.0f),
            data["hitboxSize"].value("y", 1.0f),
            data["hitboxSize"].value("z", 1.0f)
        );
    }

    m_callbacks.onAttackHit(event);
}

void UnitEventHandler::HandleProjectileSpawnEvent(const json& data) {
    if (!m_callbacks.onProjectileSpawn) {
        return;
    }

    ProjectileSpawnEvent event;
    event.projectileType = data.value("type", "");
    event.spawnBone = data.value("bone", "hand_r");
    event.speed = data.value("speed", 1.0f);

    if (data.contains("offset")) {
        event.offset = glm::vec3(
            data["offset"].value("x", 0.0f),
            data["offset"].value("y", 0.0f),
            data["offset"].value("z", 0.0f)
        );
    }

    // Get spawn position from bone
    glm::mat4 boneTransform = GetBoneTransform(event.spawnBone);
    glm::vec3 bonePos = glm::vec3(boneTransform[3]);
    event.offset += bonePos;

    // Direction from unit forward
    event.direction = glm::vec3(m_unitTransform[2]);

    m_callbacks.onProjectileSpawn(event);
}

void UnitEventHandler::HandleVFXSpawnEvent(const json& data) {
    if (!m_callbacks.onVFXSpawn) {
        return;
    }

    VFXSpawnEvent event;
    event.vfxId = data.value("vfx", "");
    event.attachBone = data.value("bone", "");
    event.scale = data.value("scale", 1.0f);
    event.duration = data.value("duration", -1.0f);
    event.attachToUnit = data.value("attach", false);

    if (data.contains("offset")) {
        event.offset = glm::vec3(
            data["offset"].value("x", 0.0f),
            data["offset"].value("y", 0.0f),
            data["offset"].value("z", 0.0f)
        );
    }

    m_callbacks.onVFXSpawn(event);
}

void UnitEventHandler::HandleSoundEvent(const json& data) {
    if (!m_callbacks.onSound) {
        return;
    }

    SoundEvent event;
    event.soundId = data.value("sound", "");
    event.volume = data.value("volume", 1.0f);
    event.pitch = data.value("pitch", 1.0f);
    event.is3D = data.value("is3D", true);

    // Get position from bone or unit
    std::string bone = data.value("bone", "");
    if (!bone.empty()) {
        event.position = GetBonePosition(bone);
    } else {
        event.position = glm::vec3(m_unitTransform[3]);
    }

    m_callbacks.onSound(event);
}

void UnitEventHandler::HandleEquipmentVisibilityEvent(const json& data) {
    if (!m_callbacks.onEquipmentVisibility) {
        return;
    }

    EquipmentVisibilityEvent event;
    event.equipmentSlot = data.value("slot", "");
    event.visible = data.value("visible", true);
    event.attachBone = data.value("bone", "");

    m_callbacks.onEquipmentVisibility(event);
}

void UnitEventHandler::HandleCustomEvent(const std::string& eventName, const json& data) {
    if (m_callbacks.onCustomEvent) {
        m_callbacks.onCustomEvent(eventName, data);
    }
}

glm::vec3 UnitEventHandler::GetBonePosition(const std::string& boneName) const {
    auto it = m_boneTransforms.find(boneName);
    if (it != m_boneTransforms.end()) {
        return glm::vec3(it->second[3]);
    }
    return glm::vec3(m_unitTransform[3]);
}

glm::mat4 UnitEventHandler::GetBoneTransform(const std::string& boneName) const {
    auto it = m_boneTransforms.find(boneName);
    if (it != m_boneTransforms.end()) {
        return it->second;
    }
    return m_unitTransform;
}

// ============================================================================
// UnitEventHandlerFactory
// ============================================================================

std::unique_ptr<UnitEventHandler> UnitEventHandlerFactory::CreateHumanoid(AnimationEventSystem* eventSystem) {
    auto handler = std::make_unique<UnitEventHandler>();
    handler->Initialize(eventSystem);

    // Add standard humanoid bindings
    handler->AddBinding({"footstep", "footstep", {{"bone", "foot_l"}}, true});
    handler->AddBinding({"footstep_left", "footstep", {{"foot", "left"}, {"bone", "foot_l"}}, true});
    handler->AddBinding({"footstep_right", "footstep", {{"foot", "right"}, {"bone", "foot_r"}}, true});
    handler->AddBinding({"attack_hit", "hit", {}, true});
    handler->AddBinding({"spawn_projectile", "projectile", {{"bone", "hand_r"}}, true});

    // Default footstep sounds
    handler->SetFootstepSound("default", "sfx/footstep_default");
    handler->SetFootstepSound("grass", "sfx/footstep_grass");
    handler->SetFootstepSound("stone", "sfx/footstep_stone");
    handler->SetFootstepSound("metal", "sfx/footstep_metal");
    handler->SetFootstepSound("wood", "sfx/footstep_wood");
    handler->SetFootstepSound("water", "sfx/footstep_water");

    return handler;
}

std::unique_ptr<UnitEventHandler> UnitEventHandlerFactory::CreateCreature(AnimationEventSystem* eventSystem) {
    auto handler = std::make_unique<UnitEventHandler>();
    handler->Initialize(eventSystem);

    // Creature-specific bindings
    handler->AddBinding({"footstep", "footstep", {}, true});
    handler->AddBinding({"attack_hit", "hit", {}, true});
    handler->AddBinding({"roar", "sound", {{"sound", "sfx/creature_roar"}}, true});

    return handler;
}

std::unique_ptr<UnitEventHandler> UnitEventHandlerFactory::CreateVehicle(AnimationEventSystem* eventSystem) {
    auto handler = std::make_unique<UnitEventHandler>();
    handler->Initialize(eventSystem);

    // Vehicle-specific bindings
    handler->AddBinding({"engine_start", "sound", {{"sound", "sfx/engine_start"}}, true});
    handler->AddBinding({"engine_stop", "sound", {{"sound", "sfx/engine_stop"}}, true});
    handler->AddBinding({"horn", "sound", {{"sound", "sfx/horn"}}, true});

    return handler;
}

} // namespace Vehement
