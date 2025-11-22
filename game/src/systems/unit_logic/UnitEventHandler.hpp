#pragma once

#include "engine/animation/AnimationEventSystem.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Vehement {

using Nova::json;
using Nova::AnimationEventSystem;
using Nova::AnimationEventData;

/**
 * @brief Footstep event data
 */
struct FootstepEvent {
    std::string foot;           // "left" or "right"
    std::string surfaceType;    // "grass", "stone", "metal", etc.
    glm::vec3 position;
    float intensity = 1.0f;
};

/**
 * @brief Attack hit frame data
 */
struct AttackHitEvent {
    std::string attackId;
    int attackIndex = 0;
    float damageMultiplier = 1.0f;
    glm::vec3 hitboxOffset;
    glm::vec3 hitboxSize;
    std::string hitEffect;
};

/**
 * @brief Projectile spawn data
 */
struct ProjectileSpawnEvent {
    std::string projectileType;
    std::string spawnBone;
    glm::vec3 offset;
    glm::vec3 direction;
    float speed = 1.0f;
};

/**
 * @brief VFX spawn data
 */
struct VFXSpawnEvent {
    std::string vfxId;
    std::string attachBone;
    glm::vec3 offset;
    glm::vec3 rotation;
    float scale = 1.0f;
    float duration = -1.0f;     // -1 = use VFX default
    bool attachToUnit = false;
};

/**
 * @brief Sound event data
 */
struct SoundEvent {
    std::string soundId;
    float volume = 1.0f;
    float pitch = 1.0f;
    glm::vec3 position;
    bool is3D = true;
};

/**
 * @brief Equipment visibility event
 */
struct EquipmentVisibilityEvent {
    std::string equipmentSlot;
    bool visible = true;
    std::string attachBone;
};

/**
 * @brief Animation event handler callbacks
 */
struct UnitEventCallbacks {
    std::function<void(const FootstepEvent&)> onFootstep;
    std::function<void(const AttackHitEvent&)> onAttackHit;
    std::function<void(const ProjectileSpawnEvent&)> onProjectileSpawn;
    std::function<void(const VFXSpawnEvent&)> onVFXSpawn;
    std::function<void(const SoundEvent&)> onSound;
    std::function<void(const EquipmentVisibilityEvent&)> onEquipmentVisibility;
    std::function<void(const std::string&, const json&)> onCustomEvent;
};

/**
 * @brief Event binding configuration
 */
struct EventBinding {
    std::string eventName;
    std::string handlerType;    // "footstep", "hit", "projectile", "vfx", "sound", "equipment", "custom"
    json parameters;
    bool enabled = true;

    [[nodiscard]] json ToJson() const;
    static EventBinding FromJson(const json& j);
};

/**
 * @brief Handles animation events for units
 *
 * Processes animation events and translates them to game actions:
 * - Footstep sounds and effects
 * - Attack hit frames for damage
 * - Projectile spawn points
 * - VFX spawn points
 * - Equipment visibility toggles
 */
class UnitEventHandler {
public:
    UnitEventHandler();
    ~UnitEventHandler();

    UnitEventHandler(const UnitEventHandler&) = delete;
    UnitEventHandler& operator=(const UnitEventHandler&) = delete;
    UnitEventHandler(UnitEventHandler&&) noexcept = default;
    UnitEventHandler& operator=(UnitEventHandler&&) noexcept = default;

    /**
     * @brief Initialize with event system
     */
    void Initialize(AnimationEventSystem* eventSystem);

    /**
     * @brief Load event bindings from config
     */
    bool LoadBindings(const std::string& configPath);
    bool LoadBindingsFromJson(const json& config);

    /**
     * @brief Save bindings to file
     */
    bool SaveBindings(const std::string& configPath) const;

    /**
     * @brief Export bindings to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Set callbacks for handling events
     */
    void SetCallbacks(const UnitEventCallbacks& callbacks);

    /**
     * @brief Set unit transform for position calculations
     */
    void SetUnitTransform(const glm::mat4& transform);

