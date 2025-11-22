#pragma once

#include "Entity.hpp"
#include <vector>
#include <array>

namespace Nova {
    class InputManager;
    class Graph;
}

namespace Vehement {

// Forward declarations
class Weapon;
class EntityManager;

/**
 * @brief GPS/Real-world location data for the player
 * Stored separately from game position
 */
struct GPSPosition {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;     // Meters above sea level
    float accuracy = 0.0f;      // Accuracy in meters
    bool valid = false;         // Whether GPS data is valid

    void Set(double lat, double lon, double alt = 0.0, float acc = 0.0f) {
        latitude = lat;
        longitude = lon;
        altitude = alt;
        accuracy = acc;
        valid = true;
    }

    void Invalidate() { valid = false; }
};

/**
 * @brief Player statistics tracking
 */
struct PlayerStats {
    uint32_t kills = 0;              // Total zombie kills
    uint32_t deaths = 0;             // Number of times player died
    float survivalTime = 0.0f;       // Total survival time in seconds
    uint32_t zombiesKilledThisLife = 0;  // Kills since last death
    float longestSurvival = 0.0f;    // Longest survival streak
    uint32_t npcsRescued = 0;        // NPCs saved from infection
    uint32_t shotsFired = 0;         // Total shots fired
    uint32_t shotsHit = 0;           // Shots that hit targets

    /** @brief Calculate accuracy percentage */
    [[nodiscard]] float GetAccuracy() const {
        return shotsFired > 0 ? (static_cast<float>(shotsHit) / shotsFired) * 100.0f : 0.0f;
    }

    /** @brief Reset stats for new life */
    void OnDeath() {
        deaths++;
        if (survivalTime > longestSurvival) {
            longestSurvival = survivalTime;
        }
        survivalTime = 0.0f;
        zombiesKilledThisLife = 0;
    }
};

/**
 * @brief Weapon slot for player inventory
 */
struct WeaponSlot {
    int weaponId = -1;          // -1 means empty slot
    int ammo = 0;               // Current ammo in magazine
    int reserveAmmo = 0;        // Reserve ammo

    [[nodiscard]] bool IsEmpty() const noexcept { return weaponId < 0; }
    void Clear() { weaponId = -1; ammo = 0; reserveAmmo = 0; }
};

/**
 * @brief Player entity for Vehement2
 *
 * Handles WASD movement, mouse aiming, weapon inventory, currency,
 * statistics tracking, and interaction with game world.
 */
class Player : public Entity {
public:
    static constexpr int MAX_WEAPON_SLOTS = 4;
    static constexpr float DEFAULT_MOVE_SPEED = 8.0f;
    static constexpr float SPRINT_MULTIPLIER = 1.5f;
    static constexpr float INTERACTION_RADIUS = 2.0f;

    Player();
    ~Player() override = default;

    // =========================================================================
    // Core Update/Render
    // =========================================================================

    /**
     * @brief Update player state
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime) override;

    /**
     * @brief Render the player
     */
    void Render(Nova::Renderer& renderer) override;

    /**
     * @brief Process player input
     * @param input The input manager
     * @param deltaTime Time since last frame
     */
    void ProcessInput(Nova::InputManager& input, float deltaTime);

    // =========================================================================
    // Movement
    // =========================================================================

    /**
     * @brief Handle WASD movement input
     * @param input Input manager for key states
     * @param deltaTime Time since last frame
     */
    void HandleMovement(Nova::InputManager& input, float deltaTime);

    /**
     * @brief Handle mouse aiming - rotate to face mouse cursor
     * @param mouseWorldPos Mouse position in world coordinates
     */
    void HandleAiming(const glm::vec2& mouseWorldPos);

    /** @brief Check if player is sprinting */
    [[nodiscard]] bool IsSprinting() const noexcept { return m_sprinting; }

    /** @brief Set sprint state */
    void SetSprinting(bool sprinting) noexcept { m_sprinting = sprinting; }

    /** @brief Get current effective move speed (accounting for sprint) */
    [[nodiscard]] float GetEffectiveMoveSpeed() const noexcept {
        return m_moveSpeed * (m_sprinting ? SPRINT_MULTIPLIER : 1.0f);
    }

    // =========================================================================
    // Weapons
    // =========================================================================

    /** @brief Get current weapon slot index */
    [[nodiscard]] int GetCurrentWeaponSlot() const noexcept { return m_currentWeaponSlot; }

    /** @brief Get weapon in a slot */
    [[nodiscard]] const WeaponSlot& GetWeaponSlot(int slot) const;

    /** @brief Get current weapon slot */
    [[nodiscard]] const WeaponSlot& GetCurrentWeapon() const { return GetWeaponSlot(m_currentWeaponSlot); }

    /**
     * @brief Switch to a weapon slot
     * @param slot Slot index (0 to MAX_WEAPON_SLOTS-1)
     * @return true if switch was successful
     */
    bool SwitchWeapon(int slot);

    /**
     * @brief Switch to next weapon
     */
    void NextWeapon();

    /**
     * @brief Switch to previous weapon
     */
    void PreviousWeapon();

