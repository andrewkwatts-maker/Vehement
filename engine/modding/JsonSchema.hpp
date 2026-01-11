#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>
#include <optional>

namespace Nova {

/**
 * @brief JSON Schema validation result
 */
struct ValidationError {
    std::string path;       // JSON path where error occurred (e.g., "combat.health")
    std::string message;    // Human-readable error message
    std::string expected;   // Expected type/value
    std::string actual;     // Actual type/value found

    [[nodiscard]] std::string ToString() const {
        return path + ": " + message + " (expected: " + expected + ", got: " + actual + ")";
    }
};

struct ValidationResult {
    bool valid = true;
    std::vector<ValidationError> errors;
    std::vector<std::string> warnings;

    void AddError(const std::string& path, const std::string& message,
                  const std::string& expected = "", const std::string& actual = "") {
        valid = false;
        errors.push_back({path, message, expected, actual});
    }

    void AddWarning(const std::string& message) {
        warnings.push_back(message);
    }

    void Merge(const ValidationResult& other) {
        if (!other.valid) valid = false;
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
    }

    [[nodiscard]] std::string GetErrorsString() const {
        std::string result;
        for (const auto& error : errors) {
            result += error.ToString() + "\n";
        }
        return result;
    }
};

/**
 * @brief JSON Schema data types
 */
enum class SchemaType {
    Null,
    Boolean,
    Integer,
    Number,
    String,
    Array,
    Object,
    Any
};

/**
 * @brief Property constraint definitions
 */
struct NumberConstraints {
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> exclusiveMinimum;
    std::optional<double> exclusiveMaximum;
    std::optional<double> multipleOf;
};

struct StringConstraints {
    std::optional<size_t> minLength;
    std::optional<size_t> maxLength;
    std::optional<std::string> pattern;  // Regex pattern
    std::optional<std::string> format;   // Predefined format (email, uri, date, etc.)
};

struct ArrayConstraints {
    std::optional<size_t> minItems;
    std::optional<size_t> maxItems;
    bool uniqueItems = false;
};

/**
 * @brief Single schema property definition
 */
class SchemaProperty {
public:
    SchemaProperty() = default;
    SchemaProperty(SchemaType type, const std::string& description = "");

    // Type
    void SetType(SchemaType type) { m_type = type; }
    [[nodiscard]] SchemaType GetType() const { return m_type; }

    // Description
    void SetDescription(const std::string& desc) { m_description = desc; }
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }

    // Default value
    template<typename T>
    void SetDefault(const T& value) { m_defaultValue = value; }
    [[nodiscard]] bool HasDefault() const { return m_hasDefault; }

    // Constraints
    void SetNumberConstraints(const NumberConstraints& c) { m_numberConstraints = c; }
    void SetStringConstraints(const StringConstraints& c) { m_stringConstraints = c; }
    void SetArrayConstraints(const ArrayConstraints& c) { m_arrayConstraints = c; }

    // Enum values (allowed values)
    void SetEnumValues(const std::vector<std::string>& values) { m_enumValues = values; }
    [[nodiscard]] const std::vector<std::string>& GetEnumValues() const { return m_enumValues; }

