#pragma once

#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace Nova {

/**
 * @brief Key codes (matching GLFW key values)
 */
enum class Key : int {
    // Letters
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Special keys
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,
    Semicolon = 59,
    Equal = 61,
    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,
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
    Menu = 348,

    MaxKeys = 400
};

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
    MaxButtons = 8
};

/**
 * @brief Modifier key flags for checking combined modifier states
 */
enum class ModifierFlags : uint8_t {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3
};

inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) noexcept {
    return static_cast<ModifierFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline ModifierFlags operator&(ModifierFlags a, ModifierFlags b) noexcept {
    return static_cast<ModifierFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasFlag(ModifierFlags flags, ModifierFlags flag) noexcept {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

/**
 * @brief Input binding for action mapping - can be a key or mouse button
 */
struct InputBinding {
    enum class Type : uint8_t { Key, MouseButton };

    Type type = Type::Key;
    int code = 0;
    ModifierFlags requiredModifiers = ModifierFlags::None;

    static InputBinding FromKey(Key key, ModifierFlags mods = ModifierFlags::None) noexcept {
        return { Type::Key, static_cast<int>(key), mods };
    }

    static InputBinding FromMouseButton(MouseButton button, ModifierFlags mods = ModifierFlags::None) noexcept {
        return { Type::MouseButton, static_cast<int>(button), mods };
    }
};

/**
 * @brief Modern input handling system with action mapping support
 *
 * Provides low-level input state queries and high-level action mapping.
 * Thread-safe for callback registration; input state queries should be
 * called from the main thread only.
 */
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Non-copyable, non-movable (due to GLFW callback registration)
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    /**
     * @brief Initialize with window handle
     * @param window GLFW window to receive input from
     */
    void Initialize(GLFWwindow* window);

    /**
     * @brief Shutdown and cleanup (automatically called in destructor)
     */
    void Shutdown();

    /**
     * @brief Update input states - must be called once per frame before polling
     */
    void Update();

    /**
     * @brief Check if the input system is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_window != nullptr; }

    // -------------------------------------------------------------------------
    // Keyboard State Queries
    // -------------------------------------------------------------------------

    /** @brief Check if key is currently held down */
    [[nodiscard]] bool IsKeyDown(Key key) const noexcept;

    /** @brief Check if key was pressed this frame */
    [[nodiscard]] bool IsKeyPressed(Key key) const noexcept;

    /** @brief Check if key was released this frame */
    [[nodiscard]] bool IsKeyReleased(Key key) const noexcept;

    /** @brief Check if any key is currently held down */
    [[nodiscard]] bool IsAnyKeyDown() const noexcept;

    // -------------------------------------------------------------------------
    // Modifier Key Helpers
    // -------------------------------------------------------------------------

    /** @brief Check if any shift key is held */
    [[nodiscard]] bool IsShiftDown() const noexcept;

    /** @brief Check if any control key is held */
    [[nodiscard]] bool IsControlDown() const noexcept;

    /** @brief Check if any alt key is held */
    [[nodiscard]] bool IsAltDown() const noexcept;

    /** @brief Check if any super/command key is held */
    [[nodiscard]] bool IsSuperDown() const noexcept;

    /** @brief Get current modifier flags */
    [[nodiscard]] ModifierFlags GetModifiers() const noexcept;

    // -------------------------------------------------------------------------
    // Mouse Button State Queries
    // -------------------------------------------------------------------------

    /** @brief Check if mouse button is currently held down */
    [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const noexcept;

    /** @brief Check if mouse button was pressed this frame */
    [[nodiscard]] bool IsMouseButtonPressed(MouseButton button) const noexcept;

    /** @brief Check if mouse button was released this frame */
    [[nodiscard]] bool IsMouseButtonReleased(MouseButton button) const noexcept;

    // -------------------------------------------------------------------------
    // Mouse Position and Movement
    // -------------------------------------------------------------------------

    /** @brief Get current mouse position in window coordinates */
    [[nodiscard]] glm::vec2 GetMousePosition() const noexcept;

    /** @brief Get mouse movement since last frame */
    [[nodiscard]] glm::vec2 GetMouseDelta() const noexcept;

    /** @brief Get scroll wheel delta since last frame */
    [[nodiscard]] float GetScrollDelta() const noexcept;

    /** @brief Get horizontal scroll delta (for mice with horizontal scroll) */
    [[nodiscard]] float GetScrollDeltaX() const noexcept;

    // -------------------------------------------------------------------------
    // Mouse Control
    // -------------------------------------------------------------------------

    /** @brief Set mouse cursor position */
    void SetMousePosition(const glm::vec2& position);

    /** @brief Lock cursor to window center (for FPS-style camera) */
    void SetCursorLocked(bool locked);

    /** @brief Show or hide the cursor */
    void SetCursorVisible(bool visible);

    /** @brief Check if cursor is locked */
    [[nodiscard]] bool IsCursorLocked() const noexcept { return m_cursorLocked; }

    /** @brief Check if cursor is visible */
    [[nodiscard]] bool IsCursorVisible() const noexcept { return m_cursorVisible; }

    // -------------------------------------------------------------------------
    // Action Mapping System
    // -------------------------------------------------------------------------

    /**
     * @brief Register an action with one or more input bindings
     * @param actionName Unique name for the action (e.g., "Jump", "Fire")
     * @param bindings List of input bindings that trigger this action
     */
    void RegisterAction(const std::string& actionName, std::initializer_list<InputBinding> bindings);

    /**
     * @brief Register an action with a single binding
     */
    void RegisterAction(const std::string& actionName, InputBinding binding);

    /**
     * @brief Remove an action registration
     */
    void UnregisterAction(const std::string& actionName);

    /**
     * @brief Clear all registered actions
     */
    void ClearActions();

    /**
     * @brief Check if action is currently active (any bound input is down)
     */
    [[nodiscard]] bool IsActionDown(const std::string& actionName) const;

    /**
     * @brief Check if action was triggered this frame
     */
    [[nodiscard]] bool IsActionPressed(const std::string& actionName) const;

    /**
     * @brief Check if action was released this frame
     */
    [[nodiscard]] bool IsActionReleased(const std::string& actionName) const;

    // -------------------------------------------------------------------------
    // Axis Input Helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Get a 1D axis value from two keys (returns -1, 0, or 1)
     * @param negative Key for negative direction
     * @param positive Key for positive direction
     */
    [[nodiscard]] float GetAxis(Key negative, Key positive) const noexcept;

    /**
     * @brief Get a 2D movement vector from WASD or arrow keys
     * @param layout Use WASD (true) or arrow keys (false)
     * @return Normalized direction vector or zero vector if no input
     */
    [[nodiscard]] glm::vec2 GetMovementVector(bool useWASD = true) const noexcept;

    // -------------------------------------------------------------------------
    // Callbacks for Custom Handling
    // -------------------------------------------------------------------------

    using KeyCallback = std::function<void(Key key, bool pressed)>;
    using MouseButtonCallback = std::function<void(MouseButton button, bool pressed)>;
    using MouseMoveCallback = std::function<void(const glm::vec2& position)>;
    using ScrollCallback = std::function<void(float deltaY, float deltaX)>;

    void SetKeyCallback(KeyCallback callback) { m_keyCallback = std::move(callback); }
    void SetMouseButtonCallback(MouseButtonCallback callback) { m_mouseButtonCallback = std::move(callback); }
    void SetMouseMoveCallback(MouseMoveCallback callback) { m_mouseMoveCallback = std::move(callback); }
    void SetScrollCallback(ScrollCallback callback) { m_scrollCallback = std::move(callback); }

    void ClearCallbacks();

private:
    // Internal button state tracking
    struct ButtonState {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };

    // GLFW callback handlers
    static void KeyCallbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallbackGLFW(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallbackGLFW(GLFWwindow* window, double x, double y);
    static void ScrollCallbackGLFW(GLFWwindow* window, double x, double y);

    // Helper to check if binding is active
    [[nodiscard]] bool IsBindingDown(const InputBinding& binding) const noexcept;
    [[nodiscard]] bool IsBindingPressed(const InputBinding& binding) const noexcept;
    [[nodiscard]] bool IsBindingReleased(const InputBinding& binding) const noexcept;

    GLFWwindow* m_window = nullptr;

    // Input state arrays
    std::array<ButtonState, static_cast<size_t>(Key::MaxKeys)> m_keys{};
    std::array<ButtonState, static_cast<size_t>(MouseButton::MaxButtons)> m_mouseButtons{};

    // Track which keys/buttons changed for efficient clearing
    std::vector<int> m_changedKeys;
    std::vector<int> m_changedMouseButtons;

    // Fast check for any key down
    int m_activeKeyCount = 0;

    // Mouse state
    glm::vec2 m_mousePosition{0.0f};
    glm::vec2 m_lastMousePosition{0.0f};
    glm::vec2 m_mouseDelta{0.0f};
    float m_scrollDeltaY = 0.0f;
    float m_scrollDeltaX = 0.0f;

    bool m_cursorLocked = false;
    bool m_cursorVisible = true;
    bool m_firstMouse = true;

    // Action mapping
    std::unordered_map<std::string, std::vector<InputBinding>> m_actionBindings;

    // User callbacks
    KeyCallback m_keyCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    ScrollCallback m_scrollCallback;
};

// -----------------------------------------------------------------------------
// Utility Functions (defined in Keyboard.cpp and Mouse.cpp)
// -----------------------------------------------------------------------------

/** @brief Convert a Key enum to a human-readable string */
const char* KeyToString(Key key) noexcept;

/** @brief Check if the key is a modifier key (Shift, Ctrl, Alt, Super) */
bool IsModifierKey(Key key) noexcept;

/** @brief Check if the key produces a printable character */
bool IsPrintableKey(Key key) noexcept;

/** @brief Check if the key is a function key (F1-F12) */
bool IsFunctionKey(Key key) noexcept;

/** @brief Check if the key is a navigation key (arrows, home, end, etc.) */
bool IsNavigationKey(Key key) noexcept;

/** @brief Convert a MouseButton enum to a human-readable string */
const char* MouseButtonToString(MouseButton button) noexcept;

/** @brief Get a short string representation for the mouse button (useful for UI) */
const char* MouseButtonToShortString(MouseButton button) noexcept;

/** @brief Check if the button is a standard mouse button (left, right, middle) */
bool IsStandardMouseButton(MouseButton button) noexcept;

/** @brief Convert modifier flags to a human-readable string */
const char* ModifierFlagsToString(ModifierFlags flags) noexcept;

} // namespace Nova
