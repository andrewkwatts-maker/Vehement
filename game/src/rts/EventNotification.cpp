#include "EventNotification.hpp"
#include "EventScheduler.hpp"
#include "../network/FirebaseManager.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>

// Logging macros
#if __has_include("../../../engine/core/Logger.hpp")
#include "../../../engine/core/Logger.hpp"
#define NOTIF_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define NOTIF_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define NOTIF_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define NOTIF_LOG_INFO(...) std::cout << "[EventNotification] " << __VA_ARGS__ << std::endl
#define NOTIF_LOG_WARN(...) std::cerr << "[EventNotification WARN] " << __VA_ARGS__ << std::endl
#define NOTIF_LOG_ERROR(...) std::cerr << "[EventNotification ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ===========================================================================
// EventNotification
// ===========================================================================

EventNotification::EventNotification()
    : eventType(EventType::SupplyDrop)
    , iconColor(0xFFFFFFFF)
    , priority(NotificationPriority::Normal)
    , displayType(NotificationDisplay::Toast)
    , displayDuration(5.0f)
    , canDismiss(true)
    , playSound(true)
    , timestamp(0)
    , expiresAt(0)
    , isRead(false)
    , isDismissed(false)
    , isExpired(false)
    , hasLocation(false)
    , location(0.0f, 0.0f)
    , locationRadius(0.0f)
    , hasAction(false) {
}

EventNotification EventNotification::FromWorldEvent(const WorldEvent& event, bool isWarning) {
    EventNotification notif;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    notif.id = event.id + (isWarning ? "_warning" : "_start");
    notif.eventId = event.id;
    notif.eventType = event.type;
    notif.timestamp = timestamp;

    if (isWarning) {
        notif.title = "Incoming: " + event.name;
        notif.message = "Event starting soon! " + event.description;
    } else {
        notif.title = event.name;
        notif.message = event.description;
    }

    // Set location if not global
    if (!event.isGlobal) {
        notif.hasLocation = true;
        notif.location = event.location;
        notif.locationRadius = event.radius;
    }

    // Set priority based on event type
    EventCategory category = GetEventCategory(event.type);
    EventSeverity severity = GetDefaultSeverity(event.type);

    switch (severity) {
        case EventSeverity::Catastrophic:
            notif.priority = NotificationPriority::Critical;
            notif.displayType = NotificationDisplay::Banner;
            notif.displayDuration = 15.0f;
            notif.canDismiss = false;
            break;
        case EventSeverity::Major:
            notif.priority = NotificationPriority::Urgent;
            notif.displayType = NotificationDisplay::Banner;
            notif.displayDuration = 10.0f;
            break;
        case EventSeverity::Moderate:
            notif.priority = NotificationPriority::High;
            notif.displayType = NotificationDisplay::Toast;
            notif.displayDuration = 7.0f;
            break;
        default:
            notif.priority = NotificationPriority::Normal;
            notif.displayType = NotificationDisplay::Toast;
            notif.displayDuration = 5.0f;
            break;
    }

    // Set expiration
    notif.expiresAt = event.endTime;

    return notif;
}

nlohmann::json EventNotification::ToJson() const {
    return {
        {"id", id},
        {"eventId", eventId},
        {"eventType", EventTypeToString(eventType)},
        {"title", title},
        {"message", message},
        {"iconPath", iconPath},
        {"iconColor", iconColor},
        {"priority", static_cast<int>(priority)},
        {"displayType", static_cast<int>(displayType)},
        {"displayDuration", displayDuration},
        {"canDismiss", canDismiss},
        {"playSound", playSound},
        {"soundPath", soundPath},
        {"timestamp", timestamp},
        {"expiresAt", expiresAt},
        {"isRead", isRead},
        {"isDismissed", isDismissed},
        {"hasLocation", hasLocation},
        {"location", {{"x", location.x}, {"y", location.y}}},
        {"locationRadius", locationRadius},
        {"hasAction", hasAction},
        {"actionText", actionText},
        {"actionCallback", actionCallback}
    };
}

