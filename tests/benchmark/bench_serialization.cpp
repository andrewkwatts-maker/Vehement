/**
 * @file bench_serialization.cpp
 * @brief Performance benchmarks for serialization systems
 *
 * Tests JSON serialization, binary formats, and compression performance.
 */

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <cstring>

using json = nlohmann::json;

// ============================================================================
// Test Data Structures
// ============================================================================

namespace {

/**
 * @brief Simple transform for serialization tests
 */
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    json ToJson() const {
        return json{
            {"position", {position.x, position.y, position.z}},
            {"rotation", {rotation.x, rotation.y, rotation.z}},
            {"scale", {scale.x, scale.y, scale.z}}
        };
    }

    static Transform FromJson(const json& j) {
        Transform t;
        auto pos = j["position"];
        t.position = glm::vec3(pos[0], pos[1], pos[2]);
        auto rot = j["rotation"];
        t.rotation = glm::vec3(rot[0], rot[1], rot[2]);
        auto scl = j["scale"];
        t.scale = glm::vec3(scl[0], scl[1], scl[2]);
        return t;
    }

    void ToBinary(std::vector<uint8_t>& buffer) const {
        size_t offset = buffer.size();
        buffer.resize(buffer.size() + sizeof(glm::vec3) * 3);
        std::memcpy(buffer.data() + offset, &position, sizeof(glm::vec3));
        std::memcpy(buffer.data() + offset + sizeof(glm::vec3), &rotation, sizeof(glm::vec3));
        std::memcpy(buffer.data() + offset + sizeof(glm::vec3) * 2, &scale, sizeof(glm::vec3));
    }

    static Transform FromBinary(const uint8_t* data) {
        Transform t;
        std::memcpy(&t.position, data, sizeof(glm::vec3));
        std::memcpy(&t.rotation, data + sizeof(glm::vec3), sizeof(glm::vec3));
        std::memcpy(&t.scale, data + sizeof(glm::vec3) * 2, sizeof(glm::vec3));
        return t;
    }
};

/**
 * @brief Entity state for serialization tests
 */
struct EntityState {
    uint32_t id = 0;
    std::string name;
    std::string type;
    Transform transform;
    float health = 100.0f;
    float maxHealth = 100.0f;
    float speed = 5.0f;
    bool active = true;
    std::vector<std::string> tags;
    std::unordered_map<std::string, float> stats;

    json ToJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["type"] = type;
        j["transform"] = transform.ToJson();
        j["health"] = health;
        j["maxHealth"] = maxHealth;
        j["speed"] = speed;
        j["active"] = active;
        j["tags"] = tags;
        j["stats"] = stats;
        return j;
    }

    static EntityState FromJson(const json& j) {
        EntityState e;
        e.id = j["id"];
        e.name = j["name"];
        e.type = j["type"];
        e.transform = Transform::FromJson(j["transform"]);
        e.health = j["health"];
        e.maxHealth = j["maxHealth"];
        e.speed = j["speed"];
        e.active = j["active"];
        e.tags = j["tags"].get<std::vector<std::string>>();
        e.stats = j["stats"].get<std::unordered_map<std::string, float>>();
        return e;
    }
};

/**
 * @brief World state for serialization tests
 */
struct WorldState {
    uint64_t tick = 0;
    double time = 0.0;
    std::string mapName;
    std::vector<EntityState> entities;
    std::unordered_map<std::string, std::string> metadata;

    json ToJson() const {
        json j;
        j["tick"] = tick;
        j["time"] = time;
        j["mapName"] = mapName;
        j["metadata"] = metadata;
        json entitiesJson = json::array();
        for (const auto& e : entities) {
            entitiesJson.push_back(e.ToJson());
        }
        j["entities"] = entitiesJson;
        return j;
    }

    static WorldState FromJson(const json& j) {
        WorldState w;
        w.tick = j["tick"];
        w.time = j["time"];
        w.mapName = j["mapName"];
        w.metadata = j["metadata"].get<std::unordered_map<std::string, std::string>>();
        for (const auto& e : j["entities"]) {
            w.entities.push_back(EntityState::FromJson(e));
        }
        return w;
    }
};

// ============================================================================
// Binary Serialization Helpers
// ============================================================================

