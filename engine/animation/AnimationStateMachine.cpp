#include "AnimationStateMachine.hpp"
#include "AnimationController.hpp"
#include "AnimationEventSystem.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace Nova {

// ============================================================================
// AnimationEvent
// ============================================================================

json AnimationEvent::ToJson() const {
    return json{
        {"time", time},
        {"name", eventName},
        {"data", eventData}
    };
}

AnimationEvent AnimationEvent::FromJson(const json& j) {
    AnimationEvent event;
    event.time = j.value("time", 0.0f);
    event.eventName = j.value("name", "");
    if (j.contains("data")) {
        event.eventData = j["data"];
    }
    return event;
}

// ============================================================================
// TransitionCondition
// ============================================================================

json TransitionCondition::ToJson() const {
    std::string modeStr;
    switch (mode) {
        case Mode::IfTrue: modeStr = "if_true"; break;
        case Mode::IfFalse: modeStr = "if_false"; break;
        case Mode::Greater: modeStr = "greater"; break;
        case Mode::Less: modeStr = "less"; break;
        case Mode::Equals: modeStr = "equals"; break;
        case Mode::NotEquals: modeStr = "not_equals"; break;
        case Mode::GreaterOrEqual: modeStr = "greater_or_equal"; break;
        case Mode::LessOrEqual: modeStr = "less_or_equal"; break;
    }

    json j = {
        {"parameter", parameter},
        {"mode", modeStr}
    };

    if (mode != Mode::IfTrue && mode != Mode::IfFalse) {
        j["threshold"] = threshold;
    }

    return j;
}

TransitionCondition TransitionCondition::FromJson(const json& j) {
    TransitionCondition cond;
    cond.parameter = j.value("parameter", "");

    std::string modeStr = j.value("mode", "if_true");
    if (modeStr == "if_true") cond.mode = Mode::IfTrue;
    else if (modeStr == "if_false") cond.mode = Mode::IfFalse;
    else if (modeStr == "greater") cond.mode = Mode::Greater;
    else if (modeStr == "less") cond.mode = Mode::Less;
    else if (modeStr == "equals") cond.mode = Mode::Equals;
    else if (modeStr == "not_equals") cond.mode = Mode::NotEquals;
    else if (modeStr == "greater_or_equal") cond.mode = Mode::GreaterOrEqual;
    else if (modeStr == "less_or_equal") cond.mode = Mode::LessOrEqual;

    cond.threshold = j.value("threshold", 0.0f);
    return cond;
}

// ============================================================================
// StateTransition
// ============================================================================

json StateTransition::ToJson() const {
    json j = {
        {"to", targetState},
        {"duration", blendDuration},
        {"priority", priority}
    };

    if (!condition.empty()) {
        j["condition"] = condition;
    }

    if (!conditions.empty()) {
        j["conditions"] = json::array();
        for (const auto& c : conditions) {
            j["conditions"].push_back(c.ToJson());
        }
    }

    if (hasExitTime) {
        j["hasExitTime"] = true;
        j["exitTime"] = exitTime;
    }

    if (canTransitionToSelf) {
        j["canTransitionToSelf"] = true;
    }

    return j;
}

StateTransition StateTransition::FromJson(const json& j) {
    StateTransition trans;
    trans.targetState = j.value("to", "");
    trans.condition = j.value("condition", "");
    trans.blendDuration = j.value("duration", 0.2f);
    trans.priority = j.value("priority", 0);
    trans.exitTime = j.value("exitTime", -1.0f);
    trans.hasExitTime = j.value("hasExitTime", false);
    trans.canTransitionToSelf = j.value("canTransitionToSelf", false);

    if (j.contains("conditions") && j["conditions"].is_array()) {
        for (const auto& c : j["conditions"]) {
            trans.conditions.push_back(TransitionCondition::FromJson(c));
        }
    }

    return trans;
}

// ============================================================================
// StateBehavior
// ============================================================================

json StateBehavior::ToJson() const {
    json j = {{"type", type}};
    if (!onEnter.empty()) j["onEnter"] = onEnter;
    if (!onExit.empty()) j["onExit"] = onExit;
    if (!onUpdate.empty()) j["onUpdate"] = onUpdate;
    return j;
}

