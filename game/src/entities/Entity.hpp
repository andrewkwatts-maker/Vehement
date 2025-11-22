#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <string>
#include <memory>
#include <cstdint>

namespace Nova {
    class Texture;
    class Renderer;
}

namespace Vehement {

/**
 * @brief Entity type enumeration for identification and filtering
 */
enum class EntityType : uint8_t {
    None = 0,
    Player,
    Zombie,
    NPC,
    Projectile,
    Pickup,
    Effect
};

/**
 * @brief Convert entity type to string for debugging
 */
inline const char* EntityTypeToString(EntityType type) {
    switch (type) {
        case EntityType::Player:     return "Player";
        case EntityType::Zombie:     return "Zombie";
        case EntityType::NPC:        return "NPC";
        case EntityType::Projectile: return "Projectile";
        case EntityType::Pickup:     return "Pickup";
        case EntityType::Effect:     return "Effect";
        default:                     return "None";
    }
}

/**
 * @brief Base entity class for all game objects in Vehement2
 *
 * Provides common functionality for position, rotation, health, collision,
 * and rendering. Designed for a top-down zombie survival game where rotation
 * is primarily around the Y axis.
 */
class Entity {
public:
    using EntityId = uint32_t;
    static constexpr EntityId INVALID_ID = 0;

    Entity();
    explicit Entity(EntityType type);
    virtual ~Entity() = default;

    // Prevent copying, allow moving
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) noexcept = default;
    Entity& operator=(Entity&&) noexcept = default;

    // =========================================================================
    // Core Update/Render Interface
    // =========================================================================

    /**
     * @brief Update entity logic
     * @param deltaTime Time since last frame in seconds
     */
    virtual void Update(float deltaTime);

    /**
     * @brief Render the entity
     * @param renderer The renderer to use for drawing
     */
    virtual void Render(Nova::Renderer& renderer);

    // =========================================================================
    // Position and Movement
    // =========================================================================

    /** @brief Get world position (XZ plane for top-down) */
    [[nodiscard]] const glm::vec3& GetPosition() const noexcept { return m_position; }

    /** @brief Set world position */
    void SetPosition(const glm::vec3& position) noexcept { m_position = position; }

    /** @brief Set position using 2D coordinates (Y is set to ground level) */
    void SetPosition2D(float x, float z) noexcept { m_position = glm::vec3(x, m_groundLevel, z); }

    /** @brief Get 2D position (XZ plane) */
    [[nodiscard]] glm::vec2 GetPosition2D() const noexcept { return glm::vec2(m_position.x, m_position.z); }

    /** @brief Get/Set ground level for 2D positioning */
    [[nodiscard]] float GetGroundLevel() const noexcept { return m_groundLevel; }
    void SetGroundLevel(float level) noexcept { m_groundLevel = level; m_position.y = level; }

    /** @brief Get rotation around Y axis in radians (facing direction in top-down) */
    [[nodiscard]] float GetRotation() const noexcept { return m_rotation; }

    /** @brief Set rotation around Y axis in radians */
    void SetRotation(float radians) noexcept { m_rotation = radians; }

    /** @brief Get forward direction vector (in XZ plane) */
    [[nodiscard]] glm::vec3 GetForward() const noexcept {
        return glm::vec3(std::sin(m_rotation), 0.0f, std::cos(m_rotation));
    }

    /** @brief Get right direction vector (in XZ plane) */
    [[nodiscard]] glm::vec3 GetRight() const noexcept {
        return glm::vec3(std::cos(m_rotation), 0.0f, -std::sin(m_rotation));
    }

    /** @brief Rotate to face a target position */
    void LookAt(const glm::vec3& target);

    /** @brief Rotate to face a 2D position */
    void LookAt2D(float x, float z);

    // =========================================================================
    // Velocity
    // =========================================================================

    /** @brief Get current velocity */
    [[nodiscard]] const glm::vec3& GetVelocity() const noexcept { return m_velocity; }

    /** @brief Set velocity */
    void SetVelocity(const glm::vec3& velocity) noexcept { m_velocity = velocity; }

    /** @brief Set velocity using 2D coordinates */
    void SetVelocity2D(float vx, float vz) noexcept { m_velocity = glm::vec3(vx, 0.0f, vz); }

    /** @brief Get speed (magnitude of velocity) */
    [[nodiscard]] float GetSpeed() const noexcept { return glm::length(m_velocity); }

    /** @brief Get movement speed multiplier */
    [[nodiscard]] float GetMoveSpeed() const noexcept { return m_moveSpeed; }

    /** @brief Set movement speed multiplier */
    void SetMoveSpeed(float speed) noexcept { m_moveSpeed = speed; }

    // =========================================================================
    // Health System
    // =========================================================================

    /** @brief Get current health */
    [[nodiscard]] float GetHealth() const noexcept { return m_health; }

    /** @brief Get maximum health */
    [[nodiscard]] float GetMaxHealth() const noexcept { return m_maxHealth; }

