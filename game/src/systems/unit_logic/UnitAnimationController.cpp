#include "UnitAnimationController.hpp"
#include <fstream>
#include <cmath>

namespace Vehement {

// ============================================================================
// UnitAnimationConfig
// ============================================================================

json UnitAnimationConfig::ToJson() const {
    json j = {
        {"stateMachineConfig", stateMachineConfig},
        {"locomotionBlendTree", locomotionBlendTree},
        {"combatConfig", combatConfig},
        {"abilityConfig", abilityConfig},
        {"locomotionBlendSpeed", locomotionBlendSpeed},
        {"combatBlendSpeed", combatBlendSpeed},
        {"transitionBlendTime", transitionBlendTime},
        {"upperBodyMask", upperBodyMask},
        {"lowerBodyMask", lowerBodyMask},
        {"fullBodyMask", fullBodyMask}
    };

    if (!clipMappings.empty()) {
        j["clipMappings"] = clipMappings;
    }

    return j;
}

UnitAnimationConfig UnitAnimationConfig::FromJson(const json& j) {
    UnitAnimationConfig config;
    config.stateMachineConfig = j.value("stateMachineConfig", "");
    config.locomotionBlendTree = j.value("locomotionBlendTree", "");
    config.combatConfig = j.value("combatConfig", "");
    config.abilityConfig = j.value("abilityConfig", "");
    config.locomotionBlendSpeed = j.value("locomotionBlendSpeed", 5.0f);
    config.combatBlendSpeed = j.value("combatBlendSpeed", 8.0f);
    config.transitionBlendTime = j.value("transitionBlendTime", 0.2f);
    config.upperBodyMask = j.value("upperBodyMask", "");
    config.lowerBodyMask = j.value("lowerBodyMask", "");
    config.fullBodyMask = j.value("fullBodyMask", "");

    if (j.contains("clipMappings") && j["clipMappings"].is_object()) {
        config.clipMappings = j["clipMappings"].get<std::unordered_map<std::string, std::string>>();
    }

    return config;
}

// ============================================================================
// UnitAnimationController
// ============================================================================

UnitAnimationController::UnitAnimationController() = default;
UnitAnimationController::~UnitAnimationController() = default;

bool UnitAnimationController::Initialize(const UnitAnimationConfig& config) {
    m_config = config;

    // Create and initialize state machine
    m_stateMachine = std::make_unique<DataDrivenStateMachine>();
    if (!config.stateMachineConfig.empty()) {
        if (!m_stateMachine->LoadFromFile(config.stateMachineConfig)) {
            return false;
        }
    }

    // Create locomotion blend tree
    m_locomotionBlendTree = std::make_unique<BlendTree>("locomotion");
    if (!config.locomotionBlendTree.empty()) {
        std::ifstream file(config.locomotionBlendTree);
        if (file.is_open()) {
            json blendConfig;
            file >> blendConfig;
            m_locomotionBlendTree->LoadFromJson(blendConfig);
        }
    }

    // Create combat state machine
    if (!config.combatConfig.empty()) {
        m_combatStateMachine = std::make_unique<DataDrivenStateMachine>();
        m_combatStateMachine->LoadFromFile(config.combatConfig);
    }

    // Create ability state machine
    if (!config.abilityConfig.empty()) {
        m_abilityStateMachine = std::make_unique<DataDrivenStateMachine>();
        m_abilityStateMachine->LoadFromFile(config.abilityConfig);
    }

    // Set up event handling
    if (m_eventSystem) {
        m_stateMachine->SetEventSystem(m_eventSystem);
        if (m_combatStateMachine) {
            m_combatStateMachine->SetEventSystem(m_eventSystem);
        }
        if (m_abilityStateMachine) {
            m_abilityStateMachine->SetEventSystem(m_eventSystem);
        }
    }

    // Start state machines
    m_stateMachine->Start();
    if (m_combatStateMachine) {
        m_combatStateMachine->Start();
    }

    m_initialized = true;
    return true;
}

bool UnitAnimationController::Initialize(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }

