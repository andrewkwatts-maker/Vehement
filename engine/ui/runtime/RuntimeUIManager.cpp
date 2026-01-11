#include "RuntimeUIManager.hpp"
#include "HTMLRenderer.hpp"
#include "UIWindow.hpp"
#include "UIBinding.hpp"
#include "UIEventRouter.hpp"
#include "UIAnimation.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace Engine {
namespace UI {

RuntimeUIManager& RuntimeUIManager::GetInstance() {
    static RuntimeUIManager instance;
    return instance;
}

RuntimeUIManager::RuntimeUIManager() = default;
RuntimeUIManager::~RuntimeUIManager() {
    if (m_initialized) {
        Shutdown();
    }
}

bool RuntimeUIManager::Initialize(const RenderContext& context) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return true;
    }

    m_context = context;

    // Initialize subsystems
    m_renderer = std::make_unique<HTMLRenderer>();
    if (!m_renderer->Initialize(context.viewportWidth, context.viewportHeight, context.dpiScale)) {
        return false;
    }

    m_binding = std::make_unique<UIBinding>();
    m_binding->Initialize();

    m_eventRouter = std::make_unique<UIEventRouter>();
    m_eventRouter->Initialize();

    m_animation = std::make_unique<UIAnimation>();
    m_animation->Initialize();

    // Register built-in bindings
    m_binding->ExposeFunction("closeWindow", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            CloseWindow(args["id"].get<std::string>());
        }
        return nlohmann::json(true);
    });

    m_binding->ExposeFunction("showWindow", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            ShowWindow(args["id"].get<std::string>());
        }
        return nlohmann::json(true);
    });

    m_binding->ExposeFunction("hideWindow", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            HideWindow(args["id"].get<std::string>());
        }
        return nlohmann::json(true);
    });

    m_binding->ExposeFunction("toggleWindow", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            return nlohmann::json(ToggleWindow(args["id"].get<std::string>()));
        }
        return nlohmann::json(false);
    });

    m_binding->ExposeFunction("playAnimation", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("name")) {
            std::string target = args.value("target", "");
            m_animation->Play(args["name"].get<std::string>(), target);
        }
        return nlohmann::json(true);
    });

    m_initialized = true;
    return true;
}

void RuntimeUIManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return;
    }

    // Close all windows
    m_windows.clear();
    m_zOrderedWindows.clear();
    m_modalStack.clear();

    // Shutdown subsystems
    if (m_animation) {
        m_animation->Shutdown();
        m_animation.reset();
    }

    if (m_eventRouter) {
        m_eventRouter->Shutdown();
        m_eventRouter.reset();
    }

    if (m_binding) {
        m_binding->Shutdown();
        m_binding.reset();
    }

    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    m_initialized = false;
}

void RuntimeUIManager::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Process pending thread-safe actions
    ProcessPendingActions();

    // Update animations
    m_animation->Update(deltaTime);

    // Update all windows
    for (auto& [id, window] : m_windows) {
        if (window->IsVisible()) {
            window->Update(deltaTime);
        }
    }

    // Update event router
    m_eventRouter->Update(deltaTime);
}

void RuntimeUIManager::Render() {
    if (!m_initialized) {
        return;
    }

    m_renderer->BeginFrame();

    // Render windows in z-order
    for (UIWindow* window : m_zOrderedWindows) {
        if (window->IsVisible()) {
            m_renderer->RenderWindow(window);
        }
    }

    // Render debug overlay if enabled
    if (m_debugRendering) {
        m_renderer->RenderDebugOverlay(m_zOrderedWindows);
    }

    m_renderer->EndFrame();
}

UIWindow* RuntimeUIManager::CreateWindow(const std::string& id, const std::string& htmlPath, UILayer layer) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_windows.find(id) != m_windows.end()) {
        // Window already exists
        return m_windows[id].get();
    }

    auto window = std::make_unique<UIWindow>(id, this);
    if (!window->LoadHTML(htmlPath)) {
        return nullptr;
    }

    window->SetLayer(layer);
    UIWindow* ptr = window.get();
    m_windows[id] = std::move(window);

    UpdateWindowZOrder();

    return ptr;
}

UIWindow* RuntimeUIManager::GetWindow(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        return it->second.get();
    }
    return nullptr;
}

