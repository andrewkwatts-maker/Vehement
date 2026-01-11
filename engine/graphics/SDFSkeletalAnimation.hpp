#pragma once

/**
 * @file SDFSkeletalAnimation.hpp
 * @brief Skeletal animation system for SDF primitives
 *
 * Attaches SDF primitives to skeleton bones and animates them along with
 * traditional skinned meshes. Supports bone-weighted blending for smooth
 * deformation and efficient GPU upload.
 *
 * Features:
 * - Bind SDF primitives to bones
 * - Per-primitive bone influence (up to 4 bones)
 * - Dual quaternion skinning support
 * - Automatic bone weight computation
 * - Efficient GPU buffer management
 * - LOD-aware animation (skip animation for distant objects)
 * - Animation blending and layering
 *
 * Example usage:
 * @code
 * SDFSkeletalAnimationSystem animSystem;
 *
 * // Bind model to skeleton
 * animSystem.BindModelToSkeleton(modelId, model, skeleton);
 *
 * // Update animation each frame
 * animSystem.UpdateAnimation(modelId, skeleton, deltaTime);
 *
 * // Get animated primitive transforms for rendering
 * auto transforms = animSystem.GetAnimatedPrimitiveTransforms(modelId);
 * @endcode
 */

#include "../sdf/SDFPrimitive.hpp"
#include "../animation/Skeleton.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

namespace Nova {

// Forward declarations
class SDFModel;

/**
 * @brief Bone influence for a single primitive
 */
struct PrimitiveBoneInfluence {
    std::array<int, 4> boneIndices = {-1, -1, -1, -1};   // -1 = no influence
    std::array<float, 4> boneWeights = {0.0f, 0.0f, 0.0f, 0.0f};

    /**
     * @brief Check if primitive is influenced by any bones
     */
    bool HasInfluence() const {
        return boneIndices[0] >= 0 && boneWeights[0] > 0.0f;
    }

    /**
     * @brief Normalize weights to sum to 1.0
     */
    void NormalizeWeights();

    /**
     * @brief Add bone influence (sorted by weight, keeps top 4)
     */
    void AddInfluence(int boneIndex, float weight);
};

/**
 * @brief Binding between SDF model and skeleton
 */
struct SDFSkeletonBinding {
    uint32_t modelId = 0;
    const Skeleton* skeleton = nullptr;

    // Per-primitive bone influences
    std::vector<PrimitiveBoneInfluence> primitiveInfluences;

    // Primitive IDs corresponding to influences
    std::vector<uint32_t> primitiveIds;

    // Bind pose transforms (local space)
    std::vector<SDFTransform> bindPoseTransforms;

    // Current animated transforms (world space)
    std::vector<glm::mat4> animatedTransforms;

    // Timestamp
    float lastUpdateTime = 0.0f;
    bool dirty = true;
};

/**
 * @brief Animation quality settings
 */
struct SDFAnimationQuality {
    bool enableDualQuaternionSkinning = false; // More accurate deformation
    bool enableLODOptimization = true;         // Skip animation for distant objects
    float maxAnimationDistance = 100.0f;       // Don't animate beyond this distance
    bool interpolateBoneTransforms = true;     // Smooth bone transitions
    int maxInfluencesPerPrimitive = 4;         // 1-4 bones per primitive
};

/**
 * @brief Animation statistics
 */
struct SDFAnimationStatistics {
    int totalBindings = 0;
    int animatedThisFrame = 0;
    int skippedByDistance = 0;
    int totalPrimitivesAnimated = 0;
    float updateTimeMs = 0.0f;
    float avgBonesPerPrimitive = 0.0f;

    void Reset();
    std::string ToString() const;
};

/**
 * @brief GPU buffer data for animated SDF primitives
 */
struct AnimatedPrimitiveGPUData {
    glm::mat4 transform;              // 64 bytes - World transform
    glm::mat4 inverseTransform;       // 64 bytes - For ray tracing
    glm::vec4 parameters;             // 16 bytes - Primitive parameters
    glm::vec4 material;               // 16 bytes - Material properties
    int primitiveType;                // 4 bytes
    int csgOperation;                 // 4 bytes
    int padding[2];                   // 8 bytes - Align to 16 bytes

    static constexpr size_t Size = 176; // Total size in bytes
};

/**
 * @brief SDF Skeletal Animation System
 *
 * Manages skeletal animation for SDF primitive-based models.
 * Handles bone binding, weight computation, and animated transform generation.
 */
class SDFSkeletalAnimationSystem {
public:
    SDFSkeletalAnimationSystem();
    ~SDFSkeletalAnimationSystem();

    // Non-copyable
    SDFSkeletalAnimationSystem(const SDFSkeletalAnimationSystem&) = delete;
    SDFSkeletalAnimationSystem& operator=(const SDFSkeletalAnimationSystem&) = delete;

    // =========================================================================
    // Binding
    // =========================================================================

    /**
     * @brief Bind SDF model to skeleton
     * @param modelId Unique model ID
     * @param model SDF model to bind
     * @param skeleton Skeleton to bind to
     * @param autoComputeWeights Automatically compute bone weights based on proximity
     */
    void BindModelToSkeleton(uint32_t modelId, const SDFModel& model,
                            const Skeleton& skeleton, bool autoComputeWeights = true);

    /**
     * @brief Unbind model from skeleton
     */
    void UnbindModel(uint32_t modelId);

    /**
     * @brief Check if model is bound
     */
    bool IsModelBound(uint32_t modelId) const;

    /**
     * @brief Get binding for model
     */
    const SDFSkeletonBinding* GetBinding(uint32_t modelId) const;

    // =========================================================================
    // Bone Weight Computation
    // =========================================================================

