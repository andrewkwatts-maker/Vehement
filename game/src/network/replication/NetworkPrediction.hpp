#pragma once

#include "NetworkedEntity.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <deque>
#include <chrono>

namespace Network {
namespace Replication {

// Input command for prediction
struct InputCommand {
    uint32_t sequenceNumber;
    std::chrono::steady_clock::time_point timestamp;

    // Movement input
    float moveX = 0.0f;
    float moveY = 0.0f;
    float moveZ = 0.0f;

    // Look input
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;

    // Action inputs
    uint32_t buttons = 0;    // Bitmask of pressed buttons
    uint32_t buttonsPressed = 0;  // Just pressed this frame
    uint32_t buttonsReleased = 0; // Just released this frame

    // Custom data
    std::vector<uint8_t> customData;
};

// Entity state snapshot
struct StateSnapshot {
    uint32_t sequenceNumber;
    std::chrono::steady_clock::time_point timestamp;

    NetVec3 position;
    NetQuat rotation;
    NetVec3 velocity;
    NetVec3 angularVelocity;

    // Game-specific state
    float health = 100.0f;
    int state = 0;
    uint32_t flags = 0;

    // Custom data
    std::vector<uint8_t> customData;
};

// Server reconciliation result
struct ReconciliationResult {
    uint32_t clientSequence;
    uint32_t serverSequence;
    StateSnapshot serverState;
    bool needsCorrection = false;
    NetVec3 positionError;
    float errorMagnitude = 0.0f;
};

// Interpolation target
struct InterpolationTarget {
    StateSnapshot from;
    StateSnapshot to;
    float t = 0.0f;
    std::chrono::steady_clock::time_point targetTime;
};

// Prediction settings
struct PredictionSettings {
    bool enabled = true;
    float maxPredictionTime = 0.5f;      // Max time to predict ahead
    float reconciliationThreshold = 0.1f; // Position error threshold
    float smoothingFactor = 0.1f;         // Correction smoothing
    int maxStoredInputs = 128;
    int maxStoredSnapshots = 64;
    bool useInterpolation = true;
    float interpolationDelay = 0.1f;      // 100ms interpolation buffer
    float extrapolationLimit = 0.25f;     // Max extrapolation time
};

// Rollback frame for fighting game style
struct RollbackFrame {
    uint32_t frame;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<uint64_t, StateSnapshot> entityStates;
    std::vector<InputCommand> inputs;
};

/**
 * NetworkPrediction - Client-side prediction and reconciliation
 *
 * Features:
 * - Input prediction
 * - Server reconciliation
 * - Entity interpolation
 * - Snapshot buffer
 * - Rollback for fighting-game style
 */
class NetworkPrediction {
public:
    static NetworkPrediction& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Settings
    void setSettings(const PredictionSettings& settings);
    const PredictionSettings& getSettings() const { return m_settings; }

    // Input prediction
    void recordInput(uint64_t entityId, const InputCommand& input);
    void predictMovement(uint64_t entityId, float deltaTime);
    const InputCommand* getInput(uint64_t entityId, uint32_t sequence) const;
    uint32_t getLatestInputSequence(uint64_t entityId) const;

    // State management
    void recordState(uint64_t entityId, const StateSnapshot& state);
    const StateSnapshot* getState(uint64_t entityId, uint32_t sequence) const;
    StateSnapshot getCurrentState(uint64_t entityId) const;
    StateSnapshot getPredictedState(uint64_t entityId) const;

    // Server reconciliation
    void receiveServerState(uint64_t entityId, const StateSnapshot& serverState);
    ReconciliationResult reconcile(uint64_t entityId, const StateSnapshot& serverState);
    void applyCorrection(uint64_t entityId, const StateSnapshot& correctedState, float smoothing);
    bool needsReconciliation(uint64_t entityId) const;

    // Interpolation
    void addInterpolationTarget(uint64_t entityId, const StateSnapshot& target);
    StateSnapshot interpolate(uint64_t entityId, std::chrono::steady_clock::time_point time) const;
    StateSnapshot extrapolate(uint64_t entityId, float deltaTime) const;
    void setInterpolationDelay(float seconds);

    // Rollback (for fighting games)
    void enableRollback(bool enabled);
    bool isRollbackEnabled() const { return m_rollbackEnabled; }
    void saveFrame(uint32_t frame);
    void rollbackTo(uint32_t frame);
    void resimulate(uint32_t fromFrame, uint32_t toFrame);
    RollbackFrame* getFrame(uint32_t frame);
    void confirmFrame(uint32_t frame);  // Can discard frames before this

    // Prediction callbacks
    using PredictCallback = std::function<void(uint64_t entityId, const InputCommand&, StateSnapshot&, float dt)>;
    using ReconcileCallback = std::function<void(uint64_t entityId, const ReconciliationResult&)>;

    void setPredictCallback(PredictCallback callback) { m_predictCallback = callback; }
    void setReconcileCallback(ReconcileCallback callback) { m_reconcileCallback = callback; }

    // Debug info
    float getAverageError(uint64_t entityId) const;
    int getPendingInputCount(uint64_t entityId) const;
    int getStoredSnapshotCount(uint64_t entityId) const;
    std::string getDebugInfo(uint64_t entityId) const;

private:
    NetworkPrediction();
    ~NetworkPrediction();
    NetworkPrediction(const NetworkPrediction&) = delete;
    NetworkPrediction& operator=(const NetworkPrediction&) = delete;

    // Internal prediction
    StateSnapshot simulateInput(const StateSnapshot& state, const InputCommand& input, float dt);
    void replayInputs(uint64_t entityId, uint32_t fromSequence);

    // State management
    void trimInputHistory(uint64_t entityId);
    void trimSnapshotHistory(uint64_t entityId);

    // Interpolation helpers
    StateSnapshot lerpState(const StateSnapshot& a, const StateSnapshot& b, float t) const;
    NetVec3 lerpVec3(const NetVec3& a, const NetVec3& b, float t) const;
    NetQuat slerpQuat(const NetQuat& a, const NetQuat& b, float t) const;

    // Error calculation
    float calculatePositionError(const NetVec3& predicted, const NetVec3& actual) const;

private:
    bool m_initialized = false;
    PredictionSettings m_settings;

    // Input history per entity
    std::unordered_map<uint64_t, std::deque<InputCommand>> m_inputHistory;
    std::unordered_map<uint64_t, uint32_t> m_latestAckedInput;

    // State snapshots per entity
    std::unordered_map<uint64_t, std::deque<StateSnapshot>> m_stateHistory;
    std::unordered_map<uint64_t, StateSnapshot> m_currentState;
    std::unordered_map<uint64_t, StateSnapshot> m_predictedState;

    // Interpolation
    std::unordered_map<uint64_t, std::deque<StateSnapshot>> m_interpolationBuffer;
    float m_interpolationDelay = 0.1f;

    // Rollback
    bool m_rollbackEnabled = false;
    std::deque<RollbackFrame> m_rollbackFrames;
    uint32_t m_confirmedFrame = 0;
    static constexpr size_t MAX_ROLLBACK_FRAMES = 10;

    // Error tracking
    std::unordered_map<uint64_t, std::deque<float>> m_errorHistory;
    static constexpr size_t ERROR_HISTORY_SIZE = 60;

    // Callbacks
    PredictCallback m_predictCallback;
    ReconcileCallback m_reconcileCallback;
};

} // namespace Replication
} // namespace Network
