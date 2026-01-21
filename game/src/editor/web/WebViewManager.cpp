#include "WebViewManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

// ImGui for fallback rendering
#include <imgui.h>

// OpenGL for texture management
#include <glad/gl.h>

// Platform detection
#if defined(_WIN32)
    #define WEBVIEW_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define WEBVIEW_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define WEBVIEW_PLATFORM_LINUX 1
#endif

namespace Vehement {
namespace WebEditor {

// ============================================================================
// WebView Implementation
// ============================================================================

WebView::WebView(const WebViewConfig& config)
    : m_config(config)
{
    CreateTexture();

    // Initialize hot-reload if paths provided
    if (!config.watchPaths.empty()) {
        EnableHotReload(config.watchPaths);
    }

    // Load initial content
    if (!config.initialUrl.empty()) {
        LoadUrl(config.initialUrl);
    } else if (!config.initialHtml.empty()) {
        LoadHtml(config.initialHtml);
    }
}

WebView::~WebView() {
    DestroyTexture();
}

WebView::WebView(WebView&& other) noexcept
    : m_config(std::move(other.m_config))
    , m_currentSource(std::move(other.m_currentSource))
    , m_loadedHtml(std::move(other.m_loadedHtml))
    , m_baseUrl(std::move(other.m_baseUrl))
    , m_nativeHandle(other.m_nativeHandle)
    , m_textureId(other.m_textureId)
    , m_glTextureId(other.m_glTextureId)
    , m_focused(other.m_focused)
    , m_hotReloadEnabled(other.m_hotReloadEnabled)
    , m_lastMouseX(other.m_lastMouseX)
    , m_lastMouseY(other.m_lastMouseY)
    , m_mouseButtonState(other.m_mouseButtonState)
    , m_watchedFiles(std::move(other.m_watchedFiles))
    , m_messageHandler(std::move(other.m_messageHandler))
{
    other.m_nativeHandle = nullptr;
    other.m_textureId = nullptr;
    other.m_glTextureId = 0;
}

WebView& WebView::operator=(WebView&& other) noexcept {
    if (this != &other) {
        DestroyTexture();

        m_config = std::move(other.m_config);
        m_currentSource = std::move(other.m_currentSource);
        m_loadedHtml = std::move(other.m_loadedHtml);
        m_baseUrl = std::move(other.m_baseUrl);
        m_nativeHandle = other.m_nativeHandle;
        m_textureId = other.m_textureId;
        m_glTextureId = other.m_glTextureId;
        m_focused = other.m_focused;
        m_hotReloadEnabled = other.m_hotReloadEnabled;
        m_lastMouseX = other.m_lastMouseX;
        m_lastMouseY = other.m_lastMouseY;
        m_mouseButtonState = other.m_mouseButtonState;
        m_watchedFiles = std::move(other.m_watchedFiles);
        m_messageHandler = std::move(other.m_messageHandler);

        other.m_nativeHandle = nullptr;
        other.m_textureId = nullptr;
        other.m_glTextureId = 0;
    }
    return *this;
}

bool WebView::LoadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    m_currentSource = path;

    // Extract base directory for relative resources
    std::filesystem::path fsPath(path);
    std::string baseUrl = "file://" + fsPath.parent_path().string() + "/";

