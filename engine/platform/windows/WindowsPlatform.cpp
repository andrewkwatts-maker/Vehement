/**
 * @file WindowsPlatform.cpp
 * @brief Full Windows platform implementation using Win32 API
 *
 * Features:
 * - Win32 window creation and message pump
 * - High-DPI support (Per-Monitor DPI Awareness v2)
 * - Multiple monitor support
 * - Fullscreen toggle (exclusive and borderless)
 * - Cursor management
 * - Clipboard access
 * - File dialogs
 * - System information
 */

#include "WindowsPlatform.hpp"

#ifdef NOVA_PLATFORM_WINDOWS

#include <Windows.h>
#include <windowsx.h>
#include <ShellScalingApi.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <VersionHelpers.h>
#include <dwmapi.h>
#include <psapi.h>
#include <powerbase.h>
#include <iphlpapi.h>
#include <wininet.h>

// GLFW for OpenGL context (optional, we also support native WGL)
#ifndef NOVA_NO_GLFW
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <chrono>
#include <thread>

#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")

namespace fs = std::filesystem;

// Undefine conflicting Windows macros that clash with our method names
#undef CreateWindow
#undef CreateDirectory
#undef DeleteFile
#undef GetMonitorInfo

namespace Nova {

// =============================================================================
// Windows Platform Internal Implementation
// =============================================================================

class WindowsPlatformImpl {
public:
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HINSTANCE hInstance = nullptr;
    HGLRC hglrc = nullptr;  // OpenGL rendering context

    std::wstring windowClassName;
    std::string windowTitle;

    glm::ivec2 windowSize{1920, 1080};
    glm::ivec2 framebufferSize{1920, 1080};
    glm::ivec2 windowPosition{100, 100};
    glm::ivec2 windowedSize{1920, 1080};
    glm::ivec2 windowedPosition{100, 100};

    float displayScale = 1.0f;
    bool fullscreen = false;
    bool borderlessFullscreen = false;
    bool vsync = true;
    bool shouldClose = false;
    bool cursorVisible = true;
    bool cursorCaptured = false;
    bool initialized = false;
    bool focused = true;
    bool iconified = false;

    DWORD windowedStyle = WS_OVERLAPPEDWINDOW;
    DWORD windowedExStyle = WS_EX_APPWINDOW;

    // Monitor info
    std::vector<HMONITOR> monitors;
    int primaryMonitorIndex = 0;

    // Cursor
    HCURSOR currentCursor = nullptr;
    HCURSOR arrowCursor = nullptr;
    HCURSOR ibeamCursor = nullptr;
    HCURSOR crosshairCursor = nullptr;
    HCURSOR handCursor = nullptr;
    HCURSOR hresizeCursor = nullptr;
    HCURSOR vresizeCursor = nullptr;

    // Cache
    mutable std::string cachedDeviceModel;
    mutable std::string cachedOSVersion;
    mutable std::string cachedLocale;
    mutable uint64_t cachedTotalMemory = 0;

    // Location
    GPSCoordinates lastLocation;
    LocationCallback locationCallback;
    LocationErrorCallback locationErrorCallback;
    bool locationUpdatesActive = false;

    // Lifecycle callbacks
    Platform::LifecycleCallbacks lifecycleCallbacks;
    Platform::StateChangeCallback stateCallback;

    // GLFW window (if using GLFW)
    GLFWwindow* glfwWindow = nullptr;

    static WindowsPlatformImpl* s_instance;
};

WindowsPlatformImpl* WindowsPlatformImpl::s_instance = nullptr;

// =============================================================================
// Utility Functions
// =============================================================================

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

static float GetDpiScaleForWindow(HWND hwnd) {
    // Try to get Per-Monitor DPI (Windows 10 1607+)
    typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);
    static auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFunc>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

    if (pGetDpiForWindow) {
        UINT dpi = pGetDpiForWindow(hwnd);
        return static_cast<float>(dpi) / 96.0f;
    }

