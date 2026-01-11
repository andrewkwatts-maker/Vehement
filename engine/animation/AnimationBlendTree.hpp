#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

namespace Nova {

using json = nlohmann::json;

// Forward declarations
class AnimationController;
class Animation;

/**
 * @brief Blend tree node types
 */
enum class BlendTreeType {
    Simple1D,       // 1D blend by single parameter
    Simple2D,       // 2D blend by two parameters
    Freeform2D,     // Freeform 2D blend with arbitrary positions
    Direct,         // Direct blend weights per child
    Additive        // Additive blend on base animation
};

/**
 * @brief Animation mask for partial body blending
 */
struct AnimationMask {
    std::string id;
    std::string name;
    std::vector<std::string> includedBones;
    std::vector<std::string> excludedBones;
    std::unordered_map<std::string, float> boneWeights;  // Per-bone weights

    [[nodiscard]] json ToJson() const;
    static AnimationMask FromJson(const json& j);

    /**
     * @brief Get weight for a specific bone
     */
    [[nodiscard]] float GetBoneWeight(const std::string& boneName) const;
};

/**
 * @brief IK blend weight configuration
 */
struct IKBlendConfig {
    std::string targetBone;
    float positionWeight = 1.0f;
    float rotationWeight = 1.0f;
    float hintWeight = 0.0f;
    std::string hintBone;

    [[nodiscard]] json ToJson() const;
    static IKBlendConfig FromJson(const json& j);
};

/**
 * @brief Child node in blend tree
 */
struct BlendTreeChild {
    std::string clipName;
    std::unique_ptr<class BlendTree> subTree;  // Nested blend tree

    // 1D blend position
    float threshold = 0.0f;

    // 2D blend position
    glm::vec2 position{0.0f};

    // Direct blend parameter
    std::string directBlendParameter;

    // Modifiers
    float timeScale = 1.0f;
    float cycleOffset = 0.0f;
    bool mirror = false;

    // Runtime state
    float currentWeight = 0.0f;
    float normalizedTime = 0.0f;

    [[nodiscard]] json ToJson() const;
    static BlendTreeChild FromJson(const json& j);
};

/**
 * @brief Blend tree for complex animation blending
 *
 * Supports:
 * - 1D blending (walk/run by speed)
 * - 2D blending (directional movement)
 * - Additive layers
 * - Blend masks (upper/lower body)
 * - IK blend weights
 * - Parameter-driven blending
 */
class BlendTree {
public:
    BlendTree() = default;
    explicit BlendTree(const std::string& id);

