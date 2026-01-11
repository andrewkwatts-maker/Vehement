#include "SDFSkeletalAnimation.hpp"
#include "../sdf/SDFModel.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <chrono>

// OpenGL for GPU buffers
#ifdef _WIN32
    #include <glad/glad.h>
#else
    #include <GL/gl.h>
#endif

namespace Nova {

// ============================================================================
// PrimitiveBoneInfluence Implementation
// ============================================================================

void PrimitiveBoneInfluence::NormalizeWeights() {
    float total = boneWeights[0] + boneWeights[1] + boneWeights[2] + boneWeights[3];

    if (total > 0.0001f) {
        boneWeights[0] /= total;
        boneWeights[1] /= total;
        boneWeights[2] /= total;
        boneWeights[3] /= total;
    }
}

void PrimitiveBoneInfluence::AddInfluence(int boneIndex, float weight) {
    // Find insertion point (keep sorted by weight, descending)
    int insertIndex = -1;
    for (int i = 0; i < 4; ++i) {
        if (weight > boneWeights[i]) {
            insertIndex = i;
            break;
        }
    }

    if (insertIndex < 0) {
        return; // Weight too small
    }

    // Shift existing influences down
    for (int i = 3; i > insertIndex; --i) {
        boneIndices[i] = boneIndices[i - 1];
        boneWeights[i] = boneWeights[i - 1];
    }

    // Insert new influence
    boneIndices[insertIndex] = boneIndex;
    boneWeights[insertIndex] = weight;

    NormalizeWeights();
}

// ============================================================================
// SDFAnimationStatistics Implementation
// ============================================================================

void SDFAnimationStatistics::Reset() {
    totalBindings = 0;
    animatedThisFrame = 0;
    skippedByDistance = 0;
    totalPrimitivesAnimated = 0;
    updateTimeMs = 0.0f;
    avgBonesPerPrimitive = 0.0f;
}

std::string SDFAnimationStatistics::ToString() const {
    std::stringstream ss;
    ss << "SDF Animation Statistics:\n";
    ss << "  Total Bindings: " << totalBindings << "\n";
    ss << "  Animated This Frame: " << animatedThisFrame << "\n";
    ss << "  Skipped by Distance: " << skippedByDistance << "\n";
    ss << "  Total Primitives Animated: " << totalPrimitivesAnimated << "\n";
    ss << "  Average Bones per Primitive: " << avgBonesPerPrimitive << "\n";
    ss << "  Update Time: " << updateTimeMs << " ms\n";
    return ss.str();
}

// ============================================================================
// DualQuaternion Implementation
// ============================================================================

DualQuaternion DualQuaternion::FromMatrix(const glm::mat4& matrix) {
    // Extract rotation
    glm::quat rotation = glm::quat_cast(matrix);

    // Extract translation
    glm::vec3 translation(matrix[3]);

    // Create dual quaternion
    DualQuaternion dq;
    dq.real = rotation;

    // Dual part = 0.5 * translation * rotation
    glm::quat translationQuat(0, translation.x, translation.y, translation.z);
    dq.dual = 0.5f * (translationQuat * rotation);

    return dq;
}

glm::mat4 DualQuaternion::ToMatrix() const {
    // Normalize
    DualQuaternion normalized = *this;
    normalized.Normalize();

    // Extract rotation matrix
    glm::mat4 rotationMat = glm::toMat4(normalized.real);

    // Extract translation
    glm::quat translationQuat = 2.0f * (normalized.dual * glm::conjugate(normalized.real));
    glm::vec3 translation(translationQuat.x, translationQuat.y, translationQuat.z);

    // Build final matrix
    glm::mat4 result = rotationMat;
    result[3] = glm::vec4(translation, 1.0f);

    return result;
}

DualQuaternion DualQuaternion::operator*(float scalar) const {
    return DualQuaternion(real * scalar, dual * scalar);
}

DualQuaternion DualQuaternion::operator+(const DualQuaternion& other) const {
    return DualQuaternion(real + other.real, dual + other.dual);
}

void DualQuaternion::Normalize() {
    float length = glm::length(real);
    if (length > 0.0001f) {
        real /= length;
        dual /= length;
    }
}

// ============================================================================
// SDFSkeletalAnimationSystem Implementation
// ============================================================================

const std::vector<glm::mat4> SDFSkeletalAnimationSystem::s_emptyTransforms;

SDFSkeletalAnimationSystem::SDFSkeletalAnimationSystem() = default;

SDFSkeletalAnimationSystem::~SDFSkeletalAnimationSystem() {
    CleanupGPUBuffers();
}

// ============================================================================
// Binding
// ============================================================================

void SDFSkeletalAnimationSystem::BindModelToSkeleton(
    uint32_t modelId,
    const SDFModel& model,
    const Skeleton& skeleton,
    bool autoComputeWeights)
{
    SDFSkeletonBinding binding;
    binding.modelId = modelId;
    binding.skeleton = &skeleton;

    // Get all primitives
    auto primitives = model.GetAllPrimitives();

    binding.primitiveInfluences.resize(primitives.size());
    binding.primitiveIds.resize(primitives.size());
    binding.bindPoseTransforms.resize(primitives.size());
    binding.animatedTransforms.resize(primitives.size(), glm::mat4(1.0f));

    // Store primitive IDs and bind poses
    for (size_t i = 0; i < primitives.size(); ++i) {
        binding.primitiveIds[i] = primitives[i]->GetId();
        binding.bindPoseTransforms[i] = primitives[i]->GetWorldTransform();
    }

    // Compute bone weights if requested
    if (autoComputeWeights) {
        m_bindings[modelId] = binding;
        ComputeBoneWeights(modelId, model, skeleton);
    } else {
        m_bindings[modelId] = binding;
    }
}

void SDFSkeletalAnimationSystem::UnbindModel(uint32_t modelId) {
    m_bindings.erase(modelId);
    m_modelDistances.erase(modelId);

    // Cleanup GPU buffer
    auto it = m_gpuBuffers.find(modelId);
    if (it != m_gpuBuffers.end()) {
        glDeleteBuffers(1, &it->second);
        m_gpuBuffers.erase(it);
    }
}

bool SDFSkeletalAnimationSystem::IsModelBound(uint32_t modelId) const {
    return m_bindings.find(modelId) != m_bindings.end();
}

const SDFSkeletonBinding* SDFSkeletalAnimationSystem::GetBinding(uint32_t modelId) const {
    auto it = m_bindings.find(modelId);
    return it != m_bindings.end() ? &it->second : nullptr;
}

// ============================================================================
// Bone Weight Computation
// ============================================================================

void SDFSkeletalAnimationSystem::ComputeBoneWeights(
    uint32_t modelId,
    const SDFModel& model,
    const Skeleton& skeleton)
{
    auto it = m_bindings.find(modelId);
    if (it == m_bindings.end()) {
        return;
    }

    SDFSkeletonBinding& binding = it->second;
    auto primitives = model.GetAllPrimitives();

    // Calculate bone matrices in world space
    std::vector<glm::mat4> boneMatrices = skeleton.GetBindPoseMatrices();

    // Compute weights for each primitive
    for (size_t i = 0; i < primitives.size(); ++i) {
        SDFTransform worldTransform = primitives[i]->GetWorldTransform();
        glm::mat4 worldMatrix = worldTransform.ToMatrix();

        binding.primitiveInfluences[i] = ComputePrimitiveBoneWeights(
            *primitives[i], skeleton, worldMatrix);
    }

    binding.dirty = true;
}

PrimitiveBoneInfluence SDFSkeletalAnimationSystem::ComputePrimitiveBoneWeights(
    const SDFPrimitive& primitive,
    const Skeleton& skeleton,
    const glm::mat4& primitiveWorldTransform) const
{
    PrimitiveBoneInfluence influence;

    // Get primitive center position in world space
    glm::vec3 primitiveCenter = glm::vec3(primitiveWorldTransform[3]);

    // Calculate bone matrices
    std::vector<glm::mat4> boneMatrices = skeleton.GetBindPoseMatrices();

    // Find nearest bones
    auto nearestBones = BoneWeightUtils::FindNearestBones(
        primitiveCenter, skeleton, boneMatrices, m_quality.maxInfluencesPerPrimitive);

    // Convert to influence
    for (size_t i = 0; i < nearestBones.size() && i < 4; ++i) {
        influence.boneIndices[i] = nearestBones[i].first;
    }

    // Calculate weights from distances
    std::vector<float> distances;
    for (const auto& [idx, dist] : nearestBones) {
        distances.push_back(dist);
    }

    std::vector<float> weights = BoneWeightUtils::DistancesToWeights(distances);

    for (size_t i = 0; i < weights.size() && i < 4; ++i) {
        influence.boneWeights[i] = weights[i];
    }

    influence.NormalizeWeights();

    return influence;
}

void SDFSkeletalAnimationSystem::SetPrimitiveBoneWeights(
    uint32_t modelId,
    uint32_t primitiveId,
    const PrimitiveBoneInfluence& influence)
{
    auto it = m_bindings.find(modelId);
    if (it == m_bindings.end()) {
        return;
    }

    SDFSkeletonBinding& binding = it->second;

    // Find primitive index
    for (size_t i = 0; i < binding.primitiveIds.size(); ++i) {
        if (binding.primitiveIds[i] == primitiveId) {
            binding.primitiveInfluences[i] = influence;
            binding.dirty = true;
            break;
        }
    }
}

// ============================================================================
// Animation Update
// ============================================================================

void SDFSkeletalAnimationSystem::UpdateAnimation(
    uint32_t modelId,
    const Skeleton& skeleton,
    float deltaTime,
    const glm::mat4& modelWorldTransform)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    auto it = m_bindings.find(modelId);
    if (it == m_bindings.end()) {
        return;
    }