    // Fallback to system DPI
    HDC hdc = GetDC(hwnd);
    float scale = static_cast<float>(GetDeviceCaps(hdc, LOGPIXELSX)) / 96.0f;
    ReleaseDC(hwnd, hdc);
    return scale;
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC /*hdcMonitor*/,
                                      LPRECT /*lprcMonitor*/, LPARAM dwData) {
    auto* impl = reinterpret_cast<WindowsPlatformImpl*>(dwData);
    impl->monitors.push_back(hMonitor);
    return TRUE;
}

// =============================================================================
// Window Procedure
// =============================================================================

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* impl = WindowsPlatformImpl::s_instance;

    switch (msg) {
        case WM_CREATE:
            return 0;

        case WM_CLOSE:
            if (impl) {
                impl->shouldClose = true;
                if (impl->lifecycleCallbacks.onTerminate) {
                    impl->lifecycleCallbacks.onTerminate();
                }
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            if (impl) {
                impl->windowSize = {LOWORD(lParam), HIWORD(lParam)};
                impl->framebufferSize = impl->windowSize;  // On Windows, framebuffer = window size
                impl->iconified = (wParam == SIZE_MINIMIZED);

                if (wParam == SIZE_MINIMIZED && impl->lifecycleCallbacks.onPause) {
                    impl->lifecycleCallbacks.onPause();
                } else if (wParam == SIZE_RESTORED && impl->lifecycleCallbacks.onResume) {
                    impl->lifecycleCallbacks.onResume();
                }
            }
            return 0;
        }

        case WM_MOVE:
            if (impl && !impl->fullscreen) {
                impl->windowPosition = {LOWORD(lParam), HIWORD(lParam)};
            }
            return 0;

        case WM_SETFOCUS:
            if (impl) {
                impl->focused = true;
                if (impl->lifecycleCallbacks.onResume) {
                    impl->lifecycleCallbacks.onResume();
                }
            }
            return 0;

        case WM_KILLFOCUS:
            if (impl) {
                impl->focused = false;
                if (impl->lifecycleCallbacks.onPause) {
                    impl->lifecycleCallbacks.onPause();
                }
            }
            return 0;

        case WM_DPICHANGED: {
            if (impl) {
                impl->displayScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
                RECT* suggested = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(hwnd, nullptr,
                            suggested->left, suggested->top,
                            suggested->right - suggested->left,
                            suggested->bottom - suggested->top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }

        case WM_GETMINMAXINFO: {
            LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lParam);
            mmi->ptMinTrackSize.x = 640;
            mmi->ptMinTrackSize.y = 480;
            return 0;
        }

        case WM_SYSCOMMAND:
            // Disable Alt+Enter default behavior
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;  // Prevent background erase flicker

        case WM_SETCURSOR:
            if (impl && LOWORD(lParam) == HTCLIENT) {
                SetCursor(impl->currentCursor ? impl->currentCursor : impl->arrowCursor);
                return TRUE;
            }
            break;

        case WM_DISPLAYCHANGE:
            // Monitor configuration changed
            if (impl) {
                impl->monitors.clear();
                EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(impl));
            }
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// =============================================================================
// WindowsPlatform Implementation
// =============================================================================

WindowsPlatform::WindowsPlatform()
    : m_impl(std::make_unique<WindowsPlatformImpl>()) {
    WindowsPlatformImpl::s_instance = m_impl.get();
}

WindowsPlatform::~WindowsPlatform() {
    Shutdown();
    if (WindowsPlatformImpl::s_instance == m_impl.get()) {
        WindowsPlatformImpl::s_instance = nullptr;
    }
}

bool WindowsPlatform::Initialize() {
    if (m_impl->initialized) {
        return true;
    }

    m_state = PlatformState::Starting;

    // Enable Per-Monitor DPI Awareness v2 (Windows 10 1703+)
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    auto pSetProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"));

    if (pSetProcessDpiAwarenessContext) {
        pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
        // Fallback to older API
        SetProcessDPIAware();
    }

    // Get module handle
    m_impl->hInstance = GetModuleHandleW(nullptr);

    // Register window class
    m_impl->windowClassName = L"Nova3DWindowClass";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_impl->hInstance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32517));  // IDI_APPLICATION
    wc.hIconSm = LoadIconW(nullptr, MAKEINTRESOURCEW(32517));  // IDI_APPLICATION
    wc.lpszClassName = m_impl->windowClassName.c_str();
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // Load standard cursors (using MAKEINTRESOURCEW for Unicode compatibility)
    m_impl->arrowCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    m_impl->ibeamCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32513));  // IDC_IBEAM
    m_impl->crosshairCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515));  // IDC_CROSS
    m_impl->handCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32649));  // IDC_HAND
    m_impl->hresizeCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32644));  // IDC_SIZEWE
    m_impl->vresizeCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32645));  // IDC_SIZENS
    m_impl->currentCursor = m_impl->arrowCursor;

    // Enumerate monitors
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(m_impl.get()));

