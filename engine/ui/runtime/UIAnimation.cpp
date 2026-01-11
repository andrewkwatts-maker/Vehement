#include "UIAnimation.hpp"
#include "HTMLRenderer.hpp"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Engine {
namespace UI {

// AnimationValue implementation
AnimationValue AnimationValue::FromNumber(float value) {
    AnimationValue av;
    av.type = Type::Number;
    av.numberValue = value;
    return av;
}

AnimationValue AnimationValue::FromColor(float r, float g, float b, float a) {
    AnimationValue av;
    av.type = Type::Color;
    av.colorR = r;
    av.colorG = g;
    av.colorB = b;
    av.colorA = a;
    return av;
}

AnimationValue AnimationValue::FromString(const std::string& value) {
    AnimationValue av;
    av.type = Type::String;
    av.stringValue = value;
    return av;
}

AnimationValue AnimationValue::Interpolate(const AnimationValue& target, float t) const {
    AnimationValue result;

    if (type == Type::Number && target.type == Type::Number) {
        result.type = Type::Number;
        result.numberValue = numberValue + (target.numberValue - numberValue) * t;
    }
    else if (type == Type::Color && target.type == Type::Color) {
        result.type = Type::Color;
        result.colorR = colorR + (target.colorR - colorR) * t;
        result.colorG = colorG + (target.colorG - colorG) * t;
        result.colorB = colorB + (target.colorB - colorB) * t;
        result.colorA = colorA + (target.colorA - colorA) * t;
    }
    else {
        // For strings, switch at midpoint
        result.type = Type::String;
        result.stringValue = (t < 0.5f) ? stringValue : target.stringValue;
    }

    return result;
}

// UIAnimation implementation
UIAnimation::UIAnimation() = default;
UIAnimation::~UIAnimation() { Shutdown(); }

void UIAnimation::Initialize() {
    if (m_initialized) return;

    // Register default animations
    AnimationDefinition fadeIn;
    fadeIn.name = "fadeIn";
    fadeIn.duration = 0.3f;
    fadeIn.keyframes = {
        {0.0f, "opacity", AnimationValue::FromNumber(0), EasingFunction::EaseOut},
        {1.0f, "opacity", AnimationValue::FromNumber(1), EasingFunction::EaseOut}
    };
    RegisterAnimation(fadeIn);

    AnimationDefinition fadeOut;
    fadeOut.name = "fadeOut";
    fadeOut.duration = 0.3f;
    fadeOut.keyframes = {
        {0.0f, "opacity", AnimationValue::FromNumber(1), EasingFunction::EaseIn},
        {1.0f, "opacity", AnimationValue::FromNumber(0), EasingFunction::EaseIn}
    };
    RegisterAnimation(fadeOut);

    AnimationDefinition slideInLeft;
    slideInLeft.name = "slideInLeft";
    slideInLeft.duration = 0.4f;
    slideInLeft.keyframes = {
        {0.0f, "translateX", AnimationValue::FromNumber(-100), EasingFunction::EaseOut},
        {0.0f, "opacity", AnimationValue::FromNumber(0), EasingFunction::EaseOut},
        {1.0f, "translateX", AnimationValue::FromNumber(0), EasingFunction::EaseOut},
        {1.0f, "opacity", AnimationValue::FromNumber(1), EasingFunction::EaseOut}
    };
    RegisterAnimation(slideInLeft);

    AnimationDefinition slideInRight;
    slideInRight.name = "slideInRight";
    slideInRight.duration = 0.4f;
    slideInRight.keyframes = {
        {0.0f, "translateX", AnimationValue::FromNumber(100), EasingFunction::EaseOut},
        {0.0f, "opacity", AnimationValue::FromNumber(0), EasingFunction::EaseOut},
        {1.0f, "translateX", AnimationValue::FromNumber(0), EasingFunction::EaseOut},
        {1.0f, "opacity", AnimationValue::FromNumber(1), EasingFunction::EaseOut}
    };
    RegisterAnimation(slideInRight);

    AnimationDefinition scaleIn;
    scaleIn.name = "scaleIn";
    scaleIn.duration = 0.3f;
    scaleIn.keyframes = {
        {0.0f, "scale", AnimationValue::FromNumber(0), EasingFunction::EaseOutBack},
        {1.0f, "scale", AnimationValue::FromNumber(1), EasingFunction::EaseOutBack}
    };
    RegisterAnimation(scaleIn);

    AnimationDefinition bounce;
    bounce.name = "bounce";
    bounce.duration = 1.0f;
    bounce.keyframes = {
        {0.0f, "translateY", AnimationValue::FromNumber(0), EasingFunction::Linear},
        {0.2f, "translateY", AnimationValue::FromNumber(-30), EasingFunction::EaseOut},
        {0.4f, "translateY", AnimationValue::FromNumber(0), EasingFunction::EaseIn},
        {0.6f, "translateY", AnimationValue::FromNumber(-15), EasingFunction::EaseOut},
        {0.8f, "translateY", AnimationValue::FromNumber(0), EasingFunction::EaseIn},
        {1.0f, "translateY", AnimationValue::FromNumber(0), EasingFunction::Linear}
    };
    RegisterAnimation(bounce);

    AnimationDefinition shake;
    shake.name = "shake";
    shake.duration = 0.5f;
    shake.keyframes = {
        {0.0f, "translateX", AnimationValue::FromNumber(0), EasingFunction::Linear},
        {0.1f, "translateX", AnimationValue::FromNumber(-10), EasingFunction::Linear},
        {0.2f, "translateX", AnimationValue::FromNumber(10), EasingFunction::Linear},
        {0.3f, "translateX", AnimationValue::FromNumber(-10), EasingFunction::Linear},
        {0.4f, "translateX", AnimationValue::FromNumber(10), EasingFunction::Linear},
        {0.5f, "translateX", AnimationValue::FromNumber(0), EasingFunction::Linear}
    };
    RegisterAnimation(shake);

    AnimationDefinition pulse;
    pulse.name = "pulse";
    pulse.duration = 1.0f;
    pulse.iterations = -1;
    pulse.keyframes = {
        {0.0f, "scale", AnimationValue::FromNumber(1), EasingFunction::EaseInOut},
        {0.5f, "scale", AnimationValue::FromNumber(1.1f), EasingFunction::EaseInOut},
        {1.0f, "scale", AnimationValue::FromNumber(1), EasingFunction::EaseInOut}
    };
    RegisterAnimation(pulse);

    m_initialized = true;
}

void UIAnimation::Shutdown() {
    Clear();
    m_definitions.clear();
    m_customEasings.clear();
    m_initialized = false;
}

void UIAnimation::Update(float deltaTime) {
    std::vector<std::string> completedInstances;

    for (auto& [id, instance] : m_instances) {
        if (!instance.playing) continue;

        UpdateInstance(instance, deltaTime);

        if (instance.completed) {
            completedInstances.push_back(id);
        }
    }

    // Remove completed instances
    for (const auto& id : completedInstances) {
        auto it = m_instances.find(id);
        if (it != m_instances.end()) {
            if (it->second.onComplete) {
                it->second.onComplete();
            }
            m_instances.erase(it);
        }
    }

    // Update sequences
    for (auto& [name, sequence] : m_sequences) {
        if (!sequence.playing) continue;

        // Check if current animation is done
        // If so, start next in sequence
    }
}

void UIAnimation::RegisterAnimation(const AnimationDefinition& animation) {
    m_definitions[animation.name] = animation;
}

void UIAnimation::LoadAnimationFromJSON(const std::string& json) {
    // Parse JSON and create animation definition
    // This would use a JSON library in production
}

void UIAnimation::LoadAnimationsFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    LoadAnimationFromJSON(buffer.str());
}

