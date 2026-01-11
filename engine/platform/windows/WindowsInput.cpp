/**
 * @file WindowsInput.cpp
 * @brief Windows input implementation
 *
 * Uses:
 * - Raw Input API for precise mouse tracking
 * - XInput for Xbox controllers
 * - Touch/Pointer API for touch and pen
 * - IMM32 for Input Method Editor support
 */

#include "WindowsInput.hpp"

#ifdef NOVA_PLATFORM_WINDOWS

#include <Windows.h>
#include <windowsx.h>
#include <Xinput.h>
#include <imm.h>
#include <hidusage.h>

#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "imm32.lib")

namespace Nova {

// =============================================================================
// Constructor/Destructor
// =============================================================================

WindowsInput::WindowsInput() {
    m_changedKeys.reserve(32);
    m_touchPoints.reserve(10);
}

WindowsInput::~WindowsInput() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool WindowsInput::Initialize(HWND hwnd) {
    if (m_initialized) {
        return true;
    }

    m_hwnd = hwnd;

    // Get initial mouse position
    POINT pt;
    if (GetCursorPos(&pt)) {
        ScreenToClient(m_hwnd, &pt);
        m_mousePosition = {static_cast<float>(pt.x), static_cast<float>(pt.y)};
        m_lastMousePosition = m_mousePosition;
    }

    // Enable touch input if available
    if (GetSystemMetrics(SM_DIGITIZER) & NID_MULTI_INPUT) {
        RegisterTouchWindow(m_hwnd, 0);
        m_touchEnabled = true;
    }

    m_initialized = true;
    return true;
}

void WindowsInput::Shutdown() {
    if (!m_initialized) {
        return;
    }

    UnregisterRawInput();

    if (m_touchEnabled && m_hwnd) {
        UnregisterTouchWindow(m_hwnd);
    }

    // Stop all gamepad vibration
    for (int i = 0; i < 4; ++i) {
        StopGamepadVibration(i);
    }

    m_initialized = false;
}

// =============================================================================
// Update
// =============================================================================

void WindowsInput::Update() {
    // Clear pressed/released states
    for (int idx : m_changedKeys) {
        m_keyStates[idx].pressed = false;
        m_keyStates[idx].released = false;
    }
    m_changedKeys.clear();

    // Clear raw mouse delta
    m_rawMouseDelta = {0.0f, 0.0f};

    // Update XInput gamepads
    UpdateXInput();
}

void WindowsInput::UpdateXInput() {
    for (DWORD i = 0; i < 4; ++i) {
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(XINPUT_STATE));

        DWORD result = XInputGetState(i, &state);
        bool connected = (result == ERROR_SUCCESS);

        // Connection changed?
        if (connected != m_gamepadWasConnected[i]) {
            m_gamepadWasConnected[i] = connected;
            if (m_gamepadCallback) {
                m_gamepadCallback(i, connected);
            }
        }

        m_gamepads[i].connected = connected;

        if (connected) {
            m_gamepads[i].buttons = state.Gamepad.wButtons;
            m_gamepads[i].leftTrigger = state.Gamepad.bLeftTrigger;
            m_gamepads[i].rightTrigger = state.Gamepad.bRightTrigger;
            m_gamepads[i].thumbLX = state.Gamepad.sThumbLX;
            m_gamepads[i].thumbLY = state.Gamepad.sThumbLY;
            m_gamepads[i].thumbRX = state.Gamepad.sThumbRX;
            m_gamepads[i].thumbRY = state.Gamepad.sThumbRY;
        }
    }
}

// =============================================================================
// Message Processing
// =============================================================================

