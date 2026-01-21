#pragma once

/**
 * @file IBlendStrategy.hpp
 * @brief Unified interface for animation blending strategies
 *
 * Consolidates duplicate blending logic from:
 * - BlendSpace1D.cpp
 * - BlendSpace2D.cpp
 * - BlendNode.cpp
 * - BlendMask.cpp
 *
 * Provides a single, template-based interface for all blending operations
 * with support for Override, Additive, and Multiply blend modes.
 */

#include "blending/BlendNode.hpp"
#include "blending/BlendMask.hpp"
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

// Forward declarations
class Animation;
class Skeleton;

// =============================================================================
// Blend Mode Enumeration
// =============================================================================

/**
 * @brief Defines how poses are combined during blending
 */
enum class BlendMode {
    Override,   ///< Standard interpolation between poses
    Additive,   ///< Add difference from reference pose
    Multiply    ///< Multiply transforms (useful for scaling)
};

// =============================================================================
// Blend Input Structure
// =============================================================================

/**
 * @brief Input structure for blend operations
 *
 * Encapsulates all data needed for a single blend input including
 * the pose, weight, and blend mode.
 */
struct BlendInput {
    AnimationPose pose;             ///< The animation pose to blend
    float weight = 1.0f;            ///< Blend weight (0.0 - 1.0)
    BlendMode mode = BlendMode::Override;  ///< How this input should be blended

    BlendInput() = default;

    BlendInput(const AnimationPose& p, float w, BlendMode m = BlendMode::Override)
        : pose(p), weight(w), mode(m) {}

    /**
     * @brief Create override blend input
     */
    static BlendInput Override(const AnimationPose& pose, float weight) {
        return BlendInput(pose, weight, BlendMode::Override);
    }

    /**
     * @brief Create additive blend input
     */
    static BlendInput Additive(const AnimationPose& pose, float weight) {
        return BlendInput(pose, weight, BlendMode::Additive);
    }

    /**
     * @brief Create multiply blend input
     */
    static BlendInput Multiply(const AnimationPose& pose, float weight) {
        return BlendInput(pose, weight, BlendMode::Multiply);
    }
};

// =============================================================================
// Blend Configuration
// =============================================================================

/**
 * @brief Configuration options for blend operations
 */
struct BlendConfig {
    bool normalizeWeights = true;       ///< Normalize weights to sum to 1.0
    bool preserveRootMotion = true;     ///< Blend root motion data
    float weightThreshold = 0.001f;     ///< Minimum weight to consider
    size_t maxActivePoses = 8;          ///< Maximum poses to blend simultaneously
};

// =============================================================================
// IBlendStrategy Interface
// =============================================================================

/**
 * @brief Abstract interface for all blending strategies
 *
 * Defines the contract for blending animation poses. Implementations
 * provide different algorithms for combining poses (linear, spherical,
 * additive, etc.).
 */
class IBlendStrategy {
public:
    virtual ~IBlendStrategy() = default;

    // Non-copyable, movable
    IBlendStrategy() = default;
    IBlendStrategy(const IBlendStrategy&) = delete;
    IBlendStrategy& operator=(const IBlendStrategy&) = delete;
    IBlendStrategy(IBlendStrategy&&) noexcept = default;
    IBlendStrategy& operator=(IBlendStrategy&&) noexcept = default;

    /**
     * @brief Blend multiple inputs into a single pose
     * @param inputs Vector of blend inputs with poses and weights
     * @return Blended animation pose
     */
    [[nodiscard]] virtual AnimationPose Blend(const std::vector<BlendInput>& inputs) = 0;

    /**
     * @brief Blend two poses with a weight factor
     * @param a First pose
     * @param b Second pose
     * @param t Interpolation factor (0.0 = a, 1.0 = b)
     * @return Blended pose
     */
    [[nodiscard]] virtual AnimationPose BlendTwo(const AnimationPose& a,
                                                  const AnimationPose& b,
                                                  float t) = 0;

    /**
     * @brief Set the blend mask for selective blending
     * @param mask Blend mask defining per-bone weights
     */
    virtual void SetMask(const BlendMask& mask) = 0;

    /**
     * @brief Set the blend mask via shared pointer
     */
    virtual void SetMask(std::shared_ptr<BlendMask> mask) = 0;

    /**
     * @brief Get the current blend mask
     */
    [[nodiscard]] virtual const BlendMask* GetMask() const = 0;

    /**
     * @brief Clear the blend mask
     */
    virtual void ClearMask() = 0;

    /**
     * @brief Set blend configuration
     */
    virtual void SetConfig(const BlendConfig& config) { m_config = config; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const BlendConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get strategy name for debugging/serialization
     * @return Name of the blending strategy
     */
    [[nodiscard]] virtual std::string_view GetName() const = 0;

protected:
    BlendConfig m_config;
    std::shared_ptr<BlendMask> m_mask;

    /**
     * @brief Normalize input weights to sum to 1.0
     */
    static void NormalizeWeights(std::vector<BlendInput>& inputs, float threshold = 0.001f) {
        float totalWeight = 0.0f;
        for (const auto& input : inputs) {
            if (input.weight > threshold) {
                totalWeight += input.weight;
            }
        }
        if (totalWeight > threshold) {
            for (auto& input : inputs) {
                input.weight /= totalWeight;
            }
        }
    }

    /**
     * @brief Filter out inputs below weight threshold
     */
    static std::vector<BlendInput> FilterInputs(const std::vector<BlendInput>& inputs,
                                                 float threshold = 0.001f) {
        std::vector<BlendInput> filtered;
        filtered.reserve(inputs.size());
        for (const auto& input : inputs) {
            if (input.weight > threshold) {
                filtered.push_back(input);
            }
        }
        return filtered;
    }
};

// =============================================================================
// LinearBlendStrategy
// =============================================================================

/**
 * @brief Linear interpolation blending strategy
 *
 * Standard linear blend using LERP for positions and scales,
 * SLERP for rotations. Suitable for most blending scenarios.
 */
class LinearBlendStrategy : public IBlendStrategy {
public:
    LinearBlendStrategy() = default;

