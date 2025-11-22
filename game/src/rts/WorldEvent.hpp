#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <optional>
#include <functional>
#include <chrono>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Types of world events that can occur in the game
 *
 * Events are categorized into:
 * - Threats: Dangerous situations players must handle
 * - Opportunities: Beneficial events players can exploit
 * - Environmental: Weather/natural events affecting gameplay
 * - Social: NPC-related events
 * - Global: Server-wide events affecting all players
 */
enum class EventType : uint8_t {
    // ===== Threats =====
    ZombieHorde,        ///< Massive zombie attack on a region
    BossZombie,         ///< Powerful boss zombie spawns
    Plague,             ///< Disease spreads, affects worker efficiency
    Infestation,        ///< Buildings become infested with zombies
    NightTerror,        ///< Extremely dangerous night-time event

    // ===== Opportunities =====
    SupplyDrop,         ///< Resources appear on the map
    RefugeeCamp,        ///< Many NPCs available to recruit
    TreasureCache,      ///< Valuable loot discovered
    AbandonedBase,      ///< Claimable structure with resources
    WeaponCache,        ///< Military weapons and ammo

    // ===== Environmental =====
    Storm,              ///< Reduces vision, slows movement
    Earthquake,         ///< Damages buildings in affected area
    Drought,            ///< Reduces farm and water output
    Bountiful,          ///< Increases all resource production
    Fog,                ///< Severely reduced visibility
    HeatWave,           ///< Units overheat, reduced stamina

    // ===== Social =====
    TradeCaravan,       ///< NPC traders arrive with goods
    MilitaryAid,        ///< NPC soldiers help defend
    Bandits,            ///< Hostile NPCs attack players
    Deserters,          ///< Enemy soldiers willing to defect
    Merchant,           ///< Rare items available for trade

    // ===== Global =====
    BloodMoon,          ///< All zombies become stronger
    Eclipse,            ///< Extended darkness, longer night
    GoldenAge,          ///< All players get production bonuses
    Apocalypse,         ///< Massive multi-wave zombie assault
    Ceasefire,          ///< PvP disabled, focus on survival
    DoubleXP,           ///< Experience points doubled

    Count               ///< Total number of event types
};

/**
 * @brief Get string representation of event type
 */
[[nodiscard]] const char* EventTypeToString(EventType type) noexcept;

/**
 * @brief Parse event type from string
 */
[[nodiscard]] std::optional<EventType> StringToEventType(std::string_view str) noexcept;

/**
 * @brief Category of an event
 */
enum class EventCategory : uint8_t {
    Threat,
    Opportunity,
    Environmental,
    Social,
    Global
};

/**
 * @brief Get the category for an event type
 */
[[nodiscard]] EventCategory GetEventCategory(EventType type) noexcept;

/**
 * @brief Severity level of an event
 */
enum class EventSeverity : uint8_t {
    Minor,      ///< Small impact, can be ignored
    Moderate,   ///< Noticeable impact on gameplay
    Major,      ///< Significant impact, should respond
    Critical,   ///< Severe impact, must respond
    Catastrophic ///< Game-changing event
};

/**
 * @brief Get default severity for an event type
 */
[[nodiscard]] EventSeverity GetDefaultSeverity(EventType type) noexcept;

/**
 * @brief Resource types used in events
 */
enum class ResourceType : uint8_t {
    Food,
    Water,
    Wood,
    Stone,
    Metal,
    Fuel,
    Medicine,
    Ammunition,
    Electronics,
    RareComponents,
    Count
};

/**
 * @brief Get string representation of resource type
 */
[[nodiscard]] const char* ResourceTypeToString(ResourceType type) noexcept;

/**
 * @brief Core world event data structure
 *
 * Represents a single event that occurs in the game world.
 * Events are synchronized across all players via Firebase.
 */
struct WorldEvent {
    // Identification
    std::string id;                 ///< Unique event ID (Firebase key)
    EventType type;                 ///< Type of event
    std::string name;               ///< Display name
    std::string description;        ///< Detailed description

