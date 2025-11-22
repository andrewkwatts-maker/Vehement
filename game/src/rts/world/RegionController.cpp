#include "RegionController.hpp"
#include <cmath>
#include <chrono>
#include <algorithm>

namespace Vehement {

// ============================================================================
// NPCSpawnPoint Implementation
// ============================================================================

nlohmann::json NPCSpawnPoint::ToJson() const {
    return {
        {"id", id},
        {"npcTypeId", npcTypeId},
        {"location", {{"lat", location.latitude}, {"lon", location.longitude}}},
        {"localPosition", {localPosition.x, localPosition.y, localPosition.z}},
        {"spawnRadius", spawnRadius},
        {"maxSpawned", maxSpawned},
        {"currentSpawned", currentSpawned},
        {"respawnTimeSeconds", respawnTimeSeconds},
        {"minLevel", minLevel},
        {"maxLevel", maxLevel},
        {"active", active},
        {"conditions", conditions}
    };
}

NPCSpawnPoint NPCSpawnPoint::FromJson(const nlohmann::json& j) {
    NPCSpawnPoint sp;
    sp.id = j.value("id", "");
    sp.npcTypeId = j.value("npcTypeId", "");

    if (j.contains("location")) {
        sp.location.latitude = j["location"].value("lat", 0.0);
        sp.location.longitude = j["location"].value("lon", 0.0);
    }
    if (j.contains("localPosition") && j["localPosition"].is_array() && j["localPosition"].size() >= 3) {
        sp.localPosition = glm::vec3(j["localPosition"][0], j["localPosition"][1], j["localPosition"][2]);
    }

    sp.spawnRadius = j.value("spawnRadius", 10.0f);
    sp.maxSpawned = j.value("maxSpawned", 5);
    sp.currentSpawned = j.value("currentSpawned", 0);
    sp.respawnTimeSeconds = j.value("respawnTimeSeconds", 300.0f);
    sp.minLevel = j.value("minLevel", 1);
    sp.maxLevel = j.value("maxLevel", 100);
    sp.active = j.value("active", true);

    if (j.contains("conditions") && j["conditions"].is_array()) {
        for (const auto& c : j["conditions"]) {
            sp.conditions.push_back(c.get<std::string>());
        }
    }

    return sp;
}

// ============================================================================
// RegionTimeOfDay Implementation
// ============================================================================

nlohmann::json RegionTimeOfDay::ToJson() const {
    return {
        {"hour", hour},
        {"minute", minute},
        {"dayProgress", dayProgress},
        {"isDaytime", isDaytime},
        {"sunAngle", sunAngle},
        {"ambientLight", ambientLight},
        {"sunColor", {sunColor.r, sunColor.g, sunColor.b}},
        {"ambientColor", {ambientColor.r, ambientColor.g, ambientColor.b}}
    };
}

RegionTimeOfDay RegionTimeOfDay::FromJson(const nlohmann::json& j) {
    RegionTimeOfDay t;
    t.hour = j.value("hour", 12.0f);
    t.minute = j.value("minute", 0.0f);
    t.dayProgress = j.value("dayProgress", 0.5f);
    t.isDaytime = j.value("isDaytime", true);
    t.sunAngle = j.value("sunAngle", 45.0f);
    t.ambientLight = j.value("ambientLight", 1.0f);

    auto loadVec3 = [&j](const std::string& key, glm::vec3 def) {
        if (j.contains(key) && j[key].is_array() && j[key].size() >= 3) {
            return glm::vec3(j[key][0], j[key][1], j[key][2]);
        }
        return def;
    };

    t.sunColor = loadVec3("sunColor", glm::vec3(1.0f, 0.95f, 0.9f));
    t.ambientColor = loadVec3("ambientColor", glm::vec3(0.4f, 0.45f, 0.5f));

    return t;
}

// ============================================================================
// RegionRules Implementation
// ============================================================================

nlohmann::json RegionRules::ToJson() const {
    return {
        {"pvpAllowed", pvpAllowed},
        {"damageMultiplier", damageMultiplier},
        {"healingMultiplier", healingMultiplier},
        {"friendlyFireEnabled", friendlyFireEnabled},
        {"deathPenalty", deathPenalty},
        {"deathPenaltyMultiplier", deathPenaltyMultiplier},
        {"resourceGatherMultiplier", resourceGatherMultiplier},
        {"tradingTaxPercent", tradingTaxPercent},
        {"buildingAllowed", buildingAllowed},
        {"buildingCostMultiplier", buildingCostMultiplier},
        {"buildingTimeMultiplier", buildingTimeMultiplier},
        {"movementSpeedMultiplier", movementSpeedMultiplier},
        {"mountsAllowed", mountsAllowed},
        {"flyingAllowed", flyingAllowed},
        {"teleportAllowed", teleportAllowed},
        {"experienceMultiplier", experienceMultiplier},
        {"levelCap", levelCap},
        {"isInstanced", isInstanced},
        {"maxPlayersPerInstance", maxPlayersPerInstance},
        {"allowGrouping", allowGrouping},
        {"maxGroupSize", maxGroupSize}
    };
}

RegionRules RegionRules::FromJson(const nlohmann::json& j) {
    RegionRules r;
    r.pvpAllowed = j.value("pvpAllowed", true);
    r.damageMultiplier = j.value("damageMultiplier", 1.0f);
    r.healingMultiplier = j.value("healingMultiplier", 1.0f);
    r.friendlyFireEnabled = j.value("friendlyFireEnabled", false);
    r.deathPenalty = j.value("deathPenalty", true);
    r.deathPenaltyMultiplier = j.value("deathPenaltyMultiplier", 1.0f);
    r.resourceGatherMultiplier = j.value("resourceGatherMultiplier", 1.0f);
    r.tradingTaxPercent = j.value("tradingTaxPercent", 0.0f);
    r.buildingAllowed = j.value("buildingAllowed", true);
    r.buildingCostMultiplier = j.value("buildingCostMultiplier", 1.0f);
    r.buildingTimeMultiplier = j.value("buildingTimeMultiplier", 1.0f);
    r.movementSpeedMultiplier = j.value("movementSpeedMultiplier", 1.0f);
    r.mountsAllowed = j.value("mountsAllowed", true);
    r.flyingAllowed = j.value("flyingAllowed", false);
    r.teleportAllowed = j.value("teleportAllowed", true);
    r.experienceMultiplier = j.value("experienceMultiplier", 1.0f);
    r.levelCap = j.value("levelCap", -1);
    r.isInstanced = j.value("isInstanced", false);
    r.maxPlayersPerInstance = j.value("maxPlayersPerInstance", 100);
    r.allowGrouping = j.value("allowGrouping", true);
    r.maxGroupSize = j.value("maxGroupSize", 10);
    return r;
}

// ============================================================================
// RegionalMilestone Implementation
// ============================================================================

nlohmann::json RegionalMilestone::ToJson() const {
    nlohmann::json rewardsJson;
    for (const auto& [k, v] : rewards) rewardsJson[k] = v;

    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"requirement", requirement},
        {"targetValue", targetValue},
        {"currentValue", currentValue},
        {"completed", completed},
        {"rewards", rewardsJson}
    };
}

