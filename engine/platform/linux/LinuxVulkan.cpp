/**
 * @file LinuxVulkan.cpp
 * @brief Linux Vulkan surface implementation for X11 and Wayland
 */

#include "LinuxVulkan.hpp"

#ifdef NOVA_PLATFORM_LINUX

#include <cstring>
#include <algorithm>
#include <iostream>
#include <dlfcn.h>

// X11 surface extension
#ifdef VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan_xlib.h>
#endif

// Wayland surface extension
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan_wayland.h>
#endif

// GLFW for surface creation helper
#include <GLFW/glfw3.h>

namespace Nova {

// =============================================================================
// Validation Layers
// =============================================================================

static const std::vector<const char*> s_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static bool CheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : s_validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

LinuxVulkan::LinuxVulkan() = default;

LinuxVulkan::~LinuxVulkan() {
    Shutdown();
}

LinuxVulkan::LinuxVulkan(LinuxVulkan&& other) noexcept
    : m_instance(other.m_instance)
    , m_debugMessenger(other.m_debugMessenger)
    , m_surfaceType(other.m_surfaceType)
    , m_hasX11Support(other.m_hasX11Support)
    , m_hasWaylandSupport(other.m_hasWaylandSupport)
    , m_validationEnabled(other.m_validationEnabled)
    , m_vkCreateXlibSurfaceKHR(other.m_vkCreateXlibSurfaceKHR)
    , m_vkCreateWaylandSurfaceKHR(other.m_vkCreateWaylandSurfaceKHR)
    , m_vkSetDebugUtilsObjectNameEXT(other.m_vkSetDebugUtilsObjectNameEXT)
{
    other.m_instance = VK_NULL_HANDLE;
    other.m_debugMessenger = VK_NULL_HANDLE;
}

LinuxVulkan& LinuxVulkan::operator=(LinuxVulkan&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_instance = other.m_instance;
        m_debugMessenger = other.m_debugMessenger;
        m_surfaceType = other.m_surfaceType;
        m_hasX11Support = other.m_hasX11Support;
        m_hasWaylandSupport = other.m_hasWaylandSupport;
        m_validationEnabled = other.m_validationEnabled;
        m_vkCreateXlibSurfaceKHR = other.m_vkCreateXlibSurfaceKHR;
        m_vkCreateWaylandSurfaceKHR = other.m_vkCreateWaylandSurfaceKHR;
        m_vkSetDebugUtilsObjectNameEXT = other.m_vkSetDebugUtilsObjectNameEXT;

        other.m_instance = VK_NULL_HANDLE;
        other.m_debugMessenger = VK_NULL_HANDLE;
    }
    return *this;
}

// =============================================================================
// Static Methods
// =============================================================================

bool LinuxVulkan::IsVulkanAvailable() {
    // Try to load Vulkan library
    void* vulkanLib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!vulkanLib) {
        vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    }

    if (!vulkanLib) {
        return false;
    }

    // Check if we can enumerate instance extensions
    auto vkEnumerateInstanceExtensionProperties =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
            dlsym(vulkanLib, "vkEnumerateInstanceExtensionProperties"));

    dlclose(vulkanLib);

    return vkEnumerateInstanceExtensionProperties != nullptr;
}

std::vector<const char*> LinuxVulkan::GetRequiredInstanceExtensions(bool preferWayland) {
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    };

    // Check for X11 or Wayland session
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* x11Display = std::getenv("DISPLAY");
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");

    bool hasWayland = waylandDisplay != nullptr ||
                      (sessionType && std::strcmp(sessionType, "wayland") == 0);
    bool hasX11 = x11Display != nullptr ||
                  (sessionType && std::strcmp(sessionType, "x11") == 0);

    // Add platform-specific surface extension
    if (preferWayland && hasWayland) {
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
    } else if (hasX11) {
#ifdef VK_USE_PLATFORM_XLIB_KHR
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
    } else if (hasWayland) {
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
    }

    return extensions;
}

std::vector<const char*> LinuxVulkan::GetRequiredDeviceExtensions() {
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
}

// =============================================================================
// Initialization
// =============================================================================

bool LinuxVulkan::Initialize(const LinuxVulkanConfig& config) {
    if (m_instance != VK_NULL_HANDLE) {
        return true;  // Already initialized
    }

    m_validationEnabled = config.enableValidationLayers;

    if (!CreateInstance(config)) {
        return false;
    }

    if (m_validationEnabled) {
        SetupDebugMessenger();
    }

    LoadExtensionFunctions();

    return true;
}

