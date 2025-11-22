#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace Vehement {

// Forward declarations
class TileMap;
class Entity;

/**
 * @brief Zone type enumeration
 */
enum class ZoneType : uint8_t {
    SafeZone,       ///< No PvP, no hostile spawns
    PvPZone,        ///< PvP enabled
    ResourceZone,   ///< Enhanced resource spawns
    DangerZone,     ///< Increased enemy spawns/difficulty
    QuestZone,      ///< Quest-related area
    TownZone,       ///< NPC town area
    WildernessZone, ///< Standard wilderness
    BossZone,       ///< Boss encounter area
    EventZone       ///< Special event area
};

/**
 * @brief Get display name for zone type
 */
inline const char* GetZoneTypeName(ZoneType type) {
    switch (type) {
        case ZoneType::SafeZone:       return "Safe Zone";
        case ZoneType::PvPZone:        return "PvP Zone";
        case ZoneType::ResourceZone:   return "Resource Zone";
        case ZoneType::DangerZone:     return "Danger Zone";
        case ZoneType::QuestZone:      return "Quest Zone";
        case ZoneType::TownZone:       return "Town Zone";
        case ZoneType::WildernessZone: return "Wilderness";
        case ZoneType::BossZone:       return "Boss Zone";
        case ZoneType::EventZone:      return "Event Zone";
        default:                       return "Unknown";
    }
}

/**
 * @brief Zone effect that applies to entities in the zone
 */
struct ZoneEffect {
    std::string name;
    std::string description;

    // Stat modifiers (multipliers, 1.0 = no change)
    float healthRegen = 1.0f;
    float damageDealt = 1.0f;
    float damageTaken = 1.0f;
    float moveSpeed = 1.0f;
    float experienceGain = 1.0f;
    float resourceGatherRate = 1.0f;

    // Flat modifiers
    float healthRegenFlat = 0.0f;
    float damagePerSecond = 0.0f;

    // Flags
    bool preventCombat = false;
    bool preventBuilding = false;
    bool preventTeleport = false;
    bool hideFromMap = false;

    // Duration (0 = instant apply/remove on zone entry/exit)
    float lingerDuration = 0.0f;    ///< Effect lingers after leaving zone
};

/**
 * @brief Zone visual settings
 */
struct ZoneVisuals {
    glm::vec4 tintColor{1.0f};      ///< Color tint for zone
    float tintStrength = 0.0f;       ///< How strong the tint is (0-1)

    std::string particleEffect;      ///< Particle effect name
    float particleDensity = 1.0f;    ///< Particle spawn density

    std::string ambientSound;        ///< Ambient sound to play
    float soundVolume = 1.0f;        ///< Sound volume

    std::string skyboxOverride;      ///< Override skybox in zone
    float fogDensity = 0.0f;         ///< Additional fog density
    glm::vec3 fogColor{0.5f};        ///< Fog color

    bool showBorder = true;          ///< Show zone border in editor
    glm::vec4 borderColor{1.0f, 0.0f, 0.0f, 0.5f};
};

/**
 * @brief Zone trigger condition
 */
struct ZoneTrigger {
    enum class TriggerType : uint8_t {
        OnEnter,        ///< Triggered when entity enters
        OnExit,         ///< Triggered when entity exits
        OnStay,         ///< Triggered while entity stays (per second)
        OnKill,         ///< Triggered when entity kills something in zone
        OnDeath,        ///< Triggered when entity dies in zone
        OnInteract      ///< Triggered on interaction with zone object
    };

    TriggerType type = TriggerType::OnEnter;
    std::string actionScript;        ///< Script to execute
    std::string targetTag;           ///< Only trigger for entities with this tag
    float cooldown = 0.0f;           ///< Cooldown between triggers
    int maxTriggers = -1;            ///< Max triggers (-1 = unlimited)
    bool enabled = true;
};

/**
 * @brief Zone polygon vertex
 */
struct ZoneVertex {
    glm::vec2 position{0.0f};
};

/**
 * @brief Complete zone definition
 */
struct GameplayZone {
    std::string name;
    std::string description;
    ZoneType type = ZoneType::WildernessZone;

    std::vector<ZoneVertex> vertices;   ///< Zone boundary polygon
    float minHeight = -100.0f;          ///< Minimum Y height
    float maxHeight = 100.0f;           ///< Maximum Y height

    ZoneEffect effect;
    ZoneVisuals visuals;
    std::vector<ZoneTrigger> triggers;

    int priority = 0;                   ///< Higher priority zones override lower
    bool enabled = true;
    bool persistAcrossSessions = true;  ///< Zone persists when player logs out
};

/**
 * @brief Zone editor mode
 */
enum class ZoneEditMode : uint8_t {
    DrawZone,       ///< Draw zone boundary
    EditEffects,    ///< Edit zone effects
    EditVisuals,    ///< Edit zone visuals
    EditTriggers    ///< Edit zone triggers
};

/**
 * @brief Gameplay zone editor for world editing
 *
 * Features:
 * - Define zone boundaries (polygons)
 * - Configure zone types (safe, PvP, resource, etc.)
 * - Set zone effects (buffs, debuffs)
 * - Configure zone visuals (tint, particles)
 * - Set up zone triggers
 *
 * Usage:
 * 1. Select zone type
 * 2. Draw zone boundary
 * 3. Configure effects and visuals
 * 4. Add triggers if needed
 * 5. Save zone
 */
