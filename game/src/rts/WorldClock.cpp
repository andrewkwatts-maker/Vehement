#include "WorldClock.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// Logging macros
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define CLOCK_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define CLOCK_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#else
#include <iostream>
#define CLOCK_LOG_INFO(...) std::cout << "[WorldClock] " << __VA_ARGS__ << std::endl
#define CLOCK_LOG_WARN(...) std::cerr << "[WorldClock WARN] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ============================================================================
// Helper Functions
// ============================================================================

const char* SeasonToString(Season season) {
    switch (season) {
        case Season::Spring: return "Spring";
        case Season::Summer: return "Summer";
        case Season::Autumn: return "Autumn";
        case Season::Winter: return "Winter";
        default: return "Unknown";
    }
}

const char* TimeOfDayToString(TimeOfDay tod) {
    switch (tod) {
        case TimeOfDay::Dawn: return "Dawn";
        case TimeOfDay::Morning: return "Morning";
        case TimeOfDay::Afternoon: return "Afternoon";
        case TimeOfDay::Dusk: return "Dusk";
        case TimeOfDay::Night: return "Night";
        case TimeOfDay::Midnight: return "Midnight";
        default: return "Unknown";
    }
}

// ============================================================================
// ScheduledEvent Implementation
// ============================================================================

nlohmann::json ScheduledEvent::ToJson() const {
    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"triggerTimestamp", triggerTimestamp},
        {"recurring", recurring},
        {"recurInterval", recurInterval},
        {"triggered", triggered},
        {"data", data}
    };
}

ScheduledEvent ScheduledEvent::FromJson(const nlohmann::json& j) {
    ScheduledEvent e;
    e.id = j.value("id", "");
    e.name = j.value("name", "");
    e.description = j.value("description", "");
    e.triggerTimestamp = j.value("triggerTimestamp", 0LL);
    e.recurring = j.value("recurring", false);
    e.recurInterval = j.value("recurInterval", 0LL);
    e.triggered = j.value("triggered", false);
    if (j.contains("data")) {
        e.data = j["data"];
    }
    return e;
}

// ============================================================================
// WorldTime Implementation
// ============================================================================

nlohmann::json WorldTime::ToJson() const {
    return {
        {"year", year},
        {"day", day},
        {"hour", hour},
        {"season", static_cast<int>(season)},
        {"timeOfDay", static_cast<int>(timeOfDay)},
        {"serverTimestamp", serverTimestamp},
        {"localTimestamp", localTimestamp}
    };
}

WorldTime WorldTime::FromJson(const nlohmann::json& j) {
    WorldTime t;
    t.year = j.value("year", 1);
    t.day = j.value("day", 1);
    t.hour = j.value("hour", 12.0f);
    t.season = static_cast<Season>(j.value("season", 0));
    t.timeOfDay = static_cast<TimeOfDay>(j.value("timeOfDay", 2));
    t.serverTimestamp = j.value("serverTimestamp", 0LL);
    t.localTimestamp = j.value("localTimestamp", 0LL);
    return t;
}

std::string WorldTime::GetTimeString() const {
    int h = static_cast<int>(hour);
    int m = static_cast<int>((hour - h) * 60.0f);

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << h << ":"
       << std::setfill('0') << std::setw(2) << m;
    return ss.str();
}

std::string WorldTime::GetDateString() const {
    std::stringstream ss;
    ss << SeasonToString(season) << " Day " << GetSeasonDay() << ", Year " << year;
    return ss.str();
}

int WorldTime::GetSeasonDay() const {
    int seasonDays = 30;  // Default
    int seasonStart = 0;

    switch (season) {
        case Season::Spring:
            seasonStart = 1;
            break;
        case Season::Summer:
            seasonStart = 31;
            break;
        case Season::Autumn:
            seasonStart = 61;
            break;
        case Season::Winter:
            seasonStart = 91;
            break;
        default:
            break;
    }

    return day - seasonStart + 1;
}

bool WorldTime::IsNight() const {
    return timeOfDay == TimeOfDay::Night || timeOfDay == TimeOfDay::Midnight;
}

bool WorldTime::IsDay() const {
    return !IsNight();
}

