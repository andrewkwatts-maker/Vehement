#include "BlendMask.hpp"
#include "../Skeleton.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>

namespace Nova {

// Standard humanoid bone names
const char* BlendMask::HumanoidBones::Hips = "Hips";
const char* BlendMask::HumanoidBones::Spine = "Spine";
const char* BlendMask::HumanoidBones::Spine1 = "Spine1";
const char* BlendMask::HumanoidBones::Spine2 = "Spine2";
const char* BlendMask::HumanoidBones::Neck = "Neck";
const char* BlendMask::HumanoidBones::Head = "Head";

const char* BlendMask::HumanoidBones::LeftShoulder = "LeftShoulder";
const char* BlendMask::HumanoidBones::LeftUpperArm = "LeftUpperArm";
const char* BlendMask::HumanoidBones::LeftLowerArm = "LeftLowerArm";
const char* BlendMask::HumanoidBones::LeftHand = "LeftHand";

const char* BlendMask::HumanoidBones::RightShoulder = "RightShoulder";
const char* BlendMask::HumanoidBones::RightUpperArm = "RightUpperArm";
const char* BlendMask::HumanoidBones::RightLowerArm = "RightLowerArm";
const char* BlendMask::HumanoidBones::RightHand = "RightHand";

const char* BlendMask::HumanoidBones::LeftUpperLeg = "LeftUpperLeg";
const char* BlendMask::HumanoidBones::LeftLowerLeg = "LeftLowerLeg";
const char* BlendMask::HumanoidBones::LeftFoot = "LeftFoot";
const char* BlendMask::HumanoidBones::LeftToes = "LeftToes";

const char* BlendMask::HumanoidBones::RightUpperLeg = "RightUpperLeg";
const char* BlendMask::HumanoidBones::RightLowerLeg = "RightLowerLeg";
const char* BlendMask::HumanoidBones::RightFoot = "RightFoot";
const char* BlendMask::HumanoidBones::RightToes = "RightToes";

BlendMask::BlendMask(const std::string& name)
    : m_name(name) {
}

BlendMask::BlendMask(Preset preset)
    : m_preset(preset) {
    m_name = GetPresetName(preset);
}

void BlendMask::SetSkeleton(Skeleton* skeleton) {
    m_skeleton = skeleton;
    m_dirty = true;

    if (skeleton) {
        m_weights.resize(skeleton->GetBoneCount(), 0.0f);

        // Update bone indices
        for (auto& bw : m_boneWeights) {
            bw.boneIndex = skeleton->GetBoneIndex(bw.boneName);
        }

        // Apply preset if set
        if (m_preset != Preset::Custom) {
            ApplyPreset(m_preset);
        }

        RebuildWeights();
    }
}

void BlendMask::SetBoneWeight(const std::string& boneName, float weight, bool includeChildren) {
    weight = std::clamp(weight, 0.0f, 1.0f);

    // Find or create bone weight entry
    auto it = m_boneNameToWeight.find(boneName);
    if (it != m_boneNameToWeight.end()) {
        m_boneWeights[it->second].weight = weight;
        m_boneWeights[it->second].includeChildren = includeChildren;
    } else {
        BoneWeight bw;
        bw.boneName = boneName;
        bw.weight = weight;
        bw.includeChildren = includeChildren;
        if (m_skeleton) {
            bw.boneIndex = m_skeleton->GetBoneIndex(boneName);
        }
        m_boneNameToWeight[boneName] = m_boneWeights.size();
        m_boneWeights.push_back(bw);
    }

    m_dirty = true;
}

void BlendMask::SetBoneWeight(size_t boneIndex, float weight) {
    if (boneIndex < m_weights.size()) {
        m_weights[boneIndex] = std::clamp(weight, 0.0f, 1.0f);
    }
}

float BlendMask::GetBoneWeight(const std::string& boneName) const {
    auto it = m_boneNameToWeight.find(boneName);
    if (it != m_boneNameToWeight.end()) {
        return m_boneWeights[it->second].weight;
    }

    if (m_skeleton) {
        int index = m_skeleton->GetBoneIndex(boneName);
        if (index >= 0 && static_cast<size_t>(index) < m_weights.size()) {
            return m_weights[index];
        }
    }

    return 0.0f;
}

float BlendMask::GetBoneWeight(size_t boneIndex) const {
    if (boneIndex < m_weights.size()) {
        return m_weights[boneIndex];
    }
    return 0.0f;
}

bool BlendMask::IsBoneMasked(const std::string& boneName) const {
    return GetBoneWeight(boneName) > 0.001f;
}

bool BlendMask::IsBoneMasked(size_t boneIndex) const {
    return GetBoneWeight(boneIndex) > 0.001f;
}

void BlendMask::ClearWeights() {
    std::fill(m_weights.begin(), m_weights.end(), 0.0f);
    m_boneWeights.clear();
    m_boneNameToWeight.clear();
    m_dirty = false;
}

void BlendMask::SetAllWeights(float weight) {
    weight = std::clamp(weight, 0.0f, 1.0f);
    std::fill(m_weights.begin(), m_weights.end(), weight);
    m_dirty = false;
}

void BlendMask::ApplyPreset(Preset preset) {
    m_preset = preset;
    ClearWeights();

    if (!m_skeleton) return;

    switch (preset) {
        case Preset::FullBody:
            SetAllWeights(1.0f);
            break;

        case Preset::UpperBody:
            SetBranchWeight(HumanoidBones::Spine, 1.0f);
            break;

        case Preset::LowerBody:
            SetBranchWeight(HumanoidBones::LeftUpperLeg, 1.0f);
            SetBranchWeight(HumanoidBones::RightUpperLeg, 1.0f);
            break;

        case Preset::LeftArm:
            SetBranchWeight(HumanoidBones::LeftShoulder, 1.0f);
            break;

        case Preset::RightArm:
            SetBranchWeight(HumanoidBones::RightShoulder, 1.0f);
            break;

        case Preset::LeftLeg:
            SetBranchWeight(HumanoidBones::LeftUpperLeg, 1.0f);
            break;

        case Preset::RightLeg:
            SetBranchWeight(HumanoidBones::RightUpperLeg, 1.0f);
            break;

        case Preset::Head:
            SetBranchWeight(HumanoidBones::Neck, 1.0f);
            break;

        case Preset::Spine:
            SetBoneWeight(HumanoidBones::Spine, 1.0f, false);
            SetBoneWeight(HumanoidBones::Spine1, 1.0f, false);
            SetBoneWeight(HumanoidBones::Spine2, 1.0f, false);
            break;

        case Preset::Hands:
            SetBranchWeight(HumanoidBones::LeftHand, 1.0f);
            SetBranchWeight(HumanoidBones::RightHand, 1.0f);
            break;

        case Preset::Feet:
            SetBranchWeight(HumanoidBones::LeftFoot, 1.0f);
            SetBranchWeight(HumanoidBones::RightFoot, 1.0f);
            break;

        case Preset::Arms:
            SetBranchWeight(HumanoidBones::LeftShoulder, 1.0f);
            SetBranchWeight(HumanoidBones::RightShoulder, 1.0f);
            break;

        case Preset::Legs:
            SetBranchWeight(HumanoidBones::LeftUpperLeg, 1.0f);
            SetBranchWeight(HumanoidBones::RightUpperLeg, 1.0f);
            break;

        case Preset::Custom:
        default:
            break;
    }

    RebuildWeights();
}

std::shared_ptr<BlendMask> BlendMask::CreateFromPreset(Preset preset, Skeleton* skeleton) {
    auto mask = std::make_shared<BlendMask>(preset);
    if (skeleton) {
        mask->SetSkeleton(skeleton);
    }
    return mask;
}

const char* BlendMask::GetPresetName(Preset preset) {
    switch (preset) {
        case Preset::FullBody: return "Full Body";
        case Preset::UpperBody: return "Upper Body";
        case Preset::LowerBody: return "Lower Body";
        case Preset::LeftArm: return "Left Arm";
        case Preset::RightArm: return "Right Arm";
        case Preset::LeftLeg: return "Left Leg";
        case Preset::RightLeg: return "Right Leg";
        case Preset::Head: return "Head";
        case Preset::Spine: return "Spine";
        case Preset::Hands: return "Hands";
        case Preset::Feet: return "Feet";
        case Preset::Arms: return "Arms";
        case Preset::Legs: return "Legs";
        case Preset::Custom: return "Custom";
        default: return "Unknown";
    }
}

std::vector<BlendMask::Preset> BlendMask::GetAvailablePresets() {
    return {
        Preset::FullBody,
        Preset::UpperBody,
        Preset::LowerBody,
        Preset::LeftArm,
        Preset::RightArm,
        Preset::LeftLeg,
        Preset::RightLeg,
        Preset::Head,
        Preset::Spine,
        Preset::Hands,
        Preset::Feet,
        Preset::Arms,
        Preset::Legs,
        Preset::Custom
    };
}

void BlendMask::SetBranchWeight(const std::string& rootBone, float weight) {
    if (!m_skeleton) return;

    int rootIndex = m_skeleton->GetBoneIndex(rootBone);
    if (rootIndex < 0) return;

    PropagateToChildren(rootIndex, weight);
}

void BlendMask::PropagateToChildren(int boneIndex, float weight) {
    if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= m_weights.size()) return;

