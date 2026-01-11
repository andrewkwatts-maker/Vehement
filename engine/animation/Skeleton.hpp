#pragma once

#include <vector>
#include <string>
#include <span>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Bone in a skeleton hierarchy
 */
struct alignas(16) Bone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 offsetMatrix{1.0f};    // Inverse bind pose matrix
    glm::mat4 localTransform{1.0f};  // Default local transform
};

/**
 * @brief Skeleton for skeletal animation
 *
 * Manages bone hierarchy and calculates final bone matrices for GPU skinning.
 * Optimized for cache-friendly bone matrix calculations.
 */
class Skeleton {
public:
    static constexpr size_t MAX_BONES = 256;

    Skeleton() = default;

    Skeleton(const Skeleton&) = default;
    Skeleton(Skeleton&&) noexcept = default;
    Skeleton& operator=(const Skeleton&) = default;
    Skeleton& operator=(Skeleton&&) noexcept = default;

    /**
     * @brief Add a bone to the skeleton
     * @note Bones must be added in parent-first order
     */
    void AddBone(const Bone& bone);
    void AddBone(Bone&& bone);

    /**
     * @brief Get bone by name
     */
    [[nodiscard]] Bone* GetBone(const std::string& name);
    [[nodiscard]] const Bone* GetBone(const std::string& name) const;

    /**
     * @brief Get bone index by name
     * @return Bone index or -1 if not found
     */
    [[nodiscard]] int GetBoneIndex(const std::string& name) const;

    /**
     * @brief Get bone by index
     */
    [[nodiscard]] Bone* GetBoneByIndex(size_t index);
    [[nodiscard]] const Bone* GetBoneByIndex(size_t index) const;

    /**
     * @brief Get all bones
     */
    [[nodiscard]] const std::vector<Bone>& GetBones() const noexcept { return m_bones; }
    [[nodiscard]] std::span<const Bone> GetBonesSpan() const noexcept {
        return std::span<const Bone>(m_bones);
    }

    /**
     * @brief Get number of bones
     */
    [[nodiscard]] size_t GetBoneCount() const noexcept { return m_bones.size(); }

    /**
     * @brief Calculate final bone matrices for shader
     * @param animationTransforms Map of bone names to animation transforms
     * @return Vector of final bone matrices ready for GPU upload
     */
    [[nodiscard]] std::vector<glm::mat4> CalculateBoneMatrices(
        const std::unordered_map<std::string, glm::mat4>& animationTransforms) const;

    /**
     * @brief Calculate bone matrices into pre-allocated buffer (avoids allocation)
     * @param animationTransforms Map of bone names to animation transforms
     * @param outMatrices Output span for bone matrices (must be at least GetBoneCount() size)
     */
    void CalculateBoneMatricesInto(
        const std::unordered_map<std::string, glm::mat4>& animationTransforms,
        std::span<glm::mat4> outMatrices) const;

    /**
     * @brief Get identity matrices for bind pose
     */
    [[nodiscard]] std::vector<glm::mat4> GetBindPoseMatrices() const;

    /**
     * @brief Global inverse transform (typically inverse of root node transform)
     */
    [[nodiscard]] const glm::mat4& GetGlobalInverseTransform() const noexcept { return m_globalInverse; }
    void SetGlobalInverseTransform(const glm::mat4& transform) noexcept {
        m_globalInverse = transform;
        m_dirty = true;
    }

    /**
     * @brief Check if skeleton structure has been modified
     */
    [[nodiscard]] bool IsDirty() const noexcept { return m_dirty; }
    void ClearDirty() noexcept { m_dirty = false; }

    /**
     * @brief Reserve capacity for bones
     */
    void Reserve(size_t capacity);

    /**
     * @brief Clear all bones
     */
    void Clear();

private:
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, int> m_boneMap;
    glm::mat4 m_globalInverse{1.0f};
    bool m_dirty = false;

    // Cached workspace for matrix calculations (avoids per-frame allocations)
    mutable std::vector<glm::mat4> m_globalTransformCache;
};

/**
 * @brief Helper to build skeleton from bone data
 */
class SkeletonBuilder {
public:
    SkeletonBuilder() = default;

    /**
     * @brief Add a bone with optional parent name
     */
    SkeletonBuilder& AddBone(const std::string& name,
                              const std::string& parentName = "",
                              const glm::mat4& offsetMatrix = glm::mat4(1.0f),
                              const glm::mat4& localTransform = glm::mat4(1.0f));

    /**
     * @brief Set the global inverse transform
     */
    SkeletonBuilder& SetGlobalInverse(const glm::mat4& transform);

    /**
     * @brief Build the skeleton
     */
    [[nodiscard]] Skeleton Build();

private:
    struct BoneData {
        std::string name;
        std::string parentName;
        glm::mat4 offsetMatrix{1.0f};
        glm::mat4 localTransform{1.0f};
    };

    std::vector<BoneData> m_boneData;
    glm::mat4 m_globalInverse{1.0f};
};

} // namespace Nova
