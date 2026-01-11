#include "JsonSchema.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <regex>
#include <filesystem>

namespace Nova {

using json = nlohmann::json;

// ============================================================================
// SchemaProperty Implementation
// ============================================================================

SchemaProperty::SchemaProperty(SchemaType type, const std::string& description)
    : m_type(type), m_description(description) {}

void SchemaProperty::AddProperty(const std::string& name, std::shared_ptr<SchemaProperty> prop) {
    m_properties[name] = prop;
}

std::shared_ptr<SchemaProperty> SchemaProperty::GetProperty(const std::string& name) const {
    auto it = m_properties.find(name);
    return it != m_properties.end() ? it->second : nullptr;
}

ValidationResult SchemaProperty::Validate(const std::string& jsonStr, const std::string& path) const {
    ValidationResult result;

    try {
        json j = json::parse(jsonStr);

        // Type checking
        switch (m_type) {
            case SchemaType::Null:
                if (!j.is_null()) {
                    result.AddError(path, "Expected null", "null", j.type_name());
                }
                break;

            case SchemaType::Boolean:
                if (!j.is_boolean()) {
                    result.AddError(path, "Expected boolean", "boolean", j.type_name());
                }
                break;

            case SchemaType::Integer:
                if (!j.is_number_integer()) {
                    result.AddError(path, "Expected integer", "integer", j.type_name());
                } else {
                    result.Merge(ValidateNumber(j.get<int>(), path));
                }
                break;

            case SchemaType::Number:
                if (!j.is_number()) {
                    result.AddError(path, "Expected number", "number", j.type_name());
                } else {
                    result.Merge(ValidateNumber(j.get<double>(), path));
                }
                break;

            case SchemaType::String:
                if (!j.is_string()) {
                    result.AddError(path, "Expected string", "string", j.type_name());
                } else {
                    result.Merge(ValidateString(j.get<std::string>(), path));
                }
                break;

            case SchemaType::Array:
                if (!j.is_array()) {
                    result.AddError(path, "Expected array", "array", j.type_name());
                } else {
                    result.Merge(ValidateArray(jsonStr, path));
                }
                break;

            case SchemaType::Object:
                if (!j.is_object()) {
                    result.AddError(path, "Expected object", "object", j.type_name());
                } else {
                    result.Merge(ValidateObject(jsonStr, path));
                }
                break;

            case SchemaType::Any:
                // Any type is always valid
                break;
        }

        // Enum validation
        if (!m_enumValues.empty() && j.is_string()) {
            std::string val = j.get<std::string>();
            bool found = false;
            for (const auto& e : m_enumValues) {
                if (e == val) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::string enumStr;
                for (size_t i = 0; i < m_enumValues.size(); ++i) {
                    if (i > 0) enumStr += ", ";
                    enumStr += m_enumValues[i];
                }
                result.AddError(path, "Value not in enum", enumStr, val);
            }
        }

    } catch (const json::parse_error& e) {
        result.AddError(path, "JSON parse error: " + std::string(e.what()));
    }

    return result;
}

ValidationResult SchemaProperty::ValidateNumber(double value, const std::string& path) const {
    ValidationResult result;

    if (m_numberConstraints.minimum && value < *m_numberConstraints.minimum) {
        result.AddError(path, "Value below minimum",
                       std::to_string(*m_numberConstraints.minimum), std::to_string(value));
    }

    if (m_numberConstraints.maximum && value > *m_numberConstraints.maximum) {
        result.AddError(path, "Value above maximum",
                       std::to_string(*m_numberConstraints.maximum), std::to_string(value));
    }

    if (m_numberConstraints.exclusiveMinimum && value <= *m_numberConstraints.exclusiveMinimum) {
        result.AddError(path, "Value must be greater than exclusive minimum",
                       ">" + std::to_string(*m_numberConstraints.exclusiveMinimum), std::to_string(value));
    }

    if (m_numberConstraints.exclusiveMaximum && value >= *m_numberConstraints.exclusiveMaximum) {
        result.AddError(path, "Value must be less than exclusive maximum",
                       "<" + std::to_string(*m_numberConstraints.exclusiveMaximum), std::to_string(value));
    }

    if (m_numberConstraints.multipleOf && *m_numberConstraints.multipleOf != 0) {
        double remainder = std::fmod(value, *m_numberConstraints.multipleOf);
        if (std::abs(remainder) > 1e-10) {
            result.AddError(path, "Value must be multiple of",
                           std::to_string(*m_numberConstraints.multipleOf), std::to_string(value));
        }
    }

    return result;
}

ValidationResult SchemaProperty::ValidateString(const std::string& value, const std::string& path) const {
    ValidationResult result;

    if (m_stringConstraints.minLength && value.length() < *m_stringConstraints.minLength) {
        result.AddError(path, "String too short",
                       "min " + std::to_string(*m_stringConstraints.minLength),
                       std::to_string(value.length()));
    }

    if (m_stringConstraints.maxLength && value.length() > *m_stringConstraints.maxLength) {
        result.AddError(path, "String too long",
                       "max " + std::to_string(*m_stringConstraints.maxLength),
                       std::to_string(value.length()));
    }

    if (m_stringConstraints.pattern) {
        try {
            std::regex pattern(*m_stringConstraints.pattern);
            if (!std::regex_match(value, pattern)) {
                result.AddError(path, "String does not match pattern",
                               *m_stringConstraints.pattern, value);
            }
        } catch (const std::regex_error&) {
            result.AddWarning("Invalid regex pattern: " + *m_stringConstraints.pattern);
        }
    }

    if (m_stringConstraints.format) {
        const std::string& format = *m_stringConstraints.format;
        bool valid = true;

        if (format == "email") {
            std::regex emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
            valid = std::regex_match(value, emailPattern);
        } else if (format == "uri" || format == "url") {
            std::regex uriPattern(R"(https?://[^\s]+)");
            valid = std::regex_match(value, uriPattern);
        } else if (format == "date") {
            std::regex datePattern(R"(\d{4}-\d{2}-\d{2})");
            valid = std::regex_match(value, datePattern);
        } else if (format == "time") {
            std::regex timePattern(R"(\d{2}:\d{2}:\d{2})");
            valid = std::regex_match(value, timePattern);
        } else if (format == "datetime") {
            std::regex datetimePattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})");
            valid = std::regex_match(value, datetimePattern);
        } else if (format == "filepath") {
            // Basic file path validation
            valid = !value.empty() && value.find("..") == std::string::npos;
        }

