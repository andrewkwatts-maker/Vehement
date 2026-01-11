#include "BlendSpace2D.hpp"
#include "../Animation.hpp"
#include "../Skeleton.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Nova {

BlendSpace2D::BlendSpace2D(const std::string& name)
    : m_name(name) {
}

void BlendSpace2D::SetParameterRangeX(float min, float max) {
    m_minBounds.x = min;
    m_maxBounds.x = max;
}

void BlendSpace2D::SetParameterRangeY(float min, float max) {
    m_minBounds.y = min;
    m_maxBounds.y = max;
}

size_t BlendSpace2D::AddSample(const Animation* clip, const glm::vec2& position, float playbackSpeed) {
    Sample sample;
    sample.clip = clip;
    sample.clipId = clip ? clip->GetName() : "";
    sample.position = position;
    sample.playbackSpeed = playbackSpeed;
    return AddSample(sample);
}

size_t BlendSpace2D::AddSample(const Sample& sample) {
    m_samples.push_back(sample);
    m_triangulationDirty = true;
    return m_samples.size() - 1;
}

void BlendSpace2D::RemoveSample(size_t index) {
    if (index < m_samples.size()) {
        m_samples.erase(m_samples.begin() + static_cast<ptrdiff_t>(index));
        m_triangulationDirty = true;
    }
}

void BlendSpace2D::ClearSamples() {
    m_samples.clear();
    m_triangles.clear();
    m_triangulationDirty = true;
}

void BlendSpace2D::SetSamplePosition(size_t index, const glm::vec2& position) {
    if (index < m_samples.size()) {
        m_samples[index].position = position;
        m_triangulationDirty = true;
    }
}

void BlendSpace2D::RebuildTriangulation() {
    m_triangles.clear();

    if (m_samples.size() < 3) {
        m_triangulationDirty = false;
        return;
    }

    BowyerWatson();
    m_triangulationDirty = false;
}