    SDFSkeletonBinding& binding = it->second;

    // Check LOD distance culling
    if (m_quality.enableLODOptimization) {
        auto distIt = m_modelDistances.find(modelId);
        if (distIt != m_modelDistances.end() &&
            distIt->second > m_quality.maxAnimationDistance) {
            m_statistics.skippedByDistance++;
            return;
        }
    }

    // Calculate bone matrices
    std::vector<glm::mat4> boneMatrices = skeleton.GetBindPoseMatrices();

    // Update each primitive transform
    for (size_t i = 0; i < binding.primitiveInfluences.size(); ++i) {
        const PrimitiveBoneInfluence& influence = binding.primitiveInfluences[i];

        if (!influence.HasInfluence()) {
            // No bone influence - use bind pose
            binding.animatedTransforms[i] = binding.bindPoseTransforms[i].ToMatrix();
            continue;
        }

        // Calculate skinned transform
        glm::mat4 skinnedTransform;

        if (m_quality.enableDualQuaternionSkinning) {
            skinnedTransform = CalculateDualQuaternionTransform(
                influence, boneMatrices, binding.bindPoseTransforms[i]);
        } else {
            skinnedTransform = CalculateSkinnedTransform(
                influence, boneMatrices, binding.bindPoseTransforms[i]);
        }

        // Apply model world transform
        binding.animatedTransforms[i] = modelWorldTransform * skinnedTransform;
    }

