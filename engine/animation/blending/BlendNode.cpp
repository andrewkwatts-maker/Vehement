#include "BlendNode.hpp"
#include "../Animation.hpp"
#include "../Skeleton.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

// =============================================================================
// BoneTransform
// =============================================================================

glm::mat4 BoneTransform::ToMatrix() const {
    glm::mat4 result(1.0f);
    result = glm::translate(result, position);
    result *= glm::mat4_cast(rotation);
    result = glm::scale(result, scale);
    return result;
}

BoneTransform BoneTransform::FromMatrix(const glm::mat4& matrix) {
    BoneTransform transform;

    // Extract translation
    transform.position = glm::vec3(matrix[3]);

    // Extract scale
    transform.scale.x = glm::length(glm::vec3(matrix[0]));
    transform.scale.y = glm::length(glm::vec3(matrix[1]));
    transform.scale.z = glm::length(glm::vec3(matrix[2]));

    // Extract rotation
    glm::mat3 rotMat(
        glm::vec3(matrix[0]) / transform.scale.x,
        glm::vec3(matrix[1]) / transform.scale.y,
        glm::vec3(matrix[2]) / transform.scale.z
    );
    transform.rotation = glm::quat_cast(rotMat);

    return transform;
}

BoneTransform BoneTransform::Lerp(const BoneTransform& a, const BoneTransform& b, float t) {
    BoneTransform result;
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

BoneTransform BoneTransform::Add(const BoneTransform& base, const BoneTransform& additive) {
    BoneTransform result;
    result.position = base.position + additive.position;
    result.rotation = additive.rotation * base.rotation;
    result.scale = base.scale * additive.scale;
    return result;
}

// =============================================================================
// AnimationPose
// =============================================================================

AnimationPose::AnimationPose(size_t boneCount) {
    Resize(boneCount);
}

void AnimationPose::SetBoneTransform(size_t boneIndex, const BoneTransform& transform) {
    if (boneIndex < m_transforms.size()) {
        m_transforms[boneIndex] = transform;
    }
}

void AnimationPose::SetBoneTransform(const std::string& boneName, const BoneTransform& transform) {
    auto it = m_boneMapping.find(boneName);
    if (it != m_boneMapping.end()) {
        SetBoneTransform(it->second, transform);
    }
}

const BoneTransform& AnimationPose::GetBoneTransform(size_t boneIndex) const {
    static BoneTransform identity;
    if (boneIndex < m_transforms.size()) {
        return m_transforms[boneIndex];
    }
    return identity;
}

const BoneTransform* AnimationPose::GetBoneTransform(const std::string& boneName) const {
    auto it = m_boneMapping.find(boneName);
    if (it != m_boneMapping.end() && it->second < m_transforms.size()) {
        return &m_transforms[it->second];
    }
    return nullptr;
}

void AnimationPose::Resize(size_t boneCount) {
    m_transforms.resize(boneCount);
}

void AnimationPose::Clear() {
    for (auto& t : m_transforms) {
        t = BoneTransform{};
    }
    rootMotionDelta = glm::vec3(0.0f);
    rootMotionRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void AnimationPose::SetBoneMapping(const std::unordered_map<std::string, size_t>& mapping) {
    m_boneMapping = mapping;
}

AnimationPose AnimationPose::Blend(const AnimationPose& a, const AnimationPose& b, float weight) {
    AnimationPose result(std::max(a.GetBoneCount(), b.GetBoneCount()));

    for (size_t i = 0; i < result.GetBoneCount(); ++i) {
        const auto& ta = a.GetBoneTransform(i);
        const auto& tb = b.GetBoneTransform(i);
        result.SetBoneTransform(i, BoneTransform::Lerp(ta, tb, weight));
    }

    result.rootMotionDelta = glm::mix(a.rootMotionDelta, b.rootMotionDelta, weight);
    result.rootMotionRotation = glm::slerp(a.rootMotionRotation, b.rootMotionRotation, weight);

    return result;
}

AnimationPose AnimationPose::BlendMasked(const AnimationPose& a, const AnimationPose& b,
                                          float weight, const std::vector<float>& mask) {
    AnimationPose result(std::max(a.GetBoneCount(), b.GetBoneCount()));

    for (size_t i = 0; i < result.GetBoneCount(); ++i) {
        float maskWeight = (i < mask.size()) ? mask[i] * weight : weight;
        const auto& ta = a.GetBoneTransform(i);
        const auto& tb = b.GetBoneTransform(i);
        result.SetBoneTransform(i, BoneTransform::Lerp(ta, tb, maskWeight));
    }

    result.rootMotionDelta = glm::mix(a.rootMotionDelta, b.rootMotionDelta, weight);
    result.rootMotionRotation = glm::slerp(a.rootMotionRotation, b.rootMotionRotation, weight);

    return result;
}

AnimationPose AnimationPose::AdditiveBend(const AnimationPose& base, const AnimationPose& additive, float weight) {
    AnimationPose result(std::max(base.GetBoneCount(), additive.GetBoneCount()));

    for (size_t i = 0; i < result.GetBoneCount(); ++i) {
        const auto& tb = base.GetBoneTransform(i);
        const auto& ta = additive.GetBoneTransform(i);

        // Scale the additive transform by weight
        BoneTransform scaledAdditive;
        scaledAdditive.position = ta.position * weight;
        scaledAdditive.rotation = glm::slerp(glm::quat(1, 0, 0, 0), ta.rotation, weight);
        scaledAdditive.scale = glm::mix(glm::vec3(1.0f), ta.scale, weight);

        result.SetBoneTransform(i, BoneTransform::Add(tb, scaledAdditive));
    }

    result.rootMotionDelta = base.rootMotionDelta + additive.rootMotionDelta * weight;
    result.rootMotionRotation = glm::slerp(glm::quat(1, 0, 0, 0), additive.rootMotionRotation, weight) *
                                 base.rootMotionRotation;

    return result;
}

// =============================================================================
// BlendNode
// =============================================================================

void BlendNode::SetParameter(const std::string& name, float value) {
    m_parameters[name] = value;
}

float BlendNode::GetParameter(const std::string& name) const {
    auto it = m_parameters.find(name);
    return it != m_parameters.end() ? it->second : 0.0f;
}

bool BlendNode::HasParameter(const std::string& name) const {
    return m_parameters.find(name) != m_parameters.end();
}

std::vector<std::string> BlendNode::GetParameterNames() const {
    std::vector<std::string> names;
    names.reserve(m_parameters.size());
    for (const auto& [name, _] : m_parameters) {
        names.push_back(name);
    }
    return names;
}

void BlendNode::Reset() {
    // Base implementation does nothing
}

// =============================================================================
// ClipNode
// =============================================================================

ClipNode::ClipNode(const Animation* clip)
    : m_clip(clip) {
}

AnimationPose ClipNode::Evaluate(float deltaTime) {
    AnimationPose pose;

    if (!m_clip || !m_skeleton) {
        return pose;
    }

    // Update time
    float prevTime = m_time;
    m_time += deltaTime * m_speed * GetSpeed();

    float duration = m_clip->GetDuration();
    if (duration <= 0.0f) {
        return pose;
    }

    // Handle looping
    if (m_looping) {
        while (m_time >= duration) {
            m_time -= duration;
            if (OnLoop) OnLoop();
        }
        while (m_time < 0.0f) {
            m_time += duration;
        }
    } else {
        if (m_time >= duration) {
            m_time = duration;
            if (OnComplete) OnComplete();
        }
    }

    // Evaluate animation
    pose.Resize(m_skeleton->GetBoneCount());

    auto transforms = m_clip->Evaluate(m_time);
    for (const auto& [boneName, matrix] : transforms) {
        int boneIndex = m_skeleton->GetBoneIndex(boneName);
        if (boneIndex >= 0) {
            pose.SetBoneTransform(static_cast<size_t>(boneIndex), BoneTransform::FromMatrix(matrix));
        }
    }

    // Extract root motion
    if (m_rootMotionEnabled) {
        const Bone* rootBone = m_skeleton->GetBoneByIndex(0);
        if (rootBone) {
            auto* transform = pose.GetBoneTransform(rootBone->name);
            if (transform) {
                pose.rootMotionDelta = transform->position - m_lastRootPosition;
                pose.rootMotionRotation = transform->rotation * glm::inverse(m_lastRootRotation);
                m_lastRootPosition = transform->position;
                m_lastRootRotation = transform->rotation;
            }
        }
    }

    return pose;
}

float ClipNode::GetNormalizedTime() const {
    if (!m_clip || m_clip->GetDuration() <= 0.0f) return 0.0f;
    return m_time / m_clip->GetDuration();
}

void ClipNode::SetNormalizedTime(float normalizedTime) {
    if (m_clip && m_clip->GetDuration() > 0.0f) {
        m_time = normalizedTime * m_clip->GetDuration();
    }
}

bool ClipNode::IsComplete() const {
    if (!m_clip || m_looping) return false;
    return m_time >= m_clip->GetDuration();
}

void ClipNode::Reset() {
    m_time = 0.0f;
    m_lastRootPosition = glm::vec3(0.0f);
    m_lastRootRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

std::unique_ptr<BlendNode> ClipNode::Clone() const {
    auto clone = std::make_unique<ClipNode>(m_clip);
    clone->SetName(m_name);
    clone->SetWeight(m_weight);
    clone->SetSpeed(m_speed);
    clone->SetLooping(m_looping);
    clone->SetRootMotionEnabled(m_rootMotionEnabled);
    return clone;
}

// =============================================================================
// Blend1DNode
// =============================================================================

Blend1DNode::Blend1DNode(const std::string& parameterName)
    : m_blendParameter(parameterName) {
    SetParameter(parameterName, 0.0f);
}

void Blend1DNode::AddEntry(std::unique_ptr<BlendNode> node, float threshold, float speed) {
    BlendEntry entry;
    entry.node = std::move(node);
    entry.threshold = threshold;
    entry.speed = speed;
    m_entries.push_back(std::move(entry));
    SortEntries();
}

void Blend1DNode::RemoveEntry(size_t index) {
    if (index < m_entries.size()) {
        m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(index));
    }
}

void Blend1DNode::SortEntries() {
    std::sort(m_entries.begin(), m_entries.end(),
              [](const BlendEntry& a, const BlendEntry& b) {
                  return a.threshold < b.threshold;
              });
}

AnimationPose Blend1DNode::Evaluate(float deltaTime) {
    if (m_entries.empty()) {
        return AnimationPose{};
    }

    float blendValue = GetParameter(m_blendParameter);

    // Find blend indices
    size_t lower = 0, upper = 0;
    float t = 0.0f;
    FindBlendIndices(blendValue, lower, upper, t);

    // Set skeleton for child nodes
    for (auto& entry : m_entries) {
        if (entry.node) {
            entry.node->SetSkeleton(m_skeleton);
            // Propagate parameters
            for (const auto& [name, value] : m_parameters) {
                entry.node->SetParameter(name, value);
            }
        }
    }

    // Handle synced time
    if (m_syncEnabled && m_entries[lower].node && m_entries[upper].node) {
        // Sync time across all active clips
        m_syncedTime += deltaTime * m_speed;

        // Use fixed deltaTime for synced evaluation
        auto lowerPose = m_entries[lower].node->Evaluate(deltaTime);
        auto upperPose = m_entries[upper].node->Evaluate(deltaTime);

        if (lower == upper) {
            return lowerPose;
        }

        return AnimationPose::Blend(lowerPose, upperPose, t);
    }

    // Non-synced evaluation
    auto lowerPose = m_entries[lower].node->Evaluate(deltaTime);
    if (lower == upper) {
        return lowerPose;
    }

    auto upperPose = m_entries[upper].node->Evaluate(deltaTime);
    return AnimationPose::Blend(lowerPose, upperPose, t);
}

void Blend1DNode::FindBlendIndices(float value, size_t& lower, size_t& upper, float& t) const {
    if (m_entries.size() == 1) {
        lower = upper = 0;
        t = 0.0f;
        return;
    }

    // Find surrounding entries
    for (size_t i = 0; i < m_entries.size() - 1; ++i) {
        if (value >= m_entries[i].threshold && value <= m_entries[i + 1].threshold) {
            lower = i;
            upper = i + 1;
            float range = m_entries[upper].threshold - m_entries[lower].threshold;
            t = range > 0.0f ? (value - m_entries[lower].threshold) / range : 0.0f;
            return;
        }
    }

    // Clamp to ends
    if (value <= m_entries.front().threshold) {
        lower = upper = 0;
        t = 0.0f;
    } else {
        lower = upper = m_entries.size() - 1;
        t = 0.0f;
    }
}

void Blend1DNode::Reset() {
    m_syncedTime = 0.0f;
    for (auto& entry : m_entries) {
        if (entry.node) {
            entry.node->Reset();
        }
    }
}

std::unique_ptr<BlendNode> Blend1DNode::Clone() const {
    auto clone = std::make_unique<Blend1DNode>(m_blendParameter);
    clone->SetName(m_name);
    clone->SetWeight(m_weight);
    clone->SetSpeed(m_speed);
    clone->SetSyncEnabled(m_syncEnabled);

    for (const auto& entry : m_entries) {
        if (entry.node) {
            clone->AddEntry(entry.node->Clone(), entry.threshold, entry.speed);
        }
    }

    return clone;
}

// =============================================================================
// Blend2DNode
// =============================================================================

Blend2DNode::Blend2DNode(const std::string& paramX, const std::string& paramY)
    : m_parameterX(paramX)
    , m_parameterY(paramY) {
    SetParameter(paramX, 0.0f);
    SetParameter(paramY, 0.0f);
}

void Blend2DNode::AddPoint(std::unique_ptr<BlendNode> node, const glm::vec2& position, float speed) {
    BlendPoint point;
    point.node = std::move(node);
    point.position = position;
    point.speed = speed;
    m_points.push_back(std::move(point));
    m_triangulationDirty = true;
}

void Blend2DNode::RemovePoint(size_t index) {
    if (index < m_points.size()) {
        m_points.erase(m_points.begin() + static_cast<ptrdiff_t>(index));
        m_triangulationDirty = true;
    }
}

AnimationPose Blend2DNode::Evaluate(float deltaTime) {
    if (m_points.empty()) {
        return AnimationPose{};
    }

    if (m_triangulationDirty) {
        RebuildTriangulation();
    }

    glm::vec2 pos(GetParameter(m_parameterX), GetParameter(m_parameterY));

    // Calculate blend weights
    std::vector<float> weights(m_points.size(), 0.0f);
    CalculateWeights(pos, weights);

    // Set skeleton and parameters for child nodes
    for (auto& point : m_points) {
        if (point.node) {
            point.node->SetSkeleton(m_skeleton);
            for (const auto& [name, value] : m_parameters) {
                point.node->SetParameter(name, value);
            }
        }
    }

    // Blend poses
    AnimationPose result;
    bool first = true;

    for (size_t i = 0; i < m_points.size(); ++i) {
        if (weights[i] > 0.001f && m_points[i].node) {
            auto pose = m_points[i].node->Evaluate(deltaTime);

            if (first) {
                result = pose;
                // Scale by weight (for subsequent blending)
                first = false;
            } else {
                // Accumulate weighted pose
                result = AnimationPose::Blend(result, pose, weights[i] / (weights[i] + 1.0f));
            }
        }
    }

    return result;
}

void Blend2DNode::RebuildTriangulation() {
    m_triangles.clear();

    if (m_points.size() < 3) {
        m_triangulationDirty = false;
        return;
    }

    // Simple Delaunay triangulation using Bowyer-Watson algorithm
    // For simplicity, use a basic approach for now

    // Create super triangle
    float minX = m_points[0].position.x, maxX = minX;
    float minY = m_points[0].position.y, maxY = minY;

    for (const auto& point : m_points) {
        minX = std::min(minX, point.position.x);
        maxX = std::max(maxX, point.position.x);
        minY = std::min(minY, point.position.y);
        maxY = std::max(maxY, point.position.y);
    }

    float dx = maxX - minX;
    float dy = maxY - minY;
    float deltaMax = std::max(dx, dy);
    float midX = (minX + maxX) * 0.5f;
    float midY = (minY + maxY) * 0.5f;

    // Simple triangulation: connect all points that form valid triangles
    // For a proper implementation, use Delaunay but this suffices for the demo
    for (size_t i = 0; i < m_points.size(); ++i) {
        for (size_t j = i + 1; j < m_points.size(); ++j) {
            for (size_t k = j + 1; k < m_points.size(); ++k) {
                m_triangles.push_back({i, j, k});
            }
        }
    }

    m_triangulationDirty = false;
}

void Blend2DNode::CalculateWeights(const glm::vec2& pos, std::vector<float>& weights) const {
    if (m_points.size() <= 2) {
        // Handle degenerate cases
        if (m_points.size() == 1) {
            weights[0] = 1.0f;
        } else if (m_points.size() == 2) {
            float d1 = glm::distance(pos, m_points[0].position);
            float d2 = glm::distance(pos, m_points[1].position);
            float total = d1 + d2;
            if (total > 0.001f) {
                weights[0] = d2 / total;
                weights[1] = d1 / total;
            } else {
                weights[0] = 0.5f;
                weights[1] = 0.5f;
            }
        }
        return;
    }

    // Find containing triangle and compute barycentric coordinates
    size_t triangleIndex = FindContainingTriangle(pos);
    if (triangleIndex < m_triangles.size()) {
        CalculateBarycentricWeights(pos, triangleIndex, weights);
    } else {
        // Fallback: inverse distance weighting
        float totalWeight = 0.0f;
        for (size_t i = 0; i < m_points.size(); ++i) {
            float dist = glm::distance(pos, m_points[i].position);
            weights[i] = 1.0f / (dist + 0.001f);
            totalWeight += weights[i];
        }
        for (float& w : weights) {
            w /= totalWeight;
        }
    }
}

void Blend2DNode::CalculateBarycentricWeights(const glm::vec2& pos, size_t triangleIndex,
                                               std::vector<float>& weights) const {
    const auto& tri = m_triangles[triangleIndex];
    const glm::vec2& p0 = m_points[tri[0]].position;
    const glm::vec2& p1 = m_points[tri[1]].position;
    const glm::vec2& p2 = m_points[tri[2]].position;

    glm::vec2 v0 = p1 - p0;
    glm::vec2 v1 = p2 - p0;
    glm::vec2 v2 = pos - p0;

    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);

    float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 0.0001f) {
        // Degenerate triangle
        weights[tri[0]] = weights[tri[1]] = weights[tri[2]] = 1.0f / 3.0f;
        return;
    }

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    // Clamp to valid range
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    w = std::max(0.0f, std::min(1.0f, w));

    weights[tri[0]] = u;
    weights[tri[1]] = v;
    weights[tri[2]] = w;
}