RegionalMilestone RegionalMilestone::FromJson(const nlohmann::json& j) {
    RegionalMilestone m;
    m.id = j.value("id", "");
    m.name = j.value("name", "");
    m.description = j.value("description", "");
    m.requirement = j.value("requirement", "");
    m.targetValue = j.value("targetValue", 0);
    m.currentValue = j.value("currentValue", 0);
    m.completed = j.value("completed", false);

    if (j.contains("rewards") && j["rewards"].is_object()) {
        for (auto& [k, v] : j["rewards"].items()) {
            m.rewards[k] = v.get<int>();
        }
    }

    return m;
}

// ============================================================================
// RegionController Implementation
// ============================================================================

RegionController& RegionController::Instance() {
    static RegionController instance;
    return instance;
}

bool RegionController::Initialize(const RegionControllerConfig& config) {
    if (m_initialized) return true;

    m_config = config;
    m_currentTime.hour = 12.0f;
    m_currentTime.minute = 0.0f;
    m_currentTime.dayProgress = 0.5f;
    m_currentTime.isDaytime = true;
    m_initialized = true;

    return true;
}

void RegionController::Shutdown() {
    m_regionRules.clear();
    m_spawnPoints.clear();
    m_milestones.clear();
    m_activeRegionId.clear();
    m_initialized = false;
}

