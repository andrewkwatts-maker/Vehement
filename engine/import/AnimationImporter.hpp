#pragma once

#include "ImportSettings.hpp"
#include "ImportProgress.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class Animation;
class Skeleton;

// ============================================================================
// Animation Data Structures
// ============================================================================

/**
 * @brief Animation keyframe data
 */
struct ImportedKeyframe {
    float time = 0.0f;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

/**
 * @brief Animation channel for a single bone
 */
struct ImportedChannel {
    std::string boneName;
    std::vector<ImportedKeyframe> keyframes;
    bool hasPosition = true;
    bool hasRotation = true;
    bool hasScale = true;
};

/**
 * @brief Animation event marker
 */
struct AnimationEvent {
    std::string name;
    float time = 0.0f;
    std::string stringParam;
    int intParam = 0;
    float floatParam = 0.0f;
};

/**
 * @brief Animation clip definition
 */
struct ImportedClip {
    std::string name;
    float startTime = 0.0f;
    float endTime = 0.0f;
    float duration = 0.0f;
    bool looping = false;
    std::vector<ImportedChannel> channels;
    std::vector<AnimationEvent> events;

    // Root motion data
    glm::vec3 rootMotionDelta{0.0f};
    float rootRotationDelta = 0.0f;
    bool hasRootMotion = false;
};

/**
 * @brief Bone mapping for retargeting
 */
struct BoneMapping {
    std::string sourceBone;
    std::string targetBone;
    glm::quat rotationOffset{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scaleOffset{1.0f};
};

/**
 * @brief Retargeting configuration
 */
struct RetargetConfig {
    std::vector<BoneMapping> mappings;
    std::string sourceSkeleton;
    std::string targetSkeleton;
    bool preserveHeight = true;
    bool preserveProportions = true;
};

/**
 * @brief Imported animation result
 */
struct ImportedAnimation {
    std::string sourcePath;
    std::string outputPath;
    std::string assetId;

    // Animation clips
    std::vector<ImportedClip> clips;

    // Original data
    float originalDuration = 0.0f;
    float originalSampleRate = 30.0f;
    std::vector<std::string> boneNames;

    // Statistics
    uint32_t totalKeyframes = 0;
    uint32_t totalChannels = 0;
    uint32_t totalClips = 0;

    // Compression info
    size_t originalSize = 0;
    size_t compressedSize = 0;
    float compressionRatio = 1.0f;

    // Import info
    bool success = false;
    std::string errorMessage;
    std::vector<std::string> warnings;
};

// ============================================================================
// Animation Importer
// ============================================================================

/**
 * @brief Comprehensive animation import pipeline
 *
 * Supports: FBX animations, GLTF animations, BVH
 *
 * Features:
 * - Skeletal animation extraction
 * - Animation clip splitting by markers
 * - Root motion extraction
 * - Animation retargeting
 * - Keyframe compression
 * - Loop detection and fixing
 * - Additive animation generation
 * - Animation event import
 */
class AnimationImporter {
public:
    AnimationImporter();
    ~AnimationImporter();

    // Non-copyable
    AnimationImporter(const AnimationImporter&) = delete;
    AnimationImporter& operator=(const AnimationImporter&) = delete;

    // -------------------------------------------------------------------------
    // Animation Import
    // -------------------------------------------------------------------------

    /**
     * @brief Import animations from file
     */
    ImportedAnimation Import(const std::string& path, const AnimationImportSettings& settings,
                              ImportProgress* progress = nullptr);

    /**
     * @brief Import with default settings
     */
    ImportedAnimation Import(const std::string& path);

    /**
     * @brief Import from model file (extract embedded animations)
     */
    std::vector<ImportedAnimation> ImportFromModel(const std::string& modelPath,
                                                     const AnimationImportSettings& settings,
                                                     ImportProgress* progress = nullptr);

    // -------------------------------------------------------------------------
    // Format-Specific Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load BVH file
     */
    ImportedAnimation LoadBVH(const std::string& path, ImportProgress* progress = nullptr);