// ============================================================================
// WorldClock Implementation
// ============================================================================

WorldClock& WorldClock::Instance() {
    static WorldClock instance;
    return instance;
}

bool WorldClock::Initialize(const WorldClockConfig& config) {
    if (m_initialized) {
        CLOCK_LOG_WARN("WorldClock already initialized");
        return true;
    }

    m_config = config;

    // Initialize time to start of Year 1, Spring, Noon
    m_time.year = 1;
    m_time.day = 1;
    m_time.hour = 12.0f;
    m_time.season = Season::Spring;
    m_time.timeOfDay = TimeOfDay::Afternoon;

    // Get initial timestamp
    auto now = std::chrono::system_clock::now();
    m_time.localTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    m_time.serverTimestamp = m_time.localTimestamp;

    m_lastHour = m_time.hour;
    m_lastSeason = m_time.season;
    m_lastTimeOfDay = m_time.timeOfDay;

    m_initialized = true;
    CLOCK_LOG_INFO("WorldClock initialized - " + m_time.GetTimeString() + " " + m_time.GetDateString());
    return true;
}

void WorldClock::Shutdown() {
    if (!m_initialized) return;

    {
        std::lock_guard<std::mutex> lock(m_eventsMutex);
        m_events.clear();
    }

    m_initialized = false;
    CLOCK_LOG_INFO("WorldClock shutdown complete");
}

void WorldClock::Update(float deltaTime) {
    if (!m_initialized || m_paused) return;

    // Convert real time to game time
    // dayLengthMinutes real minutes = 24 game hours
    // So: 1 real second = (24 * 60) / (dayLengthMinutes * 60) game seconds
    //                   = 24 / dayLengthMinutes game hours per minute
    //                   = (24 / dayLengthMinutes) / 60 game hours per second
    float gameHoursPerRealSecond = 24.0f / (m_config.dayLengthMinutes * 60.0f);
    float hoursElapsed = deltaTime * gameHoursPerRealSecond;

    {
        std::lock_guard<std::mutex> lock(m_timeMutex);

        m_time.hour += hoursElapsed;

        // Wrap hours to next day
        while (m_time.hour >= 24.0f) {
            m_time.hour -= 24.0f;
            m_time.day++;

            // Wrap days to next year
            if (m_time.day > static_cast<int>(m_config.yearLengthDays)) {
                m_time.day = 1;
                m_time.year++;
                CLOCK_LOG_INFO("New Year " + std::to_string(m_time.year) + " has begun!");
            }
        }
    }

    // Update time of day
    UpdateTimeOfDay();

    // Update season
    UpdateSeason();

    // Process scheduled events
    ProcessEvents();

    // Check for hourly callback
    int currentHour = static_cast<int>(m_time.hour);
    int lastHour = static_cast<int>(m_lastHour);
    if (currentHour != lastHour) {
        m_lastHour = m_time.hour;

        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (auto& callback : m_timeCallbacks) {
            if (callback) {
                callback(m_time);
            }
        }
    }

    // Server sync timer
    m_syncTimer += deltaTime;
    if (m_syncTimer >= m_config.syncIntervalSeconds) {
        m_syncTimer = 0.0f;
        // Could trigger server sync here
    }
}

// ==================== Time Queries ====================

WorldTime WorldClock::GetTime() const {
    std::lock_guard<std::mutex> lock(m_timeMutex);
    return m_time;
}

Season WorldClock::GetSeason() const {
    std::lock_guard<std::mutex> lock(m_timeMutex);
    return m_time.season;
}

TimeOfDay WorldClock::GetTimeOfDay() const {
    std::lock_guard<std::mutex> lock(m_timeMutex);
    return m_time.timeOfDay;
}

float WorldClock::GetHour() const {
    std::lock_guard<std::mutex> lock(m_timeMutex);
    return m_time.hour;
}

int WorldClock::GetDay() const {
    std::lock_guard<std::mutex> lock(m_timeMutex);
    return m_time.day;
}

int WorldClock::GetYear() const {
    std::lock_guard<std::mutex> lock(m_timeMutex);
    return m_time.year;
}

