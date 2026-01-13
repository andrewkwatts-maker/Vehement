#pragma once

/**
 * @file LinuxVulkan.hpp
 * @brief Linux Vulkan implementation with SOLID architecture
 *
 * Modern Vulkan 1.3 implementation supporting:
 * - X11 and Wayland surface creation
 * - DRM/KMS direct rendering mode
 * - VMA (Vulkan Memory Allocator) integration
 * - Timeline semaphores and synchronization2
 * - Dynamic rendering (VK_KHR_dynamic_rendering)
 * - Pipeline caching and shader management
 */

#ifdef NOVA_PLATFORM_LINUX

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <span>

// VMA forward declaration
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

// Forward declarations for display servers
struct _XDisplay;
typedef struct _XDisplay Display;
typedef unsigned long Window;
struct wl_display;
struct wl_surface;

// DRM forward declarations
struct gbm_device;
struct gbm_surface;

namespace Nova {

// =============================================================================
// Forward Declarations
// =============================================================================

class IVulkanContext;
class VulkanInstance;
class VulkanDevice;
class VulkanSwapchain;
class VulkanCommandPool;
class VulkanDescriptorPool;
class VulkanPipelineCache;
class VulkanSyncManager;

// =============================================================================
// Enums and Configuration
// =============================================================================

/**
 * @brief Surface type for Linux display servers
 */
enum class LinuxSurfaceType {
    Unknown,
    X11,
    Wayland,
    DRM  // Direct Rendering Manager (headless/KMS)
};

/**
 * @brief Queue family capabilities
 */
enum class QueueCapability : uint32_t {
    Graphics = 1 << 0,
    Compute  = 1 << 1,
    Transfer = 1 << 2,
    Present  = 1 << 3,
    Sparse   = 1 << 4
};

inline QueueCapability operator|(QueueCapability a, QueueCapability b) {
    return static_cast<QueueCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(QueueCapability a, QueueCapability b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief Vulkan feature level requirements
 */
enum class VulkanFeatureLevel {
    Vulkan10,      // Base Vulkan 1.0
    Vulkan11,      // Vulkan 1.1 (multiview, subgroup)
    Vulkan12,      // Vulkan 1.2 (timeline semaphores, buffer device address)
    Vulkan13       // Vulkan 1.3 (dynamic rendering, sync2)
};

/**
 * @brief Debug severity levels
 */
enum class DebugSeverity {
    Verbose,
    Info,
    Warning,
    Error
};

/**
 * @brief Configuration for Vulkan initialization
 */
struct VulkanConfig {
    std::string applicationName = "Nova3D Application";
    uint32_t applicationVersion = VK_MAKE_VERSION(1, 0, 0);

    VulkanFeatureLevel requiredFeatureLevel = VulkanFeatureLevel::Vulkan13;
    bool enableValidation = false;
    bool enableGPUAssistedValidation = false;
    bool enableSynchronizationValidation = false;
    bool enableDebugPrintf = false;

    LinuxSurfaceType preferredSurfaceType = LinuxSurfaceType::Unknown;  // Auto-detect

    std::vector<const char*> additionalInstanceExtensions;
    std::vector<const char*> additionalDeviceExtensions;

    // VMA configuration
    bool enableVMA = true;
    bool enableVMADefragmentation = true;
    VkDeviceSize vmaPreferredBlockSize = 256 * 1024 * 1024;  // 256 MB

    // Pipeline cache
    std::string pipelineCachePath = "";  // Empty = no disk cache

    // Callbacks
    std::function<void(DebugSeverity, const char*)> debugCallback;
};

/**
 * @brief Queue family information
 */
struct QueueFamilyInfo {
    uint32_t index = UINT32_MAX;
    uint32_t count = 0;
    QueueCapability capabilities = static_cast<QueueCapability>(0);
    float timestampPeriod = 0.0f;
    VkExtent3D minImageTransferGranularity = {0, 0, 0};
};

/**
 * @brief Physical device information
 */
struct PhysicalDeviceInfo {
    VkPhysicalDevice handle = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties = {};
    VkPhysicalDeviceFeatures features = {};
    VkPhysicalDeviceMemoryProperties memoryProperties = {};

    // Vulkan 1.1+
    VkPhysicalDeviceVulkan11Properties properties11 = {};
    VkPhysicalDeviceVulkan11Features features11 = {};

    // Vulkan 1.2+
    VkPhysicalDeviceVulkan12Properties properties12 = {};
    VkPhysicalDeviceVulkan12Features features12 = {};

    // Vulkan 1.3+
    VkPhysicalDeviceVulkan13Properties properties13 = {};
    VkPhysicalDeviceVulkan13Features features13 = {};

    std::vector<QueueFamilyInfo> queueFamilies;
    std::vector<VkExtensionProperties> extensions;

    bool supportsRayTracing = false;
    bool supportsMeshShaders = false;
    uint64_t deviceLocalMemorySize = 0;
    int score = 0;  // Device selection score
};

// =============================================================================
// IVulkanContext Interface (Dependency Inversion)
// =============================================================================

/**
 * @brief Abstract interface for Vulkan context
 *
 * Follows Interface Segregation Principle - clients depend only on
 * the interfaces they use.
 */
class IVulkanContext {
public:
    virtual ~IVulkanContext() = default;

    // Instance access
    virtual VkInstance GetInstance() const = 0;
    virtual bool IsValidationEnabled() const = 0;

    // Device access
    virtual VkDevice GetDevice() const = 0;
    virtual VkPhysicalDevice GetPhysicalDevice() const = 0;
    virtual const PhysicalDeviceInfo& GetPhysicalDeviceInfo() const = 0;

    // Queue access
    virtual VkQueue GetGraphicsQueue() const = 0;
    virtual VkQueue GetComputeQueue() const = 0;
    virtual VkQueue GetTransferQueue() const = 0;
    virtual uint32_t GetGraphicsQueueFamily() const = 0;
    virtual uint32_t GetComputeQueueFamily() const = 0;
    virtual uint32_t GetTransferQueueFamily() const = 0;

    // Memory allocation (VMA)
    virtual VmaAllocator GetAllocator() const = 0;

    // Surface/Swapchain
    virtual VkSurfaceKHR GetSurface() const = 0;
    virtual LinuxSurfaceType GetSurfaceType() const = 0;

    // Feature queries
    virtual bool SupportsFeature(const char* featureName) const = 0;
    virtual VulkanFeatureLevel GetFeatureLevel() const = 0;
};

// =============================================================================
// VulkanInstance (Single Responsibility)
// =============================================================================

/**
 * @brief Manages Vulkan instance lifecycle
 *
 * Responsible for:
 * - Instance creation with extensions
 * - Validation layer setup
 * - Debug messenger management
 */
class VulkanInstance {
public:
    VulkanInstance() = default;
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;
    VulkanInstance(VulkanInstance&& other) noexcept;
    VulkanInstance& operator=(VulkanInstance&& other) noexcept;

    /**
     * @brief Create Vulkan instance
     */
    bool Create(const VulkanConfig& config);

    /**
     * @brief Destroy instance and cleanup
     */
    void Destroy();

    [[nodiscard]] VkInstance GetHandle() const { return m_instance; }
    [[nodiscard]] bool IsValid() const { return m_instance != VK_NULL_HANDLE; }
    [[nodiscard]] bool IsValidationEnabled() const { return m_validationEnabled; }
    [[nodiscard]] VulkanFeatureLevel GetFeatureLevel() const { return m_featureLevel; }

    /**
     * @brief Enumerate available physical devices
     */
    std::vector<PhysicalDeviceInfo> EnumeratePhysicalDevices() const;

    /**
     * @brief Get instance extension function
     */
    template<typename T>
    T GetProcAddr(const char* name) const {
        return reinterpret_cast<T>(vkGetInstanceProcAddr(m_instance, name));
    }

private:
    bool CheckLayerSupport(const std::vector<const char*>& layers) const;
    bool CheckExtensionSupport(const std::vector<const char*>& extensions) const;
    void SetupDebugMessenger(const VulkanConfig& config);
    void PopulateDeviceInfo(VkPhysicalDevice device, PhysicalDeviceInfo& info) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* userData);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    bool m_validationEnabled = false;
    VulkanFeatureLevel m_featureLevel = VulkanFeatureLevel::Vulkan10;
    std::function<void(DebugSeverity, const char*)> m_debugCallback;
};

// =============================================================================
// VulkanDevice (Single Responsibility)
// =============================================================================

/**
 * @brief Manages logical device and queues
 *
 * Responsible for:
 * - Device creation with features
 * - Queue family management
 * - VMA allocator initialization
 */
class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;

    /**
     * @brief Create logical device from physical device
     */
    bool Create(VulkanInstance& instance, const PhysicalDeviceInfo& physicalDevice,
                VkSurfaceKHR surface, const VulkanConfig& config);

    /**
     * @brief Destroy device and allocator
     */
    void Destroy();

    [[nodiscard]] VkDevice GetHandle() const { return m_device; }
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] const PhysicalDeviceInfo& GetPhysicalDeviceInfo() const { return m_deviceInfo; }
    [[nodiscard]] bool IsValid() const { return m_device != VK_NULL_HANDLE; }