    return LoadHtml(buffer.str(), baseUrl);
}

bool WebView::LoadHtml(const std::string& html, const std::string& baseUrl) {
    // Inject the bridge script before </head> or at the start
    std::string injectedHtml = html;

    const std::string bridgeScript = R"(
<script>
// WebEditor Bridge
window.WebEditor = {
    _callbacks: {},
    _callbackId: 0,

    // Send message to C++
    postMessage: function(type, payload) {
        if (window.external && window.external.invoke) {
            window.external.invoke(JSON.stringify({type: type, payload: payload}));
        } else if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.webeditor) {
            window.webkit.messageHandlers.webeditor.postMessage({type: type, payload: payload});
        }
    },

    // Call C++ function and get result
    invoke: function(functionName, args, callback) {
        var id = ++this._callbackId;
        if (callback) {
            this._callbacks[id] = callback;
        }
        this.postMessage('invoke', {
            id: id,
            function: functionName,
            args: args || []
        });
    },

    // Called by C++ to deliver results
    _handleResult: function(id, result, error) {
        var callback = this._callbacks[id];
        if (callback) {
            delete this._callbacks[id];
            callback(error, result);
        }
    },

    // Called by C++ to deliver messages
    _handleMessage: function(type, payload) {
        if (this.onMessage) {
            this.onMessage(type, payload);
        }
        var event = new CustomEvent('webeditor-message', {
            detail: {type: type, payload: payload}
        });
        window.dispatchEvent(event);
    },

    // Subscribe to messages
    onMessage: null
};
</script>
)";

    size_t headPos = injectedHtml.find("</head>");
    if (headPos != std::string::npos) {
        injectedHtml.insert(headPos, bridgeScript);
    } else {
        size_t htmlPos = injectedHtml.find("<html");
        if (htmlPos != std::string::npos) {
            size_t insertPos = injectedHtml.find(">", htmlPos);
            if (insertPos != std::string::npos) {
                injectedHtml.insert(insertPos + 1, "<head>" + bridgeScript + "</head>");
            }
        } else {
            injectedHtml = bridgeScript + injectedHtml;
        }
    }

    // Load into native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: NavigateToString expects UTF-16 on Windows
        // For now, store for when WebView2 is fully integrated
        // ICoreWebView2* webview = static_cast<ICoreWebView2*>(m_nativeHandle);
        // webview->NavigateToString(injectedHtml.c_str());
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: loadHTMLString:baseURL:
        // Objective-C bridge would be needed here
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: webkit_web_view_load_html
        // webkit_web_view_load_html(WEBKIT_WEB_VIEW(m_nativeHandle), injectedHtml.c_str(), baseUrl.c_str());
#endif
    }

    // Store the HTML for fallback rendering and future reference
    m_loadedHtml = injectedHtml;
    m_baseUrl = baseUrl;

    return true;
}

bool WebView::LoadUrl(const std::string& url) {
    m_currentSource = url;

    // Load URL in native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: Navigate to URL
        // ICoreWebView2* webview = static_cast<ICoreWebView2*>(m_nativeHandle);
        // std::wstring wurl(url.begin(), url.end());
        // webview->Navigate(wurl.c_str());
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: loadRequest with NSURL
        // Objective-C bridge would call [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@url]]]
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: webkit_web_view_load_uri
        // webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_nativeHandle), url.c_str());
#endif
    }

    // Clear loaded HTML since we're loading a URL
    m_loadedHtml.clear();
    m_baseUrl = url;

    return true;
}

void WebView::Reload() {
    if (!m_currentSource.empty()) {
        if (m_currentSource.find("://") != std::string::npos) {
            LoadUrl(m_currentSource);
        } else {
            LoadFile(m_currentSource);
        }
    }
}

void WebView::ExecuteJS(const std::string& script,
                        std::function<void(const std::string&)> callback) {
    // Execute in native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: ExecuteScript with completion handler
        // ICoreWebView2* webview = static_cast<ICoreWebView2*>(m_nativeHandle);
        // std::wstring wscript(script.begin(), script.end());
        // webview->ExecuteScript(wscript.c_str(),
        //     Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        //         [callback](HRESULT hr, LPCWSTR result) -> HRESULT {
        //             if (callback) {
        //                 std::wstring wresult(result);
        //                 std::string sresult(wresult.begin(), wresult.end());
        //                 callback(sresult);
        //             }
        //             return S_OK;
        //         }).Get());
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: evaluateJavaScript:completionHandler:
        // [webView evaluateJavaScript:@script completionHandler:^(id result, NSError *error) { ... }]
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: webkit_web_view_run_javascript
        // webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_nativeHandle), script.c_str(),
        //     nullptr, js_finished_callback, callback_data);
#endif
        return;
    }

    // Fallback: queue the script for logging/debugging and return placeholder result
    // In a real implementation, this would be handled by the ImGui fallback renderer
    if (callback) {
        callback("null");  // Placeholder - native webview not available
    }
}

