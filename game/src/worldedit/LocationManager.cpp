#include "LocationManager.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Vehement {

// =============================================================================
// Quadtree Spatial Index Implementation
// =============================================================================

/**
 * @brief Quadtree node for spatial indexing of locations
 */
class QuadtreeNode {
public:
    static constexpr int MAX_OBJECTS = 8;
    static constexpr int MAX_DEPTH = 8;
    static constexpr float MIN_SIZE = 10.0f;

    QuadtreeNode(float x, float z, float width, float height, int depth = 0)
        : m_x(x), m_z(z), m_width(width), m_height(height), m_depth(depth) {}

    void Insert(LocationDefinition* location) {
        // If we have children, insert into them
        if (m_children[0]) {
            int index = GetChildIndex(location);
            if (index != -1) {
                m_children[index]->Insert(location);
                return;
            }
        }

        m_objects.push_back(location);

        // Subdivide if needed
        if (m_objects.size() > MAX_OBJECTS && m_depth < MAX_DEPTH &&
            m_width > MIN_SIZE && m_height > MIN_SIZE) {
            if (!m_children[0]) {
                Subdivide();
            }

            // Re-insert objects into children
            auto it = m_objects.begin();
            while (it != m_objects.end()) {
                int index = GetChildIndex(*it);
                if (index != -1) {
                    m_children[index]->Insert(*it);
                    it = m_objects.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void Remove(LocationDefinition* location) {
        if (m_children[0]) {
            int index = GetChildIndex(location);
            if (index != -1) {
                m_children[index]->Remove(location);
                return;
            }
        }

        auto it = std::find(m_objects.begin(), m_objects.end(), location);
        if (it != m_objects.end()) {
            m_objects.erase(it);
        }
    }

    void Query(const glm::vec2& min, const glm::vec2& max, std::vector<LocationDefinition*>& results) {
        // Check if query bounds intersect this node
        if (max.x < m_x || min.x > m_x + m_width ||
            max.y < m_z || min.y > m_z + m_height) {
            return;
        }

        // Add objects from this node
        for (auto* obj : m_objects) {
            const auto& bounds = obj->GetWorldBounds();
            if (bounds.max.x >= min.x && bounds.min.x <= max.x &&
                bounds.max.z >= min.y && bounds.min.z <= max.y) {
                results.push_back(obj);
            }
        }

        // Query children
        if (m_children[0]) {
            for (auto& child : m_children) {
                child->Query(min, max, results);
            }
        }
    }

    void QueryPoint(const glm::vec2& point, std::vector<LocationDefinition*>& results) {
        // Check if point is in this node
        if (point.x < m_x || point.x > m_x + m_width ||
            point.y < m_z || point.y > m_z + m_height) {
            return;
        }

        // Add objects from this node that contain the point
        for (auto* obj : m_objects) {
            if (obj->GetWorldBounds().Contains2D(point)) {
                results.push_back(obj);
            }
        }

        // Query children
        if (m_children[0]) {
            for (auto& child : m_children) {
                child->QueryPoint(point, results);
            }
        }
    }

    void Clear() {
        m_objects.clear();
        for (auto& child : m_children) {
            if (child) {
                child->Clear();
                child.reset();
            }
        }
    }

private:
    void Subdivide() {
        float halfW = m_width / 2;
        float halfH = m_height / 2;

        m_children[0] = std::make_unique<QuadtreeNode>(m_x, m_z, halfW, halfH, m_depth + 1);
        m_children[1] = std::make_unique<QuadtreeNode>(m_x + halfW, m_z, halfW, halfH, m_depth + 1);
        m_children[2] = std::make_unique<QuadtreeNode>(m_x, m_z + halfH, halfW, halfH, m_depth + 1);
        m_children[3] = std::make_unique<QuadtreeNode>(m_x + halfW, m_z + halfH, halfW, halfH, m_depth + 1);
    }

    int GetChildIndex(LocationDefinition* location) {
        const auto& bounds = location->GetWorldBounds();
        float midX = m_x + m_width / 2;
        float midZ = m_z + m_height / 2;

        bool topQuadrant = bounds.min.z >= midZ;
        bool bottomQuadrant = bounds.max.z < midZ;
        bool leftQuadrant = bounds.max.x < midX;
        bool rightQuadrant = bounds.min.x >= midX;

        if (leftQuadrant) {
            if (bottomQuadrant) return 0;
            if (topQuadrant) return 2;
        } else if (rightQuadrant) {
            if (bottomQuadrant) return 1;
            if (topQuadrant) return 3;
        }

        return -1;  // Object spans multiple quadrants
    }

    float m_x, m_z, m_width, m_height;
    int m_depth;
    std::vector<LocationDefinition*> m_objects;
    std::array<std::unique_ptr<QuadtreeNode>, 4> m_children;
};

// Global spatial index
static std::unique_ptr<QuadtreeNode> s_spatialIndex;

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
// Spatial Index Implementation
// =============================================================================

void LocationManager::RebuildSpatialIndex() {
    // Clear existing index
    if (s_spatialIndex) {
        s_spatialIndex->Clear();
    }

    // Determine world bounds from all locations
    float minX = -10000.0f, minZ = -10000.0f;
    float maxX = 10000.0f, maxZ = 10000.0f;

    if (!m_locations.empty()) {
        minX = maxX = m_locations[0]->GetWorldBounds().min.x;
        minZ = maxZ = m_locations[0]->GetWorldBounds().min.z;

        for (const auto& loc : m_locations) {
            const auto& bounds = loc->GetWorldBounds();
            minX = std::min(minX, bounds.min.x);
            minZ = std::min(minZ, bounds.min.z);
            maxX = std::max(maxX, bounds.max.x);
            maxZ = std::max(maxZ, bounds.max.z);
        }

        // Add some padding
        float padding = 100.0f;
        minX -= padding;
        minZ -= padding;
        maxX += padding;
        maxZ += padding;
    }

    // Create new index with world bounds
    float width = maxX - minX;
    float height = maxZ - minZ;
    s_spatialIndex = std::make_unique<QuadtreeNode>(minX, minZ, width, height, 0);

    // Insert all locations
    for (auto& loc : m_locations) {
        s_spatialIndex->Insert(loc.get());
    }
}

void LocationManager::AddToSpatialIndex(LocationDefinition* location) {
    if (!location) return;

    // Create index if it doesn't exist
    if (!s_spatialIndex) {
        RebuildSpatialIndex();
        return;  // RebuildSpatialIndex already inserted all locations
    }

    s_spatialIndex->Insert(location);
}

void LocationManager::RemoveFromSpatialIndex(LocationDefinition* location) {
    if (!location || !s_spatialIndex) return;

    s_spatialIndex->Remove(location);
}

} // namespace Vehement