    // Queue access
    [[nodiscard]] VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] VkQueue GetComputeQueue() const { return m_computeQueue; }
    [[nodiscard]] VkQueue GetTransferQueue() const { return m_transferQueue; }
    [[nodiscard]] VkQueue GetPresentQueue() const { return m_presentQueue; }

    [[nodiscard]] uint32_t GetGraphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] uint32_t GetComputeQueueFamily() const { return m_computeQueueFamily; }
    [[nodiscard]] uint32_t GetTransferQueueFamily() const { return m_transferQueueFamily; }
    [[nodiscard]] uint32_t GetPresentQueueFamily() const { return m_presentQueueFamily; }

    // VMA
    [[nodiscard]] VmaAllocator GetAllocator() const { return m_allocator; }

    /**
     * @brief Wait for device idle
     */
    void WaitIdle() const;

    /**
     * @brief Get device function pointer
     */
    template<typename T>
    T GetProcAddr(const char* name) const {
        return reinterpret_cast<T>(vkGetDeviceProcAddr(m_device, name));
    }

    /**
     * @brief Set debug name for Vulkan object
     */
    void SetDebugName(VkObjectType type, uint64_t handle, const char* name) const;

private:
    bool CreateAllocator(VkInstance instance, const VulkanConfig& config);
    bool FindQueueFamilies(VkSurfaceKHR surface);

    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    PhysicalDeviceInfo m_deviceInfo;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    VkQueue m_transferQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    uint32_t m_graphicsQueueFamily = UINT32_MAX;
    uint32_t m_computeQueueFamily = UINT32_MAX;
    uint32_t m_transferQueueFamily = UINT32_MAX;
    uint32_t m_presentQueueFamily = UINT32_MAX;

    VmaAllocator m_allocator = nullptr;

    // Function pointers for debug utils
    PFN_vkSetDebugUtilsObjectNameEXT m_setDebugName = nullptr;
};

