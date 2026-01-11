#pragma once

#include "BlendNode.hpp"
#include "AnimationLayer.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace Nova {

class Skeleton;
class Animation;

/**
 * @brief Runtime for efficient blend tree evaluation
 *
 * Provides optimized tree traversal, pose caching, parameter
 * smoothing, and debug visualization capabilities.
 */
class BlendTreeRuntime {
public:
    /**
     * @brief Parameter with smoothing
     */
    struct Parameter {
        std::string name;
        float value = 0.0f;
        float targetValue = 0.0f;
        float smoothSpeed = 10.0f;   // Units per second
        float minValue = -FLT_MAX;
        float maxValue = FLT_MAX;
        bool smooth = false;
    };

    /**
     * @brief Debug info for a node
     */
    struct NodeDebugInfo {
        std::string nodeName;
        std::string nodeType;
        float weight = 0.0f;
        float normalizedTime = 0.0f;
        bool active = false;
        std::vector<std::pair<std::string, float>> parameters;
    };

    /**
     * @brief Cached pose data
     */
    struct PoseCache {
        AnimationPose pose;
        float timestamp = 0.0f;
        bool valid = false;
    };

    BlendTreeRuntime() = default;

    // =========================================================================
    // Setup
    // =========================================================================

    /**
     * @brief Set the root blend tree
     */
    void SetRootTree(std::unique_ptr<BlendNode> tree);

    /**
     * @brief Set animation layer stack
     */
    void SetLayerStack(std::unique_ptr<AnimationLayerStack> stack);

    /**
     * @brief Set skeleton
     */
    void SetSkeleton(Skeleton* skeleton);

    /**
     * @brief Get skeleton
     */
    [[nodiscard]] Skeleton* GetSkeleton() const { return m_skeleton; }

    /**
     * @brief Get root tree
     */
    [[nodiscard]] BlendNode* GetRootTree() const { return m_rootTree.get(); }

    /**
     * @brief Get layer stack
     */
    [[nodiscard]] AnimationLayerStack* GetLayerStack() const { return m_layerStack.get(); }

    // =========================================================================
    // Parameters
    // =========================================================================

    /**
     * @brief Register a parameter
     */
    void RegisterParameter(const std::string& name, float defaultValue = 0.0f,
                           float min = -FLT_MAX, float max = FLT_MAX);

    /**
     * @brief Set parameter value
     */
    void SetParameter(const std::string& name, float value);

    /**
     * @brief Set parameter with smoothing
     */
    void SetParameterSmooth(const std::string& name, float targetValue, float smoothSpeed = 10.0f);

    /**
     * @brief Get parameter value
     */
    [[nodiscard]] float GetParameter(const std::string& name) const;

    /**
     * @brief Get all parameters
     */
    [[nodiscard]] const std::unordered_map<std::string, Parameter>& GetParameters() const {
        return m_parameters;
    }

    /**
     * @brief Clear all parameters
     */
    void ClearParameters();

    // =========================================================================
    // Triggers
    // =========================================================================

    /**
     * @brief Set a trigger (auto-resets after one evaluation)
     */
    void SetTrigger(const std::string& name);

    /**
     * @brief Reset a trigger
     */
    void ResetTrigger(const std::string& name);

    /**
     * @brief Check if trigger is set
     */
    [[nodiscard]] bool GetTrigger(const std::string& name) const;

    // =========================================================================
    // Evaluation
    // =========================================================================

    /**
     * @brief Update parameters and tree state
     */
    void Update(float deltaTime);

    /**
     * @brief Evaluate and get final pose
     */
    [[nodiscard]] AnimationPose Evaluate(float deltaTime);

    /**
     * @brief Get bone matrices for GPU upload
     */
    [[nodiscard]] std::vector<glm::mat4> GetBoneMatrices() const;

    /**
     * @brief Get bone matrices into pre-allocated buffer
     */
    void GetBoneMatricesInto(std::span<glm::mat4> outMatrices) const;

    /**
     * @brief Reset runtime state
     */
    void Reset();

    // =========================================================================
    // Pose Caching
    // =========================================================================

