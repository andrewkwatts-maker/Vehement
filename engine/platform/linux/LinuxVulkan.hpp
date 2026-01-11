#pragma once

/**
 * @file LinuxVulkan.hpp
 * @brief Linux Vulkan surface creation for X11 and Wayland
 *
 * Provides Vulkan surface creation and management for Linux platforms,
 * supporting both X11 (VK_KHR_xlib_surface) and Wayland (VK_KHR_wayland_surface).
 */

#ifdef NOVA_PLATFORM_LINUX

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>

// Forward declarations
struct _XDisplay;
typedef struct _XDisplay Display;
typedef unsigned long Window;
struct wl_display;
struct wl_surface;

namespace Nova {

/**
 * @brief Vulkan surface type for Linux
 */
enum class LinuxVulkanSurfaceType {
    Unknown,
    X11,
    Wayland
};

/**
 * @brief Vulkan initialization configuration
 */
struct LinuxVulkanConfig {
    bool enableValidationLayers = false;
    bool preferWayland = false;  // Prefer Wayland over X11 if both available
    std::vector<const char*> additionalInstanceExtensions;
    std::vector<const char*> additionalDeviceExtensions;
};

/**
 * @brief Linux Vulkan surface manager
 *
 * Handles Vulkan instance creation, surface creation, and extension management
 * for Linux display servers (X11 and Wayland).
 */
class LinuxVulkan {
public:
    LinuxVulkan();
    ~LinuxVulkan();

    // Non-copyable
    LinuxVulkan(const LinuxVulkan&) = delete;
    LinuxVulkan& operator=(const LinuxVulkan&) = delete;

    // Move semantics
    LinuxVulkan(LinuxVulkan&& other) noexcept;
    LinuxVulkan& operator=(LinuxVulkan&& other) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Check if Vulkan is available on this system
     */
    static bool IsVulkanAvailable();

    /**
     * @brief Get required instance extensions for Linux
     * @param preferWayland If true, prefer Wayland extensions over X11
     * @return List of required extension names
     */
    static std::vector<const char*> GetRequiredInstanceExtensions(bool preferWayland = false);

    /**
     * @brief Get required device extensions
     * @return List of required device extension names
     */
    static std::vector<const char*> GetRequiredDeviceExtensions();

    /**
     * @brief Initialize Vulkan instance
     * @param config Configuration options
     * @return true if initialization succeeded
     */
    bool Initialize(const LinuxVulkanConfig& config = {});

    /**
     * @brief Shutdown Vulkan and release resources
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_instance != VK_NULL_HANDLE; }

    // =========================================================================
    // Surface Creation
    // =========================================================================

    /**
     * @brief Create Vulkan surface for X11 window
     * @param display X11 display pointer
     * @param window X11 window handle
     * @return Created surface, or VK_NULL_HANDLE on failure
     */
    VkSurfaceKHR CreateX11Surface(Display* display, Window window);

    /**
     * @brief Create Vulkan surface for Wayland
     * @param display Wayland display pointer
     * @param surface Wayland surface pointer
     * @return Created surface, or VK_NULL_HANDLE on failure
     */
    VkSurfaceKHR CreateWaylandSurface(wl_display* display, wl_surface* surface);

    /**
     * @brief Create surface from GLFW window (auto-detects X11/Wayland)
     * @param glfwWindow GLFW window pointer
     * @return Created surface, or VK_NULL_HANDLE on failure
     */
    VkSurfaceKHR CreateSurfaceFromGLFW(void* glfwWindow);

    /**
     * @brief Destroy a previously created surface
     * @param surface Surface to destroy
     */
    void DestroySurface(VkSurfaceKHR surface);

    // =========================================================================
    // Device Selection
    // =========================================================================

    /**
     * @brief Select best physical device for rendering
     * @param surface Surface to check presentation support
     * @return Selected physical device, or VK_NULL_HANDLE if none suitable
     */
    VkPhysicalDevice SelectPhysicalDevice(VkSurfaceKHR surface);

    /**
     * @brief Get queue family indices for a device
     * @param device Physical device
     * @param surface Surface for presentation queue
     * @param graphicsFamily Output: graphics queue family index
     * @param presentFamily Output: present queue family index
     * @return true if suitable queue families found
     */
    bool FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface,
                           uint32_t& graphicsFamily, uint32_t& presentFamily);

    /**
     * @brief Check if device supports required extensions
     * @param device Physical device to check
     * @return true if all required extensions supported
     */
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

    // =========================================================================
    // Swapchain Helpers
    // =========================================================================

    /**
     * @brief Query swapchain support details
     * @param device Physical device
     * @param surface Surface
     * @param capabilities Output: surface capabilities
     * @param formats Output: supported surface formats
     * @param presentModes Output: supported present modes
     */
    void QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface,
                               VkSurfaceCapabilitiesKHR& capabilities,
                               std::vector<VkSurfaceFormatKHR>& formats,
                               std::vector<VkPresentModeKHR>& presentModes);

    /**
     * @brief Choose optimal surface format
     * @param formats Available formats
     * @return Chosen format
     */
    static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);

    /**
     * @brief Choose optimal present mode
     * @param presentModes Available present modes
     * @param vsync Enable vsync (prefer FIFO) or disable (prefer Mailbox/Immediate)
     * @return Chosen present mode
     */
    static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                               bool vsync = true);

    /**
     * @brief Choose optimal swap extent
     * @param capabilities Surface capabilities
     * @param windowWidth Desired width
     * @param windowHeight Desired height
     * @return Chosen extent
     */
    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                        uint32_t windowWidth, uint32_t windowHeight);

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] VkInstance GetInstance() const { return m_instance; }
    [[nodiscard]] LinuxVulkanSurfaceType GetSurfaceType() const { return m_surfaceType; }
    [[nodiscard]] bool HasX11Support() const { return m_hasX11Support; }
    [[nodiscard]] bool HasWaylandSupport() const { return m_hasWaylandSupport; }

    // =========================================================================
    // Debug Utils (when validation layers enabled)
    // =========================================================================

    /**
     * @brief Set debug object name (for RenderDoc, Vulkan validation, etc.)
     * @param device Logical device
     * @param objectType Type of object
     * @param object Object handle
     * @param name Debug name
     */
    void SetDebugObjectName(VkDevice device, VkObjectType objectType,
                            uint64_t object, const char* name);

private:
    // Vulkan handles
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    // Surface type tracking
    LinuxVulkanSurfaceType m_surfaceType = LinuxVulkanSurfaceType::Unknown;
    bool m_hasX11Support = false;
    bool m_hasWaylandSupport = false;
    bool m_validationEnabled = false;

    // Function pointers for extensions
    PFN_vkCreateXlibSurfaceKHR m_vkCreateXlibSurfaceKHR = nullptr;
    PFN_vkCreateWaylandSurfaceKHR m_vkCreateWaylandSurfaceKHR = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT m_vkSetDebugUtilsObjectNameEXT = nullptr;

    // Helpers
    bool CreateInstance(const LinuxVulkanConfig& config);
    bool SetupDebugMessenger();
    void LoadExtensionFunctions();

    // Validation layer callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
