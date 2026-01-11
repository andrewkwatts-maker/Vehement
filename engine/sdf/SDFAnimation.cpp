#include "SDFAnimation.hpp"
#include "SDFModel.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <sstream>

namespace Nova {

// ============================================================================
// SDFAnimationClip Implementation
// ============================================================================

SDFAnimationClip::SDFAnimationClip(const std::string& name)
    : m_name(name) {
}

SDFPoseKeyframe* SDFAnimationClip::AddKeyframe(float time) {
    m_keyframes.push_back(SDFPoseKeyframe{time});
    SortKeyframes();

    // Find the keyframe we just added
    for (auto& kf : m_keyframes) {
        if (std::abs(kf.time - time) < 0.0001f) {
            return &kf;
        }
    }
    return nullptr;
}

SDFPoseKeyframe* SDFAnimationClip::AddKeyframeFromPose(float time, const SDFPose& pose) {
    SDFPoseKeyframe kf;
    kf.time = time;
    kf.transforms = pose.transforms;
    kf.materials = pose.materials;

    m_keyframes.push_back(std::move(kf));
    SortKeyframes();

    for (auto& keyframe : m_keyframes) {
        if (std::abs(keyframe.time - time) < 0.0001f) {
            return &keyframe;
        }
    }
    return nullptr;
}

void SDFAnimationClip::RemoveKeyframe(size_t index) {
    if (index < m_keyframes.size()) {
        m_keyframes.erase(m_keyframes.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void SDFAnimationClip::RemoveKeyframeAtTime(float time, float tolerance) {
    auto it = std::remove_if(m_keyframes.begin(), m_keyframes.end(),
        [time, tolerance](const SDFPoseKeyframe& kf) {
            return std::abs(kf.time - time) <= tolerance;
        });
    m_keyframes.erase(it, m_keyframes.end());
}

SDFPoseKeyframe* SDFAnimationClip::GetKeyframe(size_t index) {
    return index < m_keyframes.size() ? &m_keyframes[index] : nullptr;
}

const SDFPoseKeyframe* SDFAnimationClip::GetKeyframe(size_t index) const {
    return index < m_keyframes.size() ? &m_keyframes[index] : nullptr;
}

SDFPoseKeyframe* SDFAnimationClip::GetKeyframeAtTime(float time, float tolerance) {
    for (auto& kf : m_keyframes) {
        if (std::abs(kf.time - time) <= tolerance) {
            return &kf;
        }
    }
    return nullptr;
}

void SDFAnimationClip::SortKeyframes() {
    std::sort(m_keyframes.begin(), m_keyframes.end(),
        [](const SDFPoseKeyframe& a, const SDFPoseKeyframe& b) {
            return a.time < b.time;
        });
}

std::unordered_map<std::string, SDFTransform> SDFAnimationClip::Evaluate(float time) const {
    if (m_keyframes.empty()) {
        return {};
    }

    // Handle looping
    if (m_looping && m_duration > 0) {
        time = std::fmod(time, m_duration);
        if (time < 0) time += m_duration;
    } else {
        time = std::clamp(time, 0.0f, m_duration);
    }

    // Find surrounding keyframes
    const SDFPoseKeyframe* before = nullptr;
    const SDFPoseKeyframe* after = nullptr;

    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].time <= time) {
            before = &m_keyframes[i];
        }
        if (m_keyframes[i].time >= time && after == nullptr) {
            after = &m_keyframes[i];
        }
    }

    // Handle edge cases
    if (!before && after) return after->transforms;
    if (before && !after) return before->transforms;
    if (!before && !after) return {};
    if (before == after) return before->transforms;

    // Interpolate
    float t = (time - before->time) / (after->time - before->time);
    t = ApplyEasing(t, after->easing);

    std::unordered_map<std::string, SDFTransform> result;

    // Get all unique primitive names
    std::vector<std::string> names;
    for (const auto& [name, _] : before->transforms) {
        names.push_back(name);
    }
    for (const auto& [name, _] : after->transforms) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }

