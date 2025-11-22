#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <functional>

namespace Vehement2 {
namespace RTS {

/**
 * @brief Type of unit that provides vision
 */
enum class VisionSourceType : uint8_t {
    Hero,           // Player's main hero unit (large radius)
    Worker,         // Worker units (small radius)
    Building,       // Structures (medium radius, always on)
    Scout,          // Scout units (large radius, fast)
    WatchTower,     // Defensive tower (very large radius)
    Flare,          // Temporary vision (decays over time)
    Custom          // User-defined vision source
};

/**
 * @brief Vision configuration for different unit types
 */
struct VisionConfig {
    float baseRadius = 10.0f;           // Base vision radius in tiles
    float heightBonus = 0.5f;           // Extra radius per unit of height
    bool nightVision = false;           // Can see in darkness
    bool detectsHidden = false;         // Can see cloaked/hidden units
    bool blockedByTerrain = true;       // Vision blocked by walls/obstacles
    float dayMultiplier = 1.0f;         // Vision multiplier during day
    float nightMultiplier = 0.5f;       // Vision multiplier at night
    float weatherPenalty = 0.3f;        // Vision reduction in bad weather

    // Default configurations for each type
    static VisionConfig ForHero() {
        VisionConfig cfg;
        cfg.baseRadius = 15.0f;
        cfg.heightBonus = 1.0f;
        cfg.dayMultiplier = 1.0f;
        cfg.nightMultiplier = 0.6f;
        return cfg;
    }

    static VisionConfig ForWorker() {
        VisionConfig cfg;
        cfg.baseRadius = 6.0f;
        cfg.heightBonus = 0.25f;
        cfg.dayMultiplier = 1.0f;
        cfg.nightMultiplier = 0.4f;
        return cfg;
    }

    static VisionConfig ForBuilding() {
        VisionConfig cfg;
        cfg.baseRadius = 8.0f;
        cfg.heightBonus = 0.0f;         // Buildings don't move
        cfg.blockedByTerrain = true;
        cfg.dayMultiplier = 1.0f;
        cfg.nightMultiplier = 0.3f;     // Buildings have limited night vision
        return cfg;
    }

    static VisionConfig ForScout() {
        VisionConfig cfg;
        cfg.baseRadius = 18.0f;
        cfg.heightBonus = 1.5f;
        cfg.dayMultiplier = 1.0f;
        cfg.nightMultiplier = 0.7f;
        return cfg;
    }

    static VisionConfig ForWatchTower() {
        VisionConfig cfg;
        cfg.baseRadius = 25.0f;
        cfg.heightBonus = 2.0f;         // Towers are tall
        cfg.nightVision = true;         // Towers have night vision
        cfg.detectsHidden = true;       // Can see hidden units
        cfg.dayMultiplier = 1.0f;
        cfg.nightMultiplier = 0.9f;
        return cfg;
    }

    static VisionConfig ForFlare() {
        VisionConfig cfg;
        cfg.baseRadius = 12.0f;
        cfg.heightBonus = 0.0f;
        cfg.nightVision = true;         // Flares light up area
        cfg.blockedByTerrain = false;   // Flares reveal through walls
        cfg.dayMultiplier = 1.0f;
        cfg.nightMultiplier = 1.0f;     // Full effect at night
        return cfg;
    }
};

/**
 * @brief Individual vision source (unit/building that provides vision)
 *
 * Vision sources are attached to game entities and determine what
 * areas of the map are visible to the player.
 */
struct VisionSource {
    // Position and geometry
    glm::vec2 position{0.0f};           // World position (2D)
    float height = 0.0f;                // Height above ground (affects range)
    float radius = 10.0f;               // Effective vision radius

    // Vision properties
    VisionSourceType type = VisionSourceType::Worker;
    bool nightVision = false;           // Can see in dark conditions
    bool detectsHidden = false;         // Can reveal cloaked units
    bool blockedByTerrain = true;       // Vision blocked by obstacles