bool LinuxVulkan::CreateInstance(const LinuxVulkanConfig& config) {
    // Check validation layer support
    if (config.enableValidationLayers && !CheckValidationLayerSupport()) {
        std::cerr << "LinuxVulkan: Validation layers requested but not available\n";
        m_validationEnabled = false;
    }

    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Nova3D Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Nova3D Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    // Get required extensions
    auto extensions = GetRequiredInstanceExtensions(config.preferWayland);

    // Add debug utils extension if validation enabled
    if (m_validationEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Add any additional extensions
    for (const auto& ext : config.additionalInstanceExtensions) {
        extensions.push_back(ext);
    }

    // Check extension availability
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    // Track which surface types are supported
    for (const auto& available : availableExtensions) {
#ifdef VK_USE_PLATFORM_XLIB_KHR
        if (std::strcmp(available.extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0) {
            m_hasX11Support = true;
        }
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
        if (std::strcmp(available.extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) == 0) {
            m_hasWaylandSupport = true;
        }
#endif
    }

    // Instance create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Debug messenger for instance creation/destruction
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_validationEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(s_validationLayers.size());
        createInfo.ppEnabledLayerNames = s_validationLayers.data();

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;

        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    // Create instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        std::cerr << "LinuxVulkan: Failed to create Vulkan instance (error: " << result << ")\n";
        return false;
    }

    return true;
}

bool LinuxVulkan::SetupDebugMessenger() {
    if (!m_validationEnabled || m_instance == VK_NULL_HANDLE) {
        return false;
    }

    auto vkCreateDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    if (!vkCreateDebugUtilsMessengerEXT) {
        return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    return vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) == VK_SUCCESS;
}

void LinuxVulkan::LoadExtensionFunctions() {
    if (m_instance == VK_NULL_HANDLE) {
        return;
    }

#ifdef VK_USE_PLATFORM_XLIB_KHR
    m_vkCreateXlibSurfaceKHR = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        vkGetInstanceProcAddr(m_instance, "vkCreateXlibSurfaceKHR"));
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    m_vkCreateWaylandSurfaceKHR = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
        vkGetInstanceProcAddr(m_instance, "vkCreateWaylandSurfaceKHR"));
#endif

    m_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(m_instance, "vkSetDebugUtilsObjectNameEXT"));
}

void LinuxVulkan::Shutdown() {
    if (m_debugMessenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        auto vkDestroyDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    m_vkCreateXlibSurfaceKHR = nullptr;
    m_vkCreateWaylandSurfaceKHR = nullptr;
    m_vkSetDebugUtilsObjectNameEXT = nullptr;
}

// =============================================================================
// Surface Creation
// =============================================================================

VkSurfaceKHR LinuxVulkan::CreateX11Surface(Display* display, Window window) {
#ifdef VK_USE_PLATFORM_XLIB_KHR
    if (!m_vkCreateXlibSurfaceKHR || m_instance == VK_NULL_HANDLE) {
        std::cerr << "LinuxVulkan: X11 surface creation not available\n";
        return VK_NULL_HANDLE;
    }

    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.dpy = display;
    createInfo.window = window;

    VkSurfaceKHR surface;
    VkResult result = m_vkCreateXlibSurfaceKHR(m_instance, &createInfo, nullptr, &surface);
    if (result != VK_SUCCESS) {
        std::cerr << "LinuxVulkan: Failed to create X11 surface (error: " << result << ")\n";
        return VK_NULL_HANDLE;
    }

    m_surfaceType = LinuxVulkanSurfaceType::X11;
    return surface;
#else
    (void)display;
    (void)window;
    std::cerr << "LinuxVulkan: X11 support not compiled in\n";
    return VK_NULL_HANDLE;
#endif
}

VkSurfaceKHR LinuxVulkan::CreateWaylandSurface(wl_display* display, wl_surface* surface) {
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    if (!m_vkCreateWaylandSurfaceKHR || m_instance == VK_NULL_HANDLE) {
        std::cerr << "LinuxVulkan: Wayland surface creation not available\n";
        return VK_NULL_HANDLE;
    }

    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.display = display;
    createInfo.surface = surface;

    VkSurfaceKHR vkSurface;
    VkResult result = m_vkCreateWaylandSurfaceKHR(m_instance, &createInfo, nullptr, &vkSurface);
    if (result != VK_SUCCESS) {
        std::cerr << "LinuxVulkan: Failed to create Wayland surface (error: " << result << ")\n";
        return VK_NULL_HANDLE;
    }

    m_surfaceType = LinuxVulkanSurfaceType::Wayland;
    return vkSurface;
#else
    (void)display;
    (void)surface;
    std::cerr << "LinuxVulkan: Wayland support not compiled in\n";
    return VK_NULL_HANDLE;
#endif
}

VkSurfaceKHR LinuxVulkan::CreateSurfaceFromGLFW(void* glfwWindow) {
    if (m_instance == VK_NULL_HANDLE || !glfwWindow) {
        return VK_NULL_HANDLE;
    }

    VkSurfaceKHR surface;
    VkResult result = glfwCreateWindowSurface(m_instance, static_cast<GLFWwindow*>(glfwWindow),
                                               nullptr, &surface);
    if (result != VK_SUCCESS) {
        std::cerr << "LinuxVulkan: Failed to create GLFW surface (error: " << result << ")\n";
        return VK_NULL_HANDLE;
    }

    // Detect surface type from environment
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    if (sessionType) {
        if (std::strcmp(sessionType, "wayland") == 0) {
            m_surfaceType = LinuxVulkanSurfaceType::Wayland;
        } else if (std::strcmp(sessionType, "x11") == 0) {
            m_surfaceType = LinuxVulkanSurfaceType::X11;
        }
    }

    return surface;
}

void LinuxVulkan::DestroySurface(VkSurfaceKHR surface) {
    if (m_instance != VK_NULL_HANDLE && surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, surface, nullptr);
    }
}