    /**
     * @brief Set bone positions for attachment points
     */
    void SetBonePositions(const std::unordered_map<std::string, glm::mat4>& boneTransforms);

    /**
     * @brief Set current surface type for footsteps
     */
    void SetSurfaceType(const std::string& surfaceType);

    // -------------------------------------------------------------------------
    // Event Bindings
    // -------------------------------------------------------------------------

    /**
     * @brief Add event binding
     */
    void AddBinding(const EventBinding& binding);

    /**
     * @brief Remove binding by event name
     */
    bool RemoveBinding(const std::string& eventName);

    /**
     * @brief Get binding by event name
     */
    [[nodiscard]] EventBinding* GetBinding(const std::string& eventName);

    /**
     * @brief Get all bindings
     */
    [[nodiscard]] const std::vector<EventBinding>& GetBindings() const { return m_bindings; }

    /**
     * @brief Enable/disable binding
     */
    void SetBindingEnabled(const std::string& eventName, bool enabled);

    // -------------------------------------------------------------------------
    // Sound Mappings
    // -------------------------------------------------------------------------

    /**
     * @brief Set footstep sound for surface type
     */
    void SetFootstepSound(const std::string& surfaceType, const std::string& soundId);

    /**
     * @brief Get footstep sound for current surface
     */
    [[nodiscard]] std::string GetFootstepSound(const std::string& surfaceType) const;

    // -------------------------------------------------------------------------
    // VFX Mappings
    // -------------------------------------------------------------------------

    /**
     * @brief Set VFX for event type
     */
    void SetEventVFX(const std::string& eventType, const std::string& vfxId);

    /**
     * @brief Get VFX for event type
     */
    [[nodiscard]] std::string GetEventVFX(const std::string& eventType) const;

    // -------------------------------------------------------------------------
    // Debug
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable debug logging
     */
    void SetDebugLogging(bool enabled) { m_debugLogging = enabled; }

    /**
     * @brief Get last processed events
     */
    [[nodiscard]] const std::vector<std::string>& GetLastEvents() const { return m_lastEvents; }

private:
    void OnAnimationEvent(AnimationEventData& event);
    void HandleFootstepEvent(const json& data);
    void HandleHitFrameEvent(const json& data);
    void HandleProjectileSpawnEvent(const json& data);
    void HandleVFXSpawnEvent(const json& data);
    void HandleSoundEvent(const json& data);
    void HandleEquipmentVisibilityEvent(const json& data);
    void HandleCustomEvent(const std::string& eventName, const json& data);

    [[nodiscard]] glm::vec3 GetBonePosition(const std::string& boneName) const;
    [[nodiscard]] glm::mat4 GetBoneTransform(const std::string& boneName) const;

    AnimationEventSystem* m_eventSystem = nullptr;
    std::string m_eventHandlerId;

    UnitEventCallbacks m_callbacks;
    std::vector<EventBinding> m_bindings;

    // Mappings
    std::unordered_map<std::string, std::string> m_footstepSounds;
    std::unordered_map<std::string, std::string> m_eventVFX;

    // State
    glm::mat4 m_unitTransform{1.0f};
    std::unordered_map<std::string, glm::mat4> m_boneTransforms;
    std::string m_currentSurfaceType = "default";

    // Debug
    bool m_debugLogging = false;
    std::vector<std::string> m_lastEvents;
    static constexpr size_t MAX_LAST_EVENTS = 20;
};

/**
 * @brief Factory for creating default event handlers
 */
namespace UnitEventHandlerFactory {
    /**
     * @brief Create humanoid event handler with standard bindings
     */
    [[nodiscard]] std::unique_ptr<UnitEventHandler> CreateHumanoid(AnimationEventSystem* eventSystem);

    /**
     * @brief Create creature event handler
     */
    [[nodiscard]] std::unique_ptr<UnitEventHandler> CreateCreature(AnimationEventSystem* eventSystem);

    /**
     * @brief Create vehicle event handler
     */
    [[nodiscard]] std::unique_ptr<UnitEventHandler> CreateVehicle(AnimationEventSystem* eventSystem);
}

} // namespace Vehement
