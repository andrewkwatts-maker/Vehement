/**
 * @file TestFixtures.hpp
 * @brief Common test fixtures for Nova3D tests
 */

#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <vector>
#include <random>

// Forward declarations for engine types
namespace Nova {
    struct AABB;
    class Frustum;
    class Animation;
    class AnimationChannel;
    struct Keyframe;
}

namespace Nova {
namespace Test {

// =============================================================================
// Base Test Fixture
// =============================================================================

/**
 * @brief Base fixture that resets mock services between tests
 */
class NovaTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        MockServiceRegistry::Instance().Reset();
    }

    void TearDown() override {
        // Additional cleanup if needed
    }
};

// =============================================================================
// Spatial Test Fixture
// =============================================================================

/**
 * @brief Fixture for spatial system tests with common setup
 */
class SpatialTestFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();

        // Create test AABBs
        m_unitAABB = CreateAABB(glm::vec3(0.0f), glm::vec3(1.0f));
        m_centeredAABB = CreateAABB(glm::vec3(-1.0f), glm::vec3(1.0f));
        m_largeAABB = CreateAABB(glm::vec3(-100.0f), glm::vec3(100.0f));
    }

    // Helper to create AABB (implemented in test file with actual AABB type)
    static AABB CreateAABB(const glm::vec3& min, const glm::vec3& max);

    // Test data
    AABB m_unitAABB;
    AABB m_centeredAABB;
    AABB m_largeAABB;

    // Tolerance for floating point comparisons
    static constexpr float kEpsilon = 0.0001f;
};

// =============================================================================
// Animation Test Fixture
// =============================================================================

/**
 * @brief Fixture for animation system tests
 */
class AnimationTestFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();
    }

    // Create a simple linear animation
    static std::vector<Keyframe> CreateLinearKeyframes(
        const glm::vec3& startPos,
        const glm::vec3& endPos,
        float duration,
        int numKeyframes = 10);

    // Create rotation keyframes
    static std::vector<Keyframe> CreateRotationKeyframes(
        const glm::quat& startRot,
        const glm::quat& endRot,
        float duration,
        int numKeyframes = 10);

    // Verify keyframe interpolation
    static bool VerifyInterpolation(
        const glm::vec3& expected,
        const glm::vec3& actual,
        float tolerance = 0.001f);

    static bool VerifyQuaternion(
        const glm::quat& expected,
        const glm::quat& actual,
        float tolerance = 0.001f);
};

// =============================================================================
// Physics Test Fixture
// =============================================================================

/**
 * @brief Fixture for physics system tests
 */
class PhysicsTestFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();
    }

    // Create standard test bodies
    void CreateTestBodies();

    // Ray setup helpers
    glm::vec3 CreateRayOrigin(float x, float y, float z) {
        return glm::vec3(x, y, z);
    }

    glm::vec3 CreateRayDirection(float x, float y, float z) {
        return glm::normalize(glm::vec3(x, y, z));
    }
};

// =============================================================================
// Job System Test Fixture
// =============================================================================

/**
 * @brief Fixture for job system tests
 */
class JobSystemTestFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();
        // Job system will be initialized per-test as needed
    }

    void TearDown() override {
        NovaTestBase::TearDown();
    }

    // Helper to wait for async operations with timeout
    template<typename Predicate>
    bool WaitFor(Predicate pred, int timeoutMs = 1000) {
        auto start = std::chrono::steady_clock::now();
        while (!pred()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }
};

// =============================================================================
// Pool Test Fixture
// =============================================================================

/**
 * @brief Fixture for memory pool tests
 */
class PoolTestFixture : public NovaTestBase {
protected:
    // Test object for pool operations
    struct TestObject {
        int id = 0;
        float value = 0.0f;
        std::string name;

        TestObject() = default;
        TestObject(int i, float v, std::string n)
            : id(i), value(v), name(std::move(n)) {}

        bool operator==(const TestObject& other) const {
            return id == other.id && value == other.value && name == other.name;
        }
    };

    // Large object for testing pool with larger items
    struct LargeTestObject {
        std::array<uint8_t, 256> data{};
        int id = 0;

        LargeTestObject() = default;
        explicit LargeTestObject(int i) : id(i) {
            std::fill(data.begin(), data.end(), static_cast<uint8_t>(i & 0xFF));
        }
    };
};

// =============================================================================
// Reflection Test Fixture
// =============================================================================

/**
 * @brief Fixture for reflection system tests
 */
class ReflectionTestFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();
    }

    // Sample types for reflection testing
    struct SimpleStruct {
        int intValue = 0;
        float floatValue = 0.0f;
        std::string stringValue;
    };

    struct NestedStruct {
        SimpleStruct inner;
        std::vector<int> values;
    };
};

// =============================================================================
// Integration Test Fixture
// =============================================================================

/**
 * @brief Fixture for integration tests that require multiple systems
 */
class IntegrationTestFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();
        InitializeSystems();
    }

    void TearDown() override {
        ShutdownSystems();
        NovaTestBase::TearDown();
    }

    virtual void InitializeSystems() {
        // Override in derived fixtures to initialize specific systems
    }

    virtual void ShutdownSystems() {
        // Override to cleanup
    }

    // Simulate a game frame
    void SimulateFrame(float deltaTime) {
        m_totalTime += deltaTime;
        m_frameCount++;
    }

    float m_totalTime = 0.0f;
    int m_frameCount = 0;
};

// =============================================================================
// Benchmark Fixture
// =============================================================================

/**
 * @brief Base fixture for performance benchmarks
 */
class BenchmarkFixture : public NovaTestBase {
protected:
    void SetUp() override {
        NovaTestBase::SetUp();
        m_startTime = std::chrono::high_resolution_clock::now();
    }

    void TearDown() override {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - m_startTime).count();
        RecordTime(duration);
        NovaTestBase::TearDown();
    }

    void RecordTime(long long microseconds) {
        m_lastDuration = microseconds;
    }

    // Get operations per second
    double GetOpsPerSecond(size_t operationCount) const {
        if (m_lastDuration == 0) return 0.0;
        return (operationCount * 1000000.0) / m_lastDuration;
    }

private:
    std::chrono::high_resolution_clock::time_point m_startTime;
    long long m_lastDuration = 0;
};

} // namespace Test
} // namespace Nova
