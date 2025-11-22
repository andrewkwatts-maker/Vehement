/**
 * @file test_main.cpp
 * @brief Test entry point - Initialize test environment and global fixtures
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mocks/MockServices.hpp"
#include "utils/TestFixtures.hpp"

#include <iostream>
#include <memory>
#include <filesystem>

namespace Nova {
namespace Test {

// =============================================================================
// Global Test Environment
// =============================================================================

/**
 * @brief Global test environment for Nova3D tests
 *
 * Handles one-time setup and teardown for the entire test suite:
 * - Initialize mock services
 * - Set up test data directories
 * - Configure logging
 */
class NovaTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        std::cout << "=== Nova3D Test Suite Starting ===" << std::endl;

        // Suppress verbose logging during tests
        SetupTestLogging();

        // Initialize mock service registry
        MockServiceRegistry::Instance().Initialize();

        // Create test data directory if needed
        SetupTestDataDirectory();

        std::cout << "Test environment initialized" << std::endl;
    }

    void TearDown() override {
        std::cout << "=== Nova3D Test Suite Complete ===" << std::endl;

        // Cleanup mock services
        MockServiceRegistry::Instance().Shutdown();

        // Cleanup any test artifacts
        CleanupTestArtifacts();
    }

private:
    void SetupTestLogging() {
        // In a real implementation, configure spdlog to minimize output
        // For now, we just note that logging is configured
    }

    void SetupTestDataDirectory() {
#ifdef NOVA_TEST_DATA_DIR
        std::filesystem::path dataDir(NOVA_TEST_DATA_DIR);
        if (!std::filesystem::exists(dataDir)) {
            std::filesystem::create_directories(dataDir);
        }
#endif
    }

    void CleanupTestArtifacts() {
        // Clean up any temporary files created during tests
    }
};

// =============================================================================
// Test Event Listener for Enhanced Output
// =============================================================================

/**
 * @brief Custom test event listener for better test output
 */
class NovaTestListener : public ::testing::EmptyTestEventListener {
public:
    void OnTestStart(const ::testing::TestInfo& test_info) override {
        // Optional: print test name on start
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        if (test_info.result()->Failed()) {
            std::cout << "[  FAILED  ] " << test_info.test_suite_name() << "."
                      << test_info.name() << std::endl;
        }
    }

    void OnTestSuiteStart(const ::testing::TestSuite& test_suite) override {
        std::cout << "\n=== Test Suite: " << test_suite.name() << " ===" << std::endl;
    }

    void OnTestSuiteEnd(const ::testing::TestSuite& test_suite) override {
        std::cout << "Suite " << test_suite.name() << ": "
                  << test_suite.successful_test_count() << " passed, "
                  << test_suite.failed_test_count() << " failed" << std::endl;
    }
};

} // namespace Test
} // namespace Nova

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char** argv) {
    // Initialize GoogleTest
    ::testing::InitGoogleTest(&argc, argv);

    // Initialize GoogleMock
    ::testing::InitGoogleMock(&argc, argv);

    // Add custom environment
    ::testing::AddGlobalTestEnvironment(new Nova::Test::NovaTestEnvironment());

    // Add custom listener (optional, for enhanced output)
    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();

    // Uncomment to use custom listener:
    // listeners.Append(new Nova::Test::NovaTestListener());

    // Run all tests
    return RUN_ALL_TESTS();
}
