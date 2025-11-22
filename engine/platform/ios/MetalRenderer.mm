/**
 * @file MetalRenderer.mm
 * @brief Metal renderer implementation for iOS
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "MetalRenderer.hpp"
#include <regex>
#include <sstream>

namespace Nova {

MetalRenderer::MetalRenderer() = default;

MetalRenderer::~MetalRenderer() {
    Shutdown();
}

bool MetalRenderer::Initialize() {
    @autoreleasepool {
        NSLog(@"Nova3D: Initializing Metal renderer...");

        // Create Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            NSLog(@"Nova3D: Metal is not supported on this device");
            return false;
        }
        m_device = (__bridge_retained void*)device;

        NSLog(@"Nova3D: Metal device: %@", device.name);

        // Create command queue
        id<MTLCommandQueue> commandQueue = [device newCommandQueue];
        if (!commandQueue) {
            NSLog(@"Nova3D: Failed to create Metal command queue");
            return false;
        }
        m_commandQueue = (__bridge_retained void*)commandQueue;

        // Create default library (from compiled shaders in app bundle)
        NSError* error = nil;
        id<MTLLibrary> defaultLibrary = [device newDefaultLibrary];
        if (defaultLibrary) {
            m_defaultLibrary = (__bridge_retained void*)defaultLibrary;
            NSLog(@"Nova3D: Loaded default Metal library");
        } else {
            NSLog(@"Nova3D: No default Metal library found (will compile shaders at runtime)");
        }

        m_initialized = true;
        NSLog(@"Nova3D: Metal renderer initialized successfully");
        return true;
    }
}

void MetalRenderer::Shutdown() {
    @autoreleasepool {
        NSLog(@"Nova3D: Shutting down Metal renderer...");

        // Clear all pipelines
        for (auto& pair : m_pipelines) {
            if (pair.second.pipelineState) {
                CFRelease(pair.second.pipelineState);
            }
            if (pair.second.depthState) {
                CFRelease(pair.second.depthState);
            }
        }
        m_pipelines.clear();

        // Release depth texture
        if (m_depthTexture) {
            CFRelease(m_depthTexture);
            m_depthTexture = nullptr;
        }

        // Release default library
        if (m_defaultLibrary) {
            CFRelease(m_defaultLibrary);
            m_defaultLibrary = nullptr;
        }

        // Release command queue
        if (m_commandQueue) {
            CFRelease(m_commandQueue);
            m_commandQueue = nullptr;
        }

        // Release device
        if (m_device) {
            CFRelease(m_device);
            m_device = nullptr;
        }

        m_initialized = false;
        NSLog(@"Nova3D: Metal renderer shutdown complete");
    }
}

// =============================================================================
// Render Pipeline
// =============================================================================

bool MetalRenderer::CreatePipeline(const std::string& name, const MetalPipelineDesc& desc) {
    @autoreleasepool {
        if (!m_device || !m_defaultLibrary) {
            NSLog(@"Nova3D: Cannot create pipeline - renderer not initialized");
            return false;
        }

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
        id<MTLLibrary> library = (__bridge id<MTLLibrary>)m_defaultLibrary;

        // Get shader functions
        NSString* vertexFuncName = [NSString stringWithUTF8String:desc.vertexFunction.c_str()];
        NSString* fragmentFuncName = [NSString stringWithUTF8String:desc.fragmentFunction.c_str()];

        id<MTLFunction> vertexFunc = [library newFunctionWithName:vertexFuncName];
        id<MTLFunction> fragmentFunc = [library newFunctionWithName:fragmentFuncName];

        if (!vertexFunc || !fragmentFunc) {
            NSLog(@"Nova3D: Failed to load shader functions for pipeline '%s'", name.c_str());
            return false;
        }

        // Create pipeline descriptor
        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.label = [NSString stringWithUTF8String:name.c_str()];
        pipelineDesc.vertexFunction = vertexFunc;
        pipelineDesc.fragmentFunction = fragmentFunc;
        pipelineDesc.colorAttachments[0].pixelFormat = (MTLPixelFormat)desc.colorPixelFormat;
        pipelineDesc.depthAttachmentPixelFormat = (MTLPixelFormat)desc.depthPixelFormat;
        pipelineDesc.stencilAttachmentPixelFormat = (MTLPixelFormat)desc.stencilPixelFormat;
        pipelineDesc.rasterSampleCount = desc.sampleCount;

        // Configure blending
        if (desc.blendingEnabled) {
            pipelineDesc.colorAttachments[0].blendingEnabled = YES;
            pipelineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            pipelineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        }

        // Create pipeline state
        NSError* error = nil;
        id<MTLRenderPipelineState> pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];

        if (!pipelineState) {
            NSLog(@"Nova3D: Failed to create pipeline '%s': %@", name.c_str(), error.localizedDescription);
            return false;
        }

        // Create depth stencil state
        void* depthState = CreateDepthStencilState(desc.depthWriteEnabled, MTLCompareFunctionLess);

        // Store pipeline
        MetalPipeline pipeline;
        pipeline.pipelineState = (__bridge_retained void*)pipelineState;
        pipeline.depthState = depthState;
        pipeline.name = name;

        m_pipelines[name] = pipeline;

        NSLog(@"Nova3D: Created Metal pipeline '%s'", name.c_str());
        return true;
    }
}

bool MetalRenderer::CreatePipeline(const std::string& name,
                                    const std::string& vertexShader,
                                    const std::string& fragmentShader) {
    @autoreleasepool {
        if (!m_device) return false;

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;

        // Combine shaders into a single source
        std::string combinedSource = vertexShader + "\n" + fragmentShader;
        NSString* source = [NSString stringWithUTF8String:combinedSource.c_str()];

        // Compile shader
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];

        if (!library) {
            NSLog(@"Nova3D: Failed to compile shaders for pipeline '%s': %@",
                  name.c_str(), error.localizedDescription);
            return false;
        }

        // Create pipeline with compiled library
        id<MTLFunction> vertexFunc = [library newFunctionWithName:@"vertexMain"];
        id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"fragmentMain"];

        if (!vertexFunc || !fragmentFunc) {
            NSLog(@"Nova3D: Failed to find shader functions in compiled source");
            return false;
        }

        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.label = [NSString stringWithUTF8String:name.c_str()];
        pipelineDesc.vertexFunction = vertexFunc;
        pipelineDesc.fragmentFunction = fragmentFunc;
        pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        id<MTLRenderPipelineState> pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];

        if (!pipelineState) {
            NSLog(@"Nova3D: Failed to create pipeline state: %@", error.localizedDescription);
            return false;
        }

        MetalPipeline pipeline;
        pipeline.pipelineState = (__bridge_retained void*)pipelineState;
        pipeline.depthState = CreateDepthStencilState(true, MTLCompareFunctionLess);
        pipeline.name = name;

        m_pipelines[name] = pipeline;

        NSLog(@"Nova3D: Created Metal pipeline '%s' from source", name.c_str());
        return true;
    }
}

MetalPipeline* MetalRenderer::GetPipeline(const std::string& name) {
    auto it = m_pipelines.find(name);
    return it != m_pipelines.end() ? &it->second : nullptr;
}

void MetalRenderer::DestroyPipeline(const std::string& name) {
    auto it = m_pipelines.find(name);
    if (it != m_pipelines.end()) {
        if (it->second.pipelineState) {
            CFRelease(it->second.pipelineState);
        }
        if (it->second.depthState) {
            CFRelease(it->second.depthState);
        }
        m_pipelines.erase(it);
    }
}

// =============================================================================
// Shader Compilation
// =============================================================================

void* MetalRenderer::CompileShader(const std::string& source) {
    @autoreleasepool {
        if (!m_device) return nullptr;

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
        NSString* shaderSource = [NSString stringWithUTF8String:source.c_str()];

        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];

        if (!library) {
            NSLog(@"Nova3D: Shader compilation failed: %@", error.localizedDescription);
            return nullptr;
        }

        return (__bridge_retained void*)library;
    }
}

std::string MetalRenderer::ConvertGLSLToMetal(const std::string& glsl, ShaderType type) {
    // Basic GLSL to Metal conversion
    // Note: This is a simplified converter. For production use, consider using
    // SPIRV-Cross or similar tools for accurate conversion.

    std::stringstream metal;

    metal << "#include <metal_stdlib>\n";
    metal << "#include <simd/simd.h>\n";
    metal << "using namespace metal;\n\n";

    // Convert common types
    std::string converted = glsl;

    // Type conversions
    converted = std::regex_replace(converted, std::regex("vec2"), "float2");
    converted = std::regex_replace(converted, std::regex("vec3"), "float3");
    converted = std::regex_replace(converted, std::regex("vec4"), "float4");
    converted = std::regex_replace(converted, std::regex("mat2"), "float2x2");
    converted = std::regex_replace(converted, std::regex("mat3"), "float3x3");
    converted = std::regex_replace(converted, std::regex("mat4"), "float4x4");
    converted = std::regex_replace(converted, std::regex("ivec2"), "int2");
    converted = std::regex_replace(converted, std::regex("ivec3"), "int3");
    converted = std::regex_replace(converted, std::regex("ivec4"), "int4");
    converted = std::regex_replace(converted, std::regex("uvec2"), "uint2");
    converted = std::regex_replace(converted, std::regex("uvec3"), "uint3");
    converted = std::regex_replace(converted, std::regex("uvec4"), "uint4");

    // Function conversions
    converted = std::regex_replace(converted, std::regex("texture\\("), "textureSample(");
    converted = std::regex_replace(converted, std::regex("mix\\("), "mix(");
    converted = std::regex_replace(converted, std::regex("fract\\("), "fract(");

    // Uniform buffer to constant buffer
    converted = std::regex_replace(converted, std::regex("uniform "), "constant ");

    if (type == ShaderType::Vertex) {
        // Convert vertex shader
        converted = std::regex_replace(converted, std::regex("in "), "");
        converted = std::regex_replace(converted, std::regex("out "), "");
        converted = std::regex_replace(converted, std::regex("gl_Position"), "out.position");
        converted = std::regex_replace(converted, std::regex("void main\\(\\)"), "vertex VertexOut vertexMain(VertexIn in [[stage_in]], constant Uniforms& uniforms [[buffer(0)]])");
    } else if (type == ShaderType::Fragment) {
        // Convert fragment shader
        converted = std::regex_replace(converted, std::regex("in "), "");
        converted = std::regex_replace(converted, std::regex("out vec4 \\w+;"), "");
        converted = std::regex_replace(converted, std::regex("void main\\(\\)"), "fragment float4 fragmentMain(VertexOut in [[stage_in]], constant Uniforms& uniforms [[buffer(0)]])");
    }

    metal << converted;

    return metal.str();
}

void* MetalRenderer::LoadMetalLibrary(const std::string& path) {
    @autoreleasepool {
        if (!m_device) return nullptr;

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
        NSString* libraryPath = [NSString stringWithUTF8String:path.c_str()];

        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithFile:libraryPath error:&error];

        if (!library) {
            NSLog(@"Nova3D: Failed to load Metal library from '%s': %@",
                  path.c_str(), error.localizedDescription);
            return nullptr;
        }

        return (__bridge_retained void*)library;
    }
}

// =============================================================================
// Buffer Management
// =============================================================================

MetalBuffer MetalRenderer::CreateBuffer(const void* data, size_t size, bool isPrivate) {
    @autoreleasepool {
        MetalBuffer result;
        if (!m_device || size == 0) return result;

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;

        MTLResourceOptions options = isPrivate ? MTLResourceStorageModePrivate : MTLResourceStorageModeShared;
        id<MTLBuffer> buffer;

        if (data && !isPrivate) {
            buffer = [device newBufferWithBytes:data length:size options:options];
        } else {
            buffer = [device newBufferWithLength:size options:options];
        }

        if (buffer) {
            result.buffer = (__bridge_retained void*)buffer;
            result.size = size;
            result.isPrivate = isPrivate;
        }

        return result;
    }
}

MetalBuffer MetalRenderer::CreateBuffer(size_t size, bool isPrivate) {
    return CreateBuffer(nullptr, size, isPrivate);
}

void MetalRenderer::UpdateBuffer(MetalBuffer& buffer, const void* data, size_t size, size_t offset) {
    @autoreleasepool {
        if (!buffer.buffer || !data || buffer.isPrivate) return;

        id<MTLBuffer> mtlBuffer = (__bridge id<MTLBuffer>)buffer.buffer;
        uint8_t* contents = (uint8_t*)[mtlBuffer contents];
        if (contents && offset + size <= buffer.size) {
            memcpy(contents + offset, data, size);
        }
    }
}

void MetalRenderer::DestroyBuffer(MetalBuffer& buffer) {
    if (buffer.buffer) {
        CFRelease(buffer.buffer);
        buffer.buffer = nullptr;
        buffer.size = 0;
    }
}

// =============================================================================
// Texture Management
// =============================================================================

MetalTexture MetalRenderer::CreateTexture2D(int width, int height, int format, const void* data) {
    @autoreleasepool {
        MetalTexture result;
        if (!m_device) return result;

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:(MTLPixelFormat)format
                                                                                        width:width
                                                                                       height:height
                                                                                    mipmapped:NO];

        id<MTLTexture> texture = [device newTextureWithDescriptor:desc];

        if (texture) {
            if (data) {
                MTLRegion region = MTLRegionMake2D(0, 0, width, height);
                NSUInteger bytesPerRow = width * 4; // Assuming RGBA8
                [texture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
            }

            result.texture = (__bridge_retained void*)texture;
            result.width = width;
            result.height = height;
            result.format = format;
        }

        return result;
    }
}

void MetalRenderer::UpdateTexture(MetalTexture& texture, const void* data, int width, int height) {
    @autoreleasepool {
        if (!texture.texture || !data) return;

        id<MTLTexture> mtlTexture = (__bridge id<MTLTexture>)texture.texture;
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        NSUInteger bytesPerRow = width * 4;
        [mtlTexture replaceRegion:region mipmapLevel:0 withBytes:data bytesPerRow:bytesPerRow];
    }
}

void MetalRenderer::DestroyTexture(MetalTexture& texture) {
    if (texture.texture) {
        CFRelease(texture.texture);
        texture.texture = nullptr;
    }
}

// =============================================================================
// Frame Rendering
// =============================================================================

void MetalRenderer::SetDrawable(void* drawable) {
    m_currentDrawable = drawable;
}

void MetalRenderer::BeginFrame() {
    @autoreleasepool {
        if (!m_commandQueue || !m_currentDrawable) return;

        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)m_commandQueue;
        id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)m_currentDrawable;

        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
        commandBuffer.label = @"Nova3D Frame";
        m_currentCommandBuffer = (__bridge_retained void*)commandBuffer;

        // Create render pass descriptor
        MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        passDesc.colorAttachments[0].texture = drawable.texture;
        passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.15, 1.0);

        // Create depth texture if needed
        int width = (int)drawable.texture.width;
        int height = (int)drawable.texture.height;
        if (m_framebufferWidth != width || m_framebufferHeight != height) {
            CreateDepthTexture(width, height);
            m_framebufferWidth = width;
            m_framebufferHeight = height;
        }

        if (m_depthTexture) {
            passDesc.depthAttachment.texture = (__bridge id<MTLTexture>)m_depthTexture;
            passDesc.depthAttachment.loadAction = MTLLoadActionClear;
            passDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
            passDesc.depthAttachment.clearDepth = 1.0;
        }

        // Create render encoder
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDesc];
        encoder.label = @"Nova3D Render Encoder";
        m_currentRenderEncoder = (__bridge_retained void*)encoder;
    }
}

void MetalRenderer::EndFrame() {
    @autoreleasepool {
        if (!m_currentRenderEncoder || !m_currentCommandBuffer || !m_currentDrawable) return;

        id<MTLRenderCommandEncoder> encoder = (__bridge_transfer id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        id<MTLCommandBuffer> commandBuffer = (__bridge_transfer id<MTLCommandBuffer>)m_currentCommandBuffer;
        id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)m_currentDrawable;

        [encoder endEncoding];
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];

        m_currentRenderEncoder = nullptr;
        m_currentCommandBuffer = nullptr;
    }
}

// =============================================================================
// Drawing
// =============================================================================

void MetalRenderer::SetPipeline(const std::string& name) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;

        MetalPipeline* pipeline = GetPipeline(name);
        if (!pipeline) return;

        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;

        if (pipeline->pipelineState) {
            [encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)pipeline->pipelineState];
        }
        if (pipeline->depthState) {
            [encoder setDepthStencilState:(__bridge id<MTLDepthStencilState>)pipeline->depthState];
        }
    }
}

void MetalRenderer::SetVertexBuffer(const MetalBuffer& buffer, size_t offset, int index) {
    @autoreleasepool {
        if (!m_currentRenderEncoder || !buffer.buffer) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setVertexBuffer:(__bridge id<MTLBuffer>)buffer.buffer offset:offset atIndex:index];
    }
}

void MetalRenderer::SetVertexBytes(const void* data, size_t size, int index) {
    @autoreleasepool {
        if (!m_currentRenderEncoder || !data) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setVertexBytes:data length:size atIndex:index];
    }
}

void MetalRenderer::SetFragmentBuffer(const MetalBuffer& buffer, size_t offset, int index) {
    @autoreleasepool {
        if (!m_currentRenderEncoder || !buffer.buffer) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setFragmentBuffer:(__bridge id<MTLBuffer>)buffer.buffer offset:offset atIndex:index];
    }
}

void MetalRenderer::SetFragmentTexture(const MetalTexture& texture, int index) {
    @autoreleasepool {
        if (!m_currentRenderEncoder || !texture.texture) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setFragmentTexture:(__bridge id<MTLTexture>)texture.texture atIndex:index];
    }
}

void MetalRenderer::Draw(int primitiveType, int vertexStart, int vertexCount) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:vertexStart vertexCount:vertexCount];
    }
}

void MetalRenderer::DrawIndexed(int primitiveType, int indexCount, int indexType,
                                 const MetalBuffer& indexBuffer, size_t indexBufferOffset) {
    @autoreleasepool {
        if (!m_currentRenderEncoder || !indexBuffer.buffer) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder drawIndexedPrimitives:(MTLPrimitiveType)primitiveType
                            indexCount:indexCount
                             indexType:(MTLIndexType)indexType
                           indexBuffer:(__bridge id<MTLBuffer>)indexBuffer.buffer
                     indexBufferOffset:indexBufferOffset];
    }
}

void MetalRenderer::DrawInstanced(int primitiveType, int vertexStart, int vertexCount, int instanceCount) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder drawPrimitives:(MTLPrimitiveType)primitiveType
                    vertexStart:vertexStart
                    vertexCount:vertexCount
                  instanceCount:instanceCount];
    }
}

// =============================================================================
// State
// =============================================================================

void MetalRenderer::SetViewport(float x, float y, float width, float height,
                                 float nearDepth, float farDepth) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        MTLViewport viewport = { x, y, width, height, nearDepth, farDepth };
        [encoder setViewport:viewport];
    }
}

void MetalRenderer::SetScissorRect(int x, int y, int width, int height) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        MTLScissorRect rect = { (NSUInteger)x, (NSUInteger)y, (NSUInteger)width, (NSUInteger)height };
        [encoder setScissorRect:rect];
    }
}

void MetalRenderer::SetCullMode(int mode) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setCullMode:(MTLCullMode)mode];
    }
}

void MetalRenderer::SetFrontFace(int winding) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setFrontFacingWinding:(MTLWinding)winding];
    }
}

void MetalRenderer::SetDepthBias(float depthBias, float slopeScale, float clamp) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setDepthBias:depthBias slopeScale:slopeScale clamp:clamp];
    }
}

void MetalRenderer::SetBlendColor(float r, float g, float b, float a) {
    @autoreleasepool {
        if (!m_currentRenderEncoder) return;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)m_currentRenderEncoder;
        [encoder setBlendColorRed:r green:g blue:b alpha:a];
    }
}

// =============================================================================
// Debug
// =============================================================================

std::string MetalRenderer::GetDeviceName() const {
    @autoreleasepool {
        if (!m_device) return "Unknown";
        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
        return std::string([device.name UTF8String]);
    }
}

bool MetalRenderer::SupportsFeatureSet(int featureSet) const {
    @autoreleasepool {
        if (!m_device) return false;
        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
        return [device supportsFamily:(MTLGPUFamily)featureSet];
    }
}

int MetalRenderer::GetMaxTextureSize() const {
    // iOS devices typically support up to 16384x16384
    return 16384;
}

size_t MetalRenderer::GetMaxBufferSize() const {
    @autoreleasepool {
        if (!m_device) return 0;
        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;
        return device.maxBufferLength;
    }
}

// =============================================================================
// Helpers
// =============================================================================

void MetalRenderer::CreateDepthTexture(int width, int height) {
    @autoreleasepool {
        if (!m_device) return;

        // Release old depth texture
        if (m_depthTexture) {
            CFRelease(m_depthTexture);
            m_depthTexture = nullptr;
        }

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                        width:width
                                                                                       height:height
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;

        id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
        if (texture) {
            m_depthTexture = (__bridge_retained void*)texture;
        }
    }
}

void* MetalRenderer::CreateDepthStencilState(bool depthWrite, int compareFunc) {
    @autoreleasepool {
        if (!m_device) return nullptr;

        id<MTLDevice> device = (__bridge id<MTLDevice>)m_device;

        MTLDepthStencilDescriptor* desc = [[MTLDepthStencilDescriptor alloc] init];
        desc.depthCompareFunction = (MTLCompareFunction)compareFunc;
        desc.depthWriteEnabled = depthWrite;

        id<MTLDepthStencilState> state = [device newDepthStencilStateWithDescriptor:desc];
        return (__bridge_retained void*)state;
    }
}

} // namespace Nova
