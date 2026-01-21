#include "WorldEvent.hpp"
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <cmath>

namespace Vehement {

// ===========================================================================
// EventType utilities
// ===========================================================================

const char* EventTypeToString(EventType type) noexcept {
    switch (type) {
        // Threats
        case EventType::ZombieHorde:    return "ZombieHorde";
        case EventType::BossZombie:     return "BossZombie";
        case EventType::Plague:         return "Plague";
        case EventType::Infestation:    return "Infestation";
        case EventType::NightTerror:    return "NightTerror";

        // Opportunities
        case EventType::SupplyDrop:     return "SupplyDrop";
        case EventType::RefugeeCamp:    return "RefugeeCamp";
        case EventType::TreasureCache:  return "TreasureCache";
        case EventType::AbandonedBase:  return "AbandonedBase";
        case EventType::WeaponCache:    return "WeaponCache";

        // Environmental
        case EventType::Storm:          return "Storm";
        case EventType::Earthquake:     return "Earthquake";
        case EventType::Drought:        return "Drought";
        case EventType::Bountiful:      return "Bountiful";
        case EventType::Fog:            return "Fog";
        case EventType::HeatWave:       return "HeatWave";

        // Social
        case EventType::TradeCaravan:   return "TradeCaravan";
        case EventType::MilitaryAid:    return "MilitaryAid";
        case EventType::Bandits:        return "Bandits";
        case EventType::Deserters:      return "Deserters";
        case EventType::Merchant:       return "Merchant";

        // Global
        case EventType::BloodMoon:      return "BloodMoon";
        case EventType::Eclipse:        return "Eclipse";
        case EventType::GoldenAge:      return "GoldenAge";
        case EventType::Apocalypse:     return "Apocalypse";
        case EventType::Ceasefire:      return "Ceasefire";
        case EventType::DoubleXP:       return "DoubleXP";

        default:                        return "Unknown";
    }
}

std::optional<EventType> StringToEventType(std::string_view str) noexcept {
    // Threats
    if (str == "ZombieHorde")   return EventType::ZombieHorde;
    if (str == "BossZombie")    return EventType::BossZombie;
    if (str == "Plague")        return EventType::Plague;
    if (str == "Infestation")   return EventType::Infestation;
    if (str == "NightTerror")   return EventType::NightTerror;

    // Opportunities
    if (str == "SupplyDrop")    return EventType::SupplyDrop;
    if (str == "RefugeeCamp")   return EventType::RefugeeCamp;
    if (str == "TreasureCache") return EventType::TreasureCache;
    if (str == "AbandonedBase") return EventType::AbandonedBase;
    if (str == "WeaponCache")   return EventType::WeaponCache;

    // Environmental
    if (str == "Storm")         return EventType::Storm;
    if (str == "Earthquake")    return EventType::Earthquake;
    if (str == "Drought")       return EventType::Drought;
    if (str == "Bountiful")     return EventType::Bountiful;
    if (str == "Fog")           return EventType::Fog;
    if (str == "HeatWave")      return EventType::HeatWave;

    // Social
    if (str == "TradeCaravan")  return EventType::TradeCaravan;
    if (str == "MilitaryAid")   return EventType::MilitaryAid;
    if (str == "Bandits")       return EventType::Bandits;
    if (str == "Deserters")     return EventType::Deserters;
    if (str == "Merchant")      return EventType::Merchant;

    // Global
    if (str == "BloodMoon")     return EventType::BloodMoon;
    if (str == "Eclipse")       return EventType::Eclipse;
    if (str == "GoldenAge")     return EventType::GoldenAge;
    if (str == "Apocalypse")    return EventType::Apocalypse;
    if (str == "Ceasefire")     return EventType::Ceasefire;
    if (str == "DoubleXP")      return EventType::DoubleXP;

    return std::nullopt;
}

EventCategory GetEventCategory(EventType type) noexcept {
    switch (type) {
        case EventType::ZombieHorde:
        case EventType::BossZombie:
        case EventType::Plague:
        case EventType::Infestation:
        case EventType::NightTerror:
            return EventCategory::Threat;

        case EventType::SupplyDrop:
        case EventType::RefugeeCamp:
        case EventType::TreasureCache:
        case EventType::AbandonedBase:
        case EventType::WeaponCache:
            return EventCategory::Opportunity;

        case EventType::Storm:
        case EventType::Earthquake:
        case EventType::Drought:
        case EventType::Bountiful:
        case EventType::Fog:
        case EventType::HeatWave:
            return EventCategory::Environmental;

        case EventType::TradeCaravan:
        case EventType::MilitaryAid:
        case EventType::Bandits:
        case EventType::Deserters:
        case EventType::Merchant:
            return EventCategory::Social;

        case EventType::BloodMoon:
        case EventType::Eclipse:
        case EventType::GoldenAge:
        case EventType::Apocalypse:
        case EventType::Ceasefire:
        case EventType::DoubleXP:
            return EventCategory::Global;

        default:
            return EventCategory::Environmental;
    }
}

EventSeverity GetDefaultSeverity(EventType type) noexcept {
    switch (type) {
        // Critical threats
        case EventType::Apocalypse:
        case EventType::NightTerror:
            return EventSeverity::Catastrophic;

        // Major threats/events
        case EventType::ZombieHorde:
        case EventType::BossZombie:
        case EventType::Earthquake:
        case EventType::BloodMoon:
            return EventSeverity::Major;

        // Moderate events
        case EventType::Plague:
        case EventType::Infestation:
        case EventType::Storm:
        case EventType::Bandits:
        case EventType::Drought:
            return EventSeverity::Moderate;

        // Minor events
        case EventType::SupplyDrop:
        case EventType::TreasureCache:
        case EventType::TradeCaravan:
        case EventType::Fog:
        case EventType::HeatWave:
        case EventType::Merchant:
        case EventType::DoubleXP:
            return EventSeverity::Minor;

        // Everything else
        default:
            return EventSeverity::Moderate;
    }
}

const char* ResourceTypeToString(ResourceType type) noexcept {
    switch (type) {
        case ResourceType::Food:            return "Food";
        case ResourceType::Water:           return "Water";
        case ResourceType::Wood:            return "Wood";
        case ResourceType::Stone:           return "Stone";
        case ResourceType::Metal:           return "Metal";
        case ResourceType::Fuel:            return "Fuel";
        case ResourceType::Medicine:        return "Medicine";
        case ResourceType::Ammunition:      return "Ammunition";
        case ResourceType::Electronics:     return "Electronics";
        case ResourceType::RareComponents:  return "RareComponents";
        default:                            return "Unknown";
    }
}

// ===========================================================================
// WorldEvent implementation
// ===========================================================================

WorldEvent::WorldEvent()
    : type(EventType::SupplyDrop)
    , location(0.0f, 0.0f)
    , radius(100.0f)
    , isGlobal(false)
    , scheduledTime(0)
    , startTime(0)
    , endTime(0)
    , warningTime(0)
    , isActive(false)
    , isCompleted(false)
    , wasCancelled(false)
    , intensity(1.0f)
    , difficultyTier(1)
    , playerScaling(1)
    , experienceReward(0) {
}

WorldEvent::WorldEvent(EventType eventType, const std::string& eventName,
                       const glm::vec2& pos, float eventRadius)
    : type(eventType)
    , name(eventName)
    , location(pos)
    , radius(eventRadius)
    , isGlobal(GetEventCategory(eventType) == EventCategory::Global)
    , scheduledTime(0)
    , startTime(0)
    , endTime(0)
    , warningTime(0)
    , isActive(false)
    , isCompleted(false)
    , wasCancelled(false)
    , intensity(1.0f)
    , difficultyTier(1)
    , playerScaling(1)
    , experienceReward(0) {
}

bool WorldEvent::IsCurrentlyActive(int64_t currentTimeMs) const noexcept {
    return !wasCancelled && !isCompleted &&
           currentTimeMs >= startTime && currentTimeMs < endTime;
}

bool WorldEvent::HasExpired(int64_t currentTimeMs) const noexcept {
    return isCompleted || wasCancelled || currentTimeMs >= endTime;
}

bool WorldEvent::ShouldShowWarning(int64_t currentTimeMs) const noexcept {
    return !isActive && !isCompleted && !wasCancelled &&
           currentTimeMs >= warningTime && currentTimeMs < startTime;
}

int64_t WorldEvent::GetRemainingDuration(int64_t currentTimeMs) const noexcept {
    if (currentTimeMs >= endTime) return 0;
    if (currentTimeMs < startTime) return endTime - startTime;
    return endTime - currentTimeMs;
}

int64_t WorldEvent::GetTimeUntilStart(int64_t currentTimeMs) const noexcept {
    if (currentTimeMs >= startTime) return 0;
    return startTime - currentTimeMs;
}

float WorldEvent::GetProgress(int64_t currentTimeMs) const noexcept {
    if (currentTimeMs <= startTime) return 0.0f;
    if (currentTimeMs >= endTime) return 1.0f;

    int64_t duration = endTime - startTime;
    if (duration <= 0) return 1.0f;

    int64_t elapsed = currentTimeMs - startTime;
    return static_cast<float>(elapsed) / static_cast<float>(duration);
}

bool WorldEvent::IsPositionAffected(const glm::vec2& pos) const noexcept {
    if (isGlobal) return true;
    return GetDistanceToCenter(pos) <= radius;
}

float WorldEvent::GetDistanceToCenter(const glm::vec2& pos) const noexcept {
    glm::vec2 diff = pos - location;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

nlohmann::json WorldEvent::ToJson() const {
    nlohmann::json j;

    j["id"] = id;
    j["type"] = EventTypeToString(type);
    j["name"] = name;
    j["description"] = description;

    j["location"] = {{"x", location.x}, {"y", location.y}};
    j["radius"] = radius;
    j["isGlobal"] = isGlobal;
    j["regionId"] = regionId;

    j["scheduledTime"] = scheduledTime;
    j["startTime"] = startTime;
    j["endTime"] = endTime;
    j["warningTime"] = warningTime;

    j["isActive"] = isActive;
    j["isCompleted"] = isCompleted;
    j["wasCancelled"] = wasCancelled;

    j["affectedPlayers"] = affectedPlayers;
    j["participatingPlayers"] = participatingPlayers;
    j["initiatorPlayerId"] = initiatorPlayerId;

    j["intensity"] = intensity;
    j["difficultyTier"] = difficultyTier;
    j["playerScaling"] = playerScaling;

    // Serialize resource rewards
    nlohmann::json rewards = nlohmann::json::object();
    for (const auto& [resType, amount] : resourceRewards) {
        rewards[ResourceTypeToString(resType)] = amount;
    }
    j["resourceRewards"] = rewards;
    j["experienceReward"] = experienceReward;
    j["itemRewards"] = itemRewards;

    j["customData"] = customData;

    return j;
}

WorldEvent WorldEvent::FromJson(const nlohmann::json& j) {
    WorldEvent event;

    event.id = j.value("id", "");
    if (auto typeOpt = StringToEventType(j.value("type", ""))) {
        event.type = *typeOpt;
    }
    event.name = j.value("name", "");
    event.description = j.value("description", "");

    if (j.contains("location")) {
        event.location.x = j["location"].value("x", 0.0f);
        event.location.y = j["location"].value("y", 0.0f);
    }
    event.radius = j.value("radius", 100.0f);
    event.isGlobal = j.value("isGlobal", false);
    event.regionId = j.value("regionId", "");

    event.scheduledTime = j.value("scheduledTime", int64_t(0));
    event.startTime = j.value("startTime", int64_t(0));
    event.endTime = j.value("endTime", int64_t(0));
    event.warningTime = j.value("warningTime", int64_t(0));

    event.isActive = j.value("isActive", false);
    event.isCompleted = j.value("isCompleted", false);
    event.wasCancelled = j.value("wasCancelled", false);

    if (j.contains("affectedPlayers")) {
        event.affectedPlayers = j["affectedPlayers"].get<std::vector<std::string>>();
    }
    if (j.contains("participatingPlayers")) {
        event.participatingPlayers = j["participatingPlayers"].get<std::vector<std::string>>();
    }
    event.initiatorPlayerId = j.value("initiatorPlayerId", "");

    event.intensity = j.value("intensity", 1.0f);
    event.difficultyTier = j.value("difficultyTier", 1);
    event.playerScaling = j.value("playerScaling", 1);

    // Parse resource rewards
    if (j.contains("resourceRewards") && j["resourceRewards"].is_object()) {
        for (const auto& [key, value] : j["resourceRewards"].items()) {
            for (int i = 0; i < static_cast<int>(ResourceType::Count); ++i) {
                auto resType = static_cast<ResourceType>(i);
                if (key == ResourceTypeToString(resType)) {
                    event.resourceRewards[resType] = value.get<int32_t>();
                    break;
                }
            }
        }
    }
    event.experienceReward = j.value("experienceReward", 0);
    if (j.contains("itemRewards")) {
        event.itemRewards = j["itemRewards"].get<std::vector<std::string>>();
    }

    if (j.contains("customData")) {
        event.customData = j["customData"];
    }

    return event;
}

std::string WorldEvent::GenerateEventId(EventType type, int64_t timestamp) {
    std::stringstream ss;
    ss << EventTypeToString(type) << "_" << std::hex << timestamp;

    // Add random suffix
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);
    ss << "_" << std::setfill('0') << std::setw(4) << dis(gen);

    return ss.str();
}

// ===========================================================================
// EventModifier implementation
// ===========================================================================

nlohmann::json EventModifier::ToJson() const {
    return {
        {"type", static_cast<int>(type)},
        {"value", value},
        {"isPercentage", isPercentage},
        {"targetTag", targetTag}
    };
}

EventModifier EventModifier::FromJson(const nlohmann::json& j) {
    EventModifier mod;
    mod.type = static_cast<ModifierType>(j.value("type", 0));
    mod.value = j.value("value", 1.0f);
    mod.isPercentage = j.value("isPercentage", false);
    mod.targetTag = j.value("targetTag", "");
    return mod;
}

// ===========================================================================
// EventSpawnPoint implementation
// ===========================================================================

nlohmann::json EventSpawnPoint::ToJson() const {
    return {
        {"position", {{"x", position.x}, {"y", position.y}}},
        {"entityType", entityType},
        {"count", count},
        {"delay", delay},
        {"hasSpawned", hasSpawned}
    };
}

EventSpawnPoint EventSpawnPoint::FromJson(const nlohmann::json& j) {
    EventSpawnPoint sp;
    if (j.contains("position")) {
        sp.position.x = j["position"].value("x", 0.0f);
        sp.position.y = j["position"].value("y", 0.0f);
    }
    sp.entityType = j.value("entityType", "");
    sp.count = j.value("count", 1);
    sp.delay = j.value("delay", 0.0f);
    sp.hasSpawned = j.value("hasSpawned", false);
    return sp;
}

// ===========================================================================
// EventObjective implementation
// ===========================================================================

nlohmann::json EventObjective::ToJson() const {
    nlohmann::json j = {
        {"id", id},
        {"description", description},
        {"type", static_cast<int>(type)},
        {"targetValue", targetValue},
        {"currentValue", currentValue},
        {"isCompleted", isCompleted},
        {"isFailed", isFailed},
        {"isOptional", isOptional},
        {"bonusExperience", bonusExperience}
    };

    nlohmann::json rewardsJson = nlohmann::json::object();
    for (const auto& [resType, amount] : rewards) {
        rewardsJson[ResourceTypeToString(resType)] = amount;
    }
    j["rewards"] = rewardsJson;

    return j;
}

EventObjective EventObjective::FromJson(const nlohmann::json& j) {
    EventObjective obj;
    obj.id = j.value("id", "");
    obj.description = j.value("description", "");
    obj.type = static_cast<EventObjective::ObjectiveType>(j.value("type", 0));
    obj.targetValue = j.value("targetValue", 0);
    obj.currentValue = j.value("currentValue", 0);
    obj.isCompleted = j.value("isCompleted", false);
    obj.isFailed = j.value("isFailed", false);
    obj.isOptional = j.value("isOptional", false);
    obj.bonusExperience = j.value("bonusExperience", 0);

    if (j.contains("rewards") && j["rewards"].is_object()) {
        for (const auto& [key, value] : j["rewards"].items()) {
            for (int i = 0; i < static_cast<int>(ResourceType::Count); ++i) {
                auto resType = static_cast<ResourceType>(i);
                if (key == ResourceTypeToString(resType)) {
                    obj.rewards[resType] = value.get<int32_t>();
                    break;
                }
            }
        }
    }

    return obj;
}

// ===========================================================================
// EventTemplate implementation
// ===========================================================================

WorldEvent EventTemplate::CreateEvent(const glm::vec2& location,
                                       int32_t playerCount) const {
    WorldEvent event;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    event.id = WorldEvent::GenerateEventId(type, timestamp);
    event.type = type;

    // Replace placeholders in name and description
    std::string processedName = nameTemplate;
    std::string processedDesc = descriptionTemplate;

    // Replace {name} with the event type name
    std::string eventTypeName = EventTypeToString(type);
    size_t pos;
    while ((pos = processedName.find("{name}")) != std::string::npos) {
        processedName.replace(pos, 6, eventTypeName);
    }
    while ((pos = processedDesc.find("{name}")) != std::string::npos) {
        processedDesc.replace(pos, 6, eventTypeName);
    }

    // Replace {location} with coordinates string
    std::stringstream locStream;
    locStream << "(" << static_cast<int>(location.x) << ", " << static_cast<int>(location.y) << ")";
    std::string locationStr = locStream.str();
    while ((pos = processedName.find("{location}")) != std::string::npos) {
        processedName.replace(pos, 10, locationStr);
    }
    while ((pos = processedDesc.find("{location}")) != std::string::npos) {
        processedDesc.replace(pos, 10, locationStr);
    }

    event.name = processedName;
    event.description = processedDesc;

    event.location = location;
    event.isGlobal = canBeGlobal;

    // Randomize radius within range
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> radiusDist(minRadius, maxRadius);
    event.radius = radiusDist(gen);

    // Randomize duration within range
    std::uniform_int_distribution<int64_t> durationDist(minDurationMs, maxDurationMs);
    int64_t duration = durationDist(gen);

    event.scheduledTime = timestamp;
    event.warningTime = timestamp + warningLeadTimeMs - warningLeadTimeMs; // Warning now
    event.startTime = timestamp + warningLeadTimeMs;
    event.endTime = event.startTime + duration;

    event.isActive = false;
    event.isCompleted = false;
    event.wasCancelled = false;

    // Scale intensity by player count
    event.intensity = baseIntensity + (intensityPerPlayer * (playerCount - 1));
    event.difficultyTier = static_cast<int32_t>(baseSeverity) + 1;
    event.playerScaling = playerCount;

    return event;
}

nlohmann::json EventTemplate::ToJson() const {
    nlohmann::json j = {
        {"type", EventTypeToString(type)},
        {"nameTemplate", nameTemplate},
        {"descriptionTemplate", descriptionTemplate},
        {"minRadius", minRadius},
        {"maxRadius", maxRadius},
        {"minDurationMs", minDurationMs},
        {"maxDurationMs", maxDurationMs},
        {"warningLeadTimeMs", warningLeadTimeMs},
        {"baseSeverity", static_cast<int>(baseSeverity)},
        {"canBeGlobal", canBeGlobal},
        {"baseIntensity", baseIntensity},
        {"intensityPerPlayer", intensityPerPlayer},
        {"rewardPerPlayer", rewardPerPlayer}
    };

    nlohmann::json modsJson = nlohmann::json::array();
    for (const auto& mod : modifiers) {
        modsJson.push_back(mod.ToJson());
    }
    j["modifiers"] = modsJson;

    nlohmann::json spawnsJson = nlohmann::json::array();
    for (const auto& sp : spawnPoints) {
        spawnsJson.push_back(sp.ToJson());
    }
    j["spawnPoints"] = spawnsJson;

    nlohmann::json objsJson = nlohmann::json::array();
    for (const auto& obj : objectives) {
        objsJson.push_back(obj.ToJson());
    }
    j["objectives"] = objsJson;

    return j;
}

EventTemplate EventTemplate::FromJson(const nlohmann::json& j) {
    EventTemplate tmpl;

    if (auto typeOpt = StringToEventType(j.value("type", ""))) {
        tmpl.type = *typeOpt;
    }
    tmpl.nameTemplate = j.value("nameTemplate", "");
    tmpl.descriptionTemplate = j.value("descriptionTemplate", "");
    tmpl.minRadius = j.value("minRadius", 50.0f);
    tmpl.maxRadius = j.value("maxRadius", 200.0f);
    tmpl.minDurationMs = j.value("minDurationMs", int64_t(60000));
    tmpl.maxDurationMs = j.value("maxDurationMs", int64_t(300000));
    tmpl.warningLeadTimeMs = j.value("warningLeadTimeMs", int64_t(30000));
    tmpl.baseSeverity = static_cast<EventSeverity>(j.value("baseSeverity", 1));
    tmpl.canBeGlobal = j.value("canBeGlobal", false);
    tmpl.baseIntensity = j.value("baseIntensity", 1.0f);
    tmpl.intensityPerPlayer = j.value("intensityPerPlayer", 0.1f);
    tmpl.rewardPerPlayer = j.value("rewardPerPlayer", 0.2f);

    if (j.contains("modifiers") && j["modifiers"].is_array()) {
        for (const auto& modJson : j["modifiers"]) {
            tmpl.modifiers.push_back(EventModifier::FromJson(modJson));
        }
    }

    if (j.contains("spawnPoints") && j["spawnPoints"].is_array()) {
        for (const auto& spJson : j["spawnPoints"]) {
            tmpl.spawnPoints.push_back(EventSpawnPoint::FromJson(spJson));
        }
    }

    if (j.contains("objectives") && j["objectives"].is_array()) {
        for (const auto& objJson : j["objectives"]) {
            tmpl.objectives.push_back(EventObjective::FromJson(objJson));
        }
    }

    return tmpl;
}

} // namespace Vehement
