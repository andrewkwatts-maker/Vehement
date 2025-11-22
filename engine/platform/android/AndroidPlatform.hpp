#pragma once

/**
 * @file AndroidPlatform.hpp
 * @brief Android platform abstraction for Nova3D Engine
 *
 * Provides Android-specific platform functionality including:
 * - Native activity integration
 * - Surface management
 * - Touch input handling
 * - GPS/Location services
 * - Asset loading from APK
 * - Lifecycle management
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <glm/glm.hpp>

// Android NDK headers
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <android/sensor.h>
#include <android/log.h>
#include <android/configuration.h>

namespace Nova {

// Forward declarations
class AndroidGLES;
class VulkanRenderer;
class AndroidTouchInput;

/**
 * @brief GPS coordinate structure
 */
struct GPSCoordinates {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    float accuracy = 0.0f;
    float bearing = 0.0f;
    float speed = 0.0f;
    int64_t timestamp = 0;
    bool valid = false;
};

/**
 * @brief Android platform lifecycle states
 */
enum class AndroidLifecycleState {
    Created,
    Started,
    Resumed,
    Paused,
    Stopped,
    Destroyed
};

/**
 * @brief Graphics backend selection
 */
enum class AndroidGraphicsBackend {
    OpenGLES,
    Vulkan,
    Auto  // Prefer Vulkan if available, fallback to GLES
};

/**
 * @brief Platform interface base class
 */
class Platform {
public:
    virtual ~Platform() = default;

    virtual bool Initialize(ANativeActivity* activity) = 0;
    virtual void Shutdown() = 0;
    virtual void CreateSurface() = 0;
    virtual void SwapBuffers() = 0;
    virtual glm::ivec2 GetScreenSize() const = 0;
    virtual float GetDisplayDensity() const = 0;
};

/**
 * @brief Android platform abstraction for Nova3D Engine
 *
 * Manages all Android-specific systems including:
 * - Native window and surface
 * - Graphics context (GLES/Vulkan)
 * - Touch input
 * - GPS location services
 * - Asset loading
 * - Lifecycle events
 */
class AndroidPlatform : public Platform {
public:
    /**
     * @brief Platform configuration
     */
    struct Config {
        AndroidGraphicsBackend graphicsBackend = AndroidGraphicsBackend::Auto;
        int glesVersionMajor = 3;
        int glesVersionMinor = 0;
        bool enableValidationLayers = false;  // Vulkan validation
        bool enableGPS = false;
        float locationUpdateInterval = 1000.0f;  // milliseconds
        float locationMinDistance = 1.0f;  // meters
    };

    /**
     * @brief Lifecycle callbacks
     */
    struct LifecycleCallbacks {
        std::function<void()> onPause;
        std::function<void()> onResume;
        std::function<void()> onDestroy;
        std::function<void(int, int)> onSurfaceChanged;
        std::function<void()> onLowMemory;
        std::function<void(const GPSCoordinates&)> onLocationUpdate;
    };

    // Singleton access
    [[nodiscard]] static AndroidPlatform& Instance() noexcept;

    // Delete copy/move - singleton pattern
    AndroidPlatform(const AndroidPlatform&) = delete;
    AndroidPlatform& operator=(const AndroidPlatform&) = delete;
    AndroidPlatform(AndroidPlatform&&) = delete;
    AndroidPlatform& operator=(AndroidPlatform&&) = delete;

    // -------------------------------------------------------------------------
    // Platform Interface Implementation
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize Android-specific systems
     * @param activity The Android native activity
     * @return true if initialization succeeded
     */
    bool Initialize(ANativeActivity* activity) override;

    /**
     * @brief Initialize with configuration
     * @param activity The Android native activity
     * @param config Platform configuration
     * @return true if initialization succeeded
     */
    bool Initialize(ANativeActivity* activity, const Config& config);

    /**
     * @brief Shutdown all systems and release resources
     */
    void Shutdown() override;

    /**
     * @brief Create rendering surface from native window
     */
    void CreateSurface() override;

    /**
     * @brief Swap front and back buffers
     */
    void SwapBuffers() override;

    /**
     * @brief Get screen dimensions
     * @return Screen size in pixels
     */
    [[nodiscard]] glm::ivec2 GetScreenSize() const override;

