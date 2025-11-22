#include "CultureSelection.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace Vehement {
namespace RTS {

// =============================================================================
// CultureSelectionUI Implementation
// =============================================================================

CultureSelectionUI::CultureSelectionUI() = default;

CultureSelectionUI::~CultureSelectionUI() {
    Shutdown();
}

bool CultureSelectionUI::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Ensure culture manager is initialized
    if (!CultureManager::Instance().IsInitialized()) {
        if (!CultureManager::Instance().Initialize()) {
            return false;
        }
    }

    // Initialize culture cards from culture data
    InitializeCultureCards();

    // Calculate initial layout
    CalculateLayout();

    m_initialized = true;
    return true;
}

void CultureSelectionUI::Shutdown() {
    m_cards.clear();
    m_backgroundTexture.reset();
    m_cardFrameTexture.reset();
    m_buttonTexture.reset();
    m_initialized = false;
}

void CultureSelectionUI::Show() {
    if (!m_initialized) {
        Initialize();
    }

    m_visible = true;
    m_state = SelectionState::Browsing;
    m_fadeInProgress = 0.0f;
    m_hoveredCard = -1;

    // Reset card animations
    for (auto& card : m_cards) {
        card.isHovered = false;
        card.isSelected = false;
        card.hoverAnimationProgress = 0.0f;
        card.selectAnimationProgress = 0.0f;
    }
}

void CultureSelectionUI::Hide() {
    m_visible = false;
}

std::optional<CultureType> CultureSelectionUI::GetSelectedCulture() const {
    if (m_state == SelectionState::Confirmed) {
        return m_confirmedCulture;
    }
    return std::nullopt;
}

void CultureSelectionUI::Update(float deltaTime) {
    if (!m_visible) {
        return;
    }

    // Update fade-in animation
    if (m_fadeInProgress < 1.0f) {
        m_fadeInProgress = std::min(1.0f, m_fadeInProgress + deltaTime * 3.0f);
    }

    // Update card animations
    UpdateCardAnimations(deltaTime);

    // Update detail view animation
    if (m_state == SelectionState::Previewing) {
        m_detailViewProgress = std::min(1.0f, m_detailViewProgress + deltaTime * 4.0f);
    } else {
        m_detailViewProgress = std::max(0.0f, m_detailViewProgress - deltaTime * 4.0f);
    }

    // Update confirm dialog animation
    if (m_state == SelectionState::Confirming) {
        m_confirmDialogProgress = std::min(1.0f, m_confirmDialogProgress + deltaTime * 5.0f);
    } else {
        m_confirmDialogProgress = std::max(0.0f, m_confirmDialogProgress - deltaTime * 5.0f);
    }
}

void CultureSelectionUI::Render(Nova::Renderer& renderer) {
    if (!m_visible) {
        return;
    }

    // Apply fade-in alpha
    float alpha = m_animationsEnabled ? m_fadeInProgress : 1.0f;

    // Render background
    // renderer.DrawRect(0, 0, m_screenWidth, m_screenHeight, 0x000000, alpha * 0.8f);

    // Render title
    // renderer.DrawText("Select Your Culture", m_screenWidth / 2, 50, ...);

    // Render based on state
    switch (m_state) {
        case SelectionState::Browsing:
            RenderGrid(renderer);
            break;

        case SelectionState::Previewing:
            RenderGrid(renderer);
            RenderDetailView(renderer);
            break;

        case SelectionState::Confirming:
            RenderGrid(renderer);
            RenderDetailView(renderer);
            RenderConfirmDialog(renderer);
            break;

        case SelectionState::Confirmed:
        case SelectionState::Cancelled:
            // Fade out and hide
            break;
    }
}

void CultureSelectionUI::RenderImGui() {
    // ImGui debug interface for development
    // Shows internal state and allows testing different cultures
}

void CultureSelectionUI::OnMouseMove(float x, float y) {
    if (!m_visible || m_state == SelectionState::Confirmed) {
        return;
    }

    UpdateHoverStates(x, y);
}