class BinaryWriter {
public:
    template<typename T>
    void Write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        size_t offset = m_buffer.size();
        m_buffer.resize(offset + sizeof(T));
        std::memcpy(m_buffer.data() + offset, &value, sizeof(T));
    }

    void WriteString(const std::string& str) {
        Write<uint32_t>(static_cast<uint32_t>(str.size()));
        size_t offset = m_buffer.size();
        m_buffer.resize(offset + str.size());
        std::memcpy(m_buffer.data() + offset, str.data(), str.size());
    }

    void WriteVec3(const glm::vec3& v) {
        Write(v.x);
        Write(v.y);
        Write(v.z);
    }

    const std::vector<uint8_t>& GetBuffer() const { return m_buffer; }
    void Clear() { m_buffer.clear(); }

private:
    std::vector<uint8_t> m_buffer;
};

class BinaryReader {
public:
    explicit BinaryReader(const std::vector<uint8_t>& buffer)
        : m_buffer(buffer), m_offset(0) {}

    template<typename T>
    T Read() {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        T value;
        std::memcpy(&value, m_buffer.data() + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return value;
    }

    std::string ReadString() {
        uint32_t size = Read<uint32_t>();
        std::string str(reinterpret_cast<const char*>(m_buffer.data() + m_offset), size);
        m_offset += size;
        return str;
    }

    glm::vec3 ReadVec3() {
        return glm::vec3(Read<float>(), Read<float>(), Read<float>());
    }

private:
    const std::vector<uint8_t>& m_buffer;
    size_t m_offset;
};

// ============================================================================
// Quantization Helpers
// ============================================================================

/**
 * @brief Quantize float to 16-bit integer
 */
inline int16_t QuantizeFloat16(float value, float min, float max) {
    float normalized = (value - min) / (max - min);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    return static_cast<int16_t>(normalized * 32767.0f);
}

/**
 * @brief Dequantize 16-bit integer to float
 */
inline float DequantizeFloat16(int16_t quantized, float min, float max) {
    float normalized = static_cast<float>(quantized) / 32767.0f;
    return min + normalized * (max - min);
}

/**
 * @brief Quantize float to 8-bit integer
 */
inline uint8_t QuantizeFloat8(float value, float min, float max) {
    float normalized = (value - min) / (max - min);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    return static_cast<uint8_t>(normalized * 255.0f);
}

/**
 * @brief Dequantize 8-bit integer to float
 */
inline float DequantizeFloat8(uint8_t quantized, float min, float max) {
    float normalized = static_cast<float>(quantized) / 255.0f;
    return min + normalized * (max - min);
}

// ============================================================================
// Simple RLE Compression
// ============================================================================

std::vector<uint8_t> RLECompress(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;
    if (data.empty()) return result;

    size_t i = 0;
    while (i < data.size()) {
        uint8_t current = data[i];
        size_t count = 1;

        while (i + count < data.size() && data[i + count] == current && count < 255) {
            count++;
        }

        result.push_back(static_cast<uint8_t>(count));
        result.push_back(current);
        i += count;
    }

    return result;
}

std::vector<uint8_t> RLEDecompress(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result;

    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        uint8_t count = data[i];
        uint8_t value = data[i + 1];
        for (uint8_t j = 0; j < count; j++) {
            result.push_back(value);
        }
    }

    return result;
}

// ============================================================================
// Delta Encoding
// ============================================================================

std::vector<int32_t> DeltaEncode(const std::vector<int32_t>& data) {
    std::vector<int32_t> result;
    if (data.empty()) return result;

    result.push_back(data[0]);
    for (size_t i = 1; i < data.size(); i++) {
        result.push_back(data[i] - data[i - 1]);
    }

    return result;
}

std::vector<int32_t> DeltaDecode(const std::vector<int32_t>& data) {
    std::vector<int32_t> result;
    if (data.empty()) return result;

    result.push_back(data[0]);
    for (size_t i = 1; i < data.size(); i++) {
        result.push_back(result[i - 1] + data[i]);
    }

    return result;
}

// ============================================================================
// Test Data Generation
// ============================================================================

