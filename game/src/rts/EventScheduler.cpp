#include "EventScheduler.hpp"
#include "../network/FirebaseManager.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>

// Logging macros
#if __has_include("../../../engine/core/Logger.hpp")
#include "../../../engine/core/Logger.hpp"
#define SCHEDULER_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define SCHEDULER_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define SCHEDULER_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define SCHEDULER_LOG_INFO(...) std::cout << "[EventScheduler] " << __VA_ARGS__ << std::endl
#define SCHEDULER_LOG_WARN(...) std::cerr << "[EventScheduler WARN] " << __VA_ARGS__ << std::endl
#define SCHEDULER_LOG_ERROR(...) std::cerr << "[EventScheduler ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

EventScheduler::EventScheduler()
    : m_rng(std::random_device{}()) {
    // Initialize default event configurations
    InitializeDefaultConfigs();
}

EventScheduler::~EventScheduler() {
    if (m_initialized) {
        Shutdown();
    }
}

void EventScheduler::InitializeDefaultConfigs() {
    // Threat events
    SetEventConfig(EventConfigBuilder(EventType::ZombieHorde)
        .Probability(0.3f)
        .Duration(5.0f, 15.0f)
        .Cooldown(30.0f)
        .Warning(1.0f)
        .Radius(150.0f, 400.0f)
        .Severity(EventSeverity::Major)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::BossZombie)
        .Probability(0.1f)
        .Duration(10.0f, 20.0f)
        .Cooldown(120.0f)
        .Warning(2.0f)
        .Radius(100.0f, 200.0f)
        .Severity(EventSeverity::Major)
        .MaxSimultaneous(1)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Plague)
        .Probability(0.15f)
        .Duration(30.0f, 60.0f)
        .Cooldown(180.0f)
        .Warning(5.0f)
        .Radius(200.0f, 500.0f)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Infestation)
        .Probability(0.2f)
        .Duration(15.0f, 30.0f)
        .Cooldown(60.0f)
        .Warning(2.0f)
        .Radius(50.0f, 150.0f)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::NightTerror)
        .Probability(0.05f)
        .Duration(5.0f, 10.0f)
        .Cooldown(240.0f)
        .Warning(0.5f)
        .Global(true)
        .Severity(EventSeverity::Catastrophic)
        .MaxSimultaneous(1)
        .Build());

    // Opportunity events
    SetEventConfig(EventConfigBuilder(EventType::SupplyDrop)
        .Probability(0.5f)
        .Duration(10.0f, 30.0f)
        .Cooldown(15.0f)
        .Warning(1.0f)
        .Radius(30.0f, 80.0f)
        .Severity(EventSeverity::Minor)
        .MaxSimultaneous(5)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::RefugeeCamp)
        .Probability(0.2f)
        .Duration(20.0f, 45.0f)
        .Cooldown(60.0f)
        .Warning(3.0f)
        .Radius(100.0f, 200.0f)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::TreasureCache)
        .Probability(0.15f)
        .Duration(15.0f, 30.0f)
        .Cooldown(45.0f)
        .Warning(2.0f)
        .Radius(20.0f, 50.0f)
        .Severity(EventSeverity::Minor)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::AbandonedBase)
        .Probability(0.1f)
        .Duration(30.0f, 60.0f)
        .Cooldown(90.0f)
        .Warning(5.0f)
        .Radius(150.0f, 300.0f)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::WeaponCache)
        .Probability(0.1f)
        .Duration(10.0f, 20.0f)
        .Cooldown(60.0f)
        .Warning(2.0f)
        .Radius(25.0f, 60.0f)
        .Severity(EventSeverity::Minor)
        .Build());

    // Environmental events
    SetEventConfig(EventConfigBuilder(EventType::Storm)
        .Probability(0.25f)
        .Duration(15.0f, 45.0f)
        .Cooldown(30.0f)
        .Warning(3.0f)
        .Radius(500.0f, 1500.0f)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Earthquake)
        .Probability(0.1f)
        .Duration(1.0f, 3.0f)
        .Cooldown(120.0f)
        .Warning(0.5f)
        .Radius(300.0f, 800.0f)
        .Severity(EventSeverity::Major)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Drought)
        .Probability(0.1f)
        .Duration(60.0f, 180.0f)
        .Cooldown(360.0f)
        .Warning(10.0f)
        .Global(true)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Bountiful)
        .Probability(0.15f)
        .Duration(30.0f, 60.0f)
        .Cooldown(120.0f)
        .Warning(5.0f)
        .Global(true)
        .Severity(EventSeverity::Minor)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Fog)
        .Probability(0.3f)
        .Duration(20.0f, 40.0f)
        .Cooldown(20.0f)
        .Warning(2.0f)
        .Radius(400.0f, 1000.0f)
        .Severity(EventSeverity::Minor)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::HeatWave)
        .Probability(0.2f)
        .Duration(45.0f, 90.0f)
        .Cooldown(60.0f)
        .Warning(5.0f)
        .Global(true)
        .Severity(EventSeverity::Moderate)
        .Build());

    // Social events
    SetEventConfig(EventConfigBuilder(EventType::TradeCaravan)
        .Probability(0.3f)
        .Duration(15.0f, 30.0f)
        .Cooldown(30.0f)
        .Warning(5.0f)
        .Radius(50.0f, 100.0f)
        .Severity(EventSeverity::Minor)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::MilitaryAid)
        .Probability(0.1f)
        .Duration(30.0f, 60.0f)
        .Cooldown(90.0f)
        .Warning(5.0f)
        .Radius(200.0f, 400.0f)
        .Severity(EventSeverity::Moderate)
        .MinPlayers(2)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Bandits)
        .Probability(0.2f)
        .Duration(10.0f, 25.0f)
        .Cooldown(45.0f)
        .Warning(1.0f)
        .Radius(100.0f, 250.0f)
        .Severity(EventSeverity::Moderate)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Deserters)
        .Probability(0.15f)
        .Duration(10.0f, 20.0f)
        .Cooldown(60.0f)
        .Warning(3.0f)
        .Radius(50.0f, 120.0f)
        .Severity(EventSeverity::Minor)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Merchant)
        .Probability(0.2f)
        .Duration(20.0f, 40.0f)
        .Cooldown(45.0f)
        .Warning(3.0f)
        .Radius(40.0f, 80.0f)
        .Severity(EventSeverity::Minor)
        .Build());

    // Global events
    SetEventConfig(EventConfigBuilder(EventType::BloodMoon)
        .Probability(0.1f)
        .Duration(15.0f, 30.0f)
        .Cooldown(180.0f)
        .Warning(5.0f)
        .Global(true)
        .Severity(EventSeverity::Major)
        .MaxSimultaneous(1)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Eclipse)
        .Probability(0.05f)
        .Duration(20.0f, 40.0f)
        .Cooldown(240.0f)
        .Warning(10.0f)
        .Global(true)
        .Severity(EventSeverity::Moderate)
        .MaxSimultaneous(1)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::GoldenAge)
        .Probability(0.1f)
        .Duration(30.0f, 60.0f)
        .Cooldown(180.0f)
        .Warning(5.0f)
        .Global(true)
        .Severity(EventSeverity::Minor)
        .MaxSimultaneous(1)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Apocalypse)
        .Probability(0.02f)
        .Duration(30.0f, 60.0f)
        .Cooldown(480.0f)
        .Warning(10.0f)
        .Global(true)
        .Severity(EventSeverity::Catastrophic)
        .MaxSimultaneous(1)
        .MinPlayers(3)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::Ceasefire)
        .Probability(0.05f)
        .Duration(30.0f, 60.0f)
        .Cooldown(120.0f)
        .Warning(5.0f)
        .Global(true)
        .Severity(EventSeverity::Minor)
        .MaxSimultaneous(1)
        .MinPlayers(2)
        .Build());

    SetEventConfig(EventConfigBuilder(EventType::DoubleXP)
        .Probability(0.1f)
        .Duration(30.0f, 60.0f)
        .Cooldown(120.0f)
        .Warning(3.0f)
        .Global(true)
        .Severity(EventSeverity::Minor)
        .MaxSimultaneous(1)
        .Build());

    // Enable all events by default
    for (const auto& [type, config] : m_eventConfigs) {
        m_enabledEvents[type] = config.enabledByDefault;
    }
}