    /**
     * @brief Load FBX animations
     */
    ImportedAnimation LoadFBXAnimation(const std::string& path, ImportProgress* progress = nullptr);

    /**
     * @brief Load GLTF animations
     */
    ImportedAnimation LoadGLTFAnimation(const std::string& path, ImportProgress* progress = nullptr);

    // -------------------------------------------------------------------------
    // Clip Splitting
    // -------------------------------------------------------------------------

    /**
     * @brief Split animation by time markers
     */
    std::vector<ImportedClip> SplitByMarkers(const ImportedClip& animation,
                                               const std::vector<std::pair<float, std::string>>& markers);

    /**
     * @brief Split animation by time ranges
     */
    std::vector<ImportedClip> SplitByRanges(const ImportedClip& animation,
                                              const std::vector<std::tuple<std::string, float, float>>& ranges);

    /**
     * @brief Auto-detect clip boundaries (by motion analysis)
     */
    std::vector<std::pair<float, float>> DetectClipBoundaries(const ImportedClip& animation);

    /**
     * @brief Extract single clip
     */
    ImportedClip ExtractClip(const ImportedClip& animation, float startTime, float endTime,
                              const std::string& name = "");

    // -------------------------------------------------------------------------
    // Root Motion
    // -------------------------------------------------------------------------

    /**
     * @brief Extract root motion from animation
     */
    void ExtractRootMotion(ImportedClip& animation, const std::string& rootBoneName,
                           bool extractTranslation = true, bool extractRotation = true);

    /**
     * @brief Lock root position on specific axes
     */
    void LockRootPosition(ImportedClip& animation, const std::string& rootBoneName,
                          bool lockX = false, bool lockY = false, bool lockZ = false);

    /**
     * @brief Bake root motion back into animation
     */
    void BakeRootMotion(ImportedClip& animation, const std::string& rootBoneName,
                        const glm::vec3& motionDelta, float rotationDelta);

    // -------------------------------------------------------------------------
    // Compression
    // -------------------------------------------------------------------------

    /**
     * @brief Compress animation using keyframe reduction
     */
    void Compress(ImportedClip& animation, float positionTolerance = 0.001f,
                  float rotationTolerance = 0.0001f, float scaleTolerance = 0.001f);

    /**
     * @brief Remove redundant keyframes
     */
    void RemoveRedundantKeyframes(ImportedChannel& channel, float tolerance);

    /**
     * @brief Resample animation to new frame rate
     */
    void Resample(ImportedClip& animation, float targetFrameRate);

    /**
     * @brief Estimate compressed size
     */
    size_t EstimateCompressedSize(const ImportedClip& animation);

    // -------------------------------------------------------------------------
    // Loop Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Detect if animation loops
     */
    bool DetectLoop(const ImportedClip& animation, float threshold = 0.01f);

    /**
     * @brief Fix loop by interpolating end to start
     */
    void FixLoop(ImportedClip& animation, float blendDuration = 0.1f);

    /**
     * @brief Make animation loop seamlessly
     */
    void MakeLoopable(ImportedClip& animation);

    // -------------------------------------------------------------------------
    // Additive Animations
    // -------------------------------------------------------------------------

    /**
     * @brief Create additive animation from reference pose
     */
    ImportedClip MakeAdditive(const ImportedClip& animation, const ImportedClip& referencePose,
                               float referenceFrame = 0.0f);

    /**
     * @brief Create additive animation from first frame
     */
    ImportedClip MakeAdditiveFromFirstFrame(const ImportedClip& animation);

    /**
     * @brief Apply additive animation to base
     */
    ImportedClip ApplyAdditive(const ImportedClip& baseAnimation, const ImportedClip& additiveAnimation,
                                float weight = 1.0f);

    // -------------------------------------------------------------------------
    // Retargeting
    // -------------------------------------------------------------------------

