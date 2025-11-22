#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

namespace Vehement {
namespace RTS {

/**
 * @brief Building types that can be constructed in the RTS mode
 */
enum class BuildingType : uint8_t {
    // Core structures
    Headquarters,       ///< Main base building
    Barracks,          ///< Unit training facility
    Workshop,          ///< Vehicle/equipment production
    Storage,           ///< Resource storage

    // Defensive structures
    Wall,              ///< Basic wall segment
    WallGate,          ///< Gate in wall
    Tower,             ///< Defensive tower
    Bunker,            ///< Reinforced defensive position
    Turret,            ///< Automated defense turret

    // Economic structures
    Farm,              ///< Food production
    Mine,              ///< Resource extraction
    Warehouse,         ///< Large storage facility
    Market,            ///< Trade and commerce

    // Support structures
    Hospital,          ///< Unit healing
    ResearchLab,       ///< Technology research
    PowerPlant,        ///< Energy production

    // Culture-specific (unlocked by culture)
    Bazaar,            ///< Merchant culture trade hub
    HiddenEntrance,    ///< Underground culture stealth entry
    MobileWorkshop,    ///< Nomad culture packable workshop
    Yurt,              ///< Nomad culture mobile housing
    Castle,            ///< Fortress culture main stronghold
    Factory,           ///< Industrial culture mass production

    Count
};

/**
 * @brief Convert building type to string for UI/debugging
 */
inline const char* BuildingTypeToString(BuildingType type) {
    switch (type) {
        case BuildingType::Headquarters:    return "Headquarters";
        case BuildingType::Barracks:        return "Barracks";
        case BuildingType::Workshop:        return "Workshop";
        case BuildingType::Storage:         return "Storage";
        case BuildingType::Wall:            return "Wall";
        case BuildingType::WallGate:        return "Wall Gate";
        case BuildingType::Tower:           return "Tower";
        case BuildingType::Bunker:          return "Bunker";
        case BuildingType::Turret:          return "Turret";
        case BuildingType::Farm:            return "Farm";
        case BuildingType::Mine:            return "Mine";
        case BuildingType::Warehouse:       return "Warehouse";
        case BuildingType::Market:          return "Market";
        case BuildingType::Hospital:        return "Hospital";
        case BuildingType::ResearchLab:     return "Research Lab";
        case BuildingType::PowerPlant:      return "Power Plant";
        case BuildingType::Bazaar:          return "Bazaar";
        case BuildingType::HiddenEntrance:  return "Hidden Entrance";
        case BuildingType::MobileWorkshop:  return "Mobile Workshop";
        case BuildingType::Yurt:            return "Yurt";
        case BuildingType::Castle:          return "Castle";
        case BuildingType::Factory:         return "Factory";
        default:                            return "Unknown";
    }
}

/**
 * @brief Culture types available to players
 *
 * Each culture provides unique bonuses, building skins, and playstyles.
 * Players select a culture at game start which persists for the session.
 */
enum class CultureType : uint8_t {
    // Defensive cultures - Focus on fortification and holding ground
    Fortress,       ///< European castle style - strong walls, slow build
    Bunker,         ///< Modern military - concrete, turrets, high-tech defense

    // Mobile cultures - Focus on flexibility and relocation
    Nomad,          ///< Tents and wagons - fast setup, easy relocation
    Scavenger,      ///< Makeshift structures - cheap, fast, weak

    // Economic cultures - Focus on resource generation and trade
    Merchant,       ///< Trade focused - bazaars, caravans, diplomatic
    Industrial,     ///< Factory style - high production, automation

    // Stealth cultures - Focus on concealment and surprise
    Underground,    ///< Hidden bunkers - hard to find, tunnel networks
    Forest,         ///< Camouflaged - blends with terrain, ambush tactics