void CultureSelectionUI::OnMouseClick(float x, float y, int button) {
    if (!m_visible || m_state == SelectionState::Confirmed) {
        return;
    }

    if (button == 0) { // Left click
        switch (m_state) {
            case SelectionState::Browsing: {
                int cardIndex = GetCardAtPosition(x, y);
                if (cardIndex >= 0) {
                    SelectCulture(cardIndex);
                }
                break;
            }

            case SelectionState::Previewing: {
                // Check if click is on confirm button
                float detailCenterX = m_screenWidth / 2.0f;
                float detailCenterY = m_screenHeight / 2.0f;
                float confirmBtnX = detailCenterX + 100.0f;
                float confirmBtnY = detailCenterY + 200.0f;
                float btnWidth = 120.0f;
                float btnHeight = 40.0f;

                if (x >= confirmBtnX - btnWidth / 2 && x <= confirmBtnX + btnWidth / 2 &&
                    y >= confirmBtnY - btnHeight / 2 && y <= confirmBtnY + btnHeight / 2) {
                    m_state = SelectionState::Confirming;
                }

                // Check if click is on back button
                float backBtnX = detailCenterX - 100.0f;
                if (x >= backBtnX - btnWidth / 2 && x <= backBtnX + btnWidth / 2 &&
                    y >= confirmBtnY - btnHeight / 2 && y <= confirmBtnY + btnHeight / 2) {
                    BackToGrid();
                }
                break;
            }

            case SelectionState::Confirming: {
                // Check confirm/cancel buttons
                float dialogCenterX = m_screenWidth / 2.0f;
                float dialogCenterY = m_screenHeight / 2.0f;
                float confirmBtnX = dialogCenterX + 80.0f;
                float cancelBtnX = dialogCenterX - 80.0f;
                float btnY = dialogCenterY + 50.0f;
                float btnWidth = 100.0f;
                float btnHeight = 35.0f;

                if (x >= confirmBtnX - btnWidth / 2 && x <= confirmBtnX + btnWidth / 2 &&
                    y >= btnY - btnHeight / 2 && y <= btnY + btnHeight / 2) {
                    ConfirmSelection();
                }

                if (x >= cancelBtnX - btnWidth / 2 && x <= cancelBtnX + btnWidth / 2 &&
                    y >= btnY - btnHeight / 2 && y <= btnY + btnHeight / 2) {
                    m_state = SelectionState::Previewing;
                }
                break;
            }

            default:
                break;
        }
    } else if (button == 1) { // Right click - go back
        switch (m_state) {
            case SelectionState::Previewing:
                BackToGrid();
                break;
            case SelectionState::Confirming:
                m_state = SelectionState::Previewing;
                break;
            default:
                break;
        }
    }
}

void CultureSelectionUI::OnKeyPress(int key, bool pressed) {
    if (!m_visible || !pressed) {
        return;
    }

    // Escape key handling
    if (key == 256) { // GLFW_KEY_ESCAPE or similar
        switch (m_state) {
            case SelectionState::Browsing:
                CancelSelection();
                break;
            case SelectionState::Previewing:
                BackToGrid();
                break;
            case SelectionState::Confirming:
                m_state = SelectionState::Previewing;
                break;
            default:
                break;
        }
    }

    // Enter key to confirm
    if (key == 257) { // GLFW_KEY_ENTER
        switch (m_state) {
            case SelectionState::Previewing:
                m_state = SelectionState::Confirming;
                break;
            case SelectionState::Confirming:
                ConfirmSelection();
                break;
            default:
                break;
        }
    }

    // Number keys 1-8 for quick selection
    if (key >= 49 && key <= 56) { // '1' to '8'
        int cardIndex = key - 49;
        if (cardIndex < static_cast<int>(m_cards.size())) {
            SelectCulture(cardIndex);
        }
    }
}

void CultureSelectionUI::OnScroll(float delta) {
    if (!m_visible || m_state != SelectionState::Browsing) {
        return;
    }

    // Handle scrolling for small screens
    m_scrollOffset = std::clamp(m_scrollOffset - delta * 30.0f, 0.0f, m_maxScroll);
}

