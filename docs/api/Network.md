# Networking API

The networking system provides Firebase integration for multiplayer features, matchmaking, and state synchronization.

## Matchmaking Class

**Header**: `game/src/network/Matchmaking.hpp`

Player matchmaking and presence management.

### Instance

```cpp
[[nodiscard]] static Matchmaking& Instance();
```

### Initialization

```cpp
[[nodiscard]] bool Initialize(const Config& config = {});
void Shutdown();
[[nodiscard]] bool IsInitialized() const;
```

### Config Struct

```cpp
struct Config {
    int maxPlayersPerTown = 32;
    int heartbeatIntervalMs = 5000;
    int offlineTimeoutMs = 15000;
    bool autoReconnect = true;
};
```

### Town Operations

```cpp
void JoinTown(const TownInfo& town, std::function<void(bool success)> callback = nullptr);
void LeaveTown();
[[nodiscard]] std::string GetCurrentTownId() const;
[[nodiscard]] bool IsInTown() const;
```

### Player Information

```cpp
[[nodiscard]] std::vector<PlayerInfo> GetPlayersInTown() const;
[[nodiscard]] int GetPlayerCount() const;
[[nodiscard]] std::optional<PlayerInfo> GetPlayer(const std::string& playerId) const;
[[nodiscard]] const PlayerInfo& GetLocalPlayer() const;
void UpdateLocalPlayer(const PlayerInfo& info);
void SetDisplayName(const std::string& name);
void UpdatePosition(float x, float y, float z, float rotation);
```

### Event Callbacks

```cpp
using PlayerCallback = std::function<void(const PlayerInfo& player)>;
using PlayerLeftCallback = std::function<void(const std::string& playerId)>;

void OnPlayerJoined(PlayerCallback callback);
void OnPlayerLeft(PlayerLeftCallback callback);
void OnPlayerUpdated(PlayerCallback callback);
```

### Town Discovery

```cpp
void FindNearbyTowns(GPSCoordinates location, float radiusKm,
    std::function<void(const std::vector<std::pair<TownInfo, int>>&)> callback);

void GetTownPlayerCount(const std::string& townId,
    std::function<void(int count)> callback);
```

### Update

```cpp
void Update(float deltaTime);
```

Call from game loop to process presence updates.

---

## PlayerInfo Struct

```cpp
struct PlayerInfo {
    std::string oderId;       // Unique player ID
    std::string displayName;  // Display name
    std::string townId;       // Current town
    float x = 0, y = 0, z = 0;
    float rotation = 0;

    enum class Status {
        Online,
        Away,
        Busy,
        Offline
    } status = Status::Online;

    int64_t lastSeen = 0;
    int level = 1;
    int zombiesKilled = 0;
    bool isHost = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static PlayerInfo FromJson(const nlohmann::json& j);
    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] bool IsOnline() const;
};
```

---

## TownInfo Struct

```cpp
struct TownInfo {
    std::string id;
    std::string name;
    GPSCoordinates location;
    float radiusKm;
};
```

---

## TownFinder Class

Static utilities for finding towns.

### SearchCriteria Struct

```cpp
struct SearchCriteria {
    GPSCoordinates location;
    float maxRadiusKm = 50.0f;
    int minPlayers = 0;
    int maxPlayers = 32;
    bool preferNearby = true;
    bool preferPopulated = true;
};
```

### Static Methods

```cpp
static void FindBestTown(const SearchCriteria& criteria,
    std::function<void(std::optional<TownInfo>)> callback);

static void FindMatchingTowns(const SearchCriteria& criteria,
    std::function<void(std::vector<TownInfo>)> callback);

static void GetOrCreateTownForLocation(GPSCoordinates location,
    std::function<void(TownInfo)> callback);
```

---

## GPSCoordinates Struct

```cpp
struct GPSCoordinates {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    float accuracy = 0.0f;

    [[nodiscard]] float DistanceTo(const GPSCoordinates& other) const;
};
```

---

## GPSLocation Class

**Header**: `game/src/network/GPSLocation.hpp`

GPS location services.

```cpp
class GPSLocation {
public:
    static GPSLocation& Instance();

    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsAvailable() const;

    void RequestPermission(std::function<void(bool granted)> callback);
    [[nodiscard]] bool HasPermission() const;

    void StartUpdates();
    void StopUpdates();
    [[nodiscard]] bool IsUpdating() const;

    [[nodiscard]] GPSCoordinates GetCurrentLocation() const;
    [[nodiscard]] GPSCoordinates GetLastKnownLocation() const;

    using LocationCallback = std::function<void(const GPSCoordinates&)>;
    void OnLocationUpdated(LocationCallback callback);
    void OnLocationError(std::function<void(const std::string&)> callback);
};
```

---

## MultiplayerSync Class

**Header**: `game/src/network/MultiplayerSync.hpp`

State synchronization between players.

### Methods

```cpp
void Initialize(FirebaseManager* firebase);
void Shutdown();

// Entity sync
void RegisterEntity(uint64_t entityId, const std::vector<std::string>& syncedProperties);
void UnregisterEntity(uint64_t entityId);
void UpdateEntity(uint64_t entityId, const nlohmann::json& state);
void OnEntityUpdated(std::function<void(uint64_t, const nlohmann::json&)> callback);

// Ownership
[[nodiscard]] bool IsOwner(uint64_t entityId) const;
void RequestOwnership(uint64_t entityId, std::function<void(bool)> callback);
void TransferOwnership(uint64_t entityId, const std::string& newOwnerId);

// RPCs
void RegisterRPC(const std::string& name, std::function<void(const nlohmann::json&)> handler);
void CallRPC(const std::string& name, const nlohmann::json& params);
void CallRPCOn(const std::string& playerId, const std::string& name, const nlohmann::json& params);
void CallRPCOnServer(const std::string& name, const nlohmann::json& params);

// Input sync
void SendInput(const nlohmann::json& input);
void OnInputReceived(std::function<void(const std::string&, const nlohmann::json&)> callback);

// Update
void Update(float deltaTime);
```

