#include "EventUI.hpp"
#include "EventScheduler.hpp"
#include "EventEffects.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

// Logging macros
#if __has_include("../../../engine/core/Logger.hpp")
#include "../../../engine/core/Logger.hpp"
#define UI_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define UI_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define UI_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define UI_LOG_INFO(...) std::cout << "[EventUI] " << __VA_ARGS__ << std::endl
#define UI_LOG_WARN(...) std::cerr << "[EventUI WARN] " << __VA_ARGS__ << std::endl
#define UI_LOG_ERROR(...) std::cerr << "[EventUI ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

EventUI::EventUI() {
}

EventUI::~EventUI() {
    if (m_initialized) {
        Shutdown();
    }
}

bool EventUI::Initialize() {
    if (m_initialized) {
        UI_LOG_WARN("EventUI already initialized");
        return true;
    }

    m_initialized = true;
    UI_LOG_INFO("EventUI initialized");
    return true;
}

void EventUI::Shutdown() {
    if (!m_initialized) return;

    UI_LOG_INFO("Shutting down EventUI");

    m_activeToasts.clear();
    m_visiblePanels.clear();
    m_selectedEventId.clear();

    m_initialized = false;
}

void EventUI::SetEventSystems(EventScheduler* scheduler,
                               EventNotificationManager* notifications,
                               EventParticipationManager* participation,
                               EventEffects* effects) {
    m_scheduler = scheduler;
    m_notifications = notifications;
    m_participation = participation;
    m_effects = effects;

    // Register for notification callbacks
    if (notifications) {
        notifications->OnNotificationDisplay([this](const EventNotification& notif) {
            ShowToast(notif);
        });
    }

    // Register for event callbacks
    if (scheduler) {
        scheduler->OnEventStarted([this](const WorldEvent& event) {
            ShowEventBanner(event);
            m_eventListDirty = true;
        });

        scheduler->OnEventEnded([this](const WorldEvent& event) {
            if (m_bannerData.eventId == event.id) {
                HideBanner();
            }
            m_eventListDirty = true;
        });
    }
}

void EventUI::SetCallbacks(const EventUICallbacks& callbacks) {
    m_callbacks = callbacks;
}

void EventUI::SetTheme(const EventUITheme& theme) {
    m_theme = theme;
}

void EventUI::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateAnimations(deltaTime);

    // Update banner timer
    if (m_showBanner && m_bannerDuration > 0.0f) {
        m_bannerTimer += deltaTime;
        if (m_bannerTimer >= m_bannerDuration) {
            HideBanner();
        }
    }

    // Update toast timers
    for (auto it = m_activeToasts.begin(); it != m_activeToasts.end();) {
        it->displayTime += deltaTime;

        if (it->isDismissing) {
            it->alpha -= deltaTime * m_theme.fadeSpeed;
            if (it->alpha <= 0.0f) {
                it = m_activeToasts.erase(it);
                continue;
            }
        } else if (it->notification.displayDuration > 0.0f &&
                   it->displayTime >= it->notification.displayDuration) {
            it->isDismissing = true;
        }

        ++it;
    }
}

void EventUI::Render() {
    if (!m_initialized) return;

    // Render banner if visible
    if (m_showBanner) {
        RenderBanner();
    }

    // Render active panel
    switch (m_activePanel) {
        case EventPanelType::EventList:
            RenderEventListPanel();
            break;
        case EventPanelType::EventDetails:
            RenderEventDetailsPanel();
            break;
        case EventPanelType::EventLog:
            RenderEventLogPanel();
            break;
        case EventPanelType::Leaderboard:
            RenderLeaderboardPanel();
            break;
        case EventPanelType::Rewards:
            RenderRewardsPanel();
            break;
        case EventPanelType::Notifications:
            RenderNotificationToasts();
            break;
        default:
            break;
    }

    // Always render notification toasts
    RenderNotificationToasts();
}

