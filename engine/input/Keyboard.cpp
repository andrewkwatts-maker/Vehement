#include "input/InputManager.hpp"

namespace Nova {

/**
 * @brief Convert a Key enum to a human-readable string
 * @param key The key code to convert
 * @return String representation of the key
 */
const char* KeyToString(Key key) noexcept {
    switch (key) {
        // Letters
        case Key::A: return "A";
        case Key::B: return "B";
        case Key::C: return "C";
        case Key::D: return "D";
        case Key::E: return "E";
        case Key::F: return "F";
        case Key::G: return "G";
        case Key::H: return "H";
        case Key::I: return "I";
        case Key::J: return "J";
        case Key::K: return "K";
        case Key::L: return "L";
        case Key::M: return "M";
        case Key::N: return "N";
        case Key::O: return "O";
        case Key::P: return "P";
        case Key::Q: return "Q";
        case Key::R: return "R";
        case Key::S: return "S";
        case Key::T: return "T";
        case Key::U: return "U";
        case Key::V: return "V";
        case Key::W: return "W";
        case Key::X: return "X";
        case Key::Y: return "Y";
        case Key::Z: return "Z";

        // Numbers
        case Key::Num0: return "0";
        case Key::Num1: return "1";
        case Key::Num2: return "2";
        case Key::Num3: return "3";
        case Key::Num4: return "4";
        case Key::Num5: return "5";
        case Key::Num6: return "6";
        case Key::Num7: return "7";
        case Key::Num8: return "8";
        case Key::Num9: return "9";

        // Function keys
        case Key::F1: return "F1";
        case Key::F2: return "F2";
        case Key::F3: return "F3";
        case Key::F4: return "F4";
        case Key::F5: return "F5";
        case Key::F6: return "F6";
        case Key::F7: return "F7";
        case Key::F8: return "F8";
        case Key::F9: return "F9";
        case Key::F10: return "F10";
        case Key::F11: return "F11";
        case Key::F12: return "F12";

        // Special keys
        case Key::Space: return "Space";
        case Key::Apostrophe: return "'";
        case Key::Comma: return ",";
        case Key::Minus: return "-";
        case Key::Period: return ".";
        case Key::Slash: return "/";
        case Key::Semicolon: return ";";
        case Key::Equal: return "=";
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
        case Key::PageUp: return "Page Up";
        case Key::PageDown: return "Page Down";
        case Key::Home: return "Home";
        case Key::End: return "End";
        case Key::CapsLock: return "Caps Lock";
        case Key::ScrollLock: return "Scroll Lock";
        case Key::NumLock: return "Num Lock";
        case Key::PrintScreen: return "Print Screen";
        case Key::Pause: return "Pause";

        // Modifiers
        case Key::LeftShift: return "Left Shift";
        case Key::LeftControl: return "Left Ctrl";
        case Key::LeftAlt: return "Left Alt";
        case Key::LeftSuper: return "Left Super";
        case Key::RightShift: return "Right Shift";
        case Key::RightControl: return "Right Ctrl";
        case Key::RightAlt: return "Right Alt";
        case Key::RightSuper: return "Right Super";
        case Key::Menu: return "Menu";

        default: return "Unknown";
    }
}

/**
 * @brief Check if the key is a modifier key
 * @param key The key to check
 * @return true if the key is a modifier (Shift, Ctrl, Alt, Super)
 */
bool IsModifierKey(Key key) noexcept {
    switch (key) {
        case Key::LeftShift:
        case Key::RightShift:
        case Key::LeftControl:
        case Key::RightControl:
        case Key::LeftAlt:
        case Key::RightAlt:
        case Key::LeftSuper:
        case Key::RightSuper:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Check if the key is a printable character
 * @param key The key to check
 * @return true if the key produces a printable character
 */
bool IsPrintableKey(Key key) noexcept {
    const int code = static_cast<int>(key);
    // Letters A-Z
    if (code >= 65 && code <= 90) return true;
    // Numbers 0-9
    if (code >= 48 && code <= 57) return true;
    // Punctuation and symbols
    switch (key) {
        case Key::Space:
        case Key::Apostrophe:
        case Key::Comma:
        case Key::Minus:
        case Key::Period:
        case Key::Slash:
        case Key::Semicolon:
        case Key::Equal:
        case Key::LeftBracket:
        case Key::Backslash:
        case Key::RightBracket:
        case Key::GraveAccent:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Check if the key is a function key (F1-F12)
 * @param key The key to check
 * @return true if the key is a function key
 */
bool IsFunctionKey(Key key) noexcept {
    const int code = static_cast<int>(key);
    return code >= static_cast<int>(Key::F1) && code <= static_cast<int>(Key::F12);
}

/**
 * @brief Check if the key is a navigation key (arrows, home, end, etc.)
 * @param key The key to check
 * @return true if the key is a navigation key
 */
bool IsNavigationKey(Key key) noexcept {
    switch (key) {
        case Key::Right:
        case Key::Left:
        case Key::Down:
        case Key::Up:
        case Key::PageUp:
        case Key::PageDown:
        case Key::Home:
        case Key::End:
            return true;
        default:
            return false;
    }
}

} // namespace Nova
