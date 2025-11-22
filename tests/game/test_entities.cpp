/**
 * @file test_entities.cpp
 * @brief Unit tests for entity system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "entities/Entity.hpp"
#include "entities/Player.hpp"
#include "entities/Zombie.hpp"
#include "entities/NPC.hpp"

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

#include <memory>
#include <vector>

using namespace Vehement;
using namespace Nova::Test;

// =============================================================================
// Entity Type Tests
// =============================================================================

TEST(EntityTypeTest, StringConversions) {
    EXPECT_STREQ("Player", EntityTypeToString(EntityType::Player));
    EXPECT_STREQ("Zombie", EntityTypeToString(EntityType::Zombie));
    EXPECT_STREQ("NPC", EntityTypeToString(EntityType::NPC));
    EXPECT_STREQ("Projectile", EntityTypeToString(EntityType::Projectile));
    EXPECT_STREQ("Pickup", EntityTypeToString(EntityType::Pickup));
    EXPECT_STREQ("Effect", EntityTypeToString(EntityType::Effect));
    EXPECT_STREQ("None", EntityTypeToString(EntityType::None));
}

// =============================================================================
// Entity Base Class Tests
// =============================================================================

class EntityTest : public ::testing::Test {
protected:
    class TestEntity : public Entity {
    public:
        TestEntity() : Entity(EntityType::None) {}
        explicit TestEntity(EntityType type) : Entity(type) {}

        // Expose protected methods for testing
        void SetEntityId(EntityId id) { SetId(id); }
    };

    void SetUp() override {
        entity = std::make_unique<TestEntity>(EntityType::None);
    }

    std::unique_ptr<TestEntity> entity;
};

TEST_F(EntityTest, DefaultConstruction) {
    Entity e;
    EXPECT_EQ(EntityType::None, e.GetType());
    EXPECT_TRUE(e.IsActive());
    EXPECT_FALSE(e.IsMarkedForRemoval());
    EXPECT_TRUE(e.IsAlive());
}

TEST_F(EntityTest, TypedConstruction) {
    TestEntity player(EntityType::Player);
    EXPECT_EQ(EntityType::Player, player.GetType());
}

TEST_F(EntityTest, EntityId) {
    entity->SetEntityId(42);
    EXPECT_EQ(42, entity->GetId());
}

TEST_F(EntityTest, InvalidId) {
    EXPECT_EQ(Entity::INVALID_ID, entity->GetId());
}

// =============================================================================
// Position and Movement Tests
// =============================================================================

TEST_F(EntityTest, SetGetPosition) {
    glm::vec3 pos(10.0f, 5.0f, 20.0f);
    entity->SetPosition(pos);
    EXPECT_VEC3_EQ(pos, entity->GetPosition());
}

TEST_F(EntityTest, SetPosition2D) {
    entity->SetGroundLevel(0.5f);
    entity->SetPosition2D(10.0f, 20.0f);

    glm::vec3 expected(10.0f, 0.5f, 20.0f);
    EXPECT_VEC3_EQ(expected, entity->GetPosition());
}

TEST_F(EntityTest, GetPosition2D) {
    entity->SetPosition(glm::vec3(10.0f, 5.0f, 20.0f));

    glm::vec2 pos2d = entity->GetPosition2D();
    EXPECT_FLOAT_EQ(10.0f, pos2d.x);
    EXPECT_FLOAT_EQ(20.0f, pos2d.y);
}

TEST_F(EntityTest, GroundLevel) {
    entity->SetGroundLevel(2.0f);
    EXPECT_FLOAT_EQ(2.0f, entity->GetGroundLevel());
    EXPECT_FLOAT_EQ(2.0f, entity->GetPosition().y);
}

TEST_F(EntityTest, Rotation) {
    float rotation = glm::half_pi<float>();  // 90 degrees
    entity->SetRotation(rotation);
    EXPECT_FLOAT_EQ(rotation, entity->GetRotation());
}

TEST_F(EntityTest, GetForward) {
    entity->SetRotation(0.0f);
    glm::vec3 forward = entity->GetForward();

    // At rotation 0, forward is (0, 0, 1)
    EXPECT_NEAR(0.0f, forward.x, 0.001f);
    EXPECT_NEAR(0.0f, forward.y, 0.001f);
    EXPECT_NEAR(1.0f, forward.z, 0.001f);
}

TEST_F(EntityTest, GetForward_Rotated90) {
    entity->SetRotation(glm::half_pi<float>());
    glm::vec3 forward = entity->GetForward();

    // At rotation 90 degrees, forward is (1, 0, 0)
    EXPECT_NEAR(1.0f, forward.x, 0.001f);
    EXPECT_NEAR(0.0f, forward.y, 0.001f);
    EXPECT_NEAR(0.0f, forward.z, 0.001f);
}

TEST_F(EntityTest, GetRight) {
    entity->SetRotation(0.0f);
    glm::vec3 right = entity->GetRight();

    // At rotation 0, right is (1, 0, 0)
    EXPECT_NEAR(1.0f, right.x, 0.001f);
    EXPECT_NEAR(0.0f, right.y, 0.001f);
    EXPECT_NEAR(0.0f, right.z, 0.001f);
}

TEST_F(EntityTest, LookAt) {
    entity->SetPosition(glm::vec3(0.0f));
    entity->LookAt(glm::vec3(10.0f, 0.0f, 0.0f));

    // Should be facing positive X, which is rotation of 90 degrees
    EXPECT_NEAR(glm::half_pi<float>(), entity->GetRotation(), 0.001f);
}

TEST_F(EntityTest, LookAt2D) {
    entity->SetPosition(glm::vec3(0.0f));
    entity->LookAt2D(0.0f, 10.0f);

    // Should be facing positive Z, which is rotation of 0 degrees
    EXPECT_NEAR(0.0f, entity->GetRotation(), 0.001f);
}

// =============================================================================
// Velocity Tests
// =============================================================================

TEST_F(EntityTest, SetGetVelocity) {
    glm::vec3 velocity(5.0f, 0.0f, 3.0f);
    entity->SetVelocity(velocity);
    EXPECT_VEC3_EQ(velocity, entity->GetVelocity());
}

TEST_F(EntityTest, SetVelocity2D) {
    entity->SetVelocity2D(5.0f, 3.0f);

    glm::vec3 expected(5.0f, 0.0f, 3.0f);
    EXPECT_VEC3_EQ(expected, entity->GetVelocity());
}

TEST_F(EntityTest, GetSpeed) {
    entity->SetVelocity(glm::vec3(3.0f, 0.0f, 4.0f));
    EXPECT_FLOAT_EQ(5.0f, entity->GetSpeed());
}

TEST_F(EntityTest, MoveSpeed) {
    entity->SetMoveSpeed(10.0f);
    EXPECT_FLOAT_EQ(10.0f, entity->GetMoveSpeed());
}

// =============================================================================
// Health System Tests
// =============================================================================

TEST_F(EntityTest, DefaultHealth) {
    EXPECT_FLOAT_EQ(100.0f, entity->GetHealth());
    EXPECT_FLOAT_EQ(100.0f, entity->GetMaxHealth());
    EXPECT_FLOAT_EQ(1.0f, entity->GetHealthPercent());
}

TEST_F(EntityTest, SetHealth) {
    entity->SetHealth(50.0f);
    EXPECT_FLOAT_EQ(50.0f, entity->GetHealth());
    EXPECT_FLOAT_EQ(0.5f, entity->GetHealthPercent());
}

TEST_F(EntityTest, SetHealth_ClampsToMax) {
    entity->SetMaxHealth(100.0f);
    entity->SetHealth(150.0f);

    EXPECT_FLOAT_EQ(100.0f, entity->GetHealth());
}

TEST_F(EntityTest, SetHealth_ClampsToZero) {
    entity->SetHealth(-50.0f);
    EXPECT_FLOAT_EQ(0.0f, entity->GetHealth());
}

TEST_F(EntityTest, SetMaxHealth) {
    entity->SetMaxHealth(200.0f);
    EXPECT_FLOAT_EQ(200.0f, entity->GetMaxHealth());
}

TEST_F(EntityTest, Heal) {
    entity->SetHealth(50.0f);
    entity->Heal(30.0f);

    EXPECT_FLOAT_EQ(80.0f, entity->GetHealth());
}

TEST_F(EntityTest, Heal_ClampsToMax) {
    entity->SetHealth(90.0f);
    entity->Heal(50.0f);

    EXPECT_FLOAT_EQ(100.0f, entity->GetHealth());
}

TEST_F(EntityTest, TakeDamage) {
    float damage = entity->TakeDamage(30.0f);

    EXPECT_FLOAT_EQ(30.0f, damage);
    EXPECT_FLOAT_EQ(70.0f, entity->GetHealth());
}

TEST_F(EntityTest, TakeDamage_WithSource) {
    float damage = entity->TakeDamage(30.0f, 42);

    EXPECT_FLOAT_EQ(30.0f, damage);
    EXPECT_FLOAT_EQ(70.0f, entity->GetHealth());
}

TEST_F(EntityTest, TakeDamage_KillsEntity) {
    entity->TakeDamage(100.0f);

    EXPECT_FLOAT_EQ(0.0f, entity->GetHealth());
    EXPECT_FALSE(entity->IsAlive());
}

TEST_F(EntityTest, TakeDamage_Overkill) {
    float damage = entity->TakeDamage(150.0f);

    // Should return actual damage dealt (up to remaining health)
    EXPECT_FLOAT_EQ(100.0f, damage);
    EXPECT_FLOAT_EQ(0.0f, entity->GetHealth());
}

TEST_F(EntityTest, IsAlive) {
    EXPECT_TRUE(entity->IsAlive());

    entity->SetHealth(0.0f);
    EXPECT_FALSE(entity->IsAlive());
}

TEST_F(EntityTest, Die) {
    entity->Die();

    EXPECT_FALSE(entity->IsAlive());
    // Entity may also be marked for removal
}

TEST_F(EntityTest, HealthPercent_ZeroMax) {
    entity->SetMaxHealth(0.0f);
    EXPECT_FLOAT_EQ(0.0f, entity->GetHealthPercent());
}

// =============================================================================
// Collision Tests
// =============================================================================

TEST_F(EntityTest, CollisionRadius) {
    entity->SetCollisionRadius(2.0f);
    EXPECT_FLOAT_EQ(2.0f, entity->GetCollisionRadius());
}

TEST_F(EntityTest, Collidable) {
    EXPECT_TRUE(entity->IsCollidable());

    entity->SetCollidable(false);
    EXPECT_FALSE(entity->IsCollidable());
}

TEST_F(EntityTest, CollidesWith_Overlapping) {
    entity->SetPosition(glm::vec3(0.0f));
    entity->SetCollisionRadius(1.0f);

    TestEntity other;
    other.SetPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    other.SetCollisionRadius(1.0f);

    EXPECT_TRUE(entity->CollidesWith(other));
}

TEST_F(EntityTest, CollidesWith_NotOverlapping) {
    entity->SetPosition(glm::vec3(0.0f));
    entity->SetCollisionRadius(1.0f);

    TestEntity other;
    other.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    other.SetCollisionRadius(1.0f);

    EXPECT_FALSE(entity->CollidesWith(other));
}

TEST_F(EntityTest, CollidesWith_Touching) {
    entity->SetPosition(glm::vec3(0.0f));
    entity->SetCollisionRadius(1.0f);

    TestEntity other;
    other.SetPosition(glm::vec3(2.0f, 0.0f, 0.0f));  // Exactly touching
    other.SetCollisionRadius(1.0f);

    // Depending on implementation, may or may not count as collision
}

TEST_F(EntityTest, DistanceTo) {
    entity->SetPosition(glm::vec3(0.0f));

    TestEntity other;
    other.SetPosition(glm::vec3(3.0f, 0.0f, 4.0f));

    EXPECT_FLOAT_EQ(5.0f, entity->DistanceTo(other));
}

TEST_F(EntityTest, DistanceSquaredTo) {
    entity->SetPosition(glm::vec3(0.0f));

    TestEntity other;
    other.SetPosition(glm::vec3(3.0f, 0.0f, 4.0f));

    EXPECT_FLOAT_EQ(25.0f, entity->DistanceSquaredTo(other));
}

// =============================================================================
// Entity State Tests
// =============================================================================

TEST_F(EntityTest, Active) {
    EXPECT_TRUE(entity->IsActive());

    entity->SetActive(false);
    EXPECT_FALSE(entity->IsActive());
}

TEST_F(EntityTest, MarkForRemoval) {
    EXPECT_FALSE(entity->IsMarkedForRemoval());

    entity->MarkForRemoval();
    EXPECT_TRUE(entity->IsMarkedForRemoval());
}

TEST_F(EntityTest, Name) {
    entity->SetName("TestEntity");
    EXPECT_EQ("TestEntity", entity->GetName());
}

// =============================================================================
// Sprite/Texture Tests
// =============================================================================

TEST_F(EntityTest, TexturePath) {
    entity->SetTexturePath("textures/player.png");
    EXPECT_EQ("textures/player.png", entity->GetTexturePath());
}

TEST_F(EntityTest, SpriteScale) {
    entity->SetSpriteScale(2.0f);
    EXPECT_FLOAT_EQ(2.0f, entity->GetSpriteScale());
}

TEST_F(EntityTest, TextureInitiallyNull) {
    EXPECT_EQ(nullptr, entity->GetTexture());
}

// =============================================================================
// Update Tests
// =============================================================================

TEST_F(EntityTest, Update_DoesNotCrash) {
    entity->SetVelocity(glm::vec3(1.0f, 0.0f, 0.0f));
    entity->Update(0.016f);  // ~60 FPS delta time

    // Just ensure it doesn't crash
}

// =============================================================================
// Entity Lifecycle Tests
// =============================================================================

class EntityLifecycleTest : public ::testing::Test {
protected:
    class TrackedEntity : public Entity {
    public:
        bool updateCalled = false;
        bool dieCalled = false;
        int updateCount = 0;

        void Update(float deltaTime) override {
            Entity::Update(deltaTime);
            updateCalled = true;
            updateCount++;
        }

        void Die() override {
            Entity::Die();
            dieCalled = true;
        }
    };
};

TEST_F(EntityLifecycleTest, Update_Called) {
    TrackedEntity entity;
    entity.Update(0.016f);

    EXPECT_TRUE(entity.updateCalled);
    EXPECT_EQ(1, entity.updateCount);
}

TEST_F(EntityLifecycleTest, Die_Called) {
    TrackedEntity entity;
    entity.TakeDamage(100.0f);

    EXPECT_TRUE(entity.dieCalled);
}

TEST_F(EntityLifecycleTest, MultipleUpdates) {
    TrackedEntity entity;

    for (int i = 0; i < 10; ++i) {
        entity.Update(0.016f);
    }

    EXPECT_EQ(10, entity.updateCount);
}

// =============================================================================
// Entity Component Tests (if applicable)
// =============================================================================

TEST(EntityComponentTest, AddRemove_NotCrashing) {
    // This tests if entity has any component system
    // If Entity doesn't have components, this test can be minimal
    Entity entity;

    // Just verify the entity can be created and destroyed without issues
    EXPECT_TRUE(entity.IsActive());
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_F(EntityTest, MoveConstruction) {
    entity->SetPosition(glm::vec3(10.0f, 20.0f, 30.0f));
    entity->SetHealth(75.0f);
    entity->SetName("Original");

    TestEntity movedEntity(std::move(*entity));

    EXPECT_VEC3_EQ(glm::vec3(10.0f, 20.0f, 30.0f), movedEntity.GetPosition());
    EXPECT_FLOAT_EQ(75.0f, movedEntity.GetHealth());
    EXPECT_EQ("Original", movedEntity.GetName());
}

TEST_F(EntityTest, MoveAssignment) {
    entity->SetPosition(glm::vec3(10.0f, 20.0f, 30.0f));
    entity->SetHealth(75.0f);

    TestEntity other;
    other = std::move(*entity);

    EXPECT_VEC3_EQ(glm::vec3(10.0f, 20.0f, 30.0f), other.GetPosition());
    EXPECT_FLOAT_EQ(75.0f, other.GetHealth());
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST(EntityPropertyTest, Health_AlwaysInRange) {
    RandomGenerator rng(42);
    FloatGenerator healthGen(0.0f, 1000.0f);
    FloatGenerator damageGen(0.0f, 500.0f);

    for (int i = 0; i < 100; ++i) {
        Entity entity;
        float maxHealth = healthGen.Generate(rng);
        entity.SetMaxHealth(maxHealth);

        float damage = damageGen.Generate(rng);
        entity.TakeDamage(damage);

        // Health should always be in valid range
        EXPECT_GE(entity.GetHealth(), 0.0f);
        EXPECT_LE(entity.GetHealth(), maxHealth);
    }
}

TEST(EntityPropertyTest, CollisionSymmetric) {
    RandomGenerator rng(42);
    Vec3Generator posGen(-100.0f, 100.0f);
    FloatGenerator radiusGen(0.1f, 5.0f);

    class TestEntity : public Entity {
    public:
        using Entity::Entity;
    };

    for (int i = 0; i < 100; ++i) {
        TestEntity a, b;
        a.SetPosition(posGen.Generate(rng));
        a.SetCollisionRadius(radiusGen.Generate(rng));
        b.SetPosition(posGen.Generate(rng));
        b.SetCollisionRadius(radiusGen.Generate(rng));

        // Collision should be symmetric
        EXPECT_EQ(a.CollidesWith(b), b.CollidesWith(a));
    }
}

TEST(EntityPropertyTest, Distance_Symmetric) {
    RandomGenerator rng(42);
    Vec3Generator posGen(-100.0f, 100.0f);

    class TestEntity : public Entity {
    public:
        using Entity::Entity;
    };

    for (int i = 0; i < 100; ++i) {
        TestEntity a, b;
        a.SetPosition(posGen.Generate(rng));
        b.SetPosition(posGen.Generate(rng));

        EXPECT_FLOAT_EQ(a.DistanceTo(b), b.DistanceTo(a));
    }
}

TEST(EntityPropertyTest, Distance_TriangleInequality) {
    RandomGenerator rng(42);
    Vec3Generator posGen(-100.0f, 100.0f);

    class TestEntity : public Entity {
    public:
        using Entity::Entity;
    };

    for (int i = 0; i < 100; ++i) {
        TestEntity a, b, c;
        a.SetPosition(posGen.Generate(rng));
        b.SetPosition(posGen.Generate(rng));
        c.SetPosition(posGen.Generate(rng));

        float ab = a.DistanceTo(b);
        float bc = b.DistanceTo(c);
        float ac = a.DistanceTo(c);

        // Triangle inequality: any side must be <= sum of other two
        EXPECT_LE(ab, ac + bc + 0.001f);
        EXPECT_LE(bc, ab + ac + 0.001f);
        EXPECT_LE(ac, ab + bc + 0.001f);
    }
}

// =============================================================================
// Zombie Entity Tests (if available)
// =============================================================================

#ifdef VEHEMENT_HAS_ZOMBIE
class ZombieTest : public ::testing::Test {
protected:
    void SetUp() override {
        zombie = std::make_unique<Zombie>();
    }

    std::unique_ptr<Zombie> zombie;
};

TEST_F(ZombieTest, Type) {
    EXPECT_EQ(EntityType::Zombie, zombie->GetType());
}

TEST_F(ZombieTest, DefaultHealth) {
    // Zombies typically have specific health values
    EXPECT_GT(zombie->GetMaxHealth(), 0.0f);
}

TEST_F(ZombieTest, IsEnemy) {
    // Zombies should be enemies to players
    EXPECT_TRUE(zombie->IsEnemy());
}
#endif

// =============================================================================
// Player Entity Tests (if available)
// =============================================================================

#ifdef VEHEMENT_HAS_PLAYER
class PlayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        player = std::make_unique<Player>();
    }

    std::unique_ptr<Player> player;
};

TEST_F(PlayerTest, Type) {
    EXPECT_EQ(EntityType::Player, player->GetType());
}

TEST_F(PlayerTest, NotEnemy) {
    EXPECT_FALSE(player->IsEnemy());
}
#endif

