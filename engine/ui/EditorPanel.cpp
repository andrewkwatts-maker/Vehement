#include "EditorPanel.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

namespace Nova {

// ============================================================================
// EditorPanel Implementation
// ============================================================================

bool EditorPanel::Initialize(const Config& config) {
    m_config = config;
    m_id = config.id.empty() ? config.title : config.id;
    m_visible = config.defaultOpen;

    OnInitialize();
    m_initialized = true;
    return true;
}

void EditorPanel::Shutdown() {
    if (m_initialized) {
        OnShutdown();
        m_initialized = false;
    }
}

void EditorPanel::Update(float deltaTime) {
    (void)deltaTime;
}

void EditorPanel::Render() {
    if (!m_initialized || !m_visible) {
        if (m_wasVisible && !m_visible) {
            OnHide();
            if (OnClosed) OnClosed();
        }
        m_wasVisible = m_visible;
        return;
    }

    if (!m_wasVisible && m_visible) {
        OnShow();
        if (OnOpened) OnOpened();
    }
    m_wasVisible = m_visible;

    // Build window flags
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (m_config.flags & Flags::NoTitleBar) flags |= ImGuiWindowFlags_NoTitleBar;
    if (m_config.flags & Flags::NoResize) flags |= ImGuiWindowFlags_NoResize;
    if (m_config.flags & Flags::NoMove) flags |= ImGuiWindowFlags_NoMove;
    if (m_config.flags & Flags::NoCollapse) flags |= ImGuiWindowFlags_NoCollapse;
    if (m_config.flags & Flags::NoScrollbar) flags |= ImGuiWindowFlags_NoScrollbar;
    if (m_config.flags & Flags::NoBackground) flags |= ImGuiWindowFlags_NoBackground;
#ifdef IMGUI_HAS_DOCK
    if (m_config.flags & Flags::NoDocking) flags |= ImGuiWindowFlags_NoDocking;
#endif
    if (m_config.flags & Flags::AlwaysAutoResize) flags |= ImGuiWindowFlags_AlwaysAutoResize;
    if (m_config.flags & Flags::HasMenuBar) flags |= ImGuiWindowFlags_MenuBar;

    // Set size constraints
    if (m_config.minSize.x > 0 || m_config.minSize.y > 0 || m_config.maxSize.x > 0 || m_config.maxSize.y > 0) {
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(m_config.minSize.x, m_config.minSize.y),
            ImVec2(m_config.maxSize.x > 0 ? m_config.maxSize.x : FLT_MAX,
                   m_config.maxSize.y > 0 ? m_config.maxSize.y : FLT_MAX)
        );
    }

    // Set default size on first appearance
    ImGui::SetNextWindowSize(ImVec2(m_config.defaultSize.x, m_config.defaultSize.y), ImGuiCond_FirstUseEver);

    // Focus if requested
    if (m_needsFocus) {
        ImGui::SetNextWindowFocus();
        m_needsFocus = false;
    }

    // Build title with dirty indicator
    std::string windowTitle = m_config.title;
    if (m_dirty) windowTitle += " *";
    windowTitle += "###" + m_id;

    bool windowOpen = m_visible;
    if (ImGui::Begin(windowTitle.c_str(), &windowOpen, flags)) {
        m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // Menu bar
        if ((m_config.flags & Flags::HasMenuBar) && ImGui::BeginMenuBar()) {
            OnRenderMenuBar();
            ImGui::EndMenuBar();
        }

        // Toolbar
        if (m_config.flags & Flags::HasToolbar) {
            EditorWidgets::BeginToolbar(("##toolbar_" + m_id).c_str());
            OnRenderToolbar();
            EditorWidgets::EndToolbar();
        }

        // Search bar
        if (m_config.flags & Flags::HasSearch) {
            RenderSearchBar();
        }

        // Main content
        OnRender();

        // Status bar
        if (m_config.flags & Flags::HasStatusBar) {
            RenderDefaultStatusBar();
        }
    }
    ImGui::End();

    if (!windowOpen && m_visible) {
        m_visible = false;
    }
}

void EditorPanel::Focus() {
    m_needsFocus = true;
    m_visible = true;
}

void EditorPanel::SetTitle(const std::string& title) {
    m_config.title = title;
}

void EditorPanel::SetSearchFilter(const std::string& filter) {
    if (m_searchFilter != filter) {
        m_searchFilter = filter;
        strncpy(m_searchBuffer, filter.c_str(), sizeof(m_searchBuffer) - 1);
        m_searchBuffer[sizeof(m_searchBuffer) - 1] = '\0';
        OnSearchChanged(filter);
    }
}

