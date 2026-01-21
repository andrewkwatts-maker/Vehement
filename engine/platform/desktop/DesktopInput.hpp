#pragma once

/**
 * @file DesktopInput.hpp
 * @brief Unified desktop input handling (keyboard, mouse, gamepad)
 *
 * Provides a platform-agnostic input interface for desktop platforms
 * (Windows, Linux, macOS). Works with GLFW for input handling.
 */

#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <memory>

struct GLFWwindow;

namespace Nova {

// =============================================================================
// Key Codes
// =============================================================================

/**
 * @brief Keyboard key codes (GLFW-compatible values)
 */
enum class Key : int {
    // Unknown/Invalid
    Unknown = -1,

    // Printable keys
    Space = 32,
    Apostrophe = 39,  // '
    Comma = 44,       // ,
    Minus = 45,       // -
    Period = 46,      // .
    Slash = 47,       // /

    // Numbers
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Semicolon = 59,   // ;
    Equal = 61,       // =

    // Letters
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    LeftBracket = 91,  // [
    Backslash = 92,    // '\'
    RightBracket = 93, // ]
    GraveAccent = 96,  // `

    // Non-US keys
    World1 = 161,
    World2 = 162,

    // Function keys
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

    // Function keys
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,

    // Keypad
    KP0 = 320, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,

    // Modifier keys
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348,

    MaxKey = 512
};

// =============================================================================
// Mouse Buttons
// =============================================================================

/**
 * @brief Mouse button codes
 */
enum class MouseButton : int {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,

    MaxButton = 8
};

// =============================================================================
// Cursor Modes
// =============================================================================

/**
 * @brief Cursor display/capture modes
 */
enum class CursorMode : int {
    Normal = 0,    // Cursor visible and free to move
    Hidden = 1,    // Cursor hidden but free to move
    Disabled = 2,  // Cursor hidden and locked (for FPS games)
    Captured = 3   // Cursor captured within window bounds
};

/**
 * @brief Standard cursor shapes
 */
enum class CursorShape : int {
    Arrow = 0,
    IBeam,
    Crosshair,
    Hand,
    HResize,
    VResize,
    ResizeNWSE,
    ResizeNESW,
    ResizeAll,
    NotAllowed,

