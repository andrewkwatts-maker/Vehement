#include "TextureEditor.hpp"
#include "ModernUI.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <cmath>

// OpenGL for texture creation
#include <glad/glad.h>

// STB image loading (implementation is in Texture.cpp or elsewhere in the engine)
#include <stb_image.h>
#include <stb_image_write.h>

#ifdef _WIN32
#include <Windows.h>
#include <commdlg.h>
#endif

// Store raw texture data for adjustments
static std::vector<unsigned char> s_originalTextureData;
static std::vector<unsigned char> s_adjustedTextureData;

TextureEditor::TextureEditor() = default;
TextureEditor::~TextureEditor() = default;

void TextureEditor::Open(const std::string& assetPath) {
    m_assetPath = assetPath;

    // Extract filename
    std::filesystem::path path(assetPath);
    m_textureName = path.filename().string();

    LoadTexture();
}

void TextureEditor::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    std::string windowTitle = "Texture Editor - " + m_textureName;
    if (m_isDirty) {
        windowTitle += "*";
    }

    if (ImGui::Begin(windowTitle.c_str(), isOpen, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save", "Ctrl+S", false, m_isDirty)) {
                    Save();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export As...")) {
                    ExportTextureAs();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close")) {
                    *isOpen = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Fit to Window", nullptr, &m_fitToWindow);
                ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Zoom")) {
                    m_zoom = 1.0f;
                    m_pan = ImVec2(0.0f, 0.0f);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Main content area
        ImGui::Columns(2, "TextureEditorColumns", true);

        // Left: Preview
        if (ImGui::BeginChild("Preview", ImVec2(0, -35), true)) {
            RenderPreview();
        }
        ImGui::EndChild();

        // Toolbar at bottom of preview
        RenderToolbar();

        ImGui::NextColumn();

        // Right: Properties
        if (ImGui::BeginChild("Properties", ImVec2(0, 0), true)) {
            RenderProperties();
        }
        ImGui::EndChild();

        ImGui::Columns(1);
    }
    ImGui::End();
}

void TextureEditor::RenderToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderFloat("##Zoom", &m_zoom, 0.1f, 10.0f, "%.1fx")) {
        m_fitToWindow = false;
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton("Fit", ImVec2(50, 0))) {
        m_fitToWindow = true;
        m_zoom = 1.0f;
        m_pan = ImVec2(0.0f, 0.0f);
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton("1:1", ImVec2(50, 0))) {
        m_zoom = 1.0f;
        m_fitToWindow = false;
    }

    ImGui::PopStyleVar();
}

