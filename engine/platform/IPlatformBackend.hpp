#pragma once

/**
 * @file IPlatformBackend.hpp
 * @brief Platform backend abstraction interface for Vehement SDF Engine
 *
 * Provides a unified interface for platform-specific graphics backends:
 * - WindowsGL (Windows OpenGL)
 * - LinuxVulkan (Linux Vulkan with X11/Wayland)
 * - VulkanRenderer (Android Vulkan)
 * - AndroidGLES (Android OpenGL ES)
 * - Metal (macOS/iOS)
 * - WebGPU (Web)
 *
 * Features:
 * - Capability querying for feature detection
 * - Native handle access for interoperability
 * - Platform-specific configuration
 * - Automatic backend selection
 * - Backend registration system
 */

#include "PlatformDetect.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <optional>

namespace Nova {

// =============================================================================
// Forward Declarations
// =============================================================================

class IPlatformBackend;

// =============================================================================
// Platform and Graphics API Types
// =============================================================================

/**
 * @brief Supported platform types
 */
enum class PlatformType : uint8_t {
    Windows,
    Linux,
    macOS,
    iOS,
    Android,
    Web
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
 * @brief Get current compile-time platform type
 */
constexpr PlatformType GetCurrentPlatformType() noexcept {
#if defined(NOVA_PLATFORM_WINDOWS)
    return PlatformType::Windows;
#elif defined(NOVA_PLATFORM_LINUX)
    return PlatformType::Linux;
#elif defined(NOVA_PLATFORM_MACOS)
    return PlatformType::macOS;
#elif defined(NOVA_PLATFORM_IOS)
    return PlatformType::iOS;
#elif defined(NOVA_PLATFORM_ANDROID)
    return PlatformType::Android;
#elif defined(NOVA_PLATFORM_WEB)
    return PlatformType::Web;
#else
    return PlatformType::Linux; // Default fallback
#endif
}

/**
 * @brief Supported graphics APIs
 */
enum class GraphicsAPI : uint8_t {
    None,
    OpenGL,
    OpenGLES,
    Vulkan,
    DirectX11,
    DirectX12,
    Metal,
    WebGPU
};

/**
 * @brief Convert graphics API to string
 */
constexpr const char* GraphicsAPIToString(GraphicsAPI api) noexcept {
    switch (api) {
        case GraphicsAPI::None:      return "None";
        case GraphicsAPI::OpenGL:    return "OpenGL";
        case GraphicsAPI::OpenGLES:  return "OpenGL ES";
        case GraphicsAPI::Vulkan:    return "Vulkan";
        case GraphicsAPI::DirectX11: return "DirectX 11";
        case GraphicsAPI::DirectX12: return "DirectX 12";
        case GraphicsAPI::Metal:     return "Metal";
        case GraphicsAPI::WebGPU:    return "WebGPU";
        default:                     return "Unknown";
    }
}

/**
 * @brief GPU vendor enumeration
 */
enum class GPUVendor : uint8_t {
    Unknown,
    NVIDIA,
    AMD,
    Intel,
    Apple,
    ARM,        // Mali GPUs
    Qualcomm,   // Adreno GPUs
    ImgTec,     // PowerVR GPUs
    Broadcom,
    Software    // Software renderer
};

/**
 * @brief Convert GPU vendor to string
 */
constexpr const char* GPUVendorToString(GPUVendor vendor) noexcept {
    switch (vendor) {
        case GPUVendor::NVIDIA:   return "NVIDIA";
        case GPUVendor::AMD:      return "AMD";
        case GPUVendor::Intel:    return "Intel";
        case GPUVendor::Apple:    return "Apple";
        case GPUVendor::ARM:      return "ARM (Mali)";
        case GPUVendor::Qualcomm: return "Qualcomm (Adreno)";
        case GPUVendor::ImgTec:   return "Imagination Technologies (PowerVR)";
        case GPUVendor::Broadcom: return "Broadcom";
        case GPUVendor::Software: return "Software Renderer";
        default:                  return "Unknown";
    }
}

// =============================================================================
// Platform Capabilities Structure
// =============================================================================

/**
 * @brief Detailed platform and GPU capabilities
 *
 * Provides comprehensive information about the graphics hardware and
 * supported features for capability-based rendering decisions.
 */
struct PlatformCapabilities {
    // -------------------------------------------------------------------------
    // Graphics API Information
    // -------------------------------------------------------------------------
    GraphicsAPI graphicsAPI = GraphicsAPI::None;
    std::string apiVersion;
    std::string shadingLanguageVersion;

