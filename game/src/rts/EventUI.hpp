#pragma once

#include "WorldEvent.hpp"
#include "EventNotification.hpp"
#include "EventParticipation.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <mutex>

namespace Vehement {

// Forward declarations
class EventScheduler;
class EventNotificationManager;
class EventParticipationManager;
class EventEffects;

/**
 * @brief UI panel types for event system
 */
enum class EventPanelType : uint8_t {
    None,
    EventBanner,        ///< Top banner for active events
    EventList,          ///< List of all events
    EventDetails,       ///< Detailed view of single event
    EventLog,           ///< History of past events
    Leaderboard,        ///< Event leaderboard
    Rewards,            ///< Unclaimed rewards panel
    Notifications       ///< Notification center
};

/**
 * @brief Banner display information
 */
struct EventBannerData {
    std::string eventId;
    std::string title;
    std::string subtitle;
    std::string iconPath;
    uint32_t backgroundColor;
    uint32_t textColor;
    float progress;             ///< 0.0 to 1.0 event progress
    int64_t remainingTimeMs;
    bool isUrgent;
    bool isBlinking;
    float animationTimer;
};

/**
 * @brief Event list item for UI display
 */
struct EventListItem {
    std::string eventId;
    EventType type;
    std::string name;
    std::string description;
    std::string statusText;
    std::string locationText;
    std::string timeText;
    std::string iconPath;
    uint32_t iconColor;
    EventCategory category;
    EventSeverity severity;
    bool isActive;
    bool isUpcoming;
    bool isParticipating;
    float progress;
    int64_t startTime;
    int64_t endTime;
    int32_t participantCount;
    glm::vec2 location;
    float distance;             ///< Distance from player
};

/**
 * @brief Detailed event view data
 */
struct EventDetailData {
    WorldEvent event;
    std::vector<EventObjective> objectives;
    EventReward potentialReward;
    std::vector<LeaderboardEntry> leaderboard;
    PlayerContribution playerContribution;
    bool isParticipating;
    bool canJoin;
    std::string joinBlockReason;
    std::vector<std::string> tips;
};

/**
 * @brief Event log entry for history
 */
struct EventLogEntry {
    std::string eventId;
    EventType type;
    std::string name;
    int64_t startTime;
    int64_t endTime;
    bool wasSuccessful;
    int32_t playerRank;
    int32_t totalParticipants;
    EventReward rewardEarned;
    bool rewardClaimed;
};

/**
 * @brief Reward display item
 */
struct RewardDisplayItem {
    std::string eventId;
    std::string eventName;
    EventReward reward;
    int64_t earnedAt;
    bool isNew;
};

/**
 * @brief UI theme configuration
 */
struct EventUITheme {
    // Colors
    uint32_t threatColor = 0xFFFF4444;
    uint32_t opportunityColor = 0xFF44FF44;
    uint32_t environmentalColor = 0xFF4488FF;
    uint32_t socialColor = 0xFFFFFF44;
    uint32_t globalColor = 0xFFFF44FF;

    // Banner colors
    uint32_t bannerBackgroundNormal = 0xDD333333;
    uint32_t bannerBackgroundUrgent = 0xDD662222;
    uint32_t bannerTextColor = 0xFFFFFFFF;

    // Panel colors
    uint32_t panelBackground = 0xEE222222;
    uint32_t panelBorder = 0xFF444444;
    uint32_t headerBackground = 0xFF333333;

    // Text colors
    uint32_t textPrimary = 0xFFFFFFFF;
    uint32_t textSecondary = 0xFFAAAAAA;
    uint32_t textHighlight = 0xFFFFDD44;

    // Animation
    float blinkSpeed = 2.0f;
    float pulseSpeed = 1.5f;
    float fadeSpeed = 0.5f;
};

/**
 * @brief UI rendering callbacks
 */
struct EventUICallbacks {
    // Panel rendering
    std::function<void(const EventBannerData&)> renderBanner;
    std::function<void(const std::vector<EventListItem>&)> renderEventList;
    std::function<void(const EventDetailData&)> renderEventDetails;
    std::function<void(const std::vector<EventLogEntry>&)> renderEventLog;
    std::function<void(const std::vector<LeaderboardEntry>&)> renderLeaderboard;
    std::function<void(const std::vector<RewardDisplayItem>&)> renderRewards;
    std::function<void(const std::vector<EventNotification>&)> renderNotifications;

    // UI elements
    std::function<void(float, float, float, uint32_t)> drawProgressBar;
    std::function<void(const glm::vec2&, float, uint32_t)> drawMinimapMarker;
    std::function<void(const std::string&, float, float)> drawIcon;

