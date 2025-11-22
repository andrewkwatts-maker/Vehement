/**
 * @file MacOSMetal.mm
 * @brief Metal rendering implementation for macOS
 */

#include "MacOSMetal.hpp"

#ifdef NOVA_PLATFORM_MACOS

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

namespace Nova {

// =============================================================================
// Implementation Structure
// =============================================================================

struct MacOSMetalContext::Impl {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    MTKView* view = nil;
    NSWindow* window = nil;

    id<MTLTexture> depthStencilTexture = nil;
    MTLRenderPassDescriptor* renderPassDescriptor = nil;

    id<MTLCommandBuffer> currentCommandBuffer = nil;
    id<CAMetalDrawable> currentDrawable = nil;

    // Triple buffering
    static constexpr int maxFramesInFlight = 3;
    dispatch_semaphore_t frameSemaphore = nil;
    int frameIndex = 0;
};

// =============================================================================
// MacOSMetalContext
// =============================================================================

MacOSMetalContext::MacOSMetalContext()
    : m_impl(std::make_unique<Impl>()) {}

MacOSMetalContext::~MacOSMetalContext() {
    Destroy();
}

bool MacOSMetalContext::Create(void* nsWindow) {
    @autoreleasepool {
        m_impl->window = (__bridge NSWindow*)nsWindow;
        if (!m_impl->window) {
            return false;
        }

        // Get default Metal device
        m_impl->device = MTLCreateSystemDefaultDevice();
        if (!m_impl->device) {
            return false;
        }

        // Create command queue
        m_impl->commandQueue = [m_impl->device newCommandQueue];
        if (!m_impl->commandQueue) {
            return false;
        }

        // Create MTKView
        NSRect frame = [[m_impl->window contentView] bounds];
        m_impl->view = [[MTKView alloc] initWithFrame:frame device:m_impl->device];
        if (!m_impl->view) {
            return false;
        }

        // Configure view
        m_impl->view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        m_impl->view.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        m_impl->view.sampleCount = 1;
        m_impl->view.clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
        m_impl->view.preferredFramesPerSecond = 60;
        m_impl->view.enableSetNeedsDisplay = NO;
        m_impl->view.paused = NO;

        // Replace window content view
        [m_impl->window setContentView:m_impl->view];

        // Create semaphore for triple buffering
        m_impl->frameSemaphore = dispatch_semaphore_create(Impl::maxFramesInFlight);

        // Query capabilities
        QueryCapabilities();

        return true;
    }
}

void MacOSMetalContext::Destroy() {
    @autoreleasepool {
        if (m_impl->frameSemaphore) {
            // Wait for all frames to complete
            for (int i = 0; i < Impl::maxFramesInFlight; ++i) {
                dispatch_semaphore_wait(m_impl->frameSemaphore, DISPATCH_TIME_FOREVER);
            }
            for (int i = 0; i < Impl::maxFramesInFlight; ++i) {
                dispatch_semaphore_signal(m_impl->frameSemaphore);
            }
        }

        m_impl->currentCommandBuffer = nil;
        m_impl->currentDrawable = nil;
        m_impl->depthStencilTexture = nil;
        m_impl->renderPassDescriptor = nil;
        m_impl->view = nil;
        m_impl->commandQueue = nil;
        m_impl->device = nil;
    }
}

bool MacOSMetalContext::Initialize(const GraphicsConfig& config) {
    m_vsync = config.vsync;
    return true;
}

void MacOSMetalContext::Shutdown() {
    Destroy();
}

void MacOSMetalContext::MakeCurrent() {
    // Metal doesn't have the concept of a current context
}

bool MacOSMetalContext::IsCurrent() const {
    return m_impl->device != nil;
}

void MacOSMetalContext::SwapBuffers() {
    // For MTKView, this is handled in EndFrame
}

void MacOSMetalContext::SetVSync(bool enabled) {
    m_vsync = enabled;
    if (m_impl->view) {
        @autoreleasepool {
            m_impl->view.preferredFramesPerSecond = enabled ? 60 : 120;
        }
    }
}

void MacOSMetalContext::QueryCapabilities() {
    m_capabilities.api = GraphicsAPI::Metal;
    m_capabilities.vendor = GPUVendor::Apple;

    @autoreleasepool {
        m_capabilities.rendererString = [[m_impl->device name] UTF8String];
        m_capabilities.vendorString = "Apple";

        // Metal 3 check
        if (@available(macOS 13.0, *)) {
            m_capabilities.apiVersion = "Metal 3";
        } else if (@available(macOS 11.0, *)) {
            m_capabilities.apiVersion = "Metal 2.3";
        } else {
            m_capabilities.apiVersion = "Metal 2";
        }

        // Query limits
        m_capabilities.maxTextureSize = 16384;
        m_capabilities.maxCubemapSize = 16384;
        m_capabilities.maxArrayTextureLayers = 2048;

#if TARGET_CPU_ARM64
        m_capabilities.maxTextureSize = 16384;
        m_capabilities.supportsRayTracing = true;
#else
        // Intel Mac limits
        m_capabilities.maxTextureSize = 16384;
        m_capabilities.supportsRayTracing = false;
#endif

        m_capabilities.maxColorAttachments = 8;
        m_capabilities.maxVertexAttributes = 31;
        m_capabilities.maxTextureUnits = 128;
        m_capabilities.maxUniformBufferBindings = 31;

        // Feature support
        m_capabilities.supportsComputeShaders = true;
        m_capabilities.supportsGeometryShaders = false;  // Metal doesn't have geometry shaders
        m_capabilities.supportsTessellation = true;
        m_capabilities.supportsInstancing = true;
        m_capabilities.supportsSSBO = true;
        m_capabilities.supportsImageLoadStore = true;
        m_capabilities.supportsMeshShaders = [m_impl->device supportsFamily:MTLGPUFamilyApple7];
    }
}

void* MacOSMetalContext::GetDevice() const {
    return (__bridge void*)m_impl->device;
}

void* MacOSMetalContext::GetCommandQueue() const {
    return (__bridge void*)m_impl->commandQueue;
}

void* MacOSMetalContext::GetView() const {
    return (__bridge void*)m_impl->view;
}

void* MacOSMetalContext::GetCurrentDrawableTexture() const {
    if (m_impl->currentDrawable) {
        return (__bridge void*)[m_impl->currentDrawable texture];
    }
    return nullptr;
}

void* MacOSMetalContext::GetDepthStencilTexture() const {
    return (__bridge void*)m_impl->view.depthStencilTexture;
}

void* MacOSMetalContext::BeginFrame() {
    @autoreleasepool {
        // Wait for available frame slot
        dispatch_semaphore_wait(m_impl->frameSemaphore, DISPATCH_TIME_FOREVER);

        // Get drawable
        m_impl->currentDrawable = [m_impl->view currentDrawable];
        if (!m_impl->currentDrawable) {
            dispatch_semaphore_signal(m_impl->frameSemaphore);
            return nullptr;
        }

        // Create command buffer
        m_impl->currentCommandBuffer = [m_impl->commandQueue commandBuffer];
        m_impl->currentCommandBuffer.label = @"Frame Command Buffer";

        // Add completion handler
        __block dispatch_semaphore_t semaphore = m_impl->frameSemaphore;
        [m_impl->currentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            (void)buffer;
            dispatch_semaphore_signal(semaphore);
        }];

        return (__bridge void*)m_impl->currentCommandBuffer;
    }
}

