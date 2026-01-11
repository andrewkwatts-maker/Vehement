#include "graphics/Texture.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>

// STB_IMAGE_IMPLEMENTATION should only be defined once in the entire project.
// This is the designated compilation unit for stb_image and stb_image_write.
#ifndef NOVA_STB_IMAGE_IMPLEMENTED
    #define NOVA_STB_IMAGE_IMPLEMENTED
    #define STB_IMAGE_IMPLEMENTATION
    #define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb_image.h>
#include <stb_image_write.h>

namespace Nova {

namespace {
    uint32_t GetGLFormat(TextureFormat format) {
        switch (format) {
            case TextureFormat::RGB:          return GL_RGB;
            case TextureFormat::RGBA:         return GL_RGBA;
            case TextureFormat::Red:          return GL_RED;
            case TextureFormat::RG:           return GL_RG;
            case TextureFormat::Depth:        return GL_DEPTH_COMPONENT;
            case TextureFormat::DepthStencil: return GL_DEPTH_STENCIL;
        }
        return GL_RGBA;
    }

    uint32_t GetGLInternalFormat(TextureFormat format, bool sRGB) {
        switch (format) {
            case TextureFormat::RGB:          return sRGB ? GL_SRGB8 : GL_RGB8;
            case TextureFormat::RGBA:         return sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            case TextureFormat::Red:          return GL_R8;
            case TextureFormat::RG:           return GL_RG8;
            case TextureFormat::Depth:        return GL_DEPTH_COMPONENT24;
            case TextureFormat::DepthStencil: return GL_DEPTH24_STENCIL8;
        }
        return GL_RGBA8;
    }

    uint32_t GetGLFilter(TextureFilter filter) {
        switch (filter) {
            case TextureFilter::Nearest:              return GL_NEAREST;
            case TextureFilter::Linear:               return GL_LINEAR;
            case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
            case TextureFilter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
            case TextureFilter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
            case TextureFilter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
        }
        return GL_LINEAR;
    }

    uint32_t GetGLWrap(TextureWrap wrap) {
        switch (wrap) {
            case TextureWrap::Repeat:         return GL_REPEAT;
            case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
            case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
            case TextureWrap::ClampToBorder:  return GL_CLAMP_TO_BORDER;
        }
        return GL_REPEAT;
    }
}

Texture::Texture() = default;

Texture::~Texture() {
    Cleanup();
}

Texture::Texture(Texture&& other) noexcept
    : m_textureID(other.m_textureID)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_channels(other.m_channels)
    , m_format(other.m_format)
    , m_path(std::move(other.m_path))
{
    other.m_textureID = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_textureID = other.m_textureID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_format = other.m_format;
        m_path = std::move(other.m_path);
        other.m_textureID = 0;
    }
    return *this;
}

bool Texture::Load(const std::string& path, bool sRGB, bool generateMipmaps) {
    m_path = path;

    stbi_set_flip_vertically_on_load(true);

    unsigned char* data = stbi_load(path.c_str(), &m_width, &m_height, &m_channels, 0);
    if (!data) {
        spdlog::error("Failed to load texture: {}", path);
        return false;
    }

    // Determine format
    switch (m_channels) {
        case 1: m_format = TextureFormat::Red; break;
        case 2: m_format = TextureFormat::RG; break;
        case 3: m_format = TextureFormat::RGB; break;
        case 4: m_format = TextureFormat::RGBA; break;
        default:
            spdlog::error("Unsupported number of channels: {}", m_channels);
            stbi_image_free(data);
            return false;
    }

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GetGLInternalFormat(m_format, sRGB),
                 m_width, m_height, 0, GetGLFormat(m_format), GL_UNSIGNED_BYTE, data);

    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        SetFilter(TextureFilter::LinearMipmapLinear, TextureFilter::Linear);
    } else {
        SetFilter(TextureFilter::Linear, TextureFilter::Linear);
    }

    SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);

    stbi_image_free(data);

    spdlog::debug("Loaded texture: {} ({}x{}, {} channels)", path, m_width, m_height, m_channels);
    return true;
}

bool Texture::Create(int width, int height, TextureFormat format, const void* data) {
    m_width = width;
    m_height = height;
    m_format = format;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    uint32_t type = (format == TextureFormat::Depth || format == TextureFormat::DepthStencil)
                    ? GL_FLOAT : GL_UNSIGNED_BYTE;

    glTexImage2D(GL_TEXTURE_2D, 0, GetGLInternalFormat(format, false),
                 width, height, 0, GetGLFormat(format), type, data);

    SetFilter(TextureFilter::Linear, TextureFilter::Linear);
    SetWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);

    return true;
}

bool Texture::CreateEmpty(int width, int height, TextureFormat format) {
    return Create(width, height, format, nullptr);
}

void Texture::Bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
}

void Texture::Unbind(uint32_t slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetGLFilter(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetGLFilter(magFilter));
}

void Texture::SetWrap(TextureWrap wrapS, TextureWrap wrapT) {
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GetGLWrap(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GetGLWrap(wrapT));
}

void Texture::SetBorderColor(const glm::vec4& color) {
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &color.r);
}

void Texture::GenerateMipmaps() {
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void Texture::Cleanup() {
    if (m_textureID) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
}

// Cubemap implementation
Cubemap::Cubemap() = default;

Cubemap::~Cubemap() {
    Cleanup();
}

Cubemap::Cubemap(Cubemap&& other) noexcept
    : m_textureID(other.m_textureID)
    , m_size(other.m_size)
{
    other.m_textureID = 0;
    other.m_size = 0;
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_textureID = other.m_textureID;
        m_size = other.m_size;
        other.m_textureID = 0;
        other.m_size = 0;
    }
    return *this;
}

void Cubemap::Cleanup() {
    if (m_textureID) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    m_size = 0;
}

bool Cubemap::Load(const std::string faces[6]) {
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < 6; ++i) {
        int width, height, channels;
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &channels, 0);
        if (!data) {
            spdlog::error("Failed to load cubemap face: {}", faces[i]);
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
            return false;
        }

        uint32_t format = channels == 4 ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format,
                     width, height, 0, format, GL_UNSIGNED_BYTE, data);

        if (i == 0) m_size = width;

        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return true;
}

bool Cubemap::LoadEquirectangular(const std::string& path) {
    // This would require HDR loading and conversion - simplified for now
    spdlog::warn("Equirectangular cubemap loading not fully implemented");
    return false;
}

void Cubemap::Bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
}

void Cubemap::Unbind(uint32_t slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // namespace Nova
