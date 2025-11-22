#include "AndroidPlatform.hpp"
#include "AndroidGLES.hpp"
#include "VulkanRenderer.hpp"
#include "AndroidTouchInput.hpp"

#include <cstdarg>
#include <cstring>
#include <dlfcn.h>
#include <sys/system_properties.h>

namespace Nova {

// -------------------------------------------------------------------------
// Singleton Implementation
// -------------------------------------------------------------------------

AndroidPlatform& AndroidPlatform::Instance() noexcept {
    static AndroidPlatform instance;
    return instance;
}

AndroidPlatform::~AndroidPlatform() {
    Shutdown();
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------

bool AndroidPlatform::Initialize(ANativeActivity* activity) {
    return Initialize(activity, Config{});
}

bool AndroidPlatform::Initialize(ANativeActivity* activity, const Config& config) {
    if (m_initialized) {
        NOVA_LOGW("AndroidPlatform already initialized");
        return true;
    }

    if (!activity) {
        NOVA_LOGE("Invalid ANativeActivity pointer");
        return false;
    }

    m_activity = activity;
    m_assetManager = activity->assetManager;
    m_config_settings = config;

    // Create configuration
    m_config = AConfiguration_new();
    if (m_config) {
        AConfiguration_fromAssetManager(m_config, m_assetManager);
    }

    // Query system information
    QuerySystemInfo();

    // Store paths
    if (activity->internalDataPath) {
        m_internalStoragePath = activity->internalDataPath;
    }
    if (activity->externalDataPath) {
        m_externalStoragePath = activity->externalDataPath;
        m_externalStorageAvailable = true;
    }

    // Check Vulkan availability
    m_vulkanAvailable = CheckVulkanSupport();
    NOVA_LOGI("Vulkan support: %s", m_vulkanAvailable ? "available" : "not available");

    // Determine graphics backend
    if (config.graphicsBackend == AndroidGraphicsBackend::Auto) {
        m_activeBackend = m_vulkanAvailable ? AndroidGraphicsBackend::Vulkan
                                            : AndroidGraphicsBackend::OpenGLES;
    } else if (config.graphicsBackend == AndroidGraphicsBackend::Vulkan && !m_vulkanAvailable) {
        NOVA_LOGW("Vulkan requested but not available, falling back to OpenGL ES");
        m_activeBackend = AndroidGraphicsBackend::OpenGLES;
    } else {
        m_activeBackend = config.graphicsBackend;
    }

    // Create touch input handler
    m_touchInput = std::make_unique<AndroidTouchInput>();

    m_lifecycleState = AndroidLifecycleState::Created;
    m_initialized = true;

    NOVA_LOGI("AndroidPlatform initialized successfully");
    NOVA_LOGI("  API Level: %d", m_apiLevel);
    NOVA_LOGI("  Device: %s", m_deviceModel.c_str());
    NOVA_LOGI("  Backend: %s", m_activeBackend == AndroidGraphicsBackend::Vulkan ? "Vulkan" : "OpenGL ES");

    return true;
}

void AndroidPlatform::Shutdown() {
    if (!m_initialized) {
        return;
    }

    NOVA_LOGI("Shutting down AndroidPlatform");

    // Stop location updates
    if (m_locationUpdatesActive) {
        StopLocationUpdates();
    }

    // Shutdown graphics
    ShutdownGraphics();

    // Release touch input
    m_touchInput.reset();

    // Release configuration
    if (m_config) {
        AConfiguration_delete(m_config);
        m_config = nullptr;
    }

    m_activity = nullptr;
    m_window = nullptr;
    m_assetManager = nullptr;
    m_initialized = false;
    m_surfaceReady = false;

    m_lifecycleState = AndroidLifecycleState::Destroyed;

    NOVA_LOGI("AndroidPlatform shutdown complete");
}

// -------------------------------------------------------------------------
// Graphics Management
// -------------------------------------------------------------------------

bool AndroidPlatform::InitializeGraphics() {
    if (!m_window) {
        NOVA_LOGE("Cannot initialize graphics: no native window");
        return false;
    }

    if (m_activeBackend == AndroidGraphicsBackend::Vulkan) {
        m_vulkanRenderer = std::make_unique<VulkanRenderer>();
        if (!m_vulkanRenderer->Initialize(m_window)) {
            NOVA_LOGW("Vulkan initialization failed, falling back to OpenGL ES");
            m_vulkanRenderer.reset();
            m_activeBackend = AndroidGraphicsBackend::OpenGLES;
        }
    }

    if (m_activeBackend == AndroidGraphicsBackend::OpenGLES) {
        m_gles = std::make_unique<AndroidGLES>();
        if (!m_gles->Initialize(m_window)) {
            NOVA_LOGE("OpenGL ES initialization failed");
            m_gles.reset();
            return false;
        }
    }

    // Get initial screen size from window
    m_screenSize.x = ANativeWindow_getWidth(m_window);
    m_screenSize.y = ANativeWindow_getHeight(m_window);

    m_surfaceReady = true;
    return true;
}

void AndroidPlatform::ShutdownGraphics() {
    m_surfaceReady = false;

    if (m_gles) {
        m_gles->Shutdown();
        m_gles.reset();
    }

    if (m_vulkanRenderer) {
        m_vulkanRenderer->Shutdown();
        m_vulkanRenderer.reset();
    }
}

void AndroidPlatform::CreateSurface() {
    if (!m_window) {
        NOVA_LOGE("Cannot create surface: no native window");
        return;
    }

    if (m_gles) {
        m_gles->CreateSurface(m_window);
    }
    // Vulkan surface is created during initialization
}

void AndroidPlatform::SwapBuffers() {
    if (m_gles) {
        m_gles->SwapBuffers();
    } else if (m_vulkanRenderer) {
        m_vulkanRenderer->EndFrame();
    }
}

bool AndroidPlatform::CheckVulkanSupport() {
    // Check if Vulkan library is available
    void* vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!vulkanLib) {
        return false;
    }

    // Check if vkCreateInstance is available
    auto vkCreateInstance = dlsym(vulkanLib, "vkCreateInstance");
    bool available = (vkCreateInstance != nullptr);

    dlclose(vulkanLib);

    // Additional check: API level 24+ required for good Vulkan support
    return available && m_apiLevel >= 24;
}

// -------------------------------------------------------------------------
// Screen Information
// -------------------------------------------------------------------------

glm::ivec2 AndroidPlatform::GetScreenSize() const {
    return m_screenSize;
}

float AndroidPlatform::GetDisplayDensity() const {
    return m_displayDensity;
}

glm::vec2 AndroidPlatform::ScreenToNDC(const glm::vec2& screenPos) const {
    if (m_screenSize.x == 0 || m_screenSize.y == 0) {
        return glm::vec2(0.0f);
    }

    return glm::vec2(
        (screenPos.x / static_cast<float>(m_screenSize.x)) * 2.0f - 1.0f,
        1.0f - (screenPos.y / static_cast<float>(m_screenSize.y)) * 2.0f
    );
}

// -------------------------------------------------------------------------
// Input Handling
// -------------------------------------------------------------------------

int32_t AndroidPlatform::HandleInputEvent(AInputEvent* event) {
    if (!event || !m_touchInput) {
        return 0;
    }

    int32_t eventType = AInputEvent_getType(event);

    if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        return m_touchInput->HandleMotionEvent(event);
    }

