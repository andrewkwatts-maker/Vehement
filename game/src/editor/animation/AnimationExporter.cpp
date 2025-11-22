#include "AnimationExporter.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Vehement {

AnimationExporter::AnimationExporter() = default;
AnimationExporter::~AnimationExporter() = default;

// ============================================================================
// Single Animation Export
// ============================================================================

ExportResult AnimationExporter::ExportAnimation(const std::string& filePath, const std::string& animationName,
                                                  const AnimationExportOptions& options) {
    ExportResult result;
    result.filePath = filePath;

    // Validate
    auto errors = ValidateExportOptions(options);
    if (!errors.empty()) {
        result.success = false;
        result.errorMessage = errors[0];
        if (OnExportError) {
            OnExportError(result.errorMessage);
        }
        return result;
    }

    // Build clip JSON
    AnimationClipJson clip = BuildClipJson(animationName, options);

    // Optimize if needed
    if (options.removeRedundantKeyframes) {
        OptimizeKeyframes(clip, options);
    }

    // Resample if needed
    if (options.resampleAnimation) {
        ResampleAnimation(clip, options.targetFrameRate);
    }

    // Serialize
    std::string jsonStr = SerializeToJson(clip, options.prettyPrint);

    // Write to file
    std::ofstream file(filePath);
    if (!file.is_open()) {
        result.success = false;
        result.errorMessage = "Failed to open file for writing: " + filePath;
        if (OnExportError) {
            OnExportError(result.errorMessage);
        }
        return result;
    }

    file << jsonStr;
    file.close();

    // Calculate stats
    result.success = true;
    result.fileSize = jsonStr.size();
    result.keyframeCount = 0;
    for (const auto& bone : clip.bones) {
        result.keyframeCount += static_cast<int>(bone.keyframes.size());
    }
    result.eventCount = static_cast<int>(clip.events.size());
    result.exportDuration = clip.duration;

    if (OnExportComplete) {
        OnExportComplete(result);
    }

    return result;
}

std::string AnimationExporter::ExportAnimationToString(const std::string& animationName,
                                                        const AnimationExportOptions& options) {
    AnimationClipJson clip = BuildClipJson(animationName, options);

    if (options.removeRedundantKeyframes) {
        OptimizeKeyframes(clip, options);
    }

    if (options.resampleAnimation) {
        ResampleAnimation(clip, options.targetFrameRate);
    }

    return SerializeToJson(clip, options.prettyPrint);
}

ExportResult AnimationExporter::ExportCurrentAnimation(const std::string& filePath,
                                                        const AnimationExportOptions& options) {
    return ExportAnimation(filePath, "untitled_animation", options);
}

// ============================================================================
// Animation Set Export
// ============================================================================

ExportResult AnimationExporter::ExportAnimationSet(const std::string& directoryPath,
                                                    const AnimationSetExportOptions& setOptions,
                                                    const AnimationExportOptions& animOptions) {
    ExportResult result;
    result.filePath = directoryPath;

    // Create directory if needed
    if (!fs::exists(directoryPath)) {
        fs::create_directories(directoryPath);
    }

    int totalKeyframes = 0;
    int totalEvents = 0;
    float totalDuration = 0.0f;
    size_t totalSize = 0;

    float progress = 0.0f;
    float progressStep = 1.0f / static_cast<float>(setOptions.animationNames.size());

    for (const auto& animName : setOptions.animationNames) {
        std::string filePath = directoryPath + "/" + animName + ".anim.json";
        auto animResult = ExportAnimation(filePath, animName, animOptions);

        if (!animResult.success) {
            result.success = false;
            result.errorMessage = "Failed to export " + animName + ": " + animResult.errorMessage;
            return result;
        }

        totalKeyframes += animResult.keyframeCount;
        totalEvents += animResult.eventCount;
        totalDuration += animResult.exportDuration;
        totalSize += animResult.fileSize;

        progress += progressStep;
        if (OnExportProgress) {
            OnExportProgress(progress);
        }
    }

    // Export manifest
    json manifest;
    manifest["name"] = setOptions.setName;
    manifest["animations"] = setOptions.animationNames;
    manifest["count"] = setOptions.animationNames.size();

    std::ofstream manifestFile(directoryPath + "/manifest.json");
    manifestFile << manifest.dump(2);
    manifestFile.close();

    result.success = true;
    result.keyframeCount = totalKeyframes;
    result.eventCount = totalEvents;
    result.exportDuration = totalDuration;
    result.fileSize = totalSize;

    if (OnExportComplete) {
        OnExportComplete(result);
    }

    return result;
}