        json config;
        file >> config;

        m_configPath = configPath;
        return Initialize(UnitAnimationConfig::FromJson(config));
    } catch (...) {
        return false;
    }
}

void UnitAnimationController::Shutdown() {
    m_stateMachine.reset();
    m_combatStateMachine.reset();
    m_abilityStateMachine.reset();
    m_locomotionBlendTree.reset();
    m_directionalBlendTree.reset();
    m_initialized = false;
}

void UnitAnimationController::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update stun timer
    if (m_stunTimer > 0.0f) {
        m_stunTimer -= deltaTime;
        if (m_stunTimer <= 0.0f) {
            m_combatState = CombatState::None;
        }
    }

    // Update movement state
    UpdateMovementState();

    // Update combat state
    UpdateCombatState();

    // Sync parameters to state machine
    SyncStateMachineParameters();

    // Update state machines
    m_stateMachine->Update(deltaTime);
    if (m_combatStateMachine) {
        m_combatStateMachine->Update(deltaTime);
    }
    if (m_abilityStateMachine) {
        m_abilityStateMachine->Update(deltaTime);
    }

    // Update locomotion blending
    UpdateLocomotionBlending(deltaTime);
}

void UnitAnimationController::SetVelocity(const glm::vec3& velocity) {
    m_velocity = velocity;
    m_movementSpeed = glm::length(glm::vec2(velocity.x, velocity.z));

    if (m_movementSpeed > 0.01f) {
        m_movementDirection = glm::normalize(glm::vec2(velocity.x, velocity.z));
    }
}

void UnitAnimationController::SetMovementSpeed(float speed) {
    m_movementSpeed = speed;
}

void UnitAnimationController::SetMovementDirection(const glm::vec2& direction) {
    m_movementDirection = direction;
}

void UnitAnimationController::SetGrounded(bool grounded) {
    m_grounded = grounded;
}

void UnitAnimationController::TriggerJump() {
    if (m_grounded && CanMove()) {
        m_stateMachine->SetTrigger("jump");
        m_movementState = MovementState::Jumping;
        m_grounded = false;
    }
}

void UnitAnimationController::TriggerLand(float impactVelocity) {
    if (!m_grounded) {
        m_grounded = true;
        m_stateMachine->SetTrigger("land");
        m_stateMachine->SetFloat("landImpact", impactVelocity);
        m_movementState = MovementState::Landing;
    }
}

void UnitAnimationController::SetCrouching(bool crouching) {
    m_crouching = crouching;
    m_stateMachine->SetBool("crouching", crouching);
}

void UnitAnimationController::SetSprinting(bool sprinting) {
    m_sprinting = sprinting;
    m_stateMachine->SetBool("sprinting", sprinting);
}

void UnitAnimationController::TriggerAttack(int attackIndex) {
    if (!CanAttack()) {
        return;
    }

    if (m_combatStateMachine) {
        m_combatStateMachine->SetInt("attackIndex", attackIndex);
        m_combatStateMachine->SetTrigger("attack");
    } else {
        m_stateMachine->SetInt("attackIndex", attackIndex);
        m_stateMachine->SetTrigger("attack");
    }

    m_combatState = CombatState::Attacking;
}

void UnitAnimationController::SetBlocking(bool blocking) {
    m_blocking = blocking;

    if (m_combatStateMachine) {
        m_combatStateMachine->SetBool("blocking", blocking);
    } else {
        m_stateMachine->SetBool("blocking", blocking);
    }

    if (blocking) {
        m_combatState = CombatState::Blocking;
    } else if (m_combatState == CombatState::Blocking) {
        m_combatState = CombatState::None;
    }
}

