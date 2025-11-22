#include "AnimationLayer.hpp"
#include <algorithm>
#include <sstream>

namespace Nova {

// =============================================================================
// AnimationLayer
// =============================================================================

AnimationLayer::AnimationLayer(const std::string& name)
    : m_name(name) {
}

void AnimationLayer::FadeIn(float duration) {
    m_targetWeight = 1.0f;
    m_weightBlendSpeed = duration > 0.0f ? 1.0f / duration : 100.0f;
}

void AnimationLayer::FadeOut(float duration) {
    m_targetWeight = 0.0f;
    m_weightBlendSpeed = duration > 0.0f ? 1.0f / duration : 100.0f;
}

void AnimationLayer::SetParameter(const std::string& name, float value) {
    m_parameters[name] = value;
    if (m_blendTree) {
        m_blendTree->SetParameter(name, value);
    }
}

float AnimationLayer::GetParameter(const std::string& name) const {
    auto it = m_parameters.find(name);
    return it != m_parameters.end() ? it->second : 0.0f;
}

void AnimationLayer::Update(float deltaTime) {
    // Blend weight towards target
    if (std::abs(m_weight - m_targetWeight) > 0.001f) {
        float direction = m_targetWeight > m_weight ? 1.0f : -1.0f;
        m_weight += direction * m_weightBlendSpeed * deltaTime;

        if ((direction > 0 && m_weight >= m_targetWeight) ||
            (direction < 0 && m_weight <= m_targetWeight)) {
            m_weight = m_targetWeight;
            if (OnWeightReachedTarget) {
                OnWeightReachedTarget();
            }
        }
    }
}

AnimationPose AnimationLayer::Evaluate(float deltaTime) {
    if (!m_blendTree || !m_enabled) {
        return AnimationPose{};
    }

    // Propagate parameters
    for (const auto& [name, value] : m_parameters) {
        m_blendTree->SetParameter(name, value);
    }

    return m_blendTree->Evaluate(deltaTime);
}

void AnimationLayer::ApplyToPose(AnimationPose& basePose, const AnimationPose& layerPose) const {
    if (m_weight <= 0.001f) return;

    size_t boneCount = std::min(basePose.GetBoneCount(), layerPose.GetBoneCount());

    for (size_t i = 0; i < boneCount; ++i) {
        float maskWeight = 1.0f;
        if (m_mask && i < m_mask->GetWeights().size()) {
            maskWeight = m_mask->GetWeights()[i];
        }

        float finalWeight = m_weight * maskWeight;
        if (finalWeight <= 0.001f) continue;

        const auto& baseTransform = basePose.GetBoneTransform(i);
        const auto& layerTransform = layerPose.GetBoneTransform(i);

        BoneTransform result;

        switch (m_blendMode) {
            case BlendMode::Override:
                result = BoneTransform::Lerp(baseTransform, layerTransform, finalWeight);
                break;

            case BlendMode::Additive: {
                // Scale additive by weight
                BoneTransform scaled;
                scaled.position = layerTransform.position * finalWeight;
                scaled.rotation = glm::slerp(glm::quat(1, 0, 0, 0), layerTransform.rotation, finalWeight);
                scaled.scale = glm::mix(glm::vec3(1.0f), layerTransform.scale, finalWeight);
                result = BoneTransform::Add(baseTransform, scaled);
                break;
            }

            case BlendMode::Multiply: {
                BoneTransform scaled;
                scaled.position = baseTransform.position * glm::mix(glm::vec3(1.0f), layerTransform.position, finalWeight);
                scaled.rotation = glm::slerp(baseTransform.rotation,
                                             layerTransform.rotation * baseTransform.rotation, finalWeight);
                scaled.scale = baseTransform.scale * glm::mix(glm::vec3(1.0f), layerTransform.scale, finalWeight);
                result = scaled;
                break;
            }
        }

        basePose.SetBoneTransform(i, result);
    }

    // Blend root motion
    basePose.rootMotionDelta = glm::mix(basePose.rootMotionDelta, layerPose.rootMotionDelta, m_weight);
    basePose.rootMotionRotation = glm::slerp(basePose.rootMotionRotation,
                                              layerPose.rootMotionRotation, m_weight);
}

void AnimationLayer::Reset() {
    m_normalizedTime = 0.0f;
    if (m_blendTree) {
        m_blendTree->Reset();
    }
}

// =============================================================================
// AnimationLayerStack
// =============================================================================

size_t AnimationLayerStack::AddLayer(std::unique_ptr<AnimationLayer> layer) {
    if (m_skeleton && layer->GetBlendTree()) {
        layer->GetBlendTree()->SetSkeleton(m_skeleton);
    }
    m_layers.push_back(std::move(layer));
    m_mutedLayers.push_back(false);
    return m_layers.size() - 1;
}

size_t AnimationLayerStack::AddLayer(const std::string& name, std::unique_ptr<BlendNode> tree,
                                      AnimationLayer::BlendMode mode, float weight) {
    auto layer = std::make_unique<AnimationLayer>(name);
    layer->SetBlendTree(std::move(tree));
    layer->SetBlendMode(mode);
    layer->SetWeight(weight);
    return AddLayer(std::move(layer));
}

void AnimationLayerStack::RemoveLayer(size_t index) {
    if (index < m_layers.size()) {
        m_layers.erase(m_layers.begin() + static_cast<ptrdiff_t>(index));
        m_mutedLayers.erase(m_mutedLayers.begin() + static_cast<ptrdiff_t>(index));

        // Update sync group indices
        for (auto& group : m_syncGroups) {
            group.layerIndices.erase(
                std::remove_if(group.layerIndices.begin(), group.layerIndices.end(),
                               [index](size_t i) { return i == index; }),
                group.layerIndices.end());

            for (size_t& i : group.layerIndices) {
                if (i > index) --i;
            }
        }

        if (m_soloLayerIndex >= static_cast<int>(index)) {
            --m_soloLayerIndex;
        }
    }
}

AnimationLayer* AnimationLayerStack::GetLayer(size_t index) {
    return index < m_layers.size() ? m_layers[index].get() : nullptr;
}

const AnimationLayer* AnimationLayerStack::GetLayer(size_t index) const {
    return index < m_layers.size() ? m_layers[index].get() : nullptr;
}

AnimationLayer* AnimationLayerStack::GetLayer(const std::string& name) {
    for (auto& layer : m_layers) {
        if (layer->GetName() == name) {
            return layer.get();
        }
    }
    return nullptr;
}

int AnimationLayerStack::GetLayerIndex(const std::string& name) const {
    for (size_t i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i]->GetName() == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AnimationLayerStack::MoveLayer(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= m_layers.size() || toIndex >= m_layers.size() || fromIndex == toIndex) {
        return;
    }

    auto layer = std::move(m_layers[fromIndex]);
    bool muted = m_mutedLayers[fromIndex];

    m_layers.erase(m_layers.begin() + static_cast<ptrdiff_t>(fromIndex));
    m_mutedLayers.erase(m_mutedLayers.begin() + static_cast<ptrdiff_t>(fromIndex));

    m_layers.insert(m_layers.begin() + static_cast<ptrdiff_t>(toIndex), std::move(layer));
    m_mutedLayers.insert(m_mutedLayers.begin() + static_cast<ptrdiff_t>(toIndex), muted);
}

void AnimationLayerStack::ClearLayers() {
    m_layers.clear();
    m_mutedLayers.clear();
    m_syncGroups.clear();
    m_soloLayerIndex = -1;
}

void AnimationLayerStack::SetBaseLayer(std::unique_ptr<AnimationLayer> layer) {
    if (m_layers.empty()) {
        AddLayer(std::move(layer));
    } else {
        if (m_skeleton && layer->GetBlendTree()) {
            layer->GetBlendTree()->SetSkeleton(m_skeleton);
        }
        m_layers[0] = std::move(layer);
    }
}

AnimationLayer* AnimationLayerStack::GetBaseLayer() {
    return m_layers.empty() ? nullptr : m_layers[0].get();
}

void AnimationLayerStack::CreateSyncGroup(const std::string& name, const std::vector<size_t>& layerIndices) {
    SyncGroup group;
    group.name = name;
    group.layerIndices = layerIndices;
    m_syncGroups.push_back(group);
}

void AnimationLayerStack::AddToSyncGroup(const std::string& groupName, size_t layerIndex) {
    for (auto& group : m_syncGroups) {
        if (group.name == groupName) {
            if (std::find(group.layerIndices.begin(), group.layerIndices.end(), layerIndex) ==
                group.layerIndices.end()) {
                group.layerIndices.push_back(layerIndex);
            }
            return;
        }
    }

    // Create new group
    CreateSyncGroup(groupName, {layerIndex});
}

void AnimationLayerStack::RemoveFromSyncGroup(const std::string& groupName, size_t layerIndex) {
    for (auto& group : m_syncGroups) {
        if (group.name == groupName) {
            group.layerIndices.erase(
                std::remove(group.layerIndices.begin(), group.layerIndices.end(), layerIndex),
                group.layerIndices.end());
            return;
        }
    }
}

AnimationLayerStack::SyncGroup* AnimationLayerStack::GetSyncGroup(const std::string& name) {
    for (auto& group : m_syncGroups) {
        if (group.name == name) {
            return &group;
        }
    }
    return nullptr;
}

void AnimationLayerStack::SetParameter(const std::string& name, float value) {
    m_globalParameters[name] = value;
    for (auto& layer : m_layers) {
        layer->SetParameter(name, value);
    }
}

void AnimationLayerStack::SetLayerParameter(size_t layerIndex, const std::string& name, float value) {
    if (layerIndex < m_layers.size()) {
        m_layers[layerIndex]->SetParameter(name, value);
    }
}

float AnimationLayerStack::GetParameter(const std::string& name) const {
    auto it = m_globalParameters.find(name);
    return it != m_globalParameters.end() ? it->second : 0.0f;
}

void AnimationLayerStack::SetSkeleton(Skeleton* skeleton) {
    m_skeleton = skeleton;
    for (auto& layer : m_layers) {
        if (layer->GetBlendTree()) {
            layer->GetBlendTree()->SetSkeleton(skeleton);
        }
        if (layer->GetMask()) {
            layer->GetMask()->SetSkeleton(skeleton);
        }
    }
}

void AnimationLayerStack::Update(float deltaTime) {
    for (auto& layer : m_layers) {
        layer->Update(deltaTime);
    }
    UpdateSyncGroups(deltaTime);
}

AnimationPose AnimationLayerStack::Evaluate(float deltaTime) {
    if (m_layers.empty() || !m_skeleton) {
        AnimationPose empty;
        empty.Resize(m_skeleton ? m_skeleton->GetBoneCount() : 0);
        return empty;
    }

    // Sync layers if needed
    SyncLayers();

    // Start with base layer
    AnimationPose result;

    // Handle solo mode
    if (m_soloLayerIndex >= 0 && static_cast<size_t>(m_soloLayerIndex) < m_layers.size()) {
        auto& soloLayer = m_layers[m_soloLayerIndex];
        if (soloLayer->IsEnabled()) {
            result = soloLayer->Evaluate(deltaTime);
        }
        return result;
    }

    // Normal evaluation - start with base layer
    if (m_layers[0]->IsEnabled() && !m_mutedLayers[0]) {
        result = m_layers[0]->Evaluate(deltaTime);
    } else {
        result.Resize(m_skeleton->GetBoneCount());
    }

    // Apply overlay layers
    for (size_t i = 1; i < m_layers.size(); ++i) {
        if (!m_layers[i]->IsActive() || m_mutedLayers[i]) continue;

        auto layerPose = m_layers[i]->Evaluate(deltaTime);
        m_layers[i]->ApplyToPose(result, layerPose);
    }

    return result;
}

void AnimationLayerStack::Reset() {
    for (auto& layer : m_layers) {
        layer->Reset();
    }
    for (auto& group : m_syncGroups) {
        group.masterNormalizedTime = 0.0f;
    }
}

void AnimationLayerStack::SoloLayer(size_t index) {
    if (index < m_layers.size()) {
        m_soloLayerIndex = static_cast<int>(index);
    }
}

void AnimationLayerStack::MuteLayer(size_t index, bool muted) {
    if (index < m_mutedLayers.size()) {
        m_mutedLayers[index] = muted;
    }
}

void AnimationLayerStack::ClearSolo() {
    m_soloLayerIndex = -1;
}

void AnimationLayerStack::SyncLayers() {
    for (auto& layer : m_layers) {
        switch (layer->GetSyncMode()) {
            case AnimationLayer::SyncMode::SyncToBase:
                if (!m_layers.empty()) {
                    layer->SetNormalizedTime(m_layers[0]->GetNormalizedTime());
                }
                break;

            case AnimationLayer::SyncMode::SyncToLayer: {
                int syncIdx = layer->GetSyncLayerIndex();
                if (syncIdx >= 0 && static_cast<size_t>(syncIdx) < m_layers.size()) {
                    layer->SetNormalizedTime(m_layers[syncIdx]->GetNormalizedTime());
                }
                break;
            }

            case AnimationLayer::SyncMode::Independent:
            default:
                break;
        }
    }
}

void AnimationLayerStack::UpdateSyncGroups(float deltaTime) {
    for (auto& group : m_syncGroups) {
        if (group.layerIndices.empty()) continue;

        // Update master time based on first active layer
        for (size_t idx : group.layerIndices) {
            if (idx < m_layers.size() && m_layers[idx]->IsActive()) {
                group.masterNormalizedTime = m_layers[idx]->GetNormalizedTime();
                break;
            }
        }

        // Sync all layers in group to master time
        for (size_t idx : group.layerIndices) {
            if (idx < m_layers.size()) {
                m_layers[idx]->SetNormalizedTime(group.masterNormalizedTime);
            }
        }
    }
}

std::string AnimationLayerStack::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"layers\": [\n";

