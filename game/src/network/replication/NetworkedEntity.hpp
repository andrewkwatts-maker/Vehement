#pragma once

#include "ReplicationManager.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <cstring>

namespace Network {
namespace Replication {

// Vector3 for network transmission
struct NetVec3 {
    float x = 0, y = 0, z = 0;

    bool operator==(const NetVec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    bool operator!=(const NetVec3& other) const { return !(*this == other); }
};

// Quaternion for network transmission
struct NetQuat {
    float x = 0, y = 0, z = 0, w = 1;

    bool operator==(const NetQuat& other) const {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
    bool operator!=(const NetQuat& other) const { return !(*this == other); }
};

// RPC parameter types
enum class RPCParamType {
    Void,
    Bool,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Float,
    Double,
    String,
    Vec3,
    Quat,
    ByteArray,
    Custom
};

// RPC target
enum class RPCTarget {
    Server,          // Call on server
    Owner,           // Call on owning client
    AllClients,      // Call on all clients
    AllClientsExceptOwner,
    NetMulticast     // Multicast to all connected peers
};

// RPC parameter
struct RPCParam {
    RPCParamType type;
    std::vector<uint8_t> data;

    // Constructors for common types
    static RPCParam fromBool(bool v) {
        RPCParam p; p.type = RPCParamType::Bool;
        p.data.push_back(v ? 1 : 0);
        return p;
    }

    static RPCParam fromInt32(int32_t v) {
        RPCParam p; p.type = RPCParamType::Int32;
        p.data.resize(4);
        std::memcpy(p.data.data(), &v, 4);
        return p;
    }

    static RPCParam fromFloat(float v) {
        RPCParam p; p.type = RPCParamType::Float;
        p.data.resize(4);
        std::memcpy(p.data.data(), &v, 4);
        return p;
    }

    static RPCParam fromString(const std::string& v) {
        RPCParam p; p.type = RPCParamType::String;
        p.data.assign(v.begin(), v.end());
        return p;
    }

    static RPCParam fromVec3(const NetVec3& v) {
        RPCParam p; p.type = RPCParamType::Vec3;
        p.data.resize(12);
        std::memcpy(p.data.data(), &v, 12);
        return p;
    }

    // Getters
    bool asBool() const { return !data.empty() && data[0] != 0; }
    int32_t asInt32() const { int32_t v; std::memcpy(&v, data.data(), 4); return v; }
    float asFloat() const { float v; std::memcpy(&v, data.data(), 4); return v; }
    std::string asString() const { return std::string(data.begin(), data.end()); }
    NetVec3 asVec3() const { NetVec3 v; std::memcpy(&v, data.data(), 12); return v; }
};

// RPC call info
struct RPCCall {
    uint64_t networkId;
    uint32_t rpcId;
    RPCTarget target;
    std::vector<RPCParam> params;
    bool reliable = true;
};

// RPC registration
struct RPCDefinition {
    uint32_t id;
    std::string name;
    RPCTarget allowedTargets;
    bool requiresAuthority;
    std::function<void(NetworkedEntity*, const std::vector<RPCParam>&)> handler;
};

/**
 * NetworkedEntity - Base class for networked entities
 *
 * Provides:
 * - Network ID and ownership
 * - Property replication macros
 * - RPC support
 * - Serialization helpers
 */
class NetworkedEntity : public std::enable_shared_from_this<NetworkedEntity> {
public:
    NetworkedEntity();
    virtual ~NetworkedEntity();

    // Network identity
    uint64_t getNetworkId() const { return m_networkId; }
    void setNetworkId(uint64_t id) { m_networkId = id; }

    // Ownership
    uint64_t getOwnerId() const { return m_ownerId; }
    void setOwner(uint64_t ownerId) { m_ownerId = ownerId; }
    bool isLocallyOwned() const;
    bool hasAuthority() const;

    // Network role
    NetworkRole getNetworkRole() const { return m_networkRole; }
    void setNetworkRole(NetworkRole role) { m_networkRole = role; }

    // Dirty tracking
    void markDirty(uint32_t propertyId);
    void markAllDirty();
    bool isDirty() const;

    // Property serialization (override in derived classes)
    virtual std::vector<uint8_t> serializeProperty(uint32_t propertyId);
    virtual void deserializeProperty(uint32_t propertyId, const std::vector<uint8_t>& data);

