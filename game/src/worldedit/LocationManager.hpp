#pragma once

#include "LocationDefinition.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Vehement {

/**
 * @brief Result of a location query
 */
struct LocationQueryResult {
    LocationDefinition* location = nullptr;
    float distance = 0.0f;              ///< Distance from query point (if applicable)
    float overlapPercent = 0.0f;        ///< Overlap percentage with query bounds (if applicable)
};

/**
 * @brief Manager for all defined locations in the world
 *
 * Provides functionality for:
 * - Loading and saving locations from/to files
 * - Querying locations by position, name, or tags
 * - Creating new locations from selection
 * - Deleting and modifying locations
 * - Import/export functionality
 *
 * Locations are stored in a spatial data structure for efficient queries.
 */
class LocationManager {
public:
    using LocationId = LocationDefinition::LocationId;
    using LocationCallback = std::function<void(LocationDefinition&)>;

    LocationManager();
    ~LocationManager();

    // Prevent copying, allow moving
    LocationManager(const LocationManager&) = delete;
    LocationManager& operator=(const LocationManager&) = delete;
    LocationManager(LocationManager&&) noexcept = default;
    LocationManager& operator=(LocationManager&&) noexcept = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the location manager
     * @param locationsDirectory Base directory for location files
     */
    void Initialize(const std::string& locationsDirectory);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Get the locations directory path
     */
    [[nodiscard]] const std::string& GetLocationsDirectory() const noexcept {
        return m_locationsDirectory;
    }

    // =========================================================================
    // Loading / Saving
    // =========================================================================

    /**
     * @brief Load all locations from the locations directory
     * @return Number of locations loaded
     */
    int LoadAllLocations();

    /**
     * @brief Load locations from a specific subdirectory
     * @param subdirectory Subdirectory name (e.g., "manual", "presets")
     * @return Number of locations loaded
     */
    int LoadLocationsFromDirectory(const std::string& subdirectory);

    /**
     * @brief Load a single location from file
     * @param filePath Path to location file
     * @return Pointer to loaded location, or nullptr on failure
     */
    LocationDefinition* LoadLocation(const std::string& filePath);

    /**
     * @brief Save a location to file
     * @param location Location to save
     * @param filePath Optional file path (uses location's path if empty)
     * @return true if successful
     */
    bool SaveLocation(LocationDefinition& location, const std::string& filePath = "");

    /**
     * @brief Save all modified locations
     * @return Number of locations saved
     */
    int SaveAllLocations();

    /**
     * @brief Reload all locations from disk
     */
    void ReloadAllLocations();

    // =========================================================================
    // Location Management
    // =========================================================================

    /**
     * @brief Create a new location
     * @param name Location name
     * @return Pointer to new location
     */
    LocationDefinition* CreateLocation(const std::string& name);

    /**
     * @brief Create a location from a world selection box
     * @param name Location name
     * @param worldMin Minimum world coordinates
     * @param worldMax Maximum world coordinates
     * @return Pointer to new location
     */
    LocationDefinition* CreateLocationFromSelection(
        const std::string& name,
        const glm::vec3& worldMin,
        const glm::vec3& worldMax
    );

    /**
     * @brief Delete a location by ID
     * @param id Location ID
     * @return true if location was deleted
     */
    bool DeleteLocation(LocationId id);

    /**
     * @brief Delete a location and its file
     * @param id Location ID
     * @return true if location and file were deleted
     */
    bool DeleteLocationAndFile(LocationId id);

    /**
     * @brief Get a location by ID
     * @param id Location ID
     * @return Pointer to location, or nullptr if not found
     */
    LocationDefinition* GetLocation(LocationId id);

    /**
     * @brief Get a location by ID (const version)
     */
    [[nodiscard]] const LocationDefinition* GetLocation(LocationId id) const;

    /**
     * @brief Get all locations
     */
    [[nodiscard]] const std::vector<std::unique_ptr<LocationDefinition>>& GetAllLocations() const {
        return m_locations;
    }

    /**
     * @brief Get number of locations
     */
    [[nodiscard]] size_t GetLocationCount() const noexcept { return m_locations.size(); }

    // =========================================================================
    // Queries by Position
    // =========================================================================

    /**
     * @brief Query locations containing a world point
     * @param point World position to check
     * @return Vector of locations containing the point
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByPosition(const glm::vec3& point);

    /**
     * @brief Query locations containing a 2D world point (XZ plane)
     * @param point 2D world position
     * @return Vector of locations containing the point
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByPosition2D(const glm::vec2& point);

    /**
     * @brief Get the location with highest priority at a position
     * @param point World position
     * @return Pointer to highest priority location, or nullptr
     */
    [[nodiscard]] LocationDefinition* GetPrimaryLocationAt(const glm::vec3& point);

    /**
     * @brief Query locations within a radius of a point
     * @param center Center point
     * @param radius Search radius
     * @return Vector of query results with distances
     */
    [[nodiscard]] std::vector<LocationQueryResult> QueryByRadius(
        const glm::vec3& center, float radius);