EntityState GenerateEntity(std::mt19937& rng, uint32_t id) {
    std::uniform_real_distribution<float> posDist(-1000.0f, 1000.0f);
    std::uniform_real_distribution<float> rotDist(0.0f, 360.0f);
    std::uniform_real_distribution<float> healthDist(1.0f, 100.0f);

    EntityState e;
    e.id = id;
    e.name = "Entity_" + std::to_string(id);
    e.type = (id % 3 == 0) ? "unit" : ((id % 3 == 1) ? "building" : "resource");
    e.transform.position = glm::vec3(posDist(rng), 0.0f, posDist(rng));
    e.transform.rotation = glm::vec3(0.0f, rotDist(rng), 0.0f);
    e.transform.scale = glm::vec3(1.0f);
    e.health = healthDist(rng);
    e.maxHealth = 100.0f;
    e.speed = 5.0f + (id % 10);
    e.active = true;
    e.tags = {"player_owned", "visible"};
    e.stats["attack"] = 10.0f + id % 20;
    e.stats["defense"] = 5.0f + id % 15;
    e.stats["range"] = 1.0f + id % 5;

    return e;
}

WorldState GenerateWorld(size_t entityCount) {
    std::mt19937 rng(12345);

    WorldState world;
    world.tick = 12345;
    world.time = 123.456;
    world.mapName = "test_map_001";
    world.metadata["version"] = "1.0";
    world.metadata["author"] = "test";

    world.entities.reserve(entityCount);
    for (size_t i = 0; i < entityCount; i++) {
        world.entities.push_back(GenerateEntity(rng, static_cast<uint32_t>(i)));
    }

    return world;
}

} // anonymous namespace

// ============================================================================
// JSON Serialization Benchmarks
// ============================================================================

static void BM_JSON_Serialize_Transform(benchmark::State& state) {
    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    t.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    t.scale = glm::vec3(1.0f);

    for (auto _ : state) {
        json j = t.ToJson();
        benchmark::DoNotOptimize(j);
    }
}
BENCHMARK(BM_JSON_Serialize_Transform);

static void BM_JSON_Deserialize_Transform(benchmark::State& state) {
    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    t.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    json j = t.ToJson();

    for (auto _ : state) {
        Transform result = Transform::FromJson(j);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Deserialize_Transform);

static void BM_JSON_Serialize_Entity(benchmark::State& state) {
    std::mt19937 rng(12345);
    EntityState entity = GenerateEntity(rng, 1);

    for (auto _ : state) {
        json j = entity.ToJson();
        benchmark::DoNotOptimize(j);
    }
}
BENCHMARK(BM_JSON_Serialize_Entity);

static void BM_JSON_Deserialize_Entity(benchmark::State& state) {
    std::mt19937 rng(12345);
    EntityState entity = GenerateEntity(rng, 1);
    json j = entity.ToJson();

    for (auto _ : state) {
        EntityState result = EntityState::FromJson(j);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_JSON_Deserialize_Entity);

static void BM_JSON_ToString_Entity(benchmark::State& state) {
    std::mt19937 rng(12345);
    EntityState entity = GenerateEntity(rng, 1);
    json j = entity.ToJson();

    for (auto _ : state) {
        std::string str = j.dump();
        benchmark::DoNotOptimize(str);
    }
}
BENCHMARK(BM_JSON_ToString_Entity);

static void BM_JSON_FromString_Entity(benchmark::State& state) {
    std::mt19937 rng(12345);
    EntityState entity = GenerateEntity(rng, 1);
    std::string str = entity.ToJson().dump();

    for (auto _ : state) {
        json j = json::parse(str);
        benchmark::DoNotOptimize(j);
    }
}
BENCHMARK(BM_JSON_FromString_Entity);

// ============================================================================
// JSON World State Benchmarks
// ============================================================================

class JSONWorldBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) {
        size_t count = static_cast<size_t>(state.range(0));
        world = GenerateWorld(count);
        worldJson = world.ToJson();
        worldString = worldJson.dump();
    }

    WorldState world;
    json worldJson;
    std::string worldString;
};

BENCHMARK_DEFINE_F(JSONWorldBenchmark, BM_JSON_Serialize_World)(benchmark::State& state) {
    for (auto _ : state) {
        json j = world.ToJson();
        benchmark::DoNotOptimize(j);
    }
    state.SetItemsProcessed(state.iterations() * world.entities.size());
}
BENCHMARK_REGISTER_F(JSONWorldBenchmark, BM_JSON_Serialize_World)->Range(10, 1000);

