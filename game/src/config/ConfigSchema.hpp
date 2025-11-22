#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <functional>

namespace Vehement {
namespace Config {

/**
 * @brief Supported schema field types for validation
 */
enum class SchemaFieldType {
    String,
    Integer,
    Float,
    Boolean,
    Array,
    Object,
    Vector2,       // [x, y]
    Vector3,       // [x, y, z]
    Vector4,       // [x, y, z, w]
    Color,         // [r, g, b] or [r, g, b, a]
    ResourcePath,  // String validated as valid path
    ScriptPath,    // String validated as Python script path
    Enum,          // String from predefined set
    Any            // No type validation
};

/**
 * @brief Schema field constraints
 */
struct SchemaConstraints {
    std::optional<double> minValue;
    std::optional<double> maxValue;
    std::optional<size_t> minLength;
    std::optional<size_t> maxLength;
    std::optional<size_t> minArraySize;
    std::optional<size_t> maxArraySize;
    std::vector<std::string> enumValues;      // Valid values for Enum type
    std::string pattern;                       // Regex pattern for strings
    bool allowEmpty = true;
    bool mustExist = false;                    // For resource paths
};

/**
 * @brief Definition of a single schema field
 */
struct SchemaField {
    std::string name;
    SchemaFieldType type = SchemaFieldType::Any;
    bool required = false;
    std::string description;
    std::string defaultValue;                  // JSON string representation
    SchemaConstraints constraints;

    // For nested objects/arrays
    std::string nestedSchemaRef;               // Reference to another schema
    std::vector<SchemaField> inlineFields;     // Inline nested fields
};

/**
 * @brief Complete schema definition for a config type
 */
struct ConfigSchemaDefinition {
    std::string id;                            // Unique schema identifier
    std::string name;                          // Human-readable name
    std::string description;
    std::string version;
    std::vector<std::string> extends;          // Parent schema IDs for inheritance
    std::vector<SchemaField> fields;

    // Validation hooks
    std::function<bool(const std::string&)> customValidator;
};

/**
 * @brief Validation result with detailed error information
 */
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void AddError(const std::string& path, const std::string& message) {
        valid = false;
        errors.push_back("[" + path + "] " + message);
    }

    void AddWarning(const std::string& path, const std::string& message) {
        warnings.push_back("[" + path + "] " + message);
    }

    void Merge(const ValidationResult& other) {
        if (!other.valid) valid = false;
        errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
    }
};

// ============================================================================
// Predefined Schema Definitions
// ============================================================================

/**
 * @brief Physics collision shape types
 */
enum class CollisionShapeType {
    None,
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Mesh,
    Compound
};

inline const char* CollisionShapeTypeToString(CollisionShapeType type) {
    switch (type) {
        case CollisionShapeType::Box:      return "box";
        case CollisionShapeType::Sphere:   return "sphere";
        case CollisionShapeType::Capsule:  return "capsule";
        case CollisionShapeType::Cylinder: return "cylinder";
        case CollisionShapeType::Mesh:     return "mesh";
        case CollisionShapeType::Compound: return "compound";
        default:                           return "none";
    }
}

inline CollisionShapeType StringToCollisionShapeType(const std::string& str) {
    if (str == "box")      return CollisionShapeType::Box;
    if (str == "sphere")   return CollisionShapeType::Sphere;
    if (str == "capsule")  return CollisionShapeType::Capsule;
    if (str == "cylinder") return CollisionShapeType::Cylinder;
    if (str == "mesh")     return CollisionShapeType::Mesh;
    if (str == "compound") return CollisionShapeType::Compound;
    return CollisionShapeType::None;
}

/**
 * @brief Grid type for building footprints
 */
enum class GridType {
    Rect,
    Hex
};

inline const char* GridTypeToString(GridType type) {
    return type == GridType::Hex ? "hex" : "rect";
}

inline GridType StringToGridType(const std::string& str) {
    return str == "hex" ? GridType::Hex : GridType::Rect;
}

/**
 * @brief Resource types used in game economy
 */
enum class ResourceType {
    None,
    Food,
    Wood,
    Stone,
    Metal,
    Gold,
    Mana,
    Population
};

inline const char* ResourceTypeToString(ResourceType type) {
    switch (type) {
        case ResourceType::Food:       return "food";
        case ResourceType::Wood:       return "wood";
        case ResourceType::Stone:      return "stone";
        case ResourceType::Metal:      return "metal";
        case ResourceType::Gold:       return "gold";
        case ResourceType::Mana:       return "mana";
        case ResourceType::Population: return "population";
        default:                       return "none";
    }
}

inline ResourceType StringToResourceType(const std::string& str) {
    if (str == "food")       return ResourceType::Food;
    if (str == "wood")       return ResourceType::Wood;
    if (str == "stone")      return ResourceType::Stone;
    if (str == "metal")      return ResourceType::Metal;
    if (str == "gold")       return ResourceType::Gold;
    if (str == "mana")       return ResourceType::Mana;
    if (str == "population") return ResourceType::Population;
    return ResourceType::None;
}

// ============================================================================
// Schema Builder Helpers
// ============================================================================

namespace SchemaBuilder {

inline SchemaField String(const std::string& name, bool required = false,
                          const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::String;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField Integer(const std::string& name, bool required = false,
                           const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Integer;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField Float(const std::string& name, bool required = false,
                         const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Float;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField Boolean(const std::string& name, bool required = false,
                           const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Boolean;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField Vec3(const std::string& name, bool required = false,
                        const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Vector3;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField ResourcePath(const std::string& name, bool required = false,
                                const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::ResourcePath;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField ScriptPath(const std::string& name, bool required = false,
                              const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::ScriptPath;
    f.required = required;
    f.description = desc;
    return f;
}

inline SchemaField Enum(const std::string& name,
                        const std::vector<std::string>& values,
                        bool required = false, const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Enum;
    f.required = required;
    f.description = desc;
    f.constraints.enumValues = values;
    return f;
}

inline SchemaField Object(const std::string& name,
                          const std::vector<SchemaField>& fields,
                          bool required = false, const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Object;
    f.required = required;
    f.description = desc;
    f.inlineFields = fields;
    return f;
}

inline SchemaField Array(const std::string& name, SchemaFieldType elementType,
                         bool required = false, const std::string& desc = "") {
    SchemaField f;
    f.name = name;
    f.type = SchemaFieldType::Array;
    f.required = required;
    f.description = desc;
    return f;
}

} // namespace SchemaBuilder

} // namespace Config
} // namespace Vehement