const AnimationDefinition* UIAnimation::GetAnimation(const std::string& name) const {
    auto it = m_definitions.find(name);
    if (it != m_definitions.end()) {
        return &it->second;
    }
    return nullptr;
}

void UIAnimation::RemoveAnimation(const std::string& name) {
    m_definitions.erase(name);
}

std::string UIAnimation::Play(const std::string& animationName, const std::string& targetId) {
    auto* def = GetAnimation(animationName);
    if (!def) return "";

    std::string instanceId = "anim_" + std::to_string(m_nextInstanceId++);

    AnimationInstance instance;
    instance.id = instanceId;
    instance.animationName = animationName;
    instance.targetId = targetId;
    instance.currentTime = 0;
    instance.playing = true;

    m_instances[instanceId] = instance;

    if (instance.onStart) {
        instance.onStart();
    }

    return instanceId;
}

std::string UIAnimation::PlayOnElement(const std::string& animationName, DOMElement* element) {
    auto* def = GetAnimation(animationName);
    if (!def) return "";

    std::string instanceId = "anim_" + std::to_string(m_nextInstanceId++);

    AnimationInstance instance;
    instance.id = instanceId;
    instance.animationName = animationName;
    instance.target = element;
    instance.currentTime = 0;
    instance.playing = true;

    m_instances[instanceId] = instance;

    return instanceId;
}