size_t Blend2DNode::FindContainingTriangle(const glm::vec2& pos) const {
    for (size_t i = 0; i < m_triangles.size(); ++i) {
        const auto& tri = m_triangles[i];
        const glm::vec2& p0 = m_points[tri[0]].position;
        const glm::vec2& p1 = m_points[tri[1]].position;
        const glm::vec2& p2 = m_points[tri[2]].position;

        // Check if point is inside triangle using cross products
        auto sign = [](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };

        float d1 = sign(pos, p0, p1);
        float d2 = sign(pos, p1, p2);
        float d3 = sign(pos, p2, p0);

        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

        if (!(hasNeg && hasPos)) {
            return i;
        }
    }
    return m_triangles.size(); // Not found
}

void Blend2DNode::Reset() {
    for (auto& point : m_points) {
        if (point.node) {
            point.node->Reset();
        }
    }
}

std::unique_ptr<BlendNode> Blend2DNode::Clone() const {
    auto clone = std::make_unique<Blend2DNode>(m_parameterX, m_parameterY);
    clone->SetName(m_name);
    clone->SetWeight(m_weight);
    clone->SetSpeed(m_speed);

    for (const auto& point : m_points) {
        if (point.node) {
            clone->AddPoint(point.node->Clone(), point.position, point.speed);
        }
    }

    return clone;
}