#ifndef NOVA_NO_GLFW
    // Initialize GLFW as well (for OpenGL context)
    if (!glfwInit()) {
        // GLFW init failed, but we can still work with native WGL
    }
#endif

    m_impl->initialized = true;
    m_state = PlatformState::Running;

    return true;
}

void WindowsPlatform::Shutdown() {
    if (!m_impl->initialized) {
        return;
    }

    m_state = PlatformState::Terminating;

    DestroyWindow();

#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwDestroyWindow(m_impl->glfwWindow);
        m_impl->glfwWindow = nullptr;
    }
    glfwTerminate();
#endif

    UnregisterClassW(m_impl->windowClassName.c_str(), m_impl->hInstance);

    m_impl->initialized = false;
    m_state = PlatformState::Unknown;
}

bool WindowsPlatform::CreateWindow(const WindowConfig& config) {
#ifndef NOVA_NO_GLFW
    // Use GLFW for window creation (simplifies OpenGL context management)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, config.floating ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, config.highDPI ? GLFW_TRUE : GLFW_FALSE);

    if (config.samples > 0) {
        glfwWindowHint(GLFW_SAMPLES, config.samples);
    }

    GLFWmonitor* monitor = nullptr;
    if (config.fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        if (config.monitor.has_value()) {
            int count;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            if (config.monitor.value() < count) {
                monitor = monitors[config.monitor.value()];
            }
        }
    }

    m_impl->glfwWindow = glfwCreateWindow(
        config.width, config.height,
        config.title.c_str(),
        monitor, nullptr);

    if (!m_impl->glfwWindow) {
        return false;
    }

    glfwMakeContextCurrent(m_impl->glfwWindow);
    glfwSwapInterval(config.vsync ? 1 : 0);

    m_impl->hwnd = glfwGetWin32Window(m_impl->glfwWindow);
    m_impl->hdc = GetDC(m_impl->hwnd);

    glfwGetWindowSize(m_impl->glfwWindow, &m_impl->windowSize.x, &m_impl->windowSize.y);
    glfwGetFramebufferSize(m_impl->glfwWindow, &m_impl->framebufferSize.x, &m_impl->framebufferSize.y);
#else
    // Native Win32 window creation
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_APPWINDOW;

    if (!config.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (!config.decorated) {
        style = WS_POPUP;
    }
    if (config.floating) {
        exStyle |= WS_EX_TOPMOST;
    }

    m_impl->windowedStyle = style;
    m_impl->windowedExStyle = exStyle;

    // Calculate window size including borders
    RECT rect = {0, 0, config.width, config.height};
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // Center on primary monitor
    int x = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

    std::wstring titleW = StringToWString(config.title);

    m_impl->hwnd = CreateWindowExW(
        exStyle,
        m_impl->windowClassName.c_str(),
        titleW.c_str(),
        style,
        x, y, windowWidth, windowHeight,
        nullptr, nullptr,
        m_impl->hInstance, nullptr);

    if (!m_impl->hwnd) {
        return false;
    }

    m_impl->hdc = GetDC(m_impl->hwnd);

    if (config.visible) {
        ShowWindow(m_impl->hwnd, config.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    }

    GetClientRect(m_impl->hwnd, &rect);
    m_impl->windowSize = {rect.right, rect.bottom};
    m_impl->framebufferSize = m_impl->windowSize;
#endif

    m_impl->windowTitle = config.title;
    m_impl->fullscreen = config.fullscreen;
    m_impl->vsync = config.vsync;
    m_impl->windowedSize = {config.width, config.height};
    m_impl->displayScale = GetDpiScaleForWindow(m_impl->hwnd);

    return true;
}

void WindowsPlatform::DestroyWindow() {
    if (m_impl->hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_impl->hglrc);
        m_impl->hglrc = nullptr;
    }

    if (m_impl->hdc && m_impl->hwnd) {
        ReleaseDC(m_impl->hwnd, m_impl->hdc);
        m_impl->hdc = nullptr;
    }

#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwDestroyWindow(m_impl->glfwWindow);
        m_impl->glfwWindow = nullptr;
        m_impl->hwnd = nullptr;
    }
#else
    if (m_impl->hwnd) {
        ::DestroyWindow(m_impl->hwnd);
        m_impl->hwnd = nullptr;
    }
#endif
}

