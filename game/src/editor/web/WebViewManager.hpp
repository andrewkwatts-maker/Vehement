#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <filesystem>
#include <chrono>
#include <cstdint>

// Forward declare GLuint to avoid including OpenGL headers in header
using GLuint = unsigned int;

namespace Vehement {
namespace WebEditor {

/**
 * @brief Message from JavaScript to C++
 */
struct JSMessage {
    std::string viewId;
    std::string type;
    std::string payload;  // JSON string
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Configuration for a web view instance
 */
struct WebViewConfig {
    std::string id;
    std::string title = "Web Panel";
    int width = 800;
    int height = 600;
    bool transparent = false;
    bool debug = false;
    std::string initialUrl;
    std::string initialHtml;
    std::vector<std::string> watchPaths;  // For hot-reload
};

/**
 * @brief A single web view instance
 */
class WebView {
public:
    WebView(const WebViewConfig& config);
    ~WebView();

    // Non-copyable
    WebView(const WebView&) = delete;
    WebView& operator=(const WebView&) = delete;

    // Move-only
    WebView(WebView&& other) noexcept;
    WebView& operator=(WebView&& other) noexcept;

    // =========================================================================
    // Content Loading
    // =========================================================================

    /**
     * @brief Load HTML from a file
     * @param path Path to HTML file
     * @return true if successful
     */
    bool LoadFile(const std::string& path);

    /**
     * @brief Load HTML from a string
     * @param html HTML content
     * @param baseUrl Base URL for relative resources
     * @return true if successful
     */
    bool LoadHtml(const std::string& html, const std::string& baseUrl = "");

    /**
     * @brief Load a URL
     * @param url URL to load
     * @return true if successful
     */
    bool LoadUrl(const std::string& url);

    /**
     * @brief Reload current content
     */
    void Reload();

    /**
     * @brief Get currently loaded path/URL
     */
    [[nodiscard]] const std::string& GetCurrentSource() const { return m_currentSource; }

    // =========================================================================
    // JavaScript Execution
    // =========================================================================

    /**
     * @brief Execute JavaScript code
     * @param script JavaScript code to execute
     * @param callback Optional callback with result
     */
    void ExecuteJS(const std::string& script,
                   std::function<void(const std::string&)> callback = nullptr);

    /**
     * @brief Call a JavaScript function by name
     * @param functionName Name of the function
     * @param argsJson Arguments as JSON array string
     * @param callback Optional callback with result
     */
    void CallFunction(const std::string& functionName,
                      const std::string& argsJson = "[]",
                      std::function<void(const std::string&)> callback = nullptr);

    /**
     * @brief Send a message to JavaScript
     * @param type Message type
     * @param payload JSON payload
     */
    void SendMessage(const std::string& type, const std::string& payload);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Update the web view (process events, check for file changes)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render to texture for ImGui integration
     * @return Texture ID for ImGui::Image
     */
    [[nodiscard]] void* GetTextureId() const { return m_textureId; }

    /**
     * @brief Get current dimensions
     */
    [[nodiscard]] int GetWidth() const { return m_config.width; }
    [[nodiscard]] int GetHeight() const { return m_config.height; }

    /**
     * @brief Resize the web view
     */
    void Resize(int width, int height);

    // =========================================================================
    // Input Handling
    // =========================================================================

    /**
     * @brief Inject mouse event
     */
    void InjectMouseMove(int x, int y);
    void InjectMouseButton(int button, bool pressed, int x, int y);
    void InjectMouseWheel(float deltaX, float deltaY);

    /**
     * @brief Inject keyboard event
     */
    void InjectKeyEvent(int key, int scancode, int action, int mods);
    void InjectCharEvent(unsigned int codepoint);

    /**
     * @brief Set focus state
     */
    void SetFocused(bool focused);
    [[nodiscard]] bool IsFocused() const { return m_focused; }

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Enable hot-reload for files
     * @param paths Paths to watch for changes
     */
    void EnableHotReload(const std::vector<std::string>& paths);

    /**
     * @brief Disable hot-reload
     */
    void DisableHotReload();