void EventUI::RenderMinimapMarkers() {
    if (!m_notifications || !m_callbacks.drawMinimapMarker) return;

    auto markers = m_notifications->GetMinimapMarkers();
    for (const auto& marker : markers) {
        float pulseScale = 1.0f;
        if (marker.pulseSpeed > 0.0f) {
            pulseScale = 1.0f + 0.2f * std::sin(m_animationTimer * marker.pulseSpeed);
        }

        uint32_t color = marker.color;
        if (marker.isBlinking) {
            float blink = GetBlinkValue();
            // Modulate alpha
            uint32_t alpha = static_cast<uint32_t>((color >> 24) * blink);
            color = (alpha << 24) | (color & 0x00FFFFFF);
        }

        m_callbacks.drawMinimapMarker(marker.position, marker.radius * pulseScale, color);
    }
}

bool EventUI::HandleInput(float mouseX, float mouseY, bool clicked) {
    if (!m_initialized) return false;

    // Handle panel-specific input
    // This would check button clicks, scrolling, etc.

    return false;
}

void EventUI::ShowPanel(EventPanelType panel) {
    m_activePanel = panel;
    m_visiblePanels.insert(panel);
}

void EventUI::HidePanel(EventPanelType panel) {
    m_visiblePanels.erase(panel);
    if (m_activePanel == panel) {
        m_activePanel = EventPanelType::None;
    }
}

void EventUI::TogglePanel(EventPanelType panel) {
    if (IsPanelVisible(panel)) {
        HidePanel(panel);
    } else {
        ShowPanel(panel);
    }
}

bool EventUI::IsPanelVisible(EventPanelType panel) const {
    return m_visiblePanels.count(panel) > 0;
}

void EventUI::CloseAllPanels() {
    m_visiblePanels.clear();
    m_activePanel = EventPanelType::None;
}

void EventUI::ShowEventBanner(const WorldEvent& event) {
    m_bannerData = PrepareBannerData(event);
    m_showBanner = true;
    m_bannerTimer = 0.0f;

    UI_LOG_INFO("Showing banner for event: " + event.name);
}

void EventUI::HideBanner() {
    m_showBanner = false;
    m_bannerData = {};
}

void EventUI::ShowEventDetails(const std::string& eventId) {
    m_selectedEventId = eventId;
    ShowPanel(EventPanelType::EventDetails);
}

void EventUI::JoinSelectedEvent() {
    if (m_selectedEventId.empty() || !m_participation) return;

    m_participation->JoinEvent(m_selectedEventId, m_localPlayerId);
}

void EventUI::LeaveSelectedEvent() {
    if (m_selectedEventId.empty() || !m_participation) return;

    m_participation->LeaveEvent(m_selectedEventId, m_localPlayerId, false);
}

void EventUI::ShowToast(const EventNotification& notification) {
    // Limit visible toasts
    if (static_cast<int32_t>(m_activeToasts.size()) >= MaxVisibleToasts) {
        // Dismiss oldest non-critical toast
        for (auto it = m_activeToasts.begin(); it != m_activeToasts.end(); ++it) {
            if (it->notification.canDismiss) {
                it->isDismissing = true;
                break;
            }
        }
    }

    ToastState toast;
    toast.notification = notification;
    toast.displayTime = 0.0f;
    toast.alpha = 1.0f;
    toast.isDismissing = false;

    m_activeToasts.push_back(toast);
}

void EventUI::DismissAllToasts() {
    for (auto& toast : m_activeToasts) {
        if (toast.notification.canDismiss) {
            toast.isDismissing = true;
        }
    }
}

int32_t EventUI::GetVisibleToastCount() const {
    return static_cast<int32_t>(m_activeToasts.size());
}

void EventUI::ShowRewardsPanel() {
    ShowPanel(EventPanelType::Rewards);
}

void EventUI::ClaimReward(const std::string& eventId) {
    if (!m_participation) return;

    auto reward = m_participation->ClaimReward(eventId, m_localPlayerId);
    if (reward) {
        UI_LOG_INFO("Claimed reward for event: " + eventId);
    }
}

