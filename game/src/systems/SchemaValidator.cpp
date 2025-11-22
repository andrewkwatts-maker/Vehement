#include "SchemaValidator.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <unordered_set>

// Using nlohmann/json - include the actual header in your project
// #include <nlohmann/json.hpp>

// For this implementation, we'll use forward declarations and assume json is available
namespace nlohmann {
    class json;
}

// Placeholder for actual JSON implementation
// In a real project, include nlohmann/json.hpp or your preferred JSON library

namespace Vehement {

// ============================================================================
// ValidationError Implementation
// ============================================================================

std::string ValidationError::ToString() const {
    std::ostringstream ss;
    ss << "[" << GetSeverityString() << "] ";

    if (!path.empty()) {
        ss << "at " << path << ": ";
    }

    ss << message;

    if (!expected.empty() && !actual.empty()) {
        ss << " (expected: " << expected << ", got: " << actual << ")";
    }

    if (line >= 0) {
        ss << " [line " << line;
        if (column >= 0) {
            ss << ", col " << column;
        }
        ss << "]";
    }

    return ss.str();
}

const char* ValidationError::GetSeverityString() const {
    switch (severity) {
        case ValidationSeverity::Error:   return "ERROR";
        case ValidationSeverity::Warning: return "WARNING";
        case ValidationSeverity::Info:    return "INFO";
        default:                          return "UNKNOWN";
    }
}

// ============================================================================
// ValidationResult Implementation
// ============================================================================

std::string ValidationResult::GetReport() const {
    std::ostringstream ss;

    ss << "Validation Report for: " << configPath << "\n";
    ss << "Schema: " << schemaId << "\n";
    ss << "Status: " << (IsValid() ? "VALID" : "INVALID") << "\n";
    ss << "Errors: " << errors.size() << ", Warnings: " << warnings.size() << "\n";
    ss << std::string(60, '-') << "\n";

    if (!errors.empty()) {
        ss << "\nErrors:\n";
        for (size_t i = 0; i < errors.size(); ++i) {
            ss << "  " << (i + 1) << ". " << errors[i].ToString() << "\n";
        }
    }

    if (!warnings.empty()) {
        ss << "\nWarnings:\n";
        for (size_t i = 0; i < warnings.size(); ++i) {
            ss << "  " << (i + 1) << ". " << warnings[i].ToString() << "\n";
        }
    }

    return ss.str();
}

void ValidationResult::AddError(ValidationError error) {
    error.severity = ValidationSeverity::Error;
    errors.push_back(std::move(error));
    valid = false;
}

void ValidationResult::AddWarning(ValidationError error) {
    error.severity = ValidationSeverity::Warning;
    warnings.push_back(std::move(error));
}

void ValidationResult::Merge(const ValidationResult& other) {
    errors.insert(errors.end(), other.errors.begin(), other.errors.end());
    warnings.insert(warnings.end(), other.warnings.begin(), other.warnings.end());
    if (!other.valid) {
        valid = false;
    }
}

// ============================================================================
// SchemaValidator Implementation
// ============================================================================

SchemaValidator::SchemaValidator()
    : m_config{}
{
    // Initialize ID patterns for auto-detection
    m_idPatterns = {
        {"^spell_",      "spell.schema.json"},
        {"^effect_",     "effect.schema.json"},
        {"^unit_",       "unit.schema.json"},
        {"^building_",   "building.schema.json"},
        {"^tile_",       "tile.schema.json"},
        {"^techtree_",   "techtree.schema.json"},
        {"^hero_",       "hero.schema.json"},
        {"^ability_",    "ability.schema.json"},
        {"^projectile_", "projectile.schema.json"},
        {"^particle_",   "particle.schema.json"},
        {"^anim_",       "animation.schema.json"},
        {"^sound_",      "sound.schema.json"},
        {"^quest_",      "quest.schema.json"},
        {"^dialogue_",   "dialogue.schema.json"},
        {"^loot_",       "loot.schema.json"}
    };
}

SchemaValidator::SchemaValidator(const SchemaValidatorConfig& config)
    : SchemaValidator()
{
    m_config = config;
}

SchemaValidator::~SchemaValidator() {
    Shutdown();
}

SchemaValidator::SchemaValidator(SchemaValidator&& other) noexcept
    : m_config(std::move(other.m_config))
    , m_schemaCache(std::move(other.m_schemaCache))
    , m_customValidators(std::move(other.m_customValidators))
    , m_registeredIds(std::move(other.m_registeredIds))
    , m_assetRoot(std::move(other.m_assetRoot))
    , m_initialized(other.m_initialized)
    , m_idPatterns(std::move(other.m_idPatterns))
{
    other.m_initialized = false;
}

SchemaValidator& SchemaValidator::operator=(SchemaValidator&& other) noexcept {
    if (this != &other) {
        Shutdown();
        m_config = std::move(other.m_config);
        m_schemaCache = std::move(other.m_schemaCache);
        m_customValidators = std::move(other.m_customValidators);
        m_registeredIds = std::move(other.m_registeredIds);
        m_assetRoot = std::move(other.m_assetRoot);
        m_initialized = other.m_initialized;
        m_idPatterns = std::move(other.m_idPatterns);
        other.m_initialized = false;
    }
    return *this;
}

bool SchemaValidator::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Load all schemas from the schema directory
    const std::vector<std::string> schemaFiles = {
        "common.schema.json",
        "spell.schema.json",
        "effect.schema.json",
        "unit.schema.json",
        "building.schema.json",
        "tile.schema.json",
        "techtree.schema.json",
        "hero.schema.json",
        "ability.schema.json",
        "projectile.schema.json",
        "particle.schema.json",
        "animation.schema.json",
        "sound.schema.json",
        "quest.schema.json",
        "dialogue.schema.json",
        "loot.schema.json",
        "master.schema.json"
    };

