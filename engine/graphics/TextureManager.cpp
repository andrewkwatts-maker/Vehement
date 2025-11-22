#include "graphics/TextureManager.hpp"
#include "graphics/Texture.hpp"
#include <spdlog/spdlog.h>

namespace Nova {

std::shared_ptr<Texture> TextureManager::Load(const std::string& path, bool sRGB) {
    // Check if already loaded
    auto it = m_textures.find(path);
    if (it != m_textures.end()) {
        return it->second;
    }

    auto texture = std::make_shared<Texture>();
    if (!texture->Load(path, sRGB)) {
        spdlog::error("Failed to load texture: {}", path);
        return nullptr;
    }

    m_textures[path] = texture;
    return texture;
}

std::shared_ptr<Texture> TextureManager::Get(const std::string& path) {
    auto it = m_textures.find(path);
    return it != m_textures.end() ? it->second : nullptr;
}

bool TextureManager::Has(const std::string& path) const {
    return m_textures.find(path) != m_textures.end();
}

void TextureManager::Remove(const std::string& path) {
    m_textures.erase(path);
}

void TextureManager::Clear() {
    m_textures.clear();
}

void TextureManager::CreateDefaultTextures() {
    // Only create if not already created
    if (m_whiteTexture && m_blackTexture && m_normalTexture) {
        return;
    }

    // White texture (1x1 white pixel)
    if (!m_whiteTexture) {
        uint8_t white[] = {255, 255, 255, 255};
        m_whiteTexture = std::make_shared<Texture>();
        m_whiteTexture->Create(1, 1, TextureFormat::RGBA, white);
        m_whiteTexture->SetFilter(TextureFilter::Nearest, TextureFilter::Nearest);
    }

    // Black texture (1x1 black pixel)
    if (!m_blackTexture) {
        uint8_t black[] = {0, 0, 0, 255};
        m_blackTexture = std::make_shared<Texture>();
        m_blackTexture->Create(1, 1, TextureFormat::RGBA, black);
        m_blackTexture->SetFilter(TextureFilter::Nearest, TextureFilter::Nearest);
    }

    // Normal texture (flat normal pointing up - tangent space +Z)
    if (!m_normalTexture) {
        uint8_t normal[] = {128, 128, 255, 255};
        m_normalTexture = std::make_shared<Texture>();
        m_normalTexture->Create(1, 1, TextureFormat::RGBA, normal);
        m_normalTexture->SetFilter(TextureFilter::Nearest, TextureFilter::Nearest);
    }
}

std::shared_ptr<Texture> TextureManager::GetWhite() {
    if (!m_whiteTexture) CreateDefaultTextures();
    return m_whiteTexture;
}

std::shared_ptr<Texture> TextureManager::GetBlack() {
    if (!m_blackTexture) CreateDefaultTextures();
    return m_blackTexture;
}

std::shared_ptr<Texture> TextureManager::GetNormal() {
    if (!m_normalTexture) CreateDefaultTextures();
    return m_normalTexture;
}

} // namespace Nova
