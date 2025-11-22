#pragma once

/**
 * @file Platform.hpp
 * @brief Cross-platform abstraction layer for Nova3D Engine
 *
 * Provides a unified interface for platform-specific functionality including:
 * - Window management
 * - File system access
 * - System information
 * - Permissions (mobile)
 * - Location services (GPS)
 *
 * Supported platforms: Windows, Linux, macOS, iOS, Android, Web (Emscripten)
 */

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>

namespace Nova {

// =============================================================================
// Platform Types
// =============================================================================

/**
 * @brief Supported platform types
 */
enum class PlatformType {
    Windows,
    Linux,
    macOS,
    iOS,
    Android,
    Web  // Future: Emscripten/WebAssembly
};

/**
 * @brief Convert platform type to string
 */
constexpr const char* PlatformTypeToString(PlatformType type) noexcept {
    switch (type) {
        case PlatformType::Windows: return "Windows";
        case PlatformType::Linux:   return "Linux";
        case PlatformType::macOS:   return "macOS";
        case PlatformType::iOS:     return "iOS";
        case PlatformType::Android: return "Android";
        case PlatformType::Web:     return "Web";
        default:                    return "Unknown";
    }
}

/**
 * @brief Check if platform is a desktop platform
 */
constexpr bool IsDesktopPlatform(PlatformType type) noexcept {
    return type == PlatformType::Windows ||
           type == PlatformType::Linux ||
           type == PlatformType::macOS;
}

/**
 * @brief Check if platform is a mobile platform
 */
constexpr bool IsMobilePlatform(PlatformType type) noexcept {
    return type == PlatformType::iOS || type == PlatformType::Android;
}

/**
 * @brief Platform lifecycle state
 */
enum class PlatformState {
    Unknown,
    Starting,
    Running,
    Background,
    Foreground,
    Terminating
};

// =============================================================================
// Window Configuration
// =============================================================================

/**
 * @brief Window creation configuration
 */
struct WindowConfig {
    int width = 1920;
    int height = 1080;
    std::string title = "Nova3D Engine";
    bool fullscreen = false;
    bool resizable = true;
    bool vsync = true;
    int samples = 4;          // MSAA samples (0 = disabled)
    bool highDPI = true;      // Enable high DPI support
    bool decorated = true;    // Window decorations (title bar, borders)
    bool floating = false;    // Always on top
    bool maximized = false;   // Start maximized
    bool visible = true;      // Start visible
    int minWidth = 640;
    int minHeight = 480;
    int maxWidth = 0;         // 0 = no limit
    int maxHeight = 0;
    std::optional<int> monitor = std::nullopt;  // Target monitor for fullscreen
};

// =============================================================================
// Permission System (Mobile)
// =============================================================================

/**
 * @brief Permission types for mobile platforms
 */
enum class Permission {
    Camera,
    Microphone,
    Location,
    LocationAlways,      // Background location
    Storage,
    Photos,
    Contacts,
    Calendar,
    Notifications,
    Bluetooth,
    HealthData,          // iOS HealthKit
    MotionSensors,
    ExternalStorage,     // Android external storage
    Phone,               // Android phone state
    SMS,                 // Android SMS
    BackgroundLocation,  // Android background location
    FineLocation,        // Android fine location
    CoarseLocation       // Android coarse location
};

/**
 * @brief Permission request result
 */
enum class PermissionResult {
    Granted,
    Denied,
    DeniedPermanently,   // User selected "never ask again"
    Restricted,          // iOS restricted by parental controls
    NotDetermined,       // Permission not yet requested
    Error
};

/**
 * @brief Callback type for permission requests
 */
using PermissionCallback = std::function<void(Permission, PermissionResult)>;

// =============================================================================
// GPS/Location Services
// =============================================================================

/**
 * @brief GPS coordinate data
 */
struct GPSCoordinates {
    double latitude = 0.0;      // Degrees (-90 to 90)
    double longitude = 0.0;     // Degrees (-180 to 180)
    double altitude = 0.0;      // Meters above sea level
    float accuracy = 0.0f;      // Horizontal accuracy in meters
    float altitudeAccuracy = 0.0f;  // Vertical accuracy in meters
    float speed = 0.0f;         // Speed in m/s
    float bearing = 0.0f;       // Heading in degrees (0-360)
    uint64_t timestamp = 0;     // Unix timestamp in milliseconds
    bool valid = false;         // Whether the data is valid

