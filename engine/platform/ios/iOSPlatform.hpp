#pragma once

/**
 * @file iOSPlatform.hpp
 * @brief iOS-specific platform implementation (stub)
 *
 * Full implementation would use UIKit with Metal.
 */

#include "../Platform.hpp"

#ifdef NOVA_PLATFORM_IOS

namespace Nova {

/**
 * @brief iOS platform implementation
 *
 * Features:
 * - UIKit integration
 * - Metal rendering
 * - Core Location for GPS
 * - Full touch and sensor support
 * - App lifecycle handling
 */
class iOSPlatform : public Platform {
public:
    iOSPlatform();
    ~iOSPlatform() override;

    // Lifecycle
    bool Initialize() override;
    void Shutdown() override;
    [[nodiscard]] bool IsInitialized() const noexcept override { return m_initialized; }
    [[nodiscard]] PlatformState GetState() const override { return m_state; }

    // Window/View Management
    bool CreateWindow(const WindowConfig& config) override;  // Creates UIWindow
    void DestroyWindow() override;
    [[nodiscard]] bool HasWindow() const noexcept override;
    void SwapBuffers() override;

    [[nodiscard]] glm::ivec2 GetWindowSize() const override;
    [[nodiscard]] glm::ivec2 GetFramebufferSize() const override;
    [[nodiscard]] float GetDisplayScale() const override;  // UIScreen.scale
    [[nodiscard]] bool IsFullscreen() const override { return true; }  // Always fullscreen
    void SetFullscreen(bool fullscreen) override;  // No-op on iOS
    void SetWindowTitle(const std::string& title) override;  // No-op
    void SetWindowSize(int width, int height) override;  // No-op

    [[nodiscard]] void* GetNativeWindowHandle() const override;  // UIWindow*
    [[nodiscard]] void* GetNativeDisplayHandle() const override; // CAMetalLayer*

    // Input/Events
    void PollEvents() override;
    void WaitEvents() override;
    void WaitEventsTimeout(double timeout) override;
    [[nodiscard]] bool ShouldClose() const override;
    void RequestClose() override;

    // File System (iOS sandbox)
    [[nodiscard]] std::string GetDataPath() const override;      // App sandbox
    [[nodiscard]] std::string GetCachePath() const override;     // Caches/
    [[nodiscard]] std::string GetDocumentsPath() const override; // Documents/
    [[nodiscard]] std::string GetBundlePath() const override;    // .app bundle
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

    // Permissions (iOS permission system)
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
    [[nodiscard]] std::string GetCPUArchitecture() const override;  // arm64
    [[nodiscard]] bool HasGPUCompute() const override;
    [[nodiscard]] std::string GetDeviceModel() const override;  // "iPhone15,2"
    [[nodiscard]] std::string GetOSVersion() const override;    // "iOS 17.2"
    [[nodiscard]] std::string GetDeviceId() const override;     // identifierForVendor
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
    [[nodiscard]] bool HasHaptics() const override { return true; }

    // iOS-specific
    void HandleAppDidBecomeActive();
    void HandleAppWillResignActive();
    void HandleAppDidEnterBackground();
    void HandleAppWillEnterForeground();
    void HandleAppWillTerminate();
    void HandleMemoryWarning();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;  // PIMPL for Obj-C types

    bool m_initialized = false;
    PlatformState m_state = PlatformState::Unknown;
};

} // namespace Nova

#endif // NOVA_PLATFORM_IOS