    BlendTree(const BlendTree&) = delete;
    BlendTree& operator=(const BlendTree&) = delete;
    BlendTree(BlendTree&&) noexcept = default;
    BlendTree& operator=(BlendTree&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Load blend tree from JSON
     */
    bool LoadFromJson(const json& config);

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Set blend tree type
     */
    void SetType(BlendTreeType type) { m_type = type; }
    [[nodiscard]] BlendTreeType GetType() const { return m_type; }

    /**
     * @brief Set parameter name for 1D blending
     */
    void SetParameter(const std::string& param) { m_parameter = param; }
    [[nodiscard]] const std::string& GetParameter() const { return m_parameter; }

    /**
     * @brief Set X parameter for 2D blending
     */
    void SetParameterX(const std::string& param) { m_parameterX = param; }
    [[nodiscard]] const std::string& GetParameterX() const { return m_parameterX; }

    /**
     * @brief Set Y parameter for 2D blending
     */
    void SetParameterY(const std::string& param) { m_parameterY = param; }
    [[nodiscard]] const std::string& GetParameterY() const { return m_parameterY; }

    /**
     * @brief Enable/disable blend value normalization
     */
    void SetNormalizeBlendValues(bool normalize) { m_normalizeBlendValues = normalize; }
    [[nodiscard]] bool GetNormalizeBlendValues() const { return m_normalizeBlendValues; }

    // -------------------------------------------------------------------------
    // Children
    // -------------------------------------------------------------------------

    /**
     * @brief Add a child animation clip
     */
    void AddChild(BlendTreeChild&& child);

    /**
     * @brief Add clip with 1D threshold
     */
    void AddClip1D(const std::string& clipName, float threshold);

    /**
     * @brief Add clip with 2D position
     */
    void AddClip2D(const std::string& clipName, const glm::vec2& position);

    /**
     * @brief Remove child by index
     */
    void RemoveChild(size_t index);

    /**
     * @brief Get all children
     */
    [[nodiscard]] const std::vector<BlendTreeChild>& GetChildren() const { return m_children; }
    [[nodiscard]] std::vector<BlendTreeChild>& GetChildren() { return m_children; }

    // -------------------------------------------------------------------------
    // Masks and Layers
    // -------------------------------------------------------------------------

    /**
     * @brief Set blend mask
     */
    void SetMask(const AnimationMask& mask) { m_mask = mask; }
    [[nodiscard]] const AnimationMask& GetMask() const { return m_mask; }

    /**
     * @brief Set additive reference pose
     */
    void SetAdditiveReferencePose(const std::string& clipName) {
        m_additiveReferencePose = clipName;
    }

    /**
     * @brief Configure IK blending
     */
    void AddIKConfig(const IKBlendConfig& config) { m_ikConfigs.push_back(config); }
    [[nodiscard]] const std::vector<IKBlendConfig>& GetIKConfigs() const { return m_ikConfigs; }

    // -------------------------------------------------------------------------
    // Runtime
    // -------------------------------------------------------------------------

    /**
     * @brief Update blend tree with current parameter values
     * @param parameters Map of parameter names to values
     * @param deltaTime Time since last update
     */
    void Update(const std::unordered_map<std::string, float>& parameters, float deltaTime);

    /**
     * @brief Calculate blend weights for all children
     * @param parameters Current parameter values
     */
    void CalculateWeights(const std::unordered_map<std::string, float>& parameters);

    /**
     * @brief Get current blend weights
     */
    [[nodiscard]] std::vector<std::pair<std::string, float>> GetBlendWeights() const;

    /**
     * @brief Get blended animation transforms
     */
    [[nodiscard]] std::unordered_map<std::string, glm::mat4> GetBlendedTransforms(
        float time,
        const std::unordered_map<std::string, Animation*>& animations) const;

    // -------------------------------------------------------------------------
    // Debug
    // -------------------------------------------------------------------------

    /**
     * @brief Get debug visualization data
     */
    [[nodiscard]] json GetDebugInfo() const;

    /**
     * @brief ID for referencing
     */
    [[nodiscard]] const std::string& GetId() const { return m_id; }

private:
    void Calculate1DWeights(float paramValue);
    void Calculate2DWeights(const glm::vec2& paramValue);
    void CalculateFreeform2DWeights(const glm::vec2& paramValue);
    void CalculateDirectWeights(const std::unordered_map<std::string, float>& parameters);

    [[nodiscard]] float CalculateGradientBandWeight(const glm::vec2& point,
                                                     const glm::vec2& samplePoint,
                                                     const std::vector<glm::vec2>& allPoints) const;

    std::string m_id;
    BlendTreeType m_type = BlendTreeType::Simple1D;
    std::string m_parameter;
    std::string m_parameterX;
    std::string m_parameterY;
    bool m_normalizeBlendValues = true;

    std::vector<BlendTreeChild> m_children;
    AnimationMask m_mask;
    std::string m_additiveReferencePose;
    std::vector<IKBlendConfig> m_ikConfigs;

    // Runtime
    float m_lastUpdateTime = 0.0f;
};

/**
 * @brief Manager for blend trees
 */
class BlendTreeManager {
public:
    BlendTreeManager() = default;

    /**
     * @brief Load blend tree from file
     */
    BlendTree* LoadFromFile(const std::string& filepath);

    /**
     * @brief Create blend tree from JSON
     */
    BlendTree* CreateFromJson(const std::string& id, const json& config);

    /**
     * @brief Get blend tree by ID
     */
    [[nodiscard]] BlendTree* Get(const std::string& id);
    [[nodiscard]] const BlendTree* Get(const std::string& id) const;

    /**
     * @brief Remove blend tree
     */
    bool Remove(const std::string& id);

    /**
     * @brief Get all blend tree IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllIds() const;

private:
    std::unordered_map<std::string, std::unique_ptr<BlendTree>> m_blendTrees;
};

/**
 * @brief Predefined blend tree templates
 */
namespace BlendTreeTemplates {
    /**
     * @brief Create locomotion blend tree (idle/walk/run)
     */
    [[nodiscard]] json CreateLocomotion1D(
        const std::string& idleClip,
        const std::string& walkClip,
        const std::string& runClip,
        const std::string& speedParameter = "speed");

    /**
     * @brief Create directional movement blend tree
     */
    [[nodiscard]] json CreateDirectional2D(
        const std::string& forwardClip,
        const std::string& backwardClip,
        const std::string& leftClip,
        const std::string& rightClip,
        const std::string& xParameter = "velocityX",
        const std::string& yParameter = "velocityZ");

    /**
     * @brief Create strafe movement blend tree (8 directions)
     */
    [[nodiscard]] json CreateStrafe8Way(
        const std::unordered_map<std::string, std::string>& directionClips,
        const std::string& xParameter = "velocityX",
        const std::string& yParameter = "velocityZ");

    /**
     * @brief Create additive lean blend tree
     */
    [[nodiscard]] json CreateAdditiveLean(
        const std::string& neutralClip,
        const std::string& leanLeftClip,
        const std::string& leanRightClip,
        const std::string& leanParameter = "leanAmount");
}

} // namespace Nova