void UIAnimation::Pause(const std::string& instanceId) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.playing = false;
    }
}

void UIAnimation::Resume(const std::string& instanceId) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.playing = true;
    }
}

void UIAnimation::Stop(const std::string& instanceId, bool reset) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        if (reset) {
            it->second.currentTime = 0;
        }
        m_instances.erase(it);
    }
}

void UIAnimation::StopAll(const std::string& targetId) {
    if (targetId.empty()) {
        m_instances.clear();
    } else {
        for (auto it = m_instances.begin(); it != m_instances.end();) {
            if (it->second.targetId == targetId) {
                it = m_instances.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void UIAnimation::Reverse(const std::string& instanceId) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.reversed = !it->second.reversed;
    }
}

void UIAnimation::SetSpeed(const std::string& instanceId, float speed) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.speed = speed;
    }
}

void UIAnimation::Seek(const std::string& instanceId, float time) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.currentTime = time;
    }
}

bool UIAnimation::IsPlaying(const std::string& instanceId) const {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        return it->second.playing;
    }
    return false;
}

float UIAnimation::GetProgress(const std::string& instanceId) const {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        auto* def = GetAnimation(it->second.animationName);
        if (def && def->duration > 0) {
            return it->second.currentTime / def->duration;
        }
    }
    return 0;
}

std::string UIAnimation::Transition(const std::string& targetId, const std::string& property,
                                   const AnimationValue& toValue, float duration,
                                   EasingFunction easing) {
    // Create a temporary animation for this transition
    AnimationDefinition def;
    def.name = "transition_" + std::to_string(m_nextInstanceId);
    def.duration = duration;
    def.keyframes = {
        {0.0f, property, AnimationValue::FromNumber(0), easing}, // Start value would be current
        {1.0f, property, toValue, easing}
    };

    RegisterAnimation(def);
    return Play(def.name, targetId);
}

std::string UIAnimation::TransitionMultiple(const std::string& targetId,
                                           const std::vector<std::pair<std::string, AnimationValue>>& properties,
                                           float duration, EasingFunction easing) {
    AnimationDefinition def;
    def.name = "transition_multi_" + std::to_string(m_nextInstanceId);
    def.duration = duration;

    for (const auto& [prop, value] : properties) {
        def.keyframes.push_back({0.0f, prop, AnimationValue::FromNumber(0), easing});
        def.keyframes.push_back({1.0f, prop, value, easing});
    }

    RegisterAnimation(def);
    return Play(def.name, targetId);
}

void UIAnimation::CancelTransition(const std::string& transitionId) {
    Stop(transitionId, false);
}

void UIAnimation::CreateSequence(const std::string& name, const std::vector<std::string>& animations) {
    AnimationSequence sequence;
    sequence.name = name;
    sequence.animations = animations;
    m_sequences[name] = sequence;
}

std::string UIAnimation::PlaySequence(const std::string& name, const std::string& targetId) {
    auto it = m_sequences.find(name);
    if (it == m_sequences.end() || it->second.animations.empty()) {
        return "";
    }

    it->second.currentIndex = 0;
    it->second.playing = true;

    return Play(it->second.animations[0], targetId);
}

void UIAnimation::CreateGroup(const std::string& name, const std::vector<std::string>& animations) {
    AnimationGroup group;
    group.name = name;
    group.animations = animations;
    m_groups[name] = group;
}

