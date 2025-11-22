#include "BlendStateEditor.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Vehement {

BlendStateEditor::BlendStateEditor() = default;
BlendStateEditor::~BlendStateEditor() = default;

void BlendStateEditor::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

// ============================================================================
// File Operations
// ============================================================================

void BlendStateEditor::NewStateMachine(const std::string& name) {
    m_name = name;
    m_filePath.clear();
    m_states.clear();
    m_transitions.clear();
    m_parameters.clear();
    m_blendTrees.clear();
    m_selectedState.clear();
    m_selectedTransition.clear();
    m_multiSelection.clear();
    m_viewOffset = glm::vec2(0.0f);
    m_zoom = 1.0f;
    m_dirty = false;

    // Add default entry state
    auto* entry = AddState("Entry", glm::vec2(-200.0f, 0.0f));
    if (entry) {
        entry->isAnyState = true;
        entry->color = m_config.anyStateColor;
    }

    // Add default state
    auto* idle = AddState("Idle", glm::vec2(0.0f, 0.0f));
    if (idle) {
        idle->isDefault = true;
        idle->color = m_config.defaultStateColor;
    }
}

bool BlendStateEditor::LoadStateMachine(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (!ImportFromJson(buffer.str())) {
        return false;
    }

    m_filePath = filePath;
    m_dirty = false;
    return true;
}

bool BlendStateEditor::SaveStateMachine(const std::string& filePath) {
    std::string path = filePath.empty() ? m_filePath : filePath;
    if (path.empty()) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << ExportToJson();
    m_filePath = path;
    m_dirty = false;
    return true;
}

std::string BlendStateEditor::ExportToJson() const {
    json j;
    j["name"] = m_name;
    j["version"] = "1.0";

    // Parameters
    json params = json::array();
    for (const auto& param : m_parameters) {
        json p;
        p["name"] = param.name;
        p["type"] = param.type;
        p["defaultValue"] = param.type == "float" ? json(param.floatValue) :
                           param.type == "int" ? json(param.intValue) :
                           param.type == "bool" ? json(param.boolValue) : json(0);
        if (param.type == "float") {
            p["min"] = param.minValue;
            p["max"] = param.maxValue;
        }
        params.push_back(p);
    }
    j["parameters"] = params;

    // States
    json states = json::array();
    for (const auto& state : m_states) {
        json s;
        s["id"] = state.id;
        s["name"] = state.name;
        s["position"] = {state.position.x, state.position.y};
        s["animationClip"] = state.animationClip;
        s["isDefault"] = state.isDefault;
        s["isAnyState"] = state.isAnyState;
        s["isBlendTree"] = state.isBlendTree;

        if (state.isBlendTree) {
            auto it = m_blendTrees.find(state.id);
            if (it != m_blendTrees.end()) {
                const auto& bt = it->second;
                json btJson;
                btJson["type"] = bt.type;
                btJson["parameterX"] = bt.parameterX;
                btJson["parameterY"] = bt.parameterY;
                btJson["normalizeWeights"] = bt.normalizeWeights;

                json children = json::array();
                for (const auto& child : bt.children) {
                    json c;
                    c["animationClip"] = child.animationClip;
                    c["threshold"] = child.threshold;
                    c["position"] = {child.position.x, child.position.y};
                    c["directWeight"] = child.directWeight;
                    c["timeScale"] = child.timeScale;
                    c["mirror"] = child.mirror;
                    children.push_back(c);
                }
                btJson["children"] = children;
                s["blendTree"] = btJson;
            }
        }

        states.push_back(s);
    }
    j["states"] = states;

    // Transitions
    json transitions = json::array();
    for (const auto& trans : m_transitions) {
        json t;
        t["id"] = trans.id;
        t["from"] = trans.fromState;
        t["to"] = trans.toState;
        t["duration"] = trans.duration;
        t["exitTime"] = trans.exitTime;
        t["hasExitTime"] = trans.hasExitTime;
        t["canTransitionToSelf"] = trans.canTransitionToSelf;
        t["priority"] = trans.priority;

        json conditions = json::array();
        for (const auto& cond : trans.conditions) {
            json c;
            c["parameter"] = cond.parameter;
            c["comparison"] = cond.comparison;
            c["threshold"] = cond.threshold;
            c["boolValue"] = cond.boolValue;
            conditions.push_back(c);
        }
        t["conditions"] = conditions;

        transitions.push_back(t);
    }
    j["transitions"] = transitions;

    return j.dump(2);
}

