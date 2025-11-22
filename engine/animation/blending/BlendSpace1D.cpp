#include "BlendSpace1D.hpp"
#include "../Animation.hpp"
#include "../Skeleton.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Nova {

BlendSpace1D::BlendSpace1D(const std::string& name)
    : m_name(name) {
}

void BlendSpace1D::SetParameterRange(float min, float max) {
    m_minParameter = min;
    m_maxParameter = max;
}

size_t BlendSpace1D::AddSample(const Animation* clip, float position, float playbackSpeed) {
    Sample sample;
    sample.clip = clip;
    sample.clipId = clip ? clip->GetName() : "";
    sample.position = position;
    sample.playbackSpeed = playbackSpeed;
    return AddSample(sample);
}

size_t BlendSpace1D::AddSample(const Sample& sample) {
    m_samples.push_back(sample);
    SortSamples();
    return m_samples.size() - 1;
}

void BlendSpace1D::RemoveSample(size_t index) {
    if (index < m_samples.size()) {
        m_samples.erase(m_samples.begin() + static_cast<ptrdiff_t>(index));
    }
}

void BlendSpace1D::ClearSamples() {
    m_samples.clear();
}

void BlendSpace1D::SortSamples() {
    std::sort(m_samples.begin(), m_samples.end(),
              [](const Sample& a, const Sample& b) {
                  return a.position < b.position;
              });
}

void BlendSpace1D::SetSamplePosition(size_t index, float position) {
    if (index < m_samples.size()) {
        m_samples[index].position = position;
        SortSamples();
    }
}

void BlendSpace1D::SetSampleSpeed(size_t index, float speed) {
    if (index < m_samples.size()) {
        m_samples[index].playbackSpeed = speed;
    }
}

BlendSpace1D::BlendResult BlendSpace1D::Evaluate(float parameterValue, float deltaTime) {
    BlendResult result;

    if (m_samples.empty() || !m_skeleton) {
        result.pose.Resize(m_skeleton ? m_skeleton->GetBoneCount() : 0);
        return result;
    }

    // Clamp parameter to range
    parameterValue = std::clamp(parameterValue, m_minParameter, m_maxParameter);

    // Find blend indices
    auto indices = FindBlendIndices(parameterValue);
    result.lowerSampleIndex = indices.lower;
    result.upperSampleIndex = indices.upper;
    result.blendWeight = indices.t;

    // Track sample transitions
    if (indices.lower != m_lastLowerIndex && OnSampleExit) {
        OnSampleExit(m_lastLowerIndex);
    }
    if (indices.lower != m_lastLowerIndex && OnSampleEnter) {
        OnSampleEnter(indices.lower);
    }
    m_lastLowerIndex = indices.lower;
    m_lastUpperIndex = indices.upper;

    // Update time
    float avgSpeed = 1.0f;
    if (indices.lower != indices.upper) {
        avgSpeed = glm::mix(m_samples[indices.lower].playbackSpeed,
                           m_samples[indices.upper].playbackSpeed, indices.t);
    } else {
        avgSpeed = m_samples[indices.lower].playbackSpeed;
    }

    m_currentTime += deltaTime * avgSpeed;

    // Get duration (use lower sample as reference)
    float duration = 1.0f;
    if (m_samples[indices.lower].clip) {
        duration = m_samples[indices.lower].clip->GetDuration();
    }

    // Handle looping
    if (duration > 0.0f) {
        while (m_currentTime >= duration) {
            m_currentTime -= duration;
            if (OnLoopComplete) OnLoopComplete();
        }
    }

    m_normalizedTime = duration > 0.0f ? m_currentTime / duration : 0.0f;
    result.normalizedTime = m_normalizedTime;

    // Evaluate samples
    const auto& lowerSample = m_samples[indices.lower];
    const auto& upperSample = m_samples[indices.upper];

    AnimationPose lowerPose, upperPose;
    lowerPose.Resize(m_skeleton->GetBoneCount());
    upperPose.Resize(m_skeleton->GetBoneCount());

    // Compute synced time for each sample
    float lowerTime = m_syncEnabled ?
        ComputeSyncedTime(m_normalizedTime, indices.lower) * (lowerSample.clip ? lowerSample.clip->GetDuration() : 1.0f) :
        m_currentTime;
    float upperTime = m_syncEnabled ?
        ComputeSyncedTime(m_normalizedTime, indices.upper) * (upperSample.clip ? upperSample.clip->GetDuration() : 1.0f) :
        m_currentTime;

    // Evaluate lower sample
    if (lowerSample.clip) {
        auto transforms = lowerSample.clip->Evaluate(lowerTime);
        for (const auto& [boneName, matrix] : transforms) {
            int boneIndex = m_skeleton->GetBoneIndex(boneName);
            if (boneIndex >= 0) {
                lowerPose.SetBoneTransform(static_cast<size_t>(boneIndex),
                                           BoneTransform::FromMatrix(matrix));
            }
        }
    }

    // Evaluate upper sample if different
    if (indices.lower != indices.upper && upperSample.clip) {
        auto transforms = upperSample.clip->Evaluate(upperTime);
        for (const auto& [boneName, matrix] : transforms) {
            int boneIndex = m_skeleton->GetBoneIndex(boneName);
            if (boneIndex >= 0) {
                upperPose.SetBoneTransform(static_cast<size_t>(boneIndex),
                                           BoneTransform::FromMatrix(matrix));
            }
        }
    } else {
        upperPose = lowerPose;
    }

    // Blend poses based on interpolation mode
    switch (m_interpolationMode) {
        case InterpolationMode::Smooth: {
            float smoothT = indices.t * indices.t * (3.0f - 2.0f * indices.t);
            result.pose = BlendPoses(lowerPose, upperPose, smoothT);
            break;
        }
        case InterpolationMode::Cubic:
            // For cubic, we would need 4 samples - fallback to linear
        case InterpolationMode::Linear:
        default:
            result.pose = BlendPoses(lowerPose, upperPose, indices.t);
            break;
    }

    // Blend root motion
    if (m_rootMotionEnabled) {
        result.rootMotionDelta = glm::mix(lowerPose.rootMotionDelta,
                                          upperPose.rootMotionDelta, indices.t);
        result.rootRotationDelta = glm::slerp(lowerPose.rootMotionRotation,
                                              upperPose.rootMotionRotation, indices.t);
    }

    return result;
}

