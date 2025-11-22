#pragma once

#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {
class TextureImportSettings;
class Texture;
struct ImageData;
}

namespace Vehement {

/**
 * @brief Texture import settings panel
 *
 * Features:
 * - Before/after compression preview
 * - Size estimation
 * - Channel configuration
 * - Normal map tools
 * - Sprite slicing preview
 */
class TextureImportPanel {
public:
    TextureImportPanel();
    ~TextureImportPanel();

    void Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    /**
     * @brief Set the texture file to configure
     */
    void SetTexturePath(const std::string& path);

    /**
     * @brief Get current settings
     */
    Nova::TextureImportSettings* GetSettings() { return m_settings.get(); }

    /**
     * @brief Apply preset
     */
    void ApplyPreset(const std::string& preset);

    /**
     * @brief Get estimated output size
     */
    size_t GetEstimatedSize() const;

    // Callbacks
    using SettingsChangedCallback = std::function<void()>;
    void SetSettingsChangedCallback(SettingsChangedCallback cb) { m_onSettingsChanged = cb; }

private:
    void RenderPreviewComparison();
    void RenderCompressionSettings();
    void RenderSizeSettings();
    void RenderChannelSettings();
    void RenderNormalMapSettings();
    void RenderSpriteSettings();
    void RenderMipmapPreview();

    void UpdatePreview();
    void GenerateComparisonPreview();

    std::string m_texturePath;
    std::unique_ptr<Nova::TextureImportSettings> m_settings;

    // Preview textures
    std::shared_ptr<Nova::Texture> m_originalPreview;
    std::shared_ptr<Nova::Texture> m_compressedPreview;
    std::vector<std::shared_ptr<Nova::Texture>> m_mipmapPreviews;

    // Image info
    int m_originalWidth = 0;
    int m_originalHeight = 0;
    int m_originalChannels = 0;
    size_t m_originalSize = 0;

    // UI state
    float m_comparisonSlider = 0.5f;
    int m_selectedMipLevel = 0;
    bool m_showAlphaChannel = false;
    bool m_previewDirty = true;

    // Sprite slicing preview
    std::vector<glm::vec4> m_spriteRects;
    int m_selectedSprite = -1;

    SettingsChangedCallback m_onSettingsChanged;
};

} // namespace Vehement
