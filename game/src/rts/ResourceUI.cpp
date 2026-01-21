#include "ResourceUI.hpp"
#include "Gathering.hpp"
#include "Production.hpp"
#include "Trading.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// Utility Functions
// ============================================================================

std::string FormatResourceAmount(int amount, bool abbreviated) {
    if (!abbreviated || amount < 1000) {
        return std::to_string(amount);
    }

    std::stringstream ss;
    if (amount >= 1000000) {
        ss << std::fixed << std::setprecision(1) << (amount / 1000000.0f) << "M";
    } else if (amount >= 1000) {
        ss << std::fixed << std::setprecision(1) << (amount / 1000.0f) << "K";
    } else {
        ss << amount;
    }
    return ss.str();
}

std::string FormatResourceRate(float rate) {
    std::stringstream ss;
    if (rate >= 0) {
        ss << "+";
    }
    ss << std::fixed << std::setprecision(1) << rate << "/s";
    return ss.str();
}

std::string FormatTimeRemaining(float seconds) {
    if (seconds < 0 || seconds > 86400 * 365) {
        return "Stable";
    }

    std::stringstream ss;
    if (seconds < 60) {
        ss << static_cast<int>(seconds) << "s";
    } else if (seconds < 3600) {
        int minutes = static_cast<int>(seconds / 60);
        int secs = static_cast<int>(seconds) % 60;
        ss << minutes << "m " << secs << "s";
    } else {
        int hours = static_cast<int>(seconds / 3600);
        int minutes = (static_cast<int>(seconds) % 3600) / 60;
        ss << hours << "h " << minutes << "m";
    }
    return ss.str();
}

uint32_t GetRateColor(float rate) {
    if (rate > 0.01f) return 0x4CAF50FF;  // Green
    if (rate < -0.01f) return 0xF44336FF; // Red
    return 0xFFFFFFFF;                     // White (neutral)
}

// ============================================================================
// ResourceBar Implementation
// ============================================================================

ResourceBar::ResourceBar() {
    // Initialize default display configs
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        ResourceDisplayConfig config;
        config.type = static_cast<ResourceType>(i);
        config.visible = true;
        config.showRate = true;
        config.showCapacity = true;
        config.pulseWhenLow = true;
        config.lowThreshold = 0.2f;
        config.displayOrder = i;
        m_displayConfigs.push_back(config);
    }

    // Default visible resources (basic ones)
    m_visibleResources = {
        ResourceType::Food,
        ResourceType::Wood,
        ResourceType::Stone,
        ResourceType::Metal,
        ResourceType::Coins,
        ResourceType::Fuel,
        ResourceType::Medicine,
        ResourceType::Ammunition
    };
}

ResourceBar::~ResourceBar() = default;

void ResourceBar::Initialize(const glm::vec2& position, float width, float height) {
    m_position = position;
    m_width = width;
    m_height = height;
    m_initialized = true;
}

void ResourceBar::Update(float deltaTime) {
    if (!m_initialized) return;

    m_globalPulseTime += deltaTime * 3.0f; // Pulse frequency
    UpdatePulseEffects(deltaTime);
}

void ResourceBar::Render(Nova::Renderer& renderer) {
    if (!m_initialized || !m_resourceStock) return;

    // Calculate slot width
    int visibleCount = static_cast<int>(m_visibleResources.size());
    if (visibleCount == 0) return;

    float slotWidth = m_width / visibleCount;
    float padding = 5.0f;

    // Render background
    // renderer.DrawRect(m_position, {m_width, m_height}, 0x1A1A1AE0);

    // Render each resource slot
    for (int i = 0; i < visibleCount; ++i) {
        ResourceType type = m_visibleResources[i];
        glm::vec2 slotPos = m_position + glm::vec2(i * slotWidth + padding, padding);
        RenderResourceSlot(renderer, type, slotPos, slotWidth - padding * 2);
    }
}

void ResourceBar::SetDisplayConfig(ResourceType type, const ResourceDisplayConfig& config) {
    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(m_displayConfigs.size())) {
        m_displayConfigs[index] = config;
    }
}

const ResourceDisplayConfig& ResourceBar::GetDisplayConfig(ResourceType type) const {
    static ResourceDisplayConfig defaultConfig;
    int index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(m_displayConfigs.size())) {
        return m_displayConfigs[index];
    }
    return defaultConfig;
}

void ResourceBar::SetVisibleResources(const std::vector<ResourceType>& types) {
    m_visibleResources = types;
}

