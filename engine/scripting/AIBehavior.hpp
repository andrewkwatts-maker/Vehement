#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>
#include <any>
#include <optional>
#include <glm/glm.hpp>

namespace Vehement {
    class Entity;
    class EntityManager;
}

namespace Nova {
namespace Scripting {

// Forward declarations
class PythonEngine;
class ScriptContext;

// ============================================================================
// Behavior Tree Types
// ============================================================================

/**
 * @brief Status returned by behavior tree nodes
 */
enum class BehaviorStatus {
    Running,   // Node is still executing
    Success,   // Node completed successfully
    Failure,   // Node failed
    Invalid    // Node not properly initialized
};

/**
 * @brief Convert status to string
 */
inline const char* BehaviorStatusToString(BehaviorStatus status) {
    switch (status) {
        case BehaviorStatus::Running: return "Running";
        case BehaviorStatus::Success: return "Success";
        case BehaviorStatus::Failure: return "Failure";
        case BehaviorStatus::Invalid: return "Invalid";
        default: return "Unknown";
    }
}

// ============================================================================
// Blackboard - Shared AI State
// ============================================================================

/**
 * @brief Blackboard value types
 */
using BlackboardValue = std::variant<
    std::monostate,
    bool,
    int,
    float,
    double,
    std::string,
    glm::vec3,
    uint32_t  // Entity ID
>;

/**
 * @brief Blackboard for sharing state between AI behaviors
 *
 * The blackboard pattern allows behavior tree nodes and other AI components
 * to share data without tight coupling.
 */
class Blackboard {
public:
    Blackboard() = default;
    ~Blackboard() = default;

    // Value access
    void Set(const std::string& key, const BlackboardValue& value);

    template<typename T>
    void SetValue(const std::string& key, const T& value) {
        Set(key, BlackboardValue(value));
    }

    [[nodiscard]] std::optional<BlackboardValue> Get(const std::string& key) const;

    template<typename T>
    [[nodiscard]] std::optional<T> GetValue(const std::string& key) const {
        auto val = Get(key);
        if (val) {
            if (auto* ptr = std::get_if<T>(&*val)) {
                return *ptr;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool Has(const std::string& key) const;
    void Remove(const std::string& key);
    void Clear();

    // Convenience accessors
    [[nodiscard]] std::optional<uint32_t> GetTargetEntity() const {
        return GetValue<uint32_t>("target_entity");
    }
    void SetTargetEntity(uint32_t entityId) {
        SetValue("target_entity", entityId);
    }

    [[nodiscard]] std::optional<glm::vec3> GetTargetPosition() const {
        return GetValue<glm::vec3>("target_position");
    }
    void SetTargetPosition(const glm::vec3& pos) {
        SetValue("target_position", pos);
    }

    [[nodiscard]] std::vector<std::string> GetKeys() const;

private:
    std::unordered_map<std::string, BlackboardValue> m_data;
};

// ============================================================================
// Behavior Tree Node Base
// ============================================================================

/**
 * @brief Base class for behavior tree nodes
 */
class BehaviorNode {
public:
    BehaviorNode() = default;
    explicit BehaviorNode(const std::string& name) : m_name(name) {}
    virtual ~BehaviorNode() = default;

    /**
     * @brief Initialize the node
     */
    virtual void Initialize() {}

    /**
     * @brief Execute the node
     * @param entityId Entity running this behavior
     * @param blackboard Shared state
     * @param deltaTime Time since last update
     * @return Execution status
     */
    virtual BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) = 0;

    /**
     * @brief Called when node is aborted
     */
    virtual void Abort() {}

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

protected:
    std::string m_name;
};

// ============================================================================
// Python Behavior Node
// ============================================================================

/**
 * @brief Behavior tree node implemented in Python
 *
 * This node calls a Python function for its execution logic.
 * The Python function should return one of: "running", "success", "failure"
 */
class PythonBehaviorNode : public BehaviorNode {
public:
    PythonBehaviorNode(const std::string& name,
                       const std::string& pythonModule,
                       const std::string& pythonFunction);

    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;

    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }
    void SetContext(ScriptContext* context) { m_context = context; }

private:
    std::string m_pythonModule;
    std::string m_pythonFunction;
    PythonEngine* m_pythonEngine = nullptr;
    ScriptContext* m_context = nullptr;
};

// ============================================================================
// Composite Nodes
// ============================================================================

/**
 * @brief Sequence node - runs children in order until one fails
 */
class SequenceNode : public BehaviorNode {
public:
    explicit SequenceNode(const std::string& name = "Sequence");

    void AddChild(std::unique_ptr<BehaviorNode> child);
    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;
    void Abort() override;

private:
    std::vector<std::unique_ptr<BehaviorNode>> m_children;
    size_t m_currentChild = 0;
};

/**
 * @brief Selector node - runs children until one succeeds
 */
class SelectorNode : public BehaviorNode {
public:
    explicit SelectorNode(const std::string& name = "Selector");

