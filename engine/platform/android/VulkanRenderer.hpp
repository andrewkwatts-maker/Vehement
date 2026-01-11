#pragma once

/**
 * @file VulkanRenderer.hpp
 * @brief Vulkan renderer for Android with high-performance rendering support
 *
 * Provides a Vulkan rendering backend for Android devices that support it.
 * Vulkan offers lower overhead and more control compared to OpenGL ES.
 */

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <android/native_window.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Nova {

/**
 * @brief Shader type enumeration
 */
enum class ShaderType {
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessControl,
    TessEvaluation
};

/**
 * @brief Vulkan device capabilities
 */
struct VulkanCapabilities {
    std::string deviceName;
    std::string driverVersion;
    uint32_t apiVersion = 0;
    uint32_t maxImageDimension2D = 0;
    uint32_t maxUniformBufferRange = 0;
    uint32_t maxStorageBufferRange = 0;
    uint32_t maxPushConstantsSize = 0;
    bool supportsMultiview = false;
    bool supportsComputeShaders = true;
    bool supportsGeometryShaders = false;
    bool supportsTessellation = false;
    bool supportsWideLines = false;
    bool supportsDepthClamp = false;
};

/**
 * @brief Vulkan pipeline configuration
 */
struct PipelineConfig {
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
    bool blendEnable = false;
    VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
};

/**
 * @brief Vulkan buffer information
 */
struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mappedData = nullptr;
};

/**
 * @brief Vulkan image information
 */
struct VulkanImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

/**
 * @brief High-performance Vulkan renderer for Android
 *
 * Features:
 * - Swapchain management with triple buffering
 * - Dynamic pipeline creation
 * - SPIR-V shader loading and runtime compilation
 * - Efficient memory management
 * - Descriptor set pooling
 */
class VulkanRenderer {
public:
    /**
     * @brief Renderer configuration
     */
    struct Config {
        bool enableValidationLayers = false;
        bool enableDebugMarkers = false;
        uint32_t preferredSwapchainImages = 3;
        VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        bool sRGB = false;
    };

    VulkanRenderer() = default;
    ~VulkanRenderer();

    // Non-copyable, non-movable
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;

    /**
     * @brief Initialize Vulkan renderer
     * @param window Native window handle
     * @return true if initialization succeeded
     */
    bool Initialize(ANativeWindow* window);

    /**
     * @brief Initialize with custom configuration
     * @param window Native window handle
     * @param config Renderer configuration
     * @return true if initialization succeeded
     */
    bool Initialize(ANativeWindow* window, const Config& config);

    /**
     * @brief Shutdown and release all resources
     */
    void Shutdown();

    /**
     * @brief Recreate swapchain (e.g., after window resize)
     * @return true if successful
     */
    bool RecreateSwapchain();

    /**
     * @brief Check if renderer is ready
     */
    [[nodiscard]] bool IsValid() const { return m_device != VK_NULL_HANDLE; }

    // -------------------------------------------------------------------------
    // Pipeline Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create a graphics pipeline
     * @param vertexSpirv Vertex shader SPIR-V bytecode
     * @param fragmentSpirv Fragment shader SPIR-V bytecode
     * @param config Pipeline configuration
     * @return Pipeline handle (0 on failure)
     */
    uint32_t CreatePipeline(const std::vector<uint8_t>& vertexSpirv,
                            const std::vector<uint8_t>& fragmentSpirv,
                            const PipelineConfig& config = PipelineConfig{});

    /**
     * @brief Create a compute pipeline
     * @param computeSpirv Compute shader SPIR-V bytecode
     * @return Pipeline handle (0 on failure)
     */
    uint32_t CreateComputePipeline(const std::vector<uint8_t>& computeSpirv);

    /**
     * @brief Destroy a pipeline
     * @param pipelineId Pipeline handle
     */
    void DestroyPipeline(uint32_t pipelineId);

    /**
     * @brief Bind a pipeline for rendering
     * @param pipelineId Pipeline handle
     */
    void BindPipeline(uint32_t pipelineId);

    // -------------------------------------------------------------------------
    // Buffer Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create a vertex buffer
     * @param data Vertex data
     * @param size Size in bytes
     * @return Buffer handle
     */
    VulkanBuffer CreateVertexBuffer(const void* data, VkDeviceSize size);

    /**
     * @brief Create an index buffer
     * @param data Index data
     * @param size Size in bytes
     * @return Buffer handle
     */
    VulkanBuffer CreateIndexBuffer(const void* data, VkDeviceSize size);

    /**
     * @brief Create a uniform buffer
     * @param size Size in bytes
     * @return Buffer handle (with mapped memory)
     */
    VulkanBuffer CreateUniformBuffer(VkDeviceSize size);

    /**
     * @brief Create a storage buffer
     * @param size Size in bytes
     * @param hostVisible If true, memory is mapped for CPU access
     * @return Buffer handle
     */
    VulkanBuffer CreateStorageBuffer(VkDeviceSize size, bool hostVisible = false);

    /**
     * @brief Destroy a buffer
     * @param buffer Buffer to destroy
     */
    void DestroyBuffer(VulkanBuffer& buffer);