    // For objects: nested properties
    void AddProperty(const std::string& name, std::shared_ptr<SchemaProperty> prop);
    [[nodiscard]] std::shared_ptr<SchemaProperty> GetProperty(const std::string& name) const;
    [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<SchemaProperty>>& GetProperties() const {
        return m_properties;
    }

    // Required properties for objects
    void SetRequired(const std::vector<std::string>& required) { m_required = required; }
    [[nodiscard]] const std::vector<std::string>& GetRequired() const { return m_required; }

    // For arrays: item schema
    void SetItemSchema(std::shared_ptr<SchemaProperty> schema) { m_itemSchema = schema; }
    [[nodiscard]] std::shared_ptr<SchemaProperty> GetItemSchema() const { return m_itemSchema; }

    // Allow additional properties in objects
    void SetAdditionalProperties(bool allow) { m_additionalProperties = allow; }
    [[nodiscard]] bool AllowsAdditionalProperties() const { return m_additionalProperties; }

    // Validation
    [[nodiscard]] ValidationResult Validate(const std::string& json, const std::string& path = "") const;

    // Serialization
    [[nodiscard]] std::string ToJsonSchema() const;
    static std::shared_ptr<SchemaProperty> FromJsonSchema(const std::string& json);

private:
    ValidationResult ValidateNumber(double value, const std::string& path) const;
    ValidationResult ValidateString(const std::string& value, const std::string& path) const;
    ValidationResult ValidateArray(const std::string& json, const std::string& path) const;
    ValidationResult ValidateObject(const std::string& json, const std::string& path) const;

    SchemaType m_type = SchemaType::Any;
    std::string m_description;
    bool m_hasDefault = false;
    std::variant<bool, int, double, std::string> m_defaultValue;

    NumberConstraints m_numberConstraints;
    StringConstraints m_stringConstraints;
    ArrayConstraints m_arrayConstraints;

    std::vector<std::string> m_enumValues;
    std::unordered_map<std::string, std::shared_ptr<SchemaProperty>> m_properties;
    std::vector<std::string> m_required;
    std::shared_ptr<SchemaProperty> m_itemSchema;
    bool m_additionalProperties = true;
};

/**
 * @brief Complete JSON Schema definition
 */
class JsonSchema {
public:
    JsonSchema(const std::string& id = "", const std::string& title = "");

    // Metadata
    void SetId(const std::string& id) { m_id = id; }
    [[nodiscard]] const std::string& GetId() const { return m_id; }

    void SetTitle(const std::string& title) { m_title = title; }
    [[nodiscard]] const std::string& GetTitle() const { return m_title; }

    void SetDescription(const std::string& desc) { m_description = desc; }
    [[nodiscard]] const std::string& GetDescription() const { return m_description; }

    void SetVersion(const std::string& version) { m_version = version; }
    [[nodiscard]] const std::string& GetVersion() const { return m_version; }

    // Root schema
    void SetRoot(std::shared_ptr<SchemaProperty> root) { m_root = root; }
    [[nodiscard]] std::shared_ptr<SchemaProperty> GetRoot() const { return m_root; }

    // Definitions (reusable schemas)
    void AddDefinition(const std::string& name, std::shared_ptr<SchemaProperty> schema);
    [[nodiscard]] std::shared_ptr<SchemaProperty> GetDefinition(const std::string& name) const;

    // Validation
    [[nodiscard]] ValidationResult Validate(const std::string& json) const;
    [[nodiscard]] ValidationResult ValidateFile(const std::string& path) const;

    // Serialization
    [[nodiscard]] std::string ToJsonSchema() const;
    static std::shared_ptr<JsonSchema> FromJsonSchema(const std::string& json);
    static std::shared_ptr<JsonSchema> LoadFromFile(const std::string& path);
    bool SaveToFile(const std::string& path) const;

private:
    std::string m_id;
    std::string m_title;
    std::string m_description;
    std::string m_version = "1.0";
    std::shared_ptr<SchemaProperty> m_root;
    std::unordered_map<std::string, std::shared_ptr<SchemaProperty>> m_definitions;
};

/**
 * @brief Fluent builder for creating schemas
 */
class SchemaBuilder {
public:
    SchemaBuilder(const std::string& id = "");

    // Metadata
    SchemaBuilder& Title(const std::string& title);
    SchemaBuilder& Description(const std::string& desc);
    SchemaBuilder& Version(const std::string& version);

    // Properties
    SchemaBuilder& Property(const std::string& name, SchemaType type,
                            const std::string& description = "");
    SchemaBuilder& Required(const std::string& name);
    SchemaBuilder& RequiredAll(const std::vector<std::string>& names);

