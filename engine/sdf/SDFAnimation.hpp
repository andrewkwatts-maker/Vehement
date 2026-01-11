#pragma once

#include "SDFPrimitive.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Nova {

class SDFModel;

/**
 * @brief Single pose keyframe for SDF animation
 */
struct SDFPoseKeyframe {
    float time = 0.0f;
    std::unordered_map<std::string, SDFTransform> transforms;

    // Optional per-primitive material overrides
    std::unordered_map<std::string, SDFMaterial> materials;

    // Easing function identifier
    std::string easing = "linear";
};

/**
 * @brief Named pose that can be blended
 */
struct SDFPose {
    std::string name;
    std::string category;
    std::unordered_map<std::string, SDFTransform> transforms;
    std::unordered_map<std::string, SDFMaterial> materials;

    // Metadata
    std::string description;
    std::vector<std::string> tags;
    uint64_t timestamp = 0;
};

/**
 * @brief Animation clip made of keyframed poses
 */
class SDFAnimationClip {
public:
    SDFAnimationClip() = default;
    explicit SDFAnimationClip(const std::string& name);

    // =========================================================================
    // Properties
    // =========================================================================

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] float GetDuration() const { return m_duration; }
    void SetDuration(float duration) { m_duration = duration; }

    [[nodiscard]] bool IsLooping() const { return m_looping; }
    void SetLooping(bool looping) { m_looping = looping; }

    [[nodiscard]] float GetFrameRate() const { return m_frameRate; }
    void SetFrameRate(float fps) { m_frameRate = fps; }

    // =========================================================================
    // Keyframe Management
    // =========================================================================

    /**
     * @brief Add keyframe at time
     */
    SDFPoseKeyframe* AddKeyframe(float time);

    /**
     * @brief Add keyframe from pose
     */
    SDFPoseKeyframe* AddKeyframeFromPose(float time, const SDFPose& pose);

    /**
     * @brief Remove keyframe at index
     */
    void RemoveKeyframe(size_t index);

    /**
     * @brief Remove keyframe at time
     */
    void RemoveKeyframeAtTime(float time, float tolerance = 0.001f);

    /**
     * @brief Get keyframe at index
     */
    [[nodiscard]] SDFPoseKeyframe* GetKeyframe(size_t index);
    [[nodiscard]] const SDFPoseKeyframe* GetKeyframe(size_t index) const;

    /**
     * @brief Get keyframe at or near time
     */
    [[nodiscard]] SDFPoseKeyframe* GetKeyframeAtTime(float time, float tolerance = 0.001f);

    /**
     * @brief Get all keyframes
     */
    [[nodiscard]] std::vector<SDFPoseKeyframe>& GetKeyframes() { return m_keyframes; }
    [[nodiscard]] const std::vector<SDFPoseKeyframe>& GetKeyframes() const { return m_keyframes; }

    /**
     * @brief Get keyframe count
     */
    [[nodiscard]] size_t GetKeyframeCount() const { return m_keyframes.size(); }

    /**
     * @brief Sort keyframes by time
     */
    void SortKeyframes();

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Evaluate animation at time
     */
    [[nodiscard]] std::unordered_map<std::string, SDFTransform> Evaluate(float time) const;

    /**
     * @brief Evaluate with material overrides
     */
    [[nodiscard]] std::pair<std::unordered_map<std::string, SDFTransform>,
                           std::unordered_map<std::string, SDFMaterial>>
    EvaluateWithMaterials(float time) const;

    /**
     * @brief Apply animation to model at time
     */
    void ApplyToModel(SDFModel& model, float time) const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Extract pose at time
     */
    [[nodiscard]] SDFPose ExtractPose(float time, const std::string& poseName) const;

    /**
     * @brief Get affected primitive names
     */
    [[nodiscard]] std::vector<std::string> GetAffectedPrimitives() const;

private:
    SDFTransform InterpolateTransform(const SDFTransform& a, const SDFTransform& b, float t, const std::string& easing) const;
    float ApplyEasing(float t, const std::string& easing) const;

    std::string m_name;
    float m_duration = 1.0f;
    float m_frameRate = 30.0f;
    bool m_looping = true;

    std::vector<SDFPoseKeyframe> m_keyframes;
};

/**
 * @brief Animation state for state machine
 */
struct SDFAnimationState {
    std::string name;
    std::shared_ptr<SDFAnimationClip> clip;
    float speed = 1.0f;
    bool loop = true;

    // Entry/exit events
    std::function<void()> onEnter;
    std::function<void()> onExit;
};

/**
 * @brief Transition between animation states
 */
struct SDFAnimationTransition {
    std::string fromState;
    std::string toState;
    float duration = 0.2f;  // Blend duration
    std::string condition;  // Condition expression or callback name

    // Optional condition callback
    std::function<bool()> conditionCallback;

    // Transition curve
    std::string blendCurve = "linear";
};

/**
 * @brief State machine for SDF animations
 */
