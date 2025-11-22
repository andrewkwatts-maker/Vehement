#include "EditorUI.hpp"
#include <algorithm>
#include <cmath>

// Forward declare ImGui functions (in real implementation, include imgui.h)
namespace ImGui {
    bool Begin(const char*, bool* = nullptr, int = 0) { return true; }
    void End() {}
    void SetNextWindowPos(const glm::vec2&, int = 0) {}
    void SetNextWindowSize(const glm::vec2&, int = 0) {}
    void PushStyleVar(int, float) {}
    void PushStyleVar(int, const glm::vec2&) {}
    void PopStyleVar(int = 1) {}
    void PushStyleColor(int, const glm::vec4&) {}
    void PopStyleColor(int = 1) {}
    bool Button(const char*, const glm::vec2& = {}) { return false; }
    bool ImageButton(const char*, void*, const glm::vec2&) { return false; }
    void Image(void*, const glm::vec2&) {}
    void Text(const char*, ...) {}
    void TextColored(const glm::vec4&, const char*, ...) {}
    void SameLine(float = 0, float = -1) {}
    void Separator() {}
    void Spacing() {}
    bool SliderInt(const char*, int*, int, int) { return false; }
    bool SliderFloat(const char*, float*, float, float) { return false; }
    bool Checkbox(const char*, bool*) { return false; }
    bool IsItemHovered() { return false; }
    void SetTooltip(const char*, ...) {}
    void BeginChild(const char*, const glm::vec2& = {}, bool = false, int = 0) {}
    void EndChild() {}
    void Columns(int = 1, const char* = nullptr, bool = true) {}
    void NextColumn() {}
    void ProgressBar(float, const glm::vec2& = {}, const char* = nullptr) {}
    bool BeginPopupModal(const char*, bool* = nullptr, int = 0) { return false; }
    void EndPopup() {}
    void OpenPopup(const char*) {}
    void CloseCurrentPopup() {}
    glm::vec2 GetWindowPos() { return {}; }
    glm::vec2 GetWindowSize() { return {}; }
    glm::vec2 GetCursorScreenPos() { return {}; }
    void SetCursorScreenPos(const glm::vec2&) {}
    void* GetWindowDrawList() { return nullptr; }
    glm::vec2 GetContentRegionAvail() { return {}; }
}

// ImGui window flags
constexpr int ImGuiWindowFlags_NoTitleBar = 1 << 0;
constexpr int ImGuiWindowFlags_NoResize = 1 << 1;
constexpr int ImGuiWindowFlags_NoMove = 1 << 2;
constexpr int ImGuiWindowFlags_NoScrollbar = 1 << 3;
constexpr int ImGuiWindowFlags_NoCollapse = 1 << 5;
constexpr int ImGuiWindowFlags_AlwaysAutoResize = 1 << 6;
constexpr int ImGuiWindowFlags_NoBackground = 1 << 7;

// ImGui style vars
constexpr int ImGuiStyleVar_WindowRounding = 0;
constexpr int ImGuiStyleVar_WindowPadding = 1;
constexpr int ImGuiStyleVar_FrameRounding = 2;
constexpr int ImGuiStyleVar_ItemSpacing = 3;

// ImGui colors
constexpr int ImGuiCol_WindowBg = 0;
constexpr int ImGuiCol_Button = 1;
constexpr int ImGuiCol_ButtonHovered = 2;
constexpr int ImGuiCol_ButtonActive = 3;
constexpr int ImGuiCol_Text = 4;

