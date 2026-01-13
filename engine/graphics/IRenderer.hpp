#pragma once

/**
 * @file IRenderer.hpp
 * @brief Pure virtual interface for all renderers in the Vehement SDF Engine
 *
 * This file defines the core IRenderer interface that provides a unified abstraction
 * for all rendering backends in the engine. It supports both traditional polygon
 * rasterization and SDF (Signed Distance Field) raymarching pipelines.
 *
 * Design Philosophy:
 * - Interface Segregation: Small, focused interfaces that can be composed
 * - Dependency Inversion: High-level modules depend on abstractions, not implementations
 * - Open/Closed: Extensible for new backends without modifying existing code
 * - Command Pattern: Render commands encapsulate draw operations for deferred execution
 *
 * Supported Rendering Approaches:
 * - Traditional polygon rasterization (OpenGL, Vulkan, DX12, Metal)
 * - SDF raymarching with global illumination
 * - Hybrid rendering combining both approaches
 * - Compute shader-based rendering
 *
 * @see RenderBackend.hpp For the existing IRenderBackend interface
 * @see Renderer.hpp For the concrete OpenGL implementation
 * @see SDFRenderer.hpp For SDF-specific rendering
 *
 * @author Nova Engine Team
 * @copyright 2025 Nova Engine
 */

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class Texture;
class Shader;
class Mesh;
class Material;
class Camera;
class Framebuffer;

// ============================================================================
// Renderer Backend Enumeration
// ============================================================================

/**
 * @brief Supported graphics API backends
 *
 * Identifies the underlying graphics API used by a renderer implementation.
 * Used for capability queries and backend-specific optimizations.
 */
enum class RendererBackend : uint8_t {
    None = 0,       ///< No backend (uninitialized or null renderer)
    OpenGL,         ///< OpenGL 4.3+ Core Profile
    Vulkan,         ///< Vulkan 1.2+
    DX12,           ///< DirectX 12 (Windows only)
    Metal,          ///< Metal (macOS/iOS only)
    WebGPU,         ///< WebGPU (cross-platform web)
    Software,       ///< CPU software rasterizer (fallback)
    Count           ///< Number of backends (for iteration)
};

/**
 * @brief Convert backend enum to string
 * @param backend The backend type
 * @return Human-readable name of the backend
 */
constexpr std::string_view RendererBackendToString(RendererBackend backend) noexcept {
    switch (backend) {
        case RendererBackend::None:     return "None";
        case RendererBackend::OpenGL:   return "OpenGL";
        case RendererBackend::Vulkan:   return "Vulkan";
        case RendererBackend::DX12:     return "DirectX 12";
        case RendererBackend::Metal:    return "Metal";
        case RendererBackend::WebGPU:   return "WebGPU";
        case RendererBackend::Software: return "Software";
        default:                        return "Unknown";
    }
}

// ============================================================================
// Renderer Capabilities
// ============================================================================

/**
 * @brief Feature flags indicating renderer capabilities
 *
 * These flags describe what features a renderer implementation supports.
 * Query capabilities before using advanced features to ensure compatibility.
 */
enum class RendererFeature : uint32_t {
    None                    = 0,

    // Core rendering features
    PolygonRendering        = 1 << 0,   ///< Traditional polygon rasterization
    SDFRendering            = 1 << 1,   ///< SDF raymarching
    HybridRendering         = 1 << 2,   ///< Combined SDF + polygon

    // Compute and raytracing
    ComputeShaders          = 1 << 3,   ///< General compute shader support
    HardwareRaytracing      = 1 << 4,   ///< RTX/DXR hardware raytracing
    AsyncCompute            = 1 << 5,   ///< Async compute queue support

    // Lighting and shading
    PBRShading              = 1 << 6,   ///< Physically Based Rendering
    GlobalIllumination      = 1 << 7,   ///< Real-time GI (radiance cascades, etc.)
    VolumetricLighting      = 1 << 8,   ///< Volumetric fog and lighting
    ClusteredLighting       = 1 << 9,   ///< Clustered forward+ lighting

    // Shadows
    ShadowMapping           = 1 << 10,  ///< Basic shadow maps
    CascadedShadows         = 1 << 11,  ///< Cascaded shadow maps
    RaytracedShadows        = 1 << 12,  ///< Hardware raytraced shadows

    // Post-processing
    TemporalAA              = 1 << 13,  ///< Temporal anti-aliasing
    MSAA                    = 1 << 14,  ///< Multi-sample anti-aliasing
    SSAO                    = 1 << 15,  ///< Screen-space ambient occlusion
    MotionBlur              = 1 << 16,  ///< Per-object motion blur
    DepthOfField            = 1 << 17,  ///< Depth of field effect
    Bloom                   = 1 << 18,  ///< HDR bloom

    // Advanced features
    Tessellation            = 1 << 19,  ///< Hardware tessellation
    GeometryShaders         = 1 << 20,  ///< Geometry shader support
    MeshShaders             = 1 << 21,  ///< Mesh shader support (Vulkan/DX12)
    BindlessTextures        = 1 << 22,  ///< Bindless texture support
    IndirectDrawing         = 1 << 23,  ///< GPU-driven indirect draws

    // Multi-view and VR
    MultiView               = 1 << 24,  ///< Multi-view rendering (VR)
    VariableRateShading     = 1 << 25,  ///< Variable rate shading (VRS)

    // Memory and performance
    SparseTextures          = 1 << 26,  ///< Virtual/sparse texture support
    MultiDrawIndirect       = 1 << 27,  ///< Multi-draw indirect batching

    // All features (for testing)
    All                     = 0xFFFFFFFF
};

