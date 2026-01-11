#pragma once

#include "HTMLRenderer.hpp"
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine {
namespace UI {

// Forward declarations
class RuntimeUIManager;
enum class UILayer;
enum class ModalResult;

/**
 * @brief Window state flags
 */
enum class WindowState {
    Normal,
    Minimized,
    Maximized,
    Closed
};

/**
 * @brief Tab data for tab containers
 */
struct TabData {
    std::string id;
    std::string title;
    std::string iconPath;
    std::string htmlPath;
    bool closable = true;
    bool active = false;
    std::unique_ptr<DOMElement> content;
};

/**
 * @brief Docking position
 */
enum class DockPosition {
    None,
    Left,
    Right,
    Top,
    Bottom,
    Center,
    Float
};

/**
 * @brief Window layout data for save/restore
 */
struct WindowLayout {
    std::string id;
    int x = 0, y = 0;
    int width = 400, height = 300;
    WindowState state = WindowState::Normal;
    DockPosition dockPosition = DockPosition::None;
    bool visible = true;
    std::vector<std::string> tabOrder;
    std::string activeTab;
};

/**
 * @brief UI Window/Panel management
 *
 * Supports draggable title bars, resizing, minimize/maximize/close,
 * tab containers, docking, and layout save/restore.
 */
class UIWindow {
public:
    /**
     * @brief Construct a new UIWindow
     * @param id Unique window identifier
     * @param manager Parent UI manager
     */
    UIWindow(const std::string& id, RuntimeUIManager* manager);
    ~UIWindow();

    // Content Loading

    /**
     * @brief Load HTML content from file
     * @param path Path to HTML file
     * @return true if loaded successfully
     */
    bool LoadHTML(const std::string& path);

    /**
     * @brief Load HTML content from string
     * @param html HTML string
     * @return true if loaded successfully
     */
    bool LoadHTMLString(const std::string& html);

    /**
     * @brief Reload the current HTML content
     */
    void Reload();

    // Window Management

    /**
     * @brief Get window ID
     */
    const std::string& GetID() const { return m_id; }

    /**
     * @brief Show the window
     */
    void Show();

    /**
     * @brief Hide the window
     */
    void Hide();

    /**
     * @brief Check if window is visible
     */
    bool IsVisible() const { return m_visible; }

    /**
     * @brief Close the window
     */
    void Close();

    /**
     * @brief Minimize the window
     */
    void Minimize();

    /**
     * @brief Maximize the window
     */
    void Maximize();

    /**
     * @brief Restore window from minimized/maximized
     */
    void Restore();

    /**
     * @brief Get current window state
     */
    WindowState GetState() const { return m_state; }

    // Position and Size

    /**
     * @brief Move window to position
     */
    void Move(int x, int y);

    /**
     * @brief Resize window
     */
    void Resize(int width, int height);

    /**
     * @brief Set bounds (position and size)
     */
    void SetBounds(int x, int y, int width, int height);

    /**
     * @brief Center window on screen
     */
    void Center();

    int GetX() const { return m_x; }
    int GetY() const { return m_y; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    /**
     * @brief Set minimum size
     */
    void SetMinSize(int minWidth, int minHeight);

    /**
     * @brief Set maximum size
     */
    void SetMaxSize(int maxWidth, int maxHeight);

    // Appearance

    /**
     * @brief Set window title
     */
    void SetTitle(const std::string& title);

    /**
     * @brief Get window title
     */
    const std::string& GetTitle() const { return m_title; }

    /**
     * @brief Set title bar visibility
     */
    void SetTitleBarVisible(bool visible);

    /**
     * @brief Check if title bar is visible
     */
    bool IsTitleBarVisible() const { return m_showTitleBar; }

    /**
     * @brief Set window resizable
     */
    void SetResizable(bool resizable);

    /**
     * @brief Check if window is resizable
     */
    bool IsResizable() const { return m_resizable; }

    /**
     * @brief Set window draggable
     */
    void SetDraggable(bool draggable);

    /**
     * @brief Check if window is draggable
     */
    bool IsDraggable() const { return m_draggable; }

    /**
     * @brief Set background color
     */
    void SetBackgroundColor(const Color& color);

    /**
     * @brief Get background color
     */
    const Color& GetBackgroundColor() const { return m_backgroundColor; }

    /**
     * @brief Set window opacity
     */
    void SetOpacity(float opacity);

    /**
     * @brief Get window opacity
     */
    float GetOpacity() const { return m_opacity; }

    // Z-Order and Layer

    /**
     * @brief Set UI layer
     */
    void SetLayer(UILayer layer);

    /**
     * @brief Get UI layer
     */
    UILayer GetLayer() const { return m_layer; }

    /**
     * @brief Set z-index within layer
     */
    void SetZIndex(int zIndex);

    /**
     * @brief Get z-index
     */
    int GetZIndex() const { return m_zIndex; }

    // Modal Support

    /**
     * @brief Set as modal window
     */
    void SetModal(bool modal);

    /**
     * @brief Check if modal
     */
    bool IsModal() const { return m_modal; }

    /**
     * @brief Set modal callback
     */
    void SetCallback(std::function<void(ModalResult, const std::string&)> callback);

    /**
     * @brief Get modal callback
     */
    std::function<void(ModalResult, const std::string&)> GetCallback() const { return m_modalCallback; }

    // Tab Container Support

    /**
     * @brief Add a tab to the window
     * @param tabData Tab configuration
     * @return Tab ID
     */
    std::string AddTab(const TabData& tabData);

    /**
     * @brief Remove a tab
     * @param tabId Tab ID
     */
    void RemoveTab(const std::string& tabId);

    /**
     * @brief Set active tab
     * @param tabId Tab ID
     */
    void SetActiveTab(const std::string& tabId);

    /**
     * @brief Get active tab ID
     */
    std::string GetActiveTab() const;

    /**
     * @brief Get all tabs
     */
    const std::vector<TabData>& GetTabs() const { return m_tabs; }

    /**
     * @brief Reorder tabs
     */
    void ReorderTabs(const std::vector<std::string>& tabOrder);

    // Docking Support

    /**
     * @brief Set dock position
     */
    void SetDockPosition(DockPosition position);

    /**
     * @brief Get dock position
     */
    DockPosition GetDockPosition() const { return m_dockPosition; }

    /**
     * @brief Undock window
     */
    void Undock();

    /**
     * @brief Dock to another window
     * @param targetWindowId Target window ID
     * @param position Dock position
     */
    void DockTo(const std::string& targetWindowId, DockPosition position);

    // Layout Save/Restore

    /**
     * @brief Get layout data
     */
    WindowLayout GetLayout() const;

    /**
     * @brief Apply layout data
     */
    void ApplyLayout(const WindowLayout& layout);

    // DOM Access

    /**
     * @brief Get root DOM element
     */
    DOMElement* GetRootElement() { return m_rootElement.get(); }

    /**
     * @brief Find element by ID
     */
    DOMElement* GetElementById(const std::string& id);

    /**
     * @brief Find elements by class
     */
    std::vector<DOMElement*> GetElementsByClass(const std::string& className);

    /**
     * @brief Query selector
     */
    DOMElement* QuerySelector(const std::string& selector);

    /**
     * @brief Query selector all
     */
    std::vector<DOMElement*> QuerySelectorAll(const std::string& selector);

    // JavaScript Execution

    /**
     * @brief Execute JavaScript in window context
     */
    std::string ExecuteScript(const std::string& script);

    /**
     * @brief Call JavaScript function
     */
    std::string CallFunction(const std::string& functionName, const std::vector<std::string>& args);

    // Event Handling

    /**
     * @brief Hit test
     */
    bool HitTest(int x, int y) const;

    /**
     * @brief Title bar hit test
     */
    bool IsTitleBarHit(int x, int y) const;

    /**
     * @brief Resize handle hit test
     */
    int GetResizeHandle(int x, int y) const;

    /**
     * @brief Handle focus gained
     */
    void OnFocusGained();

    /**
     * @brief Handle focus lost
     */
    void OnFocusLost();

    /**
     * @brief Handle viewport resize
     */
    void OnViewportResize(int viewportWidth, int viewportHeight);

    // Update

    /**
     * @brief Update window
     */
    void Update(float deltaTime);

    // Button visibility

    void SetShowCloseButton(bool show) { m_showCloseButton = show; }
    void SetShowMinimizeButton(bool show) { m_showMinimizeButton = show; }
    void SetShowMaximizeButton(bool show) { m_showMaximizeButton = show; }

private:
    void UpdateTitleBar();
    void LayoutContent();
    void ApplyStyles();

    std::string m_id;
    RuntimeUIManager* m_manager;

    // Content
    std::unique_ptr<DOMElement> m_rootElement;
    std::string m_htmlPath;
    std::string m_htmlContent;
    std::vector<CSSRule> m_styles;

    // State
    bool m_visible = true;
    WindowState m_state = WindowState::Normal;
    bool m_focused = false;

    // Position and size
    int m_x = 100, m_y = 100;
    int m_width = 400, m_height = 300;
    int m_minWidth = 100, m_minHeight = 50;
    int m_maxWidth = 0, m_maxHeight = 0; // 0 = no limit
    int m_savedX = 0, m_savedY = 0;
    int m_savedWidth = 0, m_savedHeight = 0;

    // Appearance
    std::string m_title = "Window";
    bool m_showTitleBar = true;
    bool m_showCloseButton = true;
    bool m_showMinimizeButton = true;
    bool m_showMaximizeButton = true;
    bool m_resizable = true;
    bool m_draggable = true;
    Color m_backgroundColor{40, 40, 40, 240};
    float m_opacity = 1.0f;

    // Z-order
    UILayer m_layer;
    int m_zIndex = 0;

    // Modal
    bool m_modal = false;
    std::function<void(ModalResult, const std::string&)> m_modalCallback;

    // Tabs
    std::vector<TabData> m_tabs;
    std::string m_activeTabId;

    // Docking
    DockPosition m_dockPosition = DockPosition::None;
    std::string m_dockedToWindow;

    // Title bar dimensions
    static constexpr int TITLE_BAR_HEIGHT = 30;
    static constexpr int RESIZE_BORDER = 5;
    static constexpr int BUTTON_SIZE = 24;
};

} // namespace UI
} // namespace Engine