void WebView::CallFunction(const std::string& functionName,
                           const std::string& argsJson,
                           std::function<void(const std::string&)> callback) {
    std::string script = functionName + ".apply(null, " + argsJson + ")";
    ExecuteJS(script, callback);
}

void WebView::SendMessage(const std::string& type, const std::string& payload) {
    std::string script = "WebEditor._handleMessage('" + type + "', " + payload + ");";
    ExecuteJS(script);
}

void WebView::Update(float deltaTime) {
    // Check for hot-reload
    if (m_hotReloadEnabled) {
        m_hotReloadCheckTimer += deltaTime;
        if (m_hotReloadCheckTimer >= HOT_RELOAD_CHECK_INTERVAL) {
            m_hotReloadCheckTimer = 0.0f;
            CheckHotReload();
        }
    }

    // Process pending messages
    std::lock_guard<std::mutex> lock(m_messageMutex);
    while (!m_pendingMessages.empty()) {
        const auto& msg = m_pendingMessages.front();
        if (m_messageHandler) {
            m_messageHandler(msg.type, msg.payload);
        }
        m_pendingMessages.pop();
    }
}

void WebView::Resize(int width, int height) {
    if (width != m_config.width || height != m_config.height) {
        m_config.width = width;
        m_config.height = height;

        // Recreate texture with new size
        DestroyTexture();
        CreateTexture();

        // Resize native webview if available
        if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
            // WebView2: put_Bounds on the controller
            // ICoreWebView2Controller* controller = static_cast<ICoreWebView2Controller*>(m_controllerHandle);
            // RECT bounds = { 0, 0, width, height };
            // controller->put_Bounds(bounds);
#elif WEBVIEW_PLATFORM_MACOS
            // WKWebView: setFrame on the view
            // [webView setFrame:NSMakeRect(0, 0, width, height)]
#elif WEBVIEW_PLATFORM_LINUX
            // WebKitGTK: gtk_widget_set_size_request
            // gtk_widget_set_size_request(GTK_WIDGET(m_nativeHandle), width, height);
#endif
        }
    }
}

void WebView::InjectMouseMove(int x, int y) {
    // Forward mouse move to native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: SendMouseInput on the composition controller
        // ICoreWebView2CompositionController* compController = ...;
        // POINT point = { x, y };
        // compController->SendMouseInput(COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE,
        //     COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE, 0, point);
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: Create and inject NSEvent
        // NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved ...];
        // [webView mouseEvent:event];
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: Create GdkEvent and propagate
        // GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
        // gtk_widget_event(GTK_WIDGET(m_nativeHandle), event);
#endif
    }

    // Store last mouse position for fallback renderer
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void WebView::InjectMouseButton(int button, bool pressed, int x, int y) {
    // Update mouse button state for fallback renderer
    if (pressed) {
        m_mouseButtonState |= (1u << button);
    } else {
        m_mouseButtonState &= ~(1u << button);
    }
    m_lastMouseX = x;
    m_lastMouseY = y;

    // Forward mouse button to native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: SendMouseInput on the composition controller
        // COREWEBVIEW2_MOUSE_EVENT_KIND kind = pressed ?
        //     (button == 0 ? COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN :
        //      button == 1 ? COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN :
        //                    COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN) :
        //     (button == 0 ? COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP :
        //      button == 1 ? COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP :
        //                    COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP);
        // POINT point = { x, y };
        // compController->SendMouseInput(kind, virtualKeys, 0, point);
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: Create and inject NSEvent for mouse button
        // NSEventType type = pressed ? NSEventTypeLeftMouseDown : NSEventTypeLeftMouseUp;
        // NSEvent* event = [NSEvent mouseEventWithType:type ...];
        // [webView mouseEvent:event];
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: Create GdkEvent for button press/release
        // GdkEventType type = pressed ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;
        // GdkEvent* event = gdk_event_new(type);
        // gtk_widget_event(GTK_WIDGET(m_nativeHandle), event);
#endif
    }
}