    /**
     * @brief Compute bone weights for all primitives
     */
    void ComputeBoneWeights(uint32_t modelId, const SDFModel& model, const Skeleton& skeleton);

    /**
     * @brief Compute bone weights for single primitive
     * @return Bone influences for primitive
     */
    PrimitiveBoneInfluence ComputePrimitiveBoneWeights(
        const SDFPrimitive& primitive,
        const Skeleton& skeleton,
        const glm::mat4& primitiveWorldTransform) const;

    /**
     * @brief Manually set bone weights for primitive
     */
    void SetPrimitiveBoneWeights(uint32_t modelId, uint32_t primitiveId,
                                const PrimitiveBoneInfluence& influence);

    // =========================================================================
    // Animation Update
    // =========================================================================

    /**
     * @brief Update animation for model
     * @param modelId Model to animate
     * @param skeleton Current skeleton state (with animation applied)
     * @param deltaTime Time since last update
     * @param modelWorldTransform World transform of the model
     */
    void UpdateAnimation(uint32_t modelId, const Skeleton& skeleton,
                        float deltaTime, const glm::mat4& modelWorldTransform = glm::mat4(1.0f));

    /**
     * @brief Update all bound models
     */
    void UpdateAllAnimations(float deltaTime);

    /**
     * @brief Set distance to camera for LOD optimization
     */
    void SetModelDistance(uint32_t modelId, float distance);

    // =========================================================================
    // Transform Queries
    // =========================================================================

    /**
     * @brief Get animated world transforms for all primitives
     * @return Vector of world-space transformation matrices
     */
    const std::vector<glm::mat4>& GetAnimatedPrimitiveTransforms(uint32_t modelId) const;

    /**
     * @brief Get GPU data for all animated primitives (ready for upload)
     */
    std::vector<AnimatedPrimitiveGPUData> GetGPUData(uint32_t modelId, const SDFModel& model) const;

    /**
     * @brief Get single primitive animated transform
     */
    glm::mat4 GetPrimitiveTransform(uint32_t modelId, uint32_t primitiveId) const;

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Get animation quality settings
     */
    SDFAnimationQuality& GetQualitySettings() { return m_quality; }
    const SDFAnimationQuality& GetQualitySettings() const { return m_quality; }

    /**
     * @brief Set animation quality settings
     */
    void SetQualitySettings(const SDFAnimationQuality& quality);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get animation statistics
     */
    const SDFAnimationStatistics& GetStatistics() const { return m_statistics; }

    /**
     * @brief Reset statistics
     */
    void ResetStatistics() { m_statistics.Reset(); }

    // =========================================================================
    // GPU Buffer Management
    // =========================================================================

    /**
     * @brief Create or update GPU buffer for model
     * @return OpenGL SSBO handle
     */
    unsigned int GetOrCreateGPUBuffer(uint32_t modelId);

    /**
     * @brief Upload animated transforms to GPU
     */
    void UploadToGPU(uint32_t modelId, const SDFModel& model);

    /**
     * @brief Cleanup GPU buffers
     */
    void CleanupGPUBuffers();

private:
    // Calculate skinned transform using linear blend skinning
    glm::mat4 CalculateSkinnedTransform(
        const PrimitiveBoneInfluence& influence,
        const std::vector<glm::mat4>& boneMatrices,
        const SDFTransform& bindPoseTransform) const;

    // Calculate skinned transform using dual quaternion skinning
    glm::mat4 CalculateDualQuaternionTransform(
        const PrimitiveBoneInfluence& influence,
        const std::vector<glm::mat4>& boneMatrices,
        const SDFTransform& bindPoseTransform) const;

    // Extract translation/rotation/scale from matrix
    void DecomposeMatrix(const glm::mat4& matrix,
                        glm::vec3& outTranslation,
                        glm::quat& outRotation,
                        glm::vec3& outScale) const;

    // Model bindings
    std::unordered_map<uint32_t, SDFSkeletonBinding> m_bindings;

    // Distance tracking for LOD
    std::unordered_map<uint32_t, float> m_modelDistances;

    // GPU buffers (SSBO handles)
    std::unordered_map<uint32_t, unsigned int> m_gpuBuffers;

    // Settings
    SDFAnimationQuality m_quality;

    // Statistics
    SDFAnimationStatistics m_statistics;

    // Empty transform vector for invalid queries
    static const std::vector<glm::mat4> s_emptyTransforms;
};

/**
 * @brief Helper utilities for bone weight computation
 */
namespace BoneWeightUtils {
    /**
     * @brief Calculate distance from point to bone
     */
    float DistanceToBone(const glm::vec3& point, const Bone& bone,
                        const glm::mat4& boneWorldTransform);

    /**
     * @brief Find K nearest bones to point
     */
    std::vector<std::pair<int, float>> FindNearestBones(
        const glm::vec3& point,
        const Skeleton& skeleton,
        const std::vector<glm::mat4>& boneWorldTransforms,
        int k = 4);

    /**
     * @brief Convert distances to normalized weights
     */
    std::vector<float> DistancesToWeights(const std::vector<float>& distances);
}

/**
 * @brief Dual quaternion for better skinning quality
 */
struct DualQuaternion {
    glm::quat real;
    glm::quat dual;

    DualQuaternion() : real(1, 0, 0, 0), dual(0, 0, 0, 0) {}
    DualQuaternion(const glm::quat& r, const glm::quat& d) : real(r), dual(d) {}

    static DualQuaternion FromMatrix(const glm::mat4& matrix);
    glm::mat4 ToMatrix() const;

    DualQuaternion operator*(float scalar) const;
    DualQuaternion operator+(const DualQuaternion& other) const;

    void Normalize();
};

} // namespace Nova