    /** @brief Get health as a percentage [0, 1] */
    [[nodiscard]] float GetHealthPercent() const noexcept {
        return m_maxHealth > 0.0f ? m_health / m_maxHealth : 0.0f;
    }

    /** @brief Set current health (clamped to [0, maxHealth]) */
    void SetHealth(float health) noexcept;

    /** @brief Set maximum health */
    void SetMaxHealth(float maxHealth) noexcept;

    /** @brief Heal the entity by amount (clamped to maxHealth) */
    void Heal(float amount) noexcept;

    /**
     * @brief Apply damage to the entity
     * @param amount Damage to apply
     * @param source Optional source entity ID
     * @return Actual damage dealt
     */
    virtual float TakeDamage(float amount, EntityId source = INVALID_ID);

    /** @brief Check if entity is alive (health > 0) */
    [[nodiscard]] bool IsAlive() const noexcept { return m_health > 0.0f; }

    /** @brief Called when entity health reaches zero */
    virtual void Die();

    // =========================================================================
    // Collision
    // =========================================================================

    /** @brief Get collision radius */
    [[nodiscard]] float GetCollisionRadius() const noexcept { return m_collisionRadius; }

    /** @brief Set collision radius */
    void SetCollisionRadius(float radius) noexcept { m_collisionRadius = radius; }

    /** @brief Check if collision is enabled */
    [[nodiscard]] bool IsCollidable() const noexcept { return m_collidable; }

    /** @brief Enable/disable collision */
    void SetCollidable(bool collidable) noexcept { m_collidable = collidable; }

    /**
     * @brief Check if this entity collides with another
     * @param other The other entity to check
     * @return true if collision detected
     */
    [[nodiscard]] bool CollidesWith(const Entity& other) const;

    /**
     * @brief Get distance to another entity
     */
    [[nodiscard]] float DistanceTo(const Entity& other) const;

    /**
     * @brief Get squared distance to another entity (faster, no sqrt)
     */
    [[nodiscard]] float DistanceSquaredTo(const Entity& other) const;

    // =========================================================================
    // Entity Identity
    // =========================================================================

    /** @brief Get unique entity ID */
    [[nodiscard]] EntityId GetId() const noexcept { return m_id; }

    /** @brief Get entity type */
    [[nodiscard]] EntityType GetType() const noexcept { return m_type; }

    /** @brief Get entity name/tag */
    [[nodiscard]] const std::string& GetName() const noexcept { return m_name; }

    /** @brief Set entity name/tag */
    void SetName(const std::string& name) { m_name = name; }

    /** @brief Check if entity is marked for removal */
    [[nodiscard]] bool IsMarkedForRemoval() const noexcept { return m_markedForRemoval; }

    /** @brief Mark entity for removal (will be cleaned up by EntityManager) */
    void MarkForRemoval() noexcept { m_markedForRemoval = true; }

    /** @brief Check if entity is active */
    [[nodiscard]] bool IsActive() const noexcept { return m_active; }

    /** @brief Set entity active state */
    void SetActive(bool active) noexcept { m_active = active; }

    // =========================================================================
    // Sprite/Texture
    // =========================================================================

    /** @brief Get the entity's texture */
    [[nodiscard]] std::shared_ptr<Nova::Texture> GetTexture() const { return m_texture; }

    /** @brief Set the entity's texture */
    void SetTexture(std::shared_ptr<Nova::Texture> texture) { m_texture = std::move(texture); }

    /** @brief Get texture path */
    [[nodiscard]] const std::string& GetTexturePath() const noexcept { return m_texturePath; }

    /** @brief Set texture path (for loading later) */
    void SetTexturePath(const std::string& path) { m_texturePath = path; }

    /** @brief Get sprite scale */
    [[nodiscard]] float GetSpriteScale() const noexcept { return m_spriteScale; }

    /** @brief Set sprite scale */
    void SetSpriteScale(float scale) noexcept { m_spriteScale = scale; }

protected:
    // Allow derived classes to set ID (EntityManager sets this)
    void SetId(EntityId id) noexcept { m_id = id; }
    friend class EntityManager;

    // Position and movement
    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    float m_rotation = 0.0f;          // Rotation around Y axis (radians)
    float m_moveSpeed = 5.0f;         // Movement speed multiplier
    float m_groundLevel = 0.0f;       // Y position for ground

    // Health
    float m_health = 100.0f;
    float m_maxHealth = 100.0f;

    // Collision
    float m_collisionRadius = 0.5f;
    bool m_collidable = true;

    // Identity
    EntityId m_id = INVALID_ID;
    EntityType m_type = EntityType::None;
    std::string m_name;

    // State flags
    bool m_active = true;
    bool m_markedForRemoval = false;

    // Rendering
    std::shared_ptr<Nova::Texture> m_texture;
    std::string m_texturePath;
    float m_spriteScale = 1.0f;

private:
    static EntityId s_nextId;
};

} // namespace Vehement
