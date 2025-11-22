/**
 * @file DesktopInput.cpp
 * @brief Desktop input implementation using GLFW
 */

#include "DesktopInput.hpp"
#include <GLFW/glfw3.h>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Nova {

// Static instance for GLFW callbacks
DesktopInput* DesktopInput::s_instance = nullptr;

// =============================================================================
// Constructor/Destructor
// =============================================================================

DesktopInput::DesktopInput() {
    // Reserve space for changed keys/buttons to avoid reallocations
    m_changedKeys.reserve(32);
    m_changedMouseButtons.reserve(8);
    m_textInput.reserve(32);
}

DesktopInput::~DesktopInput() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

void DesktopInput::Initialize(GLFWwindow* window) {
    if (!window) return;

    m_window = window;
    s_instance = this;

    // Set GLFW callbacks
    glfwSetKeyCallback(window, KeyCallbackGLFW);
    glfwSetMouseButtonCallback(window, MouseButtonCallbackGLFW);
    glfwSetCursorPosCallback(window, CursorPosCallbackGLFW);
    glfwSetScrollCallback(window, ScrollCallbackGLFW);
    glfwSetCharCallback(window, CharCallbackGLFW);
    glfwSetJoystickCallback(JoystickCallbackGLFW);

    // Get initial mouse position
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_mousePosition = {static_cast<float>(x), static_cast<float>(y)};
    m_lastMousePosition = m_mousePosition;

    // Initialize gamepad states
    UpdateGamepads();
}

void DesktopInput::Shutdown() {
    if (m_window && s_instance == this) {
        glfwSetKeyCallback(m_window, nullptr);
        glfwSetMouseButtonCallback(m_window, nullptr);
        glfwSetCursorPosCallback(m_window, nullptr);
        glfwSetScrollCallback(m_window, nullptr);
        glfwSetCharCallback(m_window, nullptr);
        s_instance = nullptr;
    }
    m_window = nullptr;
}

// =============================================================================
// Update
// =============================================================================

void DesktopInput::Update() {
    // Clear pressed/released states from last frame
    for (int keyIdx : m_changedKeys) {
        m_keys[keyIdx].pressed = false;
        m_keys[keyIdx].released = false;
    }
    m_changedKeys.clear();

    for (int btnIdx : m_changedMouseButtons) {
        m_mouseButtons[btnIdx].pressed = false;
        m_mouseButtons[btnIdx].released = false;
    }
    m_changedMouseButtons.clear();

    // Clear scroll delta
    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;

    // Calculate mouse delta
    m_mouseDelta = m_mousePosition - m_lastMousePosition;
    m_lastMousePosition = m_mousePosition;

    // Clear text input
    m_textInput.clear();

    // Update gamepad states
    UpdateGamepads();
}

// =============================================================================
// Keyboard
// =============================================================================

bool DesktopInput::IsKeyDown(Key key) const noexcept {
    int idx = static_cast<int>(key);
    if (idx >= 0 && idx < static_cast<int>(m_keys.size())) {
        return m_keys[idx].down;
    }
    return false;
}

bool DesktopInput::WasKeyPressed(Key key) const noexcept {
    int idx = static_cast<int>(key);
    if (idx >= 0 && idx < static_cast<int>(m_keys.size())) {
        return m_keys[idx].pressed;
    }
    return false;
}

bool DesktopInput::WasKeyReleased(Key key) const noexcept {
    int idx = static_cast<int>(key);
    if (idx >= 0 && idx < static_cast<int>(m_keys.size())) {
        return m_keys[idx].released;
    }
    return false;
}

bool DesktopInput::IsAnyKeyDown() const noexcept {
    return m_activeKeyCount > 0;
}

ModifierFlags DesktopInput::GetModifiers() const noexcept {
    ModifierFlags flags = ModifierFlags::None;

    if (IsKeyDown(Key::LeftShift) || IsKeyDown(Key::RightShift)) {
        flags = flags | ModifierFlags::Shift;
    }
    if (IsKeyDown(Key::LeftControl) || IsKeyDown(Key::RightControl)) {
        flags = flags | ModifierFlags::Control;
    }
    if (IsKeyDown(Key::LeftAlt) || IsKeyDown(Key::RightAlt)) {
        flags = flags | ModifierFlags::Alt;
    }
    if (IsKeyDown(Key::LeftSuper) || IsKeyDown(Key::RightSuper)) {
        flags = flags | ModifierFlags::Super;
    }

    return flags;
}

