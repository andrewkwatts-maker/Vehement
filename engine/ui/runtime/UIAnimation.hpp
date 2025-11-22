#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Engine {
namespace UI {

// Forward declarations
class DOMElement;

/**
 * @brief Easing function type
 */
enum class EasingFunction {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,
    EaseInQuint,
    EaseOutQuint,
    EaseInOutQuint,
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    EaseInExpo,
    EaseOutExpo,
    EaseInOutExpo,
    EaseInCirc,
    EaseOutCirc,
    EaseInOutCirc,
    EaseInElastic,
    EaseOutElastic,
    EaseInOutElastic,
    EaseInBack,
    EaseOutBack,
    EaseInOutBack,
    EaseInBounce,
    EaseOutBounce,
    EaseInOutBounce,
    Custom
};

/**
 * @brief Animation property value
 */
struct AnimationValue {
    enum class Type { Number, Color, String };
    Type type = Type::Number;
    float numberValue = 0;
    float colorR = 0, colorG = 0, colorB = 0, colorA = 1;
    std::string stringValue;

    static AnimationValue FromNumber(float value);
    static AnimationValue FromColor(float r, float g, float b, float a = 1.0f);
    static AnimationValue FromString(const std::string& value);

    AnimationValue Interpolate(const AnimationValue& target, float t) const;
};

/**
 * @brief Keyframe for animation
 */
struct Keyframe {
    float time = 0;  // 0.0 to 1.0
    std::string property;
    AnimationValue value;
    EasingFunction easing = EasingFunction::Linear;
};

/**
 * @brief CSS-like transition definition
 */
struct Transition {
    std::string property;
    float duration = 0.3f;
    EasingFunction easing = EasingFunction::EaseInOut;
    float delay = 0;
};

/**
 * @brief Animation definition
 */
struct AnimationDefinition {
    std::string name;
    std::vector<Keyframe> keyframes;
    float duration = 1.0f;
    int iterations = 1;  // -1 for infinite
    bool alternate = false;
    float delay = 0;
    EasingFunction defaultEasing = EasingFunction::Linear;
    std::string fillMode = "none";  // "none", "forwards", "backwards", "both"
};

/**
 * @brief Running animation instance
 */
struct AnimationInstance {
    std::string id;
    std::string animationName;
    std::string targetId;
    DOMElement* target = nullptr;
    float currentTime = 0;
    float speed = 1.0f;
    int currentIteration = 0;
    bool playing = true;
    bool reversed = false;
    bool completed = false;

    std::function<void()> onStart;
    std::function<void()> onComplete;
    std::function<void(int)> onIteration;
    std::function<void(float)> onUpdate;
};

/**
 * @brief Animation sequence (multiple animations in order)
 */
struct AnimationSequence {
    std::string name;
    std::vector<std::string> animations;
    int currentIndex = 0;
    bool playing = false;
    std::function<void()> onComplete;
};

/**
 * @brief Parallel animation group
 */
struct AnimationGroup {
    std::string name;
    std::vector<std::string> animations;
    bool playing = false;
    std::function<void()> onComplete;
};

/**
 * @brief Animation trigger from game event
 */
struct AnimationTrigger {
    std::string eventName;
    std::string animationName;
    std::string targetSelector;
    bool enabled = true;
};

/**
 * @brief UI Animation System
 *
 * Provides CSS-like transitions, keyframe animations, easing functions,
 * animation triggers from game events, sequence and parallel animations.
 */
class UIAnimation {
public:
    UIAnimation();
    ~UIAnimation();

    /**
     * @brief Initialize animation system
     */
    void Initialize();

    /**
     * @brief Shutdown animation system
     */
    void Shutdown();

    /**
     * @brief Update all animations
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // Animation Definition

    /**
     * @brief Register an animation definition
     * @param animation Animation definition
     */
    void RegisterAnimation(const AnimationDefinition& animation);

    /**
     * @brief Load animation from JSON
     * @param json JSON animation definition
     */
    void LoadAnimationFromJSON(const std::string& json);

    /**
     * @brief Load animations from file
     * @param path Path to animation JSON file
     */
    void LoadAnimationsFromFile(const std::string& path);

    /**
     * @brief Get animation definition
     */
    const AnimationDefinition* GetAnimation(const std::string& name) const;

    /**
     * @brief Remove animation definition
     */
    void RemoveAnimation(const std::string& name);

    // Playback Control

    /**
     * @brief Play an animation
     * @param animationName Animation name
     * @param targetId Target element ID (empty for global)
     * @return Animation instance ID
     */
    std::string Play(const std::string& animationName, const std::string& targetId = "");

    /**
     * @brief Play animation on element
     * @param animationName Animation name
     * @param element Target DOM element
     * @return Animation instance ID
     */
    std::string PlayOnElement(const std::string& animationName, DOMElement* element);

    /**
     * @brief Pause an animation
     * @param instanceId Animation instance ID
     */
    void Pause(const std::string& instanceId);

    /**
     * @brief Resume an animation
     * @param instanceId Animation instance ID
     */
    void Resume(const std::string& instanceId);

