#include "UIEventRouter.hpp"
#include "UIWindow.hpp"
#include "HTMLRenderer.hpp"

#include <algorithm>
#include <chrono>

namespace Engine {
namespace UI {

UIEventRouter::UIEventRouter() = default;
UIEventRouter::~UIEventRouter() { Shutdown(); }

void UIEventRouter::Initialize() {
    if (m_initialized) return;
    m_initialized = true;
}

void UIEventRouter::Shutdown() {
    m_listeners.clear();
    m_listenerRegistry.clear();
    m_tabIndices.clear();
    m_touchPoints.clear();
    m_keyStates.clear();
    m_focusedElement = nullptr;
    m_hoveredElement = nullptr;
    m_capturedElement = nullptr;
    m_initialized = false;
}

void UIEventRouter::Update(float deltaTime) {
    ProcessEventQueue();
}

bool UIEventRouter::RouteMouseEvent(UIWindow* window, int x, int y, int button, bool pressed) {
    if (!window) return false;

    UpdateMouseButtonState(button, pressed);
    m_mouseX = x;
    m_mouseY = y;

    // Find target element
    DOMElement* target = m_capturedElement ? m_capturedElement : FindElementAtPoint(window, x, y);
    if (!target) return false;

    UIEvent event;
    event.type = pressed ? "mousedown" : "mouseup";
    event.target = target;
    event.clientX = x;
    event.clientY = y;
    event.button = static_cast<MouseButton>(button);
    event.buttons = m_mouseButtonStates;
    event.modifiers = m_activeModifiers;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Calculate offset relative to target
    event.offsetX = x - static_cast<int>(target->layout.x);
    event.offsetY = y - static_cast<int>(target->layout.y);

    bool handled = DispatchEvent(target, event);

    // Handle click event
    if (!pressed && button == 0 && !event.defaultPrevented) {
        UIEvent clickEvent = event;
        clickEvent.type = "click";
        DispatchEvent(target, clickEvent);
    }

    return handled;
}

bool UIEventRouter::RouteMouseMoveEvent(UIWindow* window, int x, int y) {
    if (!window) return false;

    int movementX = x - m_mouseX;
    int movementY = y - m_mouseY;
    m_mouseX = x;
    m_mouseY = y;

    // Update hover state
    DOMElement* target = m_capturedElement ? m_capturedElement : FindElementAtPoint(window, x, y);

    // Handle enter/leave events
    if (target != m_hoveredElement) {
        TriggerEnterLeaveEvents(m_hoveredElement, target, x, y);
        m_hoveredElement = target;
    }

    if (!target) return false;

    UIEvent event;
    event.type = "mousemove";
    event.target = target;
    event.clientX = x;
    event.clientY = y;
    event.movementX = movementX;
    event.movementY = movementY;
    event.buttons = m_mouseButtonStates;
    event.modifiers = m_activeModifiers;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return DispatchEvent(target, event);
}

bool UIEventRouter::RouteScrollEvent(UIWindow* window, int x, int y, float scrollX, float scrollY) {
    if (!window) return false;

    DOMElement* target = FindElementAtPoint(window, x, y);
    if (!target) return false;

    UIEvent event;
    event.type = "wheel";
    event.target = target;
    event.clientX = x;
    event.clientY = y;
    event.wheelDeltaX = scrollX;
    event.wheelDeltaY = scrollY;
    event.modifiers = m_activeModifiers;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return DispatchEvent(target, event);
}

bool UIEventRouter::RouteKeyEvent(UIWindow* window, int keyCode, bool pressed, int modifiers) {
    if (!window) return false;

    UpdateKeyState(keyCode, pressed);
    m_activeModifiers = static_cast<KeyModifier>(modifiers);

    DOMElement* target = m_focusedElement;
    if (!target) {
        target = window->GetRootElement();
    }
    if (!target) return false;

    UIEvent event;
    event.type = pressed ? "keydown" : "keyup";
    event.target = target;
    event.keyCode = keyCode;
    event.modifiers = m_activeModifiers;
    event.repeat = pressed && m_keyStates[keyCode];
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Map key codes to key names (simplified)
    if (keyCode >= 65 && keyCode <= 90) {
        event.key = std::string(1, static_cast<char>(keyCode));
    } else if (keyCode >= 48 && keyCode <= 57) {
        event.key = std::string(1, static_cast<char>(keyCode));
    } else {
        switch (keyCode) {
            case 13: event.key = "Enter"; break;
            case 27: event.key = "Escape"; break;
            case 32: event.key = " "; break;
            case 9: event.key = "Tab"; break;
            case 8: event.key = "Backspace"; break;
            case 46: event.key = "Delete"; break;
            case 37: event.key = "ArrowLeft"; break;
            case 38: event.key = "ArrowUp"; break;
            case 39: event.key = "ArrowRight"; break;
            case 40: event.key = "ArrowDown"; break;
            default: event.key = "Unknown"; break;
        }
    }

    bool handled = DispatchEvent(target, event);

    // Handle tab navigation
    if (pressed && keyCode == 9 && !event.defaultPrevented) {
        if (m_activeModifiers & KeyModifier::Shift) {
            FocusPrevious();
        } else {
            FocusNext();
        }
        handled = true;
    }

    return handled;
}

bool UIEventRouter::RouteTextEvent(UIWindow* window, const std::string& text) {
    if (!window || !m_focusedElement) return false;

    UIEvent event;
    event.type = "input";
    event.target = m_focusedElement;
    event.key = text;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return DispatchEvent(m_focusedElement, event);
}

bool UIEventRouter::RouteTouchEvent(UIWindow* window, int touchId, int x, int y, int phase) {
    if (!window) return false;

    TouchPhase touchPhase = static_cast<TouchPhase>(phase);

    // Find or create touch point
    TouchPoint* touch = GetTouchPoint(touchId);
    if (!touch && touchPhase == TouchPhase::Begin) {
        TouchPoint newTouch;
        newTouch.id = touchId;
        newTouch.x = x;
        newTouch.y = y;
        newTouch.startX = x;
        newTouch.startY = y;
        newTouch.phase = touchPhase;
        newTouch.startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        newTouch.target = FindElementAtPoint(window, x, y);
        m_touchPoints.push_back(newTouch);
        touch = &m_touchPoints.back();
    }

    if (!touch) return false;

    touch->x = x;
    touch->y = y;
    touch->phase = touchPhase;

    DOMElement* target = touch->target;
    if (!target) return false;

    UIEvent event;
    event.target = target;
    event.clientX = x;
    event.clientY = y;
    event.touchId = touchId;
    event.touchPhase = touchPhase;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    switch (touchPhase) {
        case TouchPhase::Begin:
            event.type = "touchstart";
            break;
        case TouchPhase::Move:
            event.type = "touchmove";
            break;
        case TouchPhase::End:
            event.type = "touchend";
            break;
        case TouchPhase::Cancel:
            event.type = "touchcancel";
            break;
    }

    bool handled = DispatchEvent(target, event);

    // Remove ended/cancelled touches
    if (touchPhase == TouchPhase::End || touchPhase == TouchPhase::Cancel) {
        m_touchPoints.erase(
            std::remove_if(m_touchPoints.begin(), m_touchPoints.end(),
                [touchId](const TouchPoint& t) { return t.id == touchId; }),
            m_touchPoints.end());
    }

    return handled;
}

bool UIEventRouter::RouteGamepadEvent(UIWindow* window, int button, bool pressed) {
    if (!window) return false;

    GamepadButton gamepadButton = static_cast<GamepadButton>(button);

    // Handle navigation if enabled
    if (m_gamepadNavigationEnabled && pressed) {
        HandleGamepadNavigation(gamepadButton);
        return true;
    }

    DOMElement* target = m_focusedElement;
    if (!target) return false;

    UIEvent event;
    event.type = pressed ? "gamepadbuttondown" : "gamepadbuttonup";
    event.target = target;
    event.gamepadButton = gamepadButton;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return DispatchEvent(target, event);
}

bool UIEventRouter::DispatchEvent(DOMElement* element, UIEvent& event) {
    if (!element) return false;

    m_eventsDispatched++;

    // Build event path (capture phase order)
    std::vector<DOMElement*> path;
    BuildEventPath(element, path);

    // Capture phase
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if (event.immediatePropagationStopped) break;

        event.currentTarget = *it;

        auto listenerIt = m_listeners.find(*it);
        if (listenerIt != m_listeners.end()) {
            for (auto& listener : listenerIt->second) {
                if (listener.type == event.type && listener.capture) {
                    listener.handler(event);
                    if (event.immediatePropagationStopped) break;
                }
            }
        }
    }