    /**
     * @brief Calculate distance to another coordinate (Haversine formula)
     * @return Distance in meters
     */
    double DistanceTo(const GPSCoordinates& other) const noexcept;

    /**
     * @brief Calculate bearing to another coordinate
     * @return Bearing in degrees (0-360)
     */
    float BearingTo(const GPSCoordinates& other) const noexcept;
};

/**
 * @brief Location update configuration
 */
struct LocationConfig {
    float desiredAccuracy = 10.0f;      // Desired accuracy in meters
    float distanceFilter = 5.0f;        // Minimum distance before update (meters)
    float updateInterval = 1.0f;        // Update interval in seconds
    bool enableBackgroundUpdates = false;
    bool showsBackgroundLocationIndicator = true;  // iOS
};

/**
 * @brief Callback type for location updates
 */
using LocationCallback = std::function<void(const GPSCoordinates&)>;

/**
 * @brief Callback type for location errors
 */
using LocationErrorCallback = std::function<void(int errorCode, const std::string& message)>;

// =============================================================================
// Platform Interface
// =============================================================================

/**
 * @brief Abstract platform interface
 *
 * Provides a unified API for platform-specific functionality.
 * Use Platform::Create() to instantiate the correct implementation.
 */
class Platform {
public:
    virtual ~Platform() = default;

    // -------------------------------------------------------------------------
    // Factory Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Create platform instance for current platform
     * @return Unique pointer to platform implementation
     */
    static std::unique_ptr<Platform> Create();

    /**
     * @brief Get current platform type
     * @return PlatformType enum value
     */
    static PlatformType GetCurrentPlatform() noexcept;

    /**
     * @brief Get platform name as string
     */
    static const char* GetPlatformName() noexcept;

    /**
     * @brief Check if running on desktop platform
     */
    static bool IsDesktop() noexcept;

    /**
     * @brief Check if running on mobile platform
     */
    static bool IsMobile() noexcept;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize platform subsystems
     * @return true if initialization succeeded
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown platform subsystems
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Check if platform is initialized
     */
    [[nodiscard]] virtual bool IsInitialized() const noexcept = 0;

    /**
     * @brief Get the current platform state
     */
    [[nodiscard]] virtual PlatformState GetState() const = 0;

    // -------------------------------------------------------------------------
    // Window/Display Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create application window
     * @param config Window configuration
     * @return true if window was created successfully
     */
    virtual bool CreateWindow(const WindowConfig& config) = 0;

    /**
     * @brief Destroy application window
     */
    virtual void DestroyWindow() = 0;

    /**
     * @brief Check if window exists and is valid
     */
    [[nodiscard]] virtual bool HasWindow() const noexcept = 0;

    /**
     * @brief Swap front and back buffers
     */
    virtual void SwapBuffers() = 0;

    /**
     * @brief Get window size in screen coordinates
     */
    [[nodiscard]] virtual glm::ivec2 GetWindowSize() const = 0;

    /**
     * @brief Get framebuffer size in pixels (may differ on high-DPI displays)
     */
    [[nodiscard]] virtual glm::ivec2 GetFramebufferSize() const = 0;

    /**
     * @brief Get display scale factor (for high-DPI displays)
     */
    [[nodiscard]] virtual float GetDisplayScale() const = 0;

    /**
     * @brief Check if window is fullscreen
     */
    [[nodiscard]] virtual bool IsFullscreen() const = 0;

    /**
     * @brief Set fullscreen mode
     * @param fullscreen true for fullscreen, false for windowed
     */
    virtual void SetFullscreen(bool fullscreen) = 0;

    /**
     * @brief Set window title
     */
    virtual void SetWindowTitle(const std::string& title) = 0;

    /**
     * @brief Set window size
     */
    virtual void SetWindowSize(int width, int height) = 0;

    /**
     * @brief Get native window handle (HWND, NSWindow*, X11 Window, etc.)
     */
    [[nodiscard]] virtual void* GetNativeWindowHandle() const = 0;

