#include "AIBehavior.hpp"
#include "PythonEngine.hpp"
#include "ScriptContext.hpp"
#include <game/src/entities/Entity.hpp>
#include <game/src/entities/EntityManager.hpp>
#include <algorithm>
#include <cmath>
#include <random>

namespace Nova {
namespace Scripting {

// ============================================================================
// Blackboard Implementation
// ============================================================================

void Blackboard::Set(const std::string& key, const BlackboardValue& value) {
    m_data[key] = value;
}

std::optional<BlackboardValue> Blackboard::Get(const std::string& key) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Blackboard::Has(const std::string& key) const {
    return m_data.contains(key);
}

void Blackboard::Remove(const std::string& key) {
    m_data.erase(key);
}

void Blackboard::Clear() {
    m_data.clear();
}

std::vector<std::string> Blackboard::GetKeys() const {
    std::vector<std::string> keys;
    keys.reserve(m_data.size());
    for (const auto& [key, _] : m_data) {
        keys.push_back(key);
    }
    return keys;
}

// ============================================================================
// PythonBehaviorNode Implementation
// ============================================================================

PythonBehaviorNode::PythonBehaviorNode(const std::string& name,
                                       const std::string& pythonModule,
                                       const std::string& pythonFunction)
    : BehaviorNode(name)
    , m_pythonModule(pythonModule)
    , m_pythonFunction(pythonFunction) {}

BehaviorStatus PythonBehaviorNode::Execute(uint32_t entityId, Blackboard& blackboard,
                                           float deltaTime) {
    if (!m_pythonEngine) {
        return BehaviorStatus::Failure;
    }

    // Expose blackboard to Python context if available
    if (m_context) {
        // Set commonly used blackboard values as context variables
        if (auto target = blackboard.GetTargetEntity()) {
            m_context->SetGlobal("target_entity", static_cast<int>(*target));
        }
        if (auto pos = blackboard.GetTargetPosition()) {
            m_context->SetGlobal("target_position", *pos);
        }
    }

    // Call Python function
    // Arguments: entity_id, delta_time
    auto result = m_pythonEngine->CallFunction(m_pythonModule, m_pythonFunction,
                                               static_cast<int>(entityId), deltaTime);

    if (!result.success) {
        return BehaviorStatus::Failure;
    }

    // Parse result string
    auto statusStr = result.GetValue<std::string>();
    if (statusStr) {
        if (*statusStr == "running") return BehaviorStatus::Running;
        if (*statusStr == "success") return BehaviorStatus::Success;
        if (*statusStr == "failure") return BehaviorStatus::Failure;
    }

    // Also accept int/bool returns
    if (auto intVal = result.GetValue<int>()) {
        return static_cast<BehaviorStatus>(*intVal);
    }
    if (auto boolVal = result.GetValue<bool>()) {
        return *boolVal ? BehaviorStatus::Success : BehaviorStatus::Failure;
    }

    return BehaviorStatus::Success;  // Default
}

// ============================================================================
// Composite Nodes Implementation
// ============================================================================

SequenceNode::SequenceNode(const std::string& name) : BehaviorNode(name) {}

void SequenceNode::AddChild(std::unique_ptr<BehaviorNode> child) {
    m_children.push_back(std::move(child));
}

BehaviorStatus SequenceNode::Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    while (m_currentChild < m_children.size()) {
        auto status = m_children[m_currentChild]->Execute(entityId, blackboard, deltaTime);

        if (status == BehaviorStatus::Running) {
            return BehaviorStatus::Running;
        }

        if (status == BehaviorStatus::Failure) {
            m_currentChild = 0;
            return BehaviorStatus::Failure;
        }

        m_currentChild++;
    }

    m_currentChild = 0;
    return BehaviorStatus::Success;
}

void SequenceNode::Abort() {
    if (m_currentChild < m_children.size()) {
        m_children[m_currentChild]->Abort();
    }
    m_currentChild = 0;
}

SelectorNode::SelectorNode(const std::string& name) : BehaviorNode(name) {}

void SelectorNode::AddChild(std::unique_ptr<BehaviorNode> child) {
    m_children.push_back(std::move(child));
}

BehaviorStatus SelectorNode::Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    while (m_currentChild < m_children.size()) {
        auto status = m_children[m_currentChild]->Execute(entityId, blackboard, deltaTime);

        if (status == BehaviorStatus::Running) {
            return BehaviorStatus::Running;
        }

        if (status == BehaviorStatus::Success) {
            m_currentChild = 0;
            return BehaviorStatus::Success;
        }

        m_currentChild++;
    }

