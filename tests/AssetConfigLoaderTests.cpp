#include <gtest/gtest.h>
#include "../engine/game/AssetConfig.hpp"
#include "../engine/game/SDFConfigLoader.hpp"
#include <fstream>
#include <filesystem>

using namespace Engine;

class AssetConfigLoaderTest : public ::testing::Test {
protected:
    SDFConfigLoader loader;
    std::filesystem::path testDir;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "asset_config_tests";
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(testDir);
    }

    void WriteTestFile(const std::string& filename, const std::string& content) {
        std::ofstream file(testDir / filename);
        file << content;
        file.close();
    }
};

// =============================================================================
// Unit Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadUnitConfig_BasicFields) {
    const std::string unitJson = R"({
        "id": "test_unit",
        "type": "unit",
        "name": "Test Unit",
        "race": "humans",
        "category": "infantry",
        "description": "A test unit",
        "unitClass": "melee",
        "armorType": "heavy",
        "attackType": "normal",
        "squadSize": 1,
        "stats": {
            "health": 100,
            "armor": 5,
            "damage": 10,
            "attackSpeed": 1.5,
            "moveSpeed": 200
        },
        "costs": {
            "gold": 50,
            "lumber": 0,
            "food": 1
        }
    })";

    WriteTestFile("test_unit.json", unitJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_unit.json");
    ASSERT_TRUE(std::holds_alternative<UnitConfig>(result));

    const auto& unit = std::get<UnitConfig>(result);
    EXPECT_EQ(unit.id, "test_unit");
    EXPECT_EQ(unit.type, AssetType::Unit);
    EXPECT_EQ(unit.name, "Test Unit");
    EXPECT_EQ(unit.race, "humans");
    EXPECT_EQ(unit.unitClass, "melee");
    EXPECT_EQ(unit.armorType, "heavy");
    EXPECT_EQ(unit.attackType, "normal");
    EXPECT_EQ(unit.squadSize, 1);
    EXPECT_EQ(unit.stats.health, 100);
    EXPECT_EQ(unit.stats.armor, 5);
    EXPECT_EQ(unit.stats.damage, 10);
    EXPECT_FLOAT_EQ(unit.stats.attackSpeed, 1.5f);
    EXPECT_EQ(unit.costs.gold, 50);
    EXPECT_EQ(unit.costs.food, 1);
}

TEST_F(AssetConfigLoaderTest, LoadUnitConfig_WithSdfModelRef) {
    const std::string unitJson = R"({
        "id": "unit_with_ref",
        "type": "unit",
        "name": "Unit With Ref",
        "sdfModelRef": "models.footman",
        "skeletonRef": "skeleton.humanoid",
        "animationSetRef": "anims.footman",
        "behaviorRef": "behavior.melee"
    })";

    WriteTestFile("unit_with_ref.json", unitJson);

    auto result = loader.LoadAssetFromFile(testDir / "unit_with_ref.json");
    ASSERT_TRUE(std::holds_alternative<UnitConfig>(result));

    const auto& unit = std::get<UnitConfig>(result);
    EXPECT_EQ(unit.sdfModelRef, "models.footman");
    EXPECT_EQ(unit.skeletonRef, "skeleton.humanoid");
    EXPECT_EQ(unit.animationSetRef, "anims.footman");
    EXPECT_EQ(unit.behaviorRef, "behavior.melee");
}