    void AddChild(std::unique_ptr<BehaviorNode> child);
    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;
    void Abort() override;

private:
    std::vector<std::unique_ptr<BehaviorNode>> m_children;
    size_t m_currentChild = 0;
};

/**
 * @brief Parallel node - runs all children simultaneously
 */
class ParallelNode : public BehaviorNode {
public:
    enum class Policy {
        RequireOne,   // Success if any child succeeds
        RequireAll    // Success only if all children succeed
    };

    explicit ParallelNode(const std::string& name = "Parallel", Policy policy = Policy::RequireAll);

    void AddChild(std::unique_ptr<BehaviorNode> child);
    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;

private:
    std::vector<std::unique_ptr<BehaviorNode>> m_children;
    Policy m_policy;
};

// ============================================================================
// Decorator Nodes
// ============================================================================

/**
 * @brief Inverter - inverts child result
 */
class InverterNode : public BehaviorNode {
public:
    explicit InverterNode(std::unique_ptr<BehaviorNode> child);
    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;

private:
    std::unique_ptr<BehaviorNode> m_child;
};

/**
 * @brief Repeater - repeats child N times or infinitely
 */
class RepeaterNode : public BehaviorNode {
public:
    explicit RepeaterNode(std::unique_ptr<BehaviorNode> child, int repeatCount = -1);
    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;

private:
    std::unique_ptr<BehaviorNode> m_child;
    int m_repeatCount;
    int m_currentCount = 0;
};

/**
 * @brief Succeeder - always returns success
 */
class SucceederNode : public BehaviorNode {
public:
    explicit SucceederNode(std::unique_ptr<BehaviorNode> child);
    BehaviorStatus Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) override;

private:
    std::unique_ptr<BehaviorNode> m_child;
};

// ============================================================================
// State Machine
// ============================================================================

/**
 * @brief State in a finite state machine
 */
struct AIState {
    std::string name;
    std::string pythonModule;
    std::string enterFunction;     // Called when entering state
    std::string updateFunction;    // Called each tick
    std::string exitFunction;      // Called when leaving state

    // C++ callbacks (alternative to Python)
    std::function<void(uint32_t, Blackboard&)> onEnter;
    std::function<void(uint32_t, Blackboard&, float)> onUpdate;
    std::function<void(uint32_t, Blackboard&)> onExit;

    bool usePython = false;
};

/**
 * @brief Transition between states
 */
struct StateTransition {
    std::string fromState;
    std::string toState;
    std::string conditionModule;
    std::string conditionFunction;

    // C++ condition (alternative to Python)
    std::function<bool(uint32_t, const Blackboard&)> condition;

    bool usePython = false;
    int priority = 0;
};

/**
 * @brief Finite State Machine for AI
 */
class AIStateMachine {
public:
    AIStateMachine();
    ~AIStateMachine();

    // State management
    void AddState(const AIState& state);
    void RemoveState(const std::string& name);
    void SetInitialState(const std::string& name);
    [[nodiscard]] const std::string& GetCurrentState() const { return m_currentState; }

    // Transitions
    void AddTransition(const StateTransition& transition);
    void ForceTransition(const std::string& toState);

    // Update
    void Update(uint32_t entityId, Blackboard& blackboard, float deltaTime);

    // Python integration
    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }
    void SetContext(ScriptContext* context) { m_context = context; }

    // Queries
    [[nodiscard]] bool HasState(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> GetStateNames() const;

private:
    void EnterState(uint32_t entityId, Blackboard& blackboard, const std::string& stateName);
    void ExitState(uint32_t entityId, Blackboard& blackboard, const std::string& stateName);
    bool CheckTransition(uint32_t entityId, const Blackboard& blackboard,
                         const StateTransition& transition);

    std::unordered_map<std::string, AIState> m_states;
    std::vector<StateTransition> m_transitions;
    std::string m_currentState;
    std::string m_initialState;

    PythonEngine* m_pythonEngine = nullptr;
    ScriptContext* m_context = nullptr;
};

// ============================================================================
// Utility AI
// ============================================================================

/**
 * @brief Utility AI action with scoring
 */
struct UtilityAction {
    std::string name;

    // Python scoring function
    std::string pythonModule;
    std::string scoreFunction;   // Returns float 0-1

    // Python execute function
    std::string executeFunction;

    // C++ alternatives
    std::function<float(uint32_t, const Blackboard&)> scoreFn;
    std::function<void(uint32_t, Blackboard&, float)> executeFn;

    bool usePython = false;

    // Weight multiplier
    float weight = 1.0f;
};

/**
 * @brief Utility AI system for decision making
 *
 * Utility AI scores multiple actions and picks the best one.
 * Useful for complex AI that needs to weigh multiple factors.
 */
class UtilityAI {
public:
    UtilityAI();
    ~UtilityAI();