    /**
     * @brief Get native display handle (HDC, Display*, etc.)
     */
    [[nodiscard]] virtual void* GetNativeDisplayHandle() const = 0;

    // -------------------------------------------------------------------------
    // Input/Events
    // -------------------------------------------------------------------------

    /**
     * @brief Process pending platform events
     */
    virtual void PollEvents() = 0;

    /**
     * @brief Wait for events (blocks until event occurs)
     */
    virtual void WaitEvents() = 0;

    /**
     * @brief Wait for events with timeout
     * @param timeout Maximum wait time in seconds
     */
    virtual void WaitEventsTimeout(double timeout) = 0;

    /**
     * @brief Check if window should close
     */
    [[nodiscard]] virtual bool ShouldClose() const = 0;

    /**
     * @brief Request window close
     */
    virtual void RequestClose() = 0;

    // -------------------------------------------------------------------------
    // File System
    // -------------------------------------------------------------------------

    /**
     * @brief Get application data directory
     *
     * Platform paths:
     * - Windows: %APPDATA%/AppName/
     * - Linux: ~/.local/share/AppName/
     * - macOS: ~/Library/Application Support/AppName/
     * - iOS: App sandbox Documents/
     * - Android: Internal storage data directory
     */
    [[nodiscard]] virtual std::string GetDataPath() const = 0;

    /**
     * @brief Get cache/temporary directory
     *
     * Platform paths:
     * - Windows: %LOCALAPPDATA%/Temp/
     * - Linux: ~/.cache/AppName/ or /tmp/
     * - macOS: ~/Library/Caches/AppName/
     * - iOS: App sandbox Caches/
     * - Android: Internal cache directory
     */
    [[nodiscard]] virtual std::string GetCachePath() const = 0;

    /**
     * @brief Get user documents directory
     *
     * Platform paths:
     * - Windows: %USERPROFILE%/Documents/
     * - Linux: ~/Documents/ (XDG)
     * - macOS: ~/Documents/
     * - iOS: App sandbox Documents/
     * - Android: External documents directory
     */
    [[nodiscard]] virtual std::string GetDocumentsPath() const = 0;

    /**
     * @brief Get application bundle/executable directory
     */
    [[nodiscard]] virtual std::string GetBundlePath() const = 0;

    /**
     * @brief Get assets/resources directory
     */
    [[nodiscard]] virtual std::string GetAssetsPath() const = 0;

    /**
     * @brief Read entire file into memory
     * @param path File path (relative or absolute)
     * @return File contents, empty vector on failure
     */
    [[nodiscard]] virtual std::vector<uint8_t> ReadFile(const std::string& path) = 0;

    /**
     * @brief Read file as string
     * @param path File path
     * @return File contents as string, empty on failure
     */
    [[nodiscard]] virtual std::string ReadFileAsString(const std::string& path) = 0;

    /**
     * @brief Write data to file
     * @param path File path
     * @param data Data to write
     * @return true if write succeeded
     */
    virtual bool WriteFile(const std::string& path, const std::vector<uint8_t>& data) = 0;

    /**
     * @brief Write string to file
     * @param path File path
     * @param content String content to write
     * @return true if write succeeded
     */
    virtual bool WriteFile(const std::string& path, const std::string& content) = 0;

    /**
     * @brief Check if file exists
     */
    [[nodiscard]] virtual bool FileExists(const std::string& path) const = 0;

    /**
     * @brief Check if path is a directory
     */
    [[nodiscard]] virtual bool IsDirectory(const std::string& path) const = 0;

    /**
     * @brief Create directory (and parent directories)
     * @return true if directory exists or was created
     */
    virtual bool CreateDirectory(const std::string& path) = 0;

    /**
     * @brief Delete file
     * @return true if file was deleted or didn't exist
     */
    virtual bool DeleteFile(const std::string& path) = 0;

    /**
     * @brief List files in directory
     * @param path Directory path
     * @param recursive Include subdirectories
     * @return List of file paths
     */
    [[nodiscard]] virtual std::vector<std::string> ListFiles(const std::string& path, bool recursive = false) = 0;

