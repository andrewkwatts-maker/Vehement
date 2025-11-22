#pragma once

#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Texture filtering modes
 */
enum class TextureFilter {
    Nearest,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear
};

/**
 * @brief Texture wrapping modes
 */
enum class TextureWrap {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

/**
 * @brief Texture format
 */
enum class TextureFormat {
    RGB,
    RGBA,
    Red,
    RG,
    Depth,
    DepthStencil
};

/**
 * @brief OpenGL texture wrapper
 */
class Texture {
public:
    Texture();
    ~Texture();

    // Delete copy, allow move
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief Load texture from file
     */
    bool Load(const std::string& path, bool sRGB = true, bool generateMipmaps = true);

    /**
     * @brief Create texture from raw data
     */
    bool Create(int width, int height, TextureFormat format, const void* data = nullptr);

    /**
     * @brief Create empty texture (useful for render targets)
     */
    bool CreateEmpty(int width, int height, TextureFormat format = TextureFormat::RGBA);

    /**
     * @brief Bind texture to a slot
     */
    void Bind(uint32_t slot = 0) const;

    /**
     * @brief Unbind texture from slot
     */
    static void Unbind(uint32_t slot = 0);

    /**
     * @brief Set filtering mode
     */
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);

    /**
     * @brief Set wrap mode
     */
    void SetWrap(TextureWrap wrapS, TextureWrap wrapT);

    /**
     * @brief Set border color (for ClampToBorder wrap mode)
     */
    void SetBorderColor(const glm::vec4& color);

    /**
     * @brief Generate mipmaps
     */
    void GenerateMipmaps();

    /**
     * @brief Cleanup GPU resources
     */
    void Cleanup();

    // Getters
    uint32_t GetID() const { return m_textureID; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    TextureFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_textureID != 0; }
    const std::string& GetPath() const { return m_path; }

private:
    uint32_t m_textureID = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    TextureFormat m_format = TextureFormat::RGBA;
    std::string m_path;
};

/**
 * @brief Cubemap texture for skyboxes and environment mapping
 */
class Cubemap {
public:
    Cubemap();
    ~Cubemap();

    // Delete copy, allow move
    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;
    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    /**
     * @brief Cleanup GPU resources
     */
    void Cleanup();

    /**
     * @brief Load cubemap from 6 face images
     * Order: +X, -X, +Y, -Y, +Z, -Z
     */
    bool Load(const std::string faces[6]);

    /**
     * @brief Load cubemap from a single equirectangular HDR image
     */
    bool LoadEquirectangular(const std::string& path);

    void Bind(uint32_t slot = 0) const;
    static void Unbind(uint32_t slot = 0);

    uint32_t GetID() const { return m_textureID; }
    bool IsValid() const { return m_textureID != 0; }

private:
    uint32_t m_textureID = 0;
    int m_size = 0;
};

} // namespace Nova
