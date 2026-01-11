#include "AnimationImporter.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace Nova {

// ============================================================================
// Constructor/Destructor
// ============================================================================

AnimationImporter::AnimationImporter() = default;
AnimationImporter::~AnimationImporter() = default;

// ============================================================================
// Animation Import
// ============================================================================

ImportedAnimation AnimationImporter::Import(const std::string& path,
                                              const AnimationImportSettings& settings,
                                              ImportProgress* progress) {
    ImportedAnimation result;
    result.sourcePath = path;
    result.success = false;

    if (!fs::exists(path)) {
        result.errorMessage = "File not found: " + path;
        if (progress) progress->Error(result.errorMessage);
        return result;
    }

    // Setup progress stages
    if (progress) {
        progress->AddStage("load", "Loading animation", 2.0f);
        progress->AddStage("process", "Processing clips", 2.0f);
        progress->AddStage("compress", "Compressing", 1.0f);
        progress->AddStage("output", "Finalizing", 1.0f);
        progress->SetStatus(ImportStatus::InProgress);
        progress->StartTiming();
    }

    // Load based on format
    if (progress) progress->BeginStage("load");

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".bvh") {
        result = LoadBVH(path, progress);
    } else if (ext == ".fbx") {
        result = LoadFBXAnimation(path, progress);
    } else if (ext == ".gltf" || ext == ".glb") {
        result = LoadGLTFAnimation(path, progress);
    } else {
        result.errorMessage = "Unsupported animation format: " + ext;
        if (progress) {
            progress->Error(result.errorMessage);
            progress->SetStatus(ImportStatus::Failed);
        }
        return result;
    }

    if (!result.success) {
        if (progress) progress->SetStatus(ImportStatus::Failed);
        return result;
    }

    if (progress) progress->EndStage();

    // Process clips
    if (progress) progress->BeginStage("process");

    // Split clips if requested
    if (settings.splitByMarkers && !settings.clipRanges.empty()) {
        std::vector<ImportedClip> newClips;
        for (auto& clip : result.clips) {
            for (const auto& [name, range] : settings.clipRanges) {
                ImportedClip extracted = ExtractClip(clip, range.first, range.second, name);
                newClips.push_back(extracted);
            }
        }
        if (!newClips.empty()) {
            result.clips = newClips;
        }
    }

    // Resample if needed
    if (settings.resample) {
        for (auto& clip : result.clips) {
            Resample(clip, settings.targetSampleRate);
        }
        if (progress) progress->Info("Resampled to " + std::to_string(settings.targetSampleRate) + " fps");
    }

    // Extract root motion
    if (settings.extractRootMotion) {
        for (auto& clip : result.clips) {
            ExtractRootMotion(clip, settings.rootBoneName,
                              !settings.lockRootPositionXZ, !settings.lockRootRotationY);

            if (settings.lockRootHeight) {
                LockRootPosition(clip, settings.rootBoneName, false, true, false);
            }
        }
        if (progress) progress->Info("Extracted root motion");
    }

    // Loop detection and fixing
    if (settings.detectLoops) {
        for (auto& clip : result.clips) {
            clip.looping = DetectLoop(clip, settings.loopThreshold);
            if (settings.forceLoop || clip.looping) {
                FixLoop(clip);
            }
        }
    }

    // Make additive if requested
    if (settings.makeAdditive) {
        std::vector<ImportedClip> additiveClips;
        for (auto& clip : result.clips) {
            additiveClips.push_back(MakeAdditiveFromFirstFrame(clip));
        }
        result.clips = additiveClips;
        if (progress) progress->Info("Converted to additive animations");
    }

    if (progress) progress->EndStage();

    // Compression
    if (progress) progress->BeginStage("compress");

    result.originalSize = 0;
    for (const auto& clip : result.clips) {
        result.originalSize += clip.channels.size() * clip.duration * settings.sampleRate * sizeof(ImportedKeyframe);
    }

    if (settings.compression != AnimationCompression::None) {
        for (auto& clip : result.clips) {
            Compress(clip, settings.positionTolerance, settings.rotationTolerance, settings.scaleTolerance);
        }
        if (progress) progress->Info("Compressed animation data");
    }

    result.compressedSize = EstimateCompressedSize(result.clips[0]);
    for (size_t i = 1; i < result.clips.size(); ++i) {
        result.compressedSize += EstimateCompressedSize(result.clips[i]);
    }
    result.compressionRatio = result.originalSize > 0 ?
        static_cast<float>(result.compressedSize) / result.originalSize : 1.0f;

    if (progress) progress->EndStage();

    // Finalize
    if (progress) progress->BeginStage("output");

    // Calculate statistics
    result.totalClips = static_cast<uint32_t>(result.clips.size());
    result.totalChannels = 0;
    result.totalKeyframes = 0;
    for (const auto& clip : result.clips) {
        result.totalChannels += static_cast<uint32_t>(clip.channels.size());
        for (const auto& channel : clip.channels) {
            result.totalKeyframes += static_cast<uint32_t>(channel.keyframes.size());
        }
    }

    // Set output path
    if (settings.outputPath.empty()) {
        result.outputPath = path + ".nova";
    } else {
        result.outputPath = settings.outputPath;
    }

    if (progress) progress->EndStage();

    result.success = true;
    if (progress) {
        if (!result.warnings.empty()) {
            progress->SetStatus(ImportStatus::CompletedWithWarnings);
        } else {
            progress->SetStatus(ImportStatus::Completed);
        }
        progress->StopTiming();
    }

    return result;
}