    // State
    bool active = true;                 // Is this source currently providing vision
    float lifetime = -1.0f;             // Remaining lifetime (-1 = infinite)
    uint32_t ownerId = 0;               // Entity ID that owns this vision source
    uint8_t teamId = 0;                 // Team this vision belongs to

    // Culture/bonus modifiers
    float cultureBonus = 0.0f;          // Additional radius from culture upgrades
    float upgradeBonus = 0.0f;          // Additional radius from tech upgrades

    /**
     * @brief Get the effective vision radius considering all modifiers
     * @param isDaytime Whether it's currently daytime
     * @param weatherFactor Weather visibility factor (0-1)
     * @param config Vision configuration to use
     * @return Effective vision radius
     */
    float GetEffectiveRadius(bool isDaytime, float weatherFactor, const VisionConfig& config) const {
        float baseWithHeight = radius + (height * config.heightBonus);
        float timeMultiplier = isDaytime ? config.dayMultiplier : config.nightMultiplier;
        float weatherMultiplier = 1.0f - (config.weatherPenalty * (1.0f - weatherFactor));

        // Night vision negates night penalty
        if (nightVision && !isDaytime) {
            timeMultiplier = config.dayMultiplier;
        }

        return (baseWithHeight + cultureBonus + upgradeBonus) * timeMultiplier * weatherMultiplier;
    }

    /**
     * @brief Create a vision source from a configuration
     */
    static VisionSource Create(VisionSourceType sourceType, glm::vec2 pos, uint32_t owner, uint8_t team) {
        VisionSource source;
        source.position = pos;
        source.type = sourceType;
        source.ownerId = owner;
        source.teamId = team;

        VisionConfig config;
        switch (sourceType) {
            case VisionSourceType::Hero:
                config = VisionConfig::ForHero();
                break;
            case VisionSourceType::Worker:
                config = VisionConfig::ForWorker();
                break;
            case VisionSourceType::Building:
                config = VisionConfig::ForBuilding();
                break;
            case VisionSourceType::Scout:
                config = VisionConfig::ForScout();
                break;
            case VisionSourceType::WatchTower:
                config = VisionConfig::ForWatchTower();
                break;
            case VisionSourceType::Flare:
                config = VisionConfig::ForFlare();
                source.lifetime = 30.0f;  // Flares last 30 seconds
                break;
            default:
                break;
        }

        source.radius = config.baseRadius;
        source.nightVision = config.nightVision;
        source.detectsHidden = config.detectsHidden;
        source.blockedByTerrain = config.blockedByTerrain;

        return source;
    }

    /**
     * @brief Update the vision source (decrease lifetime, etc.)
     * @param deltaTime Time since last update
     * @return true if source is still active
     */
    bool Update(float deltaTime) {
        if (lifetime > 0.0f) {
            lifetime -= deltaTime;
            if (lifetime <= 0.0f) {
                active = false;
                return false;
            }
        }
        return active;
    }
};

/**
 * @brief Collection of vision sources for a team/player
 */
class VisionSourceManager {
public:
    VisionSourceManager() = default;
    ~VisionSourceManager() = default;

    /**
     * @brief Add a new vision source
     * @return Index of the added source
     */
    size_t AddSource(const VisionSource& source) {
        m_sources.push_back(source);
        return m_sources.size() - 1;
    }

    /**
     * @brief Remove a vision source by owner ID
     */
    void RemoveByOwner(uint32_t ownerId) {
        m_sources.erase(
            std::remove_if(m_sources.begin(), m_sources.end(),
                [ownerId](const VisionSource& s) { return s.ownerId == ownerId; }),
            m_sources.end()
        );
    }