    /**
     * @brief Stop an animation
     * @param instanceId Animation instance ID
     * @param reset Reset to start if true
     */
    void Stop(const std::string& instanceId, bool reset = false);

    /**
     * @brief Stop all animations on target
     * @param targetId Target element ID
     */
    void StopAll(const std::string& targetId = "");

    /**
     * @brief Reverse animation direction
     */
    void Reverse(const std::string& instanceId);

    /**
     * @brief Set animation speed
     */
    void SetSpeed(const std::string& instanceId, float speed);

    /**
     * @brief Seek to time
     */
    void Seek(const std::string& instanceId, float time);

    /**
     * @brief Check if animation is playing
     */
    bool IsPlaying(const std::string& instanceId) const;

    /**
     * @brief Get animation progress (0-1)
     */
    float GetProgress(const std::string& instanceId) const;

    // Transitions

    /**
     * @brief Apply CSS-like transition
     * @param targetId Target element ID
     * @param property Property to transition
     * @param toValue Target value
     * @param duration Duration in seconds
     * @param easing Easing function
     * @return Transition ID
     */
    std::string Transition(const std::string& targetId, const std::string& property,
                          const AnimationValue& toValue, float duration,
                          EasingFunction easing = EasingFunction::EaseInOut);

    /**
     * @brief Apply multiple transitions
     */
    std::string TransitionMultiple(const std::string& targetId,
                                  const std::vector<std::pair<std::string, AnimationValue>>& properties,
                                  float duration,
                                  EasingFunction easing = EasingFunction::EaseInOut);

    /**
     * @brief Cancel transition
     */
    void CancelTransition(const std::string& transitionId);

    // Sequences and Groups

    /**
     * @brief Create animation sequence (plays in order)
     * @param name Sequence name
     * @param animations Animation names in order
     */
    void CreateSequence(const std::string& name, const std::vector<std::string>& animations);

    /**
     * @brief Play sequence
     */
    std::string PlaySequence(const std::string& name, const std::string& targetId = "");

    /**
     * @brief Create animation group (plays in parallel)
     * @param name Group name
     * @param animations Animation names
     */
    void CreateGroup(const std::string& name, const std::vector<std::string>& animations);

    /**
     * @brief Play group
     */
    std::string PlayGroup(const std::string& name, const std::string& targetId = "");

    // Event Triggers

    /**
     * @brief Add animation trigger from game event
     * @param eventName Event name to listen for
     * @param animationName Animation to play
     * @param targetSelector CSS selector for target
     */
    void AddTrigger(const std::string& eventName, const std::string& animationName,
                   const std::string& targetSelector = "");

    /**
     * @brief Remove trigger
     */
    void RemoveTrigger(const std::string& eventName);

    /**
     * @brief Enable/disable trigger
     */
    void SetTriggerEnabled(const std::string& eventName, bool enabled);

    /**
     * @brief Handle game event (triggers animations)
     */
    void HandleEvent(const std::string& eventName, const std::string& data = "");

    // Easing Functions

    /**
     * @brief Get easing function by name
     */
    static EasingFunction GetEasingByName(const std::string& name);

    /**
     * @brief Apply easing function
     */
    static float ApplyEasing(EasingFunction easing, float t);

    /**
     * @brief Register custom easing function
     */
    void RegisterCustomEasing(const std::string& name, std::function<float(float)> fn);

    // Callbacks

    /**
     * @brief Set callback for animation start
     */
    void OnAnimationStart(const std::string& instanceId, std::function<void()> callback);

    /**
     * @brief Set callback for animation complete
     */
    void OnAnimationComplete(const std::string& instanceId, std::function<void()> callback);

    /**
     * @brief Set callback for animation iteration
     */
    void OnAnimationIteration(const std::string& instanceId, std::function<void(int)> callback);

    /**
     * @brief Set callback for animation update
     */
    void OnAnimationUpdate(const std::string& instanceId, std::function<void(float)> callback);

    // Utility

    /**
     * @brief Get all running animation IDs
     */
    std::vector<std::string> GetRunningAnimations() const;

    /**
     * @brief Get animation count
     */
    size_t GetAnimationCount() const { return m_instances.size(); }

    /**
     * @brief Clear all animations
     */
    void Clear();

private:
    void UpdateInstance(AnimationInstance& instance, float deltaTime);
    void ApplyKeyframe(AnimationInstance& instance, const Keyframe& keyframe, float t);
    AnimationValue InterpolateKeyframes(const std::vector<Keyframe>& keyframes,
                                       const std::string& property, float time);

    std::unordered_map<std::string, AnimationDefinition> m_definitions;
    std::unordered_map<std::string, AnimationInstance> m_instances;
    std::unordered_map<std::string, AnimationSequence> m_sequences;
    std::unordered_map<std::string, AnimationGroup> m_groups;
    std::vector<AnimationTrigger> m_triggers;

    std::unordered_map<std::string, std::function<float(float)>> m_customEasings;

    int m_nextInstanceId = 1;
    bool m_initialized = false;
};

} // namespace UI
} // namespace Engine
