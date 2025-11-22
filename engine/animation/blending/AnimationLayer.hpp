#pragma once

#include "BlendNode.hpp"
#include "BlendMask.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Nova {

/**
 * @brief Animation layer for blending multiple animation sources
 *
 * Layers allow combining different animations that affect different
 * parts of the body or add on top of base animations.
 */
class AnimationLayer {
public:
    /**
     * @brief Blend mode for combining layers
     */
    enum class BlendMode {
        Override,       // Replace previous layers (masked)
        Additive,       // Add to previous layers
        Multiply        // Multiply with previous layers
    };

    /**
     * @brief Sync mode for layer timing
     */
    enum class SyncMode {
        Independent,    // Layer runs independently
        SyncToBase,     // Sync normalized time to base layer
        SyncToLayer     // Sync to specific layer
    };

    AnimationLayer() = default;
    explicit AnimationLayer(const std::string& name);

    // =========================================================================
    // Configuration
    // =========================================================================

    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    void SetBlendMode(BlendMode mode) { m_blendMode = mode; }
    [[nodiscard]] BlendMode GetBlendMode() const { return m_blendMode; }

    void SetSyncMode(SyncMode mode) { m_syncMode = mode; }
    [[nodiscard]] SyncMode GetSyncMode() const { return m_syncMode; }

    void SetSyncLayerIndex(int index) { m_syncLayerIndex = index; }
    [[nodiscard]] int GetSyncLayerIndex() const { return m_syncLayerIndex; }

    // =========================================================================
    // Weight
    // =========================================================================

    /**
     * @brief Set layer weight
     */
    void SetWeight(float weight) { m_weight = std::clamp(weight, 0.0f, 1.0f); }
    [[nodiscard]] float GetWeight() const { return m_weight; }

    /**
     * @brief Set target weight for smooth transitions
     */
    void SetTargetWeight(float weight) { m_targetWeight = std::clamp(weight, 0.0f, 1.0f); }
    [[nodiscard]] float GetTargetWeight() const { return m_targetWeight; }

    /**
     * @brief Set weight blend speed
     */
    void SetWeightBlendSpeed(float speed) { m_weightBlendSpeed = speed; }

    /**
     * @brief Fade in layer
     */
    void FadeIn(float duration);

    /**
     * @brief Fade out layer
     */
    void FadeOut(float duration);

    // =========================================================================
    // Mask
    // =========================================================================

    /**
     * @brief Set blend mask
     */
    void SetMask(std::shared_ptr<BlendMask> mask) { m_mask = mask; }
    [[nodiscard]] std::shared_ptr<BlendMask> GetMask() const { return m_mask; }

    /**
     * @brief Check if layer is masked
     */
    [[nodiscard]] bool HasMask() const { return m_mask != nullptr; }

    // =========================================================================
    // Blend Tree
    // =========================================================================

    /**
     * @brief Set the blend tree for this layer
     */
    void SetBlendTree(std::unique_ptr<BlendNode> tree) { m_blendTree = std::move(tree); }
    [[nodiscard]] BlendNode* GetBlendTree() const { return m_blendTree.get(); }

    /**
     * @brief Set parameter value
     */
    void SetParameter(const std::string& name, float value);

    /**
     * @brief Get parameter value
     */
    [[nodiscard]] float GetParameter(const std::string& name) const;

    // =========================================================================
    // State
    // =========================================================================

    /**
     * @brief Enable/disable layer
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Check if layer is active (enabled and weight > 0)
     */
    [[nodiscard]] bool IsActive() const { return m_enabled && m_weight > 0.001f; }

    /**
     * @brief Get normalized time
     */
    [[nodiscard]] float GetNormalizedTime() const { return m_normalizedTime; }
    void SetNormalizedTime(float time) { m_normalizedTime = time; }

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Update layer state
     */
    void Update(float deltaTime);

    /**
     * @brief Evaluate and return pose
     */
    [[nodiscard]] AnimationPose Evaluate(float deltaTime);

    /**
     * @brief Apply this layer's pose to a base pose
     */
    void ApplyToPose(AnimationPose& basePose, const AnimationPose& layerPose) const;

    /**
     * @brief Reset layer state
     */
    void Reset();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnWeightReachedTarget;

private:
    std::string m_name = "Layer";
    BlendMode m_blendMode = BlendMode::Override;
    SyncMode m_syncMode = SyncMode::Independent;
    int m_syncLayerIndex = 0;

    float m_weight = 1.0f;
    float m_targetWeight = 1.0f;
    float m_weightBlendSpeed = 5.0f;

    std::shared_ptr<BlendMask> m_mask;
    std::unique_ptr<BlendNode> m_blendTree;