ImportedAnimation AnimationImporter::Import(const std::string& path) {
    AnimationImportSettings settings;
    return Import(path, settings);
}

// ============================================================================
// BVH Loading
// ============================================================================

ImportedAnimation AnimationImporter::LoadBVH(const std::string& path, ImportProgress* progress) {
    ImportedAnimation result;
    result.sourcePath = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        result.errorMessage = "Failed to open file";
        return result;
    }

    // Parse hierarchy
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("HIERARCHY") != std::string::npos) break;
    }

    std::vector<BVHJoint> joints = ParseBVHHierarchy(file);

    if (joints.empty()) {
        result.errorMessage = "Failed to parse BVH hierarchy";
        return result;
    }

    // Parse motion data
    int frameCount = 0;
    float frameTime = 1.0f / 30.0f;

    while (std::getline(file, line)) {
        if (line.find("MOTION") != std::string::npos) break;
    }

    while (std::getline(file, line)) {
        if (line.find("Frames:") != std::string::npos) {
            sscanf(line.c_str(), "Frames: %d", &frameCount);
        } else if (line.find("Frame Time:") != std::string::npos) {
            sscanf(line.c_str(), "Frame Time: %f", &frameTime);
        } else if (frameCount > 0 && !line.empty()) {
            break;
        }
    }

    ImportedClip clip;
    clip.name = fs::path(path).stem().string();
    clip.startTime = 0.0f;
    clip.duration = frameCount * frameTime;
    clip.endTime = clip.duration;

    // Create channels for each joint
    for (const auto& joint : joints) {
        ImportedChannel channel;
        channel.boneName = joint.name;
        clip.channels.push_back(channel);
        result.boneNames.push_back(joint.name);
    }

    // Parse motion data
    file.clear();
    file.seekg(0);
    while (std::getline(file, line)) {
        if (line.find("Frame Time:") != std::string::npos) break;
    }

    ParseBVHMotion(file, joints, clip, frameCount, frameTime);

    result.clips.push_back(clip);
    result.originalDuration = clip.duration;
    result.originalSampleRate = 1.0f / frameTime;
    result.success = true;

    return result;
}

std::vector<AnimationImporter::BVHJoint> AnimationImporter::ParseBVHHierarchy(std::istream& stream) {
    std::vector<BVHJoint> joints;
    std::vector<int> parentStack;

    std::string line;
    BVHJoint* currentJoint = nullptr;

    while (std::getline(stream, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "ROOT" || token == "JOINT") {
            BVHJoint joint;
            iss >> joint.name;
            joint.parentIndex = parentStack.empty() ? -1 : parentStack.back();

            if (!parentStack.empty()) {
                joints[parentStack.back()].childIndices.push_back(static_cast<int>(joints.size()));
            }

            joints.push_back(joint);
            currentJoint = &joints.back();
        }
        else if (token == "End") {
            // End site - skip
        }
        else if (token == "OFFSET") {
            if (currentJoint) {
                iss >> currentJoint->offset.x >> currentJoint->offset.y >> currentJoint->offset.z;
            }
        }
        else if (token == "CHANNELS") {
            int numChannels;
            iss >> numChannels;
            if (currentJoint) {
                for (int i = 0; i < numChannels; ++i) {
                    std::string channelName;
                    iss >> channelName;
                    currentJoint->channels.push_back(channelName);
                }
            }
        }
        else if (token == "{") {
            if (!joints.empty()) {
                parentStack.push_back(static_cast<int>(joints.size() - 1));
            }
        }
        else if (token == "}") {
            if (!parentStack.empty()) {
                parentStack.pop_back();
            }
        }
        else if (token == "MOTION") {
            break;
        }
    }

    return joints;
}

