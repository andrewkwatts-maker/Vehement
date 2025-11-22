#pragma once

#include "BoneAnimationEditor.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Vehement {

/**
 * @brief Saved pose data
 */
struct SavedPose {
    std::string name;
    std::string category;
    std::string description;
    std::unordered_map<std::string, BoneTransform> boneTransforms;
    std::string thumbnailPath;
    uint64_t timestamp = 0;
    std::vector<std::string> tags;

    // Metadata for filtering
    bool isAdditive = false;
    bool isMirrored = false;
    std::string sourceAnimation;
    float sourceTime = 0.0f;
};

/**
 * @brief Pose category
 */
struct PoseCategory {
    std::string name;
    std::string icon;
    glm::vec4 color{0.5f, 0.5f, 0.8f, 1.0f};
    std::vector<std::string> poseNames;
    bool expanded = true;
};

/**
 * @brief Pose blend operation result
 */
struct PoseBlendResult {
    std::unordered_map<std::string, BoneTransform> resultPose;
    float blendWeight = 0.0f;
};

/**
 * @brief Pose library for saving/loading poses
 *
 * Features:
 * - Named poses
 * - Pose thumbnails
 * - Pose blending
 * - Pose categories
 * - Import from other animations
 */
class PoseLibrary {
public:
    struct Config {
        std::string libraryPath = "assets/poses";
        std::string thumbnailPath = "assets/poses/thumbnails";
        int thumbnailSize = 128;
        bool autoGenerateThumbnails = true;
    };

    PoseLibrary();
    ~PoseLibrary();

    /**
     * @brief Initialize the pose library
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set bone animation editor reference
     */
    void SetBoneEditor(BoneAnimationEditor* editor) { m_boneEditor = editor; }

    // =========================================================================
    // Pose Management
    // =========================================================================

    /**
     * @brief Save current pose
     */
    SavedPose* SavePose(const std::string& name, const std::string& category = "Default");

    /**
     * @brief Save pose from transforms
     */
    SavedPose* SavePose(const std::string& name,
                        const std::unordered_map<std::string, BoneTransform>& transforms,
                        const std::string& category = "Default");

    /**
     * @brief Load pose by name
     */
    bool LoadPose(const std::string& name);

    /**
     * @brief Delete pose
     */
    void DeletePose(const std::string& name);

    /**
     * @brief Rename pose
     */
    bool RenamePose(const std::string& oldName, const std::string& newName);

    /**
     * @brief Get pose by name
     */
    [[nodiscard]] SavedPose* GetPose(const std::string& name);
    [[nodiscard]] const SavedPose* GetPose(const std::string& name) const;

    /**
     * @brief Get all poses
     */
    [[nodiscard]] const std::vector<SavedPose>& GetAllPoses() const { return m_poses; }

    /**
     * @brief Get poses in category
     */
    [[nodiscard]] std::vector<const SavedPose*> GetPosesInCategory(const std::string& category) const;

    /**
     * @brief Check if pose exists
     */
    [[nodiscard]] bool HasPose(const std::string& name) const;

    // =========================================================================
    // Category Management
    // =========================================================================

    /**
     * @brief Create category
     */
    PoseCategory* CreateCategory(const std::string& name);

    /**
     * @brief Delete category
     */
    void DeleteCategory(const std::string& name);

    /**
     * @brief Rename category
     */
    bool RenameCategory(const std::string& oldName, const std::string& newName);

    /**
     * @brief Move pose to category
     */
    void MovePoseToCategory(const std::string& poseName, const std::string& categoryName);

    /**
     * @brief Get category
     */
    [[nodiscard]] PoseCategory* GetCategory(const std::string& name);

    /**
     * @brief Get all categories
     */
    [[nodiscard]] const std::vector<PoseCategory>& GetCategories() const { return m_categories; }

    // =========================================================================
    // Pose Blending
    // =========================================================================

    /**
     * @brief Blend current pose with saved pose
     */
    void BlendWithPose(const std::string& poseName, float weight);