namespace Vehement {

// ============================================================================
// Placeholder TileMap class (would be defined elsewhere)
// ============================================================================

class TileMap {
public:
    TileType GetTile(int x, int y) const { return TileType::Empty; }
    int GetWidth() const { return 100; }
    int GetHeight() const { return 100; }
    bool InBounds(int x, int y) const { return x >= 0 && y >= 0 && x < GetWidth() && y < GetHeight(); }
};

// ============================================================================
// EditorUI Implementation
// ============================================================================

EditorUI::EditorUI() = default;
EditorUI::~EditorUI() = default;

void EditorUI::Initialize(LevelEditor& editor, TileAtlas& atlas, const Config& config) {
    if (m_initialized) {
        return;
    }

    m_editor = &editor;
    m_atlas = &atlas;
    m_config = config;

    // Initialize tile palette
    TilePalette::Config paletteConfig;
    paletteConfig.thumbnailSize = 48;
    paletteConfig.tilesPerRow = 4;
    m_palette.Initialize(atlas, paletteConfig);

    // Set up palette callbacks
    m_palette.OnTileSelected = [this](TileType type, uint8_t variant) {
        if (m_editor) {
            m_editor->SetSelectedTile(type, variant);
        }
    };

    // Calculate initial layout
    UpdateLayout();

    m_initialized = true;
}

void EditorUI::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_editor = nullptr;
    m_atlas = nullptr;
    m_tileMap = nullptr;
    m_initialized = false;
}

void EditorUI::Render() {
    if (!m_initialized || !m_visible) {
        return;
    }

    // Apply theme
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, m_config.theme.cornerRadius);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, m_config.theme.backgroundColor);
    ImGui::PushStyleColor(ImGuiCol_Button, m_config.theme.buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_config.theme.buttonHoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_config.theme.buttonActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Text, m_config.theme.textColor);

    // Render components
    RenderToolBar();

    if (m_config.showPalette) {
        RenderTilePalette();
    }

    if (m_config.showProperties) {
        RenderPropertiesPanel();
    }

    if (m_config.showMiniMap) {
        RenderMiniMap();
    }

    RenderStatusBar();
    RenderCoinDisplay();

    // Render confirmation dialog if open
    if (m_showConfirmDialog) {
        RenderConfirmDialog();
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(1);
}

void EditorUI::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update palette
    m_palette.Update(deltaTime);

    // Update status timer
    if (m_statusTimer > 0.0f) {
        m_statusTimer -= deltaTime;
        if (m_statusTimer <= 0.0f) {
            ClearStatus();
        }
    }

    // Animate panel slides
    float targetPalette = m_config.showPalette ? 1.0f : 0.0f;
    float targetProperties = m_config.showProperties ? 1.0f : 0.0f;

    m_paletteSlideAnim += (targetPalette - m_paletteSlideAnim) * deltaTime * 8.0f;
    m_propertiesSlideAnim += (targetProperties - m_propertiesSlideAnim) * deltaTime * 8.0f;

    // Update mini-map if dirty
    if (m_miniMapDirty && m_tileMap) {
        UpdateMiniMapTexture();
        m_miniMapDirty = false;
    }
}

void EditorUI::SetPlayerCoins(int coins) {
    m_playerCoins = std::max(0, coins);
}

int EditorUI::GetEditCost() const {
    if (!m_editor) return 0;

    int totalCost = 0;
    const auto& changes = m_editor->GetPendingChanges();

    for (const auto& change : changes) {
        totalCost += CalculateTileCost(change.newType, change.isWall);
    }

    return totalCost;
}

bool EditorUI::CanAffordEdits() const {
    return m_playerCoins >= GetEditCost();
}

void EditorUI::SetCostConfig(const CostConfig& config) {
    m_costConfig = config;
}

void EditorUI::SetVisible(bool visible) {
    m_visible = visible;
}

void EditorUI::SetPaletteVisible(bool visible) {
    m_config.showPalette = visible;
}

void EditorUI::SetPropertiesVisible(bool visible) {
    m_config.showProperties = visible;
}

void EditorUI::SetMiniMapVisible(bool visible) {
    m_config.showMiniMap = visible;
}

void EditorUI::SetTileMap(TileMap* map) {
    m_tileMap = map;
    m_miniMapDirty = true;
}

void EditorUI::SetCameraView(const glm::vec2& center, const glm::vec2& size) {
    m_cameraCenter = center;
    m_cameraSize = size;
}

void EditorUI::ShowStatus(const std::string& message, float duration, bool isError) {
    m_statusMessage = message;
    m_statusTimer = duration;
    m_statusIsError = isError;
}

void EditorUI::ClearStatus() {
    m_statusMessage.clear();
    m_statusTimer = 0.0f;
    m_statusIsError = false;
}