void AnimationImporter::ParseBVHMotion(std::istream& stream, std::vector<BVHJoint>& joints,
                                         ImportedClip& clip, int frameCount, float frameTime) {
    std::string line;

    for (int frame = 0; frame < frameCount; ++frame) {
        if (!std::getline(stream, line)) break;

        std::istringstream iss(line);
        float time = frame * frameTime;

        size_t channelIdx = 0;
        for (size_t jointIdx = 0; jointIdx < joints.size(); ++jointIdx) {
            const auto& joint = joints[jointIdx];
            ImportedKeyframe kf;
            kf.time = time;
            kf.position = joint.offset;
            kf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            kf.scale = glm::vec3(1.0f);

            glm::vec3 eulerAngles{0.0f};

            for (const auto& channelName : joint.channels) {
                float value;
                iss >> value;

                if (channelName == "Xposition") kf.position.x += value;
                else if (channelName == "Yposition") kf.position.y += value;
                else if (channelName == "Zposition") kf.position.z += value;
                else if (channelName == "Xrotation") eulerAngles.x = glm::radians(value);
                else if (channelName == "Yrotation") eulerAngles.y = glm::radians(value);
                else if (channelName == "Zrotation") eulerAngles.z = glm::radians(value);
            }

            // Convert Euler to quaternion (ZYX order for BVH)
            kf.rotation = glm::quat(eulerAngles);

            clip.channels[jointIdx].keyframes.push_back(kf);
        }
    }
}

// ============================================================================
// FBX Animation Loading
// ============================================================================

ImportedAnimation AnimationImporter::LoadFBXAnimation(const std::string& path, ImportProgress* progress) {
    ImportedAnimation result;
    result.sourcePath = path;

    // FBX parsing is complex - would use FBX SDK
    // Simplified placeholder
    ImportedClip clip;
    clip.name = fs::path(path).stem().string();
    clip.duration = 1.0f;

    ImportedChannel channel;
    channel.boneName = "root";
    channel.keyframes.push_back({0.0f, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)});
    channel.keyframes.push_back({1.0f, glm::vec3(0, 1, 0), glm::quat(1, 0, 0, 0), glm::vec3(1)});
    clip.channels.push_back(channel);

    result.clips.push_back(clip);
    result.success = true;

    if (progress) progress->Warning("FBX animation import uses simplified parser");

    return result;
}

// ============================================================================
// GLTF Animation Loading
// ============================================================================

ImportedAnimation AnimationImporter::LoadGLTFAnimation(const std::string& path, ImportProgress* progress) {
    ImportedAnimation result;
    result.sourcePath = path;

    // GLTF parsing would use JSON parser
    // Simplified placeholder
    ImportedClip clip;
    clip.name = fs::path(path).stem().string();
    clip.duration = 1.0f;

    ImportedChannel channel;
    channel.boneName = "root";
    channel.keyframes.push_back({0.0f, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)});
    clip.channels.push_back(channel);

    result.clips.push_back(clip);
    result.success = true;

    if (progress) progress->Warning("GLTF animation import uses simplified parser");

    return result;
}

// ============================================================================
// Clip Splitting
// ============================================================================

