#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Season types affecting gameplay
 */
enum class Season {
    Spring = 0,     // Balanced production, moderate threats
    Summer,         // High food production, increased zombie activity
    Autumn,         // High resource gathering, preparing for winter
    Winter,         // Reduced production, harsh conditions
    Count
};

/**
 * @brief Get season name
 */
[[nodiscard]] const char* SeasonToString(Season season);

/**
 * @brief Time of day phases
 */
enum class TimeOfDay {
    Dawn = 0,       // 5:00 - 8:00
    Morning,        // 8:00 - 12:00
    Afternoon,      // 12:00 - 17:00
    Dusk,           // 17:00 - 20:00
    Night,          // 20:00 - 24:00
    Midnight,       // 0:00 - 5:00
    Count
};

/**
 * @brief Get time of day name
 */
[[nodiscard]] const char* TimeOfDayToString(TimeOfDay tod);

/**
 * @brief Scheduled world event
 */
struct ScheduledEvent {
    std::string id;
    std::string name;
    std::string description;
    int64_t triggerTimestamp;       // When to trigger (server time)
    bool recurring = false;
    int64_t recurInterval = 0;      // Seconds between recurrences
    bool triggered = false;
    nlohmann::json data;            // Event-specific data

    [[nodiscard]] nlohmann::json ToJson() const;
    static ScheduledEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief World clock configuration
 */
struct WorldClockConfig {
    // Time scale
    float dayLengthMinutes = 60.0f;     // Real minutes per game day (1 hour = 1 day)
    float yearLengthDays = 120.0f;      // Game days per game year

    // Season durations (in game days)
    float springDays = 30.0f;
    float summerDays = 30.0f;
    float autumnDays = 30.0f;
    float winterDays = 30.0f;

    // Sync settings
    float syncIntervalSeconds = 60.0f;  // How often to sync with server

    // Gameplay modifiers by season
    struct SeasonModifiers {
        float foodProduction = 1.0f;
        float woodProduction = 1.0f;
        float stoneProduction = 1.0f;
        float threatLevel = 1.0f;
        float travelSpeed = 1.0f;
        float healingRate = 1.0f;
    };

    SeasonModifiers spring{1.2f, 1.1f, 1.0f, 0.8f, 1.0f, 1.1f};
    SeasonModifiers summer{1.5f, 1.0f, 1.0f, 1.2f, 1.1f, 1.0f};
    SeasonModifiers autumn{1.0f, 1.2f, 1.2f, 1.0f, 1.0f, 1.0f};
    SeasonModifiers winter{0.5f, 0.7f, 0.8f, 1.5f, 0.7f, 0.8f};

    // Day/night modifiers
    float nightThreatMultiplier = 2.0f;
    float nightVisionRange = 0.5f;      // Vision reduced at night
    float dawnDuskDuration = 0.1f;      // Percentage of day for transitions
};

/**
 * @brief World time state
 */
struct WorldTime {
    int year = 1;
    int day = 1;                        // Day of year (1-365)
    float hour = 12.0f;                 // Hour of day (0.0-24.0)
    Season season = Season::Spring;
    TimeOfDay timeOfDay = TimeOfDay::Afternoon;

    int64_t serverTimestamp = 0;        // Server timestamp when synced
    int64_t localTimestamp = 0;         // Local timestamp when synced

    [[nodiscard]] nlohmann::json ToJson() const;
    static WorldTime FromJson(const nlohmann::json& j);

    /**
     * @brief Get formatted time string
     */
    [[nodiscard]] std::string GetTimeString() const;

    /**
     * @brief Get formatted date string
     */
    [[nodiscard]] std::string GetDateString() const;

    /**
     * @brief Get season day (day within current season)
     */
    [[nodiscard]] int GetSeasonDay() const;

    /**
     * @brief Check if it's night time
     */
    [[nodiscard]] bool IsNight() const;

    /**
     * @brief Check if it's daytime
     */
    [[nodiscard]] bool IsDay() const;
};

/**
 * @brief Global world clock system
 *
 * Features:
 * - Synchronized across all players via server timestamp
 * - Day/night cycle affects gameplay (visibility, threats)
 * - Seasons affect resource production
 * - Scheduled events on world clock
 * - Used for offline time calculations
 */
class WorldClock {
public:
    /**
     * @brief Callback types
     */
    using TimeChangedCallback = std::function<void(const WorldTime& time)>;
    using SeasonChangedCallback = std::function<void(Season oldSeason, Season newSeason)>;
    using TimeOfDayChangedCallback = std::function<void(TimeOfDay oldTod, TimeOfDay newTod)>;
    using EventTriggeredCallback = std::function<void(const ScheduledEvent& event)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static WorldClock& Instance();

    // Non-copyable
    WorldClock(const WorldClock&) = delete;
    WorldClock& operator=(const WorldClock&) = delete;

    /**
     * @brief Initialize world clock
     * @param config Clock configuration
     * @return true if successful
     */
    [[nodiscard]] bool Initialize(const WorldClockConfig& config = {});

    /**
     * @brief Shutdown world clock
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update world clock (call from game loop)
     * @param deltaTime Real seconds since last frame
     */
    void Update(float deltaTime);

    // ==================== Time Queries ====================

    /**
     * @brief Get current world time
     */
    [[nodiscard]] WorldTime GetTime() const;

    /**
     * @brief Get current season
     */
    [[nodiscard]] Season GetSeason() const;

    /**
     * @brief Get current time of day
     */
    [[nodiscard]] TimeOfDay GetTimeOfDay() const;

    /**
     * @brief Get current hour (0.0 - 24.0)
     */
    [[nodiscard]] float GetHour() const;

    /**
     * @brief Get current day of year
     */
    [[nodiscard]] int GetDay() const;