    m_currentChild = 0;
    return BehaviorStatus::Failure;
}

void SelectorNode::Abort() {
    if (m_currentChild < m_children.size()) {
        m_children[m_currentChild]->Abort();
    }
    m_currentChild = 0;
}

ParallelNode::ParallelNode(const std::string& name, Policy policy)
    : BehaviorNode(name), m_policy(policy) {}

void ParallelNode::AddChild(std::unique_ptr<BehaviorNode> child) {
    m_children.push_back(std::move(child));
}

BehaviorStatus ParallelNode::Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    int successCount = 0;
    int failureCount = 0;

    for (auto& child : m_children) {
        auto status = child->Execute(entityId, blackboard, deltaTime);
        if (status == BehaviorStatus::Success) successCount++;
        if (status == BehaviorStatus::Failure) failureCount++;
    }

    if (m_policy == Policy::RequireOne) {
        if (successCount > 0) return BehaviorStatus::Success;
        if (failureCount == static_cast<int>(m_children.size())) return BehaviorStatus::Failure;
    } else {  // RequireAll
        if (successCount == static_cast<int>(m_children.size())) return BehaviorStatus::Success;
        if (failureCount > 0) return BehaviorStatus::Failure;
    }

    return BehaviorStatus::Running;
}

// ============================================================================
// Decorator Nodes Implementation
// ============================================================================

InverterNode::InverterNode(std::unique_ptr<BehaviorNode> child)
    : m_child(std::move(child)) {}

BehaviorStatus InverterNode::Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    auto status = m_child->Execute(entityId, blackboard, deltaTime);

    if (status == BehaviorStatus::Success) return BehaviorStatus::Failure;
    if (status == BehaviorStatus::Failure) return BehaviorStatus::Success;
    return status;
}

RepeaterNode::RepeaterNode(std::unique_ptr<BehaviorNode> child, int repeatCount)
    : m_child(std::move(child)), m_repeatCount(repeatCount) {}

BehaviorStatus RepeaterNode::Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    auto status = m_child->Execute(entityId, blackboard, deltaTime);

    if (status == BehaviorStatus::Running) {
        return BehaviorStatus::Running;
    }

    m_currentCount++;

    // Infinite repeat
    if (m_repeatCount < 0) {
        return BehaviorStatus::Running;
    }

    // Limited repeats
    if (m_currentCount < m_repeatCount) {
        return BehaviorStatus::Running;
    }

    m_currentCount = 0;
    return status;
}

SucceederNode::SucceederNode(std::unique_ptr<BehaviorNode> child)
    : m_child(std::move(child)) {}

BehaviorStatus SucceederNode::Execute(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    auto status = m_child->Execute(entityId, blackboard, deltaTime);
    if (status == BehaviorStatus::Running) {
        return BehaviorStatus::Running;
    }
    return BehaviorStatus::Success;
}

// ============================================================================
// AIStateMachine Implementation
// ============================================================================

AIStateMachine::AIStateMachine() = default;
AIStateMachine::~AIStateMachine() = default;

void AIStateMachine::AddState(const AIState& state) {
    m_states[state.name] = state;
}

void AIStateMachine::RemoveState(const std::string& name) {
    m_states.erase(name);
}

void AIStateMachine::SetInitialState(const std::string& name) {
    m_initialState = name;
    m_currentState = name;
}

void AIStateMachine::AddTransition(const StateTransition& transition) {
    m_transitions.push_back(transition);
    // Sort by priority
    std::sort(m_transitions.begin(), m_transitions.end(),
              [](const StateTransition& a, const StateTransition& b) {
                  return a.priority > b.priority;
              });
}

void AIStateMachine::ForceTransition(const std::string& toState) {
    if (!HasState(toState)) return;

    uint32_t entityId = 0;  // TODO: Track entity ID
    Blackboard tempBlackboard;

    ExitState(entityId, tempBlackboard, m_currentState);
    m_currentState = toState;
    EnterState(entityId, tempBlackboard, m_currentState);
}