    /**
     * @brief Check if hot-reload is enabled
     */
    [[nodiscard]] bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

    // =========================================================================
    // Configuration
    // =========================================================================

    [[nodiscard]] const std::string& GetId() const { return m_config.id; }
    [[nodiscard]] const std::string& GetTitle() const { return m_config.title; }
    [[nodiscard]] const WebViewConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get native webview handle (platform-specific)
     * @return Platform-specific handle or nullptr if using fallback
     */
    [[nodiscard]] void* GetNativeHandle() const { return m_nativeHandle; }

    // =========================================================================
    // Message Callbacks
    // =========================================================================

    using MessageHandler = std::function<void(const std::string& type, const std::string& payload)>;

    /**
     * @brief Set handler for messages from JavaScript
     */
    void SetMessageHandler(MessageHandler handler) { m_messageHandler = handler; }

private:
    void CheckHotReload();
    void CreateTexture();
    void DestroyTexture();

    WebViewConfig m_config;
    std::string m_currentSource;
    std::string m_loadedHtml;      // Loaded HTML content for fallback rendering
    std::string m_baseUrl;          // Base URL for relative resources

    // Native webview handle (platform-specific)
    void* m_nativeHandle = nullptr;
    void* m_textureId = nullptr;
    GLuint m_glTextureId = 0;       // OpenGL texture ID

    // State
    bool m_focused = false;
    bool m_hotReloadEnabled = false;
    float m_hotReloadCheckTimer = 0.0f;
    static constexpr float HOT_RELOAD_CHECK_INTERVAL = 0.5f;

    // Input state for fallback renderer
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    uint32_t m_mouseButtonState = 0;  // Bitmask of pressed buttons

    // File modification times for hot-reload
    std::unordered_map<std::string, std::filesystem::file_time_type> m_watchedFiles;

    // Message handling
    MessageHandler m_messageHandler;
    std::queue<JSMessage> m_pendingMessages;
    std::mutex m_messageMutex;
};

/**
 * @brief Manager for multiple embedded web views
 *
 * Provides HTML panels inside the editor with full C++ <-> JavaScript
 * bidirectional communication:
 * - Create HTML panels inside ImGui windows
 * - Load HTML from files or embedded strings
 * - Hot-reload HTML/CSS/JS files during development
 * - Execute JavaScript from C++
 * - Receive messages from JavaScript
 *
 * Backend options:
 * - WebView2 (Windows)
 * - WebKitGTK (Linux)
 * - WKWebView (macOS)
 * - CEF (cross-platform, heavier)
 * - Fallback: ImGui-based HTML form renderer
 */
class WebViewManager {
public:
    WebViewManager();
    ~WebViewManager();

    // Singleton access
    static WebViewManager& Instance();