void RuntimeUIManager::CloseWindow(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        // Remove from modal stack if present
        auto modalIt = std::find(m_modalStack.begin(), m_modalStack.end(), id);
        if (modalIt != m_modalStack.end()) {
            m_modalStack.erase(modalIt);
        }

        // Clear focus if this was focused
        if (m_focusedWindow == it->second.get()) {
            m_focusedWindow = nullptr;
        }

        m_windows.erase(it);
        UpdateWindowZOrder();
    }
}

void RuntimeUIManager::ShowWindow(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        it->second->Show();
        BringToFront(id);
    }
}

void RuntimeUIManager::HideWindow(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        it->second->Hide();
    }
}

bool RuntimeUIManager::ToggleWindow(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        if (it->second->IsVisible()) {
            it->second->Hide();
            return false;
        } else {
            it->second->Show();
            BringToFront(id);
            return true;
        }
    }
    return false;
}

bool RuntimeUIManager::IsWindowVisible(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        return it->second->IsVisible();
    }
    return false;
}

void RuntimeUIManager::BringToFront(const std::string& id) {
    auto it = m_windows.find(id);
    if (it != m_windows.end()) {
        it->second->SetZIndex(static_cast<int>(it->second->GetLayer()) + 99);
        UpdateWindowZOrder();
    }
}

std::vector<UIWindow*> RuntimeUIManager::GetWindowsInLayer(UILayer layer) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<UIWindow*> result;
    for (const auto& [id, window] : m_windows) {
        if (window->GetLayer() == layer) {
            result.push_back(window.get());
        }
    }
    return result;
}

std::string RuntimeUIManager::ShowModal(const ModalConfig& config) {
    static int modalCounter = 0;
    std::string modalId = "modal_" + std::to_string(++modalCounter);

    // Create modal window
    auto window = std::make_unique<UIWindow>(modalId, this);
    window->SetLayer(UILayer::Modals);
    window->SetModal(true);

    // Build modal HTML
    std::stringstream html;
    html << "<!DOCTYPE html><html><head><style>";
    html << ".modal-overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.5);display:flex;align-items:center;justify-content:center;}";
    html << ".modal-content{background:#2a2a2a;border-radius:8px;padding:20px;min-width:" << config.width << "px;max-width:90%;box-shadow:0 4px 20px rgba(0,0,0,0.3);}";
    html << ".modal-title{font-size:18px;font-weight:bold;margin-bottom:15px;color:#fff;}";
    html << ".modal-body{color:#ccc;margin-bottom:20px;}";
    html << ".modal-buttons{display:flex;justify-content:flex-end;gap:10px;}";
    html << ".modal-btn{padding:8px 16px;border:none;border-radius:4px;cursor:pointer;font-size:14px;}";
    html << ".modal-btn-primary{background:#4a9eff;color:#fff;}.modal-btn-primary:hover{background:#3a8eef;}";
    html << ".modal-btn-secondary{background:#555;color:#fff;}.modal-btn-secondary:hover{background:#666;}";
    html << "</style></head><body>";
    html << "<div class='modal-overlay' onclick='if(event.target===this)" << (config.closeOnOutsideClick ? "closeModal()" : "") << "'>";
    html << "<div class='modal-content'>";

    if (!config.title.empty()) {
        html << "<div class='modal-title'>" << config.title;
        if (config.showCloseButton) {
            html << "<span style='float:right;cursor:pointer;' onclick='closeModal()'>×</span>";
        }
        html << "</div>";
    }

    html << "<div class='modal-body'>";
    if (!config.htmlContent.empty()) {
        html << config.htmlContent;
    } else {
        html << config.message;
    }
    html << "</div>";

    html << "<div class='modal-buttons'>";
    for (size_t i = 0; i < config.buttons.size(); ++i) {
        std::string btnClass = (i == config.buttons.size() - 1) ? "modal-btn-primary" : "modal-btn-secondary";
        html << "<button class='modal-btn " << btnClass << "' onclick='selectButton(" << i << ")'>" << config.buttons[i] << "</button>";
    }
    html << "</div></div></div>";
    html << "<script>";
    html << "function closeModal(){Engine.closeModal('" << modalId << "','cancel','');}";
    html << "function selectButton(idx){Engine.closeModal('" << modalId << "','custom',idx.toString());}";
    html << "</script></body></html>";

    window->LoadHTMLString(html.str());
    window->SetCallback(config.callback);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_windows[modalId] = std::move(window);
        m_modalStack.push_back(modalId);
        UpdateWindowZOrder();
    }

    return modalId;
}