void MacOSMetalContext::EndFrame() {
    @autoreleasepool {
        if (m_impl->currentCommandBuffer && m_impl->currentDrawable) {
            [m_impl->currentCommandBuffer presentDrawable:m_impl->currentDrawable];
            [m_impl->currentCommandBuffer commit];
        }

        m_impl->currentCommandBuffer = nil;
        m_impl->currentDrawable = nil;
        m_impl->frameIndex = (m_impl->frameIndex + 1) % Impl::maxFramesInFlight;
    }
}

void* MacOSMetalContext::CreateRenderEncoder(void* commandBuffer, const MetalRenderPassDesc& desc) {
    @autoreleasepool {
        id<MTLCommandBuffer> cmdBuffer = (__bridge id<MTLCommandBuffer>)commandBuffer;

        MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];

        // Color attachment
        renderPass.colorAttachments[0].texture = [m_impl->currentDrawable texture];
        renderPass.colorAttachments[0].loadAction = desc.clearColor ? MTLLoadActionClear : MTLLoadActionLoad;
        renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[0].clearColor = MTLClearColorMake(
            desc.clearColorValue[0], desc.clearColorValue[1],
            desc.clearColorValue[2], desc.clearColorValue[3]);

        // Depth attachment
        if (m_impl->view.depthStencilTexture) {
            renderPass.depthAttachment.texture = m_impl->view.depthStencilTexture;
            renderPass.depthAttachment.loadAction = desc.clearDepth ? MTLLoadActionClear : MTLLoadActionLoad;
            renderPass.depthAttachment.storeAction = MTLStoreActionStore;
            renderPass.depthAttachment.clearDepth = desc.clearDepthValue;

            renderPass.stencilAttachment.texture = m_impl->view.depthStencilTexture;
            renderPass.stencilAttachment.loadAction = desc.clearStencil ? MTLLoadActionClear : MTLLoadActionLoad;
            renderPass.stencilAttachment.storeAction = MTLStoreActionStore;
            renderPass.stencilAttachment.clearStencil = desc.clearStencilValue;
        }

        id<MTLRenderCommandEncoder> encoder = [cmdBuffer renderCommandEncoderWithDescriptor:renderPass];
        return (__bridge_retained void*)encoder;
    }
}