void RegionController::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateTimeOfDay(deltaTime);

    m_npcUpdateTimer += deltaTime;
    m_weatherUpdateTimer += deltaTime;

    if (m_npcUpdateTimer >= m_config.npcUpdateInterval) {
        UpdateNPCSpawning(m_npcUpdateTimer);
        m_npcUpdateTimer = 0.0f;
    }

    if (m_weatherUpdateTimer >= m_config.weatherUpdateInterval) {
        UpdateWeather(m_weatherUpdateTimer);
        m_weatherUpdateTimer = 0.0f;
    }

    CheckMilestones();
}

void RegionController::SetActiveRegion(const std::string& regionId) {
    m_activeRegionId = regionId;

    // Setup rules if not already set
    std::lock_guard<std::mutex> lock(m_rulesMutex);
    if (m_regionRules.find(regionId) == m_regionRules.end()) {
        m_regionRules[regionId] = RegionRules();
    }
}

const WorldRegion* RegionController::GetActiveRegion() const {
    if (m_activeRegionId.empty()) return nullptr;
    return RegionManager::Instance().GetRegion(m_activeRegionId);
}

bool RegionController::EnterRegion(const std::string& regionId, const std::string& playerId) {
    const auto* region = RegionManager::Instance().GetRegion(regionId);
    if (!region) return false;

    if (!region->accessible) return false;

    SetActiveRegion(regionId);
    RegionManager::Instance().DiscoverRegion(regionId, playerId);

    return true;
}

void RegionController::ExitRegion(const std::string& playerId) {
    m_activeRegionId.clear();
}

void RegionController::SetViewMode(RegionViewMode mode) {
    if (m_viewMode == mode) return;

    m_viewMode = mode;

    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (const auto& cb : m_viewModeCallbacks) {
        cb(mode);
    }
}

void RegionController::ToggleViewMode() {
    switch (m_viewMode) {
        case RegionViewMode::Local:
            SetViewMode(RegionViewMode::Regional);
            break;
        case RegionViewMode::Regional:
            SetViewMode(RegionViewMode::Global);
            break;
        case RegionViewMode::Global:
            SetViewMode(RegionViewMode::Local);
            break;
        default:
            SetViewMode(RegionViewMode::Local);
    }
}

RegionRules RegionController::GetRegionRules(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_rulesMutex);
    auto it = m_regionRules.find(regionId);
    if (it != m_regionRules.end()) {
        return it->second;
    }
    return RegionRules();
}

void RegionController::SetRegionRules(const std::string& regionId, const RegionRules& rules) {
    std::lock_guard<std::mutex> lock(m_rulesMutex);
    m_regionRules[regionId] = rules;
}

bool RegionController::IsActionAllowed(const std::string& action) const {
    auto rules = GetRegionRules(m_activeRegionId);

    if (action == "pvp") return rules.pvpAllowed;
    if (action == "build") return rules.buildingAllowed;
    if (action == "mount") return rules.mountsAllowed;
    if (action == "fly") return rules.flyingAllowed;
    if (action == "teleport") return rules.teleportAllowed;
    if (action == "group") return rules.allowGrouping;

    return true;
}

float RegionController::GetDamageMultiplier() const {
    auto rules = GetRegionRules(m_activeRegionId);
    const auto* region = GetActiveRegion();
    float mult = rules.damageMultiplier;
    if (region) {
        mult *= region->combatDifficultyMultiplier;
    }
    return mult;
}

float RegionController::GetExperienceMultiplier() const {
    auto rules = GetRegionRules(m_activeRegionId);
    const auto* region = GetActiveRegion();
    float mult = rules.experienceMultiplier;
    if (region) {
        mult *= region->experienceMultiplier;
    }
    return mult;
}

RegionTimeOfDay RegionController::GetTimeOfDay() const {
    return m_currentTime;
}

void RegionController::SetTimeOfDay(float hour, float minute) {
    m_currentTime.hour = std::fmod(hour, 24.0f);
    m_currentTime.minute = std::fmod(minute, 60.0f);
    m_currentTime.dayProgress = m_currentTime.hour / 24.0f;
    m_currentTime.isDaytime = (m_currentTime.hour >= 6.0f && m_currentTime.hour < 20.0f);

    // Calculate sun angle (0 at sunrise, 90 at noon, 180 at sunset)
    float dayHour = m_currentTime.hour - 6.0f;
    if (dayHour < 0) dayHour += 24.0f;
    m_currentTime.sunAngle = (dayHour / 14.0f) * 180.0f;  // 14 hours of daylight
    m_currentTime.sunAngle = std::clamp(m_currentTime.sunAngle, 0.0f, 180.0f);

    // Ambient light based on time
    if (m_currentTime.isDaytime) {
        float dayProgress = (m_currentTime.hour - 6.0f) / 14.0f;
        m_currentTime.ambientLight = 0.3f + 0.7f * std::sin(dayProgress * 3.14159f);
    } else {
        m_currentTime.ambientLight = 0.1f;
    }
}