class SDFAnimationStateMachine {
public:
    SDFAnimationStateMachine() = default;

    // =========================================================================
    // State Management
    // =========================================================================

    /**
     * @brief Add animation state
     */
    SDFAnimationState* AddState(const std::string& name, std::shared_ptr<SDFAnimationClip> clip);

    /**
     * @brief Remove state
     */
    void RemoveState(const std::string& name);

    /**
     * @brief Get state
     */
    [[nodiscard]] SDFAnimationState* GetState(const std::string& name);

    /**
     * @brief Get all states
     */
    [[nodiscard]] const std::unordered_map<std::string, SDFAnimationState>& GetStates() const { return m_states; }

    /**
     * @brief Set default state
     */
    void SetDefaultState(const std::string& name);

    // =========================================================================
    // Transitions
    // =========================================================================

    /**
     * @brief Add transition
     */
    SDFAnimationTransition* AddTransition(const std::string& from, const std::string& to, float duration = 0.2f);

    /**
     * @brief Remove transition
     */
    void RemoveTransition(const std::string& from, const std::string& to);

    /**
     * @brief Get transitions from state
     */
    [[nodiscard]] std::vector<SDFAnimationTransition*> GetTransitionsFrom(const std::string& state);

    // =========================================================================
    // Playback
    // =========================================================================

    /**
     * @brief Start state machine
     */
    void Start();

    /**
     * @brief Stop state machine
     */
    void Stop();

    /**
     * @brief Reset to default state
     */
    void Reset();

    /**
     * @brief Update state machine
     */
    void Update(float deltaTime);

    /**
     * @brief Force transition to state
     */
    void TransitionTo(const std::string& stateName, float blendTime = -1.0f);

    /**
     * @brief Set parameter for conditions
     */
    void SetParameter(const std::string& name, float value);
    void SetParameter(const std::string& name, bool value);
    void SetParameter(const std::string& name, int value);

    /**
     * @brief Get parameter
     */
    [[nodiscard]] float GetFloatParameter(const std::string& name) const;
    [[nodiscard]] bool GetBoolParameter(const std::string& name) const;
    [[nodiscard]] int GetIntParameter(const std::string& name) const;

    /**
     * @brief Get current state name
     */
    [[nodiscard]] const std::string& GetCurrentStateName() const { return m_currentStateName; }

    /**
     * @brief Is currently transitioning
     */
    [[nodiscard]] bool IsTransitioning() const { return m_isTransitioning; }

    /**
     * @brief Get transition progress (0-1)
     */
    [[nodiscard]] float GetTransitionProgress() const;

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Get current pose (blended if transitioning)
     */
    [[nodiscard]] std::unordered_map<std::string, SDFTransform> GetCurrentPose() const;

    /**
     * @brief Apply current pose to model
     */
    void ApplyToModel(SDFModel& model) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnStateChanged;
    std::function<void(const std::string&, const std::string&)> OnTransitionStarted;
    std::function<void(const std::string&)> OnTransitionCompleted;

private:
    void CheckTransitions();
    void UpdateTransition(float deltaTime);
    bool EvaluateCondition(const SDFAnimationTransition& transition) const;

    std::unordered_map<std::string, SDFAnimationState> m_states;
    std::vector<SDFAnimationTransition> m_transitions;
    std::string m_defaultStateName;

    // Playback state
    std::string m_currentStateName;
    float m_currentTime = 0.0f;
    bool m_isRunning = false;

    // Transition state
    bool m_isTransitioning = false;
    std::string m_targetStateName;
    float m_transitionTime = 0.0f;
    float m_transitionDuration = 0.0f;
    std::string m_transitionBlendCurve;
    float m_previousStateTime = 0.0f;

    // Parameters
    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, bool> m_boolParams;
    std::unordered_map<std::string, int> m_intParams;
};

/**
 * @brief Pose library for storing and retrieving poses
 */
class SDFPoseLibrary {
public:
    SDFPoseLibrary() = default;

    // =========================================================================
    // Pose Management
    // =========================================================================

    /**
     * @brief Save pose
     */
    SDFPose* SavePose(const std::string& name, const std::unordered_map<std::string, SDFTransform>& transforms,
                      const std::string& category = "Default");

    /**
     * @brief Save pose from model
     */
    SDFPose* SavePoseFromModel(const std::string& name, const SDFModel& model, const std::string& category = "Default");

    /**
     * @brief Delete pose
     */
    void DeletePose(const std::string& name);

    /**
     * @brief Get pose
     */
    [[nodiscard]] SDFPose* GetPose(const std::string& name);
    [[nodiscard]] const SDFPose* GetPose(const std::string& name) const;

    /**
     * @brief Get all poses
     */
    [[nodiscard]] const std::vector<SDFPose>& GetAllPoses() const { return m_poses; }

    /**
     * @brief Get poses by category
     */
    [[nodiscard]] std::vector<const SDFPose*> GetPosesByCategory(const std::string& category) const;