BENCHMARK_DEFINE_F(JSONWorldBenchmark, BM_JSON_Deserialize_World)(benchmark::State& state) {
    for (auto _ : state) {
        WorldState result = WorldState::FromJson(worldJson);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * world.entities.size());
}
BENCHMARK_REGISTER_F(JSONWorldBenchmark, BM_JSON_Deserialize_World)->Range(10, 1000);

BENCHMARK_DEFINE_F(JSONWorldBenchmark, BM_JSON_ToString_World)(benchmark::State& state) {
    for (auto _ : state) {
        std::string str = worldJson.dump();
        benchmark::DoNotOptimize(str);
    }
    state.SetBytesProcessed(state.iterations() * worldString.size());
}
BENCHMARK_REGISTER_F(JSONWorldBenchmark, BM_JSON_ToString_World)->Range(10, 1000);

BENCHMARK_DEFINE_F(JSONWorldBenchmark, BM_JSON_FromString_World)(benchmark::State& state) {
    for (auto _ : state) {
        json j = json::parse(worldString);
        benchmark::DoNotOptimize(j);
    }
    state.SetBytesProcessed(state.iterations() * worldString.size());
}
BENCHMARK_REGISTER_F(JSONWorldBenchmark, BM_JSON_FromString_World)->Range(10, 1000);

// ============================================================================
// Binary Serialization Benchmarks
// ============================================================================

static void BM_Binary_Serialize_Transform(benchmark::State& state) {
    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    t.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    std::vector<uint8_t> buffer;
    buffer.reserve(64);

    for (auto _ : state) {
        buffer.clear();
        t.ToBinary(buffer);
        benchmark::DoNotOptimize(buffer);
    }
}
BENCHMARK(BM_Binary_Serialize_Transform);

static void BM_Binary_Deserialize_Transform(benchmark::State& state) {
    Transform t;
    t.position = glm::vec3(1.0f, 2.0f, 3.0f);
    t.rotation = glm::vec3(0.0f, 45.0f, 0.0f);
    t.scale = glm::vec3(1.0f);
    std::vector<uint8_t> buffer;
    t.ToBinary(buffer);

    for (auto _ : state) {
        Transform result = Transform::FromBinary(buffer.data());
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Binary_Deserialize_Transform);

class BinaryWorldBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) {
        size_t count = static_cast<size_t>(state.range(0));
        world = GenerateWorld(count);

        // Pre-serialize to binary
        BinaryWriter writer;
        writer.Write<uint64_t>(world.tick);
        writer.Write<double>(world.time);
        writer.WriteString(world.mapName);
        writer.Write<uint32_t>(static_cast<uint32_t>(world.entities.size()));
        for (const auto& entity : world.entities) {
            writer.Write<uint32_t>(entity.id);
            writer.WriteString(entity.name);
            writer.WriteString(entity.type);
            writer.WriteVec3(entity.transform.position);
            writer.WriteVec3(entity.transform.rotation);
            writer.WriteVec3(entity.transform.scale);
            writer.Write<float>(entity.health);
            writer.Write<float>(entity.maxHealth);
            writer.Write<float>(entity.speed);
            writer.Write<bool>(entity.active);
        }
        binaryBuffer = writer.GetBuffer();
    }

    WorldState world;
    std::vector<uint8_t> binaryBuffer;
};

BENCHMARK_DEFINE_F(BinaryWorldBenchmark, BM_Binary_Serialize_World)(benchmark::State& state) {
    BinaryWriter writer;

    for (auto _ : state) {
        writer.Clear();
        writer.Write<uint64_t>(world.tick);
        writer.Write<double>(world.time);
        writer.WriteString(world.mapName);
        writer.Write<uint32_t>(static_cast<uint32_t>(world.entities.size()));
        for (const auto& entity : world.entities) {
            writer.Write<uint32_t>(entity.id);
            writer.WriteString(entity.name);
            writer.WriteString(entity.type);
            writer.WriteVec3(entity.transform.position);
            writer.WriteVec3(entity.transform.rotation);
            writer.WriteVec3(entity.transform.scale);
            writer.Write<float>(entity.health);
            writer.Write<float>(entity.maxHealth);
            writer.Write<float>(entity.speed);
            writer.Write<bool>(entity.active);
        }
        benchmark::DoNotOptimize(writer.GetBuffer());
    }
    state.SetBytesProcessed(state.iterations() * binaryBuffer.size());
}
BENCHMARK_REGISTER_F(BinaryWorldBenchmark, BM_Binary_Serialize_World)->Range(10, 1000);