EventNotification EventNotification::FromJson(const nlohmann::json& j) {
    EventNotification notif;

    notif.id = j.value("id", "");
    notif.eventId = j.value("eventId", "");
    if (auto typeOpt = StringToEventType(j.value("eventType", ""))) {
        notif.eventType = *typeOpt;
    }
    notif.title = j.value("title", "");
    notif.message = j.value("message", "");
    notif.iconPath = j.value("iconPath", "");
    notif.iconColor = j.value("iconColor", 0xFFFFFFFF);
    notif.priority = static_cast<NotificationPriority>(j.value("priority", 1));
    notif.displayType = static_cast<NotificationDisplay>(j.value("displayType", 0));
    notif.displayDuration = j.value("displayDuration", 5.0f);
    notif.canDismiss = j.value("canDismiss", true);
    notif.playSound = j.value("playSound", true);
    notif.soundPath = j.value("soundPath", "");
    notif.timestamp = j.value("timestamp", int64_t(0));
    notif.expiresAt = j.value("expiresAt", int64_t(0));
    notif.isRead = j.value("isRead", false);
    notif.isDismissed = j.value("isDismissed", false);
    notif.hasLocation = j.value("hasLocation", false);
    if (j.contains("location")) {
        notif.location.x = j["location"].value("x", 0.0f);
        notif.location.y = j["location"].value("y", 0.0f);
    }
    notif.locationRadius = j.value("locationRadius", 0.0f);
    notif.hasAction = j.value("hasAction", false);
    notif.actionText = j.value("actionText", "");
    notif.actionCallback = j.value("actionCallback", "");

    return notif;
}

// ===========================================================================
// EventNotificationManager
// ===========================================================================

EventNotificationManager::EventNotificationManager() {
}

EventNotificationManager::~EventNotificationManager() {
    if (m_initialized) {
        Shutdown();
    }
}

bool EventNotificationManager::Initialize() {
    if (m_initialized) {
        NOTIF_LOG_WARN("EventNotificationManager already initialized");
        return true;
    }

    // Load history from persistent storage
    LoadHistory();

    m_initialized = true;
    NOTIF_LOG_INFO("EventNotificationManager initialized");
    return true;
}

void EventNotificationManager::Shutdown() {
    if (!m_initialized) return;

    NOTIF_LOG_INFO("Shutting down EventNotificationManager");

    // Save history
    if (m_config.persistHistory) {
        SaveHistory();
    }

    // Stop Firebase listener
    if (!m_firebaseListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_firebaseListenerId);
        m_firebaseListenerId.clear();
    }

    // Clear data
    {
        std::lock_guard<std::mutex> lock(m_notificationMutex);
        m_activeNotifications.clear();
        m_notificationQueue.clear();
        m_history.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_markerMutex);
        m_minimapMarkers.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_displayCallbacks.clear();
        m_dismissCallbacks.clear();
        m_actionCallbacks.clear();
        m_soundCallbacks.clear();
        m_markerCallbacks.clear();
    }

    m_initialized = false;
}

void EventNotificationManager::SetEventScheduler(EventScheduler* scheduler) {
    m_scheduler = scheduler;

    if (scheduler) {
        // Register for event callbacks
        scheduler->OnEventScheduled([this](const WorldEvent& event) {
            NotifyEventWarning(event);
        });

        scheduler->OnEventStarted([this](const WorldEvent& event) {
            NotifyEventStarted(event);
            AddMinimapMarker(event);
        });

        scheduler->OnEventEnded([this](const WorldEvent& event) {
            NotifyEventEnded(event, true);
            RemoveMinimapMarker(event.id);
        });

        scheduler->OnEventCancelled([this](const WorldEvent& event) {
            RemoveMinimapMarker(event.id);
        });
    }
}

void EventNotificationManager::LoadConfig(const NotificationConfig& config) {
    m_config = config;
}