void WebView::InjectMouseWheel(float deltaX, float deltaY) {
    // Forward mouse wheel to native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: SendMouseInput with wheel delta
        // POINT point = { m_lastMouseX, m_lastMouseY };
        // int wheelDelta = static_cast<int>(deltaY * WHEEL_DELTA);
        // compController->SendMouseInput(COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL,
        //     COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE, wheelDelta, point);
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: Create and inject NSEvent for scroll wheel
        // NSEvent* event = [NSEvent scrollWheelEventWithType:NSEventTypeScrollWheel
        //     deltaX:deltaX deltaY:deltaY ...];
        // [webView scrollWheel:event];
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: Create GdkEvent for scroll
        // GdkEvent* event = gdk_event_new(GDK_SCROLL);
        // GdkEventScroll* scroll = (GdkEventScroll*)event;
        // scroll->delta_x = deltaX;
        // scroll->delta_y = deltaY;
        // gtk_widget_event(GTK_WIDGET(m_nativeHandle), event);
#endif
    }
    // Note: Fallback renderer scrolling could be handled here
    // by tracking scroll offset and applying to rendered content
}

void WebView::InjectKeyEvent(int key, int scancode, int action, int mods) {
    // Forward key event to native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: SendKeyInput or via Windows message
        // ICoreWebView2Controller* controller = ...;
        // UINT msg = (action == 1) ? WM_KEYDOWN : (action == 0) ? WM_KEYUP : WM_KEYDOWN;
        // WPARAM wParam = MapVirtualKey(scancode, MAPVK_VSC_TO_VK);
        // LPARAM lParam = (scancode << 16) | 1;
        // PostMessage(hwnd, msg, wParam, lParam);
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: Create and inject NSEvent for key
        // NSEventType type = (action == 1) ? NSEventTypeKeyDown : NSEventTypeKeyUp;
        // NSEvent* event = [NSEvent keyEventWithType:type ...];
        // [webView keyDown:event] or [webView keyUp:event];
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: Create GdkEvent for key press/release
        // GdkEventType type = (action == 1) ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
        // GdkEvent* event = gdk_event_new(type);
        // GdkEventKey* keyEvent = (GdkEventKey*)event;
        // keyEvent->keyval = gdk_keyval_from_name(...);
        // gtk_widget_event(GTK_WIDGET(m_nativeHandle), event);
#endif
    }
    // Note: Key events could be captured for fallback form input handling
}

void WebView::InjectCharEvent(unsigned int codepoint) {
    // Forward character input to native webview if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: Send WM_CHAR message
        // PostMessage(hwnd, WM_CHAR, static_cast<WPARAM>(codepoint), 0);
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: Create key event with the character
        // NSString* chars = [[NSString alloc] initWithBytes:&codepoint
        //     length:sizeof(codepoint) encoding:NSUTF32LittleEndianStringEncoding];
        // NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
        //     characters:chars charactersIgnoringModifiers:chars ...];
        // [webView interpretKeyEvents:@[event]];
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: Create GdkEvent for key with unicode character
        // GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);
        // GdkEventKey* keyEvent = (GdkEventKey*)event;
        // keyEvent->keyval = gdk_unicode_to_keyval(codepoint);
        // gtk_widget_event(GTK_WIDGET(m_nativeHandle), event);
#endif
    }
    // Note: Character events could be used for text input in fallback forms
}

void WebView::SetFocused(bool focused) {
    if (m_focused == focused) {
        return;  // No change
    }

    m_focused = focused;

    // Notify native webview of focus change if available
    if (m_nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
        // WebView2: MoveFocus on the controller
        // ICoreWebView2Controller* controller = static_cast<ICoreWebView2Controller*>(m_controllerHandle);
        // if (focused) {
        //     controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        // }
        // Note: WebView2 doesn't have explicit "unfocus" - focus is moved away
#elif WEBVIEW_PLATFORM_MACOS
        // WKWebView: Make first responder or resign
        // if (focused) {
        //     [[webView window] makeFirstResponder:webView];
        // } else {
        //     [[webView window] makeFirstResponder:nil];
        // }
#elif WEBVIEW_PLATFORM_LINUX
        // WebKitGTK: gtk_widget_grab_focus or clear focus
        // if (focused) {
        //     gtk_widget_grab_focus(GTK_WIDGET(m_nativeHandle));
        // } else {
        //     GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(m_nativeHandle));
        //     if (parent) gtk_widget_grab_focus(parent);
        // }
#endif
    }
}