bool WorldClock::IsNight() const {
    return GetTime().IsNight();
}

bool WorldClock::IsDay() const {
    return GetTime().IsDay();
}

float WorldClock::GetDayNightBlend() const {
    float hour = GetHour();

    // Full night: 22:00 - 5:00
    // Dawn transition: 5:00 - 7:00
    // Full day: 7:00 - 19:00
    // Dusk transition: 19:00 - 22:00

    if (hour >= 7.0f && hour < 19.0f) {
        return 1.0f;  // Full day
    } else if (hour >= 22.0f || hour < 5.0f) {
        return 0.0f;  // Full night
    } else if (hour >= 5.0f && hour < 7.0f) {
        // Dawn
        return (hour - 5.0f) / 2.0f;
    } else {
        // Dusk (19:00 - 22:00)
        return 1.0f - (hour - 19.0f) / 3.0f;
    }
}

int64_t WorldClock::GetServerTimestamp() const {
    auto now = std::chrono::system_clock::now();
    int64_t localTime = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    return localTime + m_serverTimeOffset;
}

WorldTime WorldClock::TimestampToWorldTime(int64_t timestamp) const {
    // Calculate time difference from reference point
    int64_t referenceTimestamp = m_time.serverTimestamp;
    int64_t diffSeconds = timestamp - referenceTimestamp;

    // Convert to game time
    float gameHoursPerRealSecond = 24.0f / (m_config.dayLengthMinutes * 60.0f);
    float hoursDiff = static_cast<float>(diffSeconds) * gameHoursPerRealSecond;

    WorldTime result = m_time;
    result.hour += hoursDiff;

    // Normalize
    while (result.hour >= 24.0f) {
        result.hour -= 24.0f;
        result.day++;
    }
    while (result.hour < 0.0f) {
        result.hour += 24.0f;
        result.day--;
    }
    while (result.day > static_cast<int>(m_config.yearLengthDays)) {
        result.day -= static_cast<int>(m_config.yearLengthDays);
        result.year++;
    }
    while (result.day < 1) {
        result.day += static_cast<int>(m_config.yearLengthDays);
        result.year--;
    }

    result.season = CalculateSeason(result.day);
    result.timeOfDay = CalculateTimeOfDay(result.hour);
    result.serverTimestamp = timestamp;

    return result;
}

int64_t WorldClock::WorldTimeToTimestamp(const WorldTime& time) const {
    // Calculate game time difference
    float hoursDiff = 0.0f;

    // Days difference
    int daysDiff = (time.year - m_time.year) * static_cast<int>(m_config.yearLengthDays);
    daysDiff += (time.day - m_time.day);
    hoursDiff += daysDiff * 24.0f;

    // Hours difference
    hoursDiff += (time.hour - m_time.hour);

    // Convert to real seconds
    float gameHoursPerRealSecond = 24.0f / (m_config.dayLengthMinutes * 60.0f);
    int64_t secondsDiff = static_cast<int64_t>(hoursDiff / gameHoursPerRealSecond);

    return m_time.serverTimestamp + secondsDiff;
}

// ==================== Modifiers ====================

const WorldClockConfig::SeasonModifiers& WorldClock::GetSeasonModifiers() const {
    switch (GetSeason()) {
        case Season::Spring: return m_config.spring;
        case Season::Summer: return m_config.summer;
        case Season::Autumn: return m_config.autumn;
        case Season::Winter: return m_config.winter;
        default: return m_config.spring;
    }
}

float WorldClock::GetFoodProductionModifier() const {
    return GetSeasonModifiers().foodProduction;
}

float WorldClock::GetWoodProductionModifier() const {
    return GetSeasonModifiers().woodProduction;
}

float WorldClock::GetThreatModifier() const {
    float base = GetSeasonModifiers().threatLevel;
    if (IsNight()) {
        base *= m_config.nightThreatMultiplier;
    }
    return base;
}

float WorldClock::GetVisionModifier() const {
    float blend = GetDayNightBlend();
    return m_config.nightVisionRange + (1.0f - m_config.nightVisionRange) * blend;
}