ExportResult AnimationExporter::ExportAnimationBundle(const std::string& filePath,
                                                       const AnimationSetExportOptions& setOptions,
                                                       const AnimationExportOptions& animOptions) {
    ExportResult result;
    result.filePath = filePath;

    json bundle;
    bundle["name"] = setOptions.setName;
    bundle["version"] = "1.0";

    json animations = json::array();

    for (const auto& animName : setOptions.animationNames) {
        AnimationClipJson clip = BuildClipJson(animName, animOptions);

        if (animOptions.removeRedundantKeyframes) {
            OptimizeKeyframes(clip, animOptions);
        }

        // Convert clip to JSON object
        json animJson;
        animJson["name"] = clip.name;
        animJson["duration"] = clip.duration;
        animJson["frameRate"] = clip.frameRate;
        animJson["looping"] = clip.looping;

        json bones = json::array();
        for (const auto& bone : clip.bones) {
            json boneJson;
            boneJson["name"] = bone.boneName;

            json keyframes = json::array();
            for (const auto& kf : bone.keyframes) {
                json kfJson;
                kfJson["time"] = kf.time;
                kfJson["position"] = {kf.position[0], kf.position[1], kf.position[2]};
                kfJson["rotation"] = {kf.rotation[0], kf.rotation[1], kf.rotation[2], kf.rotation[3]};
                kfJson["scale"] = {kf.scale[0], kf.scale[1], kf.scale[2]};
                keyframes.push_back(kfJson);
            }
            boneJson["keyframes"] = keyframes;
            bones.push_back(boneJson);
        }
        animJson["bones"] = bones;

        json events = json::array();
        for (const auto& event : clip.events) {
            json eventJson;
            eventJson["time"] = event.time;
            eventJson["name"] = event.name;
            if (!event.parameter.empty()) {
                eventJson["parameter"] = event.parameter;
            }
            events.push_back(eventJson);
        }
        animJson["events"] = events;

        animations.push_back(animJson);

        result.keyframeCount += static_cast<int>(clip.bones.size());
        result.eventCount += static_cast<int>(clip.events.size());
        result.exportDuration += clip.duration;
    }

    bundle["animations"] = animations;

    // Write to file
    std::ofstream file(filePath);
    if (!file.is_open()) {
        result.success = false;
        result.errorMessage = "Failed to open file: " + filePath;
        return result;
    }

    std::string jsonStr = animOptions.prettyPrint ? bundle.dump(2) : bundle.dump();
    file << jsonStr;
    file.close();

    result.success = true;
    result.fileSize = jsonStr.size();

    return result;
}

// ============================================================================
// Import
// ============================================================================

bool AnimationExporter::ImportAnimation(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return ImportAnimationFromString(buffer.str());
}

bool AnimationExporter::ImportAnimationFromString(const std::string& jsonStr) {
    return ParseFromJson(jsonStr, m_importedClip);
}

// ============================================================================
// Validation
// ============================================================================

std::vector<std::string> AnimationExporter::ValidateExportOptions(const AnimationExportOptions& options) const {
    std::vector<std::string> errors;

    if (options.targetFrameRate <= 0.0f) {
        errors.push_back("Target frame rate must be positive");
    }

    if (options.targetFrameRate > 120.0f) {
        errors.push_back("Target frame rate exceeds maximum (120 fps)");
    }

    if (!options.exportFullRange && options.startTime >= options.endTime) {
        errors.push_back("Invalid time range: start must be less than end");
    }

    if (!options.exportAllBones && options.selectedBones.empty()) {
        errors.push_back("No bones selected for export");
    }

    return errors;
}

size_t AnimationExporter::EstimateExportSize(const AnimationExportOptions& options) const {
    if (!m_keyframeEditor) return 0;

    size_t estimatedSize = 500;  // Base JSON overhead

    int frameCount = static_cast<int>(m_keyframeEditor->GetDuration() * options.targetFrameRate);
    size_t boneCount = m_keyframeEditor->GetTracks().size();

    // Each keyframe: ~150 bytes in JSON (position, rotation, scale, time)
    estimatedSize += frameCount * boneCount * 150;

    // Events
    if (m_timeline && options.includeEvents) {
        estimatedSize += m_timeline->GetEventMarkers().size() * 100;
    }

    // Apply compression factor
    switch (options.compression) {
        case CompressionLevel::Low:
            estimatedSize = static_cast<size_t>(static_cast<double>(estimatedSize) * 0.8);
            break;
        case CompressionLevel::Medium:
            estimatedSize = static_cast<size_t>(static_cast<double>(estimatedSize) * 0.5);
            break;
        case CompressionLevel::High:
            estimatedSize = static_cast<size_t>(static_cast<double>(estimatedSize) * 0.3);
            break;
        default:
            break;
    }

    return estimatedSize;
}

