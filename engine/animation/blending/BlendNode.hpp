#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

// Forward declarations
class Animation;
class Skeleton;

/**
 * @brief Bone transform for animation pose
 */
struct BoneTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    /**
     * @brief Convert to matrix
     */
    [[nodiscard]] glm::mat4 ToMatrix() const;

    /**
     * @brief Create from matrix
     */
    static BoneTransform FromMatrix(const glm::mat4& matrix);

    /**
     * @brief Interpolate between two transforms
     */
    static BoneTransform Lerp(const BoneTransform& a, const BoneTransform& b, float t);

    /**
     * @brief Add transforms (for additive blending)
     */
    static BoneTransform Add(const BoneTransform& base, const BoneTransform& additive);
};

/**
 * @brief Animation pose containing transforms for all bones
 */
class AnimationPose {
public:
    AnimationPose() = default;
    explicit AnimationPose(size_t boneCount);

    /**
     * @brief Set transform for a bone
     */
    void SetBoneTransform(size_t boneIndex, const BoneTransform& transform);
    void SetBoneTransform(const std::string& boneName, const BoneTransform& transform);

    /**
     * @brief Get transform for a bone
     */
    [[nodiscard]] const BoneTransform& GetBoneTransform(size_t boneIndex) const;
    [[nodiscard]] const BoneTransform* GetBoneTransform(const std::string& boneName) const;

    /**
     * @brief Get all transforms
     */
    [[nodiscard]] const std::vector<BoneTransform>& GetTransforms() const { return m_transforms; }
    [[nodiscard]] std::vector<BoneTransform>& GetTransforms() { return m_transforms; }

    /**
     * @brief Get bone count
     */
    [[nodiscard]] size_t GetBoneCount() const { return m_transforms.size(); }

    /**
     * @brief Resize to accommodate bones
     */
    void Resize(size_t boneCount);

    /**
     * @brief Clear all transforms to identity
     */
    void Clear();

    /**
     * @brief Set bone name mapping
     */
    void SetBoneMapping(const std::unordered_map<std::string, size_t>& mapping);

    /**
     * @brief Blend two poses
     */
    static AnimationPose Blend(const AnimationPose& a, const AnimationPose& b, float weight);

    /**
     * @brief Blend with mask
     */
    static AnimationPose BlendMasked(const AnimationPose& a, const AnimationPose& b,
                                      float weight, const std::vector<float>& mask);

    /**
     * @brief Additive blend
     */
    static AnimationPose AdditiveBend(const AnimationPose& base, const AnimationPose& additive, float weight);

    /**
     * @brief Root motion data
     */
    glm::vec3 rootMotionDelta{0.0f};
    glm::quat rootMotionRotation{1.0f, 0.0f, 0.0f, 0.0f};

private:
    std::vector<BoneTransform> m_transforms;
    std::unordered_map<std::string, size_t> m_boneMapping;
};

/**
 * @brief Base class for all blend nodes
 */
class BlendNode {
public:
    BlendNode() = default;
    virtual ~BlendNode() = default;

    // Non-copyable, movable
    BlendNode(const BlendNode&) = delete;
    BlendNode& operator=(const BlendNode&) = delete;
    BlendNode(BlendNode&&) noexcept = default;
    BlendNode& operator=(BlendNode&&) noexcept = default;

    /**
     * @brief Evaluate the blend node and produce a pose
     * @param deltaTime Time since last evaluation
     * @return Blended animation pose
     */
    virtual AnimationPose Evaluate(float deltaTime) = 0;

    /**
     * @brief Set a parameter value
     * @param name Parameter name
     * @param value Parameter value
     */
    virtual void SetParameter(const std::string& name, float value);

    /**
     * @brief Get a parameter value
     */
    [[nodiscard]] virtual float GetParameter(const std::string& name) const;

    /**
     * @brief Check if node has parameter
     */
    [[nodiscard]] bool HasParameter(const std::string& name) const;

    /**
     * @brief Get all parameter names
     */
    [[nodiscard]] std::vector<std::string> GetParameterNames() const;

    /**
     * @brief Set the skeleton reference
     */
    void SetSkeleton(Skeleton* skeleton) { m_skeleton = skeleton; }
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

    /**
     * @brief Get node name
     */
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    /**
     * @brief Reset node state
     */
    virtual void Reset();

    /**
     * @brief Get weight for blending
     */
    [[nodiscard]] float GetWeight() const { return m_weight; }
    void SetWeight(float weight) { m_weight = weight; }

    /**
     * @brief Get playback speed
     */
    [[nodiscard]] float GetSpeed() const { return m_speed; }
    void SetSpeed(float speed) { m_speed = speed; }

    /**
     * @brief Clone the node
     */
    [[nodiscard]] virtual std::unique_ptr<BlendNode> Clone() const = 0;

protected:
    std::string m_name;
    float m_weight = 1.0f;
    float m_speed = 1.0f;
    Skeleton* m_skeleton = nullptr;
    std::unordered_map<std::string, float> m_parameters;
};

/**
 * @brief Clip node - plays a single animation clip
 */