void AIStateMachine::Update(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    if (m_currentState.empty()) {
        if (!m_initialState.empty()) {
            m_currentState = m_initialState;
            EnterState(entityId, blackboard, m_currentState);
        }
        return;
    }

    // Check transitions
    for (const auto& transition : m_transitions) {
        if (transition.fromState != m_currentState) continue;

        if (CheckTransition(entityId, blackboard, transition)) {
            ExitState(entityId, blackboard, m_currentState);
            m_currentState = transition.toState;
            EnterState(entityId, blackboard, m_currentState);
            break;
        }
    }

    // Update current state
    auto it = m_states.find(m_currentState);
    if (it != m_states.end()) {
        const auto& state = it->second;

        if (state.usePython && m_pythonEngine && !state.updateFunction.empty()) {
            m_pythonEngine->CallFunction(state.pythonModule, state.updateFunction,
                                         static_cast<int>(entityId), deltaTime);
        } else if (state.onUpdate) {
            state.onUpdate(entityId, blackboard, deltaTime);
        }
    }
}

bool AIStateMachine::HasState(const std::string& name) const {
    return m_states.contains(name);
}

std::vector<std::string> AIStateMachine::GetStateNames() const {
    std::vector<std::string> names;
    names.reserve(m_states.size());
    for (const auto& [name, _] : m_states) {
        names.push_back(name);
    }
    return names;
}

void AIStateMachine::EnterState(uint32_t entityId, Blackboard& blackboard,
                                const std::string& stateName) {
    auto it = m_states.find(stateName);
    if (it == m_states.end()) return;

    const auto& state = it->second;

    if (state.usePython && m_pythonEngine && !state.enterFunction.empty()) {
        m_pythonEngine->CallFunction(state.pythonModule, state.enterFunction,
                                     static_cast<int>(entityId));
    } else if (state.onEnter) {
        state.onEnter(entityId, blackboard);
    }
}

void AIStateMachine::ExitState(uint32_t entityId, Blackboard& blackboard,
                               const std::string& stateName) {
    auto it = m_states.find(stateName);
    if (it == m_states.end()) return;

    const auto& state = it->second;

    if (state.usePython && m_pythonEngine && !state.exitFunction.empty()) {
        m_pythonEngine->CallFunction(state.pythonModule, state.exitFunction,
                                     static_cast<int>(entityId));
    } else if (state.onExit) {
        state.onExit(entityId, blackboard);
    }
}

bool AIStateMachine::CheckTransition(uint32_t entityId, const Blackboard& blackboard,
                                     const StateTransition& transition) {
    if (transition.usePython && m_pythonEngine && !transition.conditionFunction.empty()) {
        auto result = m_pythonEngine->CallFunction(transition.conditionModule,
                                                   transition.conditionFunction,
                                                   static_cast<int>(entityId));
        if (result.success) {
            if (auto val = result.GetValue<bool>()) {
                return *val;
            }
        }
        return false;
    } else if (transition.condition) {
        return transition.condition(entityId, blackboard);
    }
    return false;
}

// ============================================================================
// UtilityAI Implementation
// ============================================================================

UtilityAI::UtilityAI() = default;
UtilityAI::~UtilityAI() = default;

void UtilityAI::AddAction(const UtilityAction& action) {
    m_actions.push_back(action);
}

void UtilityAI::RemoveAction(const std::string& name) {
    m_actions.erase(
        std::remove_if(m_actions.begin(), m_actions.end(),
                       [&name](const UtilityAction& a) { return a.name == name; }),
        m_actions.end());
}

void UtilityAI::Update(uint32_t entityId, Blackboard& blackboard, float deltaTime) {
    if (m_actions.empty()) return;

    // Score all actions
    float bestScore = m_minThreshold;
    UtilityAction* bestAction = nullptr;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-m_randomness, m_randomness);

    for (auto& action : m_actions) {
        float score = ScoreAction(entityId, blackboard, action);
        score *= action.weight;
        score += dis(gen);  // Add randomness

        if (score > bestScore) {
            bestScore = score;
            bestAction = &action;
        }
    }

    // Execute best action
    if (bestAction) {
        m_currentAction = bestAction->name;
        ExecuteAction(entityId, blackboard, deltaTime, *bestAction);
    }
}