// =============================================================================
// VulkanSwapchain (Single Responsibility)
// =============================================================================

/**
 * @brief Swapchain configuration
 */
struct SwapchainConfig {
    uint32_t preferredImageCount = 3;  // Triple buffering
    VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    VkFormat preferredFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    bool enableHDR = false;
    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
};

/**
 * @brief Manages swapchain lifecycle
 */
class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain& operator=(VulkanSwapchain&& other) noexcept;

    /**
     * @brief Create swapchain
     */
    bool Create(VulkanDevice& device, VkSurfaceKHR surface,
                uint32_t width, uint32_t height, const SwapchainConfig& config);

    /**
     * @brief Recreate swapchain (resize, format change)
     */
    bool Recreate(uint32_t width, uint32_t height);

    /**
     * @brief Destroy swapchain
     */
    void Destroy();

    /**
     * @brief Acquire next image for rendering
     * @param semaphore Semaphore to signal when image is available
     * @param fence Fence to signal (optional)
     * @param timeout Timeout in nanoseconds
     * @return Image index, or UINT32_MAX on failure
     */
    uint32_t AcquireNextImage(VkSemaphore semaphore, VkFence fence = VK_NULL_HANDLE,
                               uint64_t timeout = UINT64_MAX);

    /**
     * @brief Present rendered image
     * @param waitSemaphore Semaphore to wait on
     * @param imageIndex Image index to present
     * @return true if present succeeded
     */
    bool Present(VkSemaphore waitSemaphore, uint32_t imageIndex);

    [[nodiscard]] VkSwapchainKHR GetHandle() const { return m_swapchain; }
    [[nodiscard]] bool IsValid() const { return m_swapchain != VK_NULL_HANDLE; }
    [[nodiscard]] VkFormat GetFormat() const { return m_format; }
    [[nodiscard]] VkExtent2D GetExtent() const { return m_extent; }
    [[nodiscard]] uint32_t GetImageCount() const { return static_cast<uint32_t>(m_images.size()); }
    [[nodiscard]] VkImage GetImage(uint32_t index) const { return m_images[index]; }
    [[nodiscard]] VkImageView GetImageView(uint32_t index) const { return m_imageViews[index]; }
    [[nodiscard]] const std::vector<VkImage>& GetImages() const { return m_images; }
    [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    [[nodiscard]] bool NeedsRecreation() const { return m_needsRecreation; }

private:
    VkSurfaceFormatKHR ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                            uint32_t width, uint32_t height) const;
    bool CreateImageViews();

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR m_colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D m_extent = {0, 0};
    VkPresentModeKHR m_presentMode = VK_PRESENT_MODE_FIFO_KHR;

    SwapchainConfig m_config;
    bool m_needsRecreation = false;
};

