#include "AnimationBlendTree.hpp"
#include "Animation.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>

namespace Nova {

// ============================================================================
// AnimationMask
// ============================================================================

json AnimationMask::ToJson() const {
    json j = {
        {"id", id},
        {"name", name}
    };

    if (!includedBones.empty()) {
        j["includedBones"] = includedBones;
    }
    if (!excludedBones.empty()) {
        j["excludedBones"] = excludedBones;
    }
    if (!boneWeights.empty()) {
        j["boneWeights"] = json::object();
        for (const auto& [bone, weight] : boneWeights) {
            j["boneWeights"][bone] = weight;
        }
    }

    return j;
}

AnimationMask AnimationMask::FromJson(const json& j) {
    AnimationMask mask;
    mask.id = j.value("id", "");
    mask.name = j.value("name", "");

    if (j.contains("includedBones") && j["includedBones"].is_array()) {
        mask.includedBones = j["includedBones"].get<std::vector<std::string>>();
    }
    if (j.contains("excludedBones") && j["excludedBones"].is_array()) {
        mask.excludedBones = j["excludedBones"].get<std::vector<std::string>>();
    }
    if (j.contains("boneWeights") && j["boneWeights"].is_object()) {
        for (const auto& [bone, weight] : j["boneWeights"].items()) {
            mask.boneWeights[bone] = weight.get<float>();
        }
    }

    return mask;
}

float AnimationMask::GetBoneWeight(const std::string& boneName) const {
    // Check explicit weight
    auto it = boneWeights.find(boneName);
    if (it != boneWeights.end()) {
        return it->second;
    }

    // Check exclusion list
    if (std::find(excludedBones.begin(), excludedBones.end(), boneName) != excludedBones.end()) {
        return 0.0f;
    }

    // Check inclusion list
    if (!includedBones.empty()) {
        if (std::find(includedBones.begin(), includedBones.end(), boneName) != includedBones.end()) {
            return 1.0f;
        }
        return 0.0f;
    }

    return 1.0f;  // Default to full weight
}

// ============================================================================
// IKBlendConfig
// ============================================================================

json IKBlendConfig::ToJson() const {
    return json{
        {"targetBone", targetBone},
        {"positionWeight", positionWeight},
        {"rotationWeight", rotationWeight},
        {"hintWeight", hintWeight},
        {"hintBone", hintBone}
    };
}

IKBlendConfig IKBlendConfig::FromJson(const json& j) {
    IKBlendConfig config;
    config.targetBone = j.value("targetBone", "");
    config.positionWeight = j.value("positionWeight", 1.0f);
    config.rotationWeight = j.value("rotationWeight", 1.0f);
    config.hintWeight = j.value("hintWeight", 0.0f);
    config.hintBone = j.value("hintBone", "");
    return config;
}

// ============================================================================
// BlendTreeChild
// ============================================================================

json BlendTreeChild::ToJson() const {
    json j;

    if (!clipName.empty()) {
        j["clip"] = clipName;
    }
    if (subTree) {
        j["blendTree"] = subTree->ToJson();
    }

    j["threshold"] = threshold;
    j["position"] = {{"x", position.x}, {"y", position.y}};

    if (!directBlendParameter.empty()) {
        j["directBlendParameter"] = directBlendParameter;
    }

    if (timeScale != 1.0f) j["timeScale"] = timeScale;
    if (cycleOffset != 0.0f) j["cycleOffset"] = cycleOffset;
    if (mirror) j["mirror"] = true;

    return j;
}

BlendTreeChild BlendTreeChild::FromJson(const json& j) {
    BlendTreeChild child;
    child.clipName = j.value("clip", "");
    child.threshold = j.value("threshold", 0.0f);
    child.directBlendParameter = j.value("directBlendParameter", "");
    child.timeScale = j.value("timeScale", 1.0f);
    child.cycleOffset = j.value("cycleOffset", 0.0f);
    child.mirror = j.value("mirror", false);

    if (j.contains("position")) {
        if (j["position"].is_array()) {
            child.position.x = j["position"][0].get<float>();
            child.position.y = j["position"][1].get<float>();
        } else {
            child.position.x = j["position"].value("x", 0.0f);
            child.position.y = j["position"].value("y", 0.0f);
        }
    }

    if (j.contains("blendTree")) {
        child.subTree = std::make_unique<BlendTree>();
        child.subTree->LoadFromJson(j["blendTree"]);
    }

    return child;
}

// ============================================================================
// BlendTree
// ============================================================================

BlendTree::BlendTree(const std::string& id) : m_id(id) {}

bool BlendTree::LoadFromJson(const json& config) {
    try {
        m_id = config.value("id", m_id);

        // Parse type
        std::string typeStr = config.value("type", "simple_1d");
        if (typeStr == "simple_1d") m_type = BlendTreeType::Simple1D;
        else if (typeStr == "simple_2d") m_type = BlendTreeType::Simple2D;
        else if (typeStr == "freeform_2d") m_type = BlendTreeType::Freeform2D;
        else if (typeStr == "direct") m_type = BlendTreeType::Direct;
        else if (typeStr == "additive") m_type = BlendTreeType::Additive;

        m_parameter = config.value("parameter", "");
        m_parameterX = config.value("parameterX", "");
        m_parameterY = config.value("parameterY", "");
        m_normalizeBlendValues = config.value("normalizeBlendValues", true);

        // Load children
        m_children.clear();
        if (config.contains("children") && config["children"].is_array()) {
            for (const auto& c : config["children"]) {
                m_children.push_back(BlendTreeChild::FromJson(c));
            }
        }

        // Load mask
        if (config.contains("mask")) {
            m_mask = AnimationMask::FromJson(config["mask"]);
        }

        m_additiveReferencePose = config.value("additiveReferencePose", "");

        // Load IK configs
        m_ikConfigs.clear();
        if (config.contains("ikConfigs") && config["ikConfigs"].is_array()) {
            for (const auto& ik : config["ikConfigs"]) {
                m_ikConfigs.push_back(IKBlendConfig::FromJson(ik));
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

json BlendTree::ToJson() const {
    std::string typeStr;
    switch (m_type) {
        case BlendTreeType::Simple1D: typeStr = "simple_1d"; break;
        case BlendTreeType::Simple2D: typeStr = "simple_2d"; break;
        case BlendTreeType::Freeform2D: typeStr = "freeform_2d"; break;
        case BlendTreeType::Direct: typeStr = "direct"; break;
        case BlendTreeType::Additive: typeStr = "additive"; break;
    }

    json j = {
        {"id", m_id},
        {"type", typeStr},
        {"normalizeBlendValues", m_normalizeBlendValues}
    };

    if (!m_parameter.empty()) j["parameter"] = m_parameter;
    if (!m_parameterX.empty()) j["parameterX"] = m_parameterX;
    if (!m_parameterY.empty()) j["parameterY"] = m_parameterY;

    j["children"] = json::array();
    for (const auto& child : m_children) {
        j["children"].push_back(child.ToJson());
    }

    if (!m_mask.id.empty()) {
        j["mask"] = m_mask.ToJson();
    }

    if (!m_additiveReferencePose.empty()) {
        j["additiveReferencePose"] = m_additiveReferencePose;
    }

    if (!m_ikConfigs.empty()) {
        j["ikConfigs"] = json::array();
        for (const auto& ik : m_ikConfigs) {
            j["ikConfigs"].push_back(ik.ToJson());
        }
    }

    return j;
}

void BlendTree::AddChild(BlendTreeChild&& child) {
    m_children.push_back(std::move(child));
}

void BlendTree::AddClip1D(const std::string& clipName, float threshold) {
    BlendTreeChild child;
    child.clipName = clipName;
    child.threshold = threshold;
    m_children.push_back(std::move(child));
}

void BlendTree::AddClip2D(const std::string& clipName, const glm::vec2& position) {
    BlendTreeChild child;
    child.clipName = clipName;
    child.position = position;
    m_children.push_back(std::move(child));
}

void BlendTree::RemoveChild(size_t index) {
    if (index < m_children.size()) {
        m_children.erase(m_children.begin() + static_cast<ptrdiff_t>(index));
    }
}

void BlendTree::Update(const std::unordered_map<std::string, float>& parameters, float deltaTime) {
    CalculateWeights(parameters);
    m_lastUpdateTime += deltaTime;

    // Update child normalized times
    for (auto& child : m_children) {
        if (child.currentWeight > 0.001f) {
            child.normalizedTime += deltaTime * child.timeScale;
            child.normalizedTime = std::fmod(child.normalizedTime, 1.0f);
        }
    }
}

void BlendTree::CalculateWeights(const std::unordered_map<std::string, float>& parameters) {
    // Reset all weights
    for (auto& child : m_children) {
        child.currentWeight = 0.0f;
    }

    if (m_children.empty()) {
        return;
    }

    switch (m_type) {
        case BlendTreeType::Simple1D: {
            float paramValue = 0.0f;
            auto it = parameters.find(m_parameter);
            if (it != parameters.end()) {
                paramValue = it->second;
            }
            Calculate1DWeights(paramValue);
            break;
        }
        case BlendTreeType::Simple2D:
        case BlendTreeType::Freeform2D: {
            glm::vec2 paramValue{0.0f};
            auto itX = parameters.find(m_parameterX);
            auto itY = parameters.find(m_parameterY);
            if (itX != parameters.end()) paramValue.x = itX->second;
            if (itY != parameters.end()) paramValue.y = itY->second;

            if (m_type == BlendTreeType::Simple2D) {
                Calculate2DWeights(paramValue);
            } else {
                CalculateFreeform2DWeights(paramValue);
            }
            break;
        }
        case BlendTreeType::Direct:
            CalculateDirectWeights(parameters);
            break;
        case BlendTreeType::Additive:
            // All additive children get full weight
            for (auto& child : m_children) {
                child.currentWeight = 1.0f;
            }
            break;
    }

    // Normalize if enabled
    if (m_normalizeBlendValues) {
        float totalWeight = 0.0f;
        for (const auto& child : m_children) {
            totalWeight += child.currentWeight;
        }
        if (totalWeight > 0.001f) {
            for (auto& child : m_children) {
                child.currentWeight /= totalWeight;
            }
        }
    }
}

void BlendTree::Calculate1DWeights(float paramValue) {
    if (m_children.size() == 1) {
        m_children[0].currentWeight = 1.0f;
        return;
    }

    // Sort children by threshold
    std::vector<size_t> indices(m_children.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
        return m_children[a].threshold < m_children[b].threshold;
    });

    // Find the two clips to blend between
    for (size_t i = 0; i < indices.size() - 1; ++i) {
        size_t lowIdx = indices[i];
        size_t highIdx = indices[i + 1];

        float lowThreshold = m_children[lowIdx].threshold;
        float highThreshold = m_children[highIdx].threshold;

        if (paramValue >= lowThreshold && paramValue <= highThreshold) {
            float range = highThreshold - lowThreshold;
            if (range > 0.0001f) {
                float t = (paramValue - lowThreshold) / range;
                m_children[lowIdx].currentWeight = 1.0f - t;
                m_children[highIdx].currentWeight = t;
            } else {
                m_children[lowIdx].currentWeight = 0.5f;
                m_children[highIdx].currentWeight = 0.5f;
            }
            return;
        }
    }

    // Parameter is outside the range - use endpoint
    if (paramValue < m_children[indices.front()].threshold) {
        m_children[indices.front()].currentWeight = 1.0f;
    } else {
        m_children[indices.back()].currentWeight = 1.0f;
    }
}

void BlendTree::Calculate2DWeights(const glm::vec2& paramValue) {
    // Simple bilinear interpolation for 2D blend
    // Find the 4 closest points forming a quad

    if (m_children.size() < 4) {
        // Fall back to inverse distance weighting
        CalculateFreeform2DWeights(paramValue);
        return;
    }

    // Use inverse distance weighting for simplicity
    float totalWeight = 0.0f;
    constexpr float minDistance = 0.001f;

    for (auto& child : m_children) {
        float dist = glm::length(paramValue - child.position);
        if (dist < minDistance) {
            // Very close to this point
            child.currentWeight = 1.0f;
            for (auto& other : m_children) {
                if (&other != &child) {
                    other.currentWeight = 0.0f;
                }
            }
            return;
        }
        child.currentWeight = 1.0f / (dist * dist);
        totalWeight += child.currentWeight;
    }

    // Normalize
    if (totalWeight > 0.0f) {
        for (auto& child : m_children) {
            child.currentWeight /= totalWeight;
        }
    }
}

void BlendTree::CalculateFreeform2DWeights(const glm::vec2& paramValue) {
    if (m_children.empty()) return;

    if (m_children.size() == 1) {
        m_children[0].currentWeight = 1.0f;
        return;
    }

    // Collect all positions
    std::vector<glm::vec2> positions;
    for (const auto& child : m_children) {
        positions.push_back(child.position);
    }

    // Calculate weights using gradient band interpolation
    for (size_t i = 0; i < m_children.size(); ++i) {
        m_children[i].currentWeight = CalculateGradientBandWeight(
            paramValue, m_children[i].position, positions);
    }
}

float BlendTree::CalculateGradientBandWeight(const glm::vec2& point,
                                              const glm::vec2& samplePoint,
                                              const std::vector<glm::vec2>& allPoints) const {
    float minWeight = std::numeric_limits<float>::max();

    for (const auto& otherPoint : allPoints) {
        if (glm::length(otherPoint - samplePoint) < 0.0001f) {
            continue;
        }

        // Calculate weight based on the perpendicular bisector
        glm::vec2 midpoint = (samplePoint + otherPoint) * 0.5f;
        glm::vec2 direction = glm::normalize(otherPoint - samplePoint);

        float distToSample = glm::length(point - samplePoint);
        float distToOther = glm::length(point - otherPoint);

        if (distToSample + distToOther < 0.0001f) {
            continue;
        }

        float weight = 1.0f - (distToSample / (distToSample + distToOther));
        minWeight = std::min(minWeight, weight);
    }

    return std::max(0.0f, minWeight);
}

void BlendTree::CalculateDirectWeights(const std::unordered_map<std::string, float>& parameters) {
    for (auto& child : m_children) {
        if (!child.directBlendParameter.empty()) {
            auto it = parameters.find(child.directBlendParameter);
            if (it != parameters.end()) {
                child.currentWeight = std::clamp(it->second, 0.0f, 1.0f);
            }
        }
    }
}

std::vector<std::pair<std::string, float>> BlendTree::GetBlendWeights() const {
    std::vector<std::pair<std::string, float>> weights;
    for (const auto& child : m_children) {
        if (child.currentWeight > 0.001f) {
            weights.emplace_back(child.clipName, child.currentWeight);
        }
    }
    return weights;
}

std::unordered_map<std::string, glm::mat4> BlendTree::GetBlendedTransforms(
    float time,
    const std::unordered_map<std::string, Animation*>& animations) const {

    std::unordered_map<std::string, glm::mat4> result;
    bool firstClip = true;

    for (const auto& child : m_children) {
        if (child.currentWeight < 0.001f) {
            continue;
        }

        auto it = animations.find(child.clipName);
        if (it == animations.end() || !it->second) {
            continue;
        }

        float childTime = time * child.timeScale + child.cycleOffset;
        auto transforms = it->second->Evaluate(childTime);

        for (const auto& [boneName, transform] : transforms) {
            float boneWeight = m_mask.id.empty() ? 1.0f : m_mask.GetBoneWeight(boneName);
            float effectiveWeight = child.currentWeight * boneWeight;

            if (effectiveWeight < 0.001f) {
                continue;
            }

            if (firstClip || result.find(boneName) == result.end()) {
                result[boneName] = transform;
            } else {
                // Blend transforms
                result[boneName] = BlendTransforms(result[boneName], transform, effectiveWeight);
            }
        }

        firstClip = false;
    }

    return result;
}

json BlendTree::GetDebugInfo() const {
    json info;
    info["id"] = m_id;
    info["type"] = static_cast<int>(m_type);
    info["childCount"] = m_children.size();

    info["children"] = json::array();
    for (const auto& child : m_children) {
        info["children"].push_back({
            {"clip", child.clipName},
            {"weight", child.currentWeight},
            {"threshold", child.threshold},
            {"position", {child.position.x, child.position.y}}
        });
    }

    return info;
}

// ============================================================================
// BlendTreeManager
// ============================================================================

BlendTree* BlendTreeManager::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return nullptr;
        }

        json config;
        file >> config;

        std::string id = config.value("id", filepath);
        return CreateFromJson(id, config);
    } catch (...) {
        return nullptr;
    }
}

BlendTree* BlendTreeManager::CreateFromJson(const std::string& id, const json& config) {
    auto tree = std::make_unique<BlendTree>(id);
    if (!tree->LoadFromJson(config)) {
        return nullptr;
    }

    auto* ptr = tree.get();
    m_blendTrees[id] = std::move(tree);
    return ptr;
}

BlendTree* BlendTreeManager::Get(const std::string& id) {
    auto it = m_blendTrees.find(id);
    return it != m_blendTrees.end() ? it->second.get() : nullptr;
}

const BlendTree* BlendTreeManager::Get(const std::string& id) const {
    auto it = m_blendTrees.find(id);
    return it != m_blendTrees.end() ? it->second.get() : nullptr;
}

bool BlendTreeManager::Remove(const std::string& id) {
    return m_blendTrees.erase(id) > 0;
}

std::vector<std::string> BlendTreeManager::GetAllIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_blendTrees.size());
    for (const auto& [id, tree] : m_blendTrees) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// BlendTreeTemplates
