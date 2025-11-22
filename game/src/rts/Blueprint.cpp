#include "Blueprint.hpp"
#include "WorldBuilding.hpp"
#include <sstream>
#include <algorithm>
#include <ctime>
#include <random>
#include <fstream>

namespace Vehement {
namespace RTS {

// ============================================================================
// Blueprint Implementation
// ============================================================================

std::string Blueprint::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id << "\",\n";
    ss << "  \"name\": \"" << name << "\",\n";
    ss << "  \"description\": \"" << description << "\",\n";
    ss << "  \"author\": \"" << author << "\",\n";
    ss << "  \"createdTime\": " << createdTime << ",\n";
    ss << "  \"modifiedTime\": " << modifiedTime << ",\n";
    ss << "  \"category\": " << static_cast<int>(category) << ",\n";
    ss << "  \"tags\": " << static_cast<uint32_t>(tags) << ",\n";
    ss << "  \"version\": " << version << ",\n";
    ss << "  \"size\": [" << size.x << ", " << size.y << ", " << size.z << "],\n";
    ss << "  \"origin\": [" << origin.x << ", " << origin.y << ", " << origin.z << "],\n";
    ss << "  \"voxels\": [\n";

    for (size_t i = 0; i < voxels.size(); ++i) {
        ss << "    " << voxels[i].ToJson();
        if (i < voxels.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ],\n";
    ss << "  \"downloads\": " << downloads << ",\n";
    ss << "  \"likes\": " << likes << ",\n";
    ss << "  \"rating\": " << rating << ",\n";
    ss << "  \"ratingCount\": " << ratingCount << "\n";
    ss << "}";

    return ss.str();
}

Blueprint Blueprint::FromJson(const std::string& /* json */) {
    // Simplified - would use proper JSON parsing in production
    Blueprint bp;
    return bp;
}

std::vector<uint8_t> Blueprint::ToBinary() const {
    std::vector<uint8_t> data;

    // Header
    data.push_back('V');
    data.push_back('B');
    data.push_back('P'); // Vehement BluePrint
    data.push_back(static_cast<uint8_t>(version));

    // Size (12 bytes)
    auto writeInt = [&data](int32_t val) {
        data.push_back(static_cast<uint8_t>(val & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    };

    writeInt(size.x);
    writeInt(size.y);
    writeInt(size.z);

    // Voxel count
    writeInt(static_cast<int32_t>(voxels.size()));

    // Voxel data
    for (const auto& v : voxels) {
        writeInt(v.position.x);
        writeInt(v.position.y);
        writeInt(v.position.z);
        data.push_back(static_cast<uint8_t>(v.type));
        uint8_t flags = 0;
        if (v.isWall) flags |= 0x01;
        if (v.isFloor) flags |= 0x02;
        if (v.isRoof) flags |= 0x04;
        if (v.isStairs) flags |= 0x08;
        if (v.isDoor) flags |= 0x10;
        if (v.isWindow) flags |= 0x20;
        if (v.isSupport) flags |= 0x40;
        data.push_back(flags);
    }

    return data;
}

Blueprint Blueprint::FromBinary(const std::vector<uint8_t>& data) {
    Blueprint bp;

    if (data.size() < 20) return bp; // Invalid

    // Check header
    if (data[0] != 'V' || data[1] != 'B' || data[2] != 'P') {
        return bp; // Invalid header
    }

    bp.version = data[3];

    auto readInt = [&data](size_t offset) -> int32_t {
        return static_cast<int32_t>(data[offset]) |
               (static_cast<int32_t>(data[offset + 1]) << 8) |
               (static_cast<int32_t>(data[offset + 2]) << 16) |
               (static_cast<int32_t>(data[offset + 3]) << 24);
    };

    bp.size.x = readInt(4);
    bp.size.y = readInt(8);
    bp.size.z = readInt(12);

    int voxelCount = readInt(16);
    size_t offset = 20;

    for (int i = 0; i < voxelCount && offset + 14 <= data.size(); ++i) {
        Voxel v;
        v.position.x = readInt(offset); offset += 4;
        v.position.y = readInt(offset); offset += 4;
        v.position.z = readInt(offset); offset += 4;
        v.type = static_cast<TileType>(data[offset++]);
        uint8_t flags = data[offset++];
        v.isWall = (flags & 0x01) != 0;
        v.isFloor = (flags & 0x02) != 0;
        v.isRoof = (flags & 0x04) != 0;
        v.isStairs = (flags & 0x08) != 0;
        v.isDoor = (flags & 0x10) != 0;
        v.isWindow = (flags & 0x20) != 0;
        v.isSupport = (flags & 0x40) != 0;
        bp.voxels.push_back(v);
    }

    bp.Recalculate();
    return bp;
}

bool Blueprint::IsValid() const {
    if (name.empty()) return false;
    if (voxels.empty()) return false;
    if (size.x <= 0 || size.y <= 0 || size.z <= 0) return false;
    return true;
}

bool Blueprint::CanPlace(const Voxel3DMap& map, const glm::ivec3& pos) const {
    for (const auto& v : voxels) {
        glm::ivec3 worldPos = v.position + pos;

        if (!map.IsInBounds(worldPos)) {
            return false;
        }

        if (map.IsSolid(worldPos.x, worldPos.y, worldPos.z)) {
            return false; // Already occupied
        }
    }
    return true;
}

std::vector<std::string> Blueprint::GetPlacementIssues(
    const Voxel3DMap& map, const glm::ivec3& pos) const {

    std::vector<std::string> issues;

    int outOfBounds = 0;
    int occupied = 0;

    for (const auto& v : voxels) {
        glm::ivec3 worldPos = v.position + pos;

        if (!map.IsInBounds(worldPos)) {
            ++outOfBounds;
        } else if (map.IsSolid(worldPos.x, worldPos.y, worldPos.z)) {
            ++occupied;
        }
    }

    if (outOfBounds > 0) {
        issues.push_back(std::to_string(outOfBounds) + " voxels out of bounds");
    }

    if (occupied > 0) {
        issues.push_back(std::to_string(occupied) + " positions already occupied");
    }

    return issues;
}

void Blueprint::Rotate90() {
    for (auto& v : voxels) {
        int newX = -v.position.z;
        int newZ = v.position.x;
        v.position.x = newX;
        v.position.z = newZ;

        // Rotate direction
        int newDirX = -v.direction.z;
        int newDirZ = v.direction.x;
        v.direction.x = newDirX;
        v.direction.z = newDirZ;

        v.rotation += 90.0f;
        if (v.rotation >= 360.0f) v.rotation -= 360.0f;
    }

    // Swap size X and Z
    std::swap(size.x, size.z);
    Recalculate();
}

void Blueprint::FlipX() {
    for (auto& v : voxels) {
        v.position.x = -v.position.x;
        v.direction.x = -v.direction.x;
    }
    Recalculate();
}

void Blueprint::FlipZ() {
    for (auto& v : voxels) {
        v.position.z = -v.position.z;
        v.direction.z = -v.direction.z;
    }
    Recalculate();
}

void Blueprint::Recalculate() {
    if (voxels.empty()) {
        size = {0, 0, 0};
        totalCost = ResourceCost();
        materialCounts.clear();
        return;
    }

    // Recalculate bounds
    glm::ivec3 minPos = voxels[0].position;
    glm::ivec3 maxPos = voxels[0].position;

    for (const auto& v : voxels) {
        minPos = glm::min(minPos, v.position);
        maxPos = glm::max(maxPos, v.position);
    }

    size = maxPos - minPos + glm::ivec3(1);

    // Normalize positions to start at origin
    for (auto& v : voxels) {
        v.position -= minPos;
    }

    // Recalculate materials
    materialCounts.clear();
    for (const auto& v : voxels) {
        materialCounts[v.type]++;
    }

    // Recalculate total cost
    totalCost = ResourceCost();
    for (const auto& [type, count] : materialCounts) {
        // Base cost per material type
        if (type >= TileType::Wood1 && type <= TileType::WoodFlooring2) {
            totalCost.Add(ResourceType::Wood, count * 2);
        } else if (type >= TileType::StoneBlack && type <= TileType::StoneRaw) {
            totalCost.Add(ResourceType::Stone, count * 3);
        } else if (type >= TileType::Metal1 && type <= TileType::MetalShopFrontTop) {
            totalCost.Add(ResourceType::Metal, count * 4);
        } else {
            totalCost.Add(ResourceType::Wood, count);
            totalCost.Add(ResourceType::Stone, count);
        }
    }
}

void Blueprint::GeneratePreview() {
    // Would generate an isometric preview image from voxel data
    // This would use software rendering or capture from GPU
    previewTexture = 0;
    previewData.clear();
}

int Blueprint::GetFloorCount() const {
    if (voxels.empty()) return 0;

    int minY = voxels[0].position.y;
    int maxY = minY;

    for (const auto& v : voxels) {
        if (v.isFloor) {
            minY = std::min(minY, v.position.y);
            maxY = std::max(maxY, v.position.y);
        }
    }

    return maxY - minY + 1;
}

TileType Blueprint::GetDominantMaterial() const {
    if (materialCounts.empty()) return TileType::None;

    TileType dominant = TileType::None;
    int maxCount = 0;

    for (const auto& [type, count] : materialCounts) {
        if (count > maxCount) {
            maxCount = count;
            dominant = type;
        }
    }

    return dominant;
}

bool Blueprint::FitsIn(int maxWidth, int maxHeight, int maxDepth) const {
    return size.x <= maxWidth && size.y <= maxHeight && size.z <= maxDepth;
}

// ============================================================================
// BlueprintLibrary Implementation
// ============================================================================

BlueprintLibrary::BlueprintLibrary() = default;

BlueprintLibrary::~BlueprintLibrary() {
    SaveUserBlueprints();
}

void BlueprintLibrary::LoadDefaultBlueprints() {
    m_defaultBlueprints.clear();

    CreateDefaultHouse();
    CreateDefaultWatchTower();
    CreateDefaultWall();
    CreateDefaultFarm();
    CreateDefaultWorkshop();
    CreateDefaultBarracks();
    CreateDefaultFortress();
    CreateDefaultBridge();

    // Add defaults to main list
    for (const auto& bp : m_defaultBlueprints) {
        m_blueprints.push_back(bp);
    }
}

bool BlueprintLibrary::LoadUserBlueprints() {
    std::string path = GetBlueprintsPath();
    // Would scan directory for .vbp files and load them
    return true;
}

bool BlueprintLibrary::SaveUserBlueprints() const {
    // Would save non-default blueprints to disk
    return true;
}

std::string BlueprintLibrary::GetBlueprintsPath() {
    return "data/blueprints/";
}

const Blueprint* BlueprintLibrary::GetBlueprint(const std::string& name) const {
    for (const auto& bp : m_blueprints) {
        if (bp.name == name) return &bp;
    }
    return nullptr;
}

const Blueprint* BlueprintLibrary::GetBlueprintById(const std::string& id) const {
    for (const auto& bp : m_blueprints) {
        if (bp.id == id) return &bp;
    }
    return nullptr;
}

std::vector<std::string> BlueprintLibrary::GetBlueprintNames() const {
    std::vector<std::string> names;
    names.reserve(m_blueprints.size());
    for (const auto& bp : m_blueprints) {
        names.push_back(bp.name);
    }
    return names;
}

std::vector<const Blueprint*> BlueprintLibrary::GetByCategory(BlueprintCategory cat) const {
    std::vector<const Blueprint*> result;
    for (const auto& bp : m_blueprints) {
        if (bp.category == cat) {
            result.push_back(&bp);
        }
    }
    return result;
}

std::vector<const Blueprint*> BlueprintLibrary::GetByTag(BlueprintTag tag) const {
    std::vector<const Blueprint*> result;
    for (const auto& bp : m_blueprints) {
        if (HasTag(bp.tags, tag)) {
            result.push_back(&bp);
        }
    }
    return result;
}

std::vector<const Blueprint*> BlueprintLibrary::Search(const std::string& query) const {
    std::vector<const Blueprint*> result;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& bp : m_blueprints) {
        std::string lowerName = bp.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos) {
            result.push_back(&bp);
        }
    }
    return result;
}

std::vector<const Blueprint*> BlueprintLibrary::GetDefaultBlueprints() const {
    std::vector<const Blueprint*> result;
    for (const auto& bp : m_defaultBlueprints) {
        result.push_back(&bp);
    }
    return result;
}

std::vector<const Blueprint*> BlueprintLibrary::GetUserBlueprints() const {
    std::vector<const Blueprint*> result;
    for (const auto& bp : m_blueprints) {
        // Check if not in defaults
        bool isDefault = false;
        for (const auto& def : m_defaultBlueprints) {
            if (def.id == bp.id) {
                isDefault = true;
                break;
            }
        }
        if (!isDefault) {
            result.push_back(&bp);
        }
    }
    return result;
}

bool BlueprintLibrary::SaveUserBlueprint(const Blueprint& bp) {
    // Check if already exists
    for (auto& existing : m_blueprints) {
        if (existing.name == bp.name) {
            existing = bp;
            existing.modifiedTime = std::time(nullptr);
            return true;
        }
    }

    // Add new
    Blueprint newBp = bp;
    newBp.id = GenerateUUID();
    newBp.createdTime = std::time(nullptr);
    newBp.modifiedTime = newBp.createdTime;
    m_blueprints.push_back(newBp);
    return true;
}

bool BlueprintLibrary::UpdateUserBlueprint(const std::string& name, const Blueprint& bp) {
    for (auto& existing : m_blueprints) {
        if (existing.name == name) {
            std::string oldId = existing.id;
            int64_t oldCreated = existing.createdTime;
            existing = bp;
            existing.id = oldId;
            existing.createdTime = oldCreated;
            existing.modifiedTime = std::time(nullptr);
            return true;
        }
    }
    return false;
}

bool BlueprintLibrary::DeleteUserBlueprint(const std::string& name) {
    auto it = std::remove_if(m_blueprints.begin(), m_blueprints.end(),
        [&name](const Blueprint& bp) { return bp.name == name; });

    if (it != m_blueprints.end()) {
        m_blueprints.erase(it, m_blueprints.end());
        return true;
    }
    return false;
}

bool BlueprintLibrary::RenameBlueprint(const std::string& oldName, const std::string& newName) {
    for (auto& bp : m_blueprints) {
        if (bp.name == oldName) {
            bp.name = newName;
            bp.modifiedTime = std::time(nullptr);
            return true;
        }
    }
    return false;
}

Blueprint* BlueprintLibrary::DuplicateBlueprint(const std::string& name) {
    const Blueprint* original = GetBlueprint(name);
    if (!original) return nullptr;

    Blueprint copy = *original;
    copy.id = GenerateUUID();
    copy.name = name + " (Copy)";
    copy.createdTime = std::time(nullptr);
    copy.modifiedTime = copy.createdTime;

    m_blueprints.push_back(copy);
    return &m_blueprints.back();
}

// Community functions (stubs - would integrate with Firebase)

void BlueprintLibrary::UploadBlueprint(const Blueprint& /* bp */,
    std::function<void(bool success, const std::string& id)> callback) {
    // Would use FirebaseManager to upload
    if (callback) {
        callback(false, ""); // Stub
    }
}

void BlueprintLibrary::DownloadBlueprint(const std::string& /* id */,
    std::function<void(bool success, const Blueprint& bp)> callback) {
    // Would use FirebaseManager to download
    if (callback) {
        callback(false, Blueprint{});
    }
}

void BlueprintLibrary::BrowseCommunityBlueprints(
    int /* page */, int /* perPage */,
    BlueprintCategory /* categoryFilter */,
    const std::string& /* sortBy */,
    std::function<void(const std::vector<BlueprintInfo>& results)> callback) {
    if (callback) {
        callback({});
    }
}

void BlueprintLibrary::SearchCommunity(
    const std::string& /* query */,
    std::function<void(const std::vector<BlueprintInfo>& results)> callback) {
    if (callback) {
        callback({});
    }
}

void BlueprintLibrary::RateBlueprint(const std::string& /* id */, int /* stars */) {
    // Would send to Firebase
}

void BlueprintLibrary::LikeBlueprint(const std::string& /* id */) {
    // Would send to Firebase
}

void BlueprintLibrary::ReportBlueprint(const std::string& /* id */, const std::string& /* reason */) {
    // Would send to Firebase
}

bool BlueprintLibrary::ExportToFile(const std::string& name, const std::string& filepath) const {
    const Blueprint* bp = GetBlueprint(name);
    if (!bp) return false;

    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;

    auto data = bp->ToBinary();
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool BlueprintLibrary::ImportFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) return false;

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    Blueprint bp = Blueprint::FromBinary(data);
    if (!bp.IsValid()) return false;

