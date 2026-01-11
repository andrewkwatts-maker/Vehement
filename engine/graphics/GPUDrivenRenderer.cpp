#include "GPUDrivenRenderer.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>

#include <glad/gl.h>

// Platform-specific OpenGL headers
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

namespace Engine {
namespace Graphics {

// OpenGL function pointers (would normally be loaded via GLAD/GLEW)
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

// ============================================================================
// GPUBuffer Implementation
// ============================================================================

GPUBuffer::GPUBuffer(Type type, Usage usage)
    : m_handle(0)
    , m_type(type)
    , m_usage(usage)
    , m_size(0)
    , m_mappedPointer(nullptr) {

    glGenBuffers(1, &m_handle);
}

GPUBuffer::~GPUBuffer() {
    if (m_mappedPointer) {
        Unmap();
    }
    if (m_handle) {
        glDeleteBuffers(1, &m_handle);
    }
}

void GPUBuffer::Allocate(size_t size) {
    m_size = size;

    GLenum target = GL_ARRAY_BUFFER;
    switch (m_type) {
        case Type::Vertex: target = GL_ARRAY_BUFFER; break;
        case Type::Index: target = GL_ELEMENT_ARRAY_BUFFER; break;
        case Type::Uniform: target = GL_UNIFORM_BUFFER; break;
        case Type::ShaderStorage: target = GL_SHADER_STORAGE_BUFFER; break;
        case Type::Indirect: target = GL_DRAW_INDIRECT_BUFFER; break;
        case Type::AtomicCounter: target = GL_ATOMIC_COUNTER_BUFFER; break;
    }

    GLenum usageHint = GL_STATIC_DRAW;
    switch (m_usage) {
        case Usage::Static: usageHint = GL_STATIC_DRAW; break;
        case Usage::Dynamic: usageHint = GL_DYNAMIC_DRAW; break;
        case Usage::Stream: usageHint = GL_STREAM_DRAW; break;
    }

    glBindBuffer(target, m_handle);
    glBufferData(target, size, nullptr, usageHint);
    glBindBuffer(target, 0);
}

void GPUBuffer::Upload(const void* data, size_t size, size_t offset) {
    GLenum target = GL_ARRAY_BUFFER;
    switch (m_type) {
        case Type::Vertex: target = GL_ARRAY_BUFFER; break;
        case Type::Index: target = GL_ELEMENT_ARRAY_BUFFER; break;
        case Type::Uniform: target = GL_UNIFORM_BUFFER; break;
        case Type::ShaderStorage: target = GL_SHADER_STORAGE_BUFFER; break;
        case Type::Indirect: target = GL_DRAW_INDIRECT_BUFFER; break;
        case Type::AtomicCounter: target = GL_ATOMIC_COUNTER_BUFFER; break;
    }

    glBindBuffer(target, m_handle);
    glBufferSubData(target, offset, size, data);
    glBindBuffer(target, 0);
}

void* GPUBuffer::Map(size_t offset, size_t size) {
    if (m_mappedPointer) {
        return m_mappedPointer;
    }

    GLenum target = GL_SHADER_STORAGE_BUFFER;
    switch (m_type) {
        case Type::ShaderStorage: target = GL_SHADER_STORAGE_BUFFER; break;
        case Type::Indirect: target = GL_DRAW_INDIRECT_BUFFER; break;
        default: target = GL_ARRAY_BUFFER; break;
    }

    if (size == 0) {
        size = m_size - offset;
    }

    glBindBuffer(target, m_handle);
    m_mappedPointer = glMapBufferRange(target, offset, size,
                                       GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    glBindBuffer(target, 0);

    return m_mappedPointer;
}

void GPUBuffer::Unmap() {
    if (!m_mappedPointer) {
        return;
    }

    GLenum target = GL_SHADER_STORAGE_BUFFER;
    switch (m_type) {
        case Type::ShaderStorage: target = GL_SHADER_STORAGE_BUFFER; break;
        case Type::Indirect: target = GL_DRAW_INDIRECT_BUFFER; break;
        default: target = GL_ARRAY_BUFFER; break;
    }

    glBindBuffer(target, m_handle);
    glUnmapBuffer(target);
    glBindBuffer(target, 0);

    m_mappedPointer = nullptr;
}

void GPUBuffer::Bind() const {
    GLenum target = GL_ARRAY_BUFFER;
    switch (m_type) {
        case Type::Vertex: target = GL_ARRAY_BUFFER; break;
        case Type::Index: target = GL_ELEMENT_ARRAY_BUFFER; break;
        case Type::Uniform: target = GL_UNIFORM_BUFFER; break;
        case Type::ShaderStorage: target = GL_SHADER_STORAGE_BUFFER; break;
        case Type::Indirect: target = GL_DRAW_INDIRECT_BUFFER; break;
        case Type::AtomicCounter: target = GL_ATOMIC_COUNTER_BUFFER; break;
    }

    glBindBuffer(target, m_handle);
}

void GPUBuffer::BindBase(uint32_t bindingPoint) const {
    GLenum target = GL_SHADER_STORAGE_BUFFER;
    switch (m_type) {
        case Type::Uniform: target = GL_UNIFORM_BUFFER; break;
        case Type::ShaderStorage: target = GL_SHADER_STORAGE_BUFFER; break;
        case Type::AtomicCounter: target = GL_ATOMIC_COUNTER_BUFFER; break;
        default: return;
    }

    glBindBufferBase(target, bindingPoint, m_handle);
}

// ============================================================================
// ComputeShader Implementation
// ============================================================================

ComputeShader::ComputeShader()
    : m_program(0)
    , m_shader(0) {
}

ComputeShader::~ComputeShader() {
    if (m_program) {
        glDeleteProgram(m_program);
    }
    if (m_shader) {
        glDeleteShader(m_shader);
    }
}

bool ComputeShader::LoadFromFile(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    return LoadFromSource(source.c_str());
}

bool ComputeShader::LoadFromSource(const char* source) {
    m_shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(m_shader, 1, &source, nullptr);
    glCompileShader(m_shader);

    // Check compilation
    GLint success;
    glGetShaderiv(m_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(m_shader, 512, nullptr, infoLog);
        // Log error
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, m_shader);
    glLinkProgram(m_program);

    // Check linking
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_program, 512, nullptr, infoLog);
        // Log error
        return false;
    }