BENCHMARK_DEFINE_F(BinaryWorldBenchmark, BM_Binary_Deserialize_World)(benchmark::State& state) {
    for (auto _ : state) {
        BinaryReader reader(binaryBuffer);
        WorldState result;
        result.tick = reader.Read<uint64_t>();
        result.time = reader.Read<double>();
        result.mapName = reader.ReadString();
        uint32_t entityCount = reader.Read<uint32_t>();
        result.entities.reserve(entityCount);
        for (uint32_t i = 0; i < entityCount; i++) {
            EntityState entity;
            entity.id = reader.Read<uint32_t>();
            entity.name = reader.ReadString();
            entity.type = reader.ReadString();
            entity.transform.position = reader.ReadVec3();
            entity.transform.rotation = reader.ReadVec3();
            entity.transform.scale = reader.ReadVec3();
            entity.health = reader.Read<float>();
            entity.maxHealth = reader.Read<float>();
            entity.speed = reader.Read<float>();
            entity.active = reader.Read<bool>();
            result.entities.push_back(std::move(entity));
        }
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * binaryBuffer.size());
}
BENCHMARK_REGISTER_F(BinaryWorldBenchmark, BM_Binary_Deserialize_World)->Range(10, 1000);

// ============================================================================
// Quantization Benchmarks
// ============================================================================

static void BM_Quantize_Float16(benchmark::State& state) {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
    std::vector<float> values(1000);
    for (auto& v : values) v = dist(rng);

    for (auto _ : state) {
        for (float v : values) {
            int16_t q = QuantizeFloat16(v, -1000.0f, 1000.0f);
            benchmark::DoNotOptimize(q);
        }
    }
    state.SetItemsProcessed(state.iterations() * values.size());
}
BENCHMARK(BM_Quantize_Float16);

static void BM_Dequantize_Float16(benchmark::State& state) {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int16_t> dist(-32767, 32767);
    std::vector<int16_t> values(1000);
    for (auto& v : values) v = dist(rng);

    for (auto _ : state) {
        for (int16_t v : values) {
            float f = DequantizeFloat16(v, -1000.0f, 1000.0f);
            benchmark::DoNotOptimize(f);
        }
    }
    state.SetItemsProcessed(state.iterations() * values.size());
}
BENCHMARK(BM_Dequantize_Float16);

static void BM_Quantize_Float8(benchmark::State& state) {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> values(1000);
    for (auto& v : values) v = dist(rng);

    for (auto _ : state) {
        for (float v : values) {
            uint8_t q = QuantizeFloat8(v, 0.0f, 1.0f);
            benchmark::DoNotOptimize(q);
        }
    }
    state.SetItemsProcessed(state.iterations() * values.size());
}
BENCHMARK(BM_Quantize_Float8);

// ============================================================================
// Compression Benchmarks
// ============================================================================

class CompressionBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) {
        size_t size = static_cast<size_t>(state.range(0));

        // Generate compressible data (lots of repeated bytes)
        std::mt19937 rng(12345);
        std::uniform_int_distribution<int> dist(0, 10);
        data.resize(size);
        for (size_t i = 0; i < size; i++) {
            // Create runs of same bytes
            data[i] = static_cast<uint8_t>(dist(rng));
        }

        compressedData = RLECompress(data);
    }

    std::vector<uint8_t> data;
    std::vector<uint8_t> compressedData;
};

BENCHMARK_DEFINE_F(CompressionBenchmark, BM_RLE_Compress)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = RLECompress(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK_REGISTER_F(CompressionBenchmark, BM_RLE_Compress)->Range(1024, 1 << 20);

BENCHMARK_DEFINE_F(CompressionBenchmark, BM_RLE_Decompress)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = RLEDecompress(compressedData);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK_REGISTER_F(CompressionBenchmark, BM_RLE_Decompress)->Range(1024, 1 << 20);

// ============================================================================
// Delta Encoding Benchmarks
// ============================================================================

class DeltaEncodingBenchmark : public benchmark::Fixture {
public:
    void SetUp(const benchmark::State& state) {
        size_t size = static_cast<size_t>(state.range(0));

        // Generate monotonically increasing data (good for delta encoding)
        std::mt19937 rng(12345);
        std::uniform_int_distribution<int32_t> deltaDist(1, 100);
        data.resize(size);
        int32_t current = 0;
        for (size_t i = 0; i < size; i++) {
            current += deltaDist(rng);
            data[i] = current;
        }

        encodedData = DeltaEncode(data);
    }

    std::vector<int32_t> data;
    std::vector<int32_t> encodedData;
};

BENCHMARK_DEFINE_F(DeltaEncodingBenchmark, BM_Delta_Encode)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = DeltaEncode(data);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}
BENCHMARK_REGISTER_F(DeltaEncodingBenchmark, BM_Delta_Encode)->Range(1000, 100000);