void WebView::EnableHotReload(const std::vector<std::string>& paths) {
    m_hotReloadEnabled = true;
    m_watchedFiles.clear();

    for (const auto& path : paths) {
        try {
            if (std::filesystem::exists(path)) {
                m_watchedFiles[path] = std::filesystem::last_write_time(path);
            }
        } catch (...) {
            // Ignore errors
        }
    }

    // Also watch the current source if it's a file
    if (!m_currentSource.empty() && m_currentSource.find("://") == std::string::npos) {
        try {
            if (std::filesystem::exists(m_currentSource)) {
                m_watchedFiles[m_currentSource] = std::filesystem::last_write_time(m_currentSource);
            }
        } catch (...) {
            // Ignore errors
        }
    }
}

void WebView::DisableHotReload() {
    m_hotReloadEnabled = false;
    m_watchedFiles.clear();
}

void WebView::CheckHotReload() {
    bool needsReload = false;

    for (auto& [path, lastTime] : m_watchedFiles) {
        try {
            if (std::filesystem::exists(path)) {
                auto currentTime = std::filesystem::last_write_time(path);
                if (currentTime > lastTime) {
                    lastTime = currentTime;
                    needsReload = true;
                }
            }
        } catch (...) {
            // Ignore errors
        }
    }

    if (needsReload) {
        Reload();
    }
}

void WebView::CreateTexture() {
    // Ensure we don't leak textures
    if (m_glTextureId != 0) {
        DestroyTexture();
    }

    // Create OpenGL texture for rendering webview content
    glGenTextures(1, &m_glTextureId);
    if (m_glTextureId == 0) {
        m_textureId = nullptr;
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_glTextureId);

    // Set texture parameters for crisp rendering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocate texture storage (RGBA format for webview content)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 m_config.width, m_config.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Store as void* for ImGui compatibility
    m_textureId = reinterpret_cast<void*>(static_cast<uintptr_t>(m_glTextureId));
}

void WebView::DestroyTexture() {
    // Delete OpenGL texture if it exists
    if (m_glTextureId != 0) {
        glDeleteTextures(1, &m_glTextureId);
        m_glTextureId = 0;
    }
    m_textureId = nullptr;
}

// ============================================================================
// WebViewManager Implementation
// ============================================================================

WebViewManager::WebViewManager() = default;

WebViewManager::~WebViewManager() {
    Shutdown();
}

WebViewManager& WebViewManager::Instance() {
    static WebViewManager instance;
    return instance;
}

bool WebViewManager::Initialize(const std::string& assetsPath) {
    if (m_initialized) {
        return true;
    }

    m_assetsPath = assetsPath;

    // Initialize native webview backend
    InitNativeBackend();

    m_initialized = true;
    return true;
}

void WebViewManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_webViews.clear();
    ShutdownNativeBackend();

    m_initialized = false;
}

void WebViewManager::InitNativeBackend() {
#if WEBVIEW_PLATFORM_WINDOWS
    // Try to initialize WebView2
    // CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // Try creating ICoreWebView2Environment
    m_hasNativeWebView = false;  // Set to true if WebView2 available
#elif WEBVIEW_PLATFORM_LINUX
    // Try to initialize WebKitGTK
    // Check if libwebkit2gtk is available
    m_hasNativeWebView = false;  // Set to true if WebKitGTK available
#elif WEBVIEW_PLATFORM_MACOS
    // WKWebView is always available on macOS
    m_hasNativeWebView = true;
#else
    m_hasNativeWebView = false;
#endif

    // Fall back to ImGui renderer if no native webview
    if (!m_hasNativeWebView) {
        // ImGui fallback will be used automatically
    }
}

void WebViewManager::ShutdownNativeBackend() {
#if WEBVIEW_PLATFORM_WINDOWS
    // CoUninitialize();
#endif
    m_backendHandle = nullptr;
}

