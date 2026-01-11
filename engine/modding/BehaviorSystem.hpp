#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <any>

namespace Nova {

class EntityDefinition;

/**
 * @brief Context passed to behavior functions
 */
struct BehaviorContext {
    // Entity this behavior is attached to
    uint64_t entityId = 0;
    std::string entityDefId;

    // Delta time for update behaviors
    float deltaTime = 0.0f;

    // Event data for event handlers
    std::string eventType;
    std::unordered_map<std::string, std::any> eventData;

    // Target entity for interactions
    uint64_t targetEntityId = 0;

    // World position
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;

    // Custom parameters
    std::unordered_map<std::string, std::any> params;

    // Helper methods
    template<typename T>
    T GetParam(const std::string& key, const T& defaultVal = T{}) const {
        auto it = params.find(key);
        if (it != params.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (...) {}
        }
        return defaultVal;
    }

    template<typename T>
    void SetParam(const std::string& key, const T& value) {
        params[key] = value;
    }
};

/**
 * @brief Result from behavior execution
 */
struct BehaviorResult {
    bool success = true;
    std::string message;
    std::unordered_map<std::string, std::any> outputs;

    static BehaviorResult Success() { return {true, "", {}}; }
    static BehaviorResult Failure(const std::string& msg) { return {false, msg, {}}; }

    template<typename T>
    BehaviorResult& WithOutput(const std::string& key, const T& value) {
        outputs[key] = value;
        return *this;
    }
};

/**
 * @brief Behavior function signature
 */
using BehaviorFunction = std::function<BehaviorResult(BehaviorContext&)>;

/**
 * @brief Trigger conditions for behaviors
 */
enum class BehaviorTrigger {
    OnSpawn,
    OnDeath,
    OnDamaged,
    OnHealed,
    OnAttack,
    OnKill,
    OnAbilityUse,
    OnUpdate,          // Every frame
    OnFixedUpdate,     // Fixed timestep
    OnInteract,
    OnCollision,
    OnEnterArea,
    OnExitArea,
    OnStateChange,
    OnTimer,
    Custom
};

/**
 * @brief Behavior definition
 */
struct BehaviorDef {
    std::string id;
    std::string name;
    std::string description;
    std::string category;

    // When this behavior triggers
    std::vector<BehaviorTrigger> triggers;
    std::string customTrigger;  // For BehaviorTrigger::Custom

    // Execution
    BehaviorFunction function;
    std::string scriptPath;  // For Python-defined behaviors

    // Parameters this behavior accepts
    struct Parameter {
        std::string id;
        std::string name;
        std::string type;  // int, float, bool, string, entity, position
        std::any defaultValue;
    };
    std::vector<Parameter> parameters;

    // Balance
    float pointCost = 0.0f;

    // Requirements
    std::vector<std::string> requiredTags;
    std::vector<std::string> incompatibleBehaviors;

    // Metadata for modding
    std::string author;
    std::string version;
    std::vector<std::string> tags;
};

/**
 * @brief Instance of a behavior attached to an entity
 */
class BehaviorInstance {
public:
    BehaviorInstance(const std::string& behaviorId, uint64_t entityId);

    [[nodiscard]] const std::string& GetBehaviorId() const { return m_behaviorId; }
    [[nodiscard]] uint64_t GetEntityId() const { return m_entityId; }

    // Parameters
    void SetParameter(const std::string& id, const std::any& value);
    [[nodiscard]] std::any GetParameter(const std::string& id) const;

    template<typename T>
    T GetParam(const std::string& id, const T& defaultVal = T{}) const {
        auto it = m_parameters.find(id);
        if (it != m_parameters.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (...) {}
        }
        return defaultVal;
    }

    // State
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    // Execution
    BehaviorResult Execute(BehaviorContext& context);

    // Persistent state
    void SetState(const std::string& key, const std::any& value);
    [[nodiscard]] std::any GetState(const std::string& key) const;

private:
    std::string m_behaviorId;
    uint64_t m_entityId;
    bool m_enabled = true;
    std::unordered_map<std::string, std::any> m_parameters;
    std::unordered_map<std::string, std::any> m_state;
};

/**
 * @brief Behavior registry and execution system
 */
class BehaviorSystem {
public:
    static BehaviorSystem& Instance();

    // =========================================================================
    // Behavior Registration
    // =========================================================================

    /**
     * @brief Register a behavior definition
     */
    void RegisterBehavior(const BehaviorDef& behavior);

    /**
     * @brief Register behavior with function
     */
    void RegisterBehavior(const std::string& id, const std::string& name,
                          const std::vector<BehaviorTrigger>& triggers,
                          BehaviorFunction function,
                          float pointCost = 0.0f);

    /**
     * @brief Unregister behavior
     */
    void UnregisterBehavior(const std::string& id);

    /**
     * @brief Get behavior definition
     */
    [[nodiscard]] const BehaviorDef* GetBehavior(const std::string& id) const;

    /**
     * @brief Get all behaviors
     */
    [[nodiscard]] std::vector<const BehaviorDef*> GetAllBehaviors() const;

    /**
     * @brief Get behaviors by trigger
     */
    [[nodiscard]] std::vector<const BehaviorDef*> GetBehaviorsByTrigger(BehaviorTrigger trigger) const;