    // Action management
    void AddAction(const UtilityAction& action);
    void RemoveAction(const std::string& name);

    // Update - scores all actions and executes the best
    void Update(uint32_t entityId, Blackboard& blackboard, float deltaTime);

    // Get scores for debugging
    [[nodiscard]] std::vector<std::pair<std::string, float>> GetActionScores(
        uint32_t entityId, const Blackboard& blackboard) const;

    // Python integration
    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }
    void SetContext(ScriptContext* context) { m_context = context; }

    // Configuration
    void SetMinScoreThreshold(float threshold) { m_minThreshold = threshold; }
    void SetRandomness(float randomness) { m_randomness = randomness; }

private:
    float ScoreAction(uint32_t entityId, const Blackboard& blackboard,
                      const UtilityAction& action) const;
    void ExecuteAction(uint32_t entityId, Blackboard& blackboard, float deltaTime,
                       UtilityAction& action);

    std::vector<UtilityAction> m_actions;
    std::string m_currentAction;
    float m_minThreshold = 0.0f;
    float m_randomness = 0.0f;  // Add randomness to scores

    PythonEngine* m_pythonEngine = nullptr;
    ScriptContext* m_context = nullptr;
};

// ============================================================================
// Group/Squad AI Coordination
// ============================================================================

/**
 * @brief Squad member info
 */
struct SquadMember {
    uint32_t entityId;
    std::string role;  // "leader", "follower", "flanker", etc.
    glm::vec3 formationOffset{0.0f};
};

/**
 * @brief Squad coordination for group AI
 */
class SquadCoordinator {
public:
    SquadCoordinator();
    ~SquadCoordinator();

    // Squad management
    void AddMember(uint32_t entityId, const std::string& role = "follower");
    void RemoveMember(uint32_t entityId);
    void SetLeader(uint32_t entityId);
    [[nodiscard]] uint32_t GetLeader() const { return m_leaderId; }

    // Formation
    void SetFormation(const std::string& formationType);  // "line", "wedge", "circle"
    [[nodiscard]] glm::vec3 GetFormationPosition(uint32_t entityId, const glm::vec3& leaderPos,
                                                  float leaderRotation) const;

    // Commands (propagate to all members)
    void CommandMoveTo(const glm::vec3& position);
    void CommandAttack(uint32_t targetId);
    void CommandRetreat();
    void CommandHold();

    // Shared blackboard for squad
    Blackboard& GetSharedBlackboard() { return m_sharedBlackboard; }

    // Update
    void Update(float deltaTime, Vehement::EntityManager& entityManager);

    // Python integration for custom squad behaviors
    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }
    void SetSquadBehavior(const std::string& module, const std::string& function);

private:
    std::vector<SquadMember> m_members;
    uint32_t m_leaderId = 0;
    std::string m_formation = "line";
    Blackboard m_sharedBlackboard;

    // Python squad behavior
    std::string m_pythonModule;
    std::string m_pythonFunction;
    PythonEngine* m_pythonEngine = nullptr;
};

// ============================================================================
// AI Behavior Manager
// ============================================================================

/**
 * @brief Manager for AI behaviors across entities
 */
class AIBehaviorManager {
public:
    AIBehaviorManager();
    ~AIBehaviorManager();

    // Behavior tree management
    void AttachBehaviorTree(uint32_t entityId, std::unique_ptr<BehaviorNode> root);
    void DetachBehaviorTree(uint32_t entityId);

    // State machine management
    void AttachStateMachine(uint32_t entityId, std::unique_ptr<AIStateMachine> sm);
    void DetachStateMachine(uint32_t entityId);
    [[nodiscard]] AIStateMachine* GetStateMachine(uint32_t entityId);

    // Utility AI management
    void AttachUtilityAI(uint32_t entityId, std::unique_ptr<UtilityAI> utility);
    void DetachUtilityAI(uint32_t entityId);

    // Blackboard access
    [[nodiscard]] Blackboard& GetBlackboard(uint32_t entityId);

    // Update all AI
    void Update(float deltaTime, Vehement::EntityManager& entityManager);

    // Python integration
    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }
    void SetContext(ScriptContext* context) { m_context = context; }

    // Factory methods for creating Python-based behaviors
    std::unique_ptr<PythonBehaviorNode> CreatePythonNode(const std::string& name,
                                                          const std::string& module,
                                                          const std::string& function);

private:
    struct EntityAI {
        std::unique_ptr<BehaviorNode> behaviorTree;
        std::unique_ptr<AIStateMachine> stateMachine;
        std::unique_ptr<UtilityAI> utilityAI;
        Blackboard blackboard;
    };

    std::unordered_map<uint32_t, EntityAI> m_entityAIs;
    PythonEngine* m_pythonEngine = nullptr;
    ScriptContext* m_context = nullptr;
};

} // namespace Scripting
} // namespace Nova
