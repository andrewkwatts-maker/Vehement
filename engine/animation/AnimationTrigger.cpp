#include "AnimationTrigger.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

// ============================================================================
// ComboInput
// ============================================================================

json ComboInput::ToJson() const {
    return json{
        {"input", inputName},
        {"maxDelay", maxDelay},
        {"mustRelease", mustRelease}
    };
}

ComboInput ComboInput::FromJson(const json& j) {
    ComboInput input;
    input.inputName = j.value("input", "");
    input.maxDelay = j.value("maxDelay", 0.5f);
    input.mustRelease = j.value("mustRelease", false);
    return input;
}

// ============================================================================
// TriggerConfig
// ============================================================================

json TriggerConfig::ToJson() const {
    std::string typeStr;
    switch (type) {
        case TriggerType::Time: typeStr = "time"; break;
        case TriggerType::StateEnter: typeStr = "state_enter"; break;
        case TriggerType::StateExit: typeStr = "state_exit"; break;
        case TriggerType::Property: typeStr = "property"; break;
        case TriggerType::Combo: typeStr = "combo"; break;
        case TriggerType::Custom: typeStr = "custom"; break;
    }

    json j = {
        {"id", id},
        {"name", name},
        {"type", typeStr},
        {"enabled", enabled},
        {"priority", priority},
        {"cooldown", cooldown}
    };

    switch (type) {
        case TriggerType::Time:
            j["time"] = triggerTime;
            j["isFrameBased"] = isFrameBased;
            break;
        case TriggerType::StateEnter:
        case TriggerType::StateExit:
            j["targetState"] = targetState;
            break;
        case TriggerType::Property: {
            j["property"] = propertyName;
            j["threshold"] = threshold;
            std::string modeStr;
            switch (compareMode) {
                case PropertyCompareMode::CrossAbove: modeStr = "cross_above"; break;
                case PropertyCompareMode::CrossBelow: modeStr = "cross_below"; break;
                case PropertyCompareMode::WhileAbove: modeStr = "while_above"; break;
                case PropertyCompareMode::WhileBelow: modeStr = "while_below"; break;
                case PropertyCompareMode::OnChange: modeStr = "on_change"; break;
            }
            j["compareMode"] = modeStr;
            break;
        }
        case TriggerType::Combo:
            j["comboSequence"] = json::array();
            for (const auto& input : comboSequence) {
                j["comboSequence"].push_back(input.ToJson());
            }
            j["comboWindow"] = comboWindow;
            break;
        default:
            break;
    }

    if (!eventName.empty()) {
        j["event"] = eventName;
    }
    if (!eventData.is_null()) {
        j["eventData"] = eventData;
    }
    if (!scriptFunction.empty()) {
        j["script"] = scriptFunction;
    }
    if (!scriptArgs.is_null()) {
        j["scriptArgs"] = scriptArgs;
    }

    return j;
}

TriggerConfig TriggerConfig::FromJson(const json& j) {
    TriggerConfig config;
    config.id = j.value("id", "");
    config.name = j.value("name", "");
    config.enabled = j.value("enabled", true);
    config.priority = j.value("priority", 0);
    config.cooldown = j.value("cooldown", 0.0f);

    std::string typeStr = j.value("type", "time");
    if (typeStr == "time") config.type = TriggerType::Time;
    else if (typeStr == "state_enter") config.type = TriggerType::StateEnter;
    else if (typeStr == "state_exit") config.type = TriggerType::StateExit;
    else if (typeStr == "property") config.type = TriggerType::Property;
    else if (typeStr == "combo") config.type = TriggerType::Combo;
    else if (typeStr == "custom") config.type = TriggerType::Custom;

    config.triggerTime = j.value("time", 0.0f);
    config.isFrameBased = j.value("isFrameBased", false);
    config.targetState = j.value("targetState", "");
    config.propertyName = j.value("property", "");
    config.threshold = j.value("threshold", 0.0f);

    std::string modeStr = j.value("compareMode", "cross_above");
    if (modeStr == "cross_above") config.compareMode = PropertyCompareMode::CrossAbove;
    else if (modeStr == "cross_below") config.compareMode = PropertyCompareMode::CrossBelow;
    else if (modeStr == "while_above") config.compareMode = PropertyCompareMode::WhileAbove;
    else if (modeStr == "while_below") config.compareMode = PropertyCompareMode::WhileBelow;
    else if (modeStr == "on_change") config.compareMode = PropertyCompareMode::OnChange;

    if (j.contains("comboSequence") && j["comboSequence"].is_array()) {
        for (const auto& input : j["comboSequence"]) {
            config.comboSequence.push_back(ComboInput::FromJson(input));
        }
    }
    config.comboWindow = j.value("comboWindow", 1.0f);

    config.eventName = j.value("event", "");
    if (j.contains("eventData")) {
        config.eventData = j["eventData"];
    }
    config.scriptFunction = j.value("script", "");
    if (j.contains("scriptArgs")) {
        config.scriptArgs = j["scriptArgs"];
    }

    return config;
}