StateBehavior StateBehavior::FromJson(const json& j) {
    StateBehavior behavior;
    behavior.type = j.value("type", "");
    if (j.contains("onEnter")) behavior.onEnter = j["onEnter"].get<std::vector<json>>();
    if (j.contains("onExit")) behavior.onExit = j["onExit"].get<std::vector<json>>();
    if (j.contains("onUpdate")) behavior.onUpdate = j["onUpdate"].get<std::vector<json>>();
    return behavior;
}

// ============================================================================
// AnimationState
// ============================================================================

json AnimationState::ToJson() const {
    json j = {
        {"name", name},
        {"speed", speed},
        {"loop", loop}
    };

    if (!animationClip.empty()) {
        j["clip"] = animationClip;
    }

    if (!blendTreeId.empty()) {
        j["blendTreeId"] = blendTreeId;
    }

    if (!blendTreeConfig.is_null()) {
        j["blendTree"] = blendTreeConfig;
    }

    if (mirror) j["mirror"] = true;
    if (cycleOffset != 0.0f) j["cycleOffset"] = cycleOffset;
    if (!speedMultiplierParameter.empty()) j["speedMultiplierParameter"] = speedMultiplierParameter;
    if (!timeParameter.empty()) j["timeParameter"] = timeParameter;
    if (!mirrorParameter.empty()) j["mirrorParameter"] = mirrorParameter;

    if (!events.empty()) {
        j["events"] = json::array();
        for (const auto& e : events) {
            j["events"].push_back(e.ToJson());
        }
    }

    if (!transitions.empty()) {
        j["transitions"] = json::array();
        for (const auto& t : transitions) {
            j["transitions"].push_back(t.ToJson());
        }
    }

    if (!behaviors.empty()) {
        j["behaviors"] = json::array();
        for (const auto& b : behaviors) {
            j["behaviors"].push_back(b.ToJson());
        }
    }

    return j;
}

AnimationState AnimationState::FromJson(const json& j) {
    AnimationState state;
    state.name = j.value("name", "");
    state.animationClip = j.value("clip", "");
    state.speed = j.value("speed", 1.0f);
    state.loop = j.value("loop", true);
    state.mirror = j.value("mirror", false);
    state.cycleOffset = j.value("cycleOffset", 0.0f);
    state.speedMultiplierParameter = j.value("speedMultiplierParameter", "");
    state.timeParameter = j.value("timeParameter", "");
    state.mirrorParameter = j.value("mirrorParameter", "");
    state.blendTreeId = j.value("blendTreeId", "");

    if (j.contains("blendTree")) {
        state.blendTreeConfig = j["blendTree"];
    }

    if (j.contains("events") && j["events"].is_array()) {
        for (const auto& e : j["events"]) {
            state.events.push_back(AnimationEvent::FromJson(e));
        }
    }

    if (j.contains("transitions") && j["transitions"].is_array()) {
        for (const auto& t : j["transitions"]) {
            state.transitions.push_back(StateTransition::FromJson(t));
        }
    }

    if (j.contains("behaviors") && j["behaviors"].is_array()) {
        for (const auto& b : j["behaviors"]) {
            state.behaviors.push_back(StateBehavior::FromJson(b));
        }
    }

    return state;
}

// ============================================================================
// AnimationParameter
// ============================================================================

json AnimationParameter::ToJson() const {
    std::string typeStr;
    switch (type) {
        case Type::Float: typeStr = "float"; break;
        case Type::Int: typeStr = "int"; break;
        case Type::Bool: typeStr = "bool"; break;
        case Type::Trigger: typeStr = "trigger"; break;
    }

    json j = {
        {"name", name},
        {"type", typeStr}
    };

    std::visit([&j](auto&& val) {
        j["defaultValue"] = val;
    }, defaultValue);

    return j;
}

