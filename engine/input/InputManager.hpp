#pragma once

#include <glm/glm.hpp>
#include <array>
#include <functional>

struct GLFWwindow;

namespace Nova {

/**
 * @brief Key codes (matching GLFW)
 */
enum class Key {
    // Letters
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Special keys
    Space = 32,
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    // Modifiers
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,

    MaxKeys = 400
};

/**
 * @brief Mouse button codes
 */
enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    MaxButtons = 8
};

/**
 * @brief Input state for a button
 */
struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

/**
 * @brief Modern input handling system
 */
class InputManager {
public:
    InputManager();
    ~InputManager();

    /**
     * @brief Initialize with window handle
     */
    void Initialize(GLFWwindow* window);

    /**
     * @brief Update input states (call once per frame)
     */
    void Update();

    // Keyboard
    bool IsKeyDown(Key key) const;
    bool IsKeyPressed(Key key) const;
    bool IsKeyReleased(Key key) const;
    bool IsAnyKeyDown() const;

    // Mouse buttons
    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;

    // Mouse position
    glm::vec2 GetMousePosition() const;
    glm::vec2 GetMouseDelta() const;
    float GetScrollDelta() const;

    // Mouse control
    void SetMousePosition(const glm::vec2& position);
    void SetCursorLocked(bool locked);
    void SetCursorVisible(bool visible);
    bool IsCursorLocked() const { return m_cursorLocked; }

    // Callbacks for custom handling
    using KeyCallback = std::function<void(Key, bool)>;
    using MouseButtonCallback = std::function<void(MouseButton, bool)>;
    using MouseMoveCallback = std::function<void(const glm::vec2&)>;
    using ScrollCallback = std::function<void(float)>;

    void SetKeyCallback(KeyCallback callback) { m_keyCallback = std::move(callback); }
    void SetMouseButtonCallback(MouseButtonCallback callback) { m_mouseButtonCallback = std::move(callback); }
    void SetMouseMoveCallback(MouseMoveCallback callback) { m_mouseMoveCallback = std::move(callback); }
    void SetScrollCallback(ScrollCallback callback) { m_scrollCallback = std::move(callback); }

private:
    static void KeyCallbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallbackGLFW(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallbackGLFW(GLFWwindow* window, double x, double y);
    static void ScrollCallbackGLFW(GLFWwindow* window, double x, double y);

    GLFWwindow* m_window = nullptr;

    std::array<ButtonState, static_cast<size_t>(Key::MaxKeys)> m_keys;
    std::array<ButtonState, static_cast<size_t>(MouseButton::MaxButtons)> m_mouseButtons;

    glm::vec2 m_mousePosition{0};
    glm::vec2 m_lastMousePosition{0};
    glm::vec2 m_mouseDelta{0};
    float m_scrollDelta = 0.0f;

    bool m_cursorLocked = false;
    bool m_firstMouse = true;

    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    ScrollCallback m_scrollCallback;
};

} // namespace Nova
