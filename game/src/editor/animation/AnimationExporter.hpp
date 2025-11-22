#pragma once

#include "BoneAnimationEditor.hpp"
#include "KeyframeEditor.hpp"
#include "AnimationTimeline.hpp"
#include <string>
#include <vector>
#include <functional>

namespace Vehement {

/**
 * @brief Export compression level
 */
enum class CompressionLevel {
    None,           // Full precision
    Low,            // Minimal compression
    Medium,         // Balanced
    High,           // Maximum compression
    Custom          // User-defined settings
};

/**
 * @brief Export format
 */
enum class ExportFormat {
    JSON,           // JSON format
    Binary,         // Binary format
    GLTF,           // GLTF animation
    FBX             // FBX (if supported)
};

/**
 * @brief Export options
 */
struct AnimationExportOptions {
    // Format
    ExportFormat format = ExportFormat::JSON;
    bool prettyPrint = true;

    // Frame rate
    float targetFrameRate = 30.0f;
    bool resampleAnimation = false;

    // Compression
    CompressionLevel compression = CompressionLevel::None;
    float positionTolerance = 0.001f;      // Position error tolerance
    float rotationTolerance = 0.001f;      // Rotation error tolerance (radians)
    float scaleTolerance = 0.001f;         // Scale error tolerance
    int positionPrecision = 6;             // Decimal places for position
    int rotationPrecision = 6;             // Decimal places for rotation
    bool removeRedundantKeyframes = true;

    // Content
    bool includeEvents = true;
    bool includeRootMotion = true;
    bool includeCurveData = true;
    bool includeMetadata = true;

    // Bones
    bool exportAllBones = true;
    std::vector<std::string> selectedBones;  // If not all bones
    bool exportBindPose = false;

    // Time range
    bool exportFullRange = true;
    float startTime = 0.0f;
    float endTime = 1.0f;
};

/**
 * @brief Animation set export options
 */
struct AnimationSetExportOptions {
    std::string setName = "animation_set";
    std::vector<std::string> animationNames;
    bool exportAsBundle = true;
    bool includeSharedSkeleton = true;
};

/**
 * @brief Export result
 */
struct ExportResult {
    bool success = false;
    std::string filePath;
    std::string errorMessage;
    size_t fileSize = 0;
    int keyframeCount = 0;
    int eventCount = 0;
    float exportDuration = 0.0f;  // Duration of animation exported
};

/**
 * @brief Animation JSON format (matching animation_clip.schema.json)
 */
struct AnimationClipJson {
    std::string name;
    float duration = 0.0f;
    float frameRate = 30.0f;
    bool looping = true;

    // Per-bone data
    struct BoneData {
        std::string boneName;
        struct KeyframeData {
            float time = 0.0f;
            std::array<float, 3> position;
            std::array<float, 4> rotation;  // quaternion xyzw
            std::array<float, 3> scale;
        };
        std::vector<KeyframeData> keyframes;

        struct CurveSettings {
            std::string interpolation = "linear";  // linear, bezier, step
        };
        CurveSettings positionCurve;
        CurveSettings rotationCurve;
        CurveSettings scaleCurve;
    };
    std::vector<BoneData> bones;

    // Events
    struct EventData {
        float time = 0.0f;
        std::string name;
        std::string parameter;
    };
    std::vector<EventData> events;

    // Root motion
    struct RootMotionData {
        bool enabled = false;
        std::string axis = "xz";  // x, y, z, xy, xz, yz, xyz
        bool bakeIntoPose = false;
    };
    RootMotionData rootMotion;

    // Metadata
    std::string author;
    std::string description;
    uint64_t createdTimestamp = 0;
    uint64_t modifiedTimestamp = 0;
    std::vector<std::string> tags;
};

/**
 * @brief Animation exporter - exports animations to JSON files
 *
 * Features:
 * - Export single animation
 * - Export animation set
 * - Compression options
 * - Target frame rate
 * - Include events
 */
class AnimationExporter {
public:
    AnimationExporter();
    ~AnimationExporter();

