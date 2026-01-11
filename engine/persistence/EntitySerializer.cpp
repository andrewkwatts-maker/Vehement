#include "EntitySerializer.hpp"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <zlib.h>

namespace Nova {

// ============================================================================
// BINARY WRITER
// ============================================================================

void BinaryWriter::WriteByte(uint8_t value) {
    m_data.push_back(value);
}

void BinaryWriter::WriteInt16(int16_t value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteInt32(int32_t value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteInt64(int64_t value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteUInt16(uint16_t value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteUInt32(uint32_t value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteUInt64(uint64_t value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteFloat(float value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteDouble(double value) {
    WriteBytes(&value, sizeof(value));
}

void BinaryWriter::WriteBool(bool value) {
    WriteByte(value ? 1 : 0);
}

void BinaryWriter::WriteString(const std::string& value) {
    WriteUInt32(static_cast<uint32_t>(value.size()));
    if (!value.empty()) {
        WriteBytes(value.data(), value.size());
    }
}

void BinaryWriter::WriteVec2(const glm::vec2& value) {
    WriteFloat(value.x);
    WriteFloat(value.y);
}

void BinaryWriter::WriteVec3(const glm::vec3& value) {
    WriteFloat(value.x);
    WriteFloat(value.y);
    WriteFloat(value.z);
}

void BinaryWriter::WriteVec4(const glm::vec4& value) {
    WriteFloat(value.x);
    WriteFloat(value.y);
    WriteFloat(value.z);
    WriteFloat(value.w);
}

void BinaryWriter::WriteQuat(const glm::quat& value) {
    WriteFloat(value.w);
    WriteFloat(value.x);
    WriteFloat(value.y);
    WriteFloat(value.z);
}

void BinaryWriter::WriteBytes(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    m_data.insert(m_data.end(), bytes, bytes + size);
}

// ============================================================================
// BINARY READER
// ============================================================================

BinaryReader::BinaryReader(const std::vector<uint8_t>& data)
    : m_data(data.data()), m_size(data.size()) {}

BinaryReader::BinaryReader(const uint8_t* data, size_t size)
    : m_data(data), m_size(size) {}

uint8_t BinaryReader::ReadByte() {
    if (!CanRead(1)) return 0;
    return m_data[m_position++];
}

int16_t BinaryReader::ReadInt16() {
    int16_t value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

int32_t BinaryReader::ReadInt32() {
    int32_t value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

int64_t BinaryReader::ReadInt64() {
    int64_t value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

uint16_t BinaryReader::ReadUInt16() {
    uint16_t value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

uint32_t BinaryReader::ReadUInt32() {
    uint32_t value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

uint64_t BinaryReader::ReadUInt64() {
    uint64_t value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

float BinaryReader::ReadFloat() {
    float value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

double BinaryReader::ReadDouble() {
    double value = 0;
    ReadBytes(&value, sizeof(value));
    return value;
}

bool BinaryReader::ReadBool() {
    return ReadByte() != 0;
}

std::string BinaryReader::ReadString() {
    uint32_t length = ReadUInt32();
    if (length == 0 || !CanRead(length)) {
        return "";
    }

    std::string result(length, '\0');
    ReadBytes(&result[0], length);
    return result;
}

glm::vec2 BinaryReader::ReadVec2() {
    glm::vec2 value;
    value.x = ReadFloat();
    value.y = ReadFloat();
    return value;
}

glm::vec3 BinaryReader::ReadVec3() {
    glm::vec3 value;
    value.x = ReadFloat();
    value.y = ReadFloat();
    value.z = ReadFloat();
    return value;
}

glm::vec4 BinaryReader::ReadVec4() {
    glm::vec4 value;
    value.x = ReadFloat();
    value.y = ReadFloat();
    value.z = ReadFloat();
    value.w = ReadFloat();
    return value;
}

glm::quat BinaryReader::ReadQuat() {
    glm::quat value;
    value.w = ReadFloat();
    value.x = ReadFloat();
    value.y = ReadFloat();
    value.z = ReadFloat();
    return value;
}

void BinaryReader::ReadBytes(void* buffer, size_t size) {
    if (!CanRead(size)) return;
    std::memcpy(buffer, m_data + m_position, size);
    m_position += size;
}

// ============================================================================
// ENTITY SERIALIZER
// ============================================================================

std::vector<uint8_t> EntitySerializer::Serialize(const Entity& entity, bool compress) {
    auto start = std::chrono::high_resolution_clock::now();

    BinaryWriter writer;

    // Write header
    writer.WriteUInt32(MAGIC_NUMBER);
    writer.WriteUInt32(GetVersion().ToUInt32());

    uint32_t flags = compress ? Flags::Compressed : Flags::None;
    writer.WriteUInt32(flags);

    // Serialize components
    auto components = SerializeComponents(entity);
    writer.WriteUInt32(static_cast<uint32_t>(components.size()));

    for (const auto& [type, compData] : components) {
        writer.WriteUInt32(static_cast<uint32_t>(type));
        writer.WriteUInt32(static_cast<uint32_t>(compData.data.size()));
        writer.WriteBytes(compData.data.data(), compData.data.size());
    }

    std::vector<uint8_t> result = writer.GetData();

    // Compress if requested
    if (compress) {
        // Keep header uncompressed
        std::vector<uint8_t> header(result.begin(), result.begin() + 12);
        std::vector<uint8_t> payload(result.begin() + 12, result.end());

        std::vector<uint8_t> compressed = Compress(payload);

        result.clear();
        result.insert(result.end(), header.begin(), header.end());
        result.insert(result.end(), compressed.begin(), compressed.end());
    }

    return result;
}

Entity EntitySerializer::Deserialize(const std::vector<uint8_t>& data) {
    Entity entity;
    if (data.size() < 12) return entity;

    BinaryReader reader(data);

    // Read header
    uint32_t magic = reader.ReadUInt32();
    if (magic != MAGIC_NUMBER) {
        return entity;
    }

    uint32_t version = reader.ReadUInt32();
    uint32_t flags = reader.ReadUInt32();

    // Decompress if needed
    std::vector<uint8_t> payload;
    if (flags & Flags::Compressed) {
        std::vector<uint8_t> compressed(data.begin() + 12, data.end());
        payload = Decompress(compressed);
    } else {
        payload.assign(data.begin() + 12, data.end());
    }

    BinaryReader payloadReader(payload);

    // Read components
    uint32_t componentCount = payloadReader.ReadUInt32();
    std::map<ComponentType, ComponentData> components;

    for (uint32_t i = 0; i < componentCount; i++) {
        ComponentType type = static_cast<ComponentType>(payloadReader.ReadUInt32());
        uint32_t size = payloadReader.ReadUInt32();

        ComponentData compData;
        compData.type = type;
        compData.dataSize = size;
        compData.data.resize(size);
        payloadReader.ReadBytes(compData.data.data(), size);

        components[type] = compData;
    }

    DeserializeComponents(components, entity);

    return entity;
}

std::map<ComponentType, ComponentData> EntitySerializer::SerializeComponents(const Entity& entity) {
    std::map<ComponentType, ComponentData> components;

    // Transform component
    {
        BinaryWriter writer;
        SerializeTransform(writer, entity);

        ComponentData compData;
        compData.type = ComponentType::Transform;
        compData.data = writer.GetData();
        compData.dataSize = static_cast<uint32_t>(compData.data.size());
        compData.componentName = "Transform";
        components[ComponentType::Transform] = compData;
    }

    // Health component
    if (entity.health > 0 || entity.maxHealth > 0) {
        BinaryWriter writer;
        SerializeHealth(writer, entity);

        ComponentData compData;
        compData.type = ComponentType::Health;
        compData.data = writer.GetData();
        compData.dataSize = static_cast<uint32_t>(compData.data.size());
        compData.componentName = "Health";
        components[ComponentType::Health] = compData;
    }

    // Custom component data
    if (!entity.data.empty()) {
        ComponentData compData;
        compData.type = ComponentType::Custom;
        compData.data = entity.data;
        compData.dataSize = static_cast<uint32_t>(compData.data.size());
        compData.componentName = "Custom";
        components[ComponentType::Custom] = compData;
    }

    return components;
}

void EntitySerializer::DeserializeComponents(const std::map<ComponentType, ComponentData>& components, Entity& entity) {
    for (const auto& [type, compData] : components) {
        BinaryReader reader(compData.data);

        switch (type) {
            case ComponentType::Transform:
                DeserializeTransform(reader, entity);
                break;

            case ComponentType::Health:
                DeserializeHealth(reader, entity);
                break;

            case ComponentType::Custom:
                entity.data = compData.data;
                break;

            default:
                break;
        }
    }
}

std::vector<uint8_t> EntitySerializer::SerializeDiff(const Entity& oldEntity, const Entity& newEntity) {
    BinaryWriter writer;

    // Write header
    writer.WriteUInt32(MAGIC_NUMBER);
    writer.WriteUInt32(GetVersion().ToUInt32());
    writer.WriteUInt32(Flags::Diff);

    // Get component diffs
    auto diffs = GetComponentDiffs(oldEntity, newEntity);
    writer.WriteUInt32(static_cast<uint32_t>(diffs.size()));

    for (const auto& diff : diffs) {
        writer.WriteUInt32(static_cast<uint32_t>(diff.type));
        writer.WriteBool(diff.added);
        writer.WriteBool(diff.removed);
        writer.WriteBool(diff.modified);

        if (diff.modified || diff.added) {
            writer.WriteUInt32(static_cast<uint32_t>(diff.newData.size()));
            writer.WriteBytes(diff.newData.data(), diff.newData.size());
        }
    }

    return writer.GetData();
}

void EntitySerializer::ApplyDiff(Entity& entity, const std::vector<uint8_t>& diff) {
    if (diff.size() < 12) return;

    BinaryReader reader(diff);

    uint32_t magic = reader.ReadUInt32();
    if (magic != MAGIC_NUMBER) return;

    uint32_t version = reader.ReadUInt32();
    uint32_t flags = reader.ReadUInt32();

    if (!(flags & Flags::Diff)) return;

    uint32_t diffCount = reader.ReadUInt32();

    for (uint32_t i = 0; i < diffCount; i++) {
        ComponentType type = static_cast<ComponentType>(reader.ReadUInt32());
        bool added = reader.ReadBool();
        bool removed = reader.ReadBool();
        bool modified = reader.ReadBool();

        if (modified || added) {
            uint32_t dataSize = reader.ReadUInt32();
            std::vector<uint8_t> data(dataSize);
            reader.ReadBytes(data.data(), dataSize);

            // Apply component data
            BinaryReader compReader(data);
            switch (type) {
                case ComponentType::Transform:
                    DeserializeTransform(compReader, entity);
                    break;
                case ComponentType::Health:
                    DeserializeHealth(compReader, entity);
                    break;
                case ComponentType::Custom:
                    entity.data = data;
                    break;
                default:
                    break;
            }
        }

        if (removed) {
            // Remove component logic
            switch (type) {
                case ComponentType::Custom:
                    entity.data.clear();
                    break;
                default:
                    break;
            }
        }
    }
}

std::vector<ComponentDiff> EntitySerializer::GetComponentDiffs(const Entity& oldEntity, const Entity& newEntity) {
    std::vector<ComponentDiff> diffs;

    auto oldComponents = SerializeComponents(oldEntity);
    auto newComponents = SerializeComponents(newEntity);

    // Check for added/modified components
    for (const auto& [type, newComp] : newComponents) {
        ComponentDiff diff;
        diff.type = type;

        auto oldIt = oldComponents.find(type);
        if (oldIt == oldComponents.end()) {
            // Component added
            diff.added = true;
            diff.newData = newComp.data;
            diffs.push_back(diff);
        } else if (oldIt->second.data != newComp.data) {
            // Component modified
            diff.modified = true;
            diff.oldData = oldIt->second.data;
            diff.newData = newComp.data;
            diffs.push_back(diff);
        }
    }

    // Check for removed components
    for (const auto& [type, oldComp] : oldComponents) {
        if (newComponents.find(type) == newComponents.end()) {
            ComponentDiff diff;
            diff.type = type;
            diff.removed = true;
            diff.oldData = oldComp.data;
            diffs.push_back(diff);
        }
    }

    return diffs;
}

std::vector<uint8_t> EntitySerializer::Compress(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    uLongf compressedSize = compressBound(data.size());
    std::vector<uint8_t> compressed(compressedSize + 4); // +4 for original size

    // Store original size
    uint32_t originalSize = static_cast<uint32_t>(data.size());
    std::memcpy(compressed.data(), &originalSize, 4);

    int result = compress2(
        compressed.data() + 4,
        &compressedSize,
        data.data(),
        data.size(),
        Z_DEFAULT_COMPRESSION
    );

    if (result != Z_OK) {
        return data; // Return uncompressed on error
    }

    compressed.resize(compressedSize + 4);
    return compressed;
}

std::vector<uint8_t> EntitySerializer::Decompress(const std::vector<uint8_t>& compressed) {
    if (compressed.size() < 4) return {};

    // Read original size
    uint32_t originalSize;
    std::memcpy(&originalSize, compressed.data(), 4);

    std::vector<uint8_t> decompressed(originalSize);
    uLongf decompressedSize = originalSize;

    int result = uncompress(
        decompressed.data(),
        &decompressedSize,
        compressed.data() + 4,
        compressed.size() - 4
    );

    if (result != Z_OK) {
        return {}; // Return empty on error
    }

    return decompressed;
}

std::vector<uint8_t> EntitySerializer::CompressLZ4(const std::vector<uint8_t>& data) {
    // LZ4 compression not implemented in this version
    // Would require linking against LZ4 library
    return Compress(data); // Fall back to zlib
}

std::vector<uint8_t> EntitySerializer::DecompressLZ4(const std::vector<uint8_t>& compressed) {
    // LZ4 decompression not implemented in this version
    return Decompress(compressed); // Fall back to zlib
}

uint32_t EntitySerializer::CalculateChecksum(const std::vector<uint8_t>& data) {
    return crc32(0L, data.data(), data.size());
}

bool EntitySerializer::ValidateChecksum(const std::vector<uint8_t>& data, uint32_t expectedChecksum) {
    return CalculateChecksum(data) == expectedChecksum;
}

SerializationStats EntitySerializer::GetStats(const std::vector<uint8_t>& data) {
    SerializationStats stats;

    if (data.size() < 12) return stats;

    BinaryReader reader(data);
    uint32_t magic = reader.ReadUInt32();
    if (magic != MAGIC_NUMBER) return stats;

    reader.ReadUInt32(); // version
    uint32_t flags = reader.ReadUInt32();

    stats.compressedSize = data.size();

    if (flags & Flags::Compressed) {
        // Read original size from compressed data
        if (data.size() >= 16) {
            uint32_t originalSize;
            std::memcpy(&originalSize, data.data() + 12, 4);
            stats.totalSize = originalSize;
        }
    } else {
        stats.totalSize = data.size();
    }

    if (stats.totalSize > 0) {
        stats.compressionRatio = static_cast<float>(stats.compressedSize) / static_cast<float>(stats.totalSize);
    }

    return stats;
}

size_t EntitySerializer::EstimateSize(const Entity& entity) {
    size_t size = 12; // Header

    // Transform: position, rotation, velocity, scale
    size += sizeof(float) * (3 + 4 + 3 + 3);

    // Health
    if (entity.health > 0 || entity.maxHealth > 0) {
        size += sizeof(float) * 2;
    }

    // Custom data
    size += entity.data.size();

    return size;
}

void EntitySerializer::SerializeTransform(BinaryWriter& writer, const Entity& entity) {
    writer.WriteVec3(entity.position);
    writer.WriteQuat(entity.rotation);
    writer.WriteVec3(entity.velocity);
    writer.WriteVec3(entity.scale);
}

void EntitySerializer::DeserializeTransform(BinaryReader& reader, Entity& entity) {
    entity.position = reader.ReadVec3();
    entity.rotation = reader.ReadQuat();
    entity.velocity = reader.ReadVec3();
    entity.scale = reader.ReadVec3();
}

void EntitySerializer::SerializeRigidBody(BinaryWriter& writer, const Entity& entity) {
    writer.WriteVec3(entity.velocity);
    writer.WriteBool(entity.isStatic);
}

void EntitySerializer::DeserializeRigidBody(BinaryReader& reader, Entity& entity) {
    entity.velocity = reader.ReadVec3();
    entity.isStatic = reader.ReadBool();
}

void EntitySerializer::SerializeCollider(BinaryWriter& writer, const Entity& entity) {
    // Placeholder for collider serialization
}

void EntitySerializer::DeserializeCollider(BinaryReader& reader, Entity& entity) {
    // Placeholder for collider deserialization
}

void EntitySerializer::SerializeHealth(BinaryWriter& writer, const Entity& entity) {
    writer.WriteFloat(entity.health);
    writer.WriteFloat(entity.maxHealth);
}

void EntitySerializer::DeserializeHealth(BinaryReader& reader, Entity& entity) {
    entity.health = reader.ReadFloat();
    entity.maxHealth = reader.ReadFloat();
}

size_t EntitySerializer::GetCompressedSizeZlib(const std::vector<uint8_t>& data) {
    return compressBound(data.size());
}

size_t EntitySerializer::GetCompressedSizeLZ4(const std::vector<uint8_t>& data) {
    // LZ4 not implemented
    return GetCompressedSizeZlib(data);
}

// ============================================================================
// PLAYER SERIALIZER
// ============================================================================

std::vector<uint8_t> PlayerSerializer::Serialize(const Player& player) {
    BinaryWriter writer;

    writer.WriteInt32(player.playerId);
    writer.WriteInt32(player.entityId);
    writer.WriteString(player.username);
    writer.WriteString(player.displayName);
    writer.WriteInt32(player.level);
    writer.WriteInt32(player.experience);
    writer.WriteFloat(player.health);
    writer.WriteFloat(player.maxHealth);
    writer.WriteFloat(player.mana);
    writer.WriteFloat(player.maxMana);
    writer.WriteFloat(player.stamina);
    writer.WriteFloat(player.maxStamina);
    writer.WriteFloat(player.hunger);
    writer.WriteFloat(player.thirst);
    writer.WriteInt32(player.deaths);
    writer.WriteInt32(player.kills);
    writer.WriteString(player.faction);
    writer.WriteInt32(player.currencyGold);
    writer.WriteInt32(player.currencySilver);
    writer.WriteInt32(player.currencyPremium);

    return writer.GetData();
}

Player PlayerSerializer::Deserialize(const std::vector<uint8_t>& data) {
    Player player;
    BinaryReader reader(data);

    player.playerId = reader.ReadInt32();
    player.entityId = reader.ReadInt32();
    player.username = reader.ReadString();
    player.displayName = reader.ReadString();
    player.level = reader.ReadInt32();
    player.experience = reader.ReadInt32();
    player.health = reader.ReadFloat();
    player.maxHealth = reader.ReadFloat();
    player.mana = reader.ReadFloat();
    player.maxMana = reader.ReadFloat();
    player.stamina = reader.ReadFloat();
    player.maxStamina = reader.ReadFloat();
    player.hunger = reader.ReadFloat();
    player.thirst = reader.ReadFloat();
    player.deaths = reader.ReadInt32();
    player.kills = reader.ReadInt32();
    player.faction = reader.ReadString();
    player.currencyGold = reader.ReadInt32();
    player.currencySilver = reader.ReadInt32();
    player.currencyPremium = reader.ReadInt32();

    return player;
}

std::vector<uint8_t> PlayerSerializer::SerializeInventory(const std::vector<InventorySlot>& inventory) {
    BinaryWriter writer;

    writer.WriteUInt32(static_cast<uint32_t>(inventory.size()));
    for (const auto& slot : inventory) {
        writer.WriteInt32(slot.slotIndex);
        writer.WriteString(slot.itemId);
        writer.WriteInt32(slot.quantity);
        writer.WriteFloat(slot.durability);
        writer.WriteFloat(slot.maxDurability);
        writer.WriteBool(slot.isEquipped);
        writer.WriteBool(slot.isLocked);
    }

    return writer.GetData();
}

std::vector<InventorySlot> PlayerSerializer::DeserializeInventory(const std::vector<uint8_t>& data) {
    std::vector<InventorySlot> inventory;
    BinaryReader reader(data);

    uint32_t count = reader.ReadUInt32();
    inventory.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        InventorySlot slot;
        slot.slotIndex = reader.ReadInt32();
        slot.itemId = reader.ReadString();
        slot.quantity = reader.ReadInt32();
        slot.durability = reader.ReadFloat();
        slot.maxDurability = reader.ReadFloat();
        slot.isEquipped = reader.ReadBool();
        slot.isLocked = reader.ReadBool();
        inventory.push_back(slot);
    }

    return inventory;
}

std::vector<uint8_t> PlayerSerializer::SerializeEquipment(const std::map<std::string, EquipmentSlot>& equipment) {
    BinaryWriter writer;

    writer.WriteUInt32(static_cast<uint32_t>(equipment.size()));
    for (const auto& [slotName, slot] : equipment) {
        writer.WriteString(slotName);
        writer.WriteString(slot.itemId);
        writer.WriteFloat(slot.durability);
        writer.WriteFloat(slot.maxDurability);
    }

    return writer.GetData();
}

std::map<std::string, EquipmentSlot> PlayerSerializer::DeserializeEquipment(const std::vector<uint8_t>& data) {
    std::map<std::string, EquipmentSlot> equipment;
    BinaryReader reader(data);

    uint32_t count = reader.ReadUInt32();

    for (uint32_t i = 0; i < count; i++) {
        std::string slotName = reader.ReadString();
        EquipmentSlot slot;
        slot.slotName = slotName;
        slot.itemId = reader.ReadString();
        slot.durability = reader.ReadFloat();
        slot.maxDurability = reader.ReadFloat();
        equipment[slotName] = slot;
    }

    return equipment;
}

// ============================================================================
// CHUNK SERIALIZER
// ============================================================================

std::vector<uint8_t> ChunkSerializer::Serialize(const ChunkData& chunk, bool compress) {
    BinaryWriter writer;

    writer.WriteInt32(chunk.chunkX);
    writer.WriteInt32(chunk.chunkY);
    writer.WriteInt32(chunk.chunkZ);
    writer.WriteBool(chunk.isGenerated);
    writer.WriteBool(chunk.isPopulated);

    // Terrain data
    writer.WriteUInt32(static_cast<uint32_t>(chunk.terrainData.size()));
    writer.WriteBytes(chunk.terrainData.data(), chunk.terrainData.size());

    // Biome data
    writer.WriteUInt32(static_cast<uint32_t>(chunk.biomeData.size()));
    writer.WriteBytes(chunk.biomeData.data(), chunk.biomeData.size());

    // Lighting data
    writer.WriteUInt32(static_cast<uint32_t>(chunk.lightingData.size()));
    writer.WriteBytes(chunk.lightingData.data(), chunk.lightingData.size());

    std::vector<uint8_t> result = writer.GetData();

    if (compress) {
        result = EntitySerializer::Compress(result);
    }

    return result;
}

ChunkData ChunkSerializer::Deserialize(const std::vector<uint8_t>& data) {
    ChunkData chunk;

    // Check if compressed (first 4 bytes contain original size)
    std::vector<uint8_t> decompressed = data;
    if (data.size() >= 4) {
        uint32_t possibleSize;
        std::memcpy(&possibleSize, data.data(), 4);
        if (possibleSize > data.size() && possibleSize < 10000000) { // Sanity check
            decompressed = EntitySerializer::Decompress(data);
        }
    }

    BinaryReader reader(decompressed);

    chunk.chunkX = reader.ReadInt32();
    chunk.chunkY = reader.ReadInt32();
    chunk.chunkZ = reader.ReadInt32();
    chunk.isGenerated = reader.ReadBool();
    chunk.isPopulated = reader.ReadBool();

    // Terrain data
    uint32_t terrainSize = reader.ReadUInt32();
    chunk.terrainData.resize(terrainSize);
    reader.ReadBytes(chunk.terrainData.data(), terrainSize);

    // Biome data
    uint32_t biomeSize = reader.ReadUInt32();
    chunk.biomeData.resize(biomeSize);
    reader.ReadBytes(chunk.biomeData.data(), biomeSize);

    // Lighting data
    uint32_t lightSize = reader.ReadUInt32();
    chunk.lightingData.resize(lightSize);
    reader.ReadBytes(chunk.lightingData.data(), lightSize);

    return chunk;
}

ChunkData ChunkSerializer::Compress(const ChunkData& chunk) {
    ChunkData compressed = chunk;
    compressed.terrainData = EntitySerializer::Compress(chunk.terrainData);
    compressed.biomeData = EntitySerializer::Compress(chunk.biomeData);
    compressed.lightingData = EntitySerializer::Compress(chunk.lightingData);
    compressed.compressionType = "zlib";
    compressed.uncompressedSize = chunk.terrainData.size() + chunk.biomeData.size() + chunk.lightingData.size();
    return compressed;
}

ChunkData ChunkSerializer::Decompress(const ChunkData& chunk) {
    ChunkData decompressed = chunk;
    if (chunk.compressionType == "zlib") {
        decompressed.terrainData = EntitySerializer::Decompress(chunk.terrainData);
        decompressed.biomeData = EntitySerializer::Decompress(chunk.biomeData);
        decompressed.lightingData = EntitySerializer::Decompress(chunk.lightingData);
    }
    return decompressed;
}

} // namespace Nova
