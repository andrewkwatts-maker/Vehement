#include "NetworkedEntity.hpp"
#include "ReplicationManager.hpp"

namespace Network {
namespace Replication {

NetworkedEntity::NetworkedEntity() {
    m_rotation.w = 1.0f;  // Identity quaternion
}

NetworkedEntity::~NetworkedEntity() {}

bool NetworkedEntity::isLocallyOwned() const {
    return ReplicationManager::getInstance().isOwner(m_networkId);
}

bool NetworkedEntity::hasAuthority() const {
    return ReplicationManager::getInstance().hasAuthority(m_networkId);
}

void NetworkedEntity::markDirty(uint32_t propertyId) {
    ReplicationManager::getInstance().markDirty(m_networkId, propertyId);
}

void NetworkedEntity::markAllDirty() {
    ReplicationManager::getInstance().markAllDirty(m_networkId);
}

bool NetworkedEntity::isDirty() const {
    return ReplicationManager::getInstance().isDirty(m_networkId);
}

std::vector<uint8_t> NetworkedEntity::serializeProperty(uint32_t propertyId) {
    switch (propertyId) {
        case PROP_POSITION:
            return serializeValue(m_position);
        case PROP_ROTATION:
            return serializeValue(m_rotation);
        case PROP_HEALTH:
            return serializeValue(m_health);
        case PROP_STATE:
            return serializeValue(m_state);
        default:
            return {};
    }
}

void NetworkedEntity::deserializeProperty(uint32_t propertyId, const std::vector<uint8_t>& data) {
    switch (propertyId) {
        case PROP_POSITION:
            if (data.size() >= sizeof(NetVec3)) {
                m_position = deserializeValue<NetVec3>(data);
            }
            break;
        case PROP_ROTATION:
            if (data.size() >= sizeof(NetQuat)) {
                m_rotation = deserializeValue<NetQuat>(data);
            }
            break;
        case PROP_HEALTH:
            if (data.size() >= sizeof(float)) {
                m_health = deserializeValue<float>(data);
            }
            break;
        case PROP_STATE:
            if (data.size() >= sizeof(int)) {
                m_state = deserializeValue<int>(data);
            }
            break;
    }
}

std::vector<uint8_t> NetworkedEntity::serialize() {
    std::vector<uint8_t> data;

    // Network ID
    auto idData = serializeValue(m_networkId);
    data.insert(data.end(), idData.begin(), idData.end());

    // Owner ID
    auto ownerData = serializeValue(m_ownerId);
    data.insert(data.end(), ownerData.begin(), ownerData.end());

    // Position
    auto posData = serializeProperty(PROP_POSITION);
    data.insert(data.end(), posData.begin(), posData.end());

    // Rotation
    auto rotData = serializeProperty(PROP_ROTATION);
    data.insert(data.end(), rotData.begin(), rotData.end());

    // Health
    auto healthData = serializeProperty(PROP_HEALTH);
    data.insert(data.end(), healthData.begin(), healthData.end());

    // State
    auto stateData = serializeProperty(PROP_STATE);
    data.insert(data.end(), stateData.begin(), stateData.end());

    return data;
}

void NetworkedEntity::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint64_t) * 2 + sizeof(NetVec3) + sizeof(NetQuat) + sizeof(float) + sizeof(int)) {
        return;
    }

    size_t offset = 0;

    // Network ID
    std::memcpy(&m_networkId, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // Owner ID
    std::memcpy(&m_ownerId, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    // Position
    std::memcpy(&m_position, data.data() + offset, sizeof(NetVec3));
    offset += sizeof(NetVec3);

    // Rotation
    std::memcpy(&m_rotation, data.data() + offset, sizeof(NetQuat));
    offset += sizeof(NetQuat);

    // Health
    std::memcpy(&m_health, data.data() + offset, sizeof(float));
    offset += sizeof(float);

    // State
    std::memcpy(&m_state, data.data() + offset, sizeof(int));
}

void NetworkedEntity::registerRPC(const RPCDefinition& rpc) {
    m_rpcs[rpc.id] = rpc;
    m_rpcNameToId[rpc.name] = rpc.id;
}

void NetworkedEntity::callRPC(uint32_t rpcId, RPCTarget target, const std::vector<RPCParam>& params) {
    auto it = m_rpcs.find(rpcId);
    if (it == m_rpcs.end()) return;

    const auto& rpc = it->second;

    // Check if we're allowed to call this RPC
    if (rpc.requiresAuthority && !hasAuthority()) {
        return;
    }

    RPCCall call;
    call.networkId = m_networkId;
    call.rpcId = rpcId;
    call.target = target;
    call.params = params;
    call.reliable = true;

    // Serialize and send RPC
    std::vector<uint8_t> buffer;

    // Header: 0xFF marker, network ID, RPC ID, param count
    buffer.push_back(0xFF);  // RPC marker

    for (int i = 0; i < 8; i++) {
        buffer.push_back(static_cast<uint8_t>((m_networkId >> (i * 8)) & 0xFF));
    }

    for (int i = 0; i < 4; i++) {
        buffer.push_back(static_cast<uint8_t>((rpcId >> (i * 8)) & 0xFF));
    }

    buffer.push_back(static_cast<uint8_t>(target));
    buffer.push_back(static_cast<uint8_t>(params.size()));

    // Serialize parameters
    for (const auto& param : params) {
        buffer.push_back(static_cast<uint8_t>(param.type));
        uint16_t dataLen = static_cast<uint16_t>(param.data.size());
        buffer.push_back(static_cast<uint8_t>(dataLen & 0xFF));
        buffer.push_back(static_cast<uint8_t>((dataLen >> 8) & 0xFF));
        buffer.insert(buffer.end(), param.data.begin(), param.data.end());
    }

    // Send via transport
    // Would use ReplicationManager or NetworkTransport
}

void NetworkedEntity::callRPC(const std::string& rpcName, RPCTarget target,
                               const std::vector<RPCParam>& params) {
    auto it = m_rpcNameToId.find(rpcName);
    if (it != m_rpcNameToId.end()) {
        callRPC(it->second, target, params);
    }
}

void NetworkedEntity::receiveRPC(uint32_t rpcId, const std::vector<RPCParam>& params) {
    auto it = m_rpcs.find(rpcId);
    if (it == m_rpcs.end()) return;

    const auto& rpc = it->second;

    // Execute the handler
    if (rpc.handler) {
        rpc.handler(this, params);
    }
}

void NetworkedEntity::setPosition(const NetVec3& pos) {
    if (m_position != pos) {
        m_position = pos;
        markDirty(PROP_POSITION);
    }
}

void NetworkedEntity::setRotation(const NetQuat& rot) {
    if (m_rotation != rot) {
        m_rotation = rot;
        markDirty(PROP_ROTATION);
    }
}

void NetworkedEntity::setHealth(float health) {
    if (m_health != health) {
        m_health = health;
        markDirty(PROP_HEALTH);
    }
}

void NetworkedEntity::setState(int state) {
    if (m_state != state) {
        m_state = state;
        markDirty(PROP_STATE);
    }
}

} // namespace Replication
} // namespace Network