ResourceType ResourceBar::GetResourceAtPosition(const glm::vec2& mousePos) const {
    if (!IsMouseOver(mousePos)) return ResourceType::COUNT;

    int visibleCount = static_cast<int>(m_visibleResources.size());
    if (visibleCount == 0) return ResourceType::COUNT;

    float slotWidth = m_width / visibleCount;
    float localX = mousePos.x - m_position.x;

    int slotIndex = static_cast<int>(localX / slotWidth);
    if (slotIndex >= 0 && slotIndex < visibleCount) {
        return m_visibleResources[slotIndex];
    }

    return ResourceType::COUNT;
}

ResourceTooltip ResourceBar::GetTooltip(ResourceType type) const {
    ResourceTooltip tooltip;
    tooltip.type = type;
    tooltip.name = GetResourceName(type);

    if (m_resourceStock) {
        tooltip.amount = m_resourceStock->GetAmount(type);
        tooltip.capacity = m_resourceStock->GetCapacity(type);
        tooltip.incomeRate = m_resourceStock->GetNetRate(type);

        if (m_upkeepSystem) {
            tooltip.expenseRate = m_upkeepSystem->GetTotalConsumption(type);
        }

        tooltip.netRate = tooltip.incomeRate - tooltip.expenseRate;
        tooltip.isDepleting = tooltip.netRate < 0;

        if (tooltip.isDepleting && tooltip.netRate != 0) {
            tooltip.timeUntilChange = tooltip.amount / (-tooltip.netRate);
        } else if (!tooltip.isDepleting && tooltip.netRate > 0) {
            int space = tooltip.capacity - tooltip.amount;
            tooltip.timeUntilChange = space / tooltip.netRate;
        }
    }

    // Add breakdown of income sources
    if (m_gatheringSystem) {
        float gatherRate = m_gatheringSystem->GetCurrentGatherRate(type);
        if (gatherRate > 0.01f) {
            tooltip.incomeSources.emplace_back("Gathering", gatherRate);
        }
    }
    if (m_productionSystem) {
        // Production system provides income for processed resources
        tooltip.incomeSources.emplace_back("Production", tooltip.incomeRate);
    }

    // Add breakdown of expense sources
    if (m_upkeepSystem) {
        float upkeepCost = m_upkeepSystem->GetTotalConsumption(type);
        if (upkeepCost > 0.01f) {
            tooltip.expenseSources.emplace_back("Upkeep", upkeepCost);
        }
    }

    return tooltip;
}

bool ResourceBar::IsMouseOver(const glm::vec2& mousePos) const {
    return mousePos.x >= m_position.x && mousePos.x <= m_position.x + m_width &&
           mousePos.y >= m_position.y && mousePos.y <= m_position.y + m_height;
}

void ResourceBar::RenderResourceSlot(Nova::Renderer& renderer, ResourceType type,
                                     const glm::vec2& position, float width) {
    if (!m_resourceStock) return;

    const auto& config = GetDisplayConfig(type);
    if (!config.visible) return;

    int amount = m_resourceStock->GetAmount(type);
    int capacity = m_resourceStock->GetCapacity(type);
    float percentage = capacity > 0 ? static_cast<float>(amount) / capacity : 0.0f;

    float height = m_height - 10.0f;

    // Get resource color
    uint32_t color = GetResourceColor(type);

    // Apply pulse effect if low
    float alpha = 1.0f;
    if (config.pulseWhenLow && percentage < config.lowThreshold) {
        alpha = 0.5f + 0.5f * std::sin(m_globalPulseTime);
    }

    // Background
    // renderer.DrawRect(position, {width, height}, 0x333333E0);

    // Capacity bar
    if (config.showCapacity) {
        float barHeight = 4.0f;
        glm::vec2 barPos = position + glm::vec2(0, height - barHeight);
        // renderer.DrawRect(barPos, {width, barHeight}, 0x222222FF);
        // renderer.DrawRect(barPos, {width * percentage, barHeight}, color);
    }

    // Icon (placeholder - would use actual texture)
    float iconSize = std::min(height - 8.0f, 24.0f);
    glm::vec2 iconPos = position + glm::vec2(4.0f, (height - iconSize) / 2.0f);
    // renderer.DrawRect(iconPos, {iconSize, iconSize}, color);

    // Amount text
    std::string amountStr = FormatResourceAmount(amount);
    // renderer.DrawText(amountStr, position + glm::vec2(iconSize + 8.0f, 4.0f), 0xFFFFFFFF);

    // Rate text
    if (config.showRate) {
        float netRate = m_resourceStock->GetNetRate(type);
        if (m_upkeepSystem) {
            netRate -= m_upkeepSystem->GetTotalConsumption(type);
        }

        if (std::abs(netRate) > 0.01f) {
            std::string rateStr = FormatResourceRate(netRate);
            uint32_t rateColor = GetRateColor(netRate);
            // renderer.DrawText(rateStr, position + glm::vec2(iconSize + 8.0f, 18.0f), rateColor);
        }
    }
}