    // Interpolate each transform
    for (const auto& name : names) {
        auto itBefore = before->transforms.find(name);
        auto itAfter = after->transforms.find(name);

        if (itBefore != before->transforms.end() && itAfter != after->transforms.end()) {
            result[name] = InterpolateTransform(itBefore->second, itAfter->second, t, after->easing);
        } else if (itBefore != before->transforms.end()) {
            result[name] = itBefore->second;
        } else if (itAfter != after->transforms.end()) {
            result[name] = itAfter->second;
        }
    }

    return result;
}

void SDFAnimationClip::ApplyToModel(SDFModel& model, float time) const {
    auto pose = Evaluate(time);
    model.ApplyPose(pose);
}

SDFPose SDFAnimationClip::ExtractPose(float time, const std::string& poseName) const {
    SDFPose pose;
    pose.name = poseName;
    pose.transforms = Evaluate(time);

    auto now = std::chrono::system_clock::now();
    pose.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

    return pose;
}

std::vector<std::string> SDFAnimationClip::GetAffectedPrimitives() const {
    std::vector<std::string> names;
    for (const auto& kf : m_keyframes) {
        for (const auto& [name, _] : kf.transforms) {
            if (std::find(names.begin(), names.end(), name) == names.end()) {
                names.push_back(name);
            }
        }
    }
    return names;
}

SDFTransform SDFAnimationClip::InterpolateTransform(const SDFTransform& a, const SDFTransform& b, float t, const std::string& easing) const {
    (void)easing;  // Easing already applied to t
    return SDFTransform::Lerp(a, b, t);
}

float SDFAnimationClip::ApplyEasing(float t, const std::string& easing) const {
    if (easing == "linear" || easing.empty()) {
        return t;
    }
    if (easing == "ease_in") {
        return t * t;
    }
    if (easing == "ease_out") {
        return 1.0f - (1.0f - t) * (1.0f - t);
    }
    if (easing == "ease_in_out") {
        return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }
    if (easing == "bounce") {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        if (t < 1.0f / d1) {
            return n1 * t * t;
        } else if (t < 2.0f / d1) {
            float t2 = t - 1.5f / d1;
            return n1 * t2 * t2 + 0.75f;
        } else if (t < 2.5f / d1) {
            float t2 = t - 2.25f / d1;
            return n1 * t2 * t2 + 0.9375f;
        } else {
            float t2 = t - 2.625f / d1;
            return n1 * t2 * t2 + 0.984375f;
        }
    }
    if (easing == "elastic") {
        const float c4 = (2.0f * 3.14159f) / 3.0f;
        if (t == 0.0f) return 0.0f;
        if (t == 1.0f) return 1.0f;
        return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }
    return t;
}

// ============================================================================
// SDFAnimationStateMachine Implementation
// ============================================================================

SDFAnimationState* SDFAnimationStateMachine::AddState(const std::string& name, std::shared_ptr<SDFAnimationClip> clip) {
    SDFAnimationState state;
    state.name = name;
    state.clip = clip;
    state.loop = clip ? clip->IsLooping() : true;

    m_states[name] = std::move(state);

    if (m_defaultStateName.empty()) {
        m_defaultStateName = name;
    }

    return &m_states[name];
}

void SDFAnimationStateMachine::RemoveState(const std::string& name) {
    m_states.erase(name);

    // Remove transitions involving this state
    auto it = std::remove_if(m_transitions.begin(), m_transitions.end(),
        [&name](const SDFAnimationTransition& t) {
            return t.fromState == name || t.toState == name;
        });
    m_transitions.erase(it, m_transitions.end());

    if (m_defaultStateName == name) {
        m_defaultStateName = m_states.empty() ? "" : m_states.begin()->first;
    }
}

SDFAnimationState* SDFAnimationStateMachine::GetState(const std::string& name) {
    auto it = m_states.find(name);
    return it != m_states.end() ? &it->second : nullptr;
}

