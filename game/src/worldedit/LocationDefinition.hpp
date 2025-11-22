#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace Vehement {

/**
 * @brief Priority mode for PCG blending
 *
 * Determines how manual edits interact with procedural content generation.
 */
enum class PCGPriority : uint8_t {
    FullyManual,    ///< Ignore PCG entirely, use only manual edits
    BlendEdges,     ///< Blend manual edits with PCG at boundaries
    PCGBase,        ///< Use PCG as base, overlay manual edits
    PCGOnly         ///< Completely procedural, no manual edits
};

/**
 * @brief Get display name for PCG priority mode
 */
inline const char* GetPCGPriorityName(PCGPriority priority) {
    switch (priority) {
        case PCGPriority::FullyManual: return "Fully Manual";
        case PCGPriority::BlendEdges:  return "Blend Edges";
        case PCGPriority::PCGBase:     return "PCG Base";
        case PCGPriority::PCGOnly:     return "PCG Only";
        default:                       return "Unknown";
    }
}

/**
 * @brief Geographic coordinate bounds for location
 */
struct GeoCoordinateBounds {
    double minLatitude = 0.0;
    double maxLatitude = 0.0;
    double minLongitude = 0.0;
    double maxLongitude = 0.0;

    /**
     * @brief Check if geo bounds are valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return minLatitude <= maxLatitude &&
               minLongitude <= maxLongitude &&
               minLatitude >= -90.0 && maxLatitude <= 90.0 &&
               minLongitude >= -180.0 && maxLongitude <= 180.0;
    }

    /**
     * @brief Check if a GPS coordinate is within bounds
     */
    [[nodiscard]] bool Contains(double lat, double lon) const noexcept {
        return lat >= minLatitude && lat <= maxLatitude &&
               lon >= minLongitude && lon <= maxLongitude;
    }

    /**
     * @brief Get center point of bounds
     */
    [[nodiscard]] glm::dvec2 GetCenter() const noexcept {
        return glm::dvec2(
            (minLatitude + maxLatitude) / 2.0,
            (minLongitude + maxLongitude) / 2.0
        );
    }
};

/**
 * @brief World coordinate bounding box
 */
