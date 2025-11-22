#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Vehement {

/**
 * @brief Map file format version
 */
struct MapVersion {
    uint8_t major = 1;
    uint8_t minor = 0;
    uint16_t patch = 0;

    std::string ToString() const;
    static MapVersion FromString(const std::string& str);
    bool IsCompatible(const MapVersion& other) const;
};

/**
 * @brief Map metadata
 */
struct MapMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::string authorId;
    MapVersion version;
    uint64_t createdTime;
    uint64_t modifiedTime;
    std::string thumbnailPath;
    std::vector<std::string> tags;
    int minPlayers;
    int maxPlayers;
    std::string suggestedPlayers;
    std::string tileset;
    glm::ivec2 size;
};

/**
 * @brief Terrain layer data
 */
struct TerrainData {
    std::vector<float> heightmap;
    std::vector<uint8_t> textureIndices;
    std::vector<float> textureBlend;
    std::vector<uint8_t> passabilityMap;
    std::vector<uint8_t> buildabilityMap;
    float waterLevel;
    std::vector<glm::vec4> cliffData;
};

/**
 * @brief Placed object data
 */
struct PlacedObject {
    uint32_t id;
    std::string typeId;
    std::string category;  // unit, building, doodad, item
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    int playerId;
    int variation;
    std::unordered_map<std::string, std::string> properties;
    bool isCustom;
    std::string customData;
};

/**
 * @brief Region definition
 */
struct RegionData {
    uint32_t id;
    std::string name;
    glm::vec3 center;
    glm::vec3 size;
    glm::vec4 color;
    bool isRect;
    float radius;  // For circular regions
    std::string script;  // Associated script/trigger
};

/**
 * @brief Trigger zone
 */
struct TriggerZoneData {
    uint32_t id;
    std::string name;
    glm::vec3 position;
    float radius;
    bool isCircle;
    glm::vec2 rectSize;
    std::vector<uint32_t> linkedTriggers;
};

/**
 * @brief Player start location
 */
struct StartLocation {
    int playerId;
    glm::vec3 position;
    float facing;
    std::string race;  // Forced race, or empty for player choice
    int goldMine;      // ID of nearby gold mine, or -1
};

/**
 * @brief Camera bounds
 */
struct CameraBounds {
    glm::vec2 min;
    glm::vec2 max;
    float minZoom;
    float maxZoom;
};

/**
 * @brief Map File - Save/load custom maps
 *
 * Features:
 * - Binary and JSON formats
 * - Compression support
 * - Backwards compatibility
 * - Validation
 * - Import/export
 */
class MapFile {
public:
    MapFile();
    ~MapFile();

    // File operations
    bool Save(const std::string& filepath);
    bool Load(const std::string& filepath);
    bool SaveJson(const std::string& filepath);
    bool LoadJson(const std::string& filepath);

    // Export/Import
    bool ExportToDirectory(const std::string& directory);
    bool ImportFromDirectory(const std::string& directory);
    std::vector<uint8_t> ToBytes() const;
    bool FromBytes(const std::vector<uint8_t>& data);

    // Metadata
    MapMetadata& GetMetadata() { return m_metadata; }
    const MapMetadata& GetMetadata() const { return m_metadata; }
    void SetMetadata(const MapMetadata& metadata) { m_metadata = metadata; }

    // Terrain
    TerrainData& GetTerrain() { return m_terrain; }
    const TerrainData& GetTerrain() const { return m_terrain; }
    void SetTerrain(const TerrainData& terrain) { m_terrain = terrain; }

    // Objects
    const std::vector<PlacedObject>& GetObjects() const { return m_objects; }
    void AddObject(const PlacedObject& obj);
    void RemoveObject(uint32_t id);
    PlacedObject* GetObject(uint32_t id);
    void ClearObjects();

    // Regions
    const std::vector<RegionData>& GetRegions() const { return m_regions; }
    void AddRegion(const RegionData& region);
    void RemoveRegion(uint32_t id);
    RegionData* GetRegion(uint32_t id);
    void ClearRegions();

    // Trigger zones
    const std::vector<TriggerZoneData>& GetTriggerZones() const { return m_triggerZones; }
    void AddTriggerZone(const TriggerZoneData& zone);
    void RemoveTriggerZone(uint32_t id);
    void ClearTriggerZones();

    // Start locations
    const std::vector<StartLocation>& GetStartLocations() const { return m_startLocations; }
    void SetStartLocations(const std::vector<StartLocation>& locations);
    void AddStartLocation(const StartLocation& location);

    // Camera
    const CameraBounds& GetCameraBounds() const { return m_cameraBounds; }
    void SetCameraBounds(const CameraBounds& bounds) { m_cameraBounds = bounds; }

    // Triggers (reference to trigger file)
    const std::string& GetTriggerScript() const { return m_triggerScript; }
    void SetTriggerScript(const std::string& script) { m_triggerScript = script; }

    // Validation
    bool Validate(std::vector<std::string>& errors) const;
    bool ValidateTerrain(std::vector<std::string>& errors) const;
    bool ValidateObjects(std::vector<std::string>& errors) const;
    bool ValidateStartLocations(std::vector<std::string>& errors) const;

    // Utilities
    void GenerateThumbnail(const std::string& outputPath);
    void CalculateBounds();
    uint32_t GenerateObjectId();
    uint32_t GenerateRegionId();

    // Compression
    void SetCompression(bool enabled) { m_useCompression = enabled; }
    bool IsCompressed() const { return m_useCompression; }

    // Events
    std::function<void(float progress)> OnLoadProgress;
    std::function<void(float progress)> OnSaveProgress;

private:
    bool SaveBinary(const std::string& filepath);
    bool LoadBinary(const std::string& filepath);
    bool CompressData(std::vector<uint8_t>& data);
    bool DecompressData(std::vector<uint8_t>& data);

    void WriteHeader(std::ostream& stream);
    bool ReadHeader(std::istream& stream);
    void WriteTerrain(std::ostream& stream);
    bool ReadTerrain(std::istream& stream);
    void WriteObjects(std::ostream& stream);
    bool ReadObjects(std::istream& stream);
    void WriteRegions(std::ostream& stream);
    bool ReadRegions(std::istream& stream);

    MapMetadata m_metadata;
    TerrainData m_terrain;
    std::vector<PlacedObject> m_objects;
    std::vector<RegionData> m_regions;
    std::vector<TriggerZoneData> m_triggerZones;
    std::vector<StartLocation> m_startLocations;
    CameraBounds m_cameraBounds;
    std::string m_triggerScript;

    bool m_useCompression = true;
    uint32_t m_nextObjectId = 1;
    uint32_t m_nextRegionId = 1;

    static const uint32_t MAGIC_NUMBER = 0x56454D50;  // "VEMP"
    static const uint32_t CURRENT_VERSION = 1;
};

/**
 * @brief Map file utilities
 */
class MapFileUtils {
public:
    static bool IsValidMapFile(const std::string& filepath);
    static MapMetadata ReadMetadataOnly(const std::string& filepath);
    static bool ConvertFormat(const std::string& input, const std::string& output, bool toBinary);
    static std::string GetMapExtension(bool binary = true);
    static std::vector<std::string> GetSupportedExtensions();
};

} // namespace Vehement
