#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace Engine {
namespace UI {

// Forward declarations
class HTMLRenderer;
class UIWindow;
class UIBinding;
class UIEventRouter;
class UIAnimation;

/**
 * @brief Rendering context for HTML UI
 */
struct RenderContext {
    int viewportWidth = 1920;
    int viewportHeight = 1080;
    float dpiScale = 1.0f;
    bool vsyncEnabled = true;
    int maxFPS = 60;
    bool hardwareAcceleration = true;
};

/**
 * @brief UI viewport mode
 */
enum class ViewportMode {
    Windowed,
    Fullscreen,
    BorderlessFullscreen
};

/**
 * @brief Z-ordering layer for UI elements
 */
enum class UILayer {
    Background = 0,
    Game = 100,
    HUD = 200,
    Windows = 300,
    Popups = 400,
    Modals = 500,
    Tooltips = 600,
    Debug = 700
};

/**
 * @brief Modal dialog result
 */
enum class ModalResult {
    None,
    OK,
    Cancel,
    Yes,
    No,
    Custom
};

/**
 * @brief Modal dialog configuration
 */
struct ModalConfig {
    std::string title;
    std::string message;
    std::string htmlContent;
    std::vector<std::string> buttons;
    bool closeOnOutsideClick = false;
    bool showCloseButton = true;
    int width = 400;
    int height = 200;
    std::function<void(ModalResult, const std::string&)> callback;
};

/**
 * @brief Main UI Manager for runtime HTML-based UI
 *
 * Manages all UI windows, panels, rendering, and input routing.
 * Provides the main interface for game code to interact with the UI system.
 */
class RuntimeUIManager {
public:
    /**
     * @brief Get singleton instance
     */
    static RuntimeUIManager& GetInstance();

    /**
     * @brief Initialize the UI system
     * @param context Rendering context configuration
     * @return true if initialization succeeded
     */
    bool Initialize(const RenderContext& context);

    /**
     * @brief Shutdown the UI system
     */
    void Shutdown();

    /**
     * @brief Update UI state (call every frame)
     * @param deltaTime Time since last frame in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Render all UI elements to the game viewport
     */
    void Render();

    // Window/Panel Management

    /**
     * @brief Create a new UI window
     * @param id Unique window identifier
     * @param htmlPath Path to HTML template
     * @param layer UI layer for z-ordering
     * @return Pointer to created window or nullptr on failure
     */
    UIWindow* CreateWindow(const std::string& id, const std::string& htmlPath, UILayer layer = UILayer::Windows);

    /**
     * @brief Get an existing window by ID
     * @param id Window identifier
     * @return Pointer to window or nullptr if not found
     */
    UIWindow* GetWindow(const std::string& id);

    /**
     * @brief Close and destroy a window
     * @param id Window identifier
     */
    void CloseWindow(const std::string& id);

    /**
     * @brief Show a window (make visible)
     * @param id Window identifier
     */
    void ShowWindow(const std::string& id);

    /**
     * @brief Hide a window (make invisible but keep in memory)
     * @param id Window identifier
     */
    void HideWindow(const std::string& id);

    /**
     * @brief Toggle window visibility
     * @param id Window identifier
     * @return New visibility state
     */
    bool ToggleWindow(const std::string& id);

    /**
     * @brief Check if window is visible
     * @param id Window identifier
     * @return true if visible
     */
    bool IsWindowVisible(const std::string& id) const;

    /**
     * @brief Bring window to front
     * @param id Window identifier
     */
    void BringToFront(const std::string& id);

    /**
     * @brief Get all windows in a layer
     * @param layer UI layer
     * @return Vector of window pointers
     */
    std::vector<UIWindow*> GetWindowsInLayer(UILayer layer) const;

    // Modal Dialog Support

    /**
     * @brief Show a modal dialog
     * @param config Modal configuration
     * @return Window ID of the modal
     */
    std::string ShowModal(const ModalConfig& config);

    /**
     * @brief Close a modal dialog
     * @param id Modal window ID
     * @param result Result to return
     * @param customData Optional custom data
     */
    void CloseModal(const std::string& id, ModalResult result, const std::string& customData = "");

    /**
     * @brief Check if any modal is currently open
     * @return true if modal is open
     */
    bool IsModalOpen() const;

    /**
     * @brief Get the topmost modal window
     * @return Pointer to modal window or nullptr
     */
    UIWindow* GetTopmostModal() const;

    // Viewport Management

    /**
     * @brief Set viewport mode
     * @param mode Viewport mode
     */
    void SetViewportMode(ViewportMode mode);

    /**
     * @brief Get current viewport mode
     * @return Current mode
     */
    ViewportMode GetViewportMode() const;

    /**
     * @brief Resize the UI viewport
     * @param width New width
     * @param height New height
     */
    void ResizeViewport(int width, int height);

    /**
     * @brief Set DPI scale
     * @param scale DPI scale factor
     */
    void SetDPIScale(float scale);

    /**
     * @brief Get current DPI scale
     * @return DPI scale factor
     */
    float GetDPIScale() const;