void EventUI::ClaimAllRewards() {
    if (!m_participation) return;

    auto unclaimed = m_participation->GetUnclaimedRewards(m_localPlayerId);
    for (const auto& [eventId, reward] : unclaimed) {
        ClaimReward(eventId);
    }
}

bool EventUI::HasUnclaimedRewards() const {
    if (!m_participation) return false;
    return m_participation->HasUnclaimedRewards(m_localPlayerId);
}

void EventUI::ShowLeaderboard(const std::string& eventId) {
    m_leaderboardEventId = eventId;
    m_showGlobalLeaderboard = false;
    ShowPanel(EventPanelType::Leaderboard);
}

void EventUI::ShowGlobalLeaderboard(EventType type) {
    m_leaderboardGlobalType = type;
    m_showGlobalLeaderboard = true;
    ShowPanel(EventPanelType::Leaderboard);
}

std::vector<EventListItem> EventUI::GetEventList() const {
    if (!m_scheduler) return {};

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    if (!m_eventListDirty) {
        return m_cachedEventList;
    }

    m_cachedEventList.clear();

    // Get active events
    auto activeEvents = m_scheduler->GetActiveEvents();
    for (const auto& event : activeEvents) {
        auto item = PrepareListItem(event);
        item.isActive = true;

        // Apply filters
        if (!m_showActiveOnly || item.isActive) {
            EventCategory cat = GetEventCategory(event.type);
            bool passFilter = true;
            if (cat == EventCategory::Threat && !m_showThreats) passFilter = false;
            if (cat == EventCategory::Opportunity && !m_showOpportunities) passFilter = false;
            if (cat == EventCategory::Environmental && !m_showEnvironmental) passFilter = false;
            if (cat == EventCategory::Social && !m_showSocial) passFilter = false;
            if (cat == EventCategory::Global && !m_showGlobal) passFilter = false;

            if (m_showParticipatingOnly && !item.isParticipating) passFilter = false;

            if (passFilter) {
                m_cachedEventList.push_back(item);
            }
        }
    }

    // Get scheduled events
    auto scheduledEvents = m_scheduler->GetScheduledEvents();
    for (const auto& event : scheduledEvents) {
        auto item = PrepareListItem(event);
        item.isUpcoming = true;

        EventCategory cat = GetEventCategory(event.type);
        bool passFilter = true;
        if (cat == EventCategory::Threat && !m_showThreats) passFilter = false;
        if (cat == EventCategory::Opportunity && !m_showOpportunities) passFilter = false;
        if (cat == EventCategory::Environmental && !m_showEnvironmental) passFilter = false;
        if (cat == EventCategory::Social && !m_showSocial) passFilter = false;
        if (cat == EventCategory::Global && !m_showGlobal) passFilter = false;

        if (!m_showActiveOnly && passFilter) {
            m_cachedEventList.push_back(item);
        }
    }

    // Sort by distance, then by start time
    std::sort(m_cachedEventList.begin(), m_cachedEventList.end(),
              [](const EventListItem& a, const EventListItem& b) {
                  if (a.isActive != b.isActive) return a.isActive;
                  return a.distance < b.distance;
              });

    m_eventListDirty = false;
    return m_cachedEventList;
}

EventDetailData EventUI::GetEventDetails(const std::string& eventId) const {
    EventDetailData data;

    if (!m_scheduler) return data;

    auto eventOpt = m_scheduler->GetEvent(eventId);
    if (!eventOpt) return data;

    data = PrepareDetailData(*eventOpt);
    return data;
}