void SDFAnimationStateMachine::SetDefaultState(const std::string& name) {
    if (m_states.count(name)) {
        m_defaultStateName = name;
    }
}

SDFAnimationTransition* SDFAnimationStateMachine::AddTransition(const std::string& from, const std::string& to, float duration) {
    SDFAnimationTransition transition;
    transition.fromState = from;
    transition.toState = to;
    transition.duration = duration;

    m_transitions.push_back(std::move(transition));
    return &m_transitions.back();
}

void SDFAnimationStateMachine::RemoveTransition(const std::string& from, const std::string& to) {
    auto it = std::remove_if(m_transitions.begin(), m_transitions.end(),
        [&from, &to](const SDFAnimationTransition& t) {
            return t.fromState == from && t.toState == to;
        });
    m_transitions.erase(it, m_transitions.end());
}

std::vector<SDFAnimationTransition*> SDFAnimationStateMachine::GetTransitionsFrom(const std::string& state) {
    std::vector<SDFAnimationTransition*> result;
    for (auto& t : m_transitions) {
        if (t.fromState == state) {
            result.push_back(&t);
        }
    }
    return result;
}

void SDFAnimationStateMachine::Start() {
    m_isRunning = true;
    if (m_currentStateName.empty() && !m_defaultStateName.empty()) {
        m_currentStateName = m_defaultStateName;
        m_currentTime = 0.0f;

        auto* state = GetState(m_currentStateName);
        if (state && state->onEnter) {
            state->onEnter();
        }
    }
}

void SDFAnimationStateMachine::Stop() {
    m_isRunning = false;
    m_isTransitioning = false;
}

void SDFAnimationStateMachine::Reset() {
    Stop();
    m_currentStateName = m_defaultStateName;
    m_currentTime = 0.0f;
    Start();
}

void SDFAnimationStateMachine::Update(float deltaTime) {
    if (!m_isRunning) return;

    if (m_isTransitioning) {
        UpdateTransition(deltaTime);
    } else {
        // Update current state
        auto* state = GetState(m_currentStateName);
        if (state && state->clip) {
            m_currentTime += deltaTime * state->speed;

            float duration = state->clip->GetDuration();
            if (!state->loop && m_currentTime >= duration) {
                m_currentTime = duration;
            } else if (state->loop && duration > 0) {
                m_currentTime = std::fmod(m_currentTime, duration);
            }
        }

        // Check for transitions
        CheckTransitions();
    }
}

void SDFAnimationStateMachine::TransitionTo(const std::string& stateName, float blendTime) {
    if (!m_states.count(stateName)) return;
    if (m_currentStateName == stateName && !m_isTransitioning) return;

    // Find transition settings
    float duration = blendTime;
    std::string curve = "linear";

    if (blendTime < 0) {
        for (const auto& t : m_transitions) {
            if (t.fromState == m_currentStateName && t.toState == stateName) {
                duration = t.duration;
                curve = t.blendCurve;
                break;
            }
        }
        if (duration < 0) duration = 0.2f;  // Default
    }

    // Start transition
    auto* currentState = GetState(m_currentStateName);
    if (currentState && currentState->onExit) {
        currentState->onExit();
    }

    m_previousStateTime = m_currentTime;
    m_targetStateName = stateName;
    m_transitionTime = 0.0f;
    m_transitionDuration = duration;
    m_transitionBlendCurve = curve;
    m_isTransitioning = true;

    if (OnTransitionStarted) {
        OnTransitionStarted(m_currentStateName, stateName);
    }
}

void SDFAnimationStateMachine::SetParameter(const std::string& name, float value) {
    m_floatParams[name] = value;
}

void SDFAnimationStateMachine::SetParameter(const std::string& name, bool value) {
    m_boolParams[name] = value;
}

void SDFAnimationStateMachine::SetParameter(const std::string& name, int value) {
    m_intParams[name] = value;
}

float SDFAnimationStateMachine::GetFloatParameter(const std::string& name) const {
    auto it = m_floatParams.find(name);
    return it != m_floatParams.end() ? it->second : 0.0f;
}

