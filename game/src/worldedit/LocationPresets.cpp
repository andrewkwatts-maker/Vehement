#include "LocationPresets.hpp"
#include "../world/TileMap.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

LocationPresets::LocationPresets() = default;

// =============================================================================
// Loading / Saving
// =============================================================================

void LocationPresets::Initialize(const std::string& presetsDirectory) {
    m_presetsDirectory = presetsDirectory;

    // Create directory if it doesn't exist
    fs::create_directories(presetsDirectory);

    // Load built-in presets first
    LoadBuiltInPresets();

    // Then load user presets
    LoadAllPresets();
}

int LocationPresets::LoadAllPresets() {
    if (m_presetsDirectory.empty()) return 0;

    int loaded = 0;

    for (const auto& entry : fs::recursive_directory_iterator(m_presetsDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            if (LoadPreset(entry.path().string())) {
                ++loaded;
            }
        }
    }

    return loaded;
}

LocationPreset* LocationPresets::LoadPreset(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return nullptr;

    std::ostringstream ss;
    ss << file.rdbuf();

    LocationPreset preset = PresetFromJson(ss.str());
    preset.filePath = filePath;
    preset.isBuiltIn = false;

    // Check for duplicate
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
        [&preset](const LocationPreset& p) { return p.name == preset.name; });

    if (it != m_presets.end()) {
        *it = preset;
        if (m_onPresetLoaded) {
            m_onPresetLoaded(*it);
        }
        return &(*it);
    }

    m_presets.push_back(preset);

    if (m_onPresetLoaded) {
        m_onPresetLoaded(m_presets.back());
    }

    return &m_presets.back();
}

bool LocationPresets::SavePreset(const LocationPreset& preset, const std::string& filePath) {
    std::string path = filePath.empty() ? preset.filePath : filePath;

    if (path.empty()) {
        // Generate path from name
        std::string filename = preset.name;
        std::replace(filename.begin(), filename.end(), ' ', '_');
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        path = m_presetsDirectory + "/" + filename + ".json";
    }

    // Ensure directory exists
    fs::create_directories(fs::path(path).parent_path());

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << PresetToJson(preset);

    if (m_onPresetSaved) {
        m_onPresetSaved(preset);
    }

    return file.good();
}

void LocationPresets::ReloadPresets() {
    m_presets.clear();
    LoadBuiltInPresets();
    LoadAllPresets();
}

// =============================================================================
// Preset Management
// =============================================================================

std::vector<const LocationPreset*> LocationPresets::GetPresetsByCategory(
    PresetCategory category) const {

    std::vector<const LocationPreset*> result;

    for (const auto& preset : m_presets) {
        if (preset.category == category) {
            result.push_back(&preset);
        }
    }

    return result;
}

std::vector<const LocationPreset*> LocationPresets::GetPresetsByTag(
    const std::string& tag) const {

    std::vector<const LocationPreset*> result;

    for (const auto& preset : m_presets) {
        if (std::find(preset.tags.begin(), preset.tags.end(), tag) != preset.tags.end()) {
            result.push_back(&preset);
        }
    }

    return result;
}

const LocationPreset* LocationPresets::FindPreset(const std::string& name) const {
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
        [&name](const LocationPreset& p) { return p.name == name; });

    return (it != m_presets.end()) ? &(*it) : nullptr;
}

std::vector<PresetCategory> LocationPresets::GetUsedCategories() const {
    std::vector<PresetCategory> categories;

    for (const auto& preset : m_presets) {
        if (std::find(categories.begin(), categories.end(), preset.category) == categories.end()) {
            categories.push_back(preset.category);
        }
    }

    return categories;
}

std::vector<std::string> LocationPresets::GetAllTags() const {
    std::vector<std::string> tags;

    for (const auto& preset : m_presets) {
        for (const auto& tag : preset.tags) {
            if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
                tags.push_back(tag);
            }
        }
    }

    std::sort(tags.begin(), tags.end());
    return tags;
}

// =============================================================================
// Create Preset from Location
// =============================================================================