    // Constraints
    SchemaBuilder& Min(double value);
    SchemaBuilder& Max(double value);
    SchemaBuilder& MinLength(size_t length);
    SchemaBuilder& MaxLength(size_t length);
    SchemaBuilder& Pattern(const std::string& regex);
    SchemaBuilder& Format(const std::string& format);
    SchemaBuilder& Enum(const std::vector<std::string>& values);

    // Arrays
    SchemaBuilder& Items(SchemaType type);
    SchemaBuilder& MinItems(size_t count);
    SchemaBuilder& MaxItems(size_t count);
    SchemaBuilder& UniqueItems(bool unique = true);

    // Nested objects
    SchemaBuilder& StartObject(const std::string& name);
    SchemaBuilder& EndObject();

    // Definitions
    SchemaBuilder& Definition(const std::string& name, std::function<void(SchemaBuilder&)> builder);
    SchemaBuilder& Ref(const std::string& definitionName);

    // Build
    [[nodiscard]] std::shared_ptr<JsonSchema> Build();

private:
    std::shared_ptr<JsonSchema> m_schema;
    std::shared_ptr<SchemaProperty> m_currentProperty;
    std::vector<std::shared_ptr<SchemaProperty>> m_propertyStack;
    std::vector<std::string> m_required;
};

/**
 * @brief Schema registry for managing game entity schemas
 */
class SchemaRegistry {
public:
    static SchemaRegistry& Instance();

    // Registration
    void RegisterSchema(const std::string& type, std::shared_ptr<JsonSchema> schema);
    void UnregisterSchema(const std::string& type);

    // Lookup
    [[nodiscard]] std::shared_ptr<JsonSchema> GetSchema(const std::string& type) const;
    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;

    // Validation
    [[nodiscard]] ValidationResult Validate(const std::string& type, const std::string& json) const;
    [[nodiscard]] ValidationResult ValidateFile(const std::string& type, const std::string& path) const;

    // Auto-detection (tries to find matching schema based on JSON content)
    [[nodiscard]] std::string DetectType(const std::string& json) const;

    // Loading schemas from files
    bool LoadSchemasFromDirectory(const std::string& path);
    bool SaveSchemasToDirectory(const std::string& path) const;

    // Built-in game schemas
    void RegisterBuiltinSchemas();

private:
    SchemaRegistry() = default;
    void RegisterUnitSchema();
    void RegisterBuildingSchema();
    void RegisterHeroSchema();
    void RegisterAbilitySchema();
    void RegisterBehaviorSchema();

    std::unordered_map<std::string, std::shared_ptr<JsonSchema>> m_schemas;
};

// ============================================================================
// Predefined Schema Templates
// ============================================================================

/**
 * @brief Common schema fragments for game entities
 */
namespace SchemaTemplates {

// Vector2 schema
std::shared_ptr<SchemaProperty> Vector2Schema();

// Vector3 schema
std::shared_ptr<SchemaProperty> Vector3Schema();

// Vector4/Color schema
std::shared_ptr<SchemaProperty> Vector4Schema();

// Transform schema (position, rotation, scale)
std::shared_ptr<SchemaProperty> TransformSchema();

// Combat stats schema
std::shared_ptr<SchemaProperty> CombatStatsSchema();

// Movement schema
std::shared_ptr<SchemaProperty> MovementSchema();

// Collision shape schema
std::shared_ptr<SchemaProperty> CollisionShapeSchema();

// Animation reference schema
std::shared_ptr<SchemaProperty> AnimationSchema();

// Sound reference schema
std::shared_ptr<SchemaProperty> SoundSchema();

// Script hook schema
std::shared_ptr<SchemaProperty> ScriptHookSchema();

// Ability schema
std::shared_ptr<SchemaProperty> AbilitySchema();

// Resource cost schema
std::shared_ptr<SchemaProperty> ResourceCostSchema();

} // namespace SchemaTemplates

} // namespace Nova