bool EventScheduler::Initialize(bool isHost) {
    if (m_initialized) {
        SCHEDULER_LOG_WARN("EventScheduler already initialized");
        return true;
    }

    m_isHost = isHost;

    // Start listening to Firebase for events
    if (FirebaseManager::Instance().IsInitialized()) {
        m_firebaseListenerId = FirebaseManager::Instance().ListenToPath(
            m_firebasePath,
            [this](const nlohmann::json& data) {
                OnFirebaseEventUpdate(data);
            }
        );
    }

    m_initialized = true;
    SCHEDULER_LOG_INFO("EventScheduler initialized (isHost: " + std::string(isHost ? "true" : "false") + ")");

    return true;
}

void EventScheduler::Shutdown() {
    if (!m_initialized) {
        return;
    }

    SCHEDULER_LOG_INFO("Shutting down EventScheduler");

    // Stop Firebase listener
    if (!m_firebaseListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_firebaseListenerId);
        m_firebaseListenerId.clear();
    }

    // Clear events
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_activeEvents.clear();
        m_scheduledEvents.clear();
        m_completedEvents.clear();
    }

    // Clear callbacks
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_scheduledCallbacks.clear();
        m_startedCallbacks.clear();
        m_endedCallbacks.clear();
        m_cancelledCallbacks.clear();
    }

    m_initialized = false;
}

