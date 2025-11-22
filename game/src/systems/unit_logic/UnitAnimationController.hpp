#pragma once

#include "engine/animation/AnimationStateMachine.hpp"
#include "engine/animation/AnimationBlendTree.hpp"
#include "engine/animation/AnimationEventSystem.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Vehement {

using Nova::json;
using Nova::DataDrivenStateMachine;
using Nova::BlendTree;
using Nova::AnimationEventSystem;

/**
 * @brief Movement state for locomotion blending
 */
enum class MovementState {
    Idle,
    Walking,
    Running,
    Sprinting,
    Crouching,
    CrouchWalking,
    Jumping,
    Falling,
    Landing,
    Swimming,
    Climbing
};

/**
 * @brief Combat state for combat animations
 */
enum class CombatState {
    None,
    Attacking,
    Blocking,
    Dodging,
    Hurt,
    Stunned,
    KnockedDown,
    GettingUp,
    Dying,
    Dead
};

/**
 * @brief Ability casting state
 */
enum class CastingState {
    None,
    Channeling,
    Casting,
    Recovering
};

/**
 * @brief Mount/vehicle state
 */
enum class MountState {
    Unmounted,
    Mounting,
    Mounted,
    Dismounting
};

/**
 * @brief Upper body action for partial body animations
 */
enum class UpperBodyAction {
    None,
    Aiming,
    Shooting,
    Reloading,
    Throwing,
    Using,
    Waving,
    Pointing
};

/**
 * @brief Unit animation controller configuration
 */
struct UnitAnimationConfig {
    std::string stateMachineConfig;     // Path to state machine JSON
    std::string locomotionBlendTree;    // Path to locomotion blend tree
    std::string combatConfig;           // Path to combat config
    std::string abilityConfig;          // Path to ability config

    // Animation clip mappings
    std::unordered_map<std::string, std::string> clipMappings;

    // Blend settings
    float locomotionBlendSpeed = 5.0f;
    float combatBlendSpeed = 8.0f;
    float transitionBlendTime = 0.2f;

    // Mask IDs
    std::string upperBodyMask;
    std::string lowerBodyMask;
    std::string fullBodyMask;

    [[nodiscard]] json ToJson() const;
    static UnitAnimationConfig FromJson(const json& j);
};

/**
 * @brief Unit animation controller
 *
 * Manages animation state machines, blend trees, and events for game units.
 * Supports:
 * - Movement state (idle, walk, run, sprint)
 * - Combat state (attack, block, dodge, hurt, death)
 * - Ability casting states
 * - Mounted/vehicle states
 * - Blending between movement and upper body actions
 */
class UnitAnimationController {
public:
    UnitAnimationController();
    ~UnitAnimationController();

    UnitAnimationController(const UnitAnimationController&) = delete;
    UnitAnimationController& operator=(const UnitAnimationController&) = delete;
    UnitAnimationController(UnitAnimationController&&) noexcept = default;
    UnitAnimationController& operator=(UnitAnimationController&&) noexcept = default;

    /**
     * @brief Initialize with configuration
     */
    bool Initialize(const UnitAnimationConfig& config);
    bool Initialize(const std::string& configPath);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update animations
     */
    void Update(float deltaTime);

    // -------------------------------------------------------------------------
    // Movement Control
    // -------------------------------------------------------------------------

    /**
     * @brief Set movement velocity for locomotion blending
     */
    void SetVelocity(const glm::vec3& velocity);

    /**
     * @brief Set movement speed (0-1 normalized)
     */
    void SetMovementSpeed(float speed);

    /**
     * @brief Set movement direction (local space)
     */
    void SetMovementDirection(const glm::vec2& direction);

    /**
     * @brief Set grounded state
     */
    void SetGrounded(bool grounded);

    /**
     * @brief Trigger jump
     */
    void TriggerJump();

    /**
     * @brief Trigger land
     */
    void TriggerLand(float impactVelocity = 0.0f);

    /**
     * @brief Set crouch state
     */
    void SetCrouching(bool crouching);

    /**
     * @brief Set sprint state
     */
    void SetSprinting(bool sprinting);

    // -------------------------------------------------------------------------
    // Combat Control
    // -------------------------------------------------------------------------

    /**
     * @brief Trigger attack animation
     * @param attackIndex Attack variation index
     */
    void TriggerAttack(int attackIndex = 0);

    /**
     * @brief Set blocking state
     */
    void SetBlocking(bool blocking);

    /**
     * @brief Trigger dodge in direction
     */
    void TriggerDodge(const glm::vec2& direction);

    /**
     * @brief Trigger hurt reaction
     * @param direction Direction of incoming damage
     * @param intensity Reaction intensity (0-1)
     */
    void TriggerHurt(const glm::vec3& direction, float intensity = 0.5f);

    /**
     * @brief Trigger stun
     */
    void TriggerStun(float duration);

    /**
     * @brief Trigger knockdown
     */
    void TriggerKnockdown();

    /**
     * @brief Trigger death
     * @param deathType Type of death animation
     */
    void TriggerDeath(int deathType = 0);

