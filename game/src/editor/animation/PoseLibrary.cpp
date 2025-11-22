#include "PoseLibrary.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Vehement {

PoseLibrary::PoseLibrary() = default;
PoseLibrary::~PoseLibrary() = default;

void PoseLibrary::Initialize(const Config& config) {
    m_config = config;

    // Create default category
    CreateCategory("Default");
    CreateCategory("Actions");
    CreateCategory("Locomotion");
    CreateCategory("Combat");
    CreateCategory("Expressions");

    LoadLibrary();
    m_initialized = true;
}

// ============================================================================
// Pose Management
// ============================================================================

SavedPose* PoseLibrary::SavePose(const std::string& name, const std::string& category) {
    if (!m_boneEditor) return nullptr;

    return SavePose(name, m_boneEditor->CaptureTransformState(), category);
}

SavedPose* PoseLibrary::SavePose(const std::string& name,
                                  const std::unordered_map<std::string, BoneTransform>& transforms,
                                  const std::string& category) {
    // Check if pose already exists
    for (auto& pose : m_poses) {
        if (pose.name == name) {
            pose.boneTransforms = transforms;
            pose.category = category;
            pose.timestamp = GetTimestamp();

            if (m_config.autoGenerateThumbnails) {
                GenerateThumbnail(name);
            }

            if (OnPoseSaved) {
                OnPoseSaved(pose);
            }

            return &pose;
        }
    }

    // Create new pose
    SavedPose pose;
    pose.name = name;
    pose.category = category;
    pose.boneTransforms = transforms;
    pose.timestamp = GetTimestamp();

    m_poses.push_back(pose);

    // Add to category
    if (auto* cat = GetCategory(category)) {
        cat->poseNames.push_back(name);
    }

    if (m_config.autoGenerateThumbnails) {
        GenerateThumbnail(name);
    }

    if (OnPoseSaved) {
        OnPoseSaved(m_poses.back());
    }

    return &m_poses.back();
}

bool PoseLibrary::LoadPose(const std::string& name) {
    const SavedPose* pose = GetPose(name);
    if (!pose || !m_boneEditor) return false;

    m_boneEditor->SetAllTransforms(pose->boneTransforms);

    if (OnPoseApplied) {
        OnPoseApplied(*pose);
    }

    return true;
}

void PoseLibrary::DeletePose(const std::string& name) {
    // Remove from category
    for (auto& cat : m_categories) {
        cat.poseNames.erase(
            std::remove(cat.poseNames.begin(), cat.poseNames.end(), name),
            cat.poseNames.end()
        );
    }

    // Remove pose
    m_poses.erase(
        std::remove_if(m_poses.begin(), m_poses.end(),
            [&name](const SavedPose& p) { return p.name == name; }),
        m_poses.end()
    );

    if (OnPoseDeleted) {
        OnPoseDeleted(name);
    }
}

bool PoseLibrary::RenamePose(const std::string& oldName, const std::string& newName) {
    if (HasPose(newName)) return false;

    SavedPose* pose = GetPose(oldName);
    if (!pose) return false;

    // Update in category
    for (auto& cat : m_categories) {
        for (auto& poseName : cat.poseNames) {
            if (poseName == oldName) {
                poseName = newName;
            }
        }
    }

    pose->name = newName;
    return true;
}

SavedPose* PoseLibrary::GetPose(const std::string& name) {
    for (auto& pose : m_poses) {
        if (pose.name == name) {
            return &pose;
        }
    }
    return nullptr;
}

const SavedPose* PoseLibrary::GetPose(const std::string& name) const {
    for (const auto& pose : m_poses) {
        if (pose.name == name) {
            return &pose;
        }
    }
    return nullptr;
}

std::vector<const SavedPose*> PoseLibrary::GetPosesInCategory(const std::string& category) const {
    std::vector<const SavedPose*> result;

    for (const auto& pose : m_poses) {
        if (pose.category == category) {
            result.push_back(&pose);
        }
    }

    return result;
}

bool PoseLibrary::HasPose(const std::string& name) const {
    return GetPose(name) != nullptr;
}

// ============================================================================
// Category Management
// ============================================================================

PoseCategory* PoseLibrary::CreateCategory(const std::string& name) {
    // Check if exists
    for (auto& cat : m_categories) {
        if (cat.name == name) {
            return &cat;
        }
    }

    PoseCategory cat;
    cat.name = name;
    m_categories.push_back(cat);
    return &m_categories.back();
}