    bool allLoaded = true;
    for (const auto& schemaFile : schemaFiles) {
        std::string schemaPath = m_config.schemaDirectory + schemaFile;
        if (!LoadSchema(schemaPath)) {
            // Log warning but continue - some schemas may not exist yet
            allLoaded = false;
        }
    }

    m_initialized = true;
    return allLoaded;
}

void SchemaValidator::Shutdown() {
    m_schemaCache.clear();
    m_customValidators.clear();
    m_registeredIds.clear();
    m_initialized = false;
}

bool SchemaValidator::ReloadSchemas() {
    m_schemaCache.clear();
    return Initialize();
}

bool SchemaValidator::CheckSchemaModified(const std::string& schemaId) {
    auto it = m_schemaCache.find(schemaId);
    if (it == m_schemaCache.end()) {
        return true; // Not loaded, needs loading
    }

    try {
        auto currentTime = std::filesystem::last_write_time(it->second.path);
        return currentTime > it->second.lastModified;
    } catch (...) {
        return false;
    }
}

bool SchemaValidator::LoadSchema(const std::string& schemaPath) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(schemaPath)) {
            return false;
        }

        // Read file content
        std::ifstream file(schemaPath);
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Extract schema ID from filename
        std::filesystem::path path(schemaPath);
        std::string schemaId = path.filename().string();

        // Parse JSON (placeholder - use actual JSON library)
        auto schema = std::make_shared<nlohmann::json>();
        // *schema = nlohmann::json::parse(content);

        // Cache the schema
        CachedSchema cached;
        cached.id = schemaId;
        cached.path = schemaPath;
        cached.schema = schema;
        cached.lastModified = std::filesystem::last_write_time(schemaPath);
        cached.valid = true;

        m_schemaCache[schemaId] = std::move(cached);

        return true;
    } catch (const std::exception& /*e*/) {
        return false;
    }
}

bool SchemaValidator::LoadSchemaFromString(const std::string& schemaId, const std::string& schemaJson) {
    try {
        auto schema = std::make_shared<nlohmann::json>();
        // *schema = nlohmann::json::parse(schemaJson);

        CachedSchema cached;
        cached.id = schemaId;
        cached.path = "<string>";
        cached.schema = schema;
        cached.lastModified = std::filesystem::file_time_type::clock::now();
        cached.valid = true;

        m_schemaCache[schemaId] = std::move(cached);

        return true;
    } catch (const std::exception& /*e*/) {
        return false;
    }
}

const nlohmann::json* SchemaValidator::GetSchema(const std::string& schemaId) const {
    auto it = m_schemaCache.find(schemaId);
    if (it != m_schemaCache.end() && it->second.valid) {
        return it->second.schema.get();
    }
    return nullptr;
}