std::string UIAnimation::PlayGroup(const std::string& name, const std::string& targetId) {
    auto it = m_groups.find(name);
    if (it == m_groups.end()) {
        return "";
    }

    it->second.playing = true;
    std::string firstId;

    for (const auto& animName : it->second.animations) {
        auto id = Play(animName, targetId);
        if (firstId.empty()) firstId = id;
    }

    return firstId;
}

void UIAnimation::AddTrigger(const std::string& eventName, const std::string& animationName,
                            const std::string& targetSelector) {
    AnimationTrigger trigger;
    trigger.eventName = eventName;
    trigger.animationName = animationName;
    trigger.targetSelector = targetSelector;
    trigger.enabled = true;

    m_triggers.push_back(trigger);
}

void UIAnimation::RemoveTrigger(const std::string& eventName) {
    m_triggers.erase(
        std::remove_if(m_triggers.begin(), m_triggers.end(),
            [&](const AnimationTrigger& t) { return t.eventName == eventName; }),
        m_triggers.end());
}

void UIAnimation::SetTriggerEnabled(const std::string& eventName, bool enabled) {
    for (auto& trigger : m_triggers) {
        if (trigger.eventName == eventName) {
            trigger.enabled = enabled;
        }
    }
}

void UIAnimation::HandleEvent(const std::string& eventName, const std::string& data) {
    for (const auto& trigger : m_triggers) {
        if (trigger.enabled && trigger.eventName == eventName) {
            Play(trigger.animationName, trigger.targetSelector);
        }
    }
}

EasingFunction UIAnimation::GetEasingByName(const std::string& name) {
    static const std::unordered_map<std::string, EasingFunction> easingMap = {
        {"linear", EasingFunction::Linear},
        {"ease", EasingFunction::EaseInOut},
        {"ease-in", EasingFunction::EaseIn},
        {"ease-out", EasingFunction::EaseOut},
        {"ease-in-out", EasingFunction::EaseInOut},
        {"ease-in-quad", EasingFunction::EaseInQuad},
        {"ease-out-quad", EasingFunction::EaseOutQuad},
        {"ease-in-out-quad", EasingFunction::EaseInOutQuad},
        {"ease-in-cubic", EasingFunction::EaseInCubic},
        {"ease-out-cubic", EasingFunction::EaseOutCubic},
        {"ease-in-out-cubic", EasingFunction::EaseInOutCubic},
        {"ease-in-elastic", EasingFunction::EaseInElastic},
        {"ease-out-elastic", EasingFunction::EaseOutElastic},
        {"ease-in-out-elastic", EasingFunction::EaseInOutElastic},
        {"ease-in-bounce", EasingFunction::EaseInBounce},
        {"ease-out-bounce", EasingFunction::EaseOutBounce},
        {"ease-in-out-bounce", EasingFunction::EaseInOutBounce},
        {"ease-in-back", EasingFunction::EaseInBack},
        {"ease-out-back", EasingFunction::EaseOutBack},
        {"ease-in-out-back", EasingFunction::EaseInOutBack}
    };

    auto it = easingMap.find(name);
    if (it != easingMap.end()) {
        return it->second;
    }
    return EasingFunction::Linear;
}