AnimationParameter AnimationParameter::FromJson(const json& j) {
    AnimationParameter param;
    param.name = j.value("name", "");

    std::string typeStr = j.value("type", "float");
    if (typeStr == "float") param.type = Type::Float;
    else if (typeStr == "int") param.type = Type::Int;
    else if (typeStr == "bool") param.type = Type::Bool;
    else if (typeStr == "trigger") param.type = Type::Trigger;

    if (j.contains("defaultValue")) {
        switch (param.type) {
            case Type::Float:
                param.defaultValue = j["defaultValue"].get<float>();
                param.currentValue = std::get<float>(param.defaultValue);
                break;
            case Type::Int:
                param.defaultValue = j["defaultValue"].get<int>();
                param.currentValue = std::get<int>(param.defaultValue);
                break;
            case Type::Bool:
            case Type::Trigger:
                param.defaultValue = j["defaultValue"].get<bool>();
                param.currentValue = std::get<bool>(param.defaultValue);
                break;
        }
    } else {
        switch (param.type) {
            case Type::Float:
                param.defaultValue = 0.0f;
                param.currentValue = 0.0f;
                break;
            case Type::Int:
                param.defaultValue = 0;
                param.currentValue = 0;
                break;
            case Type::Bool:
            case Type::Trigger:
                param.defaultValue = false;
                param.currentValue = false;
                break;
        }
    }

    return param;
}

// ============================================================================
// AnimationLayer
// ============================================================================

json AnimationLayer::ToJson() const {
    json j = {
        {"name", name},
        {"weight", weight},
        {"blendingMode", blendingMode}
    };

    if (!maskId.empty()) j["mask"] = maskId;
    if (!defaultState.empty()) j["defaultState"] = defaultState;
    if (syncedLayerIndex >= 0) j["syncedLayerIndex"] = syncedLayerIndex;
    if (ikPass) j["IKPass"] = true;

    if (!states.empty()) {
        j["states"] = json::array();
        for (const auto& s : states) {
            j["states"].push_back(s.ToJson());
        }
    }

    return j;
}

AnimationLayer AnimationLayer::FromJson(const json& j) {
    AnimationLayer layer;
    layer.name = j.value("name", "");
    layer.weight = j.value("weight", 1.0f);
    layer.blendingMode = j.value("blendingMode", "override");
    layer.maskId = j.value("mask", "");
    layer.defaultState = j.value("defaultState", "");
    layer.syncedLayerIndex = j.value("syncedLayerIndex", -1);
    layer.ikPass = j.value("IKPass", false);

    if (j.contains("states") && j["states"].is_array()) {
        for (const auto& s : j["states"]) {
            layer.states.push_back(AnimationState::FromJson(s));
        }
    }

    return layer;
}

// ============================================================================
// ConditionExpressionParser
// ============================================================================

bool ConditionExpressionParser::Evaluate(
    const std::string& expression,
    const std::unordered_map<std::string, AnimationParameter>& parameters) const {

    if (expression.empty()) {
        return true;
    }

    try {
        auto tokens = Tokenize(expression);
        auto it = tokens.cbegin();
        float result = ParseExpression(it, tokens.cend(), parameters);
        return result != 0.0f;
    } catch (...) {
        return false;
    }
}

bool ConditionExpressionParser::EvaluateCondition(
    const TransitionCondition& condition,
    const std::unordered_map<std::string, AnimationParameter>& parameters) const {

    auto it = parameters.find(condition.parameter);
    if (it == parameters.end()) {
        return false;
    }

    float value = 0.0f;
    const auto& param = it->second;

    std::visit([&value](auto&& val) {
        if constexpr (std::is_same_v<std::decay_t<decltype(val)>, float>) {
            value = val;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, int>) {
            value = static_cast<float>(val);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, bool>) {
            value = val ? 1.0f : 0.0f;
        }
    }, param.currentValue);

    switch (condition.mode) {
        case TransitionCondition::Mode::IfTrue:
            return value != 0.0f;
        case TransitionCondition::Mode::IfFalse:
            return value == 0.0f;
        case TransitionCondition::Mode::Greater:
            return value > condition.threshold;
        case TransitionCondition::Mode::Less:
            return value < condition.threshold;
        case TransitionCondition::Mode::Equals:
            return std::abs(value - condition.threshold) < 0.0001f;
        case TransitionCondition::Mode::NotEquals:
            return std::abs(value - condition.threshold) >= 0.0001f;
        case TransitionCondition::Mode::GreaterOrEqual:
            return value >= condition.threshold;
        case TransitionCondition::Mode::LessOrEqual:
            return value <= condition.threshold;
    }

    return false;
}

