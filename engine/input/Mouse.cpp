#include "input/InputManager.hpp"

namespace Nova {

/**
 * @brief Convert a MouseButton enum to a human-readable string
 * @param button The mouse button to convert
 * @return String representation of the button
 */
const char* MouseButtonToString(MouseButton button) noexcept {
    switch (button) {
        case MouseButton::Left: return "Left Mouse";
        case MouseButton::Right: return "Right Mouse";
        case MouseButton::Middle: return "Middle Mouse";
        case MouseButton::Button4: return "Mouse 4";
        case MouseButton::Button5: return "Mouse 5";
        case MouseButton::Button6: return "Mouse 6";
        case MouseButton::Button7: return "Mouse 7";
        case MouseButton::Button8: return "Mouse 8";
        default: return "Unknown Mouse Button";
    }
}

/**
 * @brief Get a short string representation for the mouse button (useful for UI)
 * @param button The mouse button to convert
 * @return Short string representation
 */
const char* MouseButtonToShortString(MouseButton button) noexcept {
    switch (button) {
        case MouseButton::Left: return "LMB";
        case MouseButton::Right: return "RMB";
        case MouseButton::Middle: return "MMB";
        case MouseButton::Button4: return "M4";
        case MouseButton::Button5: return "M5";
        case MouseButton::Button6: return "M6";
        case MouseButton::Button7: return "M7";
        case MouseButton::Button8: return "M8";
        default: return "?";
    }
}

/**
 * @brief Check if the button is a standard mouse button (left, right, middle)
 * @param button The button to check
 * @return true if it's a standard button
 */
bool IsStandardMouseButton(MouseButton button) noexcept {
    switch (button) {
        case MouseButton::Left:
        case MouseButton::Right:
        case MouseButton::Middle:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Convert modifier flags to a human-readable string
 * @param flags The modifier flags to convert
 * @return String representation (e.g., "Ctrl+Shift")
 */
const char* ModifierFlagsToString(ModifierFlags flags) noexcept {
    // Use a simple static buffer for the most common combinations
    // For more complex needs, caller should build their own string
    const uint8_t f = static_cast<uint8_t>(flags);

    switch (f) {
        case 0: return "";
        case 1: return "Shift";
        case 2: return "Ctrl";
        case 3: return "Ctrl+Shift";
        case 4: return "Alt";
        case 5: return "Alt+Shift";
        case 6: return "Ctrl+Alt";
        case 7: return "Ctrl+Alt+Shift";
        case 8: return "Super";
        case 9: return "Super+Shift";
        case 10: return "Super+Ctrl";
        case 11: return "Super+Ctrl+Shift";
        case 12: return "Super+Alt";
        case 13: return "Super+Alt+Shift";
        case 14: return "Super+Ctrl+Alt";
        case 15: return "Super+Ctrl+Alt+Shift";
        default: return "Modifiers";
    }
}

} // namespace Nova
