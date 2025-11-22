#pragma once

#include "WorldEvent.hpp"
#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <random>

namespace Vehement {

// Forward declaration
class FirebaseManager;

/**
 * @brief Configuration for how often an event type can occur
 */
struct EventConfig {
    EventType type;
    float probabilityPerHour;       ///< Base chance per hour (0.0 - 1.0)
    float minDurationMinutes;       ///< Minimum event duration
    float maxDurationMinutes;       ///< Maximum event duration
    float minCooldownMinutes;       ///< Minimum time between same event type
    float warningLeadTimeMinutes;   ///< How long before event to warn players
    bool isGlobal;                  ///< Affects entire server vs local area
    bool requiresMinPlayers;        ///< Requires minimum players online
    int32_t minPlayerCount;         ///< Minimum players needed
    int32_t maxSimultaneous;        ///< Max of this type at once
    float minRadius;                ///< Minimum effect radius
    float maxRadius;                ///< Maximum effect radius
    EventSeverity severity;         ///< Event severity level
    bool enabledByDefault;          ///< Whether enabled by default

    EventConfig()
        : type(EventType::SupplyDrop)
        , probabilityPerHour(0.5f)
        , minDurationMinutes(5.0f)
        , maxDurationMinutes(30.0f)
        , minCooldownMinutes(60.0f)
        , warningLeadTimeMinutes(2.0f)
        , isGlobal(false)
        , requiresMinPlayers(false)
        , minPlayerCount(1)
        , maxSimultaneous(3)
        , minRadius(50.0f)
        , maxRadius(200.0f)
        , severity(EventSeverity::Moderate)
        , enabledByDefault(true) {}
};

/**
 * @brief Statistics about event scheduling
 */
struct SchedulerStats {
    int64_t totalEventsScheduled = 0;
    int64_t totalEventsCompleted = 0;
    int64_t totalEventsCancelled = 0;
    int64_t totalEventsFailed = 0;

    std::map<EventType, int32_t> eventsPerType;
    std::map<EventCategory, int32_t> eventsPerCategory;

    int64_t lastScheduleTime = 0;
    int64_t lastEventStartTime = 0;
};

/**
 * @brief Server-side event scheduler
 *
 * Responsible for:
 * - Generating random events based on configuration
 * - Scheduling events in Firebase for synchronization
 * - Balancing event frequency and types
 * - Managing event cooldowns
 * - Handling event cancellation
 *
 * This should only run on the authoritative server/host.
 */
class EventScheduler {
public:
    /**
     * @brief Construct the event scheduler
     */
    EventScheduler();

    /**
     * @brief Destructor
     */
    ~EventScheduler();