void ResourceBar::UpdatePulseEffects(float deltaTime) {
    // Update individual pulse timers if needed
}

// ============================================================================
// ResourceAlertManager Implementation
// ============================================================================

ResourceAlertManager::ResourceAlertManager() = default;

ResourceAlertManager::~ResourceAlertManager() = default;

void ResourceAlertManager::Initialize() {
    m_initialized = true;
}

void ResourceAlertManager::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update alert durations and fade out
    for (auto& alert : m_alerts) {
        alert.duration -= deltaTime;
        if (alert.duration < 1.0f) {
            alert.alpha = alert.duration;
        }
    }

    // Remove expired alerts
    m_alerts.erase(
        std::remove_if(m_alerts.begin(), m_alerts.end(),
            [](const ResourceAlert& a) { return a.duration <= 0; }),
        m_alerts.end()
    );
}

void ResourceAlertManager::Render(Nova::Renderer& renderer) {
    if (!m_initialized) return;

    float yOffset = 0.0f;
    float alertHeight = 30.0f;
    float alertSpacing = 5.0f;

    for (const auto& alert : m_alerts) {
        glm::vec2 pos = m_alertPosition + glm::vec2(0, yOffset);

        // Background color based on severity
        uint32_t bgColor;
        switch (alert.severity) {
            case 2: bgColor = 0xF44336E0; break; // Critical - red
            case 1: bgColor = 0xFF9800E0; break; // Warning - orange
            default: bgColor = 0x2196F3E0; break; // Info - blue
        }

        // Apply alpha
        bgColor = (bgColor & 0xFFFFFF00) | static_cast<uint8_t>(alert.alpha * (bgColor & 0xFF));

        // Draw alert background
        // renderer.DrawRect(pos, {300.0f, alertHeight}, bgColor);

        // Draw icon and text
        // renderer.DrawText(alert.message, pos + glm::vec2(10.0f, 8.0f), 0xFFFFFFFF);

        yOffset += alertHeight + alertSpacing;
    }
}

void ResourceAlertManager::ShowInfo(const std::string& message, ResourceType type, float duration) {
    ResourceAlert alert;
    alert.message = message;
    alert.resourceType = type;
    alert.severity = 0;
    alert.duration = duration;
    alert.alpha = 1.0f;
    AddAlert(alert);
}

void ResourceAlertManager::ShowWarning(const std::string& message, ResourceType type, float duration) {
    ResourceAlert alert;
    alert.message = message;
    alert.resourceType = type;
    alert.severity = 1;
    alert.duration = duration;
    alert.alpha = 1.0f;
    AddAlert(alert);
}

void ResourceAlertManager::ShowCritical(const std::string& message, ResourceType type, float duration) {
    ResourceAlert alert;
    alert.message = message;
    alert.resourceType = type;
    alert.severity = 2;
    alert.duration = duration;
    alert.alpha = 1.0f;
    AddAlert(alert);
}

void ResourceAlertManager::ShowLocalized(const std::string& message, const glm::vec2& worldPos,
                                         ResourceType type, int severity, float duration) {
    ResourceAlert alert;
    alert.message = message;
    alert.resourceType = type;
    alert.severity = severity;
    alert.duration = duration;
    alert.alpha = 1.0f;
    alert.position = worldPos;
    alert.isLocalized = true;
    AddAlert(alert);
}

void ResourceAlertManager::ClearAll() {
    m_alerts.clear();
}

void ResourceAlertManager::ClearForResource(ResourceType type) {
    m_alerts.erase(
        std::remove_if(m_alerts.begin(), m_alerts.end(),
            [type](const ResourceAlert& a) { return a.resourceType == type; }),
        m_alerts.end()
    );
}

void ResourceAlertManager::BindUpkeepSystem(UpkeepSystem* upkeep) {
    m_upkeepSystem = upkeep;
    if (upkeep) {
        upkeep->SetOnWarning([this](const UpkeepWarning& warning) {
            OnUpkeepWarning(warning);
        });
    }
}

void ResourceAlertManager::BindResourceStock(ResourceStock* stock) {
    m_resourceStock = stock;
    if (stock) {
        stock->SetOnLowResource([this](ResourceType type, int amount, int threshold) {
            OnLowResource(type, amount, threshold);
        });
    }
}