bool RegionController::IsDaytime() const {
    return m_currentTime.isDaytime;
}

glm::vec3 RegionController::GetSunDirection() const {
    float radians = m_currentTime.sunAngle * 3.14159f / 180.0f;
    return glm::normalize(glm::vec3(std::cos(radians), std::sin(radians), 0.2f));
}

RegionWeather RegionController::GetCurrentWeather() const {
    const auto* region = GetActiveRegion();
    if (region) {
        return region->currentWeather;
    }
    return RegionWeather();
}

void RegionController::SetWeather(const RegionWeather& weather) {
    if (m_activeRegionId.empty()) return;
    RegionManager::Instance().SetRegionWeather(m_activeRegionId, weather);
}

float RegionController::GetWeatherVisibilityMultiplier() const {
    auto weather = GetCurrentWeather();
    return weather.visibility;
}

float RegionController::GetWeatherMovementMultiplier() const {
    auto weather = GetCurrentWeather();
    float mult = 1.0f;

    if (weather.type == "storm" || weather.type == "blizzard") {
        mult = 0.7f;
    } else if (weather.type == "rain" || weather.type == "snow") {
        mult = 0.85f;
    } else if (weather.type == "sandstorm") {
        mult = 0.6f;
    }

    return mult * (1.0f - weather.intensity * 0.3f);
}

void RegionController::RegisterSpawnPoint(const NPCSpawnPoint& spawn) {
    std::lock_guard<std::mutex> lock(m_spawnMutex);
    m_spawnPoints[m_activeRegionId].push_back(spawn);
}

std::vector<NPCSpawnPoint> RegionController::GetSpawnPoints(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_spawnMutex);
    auto it = m_spawnPoints.find(regionId);
    if (it != m_spawnPoints.end()) {
        return it->second;
    }
    return {};
}

void RegionController::ForceSpawn(const std::string& spawnPointId) {
    std::lock_guard<std::mutex> lock(m_spawnMutex);

    for (auto& [regionId, spawns] : m_spawnPoints) {
        for (auto& spawn : spawns) {
            if (spawn.id == spawnPointId && spawn.currentSpawned < spawn.maxSpawned) {
                spawn.currentSpawned++;

                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                for (const auto& cb : m_spawnCallbacks) {
                    cb(spawn.npcTypeId + "_" + std::to_string(spawn.currentSpawned), spawn);
                }
                return;
            }
        }
    }
}

void RegionController::ClearSpawnedNPCs(const std::string& regionId) {
    std::lock_guard<std::mutex> lock(m_spawnMutex);
    auto it = m_spawnPoints.find(regionId);
    if (it != m_spawnPoints.end()) {
        for (auto& spawn : it->second) {
            spawn.currentSpawned = 0;
        }
    }
}

void RegionController::OnNPCDeath(const std::string& spawnPointId) {
    std::lock_guard<std::mutex> lock(m_spawnMutex);

    for (auto& [regionId, spawns] : m_spawnPoints) {
        for (auto& spawn : spawns) {
            if (spawn.id == spawnPointId && spawn.currentSpawned > 0) {
                spawn.currentSpawned--;
                m_respawnTimers[spawnPointId] = spawn.respawnTimeSeconds;
                return;
            }
        }
    }
}

std::vector<ResourceNode> RegionController::GetVisibleResources() const {
    const auto* region = GetActiveRegion();
    if (!region) return {};

    std::vector<ResourceNode> visible;
    for (const auto& res : region->resources) {
        if (!res.depleted) {
            visible.push_back(res);
        }
    }
    return visible;
}

float RegionController::GetResourceAvailability(const std::string& resourceType) const {
    const auto* region = GetActiveRegion();
    if (!region) return 0.0f;

    float total = 0.0f;
    for (const auto& res : region->resources) {
        if (res.resourceType == resourceType) {
            total += res.currentYield;
        }
    }
    return total;
}