    return true;
}

void ComputeShader::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
    glUseProgram(m_program);
    glDispatchCompute(groupsX, groupsY, groupsZ);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void ComputeShader::DispatchIndirect(const GPUBuffer& commandBuffer, size_t offset) {
    glUseProgram(m_program);
    commandBuffer.Bind();
    glDispatchComputeIndirect(static_cast<GLintptr>(offset));
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
}

void ComputeShader::SetUniform(const char* name, int value) {
    GLint location = glGetUniformLocation(m_program, name);
    if (location >= 0) {
        glUseProgram(m_program);
        glUniform1i(location, value);
    }
}

void ComputeShader::SetUniform(const char* name, float value) {
    GLint location = glGetUniformLocation(m_program, name);
    if (location >= 0) {
        glUseProgram(m_program);
        glUniform1f(location, value);
    }
}

void ComputeShader::SetUniform(const char* name, const Vector3& value) {
    GLint location = glGetUniformLocation(m_program, name);
    if (location >= 0) {
        glUseProgram(m_program);
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void ComputeShader::SetUniform(const char* name, const Matrix4& value) {
    GLint location = glGetUniformLocation(m_program, name);
    if (location >= 0) {
        glUseProgram(m_program);
        glUniformMatrix4fv(location, 1, GL_FALSE, value.m);
    }
}

void ComputeShader::Bind() const {
    glUseProgram(m_program);
}

// ============================================================================
// GPUDrivenRenderer Implementation
// ============================================================================

GPUDrivenRenderer::GPUDrivenRenderer(const Config& config)
    : m_config(config)
    , m_instanceCount(0)
    , m_drawCommandCount(0)
    , m_queryObject(0)
    , m_gpuCullingTimeMs(0.0f)
    , m_persistentInstancePtr(nullptr)
    , m_persistentCommandPtr(nullptr)
    , m_frameIndex(0) {
}

GPUDrivenRenderer::~GPUDrivenRenderer() {
    if (m_queryObject) {
        glDeleteQueries(1, &m_queryObject);
    }
}

bool GPUDrivenRenderer::Initialize() {
    if (!CreateBuffers()) {
        return false;
    }

    if (!LoadShaders()) {
        return false;
    }

    // Create GPU query for timing
    glGenQueries(1, &m_queryObject);

    // Reserve instance data
    m_instanceData.reserve(m_config.maxInstances);

    return true;
}

bool GPUDrivenRenderer::CreateBuffers() {
    // Instance buffer
    m_instanceBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_instanceBuffer->Allocate(m_config.maxInstances * sizeof(GPUInstanceData));

    // Visible instance buffer (post-culling)
    m_visibleInstanceBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::ShaderStorage,
        GPUBuffer::Usage::Dynamic
    );
    m_visibleInstanceBuffer->Allocate(m_config.maxInstances * sizeof(uint32_t));

    // Draw command buffer
    m_drawCommandBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::Indirect,
        GPUBuffer::Usage::Dynamic
    );
    m_drawCommandBuffer->Allocate(m_config.maxDrawCommands * sizeof(DrawElementsIndirectCommand));

    // Frustum plane buffer
    m_frustumPlaneBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::Uniform,
        GPUBuffer::Usage::Dynamic
    );
    m_frustumPlaneBuffer->Allocate(6 * sizeof(Vector4));

