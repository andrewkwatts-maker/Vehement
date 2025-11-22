/**
 * @file TestHelpers.hpp
 * @brief Helper functions and utilities for tests
 */

#pragma once

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/epsilon.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <random>

namespace Nova {
namespace Test {

// =============================================================================
// GLM Comparison Helpers
// =============================================================================

/**
 * @brief Check if two vec3 are approximately equal
 */
inline bool Vec3Equal(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.0001f) {
    return glm::all(glm::epsilonEqual(a, b, epsilon));
}

/**
 * @brief Check if two vec4 are approximately equal
 */
inline bool Vec4Equal(const glm::vec4& a, const glm::vec4& b, float epsilon = 0.0001f) {
    return glm::all(glm::epsilonEqual(a, b, epsilon));
}

/**
 * @brief Check if two quaternions are approximately equal
 */
inline bool QuatEqual(const glm::quat& a, const glm::quat& b, float epsilon = 0.0001f) {
    // Quaternions q and -q represent the same rotation
    float dot = glm::dot(a, b);
    return std::abs(std::abs(dot) - 1.0f) < epsilon;
}

/**
 * @brief Check if two mat4 are approximately equal
 */
inline bool Mat4Equal(const glm::mat4& a, const glm::mat4& b, float epsilon = 0.0001f) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (std::abs(a[i][j] - b[i][j]) > epsilon) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Check if a float is approximately equal to expected
 */
inline bool FloatEqual(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) < epsilon;
}

// =============================================================================
// Custom GTest Assertions
// =============================================================================

#define EXPECT_VEC3_EQ(expected, actual) \
    EXPECT_TRUE(Nova::Test::Vec3Equal(expected, actual)) \
        << "Expected: (" << expected.x << ", " << expected.y << ", " << expected.z << ")\n" \
        << "Actual:   (" << actual.x << ", " << actual.y << ", " << actual.z << ")"

#define EXPECT_VEC3_NEAR(expected, actual, epsilon) \
    EXPECT_TRUE(Nova::Test::Vec3Equal(expected, actual, epsilon)) \
        << "Expected: (" << expected.x << ", " << expected.y << ", " << expected.z << ")\n" \
        << "Actual:   (" << actual.x << ", " << actual.y << ", " << actual.z << ")\n" \
        << "Epsilon:  " << epsilon

#define EXPECT_QUAT_EQ(expected, actual) \
    EXPECT_TRUE(Nova::Test::QuatEqual(expected, actual)) \
        << "Expected: (" << expected.w << ", " << expected.x << ", " << expected.y << ", " << expected.z << ")\n" \
        << "Actual:   (" << actual.w << ", " << actual.x << ", " << actual.y << ", " << actual.z << ")"

#define EXPECT_MAT4_EQ(expected, actual) \
    EXPECT_TRUE(Nova::Test::Mat4Equal(expected, actual))

#define EXPECT_FLOAT_NEAR_EPSILON(expected, actual, epsilon) \
    EXPECT_TRUE(Nova::Test::FloatEqual(expected, actual, epsilon)) \
        << "Expected: " << expected << "\n" \
        << "Actual:   " << actual << "\n" \
        << "Epsilon:  " << epsilon

// =============================================================================
// Timing Utilities
// =============================================================================

/**
 * @brief RAII timer for measuring execution time
 */
class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& name = "")
        : m_name(name)
        , m_start(std::chrono::high_resolution_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        if (!m_name.empty()) {
            std::cout << "[TIMER] " << m_name << ": " << duration.count() << " us" << std::endl;
        }
    }

    [[nodiscard]] long long ElapsedMicroseconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - m_start).count();
    }

    [[nodiscard]] double ElapsedMilliseconds() const {
        return ElapsedMicroseconds() / 1000.0;
    }

private:
    std::string m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

/**
 * @brief Measure execution time of a function
 */
template<typename Func>
double MeasureTime(Func&& func, int iterations = 1) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / static_cast<double>(iterations);
}

// =============================================================================
// String Utilities
// =============================================================================

/**
 * @brief Generate a random string of given length
 */
inline std::string RandomString(size_t length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(gen)];
    }
    return result;
}

// =============================================================================
// Test Data Generation
// =============================================================================

/**
 * @brief Generate random vec3 within bounds
 */
inline glm::vec3 RandomVec3(float minVal = -100.0f, float maxVal = 100.0f) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(minVal, maxVal);
    return glm::vec3(dist(gen), dist(gen), dist(gen));
}

/**
 * @brief Generate random normalized quaternion
 */
inline glm::quat RandomQuat() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    glm::quat q(dist(gen), dist(gen), dist(gen), dist(gen));
    return glm::normalize(q);
}

/**
 * @brief Generate random float
 */
inline float RandomFloat(float minVal = 0.0f, float maxVal = 1.0f) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(minVal, maxVal);
    return dist(gen);
}

/**
 * @brief Generate random int
 */
inline int RandomInt(int minVal = 0, int maxVal = 100) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(minVal, maxVal);
    return dist(gen);
}

// =============================================================================
// Wait/Retry Utilities
// =============================================================================

/**
 * @brief Wait for a condition with timeout
 */
template<typename Predicate>
bool WaitForCondition(Predicate pred, int timeoutMs = 1000, int checkIntervalMs = 10) {
    auto start = std::chrono::steady_clock::now();
    while (!pred()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
    }
    return true;
}

/**
 * @brief Retry an operation until it succeeds or max retries reached
 */
template<typename Func>
bool RetryOperation(Func&& func, int maxRetries = 3, int delayMs = 100) {
    for (int i = 0; i < maxRetries; ++i) {
        if (func()) {
            return true;
        }
        if (i < maxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    }
    return false;
}

// =============================================================================
// Container Utilities
// =============================================================================

/**
 * @brief Check if vector contains an element
 */
template<typename T>
bool Contains(const std::vector<T>& vec, const T& element) {
    return std::find(vec.begin(), vec.end(), element) != vec.end();
}

/**
 * @brief Check if vectors have same elements (order-independent)
 */
template<typename T>
bool SameElements(std::vector<T> a, std::vector<T> b) {
    if (a.size() != b.size()) return false;
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    return a == b;
}

// =============================================================================
// JSON Test Helpers
// =============================================================================

/**
 * @brief Create a simple JSON string for testing
 */
inline std::string CreateTestJson(
    const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string json = "{";
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) json += ",";
        json += "\"" + key + "\":" + value;
        first = false;
    }
    json += "}";
    return json;
}

} // namespace Test
} // namespace Nova