    [[nodiscard]] AnimationPose Blend(const std::vector<BlendInput>& inputs) override {
        if (inputs.empty()) {
            return AnimationPose{};
        }

        auto filtered = FilterInputs(inputs, m_config.weightThreshold);
        if (filtered.empty()) {
            return AnimationPose{};
        }

        if (m_config.normalizeWeights) {
            NormalizeWeights(filtered, m_config.weightThreshold);
        }

        // Start with first pose
        AnimationPose result = filtered[0].pose;
        float accumulatedWeight = filtered[0].weight;

        // Blend remaining poses
        for (size_t i = 1; i < filtered.size() && i < m_config.maxActivePoses; ++i) {
            const auto& input = filtered[i];

            switch (input.mode) {
                case BlendMode::Override:
                    result = BlendOverride(result, input.pose, input.weight, accumulatedWeight);
                    accumulatedWeight += input.weight;
                    break;

                case BlendMode::Additive:
                    result = BlendAdditive(result, input.pose, input.weight);
                    break;

                case BlendMode::Multiply:
                    result = BlendMultiply(result, input.pose, input.weight);
                    break;
            }
        }

        return result;
    }

    [[nodiscard]] AnimationPose BlendTwo(const AnimationPose& a,
                                          const AnimationPose& b,
                                          float t) override {
        return BlendPosesLinear(a, b, t, m_mask.get());
    }

    void SetMask(const BlendMask& mask) override {
        m_mask = std::make_shared<BlendMask>(mask.GetName());
        // Copy mask state - simplified copy
        if (mask.GetSkeleton()) {
            m_mask->SetSkeleton(mask.GetSkeleton());
        }
    }

    void SetMask(std::shared_ptr<BlendMask> mask) override {
        m_mask = std::move(mask);
    }

    [[nodiscard]] const BlendMask* GetMask() const override {
        return m_mask.get();
    }

    void ClearMask() override {
        m_mask.reset();
    }

    [[nodiscard]] std::string_view GetName() const override {
        return "LinearBlend";
    }

private:
    AnimationPose BlendOverride(const AnimationPose& base, const AnimationPose& overlay,
                                 float overlayWeight, float baseWeight) {
        float totalWeight = baseWeight + overlayWeight;
        float t = totalWeight > 0.001f ? overlayWeight / totalWeight : 0.5f;
        return BlendPosesLinear(base, overlay, t, m_mask.get());
    }

    AnimationPose BlendAdditive(const AnimationPose& base, const AnimationPose& additive,
                                 float weight) {
        return AnimationPose::AdditiveBend(base, additive, weight);
    }

    AnimationPose BlendMultiply(const AnimationPose& base, const AnimationPose& multiplier,
                                 float weight) {
        AnimationPose result(std::max(base.GetBoneCount(), multiplier.GetBoneCount()));

        for (size_t i = 0; i < result.GetBoneCount(); ++i) {
            const auto& tb = base.GetBoneTransform(i);
            const auto& tm = multiplier.GetBoneTransform(i);

            float maskWeight = GetMaskWeight(i) * weight;

            BoneTransform blended;
            blended.position = glm::mix(tb.position, tb.position * tm.position, maskWeight);
            blended.rotation = glm::slerp(tb.rotation,
                                          tb.rotation * tm.rotation, maskWeight);
            blended.scale = glm::mix(tb.scale, tb.scale * tm.scale, maskWeight);

            result.SetBoneTransform(i, blended);
        }

        return result;
    }

    static AnimationPose BlendPosesLinear(const AnimationPose& a, const AnimationPose& b,
                                           float t, const BlendMask* mask = nullptr) {
        size_t boneCount = std::max(a.GetBoneCount(), b.GetBoneCount());
        AnimationPose result(boneCount);

        for (size_t i = 0; i < boneCount; ++i) {
            float maskWeight = mask ? mask->GetBoneWeight(i) : 1.0f;
            float effectiveT = t * maskWeight;

            const auto& ta = a.GetBoneTransform(i);
            const auto& tb = b.GetBoneTransform(i);

            result.SetBoneTransform(i, BoneTransform::Lerp(ta, tb, effectiveT));
        }

        // Blend root motion
        result.rootMotionDelta = glm::mix(a.rootMotionDelta, b.rootMotionDelta, t);
        result.rootMotionRotation = glm::slerp(a.rootMotionRotation, b.rootMotionRotation, t);

        return result;
    }

    [[nodiscard]] float GetMaskWeight(size_t boneIndex) const {
        return m_mask ? m_mask->GetBoneWeight(boneIndex) : 1.0f;
    }
};

// =============================================================================
// SphericalBlendStrategy
// =============================================================================

/**
 * @brief Spherical interpolation blending strategy
 *
 * Uses SLERP for all components including positions (treated as
 * directions from origin). Better for blending rotational animations.
 */
class SphericalBlendStrategy : public IBlendStrategy {
public:
    SphericalBlendStrategy() = default;

    [[nodiscard]] AnimationPose Blend(const std::vector<BlendInput>& inputs) override {
        if (inputs.empty()) {
            return AnimationPose{};
        }

        auto filtered = FilterInputs(inputs, m_config.weightThreshold);
        if (filtered.empty()) {
            return AnimationPose{};
        }

        if (m_config.normalizeWeights) {
            NormalizeWeights(filtered, m_config.weightThreshold);
        }

        // For spherical blending with multiple inputs, use iterative approach
        AnimationPose result = filtered[0].pose;
        float accumulatedWeight = filtered[0].weight;

        for (size_t i = 1; i < filtered.size() && i < m_config.maxActivePoses; ++i) {
            float t = filtered[i].weight / (accumulatedWeight + filtered[i].weight);
            result = BlendTwo(result, filtered[i].pose, t);
            accumulatedWeight += filtered[i].weight;
        }

        return result;
    }

    [[nodiscard]] AnimationPose BlendTwo(const AnimationPose& a,
                                          const AnimationPose& b,
                                          float t) override {
        size_t boneCount = std::max(a.GetBoneCount(), b.GetBoneCount());
        AnimationPose result(boneCount);

        for (size_t i = 0; i < boneCount; ++i) {
            float maskWeight = m_mask ? m_mask->GetBoneWeight(i) : 1.0f;
            float effectiveT = t * maskWeight;

            const auto& ta = a.GetBoneTransform(i);
            const auto& tb = b.GetBoneTransform(i);

            BoneTransform blended;

            // SLERP for rotation (standard approach)
            blended.rotation = glm::slerp(ta.rotation, tb.rotation, effectiveT);

            // Spherical interpolation for position
            blended.position = SphericalLerpPosition(ta.position, tb.position, effectiveT);

            // Linear for scale (spherical doesn't make sense for scale)
            blended.scale = glm::mix(ta.scale, tb.scale, effectiveT);

            result.SetBoneTransform(i, blended);
        }

        // Blend root motion using spherical interpolation for direction
        result.rootMotionDelta = SphericalLerpPosition(a.rootMotionDelta, b.rootMotionDelta, t);
        result.rootMotionRotation = glm::slerp(a.rootMotionRotation, b.rootMotionRotation, t);

        return result;
    }