void EventScheduler::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update cooldowns
    UpdateCooldowns(deltaTime);

    // Process scheduled events (start/end)
    ProcessScheduledEvents();

    // Only host generates new events
    if (m_isHost) {
        m_timeSinceLastSchedule += deltaTime;
        m_timeSinceLastBalance += deltaTime;

        // Check if we should schedule new events
        if (m_timeSinceLastSchedule >= m_scheduleCheckInterval) {
            m_timeSinceLastSchedule = 0.0f;

            if (ShouldGenerateEvent()) {
                auto event = GenerateRandomEvent();
                if (event) {
                    ScheduleEvent(*event);
                }
            }
        }

        // Periodic balancing
        if (m_timeSinceLastBalance >= m_balanceCheckInterval) {
            m_timeSinceLastBalance = 0.0f;
            BalanceUpcomingEvents();
        }
    }
}

void EventScheduler::ProcessScheduledEvents() {
    int64_t currentTime = GetCurrentTimeMs();
    std::vector<WorldEvent> toStart;
    std::vector<WorldEvent> toEnd;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);

        // Check scheduled events for start
        for (auto it = m_scheduledEvents.begin(); it != m_scheduledEvents.end();) {
            if (currentTime >= it->second.startTime) {
                WorldEvent event = it->second;
                event.isActive = true;
                m_activeEvents[event.id] = event;
                toStart.push_back(event);
                it = m_scheduledEvents.erase(it);
            } else {
                ++it;
            }
        }

        // Check active events for end
        for (auto it = m_activeEvents.begin(); it != m_activeEvents.end();) {
            if (currentTime >= it->second.endTime) {
                WorldEvent event = it->second;
                event.isActive = false;
                event.isCompleted = true;
                toEnd.push_back(event);

                // Add to completed history
                m_completedEvents.push_back(event);
                if (m_completedEvents.size() > MaxCompletedHistory) {
                    m_completedEvents.erase(m_completedEvents.begin());
                }

                it = m_activeEvents.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Invoke callbacks outside of lock
    for (const auto& event : toStart) {
        InvokeStartedCallbacks(event);
        if (m_isHost) {
            UpdateEventInFirebase(event);
        }
    }

    for (const auto& event : toEnd) {
        InvokeEndedCallbacks(event);
        if (m_isHost) {
            RemoveEventFromFirebase(event.id);
        }
        m_stats.totalEventsCompleted++;
    }
}

bool EventScheduler::LoadConfiguration(const nlohmann::json& config) {
    try {
        if (config.contains("events") && config["events"].is_array()) {
            for (const auto& eventJson : config["events"]) {
                EventConfig cfg;
                auto typeOpt = StringToEventType(eventJson.value("type", ""));
                if (!typeOpt) continue;

                cfg.type = *typeOpt;
                cfg.probabilityPerHour = eventJson.value("probabilityPerHour", 0.5f);
                cfg.minDurationMinutes = eventJson.value("minDurationMinutes", 5.0f);
                cfg.maxDurationMinutes = eventJson.value("maxDurationMinutes", 30.0f);
                cfg.minCooldownMinutes = eventJson.value("minCooldownMinutes", 60.0f);
                cfg.warningLeadTimeMinutes = eventJson.value("warningLeadTimeMinutes", 2.0f);
                cfg.isGlobal = eventJson.value("isGlobal", false);
                cfg.requiresMinPlayers = eventJson.value("requiresMinPlayers", false);
                cfg.minPlayerCount = eventJson.value("minPlayerCount", 1);
                cfg.maxSimultaneous = eventJson.value("maxSimultaneous", 3);
                cfg.minRadius = eventJson.value("minRadius", 50.0f);
                cfg.maxRadius = eventJson.value("maxRadius", 200.0f);
                cfg.severity = static_cast<EventSeverity>(eventJson.value("severity", 1));
                cfg.enabledByDefault = eventJson.value("enabled", true);

                SetEventConfig(cfg);
            }
        }

        // Load scheduler settings
        if (config.contains("scheduler")) {
            const auto& sched = config["scheduler"];
            m_scheduleCheckInterval = sched.value("checkIntervalSeconds", 60.0f);
            m_balanceCheckInterval = sched.value("balanceIntervalSeconds", 300.0f);
        }

        // Load world bounds
        if (config.contains("worldBounds")) {
            const auto& bounds = config["worldBounds"];
            m_worldMin.x = bounds.value("minX", 0.0f);
            m_worldMin.y = bounds.value("minY", 0.0f);
            m_worldMax.x = bounds.value("maxX", 10000.0f);
            m_worldMax.y = bounds.value("maxY", 10000.0f);
        }

        return true;
    } catch (const std::exception& e) {
        SCHEDULER_LOG_ERROR("Failed to load configuration: " + std::string(e.what()));
        return false;
    }
}

bool EventScheduler::LoadConfigurationFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        SCHEDULER_LOG_ERROR("Failed to open config file: " + path);
        return false;
    }

    try {
        nlohmann::json config;
        file >> config;
        return LoadConfiguration(config);
    } catch (const std::exception& e) {
        SCHEDULER_LOG_ERROR("Failed to parse config file: " + std::string(e.what()));
        return false;
    }
}

const EventConfig* EventScheduler::GetEventConfig(EventType type) const {
    auto it = m_eventConfigs.find(type);
    if (it != m_eventConfigs.end()) {
        return &it->second;
    }
    return nullptr;
}