bool DesktopInput::IsShiftDown() const noexcept {
    return IsKeyDown(Key::LeftShift) || IsKeyDown(Key::RightShift);
}

bool DesktopInput::IsCtrlDown() const noexcept {
    return IsKeyDown(Key::LeftControl) || IsKeyDown(Key::RightControl);
}

bool DesktopInput::IsAltDown() const noexcept {
    return IsKeyDown(Key::LeftAlt) || IsKeyDown(Key::RightAlt);
}

bool DesktopInput::IsSuperDown() const noexcept {
    return IsKeyDown(Key::LeftSuper) || IsKeyDown(Key::RightSuper);
}

const char* DesktopInput::GetKeyName(Key key) noexcept {
    return KeyToString(key);
}

// =============================================================================
// Mouse
// =============================================================================

glm::vec2 DesktopInput::GetMousePosition() const noexcept {
    return m_mousePosition;
}

glm::vec2 DesktopInput::GetMouseDelta() const noexcept {
    return m_mouseDelta;
}

bool DesktopInput::IsMouseButtonDown(MouseButton button) const noexcept {
    int idx = static_cast<int>(button);
    if (idx >= 0 && idx < static_cast<int>(m_mouseButtons.size())) {
        return m_mouseButtons[idx].down;
    }
    return false;
}

bool DesktopInput::WasMouseButtonPressed(MouseButton button) const noexcept {
    int idx = static_cast<int>(button);
    if (idx >= 0 && idx < static_cast<int>(m_mouseButtons.size())) {
        return m_mouseButtons[idx].pressed;
    }
    return false;
}

bool DesktopInput::WasMouseButtonReleased(MouseButton button) const noexcept {
    int idx = static_cast<int>(button);
    if (idx >= 0 && idx < static_cast<int>(m_mouseButtons.size())) {
        return m_mouseButtons[idx].released;
    }
    return false;
}

float DesktopInput::GetScrollDelta() const noexcept {
    return m_scrollDeltaY;
}

float DesktopInput::GetScrollDeltaX() const noexcept {
    return m_scrollDeltaX;
}

void DesktopInput::SetMousePosition(const glm::vec2& pos) {
    if (m_window) {
        glfwSetCursorPos(m_window, static_cast<double>(pos.x), static_cast<double>(pos.y));
        m_mousePosition = pos;
        m_lastMousePosition = pos;
    }
}

// =============================================================================
// Cursor
// =============================================================================

void DesktopInput::SetCursorMode(CursorMode mode) {
    if (!m_window) return;

    m_cursorMode = mode;

    switch (mode) {
        case CursorMode::Normal:
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
        case CursorMode::Hidden:
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case CursorMode::Disabled:
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        case CursorMode::Captured:
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
            break;
    }
}

void DesktopInput::SetCursorShape(CursorShape shape) {
    // GLFW cursor shape support - would need to create cursors
    m_cursorShape = shape;

    // Standard cursor creation (simplified)
    int glfwShape = GLFW_ARROW_CURSOR;
    switch (shape) {
        case CursorShape::Arrow:     glfwShape = GLFW_ARROW_CURSOR; break;
        case CursorShape::IBeam:     glfwShape = GLFW_IBEAM_CURSOR; break;
        case CursorShape::Crosshair: glfwShape = GLFW_CROSSHAIR_CURSOR; break;
        case CursorShape::Hand:      glfwShape = GLFW_HAND_CURSOR; break;
        case CursorShape::HResize:   glfwShape = GLFW_HRESIZE_CURSOR; break;
        case CursorShape::VResize:   glfwShape = GLFW_VRESIZE_CURSOR; break;
        default: break;
    }

    GLFWcursor* cursor = glfwCreateStandardCursor(glfwShape);
    if (cursor && m_window) {
        glfwSetCursor(m_window, cursor);
    }
}

void DesktopInput::HideCursor() {
    m_cursorVisible = false;
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
}

void DesktopInput::ShowCursor() {
    m_cursorVisible = true;
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

// =============================================================================
// Gamepad
// =============================================================================

void DesktopInput::UpdateGamepads() {
    for (int i = 0; i < MAX_GAMEPADS; ++i) {
        if (glfwJoystickPresent(i) && glfwJoystickIsGamepad(i)) {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(i, &state)) {
                m_gamepads[i].connected = true;
                m_gamepads[i].name = glfwGetGamepadName(i);

                // Update buttons
                for (int b = 0; b < static_cast<int>(GamepadButton::MaxButton); ++b) {
                    m_gamepads[i].buttons[b] = (state.buttons[b] == GLFW_PRESS);
                }

                // Update axes with deadzone
                for (int a = 0; a < static_cast<int>(GamepadAxis::MaxAxis); ++a) {
                    m_gamepads[i].axes[a] = ApplyDeadzone(state.axes[a]);
                }
            }
        } else {
            m_gamepads[i].connected = false;
        }
    }
}

