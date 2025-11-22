/**
 * @file test_reflection.cpp
 * @brief Unit tests for reflection system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "reflection/Reflection.hpp"

#include "utils/TestHelpers.hpp"

#include <nlohmann/json.hpp>

using namespace Nova;
using namespace Nova::Reflect;
using namespace Nova::Test;
using json = nlohmann::json;

// =============================================================================
// Test Types
// =============================================================================

struct TestComponent {
    int intValue = 0;
    float floatValue = 0.0f;
    std::string stringValue;
    bool boolValue = false;

    // Required for reflection
    static const TypeInfo& GetStaticTypeInfo();
};

struct DerivedComponent : public TestComponent {
    float extraValue = 0.0f;

    static const TypeInfo& GetStaticTypeInfo();
};

struct ReadOnlyComponent {
    int readOnlyValue = 42;

    int GetReadOnlyValue() const { return readOnlyValue; }

    static const TypeInfo& GetStaticTypeInfo();
};

// =============================================================================
// Type Registration (done once in test setup)
// =============================================================================

class ReflectionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Register test types
        auto& registry = TypeRegistry::Instance();

        // Register TestComponent
        BuildType<TestComponent>("TestComponent")
            .Property("intValue", &TestComponent::intValue,
                PropertyMeta().WithDisplayName("Integer Value").WithRange(-100, 100))
            .Property("floatValue", &TestComponent::floatValue,
                PropertyMeta().WithDisplayName("Float Value"))
            .Property("stringValue", &TestComponent::stringValue,
                PropertyMeta().WithDisplayName("String Value"))
            .Property("boolValue", &TestComponent::boolValue);

        // Register DerivedComponent
        BuildType<DerivedComponent>("DerivedComponent")
            .Base<TestComponent>()
            .Property("extraValue", &DerivedComponent::extraValue);

        // Register ReadOnlyComponent with getter-only property
        auto& info = registry.RegisterType<ReadOnlyComponent>("ReadOnlyComponent");
        info.AddProperty(Property(
            "readOnlyValue",
            std::type_index(typeid(int)),
            [](const void* instance) -> std::any {
                return static_cast<const ReadOnlyComponent*>(instance)->GetReadOnlyValue();
            },
            nullptr,
            PropertyMeta().AsReadOnly()
        ));
    }
};

// =============================================================================
// Type Registration Tests
// =============================================================================

TEST_F(ReflectionTest, RegisterType_Basic) {
    auto& registry = TypeRegistry::Instance();

    EXPECT_TRUE(registry.IsRegistered<TestComponent>());

    auto* typeInfo = registry.GetType<TestComponent>();
    ASSERT_NE(nullptr, typeInfo);
    EXPECT_EQ("TestComponent", typeInfo->GetName());
    EXPECT_EQ(sizeof(TestComponent), typeInfo->GetSize());
}

TEST_F(ReflectionTest, GetTypeByName) {
    auto& registry = TypeRegistry::Instance();

    auto result = registry.GetType("TestComponent");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("TestComponent", (*result)->GetName());
}

TEST_F(ReflectionTest, GetTypeByName_NotFound) {
    auto& registry = TypeRegistry::Instance();

    auto result = registry.GetType("NonExistentType");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(ReflectionError::TypeNotFound, result.error());
}

TEST_F(ReflectionTest, FindType_NullOnMissing) {
    auto& registry = TypeRegistry::Instance();

    auto* typeInfo = registry.FindType("NonExistentType");
    EXPECT_EQ(nullptr, typeInfo);
}

TEST_F(ReflectionTest, GetAllTypes) {
    auto& registry = TypeRegistry::Instance();

    auto types = registry.GetAllTypes();
    EXPECT_GE(types.size(), 3);  // At least our test types

    // Check our types are in the list
    bool foundTestComponent = false;
    for (const auto* type : types) {
        if (type->GetName() == "TestComponent") {
            foundTestComponent = true;
            break;
        }
    }
    EXPECT_TRUE(foundTestComponent);
}

// =============================================================================
// Property Tests
// =============================================================================

TEST_F(ReflectionTest, GetProperty) {
    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();
    ASSERT_NE(nullptr, typeInfo);

    auto result = typeInfo->GetProperty("intValue");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("intValue", (*result)->GetName());
}

TEST_F(ReflectionTest, GetProperty_NotFound) {
    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();

    auto result = typeInfo->GetProperty("nonExistent");
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(ReflectionError::PropertyNotFound, result.error());
}

TEST_F(ReflectionTest, GetAllProperties) {
    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();

    auto properties = typeInfo->GetProperties();
    EXPECT_EQ(4, properties.size());
}

TEST_F(ReflectionTest, PropertyGetSet_Int) {
    TestComponent component;
    component.intValue = 42;

    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();
    auto* prop = typeInfo->FindProperty("intValue");
    ASSERT_NE(nullptr, prop);

    // Get value
    auto getValue = prop->Get<int>(&component);
    ASSERT_TRUE(getValue.has_value());
    EXPECT_EQ(42, *getValue);

    // Set value
    auto setResult = prop->Set<int>(&component, 100);
    EXPECT_TRUE(setResult.has_value());
    EXPECT_EQ(100, component.intValue);
}

TEST_F(ReflectionTest, PropertyGetSet_Float) {
    TestComponent component;
    component.floatValue = 3.14f;

    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();
    auto* prop = typeInfo->FindProperty("floatValue");

    auto getValue = prop->Get<float>(&component);
    ASSERT_TRUE(getValue.has_value());
    EXPECT_FLOAT_EQ(3.14f, *getValue);

    prop->Set<float>(&component, 2.71f);
    EXPECT_FLOAT_EQ(2.71f, component.floatValue);
}

TEST_F(ReflectionTest, PropertyGetSet_String) {
    TestComponent component;
    component.stringValue = "Hello";

    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();
    auto* prop = typeInfo->FindProperty("stringValue");

    auto getValue = prop->Get<std::string>(&component);
    ASSERT_TRUE(getValue.has_value());
    EXPECT_EQ("Hello", *getValue);

    prop->Set<std::string>(&component, "World");
    EXPECT_EQ("World", component.stringValue);
}

TEST_F(ReflectionTest, PropertyTypeMismatch) {
    TestComponent component;

    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();
    auto* prop = typeInfo->FindProperty("intValue");

    // Try to get as wrong type
    auto getValue = prop->Get<std::string>(&component);
    EXPECT_FALSE(getValue.has_value());
    EXPECT_EQ(ReflectionError::TypeMismatch, getValue.error());
}

TEST_F(ReflectionTest, PropertyReadOnly) {
    ReadOnlyComponent component;
    component.readOnlyValue = 100;

    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<ReadOnlyComponent>();
    auto* prop = typeInfo->FindProperty("readOnlyValue");
    ASSERT_NE(nullptr, prop);

    EXPECT_TRUE(prop->IsReadOnly());

    // Get should work
    auto getValue = prop->Get<int>(&component);
    ASSERT_TRUE(getValue.has_value());
    EXPECT_EQ(100, *getValue);

    // Set should fail
    auto setResult = prop->Set<int>(&component, 200);
    EXPECT_FALSE(setResult.has_value());
    EXPECT_EQ(ReflectionError::AccessDenied, setResult.error());
}

TEST_F(ReflectionTest, PropertyMetadata) {
    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();
    auto* prop = typeInfo->FindProperty("intValue");

    const auto& meta = prop->GetMeta();
    EXPECT_EQ("Integer Value", meta.displayName);
    EXPECT_TRUE(meta.hasRange);
    EXPECT_FLOAT_EQ(-100.0f, meta.minValue);
    EXPECT_FLOAT_EQ(100.0f, meta.maxValue);
}

// =============================================================================
// Inheritance Tests
// =============================================================================

TEST_F(ReflectionTest, DerivedType_HasBase) {
    auto& registry = TypeRegistry::Instance();
    auto* derivedInfo = registry.GetType<DerivedComponent>();
    ASSERT_NE(nullptr, derivedInfo);

    EXPECT_TRUE(derivedInfo->HasBase());
    EXPECT_EQ("TestComponent", derivedInfo->GetBaseType()->GetName());
}

TEST_F(ReflectionTest, DerivedType_InheritedProperties) {
    auto& registry = TypeRegistry::Instance();
    auto* derivedInfo = registry.GetType<DerivedComponent>();

    // GetAllProperties should include inherited properties
    auto allProps = derivedInfo->GetAllProperties();
    EXPECT_EQ(5, allProps.size());  // 4 from base + 1 from derived

    // Should be able to access base properties
    auto* intProp = derivedInfo->FindProperty("intValue");
    EXPECT_NE(nullptr, intProp);
}

TEST_F(ReflectionTest, DerivedFrom_Check) {
    auto& registry = TypeRegistry::Instance();
    auto* derivedInfo = registry.GetType<DerivedComponent>();
    auto* baseInfo = registry.GetType<TestComponent>();

    EXPECT_TRUE(derivedInfo->DerivedFrom(baseInfo));
    EXPECT_TRUE(derivedInfo->DerivedFrom(derivedInfo));  // Type derives from itself
    EXPECT_FALSE(baseInfo->DerivedFrom(derivedInfo));
}

// =============================================================================
// Factory Tests
// =============================================================================

TEST_F(ReflectionTest, CreateInstance) {
    auto& registry = TypeRegistry::Instance();
    auto* typeInfo = registry.GetType<TestComponent>();

    EXPECT_TRUE(typeInfo->HasFactory());

    auto instance = typeInfo->Create<TestComponent>();
    ASSERT_NE(nullptr, instance);

    // Should have default values
    EXPECT_EQ(0, instance->intValue);
    EXPECT_FLOAT_EQ(0.0f, instance->floatValue);
}

TEST_F(ReflectionTest, CreateInstance_ByName) {
    auto& registry = TypeRegistry::Instance();
    auto typeResult = registry.GetType("TestComponent");
    ASSERT_TRUE(typeResult.has_value());

    void* rawInstance = (*typeResult)->CreateInstance();
    ASSERT_NE(nullptr, rawInstance);

    auto* component = static_cast<TestComponent*>(rawInstance);
    EXPECT_EQ(0, component->intValue);

    delete component;  // Clean up
}

// =============================================================================
// JSON Serialization Round-Trip Tests
// =============================================================================

class ReflectionSerializationTest : public ReflectionTest {
protected:
    json SerializeComponent(const TestComponent& component) {
        auto& registry = TypeRegistry::Instance();
        auto* typeInfo = registry.GetType<TestComponent>();

        json j;
        j["__type"] = typeInfo->GetName();

        for (const auto* prop : typeInfo->GetProperties()) {
            std::any value = prop->GetAny(&component);

            if (prop->GetType() == std::type_index(typeid(int))) {
                j[prop->GetName()] = std::any_cast<int>(value);
            } else if (prop->GetType() == std::type_index(typeid(float))) {
                j[prop->GetName()] = std::any_cast<float>(value);
            } else if (prop->GetType() == std::type_index(typeid(std::string))) {
                j[prop->GetName()] = std::any_cast<std::string>(value);
            } else if (prop->GetType() == std::type_index(typeid(bool))) {
                j[prop->GetName()] = std::any_cast<bool>(value);
            }
        }

        return j;
    }

    void DeserializeComponent(TestComponent& component, const json& j) {
        auto& registry = TypeRegistry::Instance();
        auto* typeInfo = registry.GetType<TestComponent>();

        for (const auto* prop : typeInfo->GetProperties()) {
            if (!j.contains(prop->GetName())) continue;

            if (prop->GetType() == std::type_index(typeid(int))) {
                prop->SetAny(&component, std::any(j[prop->GetName()].get<int>()));
            } else if (prop->GetType() == std::type_index(typeid(float))) {
                prop->SetAny(&component, std::any(j[prop->GetName()].get<float>()));
            } else if (prop->GetType() == std::type_index(typeid(std::string))) {
                prop->SetAny(&component, std::any(j[prop->GetName()].get<std::string>()));
            } else if (prop->GetType() == std::type_index(typeid(bool))) {
                prop->SetAny(&component, std::any(j[prop->GetName()].get<bool>()));
            }
        }
    }
};

TEST_F(ReflectionSerializationTest, SerializeDeserialize_RoundTrip) {
    TestComponent original;
    original.intValue = 42;
    original.floatValue = 3.14f;
    original.stringValue = "Hello, World!";
    original.boolValue = true;

    // Serialize
    json serialized = SerializeComponent(original);

    EXPECT_EQ("TestComponent", serialized["__type"]);
    EXPECT_EQ(42, serialized["intValue"]);
    EXPECT_FLOAT_EQ(3.14f, serialized["floatValue"]);
    EXPECT_EQ("Hello, World!", serialized["stringValue"]);
    EXPECT_TRUE(serialized["boolValue"]);

    // Deserialize into new component
    TestComponent loaded;
    DeserializeComponent(loaded, serialized);

    EXPECT_EQ(original.intValue, loaded.intValue);
    EXPECT_FLOAT_EQ(original.floatValue, loaded.floatValue);
    EXPECT_EQ(original.stringValue, loaded.stringValue);
    EXPECT_EQ(original.boolValue, loaded.boolValue);
}

TEST_F(ReflectionSerializationTest, SerializeToString_AndBack) {
    TestComponent original;
    original.intValue = 100;
    original.floatValue = 2.718f;
    original.stringValue = "Test String";
    original.boolValue = false;

    json serialized = SerializeComponent(original);
    std::string jsonString = serialized.dump();

    // Parse back
    json parsed = json::parse(jsonString);

    TestComponent loaded;
    DeserializeComponent(loaded, parsed);

    EXPECT_EQ(original.intValue, loaded.intValue);
    EXPECT_EQ(original.stringValue, loaded.stringValue);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(ReflectionTest, ThreadSafety_ConcurrentRead) {
    auto& registry = TypeRegistry::Instance();

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Multiple threads reading type info
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&registry, &successCount]() {
            for (int j = 0; j < 100; ++j) {
                auto* typeInfo = registry.GetType<TestComponent>();
                if (typeInfo != nullptr) {
                    auto* prop = typeInfo->FindProperty("intValue");
                    if (prop != nullptr) {
                        successCount++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(1000, successCount);
}

// =============================================================================
// TypeBuilder Tests
// =============================================================================

TEST(TypeBuilderTest, FluentInterface) {
    struct LocalTestType {
        int a = 0;
        float b = 0.0f;
    };

    auto& info = TypeRegistry::Instance().RegisterType<LocalTestType>("LocalTestType");
    TypeBuilder<LocalTestType> builder(info);

    builder
        .Property("a", &LocalTestType::a)
        .Property("b", &LocalTestType::b);

    EXPECT_EQ(2, info.GetPropertyCount());
}