    /**
     * @brief Get behaviors by category
     */
    [[nodiscard]] std::vector<const BehaviorDef*> GetBehaviorsByCategory(const std::string& category) const;

    // =========================================================================
    // Entity Behavior Management
    // =========================================================================

    /**
     * @brief Attach behavior to entity
     */
    std::shared_ptr<BehaviorInstance> AttachBehavior(uint64_t entityId, const std::string& behaviorId);

    /**
     * @brief Detach behavior from entity
     */
    void DetachBehavior(uint64_t entityId, const std::string& behaviorId);

    /**
     * @brief Get behaviors attached to entity
     */
    [[nodiscard]] std::vector<std::shared_ptr<BehaviorInstance>> GetEntityBehaviors(uint64_t entityId) const;

    /**
     * @brief Clear all behaviors from entity
     */
    void ClearEntityBehaviors(uint64_t entityId);

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Trigger behaviors for event
     */
    void TriggerBehaviors(BehaviorTrigger trigger, BehaviorContext& context);

    /**
     * @brief Trigger behaviors for specific entity
     */
    void TriggerEntityBehaviors(uint64_t entityId, BehaviorTrigger trigger, BehaviorContext& context);

    /**
     * @brief Update all behaviors (call each frame)
     */
    void Update(float deltaTime);

    /**
     * @brief Fixed update (call each physics step)
     */
    void FixedUpdate(float fixedDeltaTime);

    // =========================================================================
    // Scripting Integration
    // =========================================================================

    /**
     * @brief Load behavior from Python script
     */
    bool LoadBehaviorFromScript(const std::string& path);

    /**
     * @brief Reload all script behaviors
     */
    void ReloadScriptBehaviors();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(uint64_t, const std::string&)> OnBehaviorAttached;
    std::function<void(uint64_t, const std::string&)> OnBehaviorDetached;
    std::function<void(const BehaviorResult&)> OnBehaviorExecuted;

private:
    BehaviorSystem() = default;

    std::unordered_map<std::string, BehaviorDef> m_behaviors;
    std::unordered_map<uint64_t, std::vector<std::shared_ptr<BehaviorInstance>>> m_entityBehaviors;
};

/**
 * @brief Helper to build behavior definitions
 */
class BehaviorBuilder {
public:
    explicit BehaviorBuilder(const std::string& id);

    BehaviorBuilder& Name(const std::string& name);
    BehaviorBuilder& Description(const std::string& desc);
    BehaviorBuilder& Category(const std::string& category);
    BehaviorBuilder& Trigger(BehaviorTrigger trigger);
    BehaviorBuilder& CustomTrigger(const std::string& trigger);
    BehaviorBuilder& Function(BehaviorFunction func);
    BehaviorBuilder& Script(const std::string& scriptPath);
    BehaviorBuilder& PointCost(float cost);
    BehaviorBuilder& RequiresTag(const std::string& tag);
    BehaviorBuilder& IncompatibleWith(const std::string& behaviorId);
    BehaviorBuilder& Author(const std::string& author);
    BehaviorBuilder& Version(const std::string& version);
    BehaviorBuilder& Tag(const std::string& tag);

    BehaviorBuilder& IntParam(const std::string& id, const std::string& name, int defaultVal = 0);
    BehaviorBuilder& FloatParam(const std::string& id, const std::string& name, float defaultVal = 0.0f);
    BehaviorBuilder& BoolParam(const std::string& id, const std::string& name, bool defaultVal = false);
    BehaviorBuilder& StringParam(const std::string& id, const std::string& name, const std::string& defaultVal = "");

    [[nodiscard]] BehaviorDef Build() const;
    void Register();

private:
    BehaviorDef m_behavior;
};

// ============================================================================
// Common Behavior Functions
// ============================================================================

namespace Behaviors {

/**
 * @brief Damage over time behavior
 */
BehaviorResult DamageOverTime(BehaviorContext& ctx);

/**
 * @brief Heal over time behavior
 */
BehaviorResult HealOverTime(BehaviorContext& ctx);

/**
 * @brief Move towards target behavior
 */
BehaviorResult MoveToTarget(BehaviorContext& ctx);

/**
 * @brief Flee from target behavior
 */
BehaviorResult FleeFromTarget(BehaviorContext& ctx);

/**
 * @brief Attack nearest enemy behavior
 */
BehaviorResult AttackNearest(BehaviorContext& ctx);

/**
 * @brief Patrol between waypoints behavior
 */
BehaviorResult Patrol(BehaviorContext& ctx);

/**
 * @brief Follow entity behavior
 */
BehaviorResult FollowEntity(BehaviorContext& ctx);

/**
 * @brief Spawn entity on death behavior
 */
BehaviorResult SpawnOnDeath(BehaviorContext& ctx);

/**
 * @brief Apply buff on attack behavior
 */
BehaviorResult ApplyBuffOnAttack(BehaviorContext& ctx);

/**
 * @brief Reflect damage behavior
 */
BehaviorResult ReflectDamage(BehaviorContext& ctx);

/**
 * @brief Aura effect behavior
 */
BehaviorResult AuraEffect(BehaviorContext& ctx);

/**
 * @brief Resource generation behavior
 */
BehaviorResult GenerateResource(BehaviorContext& ctx);

/**
 * @brief Register all built-in behaviors
 */
void RegisterBuiltinBehaviors();

} // namespace Behaviors

} // namespace Nova