// ============================================================================

json BlendTreeTemplates::CreateLocomotion1D(
    const std::string& idleClip,
    const std::string& walkClip,
    const std::string& runClip,
    const std::string& speedParameter) {

    return json{
        {"type", "simple_1d"},
        {"parameter", speedParameter},
        {"normalizeBlendValues", true},
        {"children", json::array({
            {{"clip", idleClip}, {"threshold", 0.0f}},
            {{"clip", walkClip}, {"threshold", 0.5f}},
            {{"clip", runClip}, {"threshold", 1.0f}}
        })}
    };
}

json BlendTreeTemplates::CreateDirectional2D(
    const std::string& forwardClip,
    const std::string& backwardClip,
    const std::string& leftClip,
    const std::string& rightClip,
    const std::string& xParameter,
    const std::string& yParameter) {

    return json{
        {"type", "simple_2d"},
        {"parameterX", xParameter},
        {"parameterY", yParameter},
        {"normalizeBlendValues", true},
        {"children", json::array({
            {{"clip", forwardClip}, {"position", {{"x", 0.0f}, {"y", 1.0f}}}},
            {{"clip", backwardClip}, {"position", {{"x", 0.0f}, {"y", -1.0f}}}},
            {{"clip", leftClip}, {"position", {{"x", -1.0f}, {"y", 0.0f}}}},
            {{"clip", rightClip}, {"position", {{"x", 1.0f}, {"y", 0.0f}}}}
        })}
    };
}