    // Non-copyable, non-movable
    WebViewManager(const WebViewManager&) = delete;
    WebViewManager& operator=(const WebViewManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the web view system
     * @param assetsPath Base path for HTML assets
     * @return true if successful
     */
    bool Initialize(const std::string& assetsPath);

    /**
     * @brief Shutdown the web view system
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Check if native web view is available
     */
    [[nodiscard]] bool HasNativeWebView() const { return m_hasNativeWebView; }

    // =========================================================================
    // Web View Management
    // =========================================================================

    /**
     * @brief Create a new web view
     * @param config Web view configuration
     * @return Pointer to created web view, or nullptr on failure
     */
    WebView* CreateWebView(const WebViewConfig& config);

    /**
     * @brief Destroy a web view
     * @param id Web view ID
     */
    void DestroyWebView(const std::string& id);

    /**
     * @brief Get a web view by ID
     * @param id Web view ID
     * @return Pointer to web view, or nullptr if not found
     */
    [[nodiscard]] WebView* GetWebView(const std::string& id);

    /**
     * @brief Get all web view IDs
     */
    [[nodiscard]] std::vector<std::string> GetWebViewIds() const;

    /**
     * @brief Check if a web view exists
     */
    [[nodiscard]] bool HasWebView(const std::string& id) const;

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update all web views
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Global Settings
    // =========================================================================

    /**
     * @brief Set global debug mode (shows dev tools)
     */
    void SetDebugMode(bool enabled);

    /**
     * @brief Enable/disable hot-reload for all views
     */
    void SetGlobalHotReload(bool enabled);

    /**
     * @brief Get assets path
     */
    [[nodiscard]] const std::string& GetAssetsPath() const { return m_assetsPath; }

    /**
     * @brief Resolve a relative path to absolute using assets path
     */
    [[nodiscard]] std::string ResolvePath(const std::string& relativePath) const;

    // =========================================================================
    // Message Broadcasting
    // =========================================================================

    /**
     * @brief Broadcast a message to all web views
     * @param type Message type
     * @param payload JSON payload
     */
    void BroadcastMessage(const std::string& type, const std::string& payload);

    /**
     * @brief Set global message handler (receives all messages)
     */
    using GlobalMessageHandler = std::function<void(const std::string& viewId,
                                                     const std::string& type,
                                                     const std::string& payload)>;
    void SetGlobalMessageHandler(GlobalMessageHandler handler) { m_globalMessageHandler = handler; }

    // =========================================================================
    // ImGui Integration
    // =========================================================================

    /**
     * @brief Render a web view in an ImGui window
     * @param viewId Web view ID
     * @param label ImGui window label
     * @param open Pointer to window open state (optional)
     */
    void RenderImGuiWindow(const std::string& viewId, const char* label, bool* open = nullptr);

    /**
     * @brief Render a web view inline (no window)
     * @param viewId Web view ID
     * @param width Width (0 for auto)
     * @param height Height (0 for auto)
     */
    void RenderImGuiInline(const std::string& viewId, float width = 0, float height = 0);

private:
    void InitNativeBackend();
    void ShutdownNativeBackend();

    bool m_initialized = false;
    bool m_hasNativeWebView = false;
    bool m_debugMode = false;
    bool m_globalHotReload = true;

    std::string m_assetsPath;

    std::unordered_map<std::string, std::unique_ptr<WebView>> m_webViews;

    GlobalMessageHandler m_globalMessageHandler;

    // Platform-specific backend handle
    void* m_backendHandle = nullptr;
};

/**
 * @brief ImGui fallback renderer for when native webview is unavailable
 *
 * Parses simple HTML forms and renders them using ImGui widgets.
 * Supports a subset of HTML:
 * - div, span, p, h1-h6
 * - input (text, number, checkbox, radio, range)
 * - select/option
 * - button
 * - table/tr/td
 * - Basic CSS properties (color, background, margin, padding)
 */
class ImGuiFallbackRenderer {
public:
    ImGuiFallbackRenderer();
    ~ImGuiFallbackRenderer();

    /**
     * @brief Parse HTML content
     * @param html HTML string
     * @return true if parsed successfully
     */
    bool ParseHtml(const std::string& html);

    /**
     * @brief Parse CSS content
     * @param css CSS string
     */
    void ParseCss(const std::string& css);

    /**
     * @brief Render using ImGui
     */
    void Render();

    /**
     * @brief Set JavaScript call handler
     */
    using JSCallHandler = std::function<void(const std::string& function, const std::string& argsJson)>;
    void SetJSCallHandler(JSCallHandler handler) { m_jsCallHandler = handler; }

    /**
     * @brief Update form data from JavaScript
     * @param elementId Element ID
     * @param value New value as JSON
     */
    void SetElementValue(const std::string& elementId, const std::string& value);

    /**
     * @brief Get form data
     * @return JSON object with all form values
     */
    [[nodiscard]] std::string GetFormData() const;

private:
    struct DOMElement;
    struct CSSRule;

    void RenderElement(const DOMElement& element);
    void ApplyStyles(const DOMElement& element);

    std::vector<std::unique_ptr<DOMElement>> m_elements;
    std::vector<CSSRule> m_cssRules;
    std::unordered_map<std::string, std::string> m_formData;
    JSCallHandler m_jsCallHandler;
};

} // namespace WebEditor
} // namespace Vehement
