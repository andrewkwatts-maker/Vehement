#include "Player.hpp"
#include "EntityManager.hpp"
#include <game/src/combat/Weapon.hpp>
#include <engine/input/InputManager.hpp>
#include <engine/graphics/Renderer.hpp>
#include <engine/graphics/TextureManager.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

Player::Player() : Entity(EntityType::Player) {
    m_moveSpeed = DEFAULT_MOVE_SPEED;
    m_maxHealth = 100.0f;
    m_health = m_maxHealth;
    m_collisionRadius = 0.4f;
    m_name = "Player";

    // Initialize empty weapon slots
    for (auto& slot : m_weapons) {
        slot.Clear();
    }

    // Set default avatar texture path
    m_texturePath = GetAvatarTexturePath(m_avatarIndex);
}

void Player::Update(float deltaTime) {
    // Update survival time if alive
    if (IsAlive()) {
        m_stats.survivalTime += deltaTime;
    }

    // Update invulnerability timer
    if (m_invulnerabilityTimer > 0.0f) {
        m_invulnerabilityTimer -= deltaTime;
    }

    // Update reload timer
    if (m_reloading) {
        m_reloadTimer -= deltaTime;
        if (m_reloadTimer <= 0.0f) {
            m_reloading = false;
            // Complete reload - transfer ammo from reserve to magazine
            auto& weapon = m_weapons[m_currentWeaponSlot];
            if (!weapon.IsEmpty()) {
                // Get magazine size from weapon stats
                WeaponType weaponType = static_cast<WeaponType>(weapon.weaponId);
                WeaponStats stats = DefaultWeaponStats::GetStats(weaponType);
                int magazineSize = stats.magazineSize;
                int needed = magazineSize - weapon.ammo;
                int transfer = std::min(needed, weapon.reserveAmmo);
                weapon.ammo += transfer;
                weapon.reserveAmmo -= transfer;
            }
        }
    }

    // Apply velocity to position
    m_position += m_velocity * deltaTime;

    // Call base update
    Entity::Update(deltaTime);
}

void Player::Render(Nova::Renderer& renderer) {
    // Skip rendering if invulnerable and blinking
    if (m_invulnerabilityTimer > 0.0f) {
        // Blink effect - skip rendering on alternating frames
        int blinkPhase = static_cast<int>(m_invulnerabilityTimer * 10.0f);
        if (blinkPhase % 2 == 0) {
            return;
        }
    }

    Entity::Render(renderer);
}

void Player::ProcessInput(Nova::InputManager& input, float deltaTime) {
    HandleMovement(input, deltaTime);

    // Sprint with Shift
    m_sprinting = input.IsKeyDown(Nova::Key::LeftShift) || input.IsKeyDown(Nova::Key::RightShift);

    // Weapon switching with number keys
    if (input.IsKeyPressed(Nova::Key::Num1)) SwitchWeapon(0);
    if (input.IsKeyPressed(Nova::Key::Num2)) SwitchWeapon(1);
    if (input.IsKeyPressed(Nova::Key::Num3)) SwitchWeapon(2);
    if (input.IsKeyPressed(Nova::Key::Num4)) SwitchWeapon(3);

    // Mouse wheel weapon switching
    float scroll = input.GetScrollDelta();
    if (scroll > 0.0f) {
        NextWeapon();
    } else if (scroll < 0.0f) {
        PreviousWeapon();
    }

    // Reload with R
    if (input.IsKeyPressed(Nova::Key::R)) {
        Reload();
    }

    // Fire with left mouse button
    if (input.IsMouseButtonDown(Nova::MouseButton::Left)) {
        Fire();
    }

    // Interact with E
    // Note: Interaction is handled by the game loop calling TryInteract()
}