    // Non-copyable, non-movable
    EventScheduler(const EventScheduler&) = delete;
    EventScheduler& operator=(const EventScheduler&) = delete;
    EventScheduler(EventScheduler&&) = delete;
    EventScheduler& operator=(EventScheduler&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the scheduler
     * @param isHost Whether this client is the authoritative host
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(bool isHost = false);

    /**
     * @brief Shutdown the scheduler
     */
    void Shutdown();

    /**
     * @brief Set whether this instance is the authoritative host
     */
    void SetIsHost(bool isHost) { m_isHost = isHost; }

    /**
     * @brief Check if this instance is the authoritative host
     */
    [[nodiscard]] bool IsHost() const noexcept { return m_isHost; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update the scheduler (call each frame)
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Process pending event start/end times
     */
    void ProcessScheduledEvents();

    // =========================================================================
    // Event Configuration
    // =========================================================================

    /**
     * @brief Load event configurations from JSON
     */
    [[nodiscard]] bool LoadConfiguration(const nlohmann::json& config);

    /**
     * @brief Load event configurations from file
     */
    [[nodiscard]] bool LoadConfigurationFromFile(const std::string& path);

    /**
     * @brief Get configuration for an event type
     */
    [[nodiscard]] const EventConfig* GetEventConfig(EventType type) const;

    /**
     * @brief Set configuration for an event type
     */
    void SetEventConfig(const EventConfig& config);

    /**
     * @brief Enable or disable an event type
     */
    void SetEventEnabled(EventType type, bool enabled);

    /**
     * @brief Check if an event type is enabled
     */
    [[nodiscard]] bool IsEventEnabled(EventType type) const;

    /**
     * @brief Get all event configurations
     */
    [[nodiscard]] const std::map<EventType, EventConfig>& GetAllConfigs() const {
        return m_eventConfigs;
    }

    // =========================================================================
    // Event Generation
    // =========================================================================

    /**
     * @brief Generate a random event based on current conditions
     * @return Generated event or nullopt if none should be generated
     */
    [[nodiscard]] std::optional<WorldEvent> GenerateRandomEvent();

    /**
     * @brief Generate an event of a specific type
     * @param type The event type to generate
     * @param location Event center location
     * @return Generated event
     */
    [[nodiscard]] WorldEvent GenerateEvent(EventType type, const glm::vec2& location);

    /**
     * @brief Generate an event from a template
     */
    [[nodiscard]] WorldEvent GenerateFromTemplate(const EventTemplate& tmpl,
                                                   const glm::vec2& location);

    /**
     * @brief Select a random location for an event
     */
    [[nodiscard]] glm::vec2 SelectRandomLocation() const;

    /**
     * @brief Select location near player bases for threat events
     */
    [[nodiscard]] glm::vec2 SelectThreatLocation() const;

    /**
     * @brief Select location away from players for opportunity events
     */
    [[nodiscard]] glm::vec2 SelectOpportunityLocation() const;

    // =========================================================================
    // Event Scheduling
    // =========================================================================

    /**
     * @brief Schedule an event to occur
     * @param event The event to schedule
     * @param callback Called when scheduling completes
     */
    void ScheduleEvent(const WorldEvent& event,
                       std::function<void(bool success)> callback = nullptr);

    /**
     * @brief Schedule an event to start at a specific time
     */
    void ScheduleEventAt(const WorldEvent& event, int64_t startTimeMs);

    /**
     * @brief Schedule an event to start after a delay
     */
    void ScheduleEventAfter(const WorldEvent& event, float delaySeconds);

    /**
     * @brief Cancel a scheduled event
     * @param eventId ID of the event to cancel
     * @return true if event was found and cancelled
     */
    bool CancelEvent(const std::string& eventId);

    /**
     * @brief Cancel all events of a specific type
     */
    int32_t CancelEventsByType(EventType type);

    /**
     * @brief Cancel all scheduled events
     */
    void CancelAllEvents();

    /**
     * @brief Extend an event's duration
     */
    bool ExtendEvent(const std::string& eventId, float additionalSeconds);

    // =========================================================================
    // Event Queries
    // =========================================================================

    /**
     * @brief Get all currently active events
     */
    [[nodiscard]] std::vector<WorldEvent> GetActiveEvents() const;

    /**
     * @brief Get all scheduled (upcoming) events
     */
    [[nodiscard]] std::vector<WorldEvent> GetScheduledEvents() const;

    /**
     * @brief Get all completed events (recent history)
     */
    [[nodiscard]] std::vector<WorldEvent> GetCompletedEvents() const;

    /**
     * @brief Get event by ID
     */
    [[nodiscard]] std::optional<WorldEvent> GetEvent(const std::string& eventId) const;

    /**
     * @brief Get events affecting a specific position
     */
    [[nodiscard]] std::vector<WorldEvent> GetEventsAtPosition(const glm::vec2& pos) const;

    /**
     * @brief Get events affecting a specific player
     */
    [[nodiscard]] std::vector<WorldEvent> GetEventsForPlayer(const std::string& playerId) const;

    /**
     * @brief Get count of active events by type
     */
    [[nodiscard]] int32_t GetActiveEventCount(EventType type) const;

    /**
     * @brief Get total count of active events
     */
    [[nodiscard]] int32_t GetTotalActiveEventCount() const;

    // =========================================================================
    // Balancing
    // =========================================================================

    /**
     * @brief Balance upcoming events
     *
     * Ensures:
     * - Not too many threats at once
     * - Mix of event categories
     * - Appropriate difficulty curve
     */
    void BalanceUpcomingEvents();

    /**
     * @brief Check if a new event should be generated
     */
    [[nodiscard]] bool ShouldGenerateEvent() const;

    /**
     * @brief Get the next event type to generate based on balance
     */
    [[nodiscard]] std::optional<EventType> SelectNextEventType() const;

    /**
     * @brief Check if event type is on cooldown
     */
    [[nodiscard]] bool IsEventOnCooldown(EventType type) const;

    /**
     * @brief Get remaining cooldown time for event type
     */
    [[nodiscard]] float GetRemainingCooldown(EventType type) const;

    // =========================================================================
    // Firebase Synchronization
    // =========================================================================

    /**
     * @brief Sync events with Firebase
     */
    void SyncWithFirebase();

    /**
     * @brief Handle event data received from Firebase
     */
    void OnFirebaseEventUpdate(const nlohmann::json& data);

    /**
     * @brief Set Firebase path for events
     */
    void SetFirebasePath(const std::string& path) { m_firebasePath = path; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Register callback for when events are scheduled
     */
    void OnEventScheduled(EventCallback callback);

    /**
     * @brief Register callback for when events start
     */
    void OnEventStarted(EventCallback callback);

    /**
     * @brief Register callback for when events end
     */
    void OnEventEnded(EventCallback callback);

    /**
     * @brief Register callback for when events are cancelled
     */
    void OnEventCancelled(EventCallback callback);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get scheduler statistics
     */
    [[nodiscard]] const SchedulerStats& GetStats() const { return m_stats; }

    /**
     * @brief Reset statistics
     */
    void ResetStats();

    // =========================================================================
    // World State
    // =========================================================================

    /**
     * @brief Set world bounds for event location generation
     */
    void SetWorldBounds(const glm::vec2& min, const glm::vec2& max);

    /**
     * @brief Update current player count
     */
    void SetPlayerCount(int32_t count) { m_currentPlayerCount = count; }

    /**
     * @brief Add player position for location-based events
     */
    void UpdatePlayerPosition(const std::string& playerId, const glm::vec2& pos);

    /**
     * @brief Remove player from tracking
     */
    void RemovePlayer(const std::string& playerId);

    /**
     * @brief Get current game time (for day/night events)
     */
    void SetGameTime(float timeOfDay) { m_gameTimeOfDay = timeOfDay; }

private:
    // Initialization
    void InitializeDefaultConfigs();

    // Event generation helpers
    WorldEvent GenerateEventInternal(const EventConfig& config, const glm::vec2& location);
    void GenerateEventName(WorldEvent& event);
    void GenerateEventDescription(WorldEvent& event);
    void GenerateEventRewards(WorldEvent& event);
    void ScaleEventForPlayers(WorldEvent& event, int32_t playerCount);

    // Cooldown management
    void UpdateCooldowns(float deltaTime);
    void SetCooldown(EventType type);

    // Firebase helpers
    void PublishEventToFirebase(const WorldEvent& event);
    void RemoveEventFromFirebase(const std::string& eventId);
    void UpdateEventInFirebase(const WorldEvent& event);

    // Callback invocation
    void InvokeScheduledCallbacks(const WorldEvent& event);
    void InvokeStartedCallbacks(const WorldEvent& event);
    void InvokeEndedCallbacks(const WorldEvent& event);
    void InvokeCancelledCallbacks(const WorldEvent& event);

    // Time utilities
    [[nodiscard]] int64_t GetCurrentTimeMs() const;

    // State
    bool m_initialized = false;
    bool m_isHost = false;

    // Event storage
    std::map<std::string, WorldEvent> m_activeEvents;
    std::map<std::string, WorldEvent> m_scheduledEvents;
    std::vector<WorldEvent> m_completedEvents;     // Recent history
    mutable std::mutex m_eventMutex;

    // Configuration
    std::map<EventType, EventConfig> m_eventConfigs;
    std::map<EventType, bool> m_enabledEvents;
    std::map<EventType, float> m_cooldowns;         // Remaining cooldown time

    // World state
    glm::vec2 m_worldMin{0.0f, 0.0f};
    glm::vec2 m_worldMax{10000.0f, 10000.0f};
    int32_t m_currentPlayerCount = 1;
    std::map<std::string, glm::vec2> m_playerPositions;
    float m_gameTimeOfDay = 0.5f;   // 0.0 = midnight, 0.5 = noon

    // Timing
    float m_timeSinceLastSchedule = 0.0f;
    float m_scheduleCheckInterval = 60.0f;  // Check every minute
    float m_balanceCheckInterval = 300.0f;  // Balance every 5 minutes
    float m_timeSinceLastBalance = 0.0f;

    // Random generation
    mutable std::mt19937 m_rng;

    // Callbacks
    std::vector<EventCallback> m_scheduledCallbacks;
    std::vector<EventCallback> m_startedCallbacks;
    std::vector<EventCallback> m_endedCallbacks;
    std::vector<EventCallback> m_cancelledCallbacks;
    std::mutex m_callbackMutex;

    // Firebase
    std::string m_firebasePath = "events";
    std::string m_firebaseListenerId;

    // Statistics
    SchedulerStats m_stats;

    // Limits
    static constexpr int32_t MaxActiveEvents = 10;
    static constexpr int32_t MaxThreatEvents = 3;
    static constexpr int32_t MaxCompletedHistory = 50;
    static constexpr float MinTimeBetweenEvents = 30.0f; // seconds
};

/**
 * @brief Builder for creating EventConfig instances
 */
class EventConfigBuilder {
public:
    EventConfigBuilder(EventType type) {
        m_config.type = type;
    }

    EventConfigBuilder& Probability(float perHour) {
        m_config.probabilityPerHour = perHour;
        return *this;
    }

    EventConfigBuilder& Duration(float minMinutes, float maxMinutes) {
        m_config.minDurationMinutes = minMinutes;
        m_config.maxDurationMinutes = maxMinutes;
        return *this;
    }

    EventConfigBuilder& Cooldown(float minutes) {
        m_config.minCooldownMinutes = minutes;
        return *this;
    }

    EventConfigBuilder& Warning(float minutes) {
        m_config.warningLeadTimeMinutes = minutes;
        return *this;
    }

    EventConfigBuilder& Global(bool isGlobal = true) {
        m_config.isGlobal = isGlobal;
        return *this;
    }

    EventConfigBuilder& MinPlayers(int32_t count) {
        m_config.requiresMinPlayers = true;
        m_config.minPlayerCount = count;
        return *this;
    }

    EventConfigBuilder& MaxSimultaneous(int32_t count) {
        m_config.maxSimultaneous = count;
        return *this;
    }

    EventConfigBuilder& Radius(float minR, float maxR) {
        m_config.minRadius = minR;
        m_config.maxRadius = maxR;
        return *this;
    }

    EventConfigBuilder& Severity(EventSeverity sev) {
        m_config.severity = sev;
        return *this;
    }

    EventConfigBuilder& Enabled(bool enabled = true) {
        m_config.enabledByDefault = enabled;
        return *this;
    }

    [[nodiscard]] EventConfig Build() const { return m_config; }

private:
    EventConfig m_config;
};

} // namespace Vehement