    return SaveUserBlueprint(bp);
}

std::string BlueprintLibrary::ExportAsString(const std::string& /* name */) const {
    // Would base64 encode the binary data
    return "";
}

bool BlueprintLibrary::ImportFromString(const std::string& /* data */) {
    // Would base64 decode and import
    return false;
}

std::string BlueprintLibrary::GenerateUUID() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    const char* hex = "0123456789abcdef";
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";

    for (char& c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[(dis(gen) & 0x3) | 0x8];
        }
    }

    return uuid;
}

// Default blueprint creators

void BlueprintLibrary::CreateDefaultHouse() {
    BlueprintBuilder builder("Simple House");
    builder.SetDescription("A basic 5x5 single-story house with a peaked roof")
           .SetCategory(BlueprintCategory::Housing)
           .AddTag(BlueprintTag::Starter)
           .AddTag(BlueprintTag::Small)
           .SetAuthor("System");

    // Floor
    builder.FillFloor(0, 4, 0, 0, 4, TileType::WoodFlooring1);

    // Walls
    builder.BuildWallRect(0, 0, 4, 4, 0, 3, TileType::BricksGrey);

    // Door on south wall
    builder.AddDoor(2, 0, 0);

    // Windows
    builder.AddWindow(2, 1, 4);
    builder.AddWindow(0, 1, 2);
    builder.AddWindow(4, 1, 2);

    // Roof
    for (int z = 0; z <= 4; ++z) {
        for (int x = 0; x <= 4; ++x) {
            builder.AddRoof(x, 3, z, TileType::Wood1);
        }
    }

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultWatchTower() {
    BlueprintBuilder builder("Watch Tower");
    builder.SetDescription("A 3x3 defensive tower, 4 stories tall")
           .SetCategory(BlueprintCategory::Defense)
           .AddTag(BlueprintTag::Starter)
           .AddTag(BlueprintTag::MultiStory)
           .AddTag(BlueprintTag::Defensive)
           .SetAuthor("System");

    // 4 floors
    for (int floor = 0; floor < 4; ++floor) {
        int y = floor * 3;
        builder.FillFloor(0, 2, y, 0, 2, TileType::WoodFlooring1);
        builder.BuildWallRect(0, 0, 2, 2, y, 3, TileType::BricksStacked);
    }

    // Flat roof with crenellations
    for (int z = 0; z <= 2; ++z) {
        for (int x = 0; x <= 2; ++x) {
            builder.AddRoof(x, 12, z, TileType::StoneBlack);
        }
    }

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultWall() {
    BlueprintBuilder builder("Wall Section");
    builder.SetDescription("A 5-tile wall section, 2 tiles high")
           .SetCategory(BlueprintCategory::Defense)
           .AddTag(BlueprintTag::Starter)
           .AddTag(BlueprintTag::Modular)
           .AddTag(BlueprintTag::Defensive)
           .SetAuthor("System");

    builder.BuildWallLine(0, 0, 4, 0, 0, 2, TileType::BricksStacked);

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultFarm() {
    BlueprintBuilder builder("Basic Farm");
    builder.SetDescription("A 6x6 farm plot with shelter")
           .SetCategory(BlueprintCategory::Production)
           .AddTag(BlueprintTag::Starter)
           .AddTag(BlueprintTag::Efficient)
           .SetAuthor("System");

    // Farm plots (ground level)
    for (int z = 0; z < 6; ++z) {
        for (int x = 0; x < 6; ++x) {
            if (x < 2 && z < 2) continue; // Leave corner for shelter
            builder.AddFloor(x, 0, z, TileType::GroundDirt);
        }
    }

    // Small shelter in corner
    builder.BuildWallRect(0, 0, 1, 1, 0, 2, TileType::Wood1);
    builder.AddFloor(0, 0, 0, TileType::WoodFlooring1);
    builder.AddFloor(1, 0, 0, TileType::WoodFlooring1);
    builder.AddFloor(0, 0, 1, TileType::WoodFlooring1);
    builder.AddFloor(1, 0, 1, TileType::WoodFlooring1);

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultWorkshop() {
    BlueprintBuilder builder("Workshop");
    builder.SetDescription("A 4x4 crafting workshop")
           .SetCategory(BlueprintCategory::Production)
           .AddTag(BlueprintTag::Starter)
           .SetAuthor("System");

    builder.FillFloor(0, 3, 0, 0, 3, TileType::ConcreteBlocks1);
    builder.BuildWallRect(0, 0, 3, 3, 0, 3, TileType::BricksGrey);
    builder.AddDoor(1, 0, 0);

    // Flat metal roof
    for (int z = 0; z <= 3; ++z) {
        for (int x = 0; x <= 3; ++x) {
            builder.AddRoof(x, 3, z, TileType::Metal1);
        }
    }

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultBarracks() {
    BlueprintBuilder builder("Barracks");
    builder.SetDescription("Military housing for 8 soldiers")
           .SetCategory(BlueprintCategory::Military)
           .AddTag(BlueprintTag::Medieval)
           .SetAuthor("System");

    builder.FillFloor(0, 5, 0, 0, 3, TileType::WoodFlooring1);
    builder.BuildWallRect(0, 0, 5, 3, 0, 3, TileType::BricksStacked);
    builder.AddDoor(2, 0, 0);
    builder.AddWindow(1, 1, 3);
    builder.AddWindow(4, 1, 3);

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultFortress() {
    BlueprintBuilder builder("Small Fortress");
    builder.SetDescription("A fortified compound with walls and central keep")
           .SetCategory(BlueprintCategory::Defense)
           .AddTag(BlueprintTag::Advanced)
           .AddTag(BlueprintTag::Large)
           .AddTag(BlueprintTag::Defensive)
           .AddTag(BlueprintTag::MultiStory)
           .SetAuthor("System");

    // Outer walls (10x10)
    builder.BuildWallRect(0, 0, 9, 9, 0, 3, TileType::BricksStacked);

    // Gate
    builder.AddDoor(4, 0, 0);
    builder.AddDoor(5, 0, 0);

    // Inner keep (4x4 in center)
    builder.BuildWallRect(3, 3, 6, 6, 0, 4, TileType::StoneMarble1);
    builder.FillFloor(3, 6, 0, 3, 6, TileType::StoneMarble2);

    // Corner towers
    for (int tx = 0; tx <= 9; tx += 9) {
        for (int tz = 0; tz <= 9; tz += 9) {
            for (int y = 0; y < 4; ++y) {
                builder.AddWall(tx, y, tz, TileType::BricksStacked);
            }
        }
    }

    m_defaultBlueprints.push_back(builder.Build());
}

void BlueprintLibrary::CreateDefaultBridge() {
    BlueprintBuilder builder("Wooden Bridge");
    builder.SetDescription("A 6-tile wooden bridge for crossing gaps")
           .SetCategory(BlueprintCategory::Infrastructure)
           .AddTag(BlueprintTag::Starter)
           .AddTag(BlueprintTag::Modular)
           .SetAuthor("System");

    // Bridge deck
    for (int x = 0; x < 6; ++x) {
        builder.AddFloor(x, 0, 0, TileType::WoodFlooring1);
        builder.AddFloor(x, 0, 1, TileType::WoodFlooring1);
    }

    // Railings
    for (int x = 0; x < 6; ++x) {
        builder.AddWall(x, 0, -1, TileType::Wood1, 0, 0, 1);
        builder.AddWall(x, 0, 2, TileType::Wood1, 0, 0, -1);
    }

    m_defaultBlueprints.push_back(builder.Build());
}

// ============================================================================
// BlueprintBuilder Implementation
// ============================================================================

BlueprintBuilder::BlueprintBuilder(const std::string& name) {
    m_blueprint.name = name;
    m_blueprint.createdTime = std::time(nullptr);
    m_blueprint.modifiedTime = m_blueprint.createdTime;
}

BlueprintBuilder& BlueprintBuilder::SetDescription(const std::string& desc) {
    m_blueprint.description = desc;
    return *this;
}

BlueprintBuilder& BlueprintBuilder::SetCategory(BlueprintCategory cat) {
    m_blueprint.category = cat;
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddTag(BlueprintTag tag) {
    m_blueprint.tags = m_blueprint.tags | tag;
    return *this;
}

BlueprintBuilder& BlueprintBuilder::SetAuthor(const std::string& author) {
    m_blueprint.author = author;
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddFloor(int x, int y, int z, TileType type) {
    Voxel v;
    v.position = {x, y, z};
    v.type = type;
    v.isFloor = true;
    m_blueprint.voxels.push_back(v);
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddWall(int x, int y, int z, TileType type,
                                             int dx, int dy, int dz) {
    Voxel v;
    v.position = {x, y, z};
    v.type = type;
    v.isWall = true;
    v.direction = {dx, dy, dz};
    m_blueprint.voxels.push_back(v);
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddRoof(int x, int y, int z, TileType type) {
    Voxel v;
    v.position = {x, y, z};
    v.type = type;
    v.isRoof = true;
    m_blueprint.voxels.push_back(v);
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddDoor(int x, int y, int z) {
    // Find existing wall at position and mark as door
    for (auto& v : m_blueprint.voxels) {
        if (v.position.x == x && v.position.y == y && v.position.z == z && v.isWall) {
            v.isDoor = true;
            return *this;
        }
    }

    // If no wall, add door voxel anyway
    Voxel v;
    v.position = {x, y, z};
    v.type = TileType::Wood1;
    v.isWall = true;
    v.isDoor = true;
    m_blueprint.voxels.push_back(v);
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddWindow(int x, int y, int z) {
    for (auto& v : m_blueprint.voxels) {
        if (v.position.x == x && v.position.y == y && v.position.z == z && v.isWall) {
            v.isWindow = true;
            return *this;
        }
    }

    Voxel v;
    v.position = {x, y, z};
    v.type = TileType::Metal1;
    v.isWall = true;
    v.isWindow = true;
    m_blueprint.voxels.push_back(v);
    return *this;
}

BlueprintBuilder& BlueprintBuilder::AddStairs(int x, int y, int z, int dx, int dy, int dz) {
    Voxel v;
    v.position = {x, y, z};
    v.type = TileType::Wood1;
    v.isStairs = true;
    v.direction = {dx, dy, dz};
    m_blueprint.voxels.push_back(v);
    return *this;
}

BlueprintBuilder& BlueprintBuilder::FillFloor(int minX, int maxX, int y,
                                               int minZ, int maxZ, TileType type) {
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            AddFloor(x, y, z, type);
        }
    }
    return *this;
}

BlueprintBuilder& BlueprintBuilder::BuildWallLine(int x1, int z1, int x2, int z2,
                                                   int y, int height, TileType type) {
    int dx = (x2 > x1) ? 1 : ((x2 < x1) ? -1 : 0);
    int dz = (z2 > z1) ? 1 : ((z2 < z1) ? -1 : 0);

    int x = x1, z = z1;

    while (true) {
        for (int h = 0; h < height; ++h) {
            AddWall(x, y + h, z, type, dz, 0, -dx);
        }

        if (x == x2 && z == z2) break;

        if (x != x2) x += dx;
        if (z != z2) z += dz;
    }

    return *this;
}

BlueprintBuilder& BlueprintBuilder::BuildWallRect(int minX, int minZ, int maxX, int maxZ,
                                                   int y, int height, TileType type) {
    // Four walls
    BuildWallLine(minX, minZ, maxX, minZ, y, height, type);
    BuildWallLine(maxX, minZ, maxX, maxZ, y, height, type);
    BuildWallLine(maxX, maxZ, minX, maxZ, y, height, type);
    BuildWallLine(minX, maxZ, minX, minZ, y, height, type);
    return *this;
}

Blueprint BlueprintBuilder::Build() {
    m_blueprint.Recalculate();
    return m_blueprint;
}

} // namespace RTS
} // namespace Vehement
