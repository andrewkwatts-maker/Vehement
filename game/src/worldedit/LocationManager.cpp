#include "LocationManager.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Vehement {

// =============================================================================
// Constructor / Destructor
// =============================================================================

LocationManager::LocationManager() = default;

LocationManager::~LocationManager() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

void LocationManager::Initialize(const std::string& locationsDirectory) {
    if (m_initialized) return;

    m_locationsDirectory = locationsDirectory;

    // Create directory structure if it doesn't exist
    fs::create_directories(locationsDirectory);
    fs::create_directories(locationsDirectory + "/manual");
    fs::create_directories(locationsDirectory + "/presets");
    fs::create_directories(locationsDirectory + "/zones");

    m_initialized = true;
}

void LocationManager::Shutdown() {
    if (!m_initialized) return;

    m_locations.clear();
    m_locationById.clear();
    m_locationByName.clear();

    m_initialized = false;
}

// =============================================================================
// Loading / Saving
// =============================================================================

int LocationManager::LoadAllLocations() {
    if (!m_initialized) return 0;

    int totalLoaded = 0;

    // Load from all subdirectories
    totalLoaded += LoadLocationsFromDirectory("manual");
    totalLoaded += LoadLocationsFromDirectory("presets");
    totalLoaded += LoadLocationsFromDirectory("zones");

    // Also load any files in root directory
    for (const auto& entry : fs::directory_iterator(m_locationsDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            if (LoadLocation(entry.path().string())) {
                ++totalLoaded;
            }
        }
    }

    return totalLoaded;
}

int LocationManager::LoadLocationsFromDirectory(const std::string& subdirectory) {
    std::string fullPath = m_locationsDirectory + "/" + subdirectory;

    if (!fs::exists(fullPath)) return 0;

    int loaded = 0;
    for (const auto& entry : fs::recursive_directory_iterator(fullPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            if (LoadLocation(entry.path().string())) {
                ++loaded;
            }
        }
    }

    return loaded;
}

LocationDefinition* LocationManager::LoadLocation(const std::string& filePath) {
    auto location = std::make_unique<LocationDefinition>();

    if (!location->LoadFromFile(filePath)) {
        return nullptr;
    }

    LocationDefinition* ptr = location.get();

    // Add to lookup maps
    m_locationById[ptr->GetId()] = ptr;
    if (!ptr->GetName().empty()) {
        m_locationByName[ptr->GetName()] = ptr;
    }

    m_locations.push_back(std::move(location));

    if (m_onLocationAdded) {
        m_onLocationAdded(*ptr);
    }

    return ptr;
}

bool LocationManager::SaveLocation(LocationDefinition& location, const std::string& filePath) {
    std::string path = filePath.empty() ? location.GetFilePath() : filePath;

    if (path.empty()) {
        // Generate a path based on category
        std::string subdirectory = "manual";
        if (!location.GetCategory().empty()) {
            if (location.GetCategory() == "preset") subdirectory = "presets";
            else if (location.GetCategory() == "zone") subdirectory = "zones";
        }

        // Create filename from name
        std::string filename = location.GetName();
        std::replace(filename.begin(), filename.end(), ' ', '_');
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

        path = m_locationsDirectory + "/" + subdirectory + "/" + filename + ".json";
    }

    // Ensure directory exists
    fs::create_directories(fs::path(path).parent_path());

    if (location.SaveToFile(path)) {
        location.SetFilePath(path);
        return true;
    }

    return false;
}

int LocationManager::SaveAllLocations() {
    int saved = 0;
    for (auto& location : m_locations) {
        if (SaveLocation(*location)) {
            ++saved;
        }
    }
    return saved;
}

void LocationManager::ReloadAllLocations() {
    // Clear existing
    m_locations.clear();
    m_locationById.clear();
    m_locationByName.clear();

    // Reload
    LoadAllLocations();
}

// =============================================================================
// Location Management
// =============================================================================