std::vector<EventLogEntry> EventUI::GetEventLog() const {
    std::vector<EventLogEntry> log;

    if (!m_scheduler) return log;

    auto completedEvents = m_scheduler->GetCompletedEvents();
    for (const auto& event : completedEvents) {
        EventLogEntry entry;
        entry.eventId = event.id;
        entry.type = event.type;
        entry.name = event.name;
        entry.startTime = event.startTime;
        entry.endTime = event.endTime;
        entry.wasSuccessful = event.isCompleted && !event.wasCancelled;

        if (m_participation) {
            entry.playerRank = m_participation->GetPlayerRank(event.id, m_localPlayerId);
            auto contributions = m_participation->GetEventContributions(event.id);
            entry.totalParticipants = static_cast<int32_t>(contributions.size());
        }

        log.push_back(entry);
    }

    // Sort by end time (most recent first)
    std::sort(log.begin(), log.end(),
              [](const EventLogEntry& a, const EventLogEntry& b) {
                  return a.endTime > b.endTime;
              });

    return log;
}

std::vector<RewardDisplayItem> EventUI::GetUnclaimedRewards() const {
    std::vector<RewardDisplayItem> rewards;

    if (!m_participation) return rewards;

    auto unclaimed = m_participation->GetUnclaimedRewards(m_localPlayerId);
    for (const auto& [eventId, reward] : unclaimed) {
        rewards.push_back(PrepareRewardItem(eventId, reward));
    }

    return rewards;
}

std::string EventUI::FormatTimeRemaining(int64_t milliseconds) {
    if (milliseconds <= 0) return "0:00";

    int64_t seconds = milliseconds / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    std::stringstream ss;
    if (hours > 0) {
        ss << hours << ":" << std::setfill('0') << std::setw(2) << minutes
           << ":" << std::setfill('0') << std::setw(2) << seconds;
    } else {
        ss << minutes << ":" << std::setfill('0') << std::setw(2) << seconds;
    }

    return ss.str();
}

std::string EventUI::FormatDistance(float units) {
    if (units < 1000.0f) {
        return std::to_string(static_cast<int>(units)) + "m";
    } else {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (units / 1000.0f) << "km";
        return ss.str();
    }
}

uint32_t EventUI::GetCategoryColor(EventCategory category) const {
    switch (category) {
        case EventCategory::Threat: return m_theme.threatColor;
        case EventCategory::Opportunity: return m_theme.opportunityColor;
        case EventCategory::Environmental: return m_theme.environmentalColor;
        case EventCategory::Social: return m_theme.socialColor;
        case EventCategory::Global: return m_theme.globalColor;
        default: return m_theme.textPrimary;
    }
}

std::string EventUI::GetEventIcon(EventType type) const {
    // Return icon path based on event type
    switch (type) {
        case EventType::ZombieHorde:
        case EventType::BossZombie:
            return "icons/zombie.png";
        case EventType::SupplyDrop:
            return "icons/supply.png";
        case EventType::Storm:
        case EventType::Fog:
            return "icons/weather.png";
        case EventType::TradeCaravan:
        case EventType::Merchant:
            return "icons/trade.png";
        case EventType::BloodMoon:
            return "icons/blood_moon.png";
        default:
            return "icons/event_default.png";
    }
}

std::string EventUI::GetSeverityText(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::Minor: return "Minor";
        case EventSeverity::Moderate: return "Moderate";
        case EventSeverity::Major: return "Major";
        case EventSeverity::Critical: return "Critical";
        case EventSeverity::Catastrophic: return "CATASTROPHIC";
        default: return "Unknown";
    }
}

// ===========================================================================
// Private Rendering
// ===========================================================================

void EventUI::RenderBanner() {
    if (!m_callbacks.renderBanner) return;

    // Update banner data with current time info
    int64_t currentTime = GetCurrentTimeMs();
    m_bannerData.remainingTimeMs = m_bannerData.remainingTimeMs > 0 ?
        std::max(int64_t(0), m_bannerData.remainingTimeMs - (currentTime - m_bannerData.remainingTimeMs)) : 0;

    // Calculate blink if urgent
    if (m_bannerData.isUrgent) {
        m_bannerData.isBlinking = GetBlinkValue() > 0.5f;
    }

    m_bannerData.animationTimer = m_animationTimer;

    m_callbacks.renderBanner(m_bannerData);
}

