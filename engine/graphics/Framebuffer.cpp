#include "graphics/Framebuffer.hpp"
#include "graphics/Texture.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>

namespace Nova {

Framebuffer::Framebuffer() = default;

Framebuffer::~Framebuffer() {
    Cleanup();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_fbo(other.m_fbo)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_numColorAttachments(other.m_numColorAttachments)
    , m_hasDepth(other.m_hasDepth)
    , m_colorAttachments(std::move(other.m_colorAttachments))
    , m_depthAttachment(std::move(other.m_depthAttachment))
{
    other.m_fbo = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_fbo = other.m_fbo;
        m_width = other.m_width;
        m_height = other.m_height;
        m_numColorAttachments = other.m_numColorAttachments;
        m_hasDepth = other.m_hasDepth;
        m_colorAttachments = std::move(other.m_colorAttachments);
        m_depthAttachment = std::move(other.m_depthAttachment);
        other.m_fbo = 0;
    }
    return *this;
}

bool Framebuffer::Create(int width, int height, int colorAttachments, bool depth) {
    m_width = width;
    m_height = height;
    m_numColorAttachments = colorAttachments;
    m_hasDepth = depth;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    CreateAttachments();

    if (!IsComplete()) {
        spdlog::error("Framebuffer is not complete");
        Cleanup();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Framebuffer::CreateAttachments() {
    // Color attachments
    m_colorAttachments.clear();
    std::vector<uint32_t> attachments;

    for (int i = 0; i < m_numColorAttachments; ++i) {
        auto texture = std::make_shared<Texture>();
        texture->CreateEmpty(m_width, m_height, TextureFormat::RGBA);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, texture->GetID(), 0);

        m_colorAttachments.push_back(texture);
        attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
    }

    if (!attachments.empty()) {
        glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
    }

    // Depth attachment
    if (m_hasDepth) {
        m_depthAttachment = std::make_shared<Texture>();
        m_depthAttachment->CreateEmpty(m_width, m_height, TextureFormat::Depth);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, m_depthAttachment->GetID(), 0);
    }
}

void Framebuffer::Resize(int width, int height) {
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    CreateAttachments();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
}

void Framebuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Clear(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | (m_hasDepth ? GL_DEPTH_BUFFER_BIT : 0));
}

void Framebuffer::BlitTo(Framebuffer& target, bool colorOnly) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.m_fbo);

    uint32_t mask = GL_COLOR_BUFFER_BIT;
    if (!colorOnly && m_hasDepth && target.m_hasDepth) {
        mask |= GL_DEPTH_BUFFER_BIT;
    }

    glBlitFramebuffer(0, 0, m_width, m_height,
                      0, 0, target.m_width, target.m_height,
                      mask, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::shared_ptr<Texture> Framebuffer::GetColorAttachment(int index) {
    if (index >= 0 && index < static_cast<int>(m_colorAttachments.size())) {
        return m_colorAttachments[index];
    }
    return nullptr;
}

std::shared_ptr<Texture> Framebuffer::GetDepthAttachment() {
    return m_depthAttachment;
}

bool Framebuffer::IsComplete() const {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void Framebuffer::Cleanup() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    m_colorAttachments.clear();
    m_depthAttachment.reset();
}

} // namespace Nova