---

## FirebaseManager Class

**Header**: `game/src/network/FirebaseManager.hpp`

Firebase database and authentication wrapper.

### Initialization

```cpp
[[nodiscard]] static FirebaseManager& Instance();
[[nodiscard]] bool Initialize(const FirebaseConfig& config);
void Shutdown();
[[nodiscard]] bool IsInitialized() const;
```

### Authentication

```cpp
void SignInAnonymously(std::function<void(bool, const std::string&)> callback);
void SignInWithEmail(const std::string& email, const std::string& password,
    std::function<void(bool, const std::string&)> callback);
void SignOut();
[[nodiscard]] bool IsAuthenticated() const;
[[nodiscard]] std::string GetUserId() const;
```

### Database Operations

```cpp
void Set(const std::string& path, const nlohmann::json& value,
    std::function<void(bool)> callback = nullptr);

void Get(const std::string& path,
    std::function<void(bool, const nlohmann::json&)> callback);

void Push(const std::string& path, const nlohmann::json& value,
    std::function<void(bool, const std::string&)> callback = nullptr);

void Update(const std::string& path, const nlohmann::json& updates,
    std::function<void(bool)> callback = nullptr);

void Remove(const std::string& path,
    std::function<void(bool)> callback = nullptr);
```

### Listeners

```cpp
std::string Listen(const std::string& path,
    std::function<void(const nlohmann::json&)> callback);

void StopListening(const std::string& listenerId);
```

### Callbacks

```cpp
void OnConnectionStateChanged(std::function<void(bool)> callback);
void OnAuthStateChanged(std::function<void(bool, const std::string&)> callback);
```

---

## Usage Examples

### Basic Matchmaking

```cpp
auto& matchmaking = Matchmaking::Instance();

// Initialize
Matchmaking::Config config;
config.maxPlayersPerTown = 32;
matchmaking.Initialize(config);

// Setup callbacks
matchmaking.OnPlayerJoined([](const PlayerInfo& player) {
    std::cout << player.displayName << " joined!\n";
    SpawnPlayerAvatar(player);
});

matchmaking.OnPlayerLeft([](const std::string& playerId) {
    RemovePlayerAvatar(playerId);
});

// Find and join town
auto location = GPSLocation::Instance().GetCurrentLocation();
TownFinder::GetOrCreateTownForLocation(location, [&](TownInfo town) {
    matchmaking.JoinTown(town, [](bool success) {
        if (success) {
            std::cout << "Joined town!\n";
        }
    });
});

// Update in game loop
void Update(float dt) {
    matchmaking.Update(dt);
}
```

### State Synchronization

```cpp
MultiplayerSync sync;
sync.Initialize(&FirebaseManager::Instance());

// Register entity for sync
sync.RegisterEntity(playerId, {"position", "rotation", "health", "state"});

// Update state when changed
void OnPlayerMoved() {
    sync.UpdateEntity(playerId, {
        {"position", {player.x, player.y, player.z}},
        {"rotation", player.rotation}
    });
}

// Handle remote updates
sync.OnEntityUpdated([](uint64_t id, const nlohmann::json& state) {
    if (IsRemotePlayer(id)) {
        ApplyRemoteState(id, state);
    }
});
```

### RPCs

```cpp
// Register RPC handler
sync.RegisterRPC("DealDamage", [](const nlohmann::json& params) {
    uint64_t target = params["target"];
    float damage = params["damage"];
    DealDamage(target, damage);
});

// Call RPC
sync.CallRPC("DealDamage", {
    {"target", targetId},
    {"damage", 50.0f}
});

// Call RPC on specific player
sync.CallRPCOn(targetPlayerId, "ShowMessage", {
    {"text", "You were hit!"}
});
```

### Firebase Database

```cpp
auto& firebase = FirebaseManager::Instance();

// Write data
firebase.Set("players/" + oderId, {
    {"name", playerName},
    {"level", 10},
    {"lastSeen", time(nullptr)}
});

// Read data
firebase.Get("players/" + oderId, [](bool success, const nlohmann::json& data) {
    if (success) {
        std::string name = data["name"];
        int level = data["level"];
    }
});

// Listen for changes
std::string listenerId = firebase.Listen("towns/" + townId + "/players",
    [](const nlohmann::json& players) {
        UpdatePlayerList(players);
    });

// Stop listening
firebase.StopListening(listenerId);
```

### GPS Location

```cpp
auto& gps = GPSLocation::Instance();
gps.Initialize();

// Request permission (mobile)
gps.RequestPermission([](bool granted) {
    if (granted) {
        gps.StartUpdates();
    }
});

// Get location
gps.OnLocationUpdated([](const GPSCoordinates& coords) {
    std::cout << "Location: " << coords.latitude << ", " << coords.longitude << "\n";

    // Find nearby towns
    matchmaking.FindNearbyTowns(coords, 50.0f, [](auto towns) {
        for (const auto& [town, players] : towns) {
            std::cout << town.name << ": " << players << " players\n";
        }
    });
});
```
