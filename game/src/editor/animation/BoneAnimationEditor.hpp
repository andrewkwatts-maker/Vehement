#pragma once

#include "engine/animation/Skeleton.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <set>

namespace Nova {
    class Mesh;
    class Model;
}

namespace Vehement {

/**
 * @brief Transform gizmo operation mode
 */
enum class GizmoMode {
    Translate,
    Rotate,
    Scale,
    Universal  // All three combined
};

/**
 * @brief Transform space for gizmo operations
 */
enum class TransformSpace {
    Local,
    World,
    Parent
};

/**
 * @brief Bone constraint type
 */
enum class ConstraintType {
    None,
    LookAt,
    CopyPosition,
    CopyRotation,
    CopyScale,
    LimitPosition,
    LimitRotation,
    LimitScale,
    IKChain
};

/**
 * @brief Bone constraint definition
 */
struct BoneConstraint {
    ConstraintType type = ConstraintType::None;
    std::string targetBone;
    float influence = 1.0f;

    // Axis constraints
    glm::bvec3 axisLock{false, false, false};

    // Limit constraints
    glm::vec3 limitMin{-180.0f};
    glm::vec3 limitMax{180.0f};

    // IK specific
    int chainLength = 2;
    int iterations = 10;
    float tolerance = 0.001f;
    glm::vec3 poleVector{0.0f, 0.0f, 1.0f};
};

/**
 * @brief Editable bone transform data
 */
struct BoneTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 ToMatrix() const;
    static BoneTransform FromMatrix(const glm::mat4& matrix);
    static BoneTransform Lerp(const BoneTransform& a, const BoneTransform& b, float t);
    static BoneTransform Slerp(const BoneTransform& a, const BoneTransform& b, float t);
};

/**
 * @brief Selection state for bone
 */
struct BoneSelection {
    std::string boneName;
    int boneIndex = -1;
    bool isPrimary = false;  // Primary selection for transform gizmo
};

/**
 * @brief IK chain definition for editing
 */
struct IKChain {
    std::string name;
    std::string endEffector;
    std::string rootBone;
    int chainLength = 2;
    glm::vec3 targetPosition{0.0f};
    glm::vec3 poleTarget{0.0f, 0.0f, 1.0f};
    float weight = 1.0f;
    bool enabled = true;
};

/**
 * @brief Core bone animation editor
 *
 * Features:
 * - Load skeleton from model
 * - Select/manipulate bones
 * - Transform gizmos (translate, rotate, scale)
 * - Bone hierarchy tree
 * - Bone constraints visualization
 * - IK chain editing
 * - Mirror pose tools
 */
class BoneAnimationEditor {
public:
    struct Config {
        float gizmoSize = 100.0f;
        float boneDisplaySize = 0.05f;
        bool showBoneNames = true;
        bool showConstraints = true;
        bool showIKTargets = true;
        glm::vec4 selectedBoneColor{1.0f, 0.8f, 0.0f, 1.0f};
        glm::vec4 boneColor{0.5f, 0.5f, 0.8f, 1.0f};
        glm::vec4 ikTargetColor{0.0f, 1.0f, 0.5f, 1.0f};
        float selectionRadius = 15.0f;
    };

    BoneAnimationEditor();
    ~BoneAnimationEditor();

    // Non-copyable
    BoneAnimationEditor(const BoneAnimationEditor&) = delete;
    BoneAnimationEditor& operator=(const BoneAnimationEditor&) = delete;

    /**
     * @brief Initialize the editor
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Shutdown editor
     */
    void Shutdown();

    // =========================================================================
    // Skeleton Management
    // =========================================================================

    /**
     * @brief Load skeleton from model file
     */
    bool LoadSkeletonFromModel(const std::string& modelPath);

    /**
     * @brief Load skeleton directly
     */
    bool LoadSkeleton(Nova::Skeleton* skeleton);

    /**
     * @brief Create new empty skeleton
     */
    void CreateNewSkeleton();

    /**
     * @brief Get the current skeleton
     */
    [[nodiscard]] Nova::Skeleton* GetSkeleton() { return m_skeleton; }
    [[nodiscard]] const Nova::Skeleton* GetSkeleton() const { return m_skeleton; }

    /**
     * @brief Check if skeleton is loaded
     */
    [[nodiscard]] bool HasSkeleton() const { return m_skeleton != nullptr; }

    // =========================================================================
    // Bone Selection
    // =========================================================================

    /**
     * @brief Select bone by name
     */
    void SelectBone(const std::string& boneName, bool addToSelection = false);

    /**
     * @brief Select bone by index
     */
    void SelectBoneByIndex(int index, bool addToSelection = false);