std::vector<ConditionExpressionParser::Token> ConditionExpressionParser::Tokenize(
    const std::string& expr) const {

    std::vector<Token> tokens;
    size_t i = 0;

    while (i < expr.size()) {
        // Skip whitespace
        while (i < expr.size() && std::isspace(expr[i])) ++i;
        if (i >= expr.size()) break;

        Token token;

        // Number
        if (std::isdigit(expr[i]) || (expr[i] == '.' && i + 1 < expr.size() && std::isdigit(expr[i + 1]))) {
            size_t start = i;
            while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.')) ++i;
            token.type = Token::Number;
            token.value = expr.substr(start, i - start);
            token.numValue = std::stof(token.value);
        }
        // Identifier
        else if (std::isalpha(expr[i]) || expr[i] == '_') {
            size_t start = i;
            while (i < expr.size() && (std::isalnum(expr[i]) || expr[i] == '_')) ++i;
            token.type = Token::Identifier;
            token.value = expr.substr(start, i - start);

            // Check for boolean literals
            if (token.value == "true") {
                token.type = Token::Number;
                token.numValue = 1.0f;
            } else if (token.value == "false") {
                token.type = Token::Number;
                token.numValue = 0.0f;
            }
        }
        // Operators
        else if (expr[i] == '(') {
            token.type = Token::LeftParen;
            token.value = "(";
            ++i;
        } else if (expr[i] == ')') {
            token.type = Token::RightParen;
            token.value = ")";
            ++i;
        } else {
            // Multi-char operators
            std::string op;
            if (i + 1 < expr.size()) {
                op = expr.substr(i, 2);
                if (op == "&&" || op == "||" || op == "==" || op == "!=" ||
                    op == ">=" || op == "<=") {
                    token.type = Token::Operator;
                    token.value = op;
                    i += 2;
                    tokens.push_back(token);
                    continue;
                }
            }
            // Single-char operators
            op = expr.substr(i, 1);
            if (op == ">" || op == "<" || op == "+" || op == "-" ||
                op == "*" || op == "/" || op == "!") {
                token.type = Token::Operator;
                token.value = op;
                ++i;
            } else {
                ++i; // Skip unknown character
                continue;
            }
        }

        tokens.push_back(token);
    }

    tokens.push_back({Token::End, "", 0.0f});
    return tokens;
}

float ConditionExpressionParser::ParseExpression(
    std::vector<Token>::const_iterator& it,
    std::vector<Token>::const_iterator end,
    const std::unordered_map<std::string, AnimationParameter>& params) const {

    float left = ParseTerm(it, end, params);

    while (it != end && it->type == Token::Operator &&
           (it->value == "||" || it->value == "&&")) {
        std::string op = it->value;
        ++it;
        float right = ParseTerm(it, end, params);

        if (op == "||") {
            left = (left != 0.0f || right != 0.0f) ? 1.0f : 0.0f;
        } else if (op == "&&") {
            left = (left != 0.0f && right != 0.0f) ? 1.0f : 0.0f;
        }
    }

    return left;
}

float ConditionExpressionParser::ParseTerm(
    std::vector<Token>::const_iterator& it,
    std::vector<Token>::const_iterator end,
    const std::unordered_map<std::string, AnimationParameter>& params) const {

    float left = ParseFactor(it, end, params);

    while (it != end && it->type == Token::Operator &&
           (it->value == ">" || it->value == "<" || it->value == ">=" ||
            it->value == "<=" || it->value == "==" || it->value == "!=")) {
        std::string op = it->value;
        ++it;
        float right = ParseFactor(it, end, params);

        if (op == ">") left = (left > right) ? 1.0f : 0.0f;
        else if (op == "<") left = (left < right) ? 1.0f : 0.0f;
        else if (op == ">=") left = (left >= right) ? 1.0f : 0.0f;
        else if (op == "<=") left = (left <= right) ? 1.0f : 0.0f;
        else if (op == "==") left = (std::abs(left - right) < 0.0001f) ? 1.0f : 0.0f;
        else if (op == "!=") left = (std::abs(left - right) >= 0.0001f) ? 1.0f : 0.0f;
    }

    return left;
}