void WindowsPlatform::SwapBuffers() {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwSwapBuffers(m_impl->glfwWindow);
    }
#else
    if (m_impl->hdc) {
        ::SwapBuffers(m_impl->hdc);
    }
#endif
}

glm::ivec2 WindowsPlatform::GetWindowSize() const {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glm::ivec2 size;
        glfwGetWindowSize(m_impl->glfwWindow, &size.x, &size.y);
        return size;
    }
#endif
    return m_impl->windowSize;
}

glm::ivec2 WindowsPlatform::GetFramebufferSize() const {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glm::ivec2 size;
        glfwGetFramebufferSize(m_impl->glfwWindow, &size.x, &size.y);
        return size;
    }
#endif
    return m_impl->framebufferSize;
}

float WindowsPlatform::GetDisplayScale() const {
    return m_impl->displayScale;
}

void WindowsPlatform::SetFullscreen(bool fullscreen) {
    if (m_impl->fullscreen == fullscreen) {
        return;
    }

#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        if (fullscreen) {
            glfwGetWindowPos(m_impl->glfwWindow, &m_impl->windowedPosition.x, &m_impl->windowedPosition.y);
            glfwGetWindowSize(m_impl->glfwWindow, &m_impl->windowedSize.x, &m_impl->windowedSize.y);

            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(m_impl->glfwWindow, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(m_impl->glfwWindow, nullptr,
                                 m_impl->windowedPosition.x, m_impl->windowedPosition.y,
                                 m_impl->windowedSize.x, m_impl->windowedSize.y, 0);
        }
        m_impl->fullscreen = fullscreen;
        return;
    }
#endif

    // Native Win32 fullscreen toggle
    if (fullscreen) {
        // Save windowed state
        RECT rect;
        GetWindowRect(m_impl->hwnd, &rect);
        m_impl->windowedPosition = {rect.left, rect.top};
        m_impl->windowedSize = {rect.right - rect.left, rect.bottom - rect.top};

        // Get monitor info
        HMONITOR hMonitor = MonitorFromWindow(m_impl->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(hMonitor, &mi);

        // Set borderless fullscreen
        SetWindowLongPtrW(m_impl->hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(m_impl->hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED);
    } else {
        // Restore windowed state
        SetWindowLongPtrW(m_impl->hwnd, GWL_STYLE, m_impl->windowedStyle);
        SetWindowPos(m_impl->hwnd, HWND_NOTOPMOST,
                     m_impl->windowedPosition.x, m_impl->windowedPosition.y,
                     m_impl->windowedSize.x, m_impl->windowedSize.y,
                     SWP_FRAMECHANGED);
        ShowWindow(m_impl->hwnd, SW_SHOW);
    }

    m_impl->fullscreen = fullscreen;
}

void WindowsPlatform::SetWindowTitle(const std::string& title) {
    m_impl->windowTitle = title;
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwSetWindowTitle(m_impl->glfwWindow, title.c_str());
        return;
    }
#endif
    if (m_impl->hwnd) {
        SetWindowTextW(m_impl->hwnd, StringToWString(title).c_str());
    }
}

void WindowsPlatform::SetWindowSize(int width, int height) {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow && !m_impl->fullscreen) {
        glfwSetWindowSize(m_impl->glfwWindow, width, height);
        return;
    }
#endif
    if (m_impl->hwnd && !m_impl->fullscreen) {
        RECT rect = {0, 0, width, height};
        AdjustWindowRectEx(&rect, m_impl->windowedStyle, FALSE, m_impl->windowedExStyle);
        SetWindowPos(m_impl->hwnd, nullptr, 0, 0,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    }
}