// ============================================================================
// Compression
// ============================================================================

void AnimationExporter::ApplyCompressionPreset(CompressionLevel level, AnimationExportOptions& options) {
    options.compression = level;

    switch (level) {
        case CompressionLevel::None:
            options.positionTolerance = 0.0f;
            options.rotationTolerance = 0.0f;
            options.scaleTolerance = 0.0f;
            options.positionPrecision = 8;
            options.rotationPrecision = 8;
            options.removeRedundantKeyframes = false;
            break;

        case CompressionLevel::Low:
            options.positionTolerance = 0.0001f;
            options.rotationTolerance = 0.0001f;
            options.scaleTolerance = 0.0001f;
            options.positionPrecision = 6;
            options.rotationPrecision = 6;
            options.removeRedundantKeyframes = true;
            break;

        case CompressionLevel::Medium:
            options.positionTolerance = 0.001f;
            options.rotationTolerance = 0.001f;
            options.scaleTolerance = 0.001f;
            options.positionPrecision = 5;
            options.rotationPrecision = 5;
            options.removeRedundantKeyframes = true;
            break;

        case CompressionLevel::High:
            options.positionTolerance = 0.005f;
            options.rotationTolerance = 0.01f;
            options.scaleTolerance = 0.005f;
            options.positionPrecision = 4;
            options.rotationPrecision = 4;
            options.removeRedundantKeyframes = true;
            break;

        case CompressionLevel::Custom:
            // Keep existing settings
            break;
    }
}

AnimationExporter::CompressionStats AnimationExporter::CalculateCompressionStats(const AnimationExportOptions& options) const {
    CompressionStats stats;

    if (!m_keyframeEditor) return stats;

    for (const auto& track : m_keyframeEditor->GetTracks()) {
        stats.originalKeyframes += static_cast<int>(track.keyframes.size());
    }

    // Simulate compression
    AnimationClipJson testClip = BuildClipJson("test", options);
    OptimizeKeyframes(testClip, options);

    for (const auto& bone : testClip.bones) {
        stats.compressedKeyframes += static_cast<int>(bone.keyframes.size());
    }

    if (stats.originalKeyframes > 0) {
        stats.compressionRatio = static_cast<float>(stats.compressedKeyframes) / static_cast<float>(stats.originalKeyframes);
    }

    return stats;
}

// ============================================================================
// Private Methods
// ============================================================================

AnimationClipJson AnimationExporter::BuildClipJson(const std::string& name, const AnimationExportOptions& options) {
    AnimationClipJson clip;
    clip.name = name;

    if (m_keyframeEditor) {
        clip.duration = m_keyframeEditor->GetDuration();
        clip.frameRate = options.targetFrameRate;

        float startTime = options.exportFullRange ? 0.0f : options.startTime;
        float endTime = options.exportFullRange ? clip.duration : options.endTime;

        // Build bone data
        for (const auto& track : m_keyframeEditor->GetTracks()) {
            // Check if bone should be exported
            if (!options.exportAllBones) {
                auto it = std::find(options.selectedBones.begin(), options.selectedBones.end(), track.boneName);
                if (it == options.selectedBones.end()) continue;
            }

            AnimationClipJson::BoneData boneData;
            boneData.boneName = track.boneName;

            for (const auto& kf : track.keyframes) {
                if (kf.time < startTime || kf.time > endTime) continue;

                AnimationClipJson::BoneData::KeyframeData kfData;
                kfData.time = kf.time - startTime;  // Adjust time relative to start
                kfData.position = {kf.transform.position.x, kf.transform.position.y, kf.transform.position.z};
                kfData.rotation = {kf.transform.rotation.x, kf.transform.rotation.y,
                                   kf.transform.rotation.z, kf.transform.rotation.w};
                kfData.scale = {kf.transform.scale.x, kf.transform.scale.y, kf.transform.scale.z};

                boneData.keyframes.push_back(kfData);
            }

            // Set interpolation mode
            if (!track.keyframes.empty()) {
                switch (track.keyframes[0].interpolation) {
                    case InterpolationMode::Linear:
                        boneData.positionCurve.interpolation = "linear";
                        boneData.rotationCurve.interpolation = "linear";
                        break;
                    case InterpolationMode::Bezier:
                        boneData.positionCurve.interpolation = "bezier";
                        boneData.rotationCurve.interpolation = "bezier";
                        break;
                    case InterpolationMode::Step:
                        boneData.positionCurve.interpolation = "step";
                        boneData.rotationCurve.interpolation = "step";
                        break;
                    default:
                        break;
                }
            }

            clip.bones.push_back(boneData);
        }

        clip.duration = endTime - startTime;
    }

    // Add events
    if (options.includeEvents && m_timeline) {
        for (const auto& event : m_timeline->GetEventMarkers()) {
            AnimationClipJson::EventData eventData;
            eventData.time = event.time;
            eventData.name = event.name;
            eventData.parameter = event.parameter;
            clip.events.push_back(eventData);
        }
    }

    // Root motion
    if (options.includeRootMotion) {
        clip.rootMotion.enabled = true;
        clip.rootMotion.axis = "xz";
    }

    // Metadata
    if (options.includeMetadata) {
        auto now = std::chrono::system_clock::now();
        clip.modifiedTimestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    }

    return clip;
}