LocationPreset LocationPresets::CreateFromLocation(
    const LocationDefinition& location,
    const TileMap& map,
    const std::string& name,
    PresetCategory category) {

    LocationPreset preset;
    preset.name = name;
    preset.description = location.GetDescription();
    preset.category = category;
    preset.tags = location.GetTags();

    const auto& bounds = location.GetWorldBounds();
    int minX = static_cast<int>(bounds.min.x);
    int maxX = static_cast<int>(bounds.max.x);
    int minZ = static_cast<int>(bounds.min.z);
    int maxZ = static_cast<int>(bounds.max.z);

    preset.baseSize = glm::ivec2(maxX - minX + 1, maxZ - minZ + 1);
    preset.minSize = preset.baseSize / 2;
    preset.maxSize = preset.baseSize * 2;

    // Extract tiles
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            if (map.IsValidPosition(x, z)) {
                const Tile& tile = map.GetTile(x, z);

                if (tile.type != TileType::None) {
                    PresetTile pt;
                    pt.offset = glm::ivec2(x - minX, z - minZ);
                    pt.type = tile.type;
                    pt.variant = tile.textureVariant;
                    pt.isWall = tile.isWall;
                    pt.wallHeight = tile.wallHeight;
                    preset.tiles.push_back(pt);
                }
            }
        }
    }

    return preset;
}

LocationPreset LocationPresets::CreateFromSelection(
    const glm::ivec2& min,
    const glm::ivec2& max,
    const TileMap& map,
    const std::string& name) {

    LocationPreset preset;
    preset.name = name;
    preset.category = PresetCategory::Custom;

    preset.baseSize = max - min + glm::ivec2(1);

    for (int z = min.y; z <= max.y; ++z) {
        for (int x = min.x; x <= max.x; ++x) {
            if (map.IsValidPosition(x, z)) {
                const Tile& tile = map.GetTile(x, z);

                if (tile.type != TileType::None) {
                    PresetTile pt;
                    pt.offset = glm::ivec2(x - min.x, z - min.y);
                    pt.type = tile.type;
                    pt.variant = tile.textureVariant;
                    pt.isWall = tile.isWall;
                    pt.wallHeight = tile.wallHeight;
                    preset.tiles.push_back(pt);
                }
            }
        }
    }

    return preset;
}

// =============================================================================
// Apply Preset
// =============================================================================

PresetApplyResult LocationPresets::ApplyPreset(
    const LocationPreset& preset,
    const glm::ivec2& position,
    TileMap& map,
    bool centerPlacement) {

    PresetApplyResult result;

    glm::ivec2 origin = position;
    if (centerPlacement) {
        origin -= preset.baseSize / 2;
    }

    // Apply tiles
    for (const auto& pt : preset.tiles) {
        int x = origin.x + pt.offset.x;
        int z = origin.y + pt.offset.y;

        if (map.IsValidPosition(x, z)) {
            Tile& tile = map.GetTile(x, z);
            tile.type = pt.type;
            tile.textureVariant = pt.variant;
            tile.isWall = pt.isWall;
            tile.wallHeight = pt.wallHeight;
            ++result.tilesPlaced;
        }
    }

    result.actualSize = preset.baseSize;
    result.success = true;

    if (m_onPresetApplied) {
        m_onPresetApplied(preset);
    }

    return result;
}

PresetApplyResult LocationPresets::ApplyPresetWithParameters(
    const LocationPreset& preset,
    const glm::ivec2& position,
    TileMap& map,
    const std::unordered_map<std::string, float>& parameters) {

    // Create a copy and apply parameters
    LocationPreset scaledPreset = preset;
    ApplyParameters(scaledPreset, parameters);

    return ApplyPreset(scaledPreset, position, map);
}

std::vector<PresetTile> LocationPresets::PreviewPreset(
    const LocationPreset& preset,
    const glm::ivec2& position) const {

    std::vector<PresetTile> preview;

    glm::ivec2 origin = position - preset.baseSize / 2;

    for (const auto& pt : preset.tiles) {
        PresetTile transformed = pt;
        transformed.offset = glm::ivec2(
            origin.x + pt.offset.x,
            origin.y + pt.offset.y
        );
        preview.push_back(transformed);
    }

    return preview;
}