void EventNotificationManager::Update(float deltaTime) {
    if (!m_initialized) return;

    // Process notification queue
    ProcessNotificationQueue();

    // Expire old notifications
    ExpireOldNotifications();

    // Update minimap markers
    UpdateMarkers();
}

void EventNotificationManager::ShowNotification(const EventNotification& notification) {
    if (!ShouldShowNotification(notification)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_notificationMutex);

        // Check if we should group with existing notification
        bool grouped = false;
        if (m_config.groupSimilar) {
            auto lastTimeIt = m_lastNotificationTime.find(notification.eventType);
            if (lastTimeIt != m_lastNotificationTime.end()) {
                int64_t timeSinceLast = GetCurrentTimeMs() - lastTimeIt->second;
                if (timeSinceLast < static_cast<int64_t>(m_config.groupingTimeWindow * 1000)) {
                    m_groupedCounts[notification.eventType]++;
                    grouped = true;
                }
            }
        }

        if (!grouped) {
            m_notificationQueue.push_back(notification);
            m_lastNotificationTime[notification.eventType] = GetCurrentTimeMs();
            m_groupedCounts[notification.eventType] = 1;
        }
    }

    // Play sound if enabled
    if (notification.playSound && m_config.enableSounds) {
        PlayNotificationSound(notification.priority);
    }
}

void EventNotificationManager::NotifyEvent(const WorldEvent& event, bool isWarning) {
    EventNotification notif = CreateEventNotification(event, isWarning);
    ShowNotification(notif);
}

void EventNotificationManager::NotifyEventStarted(const WorldEvent& event) {
    EventNotification notif = CreateEventNotification(event, false);
    notif.title = "Event Started: " + event.name;
    ShowNotification(notif);
}

void EventNotificationManager::NotifyEventEnded(const WorldEvent& event, bool wasSuccessful) {
    EventNotification notif;
    notif.id = event.id + "_ended";
    notif.eventId = event.id;
    notif.eventType = event.type;
    notif.timestamp = GetCurrentTimeMs();

    if (wasSuccessful) {
        notif.title = "Event Complete: " + event.name;
        notif.message = "The event has ended successfully.";
    } else {
        notif.title = "Event Failed: " + event.name;
        notif.message = "The event has ended without completion.";
    }

    notif.priority = NotificationPriority::Normal;
    notif.displayType = NotificationDisplay::Toast;
    notif.displayDuration = 5.0f;

    ShowNotification(notif);
}

void EventNotificationManager::NotifyEventWarning(const WorldEvent& event) {
    EventNotification notif = CreateEventNotification(event, true);

    // Calculate time until start
    int64_t timeUntilStart = event.startTime - GetCurrentTimeMs();
    int32_t secondsUntilStart = static_cast<int32_t>(timeUntilStart / 1000);
    int32_t minutesUntilStart = secondsUntilStart / 60;

    if (minutesUntilStart > 0) {
        notif.message = "Starting in " + std::to_string(minutesUntilStart) + " minute(s). " + event.description;
    } else {
        notif.message = "Starting in " + std::to_string(secondsUntilStart) + " seconds. " + event.description;
    }

    ShowNotification(notif);
}

void EventNotificationManager::ShowCustomNotification(const std::string& title,
                                                       const std::string& message,
                                                       NotificationPriority priority,
                                                       NotificationDisplay display) {
    EventNotification notif;
    notif.id = "custom_" + std::to_string(GetCurrentTimeMs());
    notif.title = title;
    notif.message = message;
    notif.priority = priority;
    notif.displayType = display;
    notif.displayDuration = m_config.defaultToastDuration;
    notif.timestamp = GetCurrentTimeMs();
    notif.canDismiss = true;

    ShowNotification(notif);
}

