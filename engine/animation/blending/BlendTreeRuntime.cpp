#include "BlendTreeRuntime.hpp"
#include "../Skeleton.hpp"
#include <chrono>
#include <algorithm>

namespace Nova {

void BlendTreeRuntime::SetRootTree(std::unique_ptr<BlendNode> tree) {
    m_rootTree = std::move(tree);
    if (m_rootTree && m_skeleton) {
        m_rootTree->SetSkeleton(m_skeleton);
    }
}

void BlendTreeRuntime::SetLayerStack(std::unique_ptr<AnimationLayerStack> stack) {
    m_layerStack = std::move(stack);
    if (m_layerStack && m_skeleton) {
        m_layerStack->SetSkeleton(m_skeleton);
    }
}

void BlendTreeRuntime::SetSkeleton(Skeleton* skeleton) {
    m_skeleton = skeleton;

    if (m_rootTree) {
        m_rootTree->SetSkeleton(skeleton);
    }
    if (m_layerStack) {
        m_layerStack->SetSkeleton(skeleton);
    }

    if (skeleton) {
        m_currentPose.Resize(skeleton->GetBoneCount());
    }
}

void BlendTreeRuntime::RegisterParameter(const std::string& name, float defaultValue,
                                          float min, float max) {
    Parameter param;
    param.name = name;
    param.value = defaultValue;
    param.targetValue = defaultValue;
    param.minValue = min;
    param.maxValue = max;
    m_parameters[name] = param;
}

void BlendTreeRuntime::SetParameter(const std::string& name, float value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        it->second.value = std::clamp(value, it->second.minValue, it->second.maxValue);
        it->second.targetValue = it->second.value;
        it->second.smooth = false;
    } else {
        RegisterParameter(name, value);
    }
}

void BlendTreeRuntime::SetParameterSmooth(const std::string& name, float targetValue, float smoothSpeed) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        it->second.targetValue = std::clamp(targetValue, it->second.minValue, it->second.maxValue);
        it->second.smoothSpeed = smoothSpeed;
        it->second.smooth = true;
    } else {
        RegisterParameter(name, targetValue);
        m_parameters[name].smooth = true;
        m_parameters[name].smoothSpeed = smoothSpeed;
    }
}

float BlendTreeRuntime::GetParameter(const std::string& name) const {
    auto it = m_parameters.find(name);
    return it != m_parameters.end() ? it->second.value : 0.0f;
}

void BlendTreeRuntime::ClearParameters() {
    m_parameters.clear();
}

void BlendTreeRuntime::SetTrigger(const std::string& name) {
    m_triggers.insert(name);
}

void BlendTreeRuntime::ResetTrigger(const std::string& name) {
    m_triggers.erase(name);
}

bool BlendTreeRuntime::GetTrigger(const std::string& name) const {
    return m_triggers.find(name) != m_triggers.end();
}

void BlendTreeRuntime::Update(float deltaTime) {
    m_currentTime += deltaTime;
    UpdateParameters(deltaTime);

    if (m_layerStack) {
        m_layerStack->Update(deltaTime);
    }
}

void BlendTreeRuntime::UpdateParameters(float deltaTime) {
    for (auto& [name, param] : m_parameters) {
        if (param.smooth && std::abs(param.value - param.targetValue) > 0.0001f) {
            float direction = param.targetValue > param.value ? 1.0f : -1.0f;
            param.value += direction * param.smoothSpeed * deltaTime;

            if ((direction > 0 && param.value >= param.targetValue) ||
                (direction < 0 && param.value <= param.targetValue)) {
                param.value = param.targetValue;
                param.smooth = false;
            }
        }
    }
}

AnimationPose BlendTreeRuntime::Evaluate(float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();
    m_stats.nodesEvaluated = 0;
    m_stats.cacheHits = 0;
    m_stats.cacheMisses = 0;

    PropagateParameters();

    // Use layer stack if available, otherwise use root tree
    if (m_layerStack) {
        m_currentPose = m_layerStack->Evaluate(deltaTime);
    } else if (m_rootTree) {
        m_currentPose = m_rootTree->Evaluate(deltaTime);
        m_stats.nodesEvaluated++;
    }

    // Extract root motion
    if (m_rootMotionEnabled) {
        m_rootMotionDelta = m_currentPose.rootMotionDelta;
        m_rootRotationDelta = m_currentPose.rootMotionRotation;
    }

    // Clear triggers after evaluation
    m_triggers.clear();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.lastEvaluationTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    return m_currentPose;
}

void BlendTreeRuntime::PropagateParameters() {
    for (const auto& [name, param] : m_parameters) {
        if (m_rootTree) {
            m_rootTree->SetParameter(name, param.value);
        }
        if (m_layerStack) {
            m_layerStack->SetParameter(name, param.value);
        }
    }
}

std::vector<glm::mat4> BlendTreeRuntime::GetBoneMatrices() const {
    if (!m_skeleton) {
        return {};
    }

    std::unordered_map<std::string, glm::mat4> transforms;
    const auto& bones = m_skeleton->GetBones();

    for (size_t i = 0; i < bones.size() && i < m_currentPose.GetBoneCount(); ++i) {
        transforms[bones[i].name] = m_currentPose.GetBoneTransform(i).ToMatrix();
    }

    return m_skeleton->CalculateBoneMatrices(transforms);
}

