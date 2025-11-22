#pragma once

#include <string>
#include <memory>
#include <array>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class Texture;
}

namespace Vehement {

// ============================================================================
// Weapon Types
// ============================================================================

/**
 * @brief Enumeration of all weapon types in the game
 */
enum class WeaponType {
    Glock,      // Pistol - fast fire, low damage, common ammo
    AK47,       // Assault rifle - medium fire, medium damage
    Sniper,     // High damage, slow fire, penetrates zombies
    Grenade,    // Thrown, area damage
    FlashNade,  // Stuns zombies (blinds them temporarily)
    StunNade,   // Slows zombies
    Claymore,   // Placeable mine

    Count       // Number of weapon types
};

/**
 * @brief Grenade subtypes for colored variants
 */
enum class GrenadeVariant {
    Green,      // Standard frag grenade
    Grey,       // Smoke grenade
    Red,        // Incendiary grenade
    Flash,      // Flashbang
    Stun,       // Stun/concussion
    Claymore    // Placeable mine
};

// ============================================================================
// Weapon Stats
// ============================================================================

/**
 * @brief Stats that define weapon behavior
 */
struct WeaponStats {
    float damage = 10.0f;           // Damage per hit
    float fireRate = 1.0f;          // Shots per second
    float reloadTime = 2.0f;        // Seconds to reload
    int magazineSize = 10;          // Rounds per magazine
    float range = 100.0f;           // Max effective range
    float spread = 0.0f;            // Accuracy (0 = perfect, higher = less accurate)
    float bulletSpeed = 500.0f;     // Projectile velocity (units/second)
    int penetration = 1;            // How many enemies bullet can hit
    int price = 100;                // Shop price in coins
    int ammoPrice = 25;             // Price per magazine

    // Builder-style setters
    WeaponStats& SetDamage(float d) { damage = d; return *this; }
    WeaponStats& SetFireRate(float r) { fireRate = r; return *this; }
    WeaponStats& SetReloadTime(float t) { reloadTime = t; return *this; }
    WeaponStats& SetMagazineSize(int s) { magazineSize = s; return *this; }
    WeaponStats& SetRange(float r) { range = r; return *this; }
    WeaponStats& SetSpread(float s) { spread = s; return *this; }
    WeaponStats& SetBulletSpeed(float s) { bulletSpeed = s; return *this; }
    WeaponStats& SetPenetration(int p) { penetration = p; return *this; }
    WeaponStats& SetPrice(int p) { price = p; return *this; }
    WeaponStats& SetAmmoPrice(int p) { ammoPrice = p; return *this; }
};

// ============================================================================
// Weapon Texture Paths
// ============================================================================

namespace WeaponTextures {
    // Base path for weapon textures
    constexpr const char* kBasePath = "Vehement2/images/Weapons/";

    // Glock textures
    constexpr const char* kGlockTop = "Vehement2/images/Weapons/GlockTop.png";
    constexpr const char* kGlockTopFiring = "Vehement2/images/Weapons/GlockTopFiring.png";
    constexpr const char* kGlockSide = "Vehement2/images/Weapons/GlockSide.png";

    // AK47 textures
    constexpr const char* kAK47Top = "Vehement2/images/Weapons/AK47Top.png";
    constexpr const char* kAK47TopFiring = "Vehement2/images/Weapons/AK47TopFiring.png";
    constexpr const char* kAK47Side = "Vehement2/images/Weapons/AK47Side.png";

    // Sniper textures
    constexpr const char* kSniperTop = "Vehement2/images/Weapons/SniperTop.png";
    constexpr const char* kSniperTopFiring = "Vehement2/images/Weapons/SniperTopFiring.png";
    constexpr const char* kSniperSide = "Vehement2/images/Weapons/SniperSide.png";

    // Grenade textures
    constexpr const char* kGrenadeGreen = "Vehement2/images/Weapons/GrenadeGreen.png";
    constexpr const char* kGrenadeGrey = "Vehement2/images/Weapons/GrenadeGrey.png";
    constexpr const char* kGrenadeRed = "Vehement2/images/Weapons/GrenadeRed.png";
    constexpr const char* kFlashNade = "Vehement2/images/Weapons/FlashNade.png";
    constexpr const char* kStunNade = "Vehement2/images/Weapons/StunNade.png";
    constexpr const char* kStunNadeDark = "Vehement2/images/Weapons/StunNadeDark.png";
    constexpr const char* kClaymore = "Vehement2/images/Weapons/Claymore.png";
}

// ============================================================================
// Default Weapon Stats
// ============================================================================

namespace DefaultWeaponStats {

    inline WeaponStats GetGlockStats() {
        return WeaponStats()
            .SetDamage(15.0f)
            .SetFireRate(6.0f)          // 6 shots per second
            .SetReloadTime(1.5f)
            .SetMagazineSize(12)
            .SetRange(80.0f)
            .SetSpread(0.02f)           // Very accurate
            .SetBulletSpeed(400.0f)
            .SetPenetration(1)
            .SetPrice(0)                // Starting weapon (free)
            .SetAmmoPrice(15);
    }