std::vector<std::pair<std::string, float>> UtilityAI::GetActionScores(
    uint32_t entityId, const Blackboard& blackboard) const {

    std::vector<std::pair<std::string, float>> scores;
    scores.reserve(m_actions.size());

    for (const auto& action : m_actions) {
        float score = ScoreAction(entityId, blackboard, action) * action.weight;
        scores.emplace_back(action.name, score);
    }

    return scores;
}

float UtilityAI::ScoreAction(uint32_t entityId, const Blackboard& blackboard,
                             const UtilityAction& action) const {
    if (action.usePython && m_pythonEngine && !action.scoreFunction.empty()) {
        auto result = m_pythonEngine->CallFunction(action.pythonModule, action.scoreFunction,
                                                   static_cast<int>(entityId));
        if (result.success) {
            if (auto val = result.GetValue<double>()) {
                return static_cast<float>(*val);
            }
            if (auto val = result.GetValue<float>()) {
                return *val;
            }
        }
        return 0.0f;
    } else if (action.scoreFn) {
        return action.scoreFn(entityId, blackboard);
    }
    return 0.0f;
}

void UtilityAI::ExecuteAction(uint32_t entityId, Blackboard& blackboard, float deltaTime,
                              UtilityAction& action) {
    if (action.usePython && m_pythonEngine && !action.executeFunction.empty()) {
        m_pythonEngine->CallFunction(action.pythonModule, action.executeFunction,
                                     static_cast<int>(entityId), deltaTime);
    } else if (action.executeFn) {
        action.executeFn(entityId, blackboard, deltaTime);
    }
}

// ============================================================================
// SquadCoordinator Implementation
// ============================================================================

SquadCoordinator::SquadCoordinator() = default;
SquadCoordinator::~SquadCoordinator() = default;

void SquadCoordinator::AddMember(uint32_t entityId, const std::string& role) {
    SquadMember member;
    member.entityId = entityId;
    member.role = role;
    m_members.push_back(member);

    if (role == "leader" || m_leaderId == 0) {
        m_leaderId = entityId;
    }
}

void SquadCoordinator::RemoveMember(uint32_t entityId) {
    m_members.erase(
        std::remove_if(m_members.begin(), m_members.end(),
                       [entityId](const SquadMember& m) { return m.entityId == entityId; }),
        m_members.end());

    if (m_leaderId == entityId) {
        m_leaderId = m_members.empty() ? 0 : m_members[0].entityId;
    }
}

void SquadCoordinator::SetLeader(uint32_t entityId) {
    for (auto& member : m_members) {
        if (member.entityId == entityId) {
            member.role = "leader";
            m_leaderId = entityId;
        } else if (member.role == "leader") {
            member.role = "follower";
        }
    }
}

void SquadCoordinator::SetFormation(const std::string& formationType) {
    m_formation = formationType;

    // Calculate formation offsets
    int index = 0;
    for (auto& member : m_members) {
        if (member.entityId == m_leaderId) {
            member.formationOffset = glm::vec3(0.0f);
            continue;
        }

        index++;
        float spacing = 2.0f;

        if (formationType == "line") {
            // Single file behind leader
            member.formationOffset = glm::vec3(0.0f, 0.0f, -spacing * index);
        } else if (formationType == "wedge") {
            // V formation
            int side = (index % 2 == 0) ? 1 : -1;
            int row = (index + 1) / 2;
            member.formationOffset = glm::vec3(side * spacing * row, 0.0f, -spacing * row);
        } else if (formationType == "circle") {
            // Circle around leader
            float angle = (index - 1) * (2.0f * 3.14159f / (m_members.size() - 1));
            member.formationOffset = glm::vec3(std::cos(angle) * spacing, 0.0f,
                                               std::sin(angle) * spacing);
        }
    }
}

glm::vec3 SquadCoordinator::GetFormationPosition(uint32_t entityId, const glm::vec3& leaderPos,
                                                  float leaderRotation) const {
    for (const auto& member : m_members) {
        if (member.entityId == entityId) {
            // Rotate offset by leader rotation
            float c = std::cos(leaderRotation);
            float s = std::sin(leaderRotation);
            glm::vec3 rotatedOffset(
                member.formationOffset.x * c - member.formationOffset.z * s,
                member.formationOffset.y,
                member.formationOffset.x * s + member.formationOffset.z * c
            );
            return leaderPos + rotatedOffset;
        }
    }
    return leaderPos;
}