    /**
     * @brief Blend two poses
     */
    [[nodiscard]] PoseBlendResult BlendPoses(const std::string& poseA, const std::string& poseB, float weight) const;

    /**
     * @brief Additive blend pose onto current
     */
    void AdditivePose(const std::string& poseName, float weight);

    /**
     * @brief Apply pose partially (selected bones only)
     */
    void ApplyPosePartial(const std::string& poseName, const std::vector<std::string>& bones, float weight = 1.0f);

    /**
     * @brief Blend multiple poses with weights
     */
    [[nodiscard]] std::unordered_map<std::string, BoneTransform> BlendMultiplePoses(
        const std::vector<std::pair<std::string, float>>& posesAndWeights) const;

    // =========================================================================
    // Thumbnails
    // =========================================================================

    /**
     * @brief Generate thumbnail for pose
     */
    void GenerateThumbnail(const std::string& poseName);

    /**
     * @brief Regenerate all thumbnails
     */
    void RegenerateAllThumbnails();

    /**
     * @brief Get thumbnail path
     */
    [[nodiscard]] std::string GetThumbnailPath(const std::string& poseName) const;

    /**
     * @brief Set thumbnail generator callback
     */
    void SetThumbnailGenerator(std::function<void(const std::string&, const std::unordered_map<std::string, BoneTransform>&)> generator) {
        m_thumbnailGenerator = std::move(generator);
    }

    // =========================================================================
    // Import/Export
    // =========================================================================

    /**
     * @brief Import pose from animation at time
     */
    SavedPose* ImportFromAnimation(const std::string& animationPath, float time, const std::string& poseName);

    /**
     * @brief Import poses from animation file
     */
    std::vector<std::string> ImportAllPosesFromAnimation(const std::string& animationPath, float interval = 0.5f);

    /**
     * @brief Export pose to file
     */
    bool ExportPose(const std::string& poseName, const std::string& filePath) const;

    /**
     * @brief Import pose from file
     */
    SavedPose* ImportPose(const std::string& filePath);

    /**
     * @brief Export entire library
     */
    bool ExportLibrary(const std::string& filePath) const;

    /**
     * @brief Import library from file
     */
    bool ImportLibrary(const std::string& filePath);

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Save library to disk
     */
    bool SaveLibrary();

    /**
     * @brief Load library from disk
     */
    bool LoadLibrary();

    /**
     * @brief Get library file path
     */
    [[nodiscard]] std::string GetLibraryFilePath() const;

    // =========================================================================
    // Search & Filter
    // =========================================================================

    /**
     * @brief Search poses by name
     */
    [[nodiscard]] std::vector<const SavedPose*> SearchPoses(const std::string& query) const;

    /**
     * @brief Filter poses by tags
     */
    [[nodiscard]] std::vector<const SavedPose*> FilterByTags(const std::vector<std::string>& tags) const;

    /**
     * @brief Get recent poses
     */
    [[nodiscard]] std::vector<const SavedPose*> GetRecentPoses(size_t count = 10) const;

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Mirror pose
     */
    [[nodiscard]] std::unordered_map<std::string, BoneTransform> MirrorPose(const std::string& poseName) const;

    /**
     * @brief Create additive pose from difference
     */
    SavedPose* CreateAdditivePose(const std::string& basePose, const std::string& targetPose, const std::string& newName);

    /**
     * @brief Copy pose
     */
    SavedPose* DuplicatePose(const std::string& poseName, const std::string& newName);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const SavedPose&)> OnPoseSaved;
    std::function<void(const std::string&)> OnPoseDeleted;
    std::function<void(const SavedPose&)> OnPoseApplied;

private:
    uint64_t GetTimestamp() const;
    void SortPosesByTimestamp();

    Config m_config;
    BoneAnimationEditor* m_boneEditor = nullptr;

    std::vector<SavedPose> m_poses;
    std::vector<PoseCategory> m_categories;

    std::function<void(const std::string&, const std::unordered_map<std::string, BoneTransform>&)> m_thumbnailGenerator;

    bool m_initialized = false;
};

} // namespace Vehement