bool EditorUI::IsOverUI(const glm::vec2& screenPos) const {
    if (!m_visible) return false;

    // Check tool bar
    if (screenPos.y < m_config.toolBarHeight) {
        return true;
    }

    // Check status bar
    if (screenPos.y > m_config.windowHeight - m_config.statusBarHeight) {
        return true;
    }

    // Check palette (left side)
    if (m_config.showPalette && screenPos.x < m_config.paletteWidth) {
        return true;
    }

    // Check properties (right side)
    if (m_config.showProperties && screenPos.x > m_config.windowWidth - m_config.propertiesWidth) {
        return true;
    }

    // Check mini-map
    if (m_config.showMiniMap) {
        glm::vec2 miniMapPos{
            m_config.windowWidth - m_config.propertiesWidth - m_config.miniMapSize - 10.0f,
            m_config.windowHeight - m_config.statusBarHeight - m_config.miniMapSize - 10.0f
        };
        if (screenPos.x >= miniMapPos.x && screenPos.x <= miniMapPos.x + m_config.miniMapSize &&
            screenPos.y >= miniMapPos.y && screenPos.y <= miniMapPos.y + m_config.miniMapSize) {
            return true;
        }
    }

    return false;
}

bool EditorUI::OnMouseClick(const glm::vec2& screenPos, int button) {
    if (!m_visible || !m_initialized) {
        return false;
    }

    // Check mini-map click
    if (m_config.showMiniMap) {
        glm::vec2 miniMapPos{
            m_config.windowWidth - m_config.propertiesWidth - m_config.miniMapSize - 10.0f,
            m_config.windowHeight - m_config.statusBarHeight - m_config.miniMapSize - 10.0f
        };
        if (screenPos.x >= miniMapPos.x && screenPos.x <= miniMapPos.x + m_config.miniMapSize &&
            screenPos.y >= miniMapPos.y && screenPos.y <= miniMapPos.y + m_config.miniMapSize) {

            glm::vec2 worldPos = MiniMapToWorld(screenPos);
            if (OnMiniMapClick) {
                OnMiniMapClick(worldPos);
            }
            return true;
        }
    }

    // Check palette click
    if (m_config.showPalette && screenPos.x < m_config.paletteWidth) {
        return m_palette.OnClick(screenPos);
    }

    return IsOverUI(screenPos);
}

void EditorUI::OnMouseMove(const glm::vec2& screenPos) {
    m_mousePos = screenPos;

    if (m_config.showPalette) {
        m_palette.OnMouseMove(screenPos);
    }
}

bool EditorUI::OnKeyPress(int key, int mods) {
    if (!m_visible || !m_editor) {
        return false;
    }

    // Toggle panels
    if (key == 'P' && (mods & 2)) { // Ctrl+P
        SetPaletteVisible(!m_config.showPalette);
        return true;
    }
    if (key == 'H' && (mods & 2)) { // Ctrl+H
        SetPropertiesVisible(!m_config.showProperties);
        return true;
    }
    if (key == 'M' && (mods & 2)) { // Ctrl+M
        SetMiniMapVisible(!m_config.showMiniMap);
        return true;
    }

    // Save/Load
    if (key == 'S' && (mods & 2)) { // Ctrl+S
        if (OnSave) OnSave();
        return true;
    }
    if (key == 'O' && (mods & 2)) { // Ctrl+O
        if (OnLoad) OnLoad();
        return true;
    }

    return false;
}

// ============================================================================
// Private Render Methods
// ============================================================================