    /**
     * @brief Retarget animation to different skeleton
     */
    ImportedClip Retarget(const ImportedClip& animation, const RetargetConfig& config,
                           const Skeleton* sourceSkeleton, const Skeleton* targetSkeleton);

    /**
     * @brief Auto-generate bone mapping by name matching
     */
    std::vector<BoneMapping> AutoGenerateBoneMapping(const std::vector<std::string>& sourceBones,
                                                       const std::vector<std::string>& targetBones);

    /**
     * @brief Validate bone mapping
     */
    bool ValidateBoneMapping(const std::vector<BoneMapping>& mappings,
                              const std::vector<std::string>& targetBones);

    // -------------------------------------------------------------------------
    // Event Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Import animation events from file
     */
    std::vector<AnimationEvent> ImportEvents(const std::string& path);

    /**
     * @brief Add event to animation
     */
    void AddEvent(ImportedClip& animation, const AnimationEvent& event);

    /**
     * @brief Find events at time
     */
    std::vector<AnimationEvent> FindEventsAtTime(const ImportedClip& animation,
                                                   float time, float tolerance = 0.01f);

    // -------------------------------------------------------------------------
    // IK Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Bake IK constraints into animation
     */
    void BakeIK(ImportedClip& animation, const std::string& endEffector,
                const std::string& ikTarget, const std::vector<std::string>& ikChain);

    // -------------------------------------------------------------------------
    // Utilities
    // -------------------------------------------------------------------------

    /**
     * @brief Interpolate keyframe at time
     */
    ImportedKeyframe InterpolateKeyframe(const ImportedChannel& channel, float time);

    /**
     * @brief Calculate animation duration
     */
    float CalculateDuration(const ImportedClip& animation);

    /**
     * @brief Get bone transform at time
     */
    glm::mat4 GetBoneTransform(const ImportedClip& animation, const std::string& boneName, float time);

    // -------------------------------------------------------------------------
    // File Format Support
    // -------------------------------------------------------------------------

    /**
     * @brief Check if format is supported
     */
    static bool IsFormatSupported(const std::string& extension);

    /**
     * @brief Get supported extensions
     */
    static std::vector<std::string> GetSupportedExtensions();

    // -------------------------------------------------------------------------
    // Output
    // -------------------------------------------------------------------------

    /**
     * @brief Save to engine format
     */
    bool SaveEngineFormat(const ImportedAnimation& animation, const std::string& path);

    /**
     * @brief Export single clip
     */
    bool ExportClip(const ImportedClip& clip, const std::string& path);

    /**
     * @brief Export metadata
     */
    std::string ExportMetadata(const ImportedAnimation& animation);

private:
    // BVH parsing helpers
    struct BVHJoint {
        std::string name;
        int parentIndex = -1;
        glm::vec3 offset{0.0f};
        std::vector<std::string> channels;
        std::vector<int> childIndices;
    };

    std::vector<BVHJoint> ParseBVHHierarchy(std::istream& stream);
    void ParseBVHMotion(std::istream& stream, std::vector<BVHJoint>& joints,
                         ImportedClip& clip, int frameCount, float frameTime);

    // Compression helpers
    float ComputeKeyframeError(const ImportedKeyframe& a, const ImportedKeyframe& b,
                                const ImportedKeyframe& interpolated);

    bool IsKeyframeRedundant(const std::vector<ImportedKeyframe>& keyframes, size_t index,
                              float posTol, float rotTol, float scaleTol);
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Interpolate between two keyframes
 */
ImportedKeyframe InterpolateKeyframes(const ImportedKeyframe& a, const ImportedKeyframe& b, float t);

/**
 * @brief Compose transform from keyframe
 */
glm::mat4 KeyframeToMatrix(const ImportedKeyframe& kf);

/**
 * @brief Decompose transform to keyframe
 */
ImportedKeyframe MatrixToKeyframe(const glm::mat4& matrix, float time = 0.0f);

/**
 * @brief Calculate keyframe difference
 */
float KeyframeDifference(const ImportedKeyframe& a, const ImportedKeyframe& b);

} // namespace Nova