// =============================================================================
// Device Selection
// =============================================================================

VkPhysicalDevice LinuxVulkan::SelectPhysicalDevice(VkSurfaceKHR surface) {
    if (m_instance == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        std::cerr << "LinuxVulkan: No Vulkan-capable GPUs found\n";
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Score and select best device
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = 0;

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

        // Check queue family support
        uint32_t graphicsFamily, presentFamily;
        if (!FindQueueFamilies(device, surface, graphicsFamily, presentFamily)) {
            continue;
        }

        // Check extension support
        if (!CheckDeviceExtensionSupport(device)) {
            continue;
        }

        // Check swapchain support
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
        QuerySwapchainSupport(device, surface, capabilities, formats, presentModes);

        if (formats.empty() || presentModes.empty()) {
            continue;
        }

        // Score the device
        int score = 0;

        // Prefer discrete GPUs
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }

        // Higher VRAM is better
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
            if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                score += static_cast<int>(memProperties.memoryHeaps[i].size / (1024 * 1024 * 100));
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }

    if (bestDevice != VK_NULL_HANDLE) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(bestDevice, &properties);
        std::cout << "LinuxVulkan: Selected GPU: " << properties.deviceName << "\n";
    }

    return bestDevice;
}

bool LinuxVulkan::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface,
                                     uint32_t& graphicsFamily, uint32_t& presentFamily) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    bool foundGraphics = false;
    bool foundPresent = false;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
            foundGraphics = true;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            presentFamily = i;
            foundPresent = true;
        }

        if (foundGraphics && foundPresent) {
            break;
        }
    }

    return foundGraphics && foundPresent;
}

bool LinuxVulkan::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    auto requiredExtensions = GetRequiredDeviceExtensions();

    for (const char* required : requiredExtensions) {
        bool found = false;
        for (const auto& available : availableExtensions) {
            if (std::strcmp(required, available.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// Swapchain Helpers
// =============================================================================

void LinuxVulkan::QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface,
                                         VkSurfaceCapabilitiesKHR& capabilities,
                                         std::vector<VkSurfaceFormatKHR>& formats,
                                         std::vector<VkPresentModeKHR>& presentModes) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
    }
}

VkSurfaceFormatKHR LinuxVulkan::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    // Prefer SRGB format
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // Fallback to first available
    return formats[0];
}

VkPresentModeKHR LinuxVulkan::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                                 bool vsync) {
    if (vsync) {
        // FIFO is always supported and provides vsync
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // Prefer mailbox (triple buffering) for tearing-free non-vsync
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }

    // Immediate for lowest latency
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D LinuxVulkan::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                          uint32_t windowWidth, uint32_t windowHeight) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {windowWidth, windowHeight};

    extent.width = std::clamp(extent.width,
                              capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);

    return extent;
}

// =============================================================================
// Debug Utils
// =============================================================================

void LinuxVulkan::SetDebugObjectName(VkDevice device, VkObjectType objectType,
                                      uint64_t object, const char* name) {
    if (!m_vkSetDebugUtilsObjectNameEXT || !m_validationEnabled) {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = object;
    nameInfo.pObjectName = name;

    m_vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}

VKAPI_ATTR VkBool32 VKAPI_CALL LinuxVulkan::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    (void)messageType;
    (void)pUserData;

    const char* severity = "";
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            severity = "VERBOSE";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            severity = "INFO";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            severity = "WARNING";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            severity = "ERROR";
            break;
        default:
            break;
    }

    std::cerr << "[Vulkan " << severity << "] " << pCallbackData->pMessage << "\n";

    return VK_FALSE;
}

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