    for (size_t i = 0; i < m_layers.size(); ++i) {
        const auto& layer = m_layers[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << layer->GetName() << "\",\n";
        ss << "      \"weight\": " << layer->GetWeight() << ",\n";
        ss << "      \"enabled\": " << (layer->IsEnabled() ? "true" : "false") << ",\n";
        ss << "      \"blendMode\": \"" <<
            (layer->GetBlendMode() == AnimationLayer::BlendMode::Additive ? "additive" :
             layer->GetBlendMode() == AnimationLayer::BlendMode::Multiply ? "multiply" : "override") << "\",\n";
        ss << "      \"syncMode\": \"" <<
            (layer->GetSyncMode() == AnimationLayer::SyncMode::SyncToBase ? "sync_to_base" :
             layer->GetSyncMode() == AnimationLayer::SyncMode::SyncToLayer ? "sync_to_layer" : "independent") << "\"\n";
        ss << "    }" << (i < m_layers.size() - 1 ? "," : "") << "\n";
    }

    ss << "  ],\n";
    ss << "  \"syncGroups\": [\n";

    for (size_t i = 0; i < m_syncGroups.size(); ++i) {
        const auto& group = m_syncGroups[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << group.name << "\",\n";
        ss << "      \"layers\": [";
        for (size_t j = 0; j < group.layerIndices.size(); ++j) {
            ss << group.layerIndices[j];
            if (j < group.layerIndices.size() - 1) ss << ", ";
        }
        ss << "]\n";
        ss << "    }" << (i < m_syncGroups.size() - 1 ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

bool AnimationLayerStack::FromJson(const std::string& json) {
    // Simplified - would use proper JSON library
    return true;
}

} // namespace Nova
