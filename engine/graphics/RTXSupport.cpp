#include "RTXSupport.hpp"
#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>
#include <algorithm>

namespace Nova {

// =============================================================================
// RTXCapabilities Implementation
// =============================================================================

bool RTXCapabilities::HasExtension(const std::string& ext) const {
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

std::string RTXCapabilities::ToString() const {
    std::stringstream ss;
    ss << "=== RTX Capabilities ===\n";
    ss << "Ray Tracing Available: " << (hasRayTracing ? "Yes" : "No") << "\n";

    if (hasRayTracing) {
        ss << "API: ";
        switch (api) {
            case RayTracingAPI::OpenGL_NV_ray_tracing:
                ss << "OpenGL NV_ray_tracing\n";
                break;
            case RayTracingAPI::Vulkan_KHR:
                ss << "Vulkan KHR Ray Tracing\n";
                break;
            case RayTracingAPI::DirectX_Raytracing:
                ss << "DirectX Raytracing (DXR)\n";
                break;
            default:
                ss << "None\n";
                break;
        }

        ss << "Tier: " << static_cast<int>(tier) / 10 << "." << static_cast<int>(tier) % 10 << "\n";
        ss << "Device: " << deviceName << "\n";
        ss << "Vendor: " << vendorName << "\n";

        ss << "\nFeatures:\n";
        ss << "  Inline Ray Tracing: " << (hasInlineRayTracing ? "Yes" : "No") << "\n";
        ss << "  Ray Query: " << (hasRayQuery ? "Yes" : "No") << "\n";
        ss << "  Mesh Shaders: " << (hasMeshShaders ? "Yes" : "No") << "\n";
        ss << "  Opacity Micromap: " << (hasOpacityMicromap ? "Yes" : "No") << "\n";
        ss << "  Displacement Micromap: " << (hasDisplacementMicromap ? "Yes" : "No") << "\n";
        ss << "  Shader Execution Reordering: " << (hasShaderExecutionReordering ? "Yes" : "No") << "\n";

        ss << "\nLimits:\n";
        ss << "  Max Recursion Depth: " << maxRecursionDepth << "\n";
        ss << "  Max Ray Generation Threads: " << maxRayGenerationThreads << "\n";
        ss << "  Max Instance Count: " << maxInstanceCount << "\n";
        ss << "  Max Geometry Count: " << maxGeometryCount << "\n";
        ss << "  Max AS Size: " << (maxAccelerationStructureSize / (1024 * 1024)) << " MB\n";
    }

    return ss.str();
}

// =============================================================================
// RTXPerformanceMetrics Implementation
// =============================================================================

void RTXPerformanceMetrics::Reset() {
    *this = RTXPerformanceMetrics{};
}

void RTXPerformanceMetrics::Calculate() {
    if (totalFrameTime > 0.0) {
        raysPerSecond = (static_cast<double>(totalRaysCast) / totalFrameTime) * 1000.0;
    }
}

std::string RTXPerformanceMetrics::ToString() const {
    std::stringstream ss;
    ss << "=== RTX Performance Metrics ===\n";
    ss << "Frame Time: " << totalFrameTime << " ms\n";
    ss << "  AS Build: " << accelerationBuildTime << " ms\n";
    ss << "  AS Update: " << accelerationUpdateTime << " ms\n";
    ss << "  Ray Tracing: " << rayTracingTime << " ms\n";
    ss << "  Shading: " << shadingTime << " ms\n";
    ss << "  Denoising: " << denoisingTime << " ms\n";

    ss << "\nRay Statistics:\n";
    ss << "  Total Rays: " << totalRaysCast << "\n";
    ss << "  Primary: " << primaryRays << "\n";
    ss << "  Shadow: " << shadowRays << "\n";
    ss << "  Secondary: " << secondaryRays << "\n";
    ss << "  AO: " << aoRays << "\n";
    ss << "  Rays/Second: " << (raysPerSecond / 1000000.0) << " M\n";

    ss << "\nAcceleration Structures:\n";
    ss << "  BLAS Count: " << blasCount << "\n";
    ss << "  TLAS Instances: " << tlasInstanceCount << "\n";
    ss << "  AS Memory: " << (totalASMemory / (1024 * 1024)) << " MB\n";
    ss << "  Scratch Memory: " << (scratchMemoryUsed / (1024 * 1024)) << " MB\n";

    ss << "\nPerformance:\n";
    ss << "  Speedup vs Compute: " << speedupVsCompute << "x\n";

    return ss.str();
}

// =============================================================================
// RTXSupport Implementation
// =============================================================================

RTXSupport& RTXSupport::Get() {
    static RTXSupport instance;
    return instance;
}

bool RTXSupport::Initialize() {
    if (m_initialized) {
        return m_capabilities.hasRayTracing;
    }

    spdlog::info("Detecting hardware ray tracing support...");

    // Try different APIs in order of preference
    // 1. Try OpenGL NV_ray_tracing (easiest to integrate with existing OpenGL renderer)
    if (DetectOpenGLRayTracing()) {
        m_capabilities.api = RayTracingAPI::OpenGL_NV_ray_tracing;
        QueryOpenGLCapabilities();
        m_initialized = true;
        spdlog::info("OpenGL NV_ray_tracing detected");
        LogCapabilities();
        return true;
    }

    // 2. Try Vulkan KHR ray tracing
    if (DetectVulkanRayTracing()) {
        m_capabilities.api = RayTracingAPI::Vulkan_KHR;
        QueryVulkanCapabilities();
        m_initialized = true;
        spdlog::info("Vulkan KHR ray tracing detected");
        LogCapabilities();
        return true;
    }

    // 3. Try DirectX Raytracing
    if (DetectDirectXRayTracing()) {
        m_capabilities.api = RayTracingAPI::DirectX_Raytracing;
        QueryDirectXCapabilities();
        m_initialized = true;
        spdlog::info("DirectX Raytracing detected");
        LogCapabilities();
        return true;
    }

    spdlog::warn("No hardware ray tracing support detected - will use compute shader fallback");
    m_initialized = true;
    return false;
}

void RTXSupport::Shutdown() {
    m_initialized = false;
    m_capabilities = RTXCapabilities{};
    m_metrics.Reset();
}

bool RTXSupport::IsAvailable() {
    return Get().m_initialized && Get().m_capabilities.hasRayTracing;
}

RayTracingTier RTXSupport::GetRayTracingTier() {
    return Get().m_capabilities.tier;
}

RayTracingAPI RTXSupport::GetRayTracingAPI() {
    return Get().m_capabilities.api;
}

RTXCapabilities RTXSupport::QueryCapabilities() {
    auto& instance = Get();
    if (!instance.m_initialized) {
        instance.Initialize();
    }
    return instance.m_capabilities;
}

bool RTXSupport::HasFeature(const std::string& feature) const {
    if (feature == "ray_tracing") return m_capabilities.hasRayTracing;
    if (feature == "inline_ray_tracing") return m_capabilities.hasInlineRayTracing;
    if (feature == "ray_query") return m_capabilities.hasRayQuery;
    if (feature == "mesh_shaders") return m_capabilities.hasMeshShaders;
    if (feature == "opacity_micromap") return m_capabilities.hasOpacityMicromap;
    if (feature == "displacement_micromap") return m_capabilities.hasDisplacementMicromap;
    if (feature == "shader_execution_reordering") return m_capabilities.hasShaderExecutionReordering;
    return false;
}

void RTXSupport::LogCapabilities() const {
    spdlog::info(m_capabilities.ToString());
}

double RTXSupport::BenchmarkPerformance() {
    if (!m_capabilities.hasRayTracing) {
        spdlog::warn("Cannot benchmark: No ray tracing support");
        return 0.0;
    }

    spdlog::info("Running ray tracing benchmark...");

    // Simple benchmark: measure time to trace a fixed number of rays
    // This uses a compute shader to trace rays against a simple scene
    constexpr uint64_t benchmarkRayCount = 1000000; // 1 million rays
    constexpr int warmupIterations = 3;
    constexpr int benchmarkIterations = 10;

    // Warm-up passes (GPU needs to reach steady state)
    for (int i = 0; i < warmupIterations; ++i) {
        // In a full implementation, this would dispatch ray tracing work
        glFinish(); // Ensure GPU is synchronized
    }

    // Timed benchmark passes
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < benchmarkIterations; ++i) {
        // In a full implementation with GL_NV_ray_tracing:
        // 1. Bind acceleration structure
        // 2. Dispatch ray generation shader
        // 3. Synchronize
        // For now, we measure an estimated time based on GPU tier
        glFinish();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Calculate actual rays per second if we had real ray tracing
    // For now, estimate based on tier (this would be replaced with actual measurements)
    double estimatedRaysPerSecond = 0.0;

    switch (m_capabilities.tier) {
        case RayTracingTier::Tier_1_0:
            // GTX 16 series, RTX 20 series base - ~1 Grays/s
            estimatedRaysPerSecond = 1000000000.0;
            break;
        case RayTracingTier::Tier_1_1:
            // RTX 20/30 series with inline ray tracing - ~2-3 Grays/s
            estimatedRaysPerSecond = 2500000000.0;
            break;
        case RayTracingTier::Tier_1_2:
            // RTX 40 series with SER and micromaps - ~4-6 Grays/s
            estimatedRaysPerSecond = 5000000000.0;
            break;
        default:
            // Fallback compute path - ~100-500 Mrays/s
            estimatedRaysPerSecond = 250000000.0;
            break;
    }

    // If we had actual ray tracing, calculate real performance:
    // double measuredRaysPerSecond = (benchmarkRayCount * benchmarkIterations) / (elapsedMs / 1000.0);

    spdlog::info("Benchmark complete:");
    spdlog::info("  GPU synchronization time: {:.2f} ms ({} iterations)", elapsedMs, benchmarkIterations);
    spdlog::info("  Estimated ray tracing performance: {:.2f} Grays/s",
                 estimatedRaysPerSecond / 1000000000.0);
    spdlog::info("  Note: Actual performance requires GL_NV_ray_tracing hardware support");

    // Store in metrics for reference
    m_metrics.raysPerSecond = estimatedRaysPerSecond;

    return estimatedRaysPerSecond;
}

// =============================================================================
// Detection Methods
// =============================================================================

bool RTXSupport::DetectOpenGLRayTracing() {
    // Check for NV_ray_tracing extension
    const char* extensionsStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!extensionsStr) {
        return false;
    }

    std::string extensions(extensionsStr);
    bool hasNvRayTracing = extensions.find("GL_NV_ray_tracing") != std::string::npos;

    if (hasNvRayTracing) {
        m_capabilities.hasRayTracing = true;
        m_capabilities.extensions.push_back("GL_NV_ray_tracing");

        // Check for additional extensions
        if (extensions.find("GL_NV_ray_tracing_motion_blur") != std::string::npos) {
            m_capabilities.hasRayMotionBlur = true;
            m_capabilities.extensions.push_back("GL_NV_ray_tracing_motion_blur");
        }

        return true;
    }

    return false;
}

bool RTXSupport::DetectVulkanRayTracing() {
    // Vulkan ray tracing detection is not yet implemented
    // Full implementation would require:
    // 1. Create VkInstance with VK_KHR_get_physical_device_properties2
    // 2. Enumerate physical devices
    // 3. Check for VK_KHR_ray_tracing_pipeline extension
    // 4. Check for VK_KHR_acceleration_structure extension
    // 5. Query VkPhysicalDeviceRayTracingPipelinePropertiesKHR

    spdlog::debug("Vulkan ray tracing detection not implemented - engine uses OpenGL backend");
    spdlog::debug("  To enable: Link with Vulkan SDK and implement VK_KHR_ray_tracing_pipeline checks");
    return false;
}

bool RTXSupport::DetectDirectXRayTracing() {
    // DirectX Raytracing (DXR) detection is not yet implemented
    // Full implementation would require:
    // 1. Create ID3D12Device
    // 2. Call CheckFeatureSupport with D3D12_FEATURE_D3D12_OPTIONS5
    // 3. Check D3D12_RAYTRACING_TIER in D3D12_FEATURE_DATA_D3D12_OPTIONS5
    // 4. Query for DXR 1.0 (Tier 1.0) or DXR 1.1 (Tier 1.1) support

    spdlog::debug("DirectX Raytracing (DXR) detection not implemented - engine uses OpenGL backend");
    spdlog::debug("  To enable: Link with D3D12 and implement D3D12_RAYTRACING_TIER checks");
    return false;
}

// =============================================================================
// Capability Queries
// =============================================================================

void RTXSupport::QueryOpenGLCapabilities() {
    // Get device info
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

    if (vendor) m_capabilities.vendorName = vendor;
    if (renderer) m_capabilities.deviceName = renderer;
    if (version) m_capabilities.apiVersion = version;

    // Query OpenGL ray tracing limits
    // Note: These would need the actual GL_NV_ray_tracing constants
    // For now, use typical values for RTX GPUs

    m_capabilities.maxRecursionDepth = 31;
    m_capabilities.maxRayGenerationThreads = 1048576;
    m_capabilities.maxInstanceCount = 1048576;
    m_capabilities.maxGeometryCount = 65536;
    m_capabilities.maxAccelerationStructureSize = 4ULL * 1024 * 1024 * 1024; // 4GB

    // Determine tier based on GPU
    std::string gpuName = m_capabilities.deviceName;
    if (gpuName.find("RTX 40") != std::string::npos) {
        m_capabilities.tier = RayTracingTier::Tier_1_2;
        m_capabilities.hasOpacityMicromap = true;
        m_capabilities.hasDisplacementMicromap = true;
        m_capabilities.hasShaderExecutionReordering = true;
        m_capabilities.hasInlineRayTracing = true;
        m_capabilities.hasRayQuery = true;
    } else if (gpuName.find("RTX 30") != std::string::npos ||
               gpuName.find("RTX 20") != std::string::npos) {
        m_capabilities.tier = RayTracingTier::Tier_1_1;
        m_capabilities.hasInlineRayTracing = true;
        m_capabilities.hasRayQuery = true;
    } else if (gpuName.find("RTX") != std::string::npos ||
               gpuName.find("GTX 16") != std::string::npos) {
        m_capabilities.tier = RayTracingTier::Tier_1_0;
    }

    // Check for mesh shaders
    const char* extensionsStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensionsStr) {
        std::string extensions(extensionsStr);
        m_capabilities.hasMeshShaders = extensions.find("GL_NV_mesh_shader") != std::string::npos;
        m_capabilities.hasVariableRateShading = extensions.find("GL_NV_shading_rate_image") != std::string::npos;
    }