void CultureSelectionUI::SaveSelectionToFirebase(const std::string& userId) {
    if (!m_confirmedCulture.has_value()) {
        return;
    }

    nlohmann::json data;
    data["culture"] = static_cast<int>(m_confirmedCulture.value());
    data["cultureName"] = CultureTypeToString(m_confirmedCulture.value());
    data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string path = "players/" + userId + "/rts/culture";
    FirebaseManager::Instance().SetValue(path, data);
}

void CultureSelectionUI::LoadSelectionFromFirebase(
    const std::string& userId,
    std::function<void(std::optional<CultureType>)> callback) {

    std::string path = "players/" + userId + "/rts/culture";

    FirebaseManager::Instance().GetValue(path, [callback](const nlohmann::json& data) {
        if (data.is_null() || !data.contains("culture")) {
            callback(std::nullopt);
            return;
        }

        int cultureInt = data["culture"].get<int>();
        if (cultureInt >= 0 && cultureInt < static_cast<int>(CultureType::Count)) {
            callback(static_cast<CultureType>(cultureInt));
        } else {
            callback(std::nullopt);
        }
    });
}

void CultureSelectionUI::ForceSelection(CultureType culture) {
    m_confirmedCulture = culture;
    m_state = SelectionState::Confirmed;

    // Find and mark the selected card
    for (size_t i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i].type == culture) {
            m_selectedCard = static_cast<int>(i);
            m_cards[i].isSelected = true;
            break;
        }
    }

    if (m_onCultureSelected) {
        m_onCultureSelected(culture);
    }
}

void CultureSelectionUI::SetScreenSize(float width, float height) {
    m_screenWidth = width;
    m_screenHeight = height;
    CalculateLayout();
}

// =============================================================================
// Private Implementation
// =============================================================================