LocationDefinition* LocationManager::CreateLocation(const std::string& name) {
    auto location = std::make_unique<LocationDefinition>(name);
    LocationDefinition* ptr = location.get();

    m_locationById[ptr->GetId()] = ptr;
    m_locationByName[name] = ptr;

    m_locations.push_back(std::move(location));

    if (m_onLocationAdded) {
        m_onLocationAdded(*ptr);
    }

    return ptr;
}

LocationDefinition* LocationManager::CreateLocationFromSelection(
    const std::string& name,
    const glm::vec3& worldMin,
    const glm::vec3& worldMax) {

    auto* location = CreateLocation(name);
    if (location) {
        location->SetWorldBounds(worldMin, worldMax);
    }
    return location;
}

bool LocationManager::DeleteLocation(LocationId id) {
    auto it = std::find_if(m_locations.begin(), m_locations.end(),
        [id](const auto& loc) { return loc->GetId() == id; });

    if (it == m_locations.end()) {
        return false;
    }

    LocationDefinition* ptr = it->get();

    // Remove from maps
    m_locationById.erase(id);
    m_locationByName.erase(ptr->GetName());

    if (m_onLocationRemoved) {
        m_onLocationRemoved(*ptr);
    }

    m_locations.erase(it);
    return true;
}

bool LocationManager::DeleteLocationAndFile(LocationId id) {
    auto* location = GetLocation(id);
    if (!location) return false;

    std::string filePath = location->GetFilePath();

    if (DeleteLocation(id)) {
        if (!filePath.empty() && fs::exists(filePath)) {
            fs::remove(filePath);
        }
        return true;
    }

    return false;
}

LocationDefinition* LocationManager::GetLocation(LocationId id) {
    auto it = m_locationById.find(id);
    return (it != m_locationById.end()) ? it->second : nullptr;
}

const LocationDefinition* LocationManager::GetLocation(LocationId id) const {
    auto it = m_locationById.find(id);
    return (it != m_locationById.end()) ? it->second : nullptr;
}

// =============================================================================
// Queries by Position
// =============================================================================

std::vector<LocationDefinition*> LocationManager::QueryByPosition(const glm::vec3& point) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        if (location->IsEnabled() && location->ContainsWorldPoint(point)) {
            results.push_back(location.get());
        }
    }

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByPosition2D(const glm::vec2& point) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        if (location->IsEnabled() &&
            location->GetWorldBounds().Contains2D(point)) {
            results.push_back(location.get());
        }
    }

    return results;
}

LocationDefinition* LocationManager::GetPrimaryLocationAt(const glm::vec3& point) {
    LocationDefinition* primary = nullptr;
    PCGPriority highestPriority = PCGPriority::PCGOnly;

    for (auto& location : m_locations) {
        if (location->IsEnabled() && location->ContainsWorldPoint(point)) {
            if (location->GetPCGPriority() < highestPriority ||
                (location->GetPCGPriority() == highestPriority && primary == nullptr)) {
                primary = location.get();
                highestPriority = location->GetPCGPriority();
            }
        }
    }

    return primary;
}

std::vector<LocationQueryResult> LocationManager::QueryByRadius(
    const glm::vec3& center, float radius) {

    std::vector<LocationQueryResult> results;
    float radiusSq = radius * radius;

    for (auto& location : m_locations) {
        if (!location->IsEnabled()) continue;

        const auto& bounds = location->GetWorldBounds();
        glm::vec3 boundsCenter = bounds.GetCenter();

        float distSq = glm::dot(boundsCenter - center, boundsCenter - center);

        // Check if bounds could possibly intersect with sphere
        glm::vec3 size = bounds.GetSize();
        float maxExtent = std::max({size.x, size.y, size.z}) * 0.5f;
        float checkRadiusSq = (radius + maxExtent) * (radius + maxExtent);

        if (distSq <= checkRadiusSq) {
            LocationQueryResult result;
            result.location = location.get();
            result.distance = std::sqrt(distSq);
            results.push_back(result);
        }
    }

    // Sort by distance
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.distance < b.distance; });

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByBounds(const WorldBoundingBox& bounds) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        if (location->IsEnabled() &&
            location->GetWorldBounds().Intersects(bounds)) {
            results.push_back(location.get());
        }
    }

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByGPS(double latitude, double longitude) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        if (location->IsEnabled() && location->ContainsGeoPoint(latitude, longitude)) {
            results.push_back(location.get());
        }
    }

    return results;
}