    // Full serialization
    virtual std::vector<uint8_t> serialize();
    virtual void deserialize(const std::vector<uint8_t>& data);

    // RPC system
    void registerRPC(const RPCDefinition& rpc);
    void callRPC(uint32_t rpcId, RPCTarget target, const std::vector<RPCParam>& params);
    void callRPC(const std::string& rpcName, RPCTarget target, const std::vector<RPCParam>& params);
    void receiveRPC(uint32_t rpcId, const std::vector<RPCParam>& params);

    // Common replicated properties
    const NetVec3& getPosition() const { return m_position; }
    void setPosition(const NetVec3& pos);
    const NetQuat& getRotation() const { return m_rotation; }
    void setRotation(const NetQuat& rot);
    float getHealth() const { return m_health; }
    void setHealth(float health);
    int getState() const { return m_state; }
    void setState(int state);

    // Lifecycle
    virtual void onNetworkSpawn() {}
    virtual void onNetworkDespawn() {}
    virtual void onOwnershipChanged(uint64_t newOwner) {}
    virtual void onAuthorityGained() {}
    virtual void onAuthorityLost() {}

    // Update
    virtual void networkUpdate(float deltaTime) {}

    // Property IDs for common properties
    static constexpr uint32_t PROP_POSITION = 1;
    static constexpr uint32_t PROP_ROTATION = 2;
    static constexpr uint32_t PROP_HEALTH = 3;
    static constexpr uint32_t PROP_STATE = 4;

protected:
    // Serialization helpers
    template<typename T>
    static std::vector<uint8_t> serializeValue(const T& value) {
        std::vector<uint8_t> data(sizeof(T));
        std::memcpy(data.data(), &value, sizeof(T));
        return data;
    }

    template<typename T>
    static T deserializeValue(const std::vector<uint8_t>& data) {
        T value;
        std::memcpy(&value, data.data(), sizeof(T));
        return value;
    }

    static std::vector<uint8_t> serializeString(const std::string& str) {
        std::vector<uint8_t> data;
        uint32_t len = static_cast<uint32_t>(str.size());
        data.resize(4 + len);
        std::memcpy(data.data(), &len, 4);
        std::memcpy(data.data() + 4, str.c_str(), len);
        return data;
    }

    static std::string deserializeString(const std::vector<uint8_t>& data) {
        if (data.size() < 4) return "";
        uint32_t len;
        std::memcpy(&len, data.data(), 4);
        if (data.size() < 4 + len) return "";
        return std::string(data.begin() + 4, data.begin() + 4 + len);
    }

protected:
    uint64_t m_networkId = 0;
    uint64_t m_ownerId = 0;
    NetworkRole m_networkRole = NetworkRole::None;

    // Common replicated properties
    NetVec3 m_position;
    NetQuat m_rotation;
    float m_health = 100.0f;
    int m_state = 0;

    // RPC definitions
    std::unordered_map<uint32_t, RPCDefinition> m_rpcs;
    std::unordered_map<std::string, uint32_t> m_rpcNameToId;
};

// Macros for declaring replicated properties
#define NET_PROPERTY(type, name, propId) \
protected: \
    type m_##name; \
public: \
    static constexpr uint32_t PROP_##name = propId; \
    const type& get##name() const { return m_##name; } \
    void set##name(const type& value) { \
        if (m_##name != value) { \
            m_##name = value; \
            markDirty(PROP_##name); \
        } \
    }

#define NET_PROPERTY_READONLY(type, name, propId) \
protected: \
    type m_##name; \
public: \
    static constexpr uint32_t PROP_##name = propId; \
    const type& get##name() const { return m_##name; }

// Macros for RPC declarations
#define DECLARE_RPC(name) \
    static constexpr uint32_t RPC_##name = __COUNTER__; \
    void name##_Implementation

#define REGISTER_RPC(name, target, requiresAuth) \
    do { \
        RPCDefinition def; \
        def.id = RPC_##name; \
        def.name = #name; \
        def.allowedTargets = target; \
        def.requiresAuthority = requiresAuth; \
        def.handler = [this](NetworkedEntity* entity, const std::vector<RPCParam>& params) { \
            static_cast<decltype(this)>(entity)->name##_Implementation(params); \
        }; \
        registerRPC(def); \
    } while(0)

#define CALL_RPC(name, target, ...) \
    callRPC(RPC_##name, target, {__VA_ARGS__})

} // namespace Replication
} // namespace Network