void MacOSMetalContext::EndRenderEncoder(void* encoder) {
    @autoreleasepool {
        id<MTLRenderCommandEncoder> enc = (__bridge_transfer id<MTLRenderCommandEncoder>)encoder;
        [enc endEncoding];
    }
}

void* MacOSMetalContext::CreateComputeEncoder(void* commandBuffer) {
    @autoreleasepool {
        id<MTLCommandBuffer> cmdBuffer = (__bridge id<MTLCommandBuffer>)commandBuffer;
        id<MTLComputeCommandEncoder> encoder = [cmdBuffer computeCommandEncoder];
        return (__bridge_retained void*)encoder;
    }
}

void MacOSMetalContext::EndComputeEncoder(void* encoder) {
    @autoreleasepool {
        id<MTLComputeCommandEncoder> enc = (__bridge_transfer id<MTLComputeCommandEncoder>)encoder;
        [enc endEncoding];
    }
}

// =============================================================================
// Resource Creation
// =============================================================================

void* MacOSMetalContext::CreateBuffer(size_t size, MetalBufferUsage usage, const void* data) {
    @autoreleasepool {
        MTLResourceOptions options = MTLResourceStorageModeShared;

        switch (usage) {
            case MetalBufferUsage::Default:
            case MetalBufferUsage::Dynamic:
                options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
                break;
            case MetalBufferUsage::Static:
            case MetalBufferUsage::Private:
                options = MTLResourceStorageModePrivate;
                break;
            case MetalBufferUsage::Managed:
                options = MTLResourceStorageModeManaged;
                break;
        }

        id<MTLBuffer> buffer;
        if (data) {
            buffer = [m_impl->device newBufferWithBytes:data length:size options:options];
        } else {
            buffer = [m_impl->device newBufferWithLength:size options:options];
        }

        return (__bridge_retained void*)buffer;
    }
}

void MacOSMetalContext::DestroyBuffer(void* buffer) {
    if (buffer) {
        @autoreleasepool {
            id<MTLBuffer> buf = (__bridge_transfer id<MTLBuffer>)buffer;
            buf = nil;
        }
    }
}

void MacOSMetalContext::UpdateBuffer(void* buffer, const void* data, size_t size, size_t offset) {
    @autoreleasepool {
        id<MTLBuffer> buf = (__bridge id<MTLBuffer>)buffer;
        memcpy(static_cast<uint8_t*>([buf contents]) + offset, data, size);

        // Synchronize if managed
        if ([buf storageMode] == MTLStorageModeManaged) {
            [buf didModifyRange:NSMakeRange(offset, size)];
        }
    }
}

