#pragma once

#include "WorldEvent.hpp"
#include <queue>
#include <deque>
#include <mutex>
#include <functional>
#include <memory>

namespace Vehement {

// Forward declarations
class FirebaseManager;
class EventScheduler;

/**
 * @brief Priority levels for notifications
 */
enum class NotificationPriority : uint8_t {
    Low,        ///< Minor events, can be missed
    Normal,     ///< Standard notifications
    High,       ///< Important events
    Urgent,     ///< Critical events requiring attention
    Critical    ///< Game-changing events, cannot be dismissed
};

/**
 * @brief Notification display types
 */
enum class NotificationDisplay : uint8_t {
    Toast,          ///< Brief popup that auto-dismisses
    Banner,         ///< Header banner notification
    Modal,          ///< Full modal dialog
    MinimapMarker,  ///< Marker on minimap only
    Sound,          ///< Audio notification only
    Combined        ///< Multiple display types
};

/**
 * @brief Single event notification
 */
struct EventNotification {
    // Identification
    std::string id;
    std::string eventId;
    EventType eventType;

    // Content
    std::string title;
    std::string message;
    std::string iconPath;
    uint32_t iconColor;

    // Display settings
    NotificationPriority priority;
    NotificationDisplay displayType;
    float displayDuration;          ///< How long to show (0 = until dismissed)
    bool canDismiss;                ///< Can player dismiss this
    bool playSound;
    std::string soundPath;

    // Timing
    int64_t timestamp;              ///< When notification was created
    int64_t expiresAt;              ///< When notification expires (0 = never)

    // State
    bool isRead;
    bool isDismissed;
    bool isExpired;

    // Location (for minimap)
    bool hasLocation;
    glm::vec2 location;
    float locationRadius;

    // Actions
    bool hasAction;
    std::string actionText;
    std::string actionCallback;     ///< Callback identifier

    /**
     * @brief Default constructor
     */
    EventNotification();

    /**
     * @brief Create notification from world event
     */
    static EventNotification FromWorldEvent(const WorldEvent& event, bool isWarning = false);

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static EventNotification FromJson(const nlohmann::json& json);
};

/**
 * @brief Notification history entry
 */
struct NotificationHistoryEntry {
    EventNotification notification;
    int64_t receivedAt;
    int64_t readAt;
    int64_t dismissedAt;
};

/**
 * @brief Configuration for notification behavior
 */
struct NotificationConfig {
    // Display settings
    float defaultToastDuration = 5.0f;
    float defaultBannerDuration = 8.0f;
    int32_t maxVisibleToasts = 3;
    int32_t maxVisibleBanners = 1;

    // Sound settings
    bool enableSounds = true;
    float soundVolume = 1.0f;
    std::string defaultSoundPath = "audio/notification.wav";
    std::string urgentSoundPath = "audio/notification_urgent.wav";
    std::string criticalSoundPath = "audio/notification_critical.wav";

    // Filter settings
    bool showLowPriority = true;
    bool showNormalPriority = true;
    bool showHighPriority = true;
    bool showUrgentPriority = true;

    // History settings
    int32_t maxHistoryEntries = 100;
    bool persistHistory = true;

    // Grouping
    bool groupSimilar = true;
    float groupingTimeWindow = 30.0f; // seconds
};

/**
 * @brief Minimap marker for events
 */
struct MinimapMarker {
    std::string eventId;
    EventType eventType;
    glm::vec2 position;
    float radius;
    uint32_t color;
    std::string iconPath;
    float pulseSpeed;
    bool isBlinking;
    int64_t expiresAt;

    [[nodiscard]] bool IsExpired(int64_t currentTimeMs) const {
        return expiresAt > 0 && currentTimeMs >= expiresAt;
    }
};

/**
 * @brief Manages event notifications to players
 *
 * Responsibilities:
 * - Queue and display notifications
 * - Handle Firebase push notifications
 * - Manage notification history
 * - Show minimap markers for events
 * - Play notification sounds
 */
class EventNotificationManager {
public:
    EventNotificationManager();
    ~EventNotificationManager();