    /**
     * @brief Update all vision sources
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime) {
        // Update all sources and remove expired ones
        m_sources.erase(
            std::remove_if(m_sources.begin(), m_sources.end(),
                [deltaTime](VisionSource& s) { return !s.Update(deltaTime); }),
            m_sources.end()
        );
    }

    /**
     * @brief Update position of a vision source by owner ID
     */
    void UpdatePosition(uint32_t ownerId, const glm::vec2& newPosition) {
        for (auto& source : m_sources) {
            if (source.ownerId == ownerId) {
                source.position = newPosition;
            }
        }
    }

    /**
     * @brief Get all active vision sources
     */
    const std::vector<VisionSource>& GetSources() const { return m_sources; }

    /**
     * @brief Get sources for a specific team
     */
    std::vector<VisionSource> GetSourcesForTeam(uint8_t teamId) const {
        std::vector<VisionSource> result;
        for (const auto& source : m_sources) {
            if (source.teamId == teamId && source.active) {
                result.push_back(source);
            }
        }
        return result;
    }

    /**
     * @brief Clear all vision sources
     */
    void Clear() {
        m_sources.clear();
    }

    /**
     * @brief Get number of active sources
     */
    size_t GetActiveCount() const {
        size_t count = 0;
        for (const auto& source : m_sources) {
            if (source.active) count++;
        }
        return count;
    }

private:
    std::vector<VisionSource> m_sources;
};

/**
 * @brief Environmental conditions affecting vision
 */
struct VisionEnvironment {
    bool isDaytime = true;              // Day/night cycle
    float timeOfDay = 12.0f;            // 0-24 hour format
    float weatherVisibility = 1.0f;     // 0 = zero visibility, 1 = perfect

    // Weather types affecting visibility
    enum class Weather : uint8_t {
        Clear,          // Full visibility
        Cloudy,         // 90% visibility
        Fog,            // 50% visibility
        Rain,           // 70% visibility
        Storm,          // 40% visibility
        Sandstorm       // 30% visibility
    };
    Weather currentWeather = Weather::Clear;

    /**
     * @brief Update environment based on time
     * @param deltaTime Time since last update
     * @param dayLengthSeconds Length of a full day in seconds
     */
    void Update(float deltaTime, float dayLengthSeconds = 600.0f) {
        // Progress time of day
        float hoursPerSecond = 24.0f / dayLengthSeconds;
        timeOfDay += deltaTime * hoursPerSecond;
        if (timeOfDay >= 24.0f) {
            timeOfDay -= 24.0f;
        }

        // Daytime is between 6:00 and 20:00
        isDaytime = (timeOfDay >= 6.0f && timeOfDay < 20.0f);

        // Update weather visibility
        switch (currentWeather) {
            case Weather::Clear:     weatherVisibility = 1.0f; break;
            case Weather::Cloudy:    weatherVisibility = 0.9f; break;
            case Weather::Fog:       weatherVisibility = 0.5f; break;
            case Weather::Rain:      weatherVisibility = 0.7f; break;
            case Weather::Storm:     weatherVisibility = 0.4f; break;
            case Weather::Sandstorm: weatherVisibility = 0.3f; break;
        }
    }

    /**
     * @brief Set weather condition
     */
    void SetWeather(Weather weather) {
        currentWeather = weather;
    }

    /**
     * @brief Get ambient light level (0-1)
     */
    float GetAmbientLight() const {
        // Smooth transition between day and night
        if (timeOfDay >= 6.0f && timeOfDay < 8.0f) {
            // Dawn
            return (timeOfDay - 6.0f) / 2.0f;
        } else if (timeOfDay >= 8.0f && timeOfDay < 18.0f) {
            // Full day
            return 1.0f;
        } else if (timeOfDay >= 18.0f && timeOfDay < 20.0f) {
            // Dusk
            return 1.0f - (timeOfDay - 18.0f) / 2.0f;
        } else {
            // Night
            return 0.1f;  // Some moonlight
        }
    }
};

} // namespace RTS
} // namespace Vehement2