// =============================================================================
// Queries by Name/Tag
// =============================================================================

LocationDefinition* LocationManager::FindByName(const std::string& name) {
    auto it = m_locationByName.find(name);
    return (it != m_locationByName.end()) ? it->second : nullptr;
}

std::vector<LocationDefinition*> LocationManager::SearchByName(const std::string& searchTerm) {
    std::vector<LocationDefinition*> results;
    std::string searchLower = searchTerm;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (auto& location : m_locations) {
        std::string nameLower = location->GetName();
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        if (nameLower.find(searchLower) != std::string::npos) {
            results.push_back(location.get());
        }
    }

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByTag(const std::string& tag) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        if (location->HasTag(tag)) {
            results.push_back(location.get());
        }
    }

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByTags(const std::vector<std::string>& tags) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        bool hasAll = true;
        for (const auto& tag : tags) {
            if (!location->HasTag(tag)) {
                hasAll = false;
                break;
            }
        }
        if (hasAll) {
            results.push_back(location.get());
        }
    }

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByAnyTag(const std::vector<std::string>& tags) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        for (const auto& tag : tags) {
            if (location->HasTag(tag)) {
                results.push_back(location.get());
                break;
            }
        }
    }

    return results;
}

std::vector<LocationDefinition*> LocationManager::QueryByCategory(const std::string& category) {
    std::vector<LocationDefinition*> results;

    for (auto& location : m_locations) {
        if (location->GetCategory() == category) {
            results.push_back(location.get());
        }
    }

    return results;
}

std::vector<std::string> LocationManager::GetAllTags() const {
    std::vector<std::string> allTags;

    for (const auto& location : m_locations) {
        for (const auto& tag : location->GetTags()) {
            if (std::find(allTags.begin(), allTags.end(), tag) == allTags.end()) {
                allTags.push_back(tag);
            }
        }
    }

    std::sort(allTags.begin(), allTags.end());
    return allTags;
}

std::vector<std::string> LocationManager::GetAllCategories() const {
    std::vector<std::string> categories;

    for (const auto& location : m_locations) {
        const auto& cat = location->GetCategory();
        if (!cat.empty() && std::find(categories.begin(), categories.end(), cat) == categories.end()) {
            categories.push_back(cat);
        }
    }

    std::sort(categories.begin(), categories.end());
    return categories;
}

// =============================================================================
// Import / Export
// =============================================================================

bool LocationManager::ExportLocation(const LocationDefinition& location,
                                      const std::string& exportPath) {
    return location.SaveToFile(exportPath);
}

int LocationManager::ExportLocations(
    const std::vector<LocationDefinition*>& locations,
    const std::string& exportDirectory) {

    fs::create_directories(exportDirectory);

    int exported = 0;
    for (const auto* location : locations) {
        std::string filename = location->GetName();
        std::replace(filename.begin(), filename.end(), ' ', '_');
        std::string path = exportDirectory + "/" + filename + ".json";

        if (ExportLocation(*location, path)) {
            ++exported;
        }
    }

    return exported;
}