    return 0;
}

// -------------------------------------------------------------------------
// Lifecycle Management
// -------------------------------------------------------------------------

void AndroidPlatform::OnPause() {
    NOVA_LOGI("AndroidPlatform::OnPause");
    m_lifecycleState = AndroidLifecycleState::Paused;

    // Pause location updates
    if (m_locationUpdatesActive) {
        // Location updates continue in background but callbacks are paused
    }

    if (m_lifecycleCallbacks.onPause) {
        m_lifecycleCallbacks.onPause();
    }
}

void AndroidPlatform::OnResume() {
    NOVA_LOGI("AndroidPlatform::OnResume");
    m_lifecycleState = AndroidLifecycleState::Resumed;

    if (m_lifecycleCallbacks.onResume) {
        m_lifecycleCallbacks.onResume();
    }
}

void AndroidPlatform::OnDestroy() {
    NOVA_LOGI("AndroidPlatform::OnDestroy");

    if (m_lifecycleCallbacks.onDestroy) {
        m_lifecycleCallbacks.onDestroy();
    }

    Shutdown();
}

void AndroidPlatform::OnSurfaceChanged(int width, int height) {
    NOVA_LOGI("AndroidPlatform::OnSurfaceChanged: %dx%d", width, height);

    m_screenSize.x = width;
    m_screenSize.y = height;

    if (m_gles) {
        m_gles->ResizeSurface(width, height);
    } else if (m_vulkanRenderer) {
        // Vulkan swapchain recreation handled internally
    }

    if (m_lifecycleCallbacks.onSurfaceChanged) {
        m_lifecycleCallbacks.onSurfaceChanged(width, height);
    }
}

void AndroidPlatform::OnLowMemory() {
    NOVA_LOGW("AndroidPlatform::OnLowMemory");

    // Release non-essential resources
    if (m_lifecycleCallbacks.onLowMemory) {
        m_lifecycleCallbacks.onLowMemory();
    }
}

void AndroidPlatform::SetNativeWindow(ANativeWindow* window) {
    if (m_window == window) {
        return;
    }

    // Shutdown existing graphics if window is changing
    if (m_window && m_surfaceReady) {
        ShutdownGraphics();
    }

    m_window = window;

    // Initialize graphics with new window
    if (m_window && m_initialized) {
        InitializeGraphics();
    }
}

