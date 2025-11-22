#include "NetworkPrediction.hpp"
#include "ReplicationManager.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Network {
namespace Replication {

NetworkPrediction& NetworkPrediction::getInstance() {
    static NetworkPrediction instance;
    return instance;
}

NetworkPrediction::NetworkPrediction() {}

NetworkPrediction::~NetworkPrediction() {
    shutdown();
}

bool NetworkPrediction::initialize() {
    if (m_initialized) return true;

    m_initialized = true;
    return true;
}

void NetworkPrediction::shutdown() {
    if (!m_initialized) return;

    m_inputHistory.clear();
    m_stateHistory.clear();
    m_interpolationBuffer.clear();
    m_rollbackFrames.clear();

    m_initialized = false;
}

void NetworkPrediction::update(float deltaTime) {
    if (!m_initialized) return;

    // Update interpolation for all entities
    for (auto& [entityId, buffer] : m_interpolationBuffer) {
        // Remove old snapshots
        auto now = std::chrono::steady_clock::now();
        auto cutoff = now - std::chrono::milliseconds(static_cast<int>(m_interpolationDelay * 2000));

        while (buffer.size() > 2 && buffer.front().timestamp < cutoff) {
            buffer.pop_front();
        }
    }

    // Trim histories
    for (auto& [entityId, history] : m_inputHistory) {
        trimInputHistory(entityId);
    }

    for (auto& [entityId, history] : m_stateHistory) {
        trimSnapshotHistory(entityId);
    }
}

void NetworkPrediction::setSettings(const PredictionSettings& settings) {
    m_settings = settings;
    m_interpolationDelay = settings.interpolationDelay;
}

void NetworkPrediction::recordInput(uint64_t entityId, const InputCommand& input) {
    m_inputHistory[entityId].push_back(input);
    trimInputHistory(entityId);
}

void NetworkPrediction::predictMovement(uint64_t entityId, float deltaTime) {
    if (!m_settings.enabled) return;

    auto& inputs = m_inputHistory[entityId];
    if (inputs.empty()) return;

    const InputCommand& latestInput = inputs.back();

    // Get current state
    StateSnapshot state = getCurrentState(entityId);

    // Simulate the input
    StateSnapshot predicted = simulateInput(state, latestInput, deltaTime);

    // Store predicted state
    m_predictedState[entityId] = predicted;

    // If we have a custom predict callback, use it
    if (m_predictCallback) {
        StateSnapshot customPredicted = state;
        m_predictCallback(entityId, latestInput, customPredicted, deltaTime);
        m_predictedState[entityId] = customPredicted;
    }
}

const InputCommand* NetworkPrediction::getInput(uint64_t entityId, uint32_t sequence) const {
    auto it = m_inputHistory.find(entityId);
    if (it == m_inputHistory.end()) return nullptr;

    for (const auto& input : it->second) {
        if (input.sequenceNumber == sequence) {
            return &input;
        }
    }
    return nullptr;
}

uint32_t NetworkPrediction::getLatestInputSequence(uint64_t entityId) const {
    auto it = m_inputHistory.find(entityId);
    if (it == m_inputHistory.end() || it->second.empty()) return 0;
    return it->second.back().sequenceNumber;
}

void NetworkPrediction::recordState(uint64_t entityId, const StateSnapshot& state) {
    m_stateHistory[entityId].push_back(state);
    m_currentState[entityId] = state;
    trimSnapshotHistory(entityId);
}

const StateSnapshot* NetworkPrediction::getState(uint64_t entityId, uint32_t sequence) const {
    auto it = m_stateHistory.find(entityId);
    if (it == m_stateHistory.end()) return nullptr;

    for (const auto& state : it->second) {
        if (state.sequenceNumber == sequence) {
            return &state;
        }
    }
    return nullptr;
}

StateSnapshot NetworkPrediction::getCurrentState(uint64_t entityId) const {
    auto it = m_currentState.find(entityId);
    if (it != m_currentState.end()) {
        return it->second;
    }
    return StateSnapshot();
}

StateSnapshot NetworkPrediction::getPredictedState(uint64_t entityId) const {
    auto it = m_predictedState.find(entityId);
    if (it != m_predictedState.end()) {
        return it->second;
    }
    return getCurrentState(entityId);
}

void NetworkPrediction::receiveServerState(uint64_t entityId, const StateSnapshot& serverState) {
    ReconciliationResult result = reconcile(entityId, serverState);

    if (result.needsCorrection) {
        applyCorrection(entityId, serverState, m_settings.smoothingFactor);

        // Replay unacknowledged inputs
        uint32_t serverSeq = serverState.sequenceNumber;
        replayInputs(entityId, serverSeq + 1);
    }

    // Track error for debugging
    m_errorHistory[entityId].push_back(result.errorMagnitude);
    while (m_errorHistory[entityId].size() > ERROR_HISTORY_SIZE) {
        m_errorHistory[entityId].pop_front();
    }

    // Notify callback
    if (m_reconcileCallback) {
        m_reconcileCallback(entityId, result);
    }
}

ReconciliationResult NetworkPrediction::reconcile(uint64_t entityId,
                                                    const StateSnapshot& serverState) {
    ReconciliationResult result;
    result.serverSequence = serverState.sequenceNumber;
    result.serverState = serverState;

    // Find the client state at the same sequence
    const StateSnapshot* clientState = getState(entityId, serverState.sequenceNumber);
    if (!clientState) {
        // No matching state, need full correction
        result.needsCorrection = true;
        result.errorMagnitude = 999.0f;
        return result;
    }

    result.clientSequence = clientState->sequenceNumber;

    // Calculate position error
    result.positionError.x = serverState.position.x - clientState->position.x;
    result.positionError.y = serverState.position.y - clientState->position.y;
    result.positionError.z = serverState.position.z - clientState->position.z;

    result.errorMagnitude = calculatePositionError(clientState->position, serverState.position);

    // Check if correction is needed
    result.needsCorrection = result.errorMagnitude > m_settings.reconciliationThreshold;

    // Mark acknowledged input
    m_latestAckedInput[entityId] = serverState.sequenceNumber;

    return result;
}

void NetworkPrediction::applyCorrection(uint64_t entityId, const StateSnapshot& correctedState,
                                         float smoothing) {
    StateSnapshot& current = m_currentState[entityId];

    if (smoothing > 0.0f && smoothing < 1.0f) {
        // Smooth correction
        current = lerpState(current, correctedState, smoothing);
    } else {
        // Snap correction
        current = correctedState;
    }

    // Update entity if possible
    auto entity = ReplicationManager::getInstance().getEntity(entityId);
    if (entity) {
        entity->setPosition(current.position);
        entity->setRotation(current.rotation);
    }
}

bool NetworkPrediction::needsReconciliation(uint64_t entityId) const {
    auto stateIt = m_currentState.find(entityId);
    auto predictedIt = m_predictedState.find(entityId);

    if (stateIt == m_currentState.end() || predictedIt == m_predictedState.end()) {
        return false;
    }

    float error = calculatePositionError(stateIt->second.position, predictedIt->second.position);
    return error > m_settings.reconciliationThreshold;
}

void NetworkPrediction::addInterpolationTarget(uint64_t entityId, const StateSnapshot& target) {
    m_interpolationBuffer[entityId].push_back(target);

    // Keep buffer size reasonable
    while (m_interpolationBuffer[entityId].size() > 32) {
        m_interpolationBuffer[entityId].pop_front();
    }
}

StateSnapshot NetworkPrediction::interpolate(uint64_t entityId,
                                              std::chrono::steady_clock::time_point time) const {
    auto it = m_interpolationBuffer.find(entityId);
    if (it == m_interpolationBuffer.end() || it->second.size() < 2) {
        return getCurrentState(entityId);
    }

    const auto& buffer = it->second;

    // Target time is in the past by interpolation delay
    auto targetTime = time - std::chrono::milliseconds(
        static_cast<int>(m_interpolationDelay * 1000));

    // Find two snapshots to interpolate between
    for (size_t i = 1; i < buffer.size(); i++) {
        if (buffer[i].timestamp >= targetTime && buffer[i-1].timestamp <= targetTime) {
            float duration = std::chrono::duration<float>(
                buffer[i].timestamp - buffer[i-1].timestamp).count();

            if (duration > 0.0f) {
                float elapsed = std::chrono::duration<float>(
                    targetTime - buffer[i-1].timestamp).count();
                float t = elapsed / duration;

                return lerpState(buffer[i-1], buffer[i], t);
            }
        }
    }

    // If we're past all snapshots, use the last one
    return buffer.back();
}

StateSnapshot NetworkPrediction::extrapolate(uint64_t entityId, float deltaTime) const {
    StateSnapshot state = getCurrentState(entityId);

    if (deltaTime > m_settings.extrapolationLimit) {
        deltaTime = m_settings.extrapolationLimit;
    }

    // Simple linear extrapolation using velocity
    state.position.x += state.velocity.x * deltaTime;
    state.position.y += state.velocity.y * deltaTime;
    state.position.z += state.velocity.z * deltaTime;

    return state;
}

void NetworkPrediction::setInterpolationDelay(float seconds) {
    m_interpolationDelay = seconds;
    m_settings.interpolationDelay = seconds;
}

void NetworkPrediction::enableRollback(bool enabled) {
    m_rollbackEnabled = enabled;

    if (!enabled) {
        m_rollbackFrames.clear();
    }
}

void NetworkPrediction::saveFrame(uint32_t frame) {
    if (!m_rollbackEnabled) return;

    RollbackFrame rollback;
    rollback.frame = frame;
    rollback.timestamp = std::chrono::steady_clock::now();

    // Save all entity states
    auto& repManager = ReplicationManager::getInstance();
    for (const auto& [entityId, state] : m_currentState) {
        rollback.entityStates[entityId] = state;
    }

    // Save inputs
    for (const auto& [entityId, inputs] : m_inputHistory) {
        for (const auto& input : inputs) {
            rollback.inputs.push_back(input);
        }
    }

    m_rollbackFrames.push_back(rollback);

    // Trim old frames
    while (m_rollbackFrames.size() > MAX_ROLLBACK_FRAMES) {
        m_rollbackFrames.pop_front();
    }
}

void NetworkPrediction::rollbackTo(uint32_t frame) {
    if (!m_rollbackEnabled) return;

    RollbackFrame* rollback = getFrame(frame);
    if (!rollback) return;

    // Restore all entity states
    for (const auto& [entityId, state] : rollback->entityStates) {
        m_currentState[entityId] = state;

        auto entity = ReplicationManager::getInstance().getEntity(entityId);
        if (entity) {
            entity->setPosition(state.position);
            entity->setRotation(state.rotation);
            entity->setHealth(state.health);
            entity->setState(state.state);
        }
    }
}

void NetworkPrediction::resimulate(uint32_t fromFrame, uint32_t toFrame) {
    if (!m_rollbackEnabled) return;

    // Roll back
    rollbackTo(fromFrame);

    // Re-simulate all frames
    for (uint32_t frame = fromFrame; frame <= toFrame; frame++) {
        RollbackFrame* rollback = getFrame(frame);
        if (!rollback) continue;

        // Apply all inputs for this frame
        for (const auto& input : rollback->inputs) {
            // Find the entity and simulate
            // This would use the game's simulation logic
        }

        // Save the new state
        saveFrame(frame);
    }
}

RollbackFrame* NetworkPrediction::getFrame(uint32_t frame) {
    for (auto& rollback : m_rollbackFrames) {
        if (rollback.frame == frame) {
            return &rollback;
        }
    }
    return nullptr;
}

void NetworkPrediction::confirmFrame(uint32_t frame) {
    m_confirmedFrame = frame;

    // Remove frames older than confirmed
    while (!m_rollbackFrames.empty() && m_rollbackFrames.front().frame < frame) {
        m_rollbackFrames.pop_front();
    }
}

float NetworkPrediction::getAverageError(uint64_t entityId) const {
    auto it = m_errorHistory.find(entityId);
    if (it == m_errorHistory.end() || it->second.empty()) return 0.0f;

    float sum = 0.0f;
    for (float error : it->second) {
        sum += error;
    }
    return sum / it->second.size();
}

int NetworkPrediction::getPendingInputCount(uint64_t entityId) const {
    auto it = m_inputHistory.find(entityId);
    if (it == m_inputHistory.end()) return 0;

    uint32_t latestAcked = 0;
    auto ackedIt = m_latestAckedInput.find(entityId);
    if (ackedIt != m_latestAckedInput.end()) {
        latestAcked = ackedIt->second;
    }

    int count = 0;
    for (const auto& input : it->second) {
        if (input.sequenceNumber > latestAcked) {
            count++;
        }
    }
    return count;
}

int NetworkPrediction::getStoredSnapshotCount(uint64_t entityId) const {
    auto it = m_stateHistory.find(entityId);
    if (it == m_stateHistory.end()) return 0;
    return static_cast<int>(it->second.size());
}

std::string NetworkPrediction::getDebugInfo(uint64_t entityId) const {
    std::ostringstream info;
    info << "Entity " << entityId << " Prediction Info:\n";
    info << "  Pending inputs: " << getPendingInputCount(entityId) << "\n";
    info << "  Stored snapshots: " << getStoredSnapshotCount(entityId) << "\n";
    info << "  Average error: " << getAverageError(entityId) << "\n";
    info << "  Interpolation delay: " << m_interpolationDelay << "s\n";
    info << "  Rollback enabled: " << (m_rollbackEnabled ? "yes" : "no") << "\n";
    if (m_rollbackEnabled) {
        info << "  Rollback frames: " << m_rollbackFrames.size() << "\n";
        info << "  Confirmed frame: " << m_confirmedFrame << "\n";
    }
    return info.str();
}

// Private methods

StateSnapshot NetworkPrediction::simulateInput(const StateSnapshot& state,
                                                const InputCommand& input, float dt) {
    StateSnapshot result = state;

    // Simple physics simulation
    // This would be replaced with game-specific logic

    // Apply movement input to velocity
    float speed = 5.0f;  // Units per second
    result.velocity.x = input.moveX * speed;
    result.velocity.y = input.moveY * speed;
    result.velocity.z = input.moveZ * speed;

    // Integrate velocity to position
    result.position.x += result.velocity.x * dt;
    result.position.y += result.velocity.y * dt;
    result.position.z += result.velocity.z * dt;

    // Apply rotation
    // This is simplified - real implementation would use proper quaternion math
    result.rotation.y = input.lookYaw;
    result.rotation.x = input.lookPitch;

    // Increment sequence
    result.sequenceNumber = input.sequenceNumber;
    result.timestamp = input.timestamp;

    return result;
}

void NetworkPrediction::replayInputs(uint64_t entityId, uint32_t fromSequence) {
    auto it = m_inputHistory.find(entityId);
    if (it == m_inputHistory.end()) return;

    StateSnapshot state = getCurrentState(entityId);

    for (const auto& input : it->second) {
        if (input.sequenceNumber >= fromSequence) {
            // Calculate delta time (simplified)
            float dt = 1.0f / 60.0f;  // Assume 60 FPS

            state = simulateInput(state, input, dt);
        }
    }

    m_predictedState[entityId] = state;
}

void NetworkPrediction::trimInputHistory(uint64_t entityId) {
    auto& history = m_inputHistory[entityId];

    while (history.size() > static_cast<size_t>(m_settings.maxStoredInputs)) {
        history.pop_front();
    }

    // Also remove inputs that have been acknowledged
    auto ackedIt = m_latestAckedInput.find(entityId);
    if (ackedIt != m_latestAckedInput.end()) {
        uint32_t latestAcked = ackedIt->second;

        // Keep some buffer of acknowledged inputs for debugging
        while (history.size() > 10 && history.front().sequenceNumber < latestAcked - 10) {
            history.pop_front();
        }
    }
}

void NetworkPrediction::trimSnapshotHistory(uint64_t entityId) {
    auto& history = m_stateHistory[entityId];

    while (history.size() > static_cast<size_t>(m_settings.maxStoredSnapshots)) {
        history.pop_front();
    }
}

StateSnapshot NetworkPrediction::lerpState(const StateSnapshot& a, const StateSnapshot& b,
                                            float t) const {
    StateSnapshot result;
    result.position = lerpVec3(a.position, b.position, t);
    result.rotation = slerpQuat(a.rotation, b.rotation, t);
    result.velocity = lerpVec3(a.velocity, b.velocity, t);
    result.angularVelocity = lerpVec3(a.angularVelocity, b.angularVelocity, t);
    result.health = a.health + (b.health - a.health) * t;
    result.state = t < 0.5f ? a.state : b.state;
    result.flags = t < 0.5f ? a.flags : b.flags;
    result.sequenceNumber = t < 0.5f ? a.sequenceNumber : b.sequenceNumber;
    return result;
}

NetVec3 NetworkPrediction::lerpVec3(const NetVec3& a, const NetVec3& b, float t) const {
    NetVec3 result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    return result;
}

NetQuat NetworkPrediction::slerpQuat(const NetQuat& a, const NetQuat& b, float t) const {
    // Simplified slerp - real implementation would be more accurate
    NetQuat result;

    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

    NetQuat b2 = b;
    if (dot < 0.0f) {
        dot = -dot;
        b2.x = -b.x;
        b2.y = -b.y;
        b2.z = -b.z;
        b2.w = -b.w;
    }

    if (dot > 0.9995f) {
        // Linear interpolation for small angles
        result.x = a.x + (b2.x - a.x) * t;
        result.y = a.y + (b2.y - a.y) * t;
        result.z = a.z + (b2.z - a.z) * t;
        result.w = a.w + (b2.w - a.w) * t;

        // Normalize
        float len = std::sqrt(result.x*result.x + result.y*result.y +
                              result.z*result.z + result.w*result.w);
        if (len > 0.0f) {
            result.x /= len;
            result.y /= len;
            result.z /= len;
            result.w /= len;
        }
    } else {
        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);
        float wa = std::sin((1.0f - t) * theta) / sinTheta;
        float wb = std::sin(t * theta) / sinTheta;

        result.x = a.x * wa + b2.x * wb;
        result.y = a.y * wa + b2.y * wb;
        result.z = a.z * wa + b2.z * wb;
        result.w = a.w * wa + b2.w * wb;
    }

    return result;
}

float NetworkPrediction::calculatePositionError(const NetVec3& predicted,
                                                 const NetVec3& actual) const {
    float dx = predicted.x - actual.x;
    float dy = predicted.y - actual.y;
    float dz = predicted.z - actual.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

} // namespace Replication
} // namespace Network