// =============================================================================
// Hero Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadHeroConfig_FullHero) {
    const std::string heroJson = R"({
        "id": "test_hero",
        "type": "hero",
        "name": "Test Hero",
        "heroClass": "warrior",
        "startingLevel": 1,
        "healthPerLevel": 100,
        "manaPerLevel": 50,
        "damagePerLevel": 5,
        "strPerLevel": 3.0,
        "agiPerLevel": 1.5,
        "intPerLevel": 2.0,
        "stats": {
            "health": 650,
            "mana": 200,
            "strength": 22,
            "agility": 14,
            "intelligence": 18
        },
        "heroAbilityRefs": ["ability.q", "ability.w", "ability.e"],
        "ultimateAbilityRef": "ability.r"
    })";

    WriteTestFile("test_hero.json", heroJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_hero.json");
    ASSERT_TRUE(std::holds_alternative<HeroConfig>(result));

    const auto& hero = std::get<HeroConfig>(result);
    EXPECT_EQ(hero.id, "test_hero");
    EXPECT_EQ(hero.type, AssetType::Hero);
    EXPECT_EQ(hero.heroClass, "warrior");
    EXPECT_EQ(hero.startingLevel, 1);
    EXPECT_EQ(hero.healthPerLevel, 100);
    EXPECT_EQ(hero.manaPerLevel, 50);
    EXPECT_EQ(hero.damagePerLevel, 5);
    EXPECT_FLOAT_EQ(hero.strPerLevel, 3.0f);
    EXPECT_EQ(hero.stats.strength, 22);
    EXPECT_EQ(hero.heroAbilityRefs.size(), 3);
    EXPECT_EQ(hero.ultimateAbilityRef, "ability.r");
}

// =============================================================================
// Building Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadBuildingConfig_WithTraining) {
    const std::string buildingJson = R"({
        "id": "test_barracks",
        "type": "building",
        "name": "Barracks",
        "isDefensive": false,
        "isMainBuilding": false,
        "providesDropOff": false,
        "footprint": [3, 3],
        "trains": ["footman", "rifleman"],
        "upgrades": ["advanced_barracks"],
        "researches": ["improved_weapons"],
        "stats": {
            "health": 1500,
            "armor": 5,
            "buildTime": 60.0
        }
    })";

    WriteTestFile("test_barracks.json", buildingJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_barracks.json");
    ASSERT_TRUE(std::holds_alternative<BuildingConfig>(result));

    const auto& building = std::get<BuildingConfig>(result);
    EXPECT_EQ(building.id, "test_barracks");
    EXPECT_EQ(building.type, AssetType::Building);
    EXPECT_EQ(building.trains.size(), 2);
    EXPECT_EQ(building.trains[0], "footman");
    EXPECT_EQ(building.upgrades.size(), 1);
    EXPECT_EQ(building.researches.size(), 1);
    EXPECT_FLOAT_EQ(building.footprint.x, 3.0f);
    EXPECT_FLOAT_EQ(building.footprint.y, 3.0f);
}

// =============================================================================
// SDF Model Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadSDFModel_WithPrimitives) {
    const std::string sdfJson = R"({
        "id": "test_model",
        "type": "sdf_model",
        "name": "Test Model",
        "bounds": {
            "min": [-1.0, 0.0, -1.0],
            "max": [1.0, 2.0, 1.0]
        },
        "primitives": [
            {
                "id": "body",
                "type": "Sphere",
                "params": { "radius": 0.5 },
                "transform": {
                    "position": [0.0, 1.0, 0.0],
                    "rotation": [0.0, 0.0, 0.0, 1.0],
                    "scale": [1.0, 1.0, 1.0]
                },
                "material": {
                    "baseColor": [1.0, 0.0, 0.0, 1.0],
                    "metallic": 0.5,
                    "roughness": 0.5
                },
                "operation": "Union"
            },
            {
                "id": "arm",
                "type": "Capsule",
                "params": { "radius": 0.1, "height": 0.5 },
                "transform": {
                    "position": [0.3, 1.0, 0.0]
                },
                "material": {
                    "baseColor": [0.0, 1.0, 0.0, 1.0]
                },
                "operation": "SmoothUnion",
                "smoothness": 0.05,
                "bone": "arm_r"
            }
        ]
    })";

    WriteTestFile("test_model.json", sdfJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_model.json");
    ASSERT_TRUE(std::holds_alternative<SDFModelConfig>(result));

    const auto& model = std::get<SDFModelConfig>(result);
    EXPECT_EQ(model.id, "test_model");
    EXPECT_EQ(model.type, AssetType::SDFModel);
    EXPECT_EQ(model.primitives.size(), 2);

    const auto& sphere = model.primitives[0];
    EXPECT_EQ(sphere.id, "body");
    EXPECT_EQ(sphere.type, "Sphere");
    EXPECT_EQ(sphere.operation, "Union");
    EXPECT_FLOAT_EQ(sphere.position.y, 1.0f);
    EXPECT_FLOAT_EQ(sphere.baseColor.r, 1.0f);

    const auto& capsule = model.primitives[1];
    EXPECT_EQ(capsule.bone, "arm_r");
    EXPECT_EQ(capsule.operation, "SmoothUnion");
    EXPECT_FLOAT_EQ(capsule.smoothness, 0.05f);
}