    m_weights[boneIndex] = weight;

    // Find and propagate to children
    const auto& bones = m_skeleton->GetBones();
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == boneIndex) {
            PropagateToChildren(static_cast<int>(i), weight);
        }
    }
}

void BlendMask::AddFeathering(const std::string& startBone, int levels,
                               float startWeight, float endWeight) {
    if (!m_skeleton) return;

    int boneIndex = m_skeleton->GetBoneIndex(startBone);
    if (boneIndex < 0) return;

    const auto& bones = m_skeleton->GetBones();

    // Apply feathering along parent chain
    for (int i = 0; i <= levels && boneIndex >= 0; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(levels);
        float weight = startWeight + (endWeight - startWeight) * t;
        m_weights[boneIndex] = weight;

        boneIndex = bones[boneIndex].parentIndex;
    }
}

void BlendMask::Mirror() {
    if (!m_skeleton) return;

    std::vector<float> mirrored = m_weights;

    // Common left/right bone name patterns
    auto mirrorName = [](const std::string& name) -> std::string {
        std::string result = name;

        // Try different patterns
        size_t pos = result.find("Left");
        if (pos != std::string::npos) {
            result.replace(pos, 4, "Right");
            return result;
        }

        pos = result.find("Right");
        if (pos != std::string::npos) {
            result.replace(pos, 5, "Left");
            return result;
        }

        pos = result.find("_L_");
        if (pos != std::string::npos) {
            result.replace(pos, 3, "_R_");
            return result;
        }

        pos = result.find("_R_");
        if (pos != std::string::npos) {
            result.replace(pos, 3, "_L_");
            return result;
        }

        if (result.size() >= 2) {
            if (result.substr(result.size() - 2) == "_L") {
                result = result.substr(0, result.size() - 2) + "_R";
                return result;
            }
            if (result.substr(result.size() - 2) == "_R") {
                result = result.substr(0, result.size() - 2) + "_L";
                return result;
            }
        }

        return result;
    };

    const auto& bones = m_skeleton->GetBones();
    for (size_t i = 0; i < bones.size(); ++i) {
        std::string mirroredName = mirrorName(bones[i].name);
        int mirroredIndex = m_skeleton->GetBoneIndex(mirroredName);

        if (mirroredIndex >= 0 && mirroredIndex != static_cast<int>(i)) {
            mirrored[mirroredIndex] = m_weights[i];
        }
    }

    m_weights = mirrored;
}