void RuntimeUIManager::CloseModal(const std::string& id, ModalResult result, const std::string& customData) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_windows.find(id);
    if (it != m_windows.end() && it->second->IsModal()) {
        // Call callback
        auto callback = it->second->GetCallback();
        if (callback) {
            callback(result, customData);
        }

        // Remove from modal stack
        auto modalIt = std::find(m_modalStack.begin(), m_modalStack.end(), id);
        if (modalIt != m_modalStack.end()) {
            m_modalStack.erase(modalIt);
        }

        m_windows.erase(it);
        UpdateWindowZOrder();
    }
}

bool RuntimeUIManager::IsModalOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_modalStack.empty();
}

UIWindow* RuntimeUIManager::GetTopmostModal() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_modalStack.empty()) {
        return nullptr;
    }

    auto it = m_windows.find(m_modalStack.back());
    if (it != m_windows.end()) {
        return it->second.get();
    }
    return nullptr;
}

void RuntimeUIManager::SetViewportMode(ViewportMode mode) {
    m_viewportMode = mode;

    // Notify renderer of mode change
    if (m_renderer) {
        m_renderer->SetViewportMode(mode == ViewportMode::Fullscreen || mode == ViewportMode::BorderlessFullscreen);
    }
}

ViewportMode RuntimeUIManager::GetViewportMode() const {
    return m_viewportMode;
}

void RuntimeUIManager::ResizeViewport(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_context.viewportWidth = width;
    m_context.viewportHeight = height;

    if (m_renderer) {
        m_renderer->Resize(width, height);
    }

    // Notify windows of resize
    for (auto& [id, window] : m_windows) {
        window->OnViewportResize(width, height);
    }
}

void RuntimeUIManager::SetDPIScale(float scale) {
    m_context.dpiScale = scale;
    if (m_renderer) {
        m_renderer->SetDPIScale(scale);
    }
}

float RuntimeUIManager::GetDPIScale() const {
    return m_context.dpiScale;
}

bool RuntimeUIManager::RouteMouseInput(int x, int y, int button, bool pressed) {
    m_mouseX = x;
    m_mouseY = y;

    // Check if modal is blocking
    if (!m_modalStack.empty()) {
        UIWindow* modal = GetTopmostModal();
        if (modal && modal->HitTest(x, y)) {
            return m_eventRouter->RouteMouseEvent(modal, x, y, button, pressed);
        }
        return true; // Block input when modal is open
    }

    // Find window at point
    UIWindow* targetWindow = FindWindowAtPoint(x, y);
    if (targetWindow) {
        // Handle window dragging
        if (pressed && button == 0 && targetWindow->IsTitleBarHit(x, y)) {
            m_draggedWindow = targetWindow;
        }

        // Update focus
        if (pressed && m_focusedWindow != targetWindow) {
            if (m_focusedWindow) {
                m_focusedWindow->OnFocusLost();
            }
            m_focusedWindow = targetWindow;
            m_focusedWindow->OnFocusGained();
            BringToFront(targetWindow->GetID());
        }

        return m_eventRouter->RouteMouseEvent(targetWindow, x, y, button, pressed);
    }

    // Release drag
    if (!pressed) {
        m_draggedWindow = nullptr;
    }

    return false;
}

bool RuntimeUIManager::RouteMouseMove(int x, int y) {
    int deltaX = x - m_mouseX;
    int deltaY = y - m_mouseY;
    m_mouseX = x;
    m_mouseY = y;

    // Handle window dragging
    if (m_draggedWindow) {
        m_draggedWindow->Move(m_draggedWindow->GetX() + deltaX, m_draggedWindow->GetY() + deltaY);
        return true;
    }

    // Route to target window
    UIWindow* targetWindow = FindWindowAtPoint(x, y);
    if (targetWindow) {
        return m_eventRouter->RouteMouseMoveEvent(targetWindow, x, y);
    }

    return false;
}

bool RuntimeUIManager::RouteMouseScroll(int x, int y, float scrollX, float scrollY) {
    UIWindow* targetWindow = FindWindowAtPoint(x, y);
    if (targetWindow) {
        return m_eventRouter->RouteScrollEvent(targetWindow, x, y, scrollX, scrollY);
    }
    return false;
}