    void SetMask(const BlendMask& mask) override {
        m_mask = std::make_shared<BlendMask>(mask.GetName());
        if (mask.GetSkeleton()) {
            m_mask->SetSkeleton(mask.GetSkeleton());
        }
    }

    void SetMask(std::shared_ptr<BlendMask> mask) override {
        m_mask = std::move(mask);
    }

    [[nodiscard]] const BlendMask* GetMask() const override {
        return m_mask.get();
    }

    void ClearMask() override {
        m_mask.reset();
    }

    [[nodiscard]] std::string_view GetName() const override {
        return "SphericalBlend";
    }

private:
    /**
     * @brief Spherical linear interpolation for positions
     *
     * Treats positions as points on a sphere and interpolates along
     * the great circle arc between them.
     */
    static glm::vec3 SphericalLerpPosition(const glm::vec3& a, const glm::vec3& b, float t) {
        float lenA = glm::length(a);
        float lenB = glm::length(b);

        if (lenA < 0.0001f || lenB < 0.0001f) {
            return glm::mix(a, b, t);
        }

        glm::vec3 normA = a / lenA;
        glm::vec3 normB = b / lenB;

        float dot = glm::clamp(glm::dot(normA, normB), -1.0f, 1.0f);

        // If vectors are nearly parallel, use linear interpolation
        if (dot > 0.9995f) {
            return glm::mix(a, b, t);
        }

        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);

        float wa = std::sin((1.0f - t) * theta) / sinTheta;
        float wb = std::sin(t * theta) / sinTheta;

        glm::vec3 direction = normA * wa + normB * wb;
        float length = glm::mix(lenA, lenB, t);

        return direction * length;
    }
};

// =============================================================================
// AdditiveBlendStrategy
// =============================================================================

/**
 * @brief Additive blending strategy
 *
 * Computes the difference between poses and a reference, then adds
 * that difference to a base pose. Useful for layered animations.
 */
class AdditiveBlendStrategy : public IBlendStrategy {
public:
    AdditiveBlendStrategy() = default;

    /**
     * @brief Set the reference pose for additive calculation
     * @param pose Reference pose (typically bind pose or first frame)
     */
    void SetReferencePose(const AnimationPose& pose) {
        m_referencePose = pose;
        m_hasReference = true;
    }

    /**
     * @brief Check if reference pose is set
     */
    [[nodiscard]] bool HasReferencePose() const { return m_hasReference; }

    /**
     * @brief Clear the reference pose
     */
    void ClearReferencePose() {
        m_referencePose = AnimationPose{};
        m_hasReference = false;
    }

    [[nodiscard]] AnimationPose Blend(const std::vector<BlendInput>& inputs) override {
        if (inputs.empty()) {
            return AnimationPose{};
        }

        auto filtered = FilterInputs(inputs, m_config.weightThreshold);
        if (filtered.empty()) {
            return AnimationPose{};
        }

        // First input is always the base
        AnimationPose result = filtered[0].pose;

        // Apply all additive layers
        for (size_t i = 1; i < filtered.size() && i < m_config.maxActivePoses; ++i) {
            const auto& input = filtered[i];
            AnimationPose additivePose = input.pose;

            // Compute difference from reference if available
            if (m_hasReference && m_referencePose.GetBoneCount() > 0) {
                additivePose = ComputeAdditiveDifference(input.pose, m_referencePose);
            }

            result = ApplyAdditive(result, additivePose, input.weight);
        }

        return result;
    }

    [[nodiscard]] AnimationPose BlendTwo(const AnimationPose& base,
                                          const AnimationPose& additive,
                                          float weight) override {
        AnimationPose additivePose = additive;

        if (m_hasReference && m_referencePose.GetBoneCount() > 0) {
            additivePose = ComputeAdditiveDifference(additive, m_referencePose);
        }

        return ApplyAdditive(base, additivePose, weight);
    }

    void SetMask(const BlendMask& mask) override {
        m_mask = std::make_shared<BlendMask>(mask.GetName());
        if (mask.GetSkeleton()) {
            m_mask->SetSkeleton(mask.GetSkeleton());
        }
    }

    void SetMask(std::shared_ptr<BlendMask> mask) override {
        m_mask = std::move(mask);
    }

    [[nodiscard]] const BlendMask* GetMask() const override {
        return m_mask.get();
    }

    void ClearMask() override {
        m_mask.reset();
    }

    [[nodiscard]] std::string_view GetName() const override {
        return "AdditiveBlend";
    }

private:
    AnimationPose m_referencePose;
    bool m_hasReference = false;

    /**
     * @brief Compute the difference between a pose and the reference
     */
    [[nodiscard]] AnimationPose ComputeAdditiveDifference(const AnimationPose& pose,
                                                           const AnimationPose& reference) const {
        size_t boneCount = std::max(pose.GetBoneCount(), reference.GetBoneCount());
        AnimationPose difference(boneCount);

        for (size_t i = 0; i < boneCount; ++i) {
            const auto& tp = pose.GetBoneTransform(i);
            const auto& tr = reference.GetBoneTransform(i);

            BoneTransform diff;
            diff.position = tp.position - tr.position;
            diff.rotation = tp.rotation * glm::inverse(tr.rotation);
            diff.scale = tp.scale / tr.scale;

            difference.SetBoneTransform(i, diff);
        }

        difference.rootMotionDelta = pose.rootMotionDelta - reference.rootMotionDelta;
        difference.rootMotionRotation = pose.rootMotionRotation *
                                        glm::inverse(reference.rootMotionRotation);

        return difference;
    }

