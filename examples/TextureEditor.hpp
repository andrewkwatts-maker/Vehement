#pragma once

#include "AssetEditor.hpp"
#include <imgui.h>

/**
 * @brief Texture asset editor
 *
 * Features:
 * - Texture preview with zoom and pan
 * - Display texture information (dimensions, format, channels)
 * - Color channel visualization (R, G, B, A, RGB, RGBA)
 * - Basic adjustments (brightness, contrast preview)
 * - Mipmap level visualization
 * - Export options
 */
class TextureEditor : public IAssetEditor {
public:
    TextureEditor();
    ~TextureEditor() override;

    void Open(const std::string& assetPath) override;
    void Render(bool* isOpen) override;
    std::string GetEditorName() const override;
    bool IsDirty() const override;
    void Save() override;
    std::string GetAssetPath() const override;

private:
    void RenderToolbar();
    void RenderPreview();
    void RenderProperties();
    void LoadTexture();
    void UpdatePreview();
    void ExportTextureAs();

    std::string m_assetPath;
    std::string m_textureName;
    bool m_isDirty = false;
    bool m_isLoaded = false;

    // Texture info
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    std::string m_format = "Unknown";
    int m_mipLevels = 1;
    size_t m_fileSize = 0;

    // Display options
    float m_zoom = 1.0f;
    ImVec2 m_pan{0.0f, 0.0f};
    bool m_showRed = true;
    bool m_showGreen = true;
    bool m_showBlue = true;
    bool m_showAlpha = true;
    int m_currentMipLevel = 0;

    // Adjustments (preview only, not saved)
    float m_brightness = 0.0f;
    float m_contrast = 1.0f;

    // Texture ID for ImGui (would be actual GPU texture in real implementation)
    ImTextureID m_textureID = nullptr;

    // UI state
    bool m_fitToWindow = true;
    bool m_showGrid = false;
    float m_gridSize = 16.0f;
};