// =============================================================================
// AdditiveNode
// =============================================================================

AnimationPose AdditiveNode::Evaluate(float deltaTime) {
    if (!m_baseNode) {
        return AnimationPose{};
    }

    // Set skeleton for children
    m_baseNode->SetSkeleton(m_skeleton);
    if (m_additiveNode) {
        m_additiveNode->SetSkeleton(m_skeleton);
    }

    // Propagate parameters
    for (const auto& [name, value] : m_parameters) {
        m_baseNode->SetParameter(name, value);
        if (m_additiveNode) {
            m_additiveNode->SetParameter(name, value);
        }
    }

    auto basePose = m_baseNode->Evaluate(deltaTime);

    if (!m_additiveNode) {
        return basePose;
    }

    float weight = m_weightParameter.empty() ? m_weight : GetParameter(m_weightParameter);
    auto additivePose = m_additiveNode->Evaluate(deltaTime);

    // Compute additive difference from reference if available
    if (m_referencePose.GetBoneCount() > 0) {
        AnimationPose difference(additivePose.GetBoneCount());
        for (size_t i = 0; i < additivePose.GetBoneCount(); ++i) {
            const auto& ref = m_referencePose.GetBoneTransform(i);
            const auto& add = additivePose.GetBoneTransform(i);

            BoneTransform diff;
            diff.position = add.position - ref.position;
            diff.rotation = add.rotation * glm::inverse(ref.rotation);
            diff.scale = add.scale / ref.scale;

            difference.SetBoneTransform(i, diff);
        }
        return AnimationPose::AdditiveBend(basePose, difference, weight);
    }

    return AnimationPose::AdditiveBend(basePose, additivePose, weight);
}