    /**
     * @brief Apply additive pose to base pose
     */
    [[nodiscard]] AnimationPose ApplyAdditive(const AnimationPose& base,
                                               const AnimationPose& additive,
                                               float weight) const {
        size_t boneCount = std::max(base.GetBoneCount(), additive.GetBoneCount());
        AnimationPose result(boneCount);

        for (size_t i = 0; i < boneCount; ++i) {
            float maskWeight = m_mask ? m_mask->GetBoneWeight(i) : 1.0f;
            float effectiveWeight = weight * maskWeight;

            const auto& tb = base.GetBoneTransform(i);
            const auto& ta = additive.GetBoneTransform(i);

            // Scale the additive transform by weight
            BoneTransform scaledAdditive;
            scaledAdditive.position = ta.position * effectiveWeight;
            scaledAdditive.rotation = glm::slerp(glm::quat(1, 0, 0, 0), ta.rotation, effectiveWeight);
            scaledAdditive.scale = glm::mix(glm::vec3(1.0f), ta.scale, effectiveWeight);

            result.SetBoneTransform(i, BoneTransform::Add(tb, scaledAdditive));
        }

        result.rootMotionDelta = base.rootMotionDelta + additive.rootMotionDelta * weight;
        result.rootMotionRotation = glm::slerp(glm::quat(1, 0, 0, 0),
                                                additive.rootMotionRotation, weight) *
                                    base.rootMotionRotation;

        return result;
    }
};

// =============================================================================
// BlendSpace Template
// =============================================================================

/**
 * @brief N-dimensional blend space template
 *
 * Provides a unified interface for 1D, 2D, and N-dimensional blend spaces.
 * Uses template parameter to specify the number of dimensions.
 *
 * @tparam Dimensions Number of parameter dimensions (1, 2, 3, etc.)
 */
template<size_t Dimensions>
class BlendSpace : public IBlendStrategy {
    static_assert(Dimensions >= 1, "BlendSpace requires at least 1 dimension");
    static_assert(Dimensions <= 4, "BlendSpace supports up to 4 dimensions");

public:
    using PositionType = std::array<float, Dimensions>;

    /**
     * @brief Sample in the N-dimensional blend space
     */
    struct Sample {
        PositionType position{};            ///< Position in parameter space
        const Animation* clip = nullptr;     ///< Animation clip
        std::string clipId;                  ///< Clip identifier
        float playbackSpeed = 1.0f;          ///< Playback speed multiplier

        Sample() {
            position.fill(0.0f);
        }
    };

    BlendSpace() = default;
    explicit BlendSpace(const std::string& name) : m_name(name) {}

    // =========================================================================
    // Sample Management
    // =========================================================================

    /**
     * @brief Add a sample to the blend space
     * @param position Position in parameter space
     * @param clip Animation clip
     * @return Index of added sample
     */
    size_t AddSample(const PositionType& position, const Animation* clip) {
        Sample sample;
        sample.position = position;
        sample.clip = clip;
        sample.clipId = clip ? clip->GetName() : "";
        m_samples.push_back(sample);
        m_dirty = true;
        return m_samples.size() - 1;
    }

    /**
     * @brief Add a sample with full configuration
     */
    size_t AddSample(const Sample& sample) {
        m_samples.push_back(sample);
        m_dirty = true;
        return m_samples.size() - 1;
    }

    /**
     * @brief Remove sample at index
     */
    void RemoveSample(size_t index) {
        if (index < m_samples.size()) {
            m_samples.erase(m_samples.begin() + static_cast<ptrdiff_t>(index));
            m_dirty = true;
        }
    }

    /**
     * @brief Clear all samples
     */
    void ClearSamples() {
        m_samples.clear();
        m_dirty = true;
    }

    /**
     * @brief Get sample count
     */
    [[nodiscard]] size_t GetSampleCount() const { return m_samples.size(); }

    /**
     * @brief Get sample at index
     */
    [[nodiscard]] const Sample& GetSample(size_t index) const { return m_samples[index]; }
    [[nodiscard]] Sample& GetSample(size_t index) { return m_samples[index]; }

    /**
     * @brief Get all samples
     */
    [[nodiscard]] const std::vector<Sample>& GetSamples() const { return m_samples; }

    // =========================================================================
    // Parameter Control
    // =========================================================================

    /**
     * @brief Set the current parameter value
     * @param value Parameter position for evaluation
     */
    void SetParameter(const PositionType& value) {
        m_currentParameter = value;
    }

    /**
     * @brief Get the current parameter value
     */
    [[nodiscard]] const PositionType& GetParameter() const { return m_currentParameter; }

    /**
     * @brief Set parameter bounds
     * @param min Minimum bounds for each dimension
     * @param max Maximum bounds for each dimension
     */
    void SetBounds(const PositionType& min, const PositionType& max) {
        m_minBounds = min;
        m_maxBounds = max;
    }

    /**
     * @brief Get minimum bounds
     */
    [[nodiscard]] const PositionType& GetMinBounds() const { return m_minBounds; }

    /**
     * @brief Get maximum bounds
     */
    [[nodiscard]] const PositionType& GetMaxBounds() const { return m_maxBounds; }

    // =========================================================================
    // IBlendStrategy Interface
    // =========================================================================

    [[nodiscard]] AnimationPose Blend(const std::vector<BlendInput>& inputs) override {
        // BlendSpace uses its internal samples and parameter, not external inputs
        // This method provides compatibility with IBlendStrategy interface
        if (!inputs.empty()) {
            return m_linearStrategy.Blend(inputs);
        }
        return EvaluateAtParameter(m_currentParameter);
    }

    [[nodiscard]] AnimationPose BlendTwo(const AnimationPose& a,
                                          const AnimationPose& b,
                                          float t) override {
        return m_linearStrategy.BlendTwo(a, b, t);
    }

    /**
     * @brief Evaluate the blend space at current parameter
     * @return Blended animation pose
     */
    [[nodiscard]] AnimationPose EvaluateAtParameter(const PositionType& parameter) {
        if (m_samples.empty()) {
            return AnimationPose{};
        }

        // Clamp parameter to bounds
        PositionType clampedParam = ClampToBounds(parameter);

        // Calculate weights for all samples
        std::vector<float> weights = CalculateWeights(clampedParam);

        // Build blend inputs
        std::vector<BlendInput> inputs;
        inputs.reserve(m_samples.size());

        for (size_t i = 0; i < m_samples.size(); ++i) {
            if (weights[i] > m_config.weightThreshold && m_samples[i].clip) {
                // Evaluate the clip - this would need skeleton and time info
                // For now, create placeholder
                AnimationPose pose;
                inputs.push_back(BlendInput::Override(pose, weights[i]));
            }
        }

        return m_linearStrategy.Blend(inputs);
    }