float ConditionExpressionParser::ParseFactor(
    std::vector<Token>::const_iterator& it,
    std::vector<Token>::const_iterator end,
    const std::unordered_map<std::string, AnimationParameter>& params) const {

    if (it == end || it->type == Token::End) {
        return 0.0f;
    }

    // Handle negation
    if (it->type == Token::Operator && it->value == "!") {
        ++it;
        float value = ParseFactor(it, end, params);
        return value == 0.0f ? 1.0f : 0.0f;
    }

    // Parentheses
    if (it->type == Token::LeftParen) {
        ++it;
        float value = ParseExpression(it, end, params);
        if (it != end && it->type == Token::RightParen) {
            ++it;
        }
        return value;
    }

    // Number
    if (it->type == Token::Number) {
        float value = it->numValue;
        ++it;
        return value;
    }

    // Identifier (parameter name)
    if (it->type == Token::Identifier) {
        float value = GetParameterValue(it->value, params);
        ++it;
        return value;
    }

    return 0.0f;
}

float ConditionExpressionParser::GetParameterValue(
    const std::string& name,
    const std::unordered_map<std::string, AnimationParameter>& params) const {

    auto it = params.find(name);
    if (it == params.end()) {
        return 0.0f;
    }

    float value = 0.0f;
    std::visit([&value](auto&& val) {
        if constexpr (std::is_same_v<std::decay_t<decltype(val)>, float>) {
            value = val;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, int>) {
            value = static_cast<float>(val);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(val)>, bool>) {
            value = val ? 1.0f : 0.0f;
        }
    }, it->second.currentValue);

    return value;
}

// ============================================================================
// DataDrivenStateMachine
// ============================================================================

DataDrivenStateMachine::DataDrivenStateMachine(AnimationController* controller)
    : m_controller(controller) {
}

bool DataDrivenStateMachine::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        json config;
        file >> config;
        m_configPath = filepath;
        return LoadFromJson(config);
    } catch (...) {
        return false;
    }
}

