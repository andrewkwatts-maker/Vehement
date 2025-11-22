#include "BoneAnimationEditor.hpp"
#include "engine/graphics/ModelLoader.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <queue>

namespace Vehement {

// ============================================================================
// BoneTransform Implementation
// ============================================================================

glm::mat4 BoneTransform::ToMatrix() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rot = glm::toMat4(rotation);
    glm::mat4 scl = glm::scale(glm::mat4(1.0f), scale);
    return translation * rot * scl;
}

BoneTransform BoneTransform::FromMatrix(const glm::mat4& matrix) {
    BoneTransform result;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, result.scale, result.rotation, result.position, skew, perspective);
    return result;
}

BoneTransform BoneTransform::Lerp(const BoneTransform& a, const BoneTransform& b, float t) {
    BoneTransform result;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

BoneTransform BoneTransform::Slerp(const BoneTransform& a, const BoneTransform& b, float t) {
    return Lerp(a, b, t);  // Position and scale linear, rotation slerp
}

// ============================================================================
// BoneAnimationEditor Implementation
// ============================================================================

BoneAnimationEditor::BoneAnimationEditor() = default;

BoneAnimationEditor::~BoneAnimationEditor() {
    Shutdown();
}

void BoneAnimationEditor::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void BoneAnimationEditor::Shutdown() {
    if (m_ownsSkeleton && m_skeleton) {
        delete m_skeleton;
    }
    m_skeleton = nullptr;
    m_ownsSkeleton = false;
    m_boneTransforms.clear();
    m_worldTransforms.clear();
    m_selectedBones.clear();
    m_primarySelection.clear();
    m_constraints.clear();
    m_ikChains.clear();
    m_initialized = false;
}

// ============================================================================
// Skeleton Management
// ============================================================================

bool BoneAnimationEditor::LoadSkeletonFromModel(const std::string& modelPath) {
    // In a real implementation, this would use the ModelLoader
    // For now, we create a placeholder
    if (m_ownsSkeleton && m_skeleton) {
        delete m_skeleton;
    }

    // Load model and extract skeleton
    // Nova::Model model;
    // if (!Nova::ModelLoader::LoadModel(modelPath, model)) {
    //     return false;
    // }

    // m_skeleton = new Nova::Skeleton(model.GetSkeleton());
    // m_ownsSkeleton = true;

    // For now, create a test skeleton
    m_skeleton = new Nova::Skeleton();
    m_ownsSkeleton = true;

    Nova::SkeletonBuilder builder;
    builder.AddBone("root", "", glm::mat4(1.0f), glm::mat4(1.0f));
    builder.AddBone("spine", "root");
    builder.AddBone("spine1", "spine");
    builder.AddBone("spine2", "spine1");
    builder.AddBone("neck", "spine2");
    builder.AddBone("head", "neck");
    builder.AddBone("shoulder_L", "spine2");
    builder.AddBone("upperarm_L", "shoulder_L");
    builder.AddBone("lowerarm_L", "upperarm_L");
    builder.AddBone("hand_L", "lowerarm_L");
    builder.AddBone("shoulder_R", "spine2");
    builder.AddBone("upperarm_R", "shoulder_R");
    builder.AddBone("lowerarm_R", "upperarm_R");
    builder.AddBone("hand_R", "lowerarm_R");
    builder.AddBone("hip_L", "root");
    builder.AddBone("thigh_L", "hip_L");
    builder.AddBone("calf_L", "thigh_L");
    builder.AddBone("foot_L", "calf_L");
    builder.AddBone("hip_R", "root");
    builder.AddBone("thigh_R", "hip_R");
    builder.AddBone("calf_R", "thigh_R");
    builder.AddBone("foot_R", "calf_R");

    *m_skeleton = builder.Build();

    BuildBoneHierarchyCache();
    ResetAllToBindPose();

    if (OnSkeletonLoaded) {
        OnSkeletonLoaded();
    }

    return true;
}

bool BoneAnimationEditor::LoadSkeleton(Nova::Skeleton* skeleton) {
    if (!skeleton) return false;

    if (m_ownsSkeleton && m_skeleton) {
        delete m_skeleton;
    }

    m_skeleton = skeleton;
    m_ownsSkeleton = false;

    BuildBoneHierarchyCache();
    ResetAllToBindPose();

    if (OnSkeletonLoaded) {
        OnSkeletonLoaded();
    }

    return true;
}

void BoneAnimationEditor::CreateNewSkeleton() {
    if (m_ownsSkeleton && m_skeleton) {
        delete m_skeleton;
    }

    m_skeleton = new Nova::Skeleton();
    m_ownsSkeleton = true;

    // Add root bone
    Nova::Bone root;
    root.name = "root";
    root.parentIndex = -1;
    m_skeleton->AddBone(root);

    BuildBoneHierarchyCache();
    ResetAllToBindPose();

    if (OnSkeletonLoaded) {
        OnSkeletonLoaded();
    }
}

// ============================================================================
// Bone Selection
// ============================================================================

void BoneAnimationEditor::SelectBone(const std::string& boneName, bool addToSelection) {
    if (!m_skeleton) return;

    if (m_skeleton->GetBoneIndex(boneName) < 0) return;

    if (!addToSelection) {
        m_selectedBones.clear();
    }

    m_selectedBones.insert(boneName);
    m_primarySelection = boneName;

    if (OnBoneSelected) {
        OnBoneSelected(boneName);
    }
}

void BoneAnimationEditor::SelectBoneByIndex(int index, bool addToSelection) {
    if (!m_skeleton || index < 0 || index >= static_cast<int>(m_skeleton->GetBoneCount())) {
        return;
    }

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(index));
    if (bone) {
        SelectBone(bone->name, addToSelection);
    }
}