std::vector<ImportedClip> AnimationImporter::SplitByMarkers(const ImportedClip& animation,
                                                              const std::vector<std::pair<float, std::string>>& markers) {
    std::vector<ImportedClip> clips;

    std::vector<std::pair<float, std::string>> sortedMarkers = markers;
    std::sort(sortedMarkers.begin(), sortedMarkers.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (size_t i = 0; i < sortedMarkers.size(); ++i) {
        float startTime = sortedMarkers[i].first;
        float endTime = (i + 1 < sortedMarkers.size()) ? sortedMarkers[i + 1].first : animation.endTime;

        ImportedClip clip = ExtractClip(animation, startTime, endTime, sortedMarkers[i].second);
        clips.push_back(clip);
    }

    return clips;
}

std::vector<ImportedClip> AnimationImporter::SplitByRanges(const ImportedClip& animation,
                                                            const std::vector<std::tuple<std::string, float, float>>& ranges) {
    std::vector<ImportedClip> clips;

    for (const auto& [name, start, end] : ranges) {
        clips.push_back(ExtractClip(animation, start, end, name));
    }

    return clips;
}

std::vector<std::pair<float, float>> AnimationImporter::DetectClipBoundaries(const ImportedClip& animation) {
    std::vector<std::pair<float, float>> boundaries;

    if (animation.channels.empty()) return boundaries;

    // Analyze motion to find pauses
    std::vector<float> motionIntensity;
    float sampleRate = 30.0f;
    int numSamples = static_cast<int>(animation.duration * sampleRate);

    for (int i = 0; i < numSamples; ++i) {
        float time = i / sampleRate;
        float intensity = 0.0f;

        for (const auto& channel : animation.channels) {
            if (channel.keyframes.size() < 2) continue;

            ImportedKeyframe kf1 = InterpolateKeyframe(channel, time);
            ImportedKeyframe kf2 = InterpolateKeyframe(channel, time + 1.0f / sampleRate);

            intensity += glm::length(kf2.position - kf1.position);
            intensity += glm::length(glm::eulerAngles(kf2.rotation) - glm::eulerAngles(kf1.rotation));
        }

        motionIntensity.push_back(intensity);
    }

    // Find low-motion regions
    float threshold = 0.01f;
    float clipStart = 0.0f;
    bool inClip = false;

    for (int i = 0; i < numSamples; ++i) {
        float time = i / sampleRate;

        if (motionIntensity[i] > threshold) {
            if (!inClip) {
                clipStart = time;
                inClip = true;
            }
        } else {
            if (inClip && time - clipStart > 0.5f) {
                boundaries.push_back({clipStart, time});
                inClip = false;
            }
        }
    }

    if (inClip) {
        boundaries.push_back({clipStart, animation.duration});
    }

    return boundaries;
}

ImportedClip AnimationImporter::ExtractClip(const ImportedClip& animation, float startTime, float endTime,
                                             const std::string& name) {
    ImportedClip clip;
    clip.name = name.empty() ? animation.name + "_clip" : name;
    clip.startTime = 0.0f;
    clip.duration = endTime - startTime;
    clip.endTime = clip.duration;
    clip.looping = animation.looping;

    for (const auto& channel : animation.channels) {
        ImportedChannel newChannel;
        newChannel.boneName = channel.boneName;
        newChannel.hasPosition = channel.hasPosition;
        newChannel.hasRotation = channel.hasRotation;
        newChannel.hasScale = channel.hasScale;

        for (const auto& kf : channel.keyframes) {
            if (kf.time >= startTime && kf.time <= endTime) {
                ImportedKeyframe newKf = kf;
                newKf.time -= startTime;
                newChannel.keyframes.push_back(newKf);
            }
        }

        // Add interpolated keyframes at boundaries
        if (newChannel.keyframes.empty() || newChannel.keyframes.front().time > 0.001f) {
            ImportedKeyframe startKf = InterpolateKeyframe(channel, startTime);
            startKf.time = 0.0f;
            newChannel.keyframes.insert(newChannel.keyframes.begin(), startKf);
        }

        if (newChannel.keyframes.empty() || newChannel.keyframes.back().time < clip.duration - 0.001f) {
            ImportedKeyframe endKf = InterpolateKeyframe(channel, endTime);
            endKf.time = clip.duration;
            newChannel.keyframes.push_back(endKf);
        }

        clip.channels.push_back(newChannel);
    }

    // Extract events in range
    for (const auto& event : animation.events) {
        if (event.time >= startTime && event.time <= endTime) {
            AnimationEvent newEvent = event;
            newEvent.time -= startTime;
            clip.events.push_back(newEvent);
        }
    }

    return clip;
}

// ============================================================================
// Root Motion
// ============================================================================

void AnimationImporter::ExtractRootMotion(ImportedClip& animation, const std::string& rootBoneName,
                                           bool extractTranslation, bool extractRotation) {
    ImportedChannel* rootChannel = nullptr;
    for (auto& channel : animation.channels) {
        if (channel.boneName == rootBoneName) {
            rootChannel = &channel;
            break;
        }
    }

    if (!rootChannel || rootChannel->keyframes.empty()) return;

    // Calculate total motion
    const ImportedKeyframe& firstKf = rootChannel->keyframes.front();
    const ImportedKeyframe& lastKf = rootChannel->keyframes.back();

    if (extractTranslation) {
        animation.rootMotionDelta = lastKf.position - firstKf.position;
        animation.rootMotionDelta.y = 0.0f;  // Keep vertical motion in animation
    }

    if (extractRotation) {
        glm::vec3 firstEuler = glm::eulerAngles(firstKf.rotation);
        glm::vec3 lastEuler = glm::eulerAngles(lastKf.rotation);
        animation.rootRotationDelta = lastEuler.y - firstEuler.y;
    }

    animation.hasRootMotion = true;

    // Remove extracted motion from keyframes
    glm::vec3 motionPerSecond = animation.rootMotionDelta / animation.duration;
    float rotationPerSecond = animation.rootRotationDelta / animation.duration;

    for (auto& kf : rootChannel->keyframes) {
        if (extractTranslation) {
            glm::vec3 motionAtTime = motionPerSecond * kf.time;
            kf.position.x -= motionAtTime.x;
            kf.position.z -= motionAtTime.z;
        }

        if (extractRotation) {
            float rotationAtTime = rotationPerSecond * kf.time;
            kf.rotation = glm::angleAxis(-rotationAtTime, glm::vec3(0, 1, 0)) * kf.rotation;
        }
    }
}

void AnimationImporter::LockRootPosition(ImportedClip& animation, const std::string& rootBoneName,
                                          bool lockX, bool lockY, bool lockZ) {
    for (auto& channel : animation.channels) {
        if (channel.boneName == rootBoneName) {
            glm::vec3 firstPos = channel.keyframes.empty() ? glm::vec3(0) : channel.keyframes[0].position;

            for (auto& kf : channel.keyframes) {
                if (lockX) kf.position.x = firstPos.x;
                if (lockY) kf.position.y = firstPos.y;
                if (lockZ) kf.position.z = firstPos.z;
            }
            break;
        }
    }
}

// ============================================================================
// Compression
// ============================================================================

void AnimationImporter::Compress(ImportedClip& animation, float positionTolerance,
                                  float rotationTolerance, float scaleTolerance) {
    for (auto& channel : animation.channels) {
        // Remove redundant keyframes
        std::vector<ImportedKeyframe> compressed;

        if (channel.keyframes.empty()) continue;

        compressed.push_back(channel.keyframes.front());

        for (size_t i = 1; i < channel.keyframes.size() - 1; ++i) {
            if (!IsKeyframeRedundant(channel.keyframes, i, positionTolerance, rotationTolerance, scaleTolerance)) {
                compressed.push_back(channel.keyframes[i]);
            }
        }

        if (channel.keyframes.size() > 1) {
            compressed.push_back(channel.keyframes.back());
        }

        channel.keyframes = compressed;
    }
}

bool AnimationImporter::IsKeyframeRedundant(const std::vector<ImportedKeyframe>& keyframes, size_t index,
                                             float posTol, float rotTol, float scaleTol) {
    if (index == 0 || index >= keyframes.size() - 1) return false;

    const auto& prev = keyframes[index - 1];
    const auto& curr = keyframes[index];
    const auto& next = keyframes[index + 1];

    float t = (curr.time - prev.time) / (next.time - prev.time);
    ImportedKeyframe interpolated = InterpolateKeyframes(prev, next, t);

    float posError = glm::length(curr.position - interpolated.position);
    float rotError = 1.0f - std::abs(glm::dot(curr.rotation, interpolated.rotation));
    float scaleError = glm::length(curr.scale - interpolated.scale);

    return posError < posTol && rotError < rotTol && scaleError < scaleTol;
}

void AnimationImporter::Resample(ImportedClip& animation, float targetFrameRate) {
    float frameTime = 1.0f / targetFrameRate;
    int numFrames = static_cast<int>(animation.duration * targetFrameRate) + 1;

    for (auto& channel : animation.channels) {
        std::vector<ImportedKeyframe> resampled;

        for (int i = 0; i < numFrames; ++i) {
            float time = i * frameTime;
            resampled.push_back(InterpolateKeyframe(channel, time));
            resampled.back().time = time;
        }

        channel.keyframes = resampled;
    }
}

size_t AnimationImporter::EstimateCompressedSize(const ImportedClip& animation) {
    size_t size = 0;
    for (const auto& channel : animation.channels) {
        size += channel.keyframes.size() * sizeof(ImportedKeyframe);
    }
    return size;
}

// ============================================================================
// Loop Processing
// ============================================================================

bool AnimationImporter::DetectLoop(const ImportedClip& animation, float threshold) {
    for (const auto& channel : animation.channels) {
        if (channel.keyframes.size() < 2) continue;

        const auto& first = channel.keyframes.front();
        const auto& last = channel.keyframes.back();

        float posError = glm::length(first.position - last.position);
        float rotError = 1.0f - std::abs(glm::dot(first.rotation, last.rotation));

        if (posError > threshold || rotError > threshold) {
            return false;
        }
    }
    return true;
}

void AnimationImporter::FixLoop(ImportedClip& animation, float blendDuration) {
    for (auto& channel : animation.channels) {
        if (channel.keyframes.size() < 2) continue;

        const auto& first = channel.keyframes.front();
        auto& last = channel.keyframes.back();

        // Blend the last keyframe toward the first
        float t = 0.5f;  // How much to blend
        last.position = glm::mix(last.position, first.position, t);
        last.rotation = glm::slerp(last.rotation, first.rotation, t);
        last.scale = glm::mix(last.scale, first.scale, t);
    }

    animation.looping = true;
}

void AnimationImporter::MakeLoopable(ImportedClip& animation) {
    for (auto& channel : animation.channels) {
        if (channel.keyframes.empty()) continue;

        // Add a keyframe at the end that matches the start
        ImportedKeyframe loopKf = channel.keyframes.front();
        loopKf.time = animation.duration;
        channel.keyframes.push_back(loopKf);
    }

    animation.looping = true;
}

// ============================================================================
// Additive Animations
// ============================================================================

ImportedClip AnimationImporter::MakeAdditive(const ImportedClip& animation, const ImportedClip& referencePose,
                                              float referenceFrame) {
    ImportedClip additive = animation;
    additive.name = animation.name + "_additive";

    for (size_t i = 0; i < additive.channels.size(); ++i) {
        auto& channel = additive.channels[i];

        // Find reference pose for this bone
        ImportedKeyframe refKf{referenceFrame, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)};

        for (const auto& refChannel : referencePose.channels) {
            if (refChannel.boneName == channel.boneName) {
                refKf = InterpolateKeyframe(refChannel, referenceFrame);
                break;
            }
        }

        // Subtract reference pose from each keyframe
        for (auto& kf : channel.keyframes) {
            kf.position -= refKf.position;
            kf.rotation = glm::inverse(refKf.rotation) * kf.rotation;
            kf.scale /= refKf.scale;
        }
    }

    return additive;
}