glm::vec2 EditorPanel::GetContentSize() const {
    ImVec2 size = ImGui::GetContentRegionAvail();
    return glm::vec2(size.x, size.y);
}

bool EditorPanel::BeginContextMenu(const char* id) {
    return ImGui::BeginPopupContextItem(id);
}

void EditorPanel::EndContextMenu() {
    ImGui::EndPopup();
}

bool EditorPanel::BeginPopupModal(const char* title, bool* open) {
    return ImGui::BeginPopupModal(title, open, ImGuiWindowFlags_AlwaysAutoResize);
}

void EditorPanel::EndPopupModal() {
    ImGui::EndPopup();
}

void EditorPanel::RecordUndoAction(const std::string& description, std::function<void()> undoFunc, std::function<void()> redoFunc) {
    // Clear redo stack when new action is recorded
    m_redoStack.clear();

    // Add to undo stack
    m_undoStack.push_back({description, std::move(undoFunc), std::move(redoFunc)});

    // Limit stack size
    while (m_undoStack.size() > MAX_UNDO) {
        m_undoStack.erase(m_undoStack.begin());
    }

    MarkDirty();
}

void EditorPanel::RenderSearchBar() {
    auto& theme = EditorTheme::Instance();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    if (EditorWidgets::SearchInput(("##search_" + m_id).c_str(), m_searchBuffer, sizeof(m_searchBuffer), "Search...")) {
        m_searchFilter = m_searchBuffer;
        OnSearchChanged(m_searchFilter);
    }

    ImGui::PopStyleVar();
    ImGui::Separator();
}

void EditorPanel::RenderDefaultStatusBar() {
    auto& theme = EditorTheme::Instance();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorTheme::ToImVec4(theme.GetColors().panelHeader));

    ImGui::BeginChild(("##statusbar_" + m_id).c_str(), ImVec2(0, theme.GetSizes().statusBarHeight), ImGuiChildFlags_None);

    if (!m_statusMessage.empty()) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(m_statusMessage.c_str());
    }

    // Show undo info if supported
    if (m_config.flags & Flags::CanUndo) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
        ImGui::TextDisabled("Undo: %zu", m_undoStack.size());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    OnRenderStatusBar();
}

// ============================================================================
// PanelRegistry Implementation
// ============================================================================

PanelRegistry& PanelRegistry::Instance() {
    static PanelRegistry instance;
    return instance;
}

void PanelRegistry::Register(const std::string& id, std::shared_ptr<EditorPanel> panel) {
    m_panels[id] = std::move(panel);
}

void PanelRegistry::Unregister(const std::string& id) {
    auto it = m_panels.find(id);
    if (it != m_panels.end()) {
        it->second->Shutdown();
        m_panels.erase(it);
    }
}