void EditorUI::RenderToolBar() {
    ImGui::SetNextWindowPos(glm::vec2{0.0f, 0.0f});
    ImGui::SetNextWindowSize(glm::vec2{static_cast<float>(m_config.windowWidth), m_config.toolBarHeight});

    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("ToolBar", nullptr, flags)) {
        // Tool buttons
        RenderToolButton(LevelEditor::Tool::Select, "V", "Select (V)");
        ImGui::SameLine();
        RenderToolButton(LevelEditor::Tool::Paint, "P", "Paint (P/B)");
        ImGui::SameLine();
        RenderToolButton(LevelEditor::Tool::Erase, "E", "Erase (E)");
        ImGui::SameLine();
        RenderToolButton(LevelEditor::Tool::Fill, "G", "Fill (G)");
        ImGui::SameLine();
        RenderToolButton(LevelEditor::Tool::Rectangle, "R", "Rectangle (R)");
        ImGui::SameLine();
        RenderToolButton(LevelEditor::Tool::Eyedropper, "I", "Eyedropper (I)");

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        // Undo/Redo
        RenderUndoRedoButtons();

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        // Brush size
        RenderBrushSizeSlider();

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        // Wall mode toggle
        if (m_editor) {
            bool wallMode = m_editor->IsWallMode();
            if (ImGui::Checkbox("Wall Mode", &wallMode)) {
                m_editor->SetWallMode(wallMode);
            }
        }

        ImGui::SameLine();

        // Spacer to push right-side buttons
        float spacerWidth = m_config.windowWidth - 800.0f; // Adjust as needed
        ImGui::SameLine(0, spacerWidth);

        // Right side buttons
        if (ImGui::Button("Generate Town")) {
            if (OnGenerateTown) OnGenerateTown();
        }
        ImGui::SameLine();

        if (ImGui::Button("Save")) {
            if (OnSave) OnSave();
        }
        ImGui::SameLine();

        if (ImGui::Button("Load")) {
            if (OnLoad) OnLoad();
        }
        ImGui::SameLine();

        if (ImGui::Button("Exit")) {
            if (OnExit) OnExit();
        }
    }
    ImGui::End();
}

void EditorUI::RenderToolButton(LevelEditor::Tool tool, const char* label, const char* tooltip) {
    if (!m_editor) return;

    bool isSelected = (m_editor->GetTool() == tool);

    if (isSelected) {
        ImGui::PushStyleColor(ImGuiCol_Button, m_config.theme.accentColor);
    }

    if (ImGui::Button(label, glm::vec2{30.0f, 30.0f})) {
        m_editor->SetTool(tool);
    }

    if (isSelected) {
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void EditorUI::RenderUndoRedoButtons() {
    if (!m_editor) return;

    bool canUndo = m_editor->CanUndo();
    bool canRedo = m_editor->CanRedo();

    if (!canUndo) {
        ImGui::PushStyleColor(ImGuiCol_Button, glm::vec4{0.3f, 0.3f, 0.3f, 0.5f});
    }
    if (ImGui::Button("Undo") && canUndo) {
        m_editor->Undo();
    }
    if (!canUndo) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Undo (Ctrl+Z) - %zu available", m_editor->GetUndoCount());
    }

    ImGui::SameLine();

    if (!canRedo) {
        ImGui::PushStyleColor(ImGuiCol_Button, glm::vec4{0.3f, 0.3f, 0.3f, 0.5f});
    }
    if (ImGui::Button("Redo") && canRedo) {
        m_editor->Redo();
    }
    if (!canRedo) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Redo (Ctrl+Y) - %zu available", m_editor->GetRedoCount());
    }
}

void EditorUI::RenderBrushSizeSlider() {
    if (!m_editor) return;

    int brushSize = m_editor->GetBrushSize();
    ImGui::Text("Brush:");
    ImGui::SameLine();
    if (ImGui::SliderInt("##BrushSize", &brushSize, 1, 10)) {
        m_editor->SetBrushSize(brushSize);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Brush Size: %d (use [ and ] keys)", brushSize);
    }
}

void EditorUI::RenderTilePalette() {
    float xOffset = (m_paletteSlideAnim - 1.0f) * m_config.paletteWidth;

    ImGui::SetNextWindowPos(glm::vec2{xOffset, m_config.toolBarHeight});
    ImGui::SetNextWindowSize(glm::vec2{m_config.paletteWidth,
        static_cast<float>(m_config.windowHeight) - m_config.toolBarHeight - m_config.statusBarHeight});

    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("TilePalette", nullptr, flags)) {
        ImGui::Text("Tile Palette");
        ImGui::Separator();

        m_palette.SetBounds(
            glm::vec2{xOffset + 10.0f, m_config.toolBarHeight + 30.0f},
            glm::vec2{m_config.paletteWidth - 20.0f,
                static_cast<float>(m_config.windowHeight) - m_config.toolBarHeight - m_config.statusBarHeight - 40.0f}
        );
        m_palette.Render();
    }
    ImGui::End();
}