float DesktopInput::ApplyDeadzone(float value) const noexcept {
    if (std::abs(value) < m_deadzone) {
        return 0.0f;
    }
    // Rescale to 0-1 range after deadzone
    float sign = value > 0 ? 1.0f : -1.0f;
    return sign * (std::abs(value) - m_deadzone) / (1.0f - m_deadzone);
}

int DesktopInput::GetGamepadCount() const noexcept {
    int count = 0;
    for (const auto& gp : m_gamepads) {
        if (gp.connected) ++count;
    }
    return count;
}

bool DesktopInput::IsGamepadConnected(int index) const noexcept {
    if (index >= 0 && index < MAX_GAMEPADS) {
        return m_gamepads[index].connected;
    }
    return false;
}

const GamepadState& DesktopInput::GetGamepad(int index) const noexcept {
    static GamepadState empty;
    if (index >= 0 && index < MAX_GAMEPADS) {
        return m_gamepads[index];
    }
    return empty;
}

const GamepadState* DesktopInput::GetFirstConnectedGamepad() const noexcept {
    for (const auto& gp : m_gamepads) {
        if (gp.connected) {
            return &gp;
        }
    }
    return nullptr;
}

bool DesktopInput::IsGamepadButtonDown(int gamepad, GamepadButton button) const noexcept {
    if (gamepad >= 0 && gamepad < MAX_GAMEPADS) {
        return m_gamepads[gamepad].IsButtonDown(button);
    }
    return false;
}

float DesktopInput::GetGamepadAxis(int gamepad, GamepadAxis axis) const noexcept {
    if (gamepad >= 0 && gamepad < MAX_GAMEPADS) {
        return m_gamepads[gamepad].GetAxis(axis);
    }
    return 0.0f;
}

// =============================================================================
// Text Input
// =============================================================================

void DesktopInput::EnableTextInput() {
    m_textInputEnabled = true;
}

void DesktopInput::DisableTextInput() {
    m_textInputEnabled = false;
    m_textInput.clear();
}

// =============================================================================
// Axis Helpers
// =============================================================================

float DesktopInput::GetAxis(Key negative, Key positive) const noexcept {
    float value = 0.0f;
    if (IsKeyDown(negative)) value -= 1.0f;
    if (IsKeyDown(positive)) value += 1.0f;
    return value;
}

glm::vec2 DesktopInput::GetMovementVector(bool wasd) const noexcept {
    glm::vec2 move{0.0f};

    if (wasd) {
        if (IsKeyDown(Key::A)) move.x -= 1.0f;
        if (IsKeyDown(Key::D)) move.x += 1.0f;
        if (IsKeyDown(Key::W)) move.y += 1.0f;
        if (IsKeyDown(Key::S)) move.y -= 1.0f;
    } else {
        if (IsKeyDown(Key::Left))  move.x -= 1.0f;
        if (IsKeyDown(Key::Right)) move.x += 1.0f;
        if (IsKeyDown(Key::Up))    move.y += 1.0f;
        if (IsKeyDown(Key::Down))  move.y -= 1.0f;
    }

    // Normalize if moving diagonally
    float length = std::sqrt(move.x * move.x + move.y * move.y);
    if (length > 1.0f) {
        move /= length;
    }

    return move;
}

// =============================================================================
// GLFW Callbacks
// =============================================================================

void DesktopInput::KeyCallbackGLFW(GLFWwindow* /*window*/, int key, int scancode, int action, int mods) {
    if (!s_instance || key < 0 || key >= static_cast<int>(Key::MaxKey)) return;

    auto& state = s_instance->m_keys[key];
    bool wasDown = state.down;

    if (action == GLFW_PRESS) {
        state.down = true;
        state.pressed = true;
        if (!wasDown) {
            s_instance->m_activeKeyCount++;
        }
    } else if (action == GLFW_RELEASE) {
        state.down = false;
        state.released = true;
        if (wasDown) {
            s_instance->m_activeKeyCount--;
        }
    }

    s_instance->m_changedKeys.push_back(key);

    // User callback
    if (s_instance->m_keyCallback) {
        ModifierFlags modFlags = ModifierFlags::None;
        if (mods & GLFW_MOD_SHIFT) modFlags = modFlags | ModifierFlags::Shift;
        if (mods & GLFW_MOD_CONTROL) modFlags = modFlags | ModifierFlags::Control;
        if (mods & GLFW_MOD_ALT) modFlags = modFlags | ModifierFlags::Alt;
        if (mods & GLFW_MOD_SUPER) modFlags = modFlags | ModifierFlags::Super;

        s_instance->m_keyCallback(
            static_cast<Key>(key),
            scancode,
            action == GLFW_PRESS || action == GLFW_REPEAT,
            modFlags
        );
    }
}

