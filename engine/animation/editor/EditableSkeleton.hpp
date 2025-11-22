#pragma once

#include "../Skeleton.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <set>

namespace Nova {

/**
 * @brief Editable bone transform
 */
struct EditableBoneTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 ToMatrix() const;
    static EditableBoneTransform FromMatrix(const glm::mat4& matrix);
    static EditableBoneTransform Lerp(const EditableBoneTransform& a, const EditableBoneTransform& b, float t);
};

/**
 * @brief Bone constraint for editing
 */
struct EditableBoneConstraint {
    enum class Type {
        None,
        LookAt,
        CopyTransform,
        LimitRotation,
        IK
    };

    Type type = Type::None;
    std::string targetBone;
    float influence = 1.0f;
    glm::vec3 limitMin{-180.0f};
    glm::vec3 limitMax{180.0f};
    int ikChainLength = 2;
};

/**
 * @brief IK solver for editing
 */
class EditableIKSolver {
public:
    struct IKTarget {
        std::string endEffector;
        std::string rootBone;
        glm::vec3 targetPosition{0.0f};
        glm::vec3 poleVector{0.0f, 0.0f, 1.0f};
        int chainLength = 2;
        int iterations = 10;
        float tolerance = 0.001f;
        bool enabled = true;
    };

    /**
     * @brief Add IK target
     */
    void AddTarget(const std::string& name, const IKTarget& target);

    /**
     * @brief Remove IK target
     */
    void RemoveTarget(const std::string& name);

    /**
     * @brief Get IK target
     */
    [[nodiscard]] IKTarget* GetTarget(const std::string& name);

    /**
     * @brief Get all targets
     */
    [[nodiscard]] const std::unordered_map<std::string, IKTarget>& GetTargets() const { return m_targets; }

    /**
     * @brief Solve IK for all targets
     */
    void SolveAll(std::unordered_map<std::string, EditableBoneTransform>& transforms,
                  const Skeleton* skeleton);

    /**
     * @brief Solve single IK target
     */
    void Solve(const std::string& targetName,
               std::unordered_map<std::string, EditableBoneTransform>& transforms,
               const Skeleton* skeleton);

private:
    void SolveFABRIK(const IKTarget& target,
                     std::unordered_map<std::string, EditableBoneTransform>& transforms,
                     const Skeleton* skeleton);

    std::unordered_map<std::string, IKTarget> m_targets;
};

/**
 * @brief Editable skeleton wrapper
 *
 * Provides editing capabilities on top of the base Skeleton class:
 * - Bone selection
 * - Transform manipulation
 * - Constraint system
 * - IK solver
 */
class EditableSkeleton {
public:
    EditableSkeleton();
    explicit EditableSkeleton(Skeleton* skeleton);
    ~EditableSkeleton();

    /**
     * @brief Set skeleton to edit
     */
    void SetSkeleton(Skeleton* skeleton);

    /**
     * @brief Get underlying skeleton
     */
    [[nodiscard]] Skeleton* GetSkeleton() { return m_skeleton; }
    [[nodiscard]] const Skeleton* GetSkeleton() const { return m_skeleton; }

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select bone
     */
    void SelectBone(const std::string& boneName, bool addToSelection = false);

    /**
     * @brief Deselect bone
     */
    void DeselectBone(const std::string& boneName);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Select all bones
     */
    void SelectAll();

    /**
     * @brief Get primary selection
     */
    [[nodiscard]] const std::string& GetPrimarySelection() const { return m_primarySelection; }

    /**
     * @brief Get all selected bones
     */
    [[nodiscard]] const std::set<std::string>& GetSelectedBones() const { return m_selectedBones; }

    /**
     * @brief Is bone selected
     */
    [[nodiscard]] bool IsBoneSelected(const std::string& boneName) const;

    // =========================================================================
    // Transforms
    // =========================================================================

    /**
     * @brief Get bone transform
     */
    [[nodiscard]] EditableBoneTransform GetBoneTransform(const std::string& boneName) const;

    /**
     * @brief Set bone transform
     */
    void SetBoneTransform(const std::string& boneName, const EditableBoneTransform& transform);

    /**
     * @brief Get bone world transform
     */
    [[nodiscard]] glm::mat4 GetBoneWorldTransform(const std::string& boneName) const;

    /**
     * @brief Get all transforms
     */
    [[nodiscard]] const std::unordered_map<std::string, EditableBoneTransform>& GetAllTransforms() const {
        return m_transforms;
    }

    /**
     * @brief Set all transforms
     */
    void SetAllTransforms(const std::unordered_map<std::string, EditableBoneTransform>& transforms);

    /**
     * @brief Reset to bind pose
     */
    void ResetToBindPose();

    /**
     * @brief Reset bone to bind pose
     */
    void ResetBoneToBindPose(const std::string& boneName);

    // =========================================================================
    // Constraints
    // =========================================================================

    /**
     * @brief Add constraint
     */
    void AddConstraint(const std::string& boneName, const EditableBoneConstraint& constraint);

    /**
     * @brief Remove constraint
     */
    void RemoveConstraint(const std::string& boneName);

    /**
     * @brief Get constraint
     */
    [[nodiscard]] EditableBoneConstraint* GetConstraint(const std::string& boneName);

    /**
     * @brief Apply all constraints
     */
    void ApplyConstraints();

    // =========================================================================
    // IK
    // =========================================================================

    /**
     * @brief Get IK solver
     */
    [[nodiscard]] EditableIKSolver& GetIKSolver() { return m_ikSolver; }

    /**
     * @brief Solve IK
     */
    void SolveIK();

    // =========================================================================
    // Hierarchy
    // =========================================================================

    /**
     * @brief Get child bones
     */
    [[nodiscard]] std::vector<std::string> GetChildBones(const std::string& boneName) const;

    /**
     * @brief Get parent bone
     */
    [[nodiscard]] std::string GetParentBone(const std::string& boneName) const;

    /**
     * @brief Get bones in hierarchy order
     */
    [[nodiscard]] std::vector<std::string> GetBonesInHierarchyOrder() const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Update world transforms
     */
    void UpdateWorldTransforms();

    /**
     * @brief Get bone matrices for rendering
     */
    [[nodiscard]] std::vector<glm::mat4> GetBoneMatrices() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnBoneSelected;
    std::function<void(const std::string&)> OnTransformChanged;

private:
    void BuildHierarchyCache();
    glm::mat4 CalculateWorldTransform(const std::string& boneName) const;

    Skeleton* m_skeleton = nullptr;

    // Transforms
    std::unordered_map<std::string, EditableBoneTransform> m_transforms;
    std::unordered_map<std::string, glm::mat4> m_worldTransforms;

    // Selection
    std::string m_primarySelection;
    std::set<std::string> m_selectedBones;

    // Constraints
    std::unordered_map<std::string, EditableBoneConstraint> m_constraints;

    // IK
    EditableIKSolver m_ikSolver;

    // Hierarchy cache
    std::unordered_map<std::string, std::vector<std::string>> m_childrenCache;
    std::vector<std::string> m_hierarchyOrder;
};

} // namespace Nova