// =============================================================================
// VulkanCommandPool (Single Responsibility)
// =============================================================================

/**
 * @brief Command buffer allocation result
 */
struct CommandBufferHandle {
    VkCommandBuffer buffer = VK_NULL_HANDLE;
    uint32_t poolIndex = 0;
    bool isValid() const { return buffer != VK_NULL_HANDLE; }
};

/**
 * @brief Per-frame command pool for efficient allocation
 */
class VulkanCommandPool {
public:
    VulkanCommandPool() = default;
    ~VulkanCommandPool();

    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
    VulkanCommandPool(VulkanCommandPool&& other) noexcept;
    VulkanCommandPool& operator=(VulkanCommandPool&& other) noexcept;

    /**
     * @brief Create command pool
     * @param device Vulkan device
     * @param queueFamily Queue family index
     * @param flags Pool creation flags
     * @param framesInFlight Number of frames in flight for per-frame pools
     */
    bool Create(VkDevice device, uint32_t queueFamily,
                VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                uint32_t framesInFlight = 2);

    /**
     * @brief Destroy command pool
     */
    void Destroy();

    /**
     * @brief Allocate primary command buffer for current frame
     */
    CommandBufferHandle AllocatePrimary(uint32_t frameIndex);

    /**
     * @brief Allocate secondary command buffers
     */
    std::vector<CommandBufferHandle> AllocateSecondary(uint32_t frameIndex, uint32_t count);

    /**
     * @brief Reset all command buffers for a frame
     */
    void ResetFrame(uint32_t frameIndex);