bool DataDrivenStateMachine::LoadFromJson(const json& config) {
    try {
        m_id = config.value("id", "");
        m_name = config.value("name", "");
        m_defaultState = config.value("defaultState", "");

        // Load parameters
        m_parameters.clear();
        if (config.contains("parameters") && config["parameters"].is_array()) {
            for (const auto& p : config["parameters"]) {
                auto param = AnimationParameter::FromJson(p);
                m_parameters[param.name] = param;
            }
        }

        // Load states
        m_states.clear();
        if (config.contains("states") && config["states"].is_array()) {
            for (const auto& s : config["states"]) {
                m_states.push_back(AnimationState::FromJson(s));
            }
        }

        // Load layers
        m_layers.clear();
        if (config.contains("layers") && config["layers"].is_array()) {
            for (const auto& l : config["layers"]) {
                m_layers.push_back(AnimationLayer::FromJson(l));
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool DataDrivenStateMachine::SaveToFile(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << ToJson().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

json DataDrivenStateMachine::ToJson() const {
    json j = {
        {"id", m_id},
        {"name", m_name},
        {"defaultState", m_defaultState}
    };

    // Parameters
    j["parameters"] = json::array();
    for (const auto& [name, param] : m_parameters) {
        j["parameters"].push_back(param.ToJson());
    }

    // States
    j["states"] = json::array();
    for (const auto& state : m_states) {
        j["states"].push_back(state.ToJson());
    }

    // Layers
    if (!m_layers.empty()) {
        j["layers"] = json::array();
        for (const auto& layer : m_layers) {
            j["layers"].push_back(layer.ToJson());
        }
    }

    return j;
}

bool DataDrivenStateMachine::Reload() {
    if (m_configPath.empty()) {
        return false;
    }
    return LoadFromFile(m_configPath);
}

void DataDrivenStateMachine::AddState(const AnimationState& state) {
    m_states.push_back(state);
}

void DataDrivenStateMachine::AddState(AnimationState&& state) {
    m_states.push_back(std::move(state));
}

bool DataDrivenStateMachine::RemoveState(const std::string& name) {
    auto it = std::find_if(m_states.begin(), m_states.end(),
                           [&name](const AnimationState& s) { return s.name == name; });
    if (it != m_states.end()) {
        m_states.erase(it);
        return true;
    }
    return false;
}

AnimationState* DataDrivenStateMachine::GetState(const std::string& name) {
    auto it = std::find_if(m_states.begin(), m_states.end(),
                           [&name](const AnimationState& s) { return s.name == name; });
    return it != m_states.end() ? &(*it) : nullptr;
}

const AnimationState* DataDrivenStateMachine::GetState(const std::string& name) const {
    auto it = std::find_if(m_states.begin(), m_states.end(),
                           [&name](const AnimationState& s) { return s.name == name; });
    return it != m_states.end() ? &(*it) : nullptr;
}

void DataDrivenStateMachine::SetDefaultState(const std::string& stateName) {
    m_defaultState = stateName;
}

void DataDrivenStateMachine::AddLayer(const AnimationLayer& layer) {
    m_layers.push_back(layer);
}

AnimationLayer* DataDrivenStateMachine::GetLayer(const std::string& name) {
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
                           [&name](const AnimationLayer& l) { return l.name == name; });
    return it != m_layers.end() ? &(*it) : nullptr;
}

const AnimationLayer* DataDrivenStateMachine::GetLayer(const std::string& name) const {
    auto it = std::find_if(m_layers.begin(), m_layers.end(),
                           [&name](const AnimationLayer& l) { return l.name == name; });
    return it != m_layers.end() ? &(*it) : nullptr;
}

void DataDrivenStateMachine::SetLayerWeight(const std::string& name, float weight) {
    if (auto* layer = GetLayer(name)) {
        layer->weight = weight;
    }
}

void DataDrivenStateMachine::AddParameter(const AnimationParameter& param) {
    m_parameters[param.name] = param;
}

void DataDrivenStateMachine::SetFloat(const std::string& name, float value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Float) {
        it->second.currentValue = value;
    }
}

void DataDrivenStateMachine::SetInt(const std::string& name, int value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Int) {
        it->second.currentValue = value;
    }
}

void DataDrivenStateMachine::SetBool(const std::string& name, bool value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Bool) {
        it->second.currentValue = value;
    }
}

void DataDrivenStateMachine::SetTrigger(const std::string& name) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Trigger) {
        it->second.currentValue = true;
    }
}

void DataDrivenStateMachine::ResetTrigger(const std::string& name) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Trigger) {
        it->second.currentValue = false;
    }
}

float DataDrivenStateMachine::GetFloat(const std::string& name) const {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Float) {
        return std::get<float>(it->second.currentValue);
    }
    return 0.0f;
}

int DataDrivenStateMachine::GetInt(const std::string& name) const {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Int) {
        return std::get<int>(it->second.currentValue);
    }
    return 0;
}

bool DataDrivenStateMachine::GetBool(const std::string& name) const {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() &&
        (it->second.type == AnimationParameter::Type::Bool ||
         it->second.type == AnimationParameter::Type::Trigger)) {
        return std::get<bool>(it->second.currentValue);
    }
    return false;
}

void DataDrivenStateMachine::Start() {
    m_currentState = m_defaultState;
    m_stateTime = 0.0f;
    m_normalizedTime = 0.0f;
    m_isTransitioning = false;

    // Trigger enter callbacks for initial state
    auto it = m_onEnterCallbacks.find(m_currentState);
    if (it != m_onEnterCallbacks.end()) {
        for (const auto& callback : it->second) {
            callback();
        }
    }

    // Execute state enter behaviors
    if (auto* state = GetState(m_currentState)) {
        ExecuteBehaviors(state->behaviors, "enter");
    }

    // Start animation on controller
    if (m_controller && !m_currentState.empty()) {
        if (auto* state = GetState(m_currentState)) {
            m_controller->Play(state->animationClip, 0.0f, state->loop);
        }
    }
}