    // Location and area
    glm::vec2 location;             ///< Center point of the event
    float radius;                   ///< Affected area radius in world units
    bool isGlobal;                  ///< If true, affects entire server
    std::string regionId;           ///< Region identifier (if regional)

    // Timing
    int64_t scheduledTime;          ///< When event was scheduled (Unix ms)
    int64_t startTime;              ///< When event starts (Unix ms)
    int64_t endTime;                ///< When event ends (Unix ms)
    int64_t warningTime;            ///< When players should be warned (Unix ms)

    // State
    bool isActive;                  ///< Currently active
    bool isCompleted;               ///< Has finished
    bool wasCancelled;              ///< Was cancelled before completion

    // Participation
    std::vector<std::string> affectedPlayers;   ///< Players in affected area
    std::vector<std::string> participatingPlayers; ///< Players actively participating
    std::string initiatorPlayerId;  ///< Player who triggered event (if any)

    // Scaling
    float intensity;                ///< Event intensity multiplier (0.5 - 2.0)
    int32_t difficultyTier;         ///< Difficulty tier (1-5)
    int32_t playerScaling;          ///< Number of players event scales for

    // Rewards (for opportunity events)
    std::map<ResourceType, int32_t> resourceRewards;
    int32_t experienceReward;
    std::vector<std::string> itemRewards;

    // Custom data for specific event types
    nlohmann::json customData;

    /**
     * @brief Default constructor
     */
    WorldEvent();

    /**
     * @brief Construct with type and basic info
     */
    WorldEvent(EventType eventType, const std::string& eventName,
               const glm::vec2& pos, float eventRadius);

    /**
     * @brief Check if event is currently active based on time
     */
    [[nodiscard]] bool IsCurrentlyActive(int64_t currentTimeMs) const noexcept;

    /**
     * @brief Check if event has expired
     */
    [[nodiscard]] bool HasExpired(int64_t currentTimeMs) const noexcept;

    /**
     * @brief Check if warning should be shown
     */
    [[nodiscard]] bool ShouldShowWarning(int64_t currentTimeMs) const noexcept;

    /**
     * @brief Get remaining duration in milliseconds
     */
    [[nodiscard]] int64_t GetRemainingDuration(int64_t currentTimeMs) const noexcept;

    /**
     * @brief Get time until event starts in milliseconds
     */
    [[nodiscard]] int64_t GetTimeUntilStart(int64_t currentTimeMs) const noexcept;

    /**
     * @brief Get progress through the event (0.0 to 1.0)
     */
    [[nodiscard]] float GetProgress(int64_t currentTimeMs) const noexcept;

    /**
     * @brief Check if a position is within the event area
     */
    [[nodiscard]] bool IsPositionAffected(const glm::vec2& pos) const noexcept;

    /**
     * @brief Get distance from position to event center
     */
    [[nodiscard]] float GetDistanceToCenter(const glm::vec2& pos) const noexcept;

    /**
     * @brief Serialize to JSON for Firebase
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static WorldEvent FromJson(const nlohmann::json& json);

    /**
     * @brief Create event ID from type and timestamp
     */
    static std::string GenerateEventId(EventType type, int64_t timestamp);
};

/**
 * @brief Modifier effects that events can apply
 */
struct EventModifier {
    enum class ModifierType : uint8_t {
        // Unit modifiers
        MovementSpeed,
        AttackDamage,
        AttackSpeed,
        Defense,
        MaxHealth,
        HealthRegen,
        VisionRange,
        DetectionRange,

        // Resource modifiers
        GatheringSpeed,
        ProductionSpeed,
        ConsumptionRate,

        // Building modifiers
        BuildSpeed,
        RepairSpeed,
        BuildingHealth,

        // Global modifiers
        ExperienceGain,
        LootQuality,
        SpawnRate
    };

