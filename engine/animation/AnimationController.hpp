#pragma once

#include "animation/Animation.hpp"
#include "animation/Skeleton.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace Nova {

/**
 * @brief Animation playback state
 */
enum class AnimationState : uint8_t {
    Stopped,
    Playing,
    Paused,
    BlendingIn,
    BlendingOut
};

/**
 * @brief Animation playback instance
 */
struct AnimationInstance {
    const Animation* animation = nullptr;
    float currentTime = 0.0f;
    float playbackSpeed = 1.0f;
    float weight = 1.0f;
    float targetWeight = 1.0f;
    float blendSpeed = 5.0f;     // Weight change per second
    AnimationState state = AnimationState::Stopped;
    bool looping = true;

    // Blend settings
    BlendMode blendMode = BlendMode::Override;

    // Callbacks
    std::function<void()> onAnimationEnd;
    std::function<void()> onLoop;
};

/**
 * @brief Controls animation playback, blending, and state management
 *
 * Supports multiple concurrent animations with blending, layers,
 * and crossfade transitions.
 */
class AnimationController {
public:
    AnimationController() = default;
    explicit AnimationController(Skeleton* skeleton);

    AnimationController(const AnimationController&) = delete;
    AnimationController& operator=(const AnimationController&) = delete;
    AnimationController(AnimationController&&) noexcept = default;
    AnimationController& operator=(AnimationController&&) noexcept = default;

    /**
     * @brief Set the skeleton to animate
     */
    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

    /**
     * @brief Add an animation to the controller's library
     */
    void AddAnimation(const std::string& name, std::shared_ptr<Animation> animation);

    /**
     * @brief Get animation by name
     */
    [[nodiscard]] Animation* GetAnimation(const std::string& name);
    [[nodiscard]] const Animation* GetAnimation(const std::string& name) const;

    /**
     * @brief Play an animation
     * @param name Animation name
     * @param blendTime Time to blend from current animation
     * @param looping Whether the animation should loop
     */
    void Play(const std::string& name, float blendTime = 0.2f, bool looping = true);

    /**
     * @brief Crossfade to another animation
     */
    void CrossFade(const std::string& name, float fadeTime = 0.3f, bool looping = true);

    /**
     * @brief Stop all animations
     */
    void Stop(float blendOutTime = 0.2f);

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Resume playback
     */
    void Resume();

    /**
     * @brief Update animation state
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Get the blended bone transforms
     */
    [[nodiscard]] const std::unordered_map<std::string, glm::mat4>& GetBoneTransforms() const {
        return m_blendedTransforms;
    }

    /**
     * @brief Get final bone matrices ready for GPU upload
     */
    [[nodiscard]] std::vector<glm::mat4> GetBoneMatrices() const;

    /**
     * @brief Get bone matrices into pre-allocated buffer
     */
    void GetBoneMatricesInto(std::span<glm::mat4> outMatrices) const;

    /**
     * @brief Check if currently playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_playing; }

    /**
     * @brief Get current animation time
     */
    [[nodiscard]] float GetCurrentTime() const;

    /**
     * @brief Set current animation time
     */
    void SetCurrentTime(float time);

    /**
     * @brief Get playback speed multiplier
     */
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }
    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }

    /**
     * @brief Get the name of the currently playing animation
     */
    [[nodiscard]] const std::string& GetCurrentAnimationName() const { return m_currentAnimationName; }

    /**
     * @brief Layer management
     */
    void SetLayerWeight(size_t layerIndex, float weight);
    [[nodiscard]] float GetLayerWeight(size_t layerIndex) const;

private:
    void UpdateInstance(AnimationInstance& instance, float deltaTime);
    void BlendAnimations();
    void CleanupFinishedAnimations();

    Skeleton* m_skeleton = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Animation>> m_animations;

    // Active animation instances (supports blending multiple)
    std::vector<AnimationInstance> m_activeAnimations;

    // Blended result
    std::unordered_map<std::string, glm::mat4> m_blendedTransforms;

    // Playback state
    bool m_playing = false;
    float m_playbackSpeed = 1.0f;
    std::string m_currentAnimationName;

    // Layer weights for masked blending
    std::vector<float> m_layerWeights;
};

/**
 * @brief Simple animation state machine
 */
class AnimationStateMachine {
public:
    struct Transition {
        std::string fromState;
        std::string toState;
        std::function<bool()> condition;
        float blendTime = 0.2f;
    };

    AnimationStateMachine() = default;
    explicit AnimationStateMachine(AnimationController* controller);

    /**
     * @brief Add a state with an associated animation
     */
    void AddState(const std::string& stateName, const std::string& animationName, bool looping = true);

    /**
     * @brief Add a transition between states
     */
    void AddTransition(const std::string& from, const std::string& to,
                       std::function<bool()> condition, float blendTime = 0.2f);

    /**
     * @brief Add a transition from any state
     */
    void AddAnyStateTransition(const std::string& to,
                                std::function<bool()> condition, float blendTime = 0.2f);

    /**
     * @brief Set the initial state
     */
    void SetInitialState(const std::string& stateName);

    /**
     * @brief Update the state machine
     */
    void Update(float deltaTime);

    /**
     * @brief Force transition to a state
     */
    void ForceState(const std::string& stateName, float blendTime = 0.2f);

    /**
     * @brief Get current state name
     */
    [[nodiscard]] const std::string& GetCurrentState() const { return m_currentState; }

private:
    struct State {
        std::string animationName;
        bool looping = true;
    };

    AnimationController* m_controller = nullptr;
    std::unordered_map<std::string, State> m_states;
    std::vector<Transition> m_transitions;
    std::vector<Transition> m_anyStateTransitions;
    std::string m_currentState;
};

} // namespace Nova
