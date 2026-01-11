#pragma once

#include "WorldDatabase.hpp"
#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {

/**
 * @brief Component types for entity serialization
 */
enum class ComponentType : uint32_t {
    Transform = 0x00000001,
    RigidBody = 0x00000002,
    Collider = 0x00000003,
    Mesh = 0x00000004,
    Material = 0x00000005,
    Light = 0x00000006,
    Camera = 0x00000007,
    Script = 0x00000008,
    Animation = 0x00000009,
    Audio = 0x0000000A,
    ParticleSystem = 0x0000000B,
    AI = 0x0000000C,
    Health = 0x0000000D,
    Inventory = 0x0000000E,
    Equipment = 0x0000000F,
    Stats = 0x00000010,
    Skills = 0x00000011,
    Quest = 0x00000012,
    Faction = 0x00000013,
    Custom = 0xFFFFFFFF
};

/**
 * @brief Serialization version info
 */
struct SerializationVersion {
    uint32_t major = 1;
    uint32_t minor = 0;
    uint32_t patch = 0;

    uint32_t ToUInt32() const {
        return (major << 24) | (minor << 16) | patch;
    }

    static SerializationVersion FromUInt32(uint32_t value) {
        SerializationVersion version;
        version.major = (value >> 24) & 0xFF;
        version.minor = (value >> 16) & 0xFF;
        version.patch = value & 0xFFFF;
        return version;
    }
};

/**
 * @brief Component data for serialization
 */
struct ComponentData {
    ComponentType type = ComponentType::Transform;
    std::vector<uint8_t> data;
    uint32_t dataSize = 0;
    std::string componentName;
};

/**
 * @brief Diff entry for incremental updates
 */
struct ComponentDiff {
    ComponentType type;
    bool added = false;
    bool removed = false;
    bool modified = false;
    std::vector<uint8_t> oldData;
    std::vector<uint8_t> newData;
};

/**
 * @brief Serialization statistics
 */
struct SerializationStats {
    size_t totalSize = 0;
    size_t compressedSize = 0;
    float compressionRatio = 0.0f;
    float serializeTime = 0.0f;    // Milliseconds
    float deserializeTime = 0.0f;  // Milliseconds
    size_t componentCount = 0;
};

/**
 * @brief Binary writer for entity serialization
 */
class BinaryWriter {
public:
    BinaryWriter() = default;

    void WriteByte(uint8_t value);
    void WriteInt16(int16_t value);
    void WriteInt32(int32_t value);
    void WriteInt64(int64_t value);
    void WriteUInt16(uint16_t value);
    void WriteUInt32(uint32_t value);
    void WriteUInt64(uint64_t value);
    void WriteFloat(float value);
    void WriteDouble(double value);
    void WriteBool(bool value);
    void WriteString(const std::string& value);
    void WriteVec2(const glm::vec2& value);
    void WriteVec3(const glm::vec3& value);
    void WriteVec4(const glm::vec4& value);
    void WriteQuat(const glm::quat& value);
    void WriteBytes(const void* data, size_t size);

    const std::vector<uint8_t>& GetData() const { return m_data; }
    size_t GetSize() const { return m_data.size(); }
    void Clear() { m_data.clear(); }

private:
    std::vector<uint8_t> m_data;
};

/**
 * @brief Binary reader for entity deserialization
 */
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<uint8_t>& data);
    explicit BinaryReader(const uint8_t* data, size_t size);

    uint8_t ReadByte();
    int16_t ReadInt16();
    int32_t ReadInt32();
    int64_t ReadInt64();
    uint16_t ReadUInt16();
    uint32_t ReadUInt32();
    uint64_t ReadUInt64();
    float ReadFloat();
    double ReadDouble();
    bool ReadBool();
    std::string ReadString();
    glm::vec2 ReadVec2();
    glm::vec3 ReadVec3();
    glm::vec4 ReadVec4();
    glm::quat ReadQuat();
    void ReadBytes(void* buffer, size_t size);

    size_t GetPosition() const { return m_position; }
    size_t GetSize() const { return m_size; }
    bool CanRead(size_t bytes) const { return m_position + bytes <= m_size; }
    bool IsEOF() const { return m_position >= m_size; }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_position = 0;
};

/**
 * @brief Entity serializer with compression and diff support
 *
 * Features:
 * - Component-based serialization
 * - Version compatibility
 * - Compression (zlib)
 * - Diff-based updates for network efficiency
 * - Binary format for compact storage
 *
 * Binary Format:
 * [Header]
 * - Magic Number: uint32 (0x4E4F5645 = "NOVE")
 * - Version: uint32
 * - Flags: uint32 (compression, diff, etc.)
 * - Component Count: uint32
 *
 * [Components]
 * For each component:
 * - Component Type: uint32
 * - Data Size: uint32
 * - Data: bytes
 */
class EntitySerializer {
public:
    static constexpr uint32_t MAGIC_NUMBER = 0x4E4F5645; // "NOVE"

    /**
     * @brief Serialization flags
     */
    enum Flags : uint32_t {
        None = 0,
        Compressed = 1 << 0,
        Diff = 1 << 1,
        Encrypted = 1 << 2
    };

    /**
     * @brief Serialize entity to binary blob
     * @param entity Entity to serialize
     * @param compress Enable compression
     * @return Serialized data
     */
    static std::vector<uint8_t> Serialize(const Entity& entity, bool compress = true);

    /**
     * @brief Deserialize entity from binary blob
     * @param data Serialized data
     * @return Deserialized entity
     */
    static Entity Deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize entity components
     * @param entity Entity to serialize
     * @return Component data map
     */
    static std::map<ComponentType, ComponentData> SerializeComponents(const Entity& entity);

