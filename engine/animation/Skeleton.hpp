#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Bone in a skeleton hierarchy
 */
struct Bone {
    std::string name;
    int parentIndex = -1;
    glm::mat4 offsetMatrix{1.0f};
    glm::mat4 localTransform{1.0f};
};

/**
 * @brief Skeleton for skeletal animation
 */
class Skeleton {
public:
    Skeleton() = default;

    void AddBone(const Bone& bone);
    Bone* GetBone(const std::string& name);
    const Bone* GetBone(const std::string& name) const;
    int GetBoneIndex(const std::string& name) const;

    const std::vector<Bone>& GetBones() const { return m_bones; }

    /**
     * @brief Calculate final bone matrices for shader
     */
    std::vector<glm::mat4> CalculateBoneMatrices(
        const std::unordered_map<std::string, glm::mat4>& animationTransforms) const;

    const glm::mat4& GetGlobalInverseTransform() const { return m_globalInverse; }
    void SetGlobalInverseTransform(const glm::mat4& transform) { m_globalInverse = transform; }

private:
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, int> m_boneMap;
    glm::mat4 m_globalInverse{1.0f};
};

} // namespace Nova