WebView* WebViewManager::CreateWebView(const WebViewConfig& config) {
    if (!m_initialized) {
        return nullptr;
    }

    // Check for duplicate ID
    if (m_webViews.find(config.id) != m_webViews.end()) {
        return nullptr;
    }

    auto webView = std::make_unique<WebView>(config);

    // Set up message handling to route to global handler
    webView->SetMessageHandler([this, id = config.id](const std::string& type,
                                                       const std::string& payload) {
        if (m_globalMessageHandler) {
            m_globalMessageHandler(id, type, payload);
        }
    });

    WebView* ptr = webView.get();
    m_webViews[config.id] = std::move(webView);

    return ptr;
}

void WebViewManager::DestroyWebView(const std::string& id) {
    m_webViews.erase(id);
}

WebView* WebViewManager::GetWebView(const std::string& id) {
    auto it = m_webViews.find(id);
    return it != m_webViews.end() ? it->second.get() : nullptr;
}

std::vector<std::string> WebViewManager::GetWebViewIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_webViews.size());
    for (const auto& [id, view] : m_webViews) {
        ids.push_back(id);
    }
    return ids;
}

bool WebViewManager::HasWebView(const std::string& id) const {
    return m_webViews.find(id) != m_webViews.end();
}

void WebViewManager::Update(float deltaTime) {
    for (auto& [id, webView] : m_webViews) {
        webView->Update(deltaTime);
    }
}

void WebViewManager::SetDebugMode(bool enabled) {
    m_debugMode = enabled;

    // Enable/disable developer tools in all webviews
    for (auto& [id, webView] : m_webViews) {
        void* nativeHandle = webView->GetNativeHandle();
        if (nativeHandle) {
#if WEBVIEW_PLATFORM_WINDOWS
            // WebView2: Enable/disable DevTools
            // ICoreWebView2Settings* settings = nullptr;
            // ICoreWebView2* webview = static_cast<ICoreWebView2*>(nativeHandle);
            // webview->get_Settings(&settings);
            // settings->put_AreDevToolsEnabled(enabled ? TRUE : FALSE);
            // if (enabled) {
            //     webview->OpenDevToolsWindow();
            // }
#elif WEBVIEW_PLATFORM_MACOS
            // WKWebView: Enable Web Inspector via WKPreferences
            // WKPreferences* prefs = [[webView configuration] preferences];
            // [prefs setValue:@(enabled) forKey:@"developerExtrasEnabled"];
            // Note: User opens with right-click -> Inspect Element
#elif WEBVIEW_PLATFORM_LINUX
            // WebKitGTK: Enable Web Inspector
            // WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(nativeHandle));
            // webkit_settings_set_enable_developer_extras(settings, enabled);
            // if (enabled) {
            //     WebKitWebInspector* inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(nativeHandle));
            //     webkit_web_inspector_show(inspector);
            // }
#endif
        }
    }
}

void WebViewManager::SetGlobalHotReload(bool enabled) {
    m_globalHotReload = enabled;
    for (auto& [id, webView] : m_webViews) {
        if (enabled) {
            webView->EnableHotReload({webView->GetCurrentSource()});
        } else {
            webView->DisableHotReload();
        }
    }
}

std::string WebViewManager::ResolvePath(const std::string& relativePath) const {
    std::filesystem::path base(m_assetsPath);
    std::filesystem::path relative(relativePath);
    return (base / relative).string();
}

void WebViewManager::BroadcastMessage(const std::string& type, const std::string& payload) {
    for (auto& [id, webView] : m_webViews) {
        webView->SendMessage(type, payload);
    }
}

void WebViewManager::RenderImGuiWindow(const std::string& viewId, const char* label, bool* open) {
    WebView* view = GetWebView(viewId);
    if (!view) return;

    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(view->GetWidth()),
                                    static_cast<float>(view->GetHeight())),
                             ImGuiCond_FirstUseEver);

    if (ImGui::Begin(label, open)) {
        RenderImGuiInline(viewId, 0, 0);
    }
    ImGui::End();
}

