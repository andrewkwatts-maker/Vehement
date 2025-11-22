#pragma once

#include "BlendNode.hpp"
#include "BlendSpace1D.hpp"
#include "BlendSpace2D.hpp"
#include "BlendMask.hpp"
#include "AnimationLayer.hpp"
#include "BlendTreeRuntime.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stack>

namespace Nova {

class Animation;
class Skeleton;

/**
 * @brief Fluent API for building blend trees programmatically
 *
 * Usage example:
 * ```cpp
 * auto tree = BlendTreeBuilder()
 *     .SetSkeleton(skeleton)
 *     .BeginBlend1D("Speed")
 *         .AddClip(idleClip, 0.0f)
 *         .AddClip(walkClip, 1.0f)
 *         .AddClip(runClip, 2.0f)
 *     .EndBlend()
 *     .Build();
 * ```
 */
class BlendTreeBuilder {
public:
    BlendTreeBuilder() = default;

    // =========================================================================
    // Setup
    // =========================================================================

    /**
     * @brief Set skeleton for the tree
     */
    BlendTreeBuilder& SetSkeleton(Skeleton* skeleton);

    /**
     * @brief Set tree name
     */
    BlendTreeBuilder& SetName(const std::string& name);

    // =========================================================================
    // Clip Nodes
    // =========================================================================

    /**
     * @brief Add a simple clip node
     */
    BlendTreeBuilder& AddClip(const Animation* clip, float speed = 1.0f);

    /**
     * @brief Add a clip with a name
     */
    BlendTreeBuilder& AddClip(const std::string& name, const Animation* clip, float speed = 1.0f);

    // =========================================================================
    // 1D Blend
    // =========================================================================

    /**
     * @brief Begin a 1D blend node
     */
    BlendTreeBuilder& BeginBlend1D(const std::string& parameter);

    /**
     * @brief Add clip to current 1D blend at threshold
     */
    BlendTreeBuilder& At(float threshold);

    /**
     * @brief End 1D blend node
     */
    BlendTreeBuilder& EndBlend1D();

    // =========================================================================
    // 2D Blend
    // =========================================================================

    /**
     * @brief Begin a 2D blend node
     */
    BlendTreeBuilder& BeginBlend2D(const std::string& paramX, const std::string& paramY);

    /**
     * @brief Add clip to current 2D blend at position
     */
    BlendTreeBuilder& At(float x, float y);

    /**
     * @brief End 2D blend node
     */
    BlendTreeBuilder& EndBlend2D();

    // =========================================================================
    // Additive Blend
    // =========================================================================

    /**
     * @brief Begin an additive blend node
     */
    BlendTreeBuilder& BeginAdditive();

    /**
     * @brief Set base for additive
     */
    BlendTreeBuilder& Base();

    /**
     * @brief Set additive layer
     */
    BlendTreeBuilder& Additive(const std::string& weightParam = "");

    /**
     * @brief End additive node
     */
    BlendTreeBuilder& EndAdditive();

    // =========================================================================
    // Layered Blend
    // =========================================================================

    /**
     * @brief Begin a layered blend node
     */
    BlendTreeBuilder& BeginLayered();

    /**
     * @brief Add base layer
     */
    BlendTreeBuilder& BaseLayer();

    /**
     * @brief Add overlay layer
     */
    BlendTreeBuilder& Layer(float weight = 1.0f, bool additive = false);

    /**
     * @brief Set mask for current layer
     */
    BlendTreeBuilder& WithMask(std::shared_ptr<BlendMask> mask);

    /**
     * @brief Set mask preset for current layer
     */
    BlendTreeBuilder& WithMask(BlendMask::Preset preset);

    /**
     * @brief End layered node
     */
    BlendTreeBuilder& EndLayered();

    // =========================================================================
    // State Selector
    // =========================================================================

    /**
     * @brief Begin a state selector node
     */
    BlendTreeBuilder& BeginStates();

    /**
     * @brief Add a state
     */
    BlendTreeBuilder& State(const std::string& name);

    /**
     * @brief Set default state
     */
    BlendTreeBuilder& DefaultState(const std::string& name);

    /**
     * @brief End state selector
     */
    BlendTreeBuilder& EndStates();

    // =========================================================================
    // Options
    // =========================================================================

    /**
     * @brief Set weight for last added node
     */
    BlendTreeBuilder& Weight(float weight);

    /**
     * @brief Set playback speed
     */
    BlendTreeBuilder& Speed(float speed);

