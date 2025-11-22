# Networking Guide

Nova3D Engine includes networking support through Firebase for multiplayer features in the Vehement2 game. This guide covers Firebase setup, matchmaking, state replication, and best practices.

## Overview

The networking system provides:

- **Firebase Integration**: Real-time database and authentication
- **Matchmaking**: Find and join game sessions
- **State Replication**: Sync game state between players
- **GPS Location**: Location-based gameplay
- **Presence System**: Player online status

## Firebase Setup

### Creating a Firebase Project

1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Click "Add Project"
3. Enter project name
4. Enable Google Analytics (optional)
5. Create project

### Enabling Services

Enable the following Firebase services:

1. **Authentication**
   - Go to Authentication > Sign-in method
   - Enable Anonymous, Email/Password, and/or Google Sign-in

2. **Realtime Database**
   - Go to Realtime Database
   - Create database in your region
   - Start in test mode (configure rules later)

3. **Cloud Functions** (optional)
   - For server-side validation
   - Enable in Firebase console

### Configuration

Create `config/firebase_config.json`:

```json
{
    "apiKey": "your-api-key",
    "authDomain": "your-project.firebaseapp.com",
    "databaseURL": "https://your-project.firebaseio.com",
    "projectId": "your-project-id",
    "storageBucket": "your-project.appspot.com",
    "messagingSenderId": "your-sender-id",
    "appId": "your-app-id"
}
```

### Security Rules

Configure database rules in Firebase Console:

```json
{
  "rules": {
    "towns": {
      "$townId": {
        "players": {
          "$playerId": {
            ".read": true,
            ".write": "auth != null && auth.uid == $playerId"
          }
        },
        "state": {
          ".read": true,
          ".write": "auth != null && data.child('players').child(auth.uid).exists()"
        }
      }
    },
    "players": {
      "$playerId": {
        ".read": true,
        ".write": "auth != null && auth.uid == $playerId"
      }
    },
    "presence": {
      "$playerId": {
        ".read": true,
        ".write": "auth != null && auth.uid == $playerId"
      }
    }
  }
}
```

## Initialization

### Firebase Manager

```cpp
#include <game/src/network/FirebaseManager.hpp>

// Initialize Firebase
auto& firebase = Vehement::FirebaseManager::Instance();

FirebaseConfig config;
config.configPath = "config/firebase_config.json";
config.persistenceEnabled = true;

if (!firebase.Initialize(config)) {
    // Handle initialization failure
}

// Authenticate user
firebase.SignInAnonymously([](bool success, const std::string& userId) {
    if (success) {
        std::cout << "Signed in as: " << userId << "\n";
    }
});
```

### Event Callbacks

```cpp
// Connection state changes
firebase.OnConnectionStateChanged([](bool connected) {
    if (connected) {
        std::cout << "Connected to Firebase\n";
    } else {
        std::cout << "Disconnected from Firebase\n";
    }
});

// Authentication changes
firebase.OnAuthStateChanged([](bool authenticated, const std::string& userId) {
    if (authenticated) {
        // User signed in
    } else {
        // User signed out
    }
});
```

## Matchmaking

### Player Info

```cpp
Vehement::PlayerInfo playerInfo;
playerInfo.displayName = "PlayerOne";
playerInfo.level = 10;
playerInfo.status = PlayerInfo::Status::Online;
```

### Joining a Town

```cpp
#include <game/src/network/Matchmaking.hpp>

auto& matchmaking = Vehement::Matchmaking::Instance();

// Initialize matchmaking
Matchmaking::Config config;
config.maxPlayersPerTown = 32;
config.heartbeatIntervalMs = 5000;
config.offlineTimeoutMs = 15000;

matchmaking.Initialize(config);

// Join a town
TownInfo town;
town.id = "town_123";
town.name = "Main Street";

matchmaking.JoinTown(town, [](bool success) {
    if (success) {
        std::cout << "Joined town!\n";
    }
});
```

### Player Events

```cpp
// When a player joins
matchmaking.OnPlayerJoined([](const PlayerInfo& player) {
    std::cout << player.displayName << " joined!\n";
    SpawnPlayerAvatar(player);
});

// When a player leaves
matchmaking.OnPlayerLeft([](const std::string& playerId) {
    std::cout << "Player " << playerId << " left\n";
    RemovePlayerAvatar(playerId);
});

// When player data updates
matchmaking.OnPlayerUpdated([](const PlayerInfo& player) {
    UpdatePlayerAvatar(player);
});
```