    /**
     * @brief Deselect bone
     */
    void DeselectBone(const std::string& boneName);

    /**
     * @brief Clear all selections
     */
    void ClearSelection();

    /**
     * @brief Select all bones
     */
    void SelectAll();

    /**
     * @brief Select bone hierarchy (bone and all children)
     */
    void SelectHierarchy(const std::string& rootBone);

    /**
     * @brief Get primary selected bone name
     */
    [[nodiscard]] const std::string& GetPrimarySelection() const { return m_primarySelection; }

    /**
     * @brief Get all selected bones
     */
    [[nodiscard]] const std::set<std::string>& GetSelectedBones() const { return m_selectedBones; }

    /**
     * @brief Check if bone is selected
     */
    [[nodiscard]] bool IsBoneSelected(const std::string& boneName) const;

    /**
     * @brief Pick bone at screen position
     */
    [[nodiscard]] std::string PickBone(const glm::vec2& screenPos, const glm::mat4& viewProj);

    // =========================================================================
    // Bone Transforms
    // =========================================================================

    /**
     * @brief Get bone transform
     */
    [[nodiscard]] BoneTransform GetBoneTransform(const std::string& boneName) const;

    /**
     * @brief Set bone transform
     */
    void SetBoneTransform(const std::string& boneName, const BoneTransform& transform);

    /**
     * @brief Get bone world transform
     */
    [[nodiscard]] glm::mat4 GetBoneWorldTransform(const std::string& boneName) const;

    /**
     * @brief Set bone world transform
     */
    void SetBoneWorldTransform(const std::string& boneName, const glm::mat4& worldTransform);

    /**
     * @brief Reset bone to bind pose
     */
    void ResetBoneToBindPose(const std::string& boneName);

    /**
     * @brief Reset all bones to bind pose
     */
    void ResetAllToBindPose();

    /**
     * @brief Get all current bone transforms
     */
    [[nodiscard]] const std::unordered_map<std::string, BoneTransform>& GetAllTransforms() const {
        return m_boneTransforms;
    }

    /**
     * @brief Set all bone transforms
     */
    void SetAllTransforms(const std::unordered_map<std::string, BoneTransform>& transforms);

    // =========================================================================
    // Gizmo Control
    // =========================================================================

    /**
     * @brief Set gizmo mode
     */
    void SetGizmoMode(GizmoMode mode) { m_gizmoMode = mode; }

    /**
     * @brief Get gizmo mode
     */
    [[nodiscard]] GizmoMode GetGizmoMode() const { return m_gizmoMode; }

    /**
     * @brief Set transform space
     */
    void SetTransformSpace(TransformSpace space) { m_transformSpace = space; }

    /**
     * @brief Get transform space
     */
    [[nodiscard]] TransformSpace GetTransformSpace() const { return m_transformSpace; }

    /**
     * @brief Begin gizmo interaction
     */
    void BeginGizmoInteraction(const glm::vec2& screenPos);

    /**
     * @brief Update gizmo interaction
     */
    void UpdateGizmoInteraction(const glm::vec2& screenPos);

    /**
     * @brief End gizmo interaction
     */
    void EndGizmoInteraction();

    /**
     * @brief Check if currently manipulating gizmo
     */
    [[nodiscard]] bool IsManipulatingGizmo() const { return m_isManipulatingGizmo; }

    // =========================================================================
    // Bone Constraints
    // =========================================================================

    /**
     * @brief Add constraint to bone
     */
    void AddConstraint(const std::string& boneName, const BoneConstraint& constraint);

    /**
     * @brief Remove constraint from bone
     */
    void RemoveConstraint(const std::string& boneName, ConstraintType type);

    /**
     * @brief Get constraints for bone
     */
    [[nodiscard]] std::vector<BoneConstraint> GetConstraints(const std::string& boneName) const;

    /**
     * @brief Apply all constraints
     */
    void ApplyConstraints();

    // =========================================================================
    // IK Chain Editing
    // =========================================================================

    /**
     * @brief Create IK chain
     */
    void CreateIKChain(const std::string& name, const std::string& endEffector, int chainLength);

    /**
     * @brief Remove IK chain
     */
    void RemoveIKChain(const std::string& name);

    /**
     * @brief Get IK chain
     */
    [[nodiscard]] IKChain* GetIKChain(const std::string& name);

    /**
     * @brief Get all IK chains
     */
    [[nodiscard]] const std::vector<IKChain>& GetIKChains() const { return m_ikChains; }

    /**
     * @brief Set IK target position
     */
    void SetIKTarget(const std::string& chainName, const glm::vec3& position);