std::shared_ptr<BlendMask> BlendMask::Blend(const BlendMask& a, const BlendMask& b, float t) {
    auto result = std::make_shared<BlendMask>("Blended");
    result->m_skeleton = a.m_skeleton ? a.m_skeleton : b.m_skeleton;

    size_t size = std::max(a.m_weights.size(), b.m_weights.size());
    result->m_weights.resize(size);

    for (size_t i = 0; i < size; ++i) {
        float wa = i < a.m_weights.size() ? a.m_weights[i] : 0.0f;
        float wb = i < b.m_weights.size() ? b.m_weights[i] : 0.0f;
        result->m_weights[i] = wa + (wb - wa) * t;
    }

    result->m_dirty = false;
    return result;
}

void BlendMask::Multiply(float factor) {
    for (float& w : m_weights) {
        w = std::clamp(w * factor, 0.0f, 1.0f);
    }
}

void BlendMask::Add(const BlendMask& other, float weight) {
    size_t size = std::min(m_weights.size(), other.m_weights.size());
    for (size_t i = 0; i < size; ++i) {
        m_weights[i] = std::clamp(m_weights[i] + other.m_weights[i] * weight, 0.0f, 1.0f);
    }
}

void BlendMask::Invert() {
    for (float& w : m_weights) {
        w = 1.0f - w;
    }
}