bool SDFAnimationStateMachine::GetBoolParameter(const std::string& name) const {
    auto it = m_boolParams.find(name);
    return it != m_boolParams.end() ? it->second : false;
}

int SDFAnimationStateMachine::GetIntParameter(const std::string& name) const {
    auto it = m_intParams.find(name);
    return it != m_intParams.end() ? it->second : 0;
}

float SDFAnimationStateMachine::GetTransitionProgress() const {
    if (!m_isTransitioning || m_transitionDuration <= 0) return 0.0f;
    return std::clamp(m_transitionTime / m_transitionDuration, 0.0f, 1.0f);
}

std::unordered_map<std::string, SDFTransform> SDFAnimationStateMachine::GetCurrentPose() const {
    if (m_isTransitioning) {
        auto* fromState = const_cast<SDFAnimationStateMachine*>(this)->GetState(m_currentStateName);
        auto* toState = const_cast<SDFAnimationStateMachine*>(this)->GetState(m_targetStateName);

        if (!fromState || !toState) return {};

        auto fromPose = fromState->clip ? fromState->clip->Evaluate(m_previousStateTime) : std::unordered_map<std::string, SDFTransform>{};
        auto toPose = toState->clip ? toState->clip->Evaluate(m_transitionTime) : std::unordered_map<std::string, SDFTransform>{};

        float t = GetTransitionProgress();

        // Blend poses
        std::unordered_map<std::string, SDFTransform> result;
        std::vector<std::string> names;

        for (const auto& [name, _] : fromPose) names.push_back(name);
        for (const auto& [name, _] : toPose) {
            if (std::find(names.begin(), names.end(), name) == names.end()) {
                names.push_back(name);
            }
        }

        for (const auto& name : names) {
            auto itFrom = fromPose.find(name);
            auto itTo = toPose.find(name);

            if (itFrom != fromPose.end() && itTo != toPose.end()) {
                result[name] = SDFTransform::Lerp(itFrom->second, itTo->second, t);
            } else if (itFrom != fromPose.end()) {
                result[name] = itFrom->second;
            } else if (itTo != toPose.end()) {
                result[name] = itTo->second;
            }
        }

        return result;
    } else {
        auto* state = const_cast<SDFAnimationStateMachine*>(this)->GetState(m_currentStateName);
        if (state && state->clip) {
            return state->clip->Evaluate(m_currentTime);
        }
    }

    return {};
}

void SDFAnimationStateMachine::ApplyToModel(SDFModel& model) const {
    auto pose = GetCurrentPose();
    model.ApplyPose(pose);
}

void SDFAnimationStateMachine::CheckTransitions() {
    for (const auto& t : m_transitions) {
        if (t.fromState == m_currentStateName) {
            if (EvaluateCondition(t)) {
                TransitionTo(t.toState, t.duration);
                break;
            }
        }
    }
}

void SDFAnimationStateMachine::UpdateTransition(float deltaTime) {
    m_transitionTime += deltaTime;

    if (m_transitionTime >= m_transitionDuration) {
        // Complete transition
        m_currentStateName = m_targetStateName;
        m_currentTime = m_transitionTime;
        m_isTransitioning = false;

        auto* state = GetState(m_currentStateName);
        if (state && state->onEnter) {
            state->onEnter();
        }

        if (OnTransitionCompleted) {
            OnTransitionCompleted(m_currentStateName);
        }

        if (OnStateChanged) {
            OnStateChanged(m_currentStateName);
        }
    }
}