void EventScheduler::SetEventConfig(const EventConfig& config) {
    m_eventConfigs[config.type] = config;
    m_enabledEvents[config.type] = config.enabledByDefault;
}

void EventScheduler::SetEventEnabled(EventType type, bool enabled) {
    m_enabledEvents[type] = enabled;
}

bool EventScheduler::IsEventEnabled(EventType type) const {
    auto it = m_enabledEvents.find(type);
    return it != m_enabledEvents.end() && it->second;
}

std::optional<WorldEvent> EventScheduler::GenerateRandomEvent() {
    auto eventType = SelectNextEventType();
    if (!eventType) {
        return std::nullopt;
    }

    const auto* config = GetEventConfig(*eventType);
    if (!config) {
        return std::nullopt;
    }

    // Select appropriate location based on category
    glm::vec2 location;
    EventCategory category = GetEventCategory(*eventType);

    if (config->isGlobal) {
        // Global events don't need specific location
        location = (m_worldMin + m_worldMax) * 0.5f;
    } else if (category == EventCategory::Threat) {
        location = SelectThreatLocation();
    } else if (category == EventCategory::Opportunity) {
        location = SelectOpportunityLocation();
    } else {
        location = SelectRandomLocation();
    }

    return GenerateEvent(*eventType, location);
}

WorldEvent EventScheduler::GenerateEvent(EventType type, const glm::vec2& location) {
    const auto* config = GetEventConfig(type);
    if (!config) {
        // Use default config
        EventConfig defaultConfig;
        defaultConfig.type = type;
        return GenerateEventInternal(defaultConfig, location);
    }

    return GenerateEventInternal(*config, location);
}

WorldEvent EventScheduler::GenerateEventInternal(const EventConfig& config,
                                                  const glm::vec2& location) {
    int64_t currentTime = GetCurrentTimeMs();

    WorldEvent event;
    event.id = WorldEvent::GenerateEventId(config.type, currentTime);
    event.type = config.type;
    event.location = location;
    event.isGlobal = config.isGlobal;

    // Randomize radius
    std::uniform_real_distribution<float> radiusDist(config.minRadius, config.maxRadius);
    event.radius = radiusDist(m_rng);

    // Randomize duration
    std::uniform_real_distribution<float> durationDist(
        config.minDurationMinutes * 60.0f * 1000.0f,
        config.maxDurationMinutes * 60.0f * 1000.0f);
    int64_t duration = static_cast<int64_t>(durationDist(m_rng));

    // Set timing
    int64_t warningLead = static_cast<int64_t>(config.warningLeadTimeMinutes * 60.0f * 1000.0f);
    event.scheduledTime = currentTime;
    event.warningTime = currentTime;
    event.startTime = currentTime + warningLead;
    event.endTime = event.startTime + duration;

    event.isActive = false;
    event.isCompleted = false;
    event.wasCancelled = false;

    // Set scaling
    event.intensity = 1.0f + (0.1f * (m_currentPlayerCount - 1));
    event.difficultyTier = static_cast<int32_t>(config.severity) + 1;
    event.playerScaling = m_currentPlayerCount;

    // Generate name and description
    GenerateEventName(event);
    GenerateEventDescription(event);

    // Generate rewards for opportunity events
    if (GetEventCategory(config.type) == EventCategory::Opportunity) {
        GenerateEventRewards(event);
    }

    // Scale for player count
    ScaleEventForPlayers(event, m_currentPlayerCount);

    return event;
}

WorldEvent EventScheduler::GenerateFromTemplate(const EventTemplate& tmpl,
                                                 const glm::vec2& location) {
    return tmpl.CreateEvent(location, m_currentPlayerCount);
}

glm::vec2 EventScheduler::SelectRandomLocation() const {
    std::uniform_real_distribution<float> xDist(m_worldMin.x, m_worldMax.x);
    std::uniform_real_distribution<float> yDist(m_worldMin.y, m_worldMax.y);
    return {xDist(m_rng), yDist(m_rng)};
}

glm::vec2 EventScheduler::SelectThreatLocation() const {
    // Select location near player positions
    if (m_playerPositions.empty()) {
        return SelectRandomLocation();
    }

    // Pick a random player
    std::uniform_int_distribution<size_t> playerDist(0, m_playerPositions.size() - 1);
    auto it = m_playerPositions.begin();
    std::advance(it, playerDist(m_rng));

    // Offset from player position
    std::uniform_real_distribution<float> offsetDist(100.0f, 500.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);

    float offset = offsetDist(m_rng);
    float angle = angleDist(m_rng);

    glm::vec2 location = it->second;
    location.x += offset * std::cos(angle);
    location.y += offset * std::sin(angle);

    // Clamp to world bounds
    location.x = std::clamp(location.x, m_worldMin.x, m_worldMax.x);
    location.y = std::clamp(location.y, m_worldMin.y, m_worldMax.y);

    return location;
}