std::string BlendMask::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"preset\": \"" << GetPresetName(m_preset) << "\",\n";
    ss << "  \"bones\": [\n";

    for (size_t i = 0; i < m_boneWeights.size(); ++i) {
        const auto& bw = m_boneWeights[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << bw.boneName << "\",\n";
        ss << "      \"weight\": " << bw.weight << ",\n";
        ss << "      \"includeChildren\": " << (bw.includeChildren ? "true" : "false") << "\n";
        ss << "    }" << (i < m_boneWeights.size() - 1 ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

bool BlendMask::FromJson(const std::string& json) {
    // Simplified parsing - in production use proper JSON library
    return true;
}

void BlendMask::RebuildWeights() {
    if (!m_skeleton) return;

    m_weights.resize(m_skeleton->GetBoneCount(), 0.0f);

    // Apply bone weights
    for (const auto& bw : m_boneWeights) {
        if (bw.boneIndex >= 0 && static_cast<size_t>(bw.boneIndex) < m_weights.size()) {
            if (bw.includeChildren) {
                PropagateToChildren(bw.boneIndex, bw.weight);
            } else {
                m_weights[bw.boneIndex] = bw.weight;
            }
        }
    }

    m_dirty = false;
}

void BlendMask::AutoMapHumanoid() {
    // Auto-detect humanoid bones by common naming conventions
    if (!m_skeleton) return;

    const auto& bones = m_skeleton->GetBones();

    auto findBone = [&](const std::vector<std::string>& patterns) -> std::string {
        for (const auto& bone : bones) {
            std::string lowerName = bone.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            for (const auto& pattern : patterns) {
                if (lowerName.find(pattern) != std::string::npos) {
                    return bone.name;
                }
            }
        }
        return "";
    };

    // Map standard bones
    // This is a simplified version - real implementation would be more robust
}

// =============================================================================
// BlendMaskLibrary
// =============================================================================

BlendMaskLibrary& BlendMaskLibrary::Instance() {
    static BlendMaskLibrary instance;
    return instance;
}

void BlendMaskLibrary::RegisterMask(const std::string& name, std::shared_ptr<BlendMask> mask) {
    m_masks[name] = mask;
}

std::shared_ptr<BlendMask> BlendMaskLibrary::GetMask(const std::string& name) const {
    auto it = m_masks.find(name);
    return it != m_masks.end() ? it->second : nullptr;
}

bool BlendMaskLibrary::HasMask(const std::string& name) const {
    return m_masks.find(name) != m_masks.end();
}

std::vector<std::string> BlendMaskLibrary::GetMaskNames() const {
    std::vector<std::string> names;
    names.reserve(m_masks.size());
    for (const auto& [name, _] : m_masks) {
        names.push_back(name);
    }
    return names;
}

void BlendMaskLibrary::RemoveMask(const std::string& name) {
    m_masks.erase(name);
}

void BlendMaskLibrary::Clear() {
    m_masks.clear();
}

bool BlendMaskLibrary::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    // Parse JSON and load masks
    // Simplified - would use proper JSON library
    return true;
}

bool BlendMaskLibrary::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n  \"masks\": [\n";

    size_t i = 0;
    for (const auto& [name, mask] : m_masks) {
        file << "    " << mask->ToJson();
        if (++i < m_masks.size()) file << ",";
        file << "\n";
    }

    file << "  ]\n}";
    return true;
}

} // namespace Nova