BENCHMARK_DEFINE_F(DeltaEncodingBenchmark, BM_Delta_Decode)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = DeltaDecode(encodedData);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * encodedData.size());
}
BENCHMARK_REGISTER_F(DeltaEncodingBenchmark, BM_Delta_Decode)->Range(1000, 100000);

// ============================================================================
// JSON vs Binary Comparison
// ============================================================================

static void BM_Compare_JSON_vs_Binary_Serialize_100(benchmark::State& state) {
    WorldState world = GenerateWorld(100);
    bool useJson = state.range(0) == 0;

    if (useJson) {
        for (auto _ : state) {
            json j = world.ToJson();
            std::string str = j.dump();
            benchmark::DoNotOptimize(str);
        }
    } else {
        BinaryWriter writer;
        for (auto _ : state) {
            writer.Clear();
            writer.Write<uint64_t>(world.tick);
            writer.Write<double>(world.time);
            writer.WriteString(world.mapName);
            writer.Write<uint32_t>(static_cast<uint32_t>(world.entities.size()));
            for (const auto& entity : world.entities) {
                writer.Write<uint32_t>(entity.id);
                writer.WriteString(entity.name);
                writer.WriteString(entity.type);
                writer.WriteVec3(entity.transform.position);
                writer.WriteVec3(entity.transform.rotation);
                writer.WriteVec3(entity.transform.scale);
                writer.Write<float>(entity.health);
                writer.Write<float>(entity.maxHealth);
                writer.Write<float>(entity.speed);
                writer.Write<bool>(entity.active);
            }
            benchmark::DoNotOptimize(writer.GetBuffer());
        }
    }
}
BENCHMARK(BM_Compare_JSON_vs_Binary_Serialize_100)
    ->Arg(0)  // JSON
    ->Arg(1); // Binary

// ============================================================================
// Memory Allocation Benchmarks
// ============================================================================

static void BM_String_Allocation_Small(benchmark::State& state) {
    for (auto _ : state) {
        std::string str = "Small string test";
        benchmark::DoNotOptimize(str);
    }
}
BENCHMARK(BM_String_Allocation_Small);

static void BM_String_Allocation_Large(benchmark::State& state) {
    for (auto _ : state) {
        std::string str(1000, 'x');
        benchmark::DoNotOptimize(str);
    }
}
BENCHMARK(BM_String_Allocation_Large);

static void BM_Vector_Reserve_vs_Push(benchmark::State& state) {
    bool useReserve = state.range(0) == 1;
    size_t count = 1000;

    for (auto _ : state) {
        std::vector<int> vec;
        if (useReserve) {
            vec.reserve(count);
        }
        for (size_t i = 0; i < count; i++) {
            vec.push_back(static_cast<int>(i));
        }
        benchmark::DoNotOptimize(vec);
    }
}
BENCHMARK(BM_Vector_Reserve_vs_Push)
    ->Arg(0)  // No reserve
    ->Arg(1); // With reserve

// ============================================================================
// JSON Pretty Print vs Compact
// ============================================================================

static void BM_JSON_Dump_Compact(benchmark::State& state) {
    WorldState world = GenerateWorld(100);
    json j = world.ToJson();

    for (auto _ : state) {
        std::string str = j.dump();
        benchmark::DoNotOptimize(str);
    }
}
BENCHMARK(BM_JSON_Dump_Compact);

static void BM_JSON_Dump_Pretty(benchmark::State& state) {
    WorldState world = GenerateWorld(100);
    json j = world.ToJson();

    for (auto _ : state) {
        std::string str = j.dump(4);  // 4-space indent
        benchmark::DoNotOptimize(str);
    }
}
BENCHMARK(BM_JSON_Dump_Pretty);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