void EventNotificationManager::DismissNotification(const std::string& notificationId) {
    std::lock_guard<std::mutex> lock(m_notificationMutex);

    auto it = m_activeNotifications.find(notificationId);
    if (it != m_activeNotifications.end()) {
        if (!it->second.canDismiss) {
            NOTIF_LOG_WARN("Cannot dismiss notification: " + notificationId);
            return;
        }

        it->second.isDismissed = true;
        AddToHistory(it->second);
        m_activeNotifications.erase(it);

        // Invoke callbacks
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& callback : m_dismissCallbacks) {
            if (callback) callback(notificationId);
        }
    }
}

void EventNotificationManager::DismissAllNotifications() {
    std::lock_guard<std::mutex> lock(m_notificationMutex);

    std::vector<std::string> toDismiss;
    for (const auto& [id, notif] : m_activeNotifications) {
        if (notif.canDismiss) {
            toDismiss.push_back(id);
        }
    }

    for (const auto& id : toDismiss) {
        auto it = m_activeNotifications.find(id);
        if (it != m_activeNotifications.end()) {
            it->second.isDismissed = true;
            AddToHistory(it->second);
            m_activeNotifications.erase(it);
        }
    }
}

void EventNotificationManager::MarkAsRead(const std::string& notificationId) {
    std::lock_guard<std::mutex> lock(m_notificationMutex);

    auto it = m_activeNotifications.find(notificationId);
    if (it != m_activeNotifications.end()) {
        it->second.isRead = true;
    }
}

void EventNotificationManager::MarkAllAsRead() {
    std::lock_guard<std::mutex> lock(m_notificationMutex);

    for (auto& [id, notif] : m_activeNotifications) {
        notif.isRead = true;
    }
}

std::vector<EventNotification> EventNotificationManager::GetActiveNotifications() const {
    std::lock_guard<std::mutex> lock(m_notificationMutex);
    std::vector<EventNotification> notifications;
    notifications.reserve(m_activeNotifications.size());

    for (const auto& [id, notif] : m_activeNotifications) {
        notifications.push_back(notif);
    }

    // Sort by priority and timestamp
    std::sort(notifications.begin(), notifications.end(),
              [](const EventNotification& a, const EventNotification& b) {
                  if (a.priority != b.priority) {
                      return static_cast<int>(a.priority) > static_cast<int>(b.priority);
                  }
                  return a.timestamp > b.timestamp;
              });

    return notifications;
}

std::vector<EventNotification> EventNotificationManager::GetPendingNotifications() const {
    std::lock_guard<std::mutex> lock(m_notificationMutex);
    return std::vector<EventNotification>(m_notificationQueue.begin(), m_notificationQueue.end());
}

std::vector<NotificationHistoryEntry> EventNotificationManager::GetHistory() const {
    std::lock_guard<std::mutex> lock(m_notificationMutex);
    return std::vector<NotificationHistoryEntry>(m_history.begin(), m_history.end());
}

int32_t EventNotificationManager::GetUnreadCount() const {
    std::lock_guard<std::mutex> lock(m_notificationMutex);
    int32_t count = 0;
    for (const auto& [id, notif] : m_activeNotifications) {
        if (!notif.isRead) count++;
    }
    return count;
}

bool EventNotificationManager::HasUrgentNotifications() const {
    std::lock_guard<std::mutex> lock(m_notificationMutex);
    for (const auto& [id, notif] : m_activeNotifications) {
        if (notif.priority >= NotificationPriority::Urgent && !notif.isRead) {
            return true;
        }
    }
    return false;
}

std::optional<EventNotification> EventNotificationManager::GetNotification(
    const std::string& notificationId) const {
    std::lock_guard<std::mutex> lock(m_notificationMutex);

    auto it = m_activeNotifications.find(notificationId);
    if (it != m_activeNotifications.end()) {
        return it->second;
    }
    return std::nullopt;
}

void EventNotificationManager::AddMinimapMarker(const WorldEvent& event) {
    if (event.isGlobal) return; // No marker for global events

    MinimapMarker marker = CreateMarkerForEvent(event);

    {
        std::lock_guard<std::mutex> lock(m_markerMutex);
        m_minimapMarkers[event.id] = marker;
    }

    // Notify callbacks
    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    auto markers = GetMinimapMarkers();
    for (const auto& callback : m_markerCallbacks) {
        if (callback) callback(markers);
    }
}