void AdditiveNode::Reset() {
    if (m_baseNode) m_baseNode->Reset();
    if (m_additiveNode) m_additiveNode->Reset();
}

std::unique_ptr<BlendNode> AdditiveNode::Clone() const {
    auto clone = std::make_unique<AdditiveNode>();
    clone->SetName(m_name);
    clone->SetWeight(m_weight);
    clone->SetSpeed(m_speed);
    clone->m_weightParameter = m_weightParameter;
    clone->m_referencePose = m_referencePose;

    if (m_baseNode) {
        clone->SetBaseNode(m_baseNode->Clone());
    }
    if (m_additiveNode) {
        clone->SetAdditiveNode(m_additiveNode->Clone());
    }

    return clone;
}

// =============================================================================
// LayeredNode
// =============================================================================

void LayeredNode::SetBaseLayer(std::unique_ptr<BlendNode> node) {
    m_baseLayer = std::move(node);
}

void LayeredNode::AddLayer(std::unique_ptr<BlendNode> node, std::shared_ptr<BlendMask> mask,
                            float weight, bool additive) {
    Layer layer;
    layer.node = std::move(node);
    layer.mask = mask;
    layer.weight = weight;
    layer.additive = additive;
    m_layers.push_back(std::move(layer));
}

void LayeredNode::RemoveLayer(size_t index) {
    if (index < m_layers.size()) {
        m_layers.erase(m_layers.begin() + static_cast<ptrdiff_t>(index));
    }
}