    // Counter buffer for atomic operations
    m_counterBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::AtomicCounter,
        GPUBuffer::Usage::Dynamic
    );
    m_counterBuffer->Allocate(16 * sizeof(uint32_t));

    // Setup persistent mapping if enabled
    if (m_config.enablePersistentMapping) {
        m_persistentInstancePtr = m_instanceBuffer->Map();
        m_persistentCommandPtr = m_drawCommandBuffer->Map();
    }

    m_stats.instanceBufferSize = m_instanceBuffer->GetSize();
    m_stats.commandBufferSize = m_drawCommandBuffer->GetSize();

    return true;
}

bool GPUDrivenRenderer::LoadShaders() {
    // Load culling compute shader
    m_cullingShader = std::make_unique<ComputeShader>();
    if (!m_cullingShader->LoadFromFile("assets/shaders/gpu_cull_sdf.comp")) {
        // Try fallback inline shader
        const char* fallbackSource = R"(
            #version 450 core
            layout(local_size_x = 256) in;

            struct Instance {
                mat4 transform;
                vec4 boundingSphere;
                uint materialID;
                uint lodLevel;
                uint instanceID;
                uint flags;
            };

            layout(std430, binding = 0) readonly buffer InstanceBuffer {
                Instance instances[];
            };

            layout(std430, binding = 1) writeonly buffer VisibleBuffer {
                uint visibleIndices[];
            };

            layout(binding = 0) uniform FrustumPlanes {
                vec4 planes[6];
            };

            layout(binding = 0) uniform atomic_uint visibleCount;

            void main() {
                uint idx = gl_GlobalInvocationID.x;
                if (idx >= instances.length()) return;

                Instance inst = instances[idx];
                vec3 center = (inst.transform * vec4(inst.boundingSphere.xyz, 1.0)).xyz;
                float radius = inst.boundingSphere.w;

                bool visible = true;
                for (int i = 0; i < 6; i++) {
                    if (dot(planes[i].xyz, center) + planes[i].w < -radius) {
                        visible = false;
                        break;
                    }
                }

                if (visible) {
                    uint visIdx = atomicCounterIncrement(visibleCount);
                    visibleIndices[visIdx] = idx;
                }
            }
        )";
        m_cullingShader->LoadFromSource(fallbackSource);
    }

    // Load compaction shader
    m_compactionShader = std::make_unique<ComputeShader>();

    return true;
}

