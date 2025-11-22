#pragma once

/**
 * @file WindowsPlatform.hpp
 * @brief Windows-specific platform implementation (stub)
 *
 * Full implementation would use Win32 API with GLFW for windowing.
 */

#include "../Platform.hpp"

#ifdef NOVA_PLATFORM_WINDOWS

#include <GLFW/glfw3.h>

namespace Nova {

/**
 * @brief Windows platform implementation
 *
 * Features:
 * - GLFW-based windowing
 * - Win32 API integration
 * - DirectX/OpenGL/Vulkan support
 */
class WindowsPlatform : public Platform {
public:
    WindowsPlatform();
    ~WindowsPlatform() override;

    // Lifecycle
    bool Initialize() override;
    void Shutdown() override;
    [[nodiscard]] bool IsInitialized() const noexcept override { return m_initialized; }
    [[nodiscard]] PlatformState GetState() const override { return m_state; }

    // Window Management
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

    // Input/Events
    void PollEvents() override;
    void WaitEvents() override;
    void WaitEventsTimeout(double timeout) override;
    [[nodiscard]] bool ShouldClose() const override;
    void RequestClose() override;

    // File System (Win32 paths)
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
    [[nodiscard]] std::vector<std::string> ListFiles(const std::string& path, bool recursive) override;

    // Permissions (Desktop - no-ops)
    void RequestPermission(Permission permission, PermissionCallback callback) override;
    [[nodiscard]] bool HasPermission(Permission permission) const override;
    [[nodiscard]] PermissionResult GetPermissionStatus(Permission permission) const override;
    void OpenPermissionSettings() override;

    // GPS/Location
    [[nodiscard]] bool IsLocationAvailable() const override;
    [[nodiscard]] bool IsLocationEnabled() const override;
    void StartLocationUpdates(const LocationConfig& config, LocationCallback callback,
                             LocationErrorCallback errorCallback) override;
    void StartLocationUpdates(LocationCallback callback) override;
    void StopLocationUpdates() override;
    void RequestSingleLocation(LocationCallback callback) override;
    [[nodiscard]] GPSCoordinates GetLastKnownLocation() const override;

    // System Information
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

    // Battery/Network
    [[nodiscard]] float GetBatteryLevel() const override;
    [[nodiscard]] bool IsBatteryCharging() const override;
    [[nodiscard]] bool IsNetworkAvailable() const override;
    [[nodiscard]] bool IsWifiConnected() const override;
    [[nodiscard]] bool IsCellularConnected() const override;

    // Lifecycle & Haptics
    void SetLifecycleCallbacks(LifecycleCallbacks callbacks) override;
    void TriggerHaptic(HapticType type) override;
    [[nodiscard]] bool HasHaptics() const override { return false; }

    // Windows-specific
    [[nodiscard]] GLFWwindow* GetGLFWWindow() const noexcept { return m_window; }

private:
    GLFWwindow* m_window = nullptr;
    glm::ivec2 m_windowSize{1920, 1080};
    glm::ivec2 m_framebufferSize{1920, 1080};
    float m_displayScale = 1.0f;
    bool m_fullscreen = false;
    bool m_initialized = false;
    PlatformState m_state = PlatformState::Unknown;
    GPSCoordinates m_lastLocation;
};

} // namespace Nova

#endif // NOVA_PLATFORM_WINDOWS