void EventNotificationManager::RemoveMinimapMarker(const std::string& eventId) {
    {
        std::lock_guard<std::mutex> lock(m_markerMutex);
        m_minimapMarkers.erase(eventId);
    }

    // Notify callbacks
    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    auto markers = GetMinimapMarkers();
    for (const auto& callback : m_markerCallbacks) {
        if (callback) callback(markers);
    }
}

std::vector<MinimapMarker> EventNotificationManager::GetMinimapMarkers() const {
    std::lock_guard<std::mutex> lock(m_markerMutex);
    std::vector<MinimapMarker> markers;
    markers.reserve(m_minimapMarkers.size());

    for (const auto& [id, marker] : m_minimapMarkers) {
        markers.push_back(marker);
    }

    return markers;
}

void EventNotificationManager::UpdateMarkerPosition(const std::string& eventId,
                                                     const glm::vec2& position) {
    std::lock_guard<std::mutex> lock(m_markerMutex);

    auto it = m_minimapMarkers.find(eventId);
    if (it != m_minimapMarkers.end()) {
        it->second.position = position;
    }
}

void EventNotificationManager::EnablePushNotifications(bool enable) {
    m_pushEnabled = enable;

    if (enable && FirebaseManager::Instance().IsInitialized()) {
        // Start listening for push notifications
        m_firebaseListenerId = FirebaseManager::Instance().ListenToPath(
            "notifications/" + FirebaseManager::Instance().GetUserId(),
            [this](const nlohmann::json& data) {
                HandlePushNotification(data);
            }
        );
    } else if (!enable && !m_firebaseListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_firebaseListenerId);
        m_firebaseListenerId.clear();
    }
}

void EventNotificationManager::HandlePushNotification(const nlohmann::json& payload) {
    if (!payload.is_object()) return;

    try {
        EventNotification notif = EventNotification::FromJson(payload);
        ShowNotification(notif);
    } catch (const std::exception& e) {
        NOTIF_LOG_ERROR("Failed to parse push notification: " + std::string(e.what()));
    }
}

void EventNotificationManager::SendPushNotification(const EventNotification& notification,
                                                     const std::vector<std::string>& playerIds) {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    nlohmann::json notifJson = notification.ToJson();

    for (const auto& playerId : playerIds) {
        std::string path = "notifications/" + playerId + "/" + notification.id;
        FirebaseManager::Instance().SetValue(path, notifJson);
    }
}

void EventNotificationManager::PlayNotificationSound(NotificationPriority priority) {
    std::string soundPath;

    switch (priority) {
        case NotificationPriority::Critical:
            soundPath = m_config.criticalSoundPath;
            break;
        case NotificationPriority::Urgent:
            soundPath = m_config.urgentSoundPath;
            break;
        default:
            soundPath = m_config.defaultSoundPath;
            break;
    }

    PlaySound(soundPath);
}

void EventNotificationManager::PlaySound(const std::string& soundPath) {
    if (!m_config.enableSounds) return;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_soundCallbacks) {
        if (callback) callback(soundPath, m_config.soundVolume);
    }
}

void EventNotificationManager::OnNotificationDisplay(DisplayCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_displayCallbacks.push_back(std::move(callback));
}

void EventNotificationManager::OnNotificationDismissed(DismissCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_dismissCallbacks.push_back(std::move(callback));
}

void EventNotificationManager::OnNotificationAction(ActionCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_actionCallbacks.push_back(std::move(callback));
}

void EventNotificationManager::OnPlaySound(SoundCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_soundCallbacks.push_back(std::move(callback));
}

void EventNotificationManager::OnMarkersUpdated(MarkerCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_markerCallbacks.push_back(std::move(callback));
}

// ===========================================================================
// Private Helpers
// ===========================================================================

