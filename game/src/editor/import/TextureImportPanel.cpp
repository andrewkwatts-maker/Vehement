#include "TextureImportPanel.hpp"
#include <engine/import/ImportSettings.hpp>
#include <engine/import/TextureImporter.hpp>
#include <filesystem>

namespace fs = std::filesystem;

namespace Vehement {

TextureImportPanel::TextureImportPanel()
    : m_settings(std::make_unique<Nova::TextureImportSettings>()) {
}

TextureImportPanel::~TextureImportPanel() = default;

void TextureImportPanel::Initialize() {
    // Setup default settings
}

void TextureImportPanel::Shutdown() {
    m_originalPreview.reset();
    m_compressedPreview.reset();
    m_mipmapPreviews.clear();
}

void TextureImportPanel::Update(float deltaTime) {
    if (m_previewDirty) {
        UpdatePreview();
        m_previewDirty = false;
    }
}

void TextureImportPanel::Render() {
    RenderPreviewComparison();
    RenderCompressionSettings();
    RenderSizeSettings();
    RenderChannelSettings();

    if (m_settings->isNormalMap || m_settings->textureType == Nova::TextureType::Normal) {
        RenderNormalMapSettings();
    }

    if (m_settings->sliceSprites || m_settings->createAtlas) {
        RenderSpriteSettings();
    }

    if (m_settings->generateMipmaps) {
        RenderMipmapPreview();
    }
}

void TextureImportPanel::SetTexturePath(const std::string& path) {
    m_texturePath = path;
    m_settings->assetPath = path;
    m_settings->AutoDetectType(path);

    // Get original file info
    if (fs::exists(path)) {
        m_originalSize = fs::file_size(path);

        Nova::TextureImporter importer;
        importer.GetImageInfo(path, m_originalWidth, m_originalHeight, m_originalChannels);
    }

    m_previewDirty = true;
}

void TextureImportPanel::ApplyPreset(const std::string& preset) {
    if (preset == "Mobile") {
        m_settings->ApplyPreset(Nova::ImportPreset::Mobile);
    } else if (preset == "Desktop") {
        m_settings->ApplyPreset(Nova::ImportPreset::Desktop);
    } else if (preset == "HighQuality") {
        m_settings->ApplyPreset(Nova::ImportPreset::HighQuality);
    }

    m_previewDirty = true;
    if (m_onSettingsChanged) m_onSettingsChanged();
}

size_t TextureImportPanel::GetEstimatedSize() const {
    float bpp = Nova::GetCompressionBPP(m_settings->compression);

    int width = std::min(m_originalWidth, m_settings->maxWidth);
    int height = std::min(m_originalHeight, m_settings->maxHeight);

    size_t baseSize = static_cast<size_t>((width * height * bpp) / 8);

    if (m_settings->generateMipmaps) {
        baseSize = static_cast<size_t>(baseSize * 1.33f);
    }

    return baseSize;
}

void TextureImportPanel::RenderPreviewComparison() {
    // Before/after comparison slider

    /*
    ImGui::Text("Preview Comparison");
    ImGui::Separator();

    ImGui::SliderFloat("##Comparison", &m_comparisonSlider, 0.0f, 1.0f, "");

    // Draw split preview
    ImVec2 previewSize(256, 256);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Original on left
    if (m_originalPreview) {
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)m_originalPreview->GetID(),
            cursorPos,
            ImVec2(cursorPos.x + previewSize.x * m_comparisonSlider, cursorPos.y + previewSize.y),
            ImVec2(0, 0),
            ImVec2(m_comparisonSlider, 1));
    }

    // Compressed on right
    if (m_compressedPreview) {
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)m_compressedPreview->GetID(),
            ImVec2(cursorPos.x + previewSize.x * m_comparisonSlider, cursorPos.y),
            ImVec2(cursorPos.x + previewSize.x, cursorPos.y + previewSize.y),
            ImVec2(m_comparisonSlider, 0),
            ImVec2(1, 1));
    }

    // Divider line
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(cursorPos.x + previewSize.x * m_comparisonSlider, cursorPos.y),
        ImVec2(cursorPos.x + previewSize.x * m_comparisonSlider, cursorPos.y + previewSize.y),
        IM_COL32(255, 255, 255, 255), 2.0f);

    ImGui::Dummy(previewSize);

    // Size info
    ImGui::Text("Original: %dx%d (%s)",
        m_originalWidth, m_originalHeight,
        FormatSize(m_originalSize).c_str());

    size_t estimatedSize = GetEstimatedSize();
    float ratio = m_originalSize > 0 ? (float)estimatedSize / m_originalSize : 1.0f;
    ImGui::Text("Estimated: %s (%.1f%%)",
        FormatSize(estimatedSize).c_str(),
        ratio * 100.0f);
    */
}

void TextureImportPanel::RenderCompressionSettings() {
    /*
    ImGui::Text("Compression");
    ImGui::Separator();

    // Compression format dropdown
    const char* compressionNames[] = {
        "None", "BC1 (DXT1)", "BC3 (DXT5)", "BC4", "BC5",
        "BC6H (HDR)", "BC7", "ETC1", "ETC2 RGB", "ETC2 RGBA",
        "ASTC 4x4", "ASTC 6x6", "ASTC 8x8", "PVRTC RGB", "PVRTC RGBA"
    };

    int currentCompression = static_cast<int>(m_settings->compression);
    if (ImGui::Combo("Format", &currentCompression, compressionNames, IM_ARRAYSIZE(compressionNames))) {
        m_settings->compression = static_cast<Nova::TextureCompression>(currentCompression);
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Quality slider
    if (ImGui::SliderInt("Quality", &m_settings->compressionQuality, 0, 100)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // sRGB checkbox
    if (ImGui::Checkbox("sRGB Color Space", &m_settings->sRGB)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Mipmaps
    if (ImGui::Checkbox("Generate Mipmaps", &m_settings->generateMipmaps)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->generateMipmaps) {
        const char* mipmapQualities[] = {"Fast", "Standard", "High Quality", "Custom"};
        int quality = static_cast<int>(m_settings->mipmapQuality);
        if (ImGui::Combo("Mipmap Quality", &quality, mipmapQualities, IM_ARRAYSIZE(mipmapQualities))) {
            m_settings->mipmapQuality = static_cast<Nova::MipmapQuality>(quality);
            m_previewDirty = true;
        }
    }
    */
}

void TextureImportPanel::RenderSizeSettings() {
    /*
    ImGui::Text("Size Settings");
    ImGui::Separator();

    // Max size
    int maxSizes[] = {256, 512, 1024, 2048, 4096, 8192};
    const char* maxSizeLabels[] = {"256", "512", "1024", "2048", "4096", "8192"};

    int currentMaxIdx = 3; // Default 2048
    for (int i = 0; i < 6; ++i) {
        if (maxSizes[i] == m_settings->maxWidth) {
            currentMaxIdx = i;
            break;
        }
    }

    if (ImGui::Combo("Max Size", &currentMaxIdx, maxSizeLabels, 6)) {
        m_settings->maxWidth = maxSizes[currentMaxIdx];
        m_settings->maxHeight = maxSizes[currentMaxIdx];
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Power of two
    if (ImGui::Checkbox("Force Power of Two", &m_settings->powerOfTwo)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Flip options
    if (ImGui::Checkbox("Flip Vertically", &m_settings->flipVertically)) {
        m_previewDirty = true;
    }

    if (ImGui::Checkbox("Flip Horizontally", &m_settings->flipHorizontally)) {
        m_previewDirty = true;
    }
    */
}

void TextureImportPanel::RenderChannelSettings() {
    /*
    ImGui::Text("Channel Configuration");
    ImGui::Separator();

    // Show channel info
    ImGui::Text("Channels: %d", m_originalChannels);

    if (m_originalChannels == 4) {
        if (ImGui::Checkbox("Premultiply Alpha", &m_settings->premultiplyAlpha)) {
            m_previewDirty = true;
        }
    }

    // Preview channel buttons
    ImGui::Text("View Channel:");
    ImGui::SameLine();

    if (ImGui::RadioButton("RGB", !m_showAlphaChannel)) {
        m_showAlphaChannel = false;
        m_previewDirty = true;
    }
    ImGui::SameLine();

    if (m_originalChannels == 4) {
        if (ImGui::RadioButton("Alpha", m_showAlphaChannel)) {
            m_showAlphaChannel = true;
            m_previewDirty = true;
        }
    }
    */
}

void TextureImportPanel::RenderNormalMapSettings() {
    /*
    ImGui::Text("Normal Map Settings");
    ImGui::Separator();

    if (ImGui::Checkbox("Is Normal Map", &m_settings->isNormalMap)) {
        if (m_settings->isNormalMap) {
            m_settings->sRGB = false;
            m_settings->compression = Nova::TextureCompression::BC5;
        }
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Checkbox("Generate from Height Map", &m_settings->normalMapFromHeight)) {
        m_previewDirty = true;
    }

    if (m_settings->normalMapFromHeight) {
        if (ImGui::SliderFloat("Normal Strength", &m_settings->normalMapStrength, 0.1f, 5.0f)) {
            m_previewDirty = true;
        }
    }

    if (ImGui::Checkbox("Reconstruct Z Channel", &m_settings->reconstructZ)) {
        m_previewDirty = true;
    }
    */
}

void TextureImportPanel::RenderSpriteSettings() {
    /*
    ImGui::Text("Sprite Settings");
    ImGui::Separator();

    if (ImGui::Checkbox("Slice as Sprite Sheet", &m_settings->sliceSprites)) {
        m_previewDirty = true;
    }

    if (m_settings->sliceSprites) {
        if (ImGui::InputInt("Slice Width", &m_settings->sliceWidth)) {
            m_settings->sliceWidth = std::max(1, m_settings->sliceWidth);
            m_previewDirty = true;
        }

        if (ImGui::InputInt("Slice Height", &m_settings->sliceHeight)) {
            m_settings->sliceHeight = std::max(1, m_settings->sliceHeight);
            m_previewDirty = true;
        }

        // Show grid preview
        int cols = m_originalWidth / m_settings->sliceWidth;
        int rows = m_originalHeight / m_settings->sliceHeight;
        ImGui::Text("Grid: %d x %d (%d sprites)", cols, rows, cols * rows);
    }

    ImGui::Separator();

    if (ImGui::Checkbox("Create Texture Atlas", &m_settings->createAtlas)) {
        m_previewDirty = true;
    }

    if (m_settings->createAtlas) {
        if (ImGui::InputInt("Atlas Max Size", &m_settings->atlasMaxSize)) {
            m_settings->atlasMaxSize = std::max(256, m_settings->atlasMaxSize);
        }

        if (ImGui::InputInt("Atlas Padding", &m_settings->atlasPadding)) {
            m_settings->atlasPadding = std::max(0, m_settings->atlasPadding);
        }

        ImGui::Checkbox("Trim Whitespace", &m_settings->trimWhitespace);
    }
    */
}

void TextureImportPanel::RenderMipmapPreview() {
    /*
    if (!m_settings->generateMipmaps || m_mipmapPreviews.empty()) return;

    ImGui::Text("Mipmap Preview");
    ImGui::Separator();

    // Mip level selector
    int maxLevel = static_cast<int>(m_mipmapPreviews.size()) - 1;
    ImGui::SliderInt("Mip Level", &m_selectedMipLevel, 0, maxLevel);

    // Show selected mip
    if (m_selectedMipLevel < static_cast<int>(m_mipmapPreviews.size()) && m_mipmapPreviews[m_selectedMipLevel]) {
        int mipWidth = m_originalWidth >> m_selectedMipLevel;
        int mipHeight = m_originalHeight >> m_selectedMipLevel;
        ImGui::Text("Size: %d x %d", mipWidth, mipHeight);

        // Scale preview to reasonable size
        float scale = 128.0f / std::max(mipWidth, mipHeight);
        ImGui::Image((ImTextureID)m_mipmapPreviews[m_selectedMipLevel]->GetID(),
                     ImVec2(mipWidth * scale, mipHeight * scale));
    }
    */
}

void TextureImportPanel::UpdatePreview() {
    if (m_texturePath.empty()) return;

    // Load and generate previews
    // This would create actual preview textures

    GenerateComparisonPreview();
}

void TextureImportPanel::GenerateComparisonPreview() {
    // Would use TextureImporter to generate compressed preview
    // m_originalPreview = load original
    // m_compressedPreview = compress with current settings
}

} // namespace Vehement