bool SchemaValidator::HasSchema(const std::string& schemaId) const {
    return m_schemaCache.find(schemaId) != m_schemaCache.end();
}

std::vector<std::string> SchemaValidator::GetLoadedSchemaIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_schemaCache.size());
    for (const auto& [id, _] : m_schemaCache) {
        ids.push_back(id);
    }
    return ids;
}

ValidationResult SchemaValidator::Validate(
    const nlohmann::json& config,
    const std::string& schemaId
) const {
    ValidationResult result;
    result.schemaId = schemaId;

    // Get the schema
    const nlohmann::json* schema = GetSchema(schemaId);
    if (!schema) {
        ValidationError error;
        error.type = ValidationErrorType::SchemaNotFound;
        error.message = "Schema not found: " + schemaId;
        result.AddError(std::move(error));
        return result;
    }

    // Validate against schema
    ValidateAgainstSchema(config, *schema, "", result, 0);

    // Run custom validators
    auto customIt = m_customValidators.find(schemaId);
    if (customIt != m_customValidators.end()) {
        customIt->second(config, "", result);
    }

    return result;
}

ValidationResult SchemaValidator::ValidateFile(
    const std::string& configPath,
    const std::string& schemaId
) const {
    ValidationResult result;
    result.configPath = configPath;
    result.schemaId = schemaId;

    try {
        // Read file
        std::ifstream file(configPath);
        if (!file.is_open()) {
            ValidationError error;
            error.type = ValidationErrorType::CustomValidation;
            error.message = "Failed to open file: " + configPath;
            result.AddError(std::move(error));
            return result;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        return ValidateString(buffer.str(), schemaId);
    } catch (const std::exception& e) {
        ValidationError error;
        error.type = ValidationErrorType::CustomValidation;
        error.message = std::string("Exception while reading file: ") + e.what();
        result.AddError(std::move(error));
        return result;
    }
}

ValidationResult SchemaValidator::ValidateFileAutoDetect(
    const std::string& configPath
) const {
    ValidationResult result;
    result.configPath = configPath;

    try {
        // Read and parse file
        std::ifstream file(configPath);
        if (!file.is_open()) {
            ValidationError error;
            error.type = ValidationErrorType::CustomValidation;
            error.message = "Failed to open file: " + configPath;
            result.AddError(std::move(error));
            return result;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Parse JSON (placeholder)
        nlohmann::json config;
        // config = nlohmann::json::parse(content);

        // Detect schema type
        std::string schemaId = DetectSchemaType(config);
        if (schemaId.empty()) {
            ValidationError error;
            error.type = ValidationErrorType::SchemaNotFound;
            error.message = "Could not auto-detect schema type";
            result.AddError(std::move(error));
            return result;
        }

        return Validate(config, schemaId);
    } catch (const std::exception& e) {
        ValidationError error;
        error.type = ValidationErrorType::SchemaParseError;
        error.message = std::string("Exception: ") + e.what();
        result.AddError(std::move(error));
        return result;
    }
}

ValidationResult SchemaValidator::ValidateString(
    const std::string& configJson,
    const std::string& schemaId
) const {
    ValidationResult result;
    result.schemaId = schemaId;

    try {
        nlohmann::json config;
        // config = nlohmann::json::parse(configJson);

        return Validate(config, schemaId);
    } catch (const std::exception& e) {
        ValidationError error;
        error.type = ValidationErrorType::SchemaParseError;
        error.message = std::string("JSON parse error: ") + e.what();
        result.AddError(std::move(error));
        return result;
    }
}

std::unordered_map<std::string, ValidationResult> SchemaValidator::ValidateDirectory(
    const std::string& directory,
    bool recursive
) const {
    std::unordered_map<std::string, ValidationResult> results;

    try {
        auto options = recursive
            ? std::filesystem::directory_options::follow_directory_symlink
            : std::filesystem::directory_options::none;

        auto iterator = recursive
            ? std::filesystem::recursive_directory_iterator(directory, options)
            : std::filesystem::recursive_directory_iterator(directory);

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".json") {
                    std::string path = entry.path().string();
                    results[path] = ValidateFileAutoDetect(path);
                }
            }
        }
    } catch (const std::exception& /*e*/) {
        // Directory access error
    }

    return results;
}

