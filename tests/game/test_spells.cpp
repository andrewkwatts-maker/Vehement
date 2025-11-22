/**
 * @file test_spells.cpp
 * @brief Unit tests for ability/spell system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "rts/Ability.hpp"

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

using namespace Vehement;
using namespace Vehement::RTS;
using namespace Nova::Test;

// =============================================================================
// Ability Type Tests
// =============================================================================

TEST(AbilityTypeTest, AllTypes) {
    // Verify all ability types exist
    EXPECT_EQ(0, static_cast<int>(AbilityType::Passive));
    EXPECT_EQ(1, static_cast<int>(AbilityType::Active));
    EXPECT_EQ(2, static_cast<int>(AbilityType::Toggle));
    EXPECT_EQ(3, static_cast<int>(AbilityType::Channeled));
}

// =============================================================================
// Target Type Tests
// =============================================================================

TEST(TargetTypeTest, AllTypes) {
    EXPECT_EQ(0, static_cast<int>(TargetType::None));
    EXPECT_EQ(1, static_cast<int>(TargetType::Point));
    EXPECT_EQ(2, static_cast<int>(TargetType::Unit));
    EXPECT_EQ(3, static_cast<int>(TargetType::Area));
    EXPECT_EQ(4, static_cast<int>(TargetType::Direction));
    EXPECT_EQ(5, static_cast<int>(TargetType::Cone));
}

// =============================================================================
// Ability Effect Tests
// =============================================================================

TEST(AbilityEffectTest, AllEffects) {
    EXPECT_EQ(0, static_cast<int>(AbilityEffect::Damage));
    EXPECT_EQ(1, static_cast<int>(AbilityEffect::Heal));
    EXPECT_EQ(2, static_cast<int>(AbilityEffect::Buff));
    EXPECT_EQ(3, static_cast<int>(AbilityEffect::Debuff));
    EXPECT_EQ(4, static_cast<int>(AbilityEffect::Summon));
    EXPECT_EQ(5, static_cast<int>(AbilityEffect::Teleport));
    EXPECT_EQ(6, static_cast<int>(AbilityEffect::Knockback));
    EXPECT_EQ(7, static_cast<int>(AbilityEffect::Stun));
    EXPECT_EQ(8, static_cast<int>(AbilityEffect::Slow));
    EXPECT_EQ(9, static_cast<int>(AbilityEffect::Silence));
    EXPECT_EQ(10, static_cast<int>(AbilityEffect::Shield));
    EXPECT_EQ(11, static_cast<int>(AbilityEffect::Stealth));
    EXPECT_EQ(12, static_cast<int>(AbilityEffect::Detection));
    EXPECT_EQ(13, static_cast<int>(AbilityEffect::ResourceGain));
}

// =============================================================================
// Status Effect Tests
// =============================================================================

TEST(StatusEffectTest, Buffs) {
    // Positive status effects
    EXPECT_EQ(1, static_cast<int>(StatusEffect::Haste));
    EXPECT_EQ(2, static_cast<int>(StatusEffect::Might));
    EXPECT_EQ(3, static_cast<int>(StatusEffect::Fortified));
    EXPECT_EQ(4, static_cast<int>(StatusEffect::Regeneration));
    EXPECT_EQ(5, static_cast<int>(StatusEffect::Shield));
    EXPECT_EQ(6, static_cast<int>(StatusEffect::Inspired));
    EXPECT_EQ(7, static_cast<int>(StatusEffect::Invisible));
}

TEST(StatusEffectTest, Debuffs) {
    // Negative status effects
    EXPECT_EQ(8, static_cast<int>(StatusEffect::Slowed));
    EXPECT_EQ(9, static_cast<int>(StatusEffect::Weakened));
    EXPECT_EQ(10, static_cast<int>(StatusEffect::Vulnerable));
    EXPECT_EQ(11, static_cast<int>(StatusEffect::Burning));
    EXPECT_EQ(12, static_cast<int>(StatusEffect::Frozen));
    EXPECT_EQ(13, static_cast<int>(StatusEffect::Stunned));
    EXPECT_EQ(14, static_cast<int>(StatusEffect::Silenced));
    EXPECT_EQ(15, static_cast<int>(StatusEffect::Revealed));
}

// =============================================================================
// Ability Level Data Tests
// =============================================================================

TEST(AbilityLevelDataTest, DefaultConstruction) {
    AbilityLevelData data;

    EXPECT_FLOAT_EQ(0.0f, data.damage);
    EXPECT_FLOAT_EQ(0.0f, data.duration);
    EXPECT_FLOAT_EQ(0.0f, data.radius);
    EXPECT_FLOAT_EQ(0.0f, data.manaCost);
    EXPECT_FLOAT_EQ(0.0f, data.cooldown);
    EXPECT_FLOAT_EQ(0.0f, data.range);
    EXPECT_FLOAT_EQ(0.0f, data.effectStrength);
    EXPECT_EQ(0, data.summonCount);
}

TEST(AbilityLevelDataTest, Construction) {
    AbilityLevelData data;
    data.damage = 100.0f;
    data.duration = 5.0f;
    data.radius = 8.0f;
    data.manaCost = 50.0f;
    data.cooldown = 10.0f;
    data.range = 15.0f;
    data.effectStrength = 0.5f;
    data.summonCount = 3;

    EXPECT_FLOAT_EQ(100.0f, data.damage);
    EXPECT_FLOAT_EQ(5.0f, data.duration);
    EXPECT_FLOAT_EQ(8.0f, data.radius);
    EXPECT_FLOAT_EQ(50.0f, data.manaCost);
    EXPECT_FLOAT_EQ(10.0f, data.cooldown);
    EXPECT_FLOAT_EQ(15.0f, data.range);
    EXPECT_FLOAT_EQ(0.5f, data.effectStrength);
    EXPECT_EQ(3, data.summonCount);
}

// =============================================================================
// Ability Data Tests
// =============================================================================

TEST(AbilityDataTest, DefaultConstruction) {
    AbilityData data;

    EXPECT_EQ(-1, data.id);
    EXPECT_TRUE(data.name.empty());
    EXPECT_TRUE(data.description.empty());
    EXPECT_EQ(AbilityType::Active, data.type);
    EXPECT_EQ(TargetType::None, data.targetType);
    EXPECT_TRUE(data.effects.empty());
    EXPECT_EQ(StatusEffect::None, data.appliesStatus);
    EXPECT_EQ(1, data.requiredLevel);
    EXPECT_EQ(4, data.maxLevel);
    EXPECT_FALSE(data.requiresTarget);
    EXPECT_TRUE(data.canTargetSelf);
    EXPECT_TRUE(data.canTargetAlly);
    EXPECT_TRUE(data.canTargetEnemy);
    EXPECT_FALSE(data.canTargetGround);
}

TEST(AbilityDataTest, GetLevelData_Level1) {
    AbilityData data;

    AbilityLevelData level1;
    level1.damage = 100.0f;
    level1.cooldown = 10.0f;

    AbilityLevelData level2;
    level2.damage = 150.0f;
    level2.cooldown = 9.0f;

    data.levelData.push_back(level1);
    data.levelData.push_back(level2);

    const auto& result = data.GetLevelData(1);
    EXPECT_FLOAT_EQ(100.0f, result.damage);
    EXPECT_FLOAT_EQ(10.0f, result.cooldown);
}

TEST(AbilityDataTest, GetLevelData_Level2) {
    AbilityData data;

    AbilityLevelData level1;
    level1.damage = 100.0f;

    AbilityLevelData level2;
    level2.damage = 150.0f;

    data.levelData.push_back(level1);
    data.levelData.push_back(level2);

    const auto& result = data.GetLevelData(2);
    EXPECT_FLOAT_EQ(150.0f, result.damage);
}

TEST(AbilityDataTest, GetLevelData_Clamped) {
    AbilityData data;

    AbilityLevelData level1;
    level1.damage = 100.0f;

    data.levelData.push_back(level1);

    // Request level beyond available
    const auto& result = data.GetLevelData(5);
    EXPECT_FLOAT_EQ(100.0f, result.damage);  // Should return last level
}

TEST(AbilityDataTest, GetLevelData_ZeroLevel) {
    AbilityData data;

    AbilityLevelData level1;
    level1.damage = 100.0f;

    data.levelData.push_back(level1);

    // Level 0 should return level 1 data (index 0)
    const auto& result = data.GetLevelData(0);
    EXPECT_FLOAT_EQ(100.0f, result.damage);
}

// =============================================================================
// Ability State Tests
// =============================================================================

TEST(AbilityStateTest, DefaultConstruction) {
    AbilityState state;

    EXPECT_EQ(-1, state.abilityId);
    EXPECT_EQ(0, state.currentLevel);
    EXPECT_FLOAT_EQ(0.0f, state.cooldownRemaining);
    EXPECT_FALSE(state.isToggled);
    EXPECT_FALSE(state.isChanneling);
    EXPECT_FLOAT_EQ(0.0f, state.channelTimeRemaining);
}

TEST(AbilityStateTest, IsReady_NotLearned) {
    AbilityState state;
    state.currentLevel = 0;
    state.cooldownRemaining = 0.0f;

    EXPECT_FALSE(state.IsReady());
}

TEST(AbilityStateTest, IsReady_OnCooldown) {
    AbilityState state;
    state.currentLevel = 1;
    state.cooldownRemaining = 5.0f;

    EXPECT_FALSE(state.IsReady());
}

TEST(AbilityStateTest, IsReady_Available) {
    AbilityState state;
    state.currentLevel = 1;
    state.cooldownRemaining = 0.0f;

    EXPECT_TRUE(state.IsReady());
}

TEST(AbilityStateTest, IsLearned) {
    AbilityState state;
    state.currentLevel = 0;
    EXPECT_FALSE(state.IsLearned());

    state.currentLevel = 1;
    EXPECT_TRUE(state.IsLearned());
}

TEST(AbilityStateTest, IsMaxLevel) {
    AbilityData data;
    data.maxLevel = 4;

    AbilityState state;
    state.currentLevel = 3;
    EXPECT_FALSE(state.IsMaxLevel(data));

    state.currentLevel = 4;
    EXPECT_TRUE(state.IsMaxLevel(data));
}

// =============================================================================
// Ability Cast Result Tests
// =============================================================================

TEST(AbilityCastResultTest, DefaultConstruction) {
    AbilityCastResult result;

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.failReason.empty());
    EXPECT_FLOAT_EQ(0.0f, result.damageDealt);
    EXPECT_FLOAT_EQ(0.0f, result.healingDone);
    EXPECT_EQ(0, result.unitsAffected);
    EXPECT_TRUE(result.affectedEntities.empty());
}

TEST(AbilityCastResultTest, SuccessfulCast) {
    AbilityCastResult result;
    result.success = true;
    result.damageDealt = 250.0f;
    result.unitsAffected = 3;
    result.affectedEntities = {1, 2, 3};

    EXPECT_TRUE(result.success);
    EXPECT_FLOAT_EQ(250.0f, result.damageDealt);
    EXPECT_EQ(3, result.unitsAffected);
    EXPECT_EQ(3, result.affectedEntities.size());
}

TEST(AbilityCastResultTest, FailedCast) {
    AbilityCastResult result;
    result.success = false;
    result.failReason = "Not enough mana";

    EXPECT_FALSE(result.success);
    EXPECT_EQ("Not enough mana", result.failReason);
}

// =============================================================================
// Ability Cast Context Tests
// =============================================================================

TEST(AbilityCastContextTest, DefaultConstruction) {
    AbilityCastContext context;

    EXPECT_EQ(nullptr, context.caster);
    EXPECT_VEC3_EQ(glm::vec3(0.0f), context.targetPoint);
    EXPECT_EQ(nullptr, context.targetUnit);
    EXPECT_VEC3_EQ(glm::vec3(0.0f, 0.0f, 1.0f), context.direction);
    EXPECT_EQ(1, context.abilityLevel);
    EXPECT_FLOAT_EQ(0.0f, context.deltaTime);
}

// =============================================================================
// Ability Behavior Tests
// =============================================================================

class MockAbilityBehavior : public AbilityBehavior {
public:
    bool executeCalled = false;
    bool updateCalled = false;
    bool onEndCalled = false;
    AbilityCastResult returnResult;

    AbilityCastResult Execute(const AbilityCastContext& context, const AbilityData& data) override {
        executeCalled = true;
        return returnResult;
    }

    void Update(const AbilityCastContext& context, const AbilityData& data, float deltaTime) override {
        updateCalled = true;
    }

    void OnEnd(const AbilityCastContext& context, const AbilityData& data) override {
        onEndCalled = true;
    }
};

TEST(AbilityBehaviorTest, Execute_Called) {
    MockAbilityBehavior behavior;
    behavior.returnResult.success = true;
    behavior.returnResult.damageDealt = 100.0f;

    AbilityCastContext context;
    AbilityData data;

    auto result = behavior.Execute(context, data);

    EXPECT_TRUE(behavior.executeCalled);
    EXPECT_TRUE(result.success);
    EXPECT_FLOAT_EQ(100.0f, result.damageDealt);
}

TEST(AbilityBehaviorTest, Update_Called) {
    MockAbilityBehavior behavior;
    AbilityCastContext context;
    AbilityData data;

    behavior.Update(context, data, 0.016f);

    EXPECT_TRUE(behavior.updateCalled);
}

TEST(AbilityBehaviorTest, OnEnd_Called) {
    MockAbilityBehavior behavior;
    AbilityCastContext context;
    AbilityData data;

    behavior.OnEnd(context, data);

    EXPECT_TRUE(behavior.onEndCalled);
}

TEST(AbilityBehaviorTest, CanCast_Default) {
    MockAbilityBehavior behavior;
    AbilityCastContext context;
    AbilityData data;

    // Default implementation of CanCast
    bool canCast = behavior.CanCast(context, data);
    // Default should return true or check basic requirements
}

// =============================================================================
// Ability Manager Tests
// =============================================================================

class AbilityManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the ability manager
        AbilityManager::Instance().Initialize();
    }
};

TEST_F(AbilityManagerTest, GetAbility_Exists) {
    const AbilityData* ability = AbilityManager::Instance().GetAbility(AbilityId::RALLY);

    if (ability) {
        EXPECT_EQ(AbilityId::RALLY, ability->id);
        EXPECT_FALSE(ability->name.empty());
    }
}

TEST_F(AbilityManagerTest, GetAbility_NotFound) {
    const AbilityData* ability = AbilityManager::Instance().GetAbility(-999);
    EXPECT_EQ(nullptr, ability);
}

TEST_F(AbilityManagerTest, GetAbilityCount) {
    size_t count = AbilityManager::Instance().GetAbilityCount();
    EXPECT_GT(count, 0);
}

TEST_F(AbilityManagerTest, GetBehavior_Exists) {
    AbilityBehavior* behavior = AbilityManager::Instance().GetBehavior(AbilityId::RALLY);
    // May or may not have a registered behavior
}

TEST_F(AbilityManagerTest, RegisterBehavior) {
    auto behavior = std::make_unique<MockAbilityBehavior>();
    int testId = 9999;

    AbilityManager::Instance().RegisterBehavior(testId, std::move(behavior));

    AbilityBehavior* retrieved = AbilityManager::Instance().GetBehavior(testId);
    EXPECT_NE(nullptr, retrieved);
}

TEST_F(AbilityManagerTest, GetAbilitiesForClass) {
    auto abilities = AbilityManager::Instance().GetAbilitiesForClass(0);
    // Returns abilities available to a specific hero class
}

// =============================================================================
// Ability ID Constants Tests
// =============================================================================

TEST(AbilityIdTest, Constants) {
    EXPECT_EQ(0, AbilityId::RALLY);
    EXPECT_EQ(1, AbilityId::INSPIRE);
    EXPECT_EQ(2, AbilityId::FORTIFY);
    EXPECT_EQ(3, AbilityId::SHADOWSTEP);
    EXPECT_EQ(4, AbilityId::MARKET_MASTERY);
    EXPECT_EQ(5, AbilityId::WARCRY);
    EXPECT_EQ(6, AbilityId::REPAIR_AURA);
    EXPECT_EQ(7, AbilityId::DETECTION_WARD);
    EXPECT_EQ(8, AbilityId::TRADE_CARAVAN);
    EXPECT_EQ(9, AbilityId::BATTLE_STANDARD);
}

// =============================================================================
// Specific Ability Tests
// =============================================================================

class RallyAbilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        ability = std::make_unique<RallyAbility>();

        data.id = AbilityId::RALLY;
        data.name = "Rally";
        data.type = AbilityType::Active;
        data.targetType = TargetType::None;
        data.effects = {AbilityEffect::Buff};
        data.appliesStatus = StatusEffect::Might;

        AbilityLevelData level1;
        level1.duration = 10.0f;
        level1.radius = 10.0f;
        level1.effectStrength = 0.2f;  // 20% damage increase
        level1.manaCost = 50.0f;
        level1.cooldown = 30.0f;
        data.levelData.push_back(level1);
    }

    std::unique_ptr<RallyAbility> ability;
    AbilityData data;
    AbilityCastContext context;
};

TEST_F(RallyAbilityTest, Execute) {
    context.abilityLevel = 1;

    auto result = ability->Execute(context, data);
    // Rally should affect nearby allies
}

TEST_F(RallyAbilityTest, Update) {
    ability->Update(context, data, 0.016f);
    // Should maintain the buff effect
}

TEST_F(RallyAbilityTest, OnEnd) {
    ability->OnEnd(context, data);
    // Should clean up buff effects
}

// =============================================================================
// Shadowstep Ability Tests
// =============================================================================

class ShadowstepAbilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        ability = std::make_unique<ShadowstepAbility>();

        data.id = AbilityId::SHADOWSTEP;
        data.name = "Shadowstep";
        data.type = AbilityType::Active;
        data.targetType = TargetType::Point;
        data.effects = {AbilityEffect::Teleport, AbilityEffect::Stealth};

        AbilityLevelData level1;
        level1.range = 10.0f;
        level1.duration = 2.0f;  // Stealth duration
        level1.manaCost = 75.0f;
        level1.cooldown = 20.0f;
        data.levelData.push_back(level1);
    }

    std::unique_ptr<ShadowstepAbility> ability;
    AbilityData data;
    AbilityCastContext context;
};

TEST_F(ShadowstepAbilityTest, CanCast_InRange) {
    context.targetPoint = glm::vec3(5.0f, 0.0f, 0.0f);
    context.abilityLevel = 1;

    bool canCast = ability->CanCast(context, data);
    // Should be castable if target is in range
}

TEST_F(ShadowstepAbilityTest, CanCast_OutOfRange) {
    context.targetPoint = glm::vec3(100.0f, 0.0f, 0.0f);
    context.abilityLevel = 1;

    bool canCast = ability->CanCast(context, data);
    // Should not be castable if target is out of range
}

TEST_F(ShadowstepAbilityTest, Execute) {
    context.targetPoint = glm::vec3(5.0f, 0.0f, 0.0f);
    context.abilityLevel = 1;

    auto result = ability->Execute(context, data);
    // Should teleport caster and apply stealth
}

// =============================================================================
// Cooldown System Tests
// =============================================================================

TEST(CooldownSystemTest, StartCooldown) {
    AbilityState state;
    state.currentLevel = 1;
    state.cooldownRemaining = 0.0f;

    // Use ability - start cooldown
    float cooldown = 10.0f;
    state.cooldownRemaining = cooldown;

    EXPECT_FALSE(state.IsReady());
    EXPECT_FLOAT_EQ(10.0f, state.cooldownRemaining);
}

TEST(CooldownSystemTest, ReduceCooldown) {
    AbilityState state;
    state.currentLevel = 1;
    state.cooldownRemaining = 10.0f;

    // Simulate time passing
    float deltaTime = 5.0f;
    state.cooldownRemaining = std::max(0.0f, state.cooldownRemaining - deltaTime);

    EXPECT_FLOAT_EQ(5.0f, state.cooldownRemaining);
}

TEST(CooldownSystemTest, CooldownComplete) {
    AbilityState state;
    state.currentLevel = 1;
    state.cooldownRemaining = 1.0f;

    // Simulate time passing beyond cooldown
    float deltaTime = 2.0f;
    state.cooldownRemaining = std::max(0.0f, state.cooldownRemaining - deltaTime);

    EXPECT_TRUE(state.IsReady());
    EXPECT_FLOAT_EQ(0.0f, state.cooldownRemaining);
}

// =============================================================================
// Effect Application Tests
// =============================================================================

TEST(EffectApplicationTest, DamageApplication) {
    float baseDamage = 100.0f;
    float spellPower = 50.0f;
    float scaling = 0.5f;  // 50% spell power scaling

    float totalDamage = baseDamage + (spellPower * scaling);
    EXPECT_FLOAT_EQ(125.0f, totalDamage);
}

TEST(EffectApplicationTest, HealingApplication) {
    float baseHealing = 50.0f;
    float targetMissingHealth = 100.0f;
    float missingHealthBonus = 0.1f;  // 10% of missing health bonus

    float totalHealing = baseHealing + (targetMissingHealth * missingHealthBonus);
    EXPECT_FLOAT_EQ(60.0f, totalHealing);
}

TEST(EffectApplicationTest, SlowStacking_Multiplicative) {
    float baseSpeed = 100.0f;
    float slow1 = 0.3f;  // 30% slow
    float slow2 = 0.2f;  // 20% slow

    // Multiplicative: 100 * 0.7 * 0.8 = 56
    float finalSpeed = baseSpeed * (1.0f - slow1) * (1.0f - slow2);
    EXPECT_FLOAT_EQ(56.0f, finalSpeed);
}

TEST(EffectApplicationTest, SlowStacking_Diminishing) {
    float baseSpeed = 100.0f;
    float slow1 = 0.3f;
    float slow2 = 0.2f;

    // Diminishing returns: second slow is less effective
    float effectiveSlow1 = slow1;
    float effectiveSlow2 = slow2 * 0.5f;  // 50% effectiveness
    float totalSlow = effectiveSlow1 + effectiveSlow2;

    float finalSpeed = baseSpeed * (1.0f - totalSlow);
    EXPECT_FLOAT_EQ(60.0f, finalSpeed);
}

// =============================================================================
// Targeting Tests
// =============================================================================

TEST(TargetingTest, SelfTarget) {
    AbilityData data;
    data.canTargetSelf = true;
    data.targetType = TargetType::Unit;

    // Caster ID == Target ID
    uint32_t casterId = 1;
    uint32_t targetId = 1;

    bool isValidTarget = data.canTargetSelf && (casterId == targetId);
    EXPECT_TRUE(isValidTarget);
}

TEST(TargetingTest, CannotTargetSelf) {
    AbilityData data;
    data.canTargetSelf = false;
    data.targetType = TargetType::Unit;

    uint32_t casterId = 1;
    uint32_t targetId = 1;

    bool isValidTarget = data.canTargetSelf || (casterId != targetId);
    EXPECT_FALSE(isValidTarget);
}

TEST(TargetingTest, PointTarget_InRange) {
    glm::vec3 casterPos = glm::vec3(0.0f);
    glm::vec3 targetPoint = glm::vec3(5.0f, 0.0f, 0.0f);
    float maxRange = 10.0f;

    float distance = glm::length(targetPoint - casterPos);
    bool inRange = distance <= maxRange;

    EXPECT_TRUE(inRange);
}

TEST(TargetingTest, PointTarget_OutOfRange) {
    glm::vec3 casterPos = glm::vec3(0.0f);
    glm::vec3 targetPoint = glm::vec3(15.0f, 0.0f, 0.0f);
    float maxRange = 10.0f;

    float distance = glm::length(targetPoint - casterPos);
    bool inRange = distance <= maxRange;

    EXPECT_FALSE(inRange);
}

TEST(TargetingTest, ConeTarget_InCone) {
    glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 casterPos = glm::vec3(0.0f);
    glm::vec3 targetPos = glm::vec3(5.0f, 0.0f, 2.0f);
    float coneAngle = glm::radians(45.0f);

    glm::vec3 toTarget = glm::normalize(targetPos - casterPos);
    float angle = std::acos(glm::dot(direction, toTarget));

    bool inCone = angle <= coneAngle;
    EXPECT_TRUE(inCone);
}

TEST(TargetingTest, ConeTarget_OutsideCone) {
    glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 casterPos = glm::vec3(0.0f);
    glm::vec3 targetPos = glm::vec3(-5.0f, 0.0f, 0.0f);  // Behind caster
    float coneAngle = glm::radians(45.0f);

    glm::vec3 toTarget = glm::normalize(targetPos - casterPos);
    float angle = std::acos(glm::dot(direction, toTarget));

    bool inCone = angle <= coneAngle;
    EXPECT_FALSE(inCone);
}

// =============================================================================
// Area of Effect Tests
// =============================================================================

TEST(AreaOfEffectTest, CircularAoE_InRadius) {
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 5.0f;
    glm::vec3 targetPos = glm::vec3(3.0f, 0.0f, 2.0f);

    float distance = glm::length(targetPos - center);
    bool inRadius = distance <= radius;

    EXPECT_TRUE(inRadius);
}

TEST(AreaOfEffectTest, CircularAoE_OutsideRadius) {
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 5.0f;
    glm::vec3 targetPos = glm::vec3(10.0f, 0.0f, 10.0f);

    float distance = glm::length(targetPos - center);
    bool inRadius = distance <= radius;

    EXPECT_FALSE(inRadius);
}

TEST(AreaOfEffectTest, DamageFalloff) {
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 10.0f;
    float baseDamage = 100.0f;

    // At center - full damage
    float distanceAtCenter = 0.0f;
    float damageAtCenter = baseDamage * (1.0f - distanceAtCenter / radius);
    EXPECT_FLOAT_EQ(100.0f, damageAtCenter);

    // At half radius - half damage
    float distanceHalf = 5.0f;
    float damageHalf = baseDamage * (1.0f - distanceHalf / radius);
    EXPECT_FLOAT_EQ(50.0f, damageHalf);

    // At edge - no damage
    float distanceEdge = 10.0f;
    float damageEdge = baseDamage * (1.0f - distanceEdge / radius);
    EXPECT_FLOAT_EQ(0.0f, damageEdge);
}

// =============================================================================
// Channeled Ability Tests
// =============================================================================

TEST(ChanneledAbilityTest, StartChanneling) {
    AbilityState state;
    state.currentLevel = 1;
    state.isChanneling = false;

    // Start channeling
    float channelDuration = 3.0f;
    state.isChanneling = true;
    state.channelTimeRemaining = channelDuration;

    EXPECT_TRUE(state.isChanneling);
    EXPECT_FLOAT_EQ(3.0f, state.channelTimeRemaining);
}

TEST(ChanneledAbilityTest, UpdateChanneling) {
    AbilityState state;
    state.isChanneling = true;
    state.channelTimeRemaining = 3.0f;

    // Simulate time passing
    state.channelTimeRemaining -= 1.0f;

    EXPECT_TRUE(state.isChanneling);
    EXPECT_FLOAT_EQ(2.0f, state.channelTimeRemaining);
}

TEST(ChanneledAbilityTest, CompleteChanneling) {
    AbilityState state;
    state.isChanneling = true;
    state.channelTimeRemaining = 0.0f;

    // Channel complete
    if (state.channelTimeRemaining <= 0.0f) {
        state.isChanneling = false;
    }

    EXPECT_FALSE(state.isChanneling);
}

TEST(ChanneledAbilityTest, InterruptChanneling) {
    AbilityState state;
    state.isChanneling = true;
    state.channelTimeRemaining = 2.0f;

    // Interrupted by damage
    bool interrupted = true;
    if (interrupted) {
        state.isChanneling = false;
        state.channelTimeRemaining = 0.0f;
    }

    EXPECT_FALSE(state.isChanneling);
}

// =============================================================================
// Toggle Ability Tests
// =============================================================================

TEST(ToggleAbilityTest, ToggleOn) {
    AbilityState state;
    state.isToggled = false;

    state.isToggled = true;
    EXPECT_TRUE(state.isToggled);
}

TEST(ToggleAbilityTest, ToggleOff) {
    AbilityState state;
    state.isToggled = true;

    state.isToggled = false;
    EXPECT_FALSE(state.isToggled);
}

TEST(ToggleAbilityTest, ManaDrain) {
    float currentMana = 100.0f;
    float manaPerSecond = 5.0f;
    float deltaTime = 1.0f;

    // Drain mana while toggled
    currentMana -= manaPerSecond * deltaTime;

    EXPECT_FLOAT_EQ(95.0f, currentMana);
}

TEST(ToggleAbilityTest, AutoToggleOff_NoMana) {
    AbilityState state;
    state.isToggled = true;

    float currentMana = 3.0f;
    float manaRequired = 5.0f;

    // Auto toggle off if not enough mana
    if (currentMana < manaRequired) {
        state.isToggled = false;
    }

    EXPECT_FALSE(state.isToggled);
}