    // Set typical alignment values
    m_capabilities.scratchBufferAlignment = 256;
    m_capabilities.shaderGroupHandleSize = 32;
    m_capabilities.shaderGroupBaseAlignment = 64;
}

void RTXSupport::QueryVulkanCapabilities() {
    // Vulkan capability query not implemented - return default/empty capabilities
    // Full implementation would query:
    // - VkPhysicalDeviceRayTracingPipelinePropertiesKHR
    // - VkPhysicalDeviceAccelerationStructurePropertiesKHR
    // - maxRayRecursionDepth, maxRayDispatchInvocationCount, etc.

    spdlog::warn("Vulkan ray tracing capability query not implemented");
    spdlog::info("  Returning default capabilities for Vulkan backend");

    // Set default capabilities (conservative estimates for Vulkan RT)
    m_capabilities.hasRayTracing = false;
    m_capabilities.hasInlineRayTracing = false;
    m_capabilities.hasRayQuery = false;
    m_capabilities.tier = RayTracingTier::None;
    m_capabilities.maxRecursionDepth = 0;
    m_capabilities.maxRayGenerationThreads = 0;
    m_capabilities.maxInstanceCount = 0;
    m_capabilities.maxGeometryCount = 0;
    m_capabilities.maxAccelerationStructureSize = 0;
    m_capabilities.apiVersion = "Vulkan (not initialized)";
}

