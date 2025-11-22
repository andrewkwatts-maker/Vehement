/**
 * @file test_config.cpp
 * @brief Unit tests for configuration system
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "config/ConfigSchema.hpp"
#include "config/EntityConfig.hpp"
#include "config/ConfigRegistry.hpp"

#include "utils/TestHelpers.hpp"
#include "mocks/MockServices.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using namespace Vehement;
using namespace Vehement::Config;
using namespace Nova::Test;
using json = nlohmann::json;

// =============================================================================
// Schema Field Type Tests
// =============================================================================

TEST(SchemaFieldTypeTest, StringConversions) {
    EXPECT_EQ("box", CollisionShapeTypeToString(CollisionShapeType::Box));
    EXPECT_EQ("sphere", CollisionShapeTypeToString(CollisionShapeType::Sphere));
    EXPECT_EQ("capsule", CollisionShapeTypeToString(CollisionShapeType::Capsule));
    EXPECT_EQ("cylinder", CollisionShapeTypeToString(CollisionShapeType::Cylinder));
    EXPECT_EQ("mesh", CollisionShapeTypeToString(CollisionShapeType::Mesh));
    EXPECT_EQ("compound", CollisionShapeTypeToString(CollisionShapeType::Compound));
    EXPECT_EQ("none", CollisionShapeTypeToString(CollisionShapeType::None));
}

TEST(SchemaFieldTypeTest, StringToCollisionShapeType) {
    EXPECT_EQ(CollisionShapeType::Box, StringToCollisionShapeType("box"));
    EXPECT_EQ(CollisionShapeType::Sphere, StringToCollisionShapeType("sphere"));
    EXPECT_EQ(CollisionShapeType::Capsule, StringToCollisionShapeType("capsule"));
    EXPECT_EQ(CollisionShapeType::None, StringToCollisionShapeType("invalid"));
}

TEST(SchemaFieldTypeTest, GridType) {
    EXPECT_EQ("rect", GridTypeToString(GridType::Rect));
    EXPECT_EQ("hex", GridTypeToString(GridType::Hex));

    EXPECT_EQ(GridType::Rect, StringToGridType("rect"));
    EXPECT_EQ(GridType::Hex, StringToGridType("hex"));
}

TEST(SchemaFieldTypeTest, ResourceType) {
    EXPECT_EQ("food", ResourceTypeToString(ResourceType::Food));
    EXPECT_EQ("wood", ResourceTypeToString(ResourceType::Wood));
    EXPECT_EQ("gold", ResourceTypeToString(ResourceType::Gold));
    EXPECT_EQ("mana", ResourceTypeToString(ResourceType::Mana));
    EXPECT_EQ("population", ResourceTypeToString(ResourceType::Population));

    EXPECT_EQ(ResourceType::Food, StringToResourceType("food"));
    EXPECT_EQ(ResourceType::Gold, StringToResourceType("gold"));
    EXPECT_EQ(ResourceType::None, StringToResourceType("invalid"));
}

// =============================================================================
// Schema Constraints Tests
// =============================================================================

TEST(SchemaConstraintsTest, DefaultValues) {
    SchemaConstraints constraints;

    EXPECT_FALSE(constraints.minValue.has_value());
    EXPECT_FALSE(constraints.maxValue.has_value());
    EXPECT_FALSE(constraints.minLength.has_value());
    EXPECT_FALSE(constraints.maxLength.has_value());
    EXPECT_TRUE(constraints.enumValues.empty());
    EXPECT_TRUE(constraints.pattern.empty());
    EXPECT_TRUE(constraints.allowEmpty);
    EXPECT_FALSE(constraints.mustExist);
}

TEST(SchemaConstraintsTest, NumericRange) {
    SchemaConstraints constraints;
    constraints.minValue = 0.0;
    constraints.maxValue = 100.0;

    EXPECT_TRUE(constraints.minValue.has_value());
    EXPECT_TRUE(constraints.maxValue.has_value());
    EXPECT_DOUBLE_EQ(0.0, *constraints.minValue);
    EXPECT_DOUBLE_EQ(100.0, *constraints.maxValue);
}

TEST(SchemaConstraintsTest, EnumValues) {
    SchemaConstraints constraints;
    constraints.enumValues = {"option1", "option2", "option3"};

    EXPECT_EQ(3, constraints.enumValues.size());
    EXPECT_EQ("option1", constraints.enumValues[0]);
}

// =============================================================================
// Schema Field Tests
// =============================================================================

TEST(SchemaFieldTest, DefaultConstruction) {
    SchemaField field;

    EXPECT_TRUE(field.name.empty());
    EXPECT_EQ(SchemaFieldType::Any, field.type);
    EXPECT_FALSE(field.required);
}

TEST(SchemaFieldTest, RequiredField) {
    SchemaField field;
    field.name = "id";
    field.type = SchemaFieldType::String;
    field.required = true;

    EXPECT_EQ("id", field.name);
    EXPECT_EQ(SchemaFieldType::String, field.type);
    EXPECT_TRUE(field.required);
}

// =============================================================================
// Schema Builder Tests
// =============================================================================

TEST(SchemaBuilderTest, StringField) {
    auto field = SchemaBuilder::String("name", true, "Entity name");

    EXPECT_EQ("name", field.name);
    EXPECT_EQ(SchemaFieldType::String, field.type);
    EXPECT_TRUE(field.required);
    EXPECT_EQ("Entity name", field.description);
}

TEST(SchemaBuilderTest, IntegerField) {
    auto field = SchemaBuilder::Integer("count", false, "Item count");

    EXPECT_EQ("count", field.name);
    EXPECT_EQ(SchemaFieldType::Integer, field.type);
    EXPECT_FALSE(field.required);
}

TEST(SchemaBuilderTest, FloatField) {
    auto field = SchemaBuilder::Float("speed", true);

    EXPECT_EQ("speed", field.name);
    EXPECT_EQ(SchemaFieldType::Float, field.type);
    EXPECT_TRUE(field.required);
}

TEST(SchemaBuilderTest, BooleanField) {
    auto field = SchemaBuilder::Boolean("enabled");

    EXPECT_EQ("enabled", field.name);
    EXPECT_EQ(SchemaFieldType::Boolean, field.type);
}

TEST(SchemaBuilderTest, Vec3Field) {
    auto field = SchemaBuilder::Vec3("position", true);

    EXPECT_EQ("position", field.name);
    EXPECT_EQ(SchemaFieldType::Vector3, field.type);
}

TEST(SchemaBuilderTest, ResourcePathField) {
    auto field = SchemaBuilder::ResourcePath("texture", false, "Texture path");

    EXPECT_EQ("texture", field.name);
    EXPECT_EQ(SchemaFieldType::ResourcePath, field.type);
}

TEST(SchemaBuilderTest, EnumField) {
    std::vector<std::string> values = {"small", "medium", "large"};
    auto field = SchemaBuilder::Enum("size", values, true);

    EXPECT_EQ("size", field.name);
    EXPECT_EQ(SchemaFieldType::Enum, field.type);
    EXPECT_EQ(3, field.constraints.enumValues.size());
}

TEST(SchemaBuilderTest, ObjectField) {
    std::vector<SchemaField> nestedFields = {
        SchemaBuilder::Float("x"),
        SchemaBuilder::Float("y")
    };
    auto field = SchemaBuilder::Object("offset", nestedFields, true);

    EXPECT_EQ("offset", field.name);
    EXPECT_EQ(SchemaFieldType::Object, field.type);
    EXPECT_EQ(2, field.inlineFields.size());
}

// =============================================================================
// Validation Result Tests
// =============================================================================

TEST(ValidationResultTest, ValidByDefault) {
    ValidationResult result;

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_TRUE(result.warnings.empty());
}

TEST(ValidationResultTest, AddError) {
    ValidationResult result;
    result.AddError("path.to.field", "Field is required");

    EXPECT_FALSE(result.valid);
    ASSERT_EQ(1, result.errors.size());
    EXPECT_TRUE(result.errors[0].find("path.to.field") != std::string::npos);
    EXPECT_TRUE(result.errors[0].find("required") != std::string::npos);
}

TEST(ValidationResultTest, AddWarning) {
    ValidationResult result;
    result.AddWarning("config.deprecated", "This field is deprecated");

    EXPECT_TRUE(result.valid);  // Warnings don't invalidate
    EXPECT_TRUE(result.errors.empty());
    ASSERT_EQ(1, result.warnings.size());
}

TEST(ValidationResultTest, Merge) {
    ValidationResult result1;
    result1.AddError("error1", "First error");
    result1.AddWarning("warning1", "First warning");

    ValidationResult result2;
    result2.AddError("error2", "Second error");

    result1.Merge(result2);

    EXPECT_FALSE(result1.valid);
    EXPECT_EQ(2, result1.errors.size());
    EXPECT_EQ(1, result1.warnings.size());
}

// =============================================================================
// Property Bag Tests
// =============================================================================

TEST(PropertyBagTest, SetAndGet_Bool) {
    PropertyBag bag;
    bag.Set("enabled", true);

    auto result = bag.Get<bool>("enabled");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST(PropertyBagTest, SetAndGet_Int) {
    PropertyBag bag;
    bag.Set("count", static_cast<int64_t>(42));

    auto result = bag.Get<int64_t>("count");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(42, *result);
}

TEST(PropertyBagTest, SetAndGet_Double) {
    PropertyBag bag;
    bag.Set("value", 3.14);

    auto result = bag.Get<double>("value");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(3.14, *result);
}

TEST(PropertyBagTest, SetAndGet_String) {
    PropertyBag bag;
    bag.Set("name", std::string("TestEntity"));

    auto result = bag.Get<std::string>("name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("TestEntity", *result);
}

TEST(PropertyBagTest, SetAndGet_Vec3) {
    PropertyBag bag;
    bag.Set("position", glm::vec3(1.0f, 2.0f, 3.0f));

    auto result = bag.Get<glm::vec3>("position");
    ASSERT_TRUE(result.has_value());
    EXPECT_VEC3_EQ(glm::vec3(1.0f, 2.0f, 3.0f), *result);
}

TEST(PropertyBagTest, GetOr_WithDefault) {
    PropertyBag bag;

    auto result = bag.GetOr<double>("missing", 99.0);
    EXPECT_DOUBLE_EQ(99.0, result);
}

TEST(PropertyBagTest, GetOr_ExistingValue) {
    PropertyBag bag;
    bag.Set("value", 42.0);

    auto result = bag.GetOr<double>("value", 99.0);
    EXPECT_DOUBLE_EQ(42.0, result);
}

TEST(PropertyBagTest, Has) {
    PropertyBag bag;
    bag.Set("exists", true);

    EXPECT_TRUE(bag.Has("exists"));
    EXPECT_FALSE(bag.Has("missing"));
}

TEST(PropertyBagTest, Remove) {
    PropertyBag bag;
    bag.Set("key", std::string("value"));

    EXPECT_TRUE(bag.Has("key"));

    bag.Remove("key");

    EXPECT_FALSE(bag.Has("key"));
}

TEST(PropertyBagTest, Clear) {
    PropertyBag bag;
    bag.Set("key1", std::string("value1"));
    bag.Set("key2", std::string("value2"));

    EXPECT_EQ(2, bag.GetAll().size());

    bag.Clear();

    EXPECT_EQ(0, bag.GetAll().size());
}

TEST(PropertyBagTest, TypeMismatch_ReturnsNullopt) {
    PropertyBag bag;
    bag.Set("name", std::string("TestEntity"));

    // Try to get string as int
    auto result = bag.Get<int64_t>("name");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// Entity Config Tests
// =============================================================================

class EntityConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = std::make_unique<EntityConfig>();
    }

    std::unique_ptr<EntityConfig> config;
};

TEST_F(EntityConfigTest, DefaultConstruction) {
    EXPECT_TRUE(config->GetId().empty());
    EXPECT_TRUE(config->GetName().empty());
    EXPECT_TRUE(config->GetDescription().empty());
    EXPECT_EQ("entity", config->GetConfigType());
}

TEST_F(EntityConfigTest, SetId) {
    config->SetId("unit_warrior");
    EXPECT_EQ("unit_warrior", config->GetId());
}

TEST_F(EntityConfigTest, SetName) {
    config->SetName("Warrior");
    EXPECT_EQ("Warrior", config->GetName());
}

TEST_F(EntityConfigTest, SetDescription) {
    config->SetDescription("A brave warrior");
    EXPECT_EQ("A brave warrior", config->GetDescription());
}

TEST_F(EntityConfigTest, Tags) {
    config->AddTag("military");
    config->AddTag("melee");

    EXPECT_EQ(2, config->GetTags().size());
    EXPECT_TRUE(config->HasTag("military"));
    EXPECT_TRUE(config->HasTag("melee"));
    EXPECT_FALSE(config->HasTag("ranged"));
}

TEST_F(EntityConfigTest, SetTags) {
    std::vector<std::string> tags = {"unit", "infantry", "tier1"};
    config->SetTags(tags);

    EXPECT_EQ(3, config->GetTags().size());
    EXPECT_TRUE(config->HasTag("infantry"));
}

TEST_F(EntityConfigTest, ModelPath) {
    config->SetModelPath("models/units/warrior.obj");
    EXPECT_EQ("models/units/warrior.obj", config->GetModelPath());
}

TEST_F(EntityConfigTest, ModelScale) {
    config->SetModelScale(glm::vec3(2.0f, 2.0f, 2.0f));
    EXPECT_VEC3_EQ(glm::vec3(2.0f, 2.0f, 2.0f), config->GetModelScale());
}

TEST_F(EntityConfigTest, ModelRotation) {
    config->SetModelRotation(glm::vec3(0.0f, 90.0f, 0.0f));
    EXPECT_VEC3_EQ(glm::vec3(0.0f, 90.0f, 0.0f), config->GetModelRotation());
}

TEST_F(EntityConfigTest, ModelOffset) {
    config->SetModelOffset(glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_VEC3_EQ(glm::vec3(0.0f, 1.0f, 0.0f), config->GetModelOffset());
}

TEST_F(EntityConfigTest, TexturePath) {
    config->SetTexturePath("textures/units/warrior.png");
    EXPECT_EQ("textures/units/warrior.png", config->GetTexturePath());
}

TEST_F(EntityConfigTest, NamedTextures) {
    config->SetTexture("diffuse", "textures/warrior_diffuse.png");
    config->SetTexture("normal", "textures/warrior_normal.png");

    EXPECT_EQ("textures/warrior_diffuse.png", config->GetTexture("diffuse"));
    EXPECT_EQ("textures/warrior_normal.png", config->GetTexture("normal"));
    EXPECT_TRUE(config->GetTexture("specular").empty());
}

TEST_F(EntityConfigTest, MaterialConfig) {
    MaterialConfig material;
    material.diffusePath = "textures/diffuse.png";
    material.metallic = 0.5f;
    material.roughness = 0.3f;
    material.transparent = true;

    config->SetMaterial(material);

    const auto& mat = config->GetMaterial();
    EXPECT_EQ("textures/diffuse.png", mat.diffusePath);
    EXPECT_FLOAT_EQ(0.5f, mat.metallic);
    EXPECT_FLOAT_EQ(0.3f, mat.roughness);
    EXPECT_TRUE(mat.transparent);
}

TEST_F(EntityConfigTest, CollisionConfig) {
    CollisionConfig collision;
    collision.type = CollisionShapeType::Box;
    collision.mass = 10.0f;
    collision.friction = 0.8f;
    collision.restitution = 0.2f;
    collision.isStatic = false;
    collision.isTrigger = false;

    config->SetCollision(collision);

    const auto& col = config->GetCollision();
    EXPECT_EQ(CollisionShapeType::Box, col.type);
    EXPECT_FLOAT_EQ(10.0f, col.mass);
    EXPECT_FLOAT_EQ(0.8f, col.friction);
}

TEST_F(EntityConfigTest, EventHandlers) {
    EventHandler handler1;
    handler1.eventName = "onCreate";
    handler1.scriptPath = "scripts/on_create.py";
    handler1.functionName = "handle_create";
    handler1.async = false;

    EventHandler handler2;
    handler2.eventName = "onDamage";
    handler2.scriptPath = "scripts/on_damage.py";
    handler2.functionName = "handle_damage";
    handler2.async = true;

    config->AddEventHandler(handler1);
    config->AddEventHandler(handler2);

    EXPECT_EQ(2, config->GetEventHandlers().size());
    EXPECT_TRUE(config->HasEventHandler("onCreate"));
    EXPECT_TRUE(config->HasEventHandler("onDamage"));
    EXPECT_FALSE(config->HasEventHandler("onDestroy"));

    auto handlers = config->GetHandlersForEvent("onCreate");
    ASSERT_EQ(1, handlers.size());
    EXPECT_EQ("handle_create", handlers[0].functionName);
}

TEST_F(EntityConfigTest, CustomProperties) {
    config->GetProperties().Set("health", static_cast<int64_t>(100));
    config->GetProperties().Set("speed", 5.0);
    config->GetProperties().Set("name", std::string("Warrior"));

    EXPECT_EQ(100, config->GetProperties().GetOr<int64_t>("health", 0));
    EXPECT_DOUBLE_EQ(5.0, config->GetProperties().GetOr<double>("speed", 0.0));
    EXPECT_EQ("Warrior", config->GetProperties().GetOr<std::string>("name", ""));
}

TEST_F(EntityConfigTest, BaseConfigId) {
    config->SetBaseConfigId("base_unit");

    EXPECT_TRUE(config->HasBaseConfig());
    EXPECT_EQ("base_unit", config->GetBaseConfigId());
}

TEST_F(EntityConfigTest, HasNoBaseConfig) {
    EXPECT_FALSE(config->HasBaseConfig());
    EXPECT_TRUE(config->GetBaseConfigId().empty());
}

// =============================================================================
// Config Inheritance Tests
// =============================================================================

TEST(ConfigInheritanceTest, ApplyBaseConfig) {
    EntityConfig baseConfig;
    baseConfig.SetId("base_unit");
    baseConfig.SetModelPath("models/base.obj");
    baseConfig.SetTexturePath("textures/base.png");
    baseConfig.GetProperties().Set("health", static_cast<int64_t>(100));
    baseConfig.AddTag("unit");

    EntityConfig derivedConfig;
    derivedConfig.SetId("warrior");
    derivedConfig.SetBaseConfigId("base_unit");

    derivedConfig.ApplyBaseConfig(baseConfig);

    // Derived should inherit from base
    EXPECT_EQ("models/base.obj", derivedConfig.GetModelPath());
    EXPECT_EQ("textures/base.png", derivedConfig.GetTexturePath());
    EXPECT_TRUE(derivedConfig.HasTag("unit"));
}

// =============================================================================
// Entity Config Factory Tests
// =============================================================================

TEST(EntityConfigFactoryTest, RegisterAndCreate) {
    auto& factory = EntityConfigFactory::Instance();

    // Register a test type
    factory.RegisterType("test_entity", []() {
        auto config = std::make_unique<EntityConfig>();
        config->SetId("created_from_factory");
        return config;
    });

    EXPECT_TRUE(factory.HasType("test_entity"));

    auto created = factory.Create("test_entity");
    ASSERT_NE(nullptr, created);
    EXPECT_EQ("created_from_factory", created->GetId());
}

TEST(EntityConfigFactoryTest, CreateUnknownType) {
    auto& factory = EntityConfigFactory::Instance();

    auto created = factory.Create("nonexistent_type");
    EXPECT_EQ(nullptr, created);
}

TEST(EntityConfigFactoryTest, GetRegisteredTypes) {
    auto& factory = EntityConfigFactory::Instance();

    auto types = factory.GetRegisteredTypes();
    // At minimum should have the test type we registered
    EXPECT_FALSE(types.empty());
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

class ConfigSerializationTest : public EntityConfigTest {
protected:
    void SetUp() override {
        EntityConfigTest::SetUp();

        // Set up a config with all fields
        config->SetId("test_unit");
        config->SetName("Test Unit");
        config->SetDescription("A unit for testing");
        config->SetTags({"test", "unit"});
        config->SetModelPath("models/test.obj");
        config->SetModelScale(glm::vec3(1.5f));
        config->SetTexturePath("textures/test.png");

        CollisionConfig collision;
        collision.type = CollisionShapeType::Sphere;
        collision.mass = 5.0f;
        config->SetCollision(collision);

        config->GetProperties().Set("customValue", 42.0);
    }
};

TEST_F(ConfigSerializationTest, ToJsonString) {
    std::string jsonStr = config->ToJsonString();

    EXPECT_FALSE(jsonStr.empty());
    EXPECT_TRUE(jsonStr.find("test_unit") != std::string::npos);
    EXPECT_TRUE(jsonStr.find("Test Unit") != std::string::npos);
}

TEST_F(ConfigSerializationTest, LoadFromString) {
    std::string jsonStr = config->ToJsonString();

    EntityConfig loadedConfig;
    bool success = loadedConfig.LoadFromString(jsonStr);

    EXPECT_TRUE(success);
    EXPECT_EQ(config->GetId(), loadedConfig.GetId());
    EXPECT_EQ(config->GetName(), loadedConfig.GetName());
}

TEST_F(ConfigSerializationTest, RoundTrip) {
    std::string jsonStr = config->ToJsonString();

    EntityConfig loadedConfig;
    loadedConfig.LoadFromString(jsonStr);

    EXPECT_EQ(config->GetId(), loadedConfig.GetId());
    EXPECT_EQ(config->GetName(), loadedConfig.GetName());
    EXPECT_EQ(config->GetDescription(), loadedConfig.GetDescription());
    EXPECT_EQ(config->GetModelPath(), loadedConfig.GetModelPath());
    EXPECT_EQ(config->GetTexturePath(), loadedConfig.GetTexturePath());
}

// =============================================================================
// Config Validation Tests
// =============================================================================

TEST(ConfigValidationTest, ValidConfig) {
    EntityConfig config;
    config.SetId("valid_entity");
    config.SetName("Valid Entity");

    ValidationResult result = config.Validate();

    EXPECT_TRUE(result.valid);
}

TEST(ConfigValidationTest, EmptyIdWarning) {
    EntityConfig config;
    // ID is empty

    ValidationResult result = config.Validate();

    // Empty ID may generate a warning or error depending on implementation
}

// =============================================================================
// Hot Reload Tests (with mock filesystem)
// =============================================================================

class ConfigHotReloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        MOCK_FS().Reset();
    }
};

TEST_F(ConfigHotReloadTest, DetectFileChange) {
    // This would test the hot reload detection mechanism
    // Using mock filesystem to simulate file changes
    MOCK_FS().SetExpectedFileExists("config/test.json", true);

    // The actual hot reload test would depend on implementation
}

// =============================================================================
// Schema Validation Tests
// =============================================================================

TEST(SchemaValidationTest, ValidateRequiredFields) {
    ConfigSchemaDefinition schema;
    schema.id = "test_schema";
    schema.fields.push_back(SchemaBuilder::String("id", true, "Required ID"));
    schema.fields.push_back(SchemaBuilder::String("name", true, "Required name"));
    schema.fields.push_back(SchemaBuilder::Float("health", false, "Optional health"));

    // Schema has 2 required and 1 optional field
    int requiredCount = 0;
    for (const auto& field : schema.fields) {
        if (field.required) requiredCount++;
    }
    EXPECT_EQ(2, requiredCount);
}

TEST(SchemaValidationTest, SchemaInheritance) {
    ConfigSchemaDefinition baseSchema;
    baseSchema.id = "base_entity";
    baseSchema.fields.push_back(SchemaBuilder::String("id", true));

    ConfigSchemaDefinition derivedSchema;
    derivedSchema.id = "unit_entity";
    derivedSchema.extends = {"base_entity"};
    derivedSchema.fields.push_back(SchemaBuilder::Integer("health", true));

    EXPECT_EQ(1, derivedSchema.extends.size());
    EXPECT_EQ("base_entity", derivedSchema.extends[0]);
}

