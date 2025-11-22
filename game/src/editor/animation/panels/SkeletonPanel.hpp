#pragma once

#include "../BoneAnimationEditor.hpp"
#include <string>
#include <functional>
#include <vector>

namespace Vehement {

/**
 * @brief Skeleton hierarchy panel for bone tree display
 *
 * Features:
 * - Bone hierarchy tree view
 * - Selection handling
 * - Bone visibility toggle
 * - Search/filter
 */
class SkeletonPanel {
public:
    struct Config {
        bool showSearchBar = true;
        bool showVisibilityToggle = true;
        bool showLockToggle = true;
        bool expandAllByDefault = true;
    };

    SkeletonPanel();
    ~SkeletonPanel();

    /**
     * @brief Initialize panel
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set bone animation editor reference
     */
    void SetBoneEditor(BoneAnimationEditor* editor) { m_boneEditor = editor; }

    /**
     * @brief Render the panel (ImGui)
     */
    void Render();

    /**
     * @brief Set search filter
     */
    void SetSearchFilter(const std::string& filter) { m_searchFilter = filter; }

    /**
     * @brief Get search filter
     */
    [[nodiscard]] const std::string& GetSearchFilter() const { return m_searchFilter; }

    /**
     * @brief Expand all nodes
     */
    void ExpandAll();

    /**
     * @brief Collapse all nodes
     */
    void CollapseAll();

    /**
     * @brief Expand to bone (show parents)
     */
    void ExpandToBone(const std::string& boneName);

    // Callbacks
    std::function<void(const std::string&)> OnBoneSelected;
    std::function<void(const std::string&)> OnBoneDoubleClicked;
    std::function<void(const std::string&, bool)> OnBoneVisibilityChanged;

private:
    void RenderBoneNode(const std::string& boneName, int depth);
    bool MatchesFilter(const std::string& boneName) const;
    bool HasVisibleChildren(const std::string& boneName) const;

    Config m_config;
    BoneAnimationEditor* m_boneEditor = nullptr;

    std::string m_searchFilter;
    std::unordered_map<std::string, bool> m_expandedNodes;
    std::unordered_map<std::string, bool> m_visibleBones;
    std::unordered_map<std::string, bool> m_lockedBones;

    bool m_initialized = false;
};

} // namespace Vehement