    // -------------------------------------------------------------------------
    // GPU Information
    // -------------------------------------------------------------------------
    GPUVendor vendor = GPUVendor::Unknown;
    std::string gpuName;
    std::string driverVersion;
    uint32_t vendorID = 0;
    uint32_t deviceID = 0;

    // -------------------------------------------------------------------------
    // Memory Information
    // -------------------------------------------------------------------------
    uint64_t dedicatedVideoMemory = 0;   // Bytes
    uint64_t sharedSystemMemory = 0;     // Bytes
    uint64_t availableVideoMemory = 0;   // Bytes (if queryable)

    // -------------------------------------------------------------------------
    // Texture Limits
    // -------------------------------------------------------------------------
    uint32_t maxTextureSize = 4096;
    uint32_t maxCubemapSize = 4096;
    uint32_t max3DTextureSize = 256;
    uint32_t maxArrayTextureLayers = 256;
    uint32_t maxTextureUnits = 16;
    float maxAnisotropy = 1.0f;

    // -------------------------------------------------------------------------
    // Compute Capabilities
    // -------------------------------------------------------------------------
    uint32_t maxComputeWorkGroupsX = 0;
    uint32_t maxComputeWorkGroupsY = 0;
    uint32_t maxComputeWorkGroupsZ = 0;
    uint32_t maxComputeWorkGroupSizeX = 0;
    uint32_t maxComputeWorkGroupSizeY = 0;
    uint32_t maxComputeWorkGroupSizeZ = 0;
    uint32_t maxComputeWorkGroupInvocations = 0;
    uint32_t maxComputeSharedMemorySize = 0;

    // -------------------------------------------------------------------------
    // Shader Capabilities
    // -------------------------------------------------------------------------
    uint32_t maxVertexAttributes = 16;
    uint32_t maxVertexUniforms = 1024;
    uint32_t maxFragmentUniforms = 1024;
    uint32_t maxUniformBlockSize = 16384;
    uint32_t maxUniformBufferBindings = 12;
    uint32_t maxStorageBufferSize = 0;
    uint32_t maxStorageBufferBindings = 0;

    // -------------------------------------------------------------------------
    // Framebuffer Capabilities
    // -------------------------------------------------------------------------
    uint32_t maxColorAttachments = 8;
    uint32_t maxDrawBuffers = 8;
    uint32_t maxFramebufferWidth = 4096;
    uint32_t maxFramebufferHeight = 4096;
    uint32_t maxFramebufferSamples = 4;
    uint32_t maxViewports = 1;

    // -------------------------------------------------------------------------
    // Feature Support Flags
    // -------------------------------------------------------------------------
    bool supportsRayTracing = false;
    bool supportsCompute = false;
    bool supportsTessellation = false;
    bool supportsGeometryShaders = false;
    bool supportsMeshShaders = false;
    bool supportsMultiDrawIndirect = false;
    bool supportsBindlessTextures = false;
    bool supportsSparseTextures = false;
    bool supportsConservativeRaster = false;
    bool supportsVariableRateShading = false;

    // -------------------------------------------------------------------------
    // Texture Compression Support
    // -------------------------------------------------------------------------
    bool supportsS3TC = false;    // DXT1, DXT3, DXT5
    bool supportsBC = false;      // BC6H, BC7
    bool supportsETC2 = false;
    bool supportsASTC = false;
    bool supportsPVRTC = false;

    // -------------------------------------------------------------------------
    // Synchronization Support
    // -------------------------------------------------------------------------
    bool supportsTimelineSemaphores = false;
    bool supportsSynchronization2 = false;

    // -------------------------------------------------------------------------
    // Utility Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Check if GPU is discrete (dedicated graphics card)
     */
    [[nodiscard]] bool IsDiscreteGPU() const noexcept {
        return dedicatedVideoMemory > 0 && vendor != GPUVendor::Intel;
    }

