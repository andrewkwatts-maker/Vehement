#pragma once

#include "EditorTheme.hpp"
#include "EditorWidgets.hpp"
#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace Nova {

/**
 * @brief Base class for all editor panels
 *
 * Provides consistent:
 * - Window management (docking, visibility)
 * - Toolbar rendering
 * - Status bar
 * - Undo/redo support hooks
 * - Search/filter
 * - Keyboard shortcuts
 */
class EditorPanel {
public:
    /**
     * @brief Panel flags for customization
     */
    enum class Flags : uint32_t {
        None = 0,
        NoTitleBar = 1 << 0,
        NoResize = 1 << 1,
        NoMove = 1 << 2,
        NoCollapse = 1 << 3,
        NoScrollbar = 1 << 4,
        NoBackground = 1 << 5,
        NoDocking = 1 << 6,
        AlwaysAutoResize = 1 << 7,
        HasToolbar = 1 << 8,
        HasStatusBar = 1 << 9,
        HasMenuBar = 1 << 10,
        HasSearch = 1 << 11,
        CanUndo = 1 << 12
    };

    friend Flags operator|(Flags a, Flags b) {
        return static_cast<Flags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    friend bool operator&(Flags a, Flags b) {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }

    /**
     * @brief Panel configuration
     */
    struct Config {
        std::string title = "Panel";
        std::string id;                         // Unique ID (defaults to title)
        Flags flags = Flags::None;
        glm::vec2 minSize{100, 100};
        glm::vec2 maxSize{0, 0};               // 0 = unlimited
        glm::vec2 defaultSize{400, 300};
        bool defaultOpen = true;
        std::string category;                   // For menu organization
        std::string tooltip;
        std::string shortcut;                   // e.g., "Ctrl+Shift+P"
    };

    EditorPanel() = default;
    virtual ~EditorPanel() = default;

    // Non-copyable
    EditorPanel(const EditorPanel&) = delete;
    EditorPanel& operator=(const EditorPanel&) = delete;

    /**
     * @brief Initialize the panel
     */
    virtual bool Initialize(const Config& config);

    /**
     * @brief Shutdown the panel
     */
    virtual void Shutdown();

    /**
     * @brief Update panel state (called every frame)
     */
    virtual void Update(float deltaTime);

    /**
     * @brief Render the panel
     */
    void Render();

    // =========================================================================
    // Visibility
    // =========================================================================

    void Show() { m_visible = true; }
    void Hide() { m_visible = false; }
    void Toggle() { m_visible = !m_visible; }
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Focus this panel
     */
    void Focus();

    /**
     * @brief Check if panel has focus
     */
    bool IsFocused() const { return m_focused; }

    // =========================================================================
    // Configuration
    // =========================================================================

    const Config& GetConfig() const { return m_config; }
    const std::string& GetTitle() const { return m_config.title; }
    const std::string& GetID() const { return m_id; }

    /**
     * @brief Set panel title
     */
    void SetTitle(const std::string& title);

    // =========================================================================
    // Undo/Redo Hooks
    // =========================================================================

    /**
     * @brief Called when undo is requested
     */
    virtual void OnUndo() {}

    /**
     * @brief Called when redo is requested
     */
    virtual void OnRedo() {}

    /**
     * @brief Check if undo is available
     */
    virtual bool CanUndo() const { return false; }

    /**
     * @brief Check if redo is available
     */
    virtual bool CanRedo() const { return false; }

    // =========================================================================
    // Search
    // =========================================================================

    /**
     * @brief Get current search filter
     */
    const std::string& GetSearchFilter() const { return m_searchFilter; }

    /**
     * @brief Set search filter
     */
    void SetSearchFilter(const std::string& filter);

    /**
     * @brief Called when search filter changes
     */
    virtual void OnSearchChanged(const std::string& filter) { (void)filter; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /** Called when panel is opened */
    std::function<void()> OnOpened;

    /** Called when panel is closed */
    std::function<void()> OnClosed;

    /** Called when panel gains focus */
    std::function<void()> OnFocused;

    /** Called when dirty state changes */
    std::function<void(bool)> OnDirtyChanged;

    // =========================================================================
    // Dirty State
    // =========================================================================

    bool IsDirty() const { return m_dirty; }

    void MarkDirty() {
        if (!m_dirty) {
            m_dirty = true;
            if (OnDirtyChanged) OnDirtyChanged(true);
        }
    }

    void ClearDirty() {
        if (m_dirty) {
            m_dirty = false;
            if (OnDirtyChanged) OnDirtyChanged(false);
        }
    }

protected:
    // =========================================================================
    // Virtual Methods for Subclasses
    // =========================================================================

    /**
     * @brief Render the main content of the panel
     * Override this to add your panel content
     */
    virtual void OnRender() = 0;

    /**
     * @brief Render the toolbar (if HasToolbar flag is set)
     */
    virtual void OnRenderToolbar() {}

    /**
     * @brief Render the menu bar (if HasMenuBar flag is set)
     */
    virtual void OnRenderMenuBar() {}

    /**
     * @brief Render the status bar (if HasStatusBar flag is set)
     */
    virtual void OnRenderStatusBar() {}

    /**
     * @brief Called when panel is shown
     */
    virtual void OnShow() {}

    /**
     * @brief Called when panel is hidden
     */
    virtual void OnHide() {}

    /**
     * @brief Called after initialization
     */
    virtual void OnInitialize() {}

    /**
     * @brief Called before shutdown
     */
    virtual void OnShutdown() {}

    // =========================================================================
    // Helper Methods
    // =========================================================================

    /**
     * @brief Get available content region size
     */
    glm::vec2 GetContentSize() const;

    /**
     * @brief Show a context menu
     */
    bool BeginContextMenu(const char* id = nullptr);
    void EndContextMenu();

    /**
     * @brief Show a popup modal
     */
    bool BeginPopupModal(const char* title, bool* open = nullptr);
    void EndPopupModal();

    /**
     * @brief Status message helpers
     */
    void SetStatus(const std::string& message) { m_statusMessage = message; }
    void ClearStatus() { m_statusMessage.clear(); }

    /**
     * @brief Record action for undo
     */
    void RecordUndoAction(const std::string& description, std::function<void()> undoFunc, std::function<void()> redoFunc);

    // Panel state
    Config m_config;
    std::string m_id;
    bool m_visible = true;
    bool m_focused = false;
    bool m_dirty = false;
    bool m_initialized = false;

    // Search
    std::string m_searchFilter;
    char m_searchBuffer[256] = "";

    // Status
    std::string m_statusMessage;

    // Undo/redo
    struct UndoAction {
        std::string description;
        std::function<void()> undo;
        std::function<void()> redo;
    };
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    static constexpr size_t MAX_UNDO = 50;

private:
    void RenderSearchBar();
    void RenderDefaultStatusBar();

    bool m_wasVisible = true;
    bool m_needsFocus = false;
};

// ============================================================================
// Panel Registry
// ============================================================================

/**
 * @brief Registry for managing all editor panels
 */
class PanelRegistry {
public:
    static PanelRegistry& Instance();

    /**
     * @brief Register a panel
     */
    void Register(const std::string& id, std::shared_ptr<EditorPanel> panel);

    /**
     * @brief Unregister a panel
     */
    void Unregister(const std::string& id);

    /**
     * @brief Get a panel by ID
     */
    std::shared_ptr<EditorPanel> Get(const std::string& id);

    /**
     * @brief Get all panels
     */
    std::vector<std::shared_ptr<EditorPanel>> GetAll();

    /**
     * @brief Get panels by category
     */
    std::vector<std::shared_ptr<EditorPanel>> GetByCategory(const std::string& category);

    /**
     * @brief Update all panels
     */
    void UpdateAll(float deltaTime);

    /**
     * @brief Render all visible panels
     */
    void RenderAll();

    /**
     * @brief Render View menu items for panels
     */
    void RenderViewMenu();

private:
    PanelRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<EditorPanel>> m_panels;
};

// ============================================================================
// Common Panel Types
// ============================================================================

/**
 * @brief Simple panel that accepts a render callback
 */
class CallbackPanel : public EditorPanel {
public:
    using RenderCallback = std::function<void()>;

    void SetRenderCallback(RenderCallback callback) { m_callback = std::move(callback); }

protected:
    void OnRender() override {
        if (m_callback) m_callback();
    }

private:
    RenderCallback m_callback;
};

/**
 * @brief Panel with list/detail layout
 */
class ListDetailPanel : public EditorPanel {
public:
    void SetListWidth(float width) { m_listWidth = width; }
    void SetShowDetail(bool show) { m_showDetail = show; }

protected:
    void OnRender() override;

    virtual void OnRenderList() = 0;
    virtual void OnRenderDetail() = 0;

    float m_listWidth = 200.0f;
    bool m_showDetail = true;
    int m_selectedIndex = -1;
};

/**
 * @brief Panel with tabs
 */
class TabbedPanel : public EditorPanel {
public:
    struct Tab {
        std::string name;
        std::string icon;
        std::function<void()> render;
        bool closeable = false;
    };

    void AddTab(const Tab& tab);
    void RemoveTab(const std::string& name);
    void SetActiveTab(const std::string& name);
    const std::string& GetActiveTab() const { return m_activeTab; }

protected:
    void OnRender() override;

    std::vector<Tab> m_tabs;
    std::string m_activeTab;
};

/**
 * @brief Panel with tree view
 */
class TreePanel : public EditorPanel {
public:
    struct TreeNode {
        std::string id;
        std::string label;
        std::string icon;
        std::vector<TreeNode> children;
        void* userData = nullptr;
        bool expanded = false;
        bool selected = false;
    };

    void SetRootNodes(std::vector<TreeNode> nodes) { m_rootNodes = std::move(nodes); }
    const TreeNode* GetSelectedNode() const;

    std::function<void(const TreeNode&)> OnNodeSelected;
    std::function<void(const TreeNode&)> OnNodeDoubleClicked;
    std::function<void(const TreeNode&)> OnNodeContextMenu;

protected:
    void OnRender() override;
    void RenderNode(TreeNode& node, int depth = 0);

    std::vector<TreeNode> m_rootNodes;
    std::string m_selectedNodeId;
};

} // namespace Nova
