#include "GBuffer.hpp"
#include "Shader.hpp"
#include <glad/glad.h>
#include <iostream>
#include <array>

namespace Nova {

// ============================================================================
// Construction / Destruction
// ============================================================================

GBuffer::GBuffer() = default;

GBuffer::~GBuffer() {
    Cleanup();
}

GBuffer::GBuffer(GBuffer&& other) noexcept
    : m_config(other.m_config)
    , m_fbo(other.m_fbo)
    , m_positionTexture(other.m_positionTexture)
    , m_normalTexture(other.m_normalTexture)
    , m_albedoTexture(other.m_albedoTexture)
    , m_materialTexture(other.m_materialTexture)
    , m_emissionTexture(other.m_emissionTexture)
    , m_velocityTexture(other.m_velocityTexture)
    , m_depthTexture(other.m_depthTexture)
    , m_attachmentCount(other.m_attachmentCount)
    , m_isValid(other.m_isValid)
    , m_debugShader(std::move(other.m_debugShader))
    , m_debugQuadVAO(other.m_debugQuadVAO)
    , m_debugQuadVBO(other.m_debugQuadVBO)
{
    other.m_fbo = 0;
    other.m_positionTexture = 0;
    other.m_normalTexture = 0;
    other.m_albedoTexture = 0;
    other.m_materialTexture = 0;
    other.m_emissionTexture = 0;
    other.m_velocityTexture = 0;
    other.m_depthTexture = 0;
    other.m_debugQuadVAO = 0;
    other.m_debugQuadVBO = 0;
    other.m_isValid = false;
}

GBuffer& GBuffer::operator=(GBuffer&& other) noexcept {
    if (this != &other) {
        Cleanup();

        m_config = other.m_config;
        m_fbo = other.m_fbo;
        m_positionTexture = other.m_positionTexture;
        m_normalTexture = other.m_normalTexture;
        m_albedoTexture = other.m_albedoTexture;
        m_materialTexture = other.m_materialTexture;
        m_emissionTexture = other.m_emissionTexture;
        m_velocityTexture = other.m_velocityTexture;
        m_depthTexture = other.m_depthTexture;
        m_attachmentCount = other.m_attachmentCount;
        m_isValid = other.m_isValid;
        m_debugShader = std::move(other.m_debugShader);
        m_debugQuadVAO = other.m_debugQuadVAO;
        m_debugQuadVBO = other.m_debugQuadVBO;

        other.m_fbo = 0;
        other.m_positionTexture = 0;
        other.m_normalTexture = 0;
        other.m_albedoTexture = 0;
        other.m_materialTexture = 0;
        other.m_emissionTexture = 0;
        other.m_velocityTexture = 0;
        other.m_depthTexture = 0;
        other.m_debugQuadVAO = 0;
        other.m_debugQuadVBO = 0;
        other.m_isValid = false;
    }
    return *this;
}

// ============================================================================
// Initialization
// ============================================================================

bool GBuffer::Create(const GBufferConfig& config) {
    // Clean up existing resources
    Cleanup();

    m_config = config;

    // Validate dimensions
    if (m_config.width <= 0 || m_config.height <= 0) {
        std::cerr << "[GBuffer] Invalid dimensions: " << m_config.width << "x" << m_config.height << std::endl;
        return false;
    }

    // Create textures and framebuffer
    if (!CreateTextures()) {
        std::cerr << "[GBuffer] Failed to create textures" << std::endl;
        Cleanup();
        return false;
    }

    if (!CreateFramebuffer()) {
        std::cerr << "[GBuffer] Failed to create framebuffer" << std::endl;
        Cleanup();
        return false;
    }

    m_isValid = true;
    std::cout << "[GBuffer] Created " << m_config.width << "x" << m_config.height
              << " with " << m_attachmentCount << " attachments" << std::endl;

    return true;
}

bool GBuffer::Create(int width, int height) {
    GBufferConfig config = GBufferConfig::Default();
    config.width = width;
    config.height = height;
    return Create(config);
}

void GBuffer::Resize(int width, int height) {
    if (width == m_config.width && height == m_config.height) {
        return;
    }

    m_config.width = width;
    m_config.height = height;

    // Recreate with new dimensions
    Create(m_config);
}

void GBuffer::Cleanup() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }

    // Delete textures
    std::array<uint32_t*, 7> textures = {
        &m_positionTexture,
        &m_normalTexture,
        &m_albedoTexture,
        &m_materialTexture,
        &m_emissionTexture,
        &m_velocityTexture,
        &m_depthTexture
    };

    for (auto* tex : textures) {
        if (*tex) {
            glDeleteTextures(1, tex);
            *tex = 0;
        }
    }

    // Debug resources
    if (m_debugQuadVAO) {
        glDeleteVertexArrays(1, &m_debugQuadVAO);
        m_debugQuadVAO = 0;
    }
    if (m_debugQuadVBO) {
        glDeleteBuffers(1, &m_debugQuadVBO);
        m_debugQuadVBO = 0;
    }

    m_isValid = false;
    m_attachmentCount = 0;
}