void BlendTreeRuntime::GetBoneMatricesInto(std::span<glm::mat4> outMatrices) const {
    if (!m_skeleton) return;

    std::unordered_map<std::string, glm::mat4> transforms;
    const auto& bones = m_skeleton->GetBones();

    for (size_t i = 0; i < bones.size() && i < m_currentPose.GetBoneCount(); ++i) {
        transforms[bones[i].name] = m_currentPose.GetBoneTransform(i).ToMatrix();
    }

    m_skeleton->CalculateBoneMatricesInto(transforms, outMatrices);
}

void BlendTreeRuntime::Reset() {
    if (m_rootTree) {
        m_rootTree->Reset();
    }
    if (m_layerStack) {
        m_layerStack->Reset();
    }

    m_currentPose.Clear();
    m_triggers.clear();
    ClearRootMotion();
    InvalidateCaches();
}

void BlendTreeRuntime::InvalidateCaches() {
    m_poseCache.clear();
}

std::vector<BlendTreeRuntime::NodeDebugInfo> BlendTreeRuntime::GetDebugInfo() const {
    std::vector<NodeDebugInfo> infos;

    if (m_rootTree) {
        CollectDebugInfo(m_rootTree.get(), infos);
    }

    if (m_layerStack) {
        for (size_t i = 0; i < m_layerStack->GetLayerCount(); ++i) {
            auto* layer = m_layerStack->GetLayer(i);
            if (layer && layer->GetBlendTree()) {
                NodeDebugInfo layerInfo;
                layerInfo.nodeName = "Layer: " + layer->GetName();
                layerInfo.nodeType = "AnimationLayer";
                layerInfo.weight = layer->GetWeight();
                layerInfo.active = layer->IsActive();
                infos.push_back(layerInfo);

                CollectDebugInfo(layer->GetBlendTree(), infos);
            }
        }
    }

    return infos;
}

void BlendTreeRuntime::CollectDebugInfo(BlendNode* node, std::vector<NodeDebugInfo>& infos) const {
    if (!node) return;

    NodeDebugInfo info;
    info.nodeName = node->GetName();
    info.weight = node->GetWeight();
    info.active = info.weight > 0.001f;

    // Determine node type
    if (dynamic_cast<ClipNode*>(node)) {
        info.nodeType = "ClipNode";
        auto* clip = dynamic_cast<ClipNode*>(node);
        info.normalizedTime = clip->GetNormalizedTime();
    } else if (dynamic_cast<Blend1DNode*>(node)) {
        info.nodeType = "Blend1DNode";
    } else if (dynamic_cast<Blend2DNode*>(node)) {
        info.nodeType = "Blend2DNode";
    } else if (dynamic_cast<AdditiveNode*>(node)) {
        info.nodeType = "AdditiveNode";
    } else if (dynamic_cast<LayeredNode*>(node)) {
        info.nodeType = "LayeredNode";
    } else if (dynamic_cast<StateSelectorNode*>(node)) {
        info.nodeType = "StateSelectorNode";
    } else {
        info.nodeType = "BlendNode";
    }

    // Get parameters
    for (const auto& name : node->GetParameterNames()) {
        info.parameters.push_back({name, node->GetParameter(name)});
    }

    infos.push_back(info);
}

void BlendTreeRuntime::OnAnimationEvent(std::function<void(const std::string&, float)> callback) {
    m_eventCallback = std::move(callback);
}

void BlendTreeRuntime::OnLoop(std::function<void(const std::string&)> callback) {
    m_loopCallback = std::move(callback);
}

void BlendTreeRuntime::ClearRootMotion() {
    m_rootMotionDelta = glm::vec3(0.0f);
    m_rootRotationDelta = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

// =============================================================================
// AnimationPosePool
// =============================================================================

AnimationPosePool& AnimationPosePool::Instance() {
    static AnimationPosePool instance;
    return instance;
}

AnimationPose* AnimationPosePool::Acquire(size_t boneCount) {
    // Try to find available pose with matching size
    for (auto it = m_available.begin(); it != m_available.end(); ++it) {
        if ((*it)->GetBoneCount() == boneCount) {
            AnimationPose* pose = *it;
            m_available.erase(it);
            pose->Clear();
            return pose;
        }
    }

    // Allocate new pose
    auto pose = std::make_unique<AnimationPose>(boneCount);
    AnimationPose* ptr = pose.get();
    m_pool.push_back(std::move(pose));
    return ptr;
}

void AnimationPosePool::Release(AnimationPose* pose) {
    if (pose) {
        m_available.push_back(pose);
    }
}

void AnimationPosePool::Clear() {
    m_pool.clear();
    m_available.clear();
}

AnimationPosePool::Stats AnimationPosePool::GetStats() const {
    Stats stats;
    stats.totalAllocated = m_pool.size();
    stats.available = m_available.size();
    stats.inUse = stats.totalAllocated - stats.available;
    return stats;
}

} // namespace Nova