void* WindowsPlatform::GetNativeWindowHandle() const {
    return m_impl->hwnd;
}

void* WindowsPlatform::GetNativeDisplayHandle() const {
    return m_impl->hdc;
}

void WindowsPlatform::PollEvents() {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwPollEvents();
        m_impl->shouldClose = glfwWindowShouldClose(m_impl->glfwWindow);
        return;
    }
#endif
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void WindowsPlatform::WaitEvents() {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwWaitEvents();
        m_impl->shouldClose = glfwWindowShouldClose(m_impl->glfwWindow);
        return;
    }
#endif
    WaitMessage();
    PollEvents();
}

void WindowsPlatform::WaitEventsTimeout(double timeout) {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwWaitEventsTimeout(timeout);
        m_impl->shouldClose = glfwWindowShouldClose(m_impl->glfwWindow);
        return;
    }
#endif
    MsgWaitForMultipleObjects(0, nullptr, FALSE,
                               static_cast<DWORD>(timeout * 1000), QS_ALLEVENTS);
    PollEvents();
}

bool WindowsPlatform::ShouldClose() const {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        return glfwWindowShouldClose(m_impl->glfwWindow);
    }
#endif
    return m_impl->shouldClose;
}

void WindowsPlatform::RequestClose() {
#ifndef NOVA_NO_GLFW
    if (m_impl->glfwWindow) {
        glfwSetWindowShouldClose(m_impl->glfwWindow, GLFW_TRUE);
        return;
    }
#endif
    m_impl->shouldClose = true;
    PostMessageW(m_impl->hwnd, WM_CLOSE, 0, 0);
}

// =============================================================================
// File System
// =============================================================================

std::string WindowsPlatform::GetDataPath() const {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return WStringToString(path) + "\\Nova3D\\";
    }
    return ".\\";
}

std::string WindowsPlatform::GetCachePath() const {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        return WStringToString(path) + "\\Nova3D\\Cache\\";
    }
    return ".\\Cache\\";
}

std::string WindowsPlatform::GetDocumentsPath() const {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, path))) {
        return WStringToString(path) + "\\";
    }
    return ".\\";
}

std::string WindowsPlatform::GetBundlePath() const {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring pathStr(path);
    size_t lastSlash = pathStr.rfind(L'\\');
    if (lastSlash != std::wstring::npos) {
        return WStringToString(pathStr.substr(0, lastSlash + 1));
    }
    return ".\\";
}

std::string WindowsPlatform::GetAssetsPath() const {
    return GetBundlePath() + "assets\\";
}

std::vector<uint8_t> WindowsPlatform::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

std::string WindowsPlatform::ReadFileAsString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool WindowsPlatform::WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool WindowsPlatform::WriteFile(const std::string& path, const std::string& content) {
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    return file.good();
}

bool WindowsPlatform::FileExists(const std::string& path) const {
    return fs::exists(path);
}

bool WindowsPlatform::IsDirectory(const std::string& path) const {
    return fs::is_directory(path);
}

bool WindowsPlatform::CreateDirectory(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec) || fs::exists(path);
}

bool WindowsPlatform::DeleteFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(path, ec) || !fs::exists(path);
}

std::vector<std::string> WindowsPlatform::ListFiles(const std::string& path, bool recursive) {
    std::vector<std::string> files;

    if (!fs::exists(path) || !fs::is_directory(path)) {
        return files;
    }

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Permission denied or other error
    }

    return files;
}

// =============================================================================
// Permissions (Desktop - no-ops)
// =============================================================================

void WindowsPlatform::RequestPermission(Permission permission, PermissionCallback callback) {
    if (callback) {
        callback(permission, PermissionResult::Granted);
    }
}

bool WindowsPlatform::HasPermission(Permission /*permission*/) const {
    return true;
}

PermissionResult WindowsPlatform::GetPermissionStatus(Permission /*permission*/) const {
    return PermissionResult::Granted;
}

void WindowsPlatform::OpenPermissionSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:privacy", nullptr, nullptr, SW_SHOWNORMAL);
}

// =============================================================================
// GPS/Location (Windows Location API)
// =============================================================================