float UIAnimation::ApplyEasing(EasingFunction easing, float t) {
    const float PI = 3.14159265359f;
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    const float c3 = c1 + 1;
    const float c4 = (2 * PI) / 3;
    const float c5 = (2 * PI) / 4.5f;

    t = std::max(0.0f, std::min(1.0f, t));

    switch (easing) {
        case EasingFunction::Linear:
            return t;

        case EasingFunction::EaseIn:
        case EasingFunction::EaseInQuad:
            return t * t;

        case EasingFunction::EaseOut:
        case EasingFunction::EaseOutQuad:
            return 1 - (1 - t) * (1 - t);

        case EasingFunction::EaseInOut:
        case EasingFunction::EaseInOutQuad:
            return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;

        case EasingFunction::EaseInCubic:
            return t * t * t;

        case EasingFunction::EaseOutCubic:
            return 1 - std::pow(1 - t, 3);

        case EasingFunction::EaseInOutCubic:
            return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;

        case EasingFunction::EaseInQuart:
            return t * t * t * t;

        case EasingFunction::EaseOutQuart:
            return 1 - std::pow(1 - t, 4);

        case EasingFunction::EaseInOutQuart:
            return t < 0.5f ? 8 * t * t * t * t : 1 - std::pow(-2 * t + 2, 4) / 2;

        case EasingFunction::EaseInQuint:
            return t * t * t * t * t;

        case EasingFunction::EaseOutQuint:
            return 1 - std::pow(1 - t, 5);

        case EasingFunction::EaseInOutQuint:
            return t < 0.5f ? 16 * t * t * t * t * t : 1 - std::pow(-2 * t + 2, 5) / 2;

        case EasingFunction::EaseInSine:
            return 1 - std::cos((t * PI) / 2);

        case EasingFunction::EaseOutSine:
            return std::sin((t * PI) / 2);

        case EasingFunction::EaseInOutSine:
            return -(std::cos(PI * t) - 1) / 2;

        case EasingFunction::EaseInExpo:
            return t == 0 ? 0 : std::pow(2, 10 * t - 10);

        case EasingFunction::EaseOutExpo:
            return t == 1 ? 1 : 1 - std::pow(2, -10 * t);

        case EasingFunction::EaseInOutExpo:
            return t == 0 ? 0 : t == 1 ? 1 : t < 0.5f ?
                std::pow(2, 20 * t - 10) / 2 : (2 - std::pow(2, -20 * t + 10)) / 2;

        case EasingFunction::EaseInCirc:
            return 1 - std::sqrt(1 - std::pow(t, 2));

        case EasingFunction::EaseOutCirc:
            return std::sqrt(1 - std::pow(t - 1, 2));

        case EasingFunction::EaseInOutCirc:
            return t < 0.5f ?
                (1 - std::sqrt(1 - std::pow(2 * t, 2))) / 2 :
                (std::sqrt(1 - std::pow(-2 * t + 2, 2)) + 1) / 2;

        case EasingFunction::EaseInElastic:
            return t == 0 ? 0 : t == 1 ? 1 :
                -std::pow(2, 10 * t - 10) * std::sin((t * 10 - 10.75f) * c4);

        case EasingFunction::EaseOutElastic:
            return t == 0 ? 0 : t == 1 ? 1 :
                std::pow(2, -10 * t) * std::sin((t * 10 - 0.75f) * c4) + 1;

        case EasingFunction::EaseInOutElastic:
            return t == 0 ? 0 : t == 1 ? 1 : t < 0.5f ?
                -(std::pow(2, 20 * t - 10) * std::sin((20 * t - 11.125f) * c5)) / 2 :
                (std::pow(2, -20 * t + 10) * std::sin((20 * t - 11.125f) * c5)) / 2 + 1;

        case EasingFunction::EaseInBack:
            return c3 * t * t * t - c1 * t * t;

        case EasingFunction::EaseOutBack:
            return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);

        case EasingFunction::EaseInOutBack:
            return t < 0.5f ?
                (std::pow(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2 :
                (std::pow(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;

        case EasingFunction::EaseOutBounce: {
            const float n1 = 7.5625f;
            const float d1 = 2.75f;
            if (t < 1 / d1) {
                return n1 * t * t;
            } else if (t < 2 / d1) {
                t -= 1.5f / d1;
                return n1 * t * t + 0.75f;
            } else if (t < 2.5f / d1) {
                t -= 2.25f / d1;
                return n1 * t * t + 0.9375f;
            } else {
                t -= 2.625f / d1;
                return n1 * t * t + 0.984375f;
            }
        }

        case EasingFunction::EaseInBounce:
            return 1 - ApplyEasing(EasingFunction::EaseOutBounce, 1 - t);

        case EasingFunction::EaseInOutBounce:
            return t < 0.5f ?
                (1 - ApplyEasing(EasingFunction::EaseOutBounce, 1 - 2 * t)) / 2 :
                (1 + ApplyEasing(EasingFunction::EaseOutBounce, 2 * t - 1)) / 2;

        default:
            return t;
    }
}

void UIAnimation::RegisterCustomEasing(const std::string& name, std::function<float(float)> fn) {
    m_customEasings[name] = fn;
}

void UIAnimation::OnAnimationStart(const std::string& instanceId, std::function<void()> callback) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.onStart = callback;
    }
}

void UIAnimation::OnAnimationComplete(const std::string& instanceId, std::function<void()> callback) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.onComplete = callback;
    }
}

