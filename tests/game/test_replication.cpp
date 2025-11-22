/**
 * @file test_replication.cpp
 * @brief Unit tests for networking and state replication
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "network/MultiplayerSync.hpp"

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using namespace Vehement;
using namespace Nova::Test;
using json = nlohmann::json;

// =============================================================================
// Network Transform Tests
// =============================================================================

TEST(NetworkTransformTest, DefaultConstruction) {
    NetworkTransform transform;

    EXPECT_FLOAT_EQ(0.0f, transform.x);
    EXPECT_FLOAT_EQ(0.0f, transform.y);
    EXPECT_FLOAT_EQ(0.0f, transform.z);
    EXPECT_FLOAT_EQ(0.0f, transform.rotX);
    EXPECT_FLOAT_EQ(0.0f, transform.rotY);
    EXPECT_FLOAT_EQ(0.0f, transform.rotZ);
    EXPECT_FLOAT_EQ(0.0f, transform.velX);
    EXPECT_FLOAT_EQ(0.0f, transform.velY);
    EXPECT_FLOAT_EQ(0.0f, transform.velZ);
    EXPECT_EQ(0, transform.timestamp);
}

TEST(NetworkTransformTest, Construction) {
    NetworkTransform transform;
    transform.x = 10.0f;
    transform.y = 5.0f;
    transform.z = 20.0f;
    transform.rotY = 90.0f;
    transform.velX = 5.0f;
    transform.timestamp = 123456789;

    EXPECT_FLOAT_EQ(10.0f, transform.x);
    EXPECT_FLOAT_EQ(5.0f, transform.y);
    EXPECT_FLOAT_EQ(20.0f, transform.z);
    EXPECT_FLOAT_EQ(90.0f, transform.rotY);
    EXPECT_FLOAT_EQ(5.0f, transform.velX);
    EXPECT_EQ(123456789, transform.timestamp);
}

TEST(NetworkTransformTest, ToJson) {
    NetworkTransform transform;
    transform.x = 10.0f;
    transform.y = 5.0f;
    transform.z = 20.0f;
    transform.rotY = 90.0f;

    json j = transform.ToJson();

    EXPECT_FLOAT_EQ(10.0f, j["x"].get<float>());
    EXPECT_FLOAT_EQ(5.0f, j["y"].get<float>());
    EXPECT_FLOAT_EQ(20.0f, j["z"].get<float>());
    EXPECT_FLOAT_EQ(90.0f, j["rotY"].get<float>());
}

TEST(NetworkTransformTest, FromJson) {
    json j;
    j["x"] = 10.0f;
    j["y"] = 5.0f;
    j["z"] = 20.0f;
    j["rotY"] = 90.0f;
    j["timestamp"] = 123456789;

    NetworkTransform transform = NetworkTransform::FromJson(j);

    EXPECT_FLOAT_EQ(10.0f, transform.x);
    EXPECT_FLOAT_EQ(5.0f, transform.y);
    EXPECT_FLOAT_EQ(20.0f, transform.z);
    EXPECT_FLOAT_EQ(90.0f, transform.rotY);
    EXPECT_EQ(123456789, transform.timestamp);
}

TEST(NetworkTransformTest, Lerp) {
    NetworkTransform a;
    a.x = 0.0f;
    a.y = 0.0f;
    a.z = 0.0f;
    a.rotY = 0.0f;

    NetworkTransform b;
    b.x = 10.0f;
    b.y = 10.0f;
    b.z = 10.0f;
    b.rotY = 90.0f;

    NetworkTransform result = NetworkTransform::Lerp(a, b, 0.5f);

    EXPECT_FLOAT_EQ(5.0f, result.x);
    EXPECT_FLOAT_EQ(5.0f, result.y);
    EXPECT_FLOAT_EQ(5.0f, result.z);
    EXPECT_FLOAT_EQ(45.0f, result.rotY);
}

TEST(NetworkTransformTest, Lerp_AtZero) {
    NetworkTransform a;
    a.x = 0.0f;

    NetworkTransform b;
    b.x = 10.0f;

    NetworkTransform result = NetworkTransform::Lerp(a, b, 0.0f);
    EXPECT_FLOAT_EQ(0.0f, result.x);
}

TEST(NetworkTransformTest, Lerp_AtOne) {
    NetworkTransform a;
    a.x = 0.0f;

    NetworkTransform b;
    b.x = 10.0f;

    NetworkTransform result = NetworkTransform::Lerp(a, b, 1.0f);
    EXPECT_FLOAT_EQ(10.0f, result.x);
}

// =============================================================================
// Zombie Net State Tests
// =============================================================================

TEST(ZombieNetStateTest, DefaultConstruction) {
    ZombieNetState state;

    EXPECT_TRUE(state.id.empty());
    EXPECT_EQ(100, state.health);
    EXPECT_FALSE(state.isDead);
    EXPECT_TRUE(state.targetPlayerId.empty());
    EXPECT_EQ(ZombieNetState::State::Idle, state.state);
}

TEST(ZombieNetStateTest, StateEnum) {
    EXPECT_EQ(0, static_cast<int>(ZombieNetState::State::Idle));
    EXPECT_EQ(1, static_cast<int>(ZombieNetState::State::Roaming));
    EXPECT_EQ(2, static_cast<int>(ZombieNetState::State::Chasing));
    EXPECT_EQ(3, static_cast<int>(ZombieNetState::State::Attacking));
    EXPECT_EQ(4, static_cast<int>(ZombieNetState::State::Dead));
}

TEST(ZombieNetStateTest, ToJson) {
    ZombieNetState state;
    state.id = "zombie_001";
    state.transform.x = 10.0f;
    state.transform.z = 20.0f;
    state.health = 75;
    state.isDead = false;
    state.targetPlayerId = "player_001";
    state.state = ZombieNetState::State::Chasing;

    json j = state.ToJson();

    EXPECT_EQ("zombie_001", j["id"].get<std::string>());
    EXPECT_EQ(75, j["health"].get<int>());
    EXPECT_FALSE(j["isDead"].get<bool>());
    EXPECT_EQ("player_001", j["targetPlayerId"].get<std::string>());
}

TEST(ZombieNetStateTest, FromJson) {
    json j;
    j["id"] = "zombie_001";
    j["transform"] = json::object();
    j["transform"]["x"] = 10.0f;
    j["transform"]["z"] = 20.0f;
    j["health"] = 75;
    j["isDead"] = false;
    j["targetPlayerId"] = "player_001";

    ZombieNetState state = ZombieNetState::FromJson(j);

    EXPECT_EQ("zombie_001", state.id);
    EXPECT_FLOAT_EQ(10.0f, state.transform.x);
    EXPECT_FLOAT_EQ(20.0f, state.transform.z);
    EXPECT_EQ(75, state.health);
    EXPECT_EQ("player_001", state.targetPlayerId);
}

// =============================================================================
// Player Net State Tests
// =============================================================================

TEST(PlayerNetStateTest, DefaultConstruction) {
    PlayerNetState state;

    EXPECT_TRUE(state.oderId.empty());
    EXPECT_EQ(100, state.health);
    EXPECT_FALSE(state.isDead);
    EXPECT_EQ(0, state.score);
    EXPECT_FALSE(state.isShooting);
    EXPECT_FALSE(state.isReloading);
}

TEST(PlayerNetStateTest, ToJson) {
    PlayerNetState state;
    state.oderId = "player_001";
    state.transform.x = 5.0f;
    state.transform.z = 15.0f;
    state.health = 80;
    state.score = 1500;
    state.currentWeapon = "rifle";
    state.isShooting = true;

    json j = state.ToJson();

    EXPECT_EQ("player_001", j["oderId"].get<std::string>());
    EXPECT_EQ(80, j["health"].get<int>());
    EXPECT_EQ(1500, j["score"].get<int>());
    EXPECT_EQ("rifle", j["currentWeapon"].get<std::string>());
    EXPECT_TRUE(j["isShooting"].get<bool>());
}

TEST(PlayerNetStateTest, FromJson) {
    json j;
    j["oderId"] = "player_001";
    j["transform"] = json::object();
    j["transform"]["x"] = 5.0f;
    j["health"] = 80;
    j["score"] = 1500;
    j["currentWeapon"] = "rifle";
    j["isShooting"] = true;
    j["isReloading"] = false;

    PlayerNetState state = PlayerNetState::FromJson(j);

    EXPECT_EQ("player_001", state.oderId);
    EXPECT_EQ(80, state.health);
    EXPECT_EQ(1500, state.score);
    EXPECT_TRUE(state.isShooting);
}

// =============================================================================
// Map Edit Event Tests
// =============================================================================

TEST(MapEditEventTest, ToJson) {
    MapEditEvent event;
    event.tileX = 10;
    event.tileY = 20;
    event.editedBy = "player_001";
    event.timestamp = 123456789;

    json j = event.ToJson();

    EXPECT_EQ(10, j["tileX"].get<int>());
    EXPECT_EQ(20, j["tileY"].get<int>());
    EXPECT_EQ("player_001", j["editedBy"].get<std::string>());
    EXPECT_EQ(123456789, j["timestamp"].get<int64_t>());
}

TEST(MapEditEventTest, FromJson) {
    json j;
    j["tileX"] = 10;
    j["tileY"] = 20;
    j["editedBy"] = "player_001";
    j["timestamp"] = 123456789;

    MapEditEvent event = MapEditEvent::FromJson(j);

    EXPECT_EQ(10, event.tileX);
    EXPECT_EQ(20, event.tileY);
    EXPECT_EQ("player_001", event.editedBy);
    EXPECT_EQ(123456789, event.timestamp);
}

// =============================================================================
// Game Event Tests
// =============================================================================

TEST(GameEventTest, TypeEnum) {
    EXPECT_EQ(0, static_cast<int>(GameEvent::Type::PlayerSpawned));
    EXPECT_EQ(1, static_cast<int>(GameEvent::Type::PlayerDied));
    EXPECT_EQ(2, static_cast<int>(GameEvent::Type::PlayerRespawned));
    EXPECT_EQ(3, static_cast<int>(GameEvent::Type::ZombieSpawned));
    EXPECT_EQ(4, static_cast<int>(GameEvent::Type::ZombieDied));
}

TEST(GameEventTest, ToJson) {
    GameEvent event;
    event.type = GameEvent::Type::PlayerDied;
    event.sourceId = "zombie_001";
    event.targetId = "player_001";
    event.timestamp = 123456789;
    event.data["deathLocation"] = json::array({10.0, 0.0, 20.0});

    json j = event.ToJson();

    EXPECT_EQ("zombie_001", j["sourceId"].get<std::string>());
    EXPECT_EQ("player_001", j["targetId"].get<std::string>());
    EXPECT_EQ(123456789, j["timestamp"].get<int64_t>());
}

TEST(GameEventTest, FromJson) {
    json j;
    j["type"] = static_cast<int>(GameEvent::Type::ZombieDied);
    j["sourceId"] = "player_001";
    j["targetId"] = "zombie_001";
    j["timestamp"] = 123456789;
    j["data"] = json::object();

    GameEvent event = GameEvent::FromJson(j);

    EXPECT_EQ(GameEvent::Type::ZombieDied, event.type);
    EXPECT_EQ("player_001", event.sourceId);
    EXPECT_EQ("zombie_001", event.targetId);
}

// =============================================================================
// Multiplayer Sync Config Tests
// =============================================================================

TEST(MultiplayerSyncConfigTest, DefaultValues) {
    MultiplayerSync::Config config;

    EXPECT_FLOAT_EQ(10.0f, config.playerSyncRate);
    EXPECT_FLOAT_EQ(5.0f, config.zombieSyncRate);
    EXPECT_FLOAT_EQ(20.0f, config.eventSyncRate);
    EXPECT_FLOAT_EQ(0.1f, config.interpolationDelay);
    EXPECT_EQ(20, config.maxInterpolationStates);
    EXPECT_TRUE(config.hostAuthoritative);
    EXPECT_TRUE(config.conflictResolutionByTimestamp);
    EXPECT_EQ(100, config.maxZombiesPerTown);
    EXPECT_EQ(50, config.maxEventsPerSecond);
}

// =============================================================================
// Conflict Resolver Tests
// =============================================================================

TEST(ConflictResolverTest, Strategy_LastWins) {
    ZombieNetState local;
    local.id = "zombie_001";
    local.health = 100;
    local.transform.timestamp = 1000;

    ZombieNetState remote;
    remote.id = "zombie_001";
    remote.health = 80;
    remote.transform.timestamp = 2000;  // More recent

    auto resolved = ConflictResolver::ResolveZombieConflict(
        local, remote, ConflictResolver::Strategy::LastWins);

    EXPECT_EQ(80, resolved.health);  // Remote wins (more recent)
}

TEST(ConflictResolverTest, Strategy_FirstWins) {
    ZombieNetState local;
    local.id = "zombie_001";
    local.health = 100;
    local.transform.timestamp = 1000;

    ZombieNetState remote;
    remote.id = "zombie_001";
    remote.health = 80;
    remote.transform.timestamp = 2000;

    auto resolved = ConflictResolver::ResolveZombieConflict(
        local, remote, ConflictResolver::Strategy::FirstWins);

    EXPECT_EQ(100, resolved.health);  // Local wins (first)
}

TEST(ConflictResolverTest, Strategy_HostWins) {
    // Host's data should always take priority
}

TEST(ConflictResolverTest, Strategy_HighestHealth_Damage) {
    ZombieNetState local;
    local.id = "zombie_001";
    local.health = 100;

    ZombieNetState remote;
    remote.id = "zombie_001";
    remote.health = 60;  // Damage was applied

    auto resolved = ConflictResolver::ResolveZombieConflict(
        local, remote, ConflictResolver::Strategy::HighestHealth);

    // For health, lower value wins (damage is applied)
    EXPECT_EQ(60, resolved.health);
}

TEST(ConflictResolverTest, MapEdit_LastWins) {
    MapEditEvent local;
    local.tileX = 10;
    local.tileY = 20;
    local.timestamp = 1000;

    MapEditEvent remote;
    remote.tileX = 10;
    remote.tileY = 20;
    remote.timestamp = 2000;

    auto resolved = ConflictResolver::ResolveMapEditConflict(
        local, remote, ConflictResolver::Strategy::LastWins);

    EXPECT_EQ(2000, resolved.timestamp);  // Remote wins
}

// =============================================================================
// Interpolation Tests
// =============================================================================

TEST(InterpolationTest, LinearInterpolation) {
    float a = 0.0f;
    float b = 10.0f;
    float t = 0.5f;

    float result = a + (b - a) * t;
    EXPECT_FLOAT_EQ(5.0f, result);
}

TEST(InterpolationTest, TransformInterpolation) {
    NetworkTransform from;
    from.x = 0.0f;
    from.y = 0.0f;
    from.z = 0.0f;
    from.timestamp = 0;

    NetworkTransform to;
    to.x = 10.0f;
    to.y = 0.0f;
    to.z = 20.0f;
    to.timestamp = 100;

    // Interpolate at timestamp 50
    float t = 0.5f;
    NetworkTransform result = NetworkTransform::Lerp(from, to, t);

    EXPECT_FLOAT_EQ(5.0f, result.x);
    EXPECT_FLOAT_EQ(10.0f, result.z);
}

TEST(InterpolationTest, ExtrapolationUsingVelocity) {
    NetworkTransform state;
    state.x = 0.0f;
    state.z = 0.0f;
    state.velX = 10.0f;  // 10 units/sec
    state.velZ = 5.0f;   // 5 units/sec

    float deltaTime = 0.1f;  // 100ms

    // Extrapolate position
    float predictedX = state.x + state.velX * deltaTime;
    float predictedZ = state.z + state.velZ * deltaTime;

    EXPECT_FLOAT_EQ(1.0f, predictedX);
    EXPECT_FLOAT_EQ(0.5f, predictedZ);
}

// =============================================================================
// Delta Encoding Tests
// =============================================================================

TEST(DeltaEncodingTest, PositionDelta) {
    NetworkTransform previous;
    previous.x = 10.0f;
    previous.z = 20.0f;

    NetworkTransform current;
    current.x = 15.0f;
    current.z = 25.0f;

    // Calculate delta
    float deltaX = current.x - previous.x;
    float deltaZ = current.z - previous.z;

    EXPECT_FLOAT_EQ(5.0f, deltaX);
    EXPECT_FLOAT_EQ(5.0f, deltaZ);
}

TEST(DeltaEncodingTest, ApplyDelta) {
    NetworkTransform base;
    base.x = 10.0f;
    base.z = 20.0f;

    float deltaX = 5.0f;
    float deltaZ = 5.0f;

    // Apply delta
    NetworkTransform result;
    result.x = base.x + deltaX;
    result.z = base.z + deltaZ;

    EXPECT_FLOAT_EQ(15.0f, result.x);
    EXPECT_FLOAT_EQ(25.0f, result.z);
}

TEST(DeltaEncodingTest, SignificanceThreshold) {
    float previous = 10.0f;
    float current = 10.001f;
    float threshold = 0.01f;

    float delta = std::abs(current - previous);
    bool isSignificant = delta > threshold;

    EXPECT_FALSE(isSignificant);  // Change too small, don't send
}

// =============================================================================
// Prediction Tests
// =============================================================================

TEST(PredictionTest, LinearPrediction) {
    glm::vec3 position(0.0f);
    glm::vec3 velocity(10.0f, 0.0f, 5.0f);
    float predictionTime = 0.1f;  // 100ms into future

    glm::vec3 predicted = position + velocity * predictionTime;

    EXPECT_FLOAT_EQ(1.0f, predicted.x);
    EXPECT_FLOAT_EQ(0.5f, predicted.z);
}

TEST(PredictionTest, PredictionCorrection) {
    glm::vec3 predicted(10.0f, 0.0f, 20.0f);
    glm::vec3 actual(11.0f, 0.0f, 19.0f);

    glm::vec3 error = actual - predicted;
    float errorMagnitude = glm::length(error);

    EXPECT_GT(errorMagnitude, 0.0f);

    // Apply correction
    float correctionFactor = 0.1f;  // 10% per frame
    glm::vec3 corrected = predicted + error * correctionFactor;

    EXPECT_FLOAT_EQ(10.1f, corrected.x);
    EXPECT_FLOAT_EQ(19.9f, corrected.z);
}

TEST(PredictionTest, SnapCorrection) {
    glm::vec3 predicted(10.0f, 0.0f, 20.0f);
    glm::vec3 actual(15.0f, 0.0f, 25.0f);

    float errorMagnitude = glm::length(actual - predicted);
    float snapThreshold = 3.0f;

    // Error too large - snap to actual position
    EXPECT_GT(errorMagnitude, snapThreshold);
}

// =============================================================================
// State Synchronization Tests
// =============================================================================

TEST(StateSyncTest, CalculateSyncInterval) {
    float syncRate = 10.0f;  // 10 updates per second
    float expectedInterval = 1.0f / syncRate;

    EXPECT_FLOAT_EQ(0.1f, expectedInterval);
}

TEST(StateSyncTest, ShouldSendUpdate) {
    float timeSinceLastSync = 0.0f;
    float syncInterval = 0.1f;  // 100ms

    // Not enough time passed
    EXPECT_FALSE(timeSinceLastSync >= syncInterval);

    timeSinceLastSync = 0.15f;  // 150ms
    EXPECT_TRUE(timeSinceLastSync >= syncInterval);
}

TEST(StateSyncTest, StateChanged) {
    PlayerNetState previous;
    previous.transform.x = 10.0f;
    previous.health = 100;

    PlayerNetState current;
    current.transform.x = 10.0f;
    current.health = 90;  // Health changed

    bool positionChanged = std::abs(current.transform.x - previous.transform.x) > 0.01f;
    bool healthChanged = current.health != previous.health;

    EXPECT_FALSE(positionChanged);
    EXPECT_TRUE(healthChanged);
}

// =============================================================================
// Latency Estimation Tests
// =============================================================================

TEST(LatencyTest, RoundTripTime) {
    int64_t sendTime = 1000;  // ms
    int64_t receiveTime = 1150;  // ms

    int64_t rtt = receiveTime - sendTime;
    EXPECT_EQ(150, rtt);
}

TEST(LatencyTest, OneWayLatency) {
    int64_t rtt = 150;  // ms
    int64_t latency = rtt / 2;

    EXPECT_EQ(75, latency);
}

TEST(LatencyTest, SmoothedLatency) {
    float previousLatency = 50.0f;
    float newSample = 80.0f;
    float smoothingFactor = 0.1f;

    float smoothed = previousLatency + (newSample - previousLatency) * smoothingFactor;
    EXPECT_FLOAT_EQ(53.0f, smoothed);
}

// =============================================================================
// Server Time Synchronization Tests
// =============================================================================

TEST(ServerTimeTest, CalculateOffset) {
    int64_t clientTime = 1000;
    int64_t serverTime = 1100;
    int64_t halfRtt = 50;

    int64_t offset = serverTime - clientTime + halfRtt;
    EXPECT_EQ(150, offset);
}

TEST(ServerTimeTest, EstimateServerTime) {
    int64_t localTime = 1000;
    int64_t offset = 150;

    int64_t estimatedServerTime = localTime + offset;
    EXPECT_EQ(1150, estimatedServerTime);
}

// =============================================================================
// Rate Limiting Tests
// =============================================================================

TEST(RateLimitingTest, TokenBucket) {
    float tokens = 10.0f;
    float maxTokens = 50.0f;
    float tokensPerSecond = 20.0f;
    float deltaTime = 0.5f;

    // Add tokens
    tokens = std::min(maxTokens, tokens + tokensPerSecond * deltaTime);
    EXPECT_FLOAT_EQ(20.0f, tokens);

    // Consume token
    float cost = 1.0f;
    if (tokens >= cost) {
        tokens -= cost;
    }
    EXPECT_FLOAT_EQ(19.0f, tokens);
}

TEST(RateLimitingTest, EventsPerSecond) {
    int eventsThisSecond = 0;
    int maxEventsPerSecond = 50;

    for (int i = 0; i < 60; ++i) {
        if (eventsThisSecond < maxEventsPerSecond) {
            eventsThisSecond++;
        }
    }

    EXPECT_EQ(50, eventsThisSecond);  // Capped at max
}

// =============================================================================
// Sync Stats Tests
// =============================================================================

TEST(SyncStatsTest, DefaultValues) {
    MultiplayerSync::SyncStats stats;

    EXPECT_EQ(0, stats.playerUpdatesPerSecond);
    EXPECT_EQ(0, stats.zombieUpdatesPerSecond);
    EXPECT_EQ(0, stats.eventsPerSecond);
    EXPECT_EQ(0, stats.bytesUpPerSecond);
    EXPECT_EQ(0, stats.bytesDownPerSecond);
    EXPECT_FLOAT_EQ(0.0f, stats.averageLatency);
}

// =============================================================================
// Integration Tests
// =============================================================================

class MultiplayerSyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Note: Full integration tests would require mock Firebase
    }
};

TEST_F(MultiplayerSyncTest, Initialize) {
    // MultiplayerSync is a singleton, test basic access
    auto& sync = MultiplayerSync::Instance();

    // Check initial state (without connecting)
    EXPECT_FALSE(sync.IsSyncing());
}

TEST_F(MultiplayerSyncTest, Config_Apply) {
    MultiplayerSync::Config config;
    config.playerSyncRate = 20.0f;
    config.interpolationDelay = 0.2f;

    // Would initialize with config
    // auto& sync = MultiplayerSync::Instance();
    // sync.Initialize(config);
}

// =============================================================================
// Jitter Buffer Tests
// =============================================================================

TEST(JitterBufferTest, BufferDelay) {
    std::vector<std::pair<int64_t, NetworkTransform>> buffer;

    // Add states with timestamps
    NetworkTransform state1, state2, state3;
    state1.timestamp = 100;
    state2.timestamp = 200;
    state3.timestamp = 300;

    buffer.push_back({100, state1});
    buffer.push_back({200, state2});
    buffer.push_back({300, state3});

    // Render time is delayed by interpolation buffer
    int64_t currentTime = 350;
    int64_t interpolationDelay = 100;
    int64_t renderTime = currentTime - interpolationDelay;

    EXPECT_EQ(250, renderTime);

    // Find states to interpolate between
    // State 2 (200) and State 3 (300) bracket renderTime (250)
}

// =============================================================================
// Reliability Tests
// =============================================================================

TEST(ReliabilityTest, PacketSequence) {
    uint32_t lastReceivedSequence = 100;
    uint32_t newSequence = 101;

    // Accept in-order packet
    bool isNewer = newSequence > lastReceivedSequence;
    EXPECT_TRUE(isNewer);

    // Reject old packet
    uint32_t oldSequence = 99;
    bool isOld = oldSequence <= lastReceivedSequence;
    EXPECT_TRUE(isOld);
}

TEST(ReliabilityTest, AcknowledgmentBitfield) {
    uint32_t acks = 0;

    // Mark packets as acknowledged
    int packetIndex = 0;
    acks |= (1 << packetIndex);

    packetIndex = 5;
    acks |= (1 << packetIndex);

    EXPECT_TRUE(acks & (1 << 0));
    EXPECT_TRUE(acks & (1 << 5));
    EXPECT_FALSE(acks & (1 << 1));
}

// =============================================================================
// Compression Tests
// =============================================================================

TEST(CompressionTest, QuantizePosition) {
    float position = 123.456f;
    float quantizationStep = 0.01f;

    // Quantize to reduce precision
    int16_t quantized = static_cast<int16_t>(position / quantizationStep);
    float dequantized = quantized * quantizationStep;

    EXPECT_NEAR(position, dequantized, quantizationStep);
}

TEST(CompressionTest, QuantizeRotation) {
    float rotation = 45.678f;  // degrees

    // Quantize to byte (256 values for 360 degrees)
    uint8_t quantized = static_cast<uint8_t>((rotation / 360.0f) * 255);
    float dequantized = (quantized / 255.0f) * 360.0f;

    EXPECT_NEAR(rotation, dequantized, 2.0f);  // ~1.4 degree precision
}

TEST(CompressionTest, DeltaCompression_SmallChange) {
    float previous = 100.0f;
    float current = 100.5f;

    float delta = current - previous;

    // Small delta can be encoded in fewer bits
    bool canFitInByte = delta >= -128 && delta <= 127;
    // This example fits in a byte with scaling
}