    /**
     * @brief Get sample weights at a parameter position
     * @param parameter Position in parameter space
     * @return Vector of weights, one per sample
     */
    [[nodiscard]] std::vector<float> GetSampleWeights(const PositionType& parameter) const {
        return CalculateWeights(ClampToBounds(parameter));
    }

    void SetMask(const BlendMask& mask) override {
        m_linearStrategy.SetMask(mask);
    }

    void SetMask(std::shared_ptr<BlendMask> mask) override {
        m_linearStrategy.SetMask(std::move(mask));
    }

    [[nodiscard]] const BlendMask* GetMask() const override {
        return m_linearStrategy.GetMask();
    }

    void ClearMask() override {
        m_linearStrategy.ClearMask();
    }

    [[nodiscard]] std::string_view GetName() const override {
        return m_name;
    }

    /**
     * @brief Set the blend space name
     */
    void SetName(const std::string& name) { m_name = name; }

    /**
     * @brief Set the skeleton for pose evaluation
     */
    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }

    /**
     * @brief Get the skeleton
     */
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

protected:
    /**
     * @brief Calculate blend weights at a parameter position
     *
     * Default implementation uses inverse distance weighting.
     * Specialized for 1D (linear interpolation) and 2D (barycentric).
     */
    [[nodiscard]] virtual std::vector<float> CalculateWeights(const PositionType& param) const {
        std::vector<float> weights(m_samples.size(), 0.0f);

        if (m_samples.empty()) {
            return weights;
        }

        if (m_samples.size() == 1) {
            weights[0] = 1.0f;
            return weights;
        }

        // Inverse distance weighting (default for N > 2)
        float totalWeight = 0.0f;
        for (size_t i = 0; i < m_samples.size(); ++i) {
            float dist = Distance(param, m_samples[i].position);
            weights[i] = 1.0f / (dist + 0.001f);
            totalWeight += weights[i];
        }

        if (totalWeight > 0.001f) {
            for (float& w : weights) {
                w /= totalWeight;
            }
        }

        return weights;
    }

    /**
     * @brief Calculate distance between two positions
     */
    [[nodiscard]] static float Distance(const PositionType& a, const PositionType& b) {
        float distSq = 0.0f;
        for (size_t i = 0; i < Dimensions; ++i) {
            float diff = a[i] - b[i];
            distSq += diff * diff;
        }
        return std::sqrt(distSq);
    }

    /**
     * @brief Clamp parameter to bounds
     */
    [[nodiscard]] PositionType ClampToBounds(const PositionType& param) const {
        PositionType clamped;
        for (size_t i = 0; i < Dimensions; ++i) {
            clamped[i] = std::clamp(param[i], m_minBounds[i], m_maxBounds[i]);
        }
        return clamped;
    }

    std::string m_name;
    std::vector<Sample> m_samples;
    PositionType m_currentParameter{};
    PositionType m_minBounds{};
    PositionType m_maxBounds{};
    Skeleton* m_skeleton = nullptr;
    LinearBlendStrategy m_linearStrategy;
    bool m_dirty = true;
};

// =============================================================================
// BlendSpace1D Specialization
// =============================================================================

/**
 * @brief 1D Blend Space specialization
 *
 * Optimized for single-parameter blending with linear interpolation
 * between adjacent samples.
 */
template<>
class BlendSpace<1> : public IBlendStrategy {
public:
    using PositionType = std::array<float, 1>;

    struct Sample {
        PositionType position{};
        const Animation* clip = nullptr;
        std::string clipId;
        float playbackSpeed = 1.0f;

        Sample() { position[0] = 0.0f; }
    };

    BlendSpace() = default;
    explicit BlendSpace(const std::string& name) : m_name(name) {}

    // Simplified sample management for 1D
    size_t AddSample(float position, const Animation* clip) {
        Sample sample;
        sample.position[0] = position;
        sample.clip = clip;
        m_samples.push_back(sample);
        SortSamples();
        return m_samples.size() - 1;
    }

    size_t AddSample(const Sample& sample) {
        m_samples.push_back(sample);
        SortSamples();
        return m_samples.size() - 1;
    }

    void RemoveSample(size_t index) {
        if (index < m_samples.size()) {
            m_samples.erase(m_samples.begin() + static_cast<ptrdiff_t>(index));
        }
    }

    void ClearSamples() { m_samples.clear(); }
    [[nodiscard]] size_t GetSampleCount() const { return m_samples.size(); }
    [[nodiscard]] const Sample& GetSample(size_t index) const { return m_samples[index]; }
    [[nodiscard]] Sample& GetSample(size_t index) { return m_samples[index]; }
    [[nodiscard]] const std::vector<Sample>& GetSamples() const { return m_samples; }

    void SetParameter(float value) { m_currentParameter = value; }
    [[nodiscard]] float GetParameter() const { return m_currentParameter; }

    void SetBounds(float min, float max) {
        m_minBound = min;
        m_maxBound = max;
    }

    [[nodiscard]] float GetMinBound() const { return m_minBound; }
    [[nodiscard]] float GetMaxBound() const { return m_maxBound; }

    [[nodiscard]] AnimationPose Blend(const std::vector<BlendInput>& inputs) override {
        if (!inputs.empty()) {
            return m_linearStrategy.Blend(inputs);
        }
        return AnimationPose{};
    }

    [[nodiscard]] AnimationPose BlendTwo(const AnimationPose& a,
                                          const AnimationPose& b,
                                          float t) override {
        return m_linearStrategy.BlendTwo(a, b, t);
    }