void CultureSelectionUI::InitializeCultureCards() {
    m_cards.clear();

    const auto& cultures = CultureManager::Instance().GetAllCultures();
    m_cards.reserve(cultures.size());

    for (const auto& culture : cultures) {
        CultureUICard card;
        card.type = culture.type;
        card.name = culture.name;
        card.description = culture.description;
        card.shortDescription = culture.shortDescription;

        // Generate bonus summary strings
        const auto& b = culture.bonuses;

        // Helper lambda to format bonus
        auto formatBonus = [](const std::string& name, float value) -> std::string {
            int percent = static_cast<int>((value - 1.0f) * 100.0f);
            std::stringstream ss;
            if (percent > 0) {
                ss << "+" << percent << "% " << name;
            } else {
                ss << percent << "% " << name;
            }
            return ss.str();
        };

        // Collect bonuses and penalties
        if (b.buildSpeedMultiplier != 1.0f) {
            (b.buildSpeedMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Build Speed", b.buildSpeedMultiplier));
        }
        if (b.wallHPMultiplier != 1.0f) {
            (b.wallHPMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Wall HP", b.wallHPMultiplier));
        }
        if (b.towerDamageMultiplier != 1.0f) {
            (b.towerDamageMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Tower Damage", b.towerDamageMultiplier));
        }
        if (b.defenseMultiplier != 1.0f) {
            (b.defenseMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Defense", b.defenseMultiplier));
        }
        if (b.gatherSpeedMultiplier != 1.0f) {
            (b.gatherSpeedMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Gather Speed", b.gatherSpeedMultiplier));
        }
        if (b.tradeMultiplier != 1.0f) {
            (b.tradeMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Trade Profits", b.tradeMultiplier));
        }
        if (b.productionMultiplier != 1.0f) {
            (b.productionMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Production", b.productionMultiplier));
        }
        if (b.stealthMultiplier != 1.0f) {
            (b.stealthMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Stealth", b.stealthMultiplier));
        }
        if (b.visionMultiplier != 1.0f) {
            (b.visionMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Vision", b.visionMultiplier));
        }
        if (b.unitSpeedMultiplier != 1.0f) {
            (b.unitSpeedMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Unit Speed", b.unitSpeedMultiplier));
        }
        if (b.buildCostMultiplier != 1.0f) {
            // Inverted - lower cost is a bonus
            (b.buildCostMultiplier < 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Build Cost", b.buildCostMultiplier));
        }
        if (b.buildingHPMultiplier != 1.0f) {
            (b.buildingHPMultiplier > 1.0f ? card.bonusSummary : card.penaltySummary)
                .push_back(formatBonus("Building HP", b.buildingHPMultiplier));
        }

        // Copy unique abilities
        card.uniqueAbilities = culture.uniqueAbilities;

        // Convert unique buildings to strings
        for (BuildingType building : culture.uniqueBuildings) {
            card.uniqueBuildings.push_back(BuildingTypeToString(building));
        }

        m_cards.push_back(std::move(card));
    }
}

void CultureSelectionUI::UpdateCardAnimations(float deltaTime) {
    if (!m_animationsEnabled) {
        return;
    }

    for (size_t i = 0; i < m_cards.size(); ++i) {
        auto& card = m_cards[i];

        // Update hover animation
        float targetHover = (static_cast<int>(i) == m_hoveredCard) ? 1.0f : 0.0f;
        float hoverSpeed = 8.0f;
        card.hoverAnimationProgress += (targetHover - card.hoverAnimationProgress) * hoverSpeed * deltaTime;

        // Update select animation
        float targetSelect = card.isSelected ? 1.0f : 0.0f;
        float selectSpeed = 6.0f;
        card.selectAnimationProgress += (targetSelect - card.selectAnimationProgress) * selectSpeed * deltaTime;
    }
}

void CultureSelectionUI::UpdateHoverStates(float mouseX, float mouseY) {
    int oldHovered = m_hoveredCard;
    m_hoveredCard = GetCardAtPosition(mouseX, mouseY);

    if (m_hoveredCard != oldHovered) {
        if (oldHovered >= 0 && oldHovered < static_cast<int>(m_cards.size())) {
            m_cards[oldHovered].isHovered = false;
        }
        if (m_hoveredCard >= 0 && m_hoveredCard < static_cast<int>(m_cards.size())) {
            m_cards[m_hoveredCard].isHovered = true;
        }
    }
}

void CultureSelectionUI::SelectCulture(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= static_cast<int>(m_cards.size())) {
        return;
    }

    // Deselect previous
    if (m_selectedCard >= 0 && m_selectedCard < static_cast<int>(m_cards.size())) {
        m_cards[m_selectedCard].isSelected = false;
    }

    // Select new
    m_selectedCard = cardIndex;
    m_cards[cardIndex].isSelected = true;
    m_state = SelectionState::Previewing;
}

void CultureSelectionUI::ConfirmSelection() {
    if (m_selectedCard < 0 || m_selectedCard >= static_cast<int>(m_cards.size())) {
        return;
    }

    m_confirmedCulture = m_cards[m_selectedCard].type;
    m_state = SelectionState::Confirmed;

    if (m_onCultureSelected) {
        m_onCultureSelected(m_confirmedCulture.value());
    }
}

void CultureSelectionUI::CancelSelection() {
    m_state = SelectionState::Cancelled;

    if (m_onCancelled) {
        m_onCancelled();
    }
}

void CultureSelectionUI::BackToGrid() {
    if (m_selectedCard >= 0 && m_selectedCard < static_cast<int>(m_cards.size())) {
        m_cards[m_selectedCard].isSelected = false;
    }
    m_selectedCard = -1;
    m_state = SelectionState::Browsing;
}

void CultureSelectionUI::RenderGrid(Nova::Renderer& renderer) {
    // Render each card in the grid
    for (size_t i = 0; i < m_cards.size(); ++i) {
        int row = static_cast<int>(i) / CARDS_PER_ROW;
        int col = static_cast<int>(i) % CARDS_PER_ROW;

        float x = m_gridStartX + col * (m_cardWidth + CARD_PADDING);
        float y = m_gridStartY + row * (m_cardHeight + CARD_PADDING) - m_scrollOffset;

        // Skip cards outside visible area
        if (y + m_cardHeight < 0 || y > m_screenHeight) {
            continue;
        }

        RenderCard(renderer, m_cards[i], x, y, m_cardWidth, m_cardHeight);
    }
}

void CultureSelectionUI::RenderDetailView(Nova::Renderer& renderer) {
    if (m_selectedCard < 0 || m_detailViewProgress <= 0.01f) {
        return;
    }

    const auto& card = m_cards[m_selectedCard];
    float progress = m_animationsEnabled ? m_detailViewProgress : 1.0f;

    // Calculate detail panel size and position
    float panelWidth = m_screenWidth * 0.7f * progress;
    float panelHeight = m_screenHeight * 0.8f * progress;
    float panelX = (m_screenWidth - panelWidth) / 2.0f;
    float panelY = (m_screenHeight - panelHeight) / 2.0f;

    // Draw panel background
    // renderer.DrawRect(panelX, panelY, panelWidth, panelHeight, 0x1a1a2e, 0.95f);
    // renderer.DrawRectOutline(panelX, panelY, panelWidth, panelHeight, 0x4a4a5e, 2.0f);

    // Draw culture name
    // renderer.DrawText(card.name, panelX + panelWidth / 2, panelY + 30, ...);

    // Draw description
    // renderer.DrawTextWrapped(card.description, panelX + 20, panelY + 80, panelWidth - 40, ...);

    // Draw bonuses
    float bonusY = panelY + 200;
    // renderer.DrawText("Bonuses:", panelX + 20, bonusY, ...);
    RenderBonusList(renderer, card.bonusSummary, panelX + 30, bonusY + 25, true);

    // Draw penalties
    float penaltyY = bonusY + 30 + card.bonusSummary.size() * 22;
    if (!card.penaltySummary.empty()) {
        // renderer.DrawText("Penalties:", panelX + 20, penaltyY, ...);
        RenderBonusList(renderer, card.penaltySummary, panelX + 30, penaltyY + 25, false);
    }

    // Draw unique abilities
    float abilityY = panelY + panelHeight * 0.5f;
    // renderer.DrawText("Unique Abilities:", panelX + 20, abilityY, ...);
    for (size_t i = 0; i < card.uniqueAbilities.size(); ++i) {
        // renderer.DrawText(card.uniqueAbilities[i], panelX + 30, abilityY + 25 + i * 22, ...);
    }

    // Draw unique buildings
    if (!card.uniqueBuildings.empty()) {
        float buildingY = abilityY + 30 + card.uniqueAbilities.size() * 22 + 20;
        // renderer.DrawText("Unique Buildings:", panelX + 20, buildingY, ...);
        for (size_t i = 0; i < card.uniqueBuildings.size(); ++i) {
            // renderer.DrawText(card.uniqueBuildings[i], panelX + 30, buildingY + 25 + i * 22, ...);
        }
    }

    // Draw buttons
    float btnY = panelY + panelHeight - 60;
    float btnWidth = 120.0f;
    float btnHeight = 40.0f;

    // Back button
    float backBtnX = panelX + panelWidth / 2 - 140;
    // renderer.DrawRect(backBtnX, btnY, btnWidth, btnHeight, 0x4a4a5e);
    // renderer.DrawText("Back", backBtnX + btnWidth / 2, btnY + btnHeight / 2, ...);

    // Confirm button
    float confirmBtnX = panelX + panelWidth / 2 + 20;
    // renderer.DrawRect(confirmBtnX, btnY, btnWidth, btnHeight, 0x2e7d32);
    // renderer.DrawText("Select", confirmBtnX + btnWidth / 2, btnY + btnHeight / 2, ...);
}

void CultureSelectionUI::RenderConfirmDialog(Nova::Renderer& renderer) {
    if (m_confirmDialogProgress <= 0.01f) {
        return;
    }

    float progress = m_animationsEnabled ? m_confirmDialogProgress : 1.0f;

    // Dialog dimensions
    float dialogWidth = 400.0f * progress;
    float dialogHeight = 150.0f * progress;
    float dialogX = (m_screenWidth - dialogWidth) / 2.0f;
    float dialogY = (m_screenHeight - dialogHeight) / 2.0f;

    // Draw dialog background
    // renderer.DrawRect(dialogX, dialogY, dialogWidth, dialogHeight, 0x2a2a3e, 0.98f);
    // renderer.DrawRectOutline(dialogX, dialogY, dialogWidth, dialogHeight, 0x6a6a7e, 2.0f);

    // Draw confirmation text
    const auto& card = m_cards[m_selectedCard];
    std::string confirmText = "Choose " + card.name + " culture?";
    // renderer.DrawText(confirmText, dialogX + dialogWidth / 2, dialogY + 40, ...);
    // renderer.DrawText("This cannot be changed during the game.", dialogX + dialogWidth / 2, dialogY + 70, ...);

    // Draw buttons
    float btnY = dialogY + dialogHeight - 50;
    float btnWidth = 100.0f;
    float btnHeight = 35.0f;

    // Cancel button
    float cancelBtnX = dialogX + dialogWidth / 2 - 120;
    // renderer.DrawRect(cancelBtnX, btnY, btnWidth, btnHeight, 0x4a4a5e);
    // renderer.DrawText("Cancel", cancelBtnX + btnWidth / 2, btnY + btnHeight / 2, ...);

    // Confirm button
    float confirmBtnX = dialogX + dialogWidth / 2 + 20;
    // renderer.DrawRect(confirmBtnX, btnY, btnWidth, btnHeight, 0x2e7d32);
    // renderer.DrawText("Confirm", confirmBtnX + btnWidth / 2, btnY + btnHeight / 2, ...);
}

void CultureSelectionUI::RenderCard(Nova::Renderer& renderer, const CultureUICard& card,
                                     float x, float y, float width, float height) {
    // Calculate hover/select effects
    float scale = 1.0f + card.hoverAnimationProgress * 0.05f + card.selectAnimationProgress * 0.02f;
    float offsetX = width * (1.0f - scale) / 2.0f;
    float offsetY = height * (1.0f - scale) / 2.0f;

    float drawX = x - offsetX;
    float drawY = y - offsetY;
    float drawW = width * scale;
    float drawH = height * scale;

    // Choose colors based on state
    uint32_t bgColor = 0x1a1a2e;
    uint32_t borderColor = card.isSelected ? 0xffd700 : (card.isHovered ? 0x6a6a7e : 0x3a3a4e);
    float borderWidth = card.isSelected ? 3.0f : (card.isHovered ? 2.0f : 1.0f);

    // Draw card background
    // renderer.DrawRect(drawX, drawY, drawW, drawH, bgColor);
    // renderer.DrawRectOutline(drawX, drawY, drawW, drawH, borderColor, borderWidth);

    // Draw preview texture if available
    if (card.previewTexture) {
        float texSize = drawW * 0.6f;
        float texX = drawX + (drawW - texSize) / 2.0f;
        float texY = drawY + 20.0f;
        // renderer.DrawTexture(card.previewTexture, texX, texY, texSize, texSize);
    }

    // Draw culture name
    // renderer.DrawText(card.name, drawX + drawW / 2, drawY + drawH * 0.65f, ...);

    // Draw short description
    // renderer.DrawTextWrapped(card.shortDescription, drawX + 10, drawY + drawH * 0.75f, drawW - 20, ...);

    // Draw selection indicator
    if (card.isSelected) {
        // renderer.DrawTexture(checkmark, drawX + drawW - 30, drawY + 10, 24, 24);
    }
}

void CultureSelectionUI::RenderBonusList(Nova::Renderer& renderer, const std::vector<std::string>& items,
                                          float x, float y, bool positive) {
    uint32_t color = positive ? 0x4caf50 : 0xf44336; // Green for bonus, red for penalty

    for (size_t i = 0; i < items.size(); ++i) {
        float itemY = y + i * 22.0f;
        // renderer.DrawText(items[i], x, itemY, color, ...);
    }
}

void CultureSelectionUI::CalculateLayout() {
    // Calculate card dimensions based on screen size
    float availableWidth = m_screenWidth - CARD_PADDING * (CARDS_PER_ROW + 1);
    float availableHeight = m_screenHeight - 150.0f - CARD_PADDING * (NUM_ROWS + 1); // 150 for header

    m_cardWidth = availableWidth / CARDS_PER_ROW;
    m_cardHeight = m_cardWidth * CARD_ASPECT_RATIO;

    // Check if we need to constrain by height
    float totalCardHeight = m_cardHeight * NUM_ROWS + CARD_PADDING * (NUM_ROWS - 1);
    if (totalCardHeight > availableHeight) {
        m_cardHeight = (availableHeight - CARD_PADDING * (NUM_ROWS - 1)) / NUM_ROWS;
        m_cardWidth = m_cardHeight / CARD_ASPECT_RATIO;
    }

    // Calculate grid start position (centered)
    float totalWidth = m_cardWidth * CARDS_PER_ROW + CARD_PADDING * (CARDS_PER_ROW - 1);
    m_gridStartX = (m_screenWidth - totalWidth) / 2.0f;
    m_gridStartY = 120.0f; // Below header

    // Calculate scroll limits
    float totalGridHeight = m_cardHeight * NUM_ROWS + CARD_PADDING * (NUM_ROWS - 1);
    m_maxScroll = std::max(0.0f, totalGridHeight - (m_screenHeight - m_gridStartY - 50.0f));
}

int CultureSelectionUI::GetCardAtPosition(float x, float y) const {
    // Adjust for scroll
    y += m_scrollOffset;

    for (size_t i = 0; i < m_cards.size(); ++i) {
        int row = static_cast<int>(i) / CARDS_PER_ROW;
        int col = static_cast<int>(i) % CARDS_PER_ROW;

        float cardX = m_gridStartX + col * (m_cardWidth + CARD_PADDING);
        float cardY = m_gridStartY + row * (m_cardHeight + CARD_PADDING);

        if (x >= cardX && x <= cardX + m_cardWidth &&
            y >= cardY && y <= cardY + m_cardHeight) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

// =============================================================================
// PlayerCulture Implementation
// =============================================================================

PlayerCulture::PlayerCulture(CultureType type)
    : m_type(type)
    , m_isSet(true) {
}

const CultureData* PlayerCulture::GetData() const {
    if (!m_isSet) {
        return nullptr;
    }
    return CultureManager::Instance().GetCultureData(m_type);
}

void PlayerCulture::SetCulture(CultureType type) {
    m_type = type;
    m_isSet = true;
}

float PlayerCulture::ApplyBonus(float baseValue, const std::string& bonusType) const {
    if (!m_isSet) {
        return baseValue;
    }
    return CultureManager::Instance().ApplyBonus(m_type, baseValue, bonusType);
}

ResourceCost PlayerCulture::ApplyCostModifier(const ResourceCost& baseCost) const {
    if (!m_isSet) {
        return baseCost;
    }
    return CultureManager::Instance().ApplyCostModifier(m_type, baseCost);
}

bool PlayerCulture::CanBuild(BuildingType building) const {
    if (!m_isSet) {
        return true; // Allow all if no culture set
    }
    return CultureManager::Instance().CanBuild(m_type, building);
}

std::string PlayerCulture::GetBuildingTexture(BuildingType building) const {
    if (!m_isSet) {
        return "Vehement2/images/Bricks/BricksGrey.png";
    }
    return CultureManager::Instance().GetBuildingTexture(m_type, building);
}

std::string PlayerCulture::ToJson() const {
    nlohmann::json j;
    j["type"] = static_cast<int>(m_type);
    j["typeName"] = CultureTypeToString(m_type);
    j["isSet"] = m_isSet;
    return j.dump();
}

PlayerCulture PlayerCulture::FromJson(const std::string& json) {
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        PlayerCulture culture;

        if (j.contains("type") && j.contains("isSet")) {
            culture.m_type = static_cast<CultureType>(j["type"].get<int>());
            culture.m_isSet = j["isSet"].get<bool>();
        }

        return culture;
    } catch (const std::exception&) {
        return PlayerCulture();
    }
}

} // namespace RTS
} // namespace Vehement