    /**
     * @brief Check if GPU supports advanced features for SDF rendering
     */
    [[nodiscard]] bool SupportsAdvancedSDF() const noexcept {
        return supportsCompute && maxComputeWorkGroupInvocations >= 256;
    }

    /**
     * @brief Get total usable memory
     */
    [[nodiscard]] uint64_t GetTotalMemory() const noexcept {
        return dedicatedVideoMemory + sharedSystemMemory;
    }
};

// =============================================================================
// Platform Configuration Structure
// =============================================================================

/**
 * @brief Configuration for platform backend initialization
 */
struct PlatformConfig {
    // -------------------------------------------------------------------------
    // Window Configuration
    // -------------------------------------------------------------------------
    uint32_t width = 1280;
    uint32_t height = 720;
    std::string windowTitle = "Nova3D Engine";
    bool fullscreen = false;
    bool resizable = true;
    bool decorated = true;
    bool maximized = false;

    // -------------------------------------------------------------------------
    // Graphics Configuration
    // -------------------------------------------------------------------------
    bool vsync = true;
    uint32_t msaaSamples = 1;       // 1 = no MSAA, 2, 4, 8, 16
    bool sRGB = true;
    bool hdr = false;
    uint32_t swapchainImages = 3;   // Triple buffering by default

    // -------------------------------------------------------------------------
    // Debug Configuration
    // -------------------------------------------------------------------------
    bool enableValidation = false;
    bool enableDebugMarkers = false;
    bool enableGPUAssistedValidation = false;

    // -------------------------------------------------------------------------
    // API Version Requirements
    // -------------------------------------------------------------------------
    uint32_t minAPIVersionMajor = 0;  // 0 = any version
    uint32_t minAPIVersionMinor = 0;

    // -------------------------------------------------------------------------
    // Platform-Specific Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Platform-specific configuration data
     *
     * This can hold platform-specific initialization data:
     * - Windows: HINSTANCE, parent HWND
     * - Linux: X11 Display*, Wayland display*
     * - Android: ANativeWindow*
     * - iOS: UIView*
     */
    void* platformData = nullptr;

    /**
     * @brief Optional callback for debug messages
     */
    std::function<void(int severity, const std::string& message)> debugCallback;
};

// =============================================================================
// IPlatformBackend Interface
// =============================================================================

/**
 * @brief Pure virtual interface for platform-specific graphics backends
 *
 * This interface provides a unified API for initializing and managing
 * platform-specific rendering backends. Implementations include:
 * - WindowsGLBackend: Windows with WGL OpenGL context
 * - LinuxVulkanBackend: Linux with Vulkan (X11/Wayland)
 * - AndroidVulkanBackend: Android with Vulkan
 * - AndroidGLESBackend: Android with OpenGL ES
 * - MetalBackend: macOS/iOS with Metal
 * - WebGPUBackend: Web with WebGPU
 *
 * Usage:
 * @code
 * auto backend = PlatformBackendRegistry::Get().CreateBestBackend();
 * if (backend && backend->Initialize(config)) {
 *     while (!backend->ShouldClose()) {
 *         backend->BeginFrame();
 *         // Render...
 *         backend->EndFrame();
 *         backend->SwapBuffers();
 *         backend->PollEvents();
 *     }
 *     backend->Shutdown();
 * }
 * @endcode
 */
class IPlatformBackend {
public:
    virtual ~IPlatformBackend() = default;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Initialize the platform backend
     * @param config Configuration for initialization
     * @return true if initialization succeeded
     */
    virtual bool Initialize(const PlatformConfig& config) = 0;

    /**
     * @brief Shutdown and release all resources
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Check if backend is initialized and valid
     */
    [[nodiscard]] virtual bool IsInitialized() const = 0;

    // =========================================================================
    // Platform Information
    // =========================================================================

    /**
     * @brief Get the platform type this backend runs on
     */
    [[nodiscard]] virtual PlatformType GetPlatformType() const = 0;

    /**
     * @brief Get the graphics API used by this backend
     */
    [[nodiscard]] virtual GraphicsAPI GetGraphicsAPI() const = 0;