void DataDrivenStateMachine::Update(float deltaTime) {
    if (m_currentState.empty()) {
        return;
    }

    m_totalTime += deltaTime;

    // Update transition
    if (m_isTransitioning) {
        m_transitionProgress += deltaTime / m_transitionDuration;
        if (m_transitionProgress >= 1.0f) {
            CompleteTransition();
        }
    }

    // Get current state
    auto* state = GetState(m_currentState);
    if (!state) {
        return;
    }

    // Calculate effective speed
    float speed = state->speed;
    if (!state->speedMultiplierParameter.empty()) {
        speed *= GetFloat(state->speedMultiplierParameter);
    }

    // Update state time
    float previousNormalized = m_normalizedTime;
    m_stateTime += deltaTime * speed;

    // Calculate normalized time
    float duration = GetAnimationDuration(state->animationClip);
    if (duration > 0.0f) {
        if (state->loop) {
            m_normalizedTime = std::fmod(m_stateTime / duration, 1.0f);
        } else {
            m_normalizedTime = std::min(m_stateTime / duration, 1.0f);
        }
    }

    // Update time parameter if specified
    if (!state->timeParameter.empty()) {
        SetFloat(state->timeParameter, m_normalizedTime);
    }

    // Process events
    ProcessStateEvents(previousNormalized, m_normalizedTime);

    // Execute update behaviors
    ExecuteBehaviors(state->behaviors, "update");

    // Evaluate transitions
    EvaluateTransitions();

    // Reset triggers at end of frame
    for (auto& [name, param] : m_parameters) {
        if (param.type == AnimationParameter::Type::Trigger) {
            param.currentValue = false;
        }
    }
}

void DataDrivenStateMachine::ForceState(const std::string& stateName, float blendTime) {
    if (stateName == m_currentState) {
        return;
    }

    StartTransition(stateName, blendTime);
    if (blendTime <= 0.0f) {
        CompleteTransition();
    }
}

void DataDrivenStateMachine::OnStateEnter(const std::string& stateName, std::function<void()> callback) {
    m_onEnterCallbacks[stateName].push_back(std::move(callback));
}

void DataDrivenStateMachine::OnStateExit(const std::string& stateName, std::function<void()> callback) {
    m_onExitCallbacks[stateName].push_back(std::move(callback));
}

json DataDrivenStateMachine::GetDebugInfo() const {
    json info;
    info["currentState"] = m_currentState;
    info["previousState"] = m_previousState;
    info["stateTime"] = m_stateTime;
    info["normalizedTime"] = m_normalizedTime;
    info["isTransitioning"] = m_isTransitioning;
    info["transitionTarget"] = m_transitionTarget;
    info["transitionProgress"] = m_transitionProgress;

    info["parameters"] = json::object();
    for (const auto& [name, param] : m_parameters) {
        std::visit([&info, &name](auto&& val) {
            info["parameters"][name] = val;
        }, param.currentValue);
    }

    return info;
}

void DataDrivenStateMachine::EvaluateTransitions() {
    if (m_isTransitioning) {
        return;
    }

    auto* state = GetState(m_currentState);
    if (!state) {
        return;
    }

    // Sort transitions by priority
    std::vector<const StateTransition*> sortedTransitions;
    for (const auto& trans : state->transitions) {
        sortedTransitions.push_back(&trans);
    }
    std::sort(sortedTransitions.begin(), sortedTransitions.end(),
              [](const StateTransition* a, const StateTransition* b) {
                  return a->priority > b->priority;
              });

    // Evaluate each transition
    for (const auto* trans : sortedTransitions) {
        // Check if can transition to self
        if (trans->targetState == m_currentState && !trans->canTransitionToSelf) {
            continue;
        }

        // Check exit time
        if (trans->hasExitTime && m_normalizedTime < trans->exitTime) {
            continue;
        }

        // Check expression condition
        bool conditionMet = true;
        if (!trans->condition.empty()) {
            conditionMet = m_conditionParser.Evaluate(trans->condition, m_parameters);
        }

        // Check individual conditions
        if (conditionMet && !trans->conditions.empty()) {
            for (const auto& cond : trans->conditions) {
                if (!m_conditionParser.EvaluateCondition(cond, m_parameters)) {
                    conditionMet = false;
                    break;
                }
            }
        }

        if (conditionMet) {
            StartTransition(trans->targetState, trans->blendDuration);
            RecordHistory(m_currentState, trans->targetState, trans->condition);
            break;
        }
    }
}