AnimationPose BlendSpace1D::Preview(float parameterValue, float normalizedTime) const {
    AnimationPose result;

    if (m_samples.empty() || !m_skeleton) {
        result.Resize(m_skeleton ? m_skeleton->GetBoneCount() : 0);
        return result;
    }

    parameterValue = std::clamp(parameterValue, m_minParameter, m_maxParameter);
    auto indices = FindBlendIndices(parameterValue);

    const auto& lowerSample = m_samples[indices.lower];
    const auto& upperSample = m_samples[indices.upper];

    AnimationPose lowerPose, upperPose;
    lowerPose.Resize(m_skeleton->GetBoneCount());
    upperPose.Resize(m_skeleton->GetBoneCount());

    float lowerTime = normalizedTime * (lowerSample.clip ? lowerSample.clip->GetDuration() : 1.0f);
    float upperTime = normalizedTime * (upperSample.clip ? upperSample.clip->GetDuration() : 1.0f);

    if (lowerSample.clip) {
        auto transforms = lowerSample.clip->Evaluate(lowerTime);
        for (const auto& [boneName, matrix] : transforms) {
            int boneIndex = m_skeleton->GetBoneIndex(boneName);
            if (boneIndex >= 0) {
                lowerPose.SetBoneTransform(static_cast<size_t>(boneIndex),
                                           BoneTransform::FromMatrix(matrix));
            }
        }
    }

    if (indices.lower != indices.upper && upperSample.clip) {
        auto transforms = upperSample.clip->Evaluate(upperTime);
        for (const auto& [boneName, matrix] : transforms) {
            int boneIndex = m_skeleton->GetBoneIndex(boneName);
            if (boneIndex >= 0) {
                upperPose.SetBoneTransform(static_cast<size_t>(boneIndex),
                                           BoneTransform::FromMatrix(matrix));
            }
        }
    } else {
        upperPose = lowerPose;
    }

    return BlendPoses(lowerPose, upperPose, indices.t);
}

std::vector<float> BlendSpace1D::GetSampleWeights(float parameterValue) const {
    std::vector<float> weights(m_samples.size(), 0.0f);

    if (m_samples.empty()) return weights;

    parameterValue = std::clamp(parameterValue, m_minParameter, m_maxParameter);
    auto indices = FindBlendIndices(parameterValue);

    if (indices.lower == indices.upper) {
        weights[indices.lower] = 1.0f;
    } else {
        weights[indices.lower] = 1.0f - indices.t;
        weights[indices.upper] = indices.t;
    }

    return weights;
}