void EditorUI::RenderPropertiesPanel() {
    float xOffset = static_cast<float>(m_config.windowWidth) - m_config.propertiesWidth +
                    (1.0f - m_propertiesSlideAnim) * m_config.propertiesWidth;

    ImGui::SetNextWindowPos(glm::vec2{xOffset, m_config.toolBarHeight});
    ImGui::SetNextWindowSize(glm::vec2{m_config.propertiesWidth,
        static_cast<float>(m_config.windowHeight) - m_config.toolBarHeight - m_config.statusBarHeight});

    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("Properties", nullptr, flags)) {
        ImGui::Text("Properties");
        ImGui::Separator();

        RenderTileInfo();
        ImGui::Separator();

        RenderWallSettings();
        ImGui::Separator();

        RenderCostBreakdown();
        ImGui::Separator();

        // Apply/Discard buttons
        if (m_editor && !m_editor->GetPendingChanges().empty()) {
            ImGui::Spacing();

            bool canAfford = CanAffordEdits();
            if (!canAfford) {
                ImGui::PushStyleColor(ImGuiCol_Button, m_config.theme.errorColor);
            }

            if (ImGui::Button("Apply Changes", glm::vec2{-1, 30})) {
                if (canAfford && OnApplyChanges) {
                    OnApplyChanges();
                } else if (!canAfford) {
                    ShowStatus("Not enough coins!", 3.0f, true);
                }
            }

            if (!canAfford) {
                ImGui::PopStyleColor();
            }

            if (ImGui::Button("Discard Changes", glm::vec2{-1, 30})) {
                if (OnDiscardChanges) {
                    OnDiscardChanges();
                }
            }
        }
    }
    ImGui::End();
}

void EditorUI::RenderWallSettings() {
    if (!m_editor) return;

    ImGui::Text("Wall Settings");

    bool wallMode = m_editor->IsWallMode();
    if (ImGui::Checkbox("Enable Walls", &wallMode)) {
        m_editor->SetWallMode(wallMode);
    }

    if (wallMode) {
        float height = m_editor->GetWallHeight();
        if (ImGui::SliderFloat("Height", &height, 0.5f, 10.0f)) {
            m_editor->SetWallHeight(height);
        }
    }
}

void EditorUI::RenderTileInfo() {
    if (!m_editor) return;

    ImGui::Text("Selected Tile");

    TileType selectedType = m_editor->GetSelectedTile();
    const char* tileName = GetTileDisplayName(selectedType);

    ImGui::Text("Name: %s", tileName);

    // Show thumbnail
    if (m_atlas) {
        auto texture = m_atlas->GetTexture(selectedType);
        if (texture) {
            void* texId = reinterpret_cast<void*>(static_cast<uintptr_t>(texture->GetID()));
            ImGui::Image(texId, glm::vec2{64.0f, 64.0f});
        }
    }

    // Show cost
    int cost = CalculateTileCost(selectedType, m_editor->IsWallMode());
    ImGui::Text("Cost: %d coins", cost);
}

void EditorUI::RenderCostBreakdown() {
    if (!m_editor) return;

    ImGui::Text("Cost Breakdown");

    int totalCost = GetEditCost();
    size_t changeCount = m_editor->GetPendingChanges().size();

    ImGui::Text("Pending changes: %zu", changeCount);
    ImGui::Text("Total cost: %d coins", totalCost);

    // Progress bar showing affordability
    float ratio = (m_playerCoins > 0) ? std::min(1.0f, static_cast<float>(totalCost) / m_playerCoins) : 1.0f;

    glm::vec4 barColor = CanAffordEdits() ? m_config.theme.successColor : m_config.theme.errorColor;
    ImGui::PushStyleColor(ImGuiCol_Button, barColor);
    ImGui::ProgressBar(ratio, glm::vec2{-1, 0}, nullptr);
    ImGui::PopStyleColor();

    if (!CanAffordEdits()) {
        ImGui::TextColored(m_config.theme.errorColor, "Insufficient coins!");
    }
}

