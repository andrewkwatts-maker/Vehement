#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <queue>
#include <memory>

namespace Engine {
namespace UI {

// Forward declarations
class UIWindow;
class DOMElement;

/**
 * @brief Mouse button enumeration
 */
enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4
};

/**
 * @brief Keyboard modifier flags
 */
enum class KeyModifier {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,
    CapsLock = 1 << 4,
    NumLock = 1 << 5
};

inline KeyModifier operator|(KeyModifier a, KeyModifier b) {
    return static_cast<KeyModifier>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool operator&(KeyModifier a, KeyModifier b) {
    return (static_cast<int>(a) & static_cast<int>(b)) != 0;
}

/**
 * @brief Touch phase
 */
enum class TouchPhase {
    Begin,
    Move,
    End,
    Cancel
};

/**
 * @brief Gamepad button enumeration
 */
enum class GamepadButton {
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    LeftBumper = 4,
    RightBumper = 5,
    Back = 6,
    Start = 7,
    Guide = 8,
    LeftStick = 9,
    RightStick = 10,
    DPadUp = 11,
    DPadRight = 12,
    DPadDown = 13,
    DPadLeft = 14
};

/**
 * @brief UI Event structure
 */
struct UIEvent {
    std::string type;
    DOMElement* target = nullptr;
    DOMElement* currentTarget = nullptr;
    bool bubbles = true;
    bool cancelable = true;
    bool defaultPrevented = false;
    bool propagationStopped = false;
    bool immediatePropagationStopped = false;
    double timestamp = 0;

    // Mouse event data
    int clientX = 0, clientY = 0;
    int screenX = 0, screenY = 0;
    int offsetX = 0, offsetY = 0;
    int movementX = 0, movementY = 0;
    MouseButton button = MouseButton::Left;
    int buttons = 0;
    float wheelDeltaX = 0, wheelDeltaY = 0;

    // Keyboard event data
    int keyCode = 0;
    std::string key;
    std::string code;
    KeyModifier modifiers = KeyModifier::None;
    bool repeat = false;

    // Touch event data
    int touchId = 0;
    TouchPhase touchPhase = TouchPhase::Begin;
    float pressure = 1.0f;
    float radiusX = 0, radiusY = 0;

    // Gamepad event data
    GamepadButton gamepadButton = GamepadButton::A;
    int gamepadIndex = 0;

    // Focus event data
    DOMElement* relatedTarget = nullptr;

    void PreventDefault() { if (cancelable) defaultPrevented = true; }
    void StopPropagation() { propagationStopped = true; }
    void StopImmediatePropagation() { immediatePropagationStopped = true; propagationStopped = true; }
};

/**
 * @brief Event listener entry
 */
struct EventListener {
    std::string type;
    std::function<void(UIEvent&)> handler;
    bool capture = false;
    bool once = false;
    bool passive = false;
    int priority = 0;
};

/**
 * @brief Focus navigation direction
 */
enum class FocusDirection {
    Next,
    Previous,
    Up,
    Down,
    Left,
    Right
};

/**
 * @brief Touch point for multi-touch
 */
struct TouchPoint {
    int id;
    int x, y;
    int startX, startY;
    TouchPhase phase;
    double startTime;
    DOMElement* target = nullptr;
};

/**
 * @brief UI Event Router
 *
 * Handles mouse, keyboard, touch, and gamepad events.
 * Provides focus management and event bubbling.
 */
class UIEventRouter {
public:
    UIEventRouter();
    ~UIEventRouter();

    /**
     * @brief Initialize the event router
     */
    void Initialize();

    /**
     * @brief Shutdown the event router
     */
    void Shutdown();

    /**
     * @brief Update event system
     */
    void Update(float deltaTime);

    // Event Routing

    /**
     * @brief Route mouse event to window
     * @return true if event was handled
     */
    bool RouteMouseEvent(UIWindow* window, int x, int y, int button, bool pressed);

    /**
     * @brief Route mouse move event
     */
    bool RouteMouseMoveEvent(UIWindow* window, int x, int y);

    /**
     * @brief Route scroll event
     */
    bool RouteScrollEvent(UIWindow* window, int x, int y, float scrollX, float scrollY);

    /**
     * @brief Route keyboard event
     */
    bool RouteKeyEvent(UIWindow* window, int keyCode, bool pressed, int modifiers);

    /**
     * @brief Route text input event
     */
    bool RouteTextEvent(UIWindow* window, const std::string& text);

    /**
     * @brief Route touch event
     */
    bool RouteTouchEvent(UIWindow* window, int touchId, int x, int y, int phase);

    /**
     * @brief Route gamepad event
     */
    bool RouteGamepadEvent(UIWindow* window, int button, bool pressed);

    // Event Dispatching

    /**
     * @brief Dispatch event to element
     * @param element Target element
     * @param event Event to dispatch
     * @return true if event was handled
     */
    bool DispatchEvent(DOMElement* element, UIEvent& event);

    /**
     * @brief Dispatch custom event
     */
    bool DispatchCustomEvent(DOMElement* element, const std::string& type,
                            const std::unordered_map<std::string, std::string>& detail = {});

    // Event Listeners

    /**
     * @brief Add event listener to element
     * @param element Target element
     * @param type Event type
     * @param handler Event handler
     * @param capture Use capture phase
     * @return Listener ID
     */
    int AddEventListener(DOMElement* element, const std::string& type,
                        std::function<void(UIEvent&)> handler, bool capture = false);

    /**
     * @brief Add event listener with options
     */
    int AddEventListener(DOMElement* element, const EventListener& listener);

    /**
     * @brief Remove event listener
     */
    void RemoveEventListener(int listenerId);

    /**
     * @brief Remove all listeners for element
     */
    void RemoveAllListeners(DOMElement* element);

    /**
     * @brief Remove all listeners for event type
     */
    void RemoveListenersByType(DOMElement* element, const std::string& type);

    // Focus Management

    /**
     * @brief Set focused element
     */
    void SetFocus(DOMElement* element);

    /**
     * @brief Get focused element
     */
    DOMElement* GetFocusedElement() const { return m_focusedElement; }

    /**
     * @brief Clear focus
     */
    void ClearFocus();

    /**
     * @brief Move focus in direction
     */
    void MoveFocus(FocusDirection direction);

    /**
     * @brief Focus next element (tab order)
     */
    void FocusNext();

    /**
     * @brief Focus previous element (tab order)
     */
    void FocusPrevious();

    /**
     * @brief Check if element is focusable
     */
    bool IsFocusable(DOMElement* element) const;

    /**
     * @brief Set tab index for element
     */
    void SetTabIndex(DOMElement* element, int tabIndex);

    /**
     * @brief Get tab index for element
     */
    int GetTabIndex(DOMElement* element) const;

    // Hover State

    /**
     * @brief Get hovered element
     */
    DOMElement* GetHoveredElement() const { return m_hoveredElement; }

    /**
     * @brief Update hover state for position
     */
    void UpdateHoverState(UIWindow* window, int x, int y);

    // Capture

    /**
     * @brief Set mouse capture to element
     */
    void SetCapture(DOMElement* element);

    /**
     * @brief Release mouse capture
     */
    void ReleaseCapture();

    /**
     * @brief Get captured element
     */
    DOMElement* GetCapturedElement() const { return m_capturedElement; }

    // Touch

    /**
     * @brief Get active touch points
     */
    const std::vector<TouchPoint>& GetTouchPoints() const { return m_touchPoints; }

    /**
     * @brief Get touch point by ID
     */
    TouchPoint* GetTouchPoint(int id);

    // Gamepad Navigation

    /**
     * @brief Enable/disable gamepad navigation
     */
    void SetGamepadNavigationEnabled(bool enabled);

    /**
     * @brief Check if gamepad navigation is enabled
     */
    bool IsGamepadNavigationEnabled() const { return m_gamepadNavigationEnabled; }

    /**
     * @brief Set gamepad navigation root
     */
    void SetGamepadNavigationRoot(DOMElement* root);

    /**
     * @brief Handle gamepad navigation
     */
    void HandleGamepadNavigation(GamepadButton button);

    // Input State

    /**
     * @brief Check if key is pressed
     */
    bool IsKeyPressed(int keyCode) const;

    /**
     * @brief Check if mouse button is pressed
     */
    bool IsMouseButtonPressed(MouseButton button) const;

    /**
     * @brief Get current mouse position
     */
    void GetMousePosition(int& x, int& y) const;

    /**
     * @brief Get active modifiers
     */
    KeyModifier GetActiveModifiers() const { return m_activeModifiers; }

    // Event Queuing

    /**
     * @brief Queue event for later processing
     */
    void QueueEvent(DOMElement* target, const UIEvent& event);

    /**
     * @brief Process queued events
     */
    void ProcessEventQueue();

    /**
     * @brief Clear event queue
     */
    void ClearEventQueue();

    // Debugging

    /**
     * @brief Set event logging enabled
     */
    void SetEventLogging(bool enabled) { m_eventLogging = enabled; }

    /**
     * @brief Get event statistics
     */
    void GetEventStats(int& dispatched, int& handled, int& queued) const;

private:
    DOMElement* FindElementAtPoint(UIWindow* window, int x, int y);
    void BuildEventPath(DOMElement* target, std::vector<DOMElement*>& path);
    void TriggerEnterLeaveEvents(DOMElement* oldElement, DOMElement* newElement, int x, int y);
    std::vector<DOMElement*> GetFocusableElements(DOMElement* root);
    DOMElement* FindNextFocusable(DOMElement* current, FocusDirection direction);
    void UpdateKeyState(int keyCode, bool pressed);
    void UpdateMouseButtonState(int button, bool pressed);

    // Focus
    DOMElement* m_focusedElement = nullptr;
    DOMElement* m_hoveredElement = nullptr;
    DOMElement* m_capturedElement = nullptr;
    DOMElement* m_gamepadNavRoot = nullptr;

    // Event listeners
    std::unordered_map<DOMElement*, std::vector<EventListener>> m_listeners;
    int m_nextListenerId = 1;
    std::unordered_map<int, std::pair<DOMElement*, size_t>> m_listenerRegistry;

    // Tab indices
    std::unordered_map<DOMElement*, int> m_tabIndices;

    // Touch state
    std::vector<TouchPoint> m_touchPoints;

    // Input state
    std::unordered_map<int, bool> m_keyStates;
    int m_mouseButtonStates = 0;
    int m_mouseX = 0, m_mouseY = 0;
    KeyModifier m_activeModifiers = KeyModifier::None;

    // Event queue
    std::queue<std::pair<DOMElement*, UIEvent>> m_eventQueue;

    // Settings
    bool m_gamepadNavigationEnabled = false;
    bool m_eventLogging = false;
    bool m_initialized = false;

    // Stats
    int m_eventsDispatched = 0;
    int m_eventsHandled = 0;
};

} // namespace UI
} // namespace Engine