    // Text
    std::function<void(const std::string&, float, float, uint32_t)> drawText;
    std::function<void(const std::string&, float, float, uint32_t, float)> drawTextCentered;

    // Input
    std::function<bool(float, float, float, float)> isMouseInRect;
    std::function<bool()> isMouseClicked;
};

/**
 * @brief Manages UI for world events
 *
 * Responsibilities:
 * - Render event banners when events start
 * - Display event timer and progress
 * - Show event objectives
 * - Render rewards preview
 * - Display event history log
 * - Manage notification toasts
 */
class EventUI {
public:
    EventUI();
    ~EventUI();

    // Non-copyable
    EventUI(const EventUI&) = delete;
    EventUI& operator=(const EventUI&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the event UI
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Shutdown the UI
     */
    void Shutdown();

    /**
     * @brief Set references to event system components
     */
    void SetEventSystems(EventScheduler* scheduler,
                         EventNotificationManager* notifications,
                         EventParticipationManager* participation,
                         EventEffects* effects);

    /**
     * @brief Set UI rendering callbacks
     */
    void SetCallbacks(const EventUICallbacks& callbacks);

    /**
     * @brief Set UI theme
     */
    void SetTheme(const EventUITheme& theme);

    /**
     * @brief Set local player ID for participation tracking
     */
    void SetLocalPlayerId(const std::string& playerId) { m_localPlayerId = playerId; }

    /**
     * @brief Set player position for distance calculations
     */
    void SetPlayerPosition(const glm::vec2& position) { m_playerPosition = position; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update UI state
     */
    void Update(float deltaTime);

    /**
     * @brief Render all event UI elements
     */
    void Render();

    /**
     * @brief Render minimap markers for events
     */
    void RenderMinimapMarkers();

    /**
     * @brief Handle input events
     * @return true if input was consumed
     */
    bool HandleInput(float mouseX, float mouseY, bool clicked);

    // =========================================================================
    // Panel Management
    // =========================================================================

    /**
     * @brief Show a specific panel
     */
    void ShowPanel(EventPanelType panel);

    /**
     * @brief Hide a specific panel
     */
    void HidePanel(EventPanelType panel);

    /**
     * @brief Toggle panel visibility
     */
    void TogglePanel(EventPanelType panel);

    /**
     * @brief Check if panel is visible
     */
    [[nodiscard]] bool IsPanelVisible(EventPanelType panel) const;

    /**
     * @brief Close all panels
     */
    void CloseAllPanels();

    /**
     * @brief Get currently open panel
     */
    [[nodiscard]] EventPanelType GetActivePanel() const { return m_activePanel; }

    // =========================================================================
    // Banner Management
    // =========================================================================

    /**
     * @brief Show event banner
     */
    void ShowEventBanner(const WorldEvent& event);

    /**
     * @brief Hide event banner
     */
    void HideBanner();

    /**
     * @brief Check if banner is showing
     */
    [[nodiscard]] bool IsBannerVisible() const { return m_showBanner; }

    /**
     * @brief Set banner auto-hide duration (0 = never auto-hide)
     */
    void SetBannerDuration(float seconds) { m_bannerDuration = seconds; }

    // =========================================================================
    // Event Details
    // =========================================================================

    /**
     * @brief Show details for a specific event
     */
    void ShowEventDetails(const std::string& eventId);

    /**
     * @brief Get currently selected event ID
     */
    [[nodiscard]] const std::string& GetSelectedEventId() const { return m_selectedEventId; }

    /**
     * @brief Join the selected event
     */
    void JoinSelectedEvent();

    /**
     * @brief Leave the selected event
     */
    void LeaveSelectedEvent();

    // =========================================================================
    // Notifications
    // =========================================================================

    /**
     * @brief Show notification toast
     */
    void ShowToast(const EventNotification& notification);

    /**
     * @brief Dismiss all toasts
     */
    void DismissAllToasts();

    /**
     * @brief Get visible toast count
     */
    [[nodiscard]] int32_t GetVisibleToastCount() const;

    // =========================================================================
    // Rewards
    // =========================================================================

    /**
     * @brief Show rewards panel
     */
    void ShowRewardsPanel();

    /**
     * @brief Claim reward for event
     */
    void ClaimReward(const std::string& eventId);

    /**
     * @brief Claim all unclaimed rewards
     */
    void ClaimAllRewards();

    /**
     * @brief Check for unclaimed rewards
     */
    [[nodiscard]] bool HasUnclaimedRewards() const;

    // =========================================================================
    // Leaderboard
    // =========================================================================

    /**
     * @brief Show leaderboard for event
     */
    void ShowLeaderboard(const std::string& eventId);

    /**
     * @brief Show global leaderboard for event type
     */
    void ShowGlobalLeaderboard(EventType type);

    // =========================================================================
    // Data Access
    // =========================================================================

    /**
     * @brief Get formatted event list for display
     */
    [[nodiscard]] std::vector<EventListItem> GetEventList() const;

    /**
     * @brief Get event detail data
     */
    [[nodiscard]] EventDetailData GetEventDetails(const std::string& eventId) const;

    /**
     * @brief Get event log entries
     */
    [[nodiscard]] std::vector<EventLogEntry> GetEventLog() const;

    /**
     * @brief Get unclaimed reward items
     */
    [[nodiscard]] std::vector<RewardDisplayItem> GetUnclaimedRewards() const;

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Format time remaining as string
     */
    [[nodiscard]] static std::string FormatTimeRemaining(int64_t milliseconds);

    /**
     * @brief Format distance as string
     */
    [[nodiscard]] static std::string FormatDistance(float units);

    /**
     * @brief Get color for event category
     */
    [[nodiscard]] uint32_t GetCategoryColor(EventCategory category) const;

    /**
     * @brief Get icon path for event type
     */
    [[nodiscard]] std::string GetEventIcon(EventType type) const;

    /**
     * @brief Get severity text
     */
    [[nodiscard]] static std::string GetSeverityText(EventSeverity severity);

private:
    // Rendering helpers
    void RenderBanner();
    void RenderEventListPanel();
    void RenderEventDetailsPanel();
    void RenderEventLogPanel();
    void RenderLeaderboardPanel();
    void RenderRewardsPanel();
    void RenderNotificationToasts();

    // Data preparation
    EventBannerData PrepareBannerData(const WorldEvent& event) const;
    EventListItem PrepareListItem(const WorldEvent& event) const;
    EventDetailData PrepareDetailData(const WorldEvent& event) const;
    EventLogEntry PrepareLogEntry(const EventParticipationRecord& record) const;
    RewardDisplayItem PrepareRewardItem(const std::string& eventId,
                                        const EventReward& reward) const;

    // Animation
    void UpdateAnimations(float deltaTime);
    float GetPulseValue() const;
    float GetBlinkValue() const;

    // Time utilities
    [[nodiscard]] int64_t GetCurrentTimeMs() const;

    // State
    bool m_initialized = false;
    std::string m_localPlayerId;
    glm::vec2 m_playerPosition{0.0f, 0.0f};

    // System references
    EventScheduler* m_scheduler = nullptr;
    EventNotificationManager* m_notifications = nullptr;
    EventParticipationManager* m_participation = nullptr;
    EventEffects* m_effects = nullptr;

    // UI callbacks and theme
    EventUICallbacks m_callbacks;
    EventUITheme m_theme;

    // Panel state
    EventPanelType m_activePanel = EventPanelType::None;
    std::set<EventPanelType> m_visiblePanels;
    std::string m_selectedEventId;

    // Banner state
    bool m_showBanner = false;
    EventBannerData m_bannerData;
    float m_bannerDuration = 10.0f;
    float m_bannerTimer = 0.0f;

    // Toast state
    struct ToastState {
        EventNotification notification;
        float displayTime = 0.0f;
        float alpha = 1.0f;
        bool isDismissing = false;
    };
    std::vector<ToastState> m_activeToasts;
    static constexpr int32_t MaxVisibleToasts = 5;

    // Animation state
    float m_animationTimer = 0.0f;
    float m_pulseTimer = 0.0f;
    float m_blinkTimer = 0.0f;

    // Scroll state for lists
    float m_eventListScroll = 0.0f;
    float m_logScroll = 0.0f;
    float m_leaderboardScroll = 0.0f;

    // Filter state
    bool m_showThreats = true;
    bool m_showOpportunities = true;
    bool m_showEnvironmental = true;
    bool m_showSocial = true;
    bool m_showGlobal = true;
    bool m_showActiveOnly = false;
    bool m_showParticipatingOnly = false;

    // Leaderboard state
    std::string m_leaderboardEventId;
    EventType m_leaderboardGlobalType = EventType::ZombieHorde;
    bool m_showGlobalLeaderboard = false;

    // Cached data
    mutable std::vector<EventListItem> m_cachedEventList;
    mutable bool m_eventListDirty = true;
    mutable std::mutex m_cacheMutex;
};

} // namespace Vehement