glm::vec2 EventScheduler::SelectOpportunityLocation() const {
    // Select location somewhat away from players
    if (m_playerPositions.empty()) {
        return SelectRandomLocation();
    }

    // Try a few random locations and pick the one furthest from players
    glm::vec2 bestLocation = SelectRandomLocation();
    float bestMinDistance = 0.0f;

    for (int i = 0; i < 5; ++i) {
        glm::vec2 candidate = SelectRandomLocation();
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& [playerId, pos] : m_playerPositions) {
            float dist = glm::length(candidate - pos);
            minDistance = std::min(minDistance, dist);
        }

        if (minDistance > bestMinDistance) {
            bestMinDistance = minDistance;
            bestLocation = candidate;
        }
    }

    return bestLocation;
}

void EventScheduler::ScheduleEvent(const WorldEvent& event,
                                    std::function<void(bool success)> callback) {
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);

        // Check if already at max events
        if (m_activeEvents.size() + m_scheduledEvents.size() >= MaxActiveEvents) {
            SCHEDULER_LOG_WARN("Max events reached, cannot schedule new event");
            if (callback) callback(false);
            return;
        }

        // Check if at max for this type
        const auto* config = GetEventConfig(event.type);
        if (config) {
            int32_t currentCount = GetActiveEventCount(event.type);
            if (currentCount >= config->maxSimultaneous) {
                SCHEDULER_LOG_WARN("Max events of type " + std::string(EventTypeToString(event.type)) + " reached");
                if (callback) callback(false);
                return;
            }
        }

        m_scheduledEvents[event.id] = event;
    }

    // Set cooldown for this event type
    SetCooldown(event.type);

    // Publish to Firebase
    if (m_isHost) {
        PublishEventToFirebase(event);
    }

    // Update stats
    m_stats.totalEventsScheduled++;
    m_stats.eventsPerType[event.type]++;
    m_stats.eventsPerCategory[GetEventCategory(event.type)]++;
    m_stats.lastScheduleTime = GetCurrentTimeMs();

    // Invoke callbacks
    InvokeScheduledCallbacks(event);

    SCHEDULER_LOG_INFO("Scheduled event: " + event.name + " (ID: " + event.id + ")");

    if (callback) callback(true);
}

void EventScheduler::ScheduleEventAt(const WorldEvent& event, int64_t startTimeMs) {
    WorldEvent modifiedEvent = event;
    int64_t duration = event.endTime - event.startTime;
    modifiedEvent.startTime = startTimeMs;
    modifiedEvent.endTime = startTimeMs + duration;
    ScheduleEvent(modifiedEvent);
}

void EventScheduler::ScheduleEventAfter(const WorldEvent& event, float delaySeconds) {
    int64_t startTime = GetCurrentTimeMs() + static_cast<int64_t>(delaySeconds * 1000.0f);
    ScheduleEventAt(event, startTime);
}

bool EventScheduler::CancelEvent(const std::string& eventId) {
    WorldEvent cancelledEvent;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);

        // Check scheduled events
        auto schedIt = m_scheduledEvents.find(eventId);
        if (schedIt != m_scheduledEvents.end()) {
            cancelledEvent = schedIt->second;
            cancelledEvent.wasCancelled = true;
            m_scheduledEvents.erase(schedIt);
            found = true;
        }

        // Check active events
        if (!found) {
            auto activeIt = m_activeEvents.find(eventId);
            if (activeIt != m_activeEvents.end()) {
                cancelledEvent = activeIt->second;
                cancelledEvent.wasCancelled = true;
                cancelledEvent.isActive = false;
                m_activeEvents.erase(activeIt);
                found = true;
            }
        }
    }

    if (found) {
        if (m_isHost) {
            RemoveEventFromFirebase(eventId);
        }
        InvokeCancelledCallbacks(cancelledEvent);
        m_stats.totalEventsCancelled++;
        SCHEDULER_LOG_INFO("Cancelled event: " + cancelledEvent.name);
    }

    return found;
}

int32_t EventScheduler::CancelEventsByType(EventType type) {
    std::vector<std::string> toCancel;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);

        for (const auto& [id, event] : m_scheduledEvents) {
            if (event.type == type) {
                toCancel.push_back(id);
            }
        }
        for (const auto& [id, event] : m_activeEvents) {
            if (event.type == type) {
                toCancel.push_back(id);
            }
        }
    }

    for (const auto& id : toCancel) {
        CancelEvent(id);
    }

    return static_cast<int32_t>(toCancel.size());
}

void EventScheduler::CancelAllEvents() {
    std::vector<std::string> allIds;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        for (const auto& [id, event] : m_scheduledEvents) {
            allIds.push_back(id);
        }
        for (const auto& [id, event] : m_activeEvents) {
            allIds.push_back(id);
        }
    }

    for (const auto& id : allIds) {
        CancelEvent(id);
    }
}

bool EventScheduler::ExtendEvent(const std::string& eventId, float additionalSeconds) {
    std::lock_guard<std::mutex> lock(m_eventMutex);

    auto it = m_activeEvents.find(eventId);
    if (it != m_activeEvents.end()) {
        it->second.endTime += static_cast<int64_t>(additionalSeconds * 1000.0f);
        if (m_isHost) {
            UpdateEventInFirebase(it->second);
        }
        return true;
    }

    auto schedIt = m_scheduledEvents.find(eventId);
    if (schedIt != m_scheduledEvents.end()) {
        schedIt->second.endTime += static_cast<int64_t>(additionalSeconds * 1000.0f);
        if (m_isHost) {
            UpdateEventInFirebase(schedIt->second);
        }
        return true;
    }

    return false;
}