std::vector<glm::ivec2> LocationPresets::GetAffectedTiles(
    const LocationPreset& preset,
    const glm::ivec2& position) const {

    std::vector<glm::ivec2> tiles;
    glm::ivec2 origin = position - preset.baseSize / 2;

    for (int z = 0; z < preset.baseSize.y; ++z) {
        for (int x = 0; x < preset.baseSize.x; ++x) {
            tiles.emplace_back(origin.x + x, origin.y + z);
        }
    }

    return tiles;
}

// =============================================================================
// Parameters
// =============================================================================

LocationPreset LocationPresets::ScalePreset(
    const LocationPreset& preset,
    const glm::ivec2& newSize) const {

    LocationPreset scaled = preset;
    scaled.tiles = ScaleTiles(preset.tiles, preset.baseSize, newSize);
    scaled.baseSize = newSize;
    return scaled;
}

void LocationPresets::ApplyParameters(
    LocationPreset& preset,
    const std::unordered_map<std::string, float>& parameters) const {

    for (auto& param : preset.parameters) {
        auto it = parameters.find(param.name);
        if (it != parameters.end()) {
            param.valueFloat = it->second;
            param.valueInt = static_cast<int>(it->second);
        }
    }

    // Check for size parameter
    auto sizeIt = parameters.find("size");
    if (sizeIt != parameters.end()) {
        float scale = sizeIt->second;
        glm::ivec2 newSize(
            static_cast<int>(preset.baseSize.x * scale),
            static_cast<int>(preset.baseSize.y * scale)
        );
        newSize = glm::clamp(newSize, preset.minSize, preset.maxSize);
        preset.tiles = ScaleTiles(preset.tiles, preset.baseSize, newSize);
        preset.baseSize = newSize;
    }
}

std::unordered_map<std::string, float> LocationPresets::GetDefaultParameters(
    const LocationPreset& preset) const {

    std::unordered_map<std::string, float> defaults;

    for (const auto& param : preset.parameters) {
        defaults[param.name] = param.defaultFloat;
    }

    return defaults;
}

// =============================================================================
// Built-in Presets
// =============================================================================

void LocationPresets::LoadBuiltInPresets() {
    AddBuiltInPreset(CreateSmallVillagePreset());
    AddBuiltInPreset(CreateMilitaryOutpostPreset());
    AddBuiltInPreset(CreateTradingPostPreset());
    AddBuiltInPreset(CreateRuinsPreset());
}

LocationPreset LocationPresets::CreateSmallVillagePreset() const {
    LocationPreset preset;
    preset.name = "Small Village";
    preset.description = "A small village with basic amenities";
    preset.category = PresetCategory::Town;
    preset.tags = {"village", "starter", "safe"};
    preset.baseSize = glm::ivec2(30, 30);
    preset.minSize = glm::ivec2(20, 20);
    preset.maxSize = glm::ivec2(50, 50);
    preset.isBuiltIn = true;

    // Ground tiles
    for (int z = 0; z < 30; ++z) {
        for (int x = 0; x < 30; ++x) {
            PresetTile tile;
            tile.offset = glm::ivec2(x, z);
            tile.type = TileType::GroundGrass1;
            preset.tiles.push_back(tile);
        }
    }

    // Central road (vertical)
    for (int z = 0; z < 30; ++z) {
        for (int x = 14; x <= 16; ++x) {
            PresetTile tile;
            tile.offset = glm::ivec2(x, z);
            tile.type = TileType::ConcreteAsphalt1;
            preset.tiles.push_back(tile);
        }
    }

    // Horizontal road
    for (int x = 0; x < 30; ++x) {
        for (int z = 14; z <= 16; ++z) {
            PresetTile tile;
            tile.offset = glm::ivec2(x, z);
            tile.type = TileType::ConcreteAsphalt1;
            preset.tiles.push_back(tile);
        }
    }

    // Buildings
    PresetBuilding house1;
    house1.offset = glm::ivec2(5, 5);
    house1.buildingType = "House";
    preset.buildings.push_back(house1);

    PresetBuilding house2;
    house2.offset = glm::ivec2(22, 5);
    house2.buildingType = "House";
    preset.buildings.push_back(house2);

    PresetBuilding shop;
    shop.offset = glm::ivec2(5, 22);
    shop.buildingType = "TradingPost";
    preset.buildings.push_back(shop);

    // Parameters
    PresetParameter sizeParam;
    sizeParam.name = "size";
    sizeParam.displayName = "Size Scale";
    sizeParam.type = PresetParameter::Type::Float;
    sizeParam.defaultFloat = 1.0f;
    sizeParam.minFloat = 0.5f;
    sizeParam.maxFloat = 2.0f;
    preset.parameters.push_back(sizeParam);

    return preset;
}