void UnitAnimationController::TriggerDodge(const glm::vec2& direction) {
    if (!CanMove()) {
        return;
    }

    if (m_combatStateMachine) {
        m_combatStateMachine->SetFloat("dodgeX", direction.x);
        m_combatStateMachine->SetFloat("dodgeY", direction.y);
        m_combatStateMachine->SetTrigger("dodge");
    } else {
        m_stateMachine->SetFloat("dodgeX", direction.x);
        m_stateMachine->SetFloat("dodgeY", direction.y);
        m_stateMachine->SetTrigger("dodge");
    }

    m_combatState = CombatState::Dodging;
}

void UnitAnimationController::TriggerHurt(const glm::vec3& direction, float intensity) {
    if (m_combatStateMachine) {
        m_combatStateMachine->SetFloat("hurtDirX", direction.x);
        m_combatStateMachine->SetFloat("hurtDirY", direction.y);
        m_combatStateMachine->SetFloat("hurtDirZ", direction.z);
        m_combatStateMachine->SetFloat("hurtIntensity", intensity);
        m_combatStateMachine->SetTrigger("hurt");
    } else {
        m_stateMachine->SetTrigger("hurt");
    }

    m_combatState = CombatState::Hurt;
}

void UnitAnimationController::TriggerStun(float duration) {
    m_stunTimer = duration;
    m_combatState = CombatState::Stunned;

    if (m_combatStateMachine) {
        m_combatStateMachine->SetTrigger("stun");
    } else {
        m_stateMachine->SetTrigger("stun");
    }
}

void UnitAnimationController::TriggerKnockdown() {
    m_combatState = CombatState::KnockedDown;

    if (m_combatStateMachine) {
        m_combatStateMachine->SetTrigger("knockdown");
    } else {
        m_stateMachine->SetTrigger("knockdown");
    }
}

void UnitAnimationController::TriggerDeath(int deathType) {
    m_combatState = CombatState::Dying;

    if (m_combatStateMachine) {
        m_combatStateMachine->SetInt("deathType", deathType);
        m_combatStateMachine->SetTrigger("death");
    } else {
        m_stateMachine->SetInt("deathType", deathType);
        m_stateMachine->SetTrigger("death");
    }
}

void UnitAnimationController::TriggerRevive() {
    if (m_combatState == CombatState::Dead || m_combatState == CombatState::Dying) {
        m_combatState = CombatState::GettingUp;

        if (m_combatStateMachine) {
            m_combatStateMachine->SetTrigger("revive");
        } else {
            m_stateMachine->SetTrigger("revive");
        }
    }
}

void UnitAnimationController::BeginCast(const std::string& abilityId) {
    if (!CanCast()) {
        return;
    }

    m_castingState = CastingState::Casting;

    if (m_abilityStateMachine) {
        m_abilityStateMachine->SetTrigger("beginCast");
    } else {
        m_stateMachine->SetTrigger("beginCast");
    }
}

void UnitAnimationController::CompleteCast() {
    m_castingState = CastingState::Recovering;

    if (m_abilityStateMachine) {
        m_abilityStateMachine->SetTrigger("completeCast");
    } else {
        m_stateMachine->SetTrigger("completeCast");
    }
}

void UnitAnimationController::CancelCast() {
    m_castingState = CastingState::None;

    if (m_abilityStateMachine) {
        m_abilityStateMachine->SetTrigger("cancelCast");
    } else {
        m_stateMachine->SetTrigger("cancelCast");
    }
}

void UnitAnimationController::BeginChannel(const std::string& abilityId) {
    if (!CanCast()) {
        return;
    }

    m_castingState = CastingState::Channeling;

    if (m_abilityStateMachine) {
        m_abilityStateMachine->SetTrigger("beginChannel");
    } else {
        m_stateMachine->SetTrigger("beginChannel");
    }
}

void UnitAnimationController::EndChannel() {
    m_castingState = CastingState::None;

    if (m_abilityStateMachine) {
        m_abilityStateMachine->SetTrigger("endChannel");
    } else {
        m_stateMachine->SetTrigger("endChannel");
    }
}