json BlendTreeTemplates::CreateStrafe8Way(
    const std::unordered_map<std::string, std::string>& directionClips,
    const std::string& xParameter,
    const std::string& yParameter) {

    json children = json::array();

    // Define 8 directions with their positions
    struct Direction {
        const char* name;
        float x, y;
    };

    const Direction directions[] = {
        {"forward", 0.0f, 1.0f},
        {"forward_right", 0.707f, 0.707f},
        {"right", 1.0f, 0.0f},
        {"backward_right", 0.707f, -0.707f},
        {"backward", 0.0f, -1.0f},
        {"backward_left", -0.707f, -0.707f},
        {"left", -1.0f, 0.0f},
        {"forward_left", -0.707f, 0.707f}
    };

    for (const auto& dir : directions) {
        auto it = directionClips.find(dir.name);
        if (it != directionClips.end()) {
            children.push_back({
                {"clip", it->second},
                {"position", {{"x", dir.x}, {"y", dir.y}}}
            });
        }
    }

    return json{
        {"type", "freeform_2d"},
        {"parameterX", xParameter},
        {"parameterY", yParameter},
        {"normalizeBlendValues", true},
        {"children", children}
    };
}

json BlendTreeTemplates::CreateAdditiveLean(
    const std::string& neutralClip,
    const std::string& leanLeftClip,
    const std::string& leanRightClip,
    const std::string& leanParameter) {

    return json{
        {"type", "simple_1d"},
        {"parameter", leanParameter},
        {"normalizeBlendValues", true},
        {"additiveReferencePose", neutralClip},
        {"children", json::array({
            {{"clip", leanLeftClip}, {"threshold", -1.0f}},
            {{"clip", neutralClip}, {"threshold", 0.0f}},
            {{"clip", leanRightClip}, {"threshold", 1.0f}}
        })}
    };
}

} // namespace Nova