void PoseLibrary::DeleteCategory(const std::string& name) {
    // Move poses to Default
    auto* defaultCat = GetCategory("Default");

    for (const auto& poseName : GetCategory(name)->poseNames) {
        if (defaultCat) {
            defaultCat->poseNames.push_back(poseName);
        }
        if (auto* pose = GetPose(poseName)) {
            pose->category = "Default";
        }
    }

    m_categories.erase(
        std::remove_if(m_categories.begin(), m_categories.end(),
            [&name](const PoseCategory& c) { return c.name == name; }),
        m_categories.end()
    );
}

bool PoseLibrary::RenameCategory(const std::string& oldName, const std::string& newName) {
    if (GetCategory(newName)) return false;

    PoseCategory* cat = GetCategory(oldName);
    if (!cat) return false;

    // Update poses
    for (auto& pose : m_poses) {
        if (pose.category == oldName) {
            pose.category = newName;
        }
    }

    cat->name = newName;
    return true;
}

void PoseLibrary::MovePoseToCategory(const std::string& poseName, const std::string& categoryName) {
    SavedPose* pose = GetPose(poseName);
    if (!pose) return;

    // Remove from old category
    for (auto& cat : m_categories) {
        cat.poseNames.erase(
            std::remove(cat.poseNames.begin(), cat.poseNames.end(), poseName),
            cat.poseNames.end()
        );
    }

    // Add to new category
    if (auto* cat = GetCategory(categoryName)) {
        cat->poseNames.push_back(poseName);
    }

    pose->category = categoryName;
}

PoseCategory* PoseLibrary::GetCategory(const std::string& name) {
    for (auto& cat : m_categories) {
        if (cat.name == name) {
            return &cat;
        }
    }
    return nullptr;
}

// ============================================================================
// Pose Blending
// ============================================================================

void PoseLibrary::BlendWithPose(const std::string& poseName, float weight) {
    const SavedPose* pose = GetPose(poseName);
    if (!pose || !m_boneEditor) return;

    auto currentTransforms = m_boneEditor->CaptureTransformState();
    std::unordered_map<std::string, BoneTransform> blendedTransforms;

    for (const auto& [boneName, currentTransform] : currentTransforms) {
        auto it = pose->boneTransforms.find(boneName);
        if (it != pose->boneTransforms.end()) {
            blendedTransforms[boneName] = BoneTransform::Lerp(currentTransform, it->second, weight);
        } else {
            blendedTransforms[boneName] = currentTransform;
        }
    }

    m_boneEditor->SetAllTransforms(blendedTransforms);
}

PoseBlendResult PoseLibrary::BlendPoses(const std::string& poseA, const std::string& poseB, float weight) const {
    PoseBlendResult result;
    result.blendWeight = weight;

    const SavedPose* a = GetPose(poseA);
    const SavedPose* b = GetPose(poseB);

    if (!a || !b) return result;

    // Get all bone names from both poses
    std::set<std::string> allBones;
    for (const auto& [name, _] : a->boneTransforms) allBones.insert(name);
    for (const auto& [name, _] : b->boneTransforms) allBones.insert(name);

    for (const auto& boneName : allBones) {
        auto itA = a->boneTransforms.find(boneName);
        auto itB = b->boneTransforms.find(boneName);

        if (itA != a->boneTransforms.end() && itB != b->boneTransforms.end()) {
            result.resultPose[boneName] = BoneTransform::Lerp(itA->second, itB->second, weight);
        } else if (itA != a->boneTransforms.end()) {
            result.resultPose[boneName] = itA->second;
        } else if (itB != b->boneTransforms.end()) {
            result.resultPose[boneName] = itB->second;
        }
    }

    return result;
}

void PoseLibrary::AdditivePose(const std::string& poseName, float weight) {
    const SavedPose* pose = GetPose(poseName);
    if (!pose || !m_boneEditor) return;

    auto currentTransforms = m_boneEditor->CaptureTransformState();

    for (const auto& [boneName, additiveTransform] : pose->boneTransforms) {
        auto it = currentTransforms.find(boneName);
        if (it != currentTransforms.end()) {
            // Add position
            it->second.position += additiveTransform.position * weight;

            // Multiply rotation
            glm::quat additiveRot = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                additiveTransform.rotation, weight);
            it->second.rotation = additiveRot * it->second.rotation;

            // Multiply scale
            it->second.scale *= glm::mix(glm::vec3(1.0f), additiveTransform.scale, weight);
        }
    }

    m_boneEditor->SetAllTransforms(currentTransforms);
}