    /**
     * @brief Enable looping
     */
    BlendTreeBuilder& Loop(bool looping = true);

    /**
     * @brief Enable root motion
     */
    BlendTreeBuilder& RootMotion(bool enabled = true);

    /**
     * @brief Enable time sync
     */
    BlendTreeBuilder& Sync(bool enabled = true);

    // =========================================================================
    // Build
    // =========================================================================

    /**
     * @brief Build the blend tree
     */
    [[nodiscard]] std::unique_ptr<BlendNode> Build();

    /**
     * @brief Build a blend tree runtime
     */
    [[nodiscard]] std::unique_ptr<BlendTreeRuntime> BuildRuntime();

    /**
     * @brief Validate the tree configuration
     * @return List of validation errors
     */
    [[nodiscard]] std::vector<std::string> Validate() const;

    /**
     * @brief Check if build would succeed
     */
    [[nodiscard]] bool IsValid() const;

private:
    enum class BuilderState {
        Root,
        Blend1D,
        Blend2D,
        Additive,
        AdditiveBase,
        AdditiveLayer,
        Layered,
        LayeredBase,
        LayeredOverlay,
        States,
        StateEntry
    };

    struct BuilderFrame {
        BuilderState state = BuilderState::Root;
        std::unique_ptr<BlendNode> node;
        float threshold = 0.0f;
        glm::vec2 position{0.0f};
        std::string stateName;
    };

    BuilderFrame& CurrentFrame();
    void PushFrame(BuilderState state);
    std::unique_ptr<BlendNode> PopFrame();

    Skeleton* m_skeleton = nullptr;
    std::string m_name;
    std::stack<BuilderFrame> m_frameStack;
    std::unique_ptr<BlendNode> m_root;

    // Pending clip/node for adding to containers
    std::unique_ptr<BlendNode> m_pendingNode;
    float m_pendingThreshold = 0.0f;
    glm::vec2 m_pendingPosition{0.0f};

    // Last added node for setting options
    BlendNode* m_lastNode = nullptr;
};

/**
 * @brief Layer stack builder
 */
class LayerStackBuilder {
public:
    LayerStackBuilder() = default;

    /**
     * @brief Set skeleton
     */
    LayerStackBuilder& SetSkeleton(Skeleton* skeleton);

    /**
     * @brief Add base layer
     */
    LayerStackBuilder& AddBaseLayer(const std::string& name, std::unique_ptr<BlendNode> tree);

    /**
     * @brief Add overlay layer
     */
    LayerStackBuilder& AddLayer(const std::string& name, std::unique_ptr<BlendNode> tree,
                                 AnimationLayer::BlendMode mode = AnimationLayer::BlendMode::Override,
                                 float weight = 1.0f);

    /**
     * @brief Set mask for last added layer
     */
    LayerStackBuilder& WithMask(std::shared_ptr<BlendMask> mask);

    /**
     * @brief Set mask preset for last added layer
     */
    LayerStackBuilder& WithMask(BlendMask::Preset preset);

    /**
     * @brief Create sync group
     */
    LayerStackBuilder& SyncGroup(const std::string& name, const std::vector<std::string>& layerNames);

    /**
     * @brief Build the layer stack
     */
    [[nodiscard]] std::unique_ptr<AnimationLayerStack> Build();

private:
    Skeleton* m_skeleton = nullptr;
    std::unique_ptr<AnimationLayerStack> m_stack = std::make_unique<AnimationLayerStack>();
    AnimationLayer* m_lastLayer = nullptr;
};

/**
 * @brief Blend tree optimization passes
 */
class BlendTreeOptimizer {
public:
    /**
     * @brief Optimization options
     */
    struct Options {
        bool removeUnusedNodes = true;
        bool foldConstantNodes = true;
        bool mergeIdenticalSubtrees = true;
        bool simplifyBlendNodes = true;
    };

    /**
     * @brief Optimize a blend tree
     */
    static void Optimize(BlendNode* tree, const Options& options = {});

    /**
     * @brief Remove nodes with zero weight
     */
    static void RemoveZeroWeightNodes(BlendNode* tree);

    /**
     * @brief Fold constant blends (single child, weight = 1)
     */
    static void FoldConstants(BlendNode* tree);

    /**
     * @brief Validate tree structure
     */
    static std::vector<std::string> Validate(const BlendNode* tree);
};

} // namespace Nova
