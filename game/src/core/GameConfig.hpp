#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <array>
#include <glm/glm.hpp>

namespace Vehement {

/**
 * @brief Game configuration constants and compile-time settings
 *
 * Contains all game-specific constants for:
 * - World/Map configuration
 * - Zombie and entity parameters
 * - Weapon statistics
 * - Economy values
 * - Network/Firebase configuration
 * - GPS and location settings
 */
namespace Config {

// =============================================================================
// Version Information
// =============================================================================
inline constexpr std::string_view GameName = "Vehement2";
inline constexpr std::string_view GameVersion = "1.0.0";
inline constexpr uint32_t VersionMajor = 1;
inline constexpr uint32_t VersionMinor = 0;
inline constexpr uint32_t VersionPatch = 0;

// =============================================================================
// World and Map Configuration
// =============================================================================
namespace World {
    // Tile dimensions
    inline constexpr float TileSize = 32.0f;
    inline constexpr float TileHalfSize = TileSize / 2.0f;

    // Map dimensions (in tiles)
    inline constexpr int32_t MapWidth = 100;
    inline constexpr int32_t MapHeight = 100;
    inline constexpr int32_t MaxMapLayers = 4;

    // World bounds (in world units)
    inline constexpr float WorldWidth = MapWidth * TileSize;
    inline constexpr float WorldHeight = MapHeight * TileSize;

    // Chunk system for streaming
    inline constexpr int32_t ChunkSize = 16;      // Tiles per chunk
    inline constexpr int32_t ViewDistance = 3;     // Chunks to load around player

    // Zone configuration
    inline constexpr int32_t MaxZones = 50;
    inline constexpr int32_t MaxPortals = 20;
    inline constexpr int32_t MaxWaypoints = 1000;
}

// =============================================================================
// Zombie Configuration
// =============================================================================
namespace Zombie {
    // Spawn settings
    inline constexpr float BaseSpawnRate = 5.0f;           // Seconds between spawns
    inline constexpr float MinSpawnRate = 1.0f;            // Minimum spawn interval
    inline constexpr float SpawnRateDecreasePerWave = 0.5f;
    inline constexpr int32_t MaxZombiesPerZone = 50;
    inline constexpr int32_t MaxTotalZombies = 200;

    // Movement
    inline constexpr float WalkSpeed = 50.0f;              // Units per second
    inline constexpr float RunSpeed = 120.0f;
    inline constexpr float CrawlSpeed = 25.0f;

    // Combat stats
    inline constexpr float BaseDamage = 10.0f;
    inline constexpr float AttackRange = 48.0f;            // Melee attack range
    inline constexpr float AttackCooldown = 1.5f;          // Seconds
    inline constexpr float DetectionRange = 300.0f;
    inline constexpr float ChaseRange = 500.0f;

    // Health by type
    inline constexpr float HealthNormal = 100.0f;
    inline constexpr float HealthFast = 60.0f;
    inline constexpr float HealthTank = 500.0f;
    inline constexpr float HealthBoss = 2000.0f;

    // Infection mechanics
    inline constexpr float InfectionChance = 0.15f;        // 15% per hit
    inline constexpr float InfectionDamagePerSecond = 2.0f;
    inline constexpr float InfectionDuration = 30.0f;      // Seconds until cured or death
}

// =============================================================================
// Player Configuration
// =============================================================================
namespace Player {
    // Health and stamina
    inline constexpr float MaxHealth = 100.0f;
    inline constexpr float MaxStamina = 100.0f;
    inline constexpr float HealthRegenRate = 1.0f;         // Per second (when safe)
    inline constexpr float StaminaRegenRate = 15.0f;
    inline constexpr float StaminaDrainSprint = 20.0f;

    // Movement
    inline constexpr float WalkSpeed = 100.0f;
    inline constexpr float SprintSpeed = 180.0f;
    inline constexpr float CrouchSpeed = 50.0f;

    // Inventory
    inline constexpr int32_t MaxInventorySlots = 20;
    inline constexpr int32_t MaxWeaponSlots = 3;
    inline constexpr int32_t MaxQuickSlots = 4;

    // Experience
    inline constexpr int32_t MaxLevel = 50;
    inline constexpr float BaseXPRequirement = 100.0f;
    inline constexpr float XPMultiplierPerLevel = 1.5f;
}

// =============================================================================
// Weapon Configuration
// =============================================================================
namespace Weapons {
    // Pistol
    struct PistolStats {
        static inline constexpr float Damage = 25.0f;
        static inline constexpr float FireRate = 0.3f;     // Seconds between shots
        static inline constexpr int32_t MagazineSize = 12;
        static inline constexpr float ReloadTime = 1.5f;
        static inline constexpr float Range = 400.0f;
        static inline constexpr float Accuracy = 0.85f;
        static inline constexpr int32_t Price = 0;         // Starting weapon
    };

    // Shotgun
    struct ShotgunStats {
        static inline constexpr float Damage = 15.0f;      // Per pellet
        static inline constexpr int32_t PelletCount = 8;
        static inline constexpr float FireRate = 0.8f;
        static inline constexpr int32_t MagazineSize = 6;
        static inline constexpr float ReloadTime = 2.5f;
        static inline constexpr float Range = 150.0f;
        static inline constexpr float Spread = 0.3f;
        static inline constexpr int32_t Price = 500;
    };