ImportedClip AnimationImporter::MakeAdditiveFromFirstFrame(const ImportedClip& animation) {
    return MakeAdditive(animation, animation, 0.0f);
}

ImportedClip AnimationImporter::ApplyAdditive(const ImportedClip& baseAnimation, const ImportedClip& additiveAnimation,
                                               float weight) {
    ImportedClip result = baseAnimation;

    for (size_t i = 0; i < result.channels.size(); ++i) {
        auto& channel = result.channels[i];

        // Find additive channel
        const ImportedChannel* addChannel = nullptr;
        for (const auto& addCh : additiveAnimation.channels) {
            if (addCh.boneName == channel.boneName) {
                addChannel = &addCh;
                break;
            }
        }

        if (!addChannel) continue;

        // Apply additive to each keyframe
        for (auto& kf : channel.keyframes) {
            ImportedKeyframe addKf = InterpolateKeyframe(*addChannel, kf.time);

            kf.position += addKf.position * weight;
            kf.rotation = glm::slerp(kf.rotation, kf.rotation * addKf.rotation, weight);
            kf.scale *= glm::mix(glm::vec3(1.0f), addKf.scale, weight);
        }
    }

    return result;
}

// ============================================================================
// Retargeting
// ============================================================================

ImportedClip AnimationImporter::Retarget(const ImportedClip& animation, const RetargetConfig& config,
                                          const Skeleton* sourceSkeleton, const Skeleton* targetSkeleton) {
    ImportedClip retargeted;
    retargeted.name = animation.name + "_retargeted";
    retargeted.duration = animation.duration;
    retargeted.startTime = animation.startTime;
    retargeted.endTime = animation.endTime;
    retargeted.looping = animation.looping;
    retargeted.events = animation.events;

    for (const auto& mapping : config.mappings) {
        // Find source channel
        const ImportedChannel* srcChannel = nullptr;
        for (const auto& channel : animation.channels) {
            if (channel.boneName == mapping.sourceBone) {
                srcChannel = &channel;
                break;
            }
        }

        if (!srcChannel) continue;

        ImportedChannel targetChannel;
        targetChannel.boneName = mapping.targetBone;
        targetChannel.hasPosition = srcChannel->hasPosition;
        targetChannel.hasRotation = srcChannel->hasRotation;
        targetChannel.hasScale = srcChannel->hasScale;

        for (const auto& kf : srcChannel->keyframes) {
            ImportedKeyframe targetKf = kf;

            // Apply rotation offset
            targetKf.rotation = mapping.rotationOffset * kf.rotation;

            // Apply scale offset
            targetKf.scale *= mapping.scaleOffset;

            targetChannel.keyframes.push_back(targetKf);
        }

        retargeted.channels.push_back(targetChannel);
    }

    return retargeted;
}