bool WindowsPlatform::IsLocationAvailable() const {
    // Windows has location services, but may be disabled
    return true;
}

bool WindowsPlatform::IsLocationEnabled() const {
    // Check if location services are enabled in settings
    // This would require COM initialization and ILocationReport interface
    return true;
}

void WindowsPlatform::StartLocationUpdates(const LocationConfig& /*config*/,
                                            LocationCallback callback,
                                            LocationErrorCallback errorCallback) {
    m_impl->locationCallback = std::move(callback);
    m_impl->locationErrorCallback = std::move(errorCallback);
    m_impl->locationUpdatesActive = true;

    // Windows Location API requires COM - stub for now
    if (m_impl->locationErrorCallback) {
        m_impl->locationErrorCallback(1, "Windows Location API not implemented");
    }
}

void WindowsPlatform::StartLocationUpdates(LocationCallback callback) {
    StartLocationUpdates(LocationConfig{}, std::move(callback), nullptr);
}

void WindowsPlatform::StopLocationUpdates() {
    m_impl->locationUpdatesActive = false;
    m_impl->locationCallback = nullptr;
    m_impl->locationErrorCallback = nullptr;
}

void WindowsPlatform::RequestSingleLocation(LocationCallback callback) {
    if (callback) {
        callback(m_impl->lastLocation);
    }
}

GPSCoordinates WindowsPlatform::GetLastKnownLocation() const {
    return m_impl->lastLocation;
}

// =============================================================================
// System Information
// =============================================================================

uint64_t WindowsPlatform::GetAvailableMemory() const {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullAvailPhys;
    }
    return 0;
}

uint64_t WindowsPlatform::GetTotalMemory() const {
    if (m_impl->cachedTotalMemory == 0) {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            m_impl->cachedTotalMemory = memInfo.ullTotalPhys;
        }
    }
    return m_impl->cachedTotalMemory;
}

int WindowsPlatform::GetCPUCores() const {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return static_cast<int>(sysInfo.dwNumberOfProcessors);
}

std::string WindowsPlatform::GetCPUArchitecture() const {
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
        case PROCESSOR_ARCHITECTURE_ARM: return "arm";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        default: return "unknown";
    }
}

bool WindowsPlatform::HasGPUCompute() const {
    // Would check for DirectCompute, CUDA, or OpenCL support
    return true;
}

std::string WindowsPlatform::GetDeviceModel() const {
    if (m_impl->cachedDeviceModel.empty()) {
        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameW(computerName, &size)) {
            m_impl->cachedDeviceModel = WStringToString(computerName);
        } else {
            m_impl->cachedDeviceModel = "Windows PC";
        }
    }
    return m_impl->cachedDeviceModel;
}

std::string WindowsPlatform::GetOSVersion() const {
    if (m_impl->cachedOSVersion.empty()) {
        // Get Windows version from registry (more reliable)
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t productName[256] = {};
            wchar_t displayVersion[256] = {};
            wchar_t buildNumber[256] = {};
            DWORD size;

            size = sizeof(productName);
            RegQueryValueExW(hKey, L"ProductName", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(productName), &size);

            size = sizeof(displayVersion);
            RegQueryValueExW(hKey, L"DisplayVersion", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(displayVersion), &size);

            size = sizeof(buildNumber);
            RegQueryValueExW(hKey, L"CurrentBuildNumber", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(buildNumber), &size);

            RegCloseKey(hKey);

            m_impl->cachedOSVersion = WStringToString(productName);
            if (displayVersion[0]) {
                m_impl->cachedOSVersion += " " + WStringToString(displayVersion);
            }
            m_impl->cachedOSVersion += " (Build " + WStringToString(buildNumber) + ")";
        } else {
            m_impl->cachedOSVersion = "Windows";
        }
    }
    return m_impl->cachedOSVersion;
}

std::string WindowsPlatform::GetDeviceId() const {
    // Get machine GUID from registry
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Cryptography",
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        wchar_t machineGuid[256] = {};
        DWORD size = sizeof(machineGuid);
        RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(machineGuid), &size);
        RegCloseKey(hKey);
        return WStringToString(machineGuid);
    }
    return "";
}