    /**
     * @brief Allocate one-time submit command buffer
     */
    VkCommandBuffer BeginSingleTimeCommands();

    /**
     * @brief End and submit one-time command buffer
     */
    void EndSingleTimeCommands(VkCommandBuffer buffer, VkQueue queue);

    [[nodiscard]] VkCommandPool GetPool(uint32_t frameIndex = 0) const;
    [[nodiscard]] bool IsValid() const { return !m_pools.empty(); }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkCommandPool> m_pools;
    std::vector<std::vector<VkCommandBuffer>> m_allocatedBuffers;
    std::vector<uint32_t> m_allocatedCount;
    uint32_t m_queueFamily = 0;
};

// =============================================================================
// VulkanDescriptorPool (Single Responsibility)
// =============================================================================

/**
 * @brief Descriptor pool configuration
 */
struct DescriptorPoolConfig {
    uint32_t maxSets = 1000;
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 500},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 500},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100}
    };
    bool allowFreeIndividual = false;  // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    bool updateAfterBind = true;       // VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
};

/**
 * @brief Manages descriptor set allocation
 */
class VulkanDescriptorPool {
public:
    VulkanDescriptorPool() = default;
    ~VulkanDescriptorPool();

    VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool(VulkanDescriptorPool&& other) noexcept;
    VulkanDescriptorPool& operator=(VulkanDescriptorPool&& other) noexcept;

    /**
     * @brief Create descriptor pool
     */
    bool Create(VkDevice device, const DescriptorPoolConfig& config = {});

    /**
     * @brief Destroy pool
     */
    void Destroy();

    /**
     * @brief Allocate descriptor sets
     */
    std::vector<VkDescriptorSet> Allocate(const std::vector<VkDescriptorSetLayout>& layouts);

    /**
     * @brief Allocate single descriptor set
     */
    VkDescriptorSet AllocateSingle(VkDescriptorSetLayout layout);

    /**
     * @brief Free descriptor sets (only if allowFreeIndividual is true)
     */
    void Free(const std::vector<VkDescriptorSet>& sets);

    /**
     * @brief Reset entire pool
     */
    void Reset();

    [[nodiscard]] VkDescriptorPool GetHandle() const { return m_pool; }
    [[nodiscard]] bool IsValid() const { return m_pool != VK_NULL_HANDLE; }
    [[nodiscard]] uint32_t GetAllocatedSetCount() const { return m_allocatedSets; }
    [[nodiscard]] uint32_t GetMaxSets() const { return m_maxSets; }

private:
    bool GrowPool();

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> m_fullPools;  // Old pools that are full

    DescriptorPoolConfig m_config;
    uint32_t m_allocatedSets = 0;
    uint32_t m_maxSets = 0;
};

// =============================================================================
// VulkanPipelineCache (Single Responsibility)
// =============================================================================

/**
 * @brief Pipeline creation info for caching
 */
struct PipelineCacheKey {
    std::vector<uint8_t> vertexShaderHash;
    std::vector<uint8_t> fragmentShaderHash;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpass = 0;
    // Add other pipeline state as needed

    bool operator==(const PipelineCacheKey& other) const;
};

/**
 * @brief Manages shader compilation and pipeline caching
 */
class VulkanPipelineCache {
public:
    VulkanPipelineCache() = default;
    ~VulkanPipelineCache();