void PoseLibrary::ApplyPosePartial(const std::string& poseName, const std::vector<std::string>& bones, float weight) {
    const SavedPose* pose = GetPose(poseName);
    if (!pose || !m_boneEditor) return;

    auto currentTransforms = m_boneEditor->CaptureTransformState();

    for (const auto& boneName : bones) {
        auto currentIt = currentTransforms.find(boneName);
        auto poseIt = pose->boneTransforms.find(boneName);

        if (currentIt != currentTransforms.end() && poseIt != pose->boneTransforms.end()) {
            currentIt->second = BoneTransform::Lerp(currentIt->second, poseIt->second, weight);
        }
    }

    m_boneEditor->SetAllTransforms(currentTransforms);
}

std::unordered_map<std::string, BoneTransform> PoseLibrary::BlendMultiplePoses(
    const std::vector<std::pair<std::string, float>>& posesAndWeights) const {

    std::unordered_map<std::string, BoneTransform> result;
    float totalWeight = 0.0f;

    // Calculate total weight
    for (const auto& [poseName, weight] : posesAndWeights) {
        totalWeight += weight;
    }

    if (totalWeight <= 0.0f) return result;

    // Initialize with first pose
    for (const auto& [poseName, weight] : posesAndWeights) {
        const SavedPose* pose = GetPose(poseName);
        if (!pose) continue;

        float normalizedWeight = weight / totalWeight;

        for (const auto& [boneName, transform] : pose->boneTransforms) {
            auto it = result.find(boneName);
            if (it == result.end()) {
                // First contribution
                BoneTransform weighted;
                weighted.position = transform.position * normalizedWeight;
                weighted.rotation = glm::slerp(glm::quat(1, 0, 0, 0), transform.rotation, normalizedWeight);
                weighted.scale = transform.scale * normalizedWeight;
                result[boneName] = weighted;
            } else {
                // Add contribution
                it->second.position += transform.position * normalizedWeight;
                it->second.rotation = glm::slerp(it->second.rotation, transform.rotation,
                                                  normalizedWeight / (totalWeight - weight + normalizedWeight));
                it->second.scale += (transform.scale - glm::vec3(1.0f)) * normalizedWeight;
            }
        }
    }

    return result;
}

// ============================================================================
// Thumbnails
// ============================================================================

void PoseLibrary::GenerateThumbnail(const std::string& poseName) {
    SavedPose* pose = GetPose(poseName);
    if (!pose) return;

    pose->thumbnailPath = GetThumbnailPath(poseName);

    if (m_thumbnailGenerator) {
        m_thumbnailGenerator(pose->thumbnailPath, pose->boneTransforms);
    }
}

void PoseLibrary::RegenerateAllThumbnails() {
    for (auto& pose : m_poses) {
        GenerateThumbnail(pose.name);
    }
}

std::string PoseLibrary::GetThumbnailPath(const std::string& poseName) const {
    return m_config.thumbnailPath + "/" + poseName + ".png";
}

// ============================================================================
// Import/Export
// ============================================================================

SavedPose* PoseLibrary::ImportFromAnimation(const std::string& /*animationPath*/, float /*time*/, const std::string& poseName) {
    // In a full implementation, this would load an animation file and sample it
    // For now, create an empty pose
    return SavePose(poseName, std::unordered_map<std::string, BoneTransform>{}, "Imported");
}

std::vector<std::string> PoseLibrary::ImportAllPosesFromAnimation(const std::string& animationPath, float interval) {
    std::vector<std::string> importedNames;

    // Placeholder - would iterate through animation at intervals
    float duration = 1.0f;  // Would get from animation
    for (float t = 0.0f; t <= duration; t += interval) {
        std::string name = animationPath + "_" + std::to_string(static_cast<int>(t * 1000));
        if (ImportFromAnimation(animationPath, t, name)) {
            importedNames.push_back(name);
        }
    }

    return importedNames;
}

bool PoseLibrary::ExportPose(const std::string& poseName, const std::string& filePath) const {
    const SavedPose* pose = GetPose(poseName);
    if (!pose) return false;

    json j;
    j["name"] = pose->name;
    j["category"] = pose->category;
    j["description"] = pose->description;
    j["tags"] = pose->tags;
    j["isAdditive"] = pose->isAdditive;

    json transforms;
    for (const auto& [boneName, transform] : pose->boneTransforms) {
        json t;
        t["position"] = {transform.position.x, transform.position.y, transform.position.z};
        t["rotation"] = {transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w};
        t["scale"] = {transform.scale.x, transform.scale.y, transform.scale.z};
        transforms[boneName] = t;
    }
    j["boneTransforms"] = transforms;

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << j.dump(2);
    return true;
}