float WorldClock::GetTravelSpeedModifier() const {
    float base = GetSeasonModifiers().travelSpeed;
    // Slightly slower at night
    if (IsNight()) {
        base *= 0.9f;
    }
    return base;
}

// ==================== Scheduled Events ====================

std::string WorldClock::ScheduleEvent(const ScheduledEvent& event) {
    std::lock_guard<std::mutex> lock(m_eventsMutex);

    ScheduledEvent e = event;
    if (e.id.empty()) {
        e.id = GenerateEventId();
    }
    e.triggered = false;

    m_events.push_back(e);
    return e.id;
}

std::string WorldClock::ScheduleEventAt(const std::string& name, const WorldTime& time,
                                        const nlohmann::json& data) {
    ScheduledEvent event;
    event.name = name;
    event.triggerTimestamp = WorldTimeToTimestamp(time);
    event.data = data;
    event.recurring = false;

    return ScheduleEvent(event);
}

std::string WorldClock::ScheduleRecurringEvent(const std::string& name, int64_t intervalSeconds,
                                                const nlohmann::json& data) {
    ScheduledEvent event;
    event.name = name;
    event.triggerTimestamp = GetServerTimestamp() + intervalSeconds;
    event.data = data;
    event.recurring = true;
    event.recurInterval = intervalSeconds;

    return ScheduleEvent(event);
}

void WorldClock::CancelEvent(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(m_eventsMutex);

    m_events.erase(
        std::remove_if(m_events.begin(), m_events.end(),
            [&eventId](const ScheduledEvent& e) { return e.id == eventId; }),
        m_events.end()
    );
}

std::vector<ScheduledEvent> WorldClock::GetScheduledEvents() const {
    std::lock_guard<std::mutex> lock(m_eventsMutex);
    return m_events;
}

std::vector<ScheduledEvent> WorldClock::GetTodaysEvents() const {
    std::lock_guard<std::mutex> lock(m_eventsMutex);

    std::vector<ScheduledEvent> todaysEvents;
    int64_t now = GetServerTimestamp();

    // Events in next 24 game hours
    float gameHoursPerRealSecond = 24.0f / (m_config.dayLengthMinutes * 60.0f);
    int64_t dayLengthSeconds = static_cast<int64_t>(24.0f / gameHoursPerRealSecond);

    for (const auto& event : m_events) {
        if (!event.triggered &&
            event.triggerTimestamp >= now &&
            event.triggerTimestamp < now + dayLengthSeconds) {
            todaysEvents.push_back(event);
        }
    }

    // Sort by trigger time
    std::sort(todaysEvents.begin(), todaysEvents.end(),
        [](const ScheduledEvent& a, const ScheduledEvent& b) {
            return a.triggerTimestamp < b.triggerTimestamp;
        });

    return todaysEvents;
}

// ==================== Synchronization ====================

void WorldClock::SyncWithServer() {
    FirebaseManager::Instance().GetValue("rts/worldTime",
        [this](const nlohmann::json& data) {
            if (data.is_null() || data.empty()) return;

            WorldTime serverTime = WorldTime::FromJson(data);
            int64_t serverTimestamp = serverTime.serverTimestamp;

            SetServerTime(serverTime, serverTimestamp);
        });
}

void WorldClock::SetServerTime(const WorldTime& serverTime, int64_t serverTimestamp) {
    auto now = std::chrono::system_clock::now();
    int64_t localTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    m_serverTimeOffset = serverTimestamp - localTimestamp;

    {
        std::lock_guard<std::mutex> lock(m_timeMutex);
        m_time = serverTime;
        m_time.localTimestamp = localTimestamp;
    }

    m_lastSeason = serverTime.season;
    m_lastTimeOfDay = serverTime.timeOfDay;
    m_lastHour = serverTime.hour;

    CLOCK_LOG_INFO("Synced with server - offset: " + std::to_string(m_serverTimeOffset) + "s");
}

// ==================== Callbacks ====================

void WorldClock::OnTimeChanged(TimeChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_timeCallbacks.push_back(std::move(callback));
}

void WorldClock::OnSeasonChanged(SeasonChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_seasonCallbacks.push_back(std::move(callback));
}