void Player::HandleMovement(Nova::InputManager& input, float deltaTime) {
    // Get WASD input
    m_inputDirection = glm::vec2(0.0f);

    if (input.IsKeyDown(Nova::Key::W)) m_inputDirection.y += 1.0f;
    if (input.IsKeyDown(Nova::Key::S)) m_inputDirection.y -= 1.0f;
    if (input.IsKeyDown(Nova::Key::A)) m_inputDirection.x -= 1.0f;
    if (input.IsKeyDown(Nova::Key::D)) m_inputDirection.x += 1.0f;

    // Normalize diagonal movement
    if (glm::length(m_inputDirection) > 0.0f) {
        m_inputDirection = glm::normalize(m_inputDirection);
    }

    // Calculate velocity
    float speed = GetEffectiveMoveSpeed();
    m_velocity.x = m_inputDirection.x * speed;
    m_velocity.z = m_inputDirection.y * speed;  // Y input maps to Z world axis
}

void Player::HandleAiming(const glm::vec2& mouseWorldPos) {
    // Calculate direction to mouse
    glm::vec2 playerPos = GetPosition2D();
    glm::vec2 toMouse = mouseWorldPos - playerPos;

    if (glm::length(toMouse) > 0.01f) {
        // Calculate angle to mouse (atan2 gives angle from positive X axis)
        // We want angle from positive Z axis (forward in our coordinate system)
        float angle = std::atan2(toMouse.x, toMouse.y);
        SetRotation(angle);
    }
}

const WeaponSlot& Player::GetWeaponSlot(int slot) const {
    static WeaponSlot emptySlot;
    if (slot >= 0 && slot < MAX_WEAPON_SLOTS) {
        return m_weapons[slot];
    }
    return emptySlot;
}

bool Player::SwitchWeapon(int slot) {
    if (slot >= 0 && slot < MAX_WEAPON_SLOTS && !m_weapons[slot].IsEmpty()) {
        if (slot != m_currentWeaponSlot) {
            m_currentWeaponSlot = slot;
            m_reloading = false;  // Cancel reload on weapon switch
            return true;
        }
    }
    return false;
}

void Player::NextWeapon() {
    int startSlot = m_currentWeaponSlot;
    do {
        m_currentWeaponSlot = (m_currentWeaponSlot + 1) % MAX_WEAPON_SLOTS;
        if (!m_weapons[m_currentWeaponSlot].IsEmpty()) {
            m_reloading = false;
            return;
        }
    } while (m_currentWeaponSlot != startSlot);
}

void Player::PreviousWeapon() {
    int startSlot = m_currentWeaponSlot;
    do {
        m_currentWeaponSlot = (m_currentWeaponSlot - 1 + MAX_WEAPON_SLOTS) % MAX_WEAPON_SLOTS;
        if (!m_weapons[m_currentWeaponSlot].IsEmpty()) {
            m_reloading = false;
            return;
        }
    } while (m_currentWeaponSlot != startSlot);
}

int Player::AddWeapon(int weaponId, int ammo, int reserveAmmo) {
    // Check if we already have this weapon
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        if (m_weapons[i].weaponId == weaponId) {
            // Add to reserve ammo
            m_weapons[i].reserveAmmo += reserveAmmo;
            return i;
        }
    }

    // Find empty slot
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        if (m_weapons[i].IsEmpty()) {
            m_weapons[i].weaponId = weaponId;
            m_weapons[i].ammo = ammo;
            m_weapons[i].reserveAmmo = reserveAmmo;
            return i;
        }
    }

    return -1;  // No empty slot
}

void Player::RemoveWeapon(int slot) {
    if (slot >= 0 && slot < MAX_WEAPON_SLOTS) {
        m_weapons[slot].Clear();

        // If we removed current weapon, switch to another
        if (slot == m_currentWeaponSlot) {
            NextWeapon();
        }
    }
}

int Player::AddAmmo(int amount) {
    auto& weapon = m_weapons[m_currentWeaponSlot];
    if (!weapon.IsEmpty()) {
        weapon.reserveAmmo += amount;
        return amount;
    }
    return 0;
}

