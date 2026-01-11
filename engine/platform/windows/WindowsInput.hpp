#pragma once

/**
 * @file WindowsInput.hpp
 * @brief Windows-specific input handling
 *
 * Features:
 * - Raw Input for precise mouse movement
 * - Keyboard with scancodes
 * - XInput for Xbox controllers
 * - Touch/Pen support (Windows 8+)
 * - IME support for text input
 */

#ifdef NOVA_PLATFORM_WINDOWS

#include "../desktop/DesktopInput.hpp"
#include <array>
#include <string>
#include <functional>
#include <cstdint>

// Forward declarations
struct HWND__;
typedef HWND__* HWND;

namespace Nova {

/**
 * @brief XInput gamepad state
 */
struct XInputGamepadState {
    bool connected = false;
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;
    int16_t thumbLX = 0;
    int16_t thumbLY = 0;
    int16_t thumbRX = 0;
    int16_t thumbRY = 0;

    // Vibration
    uint16_t leftMotorSpeed = 0;
    uint16_t rightMotorSpeed = 0;
};

/**
 * @brief Touch point information
 */
struct TouchPoint {
    uint32_t id;
    float x;
    float y;
    float pressure;
    bool isPrimary;
    bool isInContact;
    bool isPen;
    bool isEraser;
};

/**
 * @brief Windows input handler
 *
 * Provides advanced input handling using Windows APIs:
 * - Raw Input for high-precision mouse
 * - Scancodes for keyboard
 * - XInput for gamepads
 * - Touch and pen support
 */
class WindowsInput {
public:
    WindowsInput();
    ~WindowsInput();

    /**
     * @brief Initialize with window handle
     */
    bool Initialize(HWND hwnd);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update input state (call each frame)
     */
    void Update();

    /**
     * @brief Process Windows message for input
     * @return true if message was handled
     */
    bool ProcessMessage(HWND hwnd, uint32_t msg, uint64_t wParam, int64_t lParam);

    // -------------------------------------------------------------------------
    // Raw Input Mouse
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable raw input for mouse
     */
    void SetRawMouseInput(bool enabled);

    /**
     * @brief Check if raw mouse input is enabled
     */
    [[nodiscard]] bool IsRawMouseInputEnabled() const { return m_rawMouseEnabled; }

    /**
     * @brief Get raw mouse delta since last frame
     */
    [[nodiscard]] glm::vec2 GetRawMouseDelta() const { return m_rawMouseDelta; }

    /**
     * @brief Get absolute mouse position
     */
    [[nodiscard]] glm::vec2 GetMousePosition() const { return m_mousePosition; }

    // -------------------------------------------------------------------------
    // Keyboard
    // -------------------------------------------------------------------------

    /**
     * @brief Check if key is currently held (by scancode)
     */
    [[nodiscard]] bool IsKeyDown(int scancode) const;

    /**
     * @brief Check if key was pressed this frame
     */
    [[nodiscard]] bool WasKeyPressed(int scancode) const;

    /**
     * @brief Check if key was released this frame
     */
    [[nodiscard]] bool WasKeyReleased(int scancode) const;

    /**
     * @brief Get key name from scancode
     */
    [[nodiscard]] std::string GetKeyName(int scancode) const;

    /**
     * @brief Convert virtual key to scancode
     */
    [[nodiscard]] static int VirtualKeyToScancode(int vk);

    /**
     * @brief Convert scancode to virtual key
     */
    [[nodiscard]] static int ScancodeToVirtualKey(int scancode);

    // -------------------------------------------------------------------------
    // XInput Gamepad
    // -------------------------------------------------------------------------

    /**
     * @brief Get gamepad state (0-3)
     */
    [[nodiscard]] const XInputGamepadState& GetGamepad(int index) const;

    /**
     * @brief Check if gamepad is connected
     */
    [[nodiscard]] bool IsGamepadConnected(int index) const;

    /**
     * @brief Set gamepad vibration
     */
    void SetGamepadVibration(int index, float leftMotor, float rightMotor);

    /**
     * @brief Stop gamepad vibration
     */
    void StopGamepadVibration(int index);

    /**
     * @brief Get XInput button state
     */
    [[nodiscard]] bool IsGamepadButtonDown(int index, uint16_t button) const;

    /**
     * @brief Get normalized axis value (-1 to 1)
     */
    [[nodiscard]] float GetGamepadAxis(int index, int axis) const;

    // XInput button masks
    static constexpr uint16_t XINPUT_DPAD_UP = 0x0001;
    static constexpr uint16_t XINPUT_DPAD_DOWN = 0x0002;
    static constexpr uint16_t XINPUT_DPAD_LEFT = 0x0004;
    static constexpr uint16_t XINPUT_DPAD_RIGHT = 0x0008;
    static constexpr uint16_t XINPUT_START = 0x0010;
    static constexpr uint16_t XINPUT_BACK = 0x0020;
    static constexpr uint16_t XINPUT_LEFT_THUMB = 0x0040;
    static constexpr uint16_t XINPUT_RIGHT_THUMB = 0x0080;
    static constexpr uint16_t XINPUT_LEFT_SHOULDER = 0x0100;
    static constexpr uint16_t XINPUT_RIGHT_SHOULDER = 0x0200;
    static constexpr uint16_t XINPUT_A = 0x1000;
    static constexpr uint16_t XINPUT_B = 0x2000;
    static constexpr uint16_t XINPUT_X = 0x4000;
    static constexpr uint16_t XINPUT_Y = 0x8000;