    // Target phase
    if (!event.propagationStopped) {
        event.currentTarget = element;

        auto listenerIt = m_listeners.find(element);
        if (listenerIt != m_listeners.end()) {
            for (auto& listener : listenerIt->second) {
                if (listener.type == event.type) {
                    listener.handler(event);
                    if (event.immediatePropagationStopped) break;
                }
            }
        }
    }

    // Bubble phase
    if (event.bubbles && !event.propagationStopped) {
        for (DOMElement* ancestor : path) {
            if (event.propagationStopped) break;

            event.currentTarget = ancestor;

            auto listenerIt = m_listeners.find(ancestor);
            if (listenerIt != m_listeners.end()) {
                for (auto& listener : listenerIt->second) {
                    if (listener.type == event.type && !listener.capture) {
                        listener.handler(event);
                        if (event.immediatePropagationStopped) break;
                    }
                }
            }
        }
    }

    // Remove one-time listeners
    for (auto& [elem, listeners] : m_listeners) {
        listeners.erase(
            std::remove_if(listeners.begin(), listeners.end(),
                [&event](const EventListener& l) {
                    return l.type == event.type && l.once;
                }),
            listeners.end());
    }

    if (!event.defaultPrevented) {
        m_eventsHandled++;
    }

    return !event.defaultPrevented;
}

bool UIEventRouter::DispatchCustomEvent(DOMElement* element, const std::string& type,
                                        const std::unordered_map<std::string, std::string>& detail) {
    UIEvent event;
    event.type = type;
    event.target = element;
    event.bubbles = true;
    event.cancelable = true;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return DispatchEvent(element, event);
}

int UIEventRouter::AddEventListener(DOMElement* element, const std::string& type,
                                   std::function<void(UIEvent&)> handler, bool capture) {
    EventListener listener;
    listener.type = type;
    listener.handler = handler;
    listener.capture = capture;

    return AddEventListener(element, listener);
}

int UIEventRouter::AddEventListener(DOMElement* element, const EventListener& listener) {
    int id = m_nextListenerId++;

    m_listeners[element].push_back(listener);
    m_listenerRegistry[id] = {element, m_listeners[element].size() - 1};

    return id;
}

void UIEventRouter::RemoveEventListener(int listenerId) {
    auto it = m_listenerRegistry.find(listenerId);
    if (it == m_listenerRegistry.end()) return;

    DOMElement* element = it->second.first;
    size_t index = it->second.second;

    auto& listeners = m_listeners[element];
    if (index < listeners.size()) {
        listeners.erase(listeners.begin() + index);
    }

    m_listenerRegistry.erase(it);
}

void UIEventRouter::RemoveAllListeners(DOMElement* element) {
    m_listeners.erase(element);

    // Clean up registry
    for (auto it = m_listenerRegistry.begin(); it != m_listenerRegistry.end();) {
        if (it->second.first == element) {
            it = m_listenerRegistry.erase(it);
        } else {
            ++it;
        }
    }
}

void UIEventRouter::RemoveListenersByType(DOMElement* element, const std::string& type) {
    auto it = m_listeners.find(element);
    if (it != m_listeners.end()) {
        it->second.erase(
            std::remove_if(it->second.begin(), it->second.end(),
                [&type](const EventListener& l) { return l.type == type; }),
            it->second.end());
    }
}

void UIEventRouter::SetFocus(DOMElement* element) {
    if (m_focusedElement == element) return;

    DOMElement* oldFocus = m_focusedElement;

    // Trigger blur on old element
    if (oldFocus) {
        oldFocus->isFocused = false;

        UIEvent blurEvent;
        blurEvent.type = "blur";
        blurEvent.target = oldFocus;
        blurEvent.relatedTarget = element;
        blurEvent.bubbles = false;
        DispatchEvent(oldFocus, blurEvent);

        UIEvent focusOutEvent;
        focusOutEvent.type = "focusout";
        focusOutEvent.target = oldFocus;
        focusOutEvent.relatedTarget = element;
        focusOutEvent.bubbles = true;
        DispatchEvent(oldFocus, focusOutEvent);
    }

    m_focusedElement = element;

    // Trigger focus on new element
    if (element) {
        element->isFocused = true;

        UIEvent focusEvent;
        focusEvent.type = "focus";
        focusEvent.target = element;
        focusEvent.relatedTarget = oldFocus;
        focusEvent.bubbles = false;
        DispatchEvent(element, focusEvent);

        UIEvent focusInEvent;
        focusInEvent.type = "focusin";
        focusInEvent.target = element;
        focusInEvent.relatedTarget = oldFocus;
        focusInEvent.bubbles = true;
        DispatchEvent(element, focusInEvent);
    }
}

void UIEventRouter::ClearFocus() {
    SetFocus(nullptr);
}

void UIEventRouter::MoveFocus(FocusDirection direction) {
    if (!m_focusedElement) return;

    DOMElement* next = FindNextFocusable(m_focusedElement, direction);
    if (next) {
        SetFocus(next);
    }
}

void UIEventRouter::FocusNext() {
    MoveFocus(FocusDirection::Next);
}

void UIEventRouter::FocusPrevious() {
    MoveFocus(FocusDirection::Previous);
}

bool UIEventRouter::IsFocusable(DOMElement* element) const {
    if (!element) return false;

    // Check if explicitly non-focusable
    auto it = m_tabIndices.find(element);
    if (it != m_tabIndices.end() && it->second < 0) {
        return false;
    }

    // Check tag name
    static const std::vector<std::string> focusableTags = {
        "input", "button", "select", "textarea", "a"
    };

    for (const auto& tag : focusableTags) {
        if (element->tagName == tag) return true;
    }

    // Check tabindex attribute
    if (element->attributes.count("tabindex") > 0) {
        return true;
    }

    return false;
}

void UIEventRouter::SetTabIndex(DOMElement* element, int tabIndex) {
    m_tabIndices[element] = tabIndex;
}

int UIEventRouter::GetTabIndex(DOMElement* element) const {
    auto it = m_tabIndices.find(element);
    if (it != m_tabIndices.end()) {
        return it->second;
    }

    // Default tab indices
    if (element->tagName == "input" || element->tagName == "button" ||
        element->tagName == "select" || element->tagName == "textarea") {
        return 0;
    }

    return -1;
}

void UIEventRouter::UpdateHoverState(UIWindow* window, int x, int y) {
    DOMElement* target = FindElementAtPoint(window, x, y);

    if (target != m_hoveredElement) {
        TriggerEnterLeaveEvents(m_hoveredElement, target, x, y);
        m_hoveredElement = target;
    }
}

void UIEventRouter::SetCapture(DOMElement* element) {
    m_capturedElement = element;
}

void UIEventRouter::ReleaseCapture() {
    m_capturedElement = nullptr;
}

TouchPoint* UIEventRouter::GetTouchPoint(int id) {
    for (auto& touch : m_touchPoints) {
        if (touch.id == id) {
            return &touch;
        }
    }
    return nullptr;
}

void UIEventRouter::SetGamepadNavigationEnabled(bool enabled) {
    m_gamepadNavigationEnabled = enabled;
}

void UIEventRouter::SetGamepadNavigationRoot(DOMElement* root) {
    m_gamepadNavRoot = root;
}

void UIEventRouter::HandleGamepadNavigation(GamepadButton button) {
    switch (button) {
        case GamepadButton::DPadUp:
            MoveFocus(FocusDirection::Up);
            break;
        case GamepadButton::DPadDown:
            MoveFocus(FocusDirection::Down);
            break;
        case GamepadButton::DPadLeft:
            MoveFocus(FocusDirection::Left);
            break;
        case GamepadButton::DPadRight:
            MoveFocus(FocusDirection::Right);
            break;
        case GamepadButton::A:
            // Activate focused element
            if (m_focusedElement) {
                UIEvent event;
                event.type = "click";
                event.target = m_focusedElement;
                DispatchEvent(m_focusedElement, event);
            }
            break;
        case GamepadButton::B:
            // Cancel/back
            break;
        default:
            break;
    }
}

bool UIEventRouter::IsKeyPressed(int keyCode) const {
    auto it = m_keyStates.find(keyCode);
    return it != m_keyStates.end() && it->second;
}

bool UIEventRouter::IsMouseButtonPressed(MouseButton button) const {
    return (m_mouseButtonStates & (1 << static_cast<int>(button))) != 0;
}

void UIEventRouter::GetMousePosition(int& x, int& y) const {
    x = m_mouseX;
    y = m_mouseY;
}

void UIEventRouter::QueueEvent(DOMElement* target, const UIEvent& event) {
    m_eventQueue.push({target, event});
}

void UIEventRouter::ProcessEventQueue() {
    while (!m_eventQueue.empty()) {
        auto& [target, event] = m_eventQueue.front();
        UIEvent e = event;
        DispatchEvent(target, e);
        m_eventQueue.pop();
    }
}

void UIEventRouter::ClearEventQueue() {
    while (!m_eventQueue.empty()) {
        m_eventQueue.pop();
    }
}

void UIEventRouter::GetEventStats(int& dispatched, int& handled, int& queued) const {
    dispatched = m_eventsDispatched;
    handled = m_eventsHandled;
    queued = static_cast<int>(m_eventQueue.size());
}

DOMElement* UIEventRouter::FindElementAtPoint(UIWindow* window, int x, int y) {
    if (!window) return nullptr;

    DOMElement* root = window->GetRootElement();
    if (!root) return nullptr;

    // Convert to window-local coordinates
    int localX = x - window->GetX();
    int localY = y - window->GetY();

    // Recursive hit test
    std::function<DOMElement*(DOMElement*)> hitTest = [&](DOMElement* element) -> DOMElement* {
        if (!element->isVisible) return nullptr;
        if (element->computedStyle.pointerEvents == StyleProperties::PointerEvents::None) return nullptr;

        // Check bounds
        float ex = element->layout.x;
        float ey = element->layout.y;
        float ew = element->layout.width;
        float eh = element->layout.height;

        if (localX < ex || localX >= ex + ew || localY < ey || localY >= ey + eh) {
            return nullptr;
        }

        // Check children (in reverse order for z-order)
        for (auto it = element->children.rbegin(); it != element->children.rend(); ++it) {
            DOMElement* hit = hitTest(it->get());
            if (hit) return hit;
        }

        return element;
    };

    return hitTest(root);
}

void UIEventRouter::BuildEventPath(DOMElement* target, std::vector<DOMElement*>& path) {
    DOMElement* current = target->parent;
    while (current) {
        path.push_back(current);
        current = current->parent;
    }
}

void UIEventRouter::TriggerEnterLeaveEvents(DOMElement* oldElement, DOMElement* newElement, int x, int y) {
    double timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Trigger mouseleave on old element
    if (oldElement) {
        oldElement->isHovered = false;

        UIEvent leaveEvent;
        leaveEvent.type = "mouseleave";
        leaveEvent.target = oldElement;
        leaveEvent.relatedTarget = newElement;
        leaveEvent.clientX = x;
        leaveEvent.clientY = y;
        leaveEvent.bubbles = false;
        leaveEvent.timestamp = timestamp;
        DispatchEvent(oldElement, leaveEvent);

        UIEvent outEvent;
        outEvent.type = "mouseout";
        outEvent.target = oldElement;
        outEvent.relatedTarget = newElement;
        outEvent.clientX = x;
        outEvent.clientY = y;
        outEvent.bubbles = true;
        outEvent.timestamp = timestamp;
        DispatchEvent(oldElement, outEvent);
    }

    // Trigger mouseenter on new element
    if (newElement) {
        newElement->isHovered = true;

        UIEvent enterEvent;
        enterEvent.type = "mouseenter";
        enterEvent.target = newElement;
        enterEvent.relatedTarget = oldElement;
        enterEvent.clientX = x;
        enterEvent.clientY = y;
        enterEvent.bubbles = false;
        enterEvent.timestamp = timestamp;
        DispatchEvent(newElement, enterEvent);

        UIEvent overEvent;
        overEvent.type = "mouseover";
        overEvent.target = newElement;
        overEvent.relatedTarget = oldElement;
        overEvent.clientX = x;
        overEvent.clientY = y;
        overEvent.bubbles = true;
        overEvent.timestamp = timestamp;
        DispatchEvent(newElement, overEvent);
    }
}

std::vector<DOMElement*> UIEventRouter::GetFocusableElements(DOMElement* root) {
    std::vector<DOMElement*> result;

    std::function<void(DOMElement*)> collect = [&](DOMElement* element) {
        if (IsFocusable(element)) {
            result.push_back(element);
        }
        for (auto& child : element->children) {
            collect(child.get());
        }
    };

    if (root) {
        collect(root);
    }

    // Sort by tab index
    std::sort(result.begin(), result.end(), [this](DOMElement* a, DOMElement* b) {
        int tabA = GetTabIndex(a);
        int tabB = GetTabIndex(b);
        if (tabA == tabB) return false;
        if (tabA == 0) return false;
        if (tabB == 0) return true;
        return tabA < tabB;
    });

    return result;
}

DOMElement* UIEventRouter::FindNextFocusable(DOMElement* current, FocusDirection direction) {
    DOMElement* root = m_gamepadNavRoot;
    if (!root && current) {
        root = current;
        while (root->parent) {
            root = root->parent;
        }
    }

    auto focusables = GetFocusableElements(root);
    if (focusables.empty()) return nullptr;

    auto it = std::find(focusables.begin(), focusables.end(), current);
    size_t currentIdx = (it != focusables.end()) ? std::distance(focusables.begin(), it) : 0;

    switch (direction) {
        case FocusDirection::Next:
            return focusables[(currentIdx + 1) % focusables.size()];

        case FocusDirection::Previous:
            return focusables[(currentIdx + focusables.size() - 1) % focusables.size()];

        case FocusDirection::Up:
        case FocusDirection::Down:
        case FocusDirection::Left:
        case FocusDirection::Right: {
            // Spatial navigation - find nearest in direction
            if (!current) return focusables[0];

            float cx = current->layout.x + current->layout.width / 2;
            float cy = current->layout.y + current->layout.height / 2;

            DOMElement* best = nullptr;
            float bestDist = std::numeric_limits<float>::max();

            for (auto* elem : focusables) {
                if (elem == current) continue;

                float ex = elem->layout.x + elem->layout.width / 2;
                float ey = elem->layout.y + elem->layout.height / 2;

                bool inDirection = false;
                switch (direction) {
                    case FocusDirection::Up: inDirection = (ey < cy); break;
                    case FocusDirection::Down: inDirection = (ey > cy); break;
                    case FocusDirection::Left: inDirection = (ex < cx); break;
                    case FocusDirection::Right: inDirection = (ex > cx); break;
                    default: break;
                }

                if (inDirection) {
                    float dist = (ex - cx) * (ex - cx) + (ey - cy) * (ey - cy);
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = elem;
                    }
                }
            }

            return best ? best : current;
        }
    }

    return nullptr;
}

void UIEventRouter::UpdateKeyState(int keyCode, bool pressed) {
    m_keyStates[keyCode] = pressed;
}

void UIEventRouter::UpdateMouseButtonState(int button, bool pressed) {
    if (pressed) {
        m_mouseButtonStates |= (1 << button);
    } else {
        m_mouseButtonStates &= ~(1 << button);
    }
}

} // namespace UI
} // namespace Engine