std::vector<BoneMapping> AnimationImporter::AutoGenerateBoneMapping(const std::vector<std::string>& sourceBones,
                                                                      const std::vector<std::string>& targetBones) {
    std::vector<BoneMapping> mappings;

    // Common bone name patterns
    static const std::vector<std::pair<std::string, std::vector<std::string>>> patterns = {
        {"hips", {"hips", "pelvis", "root"}},
        {"spine", {"spine", "spine1", "torso"}},
        {"chest", {"chest", "spine2", "spine3"}},
        {"neck", {"neck"}},
        {"head", {"head"}},
        {"left_shoulder", {"leftshoulder", "l_shoulder", "shoulder_l"}},
        {"left_arm", {"leftarm", "l_arm", "arm_l"}},
        {"left_forearm", {"leftforearm", "l_forearm", "forearm_l"}},
        {"left_hand", {"lefthand", "l_hand", "hand_l"}},
        {"right_shoulder", {"rightshoulder", "r_shoulder", "shoulder_r"}},
        {"right_arm", {"rightarm", "r_arm", "arm_r"}},
        {"right_forearm", {"rightforearm", "r_forearm", "forearm_r"}},
        {"right_hand", {"righthand", "r_hand", "hand_r"}},
        {"left_upleg", {"leftupleg", "l_thigh", "thigh_l"}},
        {"left_leg", {"leftleg", "l_calf", "calf_l"}},
        {"left_foot", {"leftfoot", "l_foot", "foot_l"}},
        {"right_upleg", {"rightupleg", "r_thigh", "thigh_r"}},
        {"right_leg", {"rightleg", "r_calf", "calf_r"}},
        {"right_foot", {"rightfoot", "r_foot", "foot_r"}}
    };

    auto normalize = [](const std::string& name) {
        std::string result;
        for (char c : name) {
            if (std::isalnum(c)) result += std::tolower(c);
        }
        return result;
    };

    for (const auto& srcBone : sourceBones) {
        std::string srcNorm = normalize(srcBone);

        for (const auto& tgtBone : targetBones) {
            std::string tgtNorm = normalize(tgtBone);

            // Direct match
            if (srcNorm == tgtNorm) {
                BoneMapping mapping;
                mapping.sourceBone = srcBone;
                mapping.targetBone = tgtBone;
                mappings.push_back(mapping);
                break;
            }

            // Pattern match
            for (const auto& [canonical, variants] : patterns) {
                bool srcMatch = std::find(variants.begin(), variants.end(), srcNorm) != variants.end();
                bool tgtMatch = std::find(variants.begin(), variants.end(), tgtNorm) != variants.end();

                if (srcMatch && tgtMatch) {
                    BoneMapping mapping;
                    mapping.sourceBone = srcBone;
                    mapping.targetBone = tgtBone;
                    mappings.push_back(mapping);
                    break;
                }
            }
        }
    }

    return mappings;
}