    // -------------------------------------------------------------------------
    // Touch/Pen Input
    // -------------------------------------------------------------------------

    /**
     * @brief Enable/disable touch input
     */
    void SetTouchEnabled(bool enabled);

    /**
     * @brief Get current touch points
     */
    [[nodiscard]] const std::vector<TouchPoint>& GetTouchPoints() const { return m_touchPoints; }

    /**
     * @brief Get touch point by ID
     */
    [[nodiscard]] const TouchPoint* GetTouchPoint(uint32_t id) const;

    /**
     * @brief Check if pen is in use
     */
    [[nodiscard]] bool IsPenActive() const { return m_penActive; }

    /**
     * @brief Get pen pressure (0-1)
     */
    [[nodiscard]] float GetPenPressure() const { return m_penPressure; }

    /**
     * @brief Get pen tilt
     */
    [[nodiscard]] glm::vec2 GetPenTilt() const { return m_penTilt; }

    // -------------------------------------------------------------------------
    // IME Text Input
    // -------------------------------------------------------------------------

    /**
     * @brief Enable IME text input
     */
    void EnableIME();

    /**
     * @brief Disable IME text input
     */
    void DisableIME();

    /**
     * @brief Check if IME is enabled
     */
    [[nodiscard]] bool IsIMEEnabled() const { return m_imeEnabled; }

    /**
     * @brief Set IME candidate window position
     */
    void SetIMEPosition(int x, int y);

    /**
     * @brief Get composed text during IME input
     */
    [[nodiscard]] const std::wstring& GetIMEComposition() const { return m_imeComposition; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using KeyCallback = std::function<void(int scancode, bool pressed, bool repeat)>;
    using CharCallback = std::function<void(uint32_t codepoint)>;
    using MouseMoveCallback = std::function<void(float x, float y, float deltaX, float deltaY)>;
    using MouseButtonCallback = std::function<void(int button, bool pressed)>;
    using MouseWheelCallback = std::function<void(float deltaX, float deltaY)>;
    using TouchCallback = std::function<void(const TouchPoint& point, int action)>;  // 0=down, 1=move, 2=up
    using GamepadCallback = std::function<void(int index, bool connected)>;

    void SetKeyCallback(KeyCallback cb) { m_keyCallback = std::move(cb); }
    void SetCharCallback(CharCallback cb) { m_charCallback = std::move(cb); }
    void SetMouseMoveCallback(MouseMoveCallback cb) { m_mouseMoveCallback = std::move(cb); }
    void SetMouseButtonCallback(MouseButtonCallback cb) { m_mouseButtonCallback = std::move(cb); }
    void SetMouseWheelCallback(MouseWheelCallback cb) { m_mouseWheelCallback = std::move(cb); }
    void SetTouchCallback(TouchCallback cb) { m_touchCallback = std::move(cb); }
    void SetGamepadCallback(GamepadCallback cb) { m_gamepadCallback = std::move(cb); }

private:
    void UpdateXInput();
    void RegisterRawInput(HWND hwnd);
    void UnregisterRawInput();
    bool ProcessRawInput(int64_t lParam);
    bool ProcessTouch(uint64_t wParam, int64_t lParam);
    bool ProcessIME(uint32_t msg, uint64_t wParam, int64_t lParam);

    HWND m_hwnd = nullptr;
    bool m_initialized = false;

    // Raw mouse input
    bool m_rawMouseEnabled = false;
    glm::vec2 m_rawMouseDelta{0.0f};
    glm::vec2 m_mousePosition{0.0f};
    glm::vec2 m_lastMousePosition{0.0f};

    // Keyboard
    struct KeyState {
        bool down = false;
        bool pressed = false;
        bool released = false;
    };
    std::array<KeyState, 256> m_keyStates{};
    std::vector<int> m_changedKeys;

    // XInput gamepads
    std::array<XInputGamepadState, 4> m_gamepads{};
    std::array<bool, 4> m_gamepadWasConnected{};

    // Touch/Pen
    bool m_touchEnabled = false;
    std::vector<TouchPoint> m_touchPoints;
    bool m_penActive = false;
    float m_penPressure = 0.0f;
    glm::vec2 m_penTilt{0.0f};

    // IME
    bool m_imeEnabled = false;
    std::wstring m_imeComposition;

    // Callbacks
    KeyCallback m_keyCallback;
    CharCallback m_charCallback;
    MouseMoveCallback m_mouseMoveCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseWheelCallback m_mouseWheelCallback;
    TouchCallback m_touchCallback;
    GamepadCallback m_gamepadCallback;
};

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