void BlendSpace1D::Reset() {
    m_currentTime = 0.0f;
    m_normalizedTime = 0.0f;
    m_lastLowerIndex = 0;
    m_lastUpperIndex = 0;
}

void BlendSpace1D::AddSyncMarker(const std::string& name, float normalizedTime) {
    SyncMarker marker;
    marker.name = name;
    marker.normalizedTime = normalizedTime;
    m_syncMarkers.push_back(marker);

    // Sort by time
    std::sort(m_syncMarkers.begin(), m_syncMarkers.end(),
              [](const SyncMarker& a, const SyncMarker& b) {
                  return a.normalizedTime < b.normalizedTime;
              });
}

void BlendSpace1D::RemoveSyncMarker(const std::string& name) {
    m_syncMarkers.erase(
        std::remove_if(m_syncMarkers.begin(), m_syncMarkers.end(),
                       [&name](const SyncMarker& m) { return m.name == name; }),
        m_syncMarkers.end());
}

void BlendSpace1D::ComputeMotionData() {
    for (auto& sample : m_samples) {
        if (!sample.clip || !m_skeleton) continue;

        // Compute average root motion speed over the clip
        float totalDistance = 0.0f;
        float totalRotation = 0.0f;
        float duration = sample.clip->GetDuration();

        if (duration <= 0.0f) continue;

        const int numSamples = 10;
        glm::vec3 lastPos(0.0f);
        glm::quat lastRot(1.0f, 0.0f, 0.0f, 0.0f);

        int rootBoneIndex = m_skeleton->GetBoneIndex(m_rootBoneName);
        if (rootBoneIndex < 0) continue;

        for (int i = 0; i <= numSamples; ++i) {
            float t = (static_cast<float>(i) / numSamples) * duration;
            auto transforms = sample.clip->Evaluate(t);

            auto it = transforms.find(m_rootBoneName);
            if (it != transforms.end()) {
                auto transform = BoneTransform::FromMatrix(it->second);

                if (i > 0) {
                    totalDistance += glm::length(transform.position - lastPos);
                    glm::quat rotDiff = transform.rotation * glm::inverse(lastRot);
                    totalRotation += glm::angle(rotDiff);
                }

                lastPos = transform.position;
                lastRot = transform.rotation;
            }
        }

        sample.averageSpeed = totalDistance / duration;
        sample.averageAngularSpeed = totalRotation / duration;
    }
}

float BlendSpace1D::FindBestParameter(float desiredSpeed, float desiredAngularSpeed) const {
    if (m_samples.empty()) return 0.0f;

    float bestError = std::numeric_limits<float>::max();
    float bestParam = m_samples[0].position;

    for (const auto& sample : m_samples) {
        float speedError = std::abs(sample.averageSpeed - desiredSpeed);
        float angularError = std::abs(sample.averageAngularSpeed - desiredAngularSpeed) * 0.1f;
        float totalError = speedError + angularError;

        if (totalError < bestError) {
            bestError = totalError;
            bestParam = sample.position;
        }
    }

    return bestParam;
}

std::string BlendSpace1D::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"parameterName\": \"" << m_parameterName << "\",\n";
    ss << "  \"minParameter\": " << m_minParameter << ",\n";
    ss << "  \"maxParameter\": " << m_maxParameter << ",\n";
    ss << "  \"syncEnabled\": " << (m_syncEnabled ? "true" : "false") << ",\n";
    ss << "  \"rootMotionEnabled\": " << (m_rootMotionEnabled ? "true" : "false") << ",\n";
    ss << "  \"interpolation\": \"" <<
        (m_interpolationMode == InterpolationMode::Smooth ? "smooth" :
         m_interpolationMode == InterpolationMode::Cubic ? "cubic" : "linear") << "\",\n";

    ss << "  \"samples\": [\n";
    for (size_t i = 0; i < m_samples.size(); ++i) {
        const auto& s = m_samples[i];
        ss << "    {\n";
        ss << "      \"clipId\": \"" << s.clipId << "\",\n";
        ss << "      \"position\": " << s.position << ",\n";
        ss << "      \"playbackSpeed\": " << s.playbackSpeed << ",\n";
        ss << "      \"syncMarker\": " << (s.syncMarker ? "true" : "false") << ",\n";
        ss << "      \"syncOffset\": " << s.syncOffset << "\n";
        ss << "    }" << (i < m_samples.size() - 1 ? "," : "") << "\n";
    }
    ss << "  ],\n";

    ss << "  \"syncMarkers\": [\n";
    for (size_t i = 0; i < m_syncMarkers.size(); ++i) {
        const auto& m = m_syncMarkers[i];
        ss << "    { \"name\": \"" << m.name << "\", \"time\": " << m.normalizedTime << " }";
        ss << (i < m_syncMarkers.size() - 1 ? "," : "") << "\n";
    }
    ss << "  ]\n";

    ss << "}";
    return ss.str();
}

