/**
 * @file test_rigid_body.cpp
 * @brief Comprehensive unit tests for RigidBody (CollisionBody) system
 *
 * Test categories:
 * - RigidBody creation and properties
 * - Force/impulse application
 * - Integration accuracy (gravity, velocity)
 * - Mass and inertia tensor calculations
 * - Sleeping/waking behavior
 * - Body type behaviors (Static, Kinematic, Dynamic)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "physics/PhysicsWorld.hpp"
#include "physics/CollisionShape.hpp"
#include "physics/CollisionBody.hpp"

#include "../utils/TestHelpers.hpp"
#include "../utils/Generators.hpp"

#include <cmath>
#include <numbers>

using namespace Nova;
using namespace Nova::Test;

// =============================================================================
// RigidBody Creation and Properties Tests
// =============================================================================

class RigidBodyCreationTest : public ::testing::Test {
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

TEST_F(RigidBodyCreationTest, DefaultConstruction) {
    CollisionBody body;

    EXPECT_NE(CollisionBody::INVALID_ID, body.GetId());
    EXPECT_EQ(BodyType::Static, body.GetBodyType());
    EXPECT_TRUE(body.IsEnabled());
    EXPECT_FALSE(body.IsSleeping());
    EXPECT_EQ(0, body.GetShapeCount());
}

TEST_F(RigidBodyCreationTest, ConstructWithBodyType_Dynamic) {
    CollisionBody body(BodyType::Dynamic);

    EXPECT_EQ(BodyType::Dynamic, body.GetBodyType());
    EXPECT_TRUE(body.IsDynamic());
    EXPECT_FALSE(body.IsStatic());
    EXPECT_FALSE(body.IsKinematic());
    EXPECT_GT(body.GetMass(), 0.0f);
    EXPECT_GT(body.GetInverseMass(), 0.0f);
}

TEST_F(RigidBodyCreationTest, ConstructWithBodyType_Static) {
    CollisionBody body(BodyType::Static);

    EXPECT_EQ(BodyType::Static, body.GetBodyType());
    EXPECT_TRUE(body.IsStatic());
    EXPECT_EQ(0.0f, body.GetMass());
    EXPECT_EQ(0.0f, body.GetInverseMass());
}

TEST_F(RigidBodyCreationTest, ConstructWithBodyType_Kinematic) {
    CollisionBody body(BodyType::Kinematic);

    EXPECT_EQ(BodyType::Kinematic, body.GetBodyType());
    EXPECT_TRUE(body.IsKinematic());
}

TEST_F(RigidBodyCreationTest, UniqueBodyIds) {
    std::vector<CollisionBody::BodyId> ids;
    for (int i = 0; i < 100; ++i) {
        CollisionBody body;
        EXPECT_EQ(std::find(ids.begin(), ids.end(), body.GetId()), ids.end())
            << "Body ID " << body.GetId() << " was not unique";
        ids.push_back(body.GetId());
    }
}

TEST_F(RigidBodyCreationTest, CreateBodyViaWorld_Dynamic) {
    auto* body = world->CreateBody(BodyType::Dynamic);

    ASSERT_NE(nullptr, body);
    EXPECT_EQ(BodyType::Dynamic, body->GetBodyType());
    EXPECT_EQ(1, world->GetBodyCount());
}

TEST_F(RigidBodyCreationTest, CreateBodyViaWorld_Static) {
    auto* body = world->CreateBody(BodyType::Static);

    ASSERT_NE(nullptr, body);
    EXPECT_EQ(BodyType::Static, body->GetBodyType());
}

TEST_F(RigidBodyCreationTest, CreateBodyViaWorld_Kinematic) {
    auto* body = world->CreateBody(BodyType::Kinematic);

    ASSERT_NE(nullptr, body);
    EXPECT_EQ(BodyType::Kinematic, body->GetBodyType());
}

// =============================================================================
// Transform and Position Tests
// =============================================================================

class RigidBodyTransformTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyTransformTest, DefaultPosition) {
    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetPosition());
}

TEST_F(RigidBodyTransformTest, SetPosition) {
    glm::vec3 pos(10.0f, 20.0f, 30.0f);
    body->SetPosition(pos);

    EXPECT_VEC3_EQ(pos, body->GetPosition());
}

TEST_F(RigidBodyTransformTest, DefaultRotation) {
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_QUAT_EQ(identity, body->GetRotation());
}

TEST_F(RigidBodyTransformTest, SetRotation) {
    // 90 degrees around Y axis
    glm::quat rot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    body->SetRotation(rot);

    EXPECT_QUAT_EQ(rot, body->GetRotation());
}

TEST_F(RigidBodyTransformTest, RotationIsNormalized) {
    // Set unnormalized quaternion
    glm::quat rot(2.0f, 0.0f, 0.0f, 0.0f);  // Not normalized
    body->SetRotation(rot);

    glm::quat result = body->GetRotation();
    float length = glm::length(result);
    EXPECT_NEAR(1.0f, length, 0.0001f);
}

TEST_F(RigidBodyTransformTest, GetTransformMatrix) {
    body->SetPosition(glm::vec3(5.0f, 10.0f, 15.0f));
    glm::quat rot = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    body->SetRotation(rot);

    glm::mat4 transform = body->GetTransformMatrix();

    // Check translation is in the last column
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 10.0f, 15.0f),
                     glm::vec3(transform[3]), 0.0001f);
}

TEST_F(RigidBodyTransformTest, SetPositionWakesBody) {
    body->SetSleeping(true);
    EXPECT_TRUE(body->IsSleeping());

    body->SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));

    EXPECT_FALSE(body->IsSleeping());
}

TEST_F(RigidBodyTransformTest, SetRotationWakesBody) {
    body->SetSleeping(true);
    EXPECT_TRUE(body->IsSleeping());

    body->SetRotation(glm::angleAxis(0.5f, glm::vec3(0, 1, 0)));

    EXPECT_FALSE(body->IsSleeping());
}

// =============================================================================
// Velocity Tests
// =============================================================================

class RigidBodyVelocityTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyVelocityTest, DefaultLinearVelocity) {
    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetLinearVelocity());
}

TEST_F(RigidBodyVelocityTest, SetLinearVelocity) {
    glm::vec3 vel(5.0f, 10.0f, -3.0f);
    body->SetLinearVelocity(vel);

    EXPECT_VEC3_EQ(vel, body->GetLinearVelocity());
}

TEST_F(RigidBodyVelocityTest, DefaultAngularVelocity) {
    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetAngularVelocity());
}

TEST_F(RigidBodyVelocityTest, SetAngularVelocity) {
    glm::vec3 angVel(1.0f, 0.5f, 0.25f);
    body->SetAngularVelocity(angVel);

    EXPECT_VEC3_EQ(angVel, body->GetAngularVelocity());
}

TEST_F(RigidBodyVelocityTest, StaticBody_IgnoresLinearVelocity) {
    CollisionBody staticBody(BodyType::Static);
    staticBody.SetLinearVelocity(glm::vec3(10.0f, 20.0f, 30.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f), staticBody.GetLinearVelocity());
}

TEST_F(RigidBodyVelocityTest, StaticBody_IgnoresAngularVelocity) {
    CollisionBody staticBody(BodyType::Static);
    staticBody.SetAngularVelocity(glm::vec3(1.0f, 2.0f, 3.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f), staticBody.GetAngularVelocity());
}

TEST_F(RigidBodyVelocityTest, SetVelocityWakesBody) {
    body->SetSleeping(true);
    EXPECT_TRUE(body->IsSleeping());

    body->SetLinearVelocity(glm::vec3(1.0f, 0.0f, 0.0f));

    EXPECT_FALSE(body->IsSleeping());
}

// =============================================================================
// Force and Impulse Application Tests
// =============================================================================

class RigidBodyForceTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
        body->SetMass(1.0f);  // 1 kg for easy calculations
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyForceTest, ApplyForce_AccumulatesForce) {
    body->ApplyForce(glm::vec3(10.0f, 0.0f, 0.0f));

    EXPECT_VEC3_EQ(glm::vec3(10.0f, 0.0f, 0.0f), body->GetAccumulatedForce());
}

TEST_F(RigidBodyForceTest, ApplyForce_MultipleForcesAccumulate) {
    body->ApplyForce(glm::vec3(10.0f, 0.0f, 0.0f));
    body->ApplyForce(glm::vec3(0.0f, 5.0f, 0.0f));
    body->ApplyForce(glm::vec3(-3.0f, 0.0f, 2.0f));

    EXPECT_VEC3_EQ(glm::vec3(7.0f, 5.0f, 2.0f), body->GetAccumulatedForce());
}

TEST_F(RigidBodyForceTest, ClearForces) {
    body->ApplyForce(glm::vec3(10.0f, 20.0f, 30.0f));
    body->ClearForces();

    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetAccumulatedForce());
    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetAccumulatedTorque());
}

TEST_F(RigidBodyForceTest, ApplyTorque_AccumulatesTorque) {
    body->ApplyTorque(glm::vec3(0.0f, 1.0f, 0.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f, 1.0f, 0.0f), body->GetAccumulatedTorque());
}

TEST_F(RigidBodyForceTest, ApplyImpulse_ChangesVelocityInstantly) {
    body->SetMass(2.0f);  // 2 kg
    body->ApplyImpulse(glm::vec3(10.0f, 0.0f, 0.0f));  // 10 kg*m/s

    // v = impulse / mass = 10 / 2 = 5 m/s
    EXPECT_VEC3_NEAR(glm::vec3(5.0f, 0.0f, 0.0f), body->GetLinearVelocity(), 0.001f);
}

TEST_F(RigidBodyForceTest, ApplyImpulseAtPoint_AffectsBothLinearAndAngular) {
    body->SetMass(1.0f);
    body->SetPosition(glm::vec3(0.0f));

    // Apply impulse at offset point (should create torque)
    glm::vec3 impulse(10.0f, 0.0f, 0.0f);
    glm::vec3 point(0.0f, 1.0f, 0.0f);  // 1 meter above center
    body->ApplyImpulseAtPoint(impulse, point);

    // Linear velocity should change
    EXPECT_GT(glm::length(body->GetLinearVelocity()), 0.0f);

    // Angular velocity should also change (torque = r x F)
    EXPECT_GT(glm::length(body->GetAngularVelocity()), 0.0f);
}

TEST_F(RigidBodyForceTest, ApplyForceAtPoint_CreatesTorque) {
    body->SetPosition(glm::vec3(0.0f));

    glm::vec3 force(10.0f, 0.0f, 0.0f);
    glm::vec3 point(0.0f, 1.0f, 0.0f);  // 1 meter above center
    body->ApplyForceAtPoint(force, point);

    // Force should accumulate
    EXPECT_VEC3_EQ(force, body->GetAccumulatedForce());

    // Torque = r x F = (0, 1, 0) x (10, 0, 0) = (0, 0, -10)
    glm::vec3 expectedTorque = glm::cross(point, force);
    EXPECT_VEC3_NEAR(expectedTorque, body->GetAccumulatedTorque(), 0.001f);
}

TEST_F(RigidBodyForceTest, StaticBody_IgnoresForce) {
    CollisionBody staticBody(BodyType::Static);
    staticBody.ApplyForce(glm::vec3(1000.0f, 0.0f, 0.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f), staticBody.GetAccumulatedForce());
}

TEST_F(RigidBodyForceTest, StaticBody_IgnoresImpulse) {
    CollisionBody staticBody(BodyType::Static);
    staticBody.ApplyImpulse(glm::vec3(1000.0f, 0.0f, 0.0f));

    EXPECT_VEC3_EQ(glm::vec3(0.0f), staticBody.GetLinearVelocity());
}

TEST_F(RigidBodyForceTest, ApplyForceWakesBody) {
    body->SetSleeping(true);
    EXPECT_TRUE(body->IsSleeping());

    body->ApplyForce(glm::vec3(1.0f, 0.0f, 0.0f));

    EXPECT_FALSE(body->IsSleeping());
}

// =============================================================================
// Mass Properties Tests
// =============================================================================

class RigidBodyMassTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyMassTest, DefaultMass) {
    EXPECT_EQ(1.0f, body->GetMass());
}

TEST_F(RigidBodyMassTest, SetMass) {
    body->SetMass(5.0f);

    EXPECT_EQ(5.0f, body->GetMass());
    EXPECT_NEAR(0.2f, body->GetInverseMass(), 0.0001f);
}

TEST_F(RigidBodyMassTest, SetMass_ClampedToMinimum) {
    body->SetMass(0.0f);

    // Should be clamped to minimum value
    EXPECT_GT(body->GetMass(), 0.0f);
}

TEST_F(RigidBodyMassTest, StaticBody_ZeroMass) {
    CollisionBody staticBody(BodyType::Static);

    EXPECT_EQ(0.0f, staticBody.GetMass());
    EXPECT_EQ(0.0f, staticBody.GetInverseMass());
}

TEST_F(RigidBodyMassTest, StaticBody_SetMassIgnored) {
    CollisionBody staticBody(BodyType::Static);
    staticBody.SetMass(100.0f);

    EXPECT_EQ(0.0f, staticBody.GetMass());
}

TEST_F(RigidBodyMassTest, InertiaTensorFromSphereShape) {
    body->AddShape(CollisionShape::CreateSphere(1.0f));
    body->SetMass(10.0f);

    // For a solid sphere: I = (2/5) * m * r^2
    // With m = 10, r = 1: I = 4.0
    glm::mat3 inertia = body->GetInertiaTensor();

    // Diagonal elements should be approximately equal for a sphere
    EXPECT_NEAR(inertia[0][0], inertia[1][1], 0.1f);
    EXPECT_NEAR(inertia[1][1], inertia[2][2], 0.1f);
}

TEST_F(RigidBodyMassTest, InertiaTensorFromBoxShape) {
    glm::vec3 halfExtents(1.0f, 2.0f, 3.0f);
    body->AddShape(CollisionShape::CreateBox(halfExtents));
    body->SetMass(12.0f);

    glm::mat3 inertia = body->GetInertiaTensor();

    // Inertia tensor should have different diagonal values for non-uniform box
    // Off-diagonal elements should be near zero for axis-aligned box at origin
    EXPECT_NEAR(0.0f, inertia[0][1], 0.01f);
    EXPECT_NEAR(0.0f, inertia[0][2], 0.01f);
    EXPECT_NEAR(0.0f, inertia[1][2], 0.01f);
}

TEST_F(RigidBodyMassTest, RecalculateMassPropertiesWithMultipleShapes) {
    body->AddShape(CollisionShape::CreateSphere(0.5f));
    body->AddShape(CollisionShape::CreateBox(glm::vec3(0.5f)));

    body->RecalculateMassProperties();

    EXPECT_GT(body->GetMass(), 0.0f);
    EXPECT_GT(body->GetInverseMass(), 0.0f);
}

// =============================================================================
// Damping Tests
// =============================================================================

class RigidBodyDampingTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyDampingTest, DefaultLinearDamping) {
    EXPECT_NEAR(0.01f, body->GetLinearDamping(), 0.001f);
}

TEST_F(RigidBodyDampingTest, DefaultAngularDamping) {
    EXPECT_NEAR(0.05f, body->GetAngularDamping(), 0.001f);
}

TEST_F(RigidBodyDampingTest, SetLinearDamping) {
    body->SetLinearDamping(0.5f);
    EXPECT_NEAR(0.5f, body->GetLinearDamping(), 0.001f);
}

TEST_F(RigidBodyDampingTest, SetAngularDamping) {
    body->SetAngularDamping(0.3f);
    EXPECT_NEAR(0.3f, body->GetAngularDamping(), 0.001f);
}

TEST_F(RigidBodyDampingTest, LinearDampingClampedToRange) {
    body->SetLinearDamping(-0.5f);
    EXPECT_GE(body->GetLinearDamping(), 0.0f);

    body->SetLinearDamping(2.0f);
    EXPECT_LE(body->GetLinearDamping(), 1.0f);
}

TEST_F(RigidBodyDampingTest, AngularDampingClampedToRange) {
    body->SetAngularDamping(-0.5f);
    EXPECT_GE(body->GetAngularDamping(), 0.0f);

    body->SetAngularDamping(2.0f);
    EXPECT_LE(body->GetAngularDamping(), 1.0f);
}

// =============================================================================
// Gravity Scale Tests
// =============================================================================

class RigidBodyGravityTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsWorldConfig config;
        config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);
        config.fixedTimestep = 1.0f / 60.0f;
        config.linearSleepThreshold = 0.0f;  // Disable sleeping for these tests
        config.angularSleepThreshold = 0.0f;
        world = std::make_unique<PhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<PhysicsWorld> world;
};

TEST_F(RigidBodyGravityTest, DefaultGravityScale) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    EXPECT_EQ(1.0f, body->GetGravityScale());
}

TEST_F(RigidBodyGravityTest, SetGravityScale) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetGravityScale(0.5f);

    EXPECT_EQ(0.5f, body->GetGravityScale());
}

TEST_F(RigidBodyGravityTest, ZeroGravityScale_NoGravityEffect) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetGravityScale(0.0f);
    body->SetPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    body->AddShape(CollisionShape::CreateSphere(0.5f));

    // Step simulation
    for (int i = 0; i < 60; ++i) {
        world->Step(1.0f / 60.0f);
    }

    // Body should not have fallen (no gravity effect)
    EXPECT_NEAR(10.0f, body->GetPosition().y, 0.1f);
}

TEST_F(RigidBodyGravityTest, NegativeGravityScale_FallsUpward) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetGravityScale(-1.0f);
    body->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    body->AddShape(CollisionShape::CreateSphere(0.5f));

    // Step simulation
    world->Step(1.0f / 60.0f);

    // Body should move upward
    EXPECT_GT(body->GetPosition().y, 0.0f);
}

// =============================================================================
// Integration Accuracy Tests
// =============================================================================

class RigidBodyIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsWorldConfig config;
        config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);  // Simple gravity for calculations
        config.fixedTimestep = 1.0f / 60.0f;
        config.linearSleepThreshold = 0.0f;  // Disable sleeping
        config.angularSleepThreshold = 0.0f;
        config.linearDamping = 0.0f;  // No damping for accurate tests
        world = std::make_unique<PhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<PhysicsWorld> world;
};

TEST_F(RigidBodyIntegrationTest, GravityIntegration_FreeFall) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetPosition(glm::vec3(0.0f, 100.0f, 0.0f));
    body->SetLinearDamping(0.0f);
    body->AddShape(CollisionShape::CreateSphere(0.5f));

    float initialY = body->GetPosition().y;
    float t = 1.0f;  // 1 second

    // Step for 1 second
    for (int i = 0; i < 60; ++i) {
        world->Step(1.0f / 60.0f);
    }

    // Expected: y = y0 - 0.5 * g * t^2 = 100 - 0.5 * 10 * 1 = 95
    // Allow some error due to numerical integration
    float expectedY = initialY - 0.5f * 10.0f * t * t;
    EXPECT_NEAR(expectedY, body->GetPosition().y, 1.0f);  // Within 1 meter
}

TEST_F(RigidBodyIntegrationTest, VelocityIntegration_ConstantVelocity) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetGravityScale(0.0f);  // No gravity
    body->SetLinearDamping(0.0f);  // No damping
    body->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    body->SetLinearVelocity(glm::vec3(10.0f, 0.0f, 0.0f));  // 10 m/s
    body->AddShape(CollisionShape::CreateSphere(0.5f));

    // Step for 1 second
    for (int i = 0; i < 60; ++i) {
        world->Step(1.0f / 60.0f);
    }

    // Should have moved 10 meters
    EXPECT_NEAR(10.0f, body->GetPosition().x, 0.5f);
}

TEST_F(RigidBodyIntegrationTest, AngularVelocityIntegration) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetGravityScale(0.0f);
    body->SetAngularDamping(0.0f);
    body->SetAngularVelocity(glm::vec3(0.0f, glm::radians(90.0f), 0.0f));  // 90 deg/s around Y
    body->AddShape(CollisionShape::CreateBox(glm::vec3(1.0f)));

    glm::quat initialRot = body->GetRotation();

    // Step for 1 second
    for (int i = 0; i < 60; ++i) {
        world->Step(1.0f / 60.0f);
    }

    // Should have rotated approximately 90 degrees
    glm::quat finalRot = body->GetRotation();

    // Check rotation has changed
    EXPECT_FALSE(QuatEqual(initialRot, finalRot, 0.1f));
}

TEST_F(RigidBodyIntegrationTest, ForceProducesAcceleration_Newtons2ndLaw) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetGravityScale(0.0f);
    body->SetLinearDamping(0.0f);
    body->SetMass(2.0f);  // 2 kg
    body->SetPosition(glm::vec3(0.0f));
    body->AddShape(CollisionShape::CreateSphere(0.5f));

    // Apply force for one step: F = 20 N
    // a = F/m = 20/2 = 10 m/s^2
    // After dt: v = a * dt = 10 * (1/60) = 0.167 m/s
    body->ApplyForce(glm::vec3(20.0f, 0.0f, 0.0f));
    world->Step(1.0f / 60.0f);

    float expectedVelocity = 10.0f * (1.0f / 60.0f);
    EXPECT_NEAR(expectedVelocity, body->GetLinearVelocity().x, 0.01f);
}

// =============================================================================
// Sleeping/Waking Behavior Tests
// =============================================================================

class RigidBodySleepTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsWorldConfig config;
        config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);
        config.fixedTimestep = 1.0f / 60.0f;
        config.linearSleepThreshold = 0.1f;
        config.angularSleepThreshold = 0.1f;
        config.sleepTimeThreshold = 0.5f;  // Sleep after 0.5 seconds of low motion
        world = std::make_unique<PhysicsWorld>(config);
    }

    void TearDown() override {
        world.reset();
    }

    std::unique_ptr<PhysicsWorld> world;
};

TEST_F(RigidBodySleepTest, NewBody_IsAwake) {
    auto* body = world->CreateBody(BodyType::Dynamic);

    EXPECT_FALSE(body->IsSleeping());
}

TEST_F(RigidBodySleepTest, SetSleeping) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetSleeping(true);

    EXPECT_TRUE(body->IsSleeping());
}

TEST_F(RigidBodySleepTest, WakeUp) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetSleeping(true);
    body->WakeUp();

    EXPECT_FALSE(body->IsSleeping());
}

TEST_F(RigidBodySleepTest, SleepingBody_DoesNotMove) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetPosition(glm::vec3(0.0f, 10.0f, 0.0f));
    body->AddShape(CollisionShape::CreateSphere(0.5f));
    body->SetSleeping(true);

    float initialY = body->GetPosition().y;

    // Step simulation
    world->Step(1.0f / 60.0f);

    // Body should not have moved (sleeping)
    EXPECT_EQ(initialY, body->GetPosition().y);
}

TEST_F(RigidBodySleepTest, BodyAtRest_EventuallySleeps) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    body->SetLinearVelocity(glm::vec3(0.0f));
    body->SetAngularVelocity(glm::vec3(0.0f));
    body->SetGravityScale(0.0f);  // No gravity to keep it at rest
    body->AddShape(CollisionShape::CreateSphere(0.5f));

    // Step for 1 second (more than sleep threshold)
    for (int i = 0; i < 60; ++i) {
        world->Step(1.0f / 60.0f);
    }

    EXPECT_TRUE(body->IsSleeping());
}

TEST_F(RigidBodySleepTest, ApplyForceWakesSleepingBody) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->AddShape(CollisionShape::CreateSphere(0.5f));
    body->SetSleeping(true);

    body->ApplyForce(glm::vec3(100.0f, 0.0f, 0.0f));

    EXPECT_FALSE(body->IsSleeping());
}

TEST_F(RigidBodySleepTest, ApplyImpulseWakesSleepingBody) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->AddShape(CollisionShape::CreateSphere(0.5f));
    body->SetSleeping(true);

    body->ApplyImpulse(glm::vec3(10.0f, 0.0f, 0.0f));

    EXPECT_FALSE(body->IsSleeping());
}

TEST_F(RigidBodySleepTest, ApplyTorqueWakesSleepingBody) {
    auto* body = world->CreateBody(BodyType::Dynamic);
    body->AddShape(CollisionShape::CreateSphere(0.5f));
    body->SetSleeping(true);

    body->ApplyTorque(glm::vec3(0.0f, 5.0f, 0.0f));

    EXPECT_FALSE(body->IsSleeping());
}

// =============================================================================
// Body Type Transition Tests
// =============================================================================

class RigidBodyTypeTransitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
        body->SetMass(5.0f);
        body->SetLinearVelocity(glm::vec3(10.0f, 0.0f, 0.0f));
        body->SetAngularVelocity(glm::vec3(0.0f, 1.0f, 0.0f));
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyTypeTransitionTest, DynamicToStatic_ClearsVelocity) {
    body->SetBodyType(BodyType::Static);

    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetLinearVelocity());
    EXPECT_VEC3_EQ(glm::vec3(0.0f), body->GetAngularVelocity());
}

TEST_F(RigidBodyTypeTransitionTest, DynamicToStatic_ZeroMass) {
    body->SetBodyType(BodyType::Static);

    EXPECT_EQ(0.0f, body->GetMass());
    EXPECT_EQ(0.0f, body->GetInverseMass());
}

TEST_F(RigidBodyTypeTransitionTest, StaticToDynamic_RecalculatesMass) {
    body->SetBodyType(BodyType::Static);
    body->AddShape(CollisionShape::CreateSphere(1.0f));
    body->SetBodyType(BodyType::Dynamic);

    EXPECT_GT(body->GetMass(), 0.0f);
    EXPECT_GT(body->GetInverseMass(), 0.0f);
}

TEST_F(RigidBodyTypeTransitionTest, DynamicToKinematic_PreservesVelocity) {
    glm::vec3 linVel = body->GetLinearVelocity();
    body->SetBodyType(BodyType::Kinematic);

    // Kinematic bodies can have velocity (set by user)
    EXPECT_VEC3_EQ(linVel, body->GetLinearVelocity());
}

// =============================================================================
// Collision Shape Management Tests
// =============================================================================

class RigidBodyShapeTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyShapeTest, AddShape_IncreasesCount) {
    EXPECT_EQ(0, body->GetShapeCount());

    body->AddShape(CollisionShape::CreateSphere(1.0f));
    EXPECT_EQ(1, body->GetShapeCount());

    body->AddShape(CollisionShape::CreateBox(glm::vec3(1.0f)));
    EXPECT_EQ(2, body->GetShapeCount());
}

TEST_F(RigidBodyShapeTest, AddShape_ReturnsIndex) {
    size_t index1 = body->AddShape(CollisionShape::CreateSphere(1.0f));
    size_t index2 = body->AddShape(CollisionShape::CreateBox(glm::vec3(1.0f)));

    EXPECT_EQ(0, index1);
    EXPECT_EQ(1, index2);
}

TEST_F(RigidBodyShapeTest, RemoveShape_DecreasesCount) {
    body->AddShape(CollisionShape::CreateSphere(1.0f));
    body->AddShape(CollisionShape::CreateBox(glm::vec3(1.0f)));
    EXPECT_EQ(2, body->GetShapeCount());

    body->RemoveShape(0);
    EXPECT_EQ(1, body->GetShapeCount());
}

TEST_F(RigidBodyShapeTest, ClearShapes) {
    body->AddShape(CollisionShape::CreateSphere(1.0f));
    body->AddShape(CollisionShape::CreateBox(glm::vec3(1.0f)));
    body->AddShape(CollisionShape::CreateCapsule(0.5f, 1.0f));

    body->ClearShapes();

    EXPECT_EQ(0, body->GetShapeCount());
}

TEST_F(RigidBodyShapeTest, GetShape_ReturnsCorrectShape) {
    body->AddShape(CollisionShape::CreateSphere(1.0f));
    body->AddShape(CollisionShape::CreateBox(glm::vec3(2.0f)));

    EXPECT_EQ(ShapeType::Sphere, body->GetShape(0).GetType());
    EXPECT_EQ(ShapeType::Box, body->GetShape(1).GetType());
}

TEST_F(RigidBodyShapeTest, AddShape_RecalculatesMass) {
    body->SetMass(1.0f);
    float initialMass = body->GetMass();

    body->AddShape(CollisionShape::CreateSphere(2.0f));

    // Mass should be recalculated based on shape density
    // (may differ from initial manual mass)
    EXPECT_NE(initialMass, body->GetMass());
}

// =============================================================================
// Collision Layer and Mask Tests
// =============================================================================

class RigidBodyLayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
};

TEST_F(RigidBodyLayerTest, DefaultLayer) {
    EXPECT_EQ(CollisionLayer::Default, body->GetCollisionLayer());
}

TEST_F(RigidBodyLayerTest, DefaultMask) {
    EXPECT_EQ(CollisionLayer::All, body->GetCollisionMask());
}

TEST_F(RigidBodyLayerTest, SetCollisionLayer) {
    body->SetCollisionLayer(CollisionLayer::Player);

    EXPECT_EQ(CollisionLayer::Player, body->GetCollisionLayer());
}

TEST_F(RigidBodyLayerTest, SetCollisionMask) {
    body->SetCollisionMask(CollisionLayer::Terrain | CollisionLayer::Building);

    EXPECT_EQ(CollisionLayer::Terrain | CollisionLayer::Building, body->GetCollisionMask());
}

TEST_F(RigidBodyLayerTest, ShouldCollideWith_MatchingLayers) {
    CollisionBody body1(BodyType::Dynamic);
    CollisionBody body2(BodyType::Dynamic);

    body1.SetCollisionLayer(CollisionLayer::Player);
    body1.SetCollisionMask(CollisionLayer::Enemy);

    body2.SetCollisionLayer(CollisionLayer::Enemy);
    body2.SetCollisionMask(CollisionLayer::Player);

    EXPECT_TRUE(body1.ShouldCollideWith(body2));
    EXPECT_TRUE(body2.ShouldCollideWith(body1));
}

TEST_F(RigidBodyLayerTest, ShouldCollideWith_NonMatchingLayers) {
    CollisionBody body1(BodyType::Dynamic);
    CollisionBody body2(BodyType::Dynamic);

    body1.SetCollisionLayer(CollisionLayer::Player);
    body1.SetCollisionMask(CollisionLayer::Terrain);  // Only collide with terrain

    body2.SetCollisionLayer(CollisionLayer::Enemy);
    body2.SetCollisionMask(CollisionLayer::All);

    EXPECT_FALSE(body1.ShouldCollideWith(body2));
}

// =============================================================================
// User Data Tests
// =============================================================================

class RigidBodyUserDataTest : public ::testing::Test {
protected:
    struct TestUserData {
        int value = 42;
        std::string name = "test";
    };

    void SetUp() override {
        body = std::make_unique<CollisionBody>(BodyType::Dynamic);
    }

    std::unique_ptr<CollisionBody> body;
    TestUserData userData;
};

TEST_F(RigidBodyUserDataTest, DefaultUserData_IsNull) {
    EXPECT_EQ(nullptr, body->GetUserData());
}

TEST_F(RigidBodyUserDataTest, SetUserData) {
    body->SetUserData(&userData);

    EXPECT_EQ(&userData, body->GetUserData());
}

TEST_F(RigidBodyUserDataTest, GetUserDataAs) {
    body->SetUserData(&userData);

    TestUserData* retrieved = body->GetUserDataAs<TestUserData>();
    ASSERT_NE(nullptr, retrieved);
    EXPECT_EQ(42, retrieved->value);
    EXPECT_EQ("test", retrieved->name);
}

// =============================================================================
// Contact Tracking Tests
// =============================================================================

class RigidBodyContactTest : public ::testing::Test {
protected:
    void SetUp() override {
        PhysicsWorldConfig config;
        config.gravity = glm::vec3(0.0f, -10.0f, 0.0f);
        world = std::make_unique<PhysicsWorld>(config);
    }

    std::unique_ptr<PhysicsWorld> world;
};

TEST_F(RigidBodyContactTest, NewBody_NoContacts) {
    auto* body = world->CreateBody(BodyType::Dynamic);

    EXPECT_EQ(0, body->GetContactCount());
    EXPECT_TRUE(body->GetContactBodies().empty());
}

TEST_F(RigidBodyContactTest, IsInContactWith_FalseWhenNoContact) {
    auto* body1 = world->CreateBody(BodyType::Dynamic);
    auto* body2 = world->CreateBody(BodyType::Dynamic);

    EXPECT_FALSE(body1->IsInContactWith(body2->GetId()));
}

// =============================================================================
// Property-Based Tests
// =============================================================================

TEST(RigidBodyPropertyTest, ImpulseConservesMomentum) {
    RandomGenerator rng(42);
    FloatGenerator massGen(1.0f, 100.0f);
    Vec3Generator impulseGen(-100.0f, 100.0f);

    for (int i = 0; i < 100; ++i) {
        CollisionBody body(BodyType::Dynamic);
        float mass = massGen.Generate(rng);
        body.SetMass(mass);
        body.SetLinearVelocity(glm::vec3(0.0f));

        glm::vec3 impulse = impulseGen.Generate(rng);
        body.ApplyImpulse(impulse);

        // p = m * v, impulse = delta_p
        // v = impulse / m
        glm::vec3 expectedVel = impulse / mass;
        EXPECT_VEC3_NEAR(expectedVel, body.GetLinearVelocity(), 0.001f);
    }
}

TEST(RigidBodyPropertyTest, ForceAccumulationIsAdditive) {
    RandomGenerator rng(42);
    Vec3Generator forceGen(-100.0f, 100.0f);

    for (int i = 0; i < 50; ++i) {
        CollisionBody body(BodyType::Dynamic);

        std::vector<glm::vec3> forces;
        glm::vec3 totalForce(0.0f);

        // Apply random number of forces
        int numForces = 1 + (i % 10);
        for (int j = 0; j < numForces; ++j) {
            glm::vec3 force = forceGen.Generate(rng);
            forces.push_back(force);
            totalForce += force;
            body.ApplyForce(force);
        }

        EXPECT_VEC3_NEAR(totalForce, body.GetAccumulatedForce(), 0.001f);
    }
}

TEST(RigidBodyPropertyTest, PositionChangeIsSymmetric) {
    RandomGenerator rng(42);
    Vec3Generator posGen(-1000.0f, 1000.0f);

    for (int i = 0; i < 100; ++i) {
        CollisionBody body(BodyType::Dynamic);

        glm::vec3 pos1 = posGen.Generate(rng);
        glm::vec3 pos2 = posGen.Generate(rng);

        body.SetPosition(pos1);
        EXPECT_VEC3_EQ(pos1, body.GetPosition());

        body.SetPosition(pos2);
        EXPECT_VEC3_EQ(pos2, body.GetPosition());

        body.SetPosition(pos1);
        EXPECT_VEC3_EQ(pos1, body.GetPosition());
    }
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST(RigidBodyEdgeCaseTest, VerySmallMass) {
    CollisionBody body(BodyType::Dynamic);
    body.SetMass(0.0001f);

    EXPECT_GT(body.GetMass(), 0.0f);
    EXPECT_GT(body.GetInverseMass(), 0.0f);
    EXPECT_FALSE(std::isinf(body.GetInverseMass()));
}

TEST(RigidBodyEdgeCaseTest, VeryLargeMass) {
    CollisionBody body(BodyType::Dynamic);
    body.SetMass(1e10f);

    EXPECT_GT(body.GetMass(), 0.0f);
    EXPECT_GT(body.GetInverseMass(), 0.0f);
    EXPECT_FALSE(std::isinf(body.GetInverseMass()));
}

TEST(RigidBodyEdgeCaseTest, VeryLargeVelocity) {
    CollisionBody body(BodyType::Dynamic);
    body.SetLinearVelocity(glm::vec3(1e6f, 1e6f, 1e6f));

    EXPECT_FALSE(std::isnan(body.GetLinearVelocity().x));
    EXPECT_FALSE(std::isinf(body.GetLinearVelocity().x));
}

TEST(RigidBodyEdgeCaseTest, VeryLargeForce) {
    CollisionBody body(BodyType::Dynamic);
    body.ApplyForce(glm::vec3(1e10f, 0.0f, 0.0f));

    EXPECT_FALSE(std::isnan(body.GetAccumulatedForce().x));
    EXPECT_FALSE(std::isinf(body.GetAccumulatedForce().x));
}

TEST(RigidBodyEdgeCaseTest, NormalizedRotationAfterManyUpdates) {
    CollisionBody body(BodyType::Dynamic);

    // Apply many small rotations
    for (int i = 0; i < 1000; ++i) {
        glm::quat smallRot = glm::angleAxis(0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat current = body.GetRotation();
        body.SetRotation(current * smallRot);
    }

    // Rotation should still be normalized
    float length = glm::length(body.GetRotation());
    EXPECT_NEAR(1.0f, length, 0.001f);
}