bool WindowsInput::ProcessMessage(HWND hwnd, uint32_t msg, uint64_t wParam, int64_t lParam) {
    switch (msg) {
        case WM_INPUT:
            return ProcessRawInput(lParam);

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int scancode = (lParam >> 16) & 0xFF;
            bool repeat = (lParam & 0x40000000) != 0;

            if (!repeat) {
                m_keyStates[scancode].down = true;
                m_keyStates[scancode].pressed = true;
                m_changedKeys.push_back(scancode);
            }

            if (m_keyCallback) {
                m_keyCallback(scancode, true, repeat);
            }
            return true;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            int scancode = (lParam >> 16) & 0xFF;

            m_keyStates[scancode].down = false;
            m_keyStates[scancode].released = true;
            m_changedKeys.push_back(scancode);

            if (m_keyCallback) {
                m_keyCallback(scancode, false, false);
            }
            return true;
        }

        case WM_CHAR:
        case WM_SYSCHAR: {
            uint32_t codepoint = static_cast<uint32_t>(wParam);
            // Handle surrogate pairs for characters outside BMP
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                // High surrogate - wait for low
                return true;
            }
            if (m_charCallback && codepoint >= 32) {
                m_charCallback(codepoint);
            }
            return true;
        }

        case WM_MOUSEMOVE: {
            float x = static_cast<float>(GET_X_LPARAM(lParam));
            float y = static_cast<float>(GET_Y_LPARAM(lParam));

            float deltaX = x - m_lastMousePosition.x;
            float deltaY = y - m_lastMousePosition.y;

            m_lastMousePosition = m_mousePosition;
            m_mousePosition = {x, y};

            if (m_mouseMoveCallback) {
                m_mouseMoveCallback(x, y, deltaX, deltaY);
            }
            return true;
        }

        case WM_LBUTTONDOWN:
            if (m_mouseButtonCallback) m_mouseButtonCallback(0, true);
            return true;
        case WM_LBUTTONUP:
            if (m_mouseButtonCallback) m_mouseButtonCallback(0, false);
            return true;
        case WM_RBUTTONDOWN:
            if (m_mouseButtonCallback) m_mouseButtonCallback(1, true);
            return true;
        case WM_RBUTTONUP:
            if (m_mouseButtonCallback) m_mouseButtonCallback(1, false);
            return true;
        case WM_MBUTTONDOWN:
            if (m_mouseButtonCallback) m_mouseButtonCallback(2, true);
            return true;
        case WM_MBUTTONUP:
            if (m_mouseButtonCallback) m_mouseButtonCallback(2, false);
            return true;
        case WM_XBUTTONDOWN:
            if (m_mouseButtonCallback) {
                int button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;
                m_mouseButtonCallback(button, true);
            }
            return true;
        case WM_XBUTTONUP:
            if (m_mouseButtonCallback) {
                int button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4;
                m_mouseButtonCallback(button, false);
            }
            return true;

        case WM_MOUSEWHEEL: {
            float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            if (m_mouseWheelCallback) {
                m_mouseWheelCallback(0.0f, delta);
            }
            return true;
        }

        case WM_MOUSEHWHEEL: {
            float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            if (m_mouseWheelCallback) {
                m_mouseWheelCallback(delta, 0.0f);
            }
            return true;
        }

        case WM_TOUCH:
            return ProcessTouch(wParam, lParam);

        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_NOTIFY:
            return ProcessIME(msg, wParam, lParam);
    }

    return false;
}

// =============================================================================
// Raw Input
// =============================================================================

void WindowsInput::SetRawMouseInput(bool enabled) {
    if (enabled == m_rawMouseEnabled) {
        return;
    }

    if (enabled) {
        RegisterRawInput(m_hwnd);
    } else {
        UnregisterRawInput();
    }

    m_rawMouseEnabled = enabled;
}

void WindowsInput::RegisterRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid[1];

    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hwnd;

    RegisterRawInputDevices(rid, 1, sizeof(rid[0]));
}

void WindowsInput::UnregisterRawInput() {
    RAWINPUTDEVICE rid[1];

    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    rid[0].dwFlags = RIDEV_REMOVE;
    rid[0].hwndTarget = nullptr;

    RegisterRawInputDevices(rid, 1, sizeof(rid[0]));
}

bool WindowsInput::ProcessRawInput(int64_t lParam) {
    UINT size = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));

    if (size == 0) {
        return false;
    }

    std::vector<BYTE> buffer(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return false;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());

    if (raw->header.dwType == RIM_TYPEMOUSE) {
        if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
            // Absolute positioning (tablet, remote desktop)
            // Convert to relative
        } else {
            // Relative movement
            m_rawMouseDelta.x += static_cast<float>(raw->data.mouse.lLastX);
            m_rawMouseDelta.y += static_cast<float>(raw->data.mouse.lLastY);
        }
        return true;
    }

    return false;
}

// =============================================================================
// Keyboard
// =============================================================================

bool WindowsInput::IsKeyDown(int scancode) const {
    if (scancode >= 0 && scancode < 256) {
        return m_keyStates[scancode].down;
    }
    return false;
}

bool WindowsInput::WasKeyPressed(int scancode) const {
    if (scancode >= 0 && scancode < 256) {
        return m_keyStates[scancode].pressed;
    }
    return false;
}

bool WindowsInput::WasKeyReleased(int scancode) const {
    if (scancode >= 0 && scancode < 256) {
        return m_keyStates[scancode].released;
    }
    return false;
}