void EditorUI::RenderMiniMap() {
    if (!m_tileMap) return;

    glm::vec2 miniMapPos{
        static_cast<float>(m_config.windowWidth) - m_config.propertiesWidth - m_config.miniMapSize - 10.0f,
        static_cast<float>(m_config.windowHeight) - m_config.statusBarHeight - m_config.miniMapSize - 10.0f
    };

    ImGui::SetNextWindowPos(miniMapPos);
    ImGui::SetNextWindowSize(glm::vec2{m_config.miniMapSize, m_config.miniMapSize});

    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("MiniMap", nullptr, flags)) {
        // Draw mini-map texture
        if (m_miniMapTexture) {
            void* texId = reinterpret_cast<void*>(static_cast<uintptr_t>(m_miniMapTexture->GetID()));
            ImGui::Image(texId, glm::vec2{m_config.miniMapSize - 16.0f, m_config.miniMapSize - 16.0f});
        }

        // Draw camera viewport indicator
        // This would use ImGui draw list in real implementation
    }
    ImGui::End();
}

void EditorUI::RenderStatusBar() {
    ImGui::SetNextWindowPos(glm::vec2{0.0f, static_cast<float>(m_config.windowHeight) - m_config.statusBarHeight});
    ImGui::SetNextWindowSize(glm::vec2{static_cast<float>(m_config.windowWidth), m_config.statusBarHeight});

    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("StatusBar", nullptr, flags)) {
        // Tool info
        if (m_editor) {
            ImGui::Text("Tool: %s", LevelEditor::GetToolName(m_editor->GetTool()));
            ImGui::SameLine();
            ImGui::Text(" | Brush: %d", m_editor->GetBrushSize());
            ImGui::SameLine();
        }

        // Status message
        if (!m_statusMessage.empty()) {
            ImGui::SameLine();
            ImGui::Text(" | ");
            ImGui::SameLine();

            if (m_statusIsError) {
                ImGui::TextColored(m_config.theme.errorColor, "%s", m_statusMessage.c_str());
            } else {
                ImGui::Text("%s", m_statusMessage.c_str());
            }
        }

        // Mouse position
        ImGui::SameLine(static_cast<float>(m_config.windowWidth) - 150.0f);
        ImGui::Text("Pos: %.0f, %.0f", m_mousePos.x, m_mousePos.y);
    }
    ImGui::End();
}

void EditorUI::RenderCoinDisplay() {
    // Coin display in top-right area
    glm::vec2 coinPos{
        static_cast<float>(m_config.windowWidth) - 200.0f,
        5.0f
    };

    ImGui::SetNextWindowPos(coinPos);
    ImGui::SetNextWindowSize(glm::vec2{190.0f, 40.0f});

    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("CoinDisplay", nullptr, flags)) {
        ImGui::TextColored(m_config.theme.accentColor, "Coins: %d", m_playerCoins);

        int editCost = GetEditCost();
        if (editCost > 0) {
            ImGui::SameLine();
            glm::vec4 costColor = CanAffordEdits() ? m_config.theme.successColor : m_config.theme.errorColor;
            ImGui::TextColored(costColor, "(-%d)", editCost);
        }
    }
    ImGui::End();
}