    /**
     * @brief Update buffer data
     * @param buffer Target buffer (must be host visible)
     * @param data Source data
     * @param size Size in bytes
     * @param offset Offset into buffer
     */
    void UpdateBuffer(VulkanBuffer& buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    // -------------------------------------------------------------------------
    // Texture Management
    // -------------------------------------------------------------------------

    /**
     * @brief Create a 2D texture
     * @param width Texture width
     * @param height Texture height
     * @param format Pixel format
     * @param data Initial data (optional)
     * @return Texture handle
     */
    VulkanImage CreateTexture2D(uint32_t width, uint32_t height, VkFormat format, const void* data = nullptr);

    /**
     * @brief Destroy a texture
     * @param image Texture to destroy
     */
    void DestroyTexture(VulkanImage& image);

    // -------------------------------------------------------------------------
    // Frame Rendering
    // -------------------------------------------------------------------------

    /**
     * @brief Begin a new frame
     * @return true if frame can be rendered
     */
    bool BeginFrame();

    /**
     * @brief End frame and present
     */
    void EndFrame();

    /**
     * @brief Set clear color for next frame
     * @param r Red component (0-1)
     * @param g Green component (0-1)
     * @param b Blue component (0-1)
     * @param a Alpha component (0-1)
     */
    void SetClearColor(float r, float g, float b, float a = 1.0f);

    /**
     * @brief Set viewport
     * @param x X offset
     * @param y Y offset
     * @param width Viewport width
     * @param height Viewport height
     */
    void SetViewport(float x, float y, float width, float height);

    /**
     * @brief Set scissor rectangle
     */
    void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

    // -------------------------------------------------------------------------
    // Drawing Commands
    // -------------------------------------------------------------------------

    /**
     * @brief Draw indexed geometry
     * @param indexCount Number of indices
     * @param instanceCount Number of instances
     * @param firstIndex Starting index
     * @param vertexOffset Vertex offset
     * @param firstInstance First instance ID
     */
    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                     uint32_t firstInstance = 0);

    /**
     * @brief Draw non-indexed geometry
     * @param vertexCount Number of vertices
     * @param instanceCount Number of instances
     * @param firstVertex Starting vertex
     * @param firstInstance First instance ID
     */
    void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
              uint32_t firstVertex = 0, uint32_t firstInstance = 0);

    /**
     * @brief Bind vertex buffer
     * @param buffer Buffer to bind
     * @param binding Binding index
     */
    void BindVertexBuffer(const VulkanBuffer& buffer, uint32_t binding = 0);

    /**
     * @brief Bind index buffer
     * @param buffer Buffer to bind
     * @param indexType Index type (UINT16 or UINT32)
     */
    void BindIndexBuffer(const VulkanBuffer& buffer, VkIndexType indexType = VK_INDEX_TYPE_UINT32);

    /**
     * @brief Push constants to current pipeline
     * @param stageFlags Shader stages
     * @param offset Offset in push constant block
     * @param size Size of data
     * @param data Data to push
     */
    void PushConstants(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* data);

    // -------------------------------------------------------------------------
    // Shader Utilities
    // -------------------------------------------------------------------------

    /**
     * @brief Compile GLSL shader to SPIR-V
     * @param glslSource GLSL source code
     * @param type Shader type
     * @return SPIR-V bytecode (empty on failure)
     *
     * Note: Requires shaderc library at runtime
     */
    std::vector<uint8_t> CompileGLSLToSpirv(const std::string& glslSource, ShaderType type);

    /**
     * @brief Load SPIR-V shader from asset
     * @param path Asset path
     * @return SPIR-V bytecode
     */
    std::vector<uint8_t> LoadShaderAsset(const std::string& path);

    // -------------------------------------------------------------------------
    // Query Functions
    // -------------------------------------------------------------------------

    /**
     * @brief Get device capabilities
     */
    [[nodiscard]] const VulkanCapabilities& GetCapabilities() const { return m_capabilities; }

    /**
     * @brief Get current swapchain extent
     */
    [[nodiscard]] VkExtent2D GetSwapchainExtent() const { return m_swapchainExtent; }

    /**
     * @brief Get swapchain image format
     */
    [[nodiscard]] VkFormat GetSwapchainFormat() const { return m_swapchainFormat; }

    /**
     * @brief Get current frame index
     */
    [[nodiscard]] uint32_t GetCurrentFrame() const { return m_currentFrame; }

    /**
     * @brief Get Vulkan instance
     */
    [[nodiscard]] VkInstance GetInstance() const { return m_instance; }

    /**
     * @brief Get Vulkan device
     */
    [[nodiscard]] VkDevice GetDevice() const { return m_device; }

    /**
     * @brief Get physical device
     */
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }

    /**
     * @brief Get current command buffer
     */
    [[nodiscard]] VkCommandBuffer GetCommandBuffer() const;

private:
    // Initialization helpers
    bool CreateInstance();
    bool CreateSurface(ANativeWindow* window);
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
    bool CreateDescriptorPool();

    // Cleanup helpers
    void CleanupSwapchain();

    // Utility functions
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkShaderModule CreateShaderModule(const std::vector<uint8_t>& code);
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
    void TransitionImageLayout(VkImage image, VkFormat format,
                               VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    // Queue family helpers
    struct QueueFamilyIndices {
        int graphicsFamily = -1;
        int presentFamily = -1;
        bool IsComplete() const { return graphicsFamily >= 0 && presentFamily >= 0; }
    };
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

    // Swapchain support helpers
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Vulkan handles
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent{0, 0};

    // Depth buffer
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    // Render pass and framebuffers
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    // Command buffers
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Synchronization
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;

    // Descriptors
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    // Pipeline cache
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    std::unordered_map<uint32_t, VkPipeline> m_pipelines;
    std::unordered_map<uint32_t, VkPipelineLayout> m_pipelineLayouts;
    uint32_t m_nextPipelineId = 1;
    uint32_t m_boundPipelineId = 0;

    // Configuration
    Config m_config;
    VulkanCapabilities m_capabilities;
    QueueFamilyIndices m_queueFamilies;

    // Native window reference
    ANativeWindow* m_window = nullptr;

    // State
    bool m_initialized = false;
    bool m_frameStarted = false;
    VkClearColorValue m_clearColor{{0.1f, 0.1f, 0.15f, 1.0f}};
};

} // namespace Nova
