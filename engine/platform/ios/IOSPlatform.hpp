#pragma once

#include "../Platform.hpp"
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <atomic>

namespace Nova {

// Forward declarations
class IOSGLContext;
class MetalRenderer;
class IOSTouchInput;

/**
 * @brief Rendering API selection for iOS
 */
enum class IOSRenderingAPI {
    OpenGLES,   // OpenGL ES 3.0 (wider compatibility)
    Metal       // Metal (higher performance, iOS 8+)
};

/**
 * @brief iOS platform implementation
 *
 * Provides iOS-specific functionality including:
 * - CAMetalLayer / EAGLContext management
 * - Touch input handling
 * - GPS/CoreLocation integration
 * - App lifecycle management (background/foreground)
 * - Retina display support
 */
class IOSPlatform : public Platform {
public:
    IOSPlatform();
    ~IOSPlatform() override;

    // Non-copyable
    IOSPlatform(const IOSPlatform&) = delete;
    IOSPlatform& operator=(const IOSPlatform&) = delete;

    // =========================================================================
    // Platform Interface Implementation
    // =========================================================================

    bool Initialize() override;
    void Shutdown() override;
    [[nodiscard]] PlatformState GetState() const override { return m_state; }
    void ProcessEvents() override;

    void CreateWindow(int width, int height) override;
    void SwapBuffers() override;
    [[nodiscard]] glm::ivec2 GetScreenSize() const override;
    [[nodiscard]] glm::ivec2 GetFramebufferSize() const override;
    [[nodiscard]] float GetDisplayScale() const override;
    [[nodiscard]] bool SupportsFeature(const std::string& feature) const override;

    void RequestLocationPermission() override;
    void StartLocationUpdates() override;
    void StopLocationUpdates() override;
    [[nodiscard]] GPSCoordinates GetCurrentLocation() const override;
    [[nodiscard]] bool IsLocationAvailable() const override;

    [[nodiscard]] std::string GetPlatformName() const override { return "iOS"; }
    [[nodiscard]] std::string GetOSVersion() const override;
    [[nodiscard]] std::string GetDeviceModel() const override;

    // =========================================================================
    // iOS-Specific Methods
    // =========================================================================

    /**
     * @brief Set the rendering API (must be called before Initialize)
     */
    void SetRenderingAPI(IOSRenderingAPI api) { m_renderingAPI = api; }
    [[nodiscard]] IOSRenderingAPI GetRenderingAPI() const { return m_renderingAPI; }

    /**
     * @brief Set the native view (UIView*) - called from Objective-C code
     */
    void SetNativeView(void* view);

    /**
     * @brief Get the Metal layer (CAMetalLayer*)
     */
    [[nodiscard]] void* GetMetalLayer() const { return m_metalLayer; }

    /**
     * @brief Get the OpenGL ES context (EAGLContext*)
     */
    [[nodiscard]] void* GetGLContext() const { return m_glContext; }

    /**
     * @brief Get the GL context wrapper
     */
    [[nodiscard]] IOSGLContext* GetGLContextWrapper() const { return m_glContextWrapper.get(); }

    /**
     * @brief Get the Metal renderer
     */
    [[nodiscard]] MetalRenderer* GetMetalRenderer() const { return m_metalRenderer.get(); }

    /**
     * @brief Get the touch input handler
     */
    [[nodiscard]] IOSTouchInput* GetTouchInput() const { return m_touchInput.get(); }

    // =========================================================================
    // Touch Input (called from Objective-C touch handlers)
    // =========================================================================

    void HandleTouchBegan(float x, float y, int touchId);
    void HandleTouchMoved(float x, float y, int touchId);
    void HandleTouchEnded(float x, float y, int touchId);
    void HandleTouchCancelled(float x, float y, int touchId);

    // =========================================================================
    // App Lifecycle (called from AppDelegate)
    // =========================================================================

    void OnEnterBackground();
    void OnEnterForeground();
    void OnMemoryWarning();
    void OnWillTerminate();
    void OnDidBecomeActive();
    void OnWillResignActive();

    // =========================================================================
    // Location Updates (called from CLLocationManagerDelegate)
    // =========================================================================

    void OnLocationUpdate(double latitude, double longitude, double altitude,
                          double accuracy, double timestamp);
    void OnLocationError(int errorCode);

    // =========================================================================
    // Display Properties
    // =========================================================================

    /**
     * @brief Get safe area insets (notch, home indicator)
     */
    [[nodiscard]] glm::vec4 GetSafeAreaInsets() const;

    /**
     * @brief Get current device orientation
     */
    [[nodiscard]] int GetDeviceOrientation() const;

    /**
     * @brief Check if device supports haptic feedback
     */
    [[nodiscard]] bool SupportsHaptics() const;

    /**
     * @brief Trigger haptic feedback
     */
    void TriggerHaptic(int type);

private:
    // Objective-C bridge - these hold Objective-C objects
    void* m_nativeView = nullptr;       // UIView*
    void* m_metalLayer = nullptr;       // CAMetalLayer*
    void* m_glContext = nullptr;        // EAGLContext*
    void* m_locationManager = nullptr;  // CLLocationManager*
    void* m_hapticEngine = nullptr;     // CHHapticEngine*

    // C++ wrappers
    std::unique_ptr<IOSGLContext> m_glContextWrapper;
    std::unique_ptr<MetalRenderer> m_metalRenderer;
    std::unique_ptr<IOSTouchInput> m_touchInput;

    // State
    PlatformState m_state = PlatformState::Unknown;
    IOSRenderingAPI m_renderingAPI = IOSRenderingAPI::Metal;

    // Display properties
    glm::ivec2 m_screenSize{0, 0};
    glm::ivec2 m_framebufferSize{0, 0};
    float m_displayScale = 1.0f;
    glm::vec4 m_safeAreaInsets{0.0f};

    // Location
    mutable GPSCoordinates m_currentLocation;
    std::atomic<bool> m_locationUpdatesActive{false};
    std::atomic<bool> m_locationAvailable{false};

    // Helpers
    void InitializeOpenGLES();
    void InitializeMetal();
    void InitializeLocationServices();
    void CleanupLocationServices();
    void UpdateDisplayMetrics();
};

} // namespace Nova