void AnimationExporter::OptimizeKeyframes(AnimationClipJson& clip, const AnimationExportOptions& options) {
    for (auto& bone : clip.bones) {
        if (bone.keyframes.size() < 3) continue;

        std::vector<AnimationClipJson::BoneData::KeyframeData> optimized;
        optimized.push_back(bone.keyframes.front());

        for (size_t i = 1; i < bone.keyframes.size() - 1; ++i) {
            const auto& prev = optimized.back();
            const auto& curr = bone.keyframes[i];
            const auto& next = bone.keyframes[i + 1];

            // Check if keyframe is necessary (significantly different from linear interpolation)
            float t = (curr.time - prev.time) / (next.time - prev.time);

            // Interpolate expected values
            std::array<float, 3> expectedPos = {
                prev.position[0] + t * (next.position[0] - prev.position[0]),
                prev.position[1] + t * (next.position[1] - prev.position[1]),
                prev.position[2] + t * (next.position[2] - prev.position[2])
            };

            // Calculate position error
            float posError = std::sqrt(
                std::pow(curr.position[0] - expectedPos[0], 2.0f) +
                std::pow(curr.position[1] - expectedPos[1], 2.0f) +
                std::pow(curr.position[2] - expectedPos[2], 2.0f)
            );

            // Calculate rotation error (simplified - dot product of quaternions)
            float rotError = 1.0f - std::abs(
                curr.rotation[0] * prev.rotation[0] +
                curr.rotation[1] * prev.rotation[1] +
                curr.rotation[2] * prev.rotation[2] +
                curr.rotation[3] * prev.rotation[3]
            );

            if (posError > options.positionTolerance || rotError > options.rotationTolerance) {
                optimized.push_back(curr);
            }
        }

        optimized.push_back(bone.keyframes.back());
        bone.keyframes = std::move(optimized);
    }
}

void AnimationExporter::ResampleAnimation(AnimationClipJson& clip, float targetFrameRate) {
    float frameTime = 1.0f / targetFrameRate;
    int frameCount = static_cast<int>(std::ceil(clip.duration * targetFrameRate));

    for (auto& bone : clip.bones) {
        if (bone.keyframes.empty()) continue;

        std::vector<AnimationClipJson::BoneData::KeyframeData> resampled;

        for (int frame = 0; frame <= frameCount; ++frame) {
            float time = std::min(static_cast<float>(frame) * frameTime, clip.duration);

            // Find surrounding keyframes
            size_t nextIdx = 0;
            for (size_t i = 0; i < bone.keyframes.size(); ++i) {
                if (bone.keyframes[i].time >= time) {
                    nextIdx = i;
                    break;
                }
            }

            AnimationClipJson::BoneData::KeyframeData sample;
            sample.time = time;

            if (nextIdx == 0) {
                sample = bone.keyframes[0];
                sample.time = time;
            } else {
                const auto& prev = bone.keyframes[nextIdx - 1];
                const auto& next = bone.keyframes[nextIdx];
                float t = (time - prev.time) / (next.time - prev.time);

                // Linear interpolation
                for (int i = 0; i < 3; ++i) {
                    sample.position[i] = prev.position[i] + t * (next.position[i] - prev.position[i]);
                    sample.scale[i] = prev.scale[i] + t * (next.scale[i] - prev.scale[i]);
                }

                // Quaternion slerp (simplified)
                for (int i = 0; i < 4; ++i) {
                    sample.rotation[i] = prev.rotation[i] + t * (next.rotation[i] - prev.rotation[i]);
                }

                // Normalize quaternion
                float len = std::sqrt(
                    sample.rotation[0] * sample.rotation[0] +
                    sample.rotation[1] * sample.rotation[1] +
                    sample.rotation[2] * sample.rotation[2] +
                    sample.rotation[3] * sample.rotation[3]
                );
                if (len > 0.0001f) {
                    for (float& i : sample.rotation) i /= len;
                }
            }

            resampled.push_back(sample);
        }

        bone.keyframes = std::move(resampled);
    }
}

