#include "animation/Skeleton.hpp"
#include <algorithm>
#include <stdexcept>
#include <cassert>

namespace Nova {

void Skeleton::AddBone(const Bone& bone) {
    assert(m_bones.size() < MAX_BONES && "Exceeded maximum bone count");

    m_boneMap[bone.name] = static_cast<int>(m_bones.size());
    m_bones.push_back(bone);
    m_dirty = true;
}

void Skeleton::AddBone(Bone&& bone) {
    assert(m_bones.size() < MAX_BONES && "Exceeded maximum bone count");

    m_boneMap[bone.name] = static_cast<int>(m_bones.size());
    m_bones.push_back(std::move(bone));
    m_dirty = true;
}

Bone* Skeleton::GetBone(const std::string& name) {
    auto it = m_boneMap.find(name);
    return it != m_boneMap.end() ? &m_bones[it->second] : nullptr;
}

const Bone* Skeleton::GetBone(const std::string& name) const {
    auto it = m_boneMap.find(name);
    return it != m_boneMap.end() ? &m_bones[it->second] : nullptr;
}

int Skeleton::GetBoneIndex(const std::string& name) const {
    auto it = m_boneMap.find(name);
    return it != m_boneMap.end() ? it->second : -1;
}

Bone* Skeleton::GetBoneByIndex(size_t index) {
    return index < m_bones.size() ? &m_bones[index] : nullptr;
}

const Bone* Skeleton::GetBoneByIndex(size_t index) const {
    return index < m_bones.size() ? &m_bones[index] : nullptr;
}

void Skeleton::Reserve(size_t capacity) {
    m_bones.reserve(std::min(capacity, MAX_BONES));
    m_globalTransformCache.reserve(std::min(capacity, MAX_BONES));
}

void Skeleton::Clear() {
    m_bones.clear();
    m_boneMap.clear();
    m_globalTransformCache.clear();
    m_dirty = true;
}

std::vector<glm::mat4> Skeleton::CalculateBoneMatrices(
    const std::unordered_map<std::string, glm::mat4>& animationTransforms) const {

    std::vector<glm::mat4> finalMatrices(m_bones.size());
    CalculateBoneMatricesInto(animationTransforms, finalMatrices);
    return finalMatrices;
}

void Skeleton::CalculateBoneMatricesInto(
    const std::unordered_map<std::string, glm::mat4>& animationTransforms,
    std::span<glm::mat4> outMatrices) const {

    const size_t boneCount = m_bones.size();

    if (boneCount == 0 || outMatrices.size() < boneCount) {
        return;
    }

    // Resize cache if needed (this is rare after first frame)
    if (m_globalTransformCache.size() < boneCount) {
        m_globalTransformCache.resize(boneCount);
    }

    // Calculate global transforms in hierarchy order
    // Bones are assumed to be sorted parent-first, so we can iterate linearly
    for (size_t i = 0; i < boneCount; ++i) {
        const Bone& bone = m_bones[i];

        // Get animation transform or use default local transform
        glm::mat4 localTransform = bone.localTransform;

        auto it = animationTransforms.find(bone.name);
        if (it != animationTransforms.end()) {
            localTransform = it->second;
        }

        // Calculate global transform
        if (bone.parentIndex >= 0 && static_cast<size_t>(bone.parentIndex) < i) {
            // Parent transform is already calculated (parent-first ordering)
            m_globalTransformCache[i] = m_globalTransformCache[bone.parentIndex] * localTransform;
        } else {
            // Root bone or invalid parent
            m_globalTransformCache[i] = localTransform;
        }

        // Calculate final matrix: globalInverse * globalTransform * offsetMatrix
        outMatrices[i] = m_globalInverse * m_globalTransformCache[i] * bone.offsetMatrix;
    }
}

std::vector<glm::mat4> Skeleton::GetBindPoseMatrices() const {
    const size_t boneCount = m_bones.size();
    std::vector<glm::mat4> bindPose(boneCount, glm::mat4(1.0f));

    // Calculate global transforms for bind pose (using default local transforms)
    std::vector<glm::mat4> globalTransforms(boneCount);

    for (size_t i = 0; i < boneCount; ++i) {
        const Bone& bone = m_bones[i];

        if (bone.parentIndex >= 0 && static_cast<size_t>(bone.parentIndex) < i) {
            globalTransforms[i] = globalTransforms[bone.parentIndex] * bone.localTransform;
        } else {
            globalTransforms[i] = bone.localTransform;
        }

        bindPose[i] = m_globalInverse * globalTransforms[i] * bone.offsetMatrix;
    }

    return bindPose;
}

// SkeletonBuilder implementation

SkeletonBuilder& SkeletonBuilder::AddBone(const std::string& name,
                                           const std::string& parentName,
                                           const glm::mat4& offsetMatrix,
                                           const glm::mat4& localTransform) {
    m_boneData.push_back({name, parentName, offsetMatrix, localTransform});
    return *this;
}

SkeletonBuilder& SkeletonBuilder::SetGlobalInverse(const glm::mat4& transform) {
    m_globalInverse = transform;
    return *this;
}

Skeleton SkeletonBuilder::Build() {
    Skeleton skeleton;
    skeleton.SetGlobalInverseTransform(m_globalInverse);
    skeleton.Reserve(m_boneData.size());

    // Build name to index map for parent lookup
    std::unordered_map<std::string, int> nameToIndex;
    for (size_t i = 0; i < m_boneData.size(); ++i) {
        nameToIndex[m_boneData[i].name] = static_cast<int>(i);
    }

    // Sort bones so parents come before children
    std::vector<size_t> sortedIndices;
    sortedIndices.reserve(m_boneData.size());

    std::vector<bool> added(m_boneData.size(), false);

    // Topological sort
    while (sortedIndices.size() < m_boneData.size()) {
        bool progress = false;
        for (size_t i = 0; i < m_boneData.size(); ++i) {
            if (added[i]) continue;

            const auto& data = m_boneData[i];

            // Check if parent is already added (or no parent)
            if (data.parentName.empty()) {
                sortedIndices.push_back(i);
                added[i] = true;
                progress = true;
            } else {
                auto parentIt = nameToIndex.find(data.parentName);
                if (parentIt != nameToIndex.end() && added[parentIt->second]) {
                    sortedIndices.push_back(i);
                    added[i] = true;
                    progress = true;
                }
            }
        }

        // If no progress was made, we have a cycle or missing parent
        if (!progress) {
            // Add remaining bones as root bones
            for (size_t i = 0; i < m_boneData.size(); ++i) {
                if (!added[i]) {
                    sortedIndices.push_back(i);
                    added[i] = true;
                }
            }
            break;
        }
    }

    // Build index remapping
    std::unordered_map<std::string, int> newNameToIndex;
    for (size_t newIdx = 0; newIdx < sortedIndices.size(); ++newIdx) {
        newNameToIndex[m_boneData[sortedIndices[newIdx]].name] = static_cast<int>(newIdx);
    }

    // Add bones in sorted order
    for (size_t sortedIdx : sortedIndices) {
        const auto& data = m_boneData[sortedIdx];

        Bone bone;
        bone.name = data.name;
        bone.offsetMatrix = data.offsetMatrix;
        bone.localTransform = data.localTransform;

        // Find parent index in new ordering
        if (!data.parentName.empty()) {
            auto it = newNameToIndex.find(data.parentName);
            bone.parentIndex = (it != newNameToIndex.end()) ? it->second : -1;
        } else {
            bone.parentIndex = -1;
        }

        skeleton.AddBone(std::move(bone));
    }

    return skeleton;
}

} // namespace Nova
