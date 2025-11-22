/**
 * @file test_tech_tree.cpp
 * @brief Unit tests for tech tree and age progression system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "rts/TechTree.hpp"
#include "rts/Culture.hpp"
#include "rts/Resource.hpp"

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

#include <nlohmann/json.hpp>

using namespace Vehement;
using namespace Vehement::RTS;
using namespace Nova::Test;
using json = nlohmann::json;

// =============================================================================
// Age Enum Tests
// =============================================================================

TEST(AgeTest, AgeCount) {
    EXPECT_EQ(7, static_cast<int>(Age::COUNT));
}

TEST(AgeTest, AgeToString) {
    EXPECT_STREQ("Stone Age", AgeToString(Age::Stone));
    EXPECT_STREQ("Bronze Age", AgeToString(Age::Bronze));
    EXPECT_STREQ("Iron Age", AgeToString(Age::Iron));
    EXPECT_STREQ("Medieval Age", AgeToString(Age::Medieval));
    EXPECT_STREQ("Industrial Age", AgeToString(Age::Industrial));
    EXPECT_STREQ("Modern Age", AgeToString(Age::Modern));
    EXPECT_STREQ("Future Age", AgeToString(Age::Future));
}

TEST(AgeTest, AgeToShortString) {
    EXPECT_STREQ("Stone", AgeToShortString(Age::Stone));
    EXPECT_STREQ("Bronze", AgeToShortString(Age::Bronze));
    EXPECT_STREQ("Iron", AgeToShortString(Age::Iron));
    EXPECT_STREQ("Medieval", AgeToShortString(Age::Medieval));
    EXPECT_STREQ("Industrial", AgeToShortString(Age::Industrial));
    EXPECT_STREQ("Modern", AgeToShortString(Age::Modern));
    EXPECT_STREQ("Future", AgeToShortString(Age::Future));
}

TEST(AgeTest, StringToAge) {
    EXPECT_EQ(Age::Stone, StringToAge("Stone"));
    EXPECT_EQ(Age::Stone, StringToAge("Stone Age"));
    EXPECT_EQ(Age::Bronze, StringToAge("Bronze"));
    EXPECT_EQ(Age::Iron, StringToAge("Iron Age"));
    EXPECT_EQ(Age::Medieval, StringToAge("Medieval"));
    EXPECT_EQ(Age::Industrial, StringToAge("Industrial"));
    EXPECT_EQ(Age::Modern, StringToAge("Modern"));
    EXPECT_EQ(Age::Future, StringToAge("Future"));

    // Invalid string defaults to Stone
    EXPECT_EQ(Age::Stone, StringToAge("invalid"));
}

TEST(AgeTest, AgeToIndex) {
    EXPECT_EQ(0, AgeToIndex(Age::Stone));
    EXPECT_EQ(1, AgeToIndex(Age::Bronze));
    EXPECT_EQ(2, AgeToIndex(Age::Iron));
    EXPECT_EQ(6, AgeToIndex(Age::Future));
}

TEST(AgeTest, IndexToAge) {
    EXPECT_EQ(Age::Stone, IndexToAge(0));
    EXPECT_EQ(Age::Bronze, IndexToAge(1));
    EXPECT_EQ(Age::Future, IndexToAge(6));

    // Bounds checking
    EXPECT_EQ(Age::Stone, IndexToAge(-1));
    EXPECT_EQ(Age::Future, IndexToAge(100));
}

// =============================================================================
// Tech Category Tests
// =============================================================================

TEST(TechCategoryTest, TechCategoryToString) {
    EXPECT_STREQ("Military", TechCategoryToString(TechCategory::Military));
    EXPECT_STREQ("Economy", TechCategoryToString(TechCategory::Economy));
    EXPECT_STREQ("Defense", TechCategoryToString(TechCategory::Defense));
    EXPECT_STREQ("Infrastructure", TechCategoryToString(TechCategory::Infrastructure));
    EXPECT_STREQ("Science", TechCategoryToString(TechCategory::Science));
    EXPECT_STREQ("Special", TechCategoryToString(TechCategory::Special));
}

// =============================================================================
// Tech Status Tests
// =============================================================================

TEST(TechStatusTest, TechStatusToString) {
    EXPECT_STREQ("Locked", TechStatusToString(TechStatus::Locked));
    EXPECT_STREQ("Available", TechStatusToString(TechStatus::Available));
    EXPECT_STREQ("In Progress", TechStatusToString(TechStatus::InProgress));
    EXPECT_STREQ("Completed", TechStatusToString(TechStatus::Completed));
    EXPECT_STREQ("Lost", TechStatusToString(TechStatus::Lost));
}

// =============================================================================
// Tech Effect Tests
// =============================================================================

TEST(TechEffectTest, Multiplier) {
    auto effect = TechEffect::Multiplier("attack_damage", 1.25f, "+25% damage");

    EXPECT_EQ(TechEffectType::StatMultiplier, effect.type);
    EXPECT_EQ("attack_damage", effect.target);
    EXPECT_FLOAT_EQ(1.25f, effect.value);
    EXPECT_EQ("+25% damage", effect.description);
}

TEST(TechEffectTest, FlatBonus) {
    auto effect = TechEffect::FlatBonus("max_health", 50.0f, "+50 HP");

    EXPECT_EQ(TechEffectType::StatFlat, effect.type);
    EXPECT_EQ("max_health", effect.target);
    EXPECT_FLOAT_EQ(50.0f, effect.value);
}

TEST(TechEffectTest, UnlockBuilding) {
    auto effect = TechEffect::UnlockBuilding("barracks", "Unlocks Barracks");

    EXPECT_EQ(TechEffectType::UnlockBuilding, effect.type);
    EXPECT_EQ("barracks", effect.stringValue);
}

TEST(TechEffectTest, UnlockUnit) {
    auto effect = TechEffect::UnlockUnit("pikeman", "Unlocks Pikeman");

    EXPECT_EQ(TechEffectType::UnlockUnit, effect.type);
    EXPECT_EQ("pikeman", effect.stringValue);
}

TEST(TechEffectTest, UnlockAbility) {
    auto effect = TechEffect::UnlockAbility("charge", "Unlocks Charge ability");

    EXPECT_EQ(TechEffectType::UnlockAbility, effect.type);
    EXPECT_EQ("charge", effect.stringValue);
}

TEST(TechEffectTest, EnableFeature) {
    auto effect = TechEffect::EnableFeature("wall_building", "Enables wall construction");

    EXPECT_EQ(TechEffectType::EnableFeature, effect.type);
    EXPECT_EQ("wall_building", effect.stringValue);
}

TEST(TechEffectTest, ReduceCost) {
    auto effect = TechEffect::ReduceCost("unit_training", 0.2f, "-20% training cost");

    EXPECT_EQ(TechEffectType::ReduceCost, effect.type);
    EXPECT_FLOAT_EQ(0.2f, effect.value);
}

// =============================================================================
// Tech Node Tests
// =============================================================================

TEST(TechNodeTest, DefaultConstruction) {
    TechNode node;

    EXPECT_TRUE(node.id.empty());
    EXPECT_TRUE(node.name.empty());
    EXPECT_EQ(TechCategory::Military, node.category);
    EXPECT_EQ(Age::Stone, node.requiredAge);
    EXPECT_TRUE(node.prerequisites.empty());
    EXPECT_FLOAT_EQ(30.0f, node.researchTime);
    EXPECT_FLOAT_EQ(0.3f, node.lossChanceOnDeath);
    EXPECT_TRUE(node.canBeLost);
}

TEST(TechNodeTest, IsAvailableTo_Universal) {
    TechNode node;
    node.isUniversal = true;

    EXPECT_TRUE(node.IsAvailableTo(CultureType::Fortress));
    EXPECT_TRUE(node.IsAvailableTo(CultureType::Nomad));
    EXPECT_TRUE(node.IsAvailableTo(CultureType::Merchant));
}

TEST(TechNodeTest, IsAvailableTo_Restricted) {
    TechNode node;
    node.isUniversal = false;
    node.availableToCultures = {CultureType::Fortress, CultureType::Industrial};

    EXPECT_TRUE(node.IsAvailableTo(CultureType::Fortress));
    EXPECT_TRUE(node.IsAvailableTo(CultureType::Industrial));
    EXPECT_FALSE(node.IsAvailableTo(CultureType::Nomad));
}

TEST(TechNodeTest, IsAvailableTo_EmptyList) {
    TechNode node;
    node.isUniversal = false;
    node.availableToCultures.clear();

    // Empty list means available to all
    EXPECT_TRUE(node.IsAvailableTo(CultureType::Fortress));
}

TEST(TechNodeTest, GetTotalCostValue) {
    TechNode node;
    node.cost[ResourceType::Food] = 100;
    node.cost[ResourceType::Wood] = 50;
    node.cost[ResourceType::Gold] = 25;

    EXPECT_EQ(175, node.GetTotalCostValue());
}

// =============================================================================
// Research Progress Tests
// =============================================================================

TEST(ResearchProgressTest, DefaultConstruction) {
    ResearchProgress progress;

    EXPECT_TRUE(progress.techId.empty());
    EXPECT_EQ(TechStatus::Locked, progress.status);
    EXPECT_FLOAT_EQ(0.0f, progress.progressTime);
    EXPECT_FLOAT_EQ(0.0f, progress.totalTime);
    EXPECT_EQ(0, progress.timesResearched);
    EXPECT_EQ(0, progress.timesLost);
}

TEST(ResearchProgressTest, GetProgressPercent_Zero) {
    ResearchProgress progress;
    progress.progressTime = 0.0f;
    progress.totalTime = 30.0f;

    EXPECT_FLOAT_EQ(0.0f, progress.GetProgressPercent());
}

TEST(ResearchProgressTest, GetProgressPercent_HalfWay) {
    ResearchProgress progress;
    progress.progressTime = 15.0f;
    progress.totalTime = 30.0f;

    EXPECT_FLOAT_EQ(0.5f, progress.GetProgressPercent());
}

TEST(ResearchProgressTest, GetProgressPercent_Complete) {
    ResearchProgress progress;
    progress.progressTime = 30.0f;
    progress.totalTime = 30.0f;

    EXPECT_FLOAT_EQ(1.0f, progress.GetProgressPercent());
}

TEST(ResearchProgressTest, GetProgressPercent_Overflow) {
    ResearchProgress progress;
    progress.progressTime = 50.0f;
    progress.totalTime = 30.0f;

    // Should cap at 1.0
    EXPECT_FLOAT_EQ(1.0f, progress.GetProgressPercent());
}

TEST(ResearchProgressTest, GetProgressPercent_ZeroTotal) {
    ResearchProgress progress;
    progress.totalTime = 0.0f;

    EXPECT_FLOAT_EQ(0.0f, progress.GetProgressPercent());
}

TEST(ResearchProgressTest, GetRemainingTime) {
    ResearchProgress progress;
    progress.progressTime = 10.0f;
    progress.totalTime = 30.0f;

    EXPECT_FLOAT_EQ(20.0f, progress.GetRemainingTime());
}

TEST(ResearchProgressTest, GetRemainingTime_Complete) {
    ResearchProgress progress;
    progress.progressTime = 30.0f;
    progress.totalTime = 30.0f;

    EXPECT_FLOAT_EQ(0.0f, progress.GetRemainingTime());
}

// =============================================================================
// Age Requirements Tests
// =============================================================================

TEST(AgeRequirementsTest, DefaultConstruction) {
    AgeRequirements req;

    EXPECT_EQ(Age::Stone, req.age);
    EXPECT_TRUE(req.resourceCost.empty());
    EXPECT_TRUE(req.requiredTechs.empty());
    EXPECT_FLOAT_EQ(60.0f, req.researchTime);
    EXPECT_EQ(0, req.requiredBuildings);
    EXPECT_EQ(0, req.requiredPopulation);
}

// =============================================================================
// Tech Tree Tests
// =============================================================================

class TechTreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        tree = std::make_unique<TechTree>();
        ASSERT_TRUE(tree->Initialize(CultureType::Fortress, "test_player"));
    }

    void TearDown() override {
        tree->Shutdown();
    }

    std::unique_ptr<TechTree> tree;
};

TEST_F(TechTreeTest, Initialize) {
    TechTree t;
    EXPECT_FALSE(t.IsInitialized());

    bool result = t.Initialize(CultureType::Fortress);
    EXPECT_TRUE(result);
    EXPECT_TRUE(t.IsInitialized());

    t.Shutdown();
    EXPECT_FALSE(t.IsInitialized());
}

TEST_F(TechTreeTest, GetCulture) {
    EXPECT_EQ(CultureType::Fortress, tree->GetCulture());
}

TEST_F(TechTreeTest, SetCulture) {
    tree->SetCulture(CultureType::Nomad);
    EXPECT_EQ(CultureType::Nomad, tree->GetCulture());
}

TEST_F(TechTreeTest, GetCurrentAge_Initial) {
    EXPECT_EQ(Age::Stone, tree->GetCurrentAge());
}

// =============================================================================
// Tech Node Access Tests
// =============================================================================

TEST_F(TechTreeTest, GetAllTechs) {
    const auto& techs = tree->GetAllTechs();
    EXPECT_FALSE(techs.empty());
}

TEST_F(TechTreeTest, GetTech_Exists) {
    using namespace UniversalTechs;
    const TechNode* node = tree->GetTech(PRIMITIVE_TOOLS);

    ASSERT_NE(nullptr, node);
    EXPECT_EQ(PRIMITIVE_TOOLS, node->id);
}

TEST_F(TechTreeTest, GetTech_NotFound) {
    const TechNode* node = tree->GetTech("nonexistent_tech");
    EXPECT_EQ(nullptr, node);
}

TEST_F(TechTreeTest, GetAvailableTechs) {
    auto techs = tree->GetAvailableTechs();
    // Should have at least some Stone Age techs available
    EXPECT_FALSE(techs.empty());
}

TEST_F(TechTreeTest, GetTechsForAge_Stone) {
    auto techs = tree->GetTechsForAge(Age::Stone);
    EXPECT_FALSE(techs.empty());

    for (const auto* tech : techs) {
        EXPECT_EQ(Age::Stone, tech->requiredAge);
    }
}

TEST_F(TechTreeTest, GetTechsForAge_Future) {
    auto techs = tree->GetTechsForAge(Age::Future);
    // Future techs exist
    for (const auto* tech : techs) {
        EXPECT_EQ(Age::Future, tech->requiredAge);
    }
}

TEST_F(TechTreeTest, GetTechsByCategory_Military) {
    auto techs = tree->GetTechsByCategory(TechCategory::Military);

    for (const auto* tech : techs) {
        EXPECT_EQ(TechCategory::Military, tech->category);
    }
}

// =============================================================================
// Research Status Tests
// =============================================================================

TEST_F(TechTreeTest, HasTech_NotResearched) {
    using namespace UniversalTechs;
    EXPECT_FALSE(tree->HasTech(BRONZE_WORKING));
}

TEST_F(TechTreeTest, HasTech_Researched) {
    using namespace UniversalTechs;
    tree->GrantTech(PRIMITIVE_TOOLS);

    EXPECT_TRUE(tree->HasTech(PRIMITIVE_TOOLS));
}

TEST_F(TechTreeTest, GetTechStatus_Locked) {
    using namespace UniversalTechs;
    // Bronze working requires prerequisites
    TechStatus status = tree->GetTechStatus(BRONZE_WORKING);

    // May be Available or Locked depending on prerequisites
}

TEST_F(TechTreeTest, GetTechStatus_Available) {
    using namespace UniversalTechs;
    // Primitive tools should be available in Stone Age
    TechStatus status = tree->GetTechStatus(PRIMITIVE_TOOLS);

    EXPECT_EQ(TechStatus::Available, status);
}

TEST_F(TechTreeTest, GetTechStatus_Completed) {
    using namespace UniversalTechs;
    tree->GrantTech(PRIMITIVE_TOOLS);

    TechStatus status = tree->GetTechStatus(PRIMITIVE_TOOLS);
    EXPECT_EQ(TechStatus::Completed, status);
}

TEST_F(TechTreeTest, GetMissingPrerequisites) {
    using namespace UniversalTechs;
    auto missing = tree->GetMissingPrerequisites(BRONZE_WORKING);

    // Should list prerequisite techs not yet researched
    // Depends on tech tree definition
}

// =============================================================================
// Research Action Tests
// =============================================================================

TEST_F(TechTreeTest, CanResearch_Available) {
    using namespace UniversalTechs;
    EXPECT_TRUE(tree->CanResearch(PRIMITIVE_TOOLS));
}

TEST_F(TechTreeTest, CanResearch_AlreadyResearched) {
    using namespace UniversalTechs;
    tree->GrantTech(PRIMITIVE_TOOLS);

    EXPECT_FALSE(tree->CanResearch(PRIMITIVE_TOOLS));
}

TEST_F(TechTreeTest, CanResearch_MissingPrerequisites) {
    using namespace UniversalTechs;
    // Iron working requires bronze working
    EXPECT_FALSE(tree->CanResearch(IRON_WORKING));
}

TEST_F(TechTreeTest, StartResearch) {
    using namespace UniversalTechs;

    bool result = tree->StartResearch(PRIMITIVE_TOOLS);
    EXPECT_TRUE(result);
    EXPECT_TRUE(tree->IsResearching());
    EXPECT_EQ(PRIMITIVE_TOOLS, tree->GetCurrentResearch());
}

TEST_F(TechTreeTest, StartResearch_AlreadyResearching) {
    using namespace UniversalTechs;

    tree->StartResearch(PRIMITIVE_TOOLS);
    bool result = tree->StartResearch(BASIC_SHELTER);

    // May fail or replace depending on implementation
}

TEST_F(TechTreeTest, UpdateResearch) {
    using namespace UniversalTechs;

    tree->StartResearch(PRIMITIVE_TOOLS);
    const TechNode* node = tree->GetTech(PRIMITIVE_TOOLS);
    float researchTime = node ? node->researchTime : 30.0f;

    // Simulate research progress
    for (int i = 0; i < (int)(researchTime / 0.016f) + 100; ++i) {
        tree->UpdateResearch(0.016f);
    }

    // Research should be complete
    EXPECT_TRUE(tree->HasTech(PRIMITIVE_TOOLS));
    EXPECT_FALSE(tree->IsResearching());
}

TEST_F(TechTreeTest, CompleteResearch) {
    using namespace UniversalTechs;

    tree->StartResearch(PRIMITIVE_TOOLS);
    tree->CompleteResearch();

    EXPECT_TRUE(tree->HasTech(PRIMITIVE_TOOLS));
    EXPECT_FALSE(tree->IsResearching());
}

TEST_F(TechTreeTest, CancelResearch) {
    using namespace UniversalTechs;

    tree->StartResearch(PRIMITIVE_TOOLS);
    auto refund = tree->CancelResearch(0.5f);

    EXPECT_FALSE(tree->IsResearching());
    EXPECT_FALSE(tree->HasTech(PRIMITIVE_TOOLS));
    // Should have some resources refunded
}

TEST_F(TechTreeTest, GrantTech) {
    using namespace UniversalTechs;

    tree->GrantTech(BRONZE_WORKING);

    EXPECT_TRUE(tree->HasTech(BRONZE_WORKING));
}

TEST_F(TechTreeTest, LoseTech) {
    using namespace UniversalTechs;

    tree->GrantTech(BRONZE_WORKING);
    EXPECT_TRUE(tree->HasTech(BRONZE_WORKING));

    bool lost = tree->LoseTech(BRONZE_WORKING);
    EXPECT_TRUE(lost);
    EXPECT_FALSE(tree->HasTech(BRONZE_WORKING));
    EXPECT_EQ(TechStatus::Lost, tree->GetTechStatus(BRONZE_WORKING));
}

TEST_F(TechTreeTest, LoseTech_PermanentTech) {
    // Some techs cannot be lost
    TechNode node;
    node.id = "permanent_tech";
    node.canBeLost = false;

    // If tech is permanent, LoseTech should return false
}

// =============================================================================
// Research Queue Tests
// =============================================================================

TEST_F(TechTreeTest, QueueResearch) {
    using namespace UniversalTechs;

    // Start one research
    tree->StartResearch(PRIMITIVE_TOOLS);

    // Queue another
    bool queued = tree->QueueResearch(BASIC_SHELTER);
    EXPECT_TRUE(queued);

    const auto& queue = tree->GetResearchQueue();
    EXPECT_FALSE(queue.empty());
}

TEST_F(TechTreeTest, DequeueResearch) {
    using namespace UniversalTechs;

    tree->QueueResearch(PRIMITIVE_TOOLS);
    tree->QueueResearch(BASIC_SHELTER);

    tree->DequeueResearch(BASIC_SHELTER);

    const auto& queue = tree->GetResearchQueue();
    // BASIC_SHELTER should be removed
}

TEST_F(TechTreeTest, ClearResearchQueue) {
    using namespace UniversalTechs;

    tree->QueueResearch(PRIMITIVE_TOOLS);
    tree->QueueResearch(BASIC_SHELTER);

    tree->ClearResearchQueue();

    const auto& queue = tree->GetResearchQueue();
    EXPECT_TRUE(queue.empty());
}

// =============================================================================
// Age Advancement Tests
// =============================================================================

TEST_F(TechTreeTest, CanAdvanceAge_StoneAge) {
    // Need to meet requirements for Bronze Age
    // By default, probably can't advance immediately
}

TEST_F(TechTreeTest, GetNextAgeRequirements) {
    auto req = tree->GetNextAgeRequirements();

    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(Age::Bronze, req->age);
}

TEST_F(TechTreeTest, GetNextAgeRequirements_AtMaxAge) {
    // Advance to Future age (manually for testing)
    for (int i = 0; i < 6; ++i) {
        tree->AdvanceAge();
    }

    auto req = tree->GetNextAgeRequirements();
    EXPECT_FALSE(req.has_value());
}

TEST_F(TechTreeTest, AdvanceAge) {
    Age before = tree->GetCurrentAge();
    tree->AdvanceAge();
    Age after = tree->GetCurrentAge();

    EXPECT_EQ(static_cast<int>(before) + 1, static_cast<int>(after));
}

TEST_F(TechTreeTest, RegressToAge) {
    tree->AdvanceAge();  // Bronze
    tree->AdvanceAge();  // Iron

    tree->RegressToAge(Age::Stone);

    EXPECT_EQ(Age::Stone, tree->GetCurrentAge());
}

TEST_F(TechTreeTest, StartAgeAdvancement) {
    bool started = tree->StartAgeAdvancement();

    // May or may not succeed depending on requirements
    if (started) {
        EXPECT_TRUE(tree->IsAdvancingAge());
    }
}

TEST_F(TechTreeTest, GetAgeRequirements) {
    const auto& req = tree->GetAgeRequirements(Age::Bronze);

    EXPECT_EQ(Age::Bronze, req.age);
}

// =============================================================================
// Effect Calculation Tests
// =============================================================================

TEST_F(TechTreeTest, GetStatMultiplier_NoTechs) {
    float mult = tree->GetStatMultiplier("attack_damage");
    EXPECT_FLOAT_EQ(1.0f, mult);  // No bonus
}

TEST_F(TechTreeTest, GetStatMultiplier_WithTech) {
    using namespace UniversalTechs;
    tree->GrantTech(BRONZE_WEAPONS);

    float mult = tree->GetStatMultiplier("attack_damage");
    // Should have some bonus from bronze weapons
}

TEST_F(TechTreeTest, GetStatFlatBonus_NoTechs) {
    float bonus = tree->GetStatFlatBonus("max_health");
    EXPECT_FLOAT_EQ(0.0f, bonus);
}

TEST_F(TechTreeTest, IsBuildingUnlocked_Initial) {
    EXPECT_FALSE(tree->IsBuildingUnlocked("castle"));
}

TEST_F(TechTreeTest, IsBuildingUnlocked_AfterTech) {
    using namespace UniversalTechs;
    tree->GrantTech(CASTLE_BUILDING);

    EXPECT_TRUE(tree->IsBuildingUnlocked("castle"));
}

TEST_F(TechTreeTest, GetUnlockedBuildings) {
    using namespace UniversalTechs;
    tree->GrantTech(BASIC_WALLS);

    auto buildings = tree->GetUnlockedBuildings();
    // Should contain buildings unlocked by basic walls
}

TEST_F(TechTreeTest, GetUnlockedUnits) {
    auto units = tree->GetUnlockedUnits();
    // Returns list of units unlocked by current techs
}

TEST_F(TechTreeTest, GetUnlockedAbilities) {
    auto abilities = tree->GetUnlockedAbilities();
    // Returns list of abilities unlocked by current techs
}

// =============================================================================
// Tech Protection Tests
// =============================================================================

TEST_F(TechTreeTest, GetTechProtectionLevel_Default) {
    float protection = tree->GetTechProtectionLevel();
    EXPECT_GE(protection, 0.0f);
    EXPECT_LE(protection, 1.0f);
}

TEST_F(TechTreeTest, IsTechProtected) {
    using namespace UniversalTechs;
    // Some key techs may be protected
    bool isProtected = tree->IsTechProtected(PRIMITIVE_TOOLS);
    // Depends on implementation
}

TEST_F(TechTreeTest, AddTechProtection) {
    float before = tree->GetTechProtectionLevel();
    tree->AddTechProtection(0.2f, 60.0f);
    float after = tree->GetTechProtectionLevel();

    EXPECT_GT(after, before);

    // After duration expires, protection should decrease
    for (int i = 0; i < 4000; ++i) {
        tree->UpdateResearch(0.016f);
    }
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_F(TechTreeTest, OnResearchComplete_Callback) {
    using namespace UniversalTechs;

    bool callbackCalled = false;
    std::string completedTech;

    tree->SetOnResearchComplete([&](const std::string& techId, const TechNode& tech) {
        callbackCalled = true;
        completedTech = techId;
    });

    tree->StartResearch(PRIMITIVE_TOOLS);
    tree->CompleteResearch();

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(PRIMITIVE_TOOLS, completedTech);
}

TEST_F(TechTreeTest, OnAgeAdvance_Callback) {
    bool callbackCalled = false;
    Age newAge = Age::Stone;

    tree->SetOnAgeAdvance([&](Age age, Age prev) {
        callbackCalled = true;
        newAge = age;
    });

    tree->AdvanceAge();

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(Age::Bronze, newAge);
}

TEST_F(TechTreeTest, OnTechLost_Callback) {
    using namespace UniversalTechs;

    bool callbackCalled = false;
    std::string lostTech;

    tree->SetOnTechLost([&](const std::string& techId, const TechNode& tech) {
        callbackCalled = true;
        lostTech = techId;
    });

    tree->GrantTech(BRONZE_WEAPONS);
    tree->LoseTech(BRONZE_WEAPONS);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(BRONZE_WEAPONS, lostTech);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(TechTreeTest, GetTotalTechsResearched) {
    EXPECT_EQ(0, tree->GetTotalTechsResearched());

    using namespace UniversalTechs;
    tree->GrantTech(PRIMITIVE_TOOLS);

    EXPECT_EQ(1, tree->GetTotalTechsResearched());
}

TEST_F(TechTreeTest, GetTotalTechsLost) {
    EXPECT_EQ(0, tree->GetTotalTechsLost());

    using namespace UniversalTechs;
    tree->GrantTech(BRONZE_WEAPONS);
    tree->LoseTech(BRONZE_WEAPONS);

    EXPECT_EQ(1, tree->GetTotalTechsLost());
}

TEST_F(TechTreeTest, GetHighestAgeAchieved) {
    EXPECT_EQ(Age::Stone, tree->GetHighestAgeAchieved());

    tree->AdvanceAge();
    tree->AdvanceAge();

    EXPECT_EQ(Age::Iron, tree->GetHighestAgeAchieved());

    // Regressing doesn't change highest achieved
    tree->RegressToAge(Age::Stone);
    EXPECT_EQ(Age::Iron, tree->GetHighestAgeAchieved());
}

TEST_F(TechTreeTest, GetTotalResearchTime) {
    float time = tree->GetTotalResearchTime();
    EXPECT_FLOAT_EQ(0.0f, time);

    // After researching, time should increase
}

// =============================================================================
// Serialization Tests
// =============================================================================

TEST_F(TechTreeTest, ToJson) {
    using namespace UniversalTechs;
    tree->GrantTech(PRIMITIVE_TOOLS);
    tree->AdvanceAge();

    json j = tree->ToJson();

    EXPECT_FALSE(j.empty());
    // Should contain researched techs and current age
}

TEST_F(TechTreeTest, FromJson) {
    using namespace UniversalTechs;
    tree->GrantTech(PRIMITIVE_TOOLS);
    tree->AdvanceAge();

    json j = tree->ToJson();

    TechTree loaded;
    loaded.Initialize(CultureType::Fortress);
    loaded.FromJson(j);

    EXPECT_TRUE(loaded.HasTech(PRIMITIVE_TOOLS));
    EXPECT_EQ(Age::Bronze, loaded.GetCurrentAge());

    loaded.Shutdown();
}

// =============================================================================
// Prerequisite Chain Tests
// =============================================================================

TEST_F(TechTreeTest, PrerequisiteChain) {
    using namespace UniversalTechs;

    // Can't research iron without bronze
    EXPECT_FALSE(tree->CanResearch(IRON_WORKING));

    // Grant bronze
    tree->GrantTech(BRONZE_WORKING);

    // Advance to Iron Age
    tree->AdvanceAge();

    // Now iron should be available
    EXPECT_TRUE(tree->CanResearch(IRON_WORKING));
}

// =============================================================================
// Culture-Specific Tech Tests
// =============================================================================

TEST(CultureTechTest, FortressTechs) {
    TechTree tree;
    tree.Initialize(CultureType::Fortress);

    // Fortress culture should have access to fortress-specific techs
    using namespace FortressTechs;
    const TechNode* tech = tree.GetTech(STONE_MASONRY);
    ASSERT_NE(nullptr, tech);
    EXPECT_TRUE(tech->IsAvailableTo(CultureType::Fortress));

    tree.Shutdown();
}

TEST(CultureTechTest, NomadTechs) {
    TechTree tree;
    tree.Initialize(CultureType::Nomad);

    using namespace NomadTechs;
    const TechNode* tech = tree.GetTech(MOBILE_CAMPS);
    if (tech) {
        EXPECT_TRUE(tech->IsAvailableTo(CultureType::Nomad));
    }

    tree.Shutdown();
}