void GPUDrivenRenderer::UpdateInstances(const std::vector<SDFInstance>& instances) {
    auto startTime = std::chrono::high_resolution_clock::now();

    m_instanceCount = static_cast<uint32_t>(instances.size());
    m_instanceData.resize(m_instanceCount);

    // Convert to GPU format
    for (size_t i = 0; i < instances.size(); i++) {
        const SDFInstance& src = instances[i];
        GPUInstanceData& dst = m_instanceData[i];

        dst.transform = src.transform;
        dst.boundingSphere = Vector4(
            src.boundingSphereCenter.x,
            src.boundingSphereCenter.y,
            src.boundingSphereCenter.z,
            src.boundingSphereRadius
        );
        dst.materialID = src.materialID;
        dst.lodLevel = src.lodLevel;
        dst.instanceID = src.instanceID;
        dst.flags = 0;
    }

    // Upload to GPU
    if (m_config.enablePersistentMapping && m_persistentInstancePtr) {
        std::memcpy(m_persistentInstancePtr, m_instanceData.data(),
                   m_instanceCount * sizeof(GPUInstanceData));
    } else {
        m_instanceBuffer->Upload(m_instanceData.data(),
                                m_instanceCount * sizeof(GPUInstanceData));
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.uploadTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_stats.totalInstances = m_instanceCount;
}

void GPUDrivenRenderer::UpdateFrustumPlanes(const Frustum& frustum) {
    m_frustumPlaneBuffer->Upload(frustum.planes, 6 * sizeof(Vector4));
}

void GPUDrivenRenderer::CullAndGenerateDrawCommands(const CullingCamera& camera) {
    if (!m_config.enableGPUCulling || !m_cullingShader) {
        return;
    }

    // Update frustum planes
    UpdateFrustumPlanes(camera.frustum);

    // Reset counter
    uint32_t zero = 0;
    m_counterBuffer->Upload(&zero, sizeof(uint32_t));

    // Bind buffers
    m_instanceBuffer->BindBase(0);
    m_visibleInstanceBuffer->BindBase(1);
    m_frustumPlaneBuffer->BindBase(0);
    m_counterBuffer->BindBase(0);

    // Start GPU timer
    glBeginQuery(GL_TIME_ELAPSED, m_queryObject);

    // Dispatch culling compute shader
    uint32_t numGroups = (m_instanceCount + m_config.cullingThreadGroupSize - 1) /
                         m_config.cullingThreadGroupSize;

    m_cullingShader->SetUniform("u_instanceCount", static_cast<int>(m_instanceCount));
    m_cullingShader->Dispatch(numGroups, 1, 1);

    // End GPU timer
    glEndQuery(GL_TIME_ELAPSED);

    // Read back visible count (could be done asynchronously)
    uint32_t visibleCount = 0;
    void* counterPtr = m_counterBuffer->Map(0, sizeof(uint32_t));
    if (counterPtr) {
        std::memcpy(&visibleCount, counterPtr, sizeof(uint32_t));
        m_counterBuffer->Unmap();
    }

    m_stats.visibleInstances = visibleCount;
}

void GPUDrivenRenderer::ExecuteIndirectDraws() {
    if (m_drawCommandCount == 0) {
        return;
    }

    m_drawCommandBuffer->Bind();

    // Execute indirect draws
    // This would be called per material batch in practice
    glMultiDrawElementsIndirect(
        GL_TRIANGLES,
        GL_UNSIGNED_INT,
        nullptr,
        m_drawCommandCount,
        0
    );

    m_stats.drawCallCount = m_drawCommandCount;
}

void GPUDrivenRenderer::ExecuteIndirectDrawBatch(uint32_t batchIndex) {
    if (batchIndex >= m_drawCommandCount) {
        return;
    }

    m_drawCommandBuffer->Bind();

    // Draw single batch
    glDrawElementsIndirect(
        GL_TRIANGLES,
        GL_UNSIGNED_INT,
        reinterpret_cast<void*>(batchIndex * sizeof(DrawElementsIndirectCommand))
    );
}

void GPUDrivenRenderer::CompactInstanceBuffer() {
    // Use compute shader to compact visible instances
    // This creates a tightly packed array of visible instances
    if (m_compactionShader) {
        m_visibleInstanceBuffer->BindBase(0);
        m_instanceBuffer->BindBase(1);

        uint32_t numGroups = (m_stats.visibleInstances + 255) / 256;
        m_compactionShader->Dispatch(numGroups, 1, 1);
    }
}

void GPUDrivenRenderer::ReadQueryResults() {
    GLint available = 0;
    glGetQueryObjectiv(m_queryObject, GL_QUERY_RESULT_AVAILABLE, &available);

    if (available) {
        GLuint64 timeElapsed = 0;
        glGetQueryObjectui64v(m_queryObject, GL_QUERY_RESULT, &timeElapsed);
        m_gpuCullingTimeMs = timeElapsed / 1000000.0f; // Convert to milliseconds
        m_stats.gpuCullingTimeMs = m_gpuCullingTimeMs;
    }
}

void GPUDrivenRenderer::Clear() {
    m_instanceCount = 0;
    m_drawCommandCount = 0;
    m_instanceData.clear();
}

void GPUDrivenRenderer::ResetStats() {
    m_stats = Stats();
}

// ============================================================================
// MultiDrawIndirectRenderer Implementation
// ============================================================================

MultiDrawIndirectRenderer::MultiDrawIndirectRenderer() {
    m_commandBuffer = std::make_unique<GPUBuffer>(
        GPUBuffer::Type::Indirect,
        GPUBuffer::Usage::Dynamic
    );
    m_commandBuffer->Allocate(10000 * sizeof(DrawElementsIndirectCommand));
}

MultiDrawIndirectRenderer::~MultiDrawIndirectRenderer() = default;

void MultiDrawIndirectRenderer::AddDrawCommand(const DrawElementsIndirectCommand& command) {
    m_drawCommands.push_back(command);
}

void MultiDrawIndirectRenderer::ExecuteMultiDraw() {
    if (m_drawCommands.empty()) {
        return;
    }

    // Upload commands
    m_commandBuffer->Upload(m_drawCommands.data(),
                           m_drawCommands.size() * sizeof(DrawElementsIndirectCommand));

    m_commandBuffer->Bind();

    // Execute multi-draw indirect
    glMultiDrawElementsIndirect(
        GL_TRIANGLES,
        GL_UNSIGNED_INT,
        nullptr,
        static_cast<GLsizei>(m_drawCommands.size()),
        0
    );
}

void MultiDrawIndirectRenderer::Clear() {
    m_drawCommands.clear();
}

// ============================================================================
// OcclusionCuller Implementation
// ============================================================================

OcclusionCuller::OcclusionCuller()
    : m_hiZTexture(0)
    , m_hiZFBO(0)
    , m_width(0)
    , m_height(0)
    , m_mipLevels(0) {
}

OcclusionCuller::~OcclusionCuller() {
    if (m_hiZTexture) {
        glDeleteTextures(1, &m_hiZTexture);
    }
    if (m_hiZFBO) {
        glDeleteFramebuffers(1, &m_hiZFBO);
    }
}

bool OcclusionCuller::Initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    // Calculate mip levels
    m_mipLevels = 1;
    uint32_t size = std::max(width, height);
    while (size > 1) {
        size >>= 1;
        m_mipLevels++;
    }

    // Create Hi-Z texture
    glGenTextures(1, &m_hiZTexture);
    glBindTexture(GL_TEXTURE_2D, m_hiZTexture);
    glTexStorage2D(GL_TEXTURE_2D, m_mipLevels, GL_R32F, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create framebuffer
    glGenFramebuffers(1, &m_hiZFBO);

    // Load shaders
    m_hiZShader = std::make_unique<ComputeShader>();
    m_occlusionShader = std::make_unique<ComputeShader>();

    return true;
}

void OcclusionCuller::GenerateHiZ(uint32_t depthTexture) {
    // Copy depth to mip 0
    glBindFramebuffer(GL_FRAMEBUFFER, m_hiZFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hiZTexture, 0);

    // Blit depth to Hi-Z mip 0
    // ... (implementation details)

    // Generate mipmap chain using compute shader
    if (m_hiZShader) {
        for (uint32_t i = 1; i < m_mipLevels; i++) {
            uint32_t mipWidth = std::max(1u, m_width >> i);
            uint32_t mipHeight = std::max(1u, m_height >> i);

            m_hiZShader->SetUniform("u_srcMip", static_cast<int>(i - 1));
            m_hiZShader->SetUniform("u_dstMip", static_cast<int>(i));

            uint32_t groupsX = (mipWidth + 15) / 16;
            uint32_t groupsY = (mipHeight + 15) / 16;
            m_hiZShader->Dispatch(groupsX, groupsY, 1);
        }
    }
}

void OcclusionCuller::CullOccluded(
    GPUBuffer* instanceBuffer,
    GPUBuffer* visibleBuffer,
    uint32_t instanceCount) {

    if (!m_occlusionShader) {
        return;
    }

    instanceBuffer->BindBase(0);
    visibleBuffer->BindBase(1);

    m_occlusionShader->SetUniform("u_instanceCount", static_cast<int>(instanceCount));
    m_occlusionShader->SetUniform("u_hiZTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hiZTexture);

    uint32_t numGroups = (instanceCount + 255) / 256;
    m_occlusionShader->Dispatch(numGroups, 1, 1);
}

} // namespace Graphics
} // namespace Engine