void EditorUI::RenderConfirmDialog() {
    ImGui::OpenPopup("Confirm");

    if (ImGui::BeginPopupModal("Confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", m_confirmMessage.c_str());
        ImGui::Separator();

        if (ImGui::Button("Yes", glm::vec2{100, 0})) {
            if (m_confirmAction) {
                m_confirmAction();
            }
            m_showConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("No", glm::vec2{100, 0})) {
            m_showConfirmDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void EditorUI::UpdateMiniMapTexture() {
    // In a real implementation, this would render the tile map to a texture
    // for display in the mini-map
}

glm::vec2 EditorUI::MiniMapToWorld(const glm::vec2& miniMapPos) const {
    if (!m_tileMap) return glm::vec2{0.0f};

    glm::vec2 miniMapOrigin{
        static_cast<float>(m_config.windowWidth) - m_config.propertiesWidth - m_config.miniMapSize - 10.0f,
        static_cast<float>(m_config.windowHeight) - m_config.statusBarHeight - m_config.miniMapSize - 10.0f
    };

    glm::vec2 relPos = miniMapPos - miniMapOrigin;
    float normalizedX = relPos.x / m_config.miniMapSize;
    float normalizedY = relPos.y / m_config.miniMapSize;

    return glm::vec2{
        normalizedX * m_tileMap->GetWidth(),
        normalizedY * m_tileMap->GetHeight()
    };
}

glm::vec2 EditorUI::WorldToMiniMap(const glm::vec2& worldPos) const {
    if (!m_tileMap) return glm::vec2{0.0f};

    float normalizedX = worldPos.x / m_tileMap->GetWidth();
    float normalizedY = worldPos.y / m_tileMap->GetHeight();

    glm::vec2 miniMapOrigin{
        static_cast<float>(m_config.windowWidth) - m_config.propertiesWidth - m_config.miniMapSize - 10.0f,
        static_cast<float>(m_config.windowHeight) - m_config.statusBarHeight - m_config.miniMapSize - 10.0f
    };

    return miniMapOrigin + glm::vec2{normalizedX * m_config.miniMapSize, normalizedY * m_config.miniMapSize};
}

int EditorUI::CalculateTileCost(TileType type, bool isWall) const {
    int cost = m_costConfig.baseTileCost;

    // Apply category multiplier
    int category = GetTileCategory(type);
    switch (category) {
        case 7: // Foliage
            cost *= m_costConfig.foliageMultiplier;
            break;
        case 9: // Objects
            cost *= m_costConfig.objectMultiplier;
            break;
        default:
            break;
    }

    // Wall multiplier
    if (isWall) {
        cost *= m_costConfig.wallMultiplier;
    }

    // Erasing is free/cheap
    if (type == TileType::Empty) {
        cost = m_costConfig.eraseCost;
    }

    return cost;
}

void EditorUI::UpdateLayout() {
    m_layout.toolBarPos = glm::vec2{0.0f, 0.0f};
    m_layout.toolBarSize = glm::vec2{static_cast<float>(m_config.windowWidth), m_config.toolBarHeight};

    m_layout.palettePos = glm::vec2{0.0f, m_config.toolBarHeight};
    m_layout.paletteSize = glm::vec2{m_config.paletteWidth,
        static_cast<float>(m_config.windowHeight) - m_config.toolBarHeight - m_config.statusBarHeight};

    m_layout.propertiesPos = glm::vec2{
        static_cast<float>(m_config.windowWidth) - m_config.propertiesWidth,
        m_config.toolBarHeight
    };
    m_layout.propertiesSize = glm::vec2{m_config.propertiesWidth,
        static_cast<float>(m_config.windowHeight) - m_config.toolBarHeight - m_config.statusBarHeight};

    m_layout.statusBarPos = glm::vec2{0.0f, static_cast<float>(m_config.windowHeight) - m_config.statusBarHeight};
    m_layout.statusBarSize = glm::vec2{static_cast<float>(m_config.windowWidth), m_config.statusBarHeight};

    m_layout.miniMapPos = glm::vec2{
        static_cast<float>(m_config.windowWidth) - m_config.propertiesWidth - m_config.miniMapSize - 10.0f,
        static_cast<float>(m_config.windowHeight) - m_config.statusBarHeight - m_config.miniMapSize - 10.0f
    };
    m_layout.miniMapSize = glm::vec2{m_config.miniMapSize, m_config.miniMapSize};
}

glm::vec2 EditorUI::GetToolBarBounds() const {
    return m_layout.toolBarSize;
}

glm::vec2 EditorUI::GetPaletteBounds() const {
    return m_layout.paletteSize;
}

glm::vec2 EditorUI::GetPropertiesBounds() const {
    return m_layout.propertiesSize;
}

glm::vec2 EditorUI::GetMiniMapBounds() const {
    return m_layout.miniMapSize;
}

glm::vec2 EditorUI::GetStatusBarBounds() const {
    return m_layout.statusBarSize;
}

} // namespace Vehement