class ZoneEditor {
public:
    ZoneEditor();
    ~ZoneEditor() = default;

    // Allow copy and move
    ZoneEditor(const ZoneEditor&) = default;
    ZoneEditor& operator=(const ZoneEditor&) = default;
    ZoneEditor(ZoneEditor&&) noexcept = default;
    ZoneEditor& operator=(ZoneEditor&&) noexcept = default;

    // =========================================================================
    // Edit Mode
    // =========================================================================

    [[nodiscard]] ZoneEditMode GetMode() const noexcept { return m_mode; }
    void SetMode(ZoneEditMode mode) noexcept { m_mode = mode; }

    // =========================================================================
    // Zone Type
    // =========================================================================

    [[nodiscard]] ZoneType GetZoneType() const noexcept { return m_zoneType; }
    void SetZoneType(ZoneType type);

    // =========================================================================
    // Zone Drawing
    // =========================================================================

    void BeginZone(const std::string& name);
    void AddVertex(const glm::vec2& position);
    void RemoveLastVertex();
    bool FinishZone();
    void CancelZone();

    [[nodiscard]] bool IsDrawing() const noexcept { return m_isDrawing; }
    [[nodiscard]] const GameplayZone& GetCurrentZone() const noexcept { return m_currentZone; }
    [[nodiscard]] GameplayZone& GetCurrentZone() noexcept { return m_currentZone; }

    // =========================================================================
    // Zone Effects
    // =========================================================================

    void SetZoneEffect(const ZoneEffect& effect);
    [[nodiscard]] const ZoneEffect& GetDefaultEffect(ZoneType type) const;

    // =========================================================================
    // Zone Visuals
    // =========================================================================

    void SetZoneVisuals(const ZoneVisuals& visuals);
    [[nodiscard]] const ZoneVisuals& GetDefaultVisuals(ZoneType type) const;

    // =========================================================================
    // Zone Triggers
    // =========================================================================

    void AddTrigger(const ZoneTrigger& trigger);
    void RemoveTrigger(size_t index);
    void ClearTriggers();

    // =========================================================================
    // Zone Management
    // =========================================================================

    [[nodiscard]] const std::vector<GameplayZone>& GetZones() const noexcept { return m_zones; }
    bool DeleteZone(int index);
    void ClearAllZones();
    void SelectZone(int index);
    [[nodiscard]] int GetSelectedIndex() const noexcept { return m_selectedIndex; }

    // =========================================================================
    // Queries
    // =========================================================================

    [[nodiscard]] std::vector<GameplayZone*> GetZonesAtPosition(const glm::vec3& position);
    [[nodiscard]] GameplayZone* GetHighestPriorityZoneAt(const glm::vec3& position);
    [[nodiscard]] bool IsInZone(const glm::vec3& position, ZoneType type) const;
    [[nodiscard]] ZoneEffect GetCombinedEffectsAt(const glm::vec3& position) const;

    // =========================================================================
    // Tile Coverage
    // =========================================================================

    [[nodiscard]] std::vector<glm::ivec2> GetZoneTiles(const GameplayZone& zone) const;
    [[nodiscard]] std::vector<glm::ivec2> GetZoneBorderTiles(const GameplayZone& zone) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    [[nodiscard]] std::string ToJson() const;
    bool FromJson(const std::string& json);
    bool SaveZonesToFile(const std::string& path) const;
    bool LoadZonesFromFile(const std::string& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using ZoneCallback = std::function<void(const GameplayZone&)>;
    using EntityZoneCallback = std::function<void(Entity&, const GameplayZone&)>;

    void SetOnZoneCreated(ZoneCallback callback) { m_onZoneCreated = std::move(callback); }
    void SetOnZoneModified(ZoneCallback callback) { m_onZoneModified = std::move(callback); }
    void SetOnEntityEnterZone(EntityZoneCallback callback) { m_onEntityEnter = std::move(callback); }
    void SetOnEntityExitZone(EntityZoneCallback callback) { m_onEntityExit = std::move(callback); }

private:
    bool IsPointInZone(const glm::vec2& point, const GameplayZone& zone) const;
    void ApplyDefaultSettings(ZoneType type);

    // Mode
    ZoneEditMode m_mode = ZoneEditMode::DrawZone;
    ZoneType m_zoneType = ZoneType::WildernessZone;

    // Current zone
    GameplayZone m_currentZone;
    bool m_isDrawing = false;

    // All zones
    std::vector<GameplayZone> m_zones;
    int m_selectedIndex = -1;

    // Default effects/visuals by type
    std::unordered_map<ZoneType, ZoneEffect> m_defaultEffects;
    std::unordered_map<ZoneType, ZoneVisuals> m_defaultVisuals;

    // Callbacks
    ZoneCallback m_onZoneCreated;
    ZoneCallback m_onZoneModified;
    EntityZoneCallback m_onEntityEnter;
    EntityZoneCallback m_onEntityExit;
};

} // namespace Vehement