// ============================================================================
// Utilities
// ============================================================================

ImportedKeyframe AnimationImporter::InterpolateKeyframe(const ImportedChannel& channel, float time) {
    if (channel.keyframes.empty()) {
        return ImportedKeyframe{time, glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)};
    }

    if (channel.keyframes.size() == 1 || time <= channel.keyframes.front().time) {
        ImportedKeyframe kf = channel.keyframes.front();
        kf.time = time;
        return kf;
    }

    if (time >= channel.keyframes.back().time) {
        ImportedKeyframe kf = channel.keyframes.back();
        kf.time = time;
        return kf;
    }

    // Binary search
    auto it = std::lower_bound(channel.keyframes.begin(), channel.keyframes.end(), time,
                                [](const ImportedKeyframe& kf, float t) { return kf.time < t; });

    if (it == channel.keyframes.begin()) {
        ImportedKeyframe kf = *it;
        kf.time = time;
        return kf;
    }

    auto prev = std::prev(it);
    float t = (time - prev->time) / (it->time - prev->time);

    return InterpolateKeyframes(*prev, *it, t);
}

float AnimationImporter::CalculateDuration(const ImportedClip& animation) {
    float duration = 0.0f;
    for (const auto& channel : animation.channels) {
        if (!channel.keyframes.empty()) {
            duration = std::max(duration, channel.keyframes.back().time);
        }
    }
    return duration;
}

// ============================================================================
// File Support
// ============================================================================