void EventUI::RenderEventListPanel() {
    if (!m_callbacks.renderEventList) return;

    auto events = GetEventList();
    m_callbacks.renderEventList(events);
}

void EventUI::RenderEventDetailsPanel() {
    if (!m_callbacks.renderEventDetails) return;

    if (m_selectedEventId.empty()) return;

    auto details = GetEventDetails(m_selectedEventId);
    m_callbacks.renderEventDetails(details);
}

void EventUI::RenderEventLogPanel() {
    if (!m_callbacks.renderEventLog) return;

    auto log = GetEventLog();
    m_callbacks.renderEventLog(log);
}

void EventUI::RenderLeaderboardPanel() {
    if (!m_callbacks.renderLeaderboard) return;

    std::vector<LeaderboardEntry> leaderboard;

    if (m_showGlobalLeaderboard) {
        if (m_participation) {
            leaderboard = m_participation->GetGlobalLeaderboard(m_leaderboardGlobalType);
        }
    } else if (!m_leaderboardEventId.empty()) {
        if (m_participation) {
            leaderboard = m_participation->GetEventLeaderboard(m_leaderboardEventId);
        }
    }

    m_callbacks.renderLeaderboard(leaderboard);
}

void EventUI::RenderRewardsPanel() {
    if (!m_callbacks.renderRewards) return;

    auto rewards = GetUnclaimedRewards();
    m_callbacks.renderRewards(rewards);
}

void EventUI::RenderNotificationToasts() {
    if (!m_callbacks.renderNotifications) return;

    std::vector<EventNotification> notifications;
    for (const auto& toast : m_activeToasts) {
        EventNotification notif = toast.notification;
        // Could modify for alpha here
        notifications.push_back(notif);
    }

    m_callbacks.renderNotifications(notifications);
}

// ===========================================================================
// Private Data Preparation
// ===========================================================================

EventBannerData EventUI::PrepareBannerData(const WorldEvent& event) const {
    EventBannerData data;

    data.eventId = event.id;
    data.title = event.name;

    EventCategory category = GetEventCategory(event.type);
    switch (category) {
        case EventCategory::Threat:
            data.subtitle = "DANGER - " + event.description;
            break;
        case EventCategory::Opportunity:
            data.subtitle = "OPPORTUNITY - " + event.description;
            break;
        default:
            data.subtitle = event.description;
            break;
    }

    data.iconPath = GetEventIcon(event.type);
    data.backgroundColor = GetCategoryColor(category);
    data.textColor = m_theme.bannerTextColor;

    int64_t currentTime = GetCurrentTimeMs();
    data.progress = event.GetProgress(currentTime);
    data.remainingTimeMs = event.GetRemainingDuration(currentTime);

    EventSeverity severity = GetDefaultSeverity(event.type);
    data.isUrgent = severity >= EventSeverity::Major;
    data.isBlinking = severity >= EventSeverity::Critical;

    return data;
}

EventListItem EventUI::PrepareListItem(const WorldEvent& event) const {
    EventListItem item;

    item.eventId = event.id;
    item.type = event.type;
    item.name = event.name;
    item.description = event.description;
    item.iconPath = GetEventIcon(event.type);
    item.category = GetEventCategory(event.type);
    item.severity = GetDefaultSeverity(event.type);
    item.iconColor = GetCategoryColor(item.category);

    int64_t currentTime = GetCurrentTimeMs();

    // Status text
    if (event.IsCurrentlyActive(currentTime)) {
        item.statusText = "Active";
        item.isActive = true;
        item.progress = event.GetProgress(currentTime);
        item.timeText = FormatTimeRemaining(event.GetRemainingDuration(currentTime)) + " remaining";
    } else if (currentTime < event.startTime) {
        item.statusText = "Upcoming";
        item.isUpcoming = true;
        item.timeText = "Starts in " + FormatTimeRemaining(event.GetTimeUntilStart(currentTime));
    } else {
        item.statusText = "Ended";
    }

    // Location
    if (event.isGlobal) {
        item.locationText = "Global Event";
        item.distance = 0.0f;
    } else {
        item.location = event.location;
        item.distance = glm::length(event.location - m_playerPosition);
        item.locationText = FormatDistance(item.distance) + " away";
    }

    // Participation
    if (m_participation) {
        item.isParticipating = m_participation->IsParticipating(event.id, m_localPlayerId);
        auto contributions = m_participation->GetEventContributions(event.id);
        item.participantCount = static_cast<int32_t>(contributions.size());
    }

    item.startTime = event.startTime;
    item.endTime = event.endTime;

    return item;
}