std::string WindowsInput::GetKeyName(int scancode) const {
    UINT vk = MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX);

    wchar_t name[128];
    int length = GetKeyNameTextW(static_cast<LONG>(scancode) << 16, name, 128);

    if (length > 0) {
        // Convert to UTF-8
        int size = WideCharToMultiByte(CP_UTF8, 0, name, length, nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, name, length, &result[0], size, nullptr, nullptr);
        return result;
    }

    return "Unknown";
}

int WindowsInput::VirtualKeyToScancode(int vk) {
    return MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
}

int WindowsInput::ScancodeToVirtualKey(int scancode) {
    return MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK);
}

// =============================================================================
// XInput Gamepad
// =============================================================================

const XInputGamepadState& WindowsInput::GetGamepad(int index) const {
    static XInputGamepadState empty;
    if (index >= 0 && index < 4) {
        return m_gamepads[index];
    }
    return empty;
}

bool WindowsInput::IsGamepadConnected(int index) const {
    if (index >= 0 && index < 4) {
        return m_gamepads[index].connected;
    }
    return false;
}

void WindowsInput::SetGamepadVibration(int index, float leftMotor, float rightMotor) {
    if (index < 0 || index >= 4 || !m_gamepads[index].connected) {
        return;
    }

    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);
    XInputSetState(index, &vibration);

    m_gamepads[index].leftMotorSpeed = vibration.wLeftMotorSpeed;
    m_gamepads[index].rightMotorSpeed = vibration.wRightMotorSpeed;
}

void WindowsInput::StopGamepadVibration(int index) {
    SetGamepadVibration(index, 0.0f, 0.0f);
}

bool WindowsInput::IsGamepadButtonDown(int index, uint16_t button) const {
    if (index >= 0 && index < 4 && m_gamepads[index].connected) {
        return (m_gamepads[index].buttons & button) != 0;
    }
    return false;
}

float WindowsInput::GetGamepadAxis(int index, int axis) const {
    if (index < 0 || index >= 4 || !m_gamepads[index].connected) {
        return 0.0f;
    }

    const auto& gp = m_gamepads[index];

    // Apply deadzone (typically 7849 for thumbsticks, 30 for triggers)
    constexpr float THUMB_DEADZONE = 7849.0f;
    constexpr float TRIGGER_THRESHOLD = 30.0f;

    auto applyThumbDeadzone = [](int16_t value) -> float {
        float normalized = static_cast<float>(value) / 32767.0f;
        float magnitude = std::abs(normalized);
        if (magnitude < THUMB_DEADZONE / 32767.0f) {
            return 0.0f;
        }
        float sign = normalized > 0 ? 1.0f : -1.0f;
        return sign * (magnitude - THUMB_DEADZONE / 32767.0f) / (1.0f - THUMB_DEADZONE / 32767.0f);
    };

    switch (axis) {
        case 0: return applyThumbDeadzone(gp.thumbLX);
        case 1: return applyThumbDeadzone(gp.thumbLY);
        case 2: return applyThumbDeadzone(gp.thumbRX);
        case 3: return applyThumbDeadzone(gp.thumbRY);
        case 4: return (gp.leftTrigger > TRIGGER_THRESHOLD) ?
                       (static_cast<float>(gp.leftTrigger) - TRIGGER_THRESHOLD) / (255.0f - TRIGGER_THRESHOLD) : 0.0f;
        case 5: return (gp.rightTrigger > TRIGGER_THRESHOLD) ?
                       (static_cast<float>(gp.rightTrigger) - TRIGGER_THRESHOLD) / (255.0f - TRIGGER_THRESHOLD) : 0.0f;
        default: return 0.0f;
    }
}

// =============================================================================
// Touch/Pen Input
// =============================================================================

void WindowsInput::SetTouchEnabled(bool enabled) {
    if (enabled == m_touchEnabled) {
        return;
    }

    if (enabled && m_hwnd) {
        RegisterTouchWindow(m_hwnd, 0);
    } else if (!enabled && m_hwnd) {
        UnregisterTouchWindow(m_hwnd);
    }

    m_touchEnabled = enabled;
}

const TouchPoint* WindowsInput::GetTouchPoint(uint32_t id) const {
    for (const auto& pt : m_touchPoints) {
        if (pt.id == id) {
            return &pt;
        }
    }
    return nullptr;
}