void SquadCoordinator::CommandMoveTo(const glm::vec3& position) {
    m_sharedBlackboard.SetTargetPosition(position);
    m_sharedBlackboard.SetValue("command", std::string("move"));
}

void SquadCoordinator::CommandAttack(uint32_t targetId) {
    m_sharedBlackboard.SetTargetEntity(targetId);
    m_sharedBlackboard.SetValue("command", std::string("attack"));
}

void SquadCoordinator::CommandRetreat() {
    m_sharedBlackboard.SetValue("command", std::string("retreat"));
}

void SquadCoordinator::CommandHold() {
    m_sharedBlackboard.SetValue("command", std::string("hold"));
}

void SquadCoordinator::Update(float deltaTime, Vehement::EntityManager& entityManager) {
    // Custom Python squad behavior
    if (m_pythonEngine && !m_pythonModule.empty() && !m_pythonFunction.empty()) {
        m_pythonEngine->CallFunction(m_pythonModule, m_pythonFunction, deltaTime);
    }
}

void SquadCoordinator::SetSquadBehavior(const std::string& module, const std::string& function) {
    m_pythonModule = module;
    m_pythonFunction = function;
}

// ============================================================================
// AIBehaviorManager Implementation
// ============================================================================

AIBehaviorManager::AIBehaviorManager() = default;
AIBehaviorManager::~AIBehaviorManager() = default;

void AIBehaviorManager::AttachBehaviorTree(uint32_t entityId, std::unique_ptr<BehaviorNode> root) {
    m_entityAIs[entityId].behaviorTree = std::move(root);
}

void AIBehaviorManager::DetachBehaviorTree(uint32_t entityId) {
    auto it = m_entityAIs.find(entityId);
    if (it != m_entityAIs.end()) {
        it->second.behaviorTree.reset();
    }
}

void AIBehaviorManager::AttachStateMachine(uint32_t entityId, std::unique_ptr<AIStateMachine> sm) {
    sm->SetPythonEngine(m_pythonEngine);
    sm->SetContext(m_context);
    m_entityAIs[entityId].stateMachine = std::move(sm);
}

void AIBehaviorManager::DetachStateMachine(uint32_t entityId) {
    auto it = m_entityAIs.find(entityId);
    if (it != m_entityAIs.end()) {
        it->second.stateMachine.reset();
    }
}

AIStateMachine* AIBehaviorManager::GetStateMachine(uint32_t entityId) {
    auto it = m_entityAIs.find(entityId);
    if (it != m_entityAIs.end()) {
        return it->second.stateMachine.get();
    }
    return nullptr;
}

void AIBehaviorManager::AttachUtilityAI(uint32_t entityId, std::unique_ptr<UtilityAI> utility) {
    utility->SetPythonEngine(m_pythonEngine);
    utility->SetContext(m_context);
    m_entityAIs[entityId].utilityAI = std::move(utility);
}

void AIBehaviorManager::DetachUtilityAI(uint32_t entityId) {
    auto it = m_entityAIs.find(entityId);
    if (it != m_entityAIs.end()) {
        it->second.utilityAI.reset();
    }
}

Blackboard& AIBehaviorManager::GetBlackboard(uint32_t entityId) {
    return m_entityAIs[entityId].blackboard;
}

void AIBehaviorManager::Update(float deltaTime, Vehement::EntityManager& entityManager) {
    for (auto& [entityId, ai] : m_entityAIs) {
        // Check if entity still exists
        if (!entityManager.GetEntity(entityId)) {
            continue;
        }

        // Update behavior tree
        if (ai.behaviorTree) {
            ai.behaviorTree->Execute(entityId, ai.blackboard, deltaTime);
        }

        // Update state machine
        if (ai.stateMachine) {
            ai.stateMachine->Update(entityId, ai.blackboard, deltaTime);
        }

        // Update utility AI
        if (ai.utilityAI) {
            ai.utilityAI->Update(entityId, ai.blackboard, deltaTime);
        }
    }
}

std::unique_ptr<PythonBehaviorNode> AIBehaviorManager::CreatePythonNode(
    const std::string& name, const std::string& module, const std::string& function) {

    auto node = std::make_unique<PythonBehaviorNode>(name, module, function);
    node->SetPythonEngine(m_pythonEngine);
    node->SetContext(m_context);
    return node;
}

} // namespace Scripting
} // namespace Nova