    inline WeaponStats GetAK47Stats() {
        return WeaponStats()
            .SetDamage(25.0f)
            .SetFireRate(10.0f)         // 10 shots per second (full auto)
            .SetReloadTime(2.5f)
            .SetMagazineSize(30)
            .SetRange(120.0f)
            .SetSpread(0.05f)           // Moderate spread
            .SetBulletSpeed(500.0f)
            .SetPenetration(2)          // Can hit 2 enemies
            .SetPrice(500)
            .SetAmmoPrice(50);
    }

    inline WeaponStats GetSniperStats() {
        return WeaponStats()
            .SetDamage(100.0f)
            .SetFireRate(1.0f)          // 1 shot per second
            .SetReloadTime(3.0f)
            .SetMagazineSize(5)
            .SetRange(300.0f)
            .SetSpread(0.0f)            // Perfect accuracy
            .SetBulletSpeed(800.0f)
            .SetPenetration(5)          // Penetrates many enemies
            .SetPrice(1500)
            .SetAmmoPrice(75);
    }

    inline WeaponStats GetGrenadeStats() {
        return WeaponStats()
            .SetDamage(150.0f)          // High area damage
            .SetFireRate(0.5f)          // Throw rate
            .SetReloadTime(0.0f)        // No reload
            .SetMagazineSize(1)
            .SetRange(50.0f)            // Throw distance
            .SetSpread(0.0f)
            .SetBulletSpeed(200.0f)     // Throw velocity
            .SetPenetration(0)          // N/A for grenades
            .SetPrice(100)
            .SetAmmoPrice(100);
    }

    inline WeaponStats GetFlashNadeStats() {
        return WeaponStats()
            .SetDamage(0.0f)            // No damage, just flash
            .SetFireRate(0.5f)
            .SetReloadTime(0.0f)
            .SetMagazineSize(1)
            .SetRange(50.0f)
            .SetSpread(0.0f)
            .SetBulletSpeed(200.0f)
            .SetPenetration(0)
            .SetPrice(75)
            .SetAmmoPrice(75);
    }

    inline WeaponStats GetStunNadeStats() {
        return WeaponStats()
            .SetDamage(10.0f)           // Minor damage
            .SetFireRate(0.5f)
            .SetReloadTime(0.0f)
            .SetMagazineSize(1)
            .SetRange(50.0f)
            .SetSpread(0.0f)
            .SetBulletSpeed(200.0f)
            .SetPenetration(0)
            .SetPrice(75)
            .SetAmmoPrice(75);
    }

    inline WeaponStats GetClaymoreStats() {
        return WeaponStats()
            .SetDamage(200.0f)          // Very high damage
            .SetFireRate(1.0f)          // Place rate
            .SetReloadTime(0.0f)
            .SetMagazineSize(1)
            .SetRange(15.0f)            // Trigger radius
            .SetSpread(0.0f)
            .SetBulletSpeed(0.0f)       // Stationary
            .SetPenetration(0)
            .SetPrice(200)
            .SetAmmoPrice(200);
    }

    inline WeaponStats GetStats(WeaponType type) {
        switch (type) {
            case WeaponType::Glock:     return GetGlockStats();
            case WeaponType::AK47:      return GetAK47Stats();
            case WeaponType::Sniper:    return GetSniperStats();
            case WeaponType::Grenade:   return GetGrenadeStats();
            case WeaponType::FlashNade: return GetFlashNadeStats();
            case WeaponType::StunNade:  return GetStunNadeStats();
            case WeaponType::Claymore:  return GetClaymoreStats();
            default:                    return GetGlockStats();
        }
    }
}

// ============================================================================
// Weapon Class
// ============================================================================

/**
 * @brief Represents a weapon instance with current state
 */
class Weapon {
public:
    Weapon();
    explicit Weapon(WeaponType type);
    ~Weapon() = default;

    // Allow copy and move
    Weapon(const Weapon&) = default;
    Weapon& operator=(const Weapon&) = default;
    Weapon(Weapon&&) noexcept = default;
    Weapon& operator=(Weapon&&) noexcept = default;

    /**
     * @brief Initialize weapon with type
     */
    void Initialize(WeaponType type);

    /**
     * @brief Update weapon state (reload timer, fire cooldown)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Attempt to fire the weapon
     * @return true if weapon fired successfully
     */
    bool Fire();

    /**
     * @brief Start reloading the weapon
     * @return true if reload started (has spare ammo and not full)
     */
    bool StartReload();

    /**
     * @brief Cancel current reload
     */
    void CancelReload();

    /**
     * @brief Check if weapon can fire
     */
    [[nodiscard]] bool CanFire() const;

    /**
     * @brief Check if weapon is currently reloading
     */
    [[nodiscard]] bool IsReloading() const { return m_isReloading; }

    /**
     * @brief Check if weapon is currently firing (for animation)
     */
    [[nodiscard]] bool IsFiring() const { return m_firingTimer > 0.0f; }

    /**
     * @brief Get reload progress [0, 1]
     */
    [[nodiscard]] float GetReloadProgress() const;

