#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace Nova {

class Texture;

/**
 * @brief Texture resource manager
 */
class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager() = default;

    /**
     * @brief Load a texture from file
     */
    std::shared_ptr<Texture> Load(const std::string& path, bool sRGB = true);

    /**
     * @brief Get a previously loaded texture
     */
    std::shared_ptr<Texture> Get(const std::string& path);

    /**
     * @brief Check if a texture exists
     */
    bool Has(const std::string& path) const;

    /**
     * @brief Remove a texture
     */
    void Remove(const std::string& path);

    /**
     * @brief Clear all textures
     */
    void Clear();

    /**
     * @brief Get default white texture (1x1 white)
     */
    std::shared_ptr<Texture> GetWhite();

    /**
     * @brief Get default black texture (1x1 black)
     */
    std::shared_ptr<Texture> GetBlack();

    /**
     * @brief Get default normal texture (1x1 flat normal)
     */
    std::shared_ptr<Texture> GetNormal();

private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    std::shared_ptr<Texture> m_whiteTexture;
    std::shared_ptr<Texture> m_blackTexture;
    std::shared_ptr<Texture> m_normalTexture;

    void CreateDefaultTextures();
};

} // namespace Nova