std::string AnimationExporter::SerializeToJson(const AnimationClipJson& clip, bool prettyPrint) {
    json j;
    j["name"] = clip.name;
    j["duration"] = clip.duration;
    j["frameRate"] = clip.frameRate;
    j["looping"] = clip.looping;

    json bones = json::object();
    for (const auto& bone : clip.bones) {
        json boneJson;

        json keyframes = json::array();
        for (const auto& kf : bone.keyframes) {
            json kfJson;
            kfJson["time"] = kf.time;
            kfJson["position"] = {kf.position[0], kf.position[1], kf.position[2]};
            kfJson["rotation"] = {kf.rotation[0], kf.rotation[1], kf.rotation[2], kf.rotation[3]};
            kfJson["scale"] = {kf.scale[0], kf.scale[1], kf.scale[2]};
            keyframes.push_back(kfJson);
        }
        boneJson["keyframes"] = keyframes;

        json curves;
        curves["position"] = bone.positionCurve.interpolation;
        curves["rotation"] = bone.rotationCurve.interpolation;
        curves["scale"] = bone.scaleCurve.interpolation;
        boneJson["curves"] = curves;

        bones[bone.boneName] = boneJson;
    }
    j["bones"] = bones;

    json events = json::array();
    for (const auto& event : clip.events) {
        json eventJson;
        eventJson["time"] = event.time;
        eventJson["name"] = event.name;
        if (!event.parameter.empty()) {
            eventJson["parameter"] = event.parameter;
        }
        events.push_back(eventJson);
    }
    j["events"] = events;

    json rootMotion;
    rootMotion["enabled"] = clip.rootMotion.enabled;
    rootMotion["axis"] = clip.rootMotion.axis;
    j["rootMotion"] = rootMotion;

    return prettyPrint ? j.dump(2) : j.dump();
}

bool AnimationExporter::ParseFromJson(const std::string& jsonStr, AnimationClipJson& outClip) {
    try {
        json j = json::parse(jsonStr);

        outClip = AnimationClipJson();
        outClip.name = j.value("name", "imported");
        outClip.duration = j.value("duration", 1.0f);
        outClip.frameRate = j.value("frameRate", 30.0f);
        outClip.looping = j.value("looping", true);

        if (j.contains("bones")) {
            for (auto& [boneName, boneJson] : j["bones"].items()) {
                AnimationClipJson::BoneData bone;
                bone.boneName = boneName;

                if (boneJson.contains("keyframes")) {
                    for (const auto& kfJson : boneJson["keyframes"]) {
                        AnimationClipJson::BoneData::KeyframeData kf;
                        kf.time = kfJson.value("time", 0.0f);

                        if (kfJson.contains("position")) {
                            kf.position = {kfJson["position"][0], kfJson["position"][1], kfJson["position"][2]};
                        }
                        if (kfJson.contains("rotation")) {
                            kf.rotation = {kfJson["rotation"][0], kfJson["rotation"][1],
                                           kfJson["rotation"][2], kfJson["rotation"][3]};
                        }
                        if (kfJson.contains("scale")) {
                            kf.scale = {kfJson["scale"][0], kfJson["scale"][1], kfJson["scale"][2]};
                        }

                        bone.keyframes.push_back(kf);
                    }
                }

                if (boneJson.contains("curves")) {
                    bone.positionCurve.interpolation = boneJson["curves"].value("position", "linear");
                    bone.rotationCurve.interpolation = boneJson["curves"].value("rotation", "linear");
                    bone.scaleCurve.interpolation = boneJson["curves"].value("scale", "linear");
                }

                outClip.bones.push_back(bone);
            }
        }

        if (j.contains("events")) {
            for (const auto& eventJson : j["events"]) {
                AnimationClipJson::EventData event;
                event.time = eventJson.value("time", 0.0f);
                event.name = eventJson.value("name", "");
                event.parameter = eventJson.value("parameter", "");
                outClip.events.push_back(event);
            }
        }

        if (j.contains("rootMotion")) {
            outClip.rootMotion.enabled = j["rootMotion"].value("enabled", false);
            outClip.rootMotion.axis = j["rootMotion"].value("axis", "xz");
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace Vehement