void RTXSupport::QueryDirectXCapabilities() {
    // DirectX capability query not implemented - return default/empty capabilities
    // Full implementation would query:
    // - D3D12_RAYTRACING_TIER from CheckFeatureSupport
    // - D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO
    // - MaxRecursionDepth from D3D12_RAYTRACING_TIER

    spdlog::warn("DirectX ray tracing capability query not implemented");
    spdlog::info("  Returning default capabilities for DirectX backend");

    // Set default capabilities (conservative estimates for DXR)
    m_capabilities.hasRayTracing = false;
    m_capabilities.hasInlineRayTracing = false;
    m_capabilities.hasRayQuery = false;
    m_capabilities.tier = RayTracingTier::None;
    m_capabilities.maxRecursionDepth = 0;
    m_capabilities.maxRayGenerationThreads = 0;
    m_capabilities.maxInstanceCount = 0;
    m_capabilities.maxGeometryCount = 0;
    m_capabilities.maxAccelerationStructureSize = 0;
    m_capabilities.apiVersion = "DirectX 12 (not initialized)";
}

// =============================================================================
// RTXScopedTimer Implementation
// =============================================================================

RTXScopedTimer::RTXScopedTimer(double& outTime)
    : m_outTime(outTime) {
    auto now = std::chrono::high_resolution_clock::now();
    m_startTime = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
}

RTXScopedTimer::~RTXScopedTimer() {
    auto now = std::chrono::high_resolution_clock::now();
    double endTime = std::chrono::duration<double, std::milli>(now.time_since_epoch()).count();
    m_outTime = endTime - m_startTime;
}

} // namespace Nova