    /**
     * @brief Get blend indices and interpolation factor for a parameter value
     * @param value Parameter value
     * @param lower Output: lower sample index
     * @param upper Output: upper sample index
     * @param t Output: interpolation factor (0.0 = lower, 1.0 = upper)
     */
    void FindBlendIndices(float value, size_t& lower, size_t& upper, float& t) const {
        if (m_samples.empty()) {
            lower = upper = 0;
            t = 0.0f;
            return;
        }

        if (m_samples.size() == 1) {
            lower = upper = 0;
            t = 0.0f;
            return;
        }

        value = std::clamp(value, m_minBound, m_maxBound);

        // Find surrounding samples (samples are sorted)
        for (size_t i = 0; i < m_samples.size() - 1; ++i) {
            if (value >= m_samples[i].position[0] &&
                value <= m_samples[i + 1].position[0]) {
                lower = i;
                upper = i + 1;
                float range = m_samples[upper].position[0] - m_samples[lower].position[0];
                t = range > 0.0f ? (value - m_samples[lower].position[0]) / range : 0.0f;
                return;
            }
        }

        // Clamp to ends
        if (value <= m_samples.front().position[0]) {
            lower = upper = 0;
        } else {
            lower = upper = m_samples.size() - 1;
        }
        t = 0.0f;
    }

    /**
     * @brief Get sample weights at a parameter value
     */
    [[nodiscard]] std::vector<float> GetSampleWeights(float value) const {
        std::vector<float> weights(m_samples.size(), 0.0f);

        if (m_samples.empty()) return weights;

        size_t lower, upper;
        float t;
        FindBlendIndices(value, lower, upper, t);

        if (lower == upper) {
            weights[lower] = 1.0f;
        } else {
            weights[lower] = 1.0f - t;
            weights[upper] = t;
        }

        return weights;
    }

    void SetMask(const BlendMask& mask) override {
        m_linearStrategy.SetMask(mask);
    }

    void SetMask(std::shared_ptr<BlendMask> mask) override {
        m_linearStrategy.SetMask(std::move(mask));
    }

    [[nodiscard]] const BlendMask* GetMask() const override {
        return m_linearStrategy.GetMask();
    }

    void ClearMask() override {
        m_linearStrategy.ClearMask();
    }

    [[nodiscard]] std::string_view GetName() const override {
        return m_name;
    }

    void SetName(const std::string& name) { m_name = name; }
    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

private:
    void SortSamples() {
        std::sort(m_samples.begin(), m_samples.end(),
                  [](const Sample& a, const Sample& b) {
                      return a.position[0] < b.position[0];
                  });
    }

    std::string m_name;
    std::vector<Sample> m_samples;
    float m_currentParameter = 0.0f;
    float m_minBound = 0.0f;
    float m_maxBound = 1.0f;
    Skeleton* m_skeleton = nullptr;
    LinearBlendStrategy m_linearStrategy;
};

// =============================================================================
// BlendSpace2D Specialization
// =============================================================================

/**
 * @brief 2D Blend Space specialization
 *
 * Uses Delaunay triangulation and barycentric coordinates for
 * smooth blending in 2D parameter space.
 */
template<>
class BlendSpace<2> : public IBlendStrategy {
public:
    using PositionType = std::array<float, 2>;

    struct Sample {
        PositionType position{};
        const Animation* clip = nullptr;
        std::string clipId;
        float playbackSpeed = 1.0f;

        Sample() {
            position[0] = 0.0f;
            position[1] = 0.0f;
        }
    };

    struct Triangle {
        std::array<size_t, 3> indices;
        glm::vec2 circumcenter;
        float circumradiusSq;
    };

    BlendSpace() = default;
    explicit BlendSpace(const std::string& name) : m_name(name) {}

    // Sample management
    size_t AddSample(float x, float y, const Animation* clip) {
        Sample sample;
        sample.position[0] = x;
        sample.position[1] = y;
        sample.clip = clip;
        m_samples.push_back(sample);
        m_triangulationDirty = true;
        return m_samples.size() - 1;
    }

    size_t AddSample(const glm::vec2& pos, const Animation* clip) {
        return AddSample(pos.x, pos.y, clip);
    }

    size_t AddSample(const Sample& sample) {
        m_samples.push_back(sample);
        m_triangulationDirty = true;
        return m_samples.size() - 1;
    }

    void RemoveSample(size_t index) {
        if (index < m_samples.size()) {
            m_samples.erase(m_samples.begin() + static_cast<ptrdiff_t>(index));
            m_triangulationDirty = true;
        }
    }

    void ClearSamples() {
        m_samples.clear();
        m_triangles.clear();
        m_triangulationDirty = true;
    }

    [[nodiscard]] size_t GetSampleCount() const { return m_samples.size(); }
    [[nodiscard]] const Sample& GetSample(size_t index) const { return m_samples[index]; }
    [[nodiscard]] Sample& GetSample(size_t index) { return m_samples[index]; }
    [[nodiscard]] const std::vector<Sample>& GetSamples() const { return m_samples; }

    // Parameter control
    void SetParameter(float x, float y) {
        m_currentParameter[0] = x;
        m_currentParameter[1] = y;
    }

    void SetParameter(const glm::vec2& pos) {
        SetParameter(pos.x, pos.y);
    }

    [[nodiscard]] glm::vec2 GetParameter() const {
        return glm::vec2(m_currentParameter[0], m_currentParameter[1]);
    }

    void SetBounds(const glm::vec2& min, const glm::vec2& max) {
        m_minBounds = {min.x, min.y};
        m_maxBounds = {max.x, max.y};
    }

    [[nodiscard]] glm::vec2 GetMinBounds() const {
        return glm::vec2(m_minBounds[0], m_minBounds[1]);
    }

    [[nodiscard]] glm::vec2 GetMaxBounds() const {
        return glm::vec2(m_maxBounds[0], m_maxBounds[1]);
    }

    // Triangulation
    void RebuildTriangulation() {
        if (!m_triangulationDirty) return;

        m_triangles.clear();

        if (m_samples.size() < 3) {
            m_triangulationDirty = false;
            return;
        }

        // Simple Delaunay triangulation
        // For production, use a proper library like CGAL
        BowyerWatson();
        m_triangulationDirty = false;
    }

    [[nodiscard]] bool IsTriangulationDirty() const { return m_triangulationDirty; }
    [[nodiscard]] const std::vector<Triangle>& GetTriangles() const { return m_triangles; }