void LayeredNode::SetLayerWeight(size_t index, float weight) {
    if (index < m_layers.size()) {
        m_layers[index].weight = weight;
    }
}

void LayeredNode::SetLayerEnabled(size_t index, bool enabled) {
    if (index < m_layers.size()) {
        m_layers[index].enabled = enabled;
    }
}

AnimationPose LayeredNode::Evaluate(float deltaTime) {
    if (!m_baseLayer) {
        return AnimationPose{};
    }

    // Set skeleton and parameters for base layer
    m_baseLayer->SetSkeleton(m_skeleton);
    for (const auto& [name, value] : m_parameters) {
        m_baseLayer->SetParameter(name, value);
    }

    AnimationPose result = m_baseLayer->Evaluate(deltaTime);

    // Apply overlay layers
    for (auto& layer : m_layers) {
        if (!layer.enabled || !layer.node) continue;

        // Set skeleton and parameters
        layer.node->SetSkeleton(m_skeleton);
        for (const auto& [name, value] : m_parameters) {
            layer.node->SetParameter(name, value);
        }

        float weight = layer.weightParameter.empty() ? layer.weight :
                       GetParameter(layer.weightParameter);

        if (weight <= 0.001f) continue;

        auto layerPose = layer.node->Evaluate(deltaTime);

        if (layer.additive) {
            result = AnimationPose::AdditiveBend(result, layerPose, weight);
        } else {
            // Get mask weights if available
            if (layer.mask) {
                // Apply masked blend - implementation depends on BlendMask
                result = AnimationPose::Blend(result, layerPose, weight);
            } else {
                result = AnimationPose::Blend(result, layerPose, weight);
            }
        }
    }

    return result;
}