    // Non-copyable
    EventNotificationManager(const EventNotificationManager&) = delete;
    EventNotificationManager& operator=(const EventNotificationManager&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the notification manager
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Shutdown the manager
     */
    void Shutdown();

    /**
     * @brief Set reference to event scheduler for listening
     */
    void SetEventScheduler(EventScheduler* scheduler);

    /**
     * @brief Load configuration
     */
    void LoadConfig(const NotificationConfig& config);

    /**
     * @brief Save configuration
     */
    [[nodiscard]] NotificationConfig GetConfig() const { return m_config; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update notifications (process queue, expire old notifications)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Notification Management
    // =========================================================================

    /**
     * @brief Show a notification
     * @param notification The notification to show
     */
    void ShowNotification(const EventNotification& notification);

    /**
     * @brief Show notification for a world event
     * @param event The world event
     * @param isWarning If true, this is a warning before event starts
     */
    void NotifyEvent(const WorldEvent& event, bool isWarning = false);

    /**
     * @brief Show notification when event starts
     */
    void NotifyEventStarted(const WorldEvent& event);

    /**
     * @brief Show notification when event ends
     */
    void NotifyEventEnded(const WorldEvent& event, bool wasSuccessful = true);

    /**
     * @brief Show warning for upcoming event
     */
    void NotifyEventWarning(const WorldEvent& event);

    /**
     * @brief Create custom notification
     */
    void ShowCustomNotification(const std::string& title,
                                const std::string& message,
                                NotificationPriority priority = NotificationPriority::Normal,
                                NotificationDisplay display = NotificationDisplay::Toast);

    /**
     * @brief Dismiss a specific notification
     */
    void DismissNotification(const std::string& notificationId);

    /**
     * @brief Dismiss all notifications
     */
    void DismissAllNotifications();

    /**
     * @brief Mark notification as read
     */
    void MarkAsRead(const std::string& notificationId);

    /**
     * @brief Mark all notifications as read
     */
    void MarkAllAsRead();

    // =========================================================================
    // Notification Queries
    // =========================================================================

    /**
     * @brief Get all active (visible) notifications
     */
    [[nodiscard]] std::vector<EventNotification> GetActiveNotifications() const;

    /**
     * @brief Get pending notifications in queue
     */
    [[nodiscard]] std::vector<EventNotification> GetPendingNotifications() const;

    /**
     * @brief Get notification history
     */
    [[nodiscard]] std::vector<NotificationHistoryEntry> GetHistory() const;

    /**
     * @brief Get unread notification count
     */
    [[nodiscard]] int32_t GetUnreadCount() const;

    /**
     * @brief Check if any urgent notifications are active
     */
    [[nodiscard]] bool HasUrgentNotifications() const;

    /**
     * @brief Get notification by ID
     */
    [[nodiscard]] std::optional<EventNotification> GetNotification(
        const std::string& notificationId) const;

    // =========================================================================
    // Minimap Markers
    // =========================================================================

    /**
     * @brief Add a minimap marker for an event
     */
    void AddMinimapMarker(const WorldEvent& event);

    /**
     * @brief Remove minimap marker
     */
    void RemoveMinimapMarker(const std::string& eventId);

    /**
     * @brief Get all active minimap markers
     */
    [[nodiscard]] std::vector<MinimapMarker> GetMinimapMarkers() const;

    /**
     * @brief Update minimap marker position
     */
    void UpdateMarkerPosition(const std::string& eventId, const glm::vec2& position);

    // =========================================================================
    // Firebase Integration
    // =========================================================================

    /**
     * @brief Enable Firebase push notifications
     */
    void EnablePushNotifications(bool enable);

    /**
     * @brief Handle incoming push notification
     */
    void HandlePushNotification(const nlohmann::json& payload);

    /**
     * @brief Send push notification to other players
     */
    void SendPushNotification(const EventNotification& notification,
                              const std::vector<std::string>& playerIds);

    // =========================================================================
    // Sound
    // =========================================================================

    /**
     * @brief Play notification sound
     */
    void PlayNotificationSound(NotificationPriority priority);

    /**
     * @brief Play custom sound
     */
    void PlaySound(const std::string& soundPath);

    /**
     * @brief Set sound enabled
     */
    void SetSoundEnabled(bool enabled) { m_config.enableSounds = enabled; }

    /**
     * @brief Set sound volume
     */
    void SetSoundVolume(float volume) { m_config.soundVolume = volume; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Callback for when notification should be displayed
     */
    using DisplayCallback = std::function<void(const EventNotification&)>;
    void OnNotificationDisplay(DisplayCallback callback);

    /**
     * @brief Callback for when notification is dismissed
     */
    using DismissCallback = std::function<void(const std::string& notificationId)>;
    void OnNotificationDismissed(DismissCallback callback);

    /**
     * @brief Callback for notification action button clicked
     */
    using ActionCallback = std::function<void(const std::string& notificationId,
                                              const std::string& actionCallback)>;
    void OnNotificationAction(ActionCallback callback);

    /**
     * @brief Callback for sound playback
     */
    using SoundCallback = std::function<void(const std::string& soundPath, float volume)>;
    void OnPlaySound(SoundCallback callback);

    /**
     * @brief Callback for minimap marker updates
     */
    using MarkerCallback = std::function<void(const std::vector<MinimapMarker>&)>;
    void OnMarkersUpdated(MarkerCallback callback);

private:
    // Notification creation helpers
    EventNotification CreateEventNotification(const WorldEvent& event, bool isWarning);
    void SetNotificationStyle(EventNotification& notif, const WorldEvent& event);
    NotificationPriority GetPriorityForEvent(const WorldEvent& event);
    std::string GetIconForEventType(EventType type);
    uint32_t GetColorForEventType(EventType type);

    // Queue management
    void ProcessNotificationQueue();
    void ExpireOldNotifications();
    bool ShouldShowNotification(const EventNotification& notification) const;
    bool CanGroupWith(const EventNotification& a, const EventNotification& b) const;

    // History management
    void AddToHistory(const EventNotification& notification);
    void TrimHistory();
    void SaveHistory();
    void LoadHistory();

    // Minimap helpers
    MinimapMarker CreateMarkerForEvent(const WorldEvent& event);
    void UpdateMarkers();

    // Time utilities
    [[nodiscard]] int64_t GetCurrentTimeMs() const;

    // State
    bool m_initialized = false;
    NotificationConfig m_config;
    EventScheduler* m_scheduler = nullptr;

    // Active notifications
    std::map<std::string, EventNotification> m_activeNotifications;
    std::deque<EventNotification> m_notificationQueue;
    std::deque<NotificationHistoryEntry> m_history;
    mutable std::mutex m_notificationMutex;

    // Minimap markers
    std::map<std::string, MinimapMarker> m_minimapMarkers;
    mutable std::mutex m_markerMutex;

    // Firebase
    bool m_pushEnabled = false;
    std::string m_firebaseListenerId;

    // Callbacks
    std::vector<DisplayCallback> m_displayCallbacks;
    std::vector<DismissCallback> m_dismissCallbacks;
    std::vector<ActionCallback> m_actionCallbacks;
    std::vector<SoundCallback> m_soundCallbacks;
    std::vector<MarkerCallback> m_markerCallbacks;
    std::mutex m_callbackMutex;

    // Tracking for grouping
    std::map<EventType, int64_t> m_lastNotificationTime;
    std::map<EventType, int32_t> m_groupedCounts;
};

} // namespace Vehement