    VulkanPipelineCache(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache(VulkanPipelineCache&& other) noexcept;
    VulkanPipelineCache& operator=(VulkanPipelineCache&& other) noexcept;

    /**
     * @brief Create pipeline cache
     * @param device Vulkan device
     * @param cacheFilePath Path to load/save cache (empty = no disk cache)
     */
    bool Create(VkDevice device, const std::string& cacheFilePath = "");

    /**
     * @brief Destroy cache and save to disk
     */
    void Destroy();

    /**
     * @brief Save cache to disk
     */
    bool SaveToDisk(const std::string& path);

    /**
     * @brief Load cache from disk
     */
    bool LoadFromDisk(const std::string& path);

    /**
     * @brief Create shader module from SPIR-V
     */
    VkShaderModule CreateShaderModule(const std::vector<uint32_t>& spirv);
    VkShaderModule CreateShaderModule(const uint32_t* spirv, size_t sizeBytes);

    /**
     * @brief Create graphics pipeline
     */
    VkPipeline CreateGraphicsPipeline(const VkGraphicsPipelineCreateInfo& createInfo);

    /**
     * @brief Create compute pipeline
     */
    VkPipeline CreateComputePipeline(const VkComputePipelineCreateInfo& createInfo);

    /**
     * @brief Destroy shader module
     */
    void DestroyShaderModule(VkShaderModule module);

    /**
     * @brief Destroy pipeline
     */
    void DestroyPipeline(VkPipeline pipeline);

    [[nodiscard]] VkPipelineCache GetHandle() const { return m_cache; }
    [[nodiscard]] bool IsValid() const { return m_cache != VK_NULL_HANDLE; }
    [[nodiscard]] size_t GetCacheSize() const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineCache m_cache = VK_NULL_HANDLE;
    std::string m_cacheFilePath;

    std::mutex m_mutex;
    std::vector<VkShaderModule> m_shaderModules;
    std::vector<VkPipeline> m_pipelines;
};

// =============================================================================
// VulkanSyncManager (Single Responsibility)
// =============================================================================

/**
 * @brief Timeline semaphore wrapper
 */
struct TimelineSemaphore {
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::atomic<uint64_t> value{0};

    uint64_t GetCurrentValue() const { return value.load(); }
    uint64_t Signal() { return ++value; }
};

/**
 * @brief Manages synchronization primitives
 */
class VulkanSyncManager {
public:
    VulkanSyncManager() = default;
    ~VulkanSyncManager();

    VulkanSyncManager(const VulkanSyncManager&) = delete;
    VulkanSyncManager& operator=(const VulkanSyncManager&) = delete;
    VulkanSyncManager(VulkanSyncManager&& other) noexcept;
    VulkanSyncManager& operator=(VulkanSyncManager&& other) noexcept;

    /**
     * @brief Initialize sync manager
     */
    bool Create(VkDevice device, uint32_t framesInFlight = 2);

    /**
     * @brief Destroy all sync objects
     */
    void Destroy();

    // Binary semaphores
    VkSemaphore CreateBinarySemaphore();
    void DestroyBinarySemaphore(VkSemaphore semaphore);

    // Timeline semaphores (Vulkan 1.2+)
    TimelineSemaphore* CreateTimelineSemaphore(uint64_t initialValue = 0);
    void DestroyTimelineSemaphore(TimelineSemaphore* semaphore);
    bool WaitTimelineSemaphore(TimelineSemaphore* semaphore, uint64_t value, uint64_t timeout = UINT64_MAX);
    bool SignalTimelineSemaphore(TimelineSemaphore* semaphore, uint64_t value);

    // Fences
    VkFence CreateFence(bool signaled = false);
    void DestroyFence(VkFence fence);
    bool WaitFence(VkFence fence, uint64_t timeout = UINT64_MAX);
    void ResetFence(VkFence fence);
    void ResetFences(const std::vector<VkFence>& fences);

    // Per-frame sync objects
    [[nodiscard]] VkSemaphore GetImageAvailableSemaphore(uint32_t frame) const;
    [[nodiscard]] VkSemaphore GetRenderFinishedSemaphore(uint32_t frame) const;
    [[nodiscard]] VkFence GetInFlightFence(uint32_t frame) const;
    [[nodiscard]] TimelineSemaphore* GetFrameTimelineSemaphore() const { return m_frameTimeline.get(); }

    /**
     * @brief Wait for frame fence and reset
     */
    void WaitForFrame(uint32_t frame);