void DataDrivenStateMachine::ProcessStateEvents(float previousTime, float currentTime) {
    auto* state = GetState(m_currentState);
    if (!state) {
        return;
    }

    for (auto& event : state->events) {
        // Handle looping - check if we crossed the event time
        bool shouldTrigger = false;

        if (currentTime >= previousTime) {
            // Normal forward playback
            shouldTrigger = (event.time > previousTime && event.time <= currentTime);
        } else {
            // Looped - check if event is after previous or before current
            shouldTrigger = (event.time > previousTime || event.time <= currentTime);
        }

        if (shouldTrigger && !event.triggered) {
            event.triggered = true;

            // Dispatch event
            if (m_eventSystem) {
                m_eventSystem->DispatchEvent(event.eventName, event.eventData);
            }
        }

        // Reset triggered flag when we pass the event going backward or loop
        if (currentTime < event.time && previousTime >= event.time) {
            event.triggered = false;
        }
    }
}

void DataDrivenStateMachine::StartTransition(const std::string& targetState, float blendDuration) {
    // Exit current state
    auto exitIt = m_onExitCallbacks.find(m_currentState);
    if (exitIt != m_onExitCallbacks.end()) {
        for (const auto& callback : exitIt->second) {
            callback();
        }
    }

    if (auto* state = GetState(m_currentState)) {
        ExecuteBehaviors(state->behaviors, "exit");
    }

    m_previousState = m_currentState;
    m_transitionTarget = targetState;
    m_transitionDuration = blendDuration;
    m_transitionProgress = 0.0f;
    m_isTransitioning = true;

    // Start blend on controller
    if (m_controller) {
        if (auto* state = GetState(targetState)) {
            m_controller->CrossFade(state->animationClip, blendDuration, state->loop);
        }
    }
}

void DataDrivenStateMachine::CompleteTransition() {
    m_currentState = m_transitionTarget;
    m_stateTime = 0.0f;
    m_normalizedTime = 0.0f;
    m_isTransitioning = false;
    m_transitionProgress = 0.0f;

    ResetEventFlags();

    // Enter new state
    auto enterIt = m_onEnterCallbacks.find(m_currentState);
    if (enterIt != m_onEnterCallbacks.end()) {
        for (const auto& callback : enterIt->second) {
            callback();
        }
    }

    if (auto* state = GetState(m_currentState)) {
        ExecuteBehaviors(state->behaviors, "enter");
    }
}

void DataDrivenStateMachine::RecordHistory(const std::string& from, const std::string& to,
                                            const std::string& trigger) {
    if (!m_recordHistory) {
        return;
    }

    StateHistoryEntry entry;
    entry.fromState = from;
    entry.toState = to;
    entry.trigger = trigger;
    entry.timestamp = m_totalTime;

    // Capture parameter snapshot
    for (const auto& [name, param] : m_parameters) {
        std::visit([&entry, &name](auto&& val) {
            entry.parameters[name] = val;
        }, param.currentValue);
    }

    m_history.push_back(entry);

    // Trim history if too large
    while (m_history.size() > MAX_HISTORY_SIZE) {
        m_history.pop_front();
    }
}

void DataDrivenStateMachine::ExecuteBehaviors(const std::vector<StateBehavior>& behaviors,
                                               const std::string& phase) {
    for (const auto& behavior : behaviors) {
        const std::vector<json>* actions = nullptr;

        if (phase == "enter") {
            actions = &behavior.onEnter;
        } else if (phase == "exit") {
            actions = &behavior.onExit;
        } else if (phase == "update") {
            actions = &behavior.onUpdate;
        }

        if (actions && m_eventSystem) {
            for (const auto& action : *actions) {
                m_eventSystem->DispatchEvent("behavior_action", action);
            }
        }
    }
}

void DataDrivenStateMachine::ResetEventFlags() {
    if (auto* state = GetState(m_currentState)) {
        for (auto& event : state->events) {
            event.triggered = false;
        }
    }
}

float DataDrivenStateMachine::GetAnimationDuration(const std::string& clipName) const {
    if (m_controller) {
        if (auto* anim = m_controller->GetAnimation(clipName)) {
            return anim->GetDuration();
        }
    }
    return 1.0f; // Default duration
}

} // namespace Nova