void BlendSpace2D::BowyerWatson() {
    // Create super triangle that contains all points
    float minX = m_minBounds.x - 1.0f;
    float maxX = m_maxBounds.x + 1.0f;
    float minY = m_minBounds.y - 1.0f;
    float maxY = m_maxBounds.y + 1.0f;

    float dx = maxX - minX;
    float dy = maxY - minY;
    float dmax = std::max(dx, dy);
    float midX = (minX + maxX) * 0.5f;
    float midY = (minY + maxY) * 0.5f;

    // Super triangle vertices (virtual points beyond actual samples)
    glm::vec2 superV0(midX - 20.0f * dmax, midY - dmax);
    glm::vec2 superV1(midX, midY + 20.0f * dmax);
    glm::vec2 superV2(midX + 20.0f * dmax, midY - dmax);

    // Store all points including super triangle
    std::vector<glm::vec2> points;
    points.push_back(superV0);
    points.push_back(superV1);
    points.push_back(superV2);
    for (const auto& sample : m_samples) {
        points.push_back(sample.position);
    }

    // Initialize with super triangle
    std::vector<Triangle> triangulation;
    Triangle superTri;
    superTri.indices = {0, 1, 2};
    CalculateCircumcircle(superTri);
    triangulation.push_back(superTri);

    // Add each point one at a time
    for (size_t i = 3; i < points.size(); ++i) {
        const glm::vec2& p = points[i];

        // Find all triangles whose circumcircle contains the point
        std::vector<Triangle> badTriangles;
        std::vector<size_t> badIndices;

        for (size_t j = 0; j < triangulation.size(); ++j) {
            if (IsInsideCircumcircle(p, triangulation[j])) {
                badTriangles.push_back(triangulation[j]);
                badIndices.push_back(j);
            }
        }

        // Find the boundary edges of the polygonal hole
        std::vector<std::pair<size_t, size_t>> polygon;

        for (const auto& tri : badTriangles) {
            for (int e = 0; e < 3; ++e) {
                size_t e1 = tri.indices[e];
                size_t e2 = tri.indices[(e + 1) % 3];

                // Check if this edge is shared with another bad triangle
                bool shared = false;
                for (const auto& other : badTriangles) {
                    if (&tri == &other) continue;
                    for (int oe = 0; oe < 3; ++oe) {
                        size_t oe1 = other.indices[oe];
                        size_t oe2 = other.indices[(oe + 1) % 3];
                        if ((e1 == oe1 && e2 == oe2) || (e1 == oe2 && e2 == oe1)) {
                            shared = true;
                            break;
                        }
                    }
                    if (shared) break;
                }

                if (!shared) {
                    polygon.push_back({e1, e2});
                }
            }
        }

        // Remove bad triangles (in reverse order to preserve indices)
        std::sort(badIndices.begin(), badIndices.end(), std::greater<size_t>());
        for (size_t idx : badIndices) {
            triangulation.erase(triangulation.begin() + static_cast<ptrdiff_t>(idx));
        }

        // Create new triangles from polygon edges to new point
        for (const auto& edge : polygon) {
            Triangle newTri;
            newTri.indices = {edge.first, edge.second, i};

            // Calculate circumcircle using actual positions
            glm::vec2 v0 = points[newTri.indices[0]];
            glm::vec2 v1 = points[newTri.indices[1]];
            glm::vec2 v2 = points[newTri.indices[2]];

            // Cross product to check orientation
            float cross = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
            if (cross < 0) {
                std::swap(newTri.indices[1], newTri.indices[2]);
            }

            CalculateCircumcircle(newTri);
            triangulation.push_back(newTri);
        }
    }

    // Remove triangles that share vertices with super triangle
    m_triangles.clear();
    for (const auto& tri : triangulation) {
        bool hasSuperVertex = false;
        for (size_t idx : tri.indices) {
            if (idx < 3) {
                hasSuperVertex = true;
                break;
            }
        }
        if (!hasSuperVertex) {
            // Adjust indices to account for removed super triangle vertices
            Triangle adjusted;
            adjusted.indices = {tri.indices[0] - 3, tri.indices[1] - 3, tri.indices[2] - 3};
            m_triangles.push_back(adjusted);
        }
    }

    // Recalculate circumcircles for final triangles
    for (auto& tri : m_triangles) {
        const glm::vec2& v0 = m_samples[tri.indices[0]].position;
        const glm::vec2& v1 = m_samples[tri.indices[1]].position;
        const glm::vec2& v2 = m_samples[tri.indices[2]].position;

        // Calculate circumcenter
        float ax = v0.x, ay = v0.y;
        float bx = v1.x, by = v1.y;
        float cx = v2.x, cy = v2.y;

        float d = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs(d) > 0.0001f) {
            float ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) +
                       (cx * cx + cy * cy) * (ay - by)) / d;
            float uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) +
                       (cx * cx + cy * cy) * (bx - ax)) / d;
            tri.circumcenter = glm::vec2(ux, uy);
            tri.circumradiusSq = (ux - ax) * (ux - ax) + (uy - ay) * (uy - ay);
        }
    }
}

void BlendSpace2D::CalculateCircumcircle(Triangle& tri) const {
    // For triangulation, use super triangle vertices
    // This is called during BowyerWatson with temporary point array
}

bool BlendSpace2D::IsInsideCircumcircle(const glm::vec2& p, const Triangle& tri) const {
    float dx = p.x - tri.circumcenter.x;
    float dy = p.y - tri.circumcenter.y;
    return (dx * dx + dy * dy) < tri.circumradiusSq;
}