bool RuntimeUIManager::RouteKeyboardInput(int keyCode, bool pressed, int modifiers) {
    // Route to focused window
    if (m_focusedWindow) {
        return m_eventRouter->RouteKeyEvent(m_focusedWindow, keyCode, pressed, modifiers);
    }

    // Route to topmost modal
    UIWindow* modal = GetTopmostModal();
    if (modal) {
        return m_eventRouter->RouteKeyEvent(modal, keyCode, pressed, modifiers);
    }

    return false;
}

bool RuntimeUIManager::RouteTextInput(const std::string& text) {
    if (m_focusedWindow) {
        return m_eventRouter->RouteTextEvent(m_focusedWindow, text);
    }
    return false;
}

bool RuntimeUIManager::RouteTouchInput(int touchId, int x, int y, int phase) {
    UIWindow* targetWindow = FindWindowAtPoint(x, y);
    if (targetWindow) {
        return m_eventRouter->RouteTouchEvent(targetWindow, touchId, x, y, phase);
    }
    return false;
}

bool RuntimeUIManager::RouteGamepadInput(int button, bool pressed) {
    // Route to focused window for gamepad navigation
    if (m_focusedWindow) {
        return m_eventRouter->RouteGamepadEvent(m_focusedWindow, button, pressed);
    }
    return false;
}

HTMLRenderer& RuntimeUIManager::GetRenderer() {
    return *m_renderer;
}

UIBinding& RuntimeUIManager::GetBinding() {
    return *m_binding;
}

UIEventRouter& RuntimeUIManager::GetEventRouter() {
    return *m_eventRouter;
}

UIAnimation& RuntimeUIManager::GetAnimation() {
    return *m_animation;
}

void RuntimeUIManager::LoadTheme(const std::string& cssPath) {
    if (m_renderer) {
        m_renderer->LoadGlobalCSS(cssPath);
    }
}

std::string RuntimeUIManager::ExecuteScript(const std::string& windowId, const std::string& script) {
    auto it = m_windows.find(windowId);
    if (it != m_windows.end()) {
        return it->second->ExecuteScript(script);
    }
    return "";
}

void RuntimeUIManager::SetCSSVariable(const std::string& name, const std::string& value) {
    if (m_renderer) {
        m_renderer->SetCSSVariable(name, value);
    }
}

void RuntimeUIManager::SetDebugRendering(bool enabled) {
    m_debugRendering = enabled;
}

void RuntimeUIManager::GetRenderStats(int& drawCalls, int& triangles, size_t& textureMemory) const {
    if (m_renderer) {
        m_renderer->GetStats(drawCalls, triangles, textureMemory);
    }
}

void RuntimeUIManager::UpdateWindowZOrder() {
    m_zOrderedWindows.clear();

    for (auto& [id, window] : m_windows) {
        m_zOrderedWindows.push_back(window.get());
    }

    // Sort by layer then z-index
    std::sort(m_zOrderedWindows.begin(), m_zOrderedWindows.end(),
        [](const UIWindow* a, const UIWindow* b) {
            if (a->GetLayer() != b->GetLayer()) {
                return static_cast<int>(a->GetLayer()) < static_cast<int>(b->GetLayer());
            }
            return a->GetZIndex() < b->GetZIndex();
        });
}

UIWindow* RuntimeUIManager::FindWindowAtPoint(int x, int y) const {
    // Search from top to bottom (reverse z-order)
    for (auto it = m_zOrderedWindows.rbegin(); it != m_zOrderedWindows.rend(); ++it) {
        UIWindow* window = *it;
        if (window->IsVisible() && window->HitTest(x, y)) {
            return window;
        }
    }
    return nullptr;
}

void RuntimeUIManager::ProcessPendingActions() {
    std::vector<PendingAction> actions;
    {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        actions = std::move(m_pendingActions);
        m_pendingActions.clear();
    }

    for (const auto& action : actions) {
        switch (action.type) {
            case PendingAction::Close:
                CloseWindow(action.windowId);
                break;
            case PendingAction::Show:
                ShowWindow(action.windowId);
                break;
            case PendingAction::Hide:
                HideWindow(action.windowId);
                break;
            case PendingAction::BringToFront:
                BringToFront(action.windowId);
                break;
            default:
                break;
        }
    }
}

} // namespace UI
} // namespace Engine