bool GBuffer::IsValid() const {
    return m_isValid && m_fbo != 0;
}

// ============================================================================
// Texture Creation
// ============================================================================

bool GBuffer::CreateTextures() {
    const int w = m_config.width;
    const int h = m_config.height;

    // Position texture (world-space position + linear depth)
    glCreateTextures(GL_TEXTURE_2D, 1, &m_positionTexture);
    glTextureStorage2D(m_positionTexture, 1, GetPositionFormat(), w, h);
    glTextureParameteri(m_positionTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_positionTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_positionTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_positionTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Normal texture (world-space normal)
    glCreateTextures(GL_TEXTURE_2D, 1, &m_normalTexture);
    glTextureStorage2D(m_normalTexture, 1, GetNormalFormat(), w, h);
    glTextureParameteri(m_normalTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_normalTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_normalTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_normalTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Albedo texture (diffuse color + alpha)
    glCreateTextures(GL_TEXTURE_2D, 1, &m_albedoTexture);
    glTextureStorage2D(m_albedoTexture, 1, GetAlbedoFormat(), w, h);
    glTextureParameteri(m_albedoTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_albedoTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_albedoTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_albedoTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Material parameters texture (metallic, roughness, AO, materialID)
    glCreateTextures(GL_TEXTURE_2D, 1, &m_materialTexture);
    glTextureStorage2D(m_materialTexture, 1, GetMaterialFormat(), w, h);
    glTextureParameteri(m_materialTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_materialTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_materialTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_materialTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Optional: Emission texture
    if (m_config.enableEmission) {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_emissionTexture);
        glTextureStorage2D(m_emissionTexture, 1, GetEmissionFormat(), w, h);
        glTextureParameteri(m_emissionTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(m_emissionTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_emissionTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_emissionTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Optional: Velocity texture (for TAA / motion blur)
    if (m_config.enableVelocity) {
        glCreateTextures(GL_TEXTURE_2D, 1, &m_velocityTexture);
        glTextureStorage2D(m_velocityTexture, 1, GetVelocityFormat(), w, h);
        glTextureParameteri(m_velocityTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(m_velocityTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_velocityTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_velocityTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Depth texture
    glCreateTextures(GL_TEXTURE_2D, 1, &m_depthTexture);
    glTextureStorage2D(m_depthTexture, 1, GL_DEPTH_COMPONENT32F, w, h);
    glTextureParameteri(m_depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(m_depthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_depthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_depthTexture, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    return true;
}

bool GBuffer::CreateFramebuffer() {
    // Create framebuffer using DSA
    glCreateFramebuffers(1, &m_fbo);

    // Attach color textures
    std::vector<GLenum> drawBuffers;

    // Position (attachment 0)
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, m_positionTexture, 0);
    drawBuffers.push_back(GL_COLOR_ATTACHMENT0);

    // Normal (attachment 1)
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT1, m_normalTexture, 0);
    drawBuffers.push_back(GL_COLOR_ATTACHMENT1);

    // Albedo (attachment 2)
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT2, m_albedoTexture, 0);
    drawBuffers.push_back(GL_COLOR_ATTACHMENT2);

    // Material (attachment 3)
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT3, m_materialTexture, 0);
    drawBuffers.push_back(GL_COLOR_ATTACHMENT3);

    // Optional: Emission (attachment 4)
    if (m_config.enableEmission && m_emissionTexture) {
        glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT4, m_emissionTexture, 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT4);
    }

    // Optional: Velocity (attachment 5)
    if (m_config.enableVelocity && m_velocityTexture) {
        glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT5, m_velocityTexture, 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT5);
    }

    // Depth attachment
    glNamedFramebufferTexture(m_fbo, GL_DEPTH_ATTACHMENT, m_depthTexture, 0);

    // Set draw buffers
    m_attachmentCount = static_cast<int>(drawBuffers.size());
    glNamedFramebufferDrawBuffers(m_fbo, m_attachmentCount, drawBuffers.data());

    // Check completeness
    GLenum status = glCheckNamedFramebufferStatus(m_fbo, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[GBuffer] Framebuffer incomplete: ";
        switch (status) {
            case GL_FRAMEBUFFER_UNDEFINED:
                std::cerr << "GL_FRAMEBUFFER_UNDEFINED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                std::cerr << "GL_FRAMEBUFFER_UNSUPPORTED";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                std::cerr << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
                break;
            default:
                std::cerr << "Unknown error (" << status << ")";
                break;
        }
        std::cerr << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
// Format Helpers
// ============================================================================

uint32_t GBuffer::GetPositionFormat() const {
    return m_config.highPrecisionPosition ? GL_RGBA32F : GL_RGBA16F;
}

uint32_t GBuffer::GetNormalFormat() const {
    return m_config.highPrecisionNormal ? GL_RGBA16F : GL_RGB10_A2;
}

uint32_t GBuffer::GetAlbedoFormat() const {
    return GL_RGBA8;  // 8-bit per channel is sufficient for albedo
}

uint32_t GBuffer::GetMaterialFormat() const {
    return GL_RGBA8;  // 8-bit per channel for material params
}

uint32_t GBuffer::GetEmissionFormat() const {
    return GL_RGBA16F;  // HDR for emission
}

uint32_t GBuffer::GetVelocityFormat() const {
    return GL_RG16F;  // 16-bit for velocity
}

// ============================================================================
// Rendering
// ============================================================================

void GBuffer::BindForGeometryPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_config.width, m_config.height);
}

void GBuffer::BindTexturesForLighting(uint32_t baseSlot) {
    glBindTextureUnit(baseSlot + 0, m_positionTexture);
    glBindTextureUnit(baseSlot + 1, m_normalTexture);
    glBindTextureUnit(baseSlot + 2, m_albedoTexture);
    glBindTextureUnit(baseSlot + 3, m_materialTexture);

    if (m_config.enableEmission && m_emissionTexture) {
        glBindTextureUnit(baseSlot + 4, m_emissionTexture);
    }

    if (m_config.enableVelocity && m_velocityTexture) {
        glBindTextureUnit(baseSlot + 5, m_velocityTexture);
    }

    glBindTextureUnit(baseSlot + 6, m_depthTexture);
}

void GBuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::Clear(const glm::vec4& clearColor) {
    // Bind the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Clear position buffer (with max depth in alpha)
    const float positionClear[] = {0.0f, 0.0f, 0.0f, 1000.0f};
    glClearNamedFramebufferfv(m_fbo, GL_COLOR, 0, positionClear);

    // Clear normal buffer
    const float normalClear[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearNamedFramebufferfv(m_fbo, GL_COLOR, 1, normalClear);

    // Clear albedo buffer
    glClearNamedFramebufferfv(m_fbo, GL_COLOR, 2, &clearColor[0]);

    // Clear material buffer (default: non-metallic, medium roughness, full AO)
    const float materialClear[] = {0.0f, 0.5f, 1.0f, 0.0f};
    glClearNamedFramebufferfv(m_fbo, GL_COLOR, 3, materialClear);

    int attachmentIndex = 4;

    // Clear emission buffer if enabled
    if (m_config.enableEmission && m_emissionTexture) {
        const float emissionClear[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearNamedFramebufferfv(m_fbo, GL_COLOR, attachmentIndex++, emissionClear);
    }

    // Clear velocity buffer if enabled
    if (m_config.enableVelocity && m_velocityTexture) {
        const float velocityClear[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearNamedFramebufferfv(m_fbo, GL_COLOR, attachmentIndex++, velocityClear);
    }

    // Clear depth buffer
    const float depthClear = 1.0f;
    glClearNamedFramebufferfv(m_fbo, GL_DEPTH, 0, &depthClear);
}

void GBuffer::CopyDepthTo(uint32_t targetFBO) {
    glBlitNamedFramebuffer(
        m_fbo, targetFBO,
        0, 0, m_config.width, m_config.height,
        0, 0, m_config.width, m_config.height,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST
    );
}

// ============================================================================
// Texture Access
// ============================================================================

uint32_t GBuffer::GetTexture(GBufferAttachment attachment) const {
    switch (attachment) {
        case GBufferAttachment::Position:       return m_positionTexture;
        case GBufferAttachment::Normal:         return m_normalTexture;
        case GBufferAttachment::Albedo:         return m_albedoTexture;
        case GBufferAttachment::MaterialParams: return m_materialTexture;
        case GBufferAttachment::Emission:       return m_emissionTexture;
        case GBufferAttachment::Velocity:       return m_velocityTexture;
        default:                                return 0;
    }
}

// ============================================================================
// Debug
// ============================================================================

void GBuffer::DebugVisualize(GBufferAttachment attachment, Shader* shader) {
    // Create debug quad if needed
    if (m_debugQuadVAO == 0) {
        float quadVertices[] = {
            // positions        // texCoords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

        glCreateVertexArrays(1, &m_debugQuadVAO);
        glCreateBuffers(1, &m_debugQuadVBO);
        glNamedBufferStorage(m_debugQuadVBO, sizeof(quadVertices), quadVertices, 0);

        glVertexArrayVertexBuffer(m_debugQuadVAO, 0, m_debugQuadVBO, 0, 5 * sizeof(float));
        glEnableVertexArrayAttrib(m_debugQuadVAO, 0);
        glEnableVertexArrayAttrib(m_debugQuadVAO, 1);
        glVertexArrayAttribFormat(m_debugQuadVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(m_debugQuadVAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
        glVertexArrayAttribBinding(m_debugQuadVAO, 0, 0);
        glVertexArrayAttribBinding(m_debugQuadVAO, 1, 0);
    }

    // TODO: Implement debug shader visualization
    // For now, just bind the texture and draw the quad
    uint32_t tex = GetTexture(attachment);
    if (tex == 0) {
        return;
    }

    glBindTextureUnit(0, tex);
    glBindVertexArray(m_debugQuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

size_t GBuffer::GetMemoryUsage() const {
    size_t total = 0;
    const size_t pixelCount = static_cast<size_t>(m_config.width) * m_config.height;

    // Position: RGBA32F (16 bytes) or RGBA16F (8 bytes)
    total += pixelCount * (m_config.highPrecisionPosition ? 16 : 8);

    // Normal: RGBA16F (8 bytes) or RGB10A2 (4 bytes)
    total += pixelCount * (m_config.highPrecisionNormal ? 8 : 4);

    // Albedo: RGBA8 (4 bytes)
    total += pixelCount * 4;

    // Material: RGBA8 (4 bytes)
    total += pixelCount * 4;

    // Emission: RGBA16F (8 bytes) if enabled
    if (m_config.enableEmission) {
        total += pixelCount * 8;
    }

    // Velocity: RG16F (4 bytes) if enabled
    if (m_config.enableVelocity) {
        total += pixelCount * 4;
    }

    // Depth: DEPTH_COMPONENT32F (4 bytes)
    total += pixelCount * 4;

    return total;
}

const char* GBuffer::GetAttachmentName(GBufferAttachment attachment) {
    switch (attachment) {
        case GBufferAttachment::Position:       return "Position";
        case GBufferAttachment::Normal:         return "Normal";
        case GBufferAttachment::Albedo:         return "Albedo";
        case GBufferAttachment::MaterialParams: return "Material";
        case GBufferAttachment::Emission:       return "Emission";
        case GBufferAttachment::Velocity:       return "Velocity";
        default:                                return "Unknown";
    }
}

} // namespace Nova
