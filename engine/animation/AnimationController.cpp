#include "animation/AnimationController.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

AnimationController::AnimationController(Skeleton* skeleton)
    : m_skeleton(skeleton)
{}

void AnimationController::AddAnimation(const std::string& name, std::shared_ptr<Animation> animation) {
    m_animations[name] = std::move(animation);
}

Animation* AnimationController::GetAnimation(const std::string& name) {
    auto it = m_animations.find(name);
    return it != m_animations.end() ? it->second.get() : nullptr;
}

const Animation* AnimationController::GetAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    return it != m_animations.end() ? it->second.get() : nullptr;
}

void AnimationController::Play(const std::string& name, float blendTime, bool looping) {
    const Animation* anim = GetAnimation(name);
    if (!anim) {
        return;
    }

    // Mark existing animations for blend out
    for (auto& instance : m_activeAnimations) {
        if (instance.state == AnimationState::Playing ||
            instance.state == AnimationState::BlendingIn) {
            instance.state = AnimationState::BlendingOut;
            instance.targetWeight = 0.0f;
            instance.blendSpeed = blendTime > 0.0f ? 1.0f / blendTime : 100.0f;
        }
    }

    // Create new animation instance
    AnimationInstance newInstance;
    newInstance.animation = anim;
    newInstance.currentTime = 0.0f;
    newInstance.playbackSpeed = m_playbackSpeed;
    newInstance.looping = looping;

    if (blendTime > 0.0f) {
        newInstance.weight = 0.0f;
        newInstance.targetWeight = 1.0f;
        newInstance.blendSpeed = 1.0f / blendTime;
        newInstance.state = AnimationState::BlendingIn;
    } else {
        newInstance.weight = 1.0f;
        newInstance.targetWeight = 1.0f;
        newInstance.state = AnimationState::Playing;
    }

    m_activeAnimations.push_back(newInstance);
    m_currentAnimationName = name;
    m_playing = true;
}

void AnimationController::CrossFade(const std::string& name, float fadeTime, bool looping) {
    Play(name, fadeTime, looping);
}

void AnimationController::Stop(float blendOutTime) {
    for (auto& instance : m_activeAnimations) {
        instance.state = AnimationState::BlendingOut;
        instance.targetWeight = 0.0f;
        instance.blendSpeed = blendOutTime > 0.0f ? 1.0f / blendOutTime : 100.0f;
    }

    if (blendOutTime <= 0.0f) {
        m_activeAnimations.clear();
        m_playing = false;
        m_currentAnimationName.clear();
    }
}

void AnimationController::Pause() {
    for (auto& instance : m_activeAnimations) {
        if (instance.state == AnimationState::Playing) {
            instance.state = AnimationState::Paused;
        }
    }
    m_playing = false;
}

void AnimationController::Resume() {
    for (auto& instance : m_activeAnimations) {
        if (instance.state == AnimationState::Paused) {
            instance.state = AnimationState::Playing;
        }
    }
    m_playing = !m_activeAnimations.empty();
}

void AnimationController::Update(float deltaTime) {
    if (!m_playing && m_activeAnimations.empty()) {
        return;
    }

    // Update each animation instance
    for (auto& instance : m_activeAnimations) {
        UpdateInstance(instance, deltaTime);
    }

    // Blend animations together
    BlendAnimations();

    // Remove finished animations
    CleanupFinishedAnimations();
}

void AnimationController::UpdateInstance(AnimationInstance& instance, float deltaTime) {
    if (!instance.animation) {
        return;
    }

    // Update weight blending
    switch (instance.state) {
        case AnimationState::BlendingIn:
            instance.weight += instance.blendSpeed * deltaTime;
            if (instance.weight >= instance.targetWeight) {
                instance.weight = instance.targetWeight;
                instance.state = AnimationState::Playing;
            }
            break;

        case AnimationState::BlendingOut:
            instance.weight -= instance.blendSpeed * deltaTime;
            if (instance.weight <= 0.0f) {
                instance.weight = 0.0f;
                instance.state = AnimationState::Stopped;
            }
            break;

        case AnimationState::Paused:
        case AnimationState::Stopped:
            return;

        default:
            break;
    }

    // Update time
    float actualDelta = deltaTime * instance.playbackSpeed * m_playbackSpeed;
    instance.currentTime += actualDelta;

    float duration = instance.animation->GetDuration();
    if (duration > 0.0f) {
        if (instance.currentTime >= duration) {
            if (instance.looping) {
                instance.currentTime = std::fmod(instance.currentTime, duration);
                if (instance.onLoop) {
                    instance.onLoop();
                }
            } else {
                instance.currentTime = duration;
                instance.state = AnimationState::Stopped;
                if (instance.onAnimationEnd) {
                    instance.onAnimationEnd();
                }
            }
        }
    }
}