// ============================================================================
// AnimationTrigger
// ============================================================================

AnimationTrigger::AnimationTrigger(const TriggerConfig& config) : m_config(config) {}

bool AnimationTrigger::LoadFromJson(const json& j) {
    try {
        m_config = TriggerConfig::FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

json AnimationTrigger::ToJson() const {
    return m_config.ToJson();
}

bool AnimationTrigger::ShouldTrigger(const json& context, float currentTime) {
    if (!m_config.enabled) {
        return false;
    }

    // Check cooldown
    if (m_config.cooldown > 0.0f) {
        if (currentTime - m_state.lastTriggerTime < m_config.cooldown) {
            return false;
        }
    }

    bool shouldFire = false;

    switch (m_config.type) {
        case TriggerType::Time: {
            float normalizedTime = context.value("normalizedTime", 0.0f);
            shouldFire = CheckTimeTrigger(normalizedTime, currentTime);
            break;
        }
        case TriggerType::StateEnter:
        case TriggerType::StateExit: {
            std::string currentState = context.value("currentState", "");
            std::string previousState = context.value("previousState", "");
            shouldFire = CheckStateTrigger(currentState, previousState);
            break;
        }
        case TriggerType::Property:
            shouldFire = CheckPropertyTrigger(currentTime);
            break;
        case TriggerType::Combo:
            shouldFire = CheckComboTrigger(currentTime);
            break;
        case TriggerType::Custom:
            // Custom triggers are handled externally
            shouldFire = false;
            break;
    }

    if (shouldFire) {
        m_state.lastTriggerTime = currentTime;
        m_state.wasTriggered = true;
    }

    return shouldFire;
}

void AnimationTrigger::Reset() {
    m_state = TriggerState{};
}

void AnimationTrigger::ProcessInput(const std::string& inputName, float currentTime) {
    if (m_config.type != TriggerType::Combo) {
        return;
    }

    if (m_state.comboProgress < m_config.comboSequence.size()) {
        const auto& expectedInput = m_config.comboSequence[m_state.comboProgress];

        if (inputName == expectedInput.inputName) {
            // Check timing
            if (m_state.comboProgress > 0) {
                float timeSinceLastInput = currentTime - m_state.lastInputTime;
                if (timeSinceLastInput > expectedInput.maxDelay) {
                    // Combo broken - reset
                    m_state.comboProgress = 0;
                    m_state.inputTimes.clear();
                    return;
                }
            }

            // Progress combo
            m_state.comboProgress++;
            m_state.lastInputTime = currentTime;
            m_state.inputTimes.push_back(currentTime);
        }
    }
}

void AnimationTrigger::SetPropertyValue(float value) {
    m_state.previousValue = m_currentPropertyValue;
    m_currentPropertyValue = value;
}

bool AnimationTrigger::CheckTimeTrigger(float normalizedTime, float currentTime) {
    float triggerPoint = m_config.triggerTime;

    // Handle frame-based triggers
    if (m_config.isFrameBased) {
        // Assume 30 FPS for frame-based triggers
        triggerPoint = m_config.triggerTime / 30.0f;
    }

    // Check if we crossed the trigger point this frame
    float previousNormalized = m_state.previousValue;
    m_state.previousValue = normalizedTime;

    // Handle loop wrapping
    if (normalizedTime < previousNormalized) {
        // Animation looped
        if (triggerPoint > previousNormalized || triggerPoint <= normalizedTime) {
            return true;
        }
    } else {
        // Normal forward playback
        if (triggerPoint > previousNormalized && triggerPoint <= normalizedTime) {
            return true;
        }
    }

    return false;
}

bool AnimationTrigger::CheckStateTrigger(const std::string& currentState,
                                          const std::string& previousState) {
    if (m_config.type == TriggerType::StateEnter) {
        return currentState == m_config.targetState && previousState != m_config.targetState;
    } else if (m_config.type == TriggerType::StateExit) {
        return previousState == m_config.targetState && currentState != m_config.targetState;
    }
    return false;
}

bool AnimationTrigger::CheckPropertyTrigger(float currentTime) {
    float current = m_currentPropertyValue;
    float previous = m_state.previousValue;

    switch (m_config.compareMode) {
        case PropertyCompareMode::CrossAbove:
            return previous < m_config.threshold && current >= m_config.threshold;
        case PropertyCompareMode::CrossBelow:
            return previous > m_config.threshold && current <= m_config.threshold;
        case PropertyCompareMode::WhileAbove:
            return current > m_config.threshold;
        case PropertyCompareMode::WhileBelow:
            return current < m_config.threshold;
        case PropertyCompareMode::OnChange:
            return std::abs(current - previous) > 0.0001f;
    }

    return false;
}

bool AnimationTrigger::CheckComboTrigger(float currentTime) {
    if (m_state.comboProgress >= m_config.comboSequence.size()) {
        // Combo complete!
        bool result = true;

        // Check total window time
        if (!m_state.inputTimes.empty()) {
            float totalTime = currentTime - m_state.inputTimes.front();
            if (totalTime > m_config.comboWindow) {
                result = false;
            }
        }

        // Reset combo state
        m_state.comboProgress = 0;
        m_state.inputTimes.clear();

        return result;
    }

    return false;
}

// ============================================================================
// AnimationTriggerSystem
// ============================================================================

bool AnimationTriggerSystem::LoadFromJson(const json& config) {
    try {
        m_triggers.clear();

        if (config.contains("triggers") && config["triggers"].is_array()) {
            for (const auto& t : config["triggers"]) {
                auto trigger = std::make_unique<AnimationTrigger>();
                if (trigger->LoadFromJson(t)) {
                    m_triggers.push_back(std::move(trigger));
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

json AnimationTriggerSystem::ToJson() const {
    json j;
    j["triggers"] = json::array();
    for (const auto& trigger : m_triggers) {
        j["triggers"].push_back(trigger->ToJson());
    }
    return j;
}

void AnimationTriggerSystem::AddTrigger(const TriggerConfig& config) {
    m_triggers.push_back(std::make_unique<AnimationTrigger>(config));
}

void AnimationTriggerSystem::AddTrigger(std::unique_ptr<AnimationTrigger> trigger) {
    m_triggers.push_back(std::move(trigger));
}

bool AnimationTriggerSystem::RemoveTrigger(const std::string& id) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                           [&id](const auto& t) { return t->GetConfig().id == id; });
    if (it != m_triggers.end()) {
        m_triggers.erase(it);
        return true;
    }
    return false;
}

AnimationTrigger* AnimationTriggerSystem::GetTrigger(const std::string& id) {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                           [&id](const auto& t) { return t->GetConfig().id == id; });
    return it != m_triggers.end() ? it->get() : nullptr;
}

const AnimationTrigger* AnimationTriggerSystem::GetTrigger(const std::string& id) const {
    auto it = std::find_if(m_triggers.begin(), m_triggers.end(),
                           [&id](const auto& t) { return t->GetConfig().id == id; });
    return it != m_triggers.end() ? it->get() : nullptr;
}

void AnimationTriggerSystem::Update(const json& context, float currentTime) {
    m_firedTriggers.clear();

    // Sort by priority
    std::vector<AnimationTrigger*> sortedTriggers;
    for (auto& trigger : m_triggers) {
        sortedTriggers.push_back(trigger.get());
    }
    std::sort(sortedTriggers.begin(), sortedTriggers.end(),
              [](const AnimationTrigger* a, const AnimationTrigger* b) {
                  return a->GetConfig().priority > b->GetConfig().priority;
              });

    // Update property values in triggers
    for (auto* trigger : sortedTriggers) {
        const auto& config = trigger->GetConfig();
        if (config.type == TriggerType::Property) {
            auto it = m_propertyValues.find(config.propertyName);
            if (it != m_propertyValues.end()) {
                trigger->SetPropertyValue(it->second);
            }
        }
    }

    // Check and fire triggers
    for (auto* trigger : sortedTriggers) {
        if (trigger->ShouldTrigger(context, currentTime)) {
            FireTrigger(*trigger, context);
            m_firedTriggers.push_back(trigger->GetConfig().id);
        }
    }
}

void AnimationTriggerSystem::ProcessInput(const std::string& inputName, float currentTime) {
    for (auto& trigger : m_triggers) {
        trigger->ProcessInput(inputName, currentTime);
    }
}

void AnimationTriggerSystem::SetProperty(const std::string& propertyName, float value) {
    m_propertyValues[propertyName] = value;
}

void AnimationTriggerSystem::ResetAll() {
    for (auto& trigger : m_triggers) {
        trigger->Reset();
    }
    m_propertyValues.clear();
    m_firedTriggers.clear();
}

void AnimationTriggerSystem::FireTrigger(AnimationTrigger& trigger, const json& context) {
    const auto& config = trigger.GetConfig();

    // Fire event callback
    if (m_eventCallback && !config.eventName.empty()) {
        json eventContext = config.eventData;
        eventContext["triggerId"] = config.id;
        eventContext["triggerName"] = config.name;

        // Merge animation context
        if (context.is_object()) {
            for (const auto& [key, value] : context.items()) {
                if (!eventContext.contains(key)) {
                    eventContext[key] = value;
                }
            }
        }

        m_eventCallback(eventContext);
    }

    // Fire script callback
    if (m_scriptCallback && !config.scriptFunction.empty()) {
        json args = config.scriptArgs;
        args["triggerId"] = config.id;
        args["context"] = context;

        m_scriptCallback(config.scriptFunction, args);
    }
}

// ============================================================================
// ComboDetector
// ============================================================================

void ComboDetector::AddCombo(const ComboDefinition& combo) {
    m_combos.push_back(combo);
    m_states[combo.id] = ComboState{};
}

void ComboDetector::ProcessInput(const std::string& input, float currentTime) {
    // Add to history
    m_inputHistory.push_back({input, currentTime});
    if (m_inputHistory.size() > MAX_INPUT_HISTORY) {
        m_inputHistory.erase(m_inputHistory.begin());
    }

    // Check each combo
    for (const auto& combo : m_combos) {
        auto& state = m_states[combo.id];

        if (state.completed) {
            continue;
        }

        if (state.progress < combo.sequence.size()) {
            const auto& expected = combo.sequence[state.progress];

            if (input == expected.inputName) {
                // Check timing
                if (state.progress > 0) {
                    float timeSinceLastInput = currentTime - state.lastInputTime;
                    if (timeSinceLastInput > expected.maxDelay) {
                        // Reset combo
                        state.progress = 0;
                        state.lastInputTime = 0.0f;
                        continue;
                    }
                }

                // Progress combo
                state.progress++;
                state.lastInputTime = currentTime;

                // Check if complete
                if (state.progress >= combo.sequence.size()) {
                    state.completed = true;
                    m_completedCombos.push_back(combo.id);
                }
            }
        }
    }
}

void ComboDetector::Update(float currentTime) {
    // Check for expired combos
    for (const auto& combo : m_combos) {
        auto& state = m_states[combo.id];

        if (state.progress > 0 && !state.completed) {
            float timeSinceStart = currentTime - state.lastInputTime;
            if (timeSinceStart > combo.windowTime) {
                // Combo expired
                state.progress = 0;
                state.lastInputTime = 0.0f;
            }
        }
    }
}

std::vector<std::string> ComboDetector::GetCompletedCombos() {
    auto result = std::move(m_completedCombos);
    m_completedCombos.clear();

    // Reset completed combos
    for (const auto& comboId : result) {
        m_states[comboId] = ComboState{};
    }

    return result;
}

void ComboDetector::Reset() {
    for (auto& [id, state] : m_states) {
        state = ComboState{};
    }
    m_completedCombos.clear();
    m_inputHistory.clear();
}

bool ComboDetector::LoadFromJson(const json& config) {
    try {
        m_combos.clear();
        m_states.clear();

        if (config.contains("combos") && config["combos"].is_array()) {
            for (const auto& c : config["combos"]) {
                ComboDefinition combo;
                combo.id = c.value("id", "");
                combo.name = c.value("name", "");
                combo.windowTime = c.value("windowTime", 1.0f);
                combo.onComplete = c.value("onComplete", "");

                if (c.contains("completionData")) {
                    combo.completionData = c["completionData"];
                }

                if (c.contains("sequence") && c["sequence"].is_array()) {
                    for (const auto& input : c["sequence"]) {
                        combo.sequence.push_back(ComboInput::FromJson(input));
                    }
                }

                AddCombo(combo);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

json ComboDetector::ToJson() const {
    json j;
    j["combos"] = json::array();

    for (const auto& combo : m_combos) {
        json c = {
            {"id", combo.id},
            {"name", combo.name},
            {"windowTime", combo.windowTime},
            {"onComplete", combo.onComplete}
        };

        if (!combo.completionData.is_null()) {
            c["completionData"] = combo.completionData;
        }

        c["sequence"] = json::array();
        for (const auto& input : combo.sequence) {
            c["sequence"].push_back(input.ToJson());
        }

        j["combos"].push_back(c);
    }

    return j;
}

// ============================================================================
// TriggerTemplates
// ============================================================================

TriggerConfig TriggerTemplates::CreateFootstep(const std::string& id, float time,
                                                bool isLeftFoot) {
    TriggerConfig config;
    config.id = id;
    config.name = isLeftFoot ? "Left Footstep" : "Right Footstep";
    config.type = TriggerType::Time;
    config.triggerTime = time;
    config.eventName = "footstep";
    config.eventData = {
        {"foot", isLeftFoot ? "left" : "right"}
    };
    return config;
}

TriggerConfig TriggerTemplates::CreateHitFrame(const std::string& id, float time,
                                                const json& hitData) {
    TriggerConfig config;
    config.id = id;
    config.name = "Hit Frame";
    config.type = TriggerType::Time;
    config.triggerTime = time;
    config.eventName = "attack_hit";
    config.eventData = hitData;
    return config;
}

TriggerConfig TriggerTemplates::CreateProjectileSpawn(const std::string& id, float time,
                                                       const std::string& projectileType) {
    TriggerConfig config;
    config.id = id;
    config.name = "Spawn Projectile";
    config.type = TriggerType::Time;
    config.triggerTime = time;
    config.eventName = "spawn_projectile";
    config.eventData = {
        {"type", projectileType}
    };
    return config;
}

TriggerConfig TriggerTemplates::CreateVFXSpawn(const std::string& id, float time,
                                                const std::string& vfxId,
                                                const std::string& bone) {
    TriggerConfig config;
    config.id = id;
    config.name = "Spawn VFX";
    config.type = TriggerType::Time;
    config.triggerTime = time;
    config.eventName = "spawn_vfx";
    config.eventData = {
        {"vfx", vfxId},
        {"bone", bone}
    };
    return config;
}

TriggerConfig TriggerTemplates::CreateSound(const std::string& id, float time,
                                             const std::string& soundId) {
    TriggerConfig config;
    config.id = id;
    config.name = "Play Sound";
    config.type = TriggerType::Time;
    config.triggerTime = time;
    config.eventName = "play_sound";
    config.eventData = {
        {"sound", soundId}
    };
    return config;
}

TriggerConfig TriggerTemplates::CreateStateEnter(const std::string& id,
                                                  const std::string& stateName,
                                                  const std::string& eventName) {
    TriggerConfig config;
    config.id = id;
    config.name = "On Enter " + stateName;
    config.type = TriggerType::StateEnter;
    config.targetState = stateName;
    config.eventName = eventName;
    return config;
}

TriggerConfig TriggerTemplates::CreatePropertyThreshold(const std::string& id,
                                                         const std::string& property,
                                                         float threshold,
                                                         PropertyCompareMode mode) {
    TriggerConfig config;
    config.id = id;
    config.name = "Property Threshold";
    config.type = TriggerType::Property;
    config.propertyName = property;
    config.threshold = threshold;
    config.compareMode = mode;
    config.eventName = "property_trigger";
    config.eventData = {
        {"property", property},
        {"threshold", threshold}
    };
    return config;
}

TriggerConfig TriggerTemplates::CreateCombo(const std::string& id,
                                             const std::vector<std::string>& inputs,
                                             float windowTime) {
    TriggerConfig config;
    config.id = id;
    config.name = "Combo";
    config.type = TriggerType::Combo;
    config.comboWindow = windowTime;

    for (const auto& input : inputs) {
        ComboInput ci;
        ci.inputName = input;
        ci.maxDelay = windowTime / static_cast<float>(inputs.size());
        config.comboSequence.push_back(ci);
    }

    config.eventName = "combo_complete";
    config.eventData = {
        {"comboId", id}
    };
    return config;
}

} // namespace Nova