std::string WindowsPlatform::GetLocale() const {
    if (m_impl->cachedLocale.empty()) {
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
            m_impl->cachedLocale = WStringToString(localeName);
        } else {
            m_impl->cachedLocale = "en-US";
        }
    }
    return m_impl->cachedLocale;
}

int WindowsPlatform::GetTimezoneOffset() const {
    TIME_ZONE_INFORMATION tzInfo;
    GetTimeZoneInformation(&tzInfo);
    return -static_cast<int>(tzInfo.Bias) * 60;  // Convert minutes to seconds
}

bool WindowsPlatform::HasHardwareFeature(const std::string& feature) const {
    if (feature == "sse" || feature == "SSE") {
        return IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE);
    }
    if (feature == "sse2" || feature == "SSE2") {
        return IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
    }
    if (feature == "sse3" || feature == "SSE3") {
        return IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE);
    }
    if (feature == "avx" || feature == "AVX") {
        return IsProcessorFeaturePresent(PF_AVX_INSTRUCTIONS_AVAILABLE);
    }
    if (feature == "avx2" || feature == "AVX2") {
        return IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE);
    }
    if (feature == "avx512" || feature == "AVX512") {
        return IsProcessorFeaturePresent(PF_AVX512F_INSTRUCTIONS_AVAILABLE);
    }
    if (feature == "arm_neon" || feature == "NEON") {
        return IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
    }
    return false;
}

// =============================================================================
// Battery/Network Status
// =============================================================================

float WindowsPlatform::GetBatteryLevel() const {
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        if (powerStatus.BatteryLifePercent != 255) {
            return static_cast<float>(powerStatus.BatteryLifePercent) / 100.0f;
        }
    }
    return -1.0f;  // No battery
}

bool WindowsPlatform::IsBatteryCharging() const {
    SYSTEM_POWER_STATUS powerStatus;
    if (GetSystemPowerStatus(&powerStatus)) {
        return (powerStatus.BatteryFlag & 8) != 0;  // Charging flag
    }
    return false;
}

bool WindowsPlatform::IsNetworkAvailable() const {
    DWORD flags;
    return InternetGetConnectedState(&flags, 0) != 0;
}

bool WindowsPlatform::IsWifiConnected() const {
    DWORD flags;
    if (InternetGetConnectedState(&flags, 0)) {
        return (flags & INTERNET_CONNECTION_LAN) != 0;  // Includes WiFi
    }
    return false;
}

bool WindowsPlatform::IsCellularConnected() const {
    DWORD flags;
    if (InternetGetConnectedState(&flags, 0)) {
        return (flags & INTERNET_CONNECTION_MODEM) != 0;
    }
    return false;
}

// =============================================================================
// Lifecycle & Haptics
// =============================================================================

void WindowsPlatform::SetLifecycleCallbacks(LifecycleCallbacks callbacks) {
    m_impl->lifecycleCallbacks = std::move(callbacks);
    m_lifecycleCallbacks = m_impl->lifecycleCallbacks;
}

void WindowsPlatform::TriggerHaptic(HapticType /*type*/) {
    // Windows doesn't have system-wide haptics
    // Could use game controller vibration via XInput
}

// =============================================================================
// Windows-Specific Extensions
// =============================================================================

HWND WindowsPlatform::GetHWND() const {
    return m_impl->hwnd;
}

HDC WindowsPlatform::GetHDC() const {
    return m_impl->hdc;
}

HGLRC WindowsPlatform::GetHGLRC() const {
    return m_impl->hglrc;
}

bool WindowsPlatform::SetClipboardText(const std::string& text) {
    if (!OpenClipboard(m_impl->hwnd)) {
        return false;
    }

    EmptyClipboard();

    std::wstring wtext = StringToWString(text);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wtext.size() + 1) * sizeof(wchar_t));
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    memcpy(GlobalLock(hMem), wtext.c_str(), (wtext.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();

    return true;
}

std::string WindowsPlatform::GetClipboardText() const {
    if (!OpenClipboard(m_impl->hwnd)) {
        return "";
    }

    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pText) {
            result = WStringToString(pText);
            GlobalUnlock(hData);
        }
    }

    CloseClipboard();
    return result;
}