EventNotification EventNotificationManager::CreateEventNotification(const WorldEvent& event,
                                                                      bool isWarning) {
    EventNotification notif = EventNotification::FromWorldEvent(event, isWarning);
    SetNotificationStyle(notif, event);
    return notif;
}

void EventNotificationManager::SetNotificationStyle(EventNotification& notif,
                                                     const WorldEvent& event) {
    notif.iconPath = GetIconForEventType(event.type);
    notif.iconColor = GetColorForEventType(event.type);
    notif.priority = GetPriorityForEvent(event);
}

NotificationPriority EventNotificationManager::GetPriorityForEvent(const WorldEvent& event) {
    EventSeverity severity = GetDefaultSeverity(event.type);

    switch (severity) {
        case EventSeverity::Catastrophic: return NotificationPriority::Critical;
        case EventSeverity::Major: return NotificationPriority::Urgent;
        case EventSeverity::Moderate: return NotificationPriority::High;
        case EventSeverity::Minor: return NotificationPriority::Normal;
        default: return NotificationPriority::Low;
    }
}

std::string EventNotificationManager::GetIconForEventType(EventType type) {
    EventCategory category = GetEventCategory(type);

    switch (category) {
        case EventCategory::Threat:
            return "icons/event_threat.png";
        case EventCategory::Opportunity:
            return "icons/event_opportunity.png";
        case EventCategory::Environmental:
            return "icons/event_weather.png";
        case EventCategory::Social:
            return "icons/event_social.png";
        case EventCategory::Global:
            return "icons/event_global.png";
        default:
            return "icons/event_default.png";
    }
}

uint32_t EventNotificationManager::GetColorForEventType(EventType type) {
    EventCategory category = GetEventCategory(type);

    switch (category) {
        case EventCategory::Threat:
            return 0xFFFF4444; // Red
        case EventCategory::Opportunity:
            return 0xFF44FF44; // Green
        case EventCategory::Environmental:
            return 0xFF4488FF; // Blue
        case EventCategory::Social:
            return 0xFFFFFF44; // Yellow
        case EventCategory::Global:
            return 0xFFFF44FF; // Purple
        default:
            return 0xFFFFFFFF; // White
    }
}

void EventNotificationManager::ProcessNotificationQueue() {
    std::lock_guard<std::mutex> lock(m_notificationMutex);

    // Limit visible notifications by type
    int32_t visibleToasts = 0;
    int32_t visibleBanners = 0;

    for (const auto& [id, notif] : m_activeNotifications) {
        if (notif.displayType == NotificationDisplay::Toast) visibleToasts++;
        else if (notif.displayType == NotificationDisplay::Banner) visibleBanners++;
    }

    // Process queue
    while (!m_notificationQueue.empty()) {
        EventNotification& notif = m_notificationQueue.front();

        // Check if we can show this notification
        bool canShow = false;
        if (notif.displayType == NotificationDisplay::Toast &&
            visibleToasts < m_config.maxVisibleToasts) {
            canShow = true;
            visibleToasts++;
        } else if (notif.displayType == NotificationDisplay::Banner &&
                   visibleBanners < m_config.maxVisibleBanners) {
            canShow = true;
            visibleBanners++;
        } else if (notif.displayType == NotificationDisplay::Modal) {
            canShow = true; // Modals always show
        }

        if (canShow) {
            m_activeNotifications[notif.id] = notif;

            // Invoke display callbacks
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            for (const auto& callback : m_displayCallbacks) {
                if (callback) callback(notif);
            }

            m_notificationQueue.pop_front();
        } else {
            break; // Can't show more right now
        }
    }
}