    [[nodiscard]] uint32_t GetFramesInFlight() const { return m_framesInFlight; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    uint32_t m_framesInFlight = 2;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    std::unique_ptr<TimelineSemaphore> m_frameTimeline;

    // Tracking for cleanup
    std::vector<VkSemaphore> m_userSemaphores;
    std::vector<std::unique_ptr<TimelineSemaphore>> m_userTimelineSemaphores;
    std::vector<VkFence> m_userFences;

    std::mutex m_mutex;

    // Function pointers for timeline semaphores
    PFN_vkWaitSemaphores m_waitSemaphores = nullptr;
    PFN_vkSignalSemaphore m_signalSemaphore = nullptr;
    PFN_vkGetSemaphoreCounterValue m_getSemaphoreCounterValue = nullptr;
};

// =============================================================================
// LinuxVulkanContext - Main Context Class (Facade)
// =============================================================================

/**
 * @brief Linux-specific Vulkan context
 *
 * Provides a unified interface to all Vulkan subsystems following the
 * Facade pattern for simplified usage.
 */
class LinuxVulkanContext : public IVulkanContext {
public:
    LinuxVulkanContext() = default;
    ~LinuxVulkanContext() override;

    LinuxVulkanContext(const LinuxVulkanContext&) = delete;
    LinuxVulkanContext& operator=(const LinuxVulkanContext&) = delete;
    LinuxVulkanContext(LinuxVulkanContext&& other) noexcept;
    LinuxVulkanContext& operator=(LinuxVulkanContext&& other) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Check if Vulkan is available on this system
     */
    static bool IsVulkanAvailable();

    /**
     * @brief Detect preferred surface type based on environment
     */
    static LinuxSurfaceType DetectSurfaceType();

    /**
     * @brief Initialize context for X11 window
     */
    bool InitializeX11(Display* display, Window window, const VulkanConfig& config = {});

    /**
     * @brief Initialize context for Wayland surface
     */
    bool InitializeWayland(wl_display* display, wl_surface* surface, const VulkanConfig& config = {});

    /**
     * @brief Initialize context for DRM/KMS (headless or direct rendering)
     */
    bool InitializeDRM(const char* drmDevice, const VulkanConfig& config = {});

    /**
     * @brief Initialize context from GLFW window
     */
    bool InitializeGLFW(void* glfwWindow, const VulkanConfig& config = {});

    /**
     * @brief Shutdown and release all resources
     */
    void Shutdown();

    /**
     * @brief Handle context loss (GPU reset, driver crash)
     */
    void HandleContextLoss();

    /**
     * @brief Check if context is valid
     */
    [[nodiscard]] bool IsValid() const { return m_device.IsValid(); }

    // =========================================================================
    // IVulkanContext Interface Implementation
    // =========================================================================

    VkInstance GetInstance() const override { return m_instance.GetHandle(); }
    bool IsValidationEnabled() const override { return m_instance.IsValidationEnabled(); }

    VkDevice GetDevice() const override { return m_device.GetHandle(); }
    VkPhysicalDevice GetPhysicalDevice() const override { return m_device.GetPhysicalDevice(); }
    const PhysicalDeviceInfo& GetPhysicalDeviceInfo() const override { return m_device.GetPhysicalDeviceInfo(); }

    VkQueue GetGraphicsQueue() const override { return m_device.GetGraphicsQueue(); }
    VkQueue GetComputeQueue() const override { return m_device.GetComputeQueue(); }
    VkQueue GetTransferQueue() const override { return m_device.GetTransferQueue(); }
    uint32_t GetGraphicsQueueFamily() const override { return m_device.GetGraphicsQueueFamily(); }
    uint32_t GetComputeQueueFamily() const override { return m_device.GetComputeQueueFamily(); }
    uint32_t GetTransferQueueFamily() const override { return m_device.GetTransferQueueFamily(); }

    VmaAllocator GetAllocator() const override { return m_device.GetAllocator(); }

