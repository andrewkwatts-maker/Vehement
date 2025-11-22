#include "WebViewManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

// ImGui for fallback rendering
#include <imgui.h>

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
    , m_nativeHandle(other.m_nativeHandle)
    , m_textureId(other.m_textureId)
    , m_focused(other.m_focused)
    , m_hotReloadEnabled(other.m_hotReloadEnabled)
    , m_watchedFiles(std::move(other.m_watchedFiles))
    , m_messageHandler(std::move(other.m_messageHandler))
{
    other.m_nativeHandle = nullptr;
    other.m_textureId = nullptr;
}

WebView& WebView::operator=(WebView&& other) noexcept {
    if (this != &other) {
        DestroyTexture();

        m_config = std::move(other.m_config);
        m_currentSource = std::move(other.m_currentSource);
        m_nativeHandle = other.m_nativeHandle;
        m_textureId = other.m_textureId;
        m_focused = other.m_focused;
        m_hotReloadEnabled = other.m_hotReloadEnabled;
        m_watchedFiles = std::move(other.m_watchedFiles);
        m_messageHandler = std::move(other.m_messageHandler);

        other.m_nativeHandle = nullptr;
        other.m_textureId = nullptr;
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

    // TODO: Actually load into native webview
    // For now, store the HTML for fallback rendering

    return true;
}

bool WebView::LoadUrl(const std::string& url) {
    m_currentSource = url;
    // TODO: Load URL in native webview
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
    // TODO: Execute in native webview
    // For now, queue the script
    if (callback) {
        callback("null");  // Placeholder
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

        // TODO: Resize native webview
    }
}

void WebView::InjectMouseMove(int x, int y) {
    // TODO: Forward to native webview
}

void WebView::InjectMouseButton(int button, bool pressed, int x, int y) {
    // TODO: Forward to native webview
}

void WebView::InjectMouseWheel(float deltaX, float deltaY) {
    // TODO: Forward to native webview
}

void WebView::InjectKeyEvent(int key, int scancode, int action, int mods) {
    // TODO: Forward to native webview
}

void WebView::InjectCharEvent(unsigned int codepoint) {
    // TODO: Forward to native webview
}

void WebView::SetFocused(bool focused) {
    m_focused = focused;
    // TODO: Notify native webview of focus change
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
    // TODO: Create OpenGL texture for rendering
    // For now, just placeholder
    m_textureId = nullptr;
}

void WebView::DestroyTexture() {
    // TODO: Delete OpenGL texture
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
    // TODO: Enable dev tools in all webviews
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