    /**
     * @brief Get detailed platform and GPU capabilities
     */
    [[nodiscard]] virtual PlatformCapabilities GetCapabilities() const = 0;

    /**
     * @brief Get the backend name (e.g., "WindowsGL", "LinuxVulkan")
     */
    [[nodiscard]] virtual std::string_view GetName() const = 0;

    /**
     * @brief Get backend version string
     */
    [[nodiscard]] virtual std::string GetVersionString() const = 0;

    // =========================================================================
    // Native Handle Access
    // =========================================================================

    /**
     * @brief Get native window handle
     *
     * Platform-specific return values:
     * - Windows: HWND
     * - Linux X11: Window (X11)
     * - Linux Wayland: wl_surface*
     * - macOS: NSWindow*
     * - iOS: UIWindow*
     * - Android: ANativeWindow*
     */
    [[nodiscard]] virtual void* GetNativeWindowHandle() const = 0;

    /**
     * @brief Get native graphics device handle
     *
     * Platform-specific return values:
     * - OpenGL: nullptr (context-based)
     * - Vulkan: VkDevice
     * - DirectX: ID3D11Device* / ID3D12Device*
     * - Metal: id<MTLDevice>
     */
    [[nodiscard]] virtual void* GetNativeDeviceHandle() const = 0;

    /**
     * @brief Get native graphics context handle
     *
     * Platform-specific return values:
     * - Windows OpenGL: HGLRC
     * - Linux OpenGL: GLXContext or EGLContext
     * - Vulkan: VkInstance
     * - DirectX: ID3D11DeviceContext* / ID3D12CommandQueue*
     * - Metal: id<MTLCommandQueue>
     */
    [[nodiscard]] virtual void* GetNativeContextHandle() const = 0;

    /**
     * @brief Get native display/surface handle
     *
     * Platform-specific return values:
     * - Windows: HDC
     * - Linux X11: Display*
     * - Linux Wayland: wl_display*
     * - Vulkan: VkSurfaceKHR
     */
    [[nodiscard]] virtual void* GetNativeDisplayHandle() const = 0;

    // =========================================================================
    // Frame Management
    // =========================================================================

    /**
     * @brief Begin a new frame
     *
     * Call this at the start of each frame before rendering.
     * For Vulkan backends, this acquires the next swapchain image.
     */
    virtual void BeginFrame() = 0;

    /**
     * @brief End the current frame
     *
     * Call this after all rendering commands are recorded.
     * For Vulkan backends, this submits command buffers.
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Swap front and back buffers (present)
     *
     * Call this after EndFrame to present the rendered image.
     */
    virtual void SwapBuffers() = 0;

    /**
     * @brief Get current frame index (for multi-buffering)
     */
    [[nodiscard]] virtual uint32_t GetCurrentFrameIndex() const = 0;

    /**
     * @brief Get number of frames in flight
     */
    [[nodiscard]] virtual uint32_t GetFramesInFlight() const = 0;

    // =========================================================================
    // Window Management
    // =========================================================================

    /**
     * @brief Set window size
     * @param width New width in pixels
     * @param height New height in pixels
     */
    virtual void SetWindowSize(uint32_t width, uint32_t height) = 0;

    /**
     * @brief Set fullscreen mode
     * @param fullscreen true for fullscreen, false for windowed
     */
    virtual void SetFullscreen(bool fullscreen) = 0;

    /**
     * @brief Set vertical sync
     * @param enabled true to enable vsync
     */
    virtual void SetVSync(bool enabled) = 0;

    /**
     * @brief Get current window size
     */
    [[nodiscard]] virtual glm::ivec2 GetWindowSize() const = 0;

    /**
     * @brief Get framebuffer size (may differ from window size on high-DPI)
     */
    [[nodiscard]] virtual glm::ivec2 GetFramebufferSize() const = 0;

    /**
     * @brief Get display scale factor for high-DPI displays
     */
    [[nodiscard]] virtual float GetDisplayScale() const = 0;

    /**
     * @brief Check if window is currently fullscreen
     */
    [[nodiscard]] virtual bool IsFullscreen() const = 0;

    /**
     * @brief Check if vsync is enabled
     */
    [[nodiscard]] virtual bool IsVSyncEnabled() const = 0;