bool SDFAnimationStateMachine::EvaluateCondition(const SDFAnimationTransition& transition) const {
    if (transition.conditionCallback) {
        return transition.conditionCallback();
    }

    // Simple condition parsing: "paramName == value" or "paramName > value" etc.
    if (transition.condition.empty()) return false;

    // Parse condition (simplified)
    std::string cond = transition.condition;
    size_t pos;

    if ((pos = cond.find("==")) != std::string::npos) {
        std::string param = cond.substr(0, pos);
        std::string value = cond.substr(pos + 2);

        // Trim
        param.erase(0, param.find_first_not_of(" "));
        param.erase(param.find_last_not_of(" ") + 1);
        value.erase(0, value.find_first_not_of(" "));
        value.erase(value.find_last_not_of(" ") + 1);

        if (value == "true" || value == "false") {
            return GetBoolParameter(param) == (value == "true");
        }
        return GetFloatParameter(param) == std::stof(value);
    }

    return false;
}

// ============================================================================
// SDFPoseLibrary Implementation
// ============================================================================

SDFPose* SDFPoseLibrary::SavePose(const std::string& name, const std::unordered_map<std::string, SDFTransform>& transforms,
                                   const std::string& category) {
    // Remove existing pose with same name
    DeletePose(name);

    SDFPose pose;
    pose.name = name;
    pose.category = category;
    pose.transforms = transforms;
    pose.timestamp = GetTimestamp();

    m_poses.push_back(std::move(pose));
    return &m_poses.back();
}

SDFPose* SDFPoseLibrary::SavePoseFromModel(const std::string& name, const SDFModel& model, const std::string& category) {
    return SavePose(name, model.GetCurrentPose(), category);
}

void SDFPoseLibrary::DeletePose(const std::string& name) {
    auto it = std::remove_if(m_poses.begin(), m_poses.end(),
        [&name](const SDFPose& p) { return p.name == name; });
    m_poses.erase(it, m_poses.end());
}

SDFPose* SDFPoseLibrary::GetPose(const std::string& name) {
    for (auto& pose : m_poses) {
        if (pose.name == name) return &pose;
    }
    return nullptr;
}

const SDFPose* SDFPoseLibrary::GetPose(const std::string& name) const {
    for (const auto& pose : m_poses) {
        if (pose.name == name) return &pose;
    }
    return nullptr;
}

std::vector<const SDFPose*> SDFPoseLibrary::GetPosesByCategory(const std::string& category) const {
    std::vector<const SDFPose*> result;
    for (const auto& pose : m_poses) {
        if (pose.category == category) {
            result.push_back(&pose);
        }
    }
    return result;
}

std::vector<std::string> SDFPoseLibrary::GetCategories() const {
    std::vector<std::string> categories;
    for (const auto& pose : m_poses) {
        if (std::find(categories.begin(), categories.end(), pose.category) == categories.end()) {
            categories.push_back(pose.category);
        }
    }
    return categories;
}

bool SDFPoseLibrary::HasPose(const std::string& name) const {
    return GetPose(name) != nullptr;
}

std::unordered_map<std::string, SDFTransform> SDFPoseLibrary::BlendPoses(
    const std::string& poseA, const std::string& poseB, float weight) const {

    const SDFPose* a = GetPose(poseA);
    const SDFPose* b = GetPose(poseB);

    if (!a && !b) return {};
    if (!a) return b->transforms;
    if (!b) return a->transforms;

    std::unordered_map<std::string, SDFTransform> result;

    // Get all unique names
    std::vector<std::string> names;
    for (const auto& [name, _] : a->transforms) names.push_back(name);
    for (const auto& [name, _] : b->transforms) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
    }

    for (const auto& name : names) {
        auto itA = a->transforms.find(name);
        auto itB = b->transforms.find(name);

        if (itA != a->transforms.end() && itB != b->transforms.end()) {
            result[name] = SDFTransform::Lerp(itA->second, itB->second, weight);
        } else if (itA != a->transforms.end()) {
            result[name] = itA->second;
        } else if (itB != b->transforms.end()) {
            result[name] = itB->second;
        }
    }

    return result;
}