void BoneAnimationEditor::DeselectBone(const std::string& boneName) {
    m_selectedBones.erase(boneName);

    if (m_primarySelection == boneName) {
        m_primarySelection = m_selectedBones.empty() ? "" : *m_selectedBones.begin();
    }
}

void BoneAnimationEditor::ClearSelection() {
    m_selectedBones.clear();
    m_primarySelection.clear();
}

void BoneAnimationEditor::SelectAll() {
    if (!m_skeleton) return;

    m_selectedBones.clear();
    for (const auto& bone : m_skeleton->GetBones()) {
        m_selectedBones.insert(bone.name);
    }

    if (!m_selectedBones.empty()) {
        m_primarySelection = *m_selectedBones.begin();
    }
}

void BoneAnimationEditor::SelectHierarchy(const std::string& rootBone) {
    if (!m_skeleton) return;

    std::queue<std::string> toProcess;
    toProcess.push(rootBone);

    while (!toProcess.empty()) {
        std::string current = toProcess.front();
        toProcess.pop();

        m_selectedBones.insert(current);

        auto it = m_childrenCache.find(current);
        if (it != m_childrenCache.end()) {
            for (const auto& child : it->second) {
                toProcess.push(child);
            }
        }
    }

    m_primarySelection = rootBone;
}

bool BoneAnimationEditor::IsBoneSelected(const std::string& boneName) const {
    return m_selectedBones.count(boneName) > 0;
}

std::string BoneAnimationEditor::PickBone(const glm::vec2& screenPos, const glm::mat4& viewProj) {
    if (!m_skeleton) return "";

    float closestDist = m_config.selectionRadius;
    std::string closestBone;

    for (const auto& bone : m_skeleton->GetBones()) {
        auto it = m_worldTransforms.find(bone.name);
        if (it == m_worldTransforms.end()) continue;

        glm::vec4 worldPos = it->second * glm::vec4(0, 0, 0, 1);
        glm::vec4 clipPos = viewProj * worldPos;

        if (clipPos.w <= 0) continue;

        glm::vec2 ndcPos = glm::vec2(clipPos.x, clipPos.y) / clipPos.w;
        // Assuming screen coordinates normalized to [-1, 1]
        float dist = glm::length(screenPos - ndcPos);

        if (dist < closestDist) {
            closestDist = dist;
            closestBone = bone.name;
        }
    }

    return closestBone;
}

// ============================================================================
// Bone Transforms
// ============================================================================

BoneTransform BoneAnimationEditor::GetBoneTransform(const std::string& boneName) const {
    auto it = m_boneTransforms.find(boneName);
    if (it != m_boneTransforms.end()) {
        return it->second;
    }
    return BoneTransform();
}