void DesktopInput::MouseButtonCallbackGLFW(GLFWwindow* /*window*/, int button, int action, int mods) {
    if (!s_instance || button < 0 || button >= static_cast<int>(MouseButton::MaxButton)) return;

    auto& state = s_instance->m_mouseButtons[button];

    if (action == GLFW_PRESS) {
        state.down = true;
        state.pressed = true;
    } else if (action == GLFW_RELEASE) {
        state.down = false;
        state.released = true;
    }

    s_instance->m_changedMouseButtons.push_back(button);

    // User callback
    if (s_instance->m_mouseButtonCallback) {
        ModifierFlags modFlags = ModifierFlags::None;
        if (mods & GLFW_MOD_SHIFT) modFlags = modFlags | ModifierFlags::Shift;
        if (mods & GLFW_MOD_CONTROL) modFlags = modFlags | ModifierFlags::Control;
        if (mods & GLFW_MOD_ALT) modFlags = modFlags | ModifierFlags::Alt;
        if (mods & GLFW_MOD_SUPER) modFlags = modFlags | ModifierFlags::Super;

        s_instance->m_mouseButtonCallback(
            static_cast<MouseButton>(button),
            action == GLFW_PRESS,
            modFlags
        );
    }
}

void DesktopInput::CursorPosCallbackGLFW(GLFWwindow* /*window*/, double x, double y) {
    if (!s_instance) return;

    glm::vec2 newPos{static_cast<float>(x), static_cast<float>(y)};

    if (s_instance->m_firstMouseMove) {
        s_instance->m_lastMousePosition = newPos;
        s_instance->m_firstMouseMove = false;
    }

    s_instance->m_mousePosition = newPos;

    // User callback
    if (s_instance->m_mouseMoveCallback) {
        s_instance->m_mouseMoveCallback(newPos);
    }
}

void DesktopInput::ScrollCallbackGLFW(GLFWwindow* /*window*/, double x, double y) {
    if (!s_instance) return;

    s_instance->m_scrollDeltaX = static_cast<float>(x);
    s_instance->m_scrollDeltaY = static_cast<float>(y);

    // User callback
    if (s_instance->m_scrollCallback) {
        s_instance->m_scrollCallback(
            static_cast<float>(x),
            static_cast<float>(y)
        );
    }
}

void DesktopInput::CharCallbackGLFW(GLFWwindow* /*window*/, unsigned int codepoint) {
    if (!s_instance || !s_instance->m_textInputEnabled) return;

    TextInputEvent event;
    event.codepoint = codepoint;
    event.mods = s_instance->GetModifiers();
    s_instance->m_textInput.push_back(event);

    // User callback
    if (s_instance->m_charCallback) {
        s_instance->m_charCallback(codepoint);
    }
}

void DesktopInput::JoystickCallbackGLFW(int jid, int event) {
    if (!s_instance) return;

    bool connected = (event == GLFW_CONNECTED);

    if (s_instance->m_gamepadCallback) {
        s_instance->m_gamepadCallback(jid, connected);
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* KeyToString(Key key) noexcept {
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

const char* MouseButtonToString(MouseButton button) noexcept {
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

bool IsModifierKey(Key key) noexcept {
    return key == Key::LeftShift || key == Key::RightShift ||
           key == Key::LeftControl || key == Key::RightControl ||
           key == Key::LeftAlt || key == Key::RightAlt ||
           key == Key::LeftSuper || key == Key::RightSuper;
}

bool IsPrintableKey(Key key) noexcept {
    int k = static_cast<int>(key);
    return (k >= 32 && k <= 126);
}

bool IsFunctionKey(Key key) noexcept {
    int k = static_cast<int>(key);
    return (k >= static_cast<int>(Key::F1) && k <= static_cast<int>(Key::F25));
}

bool IsNumpadKey(Key key) noexcept {
    int k = static_cast<int>(key);
    return (k >= static_cast<int>(Key::KP0) && k <= static_cast<int>(Key::KPEqual));
}

} // namespace Nova