std::unordered_map<std::string, SDFTransform> SDFPoseLibrary::BlendMultiplePoses(
    const std::vector<std::pair<std::string, float>>& posesAndWeights) const {

    if (posesAndWeights.empty()) return {};

    // Normalize weights
    float totalWeight = 0.0f;
    for (const auto& [_, weight] : posesAndWeights) {
        totalWeight += weight;
    }

    if (totalWeight <= 0.0f) return {};

    // Get all unique primitive names
    std::vector<std::string> names;
    for (const auto& [poseName, _] : posesAndWeights) {
        const SDFPose* pose = GetPose(poseName);
        if (pose) {
            for (const auto& [name, __] : pose->transforms) {
                if (std::find(names.begin(), names.end(), name) == names.end()) {
                    names.push_back(name);
                }
            }
        }
    }

    // Blend all poses
    std::unordered_map<std::string, SDFTransform> result;

    for (const auto& name : names) {
        glm::vec3 position(0.0f);
        glm::quat rotation(0.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale(0.0f);
        float accumulatedWeight = 0.0f;

        for (const auto& [poseName, weight] : posesAndWeights) {
            const SDFPose* pose = GetPose(poseName);
            if (!pose) continue;

            auto it = pose->transforms.find(name);
            if (it == pose->transforms.end()) continue;

            float normalizedWeight = weight / totalWeight;
            position += it->second.position * normalizedWeight;
            scale += it->second.scale * normalizedWeight;

            // For quaternion, accumulate and normalize later
            if (accumulatedWeight == 0.0f) {
                rotation = it->second.rotation * normalizedWeight;
            } else {
                // Ensure shortest path
                if (glm::dot(rotation, it->second.rotation) < 0.0f) {
                    rotation += -it->second.rotation * normalizedWeight;
                } else {
                    rotation += it->second.rotation * normalizedWeight;
                }
            }
            accumulatedWeight += normalizedWeight;
        }

        if (accumulatedWeight > 0.0f) {
            SDFTransform transform;
            transform.position = position;
            transform.rotation = glm::normalize(rotation);
            transform.scale = scale;
            result[name] = transform;
        }
    }

    return result;
}

uint64_t SDFPoseLibrary::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}

// ============================================================================
// SDFAnimationController Implementation
// ============================================================================

void SDFAnimationController::SetStateMachine(std::shared_ptr<SDFAnimationStateMachine> stateMachine) {
    m_stateMachine = stateMachine;
}

void SDFAnimationController::PlayClip(std::shared_ptr<SDFAnimationClip> clip, float blendTime) {
    if (blendTime > 0 && m_model && m_currentClip) {
        m_blendStartPose = m_currentClip->Evaluate(m_currentTime);
        m_blendDuration = blendTime;
        m_blendTime = 0.0f;
        m_isBlending = true;
    }

    m_currentClip = clip;
    m_currentTime = 0.0f;
    m_isPlaying = true;
    m_isPaused = false;

    if (blendTime > 0) {
        m_blendTargetPose = clip->Evaluate(0.0f);
    }
}

void SDFAnimationController::PlayPose(const std::string& poseName, float blendTime) {
    if (!m_poseLibrary) return;

    const SDFPose* pose = m_poseLibrary->GetPose(poseName);
    if (!pose) return;

    BlendToPose(poseName, blendTime);
}

void SDFAnimationController::BlendToPose(const std::string& poseName, float duration) {
    if (!m_poseLibrary || !m_model) return;

    const SDFPose* pose = m_poseLibrary->GetPose(poseName);
    if (!pose) return;

    m_blendStartPose = m_model->GetCurrentPose();
    m_blendTargetPose = pose->transforms;
    m_blendDuration = duration;
    m_blendTime = 0.0f;
    m_isBlending = true;
    m_isPlaying = false;
    m_currentClip = nullptr;
}

void SDFAnimationController::Stop() {
    m_isPlaying = false;
    m_isPaused = false;
    m_currentTime = 0.0f;
    m_isBlending = false;
}

void SDFAnimationController::Pause() {
    m_isPaused = true;
}

void SDFAnimationController::Resume() {
    m_isPaused = false;
}