    // =========================================================================
    // Input and Events
    // =========================================================================

    /**
     * @brief Poll and process pending events
     */
    virtual void PollEvents() = 0;

    /**
     * @brief Check if window should close
     */
    [[nodiscard]] virtual bool ShouldClose() const = 0;

    /**
     * @brief Request window close
     */
    virtual void RequestClose() = 0;

    // =========================================================================
    // Swapchain Management (Vulkan/Modern APIs)
    // =========================================================================

    /**
     * @brief Recreate swapchain (e.g., after resize)
     * @return true if recreation succeeded
     */
    virtual bool RecreateSwapchain() = 0;

    /**
     * @brief Check if swapchain needs recreation
     */
    [[nodiscard]] virtual bool NeedsSwapchainRecreation() const = 0;

    // =========================================================================
    // Feature Queries
    // =========================================================================

    /**
     * @brief Check if a specific feature is supported
     * @param featureName Feature name string
     */
    [[nodiscard]] virtual bool SupportsFeature(const std::string& featureName) const = 0;

    /**
     * @brief Check if an extension is supported
     * @param extensionName Extension name string
     */
    [[nodiscard]] virtual bool SupportsExtension(const std::string& extensionName) const = 0;

    /**
     * @brief Get list of supported extensions
     */
    [[nodiscard]] virtual std::vector<std::string> GetSupportedExtensions() const = 0;

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Wait for all GPU operations to complete
     */
    virtual void WaitIdle() = 0;

    /**
     * @brief Get GPU function pointer by name
     * @param name Function name
     * @return Function pointer or nullptr
     */
    [[nodiscard]] virtual void* GetProcAddress(const char* name) const = 0;

    /**
     * @brief Set debug name for a GPU object (if supported)
     * @param objectHandle Native object handle
     * @param name Debug name
     */
    virtual void SetObjectDebugName(void* objectHandle, const std::string& name) = 0;

protected:
    IPlatformBackend() = default;

    // Non-copyable
    IPlatformBackend(const IPlatformBackend&) = delete;
    IPlatformBackend& operator=(const IPlatformBackend&) = delete;

    // Movable
    IPlatformBackend(IPlatformBackend&&) = default;
    IPlatformBackend& operator=(IPlatformBackend&&) = default;
};

// =============================================================================
// Backend Factory Function Type
// =============================================================================

/**
 * @brief Factory function signature for creating backend instances
 */
using BackendFactoryFunc = std::function<std::unique_ptr<IPlatformBackend>()>;

/**
 * @brief Backend availability check function signature
 */
using BackendAvailabilityFunc = std::function<bool()>;

// =============================================================================
// Backend Registration Info
// =============================================================================

/**
 * @brief Information about a registered backend
 */
struct BackendInfo {
    std::string name;
    PlatformType platformType;
    GraphicsAPI graphicsAPI;
    BackendFactoryFunc factory;
    BackendAvailabilityFunc isAvailable;
    int priority = 0;  // Higher priority = preferred when multiple backends available
};

// =============================================================================
// PlatformBackendRegistry
// =============================================================================

/**
 * @brief Singleton registry for platform backends
 *
 * Manages registration and creation of platform-specific backends.
 * Supports automatic detection of the best available backend.
 *
 * Usage:
 * @code
 * // Register backends (typically done at startup)
 * PlatformBackendRegistry::Get().RegisterBackend({
 *     "WindowsGL",
 *     PlatformType::Windows,
 *     GraphicsAPI::OpenGL,
 *     []() { return std::make_unique<WindowsGLBackend>(); },
 *     []() { return WindowsGLBackend::IsAvailable(); },
 *     100  // Priority
 * });
 *
 * // Create best available backend
 * auto backend = PlatformBackendRegistry::Get().CreateBestBackend();
 *
 * // Or create specific backend
 * auto vulkan = PlatformBackendRegistry::Get().CreateBackend("LinuxVulkan");
 * @endcode
 */
class PlatformBackendRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static PlatformBackendRegistry& Get() {
        static PlatformBackendRegistry instance;
        return instance;
    }

    // =========================================================================
    // Backend Registration
    // =========================================================================