class ClipNode : public BlendNode {
public:
    ClipNode() = default;
    explicit ClipNode(const Animation* clip);

    /**
     * @brief Set the animation clip
     */
    void SetClip(const Animation* clip) { m_clip = clip; }
    [[nodiscard]] const Animation* GetClip() const { return m_clip; }

    /**
     * @brief Evaluate the clip at current time
     */
    AnimationPose Evaluate(float deltaTime) override;

    /**
     * @brief Get current playback time
     */
    [[nodiscard]] float GetTime() const { return m_time; }
    void SetTime(float time) { m_time = time; }

    /**
     * @brief Get normalized time (0-1)
     */
    [[nodiscard]] float GetNormalizedTime() const;
    void SetNormalizedTime(float normalizedTime);

    /**
     * @brief Set looping
     */
    void SetLooping(bool looping) { m_looping = looping; }
    [[nodiscard]] bool IsLooping() const { return m_looping; }

    /**
     * @brief Check if playback is complete
     */
    [[nodiscard]] bool IsComplete() const;

    /**
     * @brief Reset playback
     */
    void Reset() override;

    /**
     * @brief Clone
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Clone() const override;

    /**
     * @brief Enable root motion extraction
     */
    void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }
    [[nodiscard]] bool IsRootMotionEnabled() const { return m_rootMotionEnabled; }

    /**
     * @brief Callbacks
     */
    std::function<void()> OnComplete;
    std::function<void()> OnLoop;

private:
    const Animation* m_clip = nullptr;
    float m_time = 0.0f;
    bool m_looping = true;
    bool m_rootMotionEnabled = false;
    glm::vec3 m_lastRootPosition{0.0f};
    glm::quat m_lastRootRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief 1D Blend node - blends between clips based on a single parameter
 */
class Blend1DNode : public BlendNode {
public:
    struct BlendEntry {
        std::unique_ptr<BlendNode> node;
        float threshold = 0.0f;
        float speed = 1.0f;
        bool syncTime = true;
    };

    Blend1DNode() = default;
    explicit Blend1DNode(const std::string& parameterName);

    /**
     * @brief Add a blend entry
     */
    void AddEntry(std::unique_ptr<BlendNode> node, float threshold, float speed = 1.0f);

    /**
     * @brief Remove entry at index
     */
    void RemoveEntry(size_t index);

    /**
     * @brief Get entries
     */
    [[nodiscard]] const std::vector<BlendEntry>& GetEntries() const { return m_entries; }

    /**
     * @brief Sort entries by threshold
     */
    void SortEntries();

    /**
     * @brief Set blend parameter name
     */
    void SetBlendParameter(const std::string& name) { m_blendParameter = name; }
    [[nodiscard]] const std::string& GetBlendParameter() const { return m_blendParameter; }

    /**
     * @brief Evaluate blend
     */
    AnimationPose Evaluate(float deltaTime) override;

    /**
     * @brief Reset all children
     */
    void Reset() override;

    /**
     * @brief Clone
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Clone() const override;

    /**
     * @brief Set time synchronization
     */
    void SetSyncEnabled(bool enabled) { m_syncEnabled = enabled; }
    [[nodiscard]] bool IsSyncEnabled() const { return m_syncEnabled; }

private:
    void FindBlendIndices(float value, size_t& lower, size_t& upper, float& t) const;

    std::string m_blendParameter;
    std::vector<BlendEntry> m_entries;
    bool m_syncEnabled = true;
    float m_syncedTime = 0.0f;
};

/**
 * @brief 2D Blend node - blends between clips in 2D parameter space
 */
class Blend2DNode : public BlendNode {
public:
    struct BlendPoint {
        std::unique_ptr<BlendNode> node;
        glm::vec2 position{0.0f};
        float speed = 1.0f;
    };

    Blend2DNode() = default;
    Blend2DNode(const std::string& paramX, const std::string& paramY);

    /**
     * @brief Add a blend point
     */
    void AddPoint(std::unique_ptr<BlendNode> node, const glm::vec2& position, float speed = 1.0f);

    /**
     * @brief Remove point at index
     */
    void RemovePoint(size_t index);

    /**
     * @brief Get points
     */
    [[nodiscard]] const std::vector<BlendPoint>& GetPoints() const { return m_points; }

    /**
     * @brief Set parameter names
     */
    void SetParameterX(const std::string& name) { m_parameterX = name; }
    void SetParameterY(const std::string& name) { m_parameterY = name; }
    [[nodiscard]] const std::string& GetParameterX() const { return m_parameterX; }
    [[nodiscard]] const std::string& GetParameterY() const { return m_parameterY; }

    /**
     * @brief Evaluate blend
     */
    AnimationPose Evaluate(float deltaTime) override;

    /**
     * @brief Reset all children
     */
    void Reset() override;

    /**
     * @brief Clone
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Clone() const override;

    /**
     * @brief Rebuild triangulation
     */
    void RebuildTriangulation();

