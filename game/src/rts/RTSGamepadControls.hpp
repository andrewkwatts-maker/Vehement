#pragma once

/**
 * @file RTSGamepadControls.hpp
 * @brief Gamepad control scheme for RTS gameplay
 *
 * Provides intuitive gamepad controls for RTS:
 * - Left Stick: Move virtual cursor
 * - Right Stick: Pan camera
 * - D-Pad: Quick camera movement
 * - Triggers: Zoom in/out
 * - Face Buttons: Select, Command, Cancel, Menu
 * - Bumpers: Cycle units, Cycle buildings
 * - Start: Pause menu
 * - Select: Toggle camera modes
 */

#include <glm/glm.hpp>

namespace Nova {
    class InputManager;
}

namespace Vehement {
namespace RTS {

class RTSInputController;

/**
 * @brief Gamepad button mapping for RTS
 */
enum class GamepadButton : uint8_t {
    // Face buttons (Xbox layout)
    A = 0,              // Select/Confirm
    B = 1,              // Cancel/Back
    X = 2,              // Command/Action
    Y = 3,              // Special/Menu

    // Bumpers
    LeftBumper = 4,     // Previous unit/building
    RightBumper = 5,    // Next unit/building

    // Special buttons
    Back = 6,           // Toggle camera mode
    Start = 7,          // Pause menu

    // Stick buttons
    LeftStick = 8,      // Reset cursor
    RightStick = 9,     // Reset camera

    // D-Pad
    DPadUp = 10,
    DPadDown = 11,
    DPadLeft = 12,
    DPadRight = 13
};

/**
 * @brief Gamepad axis indices
 */
enum class GamepadAxis : uint8_t {
    LeftStickX = 0,     // Cursor X movement
    LeftStickY = 1,     // Cursor Y movement
    RightStickX = 2,    // Camera pan X
    RightStickY = 3,    // Camera pan Y
    LeftTrigger = 4,    // Zoom out
    RightTrigger = 5    // Zoom in
};

/**
 * @brief Gamepad control configuration
 */
struct GamepadConfig {
    // Cursor settings
    float cursorSpeed = 800.0f;         // Pixels per second
    float cursorAcceleration = 2.0f;    // Acceleration multiplier
    bool cursorVisible = true;          // Show cursor sprite

    // Camera settings
    float cameraPanSpeed = 20.0f;       // Units per second
    float cameraZoomSpeed = 10.0f;      // Zoom speed

    // Stick deadzones
    float leftStickDeadzone = 0.15f;
    float rightStickDeadzone = 0.15f;
    float triggerDeadzone = 0.1f;

    // Button hold delays
    float selectionHoldTime = 0.3f;     // Hold A for area select
    float commandHoldTime = 0.3f;       // Hold X for special command

    // Sensitivity
    float stickSensitivity = 1.0f;
    bool invertYAxis = false;
};

/**
 * @brief Gamepad input handler for RTS
 */
class RTSGamepadControls {
public:
    RTSGamepadControls();
    ~RTSGamepadControls() = default;

    /**
     * @brief Initialize gamepad controls
     */
    bool Initialize(RTSInputController* controller);

    /**
     * @brief Update gamepad input
     */
    void Update(Nova::InputManager& input, float deltaTime);

    /**
     * @brief Render gamepad cursor
     */
    void RenderCursor();

    /**
     * @brief Get gamepad configuration
     */
    GamepadConfig& GetConfig() { return m_config; }
    const GamepadConfig& GetConfig() const { return m_config; }

    /**
     * @brief Get virtual cursor position (screen space)
     */
    glm::vec2 GetCursorPosition() const { return m_cursorPosition; }

    /**
     * @brief Set cursor position
     */
    void SetCursorPosition(const glm::vec2& position) { m_cursorPosition = position; }

    /**
     * @brief Check if gamepad is connected
     */
    bool IsGamepadConnected() const { return m_gamepadConnected; }

private:
    void UpdateCursor(Nova::InputManager& input, float deltaTime);
    void UpdateCamera(Nova::InputManager& input, float deltaTime);
    void UpdateButtons(Nova::InputManager& input, float deltaTime);
    void UpdateTriggers(Nova::InputManager& input, float deltaTime);

    float ApplyDeadzone(float value, float deadzone);
    glm::vec2 ApplyDeadzone(const glm::vec2& value, float deadzone);

    RTSInputController* m_controller = nullptr;
    GamepadConfig m_config;

    // Cursor state
    glm::vec2 m_cursorPosition{0.0f};
    glm::vec2 m_cursorVelocity{0.0f};

    // Gamepad state
    bool m_gamepadConnected = false;
    int m_gamepadIndex = 0;

    // Button hold timers
    float m_aButtonHoldTime = 0.0f;
    float m_xButtonHoldTime = 0.0f;

    // Previous frame button states
    bool m_aButtonWasPressed = false;
    bool m_bButtonWasPressed = false;
    bool m_xButtonWasPressed = false;
    bool m_yButtonWasPressed = false;
};

} // namespace RTS
} // namespace Vehement