    /**
     * @brief Get categories
     */
    [[nodiscard]] std::vector<std::string> GetCategories() const;

    /**
     * @brief Check if pose exists
     */
    [[nodiscard]] bool HasPose(const std::string& name) const;

    // =========================================================================
    // Pose Blending
    // =========================================================================

    /**
     * @brief Blend two poses
     */
    [[nodiscard]] std::unordered_map<std::string, SDFTransform> BlendPoses(
        const std::string& poseA, const std::string& poseB, float weight) const;

    /**
     * @brief Blend multiple poses with weights
     */
    [[nodiscard]] std::unordered_map<std::string, SDFTransform> BlendMultiplePoses(
        const std::vector<std::pair<std::string, float>>& posesAndWeights) const;

    /**
     * @brief Additive blend
     */
    [[nodiscard]] std::unordered_map<std::string, SDFTransform> AdditivePose(
        const std::unordered_map<std::string, SDFTransform>& basePose,
        const std::string& additivePoseName, float weight) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Save library to file
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief Load library from file
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Export single pose
     */
    bool ExportPose(const std::string& poseName, const std::string& path) const;

    /**
     * @brief Import pose
     */
    SDFPose* ImportPose(const std::string& path);

private:
    uint64_t GetTimestamp() const;

    std::vector<SDFPose> m_poses;
};

/**
 * @brief Animation controller that manages clips and state machines
 */
class SDFAnimationController {
public:
    SDFAnimationController() = default;

    /**
     * @brief Set target model
     */
    void SetModel(SDFModel* model) { m_model = model; }

    /**
     * @brief Set state machine
     */
    void SetStateMachine(std::shared_ptr<SDFAnimationStateMachine> stateMachine);

    /**
     * @brief Get state machine
     */
    [[nodiscard]] SDFAnimationStateMachine* GetStateMachine() { return m_stateMachine.get(); }

    /**
     * @brief Set pose library
     */
    void SetPoseLibrary(std::shared_ptr<SDFPoseLibrary> library) { m_poseLibrary = library; }

    /**
     * @brief Get pose library
     */
    [[nodiscard]] SDFPoseLibrary* GetPoseLibrary() { return m_poseLibrary.get(); }

    // =========================================================================
    // Playback
    // =========================================================================

    /**
     * @brief Play animation clip
     */
    void PlayClip(std::shared_ptr<SDFAnimationClip> clip, float blendTime = 0.0f);

    /**
     * @brief Play pose
     */
    void PlayPose(const std::string& poseName, float blendTime = 0.2f);

    /**
     * @brief Blend to pose
     */
    void BlendToPose(const std::string& poseName, float duration);

    /**
     * @brief Stop playback
     */
    void Stop();

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Resume playback
     */
    void Resume();

    /**
     * @brief Update controller
     */
    void Update(float deltaTime);

    /**
     * @brief Is playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_isPlaying; }

    /**
     * @brief Get current time
     */
    [[nodiscard]] float GetCurrentTime() const { return m_currentTime; }

    /**
     * @brief Set playback speed
     */
    void SetSpeed(float speed) { m_speed = speed; }

    /**
     * @brief Get playback speed
     */
    [[nodiscard]] float GetSpeed() const { return m_speed; }

    // =========================================================================
    // Layers
    // =========================================================================

    /**
     * @brief Add animation layer
     */
    void AddLayer(const std::string& name, float weight = 1.0f);

    /**
     * @brief Remove layer
     */
    void RemoveLayer(const std::string& name);

    /**
     * @brief Set layer weight
     */
    void SetLayerWeight(const std::string& name, float weight);

    /**
     * @brief Play clip on layer
     */
    void PlayOnLayer(const std::string& layerName, std::shared_ptr<SDFAnimationClip> clip);

private:
    struct AnimationLayer {
        std::string name;
        std::shared_ptr<SDFAnimationClip> clip;
        float time = 0.0f;
        float weight = 1.0f;
        bool active = false;
        std::vector<std::string> boneMask;  // Empty = all bones
    };

    void ApplyToModel();

    SDFModel* m_model = nullptr;
    std::shared_ptr<SDFAnimationStateMachine> m_stateMachine;
    std::shared_ptr<SDFPoseLibrary> m_poseLibrary;

    // Direct clip playback
    std::shared_ptr<SDFAnimationClip> m_currentClip;
    float m_currentTime = 0.0f;
    float m_speed = 1.0f;
    bool m_isPlaying = false;
    bool m_isPaused = false;

    // Layers
    std::vector<AnimationLayer> m_layers;

    // Blending
    bool m_isBlending = false;
    std::unordered_map<std::string, SDFTransform> m_blendStartPose;
    std::unordered_map<std::string, SDFTransform> m_blendTargetPose;
    float m_blendTime = 0.0f;
    float m_blendDuration = 0.0f;
};

} // namespace Nova
