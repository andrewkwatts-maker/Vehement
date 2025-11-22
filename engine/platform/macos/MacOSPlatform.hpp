#pragma once

/**
 * @file MacOSPlatform.hpp
 * @brief macOS-specific platform implementation (stub)
 *
 * Full implementation would use Cocoa/AppKit with Metal or OpenGL.
 */

#include "../Platform.hpp"

#ifdef NOVA_PLATFORM_MACOS

namespace Nova {

/**
 * @brief macOS platform implementation
 *
 * Features:
 * - Cocoa/AppKit integration
 * - Metal or OpenGL rendering
 * - macOS-specific file system
 * - Core Location for GPS
 */
class MacOSPlatform : public Platform {
public:
    MacOSPlatform();
    ~MacOSPlatform() override;

    // Lifecycle
    bool Initialize() override;
    void Shutdown() override;
    [[nodiscard]] bool IsInitialized() const noexcept override { return m_initialized; }
    [[nodiscard]] PlatformState GetState() const override { return m_state; }

    // Window Management
    bool CreateWindow(const WindowConfig& config) override;
    void DestroyWindow() override;
    [[nodiscard]] bool HasWindow() const noexcept override;
    void SwapBuffers() override;

    [[nodiscard]] glm::ivec2 GetWindowSize() const override;
    [[nodiscard]] glm::ivec2 GetFramebufferSize() const override;
    [[nodiscard]] float GetDisplayScale() const override;
    [[nodiscard]] bool IsFullscreen() const override;
    void SetFullscreen(bool fullscreen) override;
    void SetWindowTitle(const std::string& title) override;
    void SetWindowSize(int width, int height) override;

    [[nodiscard]] void* GetNativeWindowHandle() const override;  // NSWindow*
    [[nodiscard]] void* GetNativeDisplayHandle() const override;

    // Input/Events
    void PollEvents() override;
    void WaitEvents() override;
    void WaitEventsTimeout(double timeout) override;
    [[nodiscard]] bool ShouldClose() const override;
    void RequestClose() override;

    // File System (macOS paths)
    [[nodiscard]] std::string GetDataPath() const override;      // ~/Library/Application Support/
    [[nodiscard]] std::string GetCachePath() const override;     // ~/Library/Caches/
    [[nodiscard]] std::string GetDocumentsPath() const override; // ~/Documents/
    [[nodiscard]] std::string GetBundlePath() const override;    // .app bundle
    [[nodiscard]] std::string GetAssetsPath() const override;    // Resources/

    [[nodiscard]] std::vector<uint8_t> ReadFile(const std::string& path) override;
    [[nodiscard]] std::string ReadFileAsString(const std::string& path) override;
    bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) override;
    bool WriteFile(const std::string& path, const std::string& content) override;
    [[nodiscard]] bool FileExists(const std::string& path) const override;
    [[nodiscard]] bool IsDirectory(const std::string& path) const override;
    bool CreateDirectory(const std::string& path) override;
    bool DeleteFile(const std::string& path) override;
    [[nodiscard]] std::vector<std::string> ListFiles(const std::string& path, bool recursive) override;

    // Permissions
    void RequestPermission(Permission permission, PermissionCallback callback) override;
    [[nodiscard]] bool HasPermission(Permission permission) const override;
    [[nodiscard]] PermissionResult GetPermissionStatus(Permission permission) const override;
    void OpenPermissionSettings() override;

    // GPS/Location (Core Location)
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
    [[nodiscard]] std::string GetCPUArchitecture() const override;  // arm64 or x86_64
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
    [[nodiscard]] bool HasHaptics() const override;  // Force Touch trackpad

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;  // PIMPL for Obj-C types

    bool m_initialized = false;
    PlatformState m_state = PlatformState::Unknown;
};

} // namespace Nova

#endif // NOVA_PLATFORM_MACOS