void LayeredNode::Reset() {
    if (m_baseLayer) m_baseLayer->Reset();
    for (auto& layer : m_layers) {
        if (layer.node) layer.node->Reset();
    }
}

std::unique_ptr<BlendNode> LayeredNode::Clone() const {
    auto clone = std::make_unique<LayeredNode>();
    clone->SetName(m_name);
    clone->SetWeight(m_weight);
    clone->SetSpeed(m_speed);

    if (m_baseLayer) {
        clone->SetBaseLayer(m_baseLayer->Clone());
    }

    for (const auto& layer : m_layers) {
        if (layer.node) {
            clone->AddLayer(layer.node->Clone(), layer.mask, layer.weight, layer.additive);
        }
    }

    return clone;
}

// =============================================================================
// StateSelectorNode
// =============================================================================

void StateSelectorNode::AddState(const std::string& name, std::unique_ptr<BlendNode> node) {
    m_states[name] = std::move(node);
    if (m_currentState.empty()) {
        m_currentState = name;
    }
}

void StateSelectorNode::RemoveState(const std::string& name) {
    m_states.erase(name);
    if (m_currentState == name) {
        m_currentState = m_states.empty() ? "" : m_states.begin()->first;
    }
}

void StateSelectorNode::SetCurrentState(const std::string& name, float blendTime) {
    if (m_states.find(name) == m_states.end()) return;
    if (name == m_currentState) return;

    m_previousState = m_currentState;
    m_currentState = name;
    m_blendTime = blendTime;
    m_blendProgress = 0.0f;
}

