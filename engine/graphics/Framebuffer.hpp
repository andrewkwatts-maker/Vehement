#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {

class Texture;

/**
 * @brief Framebuffer attachment type
 */
enum class AttachmentType {
    Color,
    Depth,
    DepthStencil
};

/**
 * @brief OpenGL framebuffer wrapper for render-to-texture
 */
class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();

    // Delete copy, allow move
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    /**
     * @brief Create framebuffer with specified attachments
     */
    bool Create(int width, int height, int colorAttachments = 1, bool depth = true);

    /**
     * @brief Resize the framebuffer
     */
    void Resize(int width, int height);

    /**
     * @brief Bind for rendering
     */
    void Bind() const;

    /**
     * @brief Unbind (bind default framebuffer)
     */
    static void Unbind();

    /**
     * @brief Clear all attachments
     */
    void Clear(const glm::vec4& color = glm::vec4(0, 0, 0, 1));

    /**
     * @brief Blit to another framebuffer (for MSAA resolve)
     */
    void BlitTo(Framebuffer& target, bool colorOnly = false);

    /**
     * @brief Get color attachment texture
     */
    std::shared_ptr<Texture> GetColorAttachment(int index = 0);

    /**
     * @brief Get depth attachment texture
     */
    std::shared_ptr<Texture> GetDepthAttachment();

    /**
     * @brief Check if framebuffer is complete
     */
    bool IsComplete() const;

    // Getters
    uint32_t GetID() const { return m_fbo; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    void Cleanup();
    void CreateAttachments();

    uint32_t m_fbo = 0;
    int m_width = 0;
    int m_height = 0;
    int m_numColorAttachments = 0;
    bool m_hasDepth = false;

    std::vector<std::shared_ptr<Texture>> m_colorAttachments;
    std::shared_ptr<Texture> m_depthAttachment;
};

} // namespace Nova
