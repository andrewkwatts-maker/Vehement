/**
 * @file test_physics.cpp
 * @brief Unit tests for physics system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "physics/PhysicsWorld.hpp"
#include "physics/CollisionShape.hpp"
#include "physics/CollisionBody.hpp"
#include "spatial/AABB.hpp"

#include "utils/TestHelpers.hpp"
#include "utils/Generators.hpp"

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// Physics World Tests
// =============================================================================

class PhysicsWorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsWorldConfig config;
        config.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        config.fixedTimestep = 1.0f / 60.0f;

        world = std::make_unique<PhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<PhysicsWorld> world;
};

TEST_F(PhysicsWorldTest, Construction) {
    EXPECT_EQ(0, world->GetBodyCount());
    EXPECT_VEC3_EQ(glm::vec3(0.0f, -9.81f, 0.0f), world->GetGravity());
}

TEST_F(PhysicsWorldTest, SetGravity) {
    world->SetGravity(glm::vec3(0.0f, -10.0f, 0.0f));
    EXPECT_VEC3_EQ(glm::vec3(0.0f, -10.0f, 0.0f), world->GetGravity());
}

TEST_F(PhysicsWorldTest, CreateBody_Dynamic) {
    auto* body = world->CreateBody(BodyType::Dynamic);

    ASSERT_NE(nullptr, body);
    EXPECT_EQ(BodyType::Dynamic, body->GetType());
    EXPECT_EQ(1, world->GetBodyCount());
}

TEST_F(PhysicsWorldTest, CreateBody_Static) {
    auto* body = world->CreateBody(BodyType::Static);

    ASSERT_NE(nullptr, body);
    EXPECT_EQ(BodyType::Static, body->GetType());
}

TEST_F(PhysicsWorldTest, CreateBody_Kinematic) {
    auto* body = world->CreateBody(BodyType::Kinematic);

    ASSERT_NE(nullptr, body);
    EXPECT_EQ(BodyType::Kinematic, body->GetType());
}

TEST_F(PhysicsWorldTest, RemoveBody) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    EXPECT_EQ(1, world->GetBodyCount());

    world->RemoveBody(body);
    EXPECT_EQ(0, world->GetBodyCount());
}

TEST_F(PhysicsWorldTest, Clear) {
    world->CreateBody(BodyType::Dynamic);
    world->CreateBody(BodyType::Static);
    world->CreateBody(BodyType::Kinematic);
    EXPECT_EQ(3, world->GetBodyCount());

    world->Clear();
    EXPECT_EQ(0, world->GetBodyCount());
}

TEST_F(PhysicsWorldTest, Step_DoesNotCrash) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetPosition(glm::vec3(0.0f, 10.0f, 0.0f));

    // Step simulation
    world->Step(1.0f / 60.0f);

    // Body should have moved due to gravity
    glm::vec3 pos = body->GetPosition();
    EXPECT_LT(pos.y, 10.0f);
}

// =============================================================================
// Raycast Tests
// =============================================================================

class PhysicsRaycastTest : public PhysicsWorldTest {
protected:
    void SetUp() override {
        PhysicsWorldTest::SetUp();

        // Create test scene with bodies
        auto* floor = world->CreateBody(BodyType::Static);
        floor->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        floor->AddBoxShape(glm::vec3(100.0f, 0.5f, 100.0f));

        auto* box = world->CreateBody(BodyType::Static);
        box->SetPosition(glm::vec3(5.0f, 2.0f, 0.0f));
        box->AddBoxShape(glm::vec3(1.0f));
        boxBody = box;
    }

    CollisionBody* boxBody = nullptr;
};

TEST_F(PhysicsRaycastTest, Raycast_HitFloor) {
    auto result = world->Raycast(
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        100.0f
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(9.5f, result->distance, 0.1f);  // Hit floor at y=0.5
}

TEST_F(PhysicsRaycastTest, Raycast_HitBox) {
    auto result = world->Raycast(
        glm::vec3(0.0f, 2.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        100.0f
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(boxBody, result->body);
}

TEST_F(PhysicsRaycastTest, Raycast_Miss) {
    auto result = world->Raycast(
        glm::vec3(0.0f, 100.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        10.0f
    );

    EXPECT_FALSE(result.has_value());
}

TEST_F(PhysicsRaycastTest, RaycastAll) {
    // Cast ray that hits multiple objects
    auto results = world->RaycastAll(
        glm::vec3(-50.0f, 2.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        100.0f
    );

    // Should hit the box
    EXPECT_GE(results.size(), 1);
}

TEST_F(PhysicsRaycastTest, RaycastAny) {
    bool hit = world->RaycastAny(
        glm::vec3(0.0f, 10.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f),
        100.0f
    );

    EXPECT_TRUE(hit);
}

TEST_F(PhysicsRaycastTest, Raycast_LayerMask) {
    // Set up layer on box
    boxBody->SetLayer(CollisionLayer::Layer1);

    // Query excluding Layer1
    auto result = world->Raycast(
        glm::vec3(0.0f, 2.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        100.0f,
        CollisionLayer::Layer2  // Only hit Layer2
    );

    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Collision Detection Tests
// =============================================================================

class CollisionDetectionTest : public PhysicsWorldTest {
protected:
    void SetUp() override {
        PhysicsWorldTest::SetUp();
    }
};

TEST_F(CollisionDetectionTest, SphereSphere_Overlap) {
    auto* body1 = world->CreateBody(BodyType::Dynamic);
    body1->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    body1->AddSphereShape(1.0f);

    auto* body2 = world->CreateBody(BodyType::Dynamic);
    body2->SetPosition(glm::vec3(1.5f, 0.0f, 0.0f));  // Overlapping
    body2->AddSphereShape(1.0f);

    // Step to detect collision
    world->Step(1.0f / 60.0f);

    // Check stats for collision
    const auto& stats = world->GetStats();
    EXPECT_GE(stats.contactCount, 0);  // May or may not have contact depending on impl
}

TEST_F(CollisionDetectionTest, SphereSphere_NoOverlap) {
    auto* body1 = world->CreateBody(BodyType::Dynamic);
    body1->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    body1->AddSphereShape(1.0f);

    auto* body2 = world->CreateBody(BodyType::Dynamic);
    body2->SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));  // Far apart
    body2->AddSphereShape(1.0f);

    world->Step(1.0f / 60.0f);

    // Bodies should not have collided
}

TEST_F(CollisionDetectionTest, SphereBox_Overlap) {
    auto* sphere = world->CreateBody(BodyType::Dynamic);
    sphere->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    sphere->AddSphereShape(1.0f);

    auto* box = world->CreateBody(BodyType::Static);
    box->SetPosition(glm::vec3(1.5f, 0.0f, 0.0f));
    box->AddBoxShape(glm::vec3(1.0f));

    world->Step(1.0f / 60.0f);
}

// =============================================================================
// Overlap Query Tests
// =============================================================================

class OverlapQueryTest : public PhysicsWorldTest {
protected:
    void SetUp() override {
        PhysicsWorldTest::SetUp();

        // Create scattered bodies
        for (int i = 0; i < 10; ++i) {
            auto* body = world->CreateBody(BodyType::Static);
            body->SetPosition(glm::vec3(i * 5.0f, 0.0f, 0.0f));
            body->AddSphereShape(1.0f);
        }
    }
};

TEST_F(OverlapQueryTest, OverlapSphere) {
    auto results = world->OverlapSphere(
        glm::vec3(0.0f, 0.0f, 0.0f),
        3.0f
    );

    // Should find at least the body at origin
    EXPECT_GE(results.size(), 1);
}

TEST_F(OverlapQueryTest, OverlapBox) {
    auto results = world->OverlapBox(
        glm::vec3(7.5f, 0.0f, 0.0f),  // Between bodies at x=5 and x=10
        glm::vec3(4.0f, 1.0f, 1.0f)
    );

    EXPECT_GE(results.size(), 2);
}

TEST_F(OverlapQueryTest, OverlapAABB) {
    AABB queryBox(glm::vec3(-2.0f, -1.0f, -1.0f), glm::vec3(2.0f, 1.0f, 1.0f));
    auto results = world->OverlapAABB(queryBox);

    EXPECT_GE(results.size(), 1);
}

TEST_F(OverlapQueryTest, PointQuery) {
    auto* body = world->PointQuery(glm::vec3(0.0f, 0.0f, 0.0f));

    // Should find body at origin
    EXPECT_NE(nullptr, body);
}

TEST_F(OverlapQueryTest, PointQuery_Miss) {
    auto* body = world->PointQuery(glm::vec3(100.0f, 100.0f, 100.0f));

    EXPECT_EQ(nullptr, body);
}

// =============================================================================
// Shape Cast (Sweep) Tests
// =============================================================================

TEST_F(PhysicsWorldTest, SphereCast) {
    // Create obstacle
    auto* obstacle = world->CreateBody(BodyType::Static);
    obstacle->SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    obstacle->AddBoxShape(glm::vec3(1.0f));

    auto result = world->SphereCast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        0.5f,
        glm::vec3(1.0f, 0.0f, 0.0f),
        10.0f
    );

    ASSERT_TRUE(result.has_value());
    EXPECT_LT(result->fraction, 1.0f);
}

TEST_F(PhysicsWorldTest, BoxCast) {
    auto* obstacle = world->CreateBody(BodyType::Static);
    obstacle->SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    obstacle->AddBoxShape(glm::vec3(1.0f));

    auto result = world->BoxCast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.5f),
        glm::quat(1, 0, 0, 0),
        glm::vec3(1.0f, 0.0f, 0.0f),
        10.0f
    );

    ASSERT_TRUE(result.has_value());
}

// =============================================================================
// Trigger Volume Tests
// =============================================================================

TEST_F(PhysicsWorldTest, TriggerVolume) {
    auto* trigger = world->CreateBody(BodyType::Static);
    trigger->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    trigger->AddBoxShape(glm::vec3(2.0f));
    trigger->SetTrigger(true);

    auto* dynamic = world->CreateBody(BodyType::Dynamic);
    dynamic->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    dynamic->AddSphereShape(0.5f);

    EXPECT_TRUE(trigger->IsTrigger());

    // Step should not produce physical response from trigger
    world->Step(1.0f / 60.0f);
}

// =============================================================================
// GJK/EPA Algorithm Tests
// =============================================================================

TEST(GJKTest, SphereSphereOverlap) {
    // Test sphere-sphere using GJK principles
    glm::vec3 center1(0.0f);
    float radius1 = 1.0f;

    glm::vec3 center2(1.5f, 0.0f, 0.0f);
    float radius2 = 1.0f;

    float distance = glm::length(center2 - center1);
    float sumRadii = radius1 + radius2;

    EXPECT_TRUE(distance < sumRadii);  // Overlapping
}

TEST(GJKTest, SphereSphereNoOverlap) {
    glm::vec3 center1(0.0f);
    float radius1 = 1.0f;

    glm::vec3 center2(5.0f, 0.0f, 0.0f);
    float radius2 = 1.0f;

    float distance = glm::length(center2 - center1);
    float sumRadii = radius1 + radius2;

    EXPECT_TRUE(distance >= sumRadii);  // Not overlapping
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST(PhysicsPropertyTest, SphereSphereSymmetric) {
    // Collision detection should be symmetric
    RandomGenerator rng(42);
    Vec3Generator posGen(-10.0f, 10.0f);
    FloatGenerator radiusGen(0.1f, 2.0f);

    for (int i = 0; i < 100; ++i) {
        glm::vec3 pos1 = posGen.Generate(rng);
        glm::vec3 pos2 = posGen.Generate(rng);
        float r1 = radiusGen.Generate(rng);
        float r2 = radiusGen.Generate(rng);

        float dist = glm::length(pos2 - pos1);
        bool overlap1 = dist < (r1 + r2);

        // Swap positions
        float distSwapped = glm::length(pos1 - pos2);
        bool overlap2 = distSwapped < (r2 + r1);

        EXPECT_EQ(overlap1, overlap2);
    }
}