    /**
     * @brief Enable pose caching
     */
    void SetCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }
    [[nodiscard]] bool IsCachingEnabled() const { return m_cachingEnabled; }

    /**
     * @brief Set cache invalidation time
     */
    void SetCacheLifetime(float seconds) { m_cacheLifetime = seconds; }

    /**
     * @brief Invalidate all caches
     */
    void InvalidateCaches();

    // =========================================================================
    // Debug Visualization
    // =========================================================================

    /**
     * @brief Enable debug mode
     */
    void SetDebugEnabled(bool enabled) { m_debugEnabled = enabled; }
    [[nodiscard]] bool IsDebugEnabled() const { return m_debugEnabled; }

    /**
     * @brief Get debug info for all nodes
     */
    [[nodiscard]] std::vector<NodeDebugInfo> GetDebugInfo() const;

    /**
     * @brief Get performance stats
     */
    struct PerformanceStats {
        float lastEvaluationTimeMs = 0.0f;
        size_t nodesEvaluated = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
    };
    [[nodiscard]] const PerformanceStats& GetPerformanceStats() const { return m_stats; }

    // =========================================================================
    // Animation Events
    // =========================================================================

    /**
     * @brief Register animation event callback
     */
    void OnAnimationEvent(std::function<void(const std::string&, float)> callback);

    /**
     * @brief Register loop callback
     */
    void OnLoop(std::function<void(const std::string&)> callback);

    // =========================================================================
    // Root Motion
    // =========================================================================

    /**
     * @brief Get accumulated root motion delta
     */
    [[nodiscard]] glm::vec3 GetRootMotionDelta() const { return m_rootMotionDelta; }

    /**
     * @brief Get accumulated root rotation delta
     */
    [[nodiscard]] glm::quat GetRootRotationDelta() const { return m_rootRotationDelta; }

    /**
     * @brief Clear root motion accumulators
     */
    void ClearRootMotion();

    /**
     * @brief Enable root motion extraction
     */
    void SetRootMotionEnabled(bool enabled) { m_rootMotionEnabled = enabled; }

private:
    void UpdateParameters(float deltaTime);
    void PropagateParameters();
    void CollectDebugInfo(BlendNode* node, std::vector<NodeDebugInfo>& infos) const;

    std::unique_ptr<BlendNode> m_rootTree;
    std::unique_ptr<AnimationLayerStack> m_layerStack;
    Skeleton* m_skeleton = nullptr;

    // Parameters
    std::unordered_map<std::string, Parameter> m_parameters;
    std::unordered_set<std::string> m_triggers;

    // Caching
    bool m_cachingEnabled = true;
    float m_cacheLifetime = 0.016f;  // ~1 frame at 60fps
    std::unordered_map<BlendNode*, PoseCache> m_poseCache;
    float m_currentTime = 0.0f;

    // Current pose
    AnimationPose m_currentPose;

    // Root motion
    bool m_rootMotionEnabled = true;
    glm::vec3 m_rootMotionDelta{0.0f};
    glm::quat m_rootRotationDelta{1.0f, 0.0f, 0.0f, 0.0f};

    // Debug
    bool m_debugEnabled = false;
    PerformanceStats m_stats;

    // Callbacks
    std::function<void(const std::string&, float)> m_eventCallback;
    std::function<void(const std::string&)> m_loopCallback;
};

/**
 * @brief Pool for reusing animation pose objects
 */
class AnimationPosePool {
public:
    static AnimationPosePool& Instance();

    /**
     * @brief Acquire a pose from the pool
     */
    AnimationPose* Acquire(size_t boneCount);

    /**
     * @brief Release a pose back to the pool
     */
    void Release(AnimationPose* pose);

    /**
     * @brief Clear the pool
     */
    void Clear();

    /**
     * @brief Get pool statistics
     */
    struct Stats {
        size_t totalAllocated = 0;
        size_t inUse = 0;
        size_t available = 0;
    };
    [[nodiscard]] Stats GetStats() const;

private:
    AnimationPosePool() = default;
    std::vector<std::unique_ptr<AnimationPose>> m_pool;
    std::vector<AnimationPose*> m_available;
};

} // namespace Nova