    /**
     * @brief Solve IK for chain
     */
    void SolveIK(const std::string& chainName);

    /**
     * @brief Solve all IK chains
     */
    void SolveAllIK();

    /**
     * @brief Toggle IK/FK mode for bone
     */
    void SetIKEnabled(const std::string& chainName, bool enabled);

    // =========================================================================
    // Mirror Tools
    // =========================================================================

    /**
     * @brief Mirror pose across axis
     */
    void MirrorPose(const std::string& axis = "X");

    /**
     * @brief Mirror selected bones
     */
    void MirrorSelectedBones(const std::string& axis = "X");

    /**
     * @brief Set bone name mirroring pattern
     */
    void SetMirrorPattern(const std::string& leftPattern, const std::string& rightPattern);

    /**
     * @brief Get mirrored bone name
     */
    [[nodiscard]] std::string GetMirroredBoneName(const std::string& boneName) const;

    /**
     * @brief Copy pose from one side to the other
     */
    void CopyPoseToMirror(bool leftToRight = true);

    // =========================================================================
    // Bone Hierarchy
    // =========================================================================

    /**
     * @brief Get child bones
     */
    [[nodiscard]] std::vector<std::string> GetChildBones(const std::string& boneName) const;

    /**
     * @brief Get parent bone name
     */
    [[nodiscard]] std::string GetParentBone(const std::string& boneName) const;

    /**
     * @brief Get root bones (bones with no parent)
     */
    [[nodiscard]] std::vector<std::string> GetRootBones() const;

    /**
     * @brief Build flat list in hierarchical order
     */
    [[nodiscard]] std::vector<std::string> GetBonesInHierarchyOrder() const;

    // =========================================================================
    // Visualization
    // =========================================================================

    /**
     * @brief Update for rendering
     */
    void Update(float deltaTime);

    /**
     * @brief Get bone positions for rendering
     */
    [[nodiscard]] std::vector<std::pair<glm::vec3, glm::vec3>> GetBoneLines() const;

    /**
     * @brief Get joint positions
     */
    [[nodiscard]] std::vector<glm::vec3> GetJointPositions() const;

    /**
     * @brief Get gizmo transform matrix
     */
    [[nodiscard]] glm::mat4 GetGizmoTransform() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnBoneSelected;
    std::function<void(const std::string&, const BoneTransform&)> OnBoneTransformChanged;
    std::function<void()> OnPoseChanged;
    std::function<void()> OnSkeletonLoaded;

    // =========================================================================
    // Undo/Redo Support
    // =========================================================================

    /**
     * @brief Begin transform batch (for undo grouping)
     */
    void BeginTransformBatch();

    /**
     * @brief End transform batch
     */
    void EndTransformBatch();

    /**
     * @brief Get transform state for undo
     */
    [[nodiscard]] std::unordered_map<std::string, BoneTransform> CaptureTransformState() const;

    /**
     * @brief Restore transform state
     */
    void RestoreTransformState(const std::unordered_map<std::string, BoneTransform>& state);

private:
    void BuildBoneHierarchyCache();
    void UpdateWorldTransforms();
    void SolveIKChain(IKChain& chain);
    void ApplyFABRIK(IKChain& chain);
    glm::mat4 CalculateWorldTransform(int boneIndex) const;
    void PropagateTransformToChildren(const std::string& boneName);

    Config m_config;
    Nova::Skeleton* m_skeleton = nullptr;
    bool m_ownsSkeleton = false;

    // Bone transforms
    std::unordered_map<std::string, BoneTransform> m_boneTransforms;
    std::unordered_map<std::string, glm::mat4> m_worldTransforms;

    // Selection
    std::string m_primarySelection;
    std::set<std::string> m_selectedBones;

    // Gizmo
    GizmoMode m_gizmoMode = GizmoMode::Rotate;
    TransformSpace m_transformSpace = TransformSpace::Local;
    bool m_isManipulatingGizmo = false;
    glm::vec2 m_gizmoStartPos{0.0f};
    BoneTransform m_gizmoStartTransform;

    // Constraints
    std::unordered_map<std::string, std::vector<BoneConstraint>> m_constraints;

    // IK
    std::vector<IKChain> m_ikChains;

    // Mirror
    std::string m_mirrorLeftPattern = "_L";
    std::string m_mirrorRightPattern = "_R";

    // Hierarchy cache
    std::unordered_map<std::string, std::vector<std::string>> m_childrenCache;
    std::vector<std::string> m_hierarchyOrder;

    // Transform batch for undo
    bool m_inTransformBatch = false;
    std::unordered_map<std::string, BoneTransform> m_batchStartState;

    bool m_initialized = false;
};

} // namespace Vehement