    /**
     * @brief Query locations intersecting a bounding box
     * @param bounds Bounding box to check
     * @return Vector of intersecting locations
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByBounds(const WorldBoundingBox& bounds);

    /**
     * @brief Query locations containing a GPS coordinate
     * @param latitude GPS latitude
     * @param longitude GPS longitude
     * @return Vector of matching locations
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByGPS(double latitude, double longitude);

    // =========================================================================
    // Queries by Name/Tag
    // =========================================================================

    /**
     * @brief Find a location by name (exact match)
     * @param name Location name
     * @return Pointer to location, or nullptr if not found
     */
    [[nodiscard]] LocationDefinition* FindByName(const std::string& name);

    /**
     * @brief Search locations by name (partial match)
     * @param searchTerm Search string
     * @return Vector of matching locations
     */
    [[nodiscard]] std::vector<LocationDefinition*> SearchByName(const std::string& searchTerm);

    /**
     * @brief Query locations by tag
     * @param tag Tag to search for
     * @return Vector of locations with the tag
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByTag(const std::string& tag);

    /**
     * @brief Query locations by multiple tags (AND)
     * @param tags Tags to search for
     * @return Vector of locations with all tags
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByTags(const std::vector<std::string>& tags);

    /**
     * @brief Query locations by any of multiple tags (OR)
     * @param tags Tags to search for
     * @return Vector of locations with any of the tags
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByAnyTag(const std::vector<std::string>& tags);

    /**
     * @brief Query locations by category
     * @param category Category name
     * @return Vector of locations in the category
     */
    [[nodiscard]] std::vector<LocationDefinition*> QueryByCategory(const std::string& category);

    /**
     * @brief Get all unique tags across all locations
     */
    [[nodiscard]] std::vector<std::string> GetAllTags() const;

    /**
     * @brief Get all unique categories across all locations
     */
    [[nodiscard]] std::vector<std::string> GetAllCategories() const;

    // =========================================================================
    // Import / Export
    // =========================================================================

    /**
     * @brief Export a location to a standalone file
     * @param location Location to export
     * @param exportPath Path to export to
     * @return true if successful
     */
    bool ExportLocation(const LocationDefinition& location, const std::string& exportPath);

    /**
     * @brief Export multiple locations to a directory
     * @param locations Locations to export
     * @param exportDirectory Directory to export to
     * @return Number of locations exported
     */
    int ExportLocations(
        const std::vector<LocationDefinition*>& locations,
        const std::string& exportDirectory
    );

    /**
     * @brief Import a location from an external file
     * @param importPath Path to import from
     * @return Pointer to imported location, or nullptr on failure
     */
    LocationDefinition* ImportLocation(const std::string& importPath);

    /**
     * @brief Import all locations from a directory
     * @param importDirectory Directory to import from
     * @return Number of locations imported
     */
    int ImportLocationsFromDirectory(const std::string& importDirectory);

    /**
     * @brief Export all locations to JSON
     * @param pretty Use pretty formatting
     * @return JSON string containing all locations
     */
    [[nodiscard]] std::string ExportAllToJson(bool pretty = true) const;

    /**
     * @brief Import locations from JSON string
     * @param json JSON string
     * @return Number of locations imported
     */
    int ImportFromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for when a location is added
     */
    void SetOnLocationAdded(LocationCallback callback) { m_onLocationAdded = std::move(callback); }

    /**
     * @brief Set callback for when a location is removed
     */
    void SetOnLocationRemoved(LocationCallback callback) { m_onLocationRemoved = std::move(callback); }

    /**
     * @brief Set callback for when a location is modified
     */
    void SetOnLocationModified(LocationCallback callback) { m_onLocationModified = std::move(callback); }

    // =========================================================================
    // Iteration
    // =========================================================================

    /**
     * @brief Iterate over all locations
     * @param callback Function to call for each location
     */
    void ForEach(const std::function<void(LocationDefinition&)>& callback);

    /**
     * @brief Iterate over all enabled locations
     * @param callback Function to call for each enabled location
     */
    void ForEachEnabled(const std::function<void(LocationDefinition&)>& callback);

private:
    void RebuildSpatialIndex();
    void AddToSpatialIndex(LocationDefinition* location);
    void RemoveFromSpatialIndex(LocationDefinition* location);

    bool m_initialized = false;
    std::string m_locationsDirectory;

    // Storage
    std::vector<std::unique_ptr<LocationDefinition>> m_locations;
    std::unordered_map<LocationId, LocationDefinition*> m_locationById;
    std::unordered_map<std::string, LocationDefinition*> m_locationByName;

    // Callbacks
    LocationCallback m_onLocationAdded;
    LocationCallback m_onLocationRemoved;
    LocationCallback m_onLocationModified;
};

} // namespace Vehement