    Count
};

/**
 * @brief Convert culture type to string
 */
inline const char* CultureTypeToString(CultureType type) {
    switch (type) {
        case CultureType::Fortress:     return "Fortress";
        case CultureType::Bunker:       return "Bunker";
        case CultureType::Nomad:        return "Nomad";
        case CultureType::Scavenger:    return "Scavenger";
        case CultureType::Merchant:     return "Merchant";
        case CultureType::Industrial:   return "Industrial";
        case CultureType::Underground:  return "Underground";
        case CultureType::Forest:       return "Forest";
        default:                        return "Unknown";
    }
}

/**
 * @brief Resource cost structure for buildings and research
 */
struct ResourceCost {
    int32_t wood = 0;       ///< Wood resources required
    int32_t stone = 0;      ///< Stone resources required
    int32_t metal = 0;      ///< Metal resources required
    int32_t food = 0;       ///< Food resources required
    int32_t gold = 0;       ///< Gold/currency required

    ResourceCost() = default;
    ResourceCost(int32_t w, int32_t s, int32_t m, int32_t f, int32_t g)
        : wood(w), stone(s), metal(m), food(f), gold(g) {}

    /**
     * @brief Scale all costs by a multiplier
     */
    [[nodiscard]] ResourceCost Scaled(float multiplier) const {
        return ResourceCost(
            static_cast<int32_t>(wood * multiplier),
            static_cast<int32_t>(stone * multiplier),
            static_cast<int32_t>(metal * multiplier),
            static_cast<int32_t>(food * multiplier),
            static_cast<int32_t>(gold * multiplier)
        );
    }

    /**
     * @brief Check if this cost is affordable
     */
    [[nodiscard]] bool CanAfford(const ResourceCost& available) const {
        return wood <= available.wood &&
               stone <= available.stone &&
               metal <= available.metal &&
               food <= available.food &&
               gold <= available.gold;
    }
};

/**
 * @brief Culture-specific bonus modifiers
 *
 * All multipliers default to 1.0 (no change).
 * Values > 1.0 indicate bonuses, < 1.0 indicate penalties.
 */
struct CultureBonusModifiers {
    // Construction
    float buildSpeedMultiplier = 1.0f;      ///< Building construction speed
    float buildCostMultiplier = 1.0f;       ///< Resource cost for buildings
    float repairSpeedMultiplier = 1.0f;     ///< Repair speed for damaged buildings

    // Economy
    float gatherSpeedMultiplier = 1.0f;     ///< Resource gathering rate
    float tradeMultiplier = 1.0f;           ///< Trade profit bonus
    float storageMultiplier = 1.0f;         ///< Storage capacity bonus
    float productionMultiplier = 1.0f;      ///< Unit/item production speed

    // Combat
    float defenseMultiplier = 1.0f;         ///< Damage reduction for units/buildings
    float attackMultiplier = 1.0f;          ///< Damage dealt bonus
    float healingMultiplier = 1.0f;         ///< Healing received bonus

    // Structures
    float wallHPMultiplier = 1.0f;          ///< Wall/fortification HP bonus
    float towerDamageMultiplier = 1.0f;     ///< Tower/turret damage bonus
    float buildingHPMultiplier = 1.0f;      ///< General building HP bonus

    // Mobility
    float unitSpeedMultiplier = 1.0f;       ///< Unit movement speed
    float caravanSpeedMultiplier = 1.0f;    ///< Trade caravan speed
    float packingSpeedMultiplier = 1.0f;    ///< Building pack/unpack speed (Nomad)

    // Recon
    float visionMultiplier = 1.0f;          ///< Vision range bonus
    float stealthMultiplier = 1.0f;         ///< Detection avoidance
    float detectionMultiplier = 1.0f;       ///< Enemy detection range
};

/**
 * @brief Complete data structure for a culture/faction
 *
 * Contains all information needed to apply culture-specific
 * gameplay modifications, visual assets, and unique abilities.
 */
struct CultureData {
    CultureType type = CultureType::Fortress;
    std::string name;                       ///< Display name
    std::string description;                ///< Flavor text description
    std::string shortDescription;           ///< Brief summary for UI

    // Bonus modifiers
    CultureBonusModifiers bonuses;

    // Texture paths for buildings (relative to Vehement2/images/)
    std::unordered_map<BuildingType, std::string> buildingTextures;

    // Unit texture paths
    std::string workerTexture;              ///< Worker/builder unit texture
    std::string guardTexture;               ///< Basic combat unit texture
    std::string eliteTexture;               ///< Elite/special unit texture
    std::string scoutTexture;               ///< Reconnaissance unit texture