void* MacOSMetalContext::CreateTexture(int width, int height, MetalTextureFormat format, const void* data) {
    @autoreleasepool {
        MTLPixelFormat mtlFormat = MTLPixelFormatRGBA8Unorm;
        switch (format) {
            case MetalTextureFormat::RGBA8Unorm: mtlFormat = MTLPixelFormatRGBA8Unorm; break;
            case MetalTextureFormat::RGBA8Snorm: mtlFormat = MTLPixelFormatRGBA8Snorm; break;
            case MetalTextureFormat::RGBA16Float: mtlFormat = MTLPixelFormatRGBA16Float; break;
            case MetalTextureFormat::RGBA32Float: mtlFormat = MTLPixelFormatRGBA32Float; break;
            case MetalTextureFormat::R8Unorm: mtlFormat = MTLPixelFormatR8Unorm; break;
            case MetalTextureFormat::R16Float: mtlFormat = MTLPixelFormatR16Float; break;
            case MetalTextureFormat::R32Float: mtlFormat = MTLPixelFormatR32Float; break;
            case MetalTextureFormat::RG8Unorm: mtlFormat = MTLPixelFormatRG8Unorm; break;
            case MetalTextureFormat::RG16Float: mtlFormat = MTLPixelFormatRG16Float; break;
            case MetalTextureFormat::RG32Float: mtlFormat = MTLPixelFormatRG32Float; break;
            case MetalTextureFormat::Depth32Float: mtlFormat = MTLPixelFormatDepth32Float; break;
            case MetalTextureFormat::Depth24Stencil8: mtlFormat = MTLPixelFormatDepth24Unorm_Stencil8; break;
            case MetalTextureFormat::BGRA8Unorm: mtlFormat = MTLPixelFormatBGRA8Unorm; break;
            case MetalTextureFormat::BGRA8Srgb: mtlFormat = MTLPixelFormatBGRA8Unorm_sRGB; break;
        }

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                                                       width:width
                                                                                      height:height
                                                                                   mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;

        id<MTLTexture> texture = [m_impl->device newTextureWithDescriptor:desc];

        if (data && texture) {
            // Create staging buffer and blit
            NSUInteger bytesPerPixel = 4;
            if (format == MetalTextureFormat::RGBA16Float) bytesPerPixel = 8;
            else if (format == MetalTextureFormat::RGBA32Float) bytesPerPixel = 16;

            NSUInteger bytesPerRow = width * bytesPerPixel;
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);

            // For private storage, use a blit encoder
            // Simplified: create shared texture and copy
            MTLTextureDescriptor* stagingDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtlFormat
                                                                                                 width:width
                                                                                                height:height
                                                                                             mipmapped:NO];
            stagingDesc.storageMode = MTLStorageModeShared;
            id<MTLTexture> staging = [m_impl->device newTextureWithDescriptor:stagingDesc];
            [staging replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];

            id<MTLCommandBuffer> cmdBuffer = [m_impl->commandQueue commandBuffer];
            id<MTLBlitCommandEncoder> blit = [cmdBuffer blitCommandEncoder];
            [blit copyFromTexture:staging toTexture:texture];
            [blit endEncoding];
            [cmdBuffer commit];
            [cmdBuffer waitUntilCompleted];
        }

        return (__bridge_retained void*)texture;
    }
}

void MacOSMetalContext::DestroyTexture(void* texture) {
    if (texture) {
        @autoreleasepool {
            id<MTLTexture> tex = (__bridge_transfer id<MTLTexture>)texture;
            tex = nil;
        }
    }
}

// =============================================================================
// Shader Management
// =============================================================================

void* MacOSMetalContext::CompileShader(const std::string& source, const std::string& functionName, MetalShaderType /*type*/) {
    @autoreleasepool {
        NSError* error = nil;
        NSString* sourceStr = [NSString stringWithUTF8String:source.c_str()];

        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        options.fastMathEnabled = YES;

        id<MTLLibrary> library = [m_impl->device newLibraryWithSource:sourceStr options:options error:&error];
        if (!library) {
            if (error) {
                NSLog(@"Shader compile error: %@", error.localizedDescription);
            }
            return nullptr;
        }

        NSString* funcName = [NSString stringWithUTF8String:functionName.c_str()];
        id<MTLFunction> function = [library newFunctionWithName:funcName];

        return (__bridge_retained void*)function;
    }
}

void* MacOSMetalContext::LoadShaderLibrary(const std::string& path) {
    @autoreleasepool {
        NSError* error = nil;
        NSString* pathStr = [NSString stringWithUTF8String:path.c_str()];
        NSURL* url = [NSURL fileURLWithPath:pathStr];

        id<MTLLibrary> library = [m_impl->device newLibraryWithURL:url error:&error];
        if (!library && error) {
            NSLog(@"Failed to load shader library: %@", error.localizedDescription);
            return nullptr;
        }

        return (__bridge_retained void*)library;
    }
}

void* MacOSMetalContext::GetShaderFunction(void* library, const std::string& functionName) {
    @autoreleasepool {
        id<MTLLibrary> lib = (__bridge id<MTLLibrary>)library;
        NSString* funcName = [NSString stringWithUTF8String:functionName.c_str()];
        id<MTLFunction> function = [lib newFunctionWithName:funcName];
        return (__bridge_retained void*)function;
    }
}