    // -------------------------------------------------------------------------
    // Permissions (Mobile)
    // -------------------------------------------------------------------------

    /**
     * @brief Request permission from user
     * @param permission Permission to request
     * @param callback Callback with result
     */
    virtual void RequestPermission(Permission permission, PermissionCallback callback) = 0;

    /**
     * @brief Check if permission is granted
     * @param permission Permission to check
     * @return true if permission is granted
     */
    [[nodiscard]] virtual bool HasPermission(Permission permission) const = 0;

    /**
     * @brief Get current permission status
     * @param permission Permission to check
     * @return Current permission status
     */
    [[nodiscard]] virtual PermissionResult GetPermissionStatus(Permission permission) const = 0;

    /**
     * @brief Open system settings for app permissions
     */
    virtual void OpenPermissionSettings() = 0;

    // -------------------------------------------------------------------------
    // GPS/Location Services
    // -------------------------------------------------------------------------

    /**
     * @brief Check if location services are available
     */
    [[nodiscard]] virtual bool IsLocationAvailable() const = 0;

    /**
     * @brief Check if location services are enabled system-wide
     */
    [[nodiscard]] virtual bool IsLocationEnabled() const = 0;

    /**
     * @brief Start receiving location updates
     * @param config Location configuration
     * @param callback Location update callback
     * @param errorCallback Error callback (optional)
     */
    virtual void StartLocationUpdates(
        const LocationConfig& config,
        LocationCallback callback,
        LocationErrorCallback errorCallback = nullptr) = 0;

    /**
     * @brief Start location updates with default configuration
     */
    virtual void StartLocationUpdates(LocationCallback callback) = 0;

    /**
     * @brief Stop receiving location updates
     */
    virtual void StopLocationUpdates() = 0;

    /**
     * @brief Request single location update
     * @param callback Callback with location
     */
    virtual void RequestSingleLocation(LocationCallback callback) = 0;

    /**
     * @brief Get last known location
     * @return Last cached location (may be stale or invalid)
     */
    [[nodiscard]] virtual GPSCoordinates GetLastKnownLocation() const = 0;

    // -------------------------------------------------------------------------
    // System Information
    // -------------------------------------------------------------------------

    /**
     * @brief Get available system memory in bytes
     */
    [[nodiscard]] virtual uint64_t GetAvailableMemory() const = 0;

    /**
     * @brief Get total system memory in bytes
     */
    [[nodiscard]] virtual uint64_t GetTotalMemory() const = 0;

    /**
     * @brief Get number of CPU cores
     */
    [[nodiscard]] virtual int GetCPUCores() const = 0;

    /**
     * @brief Get CPU architecture string
     */
    [[nodiscard]] virtual std::string GetCPUArchitecture() const = 0;

    /**
     * @brief Check if GPU compute (GPGPU) is available
     */
    [[nodiscard]] virtual bool HasGPUCompute() const = 0;

    /**
     * @brief Get device model name
     *
     * Examples:
     * - Windows: "Desktop PC"
     * - Linux: hostname or "Linux Desktop"
     * - macOS: "MacBook Pro (14-inch, 2023)"
     * - iOS: "iPhone 15 Pro Max"
     * - Android: "Samsung Galaxy S24"
     */
    [[nodiscard]] virtual std::string GetDeviceModel() const = 0;

    /**
     * @brief Get operating system version string
     *
     * Examples:
     * - "Windows 11 Build 22621"
     * - "Ubuntu 24.04"
     * - "macOS 14.2"
     * - "iOS 17.2"
     * - "Android 14"
     */
    [[nodiscard]] virtual std::string GetOSVersion() const = 0;

    /**
     * @brief Get unique device identifier (if available)
     * @note May be empty on some platforms due to privacy restrictions
     */
    [[nodiscard]] virtual std::string GetDeviceId() const = 0;

    /**
     * @brief Get current locale/language code
     * @return ISO language code (e.g., "en-US", "ja-JP")
     */
    [[nodiscard]] virtual std::string GetLocale() const = 0;

    /**
     * @brief Get current timezone offset in seconds
     */
    [[nodiscard]] virtual int GetTimezoneOffset() const = 0;