    VkSurfaceKHR GetSurface() const override { return m_surface; }
    LinuxSurfaceType GetSurfaceType() const override { return m_surfaceType; }

    bool SupportsFeature(const char* featureName) const override;
    VulkanFeatureLevel GetFeatureLevel() const override { return m_instance.GetFeatureLevel(); }

    // =========================================================================
    // Component Access
    // =========================================================================

    VulkanInstance& GetInstanceComponent() { return m_instance; }
    const VulkanInstance& GetInstanceComponent() const { return m_instance; }

    VulkanDevice& GetDeviceComponent() { return m_device; }
    const VulkanDevice& GetDeviceComponent() const { return m_device; }

    VulkanSwapchain& GetSwapchain() { return m_swapchain; }
    const VulkanSwapchain& GetSwapchain() const { return m_swapchain; }

    VulkanCommandPool& GetCommandPool() { return m_commandPool; }
    const VulkanCommandPool& GetCommandPool() const { return m_commandPool; }

    VulkanDescriptorPool& GetDescriptorPool() { return m_descriptorPool; }
    const VulkanDescriptorPool& GetDescriptorPool() const { return m_descriptorPool; }

    VulkanPipelineCache& GetPipelineCache() { return m_pipelineCache; }
    const VulkanPipelineCache& GetPipelineCache() const { return m_pipelineCache; }

    VulkanSyncManager& GetSyncManager() { return m_syncManager; }
    const VulkanSyncManager& GetSyncManager() const { return m_syncManager; }

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Resize swapchain
     */
    bool ResizeSwapchain(uint32_t width, uint32_t height);

    /**
     * @brief Begin frame rendering
     * @return Frame index, or UINT32_MAX on failure
     */
    uint32_t BeginFrame();

    /**
     * @brief End frame and present
     */
    bool EndFrame(uint32_t frameIndex);

    /**
     * @brief Get current frame index
     */
    [[nodiscard]] uint32_t GetCurrentFrame() const { return m_currentFrame; }

    /**
     * @brief Wait for all GPU work to complete
     */
    void WaitIdle();

private:
    bool CreateSurface(const VulkanConfig& config);
    bool SelectPhysicalDevice(const VulkanConfig& config);
    bool CreateDeviceAndQueues(const VulkanConfig& config);
    bool CreateSwapchain(uint32_t width, uint32_t height, const VulkanConfig& config);
    bool CreateSyncObjects(const VulkanConfig& config);
    void CleanupSurface();

    // Subsystems
    VulkanInstance m_instance;
    VulkanDevice m_device;
    VulkanSwapchain m_swapchain;
    VulkanCommandPool m_commandPool;
    VulkanDescriptorPool m_descriptorPool;
    VulkanPipelineCache m_pipelineCache;
    VulkanSyncManager m_syncManager;

    // Surface management
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    LinuxSurfaceType m_surfaceType = LinuxSurfaceType::Unknown;

    // X11 state
    Display* m_x11Display = nullptr;
    Window m_x11Window = 0;

    // Wayland state
    wl_display* m_waylandDisplay = nullptr;
    wl_surface* m_waylandSurface = nullptr;

    // DRM/KMS state
    int m_drmFd = -1;
    gbm_device* m_gbmDevice = nullptr;
    gbm_surface* m_gbmSurface = nullptr;

    // Frame tracking
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_frameInProgress = false;

    // Configuration
    VulkanConfig m_config;

    // Extension function pointers
    PFN_vkCreateXlibSurfaceKHR m_createXlibSurface = nullptr;
    PFN_vkCreateWaylandSurfaceKHR m_createWaylandSurface = nullptr;
};

// =============================================================================
// Legacy Compatibility Alias
// =============================================================================

using LinuxVulkan = LinuxVulkanContext;
using LinuxVulkanConfig = VulkanConfig;
using LinuxVulkanSurfaceType = LinuxSurfaceType;

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