LocationPreset LocationPresets::CreateMilitaryOutpostPreset() const {
    LocationPreset preset;
    preset.name = "Military Outpost";
    preset.description = "A fortified military position with defensive structures";
    preset.category = PresetCategory::Military;
    preset.tags = {"military", "defense", "fortress"};
    preset.baseSize = glm::ivec2(25, 25);
    preset.minSize = glm::ivec2(15, 15);
    preset.maxSize = glm::ivec2(40, 40);
    preset.isBuiltIn = true;

    // Stone floor
    for (int z = 0; z < 25; ++z) {
        for (int x = 0; x < 25; ++x) {
            PresetTile tile;
            tile.offset = glm::ivec2(x, z);
            tile.type = TileType::StoneMarble1;
            preset.tiles.push_back(tile);
        }
    }

    // Walls around perimeter
    for (int i = 0; i < 25; ++i) {
        // Top wall
        PresetTile top;
        top.offset = glm::ivec2(i, 0);
        top.type = TileType::BricksStacked;
        top.isWall = true;
        top.wallHeight = 3.0f;
        preset.tiles.push_back(top);

        // Bottom wall
        PresetTile bottom;
        bottom.offset = glm::ivec2(i, 24);
        bottom.type = TileType::BricksStacked;
        bottom.isWall = true;
        bottom.wallHeight = 3.0f;
        preset.tiles.push_back(bottom);

        // Left wall
        PresetTile left;
        left.offset = glm::ivec2(0, i);
        left.type = TileType::BricksStacked;
        left.isWall = true;
        left.wallHeight = 3.0f;
        preset.tiles.push_back(left);

        // Right wall
        PresetTile right;
        right.offset = glm::ivec2(24, i);
        right.type = TileType::BricksStacked;
        right.isWall = true;
        right.wallHeight = 3.0f;
        preset.tiles.push_back(right);
    }

    // Gate opening
    for (int i = 11; i <= 13; ++i) {
        PresetTile gate;
        gate.offset = glm::ivec2(i, 24);
        gate.type = TileType::ConcreteAsphalt1;
        gate.isWall = false;
        preset.tiles.push_back(gate);
    }

    // Watch towers
    PresetBuilding tower1;
    tower1.offset = glm::ivec2(2, 2);
    tower1.buildingType = "WatchTower";
    preset.buildings.push_back(tower1);

    PresetBuilding tower2;
    tower2.offset = glm::ivec2(22, 2);
    tower2.buildingType = "WatchTower";
    preset.buildings.push_back(tower2);

    return preset;
}