    /**
     * @brief Get current year
     */
    [[nodiscard]] int GetYear() const;

    /**
     * @brief Check if currently night
     */
    [[nodiscard]] bool IsNight() const;

    /**
     * @brief Check if currently day
     */
    [[nodiscard]] bool IsDay() const;

    /**
     * @brief Get day/night blend factor (0 = full night, 1 = full day)
     */
    [[nodiscard]] float GetDayNightBlend() const;

    /**
     * @brief Get server timestamp
     */
    [[nodiscard]] int64_t GetServerTimestamp() const;

    /**
     * @brief Convert server timestamp to world time
     */
    [[nodiscard]] WorldTime TimestampToWorldTime(int64_t timestamp) const;

    /**
     * @brief Convert world time to server timestamp
     */
    [[nodiscard]] int64_t WorldTimeToTimestamp(const WorldTime& time) const;

    // ==================== Modifiers ====================

    /**
     * @brief Get current season modifiers
     */
    [[nodiscard]] const WorldClockConfig::SeasonModifiers& GetSeasonModifiers() const;

    /**
     * @brief Get food production modifier
     */
    [[nodiscard]] float GetFoodProductionModifier() const;

    /**
     * @brief Get wood production modifier
     */
    [[nodiscard]] float GetWoodProductionModifier() const;

    /**
     * @brief Get threat level modifier
     */
    [[nodiscard]] float GetThreatModifier() const;

    /**
     * @brief Get vision range modifier
     */
    [[nodiscard]] float GetVisionModifier() const;

    /**
     * @brief Get travel speed modifier
     */
    [[nodiscard]] float GetTravelSpeedModifier() const;

    // ==================== Scheduled Events ====================

    /**
     * @brief Schedule an event
     * @param event Event to schedule
     * @return Event ID
     */
    std::string ScheduleEvent(const ScheduledEvent& event);

    /**
     * @brief Schedule an event at specific world time
     * @param name Event name
     * @param time World time to trigger
     * @param data Event data
     * @return Event ID
     */
    std::string ScheduleEventAt(const std::string& name, const WorldTime& time,
                                const nlohmann::json& data = {});

    /**
     * @brief Schedule a recurring event
     * @param name Event name
     * @param intervalSeconds Seconds between occurrences
     * @param data Event data
     * @return Event ID
     */
    std::string ScheduleRecurringEvent(const std::string& name, int64_t intervalSeconds,
                                       const nlohmann::json& data = {});

    /**
     * @brief Cancel a scheduled event
     * @param eventId Event to cancel
     */
    void CancelEvent(const std::string& eventId);

    /**
     * @brief Get all scheduled events
     */
    [[nodiscard]] std::vector<ScheduledEvent> GetScheduledEvents() const;

    /**
     * @brief Get events scheduled for today
     */
    [[nodiscard]] std::vector<ScheduledEvent> GetTodaysEvents() const;

    // ==================== Synchronization ====================

    /**
     * @brief Sync time with server
     */
    void SyncWithServer();

    /**
     * @brief Set time from server response
     * @param serverTime Server world time
     * @param serverTimestamp Server Unix timestamp
     */
    void SetServerTime(const WorldTime& serverTime, int64_t serverTimestamp);

    /**
     * @brief Get time offset from server (for latency compensation)
     */
    [[nodiscard]] int64_t GetServerTimeOffset() const { return m_serverTimeOffset; }

    // ==================== Callbacks ====================

    /**
     * @brief Register callback for time changes (every game hour)
     */
    void OnTimeChanged(TimeChangedCallback callback);

    /**
     * @brief Register callback for season changes
     */
    void OnSeasonChanged(SeasonChangedCallback callback);

    /**
     * @brief Register callback for time of day changes
     */
    void OnTimeOfDayChanged(TimeOfDayChangedCallback callback);

    /**
     * @brief Register callback for triggered events
     */
    void OnEventTriggered(EventTriggeredCallback callback);

    // ==================== Configuration ====================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const WorldClockConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set time scale
     * @param dayLengthMinutes Real minutes per game day
     */
    void SetTimeScale(float dayLengthMinutes);

    /**
     * @brief Pause world clock
     */
    void Pause() { m_paused = true; }

    /**
     * @brief Resume world clock
     */
    void Resume() { m_paused = false; }

    /**
     * @brief Check if paused
     */
    [[nodiscard]] bool IsPaused() const { return m_paused; }

private:
    WorldClock() = default;
    ~WorldClock() = default;

    // Internal helpers
    void UpdateTimeOfDay();
    void UpdateSeason();
    void ProcessEvents();
    Season CalculateSeason(int dayOfYear) const;
    TimeOfDay CalculateTimeOfDay(float hour) const;
    std::string GenerateEventId();

    bool m_initialized = false;
    bool m_paused = false;
    WorldClockConfig m_config;

    // Current time state
    WorldTime m_time;
    mutable std::mutex m_timeMutex;

    // Server sync
    int64_t m_serverTimeOffset = 0;     // Difference between server and local time
    float m_syncTimer = 0.0f;

    // Previous state for change detection
    float m_lastHour = 0.0f;
    Season m_lastSeason = Season::Spring;
    TimeOfDay m_lastTimeOfDay = TimeOfDay::Afternoon;

    // Scheduled events
    std::vector<ScheduledEvent> m_events;
    std::mutex m_eventsMutex;
    int m_nextEventId = 1;

    // Callbacks
    std::vector<TimeChangedCallback> m_timeCallbacks;
    std::vector<SeasonChangedCallback> m_seasonCallbacks;
    std::vector<TimeOfDayChangedCallback> m_todCallbacks;
    std::vector<EventTriggeredCallback> m_eventCallbacks;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