void EventNotificationManager::ExpireOldNotifications() {
    int64_t currentTime = GetCurrentTimeMs();
    std::vector<std::string> toRemove;

    {
        std::lock_guard<std::mutex> lock(m_notificationMutex);

        for (auto& [id, notif] : m_activeNotifications) {
            // Check if expired by time
            if (notif.expiresAt > 0 && currentTime >= notif.expiresAt) {
                notif.isExpired = true;
                toRemove.push_back(id);
            }
            // Check if display duration exceeded (for auto-dismiss notifications)
            else if (notif.displayDuration > 0 && notif.canDismiss) {
                float displayedSeconds = static_cast<float>(currentTime - notif.timestamp) / 1000.0f;
                if (displayedSeconds >= notif.displayDuration) {
                    toRemove.push_back(id);
                }
            }
        }
    }

    // Remove expired notifications
    for (const auto& id : toRemove) {
        DismissNotification(id);
    }
}

bool EventNotificationManager::ShouldShowNotification(const EventNotification& notification) const {
    switch (notification.priority) {
        case NotificationPriority::Low:
            return m_config.showLowPriority;
        case NotificationPriority::Normal:
            return m_config.showNormalPriority;
        case NotificationPriority::High:
            return m_config.showHighPriority;
        case NotificationPriority::Urgent:
        case NotificationPriority::Critical:
            return m_config.showUrgentPriority;
        default:
            return true;
    }
}

bool EventNotificationManager::CanGroupWith(const EventNotification& a,
                                             const EventNotification& b) const {
    if (!m_config.groupSimilar) return false;
    if (a.eventType != b.eventType) return false;

    int64_t timeDiff = std::abs(a.timestamp - b.timestamp);
    return timeDiff < static_cast<int64_t>(m_config.groupingTimeWindow * 1000);
}

void EventNotificationManager::AddToHistory(const EventNotification& notification) {
    NotificationHistoryEntry entry;
    entry.notification = notification;
    entry.receivedAt = notification.timestamp;
    entry.readAt = notification.isRead ? GetCurrentTimeMs() : 0;
    entry.dismissedAt = GetCurrentTimeMs();

    m_history.push_back(entry);
    TrimHistory();
}

void EventNotificationManager::TrimHistory() {
    while (m_history.size() > static_cast<size_t>(m_config.maxHistoryEntries)) {
        m_history.pop_front();
    }
}

void EventNotificationManager::SaveHistory() {
    // Would save to persistent storage
    // For now, just log
    NOTIF_LOG_INFO("Saving notification history (" +
                   std::to_string(m_history.size()) + " entries)");
}

void EventNotificationManager::LoadHistory() {
    // Would load from persistent storage
    NOTIF_LOG_INFO("Loading notification history");
}

MinimapMarker EventNotificationManager::CreateMarkerForEvent(const WorldEvent& event) {
    MinimapMarker marker;
    marker.eventId = event.id;
    marker.eventType = event.type;
    marker.position = event.location;
    marker.radius = event.radius;
    marker.color = GetColorForEventType(event.type);
    marker.iconPath = GetIconForEventType(event.type);
    marker.expiresAt = event.endTime;

    // Set visual properties based on severity
    EventSeverity severity = GetDefaultSeverity(event.type);
    switch (severity) {
        case EventSeverity::Catastrophic:
        case EventSeverity::Major:
            marker.pulseSpeed = 2.0f;
            marker.isBlinking = true;
            break;
        case EventSeverity::Moderate:
            marker.pulseSpeed = 1.0f;
            marker.isBlinking = false;
            break;
        default:
            marker.pulseSpeed = 0.5f;
            marker.isBlinking = false;
            break;
    }

    return marker;
}

void EventNotificationManager::UpdateMarkers() {
    int64_t currentTime = GetCurrentTimeMs();
    std::vector<std::string> expiredMarkers;

    {
        std::lock_guard<std::mutex> lock(m_markerMutex);
        for (const auto& [id, marker] : m_minimapMarkers) {
            if (marker.IsExpired(currentTime)) {
                expiredMarkers.push_back(id);
            }
        }
    }

    for (const auto& id : expiredMarkers) {
        RemoveMinimapMarker(id);
    }
}

int64_t EventNotificationManager::GetCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

} // namespace Vehement
