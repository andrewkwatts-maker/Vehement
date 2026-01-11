#pragma once

/**
 * @file LinuxPlatform.hpp
 * @brief Linux-specific platform implementation
 *
 * Uses GLFW for windowing with X11/Wayland support.
 * Implements XDG Base Directory Specification for paths.
 */

#include "../Platform.hpp"
#include <GLFW/glfw3.h>

#ifdef NOVA_PLATFORM_LINUX

// Forward declarations for X11/Wayland (avoid including headers)
struct _XDisplay;
typedef struct _XDisplay Display;
struct wl_display;

namespace Nova {

/**
 * @brief Linux platform implementation
 *
 * Features:
 * - GLFW-based windowing (X11 and Wayland)
 * - XDG Base Directory paths
 * - procfs-based system info
 * - GeoClue2 location services (D-Bus)
 */
class LinuxPlatform : public Platform {
public:
    LinuxPlatform();
    ~LinuxPlatform() override;

    // -------------------------------------------------------------------------
    // Factory & Static
    // -------------------------------------------------------------------------

    /**
     * @brief Check if we're running on Wayland
     */
    static bool IsWaylandSession() noexcept;

    /**
     * @brief Check if we're running on X11
     */
    static bool IsX11Session() noexcept;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    bool Initialize() override;
    void Shutdown() override;
    [[nodiscard]] bool IsInitialized() const noexcept override { return m_initialized; }
    [[nodiscard]] PlatformState GetState() const override { return m_state; }

    // -------------------------------------------------------------------------
    // Window Management
    // -------------------------------------------------------------------------

    bool CreateWindow(const WindowConfig& config) override;
    void DestroyWindow() override;
    [[nodiscard]] bool HasWindow() const noexcept override { return m_window != nullptr; }
    void SwapBuffers() override;

    [[nodiscard]] glm::ivec2 GetWindowSize() const override;
    [[nodiscard]] glm::ivec2 GetFramebufferSize() const override;
    [[nodiscard]] float GetDisplayScale() const override;
    [[nodiscard]] bool IsFullscreen() const override { return m_fullscreen; }
    void SetFullscreen(bool fullscreen) override;
    void SetWindowTitle(const std::string& title) override;
    void SetWindowSize(int width, int height) override;

    [[nodiscard]] void* GetNativeWindowHandle() const override;
    [[nodiscard]] void* GetNativeDisplayHandle() const override;

    // -------------------------------------------------------------------------
    // Input/Events
    // -------------------------------------------------------------------------

    void PollEvents() override;
    void WaitEvents() override;
    void WaitEventsTimeout(double timeout) override;
    [[nodiscard]] bool ShouldClose() const override;
    void RequestClose() override;

    // -------------------------------------------------------------------------
    // File System (XDG paths)
    // -------------------------------------------------------------------------

    [[nodiscard]] std::string GetDataPath() const override;
    [[nodiscard]] std::string GetCachePath() const override;
    [[nodiscard]] std::string GetDocumentsPath() const override;
    [[nodiscard]] std::string GetBundlePath() const override;
    [[nodiscard]] std::string GetAssetsPath() const override;

    [[nodiscard]] std::vector<uint8_t> ReadFile(const std::string& path) override;
    [[nodiscard]] std::string ReadFileAsString(const std::string& path) override;
    bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) override;
    bool WriteFile(const std::string& path, const std::string& content) override;
    [[nodiscard]] bool FileExists(const std::string& path) const override;
    [[nodiscard]] bool IsDirectory(const std::string& path) const override;
    bool CreateDirectory(const std::string& path) override;
    bool DeleteFile(const std::string& path) override;
    [[nodiscard]] std::vector<std::string> ListFiles(const std::string& path, bool recursive = false) override;

    // -------------------------------------------------------------------------
    // Permissions (Desktop - mostly no-ops)
    // -------------------------------------------------------------------------

    void RequestPermission(Permission permission, PermissionCallback callback) override;
    [[nodiscard]] bool HasPermission(Permission permission) const override;
    [[nodiscard]] PermissionResult GetPermissionStatus(Permission permission) const override;
    void OpenPermissionSettings() override;