LocationDefinition* LocationManager::ImportLocation(const std::string& importPath) {
    auto location = std::make_unique<LocationDefinition>();

    if (!location->LoadFromFile(importPath)) {
        return nullptr;
    }

    // Check for name collision
    if (m_locationByName.count(location->GetName()) > 0) {
        // Append number to name
        std::string baseName = location->GetName();
        int suffix = 2;
        while (m_locationByName.count(baseName + " " + std::to_string(suffix)) > 0) {
            ++suffix;
        }
        location->SetName(baseName + " " + std::to_string(suffix));
    }

    LocationDefinition* ptr = location.get();

    m_locationById[ptr->GetId()] = ptr;
    m_locationByName[ptr->GetName()] = ptr;

    m_locations.push_back(std::move(location));

    if (m_onLocationAdded) {
        m_onLocationAdded(*ptr);
    }

    return ptr;
}

int LocationManager::ImportLocationsFromDirectory(const std::string& importDirectory) {
    if (!fs::exists(importDirectory)) return 0;

    int imported = 0;
    for (const auto& entry : fs::recursive_directory_iterator(importDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            if (ImportLocation(entry.path().string())) {
                ++imported;
            }
        }
    }

    return imported;
}

std::string LocationManager::ExportAllToJson(bool pretty) const {
    std::ostringstream ss;

    ss << "{" << (pretty ? "\n" : "");
    ss << (pretty ? "  " : "") << "\"locations\": [" << (pretty ? "\n" : "");

    for (size_t i = 0; i < m_locations.size(); ++i) {
        if (i > 0) ss << "," << (pretty ? "\n" : "");

        std::string locJson = m_locations[i]->ToJson(pretty);

        // Indent each line if pretty
        if (pretty) {
            std::istringstream iss(locJson);
            std::string line;
            bool first = true;
            while (std::getline(iss, line)) {
                if (!first) ss << "\n";
                ss << "    " << line;
                first = false;
            }
        } else {
            ss << locJson;
        }
    }

    ss << (pretty ? "\n  " : "") << "]" << (pretty ? "\n" : "");
    ss << "}";

    return ss.str();
}

int LocationManager::ImportFromJson(const std::string& json) {
    // Simple parser for the array format
    size_t pos = json.find("\"locations\"");
    if (pos == std::string::npos) return 0;

    pos = json.find('[', pos);
    if (pos == std::string::npos) return 0;

    int imported = 0;
    int braceCount = 0;
    size_t objectStart = std::string::npos;

    for (size_t i = pos + 1; i < json.size(); ++i) {
        if (json[i] == '{') {
            if (braceCount == 0) {
                objectStart = i;
            }
            ++braceCount;
        }
        else if (json[i] == '}') {
            --braceCount;
            if (braceCount == 0 && objectStart != std::string::npos) {
                std::string objectJson = json.substr(objectStart, i - objectStart + 1);

                auto location = std::make_unique<LocationDefinition>();
                if (location->FromJson(objectJson)) {
                    LocationDefinition* ptr = location.get();
                    m_locationById[ptr->GetId()] = ptr;
                    m_locationByName[ptr->GetName()] = ptr;
                    m_locations.push_back(std::move(location));
                    ++imported;

                    if (m_onLocationAdded) {
                        m_onLocationAdded(*ptr);
                    }
                }

                objectStart = std::string::npos;
            }
        }
        else if (json[i] == ']' && braceCount == 0) {
            break;
        }
    }

    return imported;
}

// =============================================================================
// Iteration
// =============================================================================

void LocationManager::ForEach(const std::function<void(LocationDefinition&)>& callback) {
    for (auto& location : m_locations) {
        callback(*location);
    }
}

void LocationManager::ForEachEnabled(const std::function<void(LocationDefinition&)>& callback) {
    for (auto& location : m_locations) {
        if (location->IsEnabled()) {
            callback(*location);
        }
    }
}

// =============================================================================
// Spatial Index (placeholder for future optimization)
// =============================================================================

void LocationManager::RebuildSpatialIndex() {
    // TODO: Implement spatial index (R-tree or quadtree) for faster queries
}

void LocationManager::AddToSpatialIndex(LocationDefinition* /*location*/) {
    // TODO: Implement
}

void LocationManager::RemoveFromSpatialIndex(LocationDefinition* /*location*/) {
    // TODO: Implement
}

} // namespace Vehement