std::string SchemaValidator::DetectSchemaType(const nlohmann::json& config) const {
    // Try to get ID field and match against patterns
    // This is a placeholder - implement with actual JSON library

    // Check for $schema field
    // if (config.contains("$schema")) {
    //     return config["$schema"].get<std::string>();
    // }

    // Check ID pattern
    // if (config.contains("id")) {
    //     std::string id = config["id"].get<std::string>();
    //     return DetectSchemaFromId(id);
    // }

    return "";
}

std::string SchemaValidator::DetectSchemaFromId(const std::string& id) const {
    for (const auto& [pattern, schemaId] : m_idPatterns) {
        std::regex rx(pattern);
        if (std::regex_search(id, rx)) {
            return schemaId;
        }
    }
    return "";
}

void SchemaValidator::RegisterCustomValidator(
    const std::string& schemaId,
    CustomValidator validator
) {
    m_customValidators[schemaId] = std::move(validator);
}

void SchemaValidator::RemoveCustomValidator(const std::string& schemaId) {
    m_customValidators.erase(schemaId);
}

void SchemaValidator::SetAssetRoot(const std::string& assetRoot) {
    m_assetRoot = assetRoot;
}

bool SchemaValidator::AssetExists(const std::string& assetPath) const {
    std::string fullPath = m_assetRoot.empty() ? assetPath : (m_assetRoot + "/" + assetPath);
    return std::filesystem::exists(fullPath);
}

void SchemaValidator::RegisterId(const std::string& schemaType, const std::string& id) {
    m_registeredIds[schemaType].insert(id);
}

bool SchemaValidator::IsIdRegistered(
    const std::string& schemaType,
    const std::string& id
) const {
    auto it = m_registeredIds.find(schemaType);
    if (it == m_registeredIds.end()) {
        return false;
    }
    return it->second.find(id) != it->second.end();
}

void SchemaValidator::ClearRegisteredIds() {
    m_registeredIds.clear();
}

void SchemaValidator::LoadIdsFromDirectory(const std::string& directory) {
    // Scan directory for config files and register their IDs
    // Implementation depends on actual JSON library
}

void SchemaValidator::SetConfig(const SchemaValidatorConfig& config) {
    m_config = config;
}

std::string SchemaValidator::GenerateDocumentation(const std::string& schemaId) const {
    const nlohmann::json* schema = GetSchema(schemaId);
    if (!schema) {
        return "Schema not found: " + schemaId;
    }

    std::ostringstream doc;

    // Generate markdown documentation from schema
    // This is a placeholder - implement with actual JSON library
    doc << "# " << schemaId << "\n\n";
    doc << "## Description\n\n";
    doc << "Schema documentation generated from JSON Schema definition.\n\n";
    doc << "## Properties\n\n";
    doc << "| Property | Type | Required | Description |\n";
    doc << "|----------|------|----------|-------------|\n";

    // Parse schema properties and generate documentation
    // ...

    return doc.str();
}

std::unordered_map<std::string, std::string> SchemaValidator::GenerateAllDocumentation() const {
    std::unordered_map<std::string, std::string> docs;
    for (const auto& [schemaId, _] : m_schemaCache) {
        docs[schemaId] = GenerateDocumentation(schemaId);
    }
    return docs;
}

// ============================================================================
// Private Validation Methods (Placeholders)
// ============================================================================

void SchemaValidator::ValidateAgainstSchema(
    const nlohmann::json& value,
    const nlohmann::json& schema,
    const std::string& path,
    ValidationResult& result,
    int depth
) const {
    if (depth > m_config.maxDepth) {
        ValidationError error;
        error.type = ValidationErrorType::CircularReference;
        error.path = path;
        error.message = "Maximum validation depth exceeded";
        result.AddError(std::move(error));
        return;
    }

    if (result.errors.size() >= static_cast<size_t>(m_config.maxErrors)) {
        return; // Stop after max errors
    }

    // Placeholder - implement actual validation with JSON library
    // This would check:
    // - type
    // - required properties
    // - enum values
    // - patterns
    // - minimum/maximum
    // - minLength/maxLength
    // - minItems/maxItems
    // - $ref resolution
    // - oneOf/anyOf/allOf
    // - additionalProperties
}

void SchemaValidator::ValidateType(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*schema*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/
) const {
    // Placeholder - implement type validation
}

void SchemaValidator::ValidateObject(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*schema*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/,
    int /*depth*/
) const {
    // Placeholder - implement object validation
}