void* MacOSMetalContext::CreateRenderPipeline(void* vertexFunction, void* fragmentFunction) {
    @autoreleasepool {
        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = (__bridge id<MTLFunction>)vertexFunction;
        desc.fragmentFunction = (__bridge id<MTLFunction>)fragmentFunction;
        desc.colorAttachments[0].pixelFormat = m_impl->view.colorPixelFormat;
        desc.depthAttachmentPixelFormat = m_impl->view.depthStencilPixelFormat;
        desc.stencilAttachmentPixelFormat = m_impl->view.depthStencilPixelFormat;

        NSError* error = nil;
        id<MTLRenderPipelineState> pipeline = [m_impl->device newRenderPipelineStateWithDescriptor:desc error:&error];

        if (!pipeline && error) {
            NSLog(@"Failed to create render pipeline: %@", error.localizedDescription);
            return nullptr;
        }

        return (__bridge_retained void*)pipeline;
    }
}

void* MacOSMetalContext::CreateComputePipeline(void* computeFunction) {
    @autoreleasepool {
        NSError* error = nil;
        id<MTLComputePipelineState> pipeline = [m_impl->device newComputePipelineStateWithFunction:(__bridge id<MTLFunction>)computeFunction error:&error];

        if (!pipeline && error) {
            NSLog(@"Failed to create compute pipeline: %@", error.localizedDescription);
            return nullptr;
        }

        return (__bridge_retained void*)pipeline;
    }
}

void MacOSMetalContext::DestroyPipeline(void* pipeline) {
    if (pipeline) {
        @autoreleasepool {
            id<MTLRenderPipelineState> p = (__bridge_transfer id<MTLRenderPipelineState>)pipeline;
            p = nil;
        }
    }
}

// =============================================================================
// Debug
// =============================================================================

void MacOSMetalContext::SetValidationEnabled(bool enabled) {
    m_validationEnabled = enabled;
    // Validation is enabled via environment variable or scheme settings
}

void MacOSMetalContext::EnableGPUCapture() {
    @autoreleasepool {
        MTLCaptureManager* captureManager = [MTLCaptureManager sharedCaptureManager];
        if ([captureManager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
            MTLCaptureDescriptor* desc = [[MTLCaptureDescriptor alloc] init];
            desc.captureObject = m_impl->device;
            desc.destination = MTLCaptureDestinationGPUTraceDocument;

            NSError* error = nil;
            [captureManager startCaptureWithDescriptor:desc error:&error];
        }
    }
}

void MacOSMetalContext::PushDebugGroup(void* encoder, const std::string& name) {
    @autoreleasepool {
        id<MTLCommandEncoder> enc = (__bridge id<MTLCommandEncoder>)encoder;
        [enc pushDebugGroup:[NSString stringWithUTF8String:name.c_str()]];
    }
}

void MacOSMetalContext::PopDebugGroup(void* encoder) {
    @autoreleasepool {
        id<MTLCommandEncoder> enc = (__bridge id<MTLCommandEncoder>)encoder;
        [enc popDebugGroup];
    }
}

// =============================================================================
// MetalUtils
// =============================================================================

bool MetalUtils::IsMetalAvailable() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        return device != nil;
    }
}

std::vector<std::string> MetalUtils::GetMetalDevices() {
    std::vector<std::string> devices;
    @autoreleasepool {
        NSArray<id<MTLDevice>>* mtlDevices = MTLCopyAllDevices();
        for (id<MTLDevice> device in mtlDevices) {
            devices.push_back([[device name] UTF8String]);
        }
    }
    return devices;
}

std::string MetalUtils::GetDefaultDeviceName() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            return [[device name] UTF8String];
        }
    }
    return "";
}

bool MetalUtils::SupportsAppleSilicon() {
#if TARGET_CPU_ARM64
    return true;
#else
    return false;
#endif
}

size_t MetalUtils::GetMaxBufferSize() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            return [device maxBufferLength];
        }
    }
    return 256 * 1024 * 1024;  // 256 MB default
}

int MetalUtils::GetMaxTextureDimension() {
    return 16384;
}

bool MetalUtils::SupportsFeatureSet(int featureSet) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device) {
            return [device supportsFamily:(MTLGPUFamily)featureSet];
        }
    }
    return false;
}

} // namespace Nova

#endif // NOVA_PLATFORM_MACOS
