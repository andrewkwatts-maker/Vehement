#include "EditableSkeleton.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <queue>
#include <algorithm>

namespace Nova {

// ============================================================================
// EditableBoneTransform
// ============================================================================

glm::mat4 EditableBoneTransform::ToMatrix() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rot = glm::toMat4(rotation);
    glm::mat4 scl = glm::scale(glm::mat4(1.0f), scale);
    return translation * rot * scl;
}

EditableBoneTransform EditableBoneTransform::FromMatrix(const glm::mat4& matrix) {
    EditableBoneTransform result;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, result.scale, result.rotation, result.position, skew, perspective);
    return result;
}

EditableBoneTransform EditableBoneTransform::Lerp(const EditableBoneTransform& a, const EditableBoneTransform& b, float t) {
    EditableBoneTransform result;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

// ============================================================================
// EditableIKSolver
// ============================================================================

void EditableIKSolver::AddTarget(const std::string& name, const IKTarget& target) {
    m_targets[name] = target;
}

void EditableIKSolver::RemoveTarget(const std::string& name) {
    m_targets.erase(name);
}

EditableIKSolver::IKTarget* EditableIKSolver::GetTarget(const std::string& name) {
    auto it = m_targets.find(name);
    return it != m_targets.end() ? &it->second : nullptr;
}

void EditableIKSolver::SolveAll(std::unordered_map<std::string, EditableBoneTransform>& transforms,
                                 const Skeleton* skeleton) {
    for (auto& [name, target] : m_targets) {
        if (target.enabled) {
            SolveFABRIK(target, transforms, skeleton);
        }
    }
}

void EditableIKSolver::Solve(const std::string& targetName,
                              std::unordered_map<std::string, EditableBoneTransform>& transforms,
                              const Skeleton* skeleton) {
    auto it = m_targets.find(targetName);
    if (it != m_targets.end() && it->second.enabled) {
        SolveFABRIK(it->second, transforms, skeleton);
    }
}

void EditableIKSolver::SolveFABRIK(const IKTarget& target,
                                    std::unordered_map<std::string, EditableBoneTransform>& transforms,
                                    const Skeleton* skeleton) {
    if (!skeleton) return;

    // Build chain from end effector to root
    std::vector<std::string> chain;
    std::string current = target.endEffector;

    for (int i = 0; i < target.chainLength; ++i) {
        int boneIdx = skeleton->GetBoneIndex(current);
        if (boneIdx < 0) break;

        chain.push_back(current);

        const auto* bone = skeleton->GetBoneByIndex(static_cast<size_t>(boneIdx));
        if (!bone || bone->parentIndex < 0) break;

        const auto* parent = skeleton->GetBoneByIndex(static_cast<size_t>(bone->parentIndex));
        if (!parent) break;

        current = parent->name;
    }

    if (chain.size() < 2) return;

    // Get world positions
    std::vector<glm::vec3> positions(chain.size());
    std::vector<float> lengths(chain.size() - 1);

    // Calculate initial positions (simplified - would need proper world transform calculation)
    for (size_t i = 0; i < chain.size(); ++i) {
        auto it = transforms.find(chain[i]);
        if (it != transforms.end()) {
            positions[i] = it->second.position;
        }
    }

    // Calculate bone lengths
    for (size_t i = 0; i < lengths.size(); ++i) {
        lengths[i] = glm::length(positions[i] - positions[i + 1]);
    }

    glm::vec3 rootPos = positions.back();

    // FABRIK iterations
    for (int iter = 0; iter < target.iterations; ++iter) {
        // Forward pass (end to root)
        positions[0] = target.targetPosition;
        for (size_t i = 0; i < positions.size() - 1; ++i) {
            glm::vec3 dir = glm::normalize(positions[i + 1] - positions[i]);
            positions[i + 1] = positions[i] + dir * lengths[i];
        }

        // Backward pass (root to end)
        positions.back() = rootPos;
        for (int i = static_cast<int>(positions.size()) - 2; i >= 0; --i) {
            size_t ui = static_cast<size_t>(i);
            glm::vec3 dir = glm::normalize(positions[ui] - positions[ui + 1]);
            positions[ui] = positions[ui + 1] + dir * lengths[ui];
        }

        // Check convergence
        if (glm::length(positions[0] - target.targetPosition) < target.tolerance) {
            break;
        }
    }

    // Convert positions back to rotations
    for (size_t i = chain.size() - 1; i > 0; --i) {
        auto it = transforms.find(chain[i]);
        if (it != transforms.end()) {
            // Calculate rotation to point at child
            glm::vec3 currentDir = glm::normalize(positions[i - 1] - positions[i]);
            glm::vec3 defaultDir = glm::vec3(0, 1, 0);  // Simplified

            if (glm::length(currentDir - defaultDir) > 0.0001f) {
                it->second.rotation = glm::rotation(defaultDir, currentDir);
            }
        }
    }
}

// ============================================================================
// EditableSkeleton
// ============================================================================

EditableSkeleton::EditableSkeleton() = default;

EditableSkeleton::EditableSkeleton(Skeleton* skeleton) {
    SetSkeleton(skeleton);
}

EditableSkeleton::~EditableSkeleton() = default;

void EditableSkeleton::SetSkeleton(Skeleton* skeleton) {
    m_skeleton = skeleton;
    m_transforms.clear();
    m_worldTransforms.clear();
    m_selectedBones.clear();
    m_primarySelection.clear();

    if (skeleton) {
        BuildHierarchyCache();
        ResetToBindPose();
    }
}

// ============================================================================
// Selection
// ============================================================================

void EditableSkeleton::SelectBone(const std::string& boneName, bool addToSelection) {
    if (!m_skeleton || m_skeleton->GetBoneIndex(boneName) < 0) return;

    if (!addToSelection) {
        m_selectedBones.clear();
    }

    m_selectedBones.insert(boneName);
    m_primarySelection = boneName;

    if (OnBoneSelected) {
        OnBoneSelected(boneName);
    }
}

void EditableSkeleton::DeselectBone(const std::string& boneName) {
    m_selectedBones.erase(boneName);
    if (m_primarySelection == boneName) {
        m_primarySelection = m_selectedBones.empty() ? "" : *m_selectedBones.begin();
    }
}

void EditableSkeleton::ClearSelection() {
    m_selectedBones.clear();
    m_primarySelection.clear();
}

void EditableSkeleton::SelectAll() {
    if (!m_skeleton) return;

    m_selectedBones.clear();
    for (const auto& bone : m_skeleton->GetBones()) {
        m_selectedBones.insert(bone.name);
    }

    if (!m_selectedBones.empty()) {
        m_primarySelection = *m_selectedBones.begin();
    }
}

bool EditableSkeleton::IsBoneSelected(const std::string& boneName) const {
    return m_selectedBones.count(boneName) > 0;
}

// ============================================================================
// Transforms
// ============================================================================

EditableBoneTransform EditableSkeleton::GetBoneTransform(const std::string& boneName) const {
    auto it = m_transforms.find(boneName);
    return it != m_transforms.end() ? it->second : EditableBoneTransform();
}

void EditableSkeleton::SetBoneTransform(const std::string& boneName, const EditableBoneTransform& transform) {
    m_transforms[boneName] = transform;
    UpdateWorldTransforms();

    if (OnTransformChanged) {
        OnTransformChanged(boneName);
    }
}

glm::mat4 EditableSkeleton::GetBoneWorldTransform(const std::string& boneName) const {
    auto it = m_worldTransforms.find(boneName);
    return it != m_worldTransforms.end() ? it->second : glm::mat4(1.0f);
}

void EditableSkeleton::SetAllTransforms(const std::unordered_map<std::string, EditableBoneTransform>& transforms) {
    m_transforms = transforms;
    UpdateWorldTransforms();
}

void EditableSkeleton::ResetToBindPose() {
    if (!m_skeleton) return;

    m_transforms.clear();
    for (const auto& bone : m_skeleton->GetBones()) {
        m_transforms[bone.name] = EditableBoneTransform::FromMatrix(bone.localTransform);
    }

    UpdateWorldTransforms();
}

void EditableSkeleton::ResetBoneToBindPose(const std::string& boneName) {
    if (!m_skeleton) return;

    const auto* bone = m_skeleton->GetBone(boneName);
    if (bone) {
        m_transforms[boneName] = EditableBoneTransform::FromMatrix(bone->localTransform);
        UpdateWorldTransforms();
    }
}

// ============================================================================
// Constraints
// ============================================================================

void EditableSkeleton::AddConstraint(const std::string& boneName, const EditableBoneConstraint& constraint) {
    m_constraints[boneName] = constraint;
}

void EditableSkeleton::RemoveConstraint(const std::string& boneName) {
    m_constraints.erase(boneName);
}

EditableBoneConstraint* EditableSkeleton::GetConstraint(const std::string& boneName) {
    auto it = m_constraints.find(boneName);
    return it != m_constraints.end() ? &it->second : nullptr;
}

void EditableSkeleton::ApplyConstraints() {
    for (const auto& [boneName, constraint] : m_constraints) {
        if (constraint.influence <= 0.0f) continue;

        auto it = m_transforms.find(boneName);
        if (it == m_transforms.end()) continue;

        switch (constraint.type) {
            case EditableBoneConstraint::Type::LimitRotation: {
                glm::vec3 euler = glm::eulerAngles(it->second.rotation);
                euler = glm::clamp(euler, glm::radians(constraint.limitMin), glm::radians(constraint.limitMax));
                it->second.rotation = glm::quat(euler);
                break;
            }

            case EditableBoneConstraint::Type::CopyTransform: {
                auto targetIt = m_transforms.find(constraint.targetBone);
                if (targetIt != m_transforms.end()) {
                    it->second = EditableBoneTransform::Lerp(it->second, targetIt->second, constraint.influence);
                }
                break;
            }

            default:
                break;
        }
    }

    UpdateWorldTransforms();
}

// ============================================================================
// IK
// ============================================================================

void EditableSkeleton::SolveIK() {
    m_ikSolver.SolveAll(m_transforms, m_skeleton);
    UpdateWorldTransforms();
}

// ============================================================================
// Hierarchy
// ============================================================================

std::vector<std::string> EditableSkeleton::GetChildBones(const std::string& boneName) const {
    auto it = m_childrenCache.find(boneName);
    return it != m_childrenCache.end() ? it->second : std::vector<std::string>{};
}

std::string EditableSkeleton::GetParentBone(const std::string& boneName) const {
    if (!m_skeleton) return "";

    int idx = m_skeleton->GetBoneIndex(boneName);
    if (idx < 0) return "";

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(idx));
    if (!bone || bone->parentIndex < 0) return "";

    const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone->parentIndex));
    return parent ? parent->name : "";
}