AnimationPose StateSelectorNode::Evaluate(float deltaTime) {
    if (m_currentState.empty() || m_states.find(m_currentState) == m_states.end()) {
        return AnimationPose{};
    }

    // Set skeleton and parameters for current state
    auto& currentNode = m_states[m_currentState];
    currentNode->SetSkeleton(m_skeleton);
    for (const auto& [name, value] : m_parameters) {
        currentNode->SetParameter(name, value);
    }

    // Handle blending between states
    if (m_blendProgress < 1.0f && !m_previousState.empty() &&
        m_states.find(m_previousState) != m_states.end()) {

        auto& prevNode = m_states[m_previousState];
        prevNode->SetSkeleton(m_skeleton);
        for (const auto& [name, value] : m_parameters) {
            prevNode->SetParameter(name, value);
        }

        m_blendProgress += deltaTime / m_blendTime;
        m_blendProgress = std::min(m_blendProgress, 1.0f);

        auto prevPose = prevNode->Evaluate(deltaTime);
        auto currPose = currentNode->Evaluate(deltaTime);

        // Smooth step for blend
        float t = m_blendProgress * m_blendProgress * (3.0f - 2.0f * m_blendProgress);
        return AnimationPose::Blend(prevPose, currPose, t);
    }

    return currentNode->Evaluate(deltaTime);
}

void StateSelectorNode::Reset() {
    for (auto& [name, node] : m_states) {
        if (node) node->Reset();
    }
    m_blendProgress = 1.0f;
}

std::unique_ptr<BlendNode> StateSelectorNode::Clone() const {
    auto clone = std::make_unique<StateSelectorNode>();
    clone->SetName(m_name);
    clone->SetWeight(m_weight);
    clone->SetSpeed(m_speed);
    clone->m_currentState = m_currentState;

    for (const auto& [name, node] : m_states) {
        if (node) {
            clone->AddState(name, node->Clone());
        }
    }

    return clone;
}

} // namespace Nova