// =============================================================================
// Skeleton Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadSkeleton_WithBoneHierarchy) {
    const std::string skeletonJson = R"({
        "id": "test_skeleton",
        "type": "skeleton",
        "name": "Test Skeleton",
        "bones": [
            { "name": "root", "parent": null, "position": [0.0, 0.0, 0.0] },
            { "name": "spine", "parent": "root", "position": [0.0, 0.5, 0.0] },
            { "name": "chest", "parent": "spine", "position": [0.0, 0.3, 0.0] },
            { "name": "head", "parent": "chest", "position": [0.0, 0.2, 0.0] },
            { "name": "arm_l", "parent": "chest", "position": [-0.2, 0.1, 0.0] },
            { "name": "arm_r", "parent": "chest", "position": [0.2, 0.1, 0.0] }
        ]
    })";

    WriteTestFile("test_skeleton.json", skeletonJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_skeleton.json");
    ASSERT_TRUE(std::holds_alternative<SkeletonConfig>(result));

    const auto& skeleton = std::get<SkeletonConfig>(result);
    EXPECT_EQ(skeleton.id, "test_skeleton");
    EXPECT_EQ(skeleton.type, AssetType::Skeleton);
    EXPECT_EQ(skeleton.bones.size(), 6);

    EXPECT_EQ(skeleton.bones[0].name, "root");
    EXPECT_TRUE(skeleton.bones[0].parent.empty() || skeleton.bones[0].parent == "null");

    EXPECT_EQ(skeleton.bones[1].name, "spine");
    EXPECT_EQ(skeleton.bones[1].parent, "root");
}

// =============================================================================
// Animation Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadAnimation_WithKeyframes) {
    const std::string animJson = R"({
        "id": "test_walk",
        "type": "animation",
        "name": "Walk Cycle",
        "duration": 1.0,
        "loop": true,
        "skeletonRef": "test_skeleton",
        "keyframes": [
            {
                "time": 0.0,
                "bones": {
                    "leg_l": { "rotation": [0.2, 0.0, 0.0, 0.98] },
                    "leg_r": { "rotation": [-0.2, 0.0, 0.0, 0.98] }
                }
            },
            {
                "time": 0.5,
                "bones": {
                    "leg_l": { "rotation": [-0.2, 0.0, 0.0, 0.98] },
                    "leg_r": { "rotation": [0.2, 0.0, 0.0, 0.98] }
                },
                "events": ["footstep"]
            },
            {
                "time": 1.0,
                "bones": {
                    "leg_l": { "rotation": [0.2, 0.0, 0.0, 0.98] },
                    "leg_r": { "rotation": [-0.2, 0.0, 0.0, 0.98] }
                }
            }
        ]
    })";

    WriteTestFile("test_walk.json", animJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_walk.json");
    ASSERT_TRUE(std::holds_alternative<AnimationConfig>(result));

    const auto& anim = std::get<AnimationConfig>(result);
    EXPECT_EQ(anim.id, "test_walk");
    EXPECT_EQ(anim.type, AssetType::Animation);
    EXPECT_FLOAT_EQ(anim.duration, 1.0f);
    EXPECT_TRUE(anim.loop);
    EXPECT_EQ(anim.skeletonRef, "test_skeleton");
    EXPECT_EQ(anim.keyframes.size(), 3);

    EXPECT_FLOAT_EQ(anim.keyframes[1].time, 0.5f);
    EXPECT_EQ(anim.keyframes[1].events.size(), 1);
    EXPECT_EQ(anim.keyframes[1].events[0], "footstep");
}