    /**
     * @brief Register a backend
     * @param info Backend registration information
     */
    void RegisterBackend(BackendInfo info) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_backends[info.name] = std::move(info);
    }

    /**
     * @brief Unregister a backend
     * @param name Backend name
     */
    void UnregisterBackend(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_backends.erase(name);
    }

    /**
     * @brief Check if a backend is registered
     * @param name Backend name
     */
    [[nodiscard]] bool HasBackend(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_backends.find(name) != m_backends.end();
    }

    // =========================================================================
    // Backend Creation
    // =========================================================================

    /**
     * @brief Create a backend by name
     * @param name Backend name
     * @return Backend instance or nullptr if not found/unavailable
     */
    [[nodiscard]] std::unique_ptr<IPlatformBackend> CreateBackend(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_backends.find(name);
        if (it != m_backends.end() && it->second.isAvailable && it->second.isAvailable()) {
            return it->second.factory();
        }
        return nullptr;
    }

    /**
     * @brief Create the best available backend for current platform
     * @return Backend instance or nullptr if none available
     */
    [[nodiscard]] std::unique_ptr<IPlatformBackend> CreateBestBackend() const {
        std::lock_guard<std::mutex> lock(m_mutex);

        const BackendInfo* best = nullptr;
        const PlatformType currentPlatform = GetCurrentPlatformType();

        for (const auto& [name, info] : m_backends) {
            // Skip backends for other platforms
            if (info.platformType != currentPlatform) {
                continue;
            }

            // Skip unavailable backends
            if (!info.isAvailable || !info.isAvailable()) {
                continue;
            }

            // Select highest priority
            if (!best || info.priority > best->priority) {
                best = &info;
            }
        }

        if (best && best->factory) {
            return best->factory();
        }
        return nullptr;
    }

    /**
     * @brief Create backend with specific graphics API
     * @param api Desired graphics API
     * @return Backend instance or nullptr if none available
     */
    [[nodiscard]] std::unique_ptr<IPlatformBackend> CreateBackendWithAPI(GraphicsAPI api) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        const BackendInfo* best = nullptr;
        const PlatformType currentPlatform = GetCurrentPlatformType();

        for (const auto& [name, info] : m_backends) {
            if (info.platformType != currentPlatform || info.graphicsAPI != api) {
                continue;
            }

            if (!info.isAvailable || !info.isAvailable()) {
                continue;
            }

            if (!best || info.priority > best->priority) {
                best = &info;
            }
        }

        if (best && best->factory) {
            return best->factory();
        }
        return nullptr;
    }

    // =========================================================================
    // Query Methods
    // =========================================================================

    /**
     * @brief Get all registered backend names
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredBackendNames() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> names;
        names.reserve(m_backends.size());
        for (const auto& [name, info] : m_backends) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Get available backends for current platform
     */
    [[nodiscard]] std::vector<std::string> GetAvailableBackends() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> available;
        const PlatformType currentPlatform = GetCurrentPlatformType();

        for (const auto& [name, info] : m_backends) {
            if (info.platformType == currentPlatform &&
                info.isAvailable && info.isAvailable()) {
                available.push_back(name);
            }
        }
        return available;
    }

    /**
     * @brief Get available graphics APIs for current platform
     */
    [[nodiscard]] std::vector<GraphicsAPI> GetAvailableAPIs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<GraphicsAPI> apis;
        const PlatformType currentPlatform = GetCurrentPlatformType();

        for (const auto& [name, info] : m_backends) {
            if (info.platformType == currentPlatform &&
                info.isAvailable && info.isAvailable()) {
                // Check if API is already in list
                bool found = false;
                for (GraphicsAPI api : apis) {
                    if (api == info.graphicsAPI) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    apis.push_back(info.graphicsAPI);
                }
            }
        }
        return apis;
    }

    /**
     * @brief Get backend info by name
     * @param name Backend name
     * @return Backend info or nullopt if not found
     */
    [[nodiscard]] std::optional<BackendInfo> GetBackendInfo(const std::string& name) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_backends.find(name);
        if (it != m_backends.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    PlatformBackendRegistry() = default;
    ~PlatformBackendRegistry() = default;

    // Non-copyable, non-movable
    PlatformBackendRegistry(const PlatformBackendRegistry&) = delete;
    PlatformBackendRegistry& operator=(const PlatformBackendRegistry&) = delete;
    PlatformBackendRegistry(PlatformBackendRegistry&&) = delete;
    PlatformBackendRegistry& operator=(PlatformBackendRegistry&&) = delete;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, BackendInfo> m_backends;
};