void UIAnimation::OnAnimationIteration(const std::string& instanceId, std::function<void(int)> callback) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.onIteration = callback;
    }
}

void UIAnimation::OnAnimationUpdate(const std::string& instanceId, std::function<void(float)> callback) {
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end()) {
        it->second.onUpdate = callback;
    }
}

std::vector<std::string> UIAnimation::GetRunningAnimations() const {
    std::vector<std::string> result;
    for (const auto& [id, instance] : m_instances) {
        if (instance.playing) {
            result.push_back(id);
        }
    }
    return result;
}

void UIAnimation::Clear() {
    m_instances.clear();
    m_sequences.clear();
    m_groups.clear();
    m_triggers.clear();
}

void UIAnimation::UpdateInstance(AnimationInstance& instance, float deltaTime) {
    auto* def = GetAnimation(instance.animationName);
    if (!def) {
        instance.completed = true;
        return;
    }

    // Update time
    float timeIncrement = deltaTime * instance.speed;
    if (instance.reversed) {
        instance.currentTime -= timeIncrement;
    } else {
        instance.currentTime += timeIncrement;
    }

    // Handle delay
    if (instance.currentTime < def->delay) {
        return;
    }

    float effectiveTime = instance.currentTime - def->delay;
    float progress = effectiveTime / def->duration;

    // Handle iteration
    if (progress >= 1.0f) {
        instance.currentIteration++;

        if (instance.onIteration) {
            instance.onIteration(instance.currentIteration);
        }

        if (def->iterations > 0 && instance.currentIteration >= def->iterations) {
            instance.completed = true;
            progress = 1.0f;
        } else {
            // Reset for next iteration
            if (def->alternate) {
                instance.reversed = !instance.reversed;
            }
            instance.currentTime = def->delay;
            progress = 0.0f;
        }
    }

    progress = std::max(0.0f, std::min(1.0f, progress));

    // Apply eased progress to keyframes
    float easedProgress = ApplyEasing(def->defaultEasing, progress);

    // Update callback
    if (instance.onUpdate) {
        instance.onUpdate(progress);
    }

    // Apply keyframe values to target
    // (In production, this would update DOM element properties)
}

void UIAnimation::ApplyKeyframe(AnimationInstance& instance, const Keyframe& keyframe, float t) {
    if (!instance.target) return;

    // Apply the interpolated value to the target element
    // This would update the appropriate style property
}

AnimationValue UIAnimation::InterpolateKeyframes(const std::vector<Keyframe>& keyframes,
                                                const std::string& property, float time) {
    // Find keyframes for this property
    std::vector<const Keyframe*> propKeyframes;
    for (const auto& kf : keyframes) {
        if (kf.property == property) {
            propKeyframes.push_back(&kf);
        }
    }

    if (propKeyframes.empty()) {
        return AnimationValue::FromNumber(0);
    }

    if (propKeyframes.size() == 1) {
        return propKeyframes[0]->value;
    }

    // Sort by time
    std::sort(propKeyframes.begin(), propKeyframes.end(),
        [](const Keyframe* a, const Keyframe* b) { return a->time < b->time; });

    // Find surrounding keyframes
    const Keyframe* prev = propKeyframes[0];
    const Keyframe* next = propKeyframes[0];

    for (size_t i = 0; i < propKeyframes.size(); ++i) {
        if (propKeyframes[i]->time <= time) {
            prev = propKeyframes[i];
        }
        if (propKeyframes[i]->time >= time) {
            next = propKeyframes[i];
            break;
        }
    }

    if (prev == next || prev->time == next->time) {
        return prev->value;
    }

    // Interpolate
    float localT = (time - prev->time) / (next->time - prev->time);
    float easedT = ApplyEasing(next->easing, localT);

    return prev->value.Interpolate(next->value, easedT);
}

} // namespace UI
} // namespace Engine