// =============================================================================
// Ability Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadAbility_WithParams) {
    const std::string abilityJson = R"({
        "id": "test_fireball",
        "type": "ability",
        "name": "Fireball",
        "description": "Launches a ball of fire",
        "hotkey": "Q",
        "targetType": "point",
        "cooldown": 8.0,
        "manaCost": 75,
        "range": 800.0,
        "castTime": 0.5,
        "radius": 200.0,
        "effectRefs": ["effect.fireball_projectile", "effect.explosion"],
        "damage": 100,
        "damageType": "fire"
    })";

    WriteTestFile("test_fireball.json", abilityJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_fireball.json");
    ASSERT_TRUE(std::holds_alternative<AbilityConfig>(result));

    const auto& ability = std::get<AbilityConfig>(result);
    EXPECT_EQ(ability.id, "test_fireball");
    EXPECT_EQ(ability.type, AssetType::Ability);
    EXPECT_EQ(ability.hotkey, "Q");
    EXPECT_EQ(ability.targetType, "point");
    EXPECT_FLOAT_EQ(ability.cooldown, 8.0f);
    EXPECT_EQ(ability.manaCost, 75);
    EXPECT_FLOAT_EQ(ability.range, 800.0f);
    EXPECT_EQ(ability.effectRefs.size(), 2);
}

// =============================================================================
// Behavior Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadBehavior_WithTriggers) {
    const std::string behaviorJson = R"({
        "id": "test_behavior",
        "type": "behavior",
        "name": "Test Behavior",
        "on_spawn": {
            "actions": [
                { "type": "play_sound", "sound": "spawn_sound" },
                { "type": "play_animation", "animation": "spawn" }
            ]
        },
        "on_death": {
            "conditions": [
                { "type": "health_below", "value": 0 }
            ],
            "actions": [
                { "type": "spawn_effect", "effect": "death_particles" }
            ]
        }
    })";

    WriteTestFile("test_behavior.json", behaviorJson);

    auto result = loader.LoadAssetFromFile(testDir / "test_behavior.json");
    ASSERT_TRUE(std::holds_alternative<BehaviorConfig>(result));

    const auto& behavior = std::get<BehaviorConfig>(result);
    EXPECT_EQ(behavior.id, "test_behavior");
    EXPECT_EQ(behavior.type, AssetType::Behavior);
    EXPECT_TRUE(behavior.triggers.count("on_spawn") > 0);
    EXPECT_TRUE(behavior.triggers.count("on_death") > 0);
    EXPECT_EQ(behavior.triggers.at("on_spawn").actions.size(), 2);
    EXPECT_EQ(behavior.triggers.at("on_death").conditions.size(), 1);
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, Validate_EmptyId_ReturnsError) {
    EntityConfig config;
    config.id = "";
    config.name = "Test";
    config.type = AssetType::Entity;

    auto errors = loader.Validate(config);
    EXPECT_FALSE(errors.empty());
    EXPECT_TRUE(std::find_if(errors.begin(), errors.end(), [](const std::string& e) {
        return e.find("ID") != std::string::npos;
    }) != errors.end());
}

TEST_F(AssetConfigLoaderTest, Validate_InvalidBoneReference_ReturnsError) {
    EntityConfig config;
    config.id = "test";
    config.name = "Test";
    config.type = AssetType::Entity;
    config.stats.health = 100;

    SkeletonConfig skeleton;
    skeleton.bones.push_back({"root", "", glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)});
    config.skeleton = skeleton;

    SDFModelConfig model;
    SDFPrimitiveConfig prim;
    prim.id = "body";
    prim.type = "Sphere";
    prim.bone = "nonexistent_bone";  // Invalid reference
    model.primitives.push_back(prim);
    config.sdfModel = model;

    auto errors = loader.Validate(config);
    EXPECT_TRUE(std::find_if(errors.begin(), errors.end(), [](const std::string& e) {
        return e.find("nonexistent_bone") != std::string::npos;
    }) != errors.end());
}