    // -------------------------------------------------------------------------
    // GPS/Location (GeoClue2 via D-Bus)
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsLocationAvailable() const override;
    [[nodiscard]] bool IsLocationEnabled() const override;
    void StartLocationUpdates(const LocationConfig& config, LocationCallback callback,
                             LocationErrorCallback errorCallback = nullptr) override;
    void StartLocationUpdates(LocationCallback callback) override;
    void StopLocationUpdates() override;
    void RequestSingleLocation(LocationCallback callback) override;
    [[nodiscard]] GPSCoordinates GetLastKnownLocation() const override;

    // -------------------------------------------------------------------------
    // System Information
    // -------------------------------------------------------------------------

    [[nodiscard]] uint64_t GetAvailableMemory() const override;
    [[nodiscard]] uint64_t GetTotalMemory() const override;
    [[nodiscard]] int GetCPUCores() const override;
    [[nodiscard]] std::string GetCPUArchitecture() const override;
    [[nodiscard]] bool HasGPUCompute() const override;
    [[nodiscard]] std::string GetDeviceModel() const override;
    [[nodiscard]] std::string GetOSVersion() const override;
    [[nodiscard]] std::string GetDeviceId() const override;
    [[nodiscard]] std::string GetLocale() const override;
    [[nodiscard]] int GetTimezoneOffset() const override;
    [[nodiscard]] bool HasHardwareFeature(const std::string& feature) const override;

    // -------------------------------------------------------------------------
    // Battery Status
    // -------------------------------------------------------------------------

    [[nodiscard]] float GetBatteryLevel() const override;
    [[nodiscard]] bool IsBatteryCharging() const override;

    // -------------------------------------------------------------------------
    // Network Status
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsNetworkAvailable() const override;
    [[nodiscard]] bool IsWifiConnected() const override;
    [[nodiscard]] bool IsCellularConnected() const override;

    // -------------------------------------------------------------------------
    // App Lifecycle
    // -------------------------------------------------------------------------

    void SetLifecycleCallbacks(LifecycleCallbacks callbacks) override;

    // -------------------------------------------------------------------------
    // Haptics (not supported on desktop)
    // -------------------------------------------------------------------------

    void TriggerHaptic(HapticType type) override;
    [[nodiscard]] bool HasHaptics() const override { return false; }

    // -------------------------------------------------------------------------
    // Linux-Specific
    // -------------------------------------------------------------------------

    /**
     * @brief Get GLFW window handle
     */
    [[nodiscard]] GLFWwindow* GetGLFWWindow() const noexcept { return m_window; }

    /**
     * @brief Get X11 Display pointer (if X11 session)
     */
    [[nodiscard]] Display* GetX11Display() const;

    /**
     * @brief Get X11 Window handle (if X11 session)
     */
    [[nodiscard]] unsigned long GetX11Window() const;

    /**
     * @brief Get Wayland display (if Wayland session)
     */
    [[nodiscard]] wl_display* GetWaylandDisplay() const;

    /**
     * @brief Set window hints for Wayland
     */
    void SetWaylandHints();

    /**
     * @brief Set window hints for X11
     */
    void SetX11Hints();

private:
    // GLFW callbacks
    static void ErrorCallback(int error, const char* description);
    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowFocusCallback(GLFWwindow* window, int focused);
    static void WindowCloseCallback(GLFWwindow* window);
    static void WindowIconifyCallback(GLFWwindow* window, int iconified);

    void SetupCallbacks();
    void UpdateDisplayScale();
    std::string GetXDGPath(const char* envVar, const char* defaultSubpath) const;
    void InitializeLocationServices();
    void ShutdownLocationServices();

    // Window state
    GLFWwindow* m_window = nullptr;
    glm::ivec2 m_windowSize{1920, 1080};
    glm::ivec2 m_framebufferSize{1920, 1080};
    glm::ivec2 m_windowedSize{1920, 1080};
    glm::ivec2 m_windowedPos{100, 100};
    float m_displayScale = 1.0f;
    bool m_fullscreen = false;
    bool m_focused = true;
    bool m_iconified = false;
    std::string m_title = "Nova3D Engine";

    // Platform state
    PlatformState m_state = PlatformState::Unknown;
    bool m_initialized = false;
    bool m_glfwInitialized = false;

    // Location services
    GPSCoordinates m_lastLocation;
    LocationCallback m_locationCallback;
    LocationErrorCallback m_locationErrorCallback;
    bool m_locationUpdatesActive = false;

    // Cached system info
    mutable std::string m_cachedOSVersion;
    mutable std::string m_cachedHostname;
    mutable uint64_t m_cachedTotalMemory = 0;
};

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