std::vector<std::string> EditableSkeleton::GetBonesInHierarchyOrder() const {
    return m_hierarchyOrder;
}

// ============================================================================
// Utility
// ============================================================================

void EditableSkeleton::UpdateWorldTransforms() {
    m_worldTransforms.clear();

    for (const auto& boneName : m_hierarchyOrder) {
        m_worldTransforms[boneName] = CalculateWorldTransform(boneName);
    }
}

std::vector<glm::mat4> EditableSkeleton::GetBoneMatrices() const {
    if (!m_skeleton) return {};

    std::unordered_map<std::string, glm::mat4> animTransforms;
    for (const auto& [name, transform] : m_transforms) {
        animTransforms[name] = transform.ToMatrix();
    }

    return m_skeleton->CalculateBoneMatrices(animTransforms);
}

// ============================================================================
// Private
// ============================================================================

void EditableSkeleton::BuildHierarchyCache() {
    m_childrenCache.clear();
    m_hierarchyOrder.clear();

    if (!m_skeleton) return;

    // Build children map
    for (const auto& bone : m_skeleton->GetBones()) {
        if (bone.parentIndex >= 0) {
            const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone.parentIndex));
            if (parent) {
                m_childrenCache[parent->name].push_back(bone.name);
            }
        }
    }

    // Build hierarchy order (BFS)
    std::queue<std::string> toProcess;
    for (const auto& bone : m_skeleton->GetBones()) {
        if (bone.parentIndex < 0) {
            toProcess.push(bone.name);
        }
    }

    while (!toProcess.empty()) {
        std::string current = toProcess.front();
        toProcess.pop();

        m_hierarchyOrder.push_back(current);

        auto it = m_childrenCache.find(current);
        if (it != m_childrenCache.end()) {
            for (const auto& child : it->second) {
                toProcess.push(child);
            }
        }
    }
}

glm::mat4 EditableSkeleton::CalculateWorldTransform(const std::string& boneName) const {
    if (!m_skeleton) return glm::mat4(1.0f);

    int idx = m_skeleton->GetBoneIndex(boneName);
    if (idx < 0) return glm::mat4(1.0f);

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(idx));
    if (!bone) return glm::mat4(1.0f);

    glm::mat4 localTransform = glm::mat4(1.0f);
    auto it = m_transforms.find(boneName);
    if (it != m_transforms.end()) {
        localTransform = it->second.ToMatrix();
    }

    if (bone->parentIndex >= 0) {
        const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone->parentIndex));
        if (parent) {
            auto parentIt = m_worldTransforms.find(parent->name);
            if (parentIt != m_worldTransforms.end()) {
                return parentIt->second * localTransform;
            }
        }
    }

    return localTransform;
}

} // namespace Nova