void SDFAnimationController::Update(float deltaTime) {
    if (m_isPaused) return;

    // Update state machine if present
    if (m_stateMachine && m_stateMachine->GetCurrentStateName().empty() == false) {
        m_stateMachine->Update(deltaTime * m_speed);
        if (m_model) {
            m_stateMachine->ApplyToModel(*m_model);
        }
        return;
    }

    // Handle blending
    if (m_isBlending) {
        m_blendTime += deltaTime * m_speed;
        float t = m_blendDuration > 0 ? std::clamp(m_blendTime / m_blendDuration, 0.0f, 1.0f) : 1.0f;

        if (m_model) {
            std::unordered_map<std::string, SDFTransform> pose;

            for (const auto& [name, _] : m_blendStartPose) {
                auto itStart = m_blendStartPose.find(name);
                auto itTarget = m_blendTargetPose.find(name);

                if (itStart != m_blendStartPose.end() && itTarget != m_blendTargetPose.end()) {
                    pose[name] = SDFTransform::Lerp(itStart->second, itTarget->second, t);
                } else if (itStart != m_blendStartPose.end()) {
                    pose[name] = itStart->second;
                }
            }

            // Include any transforms only in target
            for (const auto& [name, transform] : m_blendTargetPose) {
                if (pose.find(name) == pose.end()) {
                    pose[name] = transform;
                }
            }

            m_model->ApplyPose(pose);
        }

        if (m_blendTime >= m_blendDuration) {
            m_isBlending = false;
        }

        return;
    }

    // Update clip playback
    if (m_isPlaying && m_currentClip) {
        m_currentTime += deltaTime * m_speed;

        if (m_currentClip->IsLooping()) {
            float duration = m_currentClip->GetDuration();
            if (duration > 0) {
                m_currentTime = std::fmod(m_currentTime, duration);
            }
        } else {
            m_currentTime = std::min(m_currentTime, m_currentClip->GetDuration());
        }

        ApplyToModel();
    }

    // Update layers
    for (auto& layer : m_layers) {
        if (layer.active && layer.clip) {
            layer.time += deltaTime * m_speed;
            float duration = layer.clip->GetDuration();
            if (duration > 0) {
                layer.time = std::fmod(layer.time, duration);
            }
        }
    }
}

void SDFAnimationController::AddLayer(const std::string& name, float weight) {
    AnimationLayer layer;
    layer.name = name;
    layer.weight = weight;
    m_layers.push_back(std::move(layer));
}

void SDFAnimationController::RemoveLayer(const std::string& name) {
    auto it = std::remove_if(m_layers.begin(), m_layers.end(),
        [&name](const AnimationLayer& l) { return l.name == name; });
    m_layers.erase(it, m_layers.end());
}

void SDFAnimationController::SetLayerWeight(const std::string& name, float weight) {
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            layer.weight = weight;
            break;
        }
    }
}

void SDFAnimationController::PlayOnLayer(const std::string& layerName, std::shared_ptr<SDFAnimationClip> clip) {
    for (auto& layer : m_layers) {
        if (layer.name == layerName) {
            layer.clip = clip;
            layer.time = 0.0f;
            layer.active = true;
            break;
        }
    }
}

void SDFAnimationController::ApplyToModel() {
    if (!m_model || !m_currentClip) return;

    auto pose = m_currentClip->Evaluate(m_currentTime);

    // Apply layer overrides
    for (const auto& layer : m_layers) {
        if (!layer.active || !layer.clip || layer.weight <= 0.0f) continue;

        auto layerPose = layer.clip->Evaluate(layer.time);

        for (const auto& [name, transform] : layerPose) {
            // Check bone mask
            if (!layer.boneMask.empty()) {
                if (std::find(layer.boneMask.begin(), layer.boneMask.end(), name) == layer.boneMask.end()) {
                    continue;
                }
            }

            auto it = pose.find(name);
            if (it != pose.end()) {
                pose[name] = SDFTransform::Lerp(it->second, transform, layer.weight);
            } else {
                pose[name] = transform;
            }
        }
    }

    m_model->ApplyPose(pose);
}

} // namespace Nova