TEST_F(AssetConfigLoaderTest, Validate_ValidConfig_NoErrors) {
    EntityConfig config;
    config.id = "valid_entity";
    config.name = "Valid Entity";
    config.type = AssetType::Entity;
    config.stats.health = 100;

    SkeletonConfig skeleton;
    skeleton.bones.push_back({"root", "", glm::vec3(0), glm::quat(1, 0, 0, 0), glm::vec3(1)});
    skeleton.bones.push_back({"arm", "root", glm::vec3(0.2f, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(1)});
    config.skeleton = skeleton;

    SDFModelConfig model;
    SDFPrimitiveConfig prim;
    prim.id = "body";
    prim.type = "Sphere";
    prim.operation = "Union";
    prim.bone = "arm";  // Valid reference
    model.primitives.push_back(prim);
    config.sdfModel = model;

    auto errors = loader.Validate(config);
    EXPECT_TRUE(errors.empty()) << "Validation errors: " << (errors.empty() ? "" : errors[0]);
}

// =============================================================================
// Directory Loading Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadAssetsFromDirectory_LoadsMultipleAssets) {
    WriteTestFile("unit1.json", R"({"id": "unit1", "type": "unit", "name": "Unit 1"})");
    WriteTestFile("unit2.json", R"({"id": "unit2", "type": "unit", "name": "Unit 2"})");
    WriteTestFile("building1.json", R"({"id": "building1", "type": "building", "name": "Building 1"})");

    auto assets = loader.LoadAssetsFromDirectory(testDir, false);

    EXPECT_EQ(assets.size(), 3);
    EXPECT_TRUE(assets.count("unit1") > 0);
    EXPECT_TRUE(assets.count("unit2") > 0);
    EXPECT_TRUE(assets.count("building1") > 0);
}

// =============================================================================
// Resource Node Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadResourceNode_GoldMine) {
    const std::string resourceJson = R"({
        "id": "gold_mine",
        "type": "resource_node",
        "name": "Gold Mine",
        "resourceType": "gold",
        "resourceAmount": 12500,
        "harvestRate": 10,
        "harvestTime": 1.0,
        "depletes": true,
        "respawns": false
    })";

    WriteTestFile("gold_mine.json", resourceJson);

    auto result = loader.LoadAssetFromFile(testDir / "gold_mine.json");
    ASSERT_TRUE(std::holds_alternative<ResourceNodeConfig>(result));

    const auto& resource = std::get<ResourceNodeConfig>(result);
    EXPECT_EQ(resource.id, "gold_mine");
    EXPECT_EQ(resource.type, AssetType::ResourceNode);
    EXPECT_EQ(resource.resourceType, "gold");
    EXPECT_EQ(resource.resourceAmount, 12500);
    EXPECT_TRUE(resource.depletes);
    EXPECT_FALSE(resource.respawns);
}

// =============================================================================
// Projectile Config Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadProjectile_Arrow) {
    const std::string projectileJson = R"({
        "id": "arrow",
        "type": "projectile",
        "name": "Arrow",
        "speed": 900.0,
        "arcHeight": 50.0,
        "homing": false,
        "damage": 15,
        "splashRadius": 0.0,
        "impactEffectRef": "effect.arrow_impact"
    })";

    WriteTestFile("arrow.json", projectileJson);

    auto result = loader.LoadAssetFromFile(testDir / "arrow.json");
    ASSERT_TRUE(std::holds_alternative<ProjectileConfig>(result));

    const auto& projectile = std::get<ProjectileConfig>(result);
    EXPECT_EQ(projectile.id, "arrow");
    EXPECT_EQ(projectile.type, AssetType::Projectile);
    EXPECT_FLOAT_EQ(projectile.speed, 900.0f);
    EXPECT_FLOAT_EQ(projectile.arcHeight, 50.0f);
    EXPECT_FALSE(projectile.homing);
    EXPECT_EQ(projectile.damage, 15);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(AssetConfigLoaderTest, LoadFromFile_FileNotFound_Throws) {
    EXPECT_THROW(
        loader.LoadAssetFromFile(testDir / "nonexistent.json"),
        std::runtime_error
    );
}

TEST_F(AssetConfigLoaderTest, LoadFromFile_InvalidJson_Throws) {
    WriteTestFile("invalid.json", "{ this is not valid json }");

    EXPECT_THROW(
        loader.LoadAssetFromFile(testDir / "invalid.json"),
        std::runtime_error
    );
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