    ModifierType type;
    float value;            ///< Multiplier (1.0 = no change)
    bool isPercentage;      ///< If true, value is percentage change
    std::string targetTag;  ///< Optional: only affects tagged entities

    [[nodiscard]] nlohmann::json ToJson() const;
    static EventModifier FromJson(const nlohmann::json& json);
};

/**
 * @brief Active effect instance tracking
 */
struct ActiveEffect {
    std::string eventId;            ///< Source event ID
    EventType eventType;            ///< Type of source event
    EventModifier modifier;         ///< The modifier being applied
    int64_t startTime;              ///< When effect started
    int64_t endTime;                ///< When effect ends
    std::string targetId;           ///< Entity/building affected (empty = all)

    [[nodiscard]] bool IsExpired(int64_t currentTimeMs) const noexcept {
        return currentTimeMs >= endTime;
    }

    [[nodiscard]] float GetRemainingDuration(int64_t currentTimeMs) const noexcept {
        return static_cast<float>(endTime - currentTimeMs) / 1000.0f;
    }
};

/**
 * @brief Event spawn point for zombie/NPC events
 */
struct EventSpawnPoint {
    glm::vec2 position;
    std::string entityType;         ///< What to spawn
    int32_t count;                  ///< How many to spawn
    float delay;                    ///< Delay before spawning (seconds)
    bool hasSpawned;                ///< Already spawned

    [[nodiscard]] nlohmann::json ToJson() const;
    static EventSpawnPoint FromJson(const nlohmann::json& json);
};

/**
 * @brief Event objective for participation tracking
 */
struct EventObjective {
    std::string id;
    std::string description;

    enum class ObjectiveType : uint8_t {
        KillCount,          ///< Kill X enemies
        SurviveTime,        ///< Survive for X seconds
        CollectResources,   ///< Collect X resources
        DefendLocation,     ///< Prevent enemies from reaching location
        EscortNPC,          ///< Keep NPC alive
        DestroyTarget,      ///< Destroy specific target
        CapturePoint,       ///< Capture/hold location
        Custom              ///< Custom objective logic
    };

    ObjectiveType type;
    int32_t targetValue;            ///< Target to reach
    int32_t currentValue;           ///< Current progress
    bool isCompleted;
    bool isFailed;
    bool isOptional;                ///< Optional bonus objective

    // Rewards for completing this objective
    std::map<ResourceType, int32_t> rewards;
    int32_t bonusExperience;

    [[nodiscard]] float GetProgress() const noexcept {
        if (targetValue <= 0) return isCompleted ? 1.0f : 0.0f;
        return static_cast<float>(currentValue) / static_cast<float>(targetValue);
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static EventObjective FromJson(const nlohmann::json& json);
};

/**
 * @brief Template for creating events
 */
struct EventTemplate {
    EventType type;
    std::string nameTemplate;       ///< Name with placeholders
    std::string descriptionTemplate;

    float minRadius;
    float maxRadius;
    int64_t minDurationMs;
    int64_t maxDurationMs;
    int64_t warningLeadTimeMs;      ///< How long before start to warn

    EventSeverity baseSeverity;
    bool canBeGlobal;
    float baseIntensity;

    std::vector<EventModifier> modifiers;
    std::vector<EventSpawnPoint> spawnPoints;
    std::vector<EventObjective> objectives;

    // Scaling parameters
    float intensityPerPlayer;       ///< Intensity increase per player
    float rewardPerPlayer;          ///< Reward increase per player

    /**
     * @brief Create a world event from this template
     */
    [[nodiscard]] WorldEvent CreateEvent(const glm::vec2& location,
                                         int32_t playerCount = 1) const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static EventTemplate FromJson(const nlohmann::json& json);
};

/**
 * @brief Callback types for event system
 */
using EventCallback = std::function<void(const WorldEvent&)>;
using EventCompletionCallback = std::function<void(const WorldEvent&, bool success)>;

} // namespace Vehement