    /**
     * @brief Trigger resurrection/revive
     */
    void TriggerRevive();

    // -------------------------------------------------------------------------
    // Ability Casting
    // -------------------------------------------------------------------------

    /**
     * @brief Begin ability cast
     */
    void BeginCast(const std::string& abilityId);

    /**
     * @brief Complete ability cast
     */
    void CompleteCast();

    /**
     * @brief Cancel ability cast
     */
    void CancelCast();

    /**
     * @brief Begin channeling
     */
    void BeginChannel(const std::string& abilityId);

    /**
     * @brief End channeling
     */
    void EndChannel();

    // -------------------------------------------------------------------------
    // Mount/Vehicle
    // -------------------------------------------------------------------------

    /**
     * @brief Begin mounting
     */
    void BeginMount(const std::string& mountType);

    /**
     * @brief Complete mounting
     */
    void CompleteMount();

    /**
     * @brief Begin dismounting
     */
    void BeginDismount();

    /**
     * @brief Complete dismounting
     */
    void CompleteDismount();

    // -------------------------------------------------------------------------
    // Upper Body Actions
    // -------------------------------------------------------------------------

    /**
     * @brief Set upper body action
     */
    void SetUpperBodyAction(UpperBodyAction action);

    /**
     * @brief Set aim direction for aiming animations
     */
    void SetAimDirection(const glm::vec3& direction);

    /**
     * @brief Trigger upper body animation (additive)
     */
    void TriggerUpperBodyAnimation(const std::string& animName);

    // -------------------------------------------------------------------------
    // State Queries
    // -------------------------------------------------------------------------

    [[nodiscard]] MovementState GetMovementState() const { return m_movementState; }
    [[nodiscard]] CombatState GetCombatState() const { return m_combatState; }
    [[nodiscard]] CastingState GetCastingState() const { return m_castingState; }
    [[nodiscard]] MountState GetMountState() const { return m_mountState; }
    [[nodiscard]] UpperBodyAction GetUpperBodyAction() const { return m_upperBodyAction; }

    [[nodiscard]] bool IsInCombat() const { return m_combatState != CombatState::None; }
    [[nodiscard]] bool IsCasting() const { return m_castingState != CastingState::None; }
    [[nodiscard]] bool IsMounted() const { return m_mountState == MountState::Mounted; }
    [[nodiscard]] bool IsDead() const { return m_combatState == CombatState::Dead; }
    [[nodiscard]] bool CanMove() const;
    [[nodiscard]] bool CanAttack() const;
    [[nodiscard]] bool CanCast() const;

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------

    /**
     * @brief Set event system for receiving animation events
     */
    void SetEventSystem(AnimationEventSystem* eventSystem);

    /**
     * @brief Get animation event system
     */
    [[nodiscard]] AnimationEventSystem* GetEventSystem() { return m_eventSystem; }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Reload configuration
     */
    bool ReloadConfig();

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const UnitAnimationConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get state machine
     */
    [[nodiscard]] DataDrivenStateMachine* GetStateMachine() { return m_stateMachine.get(); }

    /**
     * @brief Get locomotion blend tree
     */
    [[nodiscard]] BlendTree* GetLocomotionBlendTree() { return m_locomotionBlendTree.get(); }

    /**
     * @brief Get debug info
     */
    [[nodiscard]] json GetDebugInfo() const;

private:
    void UpdateMovementState();
    void UpdateCombatState();
    void UpdateLocomotionBlending(float deltaTime);
    void SyncStateMachineParameters();
    void OnAnimationEvent(Nova::AnimationEventData& event);

    UnitAnimationConfig m_config;
    std::string m_configPath;

    // State machines
    std::unique_ptr<DataDrivenStateMachine> m_stateMachine;
    std::unique_ptr<DataDrivenStateMachine> m_combatStateMachine;
    std::unique_ptr<DataDrivenStateMachine> m_abilityStateMachine;

    // Blend trees
    std::unique_ptr<BlendTree> m_locomotionBlendTree;
    std::unique_ptr<BlendTree> m_directionalBlendTree;

    // Event system
    AnimationEventSystem* m_eventSystem = nullptr;

    // Current states
    MovementState m_movementState = MovementState::Idle;
    CombatState m_combatState = CombatState::None;
    CastingState m_castingState = CastingState::None;
    MountState m_mountState = MountState::Unmounted;
    UpperBodyAction m_upperBodyAction = UpperBodyAction::None;

    // Movement parameters
    glm::vec3 m_velocity{0.0f};
    glm::vec2 m_movementDirection{0.0f, 1.0f};
    float m_movementSpeed = 0.0f;
    bool m_grounded = true;
    bool m_crouching = false;
    bool m_sprinting = false;

    // Combat parameters
    bool m_blocking = false;
    float m_stunTimer = 0.0f;

    // Aim parameters
    glm::vec3 m_aimDirection{0.0f, 0.0f, 1.0f};

    // Blend weights
    float m_locomotionWeight = 1.0f;
    float m_combatWeight = 0.0f;
    float m_upperBodyWeight = 0.0f;

    bool m_initialized = false;
};

} // namespace Vehement