    Count
};

// =============================================================================
// Modifier Keys
// =============================================================================

/**
 * @brief Modifier key flags
 */
enum class ModifierFlags : uint8_t {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,    // Windows/Command key
    CapsLock = 1 << 4,
    NumLock = 1 << 5
};

inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) noexcept {
    return static_cast<ModifierFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline ModifierFlags operator&(ModifierFlags a, ModifierFlags b) noexcept {
    return static_cast<ModifierFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasModifier(ModifierFlags flags, ModifierFlags mod) noexcept {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(mod)) != 0;
}

// =============================================================================
// Gamepad Support
// =============================================================================

/**
 * @brief Gamepad buttons (Xbox-style layout)
 */
enum class GamepadButton : int {
    A = 0,              // Cross on PlayStation
    B = 1,              // Circle
    X = 2,              // Square
    Y = 3,              // Triangle
    LeftBumper = 4,     // L1
    RightBumper = 5,    // R1
    Back = 6,           // Select/Share
    Start = 7,          // Options
    Guide = 8,          // Home/PS button
    LeftThumb = 9,      // L3
    RightThumb = 10,    // R3
    DPadUp = 11,
    DPadRight = 12,
    DPadDown = 13,
    DPadLeft = 14,

    MaxButton = 15
};

/**
 * @brief Gamepad axes
 */
enum class GamepadAxis : int {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5,

    MaxAxis = 6
};

/**
 * @brief Gamepad state
 */
struct GamepadState {
    bool connected = false;
    std::string name;
    std::array<bool, static_cast<size_t>(GamepadButton::MaxButton)> buttons{};
    std::array<float, static_cast<size_t>(GamepadAxis::MaxAxis)> axes{};

    [[nodiscard]] bool IsButtonDown(GamepadButton button) const noexcept {
        return buttons[static_cast<size_t>(button)];
    }

    [[nodiscard]] float GetAxis(GamepadAxis axis) const noexcept {
        return axes[static_cast<size_t>(axis)];
    }

    [[nodiscard]] glm::vec2 GetLeftStick() const noexcept {
        return {axes[0], axes[1]};
    }

    [[nodiscard]] glm::vec2 GetRightStick() const noexcept {
        return {axes[2], axes[3]};
    }
};

// =============================================================================
// Text Input
// =============================================================================

/**
 * @brief Text input event data
 */
struct TextInputEvent {
    uint32_t codepoint;     // Unicode codepoint
    ModifierFlags mods;     // Active modifiers
};

// =============================================================================
// Desktop Input Class
// =============================================================================

/**
 * @brief Unified desktop input handler
 *
 * Provides a clean interface for keyboard, mouse, and gamepad input.
 * Works across Windows, Linux, and macOS via GLFW.
 */
class DesktopInput {
public:
    DesktopInput();
    ~DesktopInput();

    // Non-copyable, non-movable
    DesktopInput(const DesktopInput&) = delete;
    DesktopInput& operator=(const DesktopInput&) = delete;

    /**
     * @brief Initialize with GLFW window
     * @param window GLFW window handle
     */
    void Initialize(GLFWwindow* window);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update input state (call at start of each frame)
     */
    void Update();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_window != nullptr; }

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------

    /**
     * @brief Check if key is currently held down
     */
    [[nodiscard]] bool IsKeyDown(Key key) const noexcept;

    /**
     * @brief Check if key was pressed this frame
     */
    [[nodiscard]] bool WasKeyPressed(Key key) const noexcept;

    /**
     * @brief Check if key was released this frame
     */
    [[nodiscard]] bool WasKeyReleased(Key key) const noexcept;

    /**
     * @brief Check if any key is currently down
     */
    [[nodiscard]] bool IsAnyKeyDown() const noexcept;

    /**
     * @brief Get current modifier flags
     */
    [[nodiscard]] ModifierFlags GetModifiers() const noexcept;

    /**
     * @brief Check if shift is held
     */
    [[nodiscard]] bool IsShiftDown() const noexcept;

    /**
     * @brief Check if control is held
     */
    [[nodiscard]] bool IsCtrlDown() const noexcept;

    /**
     * @brief Check if alt is held
     */
    [[nodiscard]] bool IsAltDown() const noexcept;

    /**
     * @brief Check if super/command is held
     */
    [[nodiscard]] bool IsSuperDown() const noexcept;

    /**
     * @brief Get name of key
     */
    [[nodiscard]] static const char* GetKeyName(Key key) noexcept;

    // -------------------------------------------------------------------------
    // Mouse
    // -------------------------------------------------------------------------

    /**
     * @brief Get current mouse position in window coordinates
     */
    [[nodiscard]] glm::vec2 GetMousePosition() const noexcept;

    /**
     * @brief Get mouse movement since last frame
     */
    [[nodiscard]] glm::vec2 GetMouseDelta() const noexcept;

    /**
     * @brief Check if mouse button is held down
     */
    [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const noexcept;

    /**
     * @brief Check if mouse button was pressed this frame
     */
    [[nodiscard]] bool WasMouseButtonPressed(MouseButton button) const noexcept;

    /**
     * @brief Check if mouse button was released this frame
     */
    [[nodiscard]] bool WasMouseButtonReleased(MouseButton button) const noexcept;

    /**
     * @brief Get vertical scroll delta (positive = up)
     */
    [[nodiscard]] float GetScrollDelta() const noexcept;

    /**
     * @brief Get horizontal scroll delta (positive = right)
     */
    [[nodiscard]] float GetScrollDeltaX() const noexcept;

    /**
     * @brief Set mouse position
     */
    void SetMousePosition(const glm::vec2& pos);

    // -------------------------------------------------------------------------
    // Cursor
    // -------------------------------------------------------------------------

    /**
     * @brief Set cursor display mode
     */
    void SetCursorMode(CursorMode mode);

    /**
     * @brief Get current cursor mode
     */
    [[nodiscard]] CursorMode GetCursorMode() const noexcept { return m_cursorMode; }

    /**
     * @brief Set cursor shape
     */
    void SetCursorShape(CursorShape shape);

    /**
     * @brief Hide the cursor
     */
    void HideCursor();

    /**
     * @brief Show the cursor
     */
    void ShowCursor();

    /**
     * @brief Check if cursor is visible
     */
    [[nodiscard]] bool IsCursorVisible() const noexcept { return m_cursorVisible; }

    // -------------------------------------------------------------------------
    // Gamepad
    // -------------------------------------------------------------------------

    /**
     * @brief Get number of connected gamepads
     */
    [[nodiscard]] int GetGamepadCount() const noexcept;

    /**
     * @brief Check if gamepad is connected
     * @param index Gamepad index (0-15)
     */
    [[nodiscard]] bool IsGamepadConnected(int index) const noexcept;

    /**
     * @brief Get gamepad state
     * @param index Gamepad index
     */
    [[nodiscard]] const GamepadState& GetGamepad(int index) const noexcept;

    /**
     * @brief Get first connected gamepad state
     */
    [[nodiscard]] const GamepadState* GetFirstConnectedGamepad() const noexcept;

    /**
     * @brief Check if gamepad button is down
     */
    [[nodiscard]] bool IsGamepadButtonDown(int gamepad, GamepadButton button) const noexcept;

    /**
     * @brief Get gamepad axis value
     */
    [[nodiscard]] float GetGamepadAxis(int gamepad, GamepadAxis axis) const noexcept;

    /**
     * @brief Set gamepad deadzone
     */
    void SetDeadzone(float deadzone) noexcept { m_deadzone = deadzone; }

    /**
     * @brief Get gamepad deadzone
     */
    [[nodiscard]] float GetDeadzone() const noexcept { return m_deadzone; }

    // -------------------------------------------------------------------------
    // Text Input
    // -------------------------------------------------------------------------

    /**
     * @brief Enable text input mode
     */
    void EnableTextInput();

    /**
     * @brief Disable text input mode
     */
    void DisableTextInput();

    /**
     * @brief Check if text input is enabled
     */
    [[nodiscard]] bool IsTextInputEnabled() const noexcept { return m_textInputEnabled; }

    /**
     * @brief Get text input events this frame
     */
    [[nodiscard]] const std::vector<TextInputEvent>& GetTextInput() const noexcept { return m_textInput; }

    // -------------------------------------------------------------------------
    // Axis Helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Get 1D axis from two keys
     * @return -1 if negative, +1 if positive, 0 if neither/both
     */
    [[nodiscard]] float GetAxis(Key negative, Key positive) const noexcept;

    /**
     * @brief Get 2D movement vector from WASD or arrow keys
     * @param wasd Use WASD (true) or arrows (false)
     * @return Normalized direction vector
     */
    [[nodiscard]] glm::vec2 GetMovementVector(bool wasd = true) const noexcept;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using KeyCallback = std::function<void(Key key, int scancode, bool pressed, ModifierFlags mods)>;
    using MouseButtonCallback = std::function<void(MouseButton button, bool pressed, ModifierFlags mods)>;
    using MouseMoveCallback = std::function<void(const glm::vec2& position)>;
    using ScrollCallback = std::function<void(float xoffset, float yoffset)>;
    using CharCallback = std::function<void(uint32_t codepoint)>;
    using GamepadCallback = std::function<void(int gamepad, bool connected)>;

    void SetKeyCallback(KeyCallback cb) { m_keyCallback = std::move(cb); }
    void SetMouseButtonCallback(MouseButtonCallback cb) { m_mouseButtonCallback = std::move(cb); }
    void SetMouseMoveCallback(MouseMoveCallback cb) { m_mouseMoveCallback = std::move(cb); }
    void SetScrollCallback(ScrollCallback cb) { m_scrollCallback = std::move(cb); }
    void SetCharCallback(CharCallback cb) { m_charCallback = std::move(cb); }
    void SetGamepadCallback(GamepadCallback cb) { m_gamepadCallback = std::move(cb); }

private:
    // Button state tracking
    struct ButtonState {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    // GLFW callbacks
    static void KeyCallbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallbackGLFW(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallbackGLFW(GLFWwindow* window, double x, double y);
    static void ScrollCallbackGLFW(GLFWwindow* window, double x, double y);
    static void CharCallbackGLFW(GLFWwindow* window, unsigned int codepoint);
    static void JoystickCallbackGLFW(int jid, int event);

    void UpdateGamepads();
    float ApplyDeadzone(float value) const noexcept;

    GLFWwindow* m_window = nullptr;

    // Keyboard state
    std::array<ButtonState, static_cast<size_t>(Key::MaxKey)> m_keys{};
    std::vector<int> m_changedKeys;
    int m_activeKeyCount = 0;

    // Mouse state
    std::array<ButtonState, static_cast<size_t>(MouseButton::MaxButton)> m_mouseButtons{};
    std::vector<int> m_changedMouseButtons;
    glm::vec2 m_mousePosition{0.0f};
    glm::vec2 m_lastMousePosition{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    float m_scrollDeltaX = 0.0f;
    float m_scrollDeltaY = 0.0f;
    bool m_firstMouseMove = true;

    // Cursor state
    CursorMode m_cursorMode = CursorMode::Normal;
    CursorShape m_cursorShape = CursorShape::Arrow;
    bool m_cursorVisible = true;

    // Gamepad state
    static constexpr int MAX_GAMEPADS = 16;
    std::array<GamepadState, MAX_GAMEPADS> m_gamepads{};
    float m_deadzone = 0.15f;

    // Text input
    bool m_textInputEnabled = false;
    std::vector<TextInputEvent> m_textInput;

    // User callbacks
    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    ScrollCallback m_scrollCallback;
    CharCallback m_charCallback;
    GamepadCallback m_gamepadCallback;

    // Static instance for GLFW callbacks
    static DesktopInput* s_instance;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert Key enum to string name
 */
inline const char* KeyToString(Key key) noexcept {
    switch (key) {
        case Key::Space: return "Space";
        case Key::Apostrophe: return "'";
        case Key::Comma: return ",";
        case Key::Minus: return "-";
        case Key::Period: return ".";
        case Key::Slash: return "/";
        case Key::Num0: return "0"; case Key::Num1: return "1"; case Key::Num2: return "2";
        case Key::Num3: return "3"; case Key::Num4: return "4"; case Key::Num5: return "5";
        case Key::Num6: return "6"; case Key::Num7: return "7"; case Key::Num8: return "8";
        case Key::Num9: return "9";
        case Key::Semicolon: return ";";
        case Key::Equal: return "=";
        case Key::A: return "A"; case Key::B: return "B"; case Key::C: return "C";
        case Key::D: return "D"; case Key::E: return "E"; case Key::F: return "F";
        case Key::G: return "G"; case Key::H: return "H"; case Key::I: return "I";
        case Key::J: return "J"; case Key::K: return "K"; case Key::L: return "L";
        case Key::M: return "M"; case Key::N: return "N"; case Key::O: return "O";
        case Key::P: return "P"; case Key::Q: return "Q"; case Key::R: return "R";
        case Key::S: return "S"; case Key::T: return "T"; case Key::U: return "U";
        case Key::V: return "V"; case Key::W: return "W"; case Key::X: return "X";
        case Key::Y: return "Y"; case Key::Z: return "Z";
        case Key::LeftBracket: return "[";
        case Key::Backslash: return "\\";
        case Key::RightBracket: return "]";
        case Key::GraveAccent: return "`";
        case Key::Escape: return "Escape";
        case Key::Enter: return "Enter";
        case Key::Tab: return "Tab";
        case Key::Backspace: return "Backspace";
        case Key::Insert: return "Insert";
        case Key::Delete: return "Delete";
        case Key::Right: return "Right";
        case Key::Left: return "Left";
        case Key::Down: return "Down";
        case Key::Up: return "Up";
        case Key::PageUp: return "PageUp";
        case Key::PageDown: return "PageDown";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::CapsLock: return "CapsLock";
        case Key::ScrollLock: return "ScrollLock";
        case Key::NumLock: return "NumLock";
        case Key::PrintScreen: return "PrintScreen";
        case Key::Pause: return "Pause";
        case Key::F1: return "F1"; case Key::F2: return "F2"; case Key::F3: return "F3";
        case Key::F4: return "F4"; case Key::F5: return "F5"; case Key::F6: return "F6";
        case Key::F7: return "F7"; case Key::F8: return "F8"; case Key::F9: return "F9";
        case Key::F10: return "F10"; case Key::F11: return "F11"; case Key::F12: return "F12";
        case Key::LeftShift: return "LeftShift";
        case Key::LeftControl: return "LeftControl";
        case Key::LeftAlt: return "LeftAlt";
        case Key::LeftSuper: return "LeftSuper";
        case Key::RightShift: return "RightShift";
        case Key::RightControl: return "RightControl";
        case Key::RightAlt: return "RightAlt";
        case Key::RightSuper: return "RightSuper";
        case Key::Menu: return "Menu";
        default: return "Unknown";
    }
}

/**
 * @brief Convert MouseButton to string name
 */
inline const char* MouseButtonToString(MouseButton button) noexcept {
    switch (button) {
        case MouseButton::Left: return "Left";
        case MouseButton::Right: return "Right";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Button4: return "Button4";
        case MouseButton::Button5: return "Button5";
        case MouseButton::Button6: return "Button6";
        case MouseButton::Button7: return "Button7";
        case MouseButton::Button8: return "Button8";
        default: return "Unknown";
    }
}

/**
 * @brief Check if key is a modifier key
 */
inline bool IsModifierKey(Key key) noexcept {
    return key == Key::LeftShift || key == Key::RightShift ||
           key == Key::LeftControl || key == Key::RightControl ||
           key == Key::LeftAlt || key == Key::RightAlt ||
           key == Key::LeftSuper || key == Key::RightSuper;
}

/**
 * @brief Check if key produces printable character
 */
inline bool IsPrintableKey(Key key) noexcept {
    int k = static_cast<int>(key);
    return (k >= 32 && k <= 126);
}

/**
 * @brief Check if key is a function key (F1-F25)
 */
inline bool IsFunctionKey(Key key) noexcept {
    int k = static_cast<int>(key);
    return (k >= static_cast<int>(Key::F1) && k <= static_cast<int>(Key::F25));
}

/**
 * @brief Check if key is a numpad key
 */
inline bool IsNumpadKey(Key key) noexcept {
    int k = static_cast<int>(key);
    return (k >= static_cast<int>(Key::KP0) && k <= static_cast<int>(Key::KPEqual));
}

} // namespace Nova