EventDetailData EventUI::PrepareDetailData(const WorldEvent& event) const {
    EventDetailData data;

    data.event = event;
    data.isParticipating = false;
    data.canJoin = true;

    if (m_participation) {
        data.isParticipating = m_participation->IsParticipating(event.id, m_localPlayerId);
        data.playerContribution = m_participation->GetContribution(event.id, m_localPlayerId);
        data.leaderboard = m_participation->GetEventLeaderboard(event.id);
        data.potentialReward = m_participation->CalculateReward(event.id, m_localPlayerId);
    }

    // Add tips based on event type
    EventCategory category = GetEventCategory(event.type);
    switch (category) {
        case EventCategory::Threat:
            data.tips.push_back("Defend your base and help others!");
            data.tips.push_back("Higher damage = better rewards");
            break;
        case EventCategory::Opportunity:
            data.tips.push_back("Be quick - others may claim the rewards!");
            data.tips.push_back("Bring transport for resources");
            break;
        case EventCategory::Environmental:
            data.tips.push_back("Prepare for reduced visibility or movement");
            data.tips.push_back("Secure your buildings before the event");
            break;
        default:
            data.tips.push_back("Participate to earn rewards!");
            break;
    }

    return data;
}

EventLogEntry EventUI::PrepareLogEntry(const EventParticipationRecord& record) const {
    EventLogEntry entry;

    entry.eventId = record.eventId;
    entry.type = record.eventType;
    entry.name = record.eventName;
    entry.wasSuccessful = record.wasSuccessful;
    entry.totalParticipants = record.totalParticipants;

    auto it = record.participants.find(m_localPlayerId);
    if (it != record.participants.end()) {
        entry.playerRank = it->second.rank;
        entry.rewardClaimed = it->second.rewardClaimed;
    }

    return entry;
}

RewardDisplayItem EventUI::PrepareRewardItem(const std::string& eventId,
                                              const EventReward& reward) const {
    RewardDisplayItem item;

    item.eventId = eventId;
    item.reward = reward;
    item.earnedAt = GetCurrentTimeMs(); // Would be stored timestamp
    item.isNew = true;

    // Get event name if available
    if (m_scheduler) {
        auto eventOpt = m_scheduler->GetEvent(eventId);
        if (eventOpt) {
            item.eventName = eventOpt->name;
        }
    }

    if (item.eventName.empty()) {
        item.eventName = "Event Reward";
    }

    return item;
}

// ===========================================================================
// Private Animation
// ===========================================================================

void EventUI::UpdateAnimations(float deltaTime) {
    m_animationTimer += deltaTime;
    m_pulseTimer += deltaTime * m_theme.pulseSpeed;
    m_blinkTimer += deltaTime * m_theme.blinkSpeed;

    // Wrap timers
    if (m_animationTimer > 1000.0f) m_animationTimer = 0.0f;
    if (m_pulseTimer > 6.28318f) m_pulseTimer -= 6.28318f;
    if (m_blinkTimer > 6.28318f) m_blinkTimer -= 6.28318f;
}

float EventUI::GetPulseValue() const {
    return 0.5f + 0.5f * std::sin(m_pulseTimer);
}

float EventUI::GetBlinkValue() const {
    return std::sin(m_blinkTimer) > 0.0f ? 1.0f : 0.3f;
}

int64_t EventUI::GetCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

} // namespace Vehement