void AnimationController::BlendAnimations() {
    m_blendedTransforms.clear();

    // Calculate total weight for normalization
    float totalWeight = 0.0f;
    for (const auto& instance : m_activeAnimations) {
        if (instance.weight > 0.0f && instance.animation) {
            totalWeight += instance.weight;
        }
    }

    if (totalWeight <= 0.0f) {
        return;
    }

    // First pass: accumulate weighted transforms
    bool firstAnimation = true;
    for (const auto& instance : m_activeAnimations) {
        if (instance.weight <= 0.0f || !instance.animation) {
            continue;
        }

        float normalizedWeight = instance.weight / totalWeight;
        auto transforms = instance.animation->Evaluate(instance.currentTime);

        if (firstAnimation) {
            // First animation: just copy with weight
            for (const auto& [boneName, transform] : transforms) {
                m_blendedTransforms[boneName] = transform;
            }
            firstAnimation = false;
        } else {
            // Subsequent animations: blend based on mode
            for (const auto& [boneName, transform] : transforms) {
                auto it = m_blendedTransforms.find(boneName);
                if (it != m_blendedTransforms.end()) {
                    it->second = BlendTransforms(it->second, transform, normalizedWeight);
                } else {
                    m_blendedTransforms[boneName] = transform;
                }
            }
        }
    }
}

void AnimationController::CleanupFinishedAnimations() {
    m_activeAnimations.erase(
        std::remove_if(m_activeAnimations.begin(), m_activeAnimations.end(),
            [](const AnimationInstance& instance) {
                return instance.state == AnimationState::Stopped && instance.weight <= 0.0f;
            }),
        m_activeAnimations.end()
    );

    if (m_activeAnimations.empty()) {
        m_playing = false;
    }
}

std::vector<glm::mat4> AnimationController::GetBoneMatrices() const {
    if (!m_skeleton) {
        return {};
    }
    return m_skeleton->CalculateBoneMatrices(m_blendedTransforms);
}

void AnimationController::GetBoneMatricesInto(std::span<glm::mat4> outMatrices) const {
    if (!m_skeleton) {
        return;
    }
    m_skeleton->CalculateBoneMatricesInto(m_blendedTransforms, outMatrices);
}

float AnimationController::GetCurrentTime() const {
    if (!m_activeAnimations.empty()) {
        return m_activeAnimations.back().currentTime;
    }
    return 0.0f;
}

void AnimationController::SetCurrentTime(float time) {
    for (auto& instance : m_activeAnimations) {
        instance.currentTime = time;
        if (instance.animation) {
            instance.animation->ResetCaches();
        }
    }
}

void AnimationController::SetLayerWeight(size_t layerIndex, float weight) {
    if (layerIndex >= m_layerWeights.size()) {
        m_layerWeights.resize(layerIndex + 1, 1.0f);
    }
    m_layerWeights[layerIndex] = std::clamp(weight, 0.0f, 1.0f);
}

float AnimationController::GetLayerWeight(size_t layerIndex) const {
    return layerIndex < m_layerWeights.size() ? m_layerWeights[layerIndex] : 1.0f;
}

// AnimationStateMachine implementation

AnimationStateMachine::AnimationStateMachine(AnimationController* controller)
    : m_controller(controller)
{}

void AnimationStateMachine::AddState(const std::string& stateName, const std::string& animationName, bool looping) {
    m_states[stateName] = State{animationName, looping};
}

void AnimationStateMachine::AddTransition(const std::string& from, const std::string& to,
                                           std::function<bool()> condition, float blendTime) {
    m_transitions.push_back({from, to, std::move(condition), blendTime});
}

void AnimationStateMachine::AddAnyStateTransition(const std::string& to,
                                                   std::function<bool()> condition, float blendTime) {
    m_anyStateTransitions.push_back({"", to, std::move(condition), blendTime});
}

void AnimationStateMachine::SetInitialState(const std::string& stateName) {
    auto it = m_states.find(stateName);
    if (it != m_states.end() && m_controller) {
        m_currentState = stateName;
        m_controller->Play(it->second.animationName, 0.0f, it->second.looping);
    }
}

void AnimationStateMachine::Update(float deltaTime) {
    if (!m_controller || m_currentState.empty()) {
        return;
    }

    // Check any-state transitions first (higher priority)
    for (const auto& transition : m_anyStateTransitions) {
        if (transition.toState != m_currentState && transition.condition && transition.condition()) {
            ForceState(transition.toState, transition.blendTime);
            return;
        }
    }

    // Check normal transitions
    for (const auto& transition : m_transitions) {
        if (transition.fromState == m_currentState && transition.condition && transition.condition()) {
            ForceState(transition.toState, transition.blendTime);
            return;
        }
    }
}

void AnimationStateMachine::ForceState(const std::string& stateName, float blendTime) {
    auto it = m_states.find(stateName);
    if (it != m_states.end() && m_controller) {
        m_currentState = stateName;
        m_controller->CrossFade(it->second.animationName, blendTime, it->second.looping);
    }
}

} // namespace Nova