LocationPreset LocationPresets::CreateTradingPostPreset() const {
    LocationPreset preset;
    preset.name = "Trading Post";
    preset.description = "A small trading hub for merchants";
    preset.category = PresetCategory::Commercial;
    preset.tags = {"trade", "merchant", "shop"};
    preset.baseSize = glm::ivec2(15, 15);
    preset.isBuiltIn = true;

    // Wood flooring
    for (int z = 0; z < 15; ++z) {
        for (int x = 0; x < 15; ++x) {
            PresetTile tile;
            tile.offset = glm::ivec2(x, z);
            tile.type = TileType::WoodFlooring1;
            preset.tiles.push_back(tile);
        }
    }

    PresetBuilding tradingPost;
    tradingPost.offset = glm::ivec2(5, 5);
    tradingPost.buildingType = "TradingPost";
    preset.buildings.push_back(tradingPost);

    PresetEntity merchant;
    merchant.offset = glm::vec2(7.5f, 7.5f);
    merchant.entityType = "NPC";
    merchant.template_ = "Merchant";
    preset.entities.push_back(merchant);

    return preset;
}

LocationPreset LocationPresets::CreateRuinsPreset() const {
    LocationPreset preset;
    preset.name = "Ancient Ruins";
    preset.description = "Crumbling ruins of an ancient structure";
    preset.category = PresetCategory::Ruins;
    preset.tags = {"ruins", "ancient", "exploration"};
    preset.baseSize = glm::ivec2(20, 20);
    preset.isBuiltIn = true;

    // Scattered stone
    for (int z = 0; z < 20; ++z) {
        for (int x = 0; x < 20; ++x) {
            PresetTile tile;
            tile.offset = glm::ivec2(x, z);
            tile.type = (x + z) % 3 == 0 ? TileType::StoneRaw : TileType::GroundDirt;
            preset.tiles.push_back(tile);
        }
    }

    // Broken walls
    for (int i = 3; i < 17; i += 2) {
        if (i % 4 != 0) {
            PresetTile wall;
            wall.offset = glm::ivec2(3, i);
            wall.type = TileType::BricksRock;
            wall.isWall = true;
            wall.wallHeight = 1.5f;
            preset.tiles.push_back(wall);
        }
    }

    return preset;
}

// =============================================================================
// Serialization
// =============================================================================

std::string LocationPresets::PresetToJson(const LocationPreset& preset) const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << preset.name << "\",\n";
    ss << "  \"description\": \"" << preset.description << "\",\n";
    ss << "  \"category\": " << static_cast<int>(preset.category) << ",\n";
    ss << "  \"baseSize\": {\"x\": " << preset.baseSize.x << ", \"y\": " << preset.baseSize.y << "},\n";

    // Tags
    ss << "  \"tags\": [";
    for (size_t i = 0; i < preset.tags.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << preset.tags[i] << "\"";
    }
    ss << "],\n";

    // Tiles
    ss << "  \"tiles\": [\n";
    for (size_t i = 0; i < preset.tiles.size(); ++i) {
        const auto& t = preset.tiles[i];
        if (i > 0) ss << ",\n";
        ss << "    {\"x\": " << t.offset.x << ", \"z\": " << t.offset.y
           << ", \"type\": " << static_cast<int>(t.type)
           << ", \"wall\": " << (t.isWall ? "true" : "false")
           << ", \"height\": " << t.wallHeight << "}";
    }
    ss << "\n  ]\n";

    ss << "}";
    return ss.str();
}

LocationPreset LocationPresets::PresetFromJson(const std::string& /*json*/) const {
    // Simplified - would parse JSON in real implementation
    LocationPreset preset;
    return preset;
}

// =============================================================================
// Private Helpers
// =============================================================================

void LocationPresets::AddBuiltInPreset(const LocationPreset& preset) {
    m_presets.push_back(preset);
}

std::vector<PresetTile> LocationPresets::ScaleTiles(
    const std::vector<PresetTile>& tiles,
    const glm::ivec2& originalSize,
    const glm::ivec2& newSize) const {

    std::vector<PresetTile> scaled;

    float scaleX = static_cast<float>(newSize.x) / originalSize.x;
    float scaleZ = static_cast<float>(newSize.y) / originalSize.y;

    for (const auto& tile : tiles) {
        PresetTile newTile = tile;
        newTile.offset.x = static_cast<int>(tile.offset.x * scaleX);
        newTile.offset.y = static_cast<int>(tile.offset.y * scaleZ);
        scaled.push_back(newTile);
    }

    return scaled;
}

} // namespace Vehement