void UnitAnimationController::BeginMount(const std::string& mountType) {
    if (m_mountState != MountState::Unmounted) {
        return;
    }

    m_mountState = MountState::Mounting;
    m_stateMachine->SetTrigger("mount");
}

void UnitAnimationController::CompleteMount() {
    m_mountState = MountState::Mounted;
}

void UnitAnimationController::BeginDismount() {
    if (m_mountState != MountState::Mounted) {
        return;
    }

    m_mountState = MountState::Dismounting;
    m_stateMachine->SetTrigger("dismount");
}

void UnitAnimationController::CompleteDismount() {
    m_mountState = MountState::Unmounted;
}

void UnitAnimationController::SetUpperBodyAction(UpperBodyAction action) {
    m_upperBodyAction = action;

    std::string actionStr = "none";
    switch (action) {
        case UpperBodyAction::Aiming: actionStr = "aiming"; break;
        case UpperBodyAction::Shooting: actionStr = "shooting"; break;
        case UpperBodyAction::Reloading: actionStr = "reloading"; break;
        case UpperBodyAction::Throwing: actionStr = "throwing"; break;
        case UpperBodyAction::Using: actionStr = "using"; break;
        case UpperBodyAction::Waving: actionStr = "waving"; break;
        case UpperBodyAction::Pointing: actionStr = "pointing"; break;
        default: break;
    }

    m_stateMachine->SetBool("hasUpperBodyAction", action != UpperBodyAction::None);
}

void UnitAnimationController::SetAimDirection(const glm::vec3& direction) {
    m_aimDirection = direction;
    m_stateMachine->SetFloat("aimX", direction.x);
    m_stateMachine->SetFloat("aimY", direction.y);
    m_stateMachine->SetFloat("aimZ", direction.z);
}

void UnitAnimationController::TriggerUpperBodyAnimation(const std::string& animName) {
    // This would trigger an additive upper body animation
    m_stateMachine->SetTrigger("upperBody_" + animName);
}

bool UnitAnimationController::CanMove() const {
    if (m_combatState == CombatState::Dying || m_combatState == CombatState::Dead) {
        return false;
    }
    if (m_combatState == CombatState::Stunned || m_combatState == CombatState::KnockedDown) {
        return false;
    }
    if (m_mountState == MountState::Mounting || m_mountState == MountState::Dismounting) {
        return false;
    }
    return true;
}

bool UnitAnimationController::CanAttack() const {
    if (!CanMove()) {
        return false;
    }
    if (m_combatState == CombatState::Attacking || m_combatState == CombatState::Dodging) {
        return false;
    }
    if (m_castingState != CastingState::None) {
        return false;
    }
    return true;
}

bool UnitAnimationController::CanCast() const {
    if (!CanMove()) {
        return false;
    }
    if (m_combatState == CombatState::Attacking) {
        return false;
    }
    return true;
}

void UnitAnimationController::SetEventSystem(AnimationEventSystem* eventSystem) {
    m_eventSystem = eventSystem;

    if (m_stateMachine) {
        m_stateMachine->SetEventSystem(eventSystem);
    }
    if (m_combatStateMachine) {
        m_combatStateMachine->SetEventSystem(eventSystem);
    }
    if (m_abilityStateMachine) {
        m_abilityStateMachine->SetEventSystem(eventSystem);
    }
}

bool UnitAnimationController::ReloadConfig() {
    if (m_configPath.empty()) {
        return false;
    }
    return Initialize(m_configPath);
}

json UnitAnimationController::GetDebugInfo() const {
    json info;
    info["movementState"] = static_cast<int>(m_movementState);
    info["combatState"] = static_cast<int>(m_combatState);
    info["castingState"] = static_cast<int>(m_castingState);
    info["mountState"] = static_cast<int>(m_mountState);
    info["upperBodyAction"] = static_cast<int>(m_upperBodyAction);
    info["velocity"] = {m_velocity.x, m_velocity.y, m_velocity.z};
    info["movementSpeed"] = m_movementSpeed;
    info["grounded"] = m_grounded;
    info["crouching"] = m_crouching;
    info["sprinting"] = m_sprinting;
    info["blocking"] = m_blocking;
    info["stunTimer"] = m_stunTimer;

    if (m_stateMachine) {
        info["stateMachine"] = m_stateMachine->GetDebugInfo();
    }

    return info;
}