    /**
     * @brief Find triangle containing a point
     * @return Triangle index or SIZE_MAX if outside
     */
    [[nodiscard]] size_t FindContainingTriangle(float x, float y) const {
        glm::vec2 pos(x, y);
        for (size_t i = 0; i < m_triangles.size(); ++i) {
            glm::vec3 bary = ComputeBarycentric(pos, m_triangles[i]);
            if (bary.x >= 0 && bary.y >= 0 && bary.z >= 0) {
                return i;
            }
        }
        return SIZE_MAX;
    }

    /**
     * @brief Get sample weights at a parameter position
     */
    [[nodiscard]] std::vector<float> GetSampleWeights(float x, float y) const {
        std::vector<float> weights(m_samples.size(), 0.0f);

        if (m_samples.empty()) return weights;

        glm::vec2 pos = ClampToBounds(glm::vec2(x, y));

        if (m_samples.size() == 1) {
            weights[0] = 1.0f;
            return weights;
        }

        if (m_samples.size() == 2) {
            float d0 = glm::distance(pos, GetSamplePosition(0));
            float d1 = glm::distance(pos, GetSamplePosition(1));
            float total = d0 + d1;
            if (total > 0.001f) {
                weights[0] = d1 / total;
                weights[1] = d0 / total;
            } else {
                weights[0] = weights[1] = 0.5f;
            }
            return weights;
        }

        // Try to find containing triangle
        size_t triIdx = FindContainingTriangle(pos.x, pos.y);
        if (triIdx != SIZE_MAX) {
            const auto& tri = m_triangles[triIdx];
            glm::vec3 bary = ComputeBarycentric(pos, tri);
            weights[tri.indices[0]] = bary.x;
            weights[tri.indices[1]] = bary.y;
            weights[tri.indices[2]] = bary.z;
        } else {
            // Fallback: inverse distance weighting
            float totalWeight = 0.0f;
            for (size_t i = 0; i < m_samples.size(); ++i) {
                float dist = glm::distance(pos, GetSamplePosition(i));
                weights[i] = 1.0f / (dist + 0.001f);
                totalWeight += weights[i];
            }
            if (totalWeight > 0.0f) {
                for (float& w : weights) {
                    w /= totalWeight;
                }
            }
        }

        return weights;
    }

    [[nodiscard]] std::vector<float> GetSampleWeights(const glm::vec2& pos) const {
        return GetSampleWeights(pos.x, pos.y);
    }

    // IBlendStrategy interface
    [[nodiscard]] AnimationPose Blend(const std::vector<BlendInput>& inputs) override {
        if (!inputs.empty()) {
            return m_linearStrategy.Blend(inputs);
        }
        return AnimationPose{};
    }

    [[nodiscard]] AnimationPose BlendTwo(const AnimationPose& a,
                                          const AnimationPose& b,
                                          float t) override {
        return m_linearStrategy.BlendTwo(a, b, t);
    }

    void SetMask(const BlendMask& mask) override {
        m_linearStrategy.SetMask(mask);
    }

    void SetMask(std::shared_ptr<BlendMask> mask) override {
        m_linearStrategy.SetMask(std::move(mask));
    }

    [[nodiscard]] const BlendMask* GetMask() const override {
        return m_linearStrategy.GetMask();
    }

    void ClearMask() override {
        m_linearStrategy.ClearMask();
    }

    [[nodiscard]] std::string_view GetName() const override {
        return m_name;
    }

    void SetName(const std::string& name) { m_name = name; }
    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

private:
    [[nodiscard]] glm::vec2 GetSamplePosition(size_t index) const {
        return glm::vec2(m_samples[index].position[0], m_samples[index].position[1]);
    }

    [[nodiscard]] glm::vec2 ClampToBounds(const glm::vec2& pos) const {
        return glm::clamp(pos,
                          glm::vec2(m_minBounds[0], m_minBounds[1]),
                          glm::vec2(m_maxBounds[0], m_maxBounds[1]));
    }

    [[nodiscard]] glm::vec3 ComputeBarycentric(const glm::vec2& p, const Triangle& tri) const {
        glm::vec2 v0 = GetSamplePosition(tri.indices[0]);
        glm::vec2 v1 = GetSamplePosition(tri.indices[1]);
        glm::vec2 v2 = GetSamplePosition(tri.indices[2]);

        glm::vec2 e0 = v1 - v0;
        glm::vec2 e1 = v2 - v0;
        glm::vec2 e2 = p - v0;

        float d00 = glm::dot(e0, e0);
        float d01 = glm::dot(e0, e1);
        float d11 = glm::dot(e1, e1);
        float d20 = glm::dot(e2, e0);
        float d21 = glm::dot(e2, e1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 0.0001f) {
            return glm::vec3(-1.0f);
        }

        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;

        return glm::vec3(u, v, w);
    }