// Bitwise operators for RendererFeature
inline constexpr RendererFeature operator|(RendererFeature a, RendererFeature b) noexcept {
    return static_cast<RendererFeature>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr RendererFeature operator&(RendererFeature a, RendererFeature b) noexcept {
    return static_cast<RendererFeature>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline constexpr RendererFeature operator^(RendererFeature a, RendererFeature b) noexcept {
    return static_cast<RendererFeature>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}

inline constexpr RendererFeature operator~(RendererFeature a) noexcept {
    return static_cast<RendererFeature>(~static_cast<uint32_t>(a));
}

inline constexpr RendererFeature& operator|=(RendererFeature& a, RendererFeature b) noexcept {
    return a = a | b;
}

inline constexpr RendererFeature& operator&=(RendererFeature& a, RendererFeature b) noexcept {
    return a = a & b;
}

/**
 * @brief Check if a feature set contains a specific feature
 * @param features The feature set to check
 * @param feature The feature to look for
 * @return true if the feature is present
 */
inline constexpr bool HasFeature(RendererFeature features, RendererFeature feature) noexcept {
    return (features & feature) == feature;
}

/**
 * @brief Comprehensive renderer capabilities structure
 *
 * Contains detailed information about what a renderer implementation
 * supports, including limits, formats, and feature flags.
 */
struct RendererCapabilities {
    // Feature flags
    RendererFeature supportedFeatures = RendererFeature::None;

    // Texture limits
    int32_t maxTextureSize = 4096;              ///< Maximum 2D texture dimension
    int32_t maxCubemapSize = 2048;              ///< Maximum cubemap face dimension
    int32_t max3DTextureSize = 512;             ///< Maximum 3D texture dimension
    int32_t maxArrayLayers = 256;               ///< Maximum texture array layers
    int32_t maxTextureUnits = 16;               ///< Maximum bound texture units
    int32_t maxColorAttachments = 8;            ///< Maximum framebuffer color attachments

    // Shader limits
    int32_t maxUniformBufferSize = 65536;       ///< Maximum uniform buffer size (bytes)
    int32_t maxShaderStorageSize = 134217728;   ///< Maximum SSBO size (bytes)
    int32_t maxComputeWorkGroupSize[3] = {1024, 1024, 64};
    int32_t maxComputeWorkGroupCount[3] = {65535, 65535, 65535};
    int32_t maxComputeSharedMemory = 49152;     ///< Shared memory per workgroup (bytes)

    // Vertex/mesh limits
    int32_t maxVertexAttributes = 16;           ///< Maximum vertex attributes
    int32_t maxVertexStreams = 4;               ///< Maximum vertex buffer bindings
    int32_t maxDrawIndirectCount = 1048576;     ///< Maximum indirect draw count

    // SDF-specific limits
    int32_t maxSDFPrimitives = 256;             ///< Maximum SDF primitives per scene
    int32_t maxRaymarchSteps = 256;             ///< Maximum raymarch iterations
    float minRaymarchEpsilon = 0.0001f;         ///< Minimum hit threshold

    // Memory info
    uint64_t dedicatedVideoMemory = 0;          ///< Dedicated VRAM (bytes)
    uint64_t totalVideoMemory = 0;              ///< Total accessible VRAM (bytes)

    // API version
    int32_t apiVersionMajor = 0;                ///< Major API version
    int32_t apiVersionMinor = 0;                ///< Minor API version

    // Device info
    std::string vendorName;                     ///< GPU vendor name
    std::string deviceName;                     ///< GPU device name
    std::string driverVersion;                  ///< Driver version string

    /**
     * @brief Check if a specific feature is supported
     * @param feature The feature to check
     * @return true if the feature is supported
     */
    bool Supports(RendererFeature feature) const noexcept {
        return HasFeature(supportedFeatures, feature);
    }

    /**
     * @brief Check if all specified features are supported
     * @param features The features to check
     * @return true if all features are supported
     */
    bool SupportsAll(RendererFeature features) const noexcept {
        return (supportedFeatures & features) == features;
    }

    /**
     * @brief Check if any of the specified features are supported
     * @param features The features to check
     * @return true if any feature is supported
     */
    bool SupportsAny(RendererFeature features) const noexcept {
        return (supportedFeatures & features) != RendererFeature::None;
    }
};

// ============================================================================
// Renderer Configuration
// ============================================================================

/**
 * @brief MSAA sample count options
 */
enum class MSAASamples : uint8_t {
    None = 0,
    X2 = 2,
    X4 = 4,
    X8 = 8,
    X16 = 16
};

/**
 * @brief VSync modes
 */
enum class VSyncMode : uint8_t {
    Off = 0,            ///< No VSync (unlimited FPS)
    On = 1,             ///< Standard VSync (cap to refresh rate)
    Adaptive = 2,       ///< Adaptive VSync (tear if below refresh rate)
    FastSync = 3        ///< Triple buffering with no tearing
};

/**
 * @brief HDR display modes
 */
enum class HDRMode : uint8_t {
    Off = 0,            ///< SDR output
    HDR10 = 1,          ///< HDR10 (PQ transfer function)
    Dolby = 2,          ///< Dolby Vision
    HLG = 3             ///< Hybrid Log-Gamma
};

/**
 * @brief Renderer configuration for initialization
 *
 * Specifies all parameters needed to initialize a renderer instance.
 * Some settings may be ignored if not supported by the backend.
 */
struct RendererConfig {
    // Window/surface settings
    int32_t width = 1920;                       ///< Initial render width
    int32_t height = 1080;                      ///< Initial render height
    bool fullscreen = false;                    ///< Start in fullscreen mode
    bool borderless = false;                    ///< Borderless window mode

    // Quality settings
    MSAASamples msaaSamples = MSAASamples::None; ///< MSAA sample count
    bool enableHDR = false;                      ///< Enable HDR rendering
    HDRMode hdrMode = HDRMode::Off;              ///< HDR output mode
    float maxLuminance = 1000.0f;                ///< Max display luminance (nits)

    // Synchronization
    VSyncMode vsync = VSyncMode::On;             ///< VSync mode
    int32_t maxFramesInFlight = 2;               ///< Max frames queued

    // Backend preferences
    RendererBackend preferredBackend = RendererBackend::OpenGL;
    bool allowFallback = true;                   ///< Allow fallback to other backends

    // Debug options
    bool enableDebugLayer = false;               ///< Enable graphics API debug layer
    bool enableGPUValidation = false;            ///< Enable GPU-based validation
    bool enableTimestamps = true;                ///< Enable GPU timestamp queries

    // SDF-specific settings
    int32_t sdfMaxSteps = 128;                   ///< Max raymarch steps
    float sdfHitThreshold = 0.001f;              ///< Raymarch hit threshold
    float sdfMaxDistance = 100.0f;               ///< Max raymarch distance
    bool sdfEnableGI = true;                     ///< Enable SDF global illumination

    // Memory settings
    uint64_t stagingBufferSize = 64 * 1024 * 1024;  ///< Staging buffer size (bytes)
    uint64_t uniformBufferPoolSize = 16 * 1024 * 1024; ///< UBO pool size (bytes)

    // Threading
    int32_t workerThreads = 0;                   ///< Worker threads (0 = auto)
    bool enableAsyncCompute = true;              ///< Use async compute queue

    // Shader settings
    std::string shaderDirectory = "shaders/";   ///< Shader search path
    bool enableShaderHotReload = false;          ///< Enable runtime shader reload

    /**
     * @brief Create a default configuration
     */
    static RendererConfig Default() {
        return RendererConfig{};
    }

    /**
     * @brief Create a minimal configuration for testing
     */
    static RendererConfig Minimal() {
        RendererConfig config;
        config.width = 800;
        config.height = 600;
        config.msaaSamples = MSAASamples::None;
        config.vsync = VSyncMode::Off;
        config.enableDebugLayer = true;
        return config;
    }

    /**
     * @brief Create a high-quality configuration
     */
    static RendererConfig HighQuality() {
        RendererConfig config;
        config.width = 2560;
        config.height = 1440;
        config.msaaSamples = MSAASamples::X4;
        config.enableHDR = true;
        config.hdrMode = HDRMode::HDR10;
        config.sdfMaxSteps = 256;
        config.sdfHitThreshold = 0.0001f;
        return config;
    }
};

// ============================================================================
// Viewport
// ============================================================================

/**
 * @brief Viewport definition for rendering
 *
 * Defines the rectangular region of the render target where
 * rendering operations will be performed.
 */
struct Viewport {
    float x = 0.0f;                 ///< Left edge (pixels)
    float y = 0.0f;                 ///< Bottom edge (pixels)
    float width = 0.0f;             ///< Width (pixels)
    float height = 0.0f;            ///< Height (pixels)
    float minDepth = 0.0f;          ///< Minimum depth value (0-1)
    float maxDepth = 1.0f;          ///< Maximum depth value (0-1)

    /**
     * @brief Create viewport from dimensions
     */
    static Viewport FromDimensions(float w, float h) {
        return Viewport{0.0f, 0.0f, w, h, 0.0f, 1.0f};
    }

    /**
     * @brief Create viewport from rectangle
     */
    static Viewport FromRect(float x, float y, float w, float h) {
        return Viewport{x, y, w, h, 0.0f, 1.0f};
    }

    /**
     * @brief Get aspect ratio
     */
    float GetAspectRatio() const noexcept {
        return (height > 0.0f) ? (width / height) : 1.0f;
    }

    /**
     * @brief Check if point is inside viewport
     */
    bool Contains(float px, float py) const noexcept {
        return px >= x && px < (x + width) && py >= y && py < (y + height);
    }

    bool operator==(const Viewport& other) const noexcept {
        return x == other.x && y == other.y &&
               width == other.width && height == other.height &&
               minDepth == other.minDepth && maxDepth == other.maxDepth;
    }

    bool operator!=(const Viewport& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Scissor rectangle for clipping
 */
struct ScissorRect {
    int32_t x = 0;                  ///< Left edge (pixels)
    int32_t y = 0;                  ///< Bottom edge (pixels)
    int32_t width = 0;              ///< Width (pixels)
    int32_t height = 0;             ///< Height (pixels)

    bool operator==(const ScissorRect& other) const noexcept {
        return x == other.x && y == other.y &&
               width == other.width && height == other.height;
    }
};

// ============================================================================
// Render Target
// ============================================================================

/**
 * @brief Render target abstraction
 *
 * Represents a surface that can be rendered to, either the default
 * framebuffer (screen) or an off-screen framebuffer.
 */
struct RenderTarget {
    Framebuffer* framebuffer = nullptr;  ///< Framebuffer (nullptr = default)
    int32_t colorAttachment = 0;         ///< Which color attachment to use
    int32_t mipLevel = 0;                ///< Mip level to render to
    int32_t arrayLayer = 0;              ///< Array layer to render to

    /**
     * @brief Create default render target (screen)
     */
    static RenderTarget Default() {
        return RenderTarget{};
    }

    /**
     * @brief Create render target from framebuffer
     */
    static RenderTarget FromFramebuffer(Framebuffer* fb, int32_t attachment = 0) {
        RenderTarget rt;
        rt.framebuffer = fb;
        rt.colorAttachment = attachment;
        return rt;
    }

    /**
     * @brief Check if this is the default render target
     */
    bool IsDefault() const noexcept {
        return framebuffer == nullptr;
    }
};

// ============================================================================
// Resource Descriptors
// ============================================================================

/**
 * @brief Texture type enumeration
 */
enum class TextureType : uint8_t {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture2DArray,
    TextureCubeArray
};

/**
 * @brief Texture usage flags
 */
enum class TextureUsage : uint8_t {
    None            = 0,
    Sampled         = 1 << 0,   ///< Can be sampled in shaders
    Storage         = 1 << 1,   ///< Can be used as storage image
    RenderTarget    = 1 << 2,   ///< Can be used as render target
    DepthStencil    = 1 << 3,   ///< Can be used as depth/stencil
    TransferSrc     = 1 << 4,   ///< Can be copy source
    TransferDst     = 1 << 5    ///< Can be copy destination
};

inline constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) noexcept {
    return static_cast<TextureUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr TextureUsage operator&(TextureUsage a, TextureUsage b) noexcept {
    return static_cast<TextureUsage>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

/**
 * @brief Texture format enumeration (expanded)
 */
enum class TextureFormatEx : uint8_t {
    Unknown = 0,

    // 8-bit formats
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,

    // 16-bit formats
    R16_FLOAT,
    R16_UNORM,
    R16_UINT,
    R16_SINT,
    RG8_UNORM,
    RG8_SNORM,

    // 32-bit formats
    R32_FLOAT,
    R32_UINT,
    R32_SINT,
    RG16_FLOAT,
    RG16_UNORM,
    RGBA8_UNORM,
    RGBA8_UNORM_SRGB,
    RGBA8_SNORM,
    BGRA8_UNORM,
    BGRA8_UNORM_SRGB,
    RGB10A2_UNORM,

    // 64-bit formats
    RG32_FLOAT,
    RG32_UINT,
    RGBA16_FLOAT,
    RGBA16_UNORM,

    // 128-bit formats
    RGBA32_FLOAT,
    RGBA32_UINT,

    // Depth/stencil formats
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,

    // Compressed formats
    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UF16,
    BC6H_SF16,
    BC7_UNORM,
    BC7_UNORM_SRGB
};

/**
 * @brief Texture descriptor for creation
 */
struct TextureDesc {
    TextureType type = TextureType::Texture2D;
    TextureFormatEx format = TextureFormatEx::RGBA8_UNORM;
    TextureUsage usage = TextureUsage::Sampled;

    int32_t width = 1;
    int32_t height = 1;
    int32_t depth = 1;                  ///< Depth for 3D, array layers for arrays
    int32_t mipLevels = 1;              ///< Number of mip levels (0 = full chain)
    int32_t sampleCount = 1;            ///< MSAA sample count

    const void* initialData = nullptr;  ///< Optional initial data
    size_t initialDataSize = 0;         ///< Size of initial data

    std::string debugName;              ///< Debug name for profiling

    /**
     * @brief Create a 2D texture descriptor
     */
    static TextureDesc Texture2D(int32_t w, int32_t h,
                                  TextureFormatEx fmt = TextureFormatEx::RGBA8_UNORM) {
        TextureDesc desc;
        desc.type = TextureType::Texture2D;
        desc.width = w;
        desc.height = h;
        desc.format = fmt;
        return desc;
    }

    /**
     * @brief Create a render target descriptor
     */
    static TextureDesc RenderTarget2D(int32_t w, int32_t h,
                                       TextureFormatEx fmt = TextureFormatEx::RGBA16_FLOAT) {
        TextureDesc desc = Texture2D(w, h, fmt);
        desc.usage = TextureUsage::Sampled | TextureUsage::RenderTarget;
        return desc;
    }

    /**
     * @brief Create a depth texture descriptor
     */
    static TextureDesc DepthTexture(int32_t w, int32_t h,
                                     TextureFormatEx fmt = TextureFormatEx::D24_UNORM_S8_UINT) {
        TextureDesc desc = Texture2D(w, h, fmt);
        desc.usage = TextureUsage::DepthStencil | TextureUsage::Sampled;
        return desc;
    }
};

/**
 * @brief Buffer type enumeration
 */
enum class BufferType : uint8_t {
    Vertex,             ///< Vertex buffer
    Index,              ///< Index buffer
    Uniform,            ///< Uniform/constant buffer
    Storage,            ///< Shader storage buffer (SSBO)
    Indirect,           ///< Indirect draw buffer
    Staging             ///< CPU-accessible staging buffer
};

/**
 * @brief Buffer usage flags
 */
enum class BufferUsage : uint8_t {
    None        = 0,
    MapRead     = 1 << 0,   ///< Can be mapped for reading
    MapWrite    = 1 << 1,   ///< Can be mapped for writing
    CopySrc     = 1 << 2,   ///< Can be copy source
    CopyDst     = 1 << 3    ///< Can be copy destination
};

inline constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) noexcept {
    return static_cast<BufferUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/**
 * @brief Buffer memory location hint
 */
enum class MemoryLocation : uint8_t {
    Device,             ///< GPU-only memory (fastest)
    Host,               ///< CPU-accessible memory
    Shared              ///< Shared memory (unified memory architectures)
};

/**
 * @brief Buffer descriptor for creation
 */
struct BufferDesc {
    BufferType type = BufferType::Vertex;
    BufferUsage usage = BufferUsage::None;
    MemoryLocation location = MemoryLocation::Device;

    size_t size = 0;                    ///< Buffer size in bytes
    size_t stride = 0;                  ///< Element stride (for structured buffers)

    const void* initialData = nullptr;  ///< Optional initial data

    std::string debugName;              ///< Debug name for profiling

    /**
     * @brief Create a vertex buffer descriptor
     */
    static BufferDesc VertexBuffer(size_t size, const void* data = nullptr) {
        BufferDesc desc;
        desc.type = BufferType::Vertex;
        desc.size = size;
        desc.initialData = data;
        return desc;
    }

    /**
     * @brief Create an index buffer descriptor
     */
    static BufferDesc IndexBuffer(size_t size, const void* data = nullptr) {
        BufferDesc desc;
        desc.type = BufferType::Index;
        desc.size = size;
        desc.initialData = data;
        return desc;
    }

    /**
     * @brief Create a uniform buffer descriptor
     */
    static BufferDesc UniformBuffer(size_t size) {
        BufferDesc desc;
        desc.type = BufferType::Uniform;
        desc.size = size;
        desc.usage = BufferUsage::CopyDst;
        return desc;
    }

    /**
     * @brief Create a storage buffer descriptor
     */
    static BufferDesc StorageBuffer(size_t size, size_t elementStride = 0) {
        BufferDesc desc;
        desc.type = BufferType::Storage;
        desc.size = size;
        desc.stride = elementStride;
        return desc;
    }
};

/**
 * @brief Shader stage enumeration
 */
enum class ShaderStage : uint8_t {
    Vertex      = 1 << 0,
    Fragment    = 1 << 1,
    Geometry    = 1 << 2,
    TessControl = 1 << 3,
    TessEval    = 1 << 4,
    Compute     = 1 << 5,
    Mesh        = 1 << 6,
    Task        = 1 << 7
};

inline constexpr ShaderStage operator|(ShaderStage a, ShaderStage b) noexcept {
    return static_cast<ShaderStage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/**
 * @brief Shader source type
 */
enum class ShaderSourceType : uint8_t {
    GLSL,               ///< GLSL source code
    HLSL,               ///< HLSL source code
    SPIRV,              ///< Pre-compiled SPIR-V binary
    MSL,                ///< Metal Shading Language
    WGSL                ///< WebGPU Shading Language
};

/**
 * @brief Shader descriptor for creation
 */
struct ShaderDesc {
    ShaderStage stages = ShaderStage::Vertex | ShaderStage::Fragment;
    ShaderSourceType sourceType = ShaderSourceType::GLSL;

    // Source code (for source-based shaders)
    std::string vertexSource;
    std::string fragmentSource;
    std::string geometrySource;
    std::string computeSource;

    // File paths (for file-based shaders)
    std::string vertexPath;
    std::string fragmentPath;
    std::string geometryPath;
    std::string computePath;

    // Pre-compiled binary (for SPIR-V)
    const uint32_t* spirvData = nullptr;
    size_t spirvSize = 0;

    // Entry points (for HLSL/MSL)
    std::string vertexEntry = "main";
    std::string fragmentEntry = "main";
    std::string computeEntry = "main";

    std::string debugName;              ///< Debug name for profiling

    /**
     * @brief Create from file paths
     */
    static ShaderDesc FromFiles(const std::string& vertPath,
                                 const std::string& fragPath,
                                 const std::string& geomPath = "") {
        ShaderDesc desc;
        desc.vertexPath = vertPath;
        desc.fragmentPath = fragPath;
        desc.geometryPath = geomPath;
        if (!geomPath.empty()) {
            desc.stages = desc.stages | ShaderStage::Geometry;
        }
        return desc;
    }

    /**
     * @brief Create compute shader from file
     */
    static ShaderDesc ComputeFromFile(const std::string& computePath) {
        ShaderDesc desc;
        desc.stages = ShaderStage::Compute;
        desc.computePath = computePath;
        return desc;
    }
};

// ============================================================================
// GPU Buffer Handle
// ============================================================================

/**
 * @brief Abstract GPU buffer interface
 *
 * Represents a buffer allocated on the GPU. The actual implementation
 * is backend-specific.
 */
class IBuffer {
public:
    virtual ~IBuffer() = default;

    /**
     * @brief Get buffer size in bytes
     */
    virtual size_t GetSize() const = 0;

    /**
     * @brief Get buffer type
     */
    virtual BufferType GetType() const = 0;

    /**
     * @brief Map buffer for CPU access
     * @return Pointer to mapped memory, or nullptr on failure
     */
    virtual void* Map() = 0;

    /**
     * @brief Unmap buffer
     */
    virtual void Unmap() = 0;

    /**
     * @brief Update buffer data
     * @param data Source data
     * @param size Size in bytes
     * @param offset Offset into buffer
     */
    virtual void Update(const void* data, size_t size, size_t offset = 0) = 0;

    /**
     * @brief Check if buffer is valid
     */
    virtual bool IsValid() const = 0;

    /**
     * @brief Get native handle (backend-specific)
     */
    virtual void* GetNativeHandle() const = 0;
};

// ============================================================================
// Render Commands
// ============================================================================

/**
 * @brief Render command type enumeration
 */
enum class RenderCommandType : uint8_t {
    // State commands
    SetViewport,
    SetScissor,
    SetBlendState,
    SetDepthState,
    SetRasterState,
    SetStencilState,

    // Resource binding
    BindShader,
    BindVertexBuffer,
    BindIndexBuffer,
    BindUniformBuffer,
    BindTexture,
    BindSampler,
    BindRenderTarget,

    // Draw commands
    Draw,
    DrawIndexed,
    DrawInstanced,
    DrawIndexedInstanced,
    DrawIndirect,
    DrawIndexedIndirect,

    // Compute commands
    Dispatch,
    DispatchIndirect,

    // Transfer commands
    CopyBuffer,
    CopyTexture,
    UpdateBuffer,

    // Synchronization
    Barrier,

    // Debug
    BeginDebugGroup,
    EndDebugGroup,
    InsertDebugMarker,

    // Custom
    Custom
};

/**
 * @brief Primitive topology for drawing
 */
enum class PrimitiveTopology : uint8_t {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    LineListAdjacency,
    LineStripAdjacency,
    TriangleListAdjacency,
    TriangleStripAdjacency,
    PatchList
};

/**
 * @brief Index type for indexed drawing
 */
enum class IndexType : uint8_t {
    UInt16,
    UInt32
};

/**
 * @brief Blend factor enumeration
 */
enum class BlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    SrcAlphaSaturate
};

/**
 * @brief Blend operation enumeration
 */
enum class BlendOp : uint8_t {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

/**
 * @brief Blend state for a single render target
 */
struct BlendState {
    bool enabled = false;
    BlendFactor srcColorFactor = BlendFactor::SrcAlpha;
    BlendFactor dstColorFactor = BlendFactor::OneMinusSrcAlpha;
    BlendOp colorOp = BlendOp::Add;
    BlendFactor srcAlphaFactor = BlendFactor::One;
    BlendFactor dstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
    BlendOp alphaOp = BlendOp::Add;
    uint8_t colorWriteMask = 0x0F;  ///< RGBA write mask

    static BlendState Opaque() { return BlendState{}; }

    static BlendState AlphaBlend() {
        BlendState state;
        state.enabled = true;
        return state;
    }

    static BlendState Additive() {
        BlendState state;
        state.enabled = true;
        state.srcColorFactor = BlendFactor::SrcAlpha;
        state.dstColorFactor = BlendFactor::One;
        return state;
    }

    static BlendState Multiply() {
        BlendState state;
        state.enabled = true;
        state.srcColorFactor = BlendFactor::DstColor;
        state.dstColorFactor = BlendFactor::Zero;
        return state;
    }
};

/**
 * @brief Compare function for depth/stencil
 */
enum class CompareFunc : uint8_t {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

/**
 * @brief Depth state configuration
 */
struct DepthState {
    bool testEnabled = true;
    bool writeEnabled = true;
    CompareFunc compareFunc = CompareFunc::Less;
    bool boundsTestEnabled = false;
    float minBounds = 0.0f;
    float maxBounds = 1.0f;

    static DepthState Default() { return DepthState{}; }

    static DepthState ReadOnly() {
        DepthState state;
        state.writeEnabled = false;
        return state;
    }

    static DepthState Disabled() {
        DepthState state;
        state.testEnabled = false;
        state.writeEnabled = false;
        return state;
    }
};

/**
 * @brief Stencil operation enumeration
 */
enum class StencilOp : uint8_t {
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap
};

/**
 * @brief Stencil state for one face
 */
struct StencilFaceState {
    StencilOp failOp = StencilOp::Keep;
    StencilOp depthFailOp = StencilOp::Keep;
    StencilOp passOp = StencilOp::Keep;
    CompareFunc compareFunc = CompareFunc::Always;
    uint8_t readMask = 0xFF;
    uint8_t writeMask = 0xFF;
    uint8_t reference = 0;
};

/**
 * @brief Full stencil state configuration
 */
struct StencilState {
    bool enabled = false;
    StencilFaceState front;
    StencilFaceState back;

    static StencilState Disabled() { return StencilState{}; }
};

/**
 * @brief Cull mode enumeration
 */
enum class CullMode : uint8_t {
    None,
    Front,
    Back,
    FrontAndBack
};

/**
 * @brief Front face winding order
 */
enum class FrontFace : uint8_t {
    CounterClockwise,
    Clockwise
};

/**
 * @brief Polygon fill mode
 */
enum class PolygonMode : uint8_t {
    Fill,
    Line,
    Point
};

/**
 * @brief Rasterizer state configuration
 */
struct RasterState {
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
    PolygonMode polygonMode = PolygonMode::Fill;
    bool depthClampEnabled = false;
    bool depthBiasEnabled = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    float depthBiasClamp = 0.0f;
    float lineWidth = 1.0f;

    static RasterState Default() { return RasterState{}; }

    static RasterState NoCull() {
        RasterState state;
        state.cullMode = CullMode::None;
        return state;
    }

    static RasterState Wireframe() {
        RasterState state;
        state.polygonMode = PolygonMode::Line;
        return state;
    }
};

/**
 * @brief Clear flags for clearing render targets
 */
enum class ClearFlags : uint8_t {
    None    = 0,
    Color   = 1 << 0,
    Depth   = 1 << 1,
    Stencil = 1 << 2,
    All     = Color | Depth | Stencil
};

inline constexpr ClearFlags operator|(ClearFlags a, ClearFlags b) noexcept {
    return static_cast<ClearFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr ClearFlags operator&(ClearFlags a, ClearFlags b) noexcept {
    return static_cast<ClearFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

/**
 * @brief Clear values for render target clearing
 */
struct ClearValue {
    glm::vec4 color{0.0f, 0.0f, 0.0f, 1.0f};
    float depth = 1.0f;
    uint8_t stencil = 0;
};

/**
 * @brief Render command structure
 *
 * Encapsulates all information needed to execute a single render operation.
 * Commands can be recorded and executed later for deferred rendering.
 */
struct RenderCommand {
    RenderCommandType type = RenderCommandType::Draw;

    // Command data (usage depends on type)
    union {
        // Draw parameters
        struct {
            uint32_t vertexCount;
            uint32_t instanceCount;
            uint32_t firstVertex;
            uint32_t firstInstance;
        } draw;

        // Indexed draw parameters
        struct {
            uint32_t indexCount;
            uint32_t instanceCount;
            uint32_t firstIndex;
            int32_t vertexOffset;
            uint32_t firstInstance;
        } drawIndexed;

        // Compute dispatch parameters
        struct {
            uint32_t groupCountX;
            uint32_t groupCountY;
            uint32_t groupCountZ;
        } dispatch;

        // Copy parameters
        struct {
            size_t srcOffset;
            size_t dstOffset;
            size_t size;
        } copy;
    };

    // Resource references (for binding commands)
    Shader* shader = nullptr;
    IBuffer* buffer = nullptr;
    Texture* texture = nullptr;

    // State data
    Viewport viewport;
    ScissorRect scissor;
    BlendState blendState;
    DepthState depthState;
    RasterState rasterState;

    // Additional parameters
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    IndexType indexType = IndexType::UInt32;
    uint32_t bindSlot = 0;

    // Transform data
    glm::mat4 transform{1.0f};

    // Sort key for command sorting
    uint64_t sortKey = 0;

    // Custom data pointer
    void* userData = nullptr;

    /**
     * @brief Create a draw command
     */
    static RenderCommand Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
                               uint32_t firstVertex = 0, uint32_t firstInstance = 0) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Draw;
        cmd.draw.vertexCount = vertexCount;
        cmd.draw.instanceCount = instanceCount;
        cmd.draw.firstVertex = firstVertex;
        cmd.draw.firstInstance = firstInstance;
        return cmd;
    }

    /**
     * @brief Create an indexed draw command
     */
    static RenderCommand DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                                      uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                                      uint32_t firstInstance = 0) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::DrawIndexed;
        cmd.drawIndexed.indexCount = indexCount;
        cmd.drawIndexed.instanceCount = instanceCount;
        cmd.drawIndexed.firstIndex = firstIndex;
        cmd.drawIndexed.vertexOffset = vertexOffset;
        cmd.drawIndexed.firstInstance = firstInstance;
        return cmd;
    }

    /**
     * @brief Create a compute dispatch command
     */
    static RenderCommand Dispatch(uint32_t groupX, uint32_t groupY = 1, uint32_t groupZ = 1) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::Dispatch;
        cmd.dispatch.groupCountX = groupX;
        cmd.dispatch.groupCountY = groupY;
        cmd.dispatch.groupCountZ = groupZ;
        return cmd;
    }

    /**
     * @brief Create a bind shader command
     */
    static RenderCommand BindShaderCmd(Shader* shaderPtr) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::BindShader;
        cmd.shader = shaderPtr;
        return cmd;
    }

    /**
     * @brief Create a set viewport command
     */
    static RenderCommand SetViewportCmd(const Viewport& vp) {
        RenderCommand cmd;
        cmd.type = RenderCommandType::SetViewport;
        cmd.viewport = vp;
        return cmd;
    }
};

// ============================================================================
// IRenderer Interface
// ============================================================================

/**
 * @brief Pure virtual interface for all renderers
 *
 * IRenderer defines the contract that all renderer implementations must fulfill.
 * It provides a unified API for both traditional polygon rasterization and
 * SDF raymarching, enabling seamless switching between rendering approaches.
 *
 * Key Design Principles:
 * - Stateless command submission: State is encapsulated in commands
 * - Deferred execution: Commands can be recorded and executed later
 * - Resource abstraction: Textures, buffers, shaders are backend-agnostic
 * - Capability-driven: Query capabilities before using features
 *
 * Implementation Guidelines:
 * - Implementations should batch similar commands for efficiency
 * - State changes should be minimized through sorting
 * - Resources should be validated before use
 * - All methods should be thread-safe unless documented otherwise
 *
 * Usage Example:
 * @code
 *     auto renderer = CreateRenderer(RendererBackend::OpenGL);
 *
 *     RendererConfig config = RendererConfig::Default();
 *     if (!renderer->Initialize(config)) {
 *         // Handle error
 *     }
 *
 *     // Create resources
 *     auto texture = renderer->CreateTexture(TextureDesc::Texture2D(512, 512));
 *     auto shader = renderer->CreateShader(ShaderDesc::FromFiles("vs.glsl", "fs.glsl"));
 *
 *     // Main loop
 *     while (running) {
 *         renderer->BeginFrame();
 *         renderer->SetCamera(camera);
 *         renderer->SetViewport(Viewport::FromDimensions(1920, 1080));
 *
 *         renderer->Submit(RenderCommand::Draw(36));
 *         renderer->Flush();
 *
 *         renderer->EndFrame();
 *         renderer->Present();
 *     }
 *
 *     renderer->Shutdown();
 * @endcode
 *
 * Thread Safety:
 * - Initialize/Shutdown must be called from the main thread
 * - BeginFrame/EndFrame/Present must be called from the main thread
 * - Resource creation is thread-safe
 * - Command submission may be thread-safe (implementation-dependent)
 *
 * @see Renderer For the OpenGL implementation
 * @see SDFRenderer For SDF-specific features
 * @see IRenderBackend For the scene-level backend interface
 */
class IRenderer {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~IRenderer() = default;

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    /**
     * @brief Initialize the renderer with configuration
     *
     * Sets up all internal resources, creates the graphics context,
     * and prepares the renderer for use. Must be called before any
     * other operations.
     *
     * @param config Configuration parameters for initialization
     * @return true on success, false on failure
     *
     * @pre No other renderer operations have been performed
     * @post If successful, the renderer is ready for use
     *
     * @note This method is NOT thread-safe and must be called from
     *       the thread that will own the graphics context.
     *
     * @see Shutdown()
     * @see IsInitialized()
     */
    virtual bool Initialize(const RendererConfig& config) = 0;

    /**
     * @brief Shutdown the renderer and release all resources
     *
     * Cleans up all internal resources, destroys the graphics context,
     * and resets the renderer to an uninitialized state. Safe to call
     * multiple times.
     *
     * @post The renderer is in an uninitialized state
     * @post All resources created by this renderer are invalid
     *
     * @warning Calling this method invalidates all resources created
     *          by this renderer. Ensure no resources are in use.
     *
     * @see Initialize()
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Check if the renderer is initialized
     *
     * @return true if Initialize() succeeded and Shutdown() has not been called
     */
    virtual bool IsInitialized() const = 0;

    // ========================================================================
    // Frame Management
    // ========================================================================

    /**
     * @brief Begin a new rendering frame
     *
     * Prepares the renderer for a new frame of rendering. This must be
     * called before any rendering operations and must be paired with
     * EndFrame().
     *
     * Operations performed:
     * - Resets per-frame statistics
     * - Acquires the next swapchain image (if applicable)
     * - Begins command buffer recording
     * - Sets up default render state
     *
     * @pre Initialize() has succeeded
     * @pre Not currently in a frame (EndFrame() was called)
     * @post Ready to accept rendering commands
     *
     * @see EndFrame()
     * @see Present()
     */
    virtual void BeginFrame() = 0;

    /**
     * @brief End the current rendering frame
     *
     * Finalizes all rendering operations for the current frame.
     * After this call, no more rendering commands can be submitted
     * until BeginFrame() is called again.
     *
     * Operations performed:
     * - Flushes any pending commands
     * - Ends command buffer recording
     * - Computes frame statistics
     *
     * @pre Currently in a frame (BeginFrame() was called)
     * @post Frame is finalized and ready for presentation
     *
     * @note This does NOT present the frame to the screen.
     *       Call Present() after EndFrame() to display the result.
     *
     * @see BeginFrame()
     * @see Present()
     */
    virtual void EndFrame() = 0;

    /**
     * @brief Present the rendered frame to the display
     *
     * Submits the completed frame for display on the screen.
     * May block if VSync is enabled or the GPU is behind.
     *
     * @pre EndFrame() has been called
     * @post The frame is queued for display
     *
     * @see EndFrame()
     * @see RendererConfig::vsync
     */
    virtual void Present() = 0;

    // ========================================================================
    // Command Submission
    // ========================================================================

    /**
     * @brief Submit a render command for execution
     *
     * Commands are typically queued and executed during Flush().
     * The submission order may not match execution order due to
     * command sorting for optimal performance.
     *
     * @param cmd The render command to submit
     *
     * @pre Currently in a frame (between BeginFrame/EndFrame)
     *
     * @note Commands may be sorted by the renderer for efficiency.
     *       Use sort keys to control ordering when necessary.
     *
     * @see Flush()
     * @see RenderCommand
     */
    virtual void Submit(const RenderCommand& cmd) = 0;

    /**
     * @brief Submit multiple render commands
     *
     * Batch submission for improved efficiency when submitting
     * many commands at once.
     *
     * @param commands Vector of commands to submit
     *
     * @see Submit(const RenderCommand&)
     */
    virtual void SubmitBatch(const std::vector<RenderCommand>& commands) {
        for (const auto& cmd : commands) {
            Submit(cmd);
        }
    }

    /**
     * @brief Flush all pending commands for execution
     *
     * Executes all queued commands, potentially reordering them
     * for optimal GPU utilization. After this call, all submitted
     * commands have been dispatched to the GPU.
     *
     * @pre Currently in a frame
     * @post All pending commands have been dispatched
     *
     * @note This does not guarantee GPU completion. Use synchronization
     *       primitives for that purpose.
     *
     * @see Submit()
     */
    virtual void Flush() = 0;

    // ========================================================================
    // State Management
    // ========================================================================

    /**
     * @brief Set the viewport for rendering
     *
     * Defines the rectangular region of the render target where
     * rendering will occur. Affects vertex transformation.
     *
     * @param vp The viewport to set
     *
     * @see Viewport
     */
    virtual void SetViewport(const Viewport& vp) = 0;

    /**
     * @brief Set the scissor rectangle for clipping
     *
     * Pixels outside the scissor rectangle are discarded.
     * Set to the full viewport size to disable scissor testing.
     *
     * @param scissor The scissor rectangle
     *
     * @see ScissorRect
     */
    virtual void SetScissor(const ScissorRect& scissor) = 0;

    /**
     * @brief Set the active camera for rendering
     *
     * The camera provides view and projection matrices that are
     * used for all subsequent draw calls.
     *
     * @param camera Pointer to the camera (may be nullptr to clear)
     *
     * @see Camera
     */
    virtual void SetCamera(const Camera* camera) = 0;

    /**
     * @brief Set the current render target
     *
     * Redirects rendering output to a framebuffer or the default
     * screen buffer.
     *
     * @param target The render target to use (nullptr = default)
     *
     * @see RenderTarget
     */
    virtual void SetRenderTarget(RenderTarget* target) = 0;

    /**
     * @brief Clear the current render target
     *
     * Clears specified attachments with the given values.
     *
     * @param flags Which attachments to clear
     * @param clearValue Clear values for color, depth, and stencil
     *
     * @see ClearFlags
     * @see ClearValue
     */
    virtual void Clear(ClearFlags flags, const ClearValue& clearValue) = 0;

    /**
     * @brief Clear with default values
     *
     * Convenience overload that clears all attachments with
     * default clear values.
     *
     * @param color Optional clear color (default: black)
     */
    virtual void Clear(const glm::vec4& color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) {
        ClearValue cv;
        cv.color = color;
        Clear(ClearFlags::All, cv);
    }

    // ========================================================================
    // Resource Creation
    // ========================================================================

    /**
     * @brief Create a texture resource
     *
     * Creates a new texture with the specified parameters.
     * The texture is initially uninitialized unless initial data
     * is provided in the descriptor.
     *
     * @param desc Texture descriptor
     * @return Shared pointer to the created texture, or nullptr on failure
     *
     * @note This method is thread-safe.
     *
     * @see TextureDesc
     */
    virtual std::shared_ptr<Texture> CreateTexture(const TextureDesc& desc) = 0;

    /**
     * @brief Create a GPU buffer resource
     *
     * Creates a new buffer with the specified parameters.
     *
     * @param desc Buffer descriptor
     * @return Shared pointer to the created buffer, or nullptr on failure
     *
     * @note This method is thread-safe.
     *
     * @see BufferDesc
     * @see IBuffer
     */
    virtual std::shared_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) = 0;

    /**
     * @brief Create a shader program
     *
     * Compiles and links shader stages into a shader program.
     *
     * @param desc Shader descriptor
     * @return Shared pointer to the created shader, or nullptr on failure
     *
     * @note This method is thread-safe for source loading, but
     *       compilation may require the main thread depending on backend.
     *
     * @see ShaderDesc
     */
    virtual std::shared_ptr<Shader> CreateShader(const ShaderDesc& desc) = 0;

    // ========================================================================
    // Capability Queries
    // ========================================================================

    /**
     * @brief Get renderer capabilities
     *
     * Returns a structure describing the features and limits
     * supported by this renderer.
     *
     * @return Capabilities structure
     *
     * @see RendererCapabilities
     */
    virtual RendererCapabilities GetCapabilities() const = 0;

    /**
     * @brief Get the backend type
     *
     * Returns the graphics API backend used by this renderer.
     *
     * @return Backend type enumeration
     *
     * @see RendererBackend
     */
    virtual RendererBackend GetBackendType() const = 0;

    /**
     * @brief Get the renderer name
     *
     * Returns a human-readable name for this renderer,
     * suitable for display in UI.
     *
     * @return Name string view (valid for renderer lifetime)
     */
    virtual std::string_view GetName() const = 0;

    /**
     * @brief Check if a specific feature is supported
     *
     * @param feature The feature to check
     * @return true if the feature is supported
     */
    virtual bool SupportsFeature(RendererFeature feature) const {
        return HasFeature(GetCapabilities().supportedFeatures, feature);
    }

    // ========================================================================
    // Statistics and Debugging
    // ========================================================================

    /**
     * @brief Get frame statistics
     *
     * Returns statistics for the last completed frame including
     * draw call counts, timing information, and resource usage.
     *
     * @return Statistics structure
     */
    virtual RenderStats GetStats() const = 0;

    /**
     * @brief Reset statistics
     *
     * Clears accumulated statistics. Called automatically at
     * the start of each frame.
     */
    virtual void ResetStats() = 0;

    /**
     * @brief Enable or disable debug mode
     *
     * When enabled, the renderer may perform additional validation
     * and produce debug output.
     *
     * @param enabled true to enable debug mode
     */
    virtual void SetDebugMode(bool enabled) = 0;

    /**
     * @brief Check if debug mode is enabled
     *
     * @return true if debug mode is active
     */
    virtual bool IsDebugMode() const = 0;

    // ========================================================================
    // Resize Handling
    // ========================================================================

    /**
     * @brief Handle window/surface resize
     *
     * Called when the render surface changes size. Updates internal
     * resources to match the new dimensions.
     *
     * @param width New width in pixels
     * @param height New height in pixels
     *
     * @pre width > 0 && height > 0
     */
    virtual void OnResize(int32_t width, int32_t height) = 0;

    /**
     * @brief Get the current render dimensions
     *
     * @return Dimensions as (width, height)
     */
    virtual glm::ivec2 GetDimensions() const = 0;

protected:
    // Protected constructor to prevent direct instantiation
    IRenderer() = default;

    // Non-copyable
    IRenderer(const IRenderer&) = delete;
    IRenderer& operator=(const IRenderer&) = delete;

    // Movable (for derived classes)
    IRenderer(IRenderer&&) = default;
    IRenderer& operator=(IRenderer&&) = default;
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a renderer for the specified backend
 *
 * Factory function that creates the appropriate renderer implementation
 * for the requested backend.
 *
 * @param backend The graphics API backend to use
 * @return Unique pointer to the renderer, or nullptr if the backend is unavailable
 *
 * @see RendererBackend
 * @see IRenderer
 */
std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);

/**
 * @brief Create a renderer with automatic backend selection
 *
 * Attempts to create a renderer using the best available backend
 * for the current platform.
 *
 * Selection priority:
 * 1. Vulkan (if available)
 * 2. DX12 (Windows only)
 * 3. Metal (macOS/iOS only)
 * 4. OpenGL (fallback)
 *
 * @return Unique pointer to the renderer, or nullptr on failure
 */
std::unique_ptr<IRenderer> CreateBestRenderer();

/**
 * @brief Check if a backend is available on this system
 *
 * @param backend The backend to check
 * @return true if the backend can be used
 */
bool IsBackendAvailable(RendererBackend backend);

/**
 * @brief Get all available backends on this system
 *
 * @return Vector of available backend types
 */
std::vector<RendererBackend> GetAvailableBackends();

} // namespace Nova