std::vector<WorldEvent> EventScheduler::GetActiveEvents() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    std::vector<WorldEvent> events;
    events.reserve(m_activeEvents.size());
    for (const auto& [id, event] : m_activeEvents) {
        events.push_back(event);
    }
    return events;
}

std::vector<WorldEvent> EventScheduler::GetScheduledEvents() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    std::vector<WorldEvent> events;
    events.reserve(m_scheduledEvents.size());
    for (const auto& [id, event] : m_scheduledEvents) {
        events.push_back(event);
    }
    return events;
}

std::vector<WorldEvent> EventScheduler::GetCompletedEvents() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    return m_completedEvents;
}

std::optional<WorldEvent> EventScheduler::GetEvent(const std::string& eventId) const {
    std::lock_guard<std::mutex> lock(m_eventMutex);

    auto schedIt = m_scheduledEvents.find(eventId);
    if (schedIt != m_scheduledEvents.end()) {
        return schedIt->second;
    }

    auto activeIt = m_activeEvents.find(eventId);
    if (activeIt != m_activeEvents.end()) {
        return activeIt->second;
    }

    return std::nullopt;
}

std::vector<WorldEvent> EventScheduler::GetEventsAtPosition(const glm::vec2& pos) const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    std::vector<WorldEvent> events;

    for (const auto& [id, event] : m_activeEvents) {
        if (event.IsPositionAffected(pos)) {
            events.push_back(event);
        }
    }

    return events;
}

std::vector<WorldEvent> EventScheduler::GetEventsForPlayer(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    std::vector<WorldEvent> events;

    auto posIt = m_playerPositions.find(playerId);
    glm::vec2 playerPos = posIt != m_playerPositions.end() ? posIt->second : glm::vec2(0.0f);

    for (const auto& [id, event] : m_activeEvents) {
        if (event.isGlobal || event.IsPositionAffected(playerPos)) {
            events.push_back(event);
        }
    }

    return events;
}

int32_t EventScheduler::GetActiveEventCount(EventType type) const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    int32_t count = 0;

    for (const auto& [id, event] : m_activeEvents) {
        if (event.type == type) ++count;
    }
    for (const auto& [id, event] : m_scheduledEvents) {
        if (event.type == type) ++count;
    }

    return count;
}

int32_t EventScheduler::GetTotalActiveEventCount() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    return static_cast<int32_t>(m_activeEvents.size() + m_scheduledEvents.size());
}

void EventScheduler::BalanceUpcomingEvents() {
    std::lock_guard<std::mutex> lock(m_eventMutex);

    // Count events by category
    std::map<EventCategory, int32_t> categoryCounts;
    for (const auto& [id, event] : m_activeEvents) {
        categoryCounts[GetEventCategory(event.type)]++;
    }
    for (const auto& [id, event] : m_scheduledEvents) {
        categoryCounts[GetEventCategory(event.type)]++;
    }

    // Cancel excess threat events if too many
    if (categoryCounts[EventCategory::Threat] > MaxThreatEvents) {
        // Find and cancel oldest threat events
        std::vector<std::pair<int64_t, std::string>> threatEvents;
        for (const auto& [id, event] : m_scheduledEvents) {
            if (GetEventCategory(event.type) == EventCategory::Threat) {
                threatEvents.emplace_back(event.scheduledTime, id);
            }
        }

        std::sort(threatEvents.begin(), threatEvents.end());

        int32_t toCancel = categoryCounts[EventCategory::Threat] - MaxThreatEvents;
        for (int i = 0; i < toCancel && i < threatEvents.size(); ++i) {
            m_scheduledEvents.erase(threatEvents[i].second);
            SCHEDULER_LOG_INFO("Balanced: cancelled excess threat event");
        }
    }
}

bool EventScheduler::ShouldGenerateEvent() const {
    if (GetTotalActiveEventCount() >= MaxActiveEvents) {
        return false;
    }

    // Base probability check
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float roll = dist(m_rng);

    // Higher chance when fewer events are active
    float eventRatio = static_cast<float>(GetTotalActiveEventCount()) / MaxActiveEvents;
    float adjustedChance = 0.3f * (1.0f - eventRatio);

    return roll < adjustedChance;
}

std::optional<EventType> EventScheduler::SelectNextEventType() const {
    // Build list of eligible event types
    std::vector<std::pair<EventType, float>> eligibleEvents;

    for (const auto& [type, config] : m_eventConfigs) {
        if (!IsEventEnabled(type)) continue;
        if (IsEventOnCooldown(type)) continue;
        if (config.requiresMinPlayers && m_currentPlayerCount < config.minPlayerCount) continue;
        if (GetActiveEventCount(type) >= config.maxSimultaneous) continue;

        eligibleEvents.emplace_back(type, config.probabilityPerHour);
    }

    if (eligibleEvents.empty()) {
        return std::nullopt;
    }

    // Weighted random selection
    float totalWeight = 0.0f;
    for (const auto& [type, weight] : eligibleEvents) {
        totalWeight += weight;
    }

    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float roll = dist(m_rng);

    float cumulative = 0.0f;
    for (const auto& [type, weight] : eligibleEvents) {
        cumulative += weight;
        if (roll <= cumulative) {
            return type;
        }
    }

    return eligibleEvents.back().first;
}