    binding.lastUpdateTime = deltaTime;
    binding.dirty = false;
    m_statistics.animatedThisFrame++;
    m_statistics.totalPrimitivesAnimated += static_cast<int>(binding.primitiveInfluences.size());

    auto endTime = std::chrono::high_resolution_clock::now();
    m_statistics.updateTimeMs += std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void SDFSkeletalAnimationSystem::UpdateAllAnimations(float deltaTime) {
    m_statistics.Reset();
    m_statistics.totalBindings = static_cast<int>(m_bindings.size());

    for (auto& [modelId, binding] : m_bindings) {
        if (binding.skeleton) {
            UpdateAnimation(modelId, *binding.skeleton, deltaTime);
        }
    }
}

void SDFSkeletalAnimationSystem::SetModelDistance(uint32_t modelId, float distance) {
    m_modelDistances[modelId] = distance;
}

// ============================================================================
// Transform Queries
// ============================================================================

const std::vector<glm::mat4>& SDFSkeletalAnimationSystem::GetAnimatedPrimitiveTransforms(
    uint32_t modelId) const
{
    auto it = m_bindings.find(modelId);
    if (it != m_bindings.end()) {
        return it->second.animatedTransforms;
    }
    return s_emptyTransforms;
}

std::vector<AnimatedPrimitiveGPUData> SDFSkeletalAnimationSystem::GetGPUData(
    uint32_t modelId,
    const SDFModel& model) const
{
    std::vector<AnimatedPrimitiveGPUData> gpuData;

    auto it = m_bindings.find(modelId);
    if (it == m_bindings.end()) {
        return gpuData;
    }

    const SDFSkeletonBinding& binding = it->second;
    auto primitives = model.GetAllPrimitives();

    gpuData.reserve(primitives.size());

    for (size_t i = 0; i < primitives.size() && i < binding.animatedTransforms.size(); ++i) {
        AnimatedPrimitiveGPUData data;
        data.transform = binding.animatedTransforms[i];
        data.inverseTransform = glm::inverse(data.transform);

        // Fill in primitive parameters
        const SDFPrimitive* prim = primitives[i];
        const SDFParameters& params = prim->GetParameters();

        data.parameters = glm::vec4(
            params.radius,
            params.dimensions.x,
            params.dimensions.y,
            params.dimensions.z
        );

        const SDFMaterial& mat = prim->GetMaterial();
        data.material = glm::vec4(
            mat.metallic,
            mat.roughness,
            mat.emissive,
            0.0f
        );

        data.primitiveType = static_cast<int>(prim->GetType());
        data.csgOperation = static_cast<int>(prim->GetCSGOperation());

        gpuData.push_back(data);
    }

    return gpuData;
}

glm::mat4 SDFSkeletalAnimationSystem::GetPrimitiveTransform(
    uint32_t modelId,
    uint32_t primitiveId) const
{
    auto it = m_bindings.find(modelId);
    if (it == m_bindings.end()) {
        return glm::mat4(1.0f);
    }

    const SDFSkeletonBinding& binding = it->second;

    // Find primitive index
    for (size_t i = 0; i < binding.primitiveIds.size(); ++i) {
        if (binding.primitiveIds[i] == primitiveId) {
            return binding.animatedTransforms[i];
        }
    }

    return glm::mat4(1.0f);
}

// ============================================================================
// Settings
// ============================================================================

void SDFSkeletalAnimationSystem::SetQualitySettings(const SDFAnimationQuality& quality) {
    m_quality = quality;
}

// ============================================================================
// GPU Buffer Management
// ============================================================================

unsigned int SDFSkeletalAnimationSystem::GetOrCreateGPUBuffer(uint32_t modelId) {
    auto it = m_gpuBuffers.find(modelId);
    if (it != m_gpuBuffers.end()) {
        return it->second;
    }

    // Create new SSBO
    unsigned int ssbo;
    glGenBuffers(1, &ssbo);
    m_gpuBuffers[modelId] = ssbo;

    return ssbo;
}

void SDFSkeletalAnimationSystem::UploadToGPU(uint32_t modelId, const SDFModel& model) {
    unsigned int ssbo = GetOrCreateGPUBuffer(modelId);
    std::vector<AnimatedPrimitiveGPUData> gpuData = GetGPUData(modelId, model);

    if (gpuData.empty()) {
        return;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuData.size() * sizeof(AnimatedPrimitiveGPUData),
                 gpuData.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SDFSkeletalAnimationSystem::CleanupGPUBuffers() {
    for (auto& [modelId, ssbo] : m_gpuBuffers) {
        glDeleteBuffers(1, &ssbo);
    }
    m_gpuBuffers.clear();
}

// ============================================================================
// Skinning Algorithms
// ============================================================================

glm::mat4 SDFSkeletalAnimationSystem::CalculateSkinnedTransform(
    const PrimitiveBoneInfluence& influence,
    const std::vector<glm::mat4>& boneMatrices,
    const SDFTransform& bindPoseTransform) const
{
    glm::mat4 skinnedMatrix(0.0f);

    // Linear blend skinning
    for (int i = 0; i < 4; ++i) {
        int boneIndex = influence.boneIndices[i];
        float weight = influence.boneWeights[i];

        if (boneIndex >= 0 && boneIndex < static_cast<int>(boneMatrices.size()) && weight > 0.0f) {
            skinnedMatrix += boneMatrices[boneIndex] * weight;
        }
    }

    return skinnedMatrix * bindPoseTransform.ToMatrix();
}

glm::mat4 SDFSkeletalAnimationSystem::CalculateDualQuaternionTransform(
    const PrimitiveBoneInfluence& influence,
    const std::vector<glm::mat4>& boneMatrices,
    const SDFTransform& bindPoseTransform) const
{
    // Convert bone matrices to dual quaternions and blend
    DualQuaternion blendedDQ;
    bool first = true;

    for (int i = 0; i < 4; ++i) {
        int boneIndex = influence.boneIndices[i];
        float weight = influence.boneWeights[i];

        if (boneIndex >= 0 && boneIndex < static_cast<int>(boneMatrices.size()) && weight > 0.0f) {
            DualQuaternion boneDQ = DualQuaternion::FromMatrix(boneMatrices[boneIndex]);

            // Ensure consistent quaternion hemisphere
            if (!first && glm::dot(boneDQ.real, blendedDQ.real) < 0.0f) {
                weight = -weight;
            }

            blendedDQ = blendedDQ + boneDQ * weight;
            first = false;
        }
    }

    blendedDQ.Normalize();
    glm::mat4 skinnedMatrix = blendedDQ.ToMatrix();

    return skinnedMatrix * bindPoseTransform.ToMatrix();
}

void SDFSkeletalAnimationSystem::DecomposeMatrix(
    const glm::mat4& matrix,
    glm::vec3& outTranslation,
    glm::quat& outRotation,
    glm::vec3& outScale) const
{
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(matrix, outScale, outRotation, outTranslation, skew, perspective);
}

// ============================================================================
// BoneWeightUtils Implementation
// ============================================================================

namespace BoneWeightUtils {

float DistanceToBone(const glm::vec3& point, const Bone& bone, const glm::mat4& boneWorldTransform) {
    glm::vec3 bonePosition = glm::vec3(boneWorldTransform[3]);
    return glm::length(point - bonePosition);
}

std::vector<std::pair<int, float>> FindNearestBones(
    const glm::vec3& point,
    const Skeleton& skeleton,
    const std::vector<glm::mat4>& boneWorldTransforms,
    int k)
{
    std::vector<std::pair<int, float>> boneDistances;
    const auto& bones = skeleton.GetBones();

    // Calculate distances to all bones
    for (size_t i = 0; i < bones.size(); ++i) {
        if (i < boneWorldTransforms.size()) {
            float dist = DistanceToBone(point, bones[i], boneWorldTransforms[i]);
            boneDistances.emplace_back(static_cast<int>(i), dist);
        }
    }

    // Sort by distance
    std::sort(boneDistances.begin(), boneDistances.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Return top K
    if (boneDistances.size() > static_cast<size_t>(k)) {
        boneDistances.resize(k);
    }

    return boneDistances;
}

std::vector<float> DistancesToWeights(const std::vector<float>& distances) {
    std::vector<float> weights;
    weights.reserve(distances.size());

    if (distances.empty()) {
        return weights;
    }

    // Convert distances to weights using inverse distance
    float totalWeight = 0.0f;
    for (float dist : distances) {
        float weight = 1.0f / (dist + 0.0001f); // Add small epsilon to avoid division by zero
        weights.push_back(weight);
        totalWeight += weight;
    }

    // Normalize
    if (totalWeight > 0.0001f) {
        for (float& weight : weights) {
            weight /= totalWeight;
        }
    }

    return weights;
}

} // namespace BoneWeightUtils

} // namespace Nova