bool AnimationImporter::IsFormatSupported(const std::string& extension) {
    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') ext = "." + ext;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::vector<std::string> supported = {".bvh", ".fbx", ".gltf", ".glb"};
    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

std::vector<std::string> AnimationImporter::GetSupportedExtensions() {
    return {".bvh", ".fbx", ".gltf", ".glb"};
}

// ============================================================================
// Output
// ============================================================================

bool AnimationImporter::SaveEngineFormat(const ImportedAnimation& animation, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write header
    struct Header {
        char magic[4] = {'N', 'A', 'N', 'M'};
        uint32_t version = 1;
        uint32_t clipCount;
    };

    Header header;
    header.clipCount = static_cast<uint32_t>(animation.clips.size());

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write clips
    for (const auto& clip : animation.clips) {
        uint32_t nameLen = static_cast<uint32_t>(clip.name.length());
        file.write(reinterpret_cast<const char*>(&nameLen), 4);
        file.write(clip.name.c_str(), nameLen);
        file.write(reinterpret_cast<const char*>(&clip.duration), sizeof(float));

        uint32_t channelCount = static_cast<uint32_t>(clip.channels.size());
        file.write(reinterpret_cast<const char*>(&channelCount), 4);

        for (const auto& channel : clip.channels) {
            uint32_t boneNameLen = static_cast<uint32_t>(channel.boneName.length());
            file.write(reinterpret_cast<const char*>(&boneNameLen), 4);
            file.write(channel.boneName.c_str(), boneNameLen);

            uint32_t kfCount = static_cast<uint32_t>(channel.keyframes.size());
            file.write(reinterpret_cast<const char*>(&kfCount), 4);
            file.write(reinterpret_cast<const char*>(channel.keyframes.data()),
                       kfCount * sizeof(ImportedKeyframe));
        }
    }

    return true;
}

std::string AnimationImporter::ExportMetadata(const ImportedAnimation& animation) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"sourcePath\": \"" << animation.sourcePath << "\",\n";
    ss << "  \"totalClips\": " << animation.totalClips << ",\n";
    ss << "  \"totalChannels\": " << animation.totalChannels << ",\n";
    ss << "  \"totalKeyframes\": " << animation.totalKeyframes << ",\n";
    ss << "  \"originalDuration\": " << animation.originalDuration << ",\n";
    ss << "  \"compressionRatio\": " << animation.compressionRatio << ",\n";
    ss << "  \"clips\": [\n";
    for (size_t i = 0; i < animation.clips.size(); ++i) {
        const auto& clip = animation.clips[i];
        ss << "    {\"name\": \"" << clip.name << "\", \"duration\": " << clip.duration
           << ", \"looping\": " << (clip.looping ? "true" : "false") << "}";
        if (i < animation.clips.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

// ============================================================================
// Utility Functions
// ============================================================================

ImportedKeyframe InterpolateKeyframes(const ImportedKeyframe& a, const ImportedKeyframe& b, float t) {
    ImportedKeyframe result;
    result.time = glm::mix(a.time, b.time, t);
    result.position = glm::mix(a.position, b.position, t);
    result.rotation = glm::slerp(a.rotation, b.rotation, t);
    result.scale = glm::mix(a.scale, b.scale, t);
    return result;
}

glm::mat4 KeyframeToMatrix(const ImportedKeyframe& kf) {
    glm::mat4 t = glm::translate(glm::mat4(1.0f), kf.position);
    glm::mat4 r = glm::mat4_cast(kf.rotation);
    glm::mat4 s = glm::scale(glm::mat4(1.0f), kf.scale);
    return t * r * s;
}

ImportedKeyframe MatrixToKeyframe(const glm::mat4& matrix, float time) {
    ImportedKeyframe kf;
    kf.time = time;

    // Decompose matrix
    kf.position = glm::vec3(matrix[3]);

    glm::vec3 scale;
    scale.x = glm::length(glm::vec3(matrix[0]));
    scale.y = glm::length(glm::vec3(matrix[1]));
    scale.z = glm::length(glm::vec3(matrix[2]));
    kf.scale = scale;

    glm::mat3 rotMatrix(
        glm::vec3(matrix[0]) / scale.x,
        glm::vec3(matrix[1]) / scale.y,
        glm::vec3(matrix[2]) / scale.z
    );
    kf.rotation = glm::quat_cast(rotMatrix);

    return kf;
}

float KeyframeDifference(const ImportedKeyframe& a, const ImportedKeyframe& b) {
    float posDiff = glm::length(a.position - b.position);
    float rotDiff = 1.0f - std::abs(glm::dot(a.rotation, b.rotation));
    float scaleDiff = glm::length(a.scale - b.scale);
    return posDiff + rotDiff * 10.0f + scaleDiff;
}

} // namespace Nova