    /**
     * @brief Get display density (DPI)
     * @return Density value
     */
    [[nodiscard]] float GetDisplayDensity() const override;

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle input event from Android
     * @param event The input event
     * @return 1 if event was handled, 0 otherwise
     */
    int32_t HandleInputEvent(AInputEvent* event);

    /**
     * @brief Get the touch input handler
     */
    [[nodiscard]] AndroidTouchInput* GetTouchInput() const { return m_touchInput.get(); }

    // -------------------------------------------------------------------------
    // Lifecycle Management
    // -------------------------------------------------------------------------

    /**
     * @brief Called when activity is paused
     */
    void OnPause();

    /**
     * @brief Called when activity is resumed
     */
    void OnResume();

    /**
     * @brief Called when activity is destroyed
     */
    void OnDestroy();

    /**
     * @brief Called when surface dimensions change
     * @param width New width in pixels
     * @param height New height in pixels
     */
    void OnSurfaceChanged(int width, int height);

    /**
     * @brief Called when system is low on memory
     */
    void OnLowMemory();

    /**
     * @brief Set native window handle
     * @param window The native window
     */
    void SetNativeWindow(ANativeWindow* window);

    /**
     * @brief Get current lifecycle state
     */
    [[nodiscard]] AndroidLifecycleState GetLifecycleState() const { return m_lifecycleState; }

    /**
     * @brief Check if platform is ready for rendering
     */
    [[nodiscard]] bool IsReady() const;

    /**
     * @brief Set lifecycle callbacks
     */
    void SetLifecycleCallbacks(LifecycleCallbacks callbacks) {
        m_lifecycleCallbacks = std::move(callbacks);
    }

    // -------------------------------------------------------------------------
    // GPS/Location Services
    // -------------------------------------------------------------------------

    /**
     * @brief Request location permission from user
     */
    void RequestLocationPermission();

    /**
     * @brief Check if location permission is granted
     */
    [[nodiscard]] bool HasLocationPermission() const { return m_hasLocationPermission; }

    /**
     * @brief Start receiving location updates
     */
    void StartLocationUpdates();

    /**
     * @brief Stop receiving location updates
     */
    void StopLocationUpdates();

    /**
     * @brief Get the most recent GPS coordinates
     */
    [[nodiscard]] GPSCoordinates GetCurrentLocation() const;

    /**
     * @brief Set location from JNI callback
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param altitude Altitude in meters
     * @param accuracy Accuracy in meters
     * @param timestamp Time in milliseconds
     */
    void SetLocation(double latitude, double longitude, double altitude = 0.0,
                     float accuracy = 0.0f, int64_t timestamp = 0);

    // -------------------------------------------------------------------------
    // Asset Loading
    // -------------------------------------------------------------------------

    /**
     * @brief Load an asset file from the APK
     * @param path Path relative to assets folder
     * @return File contents as byte vector
     */
    [[nodiscard]] std::vector<uint8_t> LoadAsset(const std::string& path);

    /**
     * @brief Load asset as string
     * @param path Path relative to assets folder
     * @return File contents as string
     */
    [[nodiscard]] std::string LoadAssetString(const std::string& path);

    /**
     * @brief Check if an asset exists
     * @param path Path relative to assets folder
     * @return true if asset exists
     */
    [[nodiscard]] bool AssetExists(const std::string& path);

    /**
     * @brief List files in an asset directory
     * @param path Directory path relative to assets folder
     * @return List of file names
     */
    [[nodiscard]] std::vector<std::string> ListAssetDirectory(const std::string& path);

    // -------------------------------------------------------------------------
    // System Information
    // -------------------------------------------------------------------------

    /**
     * @brief Get Android API level
     */
    [[nodiscard]] int GetAPILevel() const { return m_apiLevel; }

    /**
     * @brief Get device model string
     */
    [[nodiscard]] const std::string& GetDeviceModel() const { return m_deviceModel; }

    /**
     * @brief Get active graphics backend
     */
    [[nodiscard]] AndroidGraphicsBackend GetActiveBackend() const { return m_activeBackend; }

    /**
     * @brief Check if Vulkan is available
     */
    [[nodiscard]] bool IsVulkanAvailable() const { return m_vulkanAvailable; }