bool BlendStateEditor::ImportFromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        m_name = j.value("name", "ImportedStateMachine");
        m_states.clear();
        m_transitions.clear();
        m_parameters.clear();
        m_blendTrees.clear();

        // Parameters
        if (j.contains("parameters")) {
            for (const auto& p : j["parameters"]) {
                AnimationParameter param;
                param.name = p.value("name", "");
                param.type = p.value("type", "float");
                if (param.type == "float") {
                    param.floatValue = p.value("defaultValue", 0.0f);
                    param.minValue = p.value("min", 0.0f);
                    param.maxValue = p.value("max", 1.0f);
                } else if (param.type == "int") {
                    param.intValue = p.value("defaultValue", 0);
                } else if (param.type == "bool" || param.type == "trigger") {
                    param.boolValue = p.value("defaultValue", false);
                }
                m_parameters.push_back(param);
            }
        }

        // States
        if (j.contains("states")) {
            for (const auto& s : j["states"]) {
                BlendStateNode state;
                state.id = s.value("id", GenerateId());
                state.name = s.value("name", "State");
                if (s.contains("position")) {
                    state.position.x = s["position"][0];
                    state.position.y = s["position"][1];
                }
                state.animationClip = s.value("animationClip", "");
                state.isDefault = s.value("isDefault", false);
                state.isAnyState = s.value("isAnyState", false);
                state.isBlendTree = s.value("isBlendTree", false);

                if (state.isDefault) {
                    state.color = m_config.defaultStateColor;
                } else if (state.isAnyState) {
                    state.color = m_config.anyStateColor;
                }

                if (state.isBlendTree && s.contains("blendTree")) {
                    const auto& bt = s["blendTree"];
                    BlendTreeConfig config;
                    config.type = bt.value("type", "1D");
                    config.parameterX = bt.value("parameterX", "");
                    config.parameterY = bt.value("parameterY", "");
                    config.normalizeWeights = bt.value("normalizeWeights", true);

                    if (bt.contains("children")) {
                        for (const auto& c : bt["children"]) {
                            BlendTreeChild child;
                            child.animationClip = c.value("animationClip", "");
                            child.threshold = c.value("threshold", 0.0f);
                            if (c.contains("position")) {
                                child.position.x = c["position"][0];
                                child.position.y = c["position"][1];
                            }
                            child.directWeight = c.value("directWeight", 0.0f);
                            child.timeScale = c.value("timeScale", 1.0f);
                            child.mirror = c.value("mirror", false);
                            config.children.push_back(child);
                        }
                    }
                    m_blendTrees[state.id] = config;
                }

                m_states.push_back(state);
            }
        }

        // Transitions
        if (j.contains("transitions")) {
            for (const auto& t : j["transitions"]) {
                StateTransitionConnection trans;
                trans.id = t.value("id", GenerateId());
                trans.fromState = t.value("from", "");
                trans.toState = t.value("to", "");
                trans.duration = t.value("duration", 0.2f);
                trans.exitTime = t.value("exitTime", 0.0f);
                trans.hasExitTime = t.value("hasExitTime", false);
                trans.canTransitionToSelf = t.value("canTransitionToSelf", false);
                trans.priority = t.value("priority", 0);

                if (t.contains("conditions")) {
                    for (const auto& c : t["conditions"]) {
                        TransitionCondition cond;
                        cond.parameter = c.value("parameter", "");
                        cond.comparison = c.value("comparison", "greater");
                        cond.threshold = c.value("threshold", 0.0f);
                        cond.boolValue = c.value("boolValue", false);
                        trans.conditions.push_back(cond);
                    }
                }

                m_transitions.push_back(trans);
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// ============================================================================
// State Management
// ============================================================================

BlendStateNode* BlendStateEditor::AddState(const std::string& name, const glm::vec2& position) {
    BlendStateNode state;
    state.id = GenerateId();
    state.name = name;
    state.position = m_config.snapToGrid ? SnapToGrid(position) : position;

    m_states.push_back(state);
    MarkDirty();

    return &m_states.back();
}

BlendStateNode* BlendStateEditor::AddBlendTreeState(const std::string& name, const glm::vec2& position,
                                                     const std::string& type) {
    auto* state = AddState(name, position);
    if (state) {
        state->isBlendTree = true;
        state->blendTreeType = type;

        BlendTreeConfig config;
        config.type = type;
        m_blendTrees[state->id] = config;
    }
    return state;
}

void BlendStateEditor::RemoveState(const std::string& id) {
    // Remove transitions involving this state
    m_transitions.erase(
        std::remove_if(m_transitions.begin(), m_transitions.end(),
            [&id](const StateTransitionConnection& t) {
                return t.fromState == id || t.toState == id;
            }),
        m_transitions.end()
    );

    // Remove blend tree config
    m_blendTrees.erase(id);

    // Remove state
    m_states.erase(
        std::remove_if(m_states.begin(), m_states.end(),
            [&id](const BlendStateNode& s) { return s.id == id; }),
        m_states.end()
    );

    if (m_selectedState == id) {
        m_selectedState.clear();
    }

    MarkDirty();
}

BlendStateNode* BlendStateEditor::GetState(const std::string& id) {
    for (auto& state : m_states) {
        if (state.id == id) {
            return &state;
        }
    }
    return nullptr;
}

const BlendStateNode* BlendStateEditor::GetState(const std::string& id) const {
    for (const auto& state : m_states) {
        if (state.id == id) {
            return &state;
        }
    }
    return nullptr;
}

void BlendStateEditor::SetDefaultState(const std::string& id) {
    for (auto& state : m_states) {
        bool wasDefault = state.isDefault;
        state.isDefault = (state.id == id);

        if (state.isDefault && !wasDefault) {
            state.color = m_config.defaultStateColor;
        } else if (!state.isDefault && wasDefault && !state.isAnyState) {
            state.color = glm::vec4(0.3f, 0.5f, 0.8f, 1.0f);
        }
    }
    MarkDirty();
}

std::string BlendStateEditor::GetDefaultState() const {
    for (const auto& state : m_states) {
        if (state.isDefault) {
            return state.id;
        }
    }
    return "";
}

bool BlendStateEditor::RenameState(const std::string& id, const std::string& newName) {
    if (auto* state = GetState(id)) {
        state->name = newName;
        MarkDirty();
        return true;
    }
    return false;
}

BlendStateNode* BlendStateEditor::DuplicateState(const std::string& id) {
    const auto* original = GetState(id);
    if (!original) return nullptr;

    BlendStateNode copy = *original;
    copy.id = GenerateId();
    copy.name = original->name + "_copy";
    copy.position += glm::vec2(50.0f, 50.0f);
    copy.isDefault = false;
    copy.selected = false;

    m_states.push_back(copy);

    // Copy blend tree if exists
    auto btIt = m_blendTrees.find(id);
    if (btIt != m_blendTrees.end()) {
        m_blendTrees[copy.id] = btIt->second;
    }

    MarkDirty();
    return &m_states.back();
}

// ============================================================================
// Transition Management
// ============================================================================

StateTransitionConnection* BlendStateEditor::AddTransition(const std::string& fromState, const std::string& toState) {
    // Check if transition already exists
    for (auto& trans : m_transitions) {
        if (trans.fromState == fromState && trans.toState == toState) {
            return &trans;
        }
    }

    StateTransitionConnection trans;
    trans.id = GenerateId();
    trans.fromState = fromState;
    trans.toState = toState;

    m_transitions.push_back(trans);
    MarkDirty();

    return &m_transitions.back();
}

void BlendStateEditor::RemoveTransition(const std::string& id) {
    m_transitions.erase(
        std::remove_if(m_transitions.begin(), m_transitions.end(),
            [&id](const StateTransitionConnection& t) { return t.id == id; }),
        m_transitions.end()
    );

    if (m_selectedTransition == id) {
        m_selectedTransition.clear();
    }

    MarkDirty();
}

StateTransitionConnection* BlendStateEditor::GetTransition(const std::string& id) {
    for (auto& trans : m_transitions) {
        if (trans.id == id) {
            return &trans;
        }
    }
    return nullptr;
}

std::vector<StateTransitionConnection*> BlendStateEditor::GetTransitionsFromState(const std::string& stateId) {
    std::vector<StateTransitionConnection*> result;
    for (auto& trans : m_transitions) {
        if (trans.fromState == stateId) {
            result.push_back(&trans);
        }
    }
    return result;
}

std::vector<StateTransitionConnection*> BlendStateEditor::GetTransitionsToState(const std::string& stateId) {
    std::vector<StateTransitionConnection*> result;
    for (auto& trans : m_transitions) {
        if (trans.toState == stateId) {
            result.push_back(&trans);
        }
    }
    return result;
}

void BlendStateEditor::AddTransitionCondition(const std::string& transitionId, const TransitionCondition& condition) {
    if (auto* trans = GetTransition(transitionId)) {
        trans->conditions.push_back(condition);
        MarkDirty();
    }
}

void BlendStateEditor::RemoveTransitionCondition(const std::string& transitionId, size_t conditionIndex) {
    if (auto* trans = GetTransition(transitionId)) {
        if (conditionIndex < trans->conditions.size()) {
            trans->conditions.erase(trans->conditions.begin() + static_cast<ptrdiff_t>(conditionIndex));
            MarkDirty();
        }
    }
}

void BlendStateEditor::UpdateTransition(const std::string& id, float duration, float exitTime,
                                         bool hasExitTime, bool canTransitionToSelf) {
    if (auto* trans = GetTransition(id)) {
        trans->duration = duration;
        trans->exitTime = exitTime;
        trans->hasExitTime = hasExitTime;
        trans->canTransitionToSelf = canTransitionToSelf;
        MarkDirty();
    }
}

// ============================================================================
// Parameter Management
// ============================================================================

AnimationParameter* BlendStateEditor::AddParameter(const std::string& name, const std::string& type) {
    // Check if exists
    for (auto& param : m_parameters) {
        if (param.name == name) {
            return &param;
        }
    }

    AnimationParameter param;
    param.name = name;
    param.type = type;
    m_parameters.push_back(param);
    MarkDirty();

    return &m_parameters.back();
}

void BlendStateEditor::RemoveParameter(const std::string& name) {
    m_parameters.erase(
        std::remove_if(m_parameters.begin(), m_parameters.end(),
            [&name](const AnimationParameter& p) { return p.name == name; }),
        m_parameters.end()
    );
    MarkDirty();
}

AnimationParameter* BlendStateEditor::GetParameter(const std::string& name) {
    for (auto& param : m_parameters) {
        if (param.name == name) {
            return &param;
        }
    }
    return nullptr;
}

void BlendStateEditor::SetParameterValue(const std::string& name, float value) {
    if (auto* param = GetParameter(name)) {
        param->floatValue = std::clamp(value, param->minValue, param->maxValue);
    }
}

void BlendStateEditor::SetParameterValue(const std::string& name, int value) {
    if (auto* param = GetParameter(name)) {
        param->intValue = value;
    }
}

void BlendStateEditor::SetParameterValue(const std::string& name, bool value) {
    if (auto* param = GetParameter(name)) {
        param->boolValue = value;
    }
}

void BlendStateEditor::TriggerParameter(const std::string& name) {
    if (auto* param = GetParameter(name)) {
        if (param->type == "trigger") {
            param->boolValue = true;
        }
    }
}

// ============================================================================
// Blend Tree Configuration
// ============================================================================

BlendTreeConfig* BlendStateEditor::GetBlendTreeConfig(const std::string& stateId) {
    auto it = m_blendTrees.find(stateId);
    if (it != m_blendTrees.end()) {
        return &it->second;
    }
    return nullptr;
}

void BlendStateEditor::SetBlendTreeConfig(const std::string& stateId, const BlendTreeConfig& config) {
    m_blendTrees[stateId] = config;
    MarkDirty();
}

void BlendStateEditor::AddBlendTreeChild(const std::string& stateId, const BlendTreeChild& child) {
    auto it = m_blendTrees.find(stateId);
    if (it != m_blendTrees.end()) {
        it->second.children.push_back(child);
        MarkDirty();
    }
}

void BlendStateEditor::RemoveBlendTreeChild(const std::string& stateId, size_t index) {
    auto it = m_blendTrees.find(stateId);
    if (it != m_blendTrees.end() && index < it->second.children.size()) {
        it->second.children.erase(it->second.children.begin() + static_cast<ptrdiff_t>(index));
        MarkDirty();
    }
}

void BlendStateEditor::UpdateBlendTreeChild(const std::string& stateId, size_t index, const BlendTreeChild& child) {
    auto it = m_blendTrees.find(stateId);
    if (it != m_blendTrees.end() && index < it->second.children.size()) {
        it->second.children[index] = child;
        MarkDirty();
    }
}

// ============================================================================
// Selection
// ============================================================================

void BlendStateEditor::SelectState(const std::string& id) {
    m_selectedTransition.clear();

    for (auto& state : m_states) {
        state.selected = (state.id == id);
    }
    for (auto& trans : m_transitions) {
        trans.selected = false;
    }

    m_selectedState = id;

    if (OnStateSelected) {
        OnStateSelected(id);
    }
}

void BlendStateEditor::SelectTransition(const std::string& id) {
    m_selectedState.clear();

    for (auto& state : m_states) {
        state.selected = false;
    }
    for (auto& trans : m_transitions) {
        trans.selected = (trans.id == id);
    }

    m_selectedTransition = id;

    if (OnTransitionSelected) {
        OnTransitionSelected(id);
    }
}

void BlendStateEditor::ClearSelection() {
    for (auto& state : m_states) {
        state.selected = false;
    }
    for (auto& trans : m_transitions) {
        trans.selected = false;
    }

    m_selectedState.clear();
    m_selectedTransition.clear();
    m_multiSelection.clear();
}

void BlendStateEditor::AddToSelection(const std::string& stateId) {
    if (std::find(m_multiSelection.begin(), m_multiSelection.end(), stateId) == m_multiSelection.end()) {
        m_multiSelection.push_back(stateId);
        if (auto* state = GetState(stateId)) {
            state->selected = true;
        }
    }
}

void BlendStateEditor::RemoveFromSelection(const std::string& stateId) {
    m_multiSelection.erase(
        std::remove(m_multiSelection.begin(), m_multiSelection.end(), stateId),
        m_multiSelection.end()
    );
    if (auto* state = GetState(stateId)) {
        state->selected = false;
    }
}

// ============================================================================
// View Control
// ============================================================================

void BlendStateEditor::SetZoom(float zoom) {
    m_zoom = std::clamp(zoom, m_config.zoomMin, m_config.zoomMax);
}

void BlendStateEditor::ZoomToFit() {
    if (m_states.empty()) return;

    glm::vec2 minPos(std::numeric_limits<float>::max());
    glm::vec2 maxPos(std::numeric_limits<float>::lowest());

    for (const auto& state : m_states) {
        minPos = glm::min(minPos, state.position);
        maxPos = glm::max(maxPos, state.position + state.size);
    }

    m_viewOffset = -(minPos + maxPos) * 0.5f;
    // Would calculate zoom based on viewport size
}

void BlendStateEditor::CenterOnState(const std::string& id) {
    if (const auto* state = GetState(id)) {
        m_viewOffset = -(state->position + state->size * 0.5f);
    }
}

// ============================================================================
// Testing / Preview
// ============================================================================

void BlendStateEditor::StartTestMode() {
    m_testMode = true;
    m_currentTestState = GetDefaultState();
    m_testStateTime = 0.0f;
    m_transitionProgress = 0.0f;
    m_pendingTransition.clear();
}

void BlendStateEditor::StopTestMode() {
    m_testMode = false;
}

void BlendStateEditor::UpdateTestMode(float deltaTime) {
    if (!m_testMode) return;

    m_testStateTime += deltaTime;

    // Check for transitions
    auto transitions = GetTransitionsFromState(m_currentTestState);

    // Sort by priority
    std::sort(transitions.begin(), transitions.end(),
        [](const StateTransitionConnection* a, const StateTransitionConnection* b) {
            return a->priority > b->priority;
        });

    for (auto* trans : transitions) {
        if (EvaluateTransition(*trans)) {
            // Start transition
            m_pendingTransition = trans->toState;
            m_transitionProgress = 0.0f;
            break;
        }
    }

    // Process transition
    if (!m_pendingTransition.empty()) {
        auto* trans = GetTransition(m_pendingTransition);
        if (trans) {
            m_transitionProgress += deltaTime / trans->duration;
            if (m_transitionProgress >= 1.0f) {
                m_currentTestState = m_pendingTransition;
                m_pendingTransition.clear();
                m_testStateTime = 0.0f;

                if (OnTestStateChanged) {
                    OnTestStateChanged(m_currentTestState);
                }
            }
        }
    }

    // Reset triggers
    for (auto& param : m_parameters) {
        if (param.type == "trigger") {
            param.boolValue = false;
        }
    }
}

std::unordered_map<std::string, float> BlendStateEditor::CalculateBlendWeights(const std::string& stateId) const {
    std::unordered_map<std::string, float> weights;

    auto it = m_blendTrees.find(stateId);
    if (it == m_blendTrees.end()) return weights;

    const auto& bt = it->second;

    if (bt.type == "1D") {
        // Find parameter value
        float paramValue = 0.0f;
        for (const auto& param : m_parameters) {
            if (param.name == bt.parameterX) {
                paramValue = param.floatValue;
                break;
            }
        }

        // Calculate weights based on threshold
        for (const auto& child : bt.children) {
            // Simplified linear interpolation between neighbors
            float weight = std::max(0.0f, 1.0f - std::abs(paramValue - child.threshold));
            weights[child.animationClip] = weight;
        }
    }

    // Normalize if needed
    if (bt.normalizeWeights) {
        float total = 0.0f;
        for (const auto& [_, w] : weights) {
            total += w;
        }
        if (total > 0.0f) {
            for (auto& [_, w] : weights) {
                w /= total;
            }
        }
    }

    return weights;
}

// ============================================================================
// Layout
// ============================================================================

void BlendStateEditor::AutoLayout() {
    // Simple horizontal layout
    float x = 0.0f;
    float y = 0.0f;
    float spacing = 200.0f;

    for (auto& state : m_states) {
        if (state.isAnyState) {
            state.position = glm::vec2(-spacing, 0.0f);
            continue;
        }

        state.position = glm::vec2(x, y);
        x += spacing;

        if (x > spacing * 3) {
            x = 0.0f;
            y += 150.0f;
        }
    }

    MarkDirty();
}

void BlendStateEditor::AlignSelected(const std::string& alignment) {
    if (m_multiSelection.size() < 2) return;

    float target = 0.0f;

    // Find target position
    auto* first = GetState(m_multiSelection[0]);
    if (!first) return;

    if (alignment == "left") {
        target = first->position.x;
        for (const auto& id : m_multiSelection) {
            if (auto* state = GetState(id)) {
                target = std::min(target, state->position.x);
            }
        }
        for (const auto& id : m_multiSelection) {
            if (auto* state = GetState(id)) {
                state->position.x = target;
            }
        }
    } else if (alignment == "top") {
        target = first->position.y;
        for (const auto& id : m_multiSelection) {
            if (auto* state = GetState(id)) {
                target = std::min(target, state->position.y);
            }
        }
        for (const auto& id : m_multiSelection) {
            if (auto* state = GetState(id)) {
                state->position.y = target;
            }
        }
    }

    MarkDirty();
}

void BlendStateEditor::DistributeSelected(const std::string& direction) {
    if (m_multiSelection.size() < 3) return;

    // Sort by position
    std::vector<BlendStateNode*> sorted;
    for (const auto& id : m_multiSelection) {
        if (auto* state = GetState(id)) {
            sorted.push_back(state);
        }
    }

    if (direction == "horizontal") {
        std::sort(sorted.begin(), sorted.end(),
            [](const BlendStateNode* a, const BlendStateNode* b) {
                return a->position.x < b->position.x;
            });

        float start = sorted.front()->position.x;
        float end = sorted.back()->position.x;
        float step = (end - start) / static_cast<float>(sorted.size() - 1);

        for (size_t i = 0; i < sorted.size(); ++i) {
            sorted[i]->position.x = start + step * static_cast<float>(i);
        }
    } else if (direction == "vertical") {
        std::sort(sorted.begin(), sorted.end(),
            [](const BlendStateNode* a, const BlendStateNode* b) {
                return a->position.y < b->position.y;
            });

        float start = sorted.front()->position.y;
        float end = sorted.back()->position.y;
        float step = (end - start) / static_cast<float>(sorted.size() - 1);

        for (size_t i = 0; i < sorted.size(); ++i) {
            sorted[i]->position.y = start + step * static_cast<float>(i);
        }
    }

    MarkDirty();
}

// ============================================================================
// Private Methods
// ============================================================================

std::string BlendStateEditor::GenerateId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    const char* hexChars = "0123456789abcdef";
    std::string id;
    id.reserve(16);

    for (int i = 0; i < 16; ++i) {
        id += hexChars[dis(gen)];
    }

    return id;
}

void BlendStateEditor::MarkDirty() {
    m_dirty = true;
    if (OnModified) {
        OnModified();
    }
}

bool BlendStateEditor::EvaluateCondition(const TransitionCondition& condition) const {
    for (const auto& param : m_parameters) {
        if (param.name != condition.parameter) continue;

        if (param.type == "float") {
            if (condition.comparison == "greater") return param.floatValue > condition.threshold;
            if (condition.comparison == "less") return param.floatValue < condition.threshold;
            if (condition.comparison == "equals") return std::abs(param.floatValue - condition.threshold) < 0.001f;
        } else if (param.type == "bool") {
            if (condition.comparison == "equals") return param.boolValue == condition.boolValue;
        } else if (param.type == "trigger") {
            return param.boolValue;
        }
    }

    return false;
}

bool BlendStateEditor::EvaluateTransition(const StateTransitionConnection& transition) const {
    // Check exit time
    if (transition.hasExitTime) {
        // Would need animation duration info
    }

    // Check all conditions (AND)
    for (const auto& condition : transition.conditions) {
        if (!EvaluateCondition(condition)) {
            return false;
        }
    }

    return !transition.conditions.empty();
}

glm::vec2 BlendStateEditor::SnapToGrid(const glm::vec2& pos) const {
    return glm::vec2(
        std::round(pos.x / m_config.gridSize.x) * m_config.gridSize.x,
        std::round(pos.y / m_config.gridSize.y) * m_config.gridSize.y
    );
}

} // namespace Vehement