bool EventScheduler::IsEventOnCooldown(EventType type) const {
    auto it = m_cooldowns.find(type);
    return it != m_cooldowns.end() && it->second > 0.0f;
}

float EventScheduler::GetRemainingCooldown(EventType type) const {
    auto it = m_cooldowns.find(type);
    return it != m_cooldowns.end() ? std::max(0.0f, it->second) : 0.0f;
}

void EventScheduler::SyncWithFirebase() {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    // Request current events from Firebase
    FirebaseManager::Instance().GetValue(m_firebasePath, [this](const nlohmann::json& data) {
        OnFirebaseEventUpdate(data);
    });
}

void EventScheduler::OnFirebaseEventUpdate(const nlohmann::json& data) {
    if (m_isHost || data.is_null()) return;

    std::lock_guard<std::mutex> lock(m_eventMutex);

    // Parse events from Firebase
    std::map<std::string, WorldEvent> newEvents;

    if (data.is_object()) {
        for (const auto& [id, eventJson] : data.items()) {
            try {
                WorldEvent event = WorldEvent::FromJson(eventJson);
                newEvents[id] = event;
            } catch (const std::exception& e) {
                SCHEDULER_LOG_ERROR("Failed to parse event from Firebase: " + std::string(e.what()));
            }
        }
    }

    // Update local events
    int64_t currentTime = GetCurrentTimeMs();

    for (const auto& [id, event] : newEvents) {
        if (event.IsCurrentlyActive(currentTime)) {
            m_activeEvents[id] = event;
        } else if (!event.HasExpired(currentTime)) {
            m_scheduledEvents[id] = event;
        }
    }

    // Remove events no longer in Firebase
    for (auto it = m_activeEvents.begin(); it != m_activeEvents.end();) {
        if (newEvents.find(it->first) == newEvents.end()) {
            it = m_activeEvents.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_scheduledEvents.begin(); it != m_scheduledEvents.end();) {
        if (newEvents.find(it->first) == newEvents.end()) {
            it = m_scheduledEvents.erase(it);
        } else {
            ++it;
        }
    }
}

void EventScheduler::SetWorldBounds(const glm::vec2& min, const glm::vec2& max) {
    m_worldMin = min;
    m_worldMax = max;
}

void EventScheduler::UpdatePlayerPosition(const std::string& playerId, const glm::vec2& pos) {
    m_playerPositions[playerId] = pos;
}

void EventScheduler::RemovePlayer(const std::string& playerId) {
    m_playerPositions.erase(playerId);
}

void EventScheduler::OnEventScheduled(EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_scheduledCallbacks.push_back(std::move(callback));
}

void EventScheduler::OnEventStarted(EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_startedCallbacks.push_back(std::move(callback));
}

void EventScheduler::OnEventEnded(EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_endedCallbacks.push_back(std::move(callback));
}

void EventScheduler::OnEventCancelled(EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_cancelledCallbacks.push_back(std::move(callback));
}

void EventScheduler::ResetStats() {
    m_stats = SchedulerStats{};
}

// =========================================================================
// Private helpers
// =========================================================================

void EventScheduler::GenerateEventName(WorldEvent& event) {
    // Generate contextual name based on event type
    static const std::map<EventType, std::vector<std::string>> nameTemplates = {
        {EventType::ZombieHorde, {"Zombie Horde", "Undead Swarm", "Walker Wave"}},
        {EventType::BossZombie, {"Alpha Zombie", "Undead Abomination", "Zombie Titan"}},
        {EventType::Plague, {"Outbreak", "Infection Spread", "Pandemic Warning"}},
        {EventType::SupplyDrop, {"Supply Drop", "Care Package", "Resource Cache"}},
        {EventType::RefugeeCamp, {"Refugee Camp", "Survivor Group", "Lost Civilians"}},
        {EventType::TreasureCache, {"Hidden Stash", "Treasure Trove", "Valuable Find"}},
        {EventType::Storm, {"Severe Storm", "Weather Warning", "Tempest"}},
        {EventType::Earthquake, {"Earthquake", "Seismic Event", "Tremor"}},
        {EventType::BloodMoon, {"Blood Moon Rising", "Crimson Night", "Lunar Terror"}},
        {EventType::TradeCaravan, {"Trade Caravan", "Merchants Arrive", "Traveling Traders"}},
        {EventType::Bandits, {"Bandit Raid", "Marauder Attack", "Raider Incursion"}},
        {EventType::GoldenAge, {"Golden Age", "Prosperity Period", "Blessed Times"}},
    };

    auto it = nameTemplates.find(event.type);
    if (it != nameTemplates.end() && !it->second.empty()) {
        std::uniform_int_distribution<size_t> dist(0, it->second.size() - 1);
        event.name = it->second[dist(m_rng)];
    } else {
        event.name = EventTypeToString(event.type);
    }
}

void EventScheduler::GenerateEventDescription(WorldEvent& event) {
    // Generate basic description
    switch (event.type) {
        case EventType::ZombieHorde:
            event.description = "A massive horde of zombies is approaching. Prepare your defenses!";
            break;
        case EventType::BossZombie:
            event.description = "A powerful undead creature has emerged. Extreme caution advised.";
            break;
        case EventType::SupplyDrop:
            event.description = "Supplies have been dropped nearby. Claim them before others do!";
            break;
        case EventType::Storm:
            event.description = "A severe storm is approaching. Visibility and movement will be reduced.";
            break;
        case EventType::BloodMoon:
            event.description = "The blood moon rises. All zombies become significantly stronger.";
            break;
        case EventType::GoldenAge:
            event.description = "A period of prosperity begins. All production rates are increased.";
            break;
        default:
            event.description = "A world event is occurring in your region.";
            break;
    }
}

void EventScheduler::GenerateEventRewards(WorldEvent& event) {
    // Generate rewards based on event type and difficulty
    float rewardMultiplier = event.intensity * event.difficultyTier;

    switch (event.type) {
        case EventType::SupplyDrop:
            event.resourceRewards[ResourceType::Food] = static_cast<int32_t>(50 * rewardMultiplier);
            event.resourceRewards[ResourceType::Water] = static_cast<int32_t>(30 * rewardMultiplier);
            event.resourceRewards[ResourceType::Ammunition] = static_cast<int32_t>(20 * rewardMultiplier);
            break;

        case EventType::TreasureCache:
            event.resourceRewards[ResourceType::Metal] = static_cast<int32_t>(40 * rewardMultiplier);
            event.resourceRewards[ResourceType::Electronics] = static_cast<int32_t>(20 * rewardMultiplier);
            event.resourceRewards[ResourceType::RareComponents] = static_cast<int32_t>(10 * rewardMultiplier);
            break;

        case EventType::WeaponCache:
            event.resourceRewards[ResourceType::Ammunition] = static_cast<int32_t>(100 * rewardMultiplier);
            event.itemRewards.push_back("weapon_assault_rifle");
            event.itemRewards.push_back("weapon_shotgun");
            break;

        case EventType::RefugeeCamp:
            event.experienceReward = static_cast<int32_t>(500 * rewardMultiplier);
            break;

        default:
            event.experienceReward = static_cast<int32_t>(100 * rewardMultiplier);
            break;
    }
}

void EventScheduler::ScaleEventForPlayers(WorldEvent& event, int32_t playerCount) {
    if (playerCount <= 1) return;

    // Scale intensity
    event.intensity += 0.1f * (playerCount - 1);

    // Scale rewards
    float rewardScale = 1.0f + 0.2f * (playerCount - 1);
    for (auto& [type, amount] : event.resourceRewards) {
        amount = static_cast<int32_t>(amount * rewardScale);
    }
    event.experienceReward = static_cast<int32_t>(event.experienceReward * rewardScale);

    // Scale radius slightly
    event.radius *= (1.0f + 0.1f * (playerCount - 1));
}

void EventScheduler::UpdateCooldowns(float deltaTime) {
    for (auto& [type, cooldown] : m_cooldowns) {
        cooldown = std::max(0.0f, cooldown - deltaTime);
    }
}

void EventScheduler::SetCooldown(EventType type) {
    const auto* config = GetEventConfig(type);
    if (config) {
        m_cooldowns[type] = config->minCooldownMinutes * 60.0f;
    }
}

void EventScheduler::PublishEventToFirebase(const WorldEvent& event) {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    std::string path = m_firebasePath + "/" + event.id;
    FirebaseManager::Instance().SetValue(path, event.ToJson());
}

void EventScheduler::RemoveEventFromFirebase(const std::string& eventId) {
    if (!FirebaseManager::Instance().IsInitialized()) return;

    std::string path = m_firebasePath + "/" + eventId;
    FirebaseManager::Instance().DeleteValue(path);
}

void EventScheduler::UpdateEventInFirebase(const WorldEvent& event) {
    PublishEventToFirebase(event);
}

void EventScheduler::InvokeScheduledCallbacks(const WorldEvent& event) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_scheduledCallbacks) {
        if (callback) callback(event);
    }
}

void EventScheduler::InvokeStartedCallbacks(const WorldEvent& event) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_startedCallbacks) {
        if (callback) callback(event);
    }
}

void EventScheduler::InvokeEndedCallbacks(const WorldEvent& event) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_endedCallbacks) {
        if (callback) callback(event);
    }
}

void EventScheduler::InvokeCancelledCallbacks(const WorldEvent& event) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& callback : m_cancelledCallbacks) {
        if (callback) callback(event);
    }
}

int64_t EventScheduler::GetCurrentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

} // namespace Vehement