        if (!valid) {
            result.AddError(path, "Invalid format", format, value);
        }
    }

    return result;
}

ValidationResult SchemaProperty::ValidateArray(const std::string& jsonStr, const std::string& path) const {
    ValidationResult result;
    json j = json::parse(jsonStr);

    size_t size = j.size();

    if (m_arrayConstraints.minItems && size < *m_arrayConstraints.minItems) {
        result.AddError(path, "Array too small",
                       "min " + std::to_string(*m_arrayConstraints.minItems),
                       std::to_string(size));
    }

    if (m_arrayConstraints.maxItems && size > *m_arrayConstraints.maxItems) {
        result.AddError(path, "Array too large",
                       "max " + std::to_string(*m_arrayConstraints.maxItems),
                       std::to_string(size));
    }

    if (m_arrayConstraints.uniqueItems) {
        std::vector<std::string> items;
        for (const auto& item : j) {
            std::string itemStr = item.dump();
            if (std::find(items.begin(), items.end(), itemStr) != items.end()) {
                result.AddError(path, "Array items must be unique");
                break;
            }
            items.push_back(itemStr);
        }
    }

    // Validate each item against item schema
    if (m_itemSchema) {
        for (size_t i = 0; i < j.size(); ++i) {
            std::string itemPath = path + "[" + std::to_string(i) + "]";
            result.Merge(m_itemSchema->Validate(j[i].dump(), itemPath));
        }
    }

    return result;
}

ValidationResult SchemaProperty::ValidateObject(const std::string& jsonStr, const std::string& path) const {
    ValidationResult result;
    json j = json::parse(jsonStr);

    // Check required properties
    for (const auto& req : m_required) {
        if (!j.contains(req)) {
            result.AddError(path, "Missing required property: " + req);
        }
    }

    // Validate each property
    for (auto& [key, value] : j.items()) {
        std::string propPath = path.empty() ? key : path + "." + key;
        auto propSchema = GetProperty(key);

        if (propSchema) {
            result.Merge(propSchema->Validate(value.dump(), propPath));
        } else if (!m_additionalProperties) {
            result.AddError(propPath, "Additional property not allowed: " + key);
        }
    }

    return result;
}

