#include "Weapon.hpp"

namespace Vehement {

// ============================================================================
// Weapon Implementation
// ============================================================================

Weapon::Weapon() {
    Initialize(WeaponType::Glock);
}

Weapon::Weapon(WeaponType type) {
    Initialize(type);
}

void Weapon::Initialize(WeaponType type) {
    m_type = type;
    m_stats = DefaultWeaponStats::GetStats(type);

    // Initialize ammo
    m_currentAmmo = m_stats.magazineSize;
    m_reserveAmmo = m_stats.magazineSize * 3;  // Start with 3 spare mags

    // Reset timers
    m_fireCooldown = 0.0f;
    m_reloadTimer = 0.0f;
    m_firingTimer = 0.0f;
    m_isReloading = false;

    SetupTexturePaths();
}

void Weapon::SetupTexturePaths() {
    using namespace WeaponTextures;

    switch (m_type) {
        case WeaponType::Glock:
            m_topTexturePath = kGlockTop;
            m_topFiringTexturePath = kGlockTopFiring;
            m_sideTexturePath = kGlockSide;
            break;

        case WeaponType::AK47:
            m_topTexturePath = kAK47Top;
            m_topFiringTexturePath = kAK47TopFiring;
            m_sideTexturePath = kAK47Side;
            break;

        case WeaponType::Sniper:
            m_topTexturePath = kSniperTop;
            m_topFiringTexturePath = kSniperTopFiring;
            m_sideTexturePath = kSniperSide;
            break;

        case WeaponType::Grenade:
            m_topTexturePath = kGrenadeGreen;
            m_topFiringTexturePath = kGrenadeGreen;  // No firing state
            m_sideTexturePath = kGrenadeGreen;
            break;

        case WeaponType::FlashNade:
            m_topTexturePath = kFlashNade;
            m_topFiringTexturePath = kFlashNade;
            m_sideTexturePath = kFlashNade;
            break;

        case WeaponType::StunNade:
            m_topTexturePath = kStunNade;
            m_topFiringTexturePath = kStunNade;
            m_sideTexturePath = kStunNade;
            break;

        case WeaponType::Claymore:
            m_topTexturePath = kClaymore;
            m_topFiringTexturePath = kClaymore;
            m_sideTexturePath = kClaymore;
            break;

        default:
            m_topTexturePath = kGlockTop;
            m_topFiringTexturePath = kGlockTopFiring;
            m_sideTexturePath = kGlockSide;
            break;
    }
}

void Weapon::Update(float deltaTime) {
    // Update fire cooldown
    if (m_fireCooldown > 0.0f) {
        m_fireCooldown -= deltaTime;
    }

    // Update firing visual timer
    if (m_firingTimer > 0.0f) {
        m_firingTimer -= deltaTime;
    }

    // Update reload
    if (m_isReloading) {
        m_reloadTimer -= deltaTime;
        if (m_reloadTimer <= 0.0f) {
            CompleteReload();
        }
    }
}

bool Weapon::Fire() {
    if (!CanFire()) {
        return false;
    }

    // Consume ammo
    m_currentAmmo--;

    // Set cooldown based on fire rate
    m_fireCooldown = 1.0f / m_stats.fireRate;

    // Set firing visual timer
    m_firingTimer = kFiringVisualDuration;

    // Trigger callback
    if (m_fireCallback) {
        m_fireCallback(*this);
    }

    return true;
}

bool Weapon::StartReload() {
    // Can't reload if already reloading
    if (m_isReloading) {
        return false;
    }

    // Can't reload if magazine is full
    if (m_currentAmmo >= m_stats.magazineSize) {
        return false;
    }

    // Can't reload if no reserve ammo (except for throwables which don't reload)
    if (m_reserveAmmo <= 0 && !IsThrowable(m_type) && !IsPlaceable(m_type)) {
        return false;
    }

    m_isReloading = true;
    m_reloadTimer = m_stats.reloadTime;

    return true;
}

void Weapon::CancelReload() {
    m_isReloading = false;
    m_reloadTimer = 0.0f;
}

void Weapon::CompleteReload() {
    m_isReloading = false;
    m_reloadTimer = 0.0f;

    // Calculate how much ammo to add
    int ammoNeeded = m_stats.magazineSize - m_currentAmmo;
    int ammoToAdd = std::min(ammoNeeded, m_reserveAmmo);

    m_currentAmmo += ammoToAdd;
    m_reserveAmmo -= ammoToAdd;

    // Trigger callback
    if (m_reloadCompleteCallback) {
        m_reloadCompleteCallback(*this);
    }
}

bool Weapon::CanFire() const {
    // Can't fire while reloading
    if (m_isReloading) {
        return false;
    }

    // Can't fire if cooldown not finished
    if (m_fireCooldown > 0.0f) {
        return false;
    }

    // Can't fire if no ammo
    if (m_currentAmmo <= 0) {
        return false;
    }

    return true;
}

float Weapon::GetReloadProgress() const {
    if (!m_isReloading) {
        return m_currentAmmo > 0 ? 1.0f : 0.0f;
    }
    return 1.0f - (m_reloadTimer / m_stats.reloadTime);
}

void Weapon::AddAmmo(int magazines) {
    m_reserveAmmo += magazines * m_stats.magazineSize;
}

const char* Weapon::GetWeaponName(WeaponType type) {
    switch (type) {
        case WeaponType::Glock:     return "Glock 17";
        case WeaponType::AK47:      return "AK-47";
        case WeaponType::Sniper:    return "AWP Sniper";
        case WeaponType::Grenade:   return "Frag Grenade";
        case WeaponType::FlashNade: return "Flashbang";
        case WeaponType::StunNade:  return "Stun Grenade";
        case WeaponType::Claymore:  return "Claymore Mine";
        default:                    return "Unknown";
    }
}

bool Weapon::IsThrowable(WeaponType type) {
    return type == WeaponType::Grenade ||
           type == WeaponType::FlashNade ||
           type == WeaponType::StunNade;
}

bool Weapon::IsPlaceable(WeaponType type) {
    return type == WeaponType::Claymore;
}

// ============================================================================
// WeaponInventory Implementation
// ============================================================================

WeaponInventory::WeaponInventory() {
    // Start with a Glock
    m_weapons[0] = std::make_unique<Weapon>(WeaponType::Glock);
    m_currentSlot = 0;

    // Initialize grenade counts to 0
    m_grenades.fill(0);
    m_claymores = 0;
}

void WeaponInventory::Update(float deltaTime) {
    for (auto& weapon : m_weapons) {
        if (weapon) {
            weapon->Update(deltaTime);
        }
    }
}

bool WeaponInventory::AddWeapon(WeaponType type) {
    // Check if we already have this weapon type
    for (auto& weapon : m_weapons) {
        if (weapon && weapon->GetType() == type) {
            // Add ammo instead
            weapon->AddAmmo(2);
            return true;
        }
    }

    // Find empty slot
    for (int i = 0; i < kMaxWeapons; ++i) {
        if (!m_weapons[i]) {
            m_weapons[i] = std::make_unique<Weapon>(type);
            return true;
        }
    }

    return false;  // No space
}

void WeaponInventory::RemoveCurrentWeapon() {
    if (m_weapons[m_currentSlot]) {
        m_weapons[m_currentSlot].reset();

        // Switch to another weapon if available
        for (int i = 0; i < kMaxWeapons; ++i) {
            if (m_weapons[i]) {
                m_currentSlot = i;
                return;
            }
        }
    }
}

void WeaponInventory::SwitchWeapon(int slot) {
    if (slot >= 0 && slot < kMaxWeapons && m_weapons[slot]) {
        // Cancel reload on current weapon
        if (m_weapons[m_currentSlot]) {
            m_weapons[m_currentSlot]->CancelReload();
        }
        m_currentSlot = slot;
    }
}

void WeaponInventory::NextWeapon() {
    int startSlot = m_currentSlot;
    do {
        m_currentSlot = (m_currentSlot + 1) % kMaxWeapons;
        if (m_weapons[m_currentSlot]) {
            // Cancel reload on previous weapon
            if (m_weapons[startSlot]) {
                m_weapons[startSlot]->CancelReload();
            }
            return;
        }
    } while (m_currentSlot != startSlot);
}

void WeaponInventory::PreviousWeapon() {
    int startSlot = m_currentSlot;
    do {
        m_currentSlot = (m_currentSlot - 1 + kMaxWeapons) % kMaxWeapons;
        if (m_weapons[m_currentSlot]) {
            // Cancel reload on previous weapon
            if (m_weapons[startSlot]) {
                m_weapons[startSlot]->CancelReload();
            }
            return;
        }
    } while (m_currentSlot != startSlot);
}

Weapon* WeaponInventory::GetCurrentWeapon() {
    return m_weapons[m_currentSlot].get();
}

const Weapon* WeaponInventory::GetCurrentWeapon() const {
    return m_weapons[m_currentSlot].get();
}

Weapon* WeaponInventory::GetWeaponAt(int slot) {
    if (slot >= 0 && slot < kMaxWeapons) {
        return m_weapons[slot].get();
    }
    return nullptr;
}

const Weapon* WeaponInventory::GetWeaponAt(int slot) const {
    if (slot >= 0 && slot < kMaxWeapons) {
        return m_weapons[slot].get();
    }
    return nullptr;
}

int WeaponInventory::GetWeaponCount() const {
    int count = 0;
    for (const auto& weapon : m_weapons) {
        if (weapon) ++count;
    }
    return count;
}

int WeaponInventory::GetGrenadeCount(GrenadeVariant variant) const {
    int index = static_cast<int>(variant);
    if (index >= 0 && index < static_cast<int>(m_grenades.size())) {
        return m_grenades[index];
    }
    return 0;
}

void WeaponInventory::AddGrenade(GrenadeVariant variant, int count) {
    int index = static_cast<int>(variant);
    if (index >= 0 && index < static_cast<int>(m_grenades.size())) {
        m_grenades[index] = std::min(m_grenades[index] + count, kMaxGrenades);
    }
}

bool WeaponInventory::UseGrenade(GrenadeVariant variant) {
    int index = static_cast<int>(variant);
    if (index >= 0 && index < static_cast<int>(m_grenades.size()) && m_grenades[index] > 0) {
        m_grenades[index]--;
        return true;
    }
    return false;
}

void WeaponInventory::AddClaymores(int count) {
    m_claymores = std::min(m_claymores + count, kMaxClaymores);
}

bool WeaponInventory::UseClaymore() {
    if (m_claymores > 0) {
        m_claymores--;
        return true;
    }
    return false;
}

} // namespace Vehement