    bool m_enabled = true;
    float m_normalizedTime = 0.0f;

    std::unordered_map<std::string, float> m_parameters;
};

/**
 * @brief Animation layer stack manager
 *
 * Manages multiple animation layers and combines their output
 * into a final pose.
 */
class AnimationLayerStack {
public:
    /**
     * @brief Sync group for coordinated playback
     */
    struct SyncGroup {
        std::string name;
        std::vector<size_t> layerIndices;
        float masterNormalizedTime = 0.0f;
    };

    AnimationLayerStack() = default;

    // =========================================================================
    // Layer Management
    // =========================================================================

    /**
     * @brief Add a layer
     * @return Layer index
     */
    size_t AddLayer(std::unique_ptr<AnimationLayer> layer);

    /**
     * @brief Add a layer with configuration
     */
    size_t AddLayer(const std::string& name, std::unique_ptr<BlendNode> tree,
                    AnimationLayer::BlendMode mode = AnimationLayer::BlendMode::Override,
                    float weight = 1.0f);

    /**
     * @brief Remove layer at index
     */
    void RemoveLayer(size_t index);

    /**
     * @brief Get layer count
     */
    [[nodiscard]] size_t GetLayerCount() const { return m_layers.size(); }

    /**
     * @brief Get layer by index
     */
    [[nodiscard]] AnimationLayer* GetLayer(size_t index);
    [[nodiscard]] const AnimationLayer* GetLayer(size_t index) const;

    /**
     * @brief Get layer by name
     */
    [[nodiscard]] AnimationLayer* GetLayer(const std::string& name);

    /**
     * @brief Get layer index by name
     */
    [[nodiscard]] int GetLayerIndex(const std::string& name) const;

    /**
     * @brief Reorder layers
     */
    void MoveLayer(size_t fromIndex, size_t toIndex);

    /**
     * @brief Clear all layers
     */
    void ClearLayers();

    // =========================================================================
    // Base Layer
    // =========================================================================

    /**
     * @brief Set base layer (index 0)
     */
    void SetBaseLayer(std::unique_ptr<AnimationLayer> layer);

    /**
     * @brief Get base layer
     */
    [[nodiscard]] AnimationLayer* GetBaseLayer();

    // =========================================================================
    // Sync Groups
    // =========================================================================

    /**
     * @brief Create a sync group
     */
    void CreateSyncGroup(const std::string& name, const std::vector<size_t>& layerIndices);

    /**
     * @brief Add layer to sync group
     */
    void AddToSyncGroup(const std::string& groupName, size_t layerIndex);

    /**
     * @brief Remove layer from sync group
     */
    void RemoveFromSyncGroup(const std::string& groupName, size_t layerIndex);

    /**
     * @brief Get sync group
     */
    [[nodiscard]] SyncGroup* GetSyncGroup(const std::string& name);

    // =========================================================================
    // Global Parameters
    // =========================================================================

    /**
     * @brief Set parameter for all layers
     */
    void SetParameter(const std::string& name, float value);

    /**
     * @brief Set parameter for specific layer
     */
    void SetLayerParameter(size_t layerIndex, const std::string& name, float value);

    /**
     * @brief Get parameter value
     */
    [[nodiscard]] float GetParameter(const std::string& name) const;

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Set skeleton for all layers
     */
    void SetSkeleton(Skeleton* skeleton);

    /**
     * @brief Update all layers
     */
    void Update(float deltaTime);

    /**
     * @brief Evaluate all layers and combine into final pose
     */
    [[nodiscard]] AnimationPose Evaluate(float deltaTime);

    /**
     * @brief Reset all layers
     */
    void Reset();

    // =========================================================================
    // Solo/Mute
    // =========================================================================

    /**
     * @brief Solo a layer (only this layer plays)
     */
    void SoloLayer(size_t index);

    /**
     * @brief Mute a layer
     */
    void MuteLayer(size_t index, bool muted);

    /**
     * @brief Clear solo mode
     */
    void ClearSolo();

    /**
     * @brief Check if in solo mode
     */
    [[nodiscard]] bool IsInSoloMode() const { return m_soloLayerIndex >= 0; }

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] std::string ToJson() const;
    bool FromJson(const std::string& json);

private:
    void SyncLayers();
    void UpdateSyncGroups(float deltaTime);

    std::vector<std::unique_ptr<AnimationLayer>> m_layers;
    std::vector<SyncGroup> m_syncGroups;
    std::unordered_map<std::string, float> m_globalParameters;

    Skeleton* m_skeleton = nullptr;
    int m_soloLayerIndex = -1;
    std::vector<bool> m_mutedLayers;
};

} // namespace Nova