SavedPose* PoseLibrary::ImportPose(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return nullptr;

    json j;
    file >> j;

    std::string name = j.value("name", "ImportedPose");
    std::string category = j.value("category", "Imported");

    std::unordered_map<std::string, BoneTransform> transforms;

    if (j.contains("boneTransforms")) {
        for (auto& [boneName, t] : j["boneTransforms"].items()) {
            BoneTransform transform;
            if (t.contains("position")) {
                auto& p = t["position"];
                transform.position = glm::vec3(p[0], p[1], p[2]);
            }
            if (t.contains("rotation")) {
                auto& r = t["rotation"];
                transform.rotation = glm::quat(r[3], r[0], r[1], r[2]);
            }
            if (t.contains("scale")) {
                auto& s = t["scale"];
                transform.scale = glm::vec3(s[0], s[1], s[2]);
            }
            transforms[boneName] = transform;
        }
    }

    SavedPose* pose = SavePose(name, transforms, category);
    if (pose) {
        pose->description = j.value("description", "");
        pose->isAdditive = j.value("isAdditive", false);
        if (j.contains("tags")) {
            pose->tags = j["tags"].get<std::vector<std::string>>();
        }
    }

    return pose;
}

bool PoseLibrary::ExportLibrary(const std::string& filePath) const {
    json j;

    json categories;
    for (const auto& cat : m_categories) {
        json c;
        c["name"] = cat.name;
        c["color"] = {cat.color.r, cat.color.g, cat.color.b, cat.color.a};
        c["poseNames"] = cat.poseNames;
        categories.push_back(c);
    }
    j["categories"] = categories;

    json poses;
    for (const auto& pose : m_poses) {
        json p;
        p["name"] = pose.name;
        p["category"] = pose.category;
        p["description"] = pose.description;
        p["tags"] = pose.tags;
        p["isAdditive"] = pose.isAdditive;
        p["timestamp"] = pose.timestamp;

        json transforms;
        for (const auto& [boneName, transform] : pose.boneTransforms) {
            json t;
            t["position"] = {transform.position.x, transform.position.y, transform.position.z};
            t["rotation"] = {transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w};
            t["scale"] = {transform.scale.x, transform.scale.y, transform.scale.z};
            transforms[boneName] = t;
        }
        p["boneTransforms"] = transforms;

        poses.push_back(p);
    }
    j["poses"] = poses;

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << j.dump(2);
    return true;
}

bool PoseLibrary::ImportLibrary(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    json j;
    file >> j;

    // Import categories
    if (j.contains("categories")) {
        for (const auto& c : j["categories"]) {
            std::string name = c.value("name", "");
            if (!name.empty()) {
                PoseCategory* cat = CreateCategory(name);
                if (c.contains("color")) {
                    auto& col = c["color"];
                    cat->color = glm::vec4(col[0], col[1], col[2], col[3]);
                }
            }
        }
    }

    // Import poses
    if (j.contains("poses")) {
        for (const auto& p : j["poses"]) {
            std::string name = p.value("name", "");
            if (name.empty()) continue;

            std::unordered_map<std::string, BoneTransform> transforms;
            if (p.contains("boneTransforms")) {
                for (auto& [boneName, t] : p["boneTransforms"].items()) {
                    BoneTransform transform;
                    if (t.contains("position")) {
                        auto& pos = t["position"];
                        transform.position = glm::vec3(pos[0], pos[1], pos[2]);
                    }
                    if (t.contains("rotation")) {
                        auto& r = t["rotation"];
                        transform.rotation = glm::quat(r[3], r[0], r[1], r[2]);
                    }
                    if (t.contains("scale")) {
                        auto& s = t["scale"];
                        transform.scale = glm::vec3(s[0], s[1], s[2]);
                    }
                    transforms[boneName] = transform;
                }
            }

            SavedPose* pose = SavePose(name, transforms, p.value("category", "Default"));
            if (pose) {
                pose->description = p.value("description", "");
                pose->isAdditive = p.value("isAdditive", false);
                pose->timestamp = p.value("timestamp", GetTimestamp());
                if (p.contains("tags")) {
                    pose->tags = p["tags"].get<std::vector<std::string>>();
                }
            }
        }
    }

    return true;
}