    /**
     * @brief Add ammo to reserve
     * @param magazines Number of magazines to add
     */
    void AddAmmo(int magazines);

    /**
     * @brief Set total reserve ammo directly
     */
    void SetReserveAmmo(int ammo) { m_reserveAmmo = ammo; }

    // State getters
    [[nodiscard]] WeaponType GetType() const { return m_type; }
    [[nodiscard]] const WeaponStats& GetStats() const { return m_stats; }
    [[nodiscard]] int GetCurrentAmmo() const { return m_currentAmmo; }
    [[nodiscard]] int GetReserveAmmo() const { return m_reserveAmmo; }
    [[nodiscard]] int GetMagazineSize() const { return m_stats.magazineSize; }

    // Texture access
    [[nodiscard]] const std::string& GetTopTexturePath() const { return m_topTexturePath; }
    [[nodiscard]] const std::string& GetTopFiringTexturePath() const { return m_topFiringTexturePath; }
    [[nodiscard]] const std::string& GetSideTexturePath() const { return m_sideTexturePath; }

    /**
     * @brief Get current texture path (normal or firing)
     */
    [[nodiscard]] const std::string& GetCurrentTexturePath() const {
        return IsFiring() ? m_topFiringTexturePath : m_topTexturePath;
    }

    // Callbacks
    using FireCallback = std::function<void(const Weapon&)>;
    void SetFireCallback(FireCallback callback) { m_fireCallback = std::move(callback); }

    using ReloadCallback = std::function<void(const Weapon&)>;
    void SetReloadCompleteCallback(ReloadCallback callback) { m_reloadCompleteCallback = std::move(callback); }

    // Utility
    [[nodiscard]] static const char* GetWeaponName(WeaponType type);
    [[nodiscard]] static bool IsThrowable(WeaponType type);
    [[nodiscard]] static bool IsPlaceable(WeaponType type);

private:
    void SetupTexturePaths();
    void CompleteReload();

    WeaponType m_type = WeaponType::Glock;
    WeaponStats m_stats;

    // Ammo state
    int m_currentAmmo = 0;
    int m_reserveAmmo = 0;

    // Timers
    float m_fireCooldown = 0.0f;        // Time until can fire again
    float m_reloadTimer = 0.0f;         // Remaining reload time
    float m_firingTimer = 0.0f;         // Visual firing effect duration

    bool m_isReloading = false;

    // Texture paths
    std::string m_topTexturePath;
    std::string m_topFiringTexturePath;
    std::string m_sideTexturePath;

    // Callbacks
    FireCallback m_fireCallback;
    ReloadCallback m_reloadCompleteCallback;

    // Constants
    static constexpr float kFiringVisualDuration = 0.1f;  // Duration of muzzle flash
};

// ============================================================================
// Weapon Inventory
// ============================================================================

/**
 * @brief Manages player's weapon inventory
 */
class WeaponInventory {
public:
    static constexpr int kMaxWeapons = 3;           // Max guns
    static constexpr int kMaxGrenades = 6;          // Max grenade stack
    static constexpr int kMaxClaymores = 3;         // Max claymore stack

    WeaponInventory();
    ~WeaponInventory() = default;

    /**
     * @brief Update all weapons
     */
    void Update(float deltaTime);

    /**
     * @brief Add a weapon to inventory
     * @return true if added (has space or same type)
     */
    bool AddWeapon(WeaponType type);

    /**
     * @brief Remove current weapon
     */
    void RemoveCurrentWeapon();

    /**
     * @brief Switch to weapon by slot index
     */
    void SwitchWeapon(int slot);

    /**
     * @brief Cycle to next weapon
     */
    void NextWeapon();

    /**
     * @brief Cycle to previous weapon
     */
    void PreviousWeapon();

    /**
     * @brief Get current weapon (may be nullptr)
     */
    [[nodiscard]] Weapon* GetCurrentWeapon();
    [[nodiscard]] const Weapon* GetCurrentWeapon() const;

    /**
     * @brief Get weapon at slot
     */
    [[nodiscard]] Weapon* GetWeaponAt(int slot);
    [[nodiscard]] const Weapon* GetWeaponAt(int slot) const;

    /**
     * @brief Get current slot index
     */
    [[nodiscard]] int GetCurrentSlot() const { return m_currentSlot; }

    /**
     * @brief Get number of weapons
     */
    [[nodiscard]] int GetWeaponCount() const;

    // Grenade/throwable management
    [[nodiscard]] int GetGrenadeCount(GrenadeVariant variant) const;
    void AddGrenade(GrenadeVariant variant, int count = 1);
    bool UseGrenade(GrenadeVariant variant);

    // Claymore management
    [[nodiscard]] int GetClaymoreCount() const { return m_claymores; }
    void AddClaymores(int count);
    bool UseClaymore();

private:
    std::array<std::unique_ptr<Weapon>, kMaxWeapons> m_weapons;
    int m_currentSlot = 0;

    // Throwables
    std::array<int, 6> m_grenades{};  // Indexed by GrenadeVariant
    int m_claymores = 0;
};

} // namespace Vehement