    // Input Routing

    /**
     * @brief Route mouse input to UI
     * @param x Mouse X position
     * @param y Mouse Y position
     * @param button Mouse button (0=left, 1=right, 2=middle)
     * @param pressed true if pressed, false if released
     * @return true if input was consumed by UI
     */
    bool RouteMouseInput(int x, int y, int button, bool pressed);

    /**
     * @brief Route mouse move to UI
     * @param x Mouse X position
     * @param y Mouse Y position
     * @return true if input was consumed by UI
     */
    bool RouteMouseMove(int x, int y);

    /**
     * @brief Route mouse scroll to UI
     * @param x Mouse X position
     * @param y Mouse Y position
     * @param scrollX Horizontal scroll amount
     * @param scrollY Vertical scroll amount
     * @return true if input was consumed by UI
     */
    bool RouteMouseScroll(int x, int y, float scrollX, float scrollY);

    /**
     * @brief Route keyboard input to UI
     * @param keyCode Key code
     * @param pressed true if pressed, false if released
     * @param modifiers Modifier keys (shift, ctrl, alt)
     * @return true if input was consumed by UI
     */
    bool RouteKeyboardInput(int keyCode, bool pressed, int modifiers);

    /**
     * @brief Route text input to UI
     * @param text Unicode text input
     * @return true if input was consumed by UI
     */
    bool RouteTextInput(const std::string& text);

    /**
     * @brief Route touch input to UI
     * @param touchId Touch point ID
     * @param x Touch X position
     * @param y Touch Y position
     * @param phase Touch phase (0=begin, 1=move, 2=end, 3=cancel)
     * @return true if input was consumed by UI
     */
    bool RouteTouchInput(int touchId, int x, int y, int phase);

    /**
     * @brief Route gamepad input to UI
     * @param button Gamepad button
     * @param pressed true if pressed
     * @return true if input was consumed by UI
     */
    bool RouteGamepadInput(int button, bool pressed);

    // Subsystem Access

    /**
     * @brief Get the HTML renderer
     * @return Reference to HTML renderer
     */
    HTMLRenderer& GetRenderer();

    /**
     * @brief Get the UI binding system
     * @return Reference to UI binding
     */
    UIBinding& GetBinding();

    /**
     * @brief Get the event router
     * @return Reference to event router
     */
    UIEventRouter& GetEventRouter();

    /**
     * @brief Get the animation system
     * @return Reference to animation system
     */
    UIAnimation& GetAnimation();

    // Utility Methods

    /**
     * @brief Load a UI theme
     * @param cssPath Path to theme CSS file
     */
    void LoadTheme(const std::string& cssPath);

    /**
     * @brief Execute JavaScript in a window context
     * @param windowId Window ID
     * @param script JavaScript code
     * @return Result as string
     */
    std::string ExecuteScript(const std::string& windowId, const std::string& script);

    /**
     * @brief Set global CSS variable
     * @param name Variable name (without --)
     * @param value Variable value
     */
    void SetCSSVariable(const std::string& name, const std::string& value);

    /**
     * @brief Enable/disable UI debug rendering
     * @param enabled true to enable debug rendering
     */
    void SetDebugRendering(bool enabled);

    /**
     * @brief Get render statistics
     * @param drawCalls Output draw call count
     * @param triangles Output triangle count
     * @param textureMemory Output texture memory in bytes
     */
    void GetRenderStats(int& drawCalls, int& triangles, size_t& textureMemory) const;

private:
    RuntimeUIManager();
    ~RuntimeUIManager();
    RuntimeUIManager(const RuntimeUIManager&) = delete;
    RuntimeUIManager& operator=(const RuntimeUIManager&) = delete;

    void UpdateWindowZOrder();
    void RenderLayer(UILayer layer);
    UIWindow* FindWindowAtPoint(int x, int y) const;
    void ProcessPendingActions();

    std::unique_ptr<HTMLRenderer> m_renderer;
    std::unique_ptr<UIBinding> m_binding;
    std::unique_ptr<UIEventRouter> m_eventRouter;
    std::unique_ptr<UIAnimation> m_animation;

    std::unordered_map<std::string, std::unique_ptr<UIWindow>> m_windows;
    std::vector<UIWindow*> m_zOrderedWindows;
    std::vector<std::string> m_modalStack;

    RenderContext m_context;
    ViewportMode m_viewportMode = ViewportMode::Windowed;
    bool m_initialized = false;
    bool m_debugRendering = false;

    int m_mouseX = 0;
    int m_mouseY = 0;
    UIWindow* m_focusedWindow = nullptr;
    UIWindow* m_draggedWindow = nullptr;

    mutable std::mutex m_mutex;

    // Pending actions for thread-safe operations
    struct PendingAction {
        enum Type { Create, Close, Show, Hide, BringToFront };
        Type type;
        std::string windowId;
        std::string data;
    };
    std::vector<PendingAction> m_pendingActions;
    std::mutex m_actionMutex;
};

} // namespace UI
} // namespace Engine