std::string WindowsPlatform::OpenFileDialog(const std::string& title,
                                             const std::string& defaultPath,
                                             const std::vector<std::pair<std::string, std::string>>& filters) {
    wchar_t filename[MAX_PATH] = {};

    // Build filter string
    std::wstring filterStr;
    for (const auto& filter : filters) {
        filterStr += StringToWString(filter.first) + L'\0';
        filterStr += StringToWString(filter.second) + L'\0';
    }
    filterStr += L'\0';

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_impl->hwnd;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filterStr.c_str();
    ofn.lpstrTitle = StringToWString(title).c_str();
    ofn.lpstrInitialDir = StringToWString(defaultPath).c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return WStringToString(filename);
    }
    return "";
}

std::string WindowsPlatform::SaveFileDialog(const std::string& title,
                                             const std::string& defaultPath,
                                             const std::string& defaultName,
                                             const std::vector<std::pair<std::string, std::string>>& filters) {
    wchar_t filename[MAX_PATH] = {};
    wcscpy_s(filename, StringToWString(defaultName).c_str());

    std::wstring filterStr;
    for (const auto& filter : filters) {
        filterStr += StringToWString(filter.first) + L'\0';
        filterStr += StringToWString(filter.second) + L'\0';
    }
    filterStr += L'\0';

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_impl->hwnd;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filterStr.c_str();
    ofn.lpstrTitle = StringToWString(title).c_str();
    ofn.lpstrInitialDir = StringToWString(defaultPath).c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        return WStringToString(filename);
    }
    return "";
}

int WindowsPlatform::GetMonitorCount() const {
    return static_cast<int>(m_impl->monitors.size());
}

MonitorInfo WindowsPlatform::GetMonitorInfo(int index) const {
    MonitorInfo info;

    if (index < 0 || index >= static_cast<int>(m_impl->monitors.size())) {
        return info;
    }

    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(m_impl->monitors[index], &mi)) {
        info.name = WStringToString(mi.szDevice);
        info.x = mi.rcMonitor.left;
        info.y = mi.rcMonitor.top;
        info.width = mi.rcMonitor.right - mi.rcMonitor.left;
        info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
        info.workAreaX = mi.rcWork.left;
        info.workAreaY = mi.rcWork.top;
        info.workAreaWidth = mi.rcWork.right - mi.rcWork.left;
        info.workAreaHeight = mi.rcWork.bottom - mi.rcWork.top;
        info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

        // Get DPI for this monitor
        typedef HRESULT (WINAPI *GetDpiForMonitorFunc)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
        static auto pGetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFunc>(
            GetProcAddress(GetModuleHandleW(L"Shcore.dll"), "GetDpiForMonitor"));

        if (pGetDpiForMonitor) {
            UINT dpiX, dpiY;
            if (SUCCEEDED(pGetDpiForMonitor(m_impl->monitors[index], MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
                info.dpiScale = static_cast<float>(dpiX) / 96.0f;
            }
        }
    }

    return info;
}

void WindowsPlatform::SetCursorVisible(bool visible) {
    m_impl->cursorVisible = visible;
    ShowCursor(visible ? TRUE : FALSE);
}

void WindowsPlatform::SetCursorCaptured(bool captured) {
    m_impl->cursorCaptured = captured;
    if (captured && m_impl->hwnd) {
        RECT rect;
        GetClientRect(m_impl->hwnd, &rect);
        POINT pt = {rect.left, rect.top};
        ClientToScreen(m_impl->hwnd, &pt);
        rect.left = pt.x;
        rect.top = pt.y;
        pt = {rect.right, rect.bottom};
        ClientToScreen(m_impl->hwnd, &pt);
        rect.right = pt.x;
        rect.bottom = pt.y;
        ClipCursor(&rect);
    } else {
        ClipCursor(nullptr);
    }
}

// =============================================================================
// Platform Factory Implementation
// =============================================================================

std::unique_ptr<Platform> Platform::Create() {
    return std::make_unique<WindowsPlatform>();
}

PlatformType Platform::GetCurrentPlatform() noexcept {
    return PlatformType::Windows;
}

const char* Platform::GetPlatformName() noexcept {
    return "Windows";
}

bool Platform::IsDesktop() noexcept {
    return true;
}

bool Platform::IsMobile() noexcept {
    return false;
}

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