    /**
     * @brief Check if device has specific hardware feature
     */
    [[nodiscard]] virtual bool HasHardwareFeature(const std::string& feature) const = 0;

    // -------------------------------------------------------------------------
    // Battery Status (Mobile primarily)
    // -------------------------------------------------------------------------

    /**
     * @brief Get battery level (0.0 to 1.0)
     * @return Battery level, or -1.0 if not available
     */
    [[nodiscard]] virtual float GetBatteryLevel() const = 0;

    /**
     * @brief Check if device is charging
     */
    [[nodiscard]] virtual bool IsBatteryCharging() const = 0;

    // -------------------------------------------------------------------------
    // Network Status
    // -------------------------------------------------------------------------

    /**
     * @brief Check if network is available
     */
    [[nodiscard]] virtual bool IsNetworkAvailable() const = 0;

    /**
     * @brief Check if connected via WiFi
     */
    [[nodiscard]] virtual bool IsWifiConnected() const = 0;

    /**
     * @brief Check if connected via cellular
     */
    [[nodiscard]] virtual bool IsCellularConnected() const = 0;

    // -------------------------------------------------------------------------
    // App Lifecycle (Mobile)
    // -------------------------------------------------------------------------

    /**
     * @brief Callbacks for app lifecycle events
     */
    struct LifecycleCallbacks {
        std::function<void()> onPause;       // App moving to background
        std::function<void()> onResume;      // App returning to foreground
        std::function<void()> onLowMemory;   // Low memory warning
        std::function<void()> onTerminate;   // App being terminated
    };

    /**
     * @brief Set lifecycle callbacks
     */
    virtual void SetLifecycleCallbacks(LifecycleCallbacks callbacks) = 0;

    // -------------------------------------------------------------------------
    // Haptic Feedback (Mobile)
    // -------------------------------------------------------------------------

    /**
     * @brief Haptic feedback types
     */
    enum class HapticType {
        Light,
        Medium,
        Heavy,
        Selection,
        Success,
        Warning,
        Error
    };

    /**
     * @brief Trigger haptic feedback
     */
    virtual void TriggerHaptic(HapticType type) = 0;

    /**
     * @brief Check if haptics are available
     */
    [[nodiscard]] virtual bool HasHaptics() const = 0;

    // -------------------------------------------------------------------------
    // Legacy Callbacks (for backward compatibility)
    // -------------------------------------------------------------------------

    using StateChangeCallback = std::function<void(PlatformState newState)>;
    using LocationUpdateCallback = std::function<void(const GPSCoordinates& coords)>;
    using MemoryWarningCallback = std::function<void()>;

    void SetStateChangeCallback(StateChangeCallback callback) { m_stateCallback = std::move(callback); }
    void SetLocationUpdateCallback(LocationUpdateCallback callback) { m_locationCallback = std::move(callback); }
    void SetMemoryWarningCallback(MemoryWarningCallback callback) { m_memoryCallback = std::move(callback); }

protected:
    Platform() = default;

    // Prevent copying
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    // Callbacks
    StateChangeCallback m_stateCallback;
    LocationUpdateCallback m_locationCallback;
    MemoryWarningCallback m_memoryCallback;
    LifecycleCallbacks m_lifecycleCallbacks;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get compile-time platform type
 */
constexpr PlatformType GetCompiledPlatform() noexcept {
#if defined(NOVA_PLATFORM_WINDOWS)
    return PlatformType::Windows;
#elif defined(NOVA_PLATFORM_LINUX)
    return PlatformType::Linux;
#elif defined(NOVA_PLATFORM_IOS)
    return PlatformType::iOS;
#elif defined(NOVA_PLATFORM_MACOS)
    return PlatformType::macOS;
#elif defined(NOVA_PLATFORM_ANDROID)
    return PlatformType::Android;
#elif defined(NOVA_PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    return PlatformType::Web;
#else
    return PlatformType::Linux;  // Default to Linux for unknown Unix-like systems
#endif
}

/**
 * @brief Factory function to create the appropriate platform implementation
 * @deprecated Use Platform::Create() instead
 */
[[deprecated("Use Platform::Create() instead")]]
inline std::unique_ptr<Platform> CreatePlatform() {
    return Platform::Create();
}

} // namespace Nova