void WebViewManager::RenderImGuiInline(const std::string& viewId, float width, float height) {
    WebView* view = GetWebView(viewId);
    if (!view) return;

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    float w = width > 0 ? width : availSize.x;
    float h = height > 0 ? height : availSize.y;

    // Resize webview if needed
    int iw = static_cast<int>(w);
    int ih = static_cast<int>(h);
    if (iw != view->GetWidth() || ih != view->GetHeight()) {
        view->Resize(iw, ih);
    }

    // Render texture or fallback
    if (void* texId = view->GetTextureId()) {
        ImGui::Image(texId, ImVec2(w, h));

        // Handle input if hovered
        if (ImGui::IsItemHovered()) {
            // Forward mouse events
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 itemPos = ImGui::GetItemRectMin();
            int mx = static_cast<int>(mousePos.x - itemPos.x);
            int my = static_cast<int>(mousePos.y - itemPos.y);

            view->InjectMouseMove(mx, my);

            for (int i = 0; i < 3; ++i) {
                if (ImGui::IsMouseClicked(i)) {
                    view->InjectMouseButton(i, true, mx, my);
                }
                if (ImGui::IsMouseReleased(i)) {
                    view->InjectMouseButton(i, false, mx, my);
                }
            }

            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f) {
                view->InjectMouseWheel(io.MouseWheelH, io.MouseWheel);
            }
        }

        // Focus handling
        bool isFocused = ImGui::IsItemFocused();
        if (isFocused != view->IsFocused()) {
            view->SetFocused(isFocused);
        }
    } else {
        // Fallback: Show placeholder
        ImGui::BeginChild("WebViewFallback", ImVec2(w, h), true);
        ImGui::TextWrapped("Web View: %s", viewId.c_str());
        ImGui::TextWrapped("Native webview not available. Using ImGui fallback.");
        ImGui::TextWrapped("Source: %s", view->GetCurrentSource().c_str());
        ImGui::EndChild();
    }
}

// ============================================================================
// ImGuiFallbackRenderer Implementation
// ============================================================================

struct ImGuiFallbackRenderer::DOMElement {
    std::string tagName;
    std::string id;
    std::string className;
    std::string textContent;
    std::unordered_map<std::string, std::string> attributes;
    std::vector<std::unique_ptr<DOMElement>> children;

    // Computed styles
    ImVec4 color{1, 1, 1, 1};
    ImVec4 backgroundColor{0, 0, 0, 0};
    float marginTop = 0, marginRight = 0, marginBottom = 0, marginLeft = 0;
    float paddingTop = 0, paddingRight = 0, paddingBottom = 0, paddingLeft = 0;
};

struct ImGuiFallbackRenderer::CSSRule {
    std::string selector;
    std::unordered_map<std::string, std::string> properties;
};

ImGuiFallbackRenderer::ImGuiFallbackRenderer() = default;
ImGuiFallbackRenderer::~ImGuiFallbackRenderer() = default;