    void BowyerWatson() {
        // Simplified Bowyer-Watson implementation
        // Same algorithm as in BlendSpace2D.cpp

        float minX = m_minBounds[0] - 1.0f;
        float maxX = m_maxBounds[0] + 1.0f;
        float minY = m_minBounds[1] - 1.0f;
        float maxY = m_maxBounds[1] + 1.0f;

        float dx = maxX - minX;
        float dy = maxY - minY;
        float dmax = std::max(dx, dy);
        float midX = (minX + maxX) * 0.5f;
        float midY = (minY + maxY) * 0.5f;

        // Super triangle vertices
        std::vector<glm::vec2> points;
        points.push_back(glm::vec2(midX - 20.0f * dmax, midY - dmax));
        points.push_back(glm::vec2(midX, midY + 20.0f * dmax));
        points.push_back(glm::vec2(midX + 20.0f * dmax, midY - dmax));

        for (const auto& sample : m_samples) {
            points.push_back(glm::vec2(sample.position[0], sample.position[1]));
        }

        // Initialize with super triangle
        std::vector<Triangle> triangulation;
        Triangle superTri;
        superTri.indices = {0, 1, 2};
        triangulation.push_back(superTri);

        // Add each point
        for (size_t i = 3; i < points.size(); ++i) {
            const glm::vec2& p = points[i];

            std::vector<Triangle> badTriangles;
            std::vector<size_t> badIndices;

            for (size_t j = 0; j < triangulation.size(); ++j) {
                // Simple circumcircle check
                glm::vec2 v0 = points[triangulation[j].indices[0]];
                glm::vec2 v1 = points[triangulation[j].indices[1]];
                glm::vec2 v2 = points[triangulation[j].indices[2]];

                glm::vec2 center;
                float radiusSq;
                CalculateCircumcircle(v0, v1, v2, center, radiusSq);

                float distSq = glm::dot(p - center, p - center);
                if (distSq < radiusSq) {
                    badTriangles.push_back(triangulation[j]);
                    badIndices.push_back(j);
                }
            }

            // Find boundary edges
            std::vector<std::pair<size_t, size_t>> polygon;
            for (const auto& tri : badTriangles) {
                for (int e = 0; e < 3; ++e) {
                    size_t e1 = tri.indices[e];
                    size_t e2 = tri.indices[(e + 1) % 3];

                    bool shared = false;
                    for (const auto& other : badTriangles) {
                        if (&tri == &other) continue;
                        for (int oe = 0; oe < 3; ++oe) {
                            size_t oe1 = other.indices[oe];
                            size_t oe2 = other.indices[(oe + 1) % 3];
                            if ((e1 == oe1 && e2 == oe2) || (e1 == oe2 && e2 == oe1)) {
                                shared = true;
                                break;
                            }
                        }
                        if (shared) break;
                    }

                    if (!shared) {
                        polygon.push_back({e1, e2});
                    }
                }
            }

            // Remove bad triangles
            std::sort(badIndices.begin(), badIndices.end(), std::greater<size_t>());
            for (size_t idx : badIndices) {
                triangulation.erase(triangulation.begin() + static_cast<ptrdiff_t>(idx));
            }

            // Create new triangles
            for (const auto& edge : polygon) {
                Triangle newTri;
                newTri.indices = {edge.first, edge.second, i};

                glm::vec2 v0 = points[newTri.indices[0]];
                glm::vec2 v1 = points[newTri.indices[1]];
                glm::vec2 v2 = points[newTri.indices[2]];

                float cross = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
                if (cross < 0) {
                    std::swap(newTri.indices[1], newTri.indices[2]);
                }

                triangulation.push_back(newTri);
            }
        }

        // Remove triangles with super triangle vertices and adjust indices
        m_triangles.clear();
        for (const auto& tri : triangulation) {
            bool hasSuperVertex = false;
            for (size_t idx : tri.indices) {
                if (idx < 3) {
                    hasSuperVertex = true;
                    break;
                }
            }
            if (!hasSuperVertex) {
                Triangle adjusted;
                adjusted.indices = {tri.indices[0] - 3, tri.indices[1] - 3, tri.indices[2] - 3};

                // Calculate final circumcircle
                glm::vec2 v0 = GetSamplePosition(adjusted.indices[0]);
                glm::vec2 v1 = GetSamplePosition(adjusted.indices[1]);
                glm::vec2 v2 = GetSamplePosition(adjusted.indices[2]);
                CalculateCircumcircle(v0, v1, v2, adjusted.circumcenter, adjusted.circumradiusSq);

                m_triangles.push_back(adjusted);
            }
        }
    }

    static void CalculateCircumcircle(const glm::vec2& v0, const glm::vec2& v1,
                                       const glm::vec2& v2, glm::vec2& center,
                                       float& radiusSq) {
        float ax = v0.x, ay = v0.y;
        float bx = v1.x, by = v1.y;
        float cx = v2.x, cy = v2.y;

        float d = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs(d) > 0.0001f) {
            float ux = ((ax * ax + ay * ay) * (by - cy) +
                       (bx * bx + by * by) * (cy - ay) +
                       (cx * cx + cy * cy) * (ay - by)) / d;
            float uy = ((ax * ax + ay * ay) * (cx - bx) +
                       (bx * bx + by * by) * (ax - cx) +
                       (cx * cx + cy * cy) * (bx - ax)) / d;
            center = glm::vec2(ux, uy);
            radiusSq = (ux - ax) * (ux - ax) + (uy - ay) * (uy - ay);
        } else {
            center = (v0 + v1 + v2) / 3.0f;
            radiusSq = 0.0f;
        }
    }

    std::string m_name;
    std::vector<Sample> m_samples;
    std::vector<Triangle> m_triangles;
    PositionType m_currentParameter{};
    PositionType m_minBounds{};
    PositionType m_maxBounds{};
    Skeleton* m_skeleton = nullptr;
    LinearBlendStrategy m_linearStrategy;
    bool m_triangulationDirty = true;
};

// =============================================================================
// Type Aliases
// =============================================================================

/**
 * @brief Type alias for 1D blend space
 */
using BlendSpace1DStrategy = BlendSpace<1>;

/**
 * @brief Type alias for 2D blend space
 */
using BlendSpace2DStrategy = BlendSpace<2>;

/**
 * @brief Type alias for 3D blend space (e.g., speed, direction, turn rate)
 */
using BlendSpace3D = BlendSpace<3>;

// =============================================================================
// Utility Functions
// =============================================================================

namespace BlendUtil {

/**
 * @brief Create a linear blend strategy
 */
inline std::unique_ptr<IBlendStrategy> CreateLinear() {
    return std::make_unique<LinearBlendStrategy>();
}

/**
 * @brief Create a spherical blend strategy
 */
inline std::unique_ptr<IBlendStrategy> CreateSpherical() {
    return std::make_unique<SphericalBlendStrategy>();
}

/**
 * @brief Create an additive blend strategy
 */
inline std::unique_ptr<IBlendStrategy> CreateAdditive() {
    return std::make_unique<AdditiveBlendStrategy>();
}

/**
 * @brief Create an additive blend strategy with reference pose
 */
inline std::unique_ptr<AdditiveBlendStrategy> CreateAdditive(const AnimationPose& reference) {
    auto strategy = std::make_unique<AdditiveBlendStrategy>();
    strategy->SetReferencePose(reference);
    return strategy;
}

/**
 * @brief Apply smooth step function to blend weight
 * @param t Input weight (0-1)
 * @return Smoothed weight
 */
inline float SmoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Apply smoother step function (Ken Perlin's improved version)
 * @param t Input weight (0-1)
 * @return Smoothed weight
 */
inline float SmootherStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/**
 * @brief Blend two poses with smooth interpolation
 */
inline AnimationPose BlendSmooth(const AnimationPose& a, const AnimationPose& b,
                                  float t, IBlendStrategy& strategy) {
    return strategy.BlendTwo(a, b, SmoothStep(t));
}

} // namespace BlendUtil

} // namespace Nova