    /**
     * @brief Get triangulation for visualization
     */
    [[nodiscard]] const std::vector<std::array<size_t, 3>>& GetTriangles() const { return m_triangles; }

private:
    void CalculateWeights(const glm::vec2& pos, std::vector<float>& weights) const;
    void CalculateBarycentricWeights(const glm::vec2& pos, size_t triangleIndex,
                                      std::vector<float>& weights) const;
    size_t FindContainingTriangle(const glm::vec2& pos) const;

    std::string m_parameterX;
    std::string m_parameterY;
    std::vector<BlendPoint> m_points;
    std::vector<std::array<size_t, 3>> m_triangles;
    bool m_triangulationDirty = true;
};

/**
 * @brief Additive blend node - adds animation on top of base
 */
class AdditiveNode : public BlendNode {
public:
    AdditiveNode() = default;

    /**
     * @brief Set base node
     */
    void SetBaseNode(std::unique_ptr<BlendNode> node) { m_baseNode = std::move(node); }
    [[nodiscard]] BlendNode* GetBaseNode() const { return m_baseNode.get(); }

    /**
     * @brief Set additive node
     */
    void SetAdditiveNode(std::unique_ptr<BlendNode> node) { m_additiveNode = std::move(node); }
    [[nodiscard]] BlendNode* GetAdditiveNode() const { return m_additiveNode.get(); }

    /**
     * @brief Set additive weight parameter
     */
    void SetWeightParameter(const std::string& name) { m_weightParameter = name; }

    /**
     * @brief Set reference pose for additive calculation
     */
    void SetReferencePose(const AnimationPose& pose) { m_referencePose = pose; }

    /**
     * @brief Evaluate additive blend
     */
    AnimationPose Evaluate(float deltaTime) override;

    /**
     * @brief Reset both nodes
     */
    void Reset() override;

    /**
     * @brief Clone
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Clone() const override;

private:
    std::unique_ptr<BlendNode> m_baseNode;
    std::unique_ptr<BlendNode> m_additiveNode;
    std::string m_weightParameter;
    AnimationPose m_referencePose;
};

// Forward declare BlendMask
class BlendMask;

/**
 * @brief Layered blend node - combines multiple layers with masks
 */
class LayeredNode : public BlendNode {
public:
    struct Layer {
        std::unique_ptr<BlendNode> node;
        std::shared_ptr<BlendMask> mask;
        float weight = 1.0f;
        std::string weightParameter;
        bool additive = false;
        bool enabled = true;
    };

    LayeredNode() = default;

    /**
     * @brief Set base layer
     */
    void SetBaseLayer(std::unique_ptr<BlendNode> node);
    [[nodiscard]] BlendNode* GetBaseLayer() const { return m_baseLayer.get(); }

    /**
     * @brief Add an overlay layer
     */
    void AddLayer(std::unique_ptr<BlendNode> node, std::shared_ptr<BlendMask> mask = nullptr,
                  float weight = 1.0f, bool additive = false);

    /**
     * @brief Remove layer at index
     */
    void RemoveLayer(size_t index);

    /**
     * @brief Get layer count
     */
    [[nodiscard]] size_t GetLayerCount() const { return m_layers.size(); }

    /**
     * @brief Get layer
     */
    [[nodiscard]] Layer& GetLayer(size_t index) { return m_layers[index]; }
    [[nodiscard]] const Layer& GetLayer(size_t index) const { return m_layers[index]; }

    /**
     * @brief Set layer weight
     */
    void SetLayerWeight(size_t index, float weight);

    /**
     * @brief Set layer enabled
     */
    void SetLayerEnabled(size_t index, bool enabled);

    /**
     * @brief Evaluate all layers
     */
    AnimationPose Evaluate(float deltaTime) override;

    /**
     * @brief Reset all layers
     */
    void Reset() override;

    /**
     * @brief Clone
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Clone() const override;

private:
    std::unique_ptr<BlendNode> m_baseLayer;
    std::vector<Layer> m_layers;
};

/**
 * @brief State selector node - selects active child based on state
 */
class StateSelectorNode : public BlendNode {
public:
    struct State {
        std::string name;
        std::unique_ptr<BlendNode> node;
    };

    StateSelectorNode() = default;

    /**
     * @brief Add a state
     */
    void AddState(const std::string& name, std::unique_ptr<BlendNode> node);

    /**
     * @brief Remove state
     */
    void RemoveState(const std::string& name);

    /**
     * @brief Set current state
     */
    void SetCurrentState(const std::string& name, float blendTime = 0.2f);

    /**
     * @brief Get current state name
     */
    [[nodiscard]] const std::string& GetCurrentState() const { return m_currentState; }

    /**
     * @brief Evaluate current state
     */
    AnimationPose Evaluate(float deltaTime) override;

    /**
     * @brief Reset
     */
    void Reset() override;

    /**
     * @brief Clone
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Clone() const override;

private:
    std::unordered_map<std::string, std::unique_ptr<BlendNode>> m_states;
    std::string m_currentState;
    std::string m_previousState;
    float m_blendTime = 0.0f;
    float m_blendProgress = 1.0f;
};

} // namespace Nova