bool WindowsInput::ProcessTouch(uint64_t wParam, int64_t lParam) {
    UINT numInputs = LOWORD(wParam);
    if (numInputs == 0) {
        return false;
    }

    std::vector<TOUCHINPUT> inputs(numInputs);
    if (!GetTouchInputInfo(reinterpret_cast<HTOUCHINPUT>(lParam), numInputs, inputs.data(), sizeof(TOUCHINPUT))) {
        return false;
    }

    for (const auto& input : inputs) {
        TouchPoint point;
        point.id = input.dwID;

        // Convert from hundredths of a pixel to pixels
        POINT pt = {input.x / 100, input.y / 100};
        ScreenToClient(m_hwnd, &pt);
        point.x = static_cast<float>(pt.x);
        point.y = static_cast<float>(pt.y);

        point.isPrimary = (input.dwFlags & TOUCHEVENTF_PRIMARY) != 0;
        point.isInContact = (input.dwFlags & TOUCHEVENTF_INRANGE) != 0;
        point.isPen = false;
        point.isEraser = false;
        point.pressure = 1.0f;  // Basic touch doesn't have pressure

        int action = -1;
        if (input.dwFlags & TOUCHEVENTF_DOWN) {
            action = 0;
            m_touchPoints.push_back(point);
        } else if (input.dwFlags & TOUCHEVENTF_UP) {
            action = 2;
            m_touchPoints.erase(
                std::remove_if(m_touchPoints.begin(), m_touchPoints.end(),
                    [&](const TouchPoint& p) { return p.id == point.id; }),
                m_touchPoints.end());
        } else if (input.dwFlags & TOUCHEVENTF_MOVE) {
            action = 1;
            for (auto& p : m_touchPoints) {
                if (p.id == point.id) {
                    p = point;
                    break;
                }
            }
        }

        if (action >= 0 && m_touchCallback) {
            m_touchCallback(point, action);
        }
    }

    CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(lParam));
    return true;
}

// =============================================================================
// IME Text Input
// =============================================================================

void WindowsInput::EnableIME() {
    if (m_hwnd) {
        ImmAssociateContextEx(m_hwnd, nullptr, IACE_DEFAULT);
    }
    m_imeEnabled = true;
}

void WindowsInput::DisableIME() {
    if (m_hwnd) {
        ImmAssociateContextEx(m_hwnd, nullptr, 0);
    }
    m_imeEnabled = false;
    m_imeComposition.clear();
}

void WindowsInput::SetIMEPosition(int x, int y) {
    if (!m_hwnd) return;

    HIMC imc = ImmGetContext(m_hwnd);
    if (imc) {
        COMPOSITIONFORM cf;
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos.x = x;
        cf.ptCurrentPos.y = y;
        ImmSetCompositionWindow(imc, &cf);

        CANDIDATEFORM cand;
        cand.dwIndex = 0;
        cand.dwStyle = CFS_CANDIDATEPOS;
        cand.ptCurrentPos.x = x;
        cand.ptCurrentPos.y = y + 20;  // Below the insertion point
        ImmSetCandidateWindow(imc, &cand);

        ImmReleaseContext(m_hwnd, imc);
    }
}

bool WindowsInput::ProcessIME(uint32_t msg, uint64_t wParam, int64_t lParam) {
    if (!m_imeEnabled) {
        return false;
    }

    switch (msg) {
        case WM_IME_STARTCOMPOSITION:
            m_imeComposition.clear();
            return true;

        case WM_IME_COMPOSITION: {
            HIMC imc = ImmGetContext(m_hwnd);
            if (imc) {
                if (lParam & GCS_COMPSTR) {
                    // Get composition string
                    int size = ImmGetCompositionStringW(imc, GCS_COMPSTR, nullptr, 0);
                    if (size > 0) {
                        m_imeComposition.resize(size / sizeof(wchar_t));
                        ImmGetCompositionStringW(imc, GCS_COMPSTR, &m_imeComposition[0], size);
                    } else {
                        m_imeComposition.clear();
                    }
                }

                if (lParam & GCS_RESULTSTR) {
                    // Get result string (committed text)
                    int size = ImmGetCompositionStringW(imc, GCS_RESULTSTR, nullptr, 0);
                    if (size > 0) {
                        std::wstring result(size / sizeof(wchar_t), 0);
                        ImmGetCompositionStringW(imc, GCS_RESULTSTR, &result[0], size);

                        // Send each character as a char callback
                        if (m_charCallback) {
                            for (wchar_t ch : result) {
                                m_charCallback(static_cast<uint32_t>(ch));
                            }
                        }
                    }
                    m_imeComposition.clear();
                }

                ImmReleaseContext(m_hwnd, imc);
            }
            return true;
        }

        case WM_IME_ENDCOMPOSITION:
            m_imeComposition.clear();
            return true;
    }

    return false;
}

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