std::string SchemaProperty::ToJsonSchema() const {
    json j;

    // Type mapping
    static const std::unordered_map<SchemaType, std::string> typeNames = {
        {SchemaType::Null, "null"},
        {SchemaType::Boolean, "boolean"},
        {SchemaType::Integer, "integer"},
        {SchemaType::Number, "number"},
        {SchemaType::String, "string"},
        {SchemaType::Array, "array"},
        {SchemaType::Object, "object"},
    };

    auto it = typeNames.find(m_type);
    if (it != typeNames.end()) {
        j["type"] = it->second;
    }

    if (!m_description.empty()) {
        j["description"] = m_description;
    }

    // Number constraints
    if (m_numberConstraints.minimum) j["minimum"] = *m_numberConstraints.minimum;
    if (m_numberConstraints.maximum) j["maximum"] = *m_numberConstraints.maximum;
    if (m_numberConstraints.exclusiveMinimum) j["exclusiveMinimum"] = *m_numberConstraints.exclusiveMinimum;
    if (m_numberConstraints.exclusiveMaximum) j["exclusiveMaximum"] = *m_numberConstraints.exclusiveMaximum;
    if (m_numberConstraints.multipleOf) j["multipleOf"] = *m_numberConstraints.multipleOf;

    // String constraints
    if (m_stringConstraints.minLength) j["minLength"] = *m_stringConstraints.minLength;
    if (m_stringConstraints.maxLength) j["maxLength"] = *m_stringConstraints.maxLength;
    if (m_stringConstraints.pattern) j["pattern"] = *m_stringConstraints.pattern;
    if (m_stringConstraints.format) j["format"] = *m_stringConstraints.format;

    // Array constraints
    if (m_arrayConstraints.minItems) j["minItems"] = *m_arrayConstraints.minItems;
    if (m_arrayConstraints.maxItems) j["maxItems"] = *m_arrayConstraints.maxItems;
    if (m_arrayConstraints.uniqueItems) j["uniqueItems"] = m_arrayConstraints.uniqueItems;

    // Enum
    if (!m_enumValues.empty()) {
        j["enum"] = m_enumValues;
    }

    // Object properties
    if (!m_properties.empty()) {
        json props;
        for (const auto& [name, prop] : m_properties) {
            props[name] = json::parse(prop->ToJsonSchema());
        }
        j["properties"] = props;
    }

    if (!m_required.empty()) {
        j["required"] = m_required;
    }

    j["additionalProperties"] = m_additionalProperties;

    // Array items
    if (m_itemSchema) {
        j["items"] = json::parse(m_itemSchema->ToJsonSchema());
    }

    return j.dump(2);
}

std::shared_ptr<SchemaProperty> SchemaProperty::FromJsonSchema(const std::string& jsonStr) {
    auto prop = std::make_shared<SchemaProperty>();

    try {
        json j = json::parse(jsonStr);

        // Type
        if (j.contains("type")) {
            static const std::unordered_map<std::string, SchemaType> typeMap = {
                {"null", SchemaType::Null},
                {"boolean", SchemaType::Boolean},
                {"integer", SchemaType::Integer},
                {"number", SchemaType::Number},
                {"string", SchemaType::String},
                {"array", SchemaType::Array},
                {"object", SchemaType::Object},
            };
            auto it = typeMap.find(j["type"]);
            if (it != typeMap.end()) {
                prop->SetType(it->second);
            }
        }

        if (j.contains("description")) {
            prop->SetDescription(j["description"]);
        }

        // Number constraints
        NumberConstraints nc;
        if (j.contains("minimum")) nc.minimum = j["minimum"];
        if (j.contains("maximum")) nc.maximum = j["maximum"];
        if (j.contains("exclusiveMinimum")) nc.exclusiveMinimum = j["exclusiveMinimum"];
        if (j.contains("exclusiveMaximum")) nc.exclusiveMaximum = j["exclusiveMaximum"];
        if (j.contains("multipleOf")) nc.multipleOf = j["multipleOf"];
        prop->SetNumberConstraints(nc);

        // String constraints
        StringConstraints sc;
        if (j.contains("minLength")) sc.minLength = j["minLength"];
        if (j.contains("maxLength")) sc.maxLength = j["maxLength"];
        if (j.contains("pattern")) sc.pattern = j["pattern"].get<std::string>();
        if (j.contains("format")) sc.format = j["format"].get<std::string>();
        prop->SetStringConstraints(sc);

        // Array constraints
        ArrayConstraints ac;
        if (j.contains("minItems")) ac.minItems = j["minItems"];
        if (j.contains("maxItems")) ac.maxItems = j["maxItems"];
        if (j.contains("uniqueItems")) ac.uniqueItems = j["uniqueItems"];
        prop->SetArrayConstraints(ac);

        // Enum
        if (j.contains("enum") && j["enum"].is_array()) {
            std::vector<std::string> enumValues;
            for (const auto& v : j["enum"]) {
                enumValues.push_back(v.get<std::string>());
            }
            prop->SetEnumValues(enumValues);
        }

        // Properties
        if (j.contains("properties") && j["properties"].is_object()) {
            for (auto& [name, propJson] : j["properties"].items()) {
                prop->AddProperty(name, FromJsonSchema(propJson.dump()));
            }
        }

        // Required
        if (j.contains("required") && j["required"].is_array()) {
            std::vector<std::string> required;
            for (const auto& r : j["required"]) {
                required.push_back(r.get<std::string>());
            }
            prop->SetRequired(required);
        }

        // Additional properties
        if (j.contains("additionalProperties")) {
            prop->SetAdditionalProperties(j["additionalProperties"]);
        }

        // Items
        if (j.contains("items")) {
            prop->SetItemSchema(FromJsonSchema(j["items"].dump()));
        }

    } catch (...) {}

    return prop;
}