    /**
     * @brief Set bone animation editor reference
     */
    void SetBoneEditor(BoneAnimationEditor* editor) { m_boneEditor = editor; }

    /**
     * @brief Set keyframe editor reference
     */
    void SetKeyframeEditor(KeyframeEditor* editor) { m_keyframeEditor = editor; }

    /**
     * @brief Set timeline reference
     */
    void SetTimeline(AnimationTimeline* timeline) { m_timeline = timeline; }

    // =========================================================================
    // Single Animation Export
    // =========================================================================

    /**
     * @brief Export animation to file
     */
    ExportResult ExportAnimation(const std::string& filePath, const std::string& animationName,
                                  const AnimationExportOptions& options = {});

    /**
     * @brief Export animation to string
     */
    std::string ExportAnimationToString(const std::string& animationName,
                                         const AnimationExportOptions& options = {});

    /**
     * @brief Export current animation state
     */
    ExportResult ExportCurrentAnimation(const std::string& filePath,
                                         const AnimationExportOptions& options = {});

    // =========================================================================
    // Animation Set Export
    // =========================================================================

    /**
     * @brief Export multiple animations as a set
     */
    ExportResult ExportAnimationSet(const std::string& directoryPath,
                                     const AnimationSetExportOptions& setOptions,
                                     const AnimationExportOptions& animOptions = {});

    /**
     * @brief Export animation bundle (single file with multiple animations)
     */
    ExportResult ExportAnimationBundle(const std::string& filePath,
                                        const AnimationSetExportOptions& setOptions,
                                        const AnimationExportOptions& animOptions = {});

    // =========================================================================
    // Import
    // =========================================================================

    /**
     * @brief Import animation from file
     */
    bool ImportAnimation(const std::string& filePath);

    /**
     * @brief Import animation from string
     */
    bool ImportAnimationFromString(const std::string& jsonStr);

    /**
     * @brief Get imported animation clip data
     */
    [[nodiscard]] const AnimationClipJson& GetImportedClip() const { return m_importedClip; }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate export options
     */
    [[nodiscard]] std::vector<std::string> ValidateExportOptions(const AnimationExportOptions& options) const;

    /**
     * @brief Estimate export file size
     */
    [[nodiscard]] size_t EstimateExportSize(const AnimationExportOptions& options) const;

    // =========================================================================
    // Compression
    // =========================================================================

    /**
     * @brief Apply compression settings
     */
    void ApplyCompressionPreset(CompressionLevel level, AnimationExportOptions& options);

    /**
     * @brief Get compression statistics
     */
    struct CompressionStats {
        int originalKeyframes = 0;
        int compressedKeyframes = 0;
        float compressionRatio = 0.0f;
        float maxPositionError = 0.0f;
        float maxRotationError = 0.0f;
    };
    [[nodiscard]] CompressionStats CalculateCompressionStats(const AnimationExportOptions& options) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(float)> OnExportProgress;
    std::function<void(const std::string&)> OnExportError;
    std::function<void(const ExportResult&)> OnExportComplete;

private:
    AnimationClipJson BuildClipJson(const std::string& name, const AnimationExportOptions& options);
    void OptimizeKeyframes(AnimationClipJson& clip, const AnimationExportOptions& options);
    void ResampleAnimation(AnimationClipJson& clip, float targetFrameRate);
    std::string SerializeToJson(const AnimationClipJson& clip, bool prettyPrint);
    bool ParseFromJson(const std::string& jsonStr, AnimationClipJson& outClip);

    BoneAnimationEditor* m_boneEditor = nullptr;
    KeyframeEditor* m_keyframeEditor = nullptr;
    AnimationTimeline* m_timeline = nullptr;

    AnimationClipJson m_importedClip;
};

} // namespace Vehement