### Finding Towns

```cpp
// Get current GPS location
GPSCoordinates location = GPS::GetCurrentLocation();

// Find nearby towns with players
matchmaking.FindNearbyTowns(location, 50.0f, // 50km radius
    [](const std::vector<std::pair<TownInfo, int>>& towns) {
        for (const auto& [town, playerCount] : towns) {
            std::cout << town.name << ": " << playerCount << " players\n";
        }
    });

// Auto-join best town
TownFinder::SearchCriteria criteria;
criteria.location = location;
criteria.maxRadiusKm = 50.0f;
criteria.minPlayers = 1;
criteria.preferPopulated = true;

TownFinder::FindBestTown(criteria, [&](std::optional<TownInfo> town) {
    if (town) {
        matchmaking.JoinTown(*town);
    } else {
        // Create new town
        TownFinder::GetOrCreateTownForLocation(location, [&](TownInfo newTown) {
            matchmaking.JoinTown(newTown);
        });
    }
});
```

## State Replication

### Syncing Entity State

```cpp
#include <game/src/network/MultiplayerSync.hpp>

// Create sync manager
MultiplayerSync sync;
sync.Initialize(&firebase);

// Register syncable entity
sync.RegisterEntity(entityId, {
    "position",
    "rotation",
    "health",
    "state"
});

// Update entity (call when state changes)
sync.UpdateEntity(entityId, {
    {"position", {x, y, z}},
    {"rotation", rotation},
    {"health", health}
});

// Listen for remote updates
sync.OnEntityUpdated([](uint64_t entityId, const json& state) {
    if (IsRemoteEntity(entityId)) {
        ApplyRemoteState(entityId, state);
    }
});
```

### Ownership Model

```cpp
// Check if we own an entity
bool isOwner = sync.IsOwner(entityId);

// Request ownership
sync.RequestOwnership(entityId, [](bool granted) {
    if (granted) {
        // Now have authority over entity
    }
});

// Transfer ownership
sync.TransferOwnership(entityId, newOwnerId);
```

### Interpolation

```cpp
class NetworkInterpolator {
    struct StateSnapshot {
        float timestamp;
        glm::vec3 position;
        glm::quat rotation;
    };

    std::deque<StateSnapshot> buffer;
    static constexpr float INTERPOLATION_DELAY = 0.1f;  // 100ms

    void AddSnapshot(float time, const glm::vec3& pos, const glm::quat& rot) {
        buffer.push_back({time, pos, rot});

        // Keep only last second of snapshots
        while (buffer.size() > 60) {
            buffer.pop_front();
        }
    }

    void GetInterpolatedState(float currentTime,
                              glm::vec3& outPos,
                              glm::quat& outRot) {
        float targetTime = currentTime - INTERPOLATION_DELAY;

        // Find surrounding snapshots
        StateSnapshot* before = nullptr;
        StateSnapshot* after = nullptr;

        for (size_t i = 0; i < buffer.size() - 1; i++) {
            if (buffer[i].timestamp <= targetTime &&
                buffer[i + 1].timestamp > targetTime) {
                before = &buffer[i];
                after = &buffer[i + 1];
                break;
            }
        }

        if (before && after) {
            float t = (targetTime - before->timestamp) /
                      (after->timestamp - before->timestamp);
            outPos = glm::mix(before->position, after->position, t);
            outRot = glm::slerp(before->rotation, after->rotation, t);
        } else if (!buffer.empty()) {
            outPos = buffer.back().position;
            outRot = buffer.back().rotation;
        }
    }
};
```

## RPCs (Remote Procedure Calls)

### Defining RPCs

```cpp
// Register RPC handlers
sync.RegisterRPC("DealDamage", [](const json& params) {
    uint64_t targetId = params["target"];
    float damage = params["damage"];
    std::string damageType = params["type"];

    if (auto* entity = GetEntity(targetId)) {
        entity->TakeDamage(damage, damageType);
    }
});

sync.RegisterRPC("SpawnEffect", [](const json& params) {
    std::string effectId = params["effect"];
    glm::vec3 position = JsonToVec3(params["position"]);

    SpawnEffect(effectId, position);
});
```

