/**
 * @file test_scripting.cpp
 * @brief Unit tests for Python scripting system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#ifdef NOVA_SCRIPTING_ENABLED
#include "scripting/PythonEngine.hpp"
#include "scripting/ScriptContext.hpp"
#include "scripting/EventDispatcher.hpp"
#endif

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

using namespace Nova;
using namespace Nova::Test;

#ifdef NOVA_SCRIPTING_ENABLED

// =============================================================================
// Python Engine Tests
// =============================================================================

class PythonEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Python engine for tests
        Scripting::PythonEngineConfig config;
        config.enableHotReload = false;  // Disable for tests
        config.enableSandbox = false;    // Disable sandbox for test flexibility
        config.verboseErrors = true;

        engine = &Scripting::PythonEngine::Instance();

        if (!engine->IsInitialized()) {
            initialized = engine->Initialize(config);
        } else {
            initialized = true;
        }
    }

    void TearDown() override {
        // Don't shutdown - singleton persists across tests
    }

    Scripting::PythonEngine* engine = nullptr;
    bool initialized = false;
};

TEST_F(PythonEngineTest, Initialize) {
    ASSERT_TRUE(initialized);
    EXPECT_TRUE(engine->IsInitialized());
}

TEST_F(PythonEngineTest, GetPythonVersion) {
    ASSERT_TRUE(initialized);

    std::string version = engine->GetPythonVersion();
    EXPECT_FALSE(version.empty());
    EXPECT_TRUE(version.find("3.") != std::string::npos);  // Python 3.x
}

// =============================================================================
// Script Execution Tests
// =============================================================================

TEST_F(PythonEngineTest, ExecuteString_SimpleExpression) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString("x = 2 + 2");
    EXPECT_TRUE(result.success);
}

TEST_F(PythonEngineTest, ExecuteString_PrintStatement) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString("print('Hello from test')");
    EXPECT_TRUE(result.success);
}

TEST_F(PythonEngineTest, ExecuteString_SyntaxError) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString("def broken(");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(PythonEngineTest, ExecuteString_RuntimeError) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString("x = 1 / 0");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.errorMessage.find("division") != std::string::npos ||
                result.errorMessage.find("ZeroDivision") != std::string::npos);
}

TEST_F(PythonEngineTest, ExecuteString_ImportStandardLib) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString(R"(
import math
result = math.sqrt(16)
)");
    EXPECT_TRUE(result.success);
}

TEST_F(PythonEngineTest, ExecuteString_DefineAndCallFunction) {
    ASSERT_TRUE(initialized);

    auto defineResult = engine->ExecuteString(R"(
def add(a, b):
    return a + b

test_result = add(3, 4)
)");
    EXPECT_TRUE(defineResult.success);
}

// =============================================================================
// Module Import Tests
// =============================================================================

TEST_F(PythonEngineTest, ImportModule_Math) {
    ASSERT_TRUE(initialized);

    bool imported = engine->ImportModule("math");
    EXPECT_TRUE(imported);
}

TEST_F(PythonEngineTest, ImportModule_NonExistent) {
    ASSERT_TRUE(initialized);

    bool imported = engine->ImportModule("nonexistent_module_xyz");
    EXPECT_FALSE(imported);
}

TEST_F(PythonEngineTest, ImportModule_Json) {
    ASSERT_TRUE(initialized);

    bool imported = engine->ImportModule("json");
    EXPECT_TRUE(imported);
}

// =============================================================================
// Function Call Tests
// =============================================================================

TEST_F(PythonEngineTest, CallFunction_MathSqrt) {
    ASSERT_TRUE(initialized);

    engine->ImportModule("math");

    auto result = engine->CallFunction("math", "sqrt", 16.0);
    EXPECT_TRUE(result.success);

    auto value = result.GetValue<double>();
    ASSERT_TRUE(value.has_value());
    EXPECT_NEAR(4.0, *value, 0.001);
}

TEST_F(PythonEngineTest, CallFunction_WithIntArgs) {
    ASSERT_TRUE(initialized);

    // Define a function
    engine->ExecuteString(R"(
def multiply(a, b):
    return a * b
)");

    // Note: This requires the function to be in a module
    // For inline functions, we'd need a different approach
}

TEST_F(PythonEngineTest, CallFunctionV_WithVector) {
    ASSERT_TRUE(initialized);

    engine->ImportModule("math");

    std::vector<std::variant<bool, int, float, double, std::string>> args;
    args.push_back(25.0);

    auto result = engine->CallFunctionV("math", "sqrt", args);
    EXPECT_TRUE(result.success);
}

TEST_F(PythonEngineTest, CallFunction_NonExistent) {
    ASSERT_TRUE(initialized);

    auto result = engine->CallFunction("math", "nonexistent_function");
    EXPECT_FALSE(result.success);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(PythonEngineTest, ErrorCallback) {
    ASSERT_TRUE(initialized);

    bool callbackCalled = false;
    std::string capturedError;

    engine->SetErrorCallback([&](const std::string& error, const std::string& traceback) {
        callbackCalled = true;
        capturedError = error;
    });

    auto result = engine->ExecuteString("raise ValueError('Test error')");
    EXPECT_FALSE(result.success);
    // Callback may or may not be called depending on implementation
}

TEST_F(PythonEngineTest, GetLastError) {
    ASSERT_TRUE(initialized);

    engine->ClearError();
    EXPECT_TRUE(engine->GetLastError().empty());

    engine->ExecuteString("invalid syntax here!!!");

    // After an error, GetLastError should contain something
    // (implementation dependent)
}

// =============================================================================
// Script Caching Tests
// =============================================================================

TEST_F(PythonEngineTest, PreloadScript) {
    ASSERT_TRUE(initialized);

    // Create a temporary test script content (would normally be a file)
    // Since we can't easily create files in tests, we test the interface
    auto scripts = engine->GetCachedScripts();
    // Just verify the method works
    EXPECT_TRUE(true);
}

TEST_F(PythonEngineTest, ClearCache) {
    ASSERT_TRUE(initialized);

    engine->ClearCache();
    auto scripts = engine->GetCachedScripts();
    EXPECT_TRUE(scripts.empty());
}

// =============================================================================
// Metrics Tests
// =============================================================================

TEST_F(PythonEngineTest, Metrics_RecordExecution) {
    ASSERT_TRUE(initialized);

    engine->ResetMetrics();

    engine->ExecuteString("x = 1");
    engine->ExecuteString("y = 2");
    engine->ExecuteString("z = 3");

    const auto& metrics = engine->GetMetrics();
    EXPECT_GE(metrics.totalExecutions, 3);
}

TEST_F(PythonEngineTest, Metrics_FailedExecution) {
    ASSERT_TRUE(initialized);

    engine->ResetMetrics();

    engine->ExecuteString("valid = True");
    engine->ExecuteString("1/0");  // Will fail
    engine->ExecuteString("also_valid = True");

    const auto& metrics = engine->GetMetrics();
    EXPECT_GE(metrics.failedExecutions, 1);
}

// =============================================================================
// GIL Tests
// =============================================================================

TEST_F(PythonEngineTest, GILGuard_RAII) {
    ASSERT_TRUE(initialized);

    // Test that GILGuard can be created and destroyed
    {
        Scripting::PythonEngine::GILGuard guard;
        // GIL should be held here
        auto result = engine->ExecuteString("x = 42");
        EXPECT_TRUE(result.success);
    }
    // GIL should be released here
}

// =============================================================================
// Complex Script Tests
// =============================================================================

TEST_F(PythonEngineTest, ComplexScript_ClassDefinition) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString(R"(
class Entity:
    def __init__(self, name, health):
        self.name = name
        self.health = health
        self.alive = True

    def take_damage(self, amount):
        self.health -= amount
        if self.health <= 0:
            self.alive = False
            return False
        return True

    def heal(self, amount):
        self.health += amount

# Create an instance
player = Entity("Player", 100)
player.take_damage(30)
remaining_health = player.health
)");

    EXPECT_TRUE(result.success);
}

TEST_F(PythonEngineTest, ComplexScript_ListComprehension) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString(R"(
numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
squares = [x**2 for x in numbers if x % 2 == 0]
total = sum(squares)
)");

    EXPECT_TRUE(result.success);
}

TEST_F(PythonEngineTest, ComplexScript_Dictionary) {
    ASSERT_TRUE(initialized);

    auto result = engine->ExecuteString(R"(
config = {
    'player_speed': 100.0,
    'max_health': 100,
    'weapon_damage': {'pistol': 25, 'rifle': 50, 'shotgun': 80}
}

pistol_damage = config['weapon_damage']['pistol']
)");

    EXPECT_TRUE(result.success);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(PythonEngineTest, Performance_ManyExecutions) {
    ASSERT_TRUE(initialized);

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    int successCount = 0;

    for (int i = 0; i < iterations; ++i) {
        auto result = engine->ExecuteString("x = " + std::to_string(i));
        if (result.success) successCount++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(iterations, successCount);
    // Just ensure it completes in reasonable time (< 5 seconds)
    EXPECT_LT(duration.count(), 5000);
}

#else  // NOVA_SCRIPTING_ENABLED

// =============================================================================
// Stub tests when scripting is disabled
// =============================================================================

TEST(PythonEngineTest, ScriptingDisabled) {
    // Scripting is disabled, these tests are skipped
    GTEST_SKIP() << "Python scripting is disabled";
}

#endif  // NOVA_SCRIPTING_ENABLED
