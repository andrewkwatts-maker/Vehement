#include "TextureEditor.hpp"
#include "ModernUI.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>

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
                    // TODO: Export dialog
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

    // TODO: Actual texture loading using engine's texture system
    // For now, simulate loading

    try {
        std::filesystem::path path(m_assetPath);

        if (!std::filesystem::exists(path)) {
            spdlog::error("TextureEditor: File does not exist: '{}'", m_assetPath);
            return;
        }

        m_fileSize = std::filesystem::file_size(path);

        // Simulate texture properties (would be loaded from actual file)
        m_width = 512;
        m_height = 512;
        m_channels = 4;
        m_format = "RGBA8";
        m_mipLevels = 1;

        m_isLoaded = true;
        spdlog::info("TextureEditor: Texture loaded successfully");

    } catch (const std::exception& e) {
        spdlog::error("TextureEditor: Failed to load texture: {}", e.what());
        m_isLoaded = false;
    }
}

void TextureEditor::UpdatePreview() {
    // TODO: Update preview with current adjustments
    // Would regenerate texture with brightness/contrast applied
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

    // TODO: Actual save implementation

    m_isDirty = false;
}

std::string TextureEditor::GetAssetPath() const {
    return m_assetPath;
}