### Calling RPCs

```cpp
// Call RPC on all clients
sync.CallRPC("DealDamage", {
    {"target", targetId},
    {"damage", 50.0f},
    {"type", "fire"}
});

// Call RPC on specific client
sync.CallRPCOn(targetPlayerId, "ShowMessage", {
    {"text", "You were hit!"}
});

// Call RPC on server
sync.CallRPCOnServer("ValidateAction", {
    {"action", "purchase"},
    {"item", itemId},
    {"cost", cost}
});
```

## Lag Compensation

### Client-Side Prediction

```cpp
class PredictedMovement {
    struct PredictionEntry {
        uint32_t sequenceNum;
        glm::vec3 position;
        glm::vec3 velocity;
        float timestamp;
    };

    std::deque<PredictionEntry> pendingPredictions;
    uint32_t nextSequence = 0;

    void PredictMove(const glm::vec3& input, float dt) {
        // Apply movement locally
        glm::vec3 newPos = position + input * moveSpeed * dt;
        SetPosition(newPos);

        // Record prediction
        pendingPredictions.push_back({
            nextSequence++,
            newPos,
            input * moveSpeed,
            GetTime()
        });

        // Send to server
        sync.SendInput({
            {"sequence", nextSequence - 1},
            {"input", Vec3ToJson(input)},
            {"dt", dt}
        });
    }

    void OnServerCorrection(uint32_t ackSequence, const glm::vec3& serverPos) {
        // Remove acknowledged predictions
        while (!pendingPredictions.empty() &&
               pendingPredictions.front().sequenceNum <= ackSequence) {
            pendingPredictions.pop_front();
        }

        // Check for misprediction
        if (glm::distance(serverPos, position) > CORRECTION_THRESHOLD) {
            // Snap to server position
            SetPosition(serverPos);

            // Re-apply pending predictions
            for (const auto& pred : pendingPredictions) {
                ApplyPrediction(pred);
            }
        }
    }
};
```

### Server Reconciliation

```cpp
class ServerAuthority {
    struct PlayerState {
        glm::vec3 position;
        uint32_t lastProcessedInput;
        std::queue<InputPacket> pendingInputs;
    };

    void ProcessClientInput(const std::string& playerId,
                            const InputPacket& input) {
        auto& state = playerStates[playerId];

        // Validate input timing
        if (!ValidateInputTiming(input)) {
            // Reject suspicious input
            return;
        }

        // Simulate movement on server
        glm::vec3 newPos = SimulateMovement(state.position, input);

        // Validate new position
        if (IsValidPosition(newPos)) {
            state.position = newPos;
            state.lastProcessedInput = input.sequence;

            // Broadcast to other players
            BroadcastPlayerState(playerId, state);
        } else {
            // Force correction
            SendCorrection(playerId, state.position, state.lastProcessedInput);
        }
    }
};
```

### Hit Registration

```cpp
class LagCompensatedHitReg {
    struct HistoricalState {
        float timestamp;
        std::unordered_map<uint64_t, AABB> entityBounds;
    };

    std::deque<HistoricalState> history;
    static constexpr float MAX_HISTORY = 1.0f;  // 1 second

    void RecordState(float time) {
        HistoricalState state;
        state.timestamp = time;

        for (auto* entity : GetAllEntities()) {
            state.entityBounds[entity->GetId()] = entity->GetBounds();
        }

        history.push_back(std::move(state));

        // Prune old history
        while (!history.empty() &&
               history.front().timestamp < time - MAX_HISTORY) {
            history.pop_front();
        }
    }

    bool CheckHit(uint64_t shooterId, const Ray& ray, float clientTime) {
        // Rewind to client's view time
        float serverTime = GetServerTime();
        float latency = GetPlayerLatency(shooterId);
        float rewindTime = serverTime - latency;

        // Find historical state
        const HistoricalState* state = FindStateAt(rewindTime);
        if (!state) return false;

        // Check hit against historical positions
        for (const auto& [entityId, bounds] : state->entityBounds) {
            if (entityId == shooterId) continue;  // Can't hit self

            if (RayIntersectsAABB(ray, bounds)) {
                return true;
            }
        }

        return false;
    }
};
```