bool AndroidPlatform::IsReady() const {
    return m_initialized && m_surfaceReady && m_window != nullptr;
}

// -------------------------------------------------------------------------
// GPS/Location Services
// -------------------------------------------------------------------------

void AndroidPlatform::RequestLocationPermission() {
    // This is handled through JNI - the Java side must request permission
    // The result will be set via JNI callback
    NOVA_LOGI("Location permission request should be handled via JNI");
}

void AndroidPlatform::StartLocationUpdates() {
    if (!m_hasLocationPermission) {
        NOVA_LOGW("Cannot start location updates: permission not granted");
        return;
    }

    if (m_locationUpdatesActive) {
        return;
    }

    // Location updates are started via JNI
    m_locationUpdatesActive = true;
    NOVA_LOGI("Location updates started");
}

void AndroidPlatform::StopLocationUpdates() {
    if (!m_locationUpdatesActive) {
        return;
    }

    // Location updates are stopped via JNI
    m_locationUpdatesActive = false;
    NOVA_LOGI("Location updates stopped");
}

GPSCoordinates AndroidPlatform::GetCurrentLocation() const {
    std::lock_guard<std::mutex> lock(m_locationMutex);
    return m_currentLocation;
}

void AndroidPlatform::SetLocation(double latitude, double longitude, double altitude,
                                   float accuracy, int64_t timestamp) {
    std::lock_guard<std::mutex> lock(m_locationMutex);

    m_currentLocation.latitude = latitude;
    m_currentLocation.longitude = longitude;
    m_currentLocation.altitude = altitude;
    m_currentLocation.accuracy = accuracy;
    m_currentLocation.timestamp = timestamp;
    m_currentLocation.valid = true;

    // Invoke callback on main thread
    if (m_lifecycleCallbacks.onLocationUpdate) {
        m_lifecycleCallbacks.onLocationUpdate(m_currentLocation);
    }
}

// -------------------------------------------------------------------------
// Asset Loading
// -------------------------------------------------------------------------

std::vector<uint8_t> AndroidPlatform::LoadAsset(const std::string& path) {
    std::vector<uint8_t> result;

    if (!m_assetManager) {
        NOVA_LOGE("Asset manager not available");
        return result;
    }

    AAsset* asset = AAssetManager_open(m_assetManager, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) {
        NOVA_LOGE("Failed to open asset: %s", path.c_str());
        return result;
    }

    off_t size = AAsset_getLength(asset);
    if (size > 0) {
        result.resize(static_cast<size_t>(size));
        AAsset_read(asset, result.data(), result.size());
    }

    AAsset_close(asset);
    return result;
}

std::string AndroidPlatform::LoadAssetString(const std::string& path) {
    auto data = LoadAsset(path);
    if (data.empty()) {
        return "";
    }
    return std::string(data.begin(), data.end());
}

bool AndroidPlatform::AssetExists(const std::string& path) {
    if (!m_assetManager) {
        return false;
    }

    AAsset* asset = AAssetManager_open(m_assetManager, path.c_str(), AASSET_MODE_UNKNOWN);
    if (asset) {
        AAsset_close(asset);
        return true;
    }
    return false;
}

std::vector<std::string> AndroidPlatform::ListAssetDirectory(const std::string& path) {
    std::vector<std::string> result;

    if (!m_assetManager) {
        return result;
    }

    AAssetDir* dir = AAssetManager_openDir(m_assetManager, path.c_str());
    if (!dir) {
        return result;
    }

    const char* filename;
    while ((filename = AAssetDir_getNextFileName(dir)) != nullptr) {
        result.emplace_back(filename);
    }

    AAssetDir_close(dir);
    return result;
}

// -------------------------------------------------------------------------
// System Information
// -------------------------------------------------------------------------

void AndroidPlatform::QuerySystemInfo() {
    // Get API level
    char sdkVersion[PROP_VALUE_MAX];
    if (__system_property_get("ro.build.version.sdk", sdkVersion) > 0) {
        m_apiLevel = std::atoi(sdkVersion);
    }

    // Get device model
    char model[PROP_VALUE_MAX];
    if (__system_property_get("ro.product.model", model) > 0) {
        m_deviceModel = model;
    }

    // Get display density from configuration
    if (m_config) {
        int density = AConfiguration_getDensity(m_config);
        if (density > 0) {
            m_displayDensity = static_cast<float>(density) / 160.0f;  // DENSITY_DEFAULT = 160
            m_displayScale = m_displayDensity;
        }
    }
}

std::string AndroidPlatform::GetInternalStoragePath() const {
    return m_internalStoragePath;
}

std::string AndroidPlatform::GetExternalStoragePath() const {
    return m_externalStoragePath;
}

// -------------------------------------------------------------------------
// Logging
// -------------------------------------------------------------------------

void AndroidPlatform::Log(int priority, const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    __android_log_vprint(priority, tag, format, args);
    va_end(args);
}

} // namespace Nova