void SchemaValidator::ValidateArray(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*schema*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/,
    int /*depth*/
) const {
    // Placeholder - implement array validation
}

void SchemaValidator::ValidateString(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*schema*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/
) const {
    // Placeholder - implement string validation
}

void SchemaValidator::ValidateNumber(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*schema*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/
) const {
    // Placeholder - implement number validation
}

const nlohmann::json* SchemaValidator::ResolveRef(
    const std::string& ref,
    const nlohmann::json& /*rootSchema*/
) const {
    // Parse $ref and resolve to target schema
    // Handle both local (#/definitions/...) and external (file.schema.json#/...) refs

    if (ref.empty()) {
        return nullptr;
    }

    // External reference (file.schema.json#/...)
    size_t hashPos = ref.find('#');
    if (hashPos != std::string::npos && hashPos > 0) {
        std::string schemaFile = ref.substr(0, hashPos);
        // std::string jsonPath = ref.substr(hashPos + 1);

        const nlohmann::json* schema = GetSchema(schemaFile);
        if (!schema) {
            return nullptr;
        }

        // Navigate to the path within the schema
        // Implementation depends on JSON library
        return schema;
    }

    // Local reference (#/definitions/...)
    if (ref[0] == '#') {
        // Navigate within root schema
        // Implementation depends on JSON library
    }

    return nullptr;
}

void SchemaValidator::ValidateEnum(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*enumValues*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/
) const {
    // Placeholder - implement enum validation
}

void SchemaValidator::ValidatePattern(
    const std::string& value,
    const std::string& pattern,
    const std::string& path,
    ValidationResult& result
) const {
    try {
        std::regex rx(pattern);
        if (!std::regex_match(value, rx)) {
            ValidationError error;
            error.type = ValidationErrorType::PatternMismatch;
            error.path = path;
            error.message = "Value does not match pattern";
            error.expected = pattern;
            error.actual = value;
            result.AddError(std::move(error));
        }
    } catch (const std::regex_error& e) {
        ValidationError error;
        error.type = ValidationErrorType::SchemaInvalid;
        error.path = path;
        error.message = std::string("Invalid regex pattern: ") + e.what();
        result.AddWarning(std::move(error));
    }
}

void SchemaValidator::ValidateOneOf(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*oneOf*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/,
    int /*depth*/
) const {
    // Placeholder - implement oneOf validation
}

void SchemaValidator::ValidateAllOf(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*allOf*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/,
    int /*depth*/
) const {
    // Placeholder - implement allOf validation
}

void SchemaValidator::ValidateAnyOf(
    const nlohmann::json& /*value*/,
    const nlohmann::json& /*anyOf*/,
    const std::string& /*path*/,
    ValidationResult& /*result*/,
    int /*depth*/
) const {
    // Placeholder - implement anyOf validation
}

void SchemaValidator::ValidateAssetPath(
    const std::string& assetPath,
    const std::string& path,
    ValidationResult& result
) const {
    if (!m_config.checkAssetPaths) {
        return;
    }

    if (!AssetExists(assetPath)) {
        ValidationError error;
        error.type = ValidationErrorType::AssetNotFound;
        error.path = path;
        error.message = "Asset file not found: " + assetPath;
        result.AddWarning(std::move(error));
    }
}

std::string SchemaValidator::GetTypeString(const nlohmann::json& /*value*/) const {
    // Return JSON type as string
    // Implementation depends on JSON library
    return "unknown";
}

std::string SchemaValidator::GetSchemaPath(const std::string& schemaId) const {
    return m_config.schemaDirectory + schemaId;
}

// ============================================================================
// Global Validator Instance
// ============================================================================

static std::unique_ptr<SchemaValidator> g_schemaValidator;

SchemaValidator& GetSchemaValidator() {
    if (!g_schemaValidator) {
        g_schemaValidator = std::make_unique<SchemaValidator>();
    }
    return *g_schemaValidator;
}

bool InitializeSchemaValidator(const SchemaValidatorConfig& config) {
    g_schemaValidator = std::make_unique<SchemaValidator>(config);
    return g_schemaValidator->Initialize();
}

void ShutdownSchemaValidator() {
    if (g_schemaValidator) {
        g_schemaValidator->Shutdown();
        g_schemaValidator.reset();
    }
}

} // namespace Vehement