    // Assault Rifle
    struct AssaultRifleStats {
        static inline constexpr float Damage = 30.0f;
        static inline constexpr float FireRate = 0.1f;
        static inline constexpr int32_t MagazineSize = 30;
        static inline constexpr float ReloadTime = 2.0f;
        static inline constexpr float Range = 600.0f;
        static inline constexpr float Accuracy = 0.75f;
        static inline constexpr int32_t Price = 1500;
    };

    // Sniper Rifle
    struct SniperStats {
        static inline constexpr float Damage = 150.0f;
        static inline constexpr float FireRate = 1.5f;
        static inline constexpr int32_t MagazineSize = 5;
        static inline constexpr float ReloadTime = 3.0f;
        static inline constexpr float Range = 1500.0f;
        static inline constexpr float Accuracy = 0.98f;
        static inline constexpr float HeadshotMultiplier = 3.0f;
        static inline constexpr int32_t Price = 3000;
    };

    // Melee - Knife
    struct KnifeStats {
        static inline constexpr float Damage = 50.0f;
        static inline constexpr float AttackRate = 0.5f;
        static inline constexpr float Range = 48.0f;
        static inline constexpr int32_t Price = 0;
    };
}

// =============================================================================
// Economy Configuration
// =============================================================================
namespace Economy {
    // Starting values
    inline constexpr int32_t StartingCoins = 100;
    inline constexpr int32_t StartingGems = 0;

    // Kill rewards
    inline constexpr int32_t CoinsPerZombieKill = 10;
    inline constexpr int32_t CoinsPerBossKill = 100;
    inline constexpr int32_t XPPerZombieKill = 25;
    inline constexpr int32_t XPPerBossKill = 250;

    // Wave completion bonuses
    inline constexpr int32_t WaveCompletionBonus = 50;
    inline constexpr float WaveBonusMultiplier = 1.1f;     // Per wave

    // Shop prices (base values)
    inline constexpr int32_t HealthPackPrice = 50;
    inline constexpr int32_t AmmoPackPrice = 30;
    inline constexpr int32_t ArmorPrice = 200;
    inline constexpr int32_t ReviveTokenPrice = 500;

    // Premium currency conversion
    inline constexpr int32_t GemsPerDollar = 100;
    inline constexpr int32_t CoinsPerGem = 10;
}

// =============================================================================
// Firebase Configuration
// =============================================================================
namespace Firebase {
    // Project settings (replace with actual values)
    inline constexpr std::string_view ProjectId = "vehement2-game";
    inline constexpr std::string_view ApiKey = "YOUR_API_KEY_HERE";
    inline constexpr std::string_view AuthDomain = "vehement2-game.firebaseapp.com";
    inline constexpr std::string_view DatabaseURL = "https://vehement2-game.firebaseio.com";
    inline constexpr std::string_view StorageBucket = "vehement2-game.appspot.com";

    // Realtime Database paths
    inline constexpr std::string_view PlayersPath = "players";
    inline constexpr std::string_view LeaderboardPath = "leaderboard";
    inline constexpr std::string_view MatchesPath = "matches";
    inline constexpr std::string_view WorldStatePath = "world_state";

    // Sync settings
    inline constexpr float SyncIntervalSeconds = 0.1f;     // 10Hz update rate
    inline constexpr float PositionSyncThreshold = 5.0f;   // Min distance to sync
    inline constexpr int32_t MaxPlayersPerMatch = 8;
    inline constexpr float MatchTimeoutSeconds = 300.0f;
}

// =============================================================================
// GPS and Location Configuration
// =============================================================================
namespace GPS {
    // Real-world to game-world conversion
    inline constexpr double MetersPerGameUnit = 1.0;
    inline constexpr double DegreesLatPerTile = 0.0001;    // ~11m per tile
    inline constexpr double DegreesLonPerTile = 0.0001;

    // Location update settings
    inline constexpr float UpdateIntervalSeconds = 1.0f;
    inline constexpr float MinDistanceMeters = 5.0f;       // Min movement to update
    inline constexpr float AccuracyThresholdMeters = 20.0f;

    // Geofencing
    inline constexpr float SafeZoneRadiusMeters = 50.0f;
    inline constexpr float SpawnZoneRadiusMeters = 100.0f;
    inline constexpr float MaxPlayAreaRadiusMeters = 1000.0f;
}

// =============================================================================
// Audio Configuration
// =============================================================================
namespace Audio {
    inline constexpr float MasterVolume = 1.0f;
    inline constexpr float MusicVolume = 0.7f;
    inline constexpr float SFXVolume = 1.0f;
    inline constexpr float AmbientVolume = 0.5f;

    inline constexpr int32_t MaxSimultaneousSounds = 32;
    inline constexpr float SoundFalloffDistance = 500.0f;
}

// =============================================================================
// Graphics Configuration
// =============================================================================
namespace Graphics {
    inline constexpr int32_t DefaultWindowWidth = 1280;
    inline constexpr int32_t DefaultWindowHeight = 720;
    inline constexpr bool DefaultFullscreen = false;
    inline constexpr bool DefaultVSync = true;

    inline constexpr int32_t ShadowMapResolution = 2048;
    inline constexpr float DrawDistance = 2000.0f;
    inline constexpr int32_t MaxParticles = 10000;
}

} // namespace Config
} // namespace Vehement