void ResourceAlertManager::AddAlert(const ResourceAlert& alert) {
    // Remove oldest if at max
    while (static_cast<int>(m_alerts.size()) >= m_maxAlerts) {
        m_alerts.erase(m_alerts.begin());
    }
    m_alerts.push_back(alert);

    // Play sound if enabled
    if (m_soundEnabled) {
        // Play appropriate sound based on severity
        const char* soundPath = nullptr;
        switch (alert.severity) {
            case 0: // Info
                soundPath = "audio/ui/notification_info.wav";
                break;
            case 1: // Warning
                soundPath = "audio/ui/notification_warning.wav";
                break;
            case 2: // Critical
                soundPath = "audio/ui/notification_critical.wav";
                break;
            default:
                soundPath = "audio/ui/notification_info.wav";
                break;
        }
        // Sound would be played through audio system:
        // AudioManager::Instance().PlaySound(soundPath);
        (void)soundPath; // Suppress unused variable warning until audio system integration
    }
}

void ResourceAlertManager::OnUpkeepWarning(const UpkeepWarning& warning) {
    switch (warning.status) {
        case UpkeepStatus::Critical:
            ShowCritical(warning.message, warning.resourceType);
            break;
        case UpkeepStatus::Low:
            ShowWarning(warning.message, warning.resourceType);
            break;
        case UpkeepStatus::Depleted:
            ShowCritical(warning.message, warning.resourceType, 10.0f);
            break;
        default:
            break;
    }
}

void ResourceAlertManager::OnLowResource(ResourceType type, int amount, int threshold) {
    std::stringstream ss;
    ss << GetResourceName(type) << " low: " << amount << "/" << threshold;
    ShowWarning(ss.str(), type);
}

// ============================================================================
// StorageCapacityWidget Implementation
// ============================================================================

StorageCapacityWidget::StorageCapacityWidget() = default;

StorageCapacityWidget::~StorageCapacityWidget() = default;

void StorageCapacityWidget::Initialize(const glm::vec2& position, float width, float height) {
    m_position = position;
    m_width = width;
    m_height = height;
}

void StorageCapacityWidget::Update(float deltaTime) {
    // No animation needed currently
}

void StorageCapacityWidget::Render(Nova::Renderer& renderer) {
    if (!m_visible || !m_resourceStock) return;

    // Draw background
    // renderer.DrawRect(m_position, {m_width, m_height}, 0x1A1A1AE0);

    // Draw title
    // renderer.DrawText("Storage", m_position + glm::vec2(10.0f, 5.0f), 0xFFFFFFFF);

    float yOffset = 30.0f;
    float barHeight = 16.0f;
    float barSpacing = 4.0f;

    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        if (type == ResourceType::Coins) continue; // Skip coins (unlimited)

        int amount = m_resourceStock->GetAmount(type);
        int capacity = m_resourceStock->GetCapacity(type);
        float percentage = m_resourceStock->GetFillPercentage(type);

        glm::vec2 barPos = m_position + glm::vec2(10.0f, yOffset);

        // Resource name
        // renderer.DrawText(GetResourceName(type), barPos, 0xFFFFFFFF);

        // Capacity bar
        glm::vec2 barBgPos = barPos + glm::vec2(80.0f, 0);
        float barWidth = m_width - 100.0f;
        // renderer.DrawRect(barBgPos, {barWidth, barHeight}, 0x333333FF);

        uint32_t fillColor = GetResourceColor(type);
        if (percentage > 0.9f) fillColor = 0xFFC107FF; // Yellow when almost full
        // renderer.DrawRect(barBgPos, {barWidth * percentage, barHeight}, fillColor);

        // Amount text
        std::string amountStr = std::to_string(amount) + "/" + std::to_string(capacity);
        // renderer.DrawText(amountStr, barBgPos + glm::vec2(5.0f, 2.0f), 0xFFFFFFFF);

        yOffset += barHeight + barSpacing;
    }
}

// ============================================================================
// IncomeExpenseSummary Implementation
// ============================================================================

IncomeExpenseSummary::IncomeExpenseSummary() = default;

IncomeExpenseSummary::~IncomeExpenseSummary() = default;

void IncomeExpenseSummary::Initialize(const glm::vec2& position, float width, float height) {
    m_position = position;
    m_width = width;
    m_height = height;
}

void IncomeExpenseSummary::Update(float deltaTime) {
    // Calculations done on-demand in render
}