bool ImGuiFallbackRenderer::ParseHtml(const std::string& html) {
    m_elements.clear();

    // Simple HTML parser - handles basic tags
    // This is a basic implementation; a real one would use a proper parser

    std::regex tagRegex(R"(<(\w+)([^>]*)>(.*?)</\1>)", std::regex::icase);
    std::regex attrRegex(R"((\w+)="([^"]*)")", std::regex::icase);

    std::string remaining = html;
    std::smatch match;

    while (std::regex_search(remaining, match, tagRegex)) {
        auto element = std::make_unique<DOMElement>();
        element->tagName = match[1].str();

        // Parse attributes
        std::string attrs = match[2].str();
        std::smatch attrMatch;
        while (std::regex_search(attrs, attrMatch, attrRegex)) {
            std::string name = attrMatch[1].str();
            std::string value = attrMatch[2].str();

            if (name == "id") element->id = value;
            else if (name == "class") element->className = value;
            else element->attributes[name] = value;

            attrs = attrMatch.suffix();
        }

        element->textContent = match[3].str();
        m_elements.push_back(std::move(element));

        remaining = match.suffix();
    }

    return true;
}

void ImGuiFallbackRenderer::ParseCss(const std::string& css) {
    m_cssRules.clear();

    // Simple CSS parser
    std::regex ruleRegex(R"(([^{]+)\{([^}]+)\})");
    std::regex propRegex(R"(([^:]+):([^;]+);?)");

    std::string remaining = css;
    std::smatch match;

    while (std::regex_search(remaining, match, ruleRegex)) {
        CSSRule rule;
        rule.selector = match[1].str();

        // Trim selector
        rule.selector.erase(0, rule.selector.find_first_not_of(" \t\n\r"));
        rule.selector.erase(rule.selector.find_last_not_of(" \t\n\r") + 1);

        std::string props = match[2].str();
        std::smatch propMatch;

        while (std::regex_search(props, propMatch, propRegex)) {
            std::string name = propMatch[1].str();
            std::string value = propMatch[2].str();

            // Trim
            name.erase(0, name.find_first_not_of(" \t\n\r"));
            name.erase(name.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);

            rule.properties[name] = value;
            props = propMatch.suffix();
        }

        m_cssRules.push_back(rule);
        remaining = match.suffix();
    }
}

void ImGuiFallbackRenderer::Render() {
    for (const auto& element : m_elements) {
        RenderElement(*element);
    }
}

void ImGuiFallbackRenderer::RenderElement(const DOMElement& element) {
    const std::string& tag = element.tagName;

    if (tag == "div" || tag == "p") {
        ImGui::TextWrapped("%s", element.textContent.c_str());
    }
    else if (tag == "h1") {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Assuming larger font at index 0
        ImGui::Text("%s", element.textContent.c_str());
        ImGui::PopFont();
    }
    else if (tag == "input") {
        auto typeIt = element.attributes.find("type");
        std::string type = typeIt != element.attributes.end() ? typeIt->second : "text";

        if (type == "text") {
            static char buffer[256] = "";
            if (ImGui::InputText(element.id.c_str(), buffer, sizeof(buffer))) {
                m_formData[element.id] = buffer;
            }
        }
        else if (type == "number") {
            static float value = 0;
            if (ImGui::InputFloat(element.id.c_str(), &value)) {
                m_formData[element.id] = std::to_string(value);
            }
        }
        else if (type == "checkbox") {
            static bool checked = false;
            if (ImGui::Checkbox(element.id.c_str(), &checked)) {
                m_formData[element.id] = checked ? "true" : "false";
            }
        }
        else if (type == "range") {
            static float value = 0.5f;
            float min = 0.0f, max = 1.0f;
            auto minIt = element.attributes.find("min");
            auto maxIt = element.attributes.find("max");
            if (minIt != element.attributes.end()) min = std::stof(minIt->second);
            if (maxIt != element.attributes.end()) max = std::stof(maxIt->second);

            if (ImGui::SliderFloat(element.id.c_str(), &value, min, max)) {
                m_formData[element.id] = std::to_string(value);
            }
        }
    }
    else if (tag == "button") {
        if (ImGui::Button(element.textContent.c_str())) {
            auto onclickIt = element.attributes.find("onclick");
            if (onclickIt != element.attributes.end() && m_jsCallHandler) {
                m_jsCallHandler(onclickIt->second, "[]");
            }
        }
    }
    else if (tag == "select") {
        // Handle select with combo box
        static int currentItem = 0;
        if (ImGui::BeginCombo(element.id.c_str(), "Select...")) {
            // Options would come from children
            ImGui::EndCombo();
        }
    }

    // Render children
    for (const auto& child : element.children) {
        RenderElement(*child);
    }
}

void ImGuiFallbackRenderer::SetElementValue(const std::string& elementId, const std::string& value) {
    m_formData[elementId] = value;
}

std::string ImGuiFallbackRenderer::GetFormData() const {
    std::string json = "{";
    bool first = true;
    for (const auto& [key, value] : m_formData) {
        if (!first) json += ",";
        json += "\"" + key + "\":\"" + value + "\"";
        first = false;
    }
    json += "}";
    return json;
}

} // namespace WebEditor
} // namespace Vehement