void BoneAnimationEditor::SetBoneTransform(const std::string& boneName, const BoneTransform& transform) {
    m_boneTransforms[boneName] = transform;
    UpdateWorldTransforms();

    if (OnBoneTransformChanged) {
        OnBoneTransformChanged(boneName, transform);
    }

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

glm::mat4 BoneAnimationEditor::GetBoneWorldTransform(const std::string& boneName) const {
    auto it = m_worldTransforms.find(boneName);
    if (it != m_worldTransforms.end()) {
        return it->second;
    }
    return glm::mat4(1.0f);
}

void BoneAnimationEditor::SetBoneWorldTransform(const std::string& boneName, const glm::mat4& worldTransform) {
    if (!m_skeleton) return;

    int boneIndex = m_skeleton->GetBoneIndex(boneName);
    if (boneIndex < 0) return;

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(boneIndex));
    if (!bone) return;

    glm::mat4 parentWorld = glm::mat4(1.0f);
    if (bone->parentIndex >= 0) {
        const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone->parentIndex));
        if (parent) {
            auto it = m_worldTransforms.find(parent->name);
            if (it != m_worldTransforms.end()) {
                parentWorld = it->second;
            }
        }
    }

    glm::mat4 localTransform = glm::inverse(parentWorld) * worldTransform;
    m_boneTransforms[boneName] = BoneTransform::FromMatrix(localTransform);

    UpdateWorldTransforms();

    if (OnBoneTransformChanged) {
        OnBoneTransformChanged(boneName, m_boneTransforms[boneName]);
    }
}