bool BlendSpace1D::FromJson(const std::string& json) {
    // Simplified JSON parsing - in production, use a proper JSON library
    // This is a placeholder that would need actual implementation
    return true;
}

std::unique_ptr<Blend1DNode> BlendSpace1D::CreateBlendNode() const {
    auto node = std::make_unique<Blend1DNode>(m_parameterName);
    node->SetName(m_name);
    node->SetSyncEnabled(m_syncEnabled);

    for (const auto& sample : m_samples) {
        auto clipNode = std::make_unique<ClipNode>(sample.clip);
        clipNode->SetSpeed(sample.playbackSpeed);
        node->AddEntry(std::move(clipNode), sample.position, sample.playbackSpeed);
    }

    return node;
}

BlendSpace1D::BlendIndices BlendSpace1D::FindBlendIndices(float value) const {
    BlendIndices result;

    if (m_samples.size() == 1) {
        result.lower = result.upper = 0;
        result.t = 0.0f;
        return result;
    }

    // Find surrounding samples
    for (size_t i = 0; i < m_samples.size() - 1; ++i) {
        if (value >= m_samples[i].position && value <= m_samples[i + 1].position) {
            result.lower = i;
            result.upper = i + 1;
            float range = m_samples[result.upper].position - m_samples[result.lower].position;
            result.t = range > 0.0f ? (value - m_samples[result.lower].position) / range : 0.0f;
            return result;
        }
    }

    // Clamp to ends
    if (value <= m_samples.front().position) {
        result.lower = result.upper = 0;
    } else {
        result.lower = result.upper = m_samples.size() - 1;
    }
    result.t = 0.0f;

    return result;
}

AnimationPose BlendSpace1D::BlendPoses(const AnimationPose& a, const AnimationPose& b, float t) const {
    return AnimationPose::Blend(a, b, t);
}

float BlendSpace1D::ComputeSyncedTime(float normalizedTime, size_t sampleIndex) const {
    if (sampleIndex >= m_samples.size()) return normalizedTime;

    const auto& sample = m_samples[sampleIndex];
    return std::fmod(normalizedTime + sample.syncOffset, 1.0f);
}

// =============================================================================
// BlendSpace1DBuilder
// =============================================================================

BlendSpace1DBuilder& BlendSpace1DBuilder::SetName(const std::string& name) {
    m_blendSpace->SetName(name);
    return *this;
}

BlendSpace1DBuilder& BlendSpace1DBuilder::SetParameter(const std::string& name, float min, float max) {
    m_blendSpace->SetParameterName(name);
    m_blendSpace->SetParameterRange(min, max);
    return *this;
}

BlendSpace1DBuilder& BlendSpace1DBuilder::SetSkeleton(Skeleton* skeleton) {
    m_blendSpace->SetSkeleton(skeleton);
    return *this;
}

BlendSpace1DBuilder& BlendSpace1DBuilder::AddSample(const Animation* clip, float position, float speed) {
    m_blendSpace->AddSample(clip, position, speed);
    return *this;
}

BlendSpace1DBuilder& BlendSpace1DBuilder::EnableSync(bool enabled) {
    m_blendSpace->SetSyncEnabled(enabled);
    return *this;
}

BlendSpace1DBuilder& BlendSpace1DBuilder::EnableRootMotion(bool enabled) {
    m_blendSpace->SetRootMotionEnabled(enabled);
    return *this;
}

BlendSpace1DBuilder& BlendSpace1DBuilder::SetInterpolation(BlendSpace1D::InterpolationMode mode) {
    m_blendSpace->SetInterpolationMode(mode);
    return *this;
}

std::unique_ptr<BlendSpace1D> BlendSpace1DBuilder::Build() {
    m_blendSpace->SortSamples();
    return std::move(m_blendSpace);
}

} // namespace Nova