int BlendSpace2D::FindContainingTriangle(const glm::vec2& position) const {
    for (size_t i = 0; i < m_triangles.size(); ++i) {
        glm::vec3 bary = ComputeBarycentric(position, m_triangles[i]);
        if (bary.x >= 0 && bary.y >= 0 && bary.z >= 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

glm::vec3 BlendSpace2D::ComputeBarycentric(const glm::vec2& p, const Triangle& tri) const {
    const glm::vec2& v0 = m_samples[tri.indices[0]].position;
    const glm::vec2& v1 = m_samples[tri.indices[1]].position;
    const glm::vec2& v2 = m_samples[tri.indices[2]].position;

    glm::vec2 e0 = v1 - v0;
    glm::vec2 e1 = v2 - v0;
    glm::vec2 e2 = p - v0;

    float d00 = glm::dot(e0, e0);
    float d01 = glm::dot(e0, e1);
    float d11 = glm::dot(e1, e1);
    float d20 = glm::dot(e2, e0);
    float d21 = glm::dot(e2, e1);

    float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 0.0001f) {
        return glm::vec3(-1.0f);
    }

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    return glm::vec3(u, v, w);
}

BlendSpace2D::BlendResult BlendSpace2D::Evaluate(float posX, float posY, float deltaTime) {
    BlendResult result;

    if (m_samples.empty() || !m_skeleton) {
        result.pose.Resize(m_skeleton ? m_skeleton->GetBoneCount() : 0);
        return result;
    }

    if (m_triangulationDirty) {
        RebuildTriangulation();
    }

    glm::vec2 pos = glm::clamp(glm::vec2(posX, posY), m_minBounds, m_maxBounds);

    // Calculate weights
    std::vector<float> weights = GetSampleWeights(pos);

    // Calculate average playback speed
    float avgSpeed = 0.0f;
    for (size_t i = 0; i < m_samples.size(); ++i) {
        avgSpeed += weights[i] * m_samples[i].playbackSpeed;
    }

    // Update time
    m_currentTime += deltaTime * avgSpeed;

    // Get average duration
    float avgDuration = 0.0f;
    float totalWeight = 0.0f;
    for (size_t i = 0; i < m_samples.size(); ++i) {
        if (weights[i] > 0.001f && m_samples[i].clip) {
            avgDuration += weights[i] * m_samples[i].clip->GetDuration();
            totalWeight += weights[i];
        }
    }
    if (totalWeight > 0.0f) {
        avgDuration /= totalWeight;
    } else {
        avgDuration = 1.0f;
    }

    // Handle looping
    if (avgDuration > 0.0f) {
        while (m_currentTime >= avgDuration) {
            m_currentTime -= avgDuration;
            if (OnLoopComplete) OnLoopComplete();
        }
    }

    m_normalizedTime = avgDuration > 0.0f ? m_currentTime / avgDuration : 0.0f;
    result.normalizedTime = m_normalizedTime;

    // Blend poses
    result.pose.Resize(m_skeleton->GetBoneCount());
    bool first = true;

    for (size_t i = 0; i < m_samples.size(); ++i) {
        if (weights[i] < 0.001f || !m_samples[i].clip) continue;

        result.activeWeights.push_back({i, weights[i]});

        // Evaluate sample
        float sampleTime = m_normalizedTime * m_samples[i].clip->GetDuration();
        auto transforms = m_samples[i].clip->Evaluate(sampleTime);

        AnimationPose samplePose;
        samplePose.Resize(m_skeleton->GetBoneCount());

        for (const auto& [boneName, matrix] : transforms) {
            int boneIndex = m_skeleton->GetBoneIndex(boneName);
            if (boneIndex >= 0) {
                samplePose.SetBoneTransform(static_cast<size_t>(boneIndex),
                                            BoneTransform::FromMatrix(matrix));
            }
        }

        if (first) {
            result.pose = samplePose;
            first = false;
        } else {
            // Weighted blend
            for (size_t j = 0; j < result.pose.GetBoneCount(); ++j) {
                auto& current = result.pose.GetTransforms()[j];
                const auto& sample = samplePose.GetBoneTransform(j);
                current = BoneTransform::Lerp(current, sample, weights[i]);
            }
        }
    }

    return result;
}

AnimationPose BlendSpace2D::Preview(const glm::vec2& position, float normalizedTime) const {
    AnimationPose result;

    if (m_samples.empty() || !m_skeleton) {
        result.Resize(m_skeleton ? m_skeleton->GetBoneCount() : 0);
        return result;
    }

    std::vector<float> weights = GetSampleWeights(position);

    result.Resize(m_skeleton->GetBoneCount());
    bool first = true;

    for (size_t i = 0; i < m_samples.size(); ++i) {
        if (weights[i] < 0.001f || !m_samples[i].clip) continue;

        float sampleTime = normalizedTime * m_samples[i].clip->GetDuration();
        auto transforms = m_samples[i].clip->Evaluate(sampleTime);

        AnimationPose samplePose;
        samplePose.Resize(m_skeleton->GetBoneCount());

        for (const auto& [boneName, matrix] : transforms) {
            int boneIndex = m_skeleton->GetBoneIndex(boneName);
            if (boneIndex >= 0) {
                samplePose.SetBoneTransform(static_cast<size_t>(boneIndex),
                                            BoneTransform::FromMatrix(matrix));
            }
        }

        if (first) {
            result = samplePose;
            first = false;
        } else {
            for (size_t j = 0; j < result.GetBoneCount(); ++j) {
                auto& current = result.GetTransforms()[j];
                const auto& sample = samplePose.GetBoneTransform(j);
                current = BoneTransform::Lerp(current, sample, weights[i]);
            }
        }
    }

    return result;
}

std::vector<float> BlendSpace2D::GetSampleWeights(const glm::vec2& position) const {
    std::vector<float> weights(m_samples.size(), 0.0f);

    if (m_samples.empty()) return weights;

    switch (m_blendMode) {
        case BlendMode::Directional:
            ComputeWeightsDirectional(position, weights);
            break;
        case BlendMode::FreeformDirectional:
        case BlendMode::FreeformCartesian:
            ComputeWeightsFreeform(position, weights);
            break;
    }

    return weights;
}

void BlendSpace2D::ComputeWeightsDirectional(const glm::vec2& pos, std::vector<float>& weights) const {
    if (m_samples.size() == 1) {
        weights[0] = 1.0f;
        return;
    }

    if (m_samples.size() == 2) {
        float d0 = glm::distance(pos, m_samples[0].position);
        float d1 = glm::distance(pos, m_samples[1].position);
        float total = d0 + d1;
        if (total > 0.001f) {
            weights[0] = d1 / total;
            weights[1] = d0 / total;
        } else {
            weights[0] = weights[1] = 0.5f;
        }
        return;
    }

    // Find containing triangle and use barycentric coordinates
    int triIdx = FindContainingTriangle(pos);
    if (triIdx >= 0) {
        const auto& tri = m_triangles[triIdx];
        glm::vec3 bary = ComputeBarycentric(pos, tri);
        weights[tri.indices[0]] = bary.x;
        weights[tri.indices[1]] = bary.y;
        weights[tri.indices[2]] = bary.z;
    } else {
        // Outside triangulation - use inverse distance weighting
        ComputeWeightsFreeform(pos, weights);
    }
}

void BlendSpace2D::ComputeWeightsFreeform(const glm::vec2& pos, std::vector<float>& weights) const {
    float totalWeight = 0.0f;

    for (size_t i = 0; i < m_samples.size(); ++i) {
        float dist = glm::distance(pos, m_samples[i].position);
        weights[i] = 1.0f / (dist + 0.001f);
        totalWeight += weights[i];
    }

    if (totalWeight > 0.0f) {
        for (float& w : weights) {
            w /= totalWeight;
        }
    }
}

void BlendSpace2D::Reset() {
    m_currentTime = 0.0f;
    m_normalizedTime = 0.0f;
}

void BlendSpace2D::ComputeMotionData() {
    for (auto& sample : m_samples) {
        if (!sample.clip || !m_skeleton) continue;

        float duration = sample.clip->GetDuration();
        if (duration <= 0.0f) continue;

        int rootBoneIndex = m_skeleton->GetBoneIndex(m_rootBoneName);
        if (rootBoneIndex < 0) continue;

        glm::vec3 startPos(0.0f), endPos(0.0f);

        auto startTransforms = sample.clip->Evaluate(0.0f);
        auto endTransforms = sample.clip->Evaluate(duration);

        auto it = startTransforms.find(m_rootBoneName);
        if (it != startTransforms.end()) {
            startPos = BoneTransform::FromMatrix(it->second).position;
        }

        it = endTransforms.find(m_rootBoneName);
        if (it != endTransforms.end()) {
            endPos = BoneTransform::FromMatrix(it->second).position;
        }

        glm::vec3 motion = endPos - startPos;
        sample.motionSpeed = glm::length(motion) / duration;

        if (glm::length(glm::vec2(motion.x, motion.z)) > 0.001f) {
            sample.motionDirection = glm::normalize(glm::vec2(motion.x, motion.z));
        }
    }
}

std::vector<std::vector<AnimationPose>> BlendSpace2D::GeneratePreviewGrid(
    size_t gridSize, float normalizedTime) const {

    std::vector<std::vector<AnimationPose>> grid(gridSize);

    for (size_t y = 0; y < gridSize; ++y) {
        grid[y].resize(gridSize);
        for (size_t x = 0; x < gridSize; ++x) {
            float px = m_minBounds.x + (m_maxBounds.x - m_minBounds.x) * (static_cast<float>(x) / (gridSize - 1));
            float py = m_minBounds.y + (m_maxBounds.y - m_minBounds.y) * (static_cast<float>(y) / (gridSize - 1));
            grid[y][x] = Preview(glm::vec2(px, py), normalizedTime);
        }
    }

    return grid;
}

std::string BlendSpace2D::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"parameterX\": \"" << m_parameterX << "\",\n";
    ss << "  \"parameterY\": \"" << m_parameterY << "\",\n";
    ss << "  \"minBounds\": [" << m_minBounds.x << ", " << m_minBounds.y << "],\n";
    ss << "  \"maxBounds\": [" << m_maxBounds.x << ", " << m_maxBounds.y << "],\n";
    ss << "  \"blendMode\": \"" <<
        (m_blendMode == BlendMode::FreeformDirectional ? "freeform_directional" :
         m_blendMode == BlendMode::FreeformCartesian ? "freeform_cartesian" : "directional") << "\",\n";
    ss << "  \"samples\": [\n";

    for (size_t i = 0; i < m_samples.size(); ++i) {
        const auto& s = m_samples[i];
        ss << "    {\n";
        ss << "      \"clipId\": \"" << s.clipId << "\",\n";
        ss << "      \"position\": [" << s.position.x << ", " << s.position.y << "],\n";
        ss << "      \"playbackSpeed\": " << s.playbackSpeed << "\n";
        ss << "    }" << (i < m_samples.size() - 1 ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

bool BlendSpace2D::FromJson(const std::string& json) {
    return true;
}

std::unique_ptr<Blend2DNode> BlendSpace2D::CreateBlendNode() const {
    auto node = std::make_unique<Blend2DNode>(m_parameterX, m_parameterY);
    node->SetName(m_name);

    for (const auto& sample : m_samples) {
        auto clipNode = std::make_unique<ClipNode>(sample.clip);
        clipNode->SetSpeed(sample.playbackSpeed);
        node->AddPoint(std::move(clipNode), sample.position, sample.playbackSpeed);
    }

    return node;
}

// =============================================================================
// BlendSpace2DBuilder
// =============================================================================

BlendSpace2DBuilder& BlendSpace2DBuilder::SetName(const std::string& name) {
    m_blendSpace->SetName(name);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::SetParameters(const std::string& paramX, const std::string& paramY) {
    m_blendSpace->SetParameterX(paramX);
    m_blendSpace->SetParameterY(paramY);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::SetBoundsX(float min, float max) {
    m_blendSpace->SetParameterRangeX(min, max);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::SetBoundsY(float min, float max) {
    m_blendSpace->SetParameterRangeY(min, max);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::SetSkeleton(Skeleton* skeleton) {
    m_blendSpace->SetSkeleton(skeleton);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::AddSample(const Animation* clip, float x, float y, float speed) {
    m_blendSpace->AddSample(clip, glm::vec2(x, y), speed);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::SetBlendMode(BlendSpace2D::BlendMode mode) {
    m_blendSpace->SetBlendMode(mode);
    return *this;
}

BlendSpace2DBuilder& BlendSpace2DBuilder::EnableRootMotion(bool enabled) {
    m_blendSpace->SetRootMotionEnabled(enabled);
    return *this;
}

std::unique_ptr<BlendSpace2D> BlendSpace2DBuilder::Build() {
    m_blendSpace->RebuildTriangulation();
    return std::move(m_blendSpace);
}

} // namespace Nova