void TextureEditor::RenderPreview() {
    if (!m_isLoaded) {
        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        ImVec2 textPos = ImVec2(
            windowSize.x * 0.5f - 50,
            windowSize.y * 0.5f - 10
        );
        ImGui::SetCursorPos(textPos);
        ImGui::TextDisabled("No texture loaded");
        return;
    }

    ImVec2 windowSize = ImGui::GetContentRegionAvail();

    // Calculate texture display size
    float displayWidth = static_cast<float>(m_width) * m_zoom;
    float displayHeight = static_cast<float>(m_height) * m_zoom;

    if (m_fitToWindow && m_width > 0 && m_height > 0) {
        float scaleX = windowSize.x / m_width;
        float scaleY = windowSize.y / m_height;
        float scale = std::min(scaleX, scaleY) * 0.95f; // 95% to leave some margin
        displayWidth = m_width * scale;
        displayHeight = m_height * scale;
    }

    // Center the texture
    ImVec2 imagePos = ImVec2(
        m_pan.x + (windowSize.x - displayWidth) * 0.5f,
        m_pan.y + (windowSize.y - displayHeight) * 0.5f
    );

    ImGui::SetCursorPos(imagePos);

    // Draw checkerboard background for transparency
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 screenPos = ImGui::GetCursorScreenPos();

    const float checkerSize = 10.0f * m_zoom;
    const ImU32 checkerColor1 = IM_COL32(128, 128, 128, 255);
    const ImU32 checkerColor2 = IM_COL32(96, 96, 96, 255);

    for (float y = 0; y < displayHeight; y += checkerSize) {
        for (float x = 0; x < displayWidth; x += checkerSize) {
            bool isEven = (static_cast<int>(x / checkerSize) + static_cast<int>(y / checkerSize)) % 2 == 0;
            ImU32 color = isEven ? checkerColor1 : checkerColor2;
            drawList->AddRectFilled(
                ImVec2(screenPos.x + x, screenPos.y + y),
                ImVec2(screenPos.x + x + checkerSize, screenPos.y + y + checkerSize),
                color
            );
        }
    }

    // Draw texture (would be actual texture in real implementation)
    if (m_textureID) {
        ImGui::Image(m_textureID, ImVec2(displayWidth, displayHeight));
    } else {
        // Placeholder rectangle
        drawList->AddRectFilled(
            screenPos,
            ImVec2(screenPos.x + displayWidth, screenPos.y + displayHeight),
            IM_COL32(100, 100, 150, 255)
        );

        // Draw "Texture Preview" text in center
        const char* text = "Texture Preview";
        ImVec2 textSize = ImGui::CalcTextSize(text);
        drawList->AddText(
            ImVec2(screenPos.x + displayWidth * 0.5f - textSize.x * 0.5f,
                   screenPos.y + displayHeight * 0.5f - textSize.y * 0.5f),
            IM_COL32(200, 200, 200, 255),
            text
        );
    }

    // Draw grid if enabled
    if (m_showGrid) {
        const ImU32 gridColor = IM_COL32(255, 255, 255, 50);
        for (float x = 0; x < displayWidth; x += m_gridSize * m_zoom) {
            drawList->AddLine(
                ImVec2(screenPos.x + x, screenPos.y),
                ImVec2(screenPos.x + x, screenPos.y + displayHeight),
                gridColor
            );
        }
        for (float y = 0; y < displayHeight; y += m_gridSize * m_zoom) {
            drawList->AddLine(
                ImVec2(screenPos.x, screenPos.y + y),
                ImVec2(screenPos.x + displayWidth, screenPos.y + y),
                gridColor
            );
        }
    }
}