// =============================================================================
// Auto-Registration Helper
// =============================================================================

/**
 * @brief RAII helper for automatic backend registration
 *
 * Usage:
 * @code
 * // In backend implementation file:
 * static BackendAutoRegister<WindowsGLBackend> s_register(
 *     "WindowsGL",
 *     PlatformType::Windows,
 *     GraphicsAPI::OpenGL,
 *     100  // Priority
 * );
 * @endcode
 */
template<typename BackendType>
class BackendAutoRegister {
public:
    BackendAutoRegister(
        const std::string& name,
        PlatformType platformType,
        GraphicsAPI graphicsAPI,
        int priority = 0
    ) {
        PlatformBackendRegistry::Get().RegisterBackend({
            name,
            platformType,
            graphicsAPI,
            []() { return std::make_unique<BackendType>(); },
            []() { return BackendType::IsAvailable(); },
            priority
        });
    }
};

// =============================================================================
// Platform Detection Utilities
// =============================================================================

/**
 * @brief Detect the best available graphics API for current platform
 */
inline GraphicsAPI DetectBestGraphicsAPI() {
    const auto apis = PlatformBackendRegistry::Get().GetAvailableAPIs();

    if (apis.empty()) {
        return GraphicsAPI::None;
    }

    // Prefer Vulkan on most platforms
    for (GraphicsAPI api : apis) {
        if (api == GraphicsAPI::Vulkan) {
            return GraphicsAPI::Vulkan;
        }
    }

    // Then Metal on Apple platforms
    for (GraphicsAPI api : apis) {
        if (api == GraphicsAPI::Metal) {
            return GraphicsAPI::Metal;
        }
    }

    // Then DirectX12 on Windows
    for (GraphicsAPI api : apis) {
        if (api == GraphicsAPI::DirectX12) {
            return GraphicsAPI::DirectX12;
        }
    }

    // Fall back to OpenGL/GLES
    for (GraphicsAPI api : apis) {
        if (api == GraphicsAPI::OpenGL || api == GraphicsAPI::OpenGLES) {
            return api;
        }
    }

    // Return first available
    return apis[0];
}

/**
 * @brief Check if a specific graphics API is available
 * @param api Graphics API to check
 */
inline bool IsGraphicsAPIAvailable(GraphicsAPI api) {
    const auto apis = PlatformBackendRegistry::Get().GetAvailableAPIs();
    for (GraphicsAPI available : apis) {
        if (available == api) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Get recommended graphics API for current platform
 */
inline GraphicsAPI GetRecommendedAPI() {
#if defined(NOVA_PLATFORM_WINDOWS)
    if (IsGraphicsAPIAvailable(GraphicsAPI::DirectX12)) {
        return GraphicsAPI::DirectX12;
    }
    if (IsGraphicsAPIAvailable(GraphicsAPI::Vulkan)) {
        return GraphicsAPI::Vulkan;
    }
    return GraphicsAPI::OpenGL;
#elif defined(NOVA_PLATFORM_LINUX)
    if (IsGraphicsAPIAvailable(GraphicsAPI::Vulkan)) {
        return GraphicsAPI::Vulkan;
    }
    return GraphicsAPI::OpenGL;
#elif defined(NOVA_PLATFORM_MACOS) || defined(NOVA_PLATFORM_IOS)
    return GraphicsAPI::Metal;
#elif defined(NOVA_PLATFORM_ANDROID)
    if (IsGraphicsAPIAvailable(GraphicsAPI::Vulkan)) {
        return GraphicsAPI::Vulkan;
    }
    return GraphicsAPI::OpenGLES;
#elif defined(NOVA_PLATFORM_WEB)
    return GraphicsAPI::WebGPU;
#else
    return DetectBestGraphicsAPI();
#endif
}

} // namespace Nova