void IncomeExpenseSummary::Render(Nova::Renderer& renderer) {
    if (!m_visible) return;

    // Draw background
    // renderer.DrawRect(m_position, {m_width, m_height}, 0x1A1A1AE0);

    // Draw title
    // renderer.DrawText("Economy", m_position + glm::vec2(10.0f, 5.0f), 0xFFFFFFFF);

    float yOffset = 30.0f;
    float lineHeight = 20.0f;

    // For each resource type
    std::vector<ResourceType> types = {
        ResourceType::Food, ResourceType::Wood, ResourceType::Stone,
        ResourceType::Metal, ResourceType::Fuel
    };

    for (auto type : types) {
        float income = 0.0f;
        float expense = 0.0f;

        // Get income from production/gathering
        if (m_resourceStock) {
            income = m_resourceStock->GetNetRate(type);
        }

        // Get expense from upkeep
        if (m_upkeepSystem) {
            expense = m_upkeepSystem->GetTotalConsumption(type);
        }

        float netRate = income - expense;

        glm::vec2 linePos = m_position + glm::vec2(10.0f, yOffset);

        // Resource name
        // renderer.DrawText(GetResourceName(type), linePos, GetResourceColor(type));

        // Net rate
        std::string rateStr = FormatResourceRate(netRate);
        uint32_t rateColor = GetRateColor(netRate);
        // renderer.DrawText(rateStr, linePos + glm::vec2(100.0f, 0), rateColor);

        yOffset += lineHeight;
    }
}

// ============================================================================
// ResourceUIManager Implementation
// ============================================================================

ResourceUIManager::ResourceUIManager() = default;

ResourceUIManager::~ResourceUIManager() {
    Shutdown();
}

void ResourceUIManager::Initialize(float screenWidth, float screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Initialize resource bar at top of screen
    float barWidth = screenWidth * 0.8f;
    float barHeight = 50.0f;
    glm::vec2 barPos((screenWidth - barWidth) / 2.0f, 10.0f);
    m_resourceBar.Initialize(barPos, barWidth, barHeight);

    // Initialize alert manager
    m_alertManager.Initialize();
    m_alertManager.SetAlertPosition(glm::vec2(10.0f, screenHeight - 200.0f));

    // Initialize storage widget (hidden by default)
    m_storageWidget.Initialize(glm::vec2(screenWidth - 220.0f, 70.0f), 200.0f, 200.0f);
    m_storageWidget.SetVisible(false);

    // Initialize income summary (hidden by default)
    m_incomeSummary.Initialize(glm::vec2(10.0f, 70.0f), 250.0f, 200.0f);
    m_incomeSummary.SetVisible(false);

    m_initialized = true;
}

void ResourceUIManager::Shutdown() {
    m_initialized = false;
}

void ResourceUIManager::Update(float deltaTime) {
    if (!m_initialized || !m_visible) return;

    m_resourceBar.Update(deltaTime);
    m_alertManager.Update(deltaTime);
    m_storageWidget.Update(deltaTime);
    m_incomeSummary.Update(deltaTime);
}

void ResourceUIManager::Render(Nova::Renderer& renderer) {
    if (!m_initialized || !m_visible) return;

    m_resourceBar.Render(renderer);
    m_alertManager.Render(renderer);

    if (m_detailedView) {
        m_storageWidget.Render(renderer);
        m_incomeSummary.Render(renderer);
    }
}

void ResourceUIManager::HandleMouseInput(const glm::vec2& mousePos, bool clicked) {
    if (!m_initialized || !m_visible) return;

    // Check if hovering over resource bar
    ResourceType hovered = m_resourceBar.GetResourceAtPosition(mousePos);
    if (hovered != ResourceType::COUNT && clicked) {
        // Could show tooltip or detailed view
    }
}

void ResourceUIManager::BindSystems(
    ResourceStock* stock,
    GatheringSystem* gathering,
    ProductionSystem* production,
    TradingSystem* trading,
    UpkeepSystem* upkeep
) {
    m_resourceBar.BindResourceStock(stock);
    m_resourceBar.BindUpkeepSystem(upkeep);
    m_resourceBar.BindGatheringSystem(gathering);
    m_resourceBar.BindProductionSystem(production);

    m_alertManager.BindUpkeepSystem(upkeep);
    m_alertManager.BindResourceStock(stock);

    m_storageWidget.BindResourceStock(stock);

    m_incomeSummary.BindResourceStock(stock);
    m_incomeSummary.BindGatheringSystem(gathering);
    m_incomeSummary.BindProductionSystem(production);
    m_incomeSummary.BindUpkeepSystem(upkeep);
}

void ResourceUIManager::ToggleDetailedView() {
    m_detailedView = !m_detailedView;
    m_storageWidget.SetVisible(m_detailedView);
    m_incomeSummary.SetVisible(m_detailedView);
}

} // namespace RTS
} // namespace Vehement