struct WorldBoundingBox {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    /**
     * @brief Check if bounds are valid
     */
    [[nodiscard]] bool IsValid() const noexcept {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    /**
     * @brief Check if a point is within bounds
     */
    [[nodiscard]] bool Contains(const glm::vec3& point) const noexcept {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    /**
     * @brief Check if a 2D point (XZ plane) is within bounds
     */
    [[nodiscard]] bool Contains2D(const glm::vec2& point) const noexcept {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.z && point.y <= max.z;
    }

    /**
     * @brief Get center point of bounds
     */
    [[nodiscard]] glm::vec3 GetCenter() const noexcept {
        return (min + max) * 0.5f;
    }

    /**
     * @brief Get size of bounds
     */
    [[nodiscard]] glm::vec3 GetSize() const noexcept {
        return max - min;
    }

    /**
     * @brief Expand bounds to include a point
     */
    void Expand(const glm::vec3& point) noexcept {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    /**
     * @brief Check if this box intersects another
     */
    [[nodiscard]] bool Intersects(const WorldBoundingBox& other) const noexcept {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
};

/**
 * @brief Version information for tracking edits
 */
struct LocationVersion {
    uint32_t major = 1;
    uint32_t minor = 0;
    uint64_t timestamp = 0;         ///< Unix timestamp of last edit
    std::string author;              ///< Author of last edit
    std::string changeDescription;   ///< Description of last change

    /**
     * @brief Increment minor version
     */
    void IncrementMinor() noexcept {
        ++minor;
    }

    /**
     * @brief Increment major version (resets minor)
     */
    void IncrementMajor() noexcept {
        ++major;
        minor = 0;
    }

    /**
     * @brief Get version as string "major.minor"
     */
    [[nodiscard]] std::string ToString() const {
        return std::to_string(major) + "." + std::to_string(minor);
    }
};

/**
 * @brief Definition of a craftable location in the world
 *
 * Locations are named areas with defined boundaries that can contain
 * manual edits (tiles, buildings, entities, etc.) that override or
 * blend with procedural content generation.
 *
 * Features:
 * - Named and described for easy identification
 * - World coordinate bounds for spatial queries
 * - Geographic coordinate bounds for GPS-based features
 * - Tags for categorization and filtering
 * - PCG priority settings
 * - Version tracking for edit history
 * - JSON serialization support
 */
class LocationDefinition {
public:
    using LocationId = uint32_t;
    static constexpr LocationId INVALID_ID = 0;

    LocationDefinition();
    explicit LocationDefinition(const std::string& name);
    ~LocationDefinition() = default;

    // Allow copy and move
    LocationDefinition(const LocationDefinition&) = default;
    LocationDefinition& operator=(const LocationDefinition&) = default;
    LocationDefinition(LocationDefinition&&) noexcept = default;
    LocationDefinition& operator=(LocationDefinition&&) noexcept = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /** @brief Get unique location ID */
    [[nodiscard]] LocationId GetId() const noexcept { return m_id; }

    /** @brief Set location ID */
    void SetId(LocationId id) noexcept { m_id = id; }

    /** @brief Get location name */
    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }

    /** @brief Set location name */
    void SetName(const std::string& name) { m_name = name; }

    /** @brief Get location description */
    [[nodiscard]] const std::string& GetDescription() const noexcept { return m_description; }

    /** @brief Set location description */
    void SetDescription(const std::string& desc) { m_description = desc; }

    // =========================================================================
    // Bounds
    // =========================================================================

    /** @brief Get world coordinate bounds */
    [[nodiscard]] const WorldBoundingBox& GetWorldBounds() const noexcept { return m_worldBounds; }

    /** @brief Set world coordinate bounds */
    void SetWorldBounds(const WorldBoundingBox& bounds) noexcept { m_worldBounds = bounds; }

    /** @brief Set world bounds from min/max */
    void SetWorldBounds(const glm::vec3& min, const glm::vec3& max) noexcept {
        m_worldBounds.min = min;
        m_worldBounds.max = max;
    }

    /** @brief Get geographic coordinate bounds */
    [[nodiscard]] const GeoCoordinateBounds& GetGeoBounds() const noexcept { return m_geoBounds; }

    /** @brief Set geographic coordinate bounds */
    void SetGeoBounds(const GeoCoordinateBounds& bounds) noexcept { m_geoBounds = bounds; }

    /** @brief Check if world bounds are valid */
    [[nodiscard]] bool HasValidWorldBounds() const noexcept { return m_worldBounds.IsValid(); }

    /** @brief Check if geo bounds are valid */
    [[nodiscard]] bool HasValidGeoBounds() const noexcept { return m_geoBounds.IsValid(); }

    /** @brief Check if point is within world bounds */
    [[nodiscard]] bool ContainsWorldPoint(const glm::vec3& point) const noexcept {
        return m_worldBounds.Contains(point);
    }

    /** @brief Check if GPS coordinate is within bounds */
    [[nodiscard]] bool ContainsGeoPoint(double lat, double lon) const noexcept {
        return m_geoBounds.Contains(lat, lon);
    }

    // =========================================================================
    // Tags
    // =========================================================================

    /** @brief Get all tags */
    [[nodiscard]] const std::vector<std::string>& GetTags() const noexcept { return m_tags; }

    /** @brief Add a tag */
    void AddTag(const std::string& tag);

    /** @brief Remove a tag */
    bool RemoveTag(const std::string& tag);

    /** @brief Check if location has a specific tag */
    [[nodiscard]] bool HasTag(const std::string& tag) const;

    /** @brief Clear all tags */
    void ClearTags() { m_tags.clear(); }

    /** @brief Set tags from vector */
    void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }

    // =========================================================================
    // PCG Priority
    // =========================================================================

    /** @brief Get PCG priority mode */
    [[nodiscard]] PCGPriority GetPCGPriority() const noexcept { return m_pcgPriority; }

    /** @brief Set PCG priority mode */
    void SetPCGPriority(PCGPriority priority) noexcept { m_pcgPriority = priority; }

    /** @brief Get blend radius for edge blending */
    [[nodiscard]] float GetBlendRadius() const noexcept { return m_blendRadius; }

    /** @brief Set blend radius */
    void SetBlendRadius(float radius) noexcept { m_blendRadius = radius; }

    // =========================================================================
    // Version Tracking
    // =========================================================================

    /** @brief Get version info */
    [[nodiscard]] const LocationVersion& GetVersion() const noexcept { return m_version; }

    /** @brief Get mutable version info */
    [[nodiscard]] LocationVersion& GetVersion() noexcept { return m_version; }

    /** @brief Set version info */
    void SetVersion(const LocationVersion& version) noexcept { m_version = version; }

    /** @brief Mark location as edited */
    void MarkEdited(const std::string& author = "", const std::string& description = "");

    // =========================================================================
    // Metadata
    // =========================================================================

    /** @brief Get preset template name (if based on preset) */
    [[nodiscard]] const std::string& GetPresetName() const noexcept { return m_presetName; }

    /** @brief Set preset template name */
    void SetPresetName(const std::string& name) { m_presetName = name; }

    /** @brief Check if location is based on a preset */
    [[nodiscard]] bool IsFromPreset() const noexcept { return !m_presetName.empty(); }

    /** @brief Get category for organization */
    [[nodiscard]] const std::string& GetCategory() const noexcept { return m_category; }

    /** @brief Set category */
    void SetCategory(const std::string& category) { m_category = category; }

    /** @brief Check if location is enabled */
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    /** @brief Enable/disable location */
    void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }

    /** @brief Get file path (if loaded from file) */
    [[nodiscard]] const std::string& GetFilePath() const noexcept { return m_filePath; }

    /** @brief Set file path */
    void SetFilePath(const std::string& path) { m_filePath = path; }

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * @brief Serialize location to JSON string
     * @param pretty Use pretty formatting
     * @return JSON string
     */
    [[nodiscard]] std::string ToJson(bool pretty = true) const;

    /**
     * @brief Deserialize location from JSON string
     * @param json JSON string
     * @return true if successful
     */
    bool FromJson(const std::string& json);

    /**
     * @brief Save location to file
     * @param filePath Path to save to
     * @return true if successful
     */
    bool SaveToFile(const std::string& filePath) const;

    /**
     * @brief Load location from file
     * @param filePath Path to load from
     * @return true if successful
     */
    bool LoadFromFile(const std::string& filePath);

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Create a deep copy of this location
     */
    [[nodiscard]] std::unique_ptr<LocationDefinition> Clone() const;

    /**
     * @brief Check if location is valid
     */
    [[nodiscard]] bool IsValid() const noexcept;

private:
    // Identity
    LocationId m_id = INVALID_ID;
    std::string m_name;
    std::string m_description;

    // Bounds
    WorldBoundingBox m_worldBounds;
    GeoCoordinateBounds m_geoBounds;

    // Tags
    std::vector<std::string> m_tags;

    // PCG settings
    PCGPriority m_pcgPriority = PCGPriority::BlendEdges;
    float m_blendRadius = 5.0f;

    // Version
    LocationVersion m_version;

    // Metadata
    std::string m_presetName;
    std::string m_category;
    std::string m_filePath;
    bool m_enabled = true;

    // ID generation
    static LocationId s_nextId;
};

} // namespace Vehement