std::vector<RegionalMilestone> RegionController::GetMilestones(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_milestoneMutex);
    auto it = m_milestones.find(regionId);
    if (it != m_milestones.end()) {
        return it->second;
    }
    return {};
}

void RegionController::UpdateMilestoneProgress(const std::string& milestoneId, int progress) {
    std::lock_guard<std::mutex> lock(m_milestoneMutex);

    for (auto& [regionId, milestones] : m_milestones) {
        for (auto& milestone : milestones) {
            if (milestone.id == milestoneId && !milestone.completed) {
                milestone.currentValue = std::min(progress, milestone.targetValue);
                if (milestone.currentValue >= milestone.targetValue) {
                    milestone.completed = true;

                    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                    for (const auto& cb : m_milestoneCallbacks) {
                        cb(milestone);
                    }
                }
                return;
            }
        }
    }
}

bool RegionController::IsMilestoneComplete(const std::string& milestoneId) const {
    std::lock_guard<std::mutex> lock(m_milestoneMutex);

    for (const auto& [regionId, milestones] : m_milestones) {
        for (const auto& milestone : milestones) {
            if (milestone.id == milestoneId) {
                return milestone.completed;
            }
        }
    }
    return false;
}

void RegionController::OnViewModeChanged(ViewModeChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_viewModeCallbacks.push_back(std::move(callback));
}

void RegionController::OnTimeChanged(TimeChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_timeCallbacks.push_back(std::move(callback));
}

void RegionController::OnNPCSpawned(NPCSpawnedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_spawnCallbacks.push_back(std::move(callback));
}

void RegionController::OnMilestoneCompleted(MilestoneCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_milestoneCallbacks.push_back(std::move(callback));
}

void RegionController::UpdateTimeOfDay(float deltaTime) {
    const auto* region = GetActiveRegion();

    if (m_config.useRealWorldTime && region && region->usesRealTime) {
        m_currentTime = CalculateRealWorldTime(region->timeZoneOffset);
    } else {
        m_timeAccumulator += deltaTime;

        float gameSecondsPerRealSecond = 24.0f * 60.0f / m_config.dayNightCycleDuration;
        if (region) {
            gameSecondsPerRealSecond *= region->gameTimeMultiplier;
        }

        float minutesElapsed = (deltaTime * gameSecondsPerRealSecond) / 60.0f;
        float newMinute = m_currentTime.minute + minutesElapsed;

        while (newMinute >= 60.0f) {
            newMinute -= 60.0f;
            m_currentTime.hour += 1.0f;
        }

        while (m_currentTime.hour >= 24.0f) {
            m_currentTime.hour -= 24.0f;
        }

        m_currentTime.minute = newMinute;
        SetTimeOfDay(m_currentTime.hour, m_currentTime.minute);
    }

    // Notify time change every game minute
    if (static_cast<int>(m_currentTime.minute) != static_cast<int>(m_currentTime.minute + deltaTime)) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (const auto& cb : m_timeCallbacks) {
            cb(m_currentTime);
        }
    }
}

void RegionController::UpdateWeather(float deltaTime) {
    // Weather updates are handled by RegionManager
}

void RegionController::UpdateNPCSpawning(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_spawnMutex);

    // Update respawn timers
    std::vector<std::string> readyToSpawn;
    for (auto& [spawnId, timer] : m_respawnTimers) {
        timer -= deltaTime;
        if (timer <= 0) {
            readyToSpawn.push_back(spawnId);
        }
    }

    for (const auto& spawnId : readyToSpawn) {
        m_respawnTimers.erase(spawnId);
        ForceSpawn(spawnId);
    }
}

void RegionController::CheckMilestones() {
    // Milestone checks would be triggered by game events
}

RegionTimeOfDay RegionController::CalculateRealWorldTime(float timezoneOffset) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::gmtime(&time);

    RegionTimeOfDay tod;
    tod.hour = static_cast<float>(tm->tm_hour) + timezoneOffset;
    while (tod.hour < 0) tod.hour += 24.0f;
    while (tod.hour >= 24.0f) tod.hour -= 24.0f;

    tod.minute = static_cast<float>(tm->tm_min);
    tod.dayProgress = tod.hour / 24.0f;
    tod.isDaytime = (tod.hour >= 6.0f && tod.hour < 20.0f);

    return tod;
}

} // namespace Vehement