    /**
     * @brief Deserialize entity components
     * @param components Component data map
     * @param entity Entity to populate
     */
    static void DeserializeComponents(const std::map<ComponentType, ComponentData>& components, Entity& entity);

    /**
     * @brief Create diff between two entities (only changed components)
     * @param oldEntity Previous entity state
     * @param newEntity Current entity state
     * @return Diff data
     */
    static std::vector<uint8_t> SerializeDiff(const Entity& oldEntity, const Entity& newEntity);

    /**
     * @brief Apply diff to entity
     * @param entity Entity to update
     * @param diff Diff data
     */
    static void ApplyDiff(Entity& entity, const std::vector<uint8_t>& diff);

    /**
     * @brief Get component diffs between entities
     * @param oldEntity Previous entity state
     * @param newEntity Current entity state
     * @return List of component diffs
     */
    static std::vector<ComponentDiff> GetComponentDiffs(const Entity& oldEntity, const Entity& newEntity);

    /**
     * @brief Compress data using zlib
     * @param data Uncompressed data
     * @return Compressed data
     */
    static std::vector<uint8_t> Compress(const std::vector<uint8_t>& data);

    /**
     * @brief Decompress data using zlib
     * @param compressed Compressed data
     * @return Decompressed data
     */
    static std::vector<uint8_t> Decompress(const std::vector<uint8_t>& compressed);

    /**
     * @brief Compress data using LZ4 (faster)
     * @param data Uncompressed data
     * @return Compressed data
     */
    static std::vector<uint8_t> CompressLZ4(const std::vector<uint8_t>& data);

    /**
     * @brief Decompress data using LZ4
     * @param compressed Compressed data
     * @return Decompressed data
     */
    static std::vector<uint8_t> DecompressLZ4(const std::vector<uint8_t>& compressed);

    /**
     * @brief Calculate checksum (CRC32)
     * @param data Data to checksum
     * @return CRC32 checksum
     */
    static uint32_t CalculateChecksum(const std::vector<uint8_t>& data);

    /**
     * @brief Validate data integrity
     * @param data Data to validate
     * @param expectedChecksum Expected checksum
     * @return True if valid
     */
    static bool ValidateChecksum(const std::vector<uint8_t>& data, uint32_t expectedChecksum);

    /**
     * @brief Get serialization statistics
     * @param data Serialized data
     * @return Statistics
     */
    static SerializationStats GetStats(const std::vector<uint8_t>& data);

    /**
     * @brief Estimate entity size before serialization
     * @param entity Entity to estimate
     * @return Estimated size in bytes
     */
    static size_t EstimateSize(const Entity& entity);

    /**
     * @brief Get current serialization version
     */
    static SerializationVersion GetVersion() { return SerializationVersion{1, 0, 0}; }

private:
    // Component serialization helpers
    static void SerializeTransform(BinaryWriter& writer, const Entity& entity);
    static void DeserializeTransform(BinaryReader& reader, Entity& entity);

    static void SerializeRigidBody(BinaryWriter& writer, const Entity& entity);
    static void DeserializeRigidBody(BinaryReader& reader, Entity& entity);

    static void SerializeCollider(BinaryWriter& writer, const Entity& entity);
    static void DeserializeCollider(BinaryReader& reader, Entity& entity);

    static void SerializeHealth(BinaryWriter& writer, const Entity& entity);
    static void DeserializeHealth(BinaryReader& reader, Entity& entity);

    // Compression helpers
    static size_t GetCompressedSizeZlib(const std::vector<uint8_t>& data);
    static size_t GetCompressedSizeLZ4(const std::vector<uint8_t>& data);
};

/**
 * @brief Player data serializer
 */
class PlayerSerializer {
public:
    /**
     * @brief Serialize player to binary
     * @param player Player to serialize
     * @return Serialized data
     */
    static std::vector<uint8_t> Serialize(const Player& player);

    /**
     * @brief Deserialize player from binary
     * @param data Serialized data
     * @return Deserialized player
     */
    static Player Deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize inventory
     * @param inventory Inventory to serialize
     * @return Serialized data
     */
    static std::vector<uint8_t> SerializeInventory(const std::vector<InventorySlot>& inventory);

    /**
     * @brief Deserialize inventory
     * @param data Serialized data
     * @return Deserialized inventory
     */
    static std::vector<InventorySlot> DeserializeInventory(const std::vector<uint8_t>& data);

    /**
     * @brief Serialize equipment
     * @param equipment Equipment to serialize
     * @return Serialized data
     */
    static std::vector<uint8_t> SerializeEquipment(const std::map<std::string, EquipmentSlot>& equipment);

    /**
     * @brief Deserialize equipment
     * @param data Serialized data
     * @return Deserialized equipment
     */
    static std::map<std::string, EquipmentSlot> DeserializeEquipment(const std::vector<uint8_t>& data);
};

/**
 * @brief Chunk data serializer
 */
class ChunkSerializer {
public:
    /**
     * @brief Serialize chunk to binary
     * @param chunk Chunk to serialize
     * @param compress Enable compression
     * @return Serialized data
     */
    static std::vector<uint8_t> Serialize(const ChunkData& chunk, bool compress = true);

    /**
     * @brief Deserialize chunk from binary
     * @param data Serialized data
     * @return Deserialized chunk
     */
    static ChunkData Deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Compress chunk terrain data
     * @param chunk Chunk to compress
     * @return Chunk with compressed data
     */
    static ChunkData Compress(const ChunkData& chunk);

    /**
     * @brief Decompress chunk terrain data
     * @param chunk Chunk to decompress
     * @return Chunk with decompressed data
     */
    static ChunkData Decompress(const ChunkData& chunk);
};

} // namespace Nova