## Best Practices

### Bandwidth Optimization

1. **Delta Compression**
```cpp
// Only send changed values
json GetDelta(const EntityState& prev, const EntityState& curr) {
    json delta;
    if (prev.position != curr.position) {
        delta["p"] = Vec3ToJson(curr.position);
    }
    if (prev.rotation != curr.rotation) {
        delta["r"] = QuatToJson(curr.rotation);
    }
    return delta;
}
```

2. **Quantization**
```cpp
// Reduce precision for network
uint16_t QuantizePosition(float value, float min, float max) {
    float normalized = (value - min) / (max - min);
    return static_cast<uint16_t>(normalized * 65535);
}
```

3. **Update Rates**
```cpp
// Different rates for different data
static constexpr float POSITION_UPDATE_RATE = 1.0f / 20;  // 20 Hz
static constexpr float HEALTH_UPDATE_RATE = 1.0f / 5;     // 5 Hz
static constexpr float INVENTORY_UPDATE_RATE = 1.0f / 1;  // 1 Hz
```

### Error Handling

```cpp
// Handle network errors gracefully
sync.OnError([](const NetworkError& error) {
    switch (error.type) {
        case NetworkError::ConnectionLost:
            ShowReconnectDialog();
            AttemptReconnect();
            break;

        case NetworkError::AuthenticationFailed:
            ShowLoginDialog();
            break;

        case NetworkError::RateLimited:
            // Back off
            SetUpdateRate(GetUpdateRate() * 0.5f);
            break;

        case NetworkError::DataCorrupted:
            // Request full state sync
            RequestFullSync();
            break;
    }
});
```

### Security

1. **Server Authority**
```cpp
// Validate all client actions server-side
bool ValidateAttack(const AttackRequest& request) {
    // Check cooldown
    if (GetTimeSinceLastAttack(request.attackerId) < ATTACK_COOLDOWN) {
        return false;
    }

    // Check range
    float distance = GetDistance(request.attackerId, request.targetId);
    if (distance > MAX_ATTACK_RANGE) {
        return false;
    }

    // Check line of sight
    if (!HasLineOfSight(request.attackerId, request.targetId)) {
        return false;
    }

    return true;
}
```

2. **Rate Limiting**
```cpp
class RateLimiter {
    std::unordered_map<std::string, std::deque<float>> actionHistory;

    bool CanPerformAction(const std::string& playerId,
                          const std::string& action,
                          int maxPerSecond) {
        auto& history = actionHistory[playerId + action];
        float now = GetTime();

        // Remove old entries
        while (!history.empty() && history.front() < now - 1.0f) {
            history.pop_front();
        }

        if (history.size() >= maxPerSecond) {
            return false;
        }

        history.push_back(now);
        return true;
    }
};
```

### Offline Support

```cpp
// Queue actions while offline
class OfflineQueue {
    std::vector<json> pendingActions;

    void QueueAction(const json& action) {
        pendingActions.push_back(action);
        SaveToLocalStorage();
    }

    void OnReconnect() {
        for (const auto& action : pendingActions) {
            sync.SendAction(action);
        }
        pendingActions.clear();
    }
};
```

## Debugging

### Network Statistics

```cpp
struct NetworkStats {
    float latency;
    float jitter;
    float packetLoss;
    size_t bytesSent;
    size_t bytesReceived;
    size_t messagesSent;
    size_t messagesReceived;
};

void DrawNetworkOverlay() {
    auto stats = sync.GetStats();

    ImGui::Text("Latency: %.0f ms", stats.latency * 1000);
    ImGui::Text("Jitter: %.0f ms", stats.jitter * 1000);
    ImGui::Text("Packet Loss: %.1f%%", stats.packetLoss * 100);
    ImGui::Text("Upload: %.1f KB/s", stats.bytesSent / 1024.0f);
    ImGui::Text("Download: %.1f KB/s", stats.bytesReceived / 1024.0f);
}
```

### Simulating Network Conditions

```cpp
// Add artificial latency for testing
sync.SetSimulatedLatency(100);  // 100ms

// Add packet loss
sync.SetSimulatedPacketLoss(0.05f);  // 5% loss

// Add jitter
sync.SetSimulatedJitter(20);  // +/- 20ms
```
