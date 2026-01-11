#pragma once

#include "BlendNode.hpp"
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace Nova {

class Animation;

/**
 * @brief 1D Blend Space for parameter-driven animation blending
 *
 * Allows blending between multiple animation clips based on a single
 * parameter value. Commonly used for locomotion (speed-based) or
 * directional blending.
 *
 * Features:
 * - Smooth interpolation between clips
 * - Automatic motion matching
 * - Root motion blending
 * - Sync markers for aligned playback
 * - Preview at any parameter value
 */
class BlendSpace1D {
public:
    /**
     * @brief Sample point in the blend space
     */
    struct Sample {
        std::string clipId;
        const Animation* clip = nullptr;
        float position = 0.0f;              // Parameter position
        float playbackSpeed = 1.0f;         // Speed multiplier
        bool syncMarker = false;            // Use sync markers
        float syncOffset = 0.0f;            // Sync phase offset

        // Motion data for matching
        float averageSpeed = 0.0f;          // Average root motion speed
        float averageAngularSpeed = 0.0f;   // Average rotation speed
    };

    /**
     * @brief Blend result containing pose and metadata
     */
    struct BlendResult {
        AnimationPose pose;
        glm::vec3 rootMotionDelta{0.0f};
        glm::quat rootRotationDelta{1.0f, 0.0f, 0.0f, 0.0f};
        float normalizedTime = 0.0f;
        size_t lowerSampleIndex = 0;
        size_t upperSampleIndex = 0;
        float blendWeight = 0.0f;
    };

    /**
     * @brief Sync marker for phase alignment
     */
    struct SyncMarker {
        std::string name;
        float normalizedTime;
    };

    BlendSpace1D() = default;
    explicit BlendSpace1D(const std::string& name);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set blend space name
     */
    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    /**
     * @brief Set the parameter name for this blend space
     */
    void SetParameterName(const std::string& name) { m_parameterName = name; }
    [[nodiscard]] const std::string& GetParameterName() const { return m_parameterName; }

    /**
     * @brief Set parameter range
     */
    void SetParameterRange(float min, float max);
    [[nodiscard]] float GetMinParameter() const { return m_minParameter; }
    [[nodiscard]] float GetMaxParameter() const { return m_maxParameter; }

    /**
     * @brief Set skeleton for pose evaluation
     */
    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

    // =========================================================================
    // Sample Management
    // =========================================================================

    /**
     * @brief Add a sample to the blend space
     * @return Index of added sample
     */
    size_t AddSample(const Animation* clip, float position, float playbackSpeed = 1.0f);

    /**
     * @brief Add a sample with full configuration
     */
    size_t AddSample(const Sample& sample);

    /**
     * @brief Remove sample at index
     */
    void RemoveSample(size_t index);

    /**
     * @brief Clear all samples
     */
    void ClearSamples();

    /**
     * @brief Get sample count
     */
    [[nodiscard]] size_t GetSampleCount() const { return m_samples.size(); }

    /**
     * @brief Get sample at index
     */
    [[nodiscard]] Sample& GetSample(size_t index) { return m_samples[index]; }
    [[nodiscard]] const Sample& GetSample(size_t index) const { return m_samples[index]; }

    /**
     * @brief Get all samples
     */
    [[nodiscard]] const std::vector<Sample>& GetSamples() const { return m_samples; }

    /**
     * @brief Sort samples by position
     */
    void SortSamples();

    /**
     * @brief Update sample position
     */
    void SetSamplePosition(size_t index, float position);

    /**
     * @brief Set sample playback speed
     */
    void SetSampleSpeed(size_t index, float speed);

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Evaluate the blend space at a parameter value
     * @param parameterValue Current parameter value
     * @param deltaTime Time since last evaluation
     * @return Blended animation pose
     */
    [[nodiscard]] BlendResult Evaluate(float parameterValue, float deltaTime);

    /**
     * @brief Preview at a parameter value without advancing time
     */
    [[nodiscard]] AnimationPose Preview(float parameterValue, float normalizedTime) const;

    /**
     * @brief Get weights for all samples at a parameter value
     */
    [[nodiscard]] std::vector<float> GetSampleWeights(float parameterValue) const;

    /**
     * @brief Reset playback state
     */
    void Reset();

    // =========================================================================
    // Time Synchronization
    // =========================================================================

    /**
     * @brief Enable time synchronization
     */
    void SetSyncEnabled(bool enabled) { m_syncEnabled = enabled; }
    [[nodiscard]] bool IsSyncEnabled() const { return m_syncEnabled; }

    /**
     * @brief Add sync marker
     */
    void AddSyncMarker(const std::string& name, float normalizedTime);

    /**
     * @brief Remove sync marker
     */
    void RemoveSyncMarker(const std::string& name);

    /**
     * @brief Get sync markers
     */
    [[nodiscard]] const std::vector<SyncMarker>& GetSyncMarkers() const { return m_syncMarkers; }

    // =========================================================================
    // Root Motion
    // =========================================================================

    /**
     * @brief Enable root motion blending
     */
    void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }
    [[nodiscard]] bool IsRootMotionEnabled() const { return m_rootMotionEnabled; }

    /**
     * @brief Set root bone name
     */
    void SetRootBoneName(const std::string& name) { m_rootBoneName = name; }

    /**
     * @brief Compute motion data for samples (call after adding samples)
     */
    void ComputeMotionData();

    // =========================================================================
    // Motion Matching
    // =========================================================================

    /**
     * @brief Enable automatic motion matching
     */
    void SetMotionMatchingEnabled(bool enabled) { m_motionMatchingEnabled = enabled; }
    [[nodiscard]] bool IsMotionMatchingEnabled() const { return m_motionMatchingEnabled; }

    /**
     * @brief Find best parameter value for desired motion
     */
    [[nodiscard]] float FindBestParameter(float desiredSpeed, float desiredAngularSpeed = 0.0f) const;

    // =========================================================================
    // Interpolation
    // =========================================================================

    /**
     * @brief Interpolation mode
     */
    enum class InterpolationMode {
        Linear,         // Linear blend between adjacent samples
        Smooth,         // Smooth step interpolation
        Cubic           // Cubic interpolation using 4 samples
    };

    void SetInterpolationMode(InterpolationMode mode) { m_interpolationMode = mode; }
    [[nodiscard]] InterpolationMode GetInterpolationMode() const { return m_interpolationMode; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(size_t)> OnSampleEnter;    // Called when entering a sample's influence
    std::function<void(size_t)> OnSampleExit;     // Called when exiting a sample's influence
    std::function<void()> OnLoopComplete;         // Called when animation loops

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Load from JSON
     */
    bool FromJson(const std::string& json);

    // =========================================================================
    // Build Blend Node
    // =========================================================================

    /**
     * @brief Create a Blend1DNode from this blend space
     */
    [[nodiscard]] std::unique_ptr<Blend1DNode> CreateBlendNode() const;

private:
    struct BlendIndices {
        size_t lower = 0;
        size_t upper = 0;
        float t = 0.0f;
    };

    BlendIndices FindBlendIndices(float value) const;
    AnimationPose BlendPoses(const AnimationPose& a, const AnimationPose& b, float t) const;
    float ComputeSyncedTime(float normalizedTime, size_t sampleIndex) const;

    std::string m_name;
    std::string m_parameterName = "Speed";
    float m_minParameter = 0.0f;
    float m_maxParameter = 1.0f;

    std::vector<Sample> m_samples;
    std::vector<SyncMarker> m_syncMarkers;

    Skeleton* m_skeleton = nullptr;
    std::string m_rootBoneName = "root";

    // State
    float m_currentTime = 0.0f;
    float m_normalizedTime = 0.0f;
    size_t m_lastLowerIndex = 0;
    size_t m_lastUpperIndex = 0;

    // Settings
    bool m_syncEnabled = true;
    bool m_rootMotionEnabled = true;
    bool m_motionMatchingEnabled = false;
    InterpolationMode m_interpolationMode = InterpolationMode::Linear;
};

/**
 * @brief Builder for BlendSpace1D
 */
class BlendSpace1DBuilder {
public:
    BlendSpace1DBuilder& SetName(const std::string& name);
    BlendSpace1DBuilder& SetParameter(const std::string& name, float min, float max);
    BlendSpace1DBuilder& SetSkeleton(Skeleton* skeleton);
    BlendSpace1DBuilder& AddSample(const Animation* clip, float position, float speed = 1.0f);
    BlendSpace1DBuilder& EnableSync(bool enabled = true);
    BlendSpace1DBuilder& EnableRootMotion(bool enabled = true);
    BlendSpace1DBuilder& SetInterpolation(BlendSpace1D::InterpolationMode mode);

    [[nodiscard]] std::unique_ptr<BlendSpace1D> Build();

private:
    std::unique_ptr<BlendSpace1D> m_blendSpace = std::make_unique<BlendSpace1D>();
};

} // namespace Nova