    /**
     * @brief Get the native activity
     */
    [[nodiscard]] ANativeActivity* GetActivity() const { return m_activity; }

    /**
     * @brief Get the asset manager
     */
    [[nodiscard]] AAssetManager* GetAssetManager() const { return m_assetManager; }

    /**
     * @brief Get the native window
     */
    [[nodiscard]] ANativeWindow* GetNativeWindow() const { return m_window; }

    /**
     * @brief Get the GLES context (nullptr if using Vulkan)
     */
    [[nodiscard]] AndroidGLES* GetGLES() const { return m_gles.get(); }

    /**
     * @brief Get the Vulkan renderer (nullptr if using GLES)
     */
    [[nodiscard]] VulkanRenderer* GetVulkanRenderer() const { return m_vulkanRenderer.get(); }

    // -------------------------------------------------------------------------
    // Utility Functions
    // -------------------------------------------------------------------------

    /**
     * @brief Convert screen coordinates to normalized device coordinates
     */
    [[nodiscard]] glm::vec2 ScreenToNDC(const glm::vec2& screenPos) const;

    /**
     * @brief Get internal storage path (app-specific)
     */
    [[nodiscard]] std::string GetInternalStoragePath() const;

    /**
     * @brief Get external storage path (if available)
     */
    [[nodiscard]] std::string GetExternalStoragePath() const;

    /**
     * @brief Check if external storage is available
     */
    [[nodiscard]] bool IsExternalStorageAvailable() const { return m_externalStorageAvailable; }

    /**
     * @brief Log message to Android logcat
     */
    static void Log(int priority, const char* tag, const char* format, ...);

private:
    AndroidPlatform() = default;
    ~AndroidPlatform();

    bool InitializeGraphics();
    void ShutdownGraphics();
    bool CheckVulkanSupport();
    void QuerySystemInfo();
    void SetupSensors();
    void ShutdownSensors();

    // Android handles
    ANativeActivity* m_activity = nullptr;
    ANativeWindow* m_window = nullptr;
    AAssetManager* m_assetManager = nullptr;
    AConfiguration* m_config = nullptr;

    // Graphics backends
    std::unique_ptr<AndroidGLES> m_gles;
    std::unique_ptr<VulkanRenderer> m_vulkanRenderer;
    AndroidGraphicsBackend m_activeBackend = AndroidGraphicsBackend::OpenGLES;
    bool m_vulkanAvailable = false;

    // Touch input
    std::unique_ptr<AndroidTouchInput> m_touchInput;

    // Screen info
    glm::ivec2 m_screenSize{0, 0};
    float m_displayDensity = 1.0f;
    float m_displayScale = 1.0f;

    // GPS/Location
    mutable std::mutex m_locationMutex;
    GPSCoordinates m_currentLocation;
    bool m_hasLocationPermission = false;
    bool m_locationUpdatesActive = false;

    // System info
    int m_apiLevel = 0;
    std::string m_deviceModel;
    std::string m_internalStoragePath;
    std::string m_externalStoragePath;
    bool m_externalStorageAvailable = false;

    // State
    Config m_config_settings;
    AndroidLifecycleState m_lifecycleState = AndroidLifecycleState::Created;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_surfaceReady{false};

    // Callbacks
    LifecycleCallbacks m_lifecycleCallbacks;
};

// Convenience macros for Android logging
#define NOVA_ANDROID_LOG_TAG "Nova3D"
#define NOVA_LOGV(...) AndroidPlatform::Log(ANDROID_LOG_VERBOSE, NOVA_ANDROID_LOG_TAG, __VA_ARGS__)
#define NOVA_LOGD(...) AndroidPlatform::Log(ANDROID_LOG_DEBUG, NOVA_ANDROID_LOG_TAG, __VA_ARGS__)
#define NOVA_LOGI(...) AndroidPlatform::Log(ANDROID_LOG_INFO, NOVA_ANDROID_LOG_TAG, __VA_ARGS__)
#define NOVA_LOGW(...) AndroidPlatform::Log(ANDROID_LOG_WARN, NOVA_ANDROID_LOG_TAG, __VA_ARGS__)
#define NOVA_LOGE(...) AndroidPlatform::Log(ANDROID_LOG_ERROR, NOVA_ANDROID_LOG_TAG, __VA_ARGS__)

} // namespace Nova