void TextureEditor::RenderProperties() {
    ModernUI::GradientHeader("Texture Information", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::Indent();
    ModernUI::CompactStat("File", m_textureName.c_str());

    char dimensionStr[64];
    snprintf(dimensionStr, sizeof(dimensionStr), "%d x %d", m_width, m_height);
    ModernUI::CompactStat("Dimensions", dimensionStr);

    char channelStr[32];
    snprintf(channelStr, sizeof(channelStr), "%d", m_channels);
    ModernUI::CompactStat("Channels", channelStr);

    ModernUI::CompactStat("Format", m_format.c_str());

    char sizeStr[32];
    if (m_fileSize < 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%zu B", m_fileSize);
    } else if (m_fileSize < 1024 * 1024) {
        snprintf(sizeStr, sizeof(sizeStr), "%.2f KB", m_fileSize / 1024.0);
    } else {
        snprintf(sizeStr, sizeof(sizeStr), "%.2f MB", m_fileSize / (1024.0 * 1024.0));
    }
    ModernUI::CompactStat("File Size", sizeStr);

    ImGui::Unindent();

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    if (ModernUI::GradientHeader("Channel Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::Checkbox("Red", &m_showRed);
        ImGui::Checkbox("Green", &m_showGreen);
        ImGui::Checkbox("Blue", &m_showBlue);
        if (m_channels == 4) {
            ImGui::Checkbox("Alpha", &m_showAlpha);
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    if (ModernUI::GradientHeader("Adjustments (Preview)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        if (ImGui::SliderFloat("Brightness", &m_brightness, -1.0f, 1.0f)) {
            UpdatePreview();
        }
        if (ImGui::SliderFloat("Contrast", &m_contrast, 0.0f, 3.0f)) {
            UpdatePreview();
        }

        if (ModernUI::GlowButton("Reset Adjustments", ImVec2(-1, 0))) {
            m_brightness = 0.0f;
            m_contrast = 1.0f;
            UpdatePreview();
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ModernUI::GradientSeparator();
    ImGui::Spacing();

    if (m_mipLevels > 1) {
        if (ModernUI::GradientHeader("Mipmap Levels", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            if (ImGui::SliderInt("Level", &m_currentMipLevel, 0, m_mipLevels - 1)) {
                UpdatePreview();
            }
            ImGui::Unindent();
        }
    }
}

void TextureEditor::LoadTexture() {
    spdlog::info("TextureEditor: Loading texture '{}'", m_assetPath);

    try {
        std::filesystem::path path(m_assetPath);

        if (!std::filesystem::exists(path)) {
            spdlog::error("TextureEditor: File does not exist: '{}'", m_assetPath);
            return;
        }

        m_fileSize = std::filesystem::file_size(path);

        // Load texture using stb_image
        int width, height, channels;
        unsigned char* data = stbi_load(m_assetPath.c_str(), &width, &height, &channels, 0);

        if (!data) {
            spdlog::error("TextureEditor: Failed to load texture with stb_image: {}",
                          stbi_failure_reason());
            m_isLoaded = false;
            return;
        }

        m_width = width;
        m_height = height;
        m_channels = channels;

        // Determine format string based on channels
        switch (channels) {
            case 1: m_format = "R8"; break;
            case 2: m_format = "RG8"; break;
            case 3: m_format = "RGB8"; break;
            case 4: m_format = "RGBA8"; break;
            default: m_format = "Unknown";
        }

        // Calculate mip levels (log2 of max dimension)
        int maxDim = std::max(width, height);
        m_mipLevels = static_cast<int>(std::floor(std::log2(maxDim))) + 1;

        // Store original data for adjustments
        size_t dataSize = static_cast<size_t>(width * height * channels);
        s_originalTextureData.assign(data, data + dataSize);
        s_adjustedTextureData = s_originalTextureData;

        // Create OpenGL texture for ImGui display
        // Note: In a real implementation, this would use Nova::Texture
        // For now, we'll create a raw OpenGL texture
        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Upload texture data
        GLenum format = GL_RGBA;
        switch (channels) {
            case 1: format = GL_RED; break;
            case 2: format = GL_RG; break;
            case 3: format = GL_RGB; break;
            case 4: format = GL_RGBA; break;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Store texture ID for ImGui
        m_textureID = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(textureId));

        stbi_image_free(data);

        m_isLoaded = true;
        spdlog::info("TextureEditor: Texture loaded successfully ({}x{}, {} channels)",
                     m_width, m_height, m_channels);

    } catch (const std::exception& e) {
        spdlog::error("TextureEditor: Failed to load texture: {}", e.what());
        m_isLoaded = false;
    }
}

void TextureEditor::UpdatePreview() {
    if (!m_isLoaded || s_originalTextureData.empty() || !m_textureID) {
        return;
    }

    // Apply brightness and contrast adjustments to texture data
    size_t dataSize = s_originalTextureData.size();
    s_adjustedTextureData.resize(dataSize);

    for (size_t i = 0; i < dataSize; ++i) {
        // Skip alpha channel if present (every 4th byte in RGBA)
        bool isAlpha = (m_channels == 4) && ((i % 4) == 3);
        if (isAlpha) {
            s_adjustedTextureData[i] = s_originalTextureData[i];
            continue;
        }

        // Apply channel visibility
        int channelIndex = i % m_channels;
        bool showChannel = true;
        if (channelIndex == 0 && !m_showRed) showChannel = false;
        if (channelIndex == 1 && !m_showGreen) showChannel = false;
        if (channelIndex == 2 && !m_showBlue) showChannel = false;
        if (channelIndex == 3 && !m_showAlpha) showChannel = false;

        if (!showChannel) {
            s_adjustedTextureData[i] = 0;
            continue;
        }

        // Get original value normalized to 0-1
        float value = static_cast<float>(s_originalTextureData[i]) / 255.0f;

        // Apply contrast (centered at 0.5)
        value = (value - 0.5f) * m_contrast + 0.5f;

        // Apply brightness
        value += m_brightness;

        // Clamp to valid range
        value = std::clamp(value, 0.0f, 1.0f);

        // Convert back to byte
        s_adjustedTextureData[i] = static_cast<unsigned char>(value * 255.0f);
    }

    // Update OpenGL texture with adjusted data
    GLuint textureId = static_cast<GLuint>(reinterpret_cast<intptr_t>(m_textureID));
    glBindTexture(GL_TEXTURE_2D, textureId);

    GLenum format = GL_RGBA;
    switch (m_channels) {
        case 1: format = GL_RED; break;
        case 2: format = GL_RG; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, format,
                    GL_UNSIGNED_BYTE, s_adjustedTextureData.data());

    glBindTexture(GL_TEXTURE_2D, 0);

    // Mark as dirty since adjustments were made
    if (m_brightness != 0.0f || m_contrast != 1.0f) {
        m_isDirty = true;
    }
}

std::string TextureEditor::GetEditorName() const {
    return "Texture Editor - " + m_textureName;
}

bool TextureEditor::IsDirty() const {
    return m_isDirty;
}

void TextureEditor::Save() {
    if (!m_isDirty) {
        return;
    }

    spdlog::info("TextureEditor: Saving texture '{}'", m_assetPath);

    if (s_adjustedTextureData.empty()) {
        spdlog::warn("TextureEditor: No texture data to save");
        return;
    }

    // Determine format from file extension
    std::filesystem::path path(m_assetPath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool success = false;

    if (ext == ".png") {
        success = stbi_write_png(m_assetPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data(), m_width * m_channels) != 0;
    } else if (ext == ".jpg" || ext == ".jpeg") {
        success = stbi_write_jpg(m_assetPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data(), 95) != 0;
    } else if (ext == ".bmp") {
        success = stbi_write_bmp(m_assetPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data()) != 0;
    } else if (ext == ".tga") {
        success = stbi_write_tga(m_assetPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data()) != 0;
    } else {
        // Default to PNG for unknown formats
        std::string pngPath = m_assetPath + ".png";
        success = stbi_write_png(pngPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data(), m_width * m_channels) != 0;
        if (success) {
            spdlog::info("TextureEditor: Saved as PNG (unknown format): {}", pngPath);
        }
    }

    if (success) {
        // Update original data to match saved data
        s_originalTextureData = s_adjustedTextureData;
        m_isDirty = false;
        spdlog::info("TextureEditor: Texture saved successfully");
    } else {
        spdlog::error("TextureEditor: Failed to save texture");
    }
}

std::string TextureEditor::GetAssetPath() const {
    return m_assetPath;
}

void TextureEditor::ExportTextureAs() {
    if (!m_isLoaded || s_adjustedTextureData.empty()) {
        spdlog::warn("TextureEditor: No texture loaded to export");
        return;
    }

    std::string exportPath;

#ifdef _WIN32
    // Windows native file dialog
    char filename[MAX_PATH] = {0};

    // Create default filename based on current texture name
    std::filesystem::path currentPath(m_assetPath);
    std::string defaultName = currentPath.stem().string() + "_export";
    strncpy_s(filename, defaultName.c_str(), MAX_PATH - 1);

    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "PNG Image (*.png)\0*.png\0"
                      "JPEG Image (*.jpg)\0*.jpg\0"
                      "BMP Image (*.bmp)\0*.bmp\0"
                      "TGA Image (*.tga)\0*.tga\0"
                      "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Export Texture As";
    ofn.lpstrDefExt = "png";
    ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        exportPath = filename;
    }
#else
    // Fallback for non-Windows: use a simple path
    spdlog::warn("TextureEditor: Native file dialog not available on this platform");
    exportPath = m_assetPath + "_export.png";
#endif

    if (exportPath.empty()) {
        spdlog::info("TextureEditor: Export cancelled");
        return;
    }

    spdlog::info("TextureEditor: Exporting texture to '{}'", exportPath);

    // Determine format from extension
    std::filesystem::path path(exportPath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool success = false;

    if (ext == ".png") {
        success = stbi_write_png(exportPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data(), m_width * m_channels) != 0;
    } else if (ext == ".jpg" || ext == ".jpeg") {
        success = stbi_write_jpg(exportPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data(), 95) != 0;
    } else if (ext == ".bmp") {
        success = stbi_write_bmp(exportPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data()) != 0;
    } else if (ext == ".tga") {
        success = stbi_write_tga(exportPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data()) != 0;
    } else {
        // Default to PNG
        exportPath += ".png";
        success = stbi_write_png(exportPath.c_str(), m_width, m_height, m_channels,
                                 s_adjustedTextureData.data(), m_width * m_channels) != 0;
    }

    if (success) {
        spdlog::info("TextureEditor: Texture exported successfully to '{}'", exportPath);
    } else {
        spdlog::error("TextureEditor: Failed to export texture to '{}'", exportPath);
    }
}