void WorldClock::OnTimeOfDayChanged(TimeOfDayChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_todCallbacks.push_back(std::move(callback));
}

void WorldClock::OnEventTriggered(EventTriggeredCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_eventCallbacks.push_back(std::move(callback));
}

// ==================== Configuration ====================

void WorldClock::SetTimeScale(float dayLengthMinutes) {
    m_config.dayLengthMinutes = std::max(1.0f, dayLengthMinutes);
}

// ==================== Private Methods ====================

void WorldClock::UpdateTimeOfDay() {
    TimeOfDay newTod = CalculateTimeOfDay(m_time.hour);

    if (newTod != m_lastTimeOfDay) {
        TimeOfDay old = m_lastTimeOfDay;
        m_lastTimeOfDay = newTod;

        {
            std::lock_guard<std::mutex> lock(m_timeMutex);
            m_time.timeOfDay = newTod;
        }

        CLOCK_LOG_INFO("Time of day changed: " + std::string(TimeOfDayToString(old)) +
                       " -> " + std::string(TimeOfDayToString(newTod)));

        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (auto& callback : m_todCallbacks) {
            if (callback) {
                callback(old, newTod);
            }
        }
    }
}

void WorldClock::UpdateSeason() {
    Season newSeason = CalculateSeason(m_time.day);

    if (newSeason != m_lastSeason) {
        Season old = m_lastSeason;
        m_lastSeason = newSeason;

        {
            std::lock_guard<std::mutex> lock(m_timeMutex);
            m_time.season = newSeason;
        }

        CLOCK_LOG_INFO("Season changed: " + std::string(SeasonToString(old)) +
                       " -> " + std::string(SeasonToString(newSeason)));

        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (auto& callback : m_seasonCallbacks) {
            if (callback) {
                callback(old, newSeason);
            }
        }
    }
}

void WorldClock::ProcessEvents() {
    int64_t currentTime = GetServerTimestamp();
    std::vector<ScheduledEvent> triggeredEvents;

    {
        std::lock_guard<std::mutex> lock(m_eventsMutex);

        for (auto& event : m_events) {
            if (!event.triggered && event.triggerTimestamp <= currentTime) {
                event.triggered = true;
                triggeredEvents.push_back(event);

                // Handle recurring events
                if (event.recurring && event.recurInterval > 0) {
                    event.triggerTimestamp += event.recurInterval;
                    event.triggered = false;
                }
            }
        }

        // Remove non-recurring triggered events
        m_events.erase(
            std::remove_if(m_events.begin(), m_events.end(),
                [](const ScheduledEvent& e) {
                    return e.triggered && !e.recurring;
                }),
            m_events.end()
        );
    }

    // Notify callbacks
    if (!triggeredEvents.empty()) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (const auto& event : triggeredEvents) {
            for (auto& callback : m_eventCallbacks) {
                if (callback) {
                    callback(event);
                }
            }
        }
    }
}

Season WorldClock::CalculateSeason(int dayOfYear) const {
    float springEnd = m_config.springDays;
    float summerEnd = springEnd + m_config.summerDays;
    float autumnEnd = summerEnd + m_config.autumnDays;

    float day = static_cast<float>(dayOfYear);

    if (day <= springEnd) return Season::Spring;
    if (day <= summerEnd) return Season::Summer;
    if (day <= autumnEnd) return Season::Autumn;
    return Season::Winter;
}

TimeOfDay WorldClock::CalculateTimeOfDay(float hour) const {
    if (hour >= 5.0f && hour < 8.0f) return TimeOfDay::Dawn;
    if (hour >= 8.0f && hour < 12.0f) return TimeOfDay::Morning;
    if (hour >= 12.0f && hour < 17.0f) return TimeOfDay::Afternoon;
    if (hour >= 17.0f && hour < 20.0f) return TimeOfDay::Dusk;
    if (hour >= 20.0f && hour < 24.0f) return TimeOfDay::Night;
    return TimeOfDay::Midnight;  // 0:00 - 5:00
}

std::string WorldClock::GenerateEventId() {
    return "event_" + std::to_string(m_nextEventId++);
}

} // namespace Vehement