std::shared_ptr<EditorPanel> PanelRegistry::Get(const std::string& id) {
    auto it = m_panels.find(id);
    return it != m_panels.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<EditorPanel>> PanelRegistry::GetAll() {
    std::vector<std::shared_ptr<EditorPanel>> result;
    result.reserve(m_panels.size());
    for (auto& [_, panel] : m_panels) {
        result.push_back(panel);
    }
    return result;
}

std::vector<std::shared_ptr<EditorPanel>> PanelRegistry::GetByCategory(const std::string& category) {
    std::vector<std::shared_ptr<EditorPanel>> result;
    for (auto& [_, panel] : m_panels) {
        if (panel->GetConfig().category == category) {
            result.push_back(panel);
        }
    }
    return result;
}

void PanelRegistry::UpdateAll(float deltaTime) {
    for (auto& [_, panel] : m_panels) {
        panel->Update(deltaTime);
    }
}

void PanelRegistry::RenderAll() {
    for (auto& [_, panel] : m_panels) {
        panel->Render();
    }
}

void PanelRegistry::RenderViewMenu() {
    // Group by category
    std::unordered_map<std::string, std::vector<std::shared_ptr<EditorPanel>>> byCategory;
    for (auto& [_, panel] : m_panels) {
        byCategory[panel->GetConfig().category].push_back(panel);
    }

    // Render uncategorized first
    auto it = byCategory.find("");
    if (it != byCategory.end()) {
        for (auto& panel : it->second) {
            bool visible = panel->IsVisible();
            if (ImGui::MenuItem(panel->GetTitle().c_str(), panel->GetConfig().shortcut.c_str(), &visible)) {
                panel->SetVisible(visible);
            }
        }
        byCategory.erase(it);
    }

    // Render categories
    for (auto& [category, panels] : byCategory) {
        if (ImGui::BeginMenu(category.c_str())) {
            for (auto& panel : panels) {
                bool visible = panel->IsVisible();
                if (ImGui::MenuItem(panel->GetTitle().c_str(), panel->GetConfig().shortcut.c_str(), &visible)) {
                    panel->SetVisible(visible);
                }
            }
            ImGui::EndMenu();
        }
    }
}

// ============================================================================
// ListDetailPanel Implementation
// ============================================================================

void ListDetailPanel::OnRender() {
    // List panel
    ImGui::BeginChild("##list", ImVec2(m_listWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    OnRenderList();
    ImGui::EndChild();

    if (m_showDetail) {
        ImGui::SameLine();

        // Detail panel
        ImGui::BeginChild("##detail", ImVec2(0, 0), ImGuiChildFlags_Borders);
        OnRenderDetail();
        ImGui::EndChild();
    }
}

// ============================================================================
// TabbedPanel Implementation
// ============================================================================

void TabbedPanel::AddTab(const Tab& tab) {
    m_tabs.push_back(tab);
    if (m_activeTab.empty()) {
        m_activeTab = tab.name;
    }
}

void TabbedPanel::RemoveTab(const std::string& name) {
    m_tabs.erase(std::remove_if(m_tabs.begin(), m_tabs.end(),
        [&name](const Tab& t) { return t.name == name; }), m_tabs.end());

    if (m_activeTab == name && !m_tabs.empty()) {
        m_activeTab = m_tabs[0].name;
    }
}

void TabbedPanel::SetActiveTab(const std::string& name) {
    for (const auto& tab : m_tabs) {
        if (tab.name == name) {
            m_activeTab = name;
            break;
        }
    }
}

void TabbedPanel::OnRender() {
    if (ImGui::BeginTabBar(("##tabs_" + m_id).c_str())) {
        for (auto it = m_tabs.begin(); it != m_tabs.end(); ) {
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (it->closeable) flags |= ImGuiTabItemFlags_NoCloseWithMiddleMouseButton;

            bool open = true;
            std::string tabLabel = it->icon.empty() ? it->name : (it->icon + " " + it->name);

            if (ImGui::BeginTabItem(tabLabel.c_str(), it->closeable ? &open : nullptr, flags)) {
                m_activeTab = it->name;

                if (it->render) {
                    it->render();
                }

                ImGui::EndTabItem();
            }

            if (!open) {
                it = m_tabs.erase(it);
            } else {
                ++it;
            }
        }
        ImGui::EndTabBar();
    }
}

// ============================================================================
// TreePanel Implementation
// ============================================================================

const TreePanel::TreeNode* TreePanel::GetSelectedNode() const {
    std::function<const TreeNode*(const std::vector<TreeNode>&)> findNode;
    findNode = [this, &findNode](const std::vector<TreeNode>& nodes) -> const TreeNode* {
        for (const auto& node : nodes) {
            if (node.id == m_selectedNodeId) return &node;
            if (auto* found = findNode(node.children)) return found;
        }
        return nullptr;
    };
    return findNode(m_rootNodes);
}

void TreePanel::OnRender() {
    for (auto& node : m_rootNodes) {
        RenderNode(node);
    }
}

void TreePanel::RenderNode(TreeNode& node, int depth) {
    (void)depth;

    EditorWidgets::TreeNodeFlags flags = EditorWidgets::TreeNodeFlags::OpenOnArrow |
                                          EditorWidgets::TreeNodeFlags::SpanFullWidth;

    if (node.children.empty()) {
        flags = flags | EditorWidgets::TreeNodeFlags::Leaf;
    }
    if (node.id == m_selectedNodeId) {
        flags = flags | EditorWidgets::TreeNodeFlags::Selected;
    }

    bool open = EditorWidgets::TreeNode(node.label.c_str(), flags, node.icon.c_str());

    // Handle selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        m_selectedNodeId = node.id;
        node.selected = true;
        if (OnNodeSelected) OnNodeSelected(node);
    }

    // Handle double click
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (OnNodeDoubleClicked) OnNodeDoubleClicked(node);
    }

    // Handle context menu
    if (ImGui::BeginPopupContextItem()) {
        if (OnNodeContextMenu) OnNodeContextMenu(node);
        ImGui::EndPopup();
    }

    if (open) {
        node.expanded = true;
        for (auto& child : node.children) {
            RenderNode(child, depth + 1);
        }
        EditorWidgets::TreePop();
    } else {
        node.expanded = false;
    }
}

} // namespace Nova
