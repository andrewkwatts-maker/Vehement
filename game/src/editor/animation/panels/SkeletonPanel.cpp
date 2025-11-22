#include "SkeletonPanel.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace Vehement {

SkeletonPanel::SkeletonPanel() = default;
SkeletonPanel::~SkeletonPanel() = default;

void SkeletonPanel::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void SkeletonPanel::Render() {
    if (!m_initialized || !m_boneEditor) return;

    // Search bar
    if (m_config.showSearchBar) {
        char searchBuffer[256];
        std::strncpy(searchBuffer, m_searchFilter.c_str(), sizeof(searchBuffer) - 1);
        searchBuffer[sizeof(searchBuffer) - 1] = '\0';

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##search", "Search bones...", searchBuffer, sizeof(searchBuffer))) {
            m_searchFilter = searchBuffer;
        }

        ImGui::Separator();
    }

    // Toolbar
    if (ImGui::Button("Expand All")) {
        ExpandAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Collapse All")) {
        CollapseAll();
    }
    ImGui::SameLine();
    if (ImGui::Button("Select All")) {
        m_boneEditor->SelectAll();
    }

    ImGui::Separator();

    // Bone tree
    ImGui::BeginChild("BoneTree", ImVec2(0, 0), true);

    auto rootBones = m_boneEditor->GetRootBones();
    for (const auto& rootBone : rootBones) {
        RenderBoneNode(rootBone, 0);
    }

    ImGui::EndChild();
}

void SkeletonPanel::ExpandAll() {
    if (!m_boneEditor) return;

    for (const auto& boneName : m_boneEditor->GetBonesInHierarchyOrder()) {
        m_expandedNodes[boneName] = true;
    }
}

void SkeletonPanel::CollapseAll() {
    m_expandedNodes.clear();
}

void SkeletonPanel::ExpandToBone(const std::string& boneName) {
    if (!m_boneEditor) return;

    std::string current = m_boneEditor->GetParentBone(boneName);
    while (!current.empty()) {
        m_expandedNodes[current] = true;
        current = m_boneEditor->GetParentBone(current);
    }
}

void SkeletonPanel::RenderBoneNode(const std::string& boneName, int depth) {
    if (!m_boneEditor) return;

    // Check filter
    if (!m_searchFilter.empty() && !MatchesFilter(boneName) && !HasVisibleChildren(boneName)) {
        return;
    }

    auto children = m_boneEditor->GetChildBones(boneName);
    bool hasChildren = !children.empty();
    bool isSelected = m_boneEditor->IsBoneSelected(boneName);
    bool isExpanded = m_expandedNodes[boneName];

    // Force expand if filter matches child
    if (!m_searchFilter.empty() && HasVisibleChildren(boneName)) {
        isExpanded = true;
        m_expandedNodes[boneName] = true;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                ImGuiTreeNodeFlags_SpanAvailWidth;

    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (isExpanded) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // Visibility toggle
    if (m_config.showVisibilityToggle) {
        bool visible = m_visibleBones.count(boneName) == 0 || m_visibleBones[boneName];
        ImGui::PushID((boneName + "_vis").c_str());
        if (ImGui::Checkbox("##vis", &visible)) {
            m_visibleBones[boneName] = visible;
            if (OnBoneVisibilityChanged) {
                OnBoneVisibilityChanged(boneName, visible);
            }
        }
        ImGui::PopID();
        ImGui::SameLine();
    }

    // Lock toggle
    if (m_config.showLockToggle) {
        bool locked = m_lockedBones[boneName];
        ImGui::PushID((boneName + "_lock").c_str());
        if (ImGui::Checkbox("##lock", &locked)) {
            m_lockedBones[boneName] = locked;
        }
        ImGui::PopID();
        ImGui::SameLine();
    }

    // Highlight if matches filter
    if (!m_searchFilter.empty() && MatchesFilter(boneName)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    }

    bool nodeOpen = ImGui::TreeNodeEx(boneName.c_str(), flags);

    if (!m_searchFilter.empty() && MatchesFilter(boneName)) {
        ImGui::PopStyleColor();
    }

    // Update expanded state
    if (hasChildren) {
        m_expandedNodes[boneName] = nodeOpen;
    }

    // Handle selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        bool addToSelection = ImGui::GetIO().KeyCtrl;
        m_boneEditor->SelectBone(boneName, addToSelection);

        if (OnBoneSelected) {
            OnBoneSelected(boneName);
        }
    }

    // Handle double-click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (OnBoneDoubleClicked) {
            OnBoneDoubleClicked(boneName);
        }
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Select")) {
            m_boneEditor->SelectBone(boneName);
        }
        if (ImGui::MenuItem("Select Hierarchy")) {
            m_boneEditor->SelectHierarchy(boneName);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset to Bind Pose")) {
            m_boneEditor->ResetBoneToBindPose(boneName);
        }
        if (ImGui::MenuItem("Mirror Pose")) {
            m_boneEditor->MirrorSelectedBones();
        }
        ImGui::EndPopup();
    }

    // Render children
    if (hasChildren && nodeOpen) {
        for (const auto& child : children) {
            RenderBoneNode(child, depth + 1);
        }
        ImGui::TreePop();
    }
}

bool SkeletonPanel::MatchesFilter(const std::string& boneName) const {
    if (m_searchFilter.empty()) return true;

    std::string lowerName = boneName;
    std::string lowerFilter = m_searchFilter;

    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    return lowerName.find(lowerFilter) != std::string::npos;
}

bool SkeletonPanel::HasVisibleChildren(const std::string& boneName) const {
    if (!m_boneEditor) return false;

    auto children = m_boneEditor->GetChildBones(boneName);
    for (const auto& child : children) {
        if (MatchesFilter(child) || HasVisibleChildren(child)) {
            return true;
        }
    }
    return false;
}

} // namespace Vehement