// ============================================================================
// JsonSchema Implementation
// ============================================================================

JsonSchema::JsonSchema(const std::string& id, const std::string& title)
    : m_id(id), m_title(title) {
    m_root = std::make_shared<SchemaProperty>(SchemaType::Object);
}

void JsonSchema::AddDefinition(const std::string& name, std::shared_ptr<SchemaProperty> schema) {
    m_definitions[name] = schema;
}

std::shared_ptr<SchemaProperty> JsonSchema::GetDefinition(const std::string& name) const {
    auto it = m_definitions.find(name);
    return it != m_definitions.end() ? it->second : nullptr;
}

ValidationResult JsonSchema::Validate(const std::string& json) const {
    if (!m_root) {
        ValidationResult result;
        result.AddError("", "No root schema defined");
        return result;
    }
    return m_root->Validate(json);
}

ValidationResult JsonSchema::ValidateFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        ValidationResult result;
        result.AddError("", "Cannot open file: " + path);
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return Validate(content);
}

std::string JsonSchema::ToJsonSchema() const {
    json j;
    j["$schema"] = "http://json-schema.org/draft-07/schema#";
    j["$id"] = m_id;
    j["title"] = m_title;
    j["description"] = m_description;
    j["version"] = m_version;

    if (m_root) {
        json rootJson = json::parse(m_root->ToJsonSchema());
        for (auto& [key, value] : rootJson.items()) {
            j[key] = value;
        }
    }

    if (!m_definitions.empty()) {
        json defs;
        for (const auto& [name, def] : m_definitions) {
            defs[name] = json::parse(def->ToJsonSchema());
        }
        j["definitions"] = defs;
    }

    return j.dump(2);
}

std::shared_ptr<JsonSchema> JsonSchema::FromJsonSchema(const std::string& jsonStr) {
    auto schema = std::make_shared<JsonSchema>();

    try {
        json j = json::parse(jsonStr);

        if (j.contains("$id")) schema->SetId(j["$id"]);
        if (j.contains("title")) schema->SetTitle(j["title"]);
        if (j.contains("description")) schema->SetDescription(j["description"]);
        if (j.contains("version")) schema->SetVersion(j["version"]);

        // Root schema
        schema->SetRoot(SchemaProperty::FromJsonSchema(jsonStr));

        // Definitions
        if (j.contains("definitions")) {
            for (auto& [name, defJson] : j["definitions"].items()) {
                schema->AddDefinition(name, SchemaProperty::FromJsonSchema(defJson.dump()));
            }
        }

    } catch (...) {}

    return schema;
}

std::shared_ptr<JsonSchema> JsonSchema::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return FromJsonSchema(content);
}

bool JsonSchema::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << ToJsonSchema();
    return true;
}

// ============================================================================
// SchemaBuilder Implementation
// ============================================================================

SchemaBuilder::SchemaBuilder(const std::string& id) {
    m_schema = std::make_shared<JsonSchema>(id);
    m_schema->SetRoot(std::make_shared<SchemaProperty>(SchemaType::Object));
    m_propertyStack.push_back(m_schema->GetRoot());
}

SchemaBuilder& SchemaBuilder::Title(const std::string& title) {
    m_schema->SetTitle(title);
    return *this;
}

SchemaBuilder& SchemaBuilder::Description(const std::string& desc) {
    m_schema->SetDescription(desc);
    return *this;
}

SchemaBuilder& SchemaBuilder::Version(const std::string& version) {
    m_schema->SetVersion(version);
    return *this;
}

SchemaBuilder& SchemaBuilder::Property(const std::string& name, SchemaType type, const std::string& description) {
    auto prop = std::make_shared<SchemaProperty>(type, description);
    m_propertyStack.back()->AddProperty(name, prop);
    m_currentProperty = prop;
    return *this;
}

SchemaBuilder& SchemaBuilder::Required(const std::string& name) {
    m_required.push_back(name);
    auto current = m_propertyStack.back();
    current->SetRequired(m_required);
    return *this;
}

SchemaBuilder& SchemaBuilder::RequiredAll(const std::vector<std::string>& names) {
    m_required.insert(m_required.end(), names.begin(), names.end());
    m_propertyStack.back()->SetRequired(m_required);
    return *this;
}