void BoneAnimationEditor::ResetBoneToBindPose(const std::string& boneName) {
    if (!m_skeleton) return;

    const auto* bone = m_skeleton->GetBone(boneName);
    if (!bone) return;

    m_boneTransforms[boneName] = BoneTransform::FromMatrix(bone->localTransform);
    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

void BoneAnimationEditor::ResetAllToBindPose() {
    if (!m_skeleton) return;

    m_boneTransforms.clear();

    for (const auto& bone : m_skeleton->GetBones()) {
        m_boneTransforms[bone.name] = BoneTransform::FromMatrix(bone.localTransform);
    }

    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

void BoneAnimationEditor::SetAllTransforms(const std::unordered_map<std::string, BoneTransform>& transforms) {
    m_boneTransforms = transforms;
    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

// ============================================================================
// Gizmo Control
// ============================================================================

void BoneAnimationEditor::BeginGizmoInteraction(const glm::vec2& screenPos) {
    if (m_primarySelection.empty()) return;

    m_isManipulatingGizmo = true;
    m_gizmoStartPos = screenPos;
    m_gizmoStartTransform = GetBoneTransform(m_primarySelection);

    if (!m_inTransformBatch) {
        BeginTransformBatch();
    }
}

void BoneAnimationEditor::UpdateGizmoInteraction(const glm::vec2& screenPos) {
    if (!m_isManipulatingGizmo || m_primarySelection.empty()) return;

    glm::vec2 delta = screenPos - m_gizmoStartPos;
    BoneTransform newTransform = m_gizmoStartTransform;

    // Simplified gizmo manipulation - in real implementation, this would
    // project screen delta to appropriate transform axis
    switch (m_gizmoMode) {
        case GizmoMode::Translate:
            newTransform.position += glm::vec3(delta.x * 0.01f, -delta.y * 0.01f, 0.0f);
            break;

        case GizmoMode::Rotate: {
            float angle = delta.x * 0.01f;
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0, 1, 0));
            newTransform.rotation = rotation * m_gizmoStartTransform.rotation;
            break;
        }

        case GizmoMode::Scale: {
            float scaleFactor = 1.0f + delta.x * 0.01f;
            newTransform.scale = m_gizmoStartTransform.scale * scaleFactor;
            break;
        }

        case GizmoMode::Universal:
            // Combine based on modifier keys
            break;
    }

    SetBoneTransform(m_primarySelection, newTransform);
}

void BoneAnimationEditor::EndGizmoInteraction() {
    if (m_isManipulatingGizmo && m_inTransformBatch) {
        EndTransformBatch();
    }
    m_isManipulatingGizmo = false;
}

// ============================================================================
// Bone Constraints
// ============================================================================

void BoneAnimationEditor::AddConstraint(const std::string& boneName, const BoneConstraint& constraint) {
    m_constraints[boneName].push_back(constraint);
}

void BoneAnimationEditor::RemoveConstraint(const std::string& boneName, ConstraintType type) {
    auto it = m_constraints.find(boneName);
    if (it != m_constraints.end()) {
        auto& constraints = it->second;
        constraints.erase(
            std::remove_if(constraints.begin(), constraints.end(),
                [type](const BoneConstraint& c) { return c.type == type; }),
            constraints.end()
        );
    }
}

std::vector<BoneConstraint> BoneAnimationEditor::GetConstraints(const std::string& boneName) const {
    auto it = m_constraints.find(boneName);
    if (it != m_constraints.end()) {
        return it->second;
    }
    return {};
}

void BoneAnimationEditor::ApplyConstraints() {
    for (const auto& [boneName, constraints] : m_constraints) {
        for (const auto& constraint : constraints) {
            if (constraint.influence <= 0.0f) continue;

            switch (constraint.type) {
                case ConstraintType::LimitRotation: {
                    auto& transform = m_boneTransforms[boneName];
                    glm::vec3 euler = glm::eulerAngles(transform.rotation);
                    euler = glm::clamp(euler, glm::radians(constraint.limitMin),
                                       glm::radians(constraint.limitMax));
                    transform.rotation = glm::quat(euler);
                    break;
                }

                case ConstraintType::CopyRotation: {
                    if (!constraint.targetBone.empty()) {
                        auto targetIt = m_boneTransforms.find(constraint.targetBone);
                        if (targetIt != m_boneTransforms.end()) {
                            auto& transform = m_boneTransforms[boneName];
                            transform.rotation = glm::slerp(transform.rotation,
                                targetIt->second.rotation, constraint.influence);
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    UpdateWorldTransforms();
}

// ============================================================================
// IK Chain Editing
// ============================================================================

void BoneAnimationEditor::CreateIKChain(const std::string& name, const std::string& endEffector, int chainLength) {
    IKChain chain;
    chain.name = name;
    chain.endEffector = endEffector;
    chain.chainLength = chainLength;

    // Find root bone of chain
    if (m_skeleton) {
        std::string current = endEffector;
        for (int i = 0; i < chainLength && !current.empty(); ++i) {
            chain.rootBone = current;
            current = GetParentBone(current);
        }
    }

    // Initialize target to current end effector position
    auto worldIt = m_worldTransforms.find(endEffector);
    if (worldIt != m_worldTransforms.end()) {
        chain.targetPosition = glm::vec3(worldIt->second[3]);
    }

    m_ikChains.push_back(chain);
}

void BoneAnimationEditor::RemoveIKChain(const std::string& name) {
    m_ikChains.erase(
        std::remove_if(m_ikChains.begin(), m_ikChains.end(),
            [&name](const IKChain& c) { return c.name == name; }),
        m_ikChains.end()
    );
}

IKChain* BoneAnimationEditor::GetIKChain(const std::string& name) {
    for (auto& chain : m_ikChains) {
        if (chain.name == name) {
            return &chain;
        }
    }
    return nullptr;
}

void BoneAnimationEditor::SetIKTarget(const std::string& chainName, const glm::vec3& position) {
    if (auto* chain = GetIKChain(chainName)) {
        chain->targetPosition = position;
    }
}

void BoneAnimationEditor::SolveIK(const std::string& chainName) {
    if (auto* chain = GetIKChain(chainName)) {
        if (chain->enabled) {
            SolveIKChain(*chain);
        }
    }
}

void BoneAnimationEditor::SolveAllIK() {
    for (auto& chain : m_ikChains) {
        if (chain.enabled) {
            SolveIKChain(chain);
        }
    }
}

void BoneAnimationEditor::SetIKEnabled(const std::string& chainName, bool enabled) {
    if (auto* chain = GetIKChain(chainName)) {
        chain->enabled = enabled;
    }
}

void BoneAnimationEditor::SolveIKChain(IKChain& chain) {
    ApplyFABRIK(chain);
}

void BoneAnimationEditor::ApplyFABRIK(IKChain& chain) {
    if (!m_skeleton) return;

    // Collect chain bones from end effector to root
    std::vector<std::string> chainBones;
    std::string current = chain.endEffector;

    for (int i = 0; i < chain.chainLength && !current.empty(); ++i) {
        chainBones.push_back(current);
        current = GetParentBone(current);
    }

    if (chainBones.empty()) return;

    // Get world positions
    std::vector<glm::vec3> positions;
    std::vector<float> lengths;

    for (const auto& boneName : chainBones) {
        auto it = m_worldTransforms.find(boneName);
        if (it != m_worldTransforms.end()) {
            positions.push_back(glm::vec3(it->second[3]));
        }
    }

    if (positions.size() < 2) return;

    // Calculate bone lengths
    for (size_t i = 0; i < positions.size() - 1; ++i) {
        lengths.push_back(glm::length(positions[i] - positions[i + 1]));
    }

    // FABRIK iterations
    glm::vec3 target = chain.targetPosition;
    glm::vec3 rootPos = positions.back();

    for (int iter = 0; iter < 10; ++iter) {
        // Forward reaching (from end to root)
        positions[0] = target;
        for (size_t i = 0; i < positions.size() - 1; ++i) {
            glm::vec3 dir = glm::normalize(positions[i + 1] - positions[i]);
            positions[i + 1] = positions[i] + dir * lengths[i];
        }

        // Backward reaching (from root to end)
        positions.back() = rootPos;
        for (int i = static_cast<int>(positions.size()) - 2; i >= 0; --i) {
            glm::vec3 dir = glm::normalize(positions[i] - positions[static_cast<size_t>(i) + 1]);
            positions[static_cast<size_t>(i)] = positions[static_cast<size_t>(i) + 1] + dir * lengths[static_cast<size_t>(i)];
        }

        // Check convergence
        if (glm::length(positions[0] - target) < chain.tolerance) {
            break;
        }
    }

    // Apply new positions as rotations
    for (size_t i = positions.size() - 1; i > 0; --i) {
        const std::string& boneName = chainBones[i];
        const std::string& childName = chainBones[i - 1];

        // Calculate rotation to point at next bone
        glm::vec3 currentDir = glm::normalize(
            glm::vec3(m_worldTransforms[childName][3]) -
            glm::vec3(m_worldTransforms[boneName][3])
        );
        glm::vec3 targetDir = glm::normalize(positions[i - 1] - positions[i]);

        if (glm::length(currentDir - targetDir) > 0.0001f) {
            glm::quat rotation = glm::rotation(currentDir, targetDir);
            auto& transform = m_boneTransforms[boneName];
            transform.rotation = rotation * transform.rotation;
        }
    }

    UpdateWorldTransforms();
}

// ============================================================================
// Mirror Tools
// ============================================================================

void BoneAnimationEditor::MirrorPose(const std::string& axis) {
    if (!m_skeleton) return;

    std::unordered_map<std::string, BoneTransform> mirroredTransforms;

    for (const auto& [boneName, transform] : m_boneTransforms) {
        std::string mirrorName = GetMirroredBoneName(boneName);

        if (!mirrorName.empty() && mirrorName != boneName) {
            BoneTransform mirrored = transform;

            if (axis == "X" || axis == "x") {
                mirrored.position.x = -mirrored.position.x;
                mirrored.rotation.y = -mirrored.rotation.y;
                mirrored.rotation.z = -mirrored.rotation.z;
            } else if (axis == "Y" || axis == "y") {
                mirrored.position.y = -mirrored.position.y;
                mirrored.rotation.x = -mirrored.rotation.x;
                mirrored.rotation.z = -mirrored.rotation.z;
            } else if (axis == "Z" || axis == "z") {
                mirrored.position.z = -mirrored.position.z;
                mirrored.rotation.x = -mirrored.rotation.x;
                mirrored.rotation.y = -mirrored.rotation.y;
            }

            mirroredTransforms[mirrorName] = mirrored;
        }
    }

    for (const auto& [name, transform] : mirroredTransforms) {
        m_boneTransforms[name] = transform;
    }

    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

void BoneAnimationEditor::MirrorSelectedBones(const std::string& axis) {
    for (const auto& boneName : m_selectedBones) {
        std::string mirrorName = GetMirroredBoneName(boneName);
        if (!mirrorName.empty() && mirrorName != boneName) {
            auto it = m_boneTransforms.find(boneName);
            if (it != m_boneTransforms.end()) {
                BoneTransform mirrored = it->second;

                if (axis == "X" || axis == "x") {
                    mirrored.position.x = -mirrored.position.x;
                    mirrored.rotation.y = -mirrored.rotation.y;
                    mirrored.rotation.z = -mirrored.rotation.z;
                }

                m_boneTransforms[mirrorName] = mirrored;
            }
        }
    }

    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

void BoneAnimationEditor::SetMirrorPattern(const std::string& leftPattern, const std::string& rightPattern) {
    m_mirrorLeftPattern = leftPattern;
    m_mirrorRightPattern = rightPattern;
}

std::string BoneAnimationEditor::GetMirroredBoneName(const std::string& boneName) const {
    // Check for left pattern
    size_t leftPos = boneName.find(m_mirrorLeftPattern);
    if (leftPos != std::string::npos) {
        std::string mirrored = boneName;
        mirrored.replace(leftPos, m_mirrorLeftPattern.length(), m_mirrorRightPattern);
        if (m_skeleton && m_skeleton->GetBoneIndex(mirrored) >= 0) {
            return mirrored;
        }
    }

    // Check for right pattern
    size_t rightPos = boneName.find(m_mirrorRightPattern);
    if (rightPos != std::string::npos) {
        std::string mirrored = boneName;
        mirrored.replace(rightPos, m_mirrorRightPattern.length(), m_mirrorLeftPattern);
        if (m_skeleton && m_skeleton->GetBoneIndex(mirrored) >= 0) {
            return mirrored;
        }
    }

    return "";
}

void BoneAnimationEditor::CopyPoseToMirror(bool leftToRight) {
    const std::string& fromPattern = leftToRight ? m_mirrorLeftPattern : m_mirrorRightPattern;
    const std::string& toPattern = leftToRight ? m_mirrorRightPattern : m_mirrorLeftPattern;

    std::unordered_map<std::string, BoneTransform> newTransforms;

    for (const auto& [boneName, transform] : m_boneTransforms) {
        size_t pos = boneName.find(fromPattern);
        if (pos != std::string::npos) {
            std::string targetName = boneName;
            targetName.replace(pos, fromPattern.length(), toPattern);

            if (m_skeleton && m_skeleton->GetBoneIndex(targetName) >= 0) {
                BoneTransform mirrored = transform;
                mirrored.position.x = -mirrored.position.x;
                mirrored.rotation.y = -mirrored.rotation.y;
                mirrored.rotation.z = -mirrored.rotation.z;
                newTransforms[targetName] = mirrored;
            }
        }
    }

    for (const auto& [name, transform] : newTransforms) {
        m_boneTransforms[name] = transform;
    }

    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

// ============================================================================
// Bone Hierarchy
// ============================================================================

std::vector<std::string> BoneAnimationEditor::GetChildBones(const std::string& boneName) const {
    auto it = m_childrenCache.find(boneName);
    if (it != m_childrenCache.end()) {
        return it->second;
    }
    return {};
}

std::string BoneAnimationEditor::GetParentBone(const std::string& boneName) const {
    if (!m_skeleton) return "";

    int index = m_skeleton->GetBoneIndex(boneName);
    if (index < 0) return "";

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(index));
    if (!bone || bone->parentIndex < 0) return "";

    const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone->parentIndex));
    return parent ? parent->name : "";
}

std::vector<std::string> BoneAnimationEditor::GetRootBones() const {
    std::vector<std::string> roots;
    if (!m_skeleton) return roots;

    for (const auto& bone : m_skeleton->GetBones()) {
        if (bone.parentIndex < 0) {
            roots.push_back(bone.name);
        }
    }

    return roots;
}

std::vector<std::string> BoneAnimationEditor::GetBonesInHierarchyOrder() const {
    return m_hierarchyOrder;
}

// ============================================================================
// Visualization
// ============================================================================

void BoneAnimationEditor::Update(float /*deltaTime*/) {
    UpdateWorldTransforms();
    ApplyConstraints();
}

std::vector<std::pair<glm::vec3, glm::vec3>> BoneAnimationEditor::GetBoneLines() const {
    std::vector<std::pair<glm::vec3, glm::vec3>> lines;
    if (!m_skeleton) return lines;

    for (const auto& bone : m_skeleton->GetBones()) {
        if (bone.parentIndex < 0) continue;

        const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone.parentIndex));
        if (!parent) continue;

        auto childIt = m_worldTransforms.find(bone.name);
        auto parentIt = m_worldTransforms.find(parent->name);

        if (childIt != m_worldTransforms.end() && parentIt != m_worldTransforms.end()) {
            glm::vec3 childPos = glm::vec3(childIt->second[3]);
            glm::vec3 parentPos = glm::vec3(parentIt->second[3]);
            lines.emplace_back(parentPos, childPos);
        }
    }

    return lines;
}

std::vector<glm::vec3> BoneAnimationEditor::GetJointPositions() const {
    std::vector<glm::vec3> positions;

    for (const auto& [name, transform] : m_worldTransforms) {
        positions.push_back(glm::vec3(transform[3]));
    }

    return positions;
}

glm::mat4 BoneAnimationEditor::GetGizmoTransform() const {
    if (m_primarySelection.empty()) {
        return glm::mat4(1.0f);
    }

    auto it = m_worldTransforms.find(m_primarySelection);
    if (it != m_worldTransforms.end()) {
        return it->second;
    }

    return glm::mat4(1.0f);
}

// ============================================================================
// Undo/Redo Support
// ============================================================================

void BoneAnimationEditor::BeginTransformBatch() {
    m_inTransformBatch = true;
    m_batchStartState = CaptureTransformState();
}

void BoneAnimationEditor::EndTransformBatch() {
    m_inTransformBatch = false;
    m_batchStartState.clear();
}

std::unordered_map<std::string, BoneTransform> BoneAnimationEditor::CaptureTransformState() const {
    return m_boneTransforms;
}

void BoneAnimationEditor::RestoreTransformState(const std::unordered_map<std::string, BoneTransform>& state) {
    m_boneTransforms = state;
    UpdateWorldTransforms();

    if (OnPoseChanged) {
        OnPoseChanged();
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void BoneAnimationEditor::BuildBoneHierarchyCache() {
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

    // Build hierarchy order (breadth-first)
    std::queue<std::string> toProcess;
    for (const auto& root : GetRootBones()) {
        toProcess.push(root);
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

void BoneAnimationEditor::UpdateWorldTransforms() {
    if (!m_skeleton) return;

    m_worldTransforms.clear();

    // Process in hierarchy order
    for (const auto& boneName : m_hierarchyOrder) {
        int index = m_skeleton->GetBoneIndex(boneName);
        if (index < 0) continue;

        m_worldTransforms[boneName] = CalculateWorldTransform(index);
    }
}

glm::mat4 BoneAnimationEditor::CalculateWorldTransform(int boneIndex) const {
    if (!m_skeleton || boneIndex < 0) {
        return glm::mat4(1.0f);
    }

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(boneIndex));
    if (!bone) return glm::mat4(1.0f);

    // Get local transform
    glm::mat4 localTransform = glm::mat4(1.0f);
    auto it = m_boneTransforms.find(bone->name);
    if (it != m_boneTransforms.end()) {
        localTransform = it->second.ToMatrix();
    }

    // Combine with parent
    if (bone->parentIndex >= 0) {
        const auto* parent = m_skeleton->GetBoneByIndex(static_cast<size_t>(bone->parentIndex));
        if (parent) {
            auto parentIt = m_worldTransforms.find(parent->name);
            if (parentIt != m_worldTransforms.end()) {
                return parentIt->second * localTransform;
            }
        }
    }

    return m_skeleton->GetGlobalInverseTransform() * localTransform;
}

void BoneAnimationEditor::PropagateTransformToChildren(const std::string& boneName) {
    auto it = m_childrenCache.find(boneName);
    if (it == m_childrenCache.end()) return;

    for (const auto& childName : it->second) {
        int childIndex = m_skeleton->GetBoneIndex(childName);
        if (childIndex >= 0) {
            m_worldTransforms[childName] = CalculateWorldTransform(childIndex);
            PropagateTransformToChildren(childName);
        }
    }
}

} // namespace Vehement
