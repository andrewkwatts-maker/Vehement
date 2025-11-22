#include "animation/Skeleton.hpp"

namespace Nova {

void Skeleton::AddBone(const Bone& bone) {
    m_boneMap[bone.name] = static_cast<int>(m_bones.size());
    m_bones.push_back(bone);
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

std::vector<glm::mat4> Skeleton::CalculateBoneMatrices(
    const std::unordered_map<std::string, glm::mat4>& animationTransforms) const {

    std::vector<glm::mat4> globalTransforms(m_bones.size());
    std::vector<glm::mat4> finalMatrices(m_bones.size());

    for (size_t i = 0; i < m_bones.size(); ++i) {
        const Bone& bone = m_bones[i];

        // Get animation transform or use identity
        glm::mat4 localTransform = bone.localTransform;
        auto it = animationTransforms.find(bone.name);
        if (it != animationTransforms.end()) {
            localTransform = it->second;
        }

        // Calculate global transform
        if (bone.parentIndex >= 0) {
            globalTransforms[i] = globalTransforms[bone.parentIndex] * localTransform;
        } else {
            globalTransforms[i] = localTransform;
        }

        // Calculate final matrix
        finalMatrices[i] = m_globalInverse * globalTransforms[i] * bone.offsetMatrix;
    }

    return finalMatrices;
}

} // namespace Nova