    /**
     * @brief Add a weapon to inventory
     * @param weaponId ID of the weapon to add
     * @param ammo Initial ammo
     * @param reserveAmmo Initial reserve ammo
     * @return Slot index where weapon was added, or -1 if inventory full
     */
    int AddWeapon(int weaponId, int ammo, int reserveAmmo);

    /**
     * @brief Remove weapon from slot
     * @param slot Slot to clear
     */
    void RemoveWeapon(int slot);

    /**
     * @brief Add ammo to current weapon
     * @param amount Ammo to add
     * @return Actual ammo added
     */
    int AddAmmo(int amount);

    /**
     * @brief Fire current weapon
     * @return true if weapon fired successfully
     */
    bool Fire();

    /**
     * @brief Reload current weapon
     * @return true if reload started
     */
    bool Reload();

    /** @brief Check if currently reloading */
    [[nodiscard]] bool IsReloading() const noexcept { return m_reloading; }

    // =========================================================================
    // Currency
    // =========================================================================

    /** @brief Get current coin count */
    [[nodiscard]] int GetCoins() const noexcept { return m_coins; }

    /** @brief Add coins */
    void AddCoins(int amount) { m_coins += amount; }

    /**
     * @brief Spend coins
     * @param amount Amount to spend
     * @return true if player had enough coins
     */
    bool SpendCoins(int amount);

    // =========================================================================
    // Statistics
    // =========================================================================

    /** @brief Get player statistics */
    [[nodiscard]] const PlayerStats& GetStats() const noexcept { return m_stats; }

    /** @brief Get mutable stats (for updating) */
    PlayerStats& GetStats() noexcept { return m_stats; }

    /** @brief Record a kill */
    void RecordKill() {
        m_stats.kills++;
        m_stats.zombiesKilledThisLife++;
    }

    /** @brief Record a shot fired */
    void RecordShotFired() { m_stats.shotsFired++; }

    /** @brief Record a shot hit */
    void RecordShotHit() { m_stats.shotsHit++; }

    // =========================================================================
    // GPS Position
    // =========================================================================

    /** @brief Get GPS position data */
    [[nodiscard]] const GPSPosition& GetGPSPosition() const noexcept { return m_gpsPosition; }

    /** @brief Update GPS position */
    void SetGPSPosition(double latitude, double longitude, double altitude = 0.0, float accuracy = 0.0f) {
        m_gpsPosition.Set(latitude, longitude, altitude, accuracy);
    }

    /** @brief Invalidate GPS position */
    void InvalidateGPS() { m_gpsPosition.Invalidate(); }

    // =========================================================================
    // Interaction
    // =========================================================================

    /**
     * @brief Attempt to interact with nearby objects/shops
     * @param entityManager For finding interactable entities
     * @return true if interaction occurred
     */
    bool TryInteract(EntityManager& entityManager);

    /** @brief Check if player can interact with something */
    [[nodiscard]] bool CanInteract() const noexcept { return m_interactionTarget != Entity::INVALID_ID; }

    /** @brief Get current interaction target entity ID */
    [[nodiscard]] Entity::EntityId GetInteractionTarget() const noexcept { return m_interactionTarget; }

    /** @brief Set interaction target */
    void SetInteractionTarget(Entity::EntityId target) noexcept { m_interactionTarget = target; }

    // =========================================================================
    // Health/Combat Overrides
    // =========================================================================

    /**
     * @brief Override take damage to handle player-specific effects
     */
    float TakeDamage(float amount, EntityId source = INVALID_ID) override;

    /**
     * @brief Handle player death
     */
    void Die() override;

    /**
     * @brief Respawn the player
     * @param position Respawn position
     */
    void Respawn(const glm::vec3& position);

    // =========================================================================
    // Avatar Selection
    // =========================================================================

    /** @brief Get avatar index (Person1-9) */
    [[nodiscard]] int GetAvatarIndex() const noexcept { return m_avatarIndex; }

    /** @brief Set avatar index (Person1-9) */
    void SetAvatarIndex(int index);

    /**
     * @brief Get texture path for avatar
     * @param index Avatar index (1-9)
     */
    static std::string GetAvatarTexturePath(int index);

private:
    // Movement state
    bool m_sprinting = false;
    glm::vec2 m_inputDirection{0.0f};

    // Weapons
    std::array<WeaponSlot, MAX_WEAPON_SLOTS> m_weapons;
    int m_currentWeaponSlot = 0;
    bool m_reloading = false;
    float m_reloadTimer = 0.0f;

    // Currency
    int m_coins = 0;

    // Statistics
    PlayerStats m_stats;

    // GPS
    GPSPosition m_gpsPosition;

    // Interaction
    Entity::EntityId m_interactionTarget = Entity::INVALID_ID;

    // Avatar
    int m_avatarIndex = 1;  // Default to Person1

    // Invulnerability after taking damage
    float m_invulnerabilityTimer = 0.0f;
    static constexpr float INVULNERABILITY_DURATION = 0.5f;
};

} // namespace Vehement