void UnitAnimationController::UpdateMovementState() {
    if (!m_grounded) {
        if (m_velocity.y > 0.0f) {
            m_movementState = MovementState::Jumping;
        } else {
            m_movementState = MovementState::Falling;
        }
        return;
    }

    if (m_movementSpeed < 0.1f) {
        m_movementState = m_crouching ? MovementState::Crouching : MovementState::Idle;
    } else if (m_crouching) {
        m_movementState = MovementState::CrouchWalking;
    } else if (m_sprinting && m_movementSpeed > 0.8f) {
        m_movementState = MovementState::Sprinting;
    } else if (m_movementSpeed > 0.5f) {
        m_movementState = MovementState::Running;
    } else {
        m_movementState = MovementState::Walking;
    }
}

void UnitAnimationController::UpdateCombatState() {
    // Combat state transitions based on state machine feedback
    if (m_combatStateMachine) {
        const std::string& currentState = m_combatStateMachine->GetCurrentState();

        if (currentState == "idle" || currentState == "none") {
            m_combatState = CombatState::None;
        } else if (currentState == "dead") {
            m_combatState = CombatState::Dead;
        } else if (currentState == "dying") {
            m_combatState = CombatState::Dying;
        }
    }
}

void UnitAnimationController::UpdateLocomotionBlending(float deltaTime) {
    // Update blend tree with current parameters
    if (m_locomotionBlendTree) {
        std::unordered_map<std::string, float> params;
        params["speed"] = m_movementSpeed;
        params["directionX"] = m_movementDirection.x;
        params["directionY"] = m_movementDirection.y;

        m_locomotionBlendTree->Update(params, deltaTime);
    }

    // Blend weights between locomotion and combat
    float targetLocomotionWeight = (m_combatState == CombatState::None) ? 1.0f : 0.3f;
    float targetCombatWeight = (m_combatState != CombatState::None) ? 1.0f : 0.0f;

    m_locomotionWeight += (targetLocomotionWeight - m_locomotionWeight) *
                          m_config.locomotionBlendSpeed * deltaTime;
    m_combatWeight += (targetCombatWeight - m_combatWeight) *
                      m_config.combatBlendSpeed * deltaTime;

    // Upper body weight
    float targetUpperBodyWeight = (m_upperBodyAction != UpperBodyAction::None) ? 1.0f : 0.0f;
    m_upperBodyWeight += (targetUpperBodyWeight - m_upperBodyWeight) * 10.0f * deltaTime;
}

void UnitAnimationController::SyncStateMachineParameters() {
    m_stateMachine->SetFloat("speed", m_movementSpeed);
    m_stateMachine->SetFloat("directionX", m_movementDirection.x);
    m_stateMachine->SetFloat("directionY", m_movementDirection.y);
    m_stateMachine->SetFloat("velocityY", m_velocity.y);
    m_stateMachine->SetBool("grounded", m_grounded);
    m_stateMachine->SetBool("crouching", m_crouching);
    m_stateMachine->SetBool("sprinting", m_sprinting);
    m_stateMachine->SetBool("inCombat", m_combatState != CombatState::None);
    m_stateMachine->SetBool("mounted", m_mountState == MountState::Mounted);
}

void UnitAnimationController::OnAnimationEvent(Nova::AnimationEventData& event) {
    // Handle specific animation events
    if (event.eventName == "combat_state_change") {
        std::string newState = event.data.value("state", "none");
        if (newState == "none") m_combatState = CombatState::None;
        else if (newState == "dead") m_combatState = CombatState::Dead;
        // ... etc
    }
}

} // namespace Vehement