    // Visual style
    std::string primaryColorHex;            ///< Primary culture color (e.g., "#8B4513")
    std::string secondaryColorHex;          ///< Secondary accent color
    std::string bannerTexture;              ///< Culture banner/flag texture
    std::string previewTexture;             ///< Preview image for selection screen

    // Special abilities unique to this culture
    std::vector<std::string> uniqueAbilities;

    // Buildings unique to or restricted from this culture
    std::vector<BuildingType> uniqueBuildings;      ///< Only this culture can build
    std::vector<BuildingType> restrictedBuildings;  ///< This culture cannot build

    // Starting bonuses
    ResourceCost startingResources;
    std::vector<BuildingType> startingBuildings;

    // Audio style
    std::string musicTheme;                 ///< Background music track
    std::string ambientSounds;              ///< Environmental audio
};

/**
 * @brief Manager class for culture system
 *
 * Provides access to culture data and handles culture-related operations
 * such as applying bonuses and loading culture-specific assets.
 */
class CultureManager {
public:
    /**
     * @brief Get the singleton instance
     */
    [[nodiscard]] static CultureManager& Instance();

    // Delete copy/move
    CultureManager(const CultureManager&) = delete;
    CultureManager& operator=(const CultureManager&) = delete;
    CultureManager(CultureManager&&) = delete;
    CultureManager& operator=(CultureManager&&) = delete;

    /**
     * @brief Initialize all culture data
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Get culture data by type
     * @param type Culture type to retrieve
     * @return Pointer to culture data or nullptr if not found
     */
    [[nodiscard]] const CultureData* GetCultureData(CultureType type) const;

    /**
     * @brief Get all available cultures
     * @return Vector of all culture data
     */
    [[nodiscard]] const std::vector<CultureData>& GetAllCultures() const { return m_cultures; }

    /**
     * @brief Get building texture path for a culture
     * @param culture Culture type
     * @param building Building type
     * @return Texture path or default if not found
     */
    [[nodiscard]] std::string GetBuildingTexture(CultureType culture, BuildingType building) const;

    /**
     * @brief Apply culture bonuses to a base value
     * @param culture Culture type
     * @param baseValue Base value to modify
     * @param bonusType Type of bonus (matches CultureBonusModifiers field names)
     * @return Modified value
     */
    [[nodiscard]] float ApplyBonus(CultureType culture, float baseValue,
                                   const std::string& bonusType) const;

    /**
     * @brief Apply culture cost modifier to resource cost
     * @param culture Culture type
     * @param baseCost Base resource cost
     * @return Modified resource cost
     */
    [[nodiscard]] ResourceCost ApplyCostModifier(CultureType culture,
                                                  const ResourceCost& baseCost) const;

    /**
     * @brief Check if a culture can build a specific building
     * @param culture Culture type
     * @param building Building type
     * @return true if building is available to this culture
     */
    [[nodiscard]] bool CanBuild(CultureType culture, BuildingType building) const;

    /**
     * @brief Get list of buildings available to a culture
     * @param culture Culture type
     * @return Vector of available building types
     */
    [[nodiscard]] std::vector<BuildingType> GetAvailableBuildings(CultureType culture) const;

private:
    CultureManager() = default;
    ~CultureManager() = default;

    /**
     * @brief Initialize data for a specific culture
     */
    void InitializeCultureData(CultureType type);

    /**
     * @brief Set up Fortress culture
     */
    void InitializeFortressCulture();

    /**
     * @brief Set up Bunker culture
     */
    void InitializeBunkerCulture();

    /**
     * @brief Set up Nomad culture
     */
    void InitializeNomadCulture();

    /**
     * @brief Set up Scavenger culture
     */
    void InitializeScavengerCulture();

    /**
     * @brief Set up Merchant culture
     */
    void InitializeMerchantCulture();

    /**
     * @brief Set up Industrial culture
     */
    void InitializeIndustrialCulture();

    /**
     * @brief Set up Underground culture
     */
    void InitializeUndergroundCulture();

    /**
     * @brief Set up Forest culture
     */
    void InitializeForestCulture();

    bool m_initialized = false;
    std::vector<CultureData> m_cultures;
};

} // namespace RTS
} // namespace Vehement