bool Player::Fire() {
    if (m_reloading) return false;

    auto& weapon = m_weapons[m_currentWeaponSlot];
    if (weapon.IsEmpty() || weapon.ammo <= 0) {
        // Auto-reload if empty
        Reload();
        return false;
    }

    weapon.ammo--;
    m_stats.shotsFired++;

    // Note: Projectile creation, sound effects, and visual feedback are handled
    // by the CombatSystem when FireWeapon() is called by the game loop
    // after this method returns true

    return true;
}

bool Player::Reload() {
    if (m_reloading) return false;

    auto& weapon = m_weapons[m_currentWeaponSlot];
    if (weapon.IsEmpty() || weapon.reserveAmmo <= 0) {
        return false;
    }

    // Get reload time from weapon data
    WeaponType weaponType = static_cast<WeaponType>(weapon.weaponId);
    WeaponStats stats = DefaultWeaponStats::GetStats(weaponType);
    m_reloadTimer = stats.reloadTime;
    m_reloading = true;

    return true;
}

bool Player::SpendCoins(int amount) {
    if (m_coins >= amount) {
        m_coins -= amount;
        return true;
    }
    return false;
}

bool Player::TryInteract(EntityManager& entityManager) {
    if (m_interactionTarget == Entity::INVALID_ID) {
        return false;
    }

    // Get the interaction target entity
    Entity* target = entityManager.GetEntity(m_interactionTarget);
    if (!target || !target->IsActive()) {
        m_interactionTarget = Entity::INVALID_ID;
        return false;
    }

    // Check if still in interaction range
    float distance = DistanceTo(*target);
    if (distance > INTERACTION_RADIUS) {
        m_interactionTarget = Entity::INVALID_ID;
        return false;
    }

    // Handle interaction based on entity type
    switch (target->GetType()) {
        case EntityType::Pickup:
            // Pickups are automatically collected on collision
            // Interaction confirms collection if manual pickup is required
            target->MarkForRemoval();
            break;

        case EntityType::NPC:
            // Interact with NPC (could be shop, quest giver, etc.)
            // NPCs handle their own interaction logic
            break;

        default:
            // Unknown interactable - just clear the target
            break;
    }

    // Clear target after interaction
    m_interactionTarget = Entity::INVALID_ID;
    return true;
}

float Player::TakeDamage(float amount, EntityId source) {
    // Check invulnerability
    if (m_invulnerabilityTimer > 0.0f) {
        return 0.0f;
    }

    float actualDamage = Entity::TakeDamage(amount, source);

    if (actualDamage > 0.0f && IsAlive()) {
        // Grant brief invulnerability
        m_invulnerabilityTimer = INVULNERABILITY_DURATION;
    }

    return actualDamage;
}

void Player::Die() {
    Entity::Die();

    // Update stats
    m_stats.OnDeath();

    // Stop all movement
    m_velocity = glm::vec3(0.0f);
    m_reloading = false;
    m_sprinting = false;

    // Note: Death animation and sound effects are triggered by the game's
    // event system. The game loop monitors player death state and handles:
    // - Playing death animation/sound through the AudioSystem
    // - Showing death UI overlay
    // - Initiating respawn countdown
}

void Player::Respawn(const glm::vec3& position) {
    m_position = position;
    m_health = m_maxHealth;
    m_velocity = glm::vec3(0.0f);
    m_active = true;
    m_markedForRemoval = false;
    m_invulnerabilityTimer = INVULNERABILITY_DURATION * 2.0f;  // Longer invuln on respawn
    m_reloading = false;
}

void Player::SetAvatarIndex(int index) {
    // Clamp to valid range (1-9)
    m_avatarIndex = std::clamp(index, 1, 9);
    m_texturePath = GetAvatarTexturePath(m_avatarIndex);
}

std::string Player::GetAvatarTexturePath(int index) {
    // Texture path format: Vehement2/images/People/PersonN.png
    return "Vehement2/images/People/Person" + std::to_string(std::clamp(index, 1, 9)) + ".png";
}

} // namespace Vehement