// ============================================================================
// Persistence
// ============================================================================

bool PoseLibrary::SaveLibrary() {
    return ExportLibrary(GetLibraryFilePath());
}

bool PoseLibrary::LoadLibrary() {
    return ImportLibrary(GetLibraryFilePath());
}

std::string PoseLibrary::GetLibraryFilePath() const {
    return m_config.libraryPath + "/pose_library.json";
}

// ============================================================================
// Search & Filter
// ============================================================================

std::vector<const SavedPose*> PoseLibrary::SearchPoses(const std::string& query) const {
    std::vector<const SavedPose*> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& pose : m_poses) {
        std::string lowerName = pose.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos) {
            results.push_back(&pose);
        }
    }

    return results;
}

std::vector<const SavedPose*> PoseLibrary::FilterByTags(const std::vector<std::string>& tags) const {
    std::vector<const SavedPose*> results;

    for (const auto& pose : m_poses) {
        bool hasAllTags = true;
        for (const auto& tag : tags) {
            if (std::find(pose.tags.begin(), pose.tags.end(), tag) == pose.tags.end()) {
                hasAllTags = false;
                break;
            }
        }
        if (hasAllTags) {
            results.push_back(&pose);
        }
    }

    return results;
}

std::vector<const SavedPose*> PoseLibrary::GetRecentPoses(size_t count) const {
    std::vector<const SavedPose*> sorted;
    for (const auto& pose : m_poses) {
        sorted.push_back(&pose);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const SavedPose* a, const SavedPose* b) {
            return a->timestamp > b->timestamp;
        });

    if (sorted.size() > count) {
        sorted.resize(count);
    }

    return sorted;
}

// ============================================================================
// Utilities
// ============================================================================

std::unordered_map<std::string, BoneTransform> PoseLibrary::MirrorPose(const std::string& poseName) const {
    const SavedPose* pose = GetPose(poseName);
    if (!pose) return {};

    std::unordered_map<std::string, BoneTransform> mirrored;

    for (const auto& [boneName, transform] : pose->boneTransforms) {
        // Find mirrored bone name
        std::string mirroredName = boneName;
        size_t leftPos = boneName.find("_L");
        size_t rightPos = boneName.find("_R");

        if (leftPos != std::string::npos) {
            mirroredName.replace(leftPos, 2, "_R");
        } else if (rightPos != std::string::npos) {
            mirroredName.replace(rightPos, 2, "_L");
        }

        BoneTransform mirroredTransform = transform;
        mirroredTransform.position.x = -transform.position.x;
        mirroredTransform.rotation.y = -transform.rotation.y;
        mirroredTransform.rotation.z = -transform.rotation.z;

        mirrored[mirroredName] = mirroredTransform;
    }

    return mirrored;
}

SavedPose* PoseLibrary::CreateAdditivePose(const std::string& basePose, const std::string& targetPose, const std::string& newName) {
    const SavedPose* base = GetPose(basePose);
    const SavedPose* target = GetPose(targetPose);
    if (!base || !target) return nullptr;

    std::unordered_map<std::string, BoneTransform> additiveTransforms;

    for (const auto& [boneName, targetTransform] : target->boneTransforms) {
        auto baseIt = base->boneTransforms.find(boneName);
        if (baseIt != base->boneTransforms.end()) {
            BoneTransform additive;
            additive.position = targetTransform.position - baseIt->second.position;
            additive.rotation = glm::inverse(baseIt->second.rotation) * targetTransform.rotation;
            additive.scale = targetTransform.scale / baseIt->second.scale;
            additiveTransforms[boneName] = additive;
        }
    }

    SavedPose* pose = SavePose(newName, additiveTransforms, "Additive");
    if (pose) {
        pose->isAdditive = true;
    }

    return pose;
}

SavedPose* PoseLibrary::DuplicatePose(const std::string& poseName, const std::string& newName) {
    const SavedPose* original = GetPose(poseName);
    if (!original) return nullptr;

    return SavePose(newName, original->boneTransforms, original->category);
}

// ============================================================================
// Private Methods
// ============================================================================

uint64_t PoseLibrary::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count());
}

void PoseLibrary::SortPosesByTimestamp() {
    std::sort(m_poses.begin(), m_poses.end(),
        [](const SavedPose& a, const SavedPose& b) {
            return a.timestamp > b.timestamp;
        });
}

} // namespace Vehement
