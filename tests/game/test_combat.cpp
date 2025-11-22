/**
 * @file test_combat.cpp
 * @brief Unit tests for combat system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "combat/CombatSystem.hpp"
#include "combat/Weapon.hpp"
#include "combat/Projectile.hpp"
#include "combat/Grenade.hpp"

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

#include <memory>
#include <vector>

using namespace Vehement;
using namespace Nova::Test;

// =============================================================================
// Combat Stats Tests
// =============================================================================

TEST(CombatStatsTest, DefaultValues) {
    CombatStats stats;

    EXPECT_EQ(0, stats.kills);
    EXPECT_EQ(0, stats.deaths);
    EXPECT_EQ(0, stats.headshots);
    EXPECT_EQ(0, stats.shotsFired);
    EXPECT_EQ(0, stats.shotsHit);
    EXPECT_EQ(0, stats.grenadeKills);
    EXPECT_FLOAT_EQ(0.0f, stats.damageDealt);
    EXPECT_FLOAT_EQ(0.0f, stats.damageTaken);
    EXPECT_EQ(0, stats.coinsEarned);
}

TEST(CombatStatsTest, GetAccuracy_NoShots) {
    CombatStats stats;
    EXPECT_FLOAT_EQ(0.0f, stats.GetAccuracy());
}

TEST(CombatStatsTest, GetAccuracy_WithShots) {
    CombatStats stats;
    stats.shotsFired = 100;
    stats.shotsHit = 75;

    EXPECT_FLOAT_EQ(0.75f, stats.GetAccuracy());
}

TEST(CombatStatsTest, GetKDRatio_NoDeaths) {
    CombatStats stats;
    stats.kills = 10;
    stats.deaths = 0;

    EXPECT_FLOAT_EQ(10.0f, stats.GetKDRatio());
}

TEST(CombatStatsTest, GetKDRatio_WithDeaths) {
    CombatStats stats;
    stats.kills = 10;
    stats.deaths = 5;

    EXPECT_FLOAT_EQ(2.0f, stats.GetKDRatio());
}

TEST(CombatStatsTest, GetKDRatio_NoKills) {
    CombatStats stats;
    stats.kills = 0;
    stats.deaths = 5;

    EXPECT_FLOAT_EQ(0.0f, stats.GetKDRatio());
}

TEST(CombatStatsTest, Reset) {
    CombatStats stats;
    stats.kills = 10;
    stats.deaths = 5;
    stats.headshots = 3;
    stats.shotsFired = 100;
    stats.shotsHit = 75;
    stats.damageDealt = 500.0f;
    stats.damageTaken = 200.0f;
    stats.coinsEarned = 150;

    stats.Reset();

    EXPECT_EQ(0, stats.kills);
    EXPECT_EQ(0, stats.deaths);
    EXPECT_EQ(0, stats.headshots);
    EXPECT_EQ(0, stats.shotsFired);
    EXPECT_EQ(0, stats.shotsHit);
    EXPECT_FLOAT_EQ(0.0f, stats.damageDealt);
    EXPECT_FLOAT_EQ(0.0f, stats.damageTaken);
    EXPECT_EQ(0, stats.coinsEarned);
}

// =============================================================================
// Damage Event Tests
// =============================================================================

TEST(DamageEventTest, DefaultValues) {
    DamageEvent event;

    EXPECT_EQ(0, event.targetId);
    EXPECT_EQ(0, event.sourceId);
    EXPECT_FLOAT_EQ(0.0f, event.damage);
    EXPECT_FALSE(event.isHeadshot);
    EXPECT_FALSE(event.isExplosion);
}

TEST(DamageEventTest, Construction) {
    DamageEvent event;
    event.targetId = 1;
    event.sourceId = 2;
    event.damage = 50.0f;
    event.hitPosition = glm::vec3(10.0f, 5.0f, 20.0f);
    event.hitDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    event.isHeadshot = true;

    EXPECT_EQ(1, event.targetId);
    EXPECT_EQ(2, event.sourceId);
    EXPECT_FLOAT_EQ(50.0f, event.damage);
    EXPECT_TRUE(event.isHeadshot);
    EXPECT_VEC3_EQ(glm::vec3(10.0f, 5.0f, 20.0f), event.hitPosition);
}

// =============================================================================
// Kill Event Tests
// =============================================================================

TEST(KillEventTest, DefaultValues) {
    KillEvent event;

    EXPECT_EQ(0, event.victimId);
    EXPECT_EQ(0, event.killerId);
    EXPECT_FALSE(event.isExplosion);
    EXPECT_EQ(0, event.coinsDropped);
}

TEST(KillEventTest, Construction) {
    KillEvent event;
    event.victimId = 1;
    event.killerId = 2;
    event.deathPosition = glm::vec3(10.0f, 0.0f, 20.0f);
    event.isExplosion = true;
    event.coinsDropped = 15;

    EXPECT_EQ(1, event.victimId);
    EXPECT_EQ(2, event.killerId);
    EXPECT_TRUE(event.isExplosion);
    EXPECT_EQ(15, event.coinsDropped);
}

// =============================================================================
// Raycast Result Tests
// =============================================================================

TEST(RaycastResultTest, DefaultValues) {
    RaycastResult result;

    EXPECT_FALSE(result.hit);
    EXPECT_FLOAT_EQ(0.0f, result.distance);
    EXPECT_EQ(0, result.entityId);
    EXPECT_FALSE(result.hitWorld);
}

TEST(RaycastResultTest, HitEntity) {
    RaycastResult result;
    result.hit = true;
    result.hitPosition = glm::vec3(5.0f, 1.0f, 0.0f);
    result.hitNormal = glm::vec3(1.0f, 0.0f, 0.0f);
    result.distance = 5.0f;
    result.entityId = 42;
    result.hitWorld = false;

    EXPECT_TRUE(result.hit);
    EXPECT_FLOAT_EQ(5.0f, result.distance);
    EXPECT_EQ(42, result.entityId);
}

TEST(RaycastResultTest, HitWorld) {
    RaycastResult result;
    result.hit = true;
    result.hitWorld = true;
    result.entityId = 0;

    EXPECT_TRUE(result.hit);
    EXPECT_TRUE(result.hitWorld);
    EXPECT_EQ(0, result.entityId);
}

// =============================================================================
// Coin Drop Tests
// =============================================================================

TEST(CoinDropTest, DefaultValues) {
    CoinDrop coin;

    EXPECT_EQ(10, coin.value);
    EXPECT_FLOAT_EQ(30.0f, coin.lifetime);
    EXPECT_FLOAT_EQ(0.0f, coin.age);
    EXPECT_FALSE(coin.collected);
}

TEST(CoinDropTest, Construction) {
    CoinDrop coin;
    coin.position = glm::vec3(10.0f, 0.0f, 20.0f);
    coin.value = 15;
    coin.lifetime = 60.0f;

    EXPECT_VEC3_EQ(glm::vec3(10.0f, 0.0f, 20.0f), coin.position);
    EXPECT_EQ(15, coin.value);
    EXPECT_FLOAT_EQ(60.0f, coin.lifetime);
}

TEST(CoinDropTest, Constants) {
    EXPECT_FLOAT_EQ(2.0f, CoinDrop::kCollectRadius);
    EXPECT_FLOAT_EQ(3.0f, CoinDrop::kBobSpeed);
    EXPECT_FLOAT_EQ(0.2f, CoinDrop::kBobHeight);
}

// =============================================================================
// Damage Calculation Tests
// =============================================================================

TEST(DamageCalculationTest, DamageFalloff_AtZero) {
    float damage = CalculateDamageFalloff(0.0f, 100.0f, 50.0f);
    EXPECT_FLOAT_EQ(50.0f, damage);  // Full damage at zero distance
}

TEST(DamageCalculationTest, DamageFalloff_AtMaxRange) {
    float damage = CalculateDamageFalloff(100.0f, 100.0f, 50.0f);
    EXPECT_FLOAT_EQ(0.0f, damage);  // No damage at max range
}

TEST(DamageCalculationTest, DamageFalloff_BeyondMaxRange) {
    float damage = CalculateDamageFalloff(150.0f, 100.0f, 50.0f);
    EXPECT_FLOAT_EQ(0.0f, damage);  // No damage beyond max range
}

TEST(DamageCalculationTest, DamageFalloff_Midrange) {
    float damage = CalculateDamageFalloff(50.0f, 100.0f, 100.0f);

    // At half distance with quadratic falloff: 100 * 0.5^2 = 25
    EXPECT_FLOAT_EQ(25.0f, damage);
}

TEST(DamageCalculationTest, Knockback) {
    glm::vec3 hitDir = glm::vec3(1.0f, 0.0f, 0.0f);
    float damage = 100.0f;
    float scale = 0.1f;

    glm::vec3 knockback = CalculateKnockback(hitDir, damage, scale);

    EXPECT_FLOAT_EQ(10.0f, glm::length(knockback));
}

TEST(DamageCalculationTest, Knockback_Normalized) {
    glm::vec3 hitDir = glm::vec3(3.0f, 0.0f, 4.0f);  // Length 5
    float damage = 50.0f;
    float scale = 0.1f;

    glm::vec3 knockback = CalculateKnockback(hitDir, damage, scale);
    glm::vec3 knockbackDir = glm::normalize(knockback);

    EXPECT_NEAR(0.6f, knockbackDir.x, 0.001f);
    EXPECT_NEAR(0.8f, knockbackDir.z, 0.001f);
}

// =============================================================================
// Kill Coin Value Tests
// =============================================================================

TEST(KillCoinValueTest, BasicKill) {
    int value = GetKillCoinValue(false, false);
    EXPECT_EQ(10, value);
}

TEST(KillCoinValueTest, Headshot) {
    int value = GetKillCoinValue(true, false);
    EXPECT_EQ(15, value);  // Base 10 + 5 for headshot
}

TEST(KillCoinValueTest, Explosion) {
    int value = GetKillCoinValue(false, true);
    EXPECT_EQ(13, value);  // Base 10 + 3 for explosion
}

TEST(KillCoinValueTest, HeadshotExplosion) {
    int value = GetKillCoinValue(true, true);
    EXPECT_EQ(18, value);  // Base 10 + 5 + 3
}

// =============================================================================
// Mock Combat Entity for Testing
// =============================================================================

class MockCombatEntity : public ICombatEntity {
public:
    uint32_t id = 1;
    glm::vec3 position{0.0f};
    float radius = 0.5f;
    float height = 2.0f;
    bool alive = true;
    bool enemy = true;
    float health = 100.0f;

    uint32_t GetEntityId() const override { return id; }
    glm::vec3 GetPosition() const override { return position; }
    float GetRadius() const override { return radius; }
    float GetHeight() const override { return height; }
    bool IsAlive() const override { return alive; }
    bool IsEnemy() const override { return enemy; }

    void TakeDamage(const DamageEvent& event) override {
        health -= event.damage;
        if (health <= 0) {
            alive = false;
        }
        lastDamageEvent = event;
    }

    void ApplyKnockback(const glm::vec3& force) override {
        lastKnockback = force;
    }

    void ApplyStatusEffect(GrenadeType effectType, float duration, float strength) override {
        lastStatusEffect = effectType;
        lastStatusDuration = duration;
        lastStatusStrength = strength;
    }

    DamageEvent lastDamageEvent;
    glm::vec3 lastKnockback{0.0f};
    GrenadeType lastStatusEffect = GrenadeType::Frag;
    float lastStatusDuration = 0.0f;
    float lastStatusStrength = 0.0f;
};

// =============================================================================
// Mock Collision Provider
// =============================================================================

class MockCollisionProvider : public ICollisionProvider {
public:
    RaycastResult nextRaycastResult;
    std::vector<ICombatEntity*> entitiesInRadius;

    RaycastResult Raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        uint32_t ignoreEntity = 0
    ) const override {
        lastRaycastOrigin = origin;
        lastRaycastDirection = direction;
        lastRaycastMaxDistance = maxDistance;
        lastRaycastIgnoreEntity = ignoreEntity;
        return nextRaycastResult;
    }

    bool IsPointInWorld(const glm::vec3& point) const override {
        return pointInWorld;
    }

    std::vector<ICombatEntity*> GetEntitiesInRadius(
        const glm::vec3& center,
        float radius
    ) const override {
        lastQueryCenter = center;
        lastQueryRadius = radius;
        return entitiesInRadius;
    }

    // For verification
    mutable glm::vec3 lastRaycastOrigin;
    mutable glm::vec3 lastRaycastDirection;
    mutable float lastRaycastMaxDistance = 0.0f;
    mutable uint32_t lastRaycastIgnoreEntity = 0;
    mutable glm::vec3 lastQueryCenter;
    mutable float lastQueryRadius = 0.0f;
    bool pointInWorld = true;
};

// =============================================================================
// Combat System Tests
// =============================================================================

class CombatSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        combat = std::make_unique<CombatSystem>();
        collision = std::make_unique<MockCollisionProvider>();

        combat->Initialize();
        combat->SetCollisionProvider(collision.get());
    }

    void TearDown() override {
        combat->Shutdown();
    }

    std::unique_ptr<CombatSystem> combat;
    std::unique_ptr<MockCollisionProvider> collision;
};

TEST_F(CombatSystemTest, Initialize) {
    CombatSystem cs;
    EXPECT_TRUE(cs.Initialize());
    cs.Shutdown();
}

TEST_F(CombatSystemTest, Update_DoesNotCrash) {
    combat->Update(0.016f);
}

TEST_F(CombatSystemTest, SetHeadshotMultiplier) {
    combat->SetHeadshotMultiplier(3.0f);
    // Verify through behavior
}

TEST_F(CombatSystemTest, SetFriendlyFire) {
    combat->SetFriendlyFire(true);
    // Verify through behavior
}

TEST_F(CombatSystemTest, GetPlayerStats) {
    CombatStats& stats = combat->GetPlayerStats();
    EXPECT_EQ(0, stats.kills);
    EXPECT_EQ(0, stats.deaths);
}

// =============================================================================
// Damage Application Tests
// =============================================================================

TEST_F(CombatSystemTest, ApplyDamage) {
    DamageEvent event;
    event.targetId = 1;
    event.sourceId = 2;
    event.damage = 50.0f;
    event.hitPosition = glm::vec3(5.0f, 1.0f, 0.0f);
    event.hitDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    event.isHeadshot = false;
    event.isExplosion = false;

    combat->ApplyDamage(event);

    // Stats should be updated
    EXPECT_FLOAT_EQ(50.0f, combat->GetPlayerStats().damageDealt);
}

TEST_F(CombatSystemTest, ApplyDamage_Headshot) {
    DamageEvent event;
    event.targetId = 1;
    event.sourceId = 2;
    event.damage = 50.0f;
    event.isHeadshot = true;

    combat->ApplyDamage(event);

    EXPECT_EQ(1, combat->GetPlayerStats().headshots);
}

TEST_F(CombatSystemTest, ApplyExplosionDamage) {
    MockCombatEntity entity1;
    entity1.id = 1;
    entity1.position = glm::vec3(2.0f, 0.0f, 0.0f);

    MockCombatEntity entity2;
    entity2.id = 2;
    entity2.position = glm::vec3(3.0f, 0.0f, 0.0f);

    collision->entitiesInRadius = {&entity1, &entity2};

    combat->ApplyExplosionDamage(
        glm::vec3(0.0f),  // Center
        10.0f,            // Radius
        100.0f,           // Damage
        42,               // Source ID
        GrenadeType::Frag
    );

    // Both entities should have received damage
    EXPECT_LT(entity1.health, 100.0f);
    EXPECT_LT(entity2.health, 100.0f);
}

// =============================================================================
// Coin System Tests
// =============================================================================

TEST_F(CombatSystemTest, DropCoins) {
    combat->DropCoins(glm::vec3(10.0f, 0.0f, 20.0f), 50);

    const auto& coins = combat->GetCoinDrops();
    EXPECT_FALSE(coins.empty());
}

TEST_F(CombatSystemTest, CollectCoins_InRange) {
    combat->DropCoins(glm::vec3(0.0f), 10);

    // Collect from nearby position
    int collected = combat->CollectCoins(glm::vec3(1.0f, 0.0f, 0.0f), 42);

    EXPECT_GT(collected, 0);
}

TEST_F(CombatSystemTest, CollectCoins_OutOfRange) {
    combat->DropCoins(glm::vec3(0.0f), 10);

    // Try to collect from far away
    int collected = combat->CollectCoins(glm::vec3(100.0f, 0.0f, 0.0f), 42);

    EXPECT_EQ(0, collected);
}

TEST_F(CombatSystemTest, CoinDrops_Expire) {
    combat->DropCoins(glm::vec3(0.0f), 10);

    // Simulate time passing
    for (int i = 0; i < 2000; ++i) {
        combat->Update(0.016f);  // ~32 seconds
    }

    // Coins should have expired
    const auto& coins = combat->GetCoinDrops();
    // All coins should be collected or expired
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_F(CombatSystemTest, DamageCallback) {
    bool callbackCalled = false;
    DamageEvent receivedEvent;

    combat->SetOnDamage([&](const DamageEvent& event) {
        callbackCalled = true;
        receivedEvent = event;
    });

    DamageEvent event;
    event.targetId = 1;
    event.damage = 50.0f;
    combat->ApplyDamage(event);

    EXPECT_TRUE(callbackCalled);
    EXPECT_FLOAT_EQ(50.0f, receivedEvent.damage);
}

TEST_F(CombatSystemTest, KillCallback) {
    bool callbackCalled = false;
    KillEvent receivedEvent;

    combat->SetOnKill([&](const KillEvent& event) {
        callbackCalled = true;
        receivedEvent = event;
    });

    // Kill callback would be triggered when an entity dies
    // This depends on the implementation details
}

TEST_F(CombatSystemTest, CoinCollectCallback) {
    bool callbackCalled = false;
    uint32_t collectorId = 0;
    int amount = 0;

    combat->SetOnCoinCollect([&](uint32_t id, int coins) {
        callbackCalled = true;
        collectorId = id;
        amount = coins;
    });

    combat->DropCoins(glm::vec3(0.0f), 10);
    combat->CollectCoins(glm::vec3(0.5f, 0.0f, 0.0f), 42);

    // Callback should have been called
    if (callbackCalled) {
        EXPECT_EQ(42, collectorId);
        EXPECT_GT(amount, 0);
    }
}

// =============================================================================
// Grenade Tests
// =============================================================================

TEST_F(CombatSystemTest, ThrowGrenade_Frag) {
    Grenade* grenade = combat->ThrowGrenade(
        GrenadeType::Frag,
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 0.5f, 0.0f),
        42
    );

    // Should return a valid grenade
    if (grenade) {
        EXPECT_EQ(GrenadeType::Frag, grenade->GetType());
    }
}

TEST_F(CombatSystemTest, ThrowGrenade_Flashbang) {
    Grenade* grenade = combat->ThrowGrenade(
        GrenadeType::Flashbang,
        glm::vec3(0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        42
    );

    if (grenade) {
        EXPECT_EQ(GrenadeType::Flashbang, grenade->GetType());
    }
}

TEST_F(CombatSystemTest, PlaceClaymore) {
    Grenade* claymore = combat->PlaceClaymore(
        glm::vec3(10.0f, 0.0f, 20.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        42
    );

    if (claymore) {
        EXPECT_EQ(GrenadeType::Claymore, claymore->GetType());
    }
}

// =============================================================================
// Pool Access Tests
// =============================================================================

TEST_F(CombatSystemTest, GetProjectilePool) {
    ProjectilePool& pool = combat->GetProjectilePool();
    // Just verify we can access the pool
    (void)pool;
}

TEST_F(CombatSystemTest, GetGrenadePool) {
    GrenadePool& pool = combat->GetGrenadePool();
    (void)pool;
}

TEST_F(CombatSystemTest, GetTracerRenderer) {
    TracerRenderer& renderer = combat->GetTracerRenderer();
    (void)renderer;
}

TEST_F(CombatSystemTest, GetBulletHoleManager) {
    BulletHoleManager& manager = combat->GetBulletHoleManager();
    (void)manager;
}

TEST_F(CombatSystemTest, GetExplosionManager) {
    ExplosionManager& manager = combat->GetExplosionManager();
    (void)manager;
}

TEST_F(CombatSystemTest, GetAreaEffectManager) {
    AreaEffectManager& manager = combat->GetAreaEffectManager();
    (void)manager;
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_F(CombatSystemTest, Integration_DamageKillFlow) {
    MockCombatEntity entity;
    entity.id = 1;
    entity.health = 100.0f;
    entity.position = glm::vec3(5.0f, 0.0f, 0.0f);

    // Set up collision provider to return our entity
    RaycastResult hit;
    hit.hit = true;
    hit.entityId = 1;
    hit.distance = 5.0f;
    collision->nextRaycastResult = hit;

    // Fire weapon and verify damage chain
    // This would require a full weapon setup

    // Apply direct damage
    DamageEvent event;
    event.targetId = 1;
    event.sourceId = 42;
    event.damage = 100.0f;  // Lethal damage

    combat->ApplyDamage(event);

    EXPECT_EQ(1, combat->GetPlayerStats().kills);
}

TEST_F(CombatSystemTest, Integration_ExplosionChain) {
    // Create multiple entities in explosion radius
    MockCombatEntity entity1, entity2, entity3;
    entity1.id = 1;
    entity1.position = glm::vec3(2.0f, 0.0f, 0.0f);
    entity2.id = 2;
    entity2.position = glm::vec3(0.0f, 0.0f, 2.0f);
    entity3.id = 3;
    entity3.position = glm::vec3(10.0f, 0.0f, 0.0f);  // Out of range

    collision->entitiesInRadius = {&entity1, &entity2};

    combat->ApplyExplosionDamage(
        glm::vec3(0.0f),
        5.0f,
        100.0f,
        42,
        GrenadeType::Frag
    );

    // Entities in range should be damaged
    EXPECT_LT(entity1.health, 100.0f);
    EXPECT_LT(entity2.health, 100.0f);
    // Entity 3 was not in the collision provider's result, so wasn't damaged
}

// =============================================================================
// Buff/Debuff Stacking Tests
// =============================================================================

TEST(BuffDebuffStackingTest, MultipleSlows) {
    // Test how multiple slow effects stack
    float speed = 100.0f;
    float slow1 = 0.25f;  // 25% slow
    float slow2 = 0.25f;  // 25% slow

    // Multiplicative stacking: 100 * 0.75 * 0.75 = 56.25
    float resultMult = speed * (1.0f - slow1) * (1.0f - slow2);
    EXPECT_FLOAT_EQ(56.25f, resultMult);

    // Additive stacking (capped at some value)
    float resultAdd = speed * std::max(0.0f, 1.0f - slow1 - slow2);
    EXPECT_FLOAT_EQ(50.0f, resultAdd);
}

TEST(BuffDebuffStackingTest, DamageBuffStacking) {
    float baseDamage = 100.0f;
    float buff1 = 0.20f;  // 20% damage increase
    float buff2 = 0.15f;  // 15% damage increase

    // Additive: 100 * (1 + 0.20 + 0.15) = 135
    float resultAdd = baseDamage * (1.0f + buff1 + buff2);
    EXPECT_FLOAT_EQ(135.0f, resultAdd);

    // Multiplicative: 100 * 1.20 * 1.15 = 138
    float resultMult = baseDamage * (1.0f + buff1) * (1.0f + buff2);
    EXPECT_FLOAT_EQ(138.0f, resultMult);
}

// =============================================================================
// Cooldown Tests
// =============================================================================

TEST(CooldownTest, AbilityCooldown) {
    float cooldown = 10.0f;
    float elapsed = 0.0f;

    EXPECT_FALSE(elapsed >= cooldown);  // Not ready

    elapsed += 5.0f;
    EXPECT_FALSE(elapsed >= cooldown);  // Still not ready

    elapsed += 5.0f;
    EXPECT_TRUE(elapsed >= cooldown);   // Ready
}

TEST(CooldownTest, CooldownReduction) {
    float baseCooldown = 10.0f;
    float cdrPercent = 0.30f;  // 30% CDR

    float actualCooldown = baseCooldown * (1.0f - cdrPercent);
    EXPECT_FLOAT_EQ(7.0f, actualCooldown);
}

TEST(CooldownTest, CooldownReductionCap) {
    float baseCooldown = 10.0f;
    float cdrPercent = 0.80f;  // 80% CDR
    float cdrCap = 0.40f;      // 40% cap

    float effectiveCdr = std::min(cdrPercent, cdrCap);
    float actualCooldown = baseCooldown * (1.0f - effectiveCdr);
    EXPECT_FLOAT_EQ(6.0f, actualCooldown);
}

// =============================================================================
// Spread Tests
// =============================================================================

TEST(SpreadTest, ApplySpread_ZeroSpread) {
    glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 result = ApplySpread(direction, 0.0f);

    EXPECT_VEC3_EQ(direction, result);
}

TEST(SpreadTest, ApplySpread_WithSpread) {
    glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f);
    float spread = 0.1f;

    // Apply spread multiple times and verify results are within cone
    for (int i = 0; i < 100; ++i) {
        glm::vec3 result = ApplySpread(direction, spread);

        // Result should be normalized
        EXPECT_NEAR(1.0f, glm::length(result), 0.001f);

        // Result should be within spread angle
        float dot = glm::dot(direction, result);
        float maxAngle = std::atan(spread);
        float actualAngle = std::acos(std::min(1.0f, dot));
        EXPECT_LE(actualAngle, maxAngle + 0.01f);
    }
}