SchemaBuilder& SchemaBuilder::Min(double value) {
    if (m_currentProperty) {
        NumberConstraints nc;
        nc.minimum = value;
        m_currentProperty->SetNumberConstraints(nc);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Max(double value) {
    if (m_currentProperty) {
        NumberConstraints nc;
        nc.maximum = value;
        m_currentProperty->SetNumberConstraints(nc);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::MinLength(size_t length) {
    if (m_currentProperty) {
        StringConstraints sc;
        sc.minLength = length;
        m_currentProperty->SetStringConstraints(sc);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::MaxLength(size_t length) {
    if (m_currentProperty) {
        StringConstraints sc;
        sc.maxLength = length;
        m_currentProperty->SetStringConstraints(sc);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Pattern(const std::string& regex) {
    if (m_currentProperty) {
        StringConstraints sc;
        sc.pattern = regex;
        m_currentProperty->SetStringConstraints(sc);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Format(const std::string& format) {
    if (m_currentProperty) {
        StringConstraints sc;
        sc.format = format;
        m_currentProperty->SetStringConstraints(sc);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Enum(const std::vector<std::string>& values) {
    if (m_currentProperty) {
        m_currentProperty->SetEnumValues(values);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Items(SchemaType type) {
    if (m_currentProperty) {
        m_currentProperty->SetItemSchema(std::make_shared<SchemaProperty>(type));
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::MinItems(size_t count) {
    if (m_currentProperty) {
        ArrayConstraints ac;
        ac.minItems = count;
        m_currentProperty->SetArrayConstraints(ac);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::MaxItems(size_t count) {
    if (m_currentProperty) {
        ArrayConstraints ac;
        ac.maxItems = count;
        m_currentProperty->SetArrayConstraints(ac);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::UniqueItems(bool unique) {
    if (m_currentProperty) {
        ArrayConstraints ac;
        ac.uniqueItems = unique;
        m_currentProperty->SetArrayConstraints(ac);
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::StartObject(const std::string& name) {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object);
    m_propertyStack.back()->AddProperty(name, prop);
    m_propertyStack.push_back(prop);
    m_currentProperty = prop;
    m_required.clear();
    return *this;
}

SchemaBuilder& SchemaBuilder::EndObject() {
    if (m_propertyStack.size() > 1) {
        m_propertyStack.pop_back();
        m_currentProperty = m_propertyStack.back();
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Definition(const std::string& name, std::function<void(SchemaBuilder&)> builder) {
    SchemaBuilder defBuilder;
    builder(defBuilder);
    auto defSchema = defBuilder.Build();
    if (defSchema && defSchema->GetRoot()) {
        m_schema->AddDefinition(name, defSchema->GetRoot());
    }
    return *this;
}

SchemaBuilder& SchemaBuilder::Ref(const std::string& definitionName) {
    auto def = m_schema->GetDefinition(definitionName);
    if (def && m_currentProperty) {
        // Copy properties from definition
        for (const auto& [name, prop] : def->GetProperties()) {
            m_currentProperty->AddProperty(name, prop);
        }
    }
    return *this;
}

std::shared_ptr<JsonSchema> SchemaBuilder::Build() {
    return m_schema;
}

// ============================================================================
// SchemaRegistry Implementation
// ============================================================================

SchemaRegistry& SchemaRegistry::Instance() {
    static SchemaRegistry instance;
    return instance;
}

void SchemaRegistry::RegisterSchema(const std::string& type, std::shared_ptr<JsonSchema> schema) {
    m_schemas[type] = schema;
}

void SchemaRegistry::UnregisterSchema(const std::string& type) {
    m_schemas.erase(type);
}

std::shared_ptr<JsonSchema> SchemaRegistry::GetSchema(const std::string& type) const {
    auto it = m_schemas.find(type);
    return it != m_schemas.end() ? it->second : nullptr;
}

std::vector<std::string> SchemaRegistry::GetRegisteredTypes() const {
    std::vector<std::string> types;
    for (const auto& [type, _] : m_schemas) {
        types.push_back(type);
    }
    return types;
}

ValidationResult SchemaRegistry::Validate(const std::string& type, const std::string& json) const {
    auto schema = GetSchema(type);
    if (!schema) {
        ValidationResult result;
        result.AddError("", "Unknown schema type: " + type);
        return result;
    }
    return schema->Validate(json);
}

ValidationResult SchemaRegistry::ValidateFile(const std::string& type, const std::string& path) const {
    auto schema = GetSchema(type);
    if (!schema) {
        ValidationResult result;
        result.AddError("", "Unknown schema type: " + type);
        return result;
    }
    return schema->ValidateFile(path);
}

std::string SchemaRegistry::DetectType(const std::string& jsonStr) const {
    try {
        json j = json::parse(jsonStr);

        // Check for type field
        if (j.contains("type")) {
            std::string type = j["type"];
            if (m_schemas.find(type) != m_schemas.end()) {
                return type;
            }
        }

        // Check for distinguishing features
        if (j.contains("base_stats") && j.contains("abilities")) {
            return "hero";
        }
        if (j.contains("combat") && j.contains("movement")) {
            return "unit";
        }
        if (j.contains("footprint") && j.contains("construction")) {
            return "building";
        }
        if (j.contains("cooldown") && j.contains("targetType")) {
            return "ability";
        }
        if (j.contains("trigger") && j.contains("function")) {
            return "behavior";
        }

    } catch (...) {}

    return "";
}

bool SchemaRegistry::LoadSchemasFromDirectory(const std::string& path) {
    namespace fs = std::filesystem;
    if (!fs::exists(path) || !fs::is_directory(path)) return false;

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.path().extension() == ".schema.json" ||
            entry.path().extension() == ".json") {
            auto schema = JsonSchema::LoadFromFile(entry.path().string());
            if (schema) {
                std::string name = entry.path().stem().string();
                // Remove .schema if present
                if (name.length() > 7 && name.substr(name.length() - 7) == ".schema") {
                    name = name.substr(0, name.length() - 7);
                }
                RegisterSchema(name, schema);
            }
        }
    }
    return true;
}

bool SchemaRegistry::SaveSchemasToDirectory(const std::string& path) const {
    namespace fs = std::filesystem;
    if (!fs::exists(path)) {
        fs::create_directories(path);
    }

    for (const auto& [type, schema] : m_schemas) {
        std::string filePath = path + "/" + type + ".schema.json";
        if (!schema->SaveToFile(filePath)) {
            return false;
        }
    }
    return true;
}

void SchemaRegistry::RegisterBuiltinSchemas() {
    RegisterUnitSchema();
    RegisterBuildingSchema();
    RegisterHeroSchema();
    RegisterAbilitySchema();
    RegisterBehaviorSchema();
}

void SchemaRegistry::RegisterUnitSchema() {
    auto schema = SchemaBuilder("unit")
        .Title("Unit Definition")
        .Description("Schema for game unit definitions")
        .Version("1.0")
        .Property("id", SchemaType::String, "Unique unit identifier").Required("id")
        .Property("type", SchemaType::String, "Entity type").Enum({"unit"})
        .Property("name", SchemaType::String, "Display name").Required("name")
        .Property("description", SchemaType::String, "Unit description")
        .Property("tags", SchemaType::Array, "Classification tags").Items(SchemaType::String)
        .StartObject("combat")
            .Property("health", SchemaType::Integer, "Base health").Min(1).Max(10000)
            .Property("maxHealth", SchemaType::Integer, "Maximum health")
            .Property("armor", SchemaType::Integer, "Armor value").Min(0)
            .Property("attackDamage", SchemaType::Integer, "Base attack damage")
            .Property("attackSpeed", SchemaType::Number, "Attacks per second")
            .Property("attackRange", SchemaType::Number, "Attack range")
        .EndObject()
        .StartObject("movement")
            .Property("speed", SchemaType::Number, "Movement speed").Min(0)
            .Property("turnRate", SchemaType::Number, "Turn rate in degrees/sec")
            .Property("canSwim", SchemaType::Boolean, "Can traverse water")
        .EndObject()
        .Property("abilities", SchemaType::Array, "Unit abilities")
        .Build();

    RegisterSchema("unit", schema);
}

void SchemaRegistry::RegisterBuildingSchema() {
    auto schema = SchemaBuilder("building")
        .Title("Building Definition")
        .Description("Schema for game building definitions")
        .Version("1.0")
        .Property("id", SchemaType::String).Required("id")
        .Property("type", SchemaType::String).Enum({"building"})
        .Property("name", SchemaType::String).Required("name")
        .StartObject("footprint")
            .Property("width", SchemaType::Integer, "Building width in tiles").Min(1)
            .Property("height", SchemaType::Integer, "Building height in tiles").Min(1)
        .EndObject()
        .StartObject("construction")
            .Property("buildTime", SchemaType::Number, "Build time in seconds")
            .Property("cost", SchemaType::Object, "Resource costs")
        .EndObject()
        .Build();

    RegisterSchema("building", schema);
}

void SchemaRegistry::RegisterHeroSchema() {
    auto schema = SchemaBuilder("hero")
        .Title("Hero Definition")
        .Description("Schema for hero unit definitions")
        .Version("1.0")
        .Property("id", SchemaType::String).Required("id")
        .Property("name", SchemaType::String).Required("name")
        .Property("title", SchemaType::String, "Hero title")
        .Property("class", SchemaType::String).Enum({"warrior", "mage", "rogue", "support"})
        .Property("primary_attribute", SchemaType::String).Enum({"strength", "agility", "intelligence"})
        .StartObject("base_stats")
            .Property("health", SchemaType::Integer).Min(1)
            .Property("mana", SchemaType::Integer).Min(0)
            .Property("damage", SchemaType::Integer)
            .Property("armor", SchemaType::Integer)
            .Property("strength", SchemaType::Integer)
            .Property("agility", SchemaType::Integer)
            .Property("intelligence", SchemaType::Integer)
        .EndObject()
        .Property("abilities", SchemaType::Array)
        .Property("talents", SchemaType::Array)
        .Build();

    RegisterSchema("hero", schema);
}

void SchemaRegistry::RegisterAbilitySchema() {
    auto schema = SchemaBuilder("ability")
        .Title("Ability Definition")
        .Description("Schema for ability definitions")
        .Version("1.0")
        .Property("id", SchemaType::String).Required("id")
        .Property("name", SchemaType::String).Required("name")
        .Property("description", SchemaType::String)
        .Property("cooldown", SchemaType::Number).Min(0)
        .Property("manaCost", SchemaType::Integer).Min(0)
        .Property("range", SchemaType::Number).Min(0)
        .Property("targetType", SchemaType::String).Enum({"self", "unit", "ground", "direction", "passive"})
        .Property("script", SchemaType::String, "Script file path").Format("filepath")
        .Build();

    RegisterSchema("ability", schema);
}

void SchemaRegistry::RegisterBehaviorSchema() {
    auto schema = SchemaBuilder("behavior")
        .Title("Behavior Definition")
        .Description("Schema for behavior definitions")
        .Version("1.0")
        .Property("id", SchemaType::String).Required("id")
        .Property("name", SchemaType::String).Required("name")
        .Property("category", SchemaType::String)
        .Property("trigger", SchemaType::String).Enum({"OnSpawn", "OnDeath", "OnDamaged", "OnAttack", "OnUpdate", "OnAbilityCast", "OnTargetAcquired", "OnIdle", "Custom"})
        .Property("pointCost", SchemaType::Number, "Point cost for balance system").Min(0)
        .Property("parameters", SchemaType::Array, "Behavior parameters")
        .Build();

    RegisterSchema("behavior", schema);
}

// ============================================================================
// Schema Templates
// ============================================================================

namespace SchemaTemplates {

std::shared_ptr<SchemaProperty> Vector2Schema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Array, "2D vector [x, y]");
    ArrayConstraints ac;
    ac.minItems = 2;
    ac.maxItems = 2;
    prop->SetArrayConstraints(ac);
    prop->SetItemSchema(std::make_shared<SchemaProperty>(SchemaType::Number));
    return prop;
}

std::shared_ptr<SchemaProperty> Vector3Schema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Array, "3D vector [x, y, z]");
    ArrayConstraints ac;
    ac.minItems = 3;
    ac.maxItems = 3;
    prop->SetArrayConstraints(ac);
    prop->SetItemSchema(std::make_shared<SchemaProperty>(SchemaType::Number));
    return prop;
}

std::shared_ptr<SchemaProperty> Vector4Schema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Array, "4D vector/color [r, g, b, a]");
    ArrayConstraints ac;
    ac.minItems = 4;
    ac.maxItems = 4;
    prop->SetArrayConstraints(ac);
    prop->SetItemSchema(std::make_shared<SchemaProperty>(SchemaType::Number));
    return prop;
}

std::shared_ptr<SchemaProperty> TransformSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Transform with position, rotation, scale");
    prop->AddProperty("position", Vector3Schema());
    prop->AddProperty("rotation", Vector3Schema());
    prop->AddProperty("scale", Vector3Schema());
    return prop;
}

std::shared_ptr<SchemaProperty> CombatStatsSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Combat statistics");
    prop->AddProperty("health", std::make_shared<SchemaProperty>(SchemaType::Integer, "Current health"));
    prop->AddProperty("maxHealth", std::make_shared<SchemaProperty>(SchemaType::Integer, "Maximum health"));
    prop->AddProperty("armor", std::make_shared<SchemaProperty>(SchemaType::Integer, "Armor value"));
    prop->AddProperty("attackDamage", std::make_shared<SchemaProperty>(SchemaType::Integer, "Attack damage"));
    prop->AddProperty("attackSpeed", std::make_shared<SchemaProperty>(SchemaType::Number, "Attacks per second"));
    prop->AddProperty("attackRange", std::make_shared<SchemaProperty>(SchemaType::Number, "Attack range"));
    return prop;
}

std::shared_ptr<SchemaProperty> MovementSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Movement configuration");
    prop->AddProperty("speed", std::make_shared<SchemaProperty>(SchemaType::Number, "Movement speed"));
    prop->AddProperty("turnRate", std::make_shared<SchemaProperty>(SchemaType::Number, "Turn rate"));
    prop->AddProperty("acceleration", std::make_shared<SchemaProperty>(SchemaType::Number, "Acceleration"));
    prop->AddProperty("canSwim", std::make_shared<SchemaProperty>(SchemaType::Boolean, "Can traverse water"));
    prop->AddProperty("canFly", std::make_shared<SchemaProperty>(SchemaType::Boolean, "Can fly"));
    return prop;
}

std::shared_ptr<SchemaProperty> CollisionShapeSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Collision shape");

    auto typeProp = std::make_shared<SchemaProperty>(SchemaType::String, "Shape type");
    typeProp->SetEnumValues({"box", "sphere", "capsule", "cylinder", "mesh"});
    prop->AddProperty("type", typeProp);

    prop->AddProperty("radius", std::make_shared<SchemaProperty>(SchemaType::Number, "Shape radius"));
    prop->AddProperty("height", std::make_shared<SchemaProperty>(SchemaType::Number, "Shape height"));
    prop->AddProperty("offset", Vector3Schema());
    prop->AddProperty("is_trigger", std::make_shared<SchemaProperty>(SchemaType::Boolean, "Is trigger volume"));
    return prop;
}

std::shared_ptr<SchemaProperty> AnimationSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Animation reference");
    prop->AddProperty("path", std::make_shared<SchemaProperty>(SchemaType::String, "Animation file path"));
    prop->AddProperty("loop", std::make_shared<SchemaProperty>(SchemaType::Boolean, "Loop animation"));
    prop->AddProperty("speed", std::make_shared<SchemaProperty>(SchemaType::Number, "Playback speed"));
    return prop;
}

std::shared_ptr<SchemaProperty> SoundSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Sound reference");
    prop->AddProperty("path", std::make_shared<SchemaProperty>(SchemaType::String, "Sound file path"));
    prop->AddProperty("paths", std::make_shared<SchemaProperty>(SchemaType::Array, "Multiple sound files"));
    prop->AddProperty("volume", std::make_shared<SchemaProperty>(SchemaType::Number, "Volume (0-1)"));
    prop->AddProperty("pitchVariation", std::make_shared<SchemaProperty>(SchemaType::Number, "Pitch variation"));
    return prop;
}

std::shared_ptr<SchemaProperty> ScriptHookSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::String, "Python script file path");
    StringConstraints sc;
    sc.format = "filepath";
    sc.pattern = R"(.*\.py$)";
    prop->SetStringConstraints(sc);
    return prop;
}

std::shared_ptr<SchemaProperty> AbilitySchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Ability definition");
    prop->AddProperty("id", std::make_shared<SchemaProperty>(SchemaType::String, "Ability ID"));
    prop->AddProperty("name", std::make_shared<SchemaProperty>(SchemaType::String, "Display name"));
    prop->AddProperty("description", std::make_shared<SchemaProperty>(SchemaType::String, "Description"));
    prop->AddProperty("cooldown", std::make_shared<SchemaProperty>(SchemaType::Number, "Cooldown in seconds"));
    prop->AddProperty("manaCost", std::make_shared<SchemaProperty>(SchemaType::Integer, "Mana cost"));
    prop->AddProperty("range", std::make_shared<SchemaProperty>(SchemaType::Number, "Cast range"));

    auto targetType = std::make_shared<SchemaProperty>(SchemaType::String, "Target type");
    targetType->SetEnumValues({"self", "unit", "ground", "direction", "passive"});
    prop->AddProperty("targetType", targetType);

    prop->AddProperty("script", ScriptHookSchema());
    return prop;
}

std::shared_ptr<SchemaProperty> ResourceCostSchema() {
    auto prop = std::make_shared<SchemaProperty>(SchemaType::Object, "Resource costs");
    prop->AddProperty("gold", std::make_shared<SchemaProperty>(SchemaType::Integer, "Gold cost"));
    prop->AddProperty("wood", std::make_shared<SchemaProperty>(SchemaType::Integer, "Wood cost"));
    prop->AddProperty("stone", std::make_shared<SchemaProperty>(SchemaType::Integer, "Stone cost"));
    prop->AddProperty("food", std::make_shared<SchemaProperty>(SchemaType::Integer, "Food cost"));
    prop->AddProperty("mana", std::make_shared<SchemaProperty>(SchemaType::Integer, "Mana cost"));
    prop->SetAdditionalProperties(true);  // Allow custom resources
    return prop;
}

} // namespace SchemaTemplates

} // namespace Nova
