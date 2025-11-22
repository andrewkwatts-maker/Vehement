#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <filesystem>

// Forward declarations for JSON library (nlohmann/json or similar)
namespace nlohmann {
    class json;
}

namespace Vehement {

// ============================================================================
// Validation Error Types
// ============================================================================

/**
 * @brief Severity level for validation errors
 */
enum class ValidationSeverity {
    Error,      // Critical error, config is invalid
    Warning,    // Non-critical issue, config may work
    Info        // Informational message
};

/**
 * @brief Type of validation error
 */
enum class ValidationErrorType {
    // Schema errors
    SchemaNotFound,
    SchemaInvalid,
    SchemaParseError,

    // Type errors
    TypeMismatch,
    InvalidEnum,
    PatternMismatch,

    // Value errors
    ValueOutOfRange,
    ValueTooLong,
    ValueTooShort,

    // Structure errors
    MissingRequired,
    UnknownProperty,
    ArrayTooShort,
    ArrayTooLong,

    // Reference errors
    InvalidReference,
    CircularReference,
    UnresolvedReference,

    // Custom validation errors
    CustomValidation,
    DuplicateId,
    AssetNotFound
};

/**
 * @brief Represents a single validation error
 */
struct ValidationError {
    ValidationSeverity severity = ValidationSeverity::Error;
    ValidationErrorType type = ValidationErrorType::CustomValidation;
    std::string path;           // JSON path to the error (e.g., "/effects/0/damage")
    std::string message;        // Human-readable error message
    std::string expected;       // What was expected
    std::string actual;         // What was found
    int line = -1;              // Line number in source file (if available)
    int column = -1;            // Column number in source file (if available)

    /**
     * @brief Format error as string
     */
    [[nodiscard]] std::string ToString() const;

    /**
     * @brief Get severity as string
     */
    [[nodiscard]] const char* GetSeverityString() const;
};

/**
 * @brief Result of a validation operation
 */
struct ValidationResult {
    bool valid = true;
    std::vector<ValidationError> errors;
    std::vector<ValidationError> warnings;
    std::string schemaId;
    std::string configPath;

    /**
     * @brief Check if validation passed (no errors)
     */
    [[nodiscard]] bool IsValid() const { return valid && errors.empty(); }

    /**
     * @brief Check if there are any warnings
     */
    [[nodiscard]] bool HasWarnings() const { return !warnings.empty(); }

    /**
     * @brief Get total issue count
     */
    [[nodiscard]] size_t GetIssueCount() const { return errors.size() + warnings.size(); }

    /**
     * @brief Get formatted error report
     */
    [[nodiscard]] std::string GetReport() const;

    /**
     * @brief Add an error
     */
    void AddError(ValidationError error);

    /**
     * @brief Add a warning
     */
    void AddWarning(ValidationError error);

    /**
     * @brief Merge another result into this one
     */
    void Merge(const ValidationResult& other);
};

// ============================================================================
// Schema Cache
// ============================================================================

/**
 * @brief Cached schema with metadata
 */
struct CachedSchema {
    std::string id;
    std::string path;
    std::shared_ptr<nlohmann::json> schema;
    std::filesystem::file_time_type lastModified;
    bool valid = false;
};

// ============================================================================
// Schema Validator
// ============================================================================

/**
 * @brief Configuration for the schema validator
 */
struct SchemaValidatorConfig {
    std::string schemaDirectory = "game/assets/schemas/";
    bool cacheSchemas = true;
    bool allowUnknownProperties = false;
    bool checkAssetPaths = true;
    bool checkReferences = true;
    bool verboseErrors = true;
    int maxErrors = 100;            // Stop after this many errors
    int maxDepth = 100;             // Max recursion depth
};

/**
 * @brief Custom validation callback
 * @param json The JSON value being validated
 * @param path The JSON path to the value
 * @param result The validation result to add errors to
 */
using CustomValidator = std::function<void(
    const nlohmann::json& json,
    const std::string& path,
    ValidationResult& result
)>;

/**
 * @brief JSON Schema validator for game configuration files
 *
 * Features:
 * - Load and cache JSON Schema files
 * - Validate configs against schemas
 * - Detailed error messages with paths
 * - Support for $ref resolution
 * - Custom validation callbacks
 * - Asset path verification
 * - Reference validation
 */
class SchemaValidator {
public:
    SchemaValidator();
    explicit SchemaValidator(const SchemaValidatorConfig& config);
    ~SchemaValidator();

    // Non-copyable, movable
    SchemaValidator(const SchemaValidator&) = delete;
    SchemaValidator& operator=(const SchemaValidator&) = delete;
    SchemaValidator(SchemaValidator&&) noexcept;
    SchemaValidator& operator=(SchemaValidator&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the validator and load schemas
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    /**
     * @brief Reload all schemas from disk
     */
    bool ReloadSchemas();

    /**
     * @brief Check if a specific schema needs reloading
     */
    bool CheckSchemaModified(const std::string& schemaId);

    // =========================================================================
    // Schema Loading
    // =========================================================================

    /**
     * @brief Load a schema from file
     * @param schemaPath Path to schema file
     * @return true if schema loaded successfully
     */
    bool LoadSchema(const std::string& schemaPath);

    /**
     * @brief Load a schema from string
     * @param schemaId Schema identifier
     * @param schemaJson Schema JSON string
     * @return true if schema loaded successfully
     */
    bool LoadSchemaFromString(const std::string& schemaId, const std::string& schemaJson);

    /**
     * @brief Get a loaded schema by ID
     * @param schemaId Schema identifier (e.g., "spell.schema.json")
     * @return Pointer to schema or nullptr if not found
     */
    [[nodiscard]] const nlohmann::json* GetSchema(const std::string& schemaId) const;

    /**
     * @brief Check if a schema is loaded
     */
    [[nodiscard]] bool HasSchema(const std::string& schemaId) const;

    /**
     * @brief Get list of loaded schema IDs
     */
    [[nodiscard]] std::vector<std::string> GetLoadedSchemaIds() const;

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate a JSON config against a schema
     * @param config The JSON config to validate
     * @param schemaId Schema ID to validate against
     * @return Validation result
     */
    [[nodiscard]] ValidationResult Validate(
        const nlohmann::json& config,
        const std::string& schemaId
    ) const;

    /**
     * @brief Validate a config file against a schema
     * @param configPath Path to config file
     * @param schemaId Schema ID to validate against
     * @return Validation result
     */
    [[nodiscard]] ValidationResult ValidateFile(
        const std::string& configPath,
        const std::string& schemaId
    ) const;

    /**
     * @brief Validate a config file, auto-detecting schema from file content
     * @param configPath Path to config file
     * @return Validation result
     */
    [[nodiscard]] ValidationResult ValidateFileAutoDetect(
        const std::string& configPath
    ) const;

    /**
     * @brief Validate a config string against a schema
     * @param configJson JSON string to validate
     * @param schemaId Schema ID to validate against
     * @return Validation result
     */
    [[nodiscard]] ValidationResult ValidateString(
        const std::string& configJson,
        const std::string& schemaId
    ) const;

    /**
     * @brief Validate all config files in a directory
     * @param directory Directory to scan
     * @param recursive Whether to scan subdirectories
     * @return Map of file path to validation result
     */
    [[nodiscard]] std::unordered_map<std::string, ValidationResult> ValidateDirectory(
        const std::string& directory,
        bool recursive = true
    ) const;

    // =========================================================================
    // Schema Detection
    // =========================================================================

    /**
     * @brief Detect schema type from config content
     * @param config The JSON config
     * @return Schema ID or empty string if cannot detect
     */
    [[nodiscard]] std::string DetectSchemaType(const nlohmann::json& config) const;

    /**
     * @brief Detect schema type from ID pattern
     * @param id The config ID string
     * @return Schema ID or empty string if cannot detect
     */
    [[nodiscard]] std::string DetectSchemaFromId(const std::string& id) const;

    // =========================================================================
    // Custom Validators
    // =========================================================================

    /**
     * @brief Register a custom validator for a schema type
     * @param schemaId Schema ID
     * @param validator Validation callback
     */
    void RegisterCustomValidator(
        const std::string& schemaId,
        CustomValidator validator
    );

    /**
     * @brief Remove a custom validator
     */
    void RemoveCustomValidator(const std::string& schemaId);

    // =========================================================================
    // Asset Path Validation
    // =========================================================================

    /**
     * @brief Set the asset root directory for path validation
     */
    void SetAssetRoot(const std::string& assetRoot);

    /**
     * @brief Check if an asset path exists
     */
    [[nodiscard]] bool AssetExists(const std::string& assetPath) const;

    // =========================================================================
    // Reference Tracking
    // =========================================================================

    /**
     * @brief Register a valid ID for reference validation
     * @param schemaType Schema type (e.g., "spell", "effect")
     * @param id The ID to register
     */
    void RegisterId(const std::string& schemaType, const std::string& id);

    /**
     * @brief Check if an ID is registered
     */
    [[nodiscard]] bool IsIdRegistered(
        const std::string& schemaType,
        const std::string& id
    ) const;

    /**
     * @brief Clear all registered IDs
     */
    void ClearRegisteredIds();

    /**
     * @brief Load registered IDs from config files in a directory
     */
    void LoadIdsFromDirectory(const std::string& directory);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const SchemaValidatorConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void SetConfig(const SchemaValidatorConfig& config);

    // =========================================================================
    // Documentation Generation
    // =========================================================================

    /**
     * @brief Generate markdown documentation for a schema
     * @param schemaId Schema ID
     * @return Markdown documentation string
     */
    [[nodiscard]] std::string GenerateDocumentation(const std::string& schemaId) const;

    /**
     * @brief Generate documentation for all schemas
     * @return Map of schema ID to documentation
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> GenerateAllDocumentation() const;

private:
    // Core validation implementation
    void ValidateAgainstSchema(
        const nlohmann::json& value,
        const nlohmann::json& schema,
        const std::string& path,
        ValidationResult& result,
        int depth
    ) const;

    // Type validators
    void ValidateType(
        const nlohmann::json& value,
        const nlohmann::json& schema,
        const std::string& path,
        ValidationResult& result
    ) const;

    void ValidateObject(
        const nlohmann::json& value,
        const nlohmann::json& schema,
        const std::string& path,
        ValidationResult& result,
        int depth
    ) const;

    void ValidateArray(
        const nlohmann::json& value,
        const nlohmann::json& schema,
        const std::string& path,
        ValidationResult& result,
        int depth
    ) const;

    void ValidateString(
        const nlohmann::json& value,
        const nlohmann::json& schema,
        const std::string& path,
        ValidationResult& result
    ) const;

    void ValidateNumber(
        const nlohmann::json& value,
        const nlohmann::json& schema,
        const std::string& path,
        ValidationResult& result
    ) const;

    // Reference resolution
    const nlohmann::json* ResolveRef(
        const std::string& ref,
        const nlohmann::json& rootSchema
    ) const;

    // Helper methods
    void ValidateEnum(
        const nlohmann::json& value,
        const nlohmann::json& enumValues,
        const std::string& path,
        ValidationResult& result
    ) const;

    void ValidatePattern(
        const std::string& value,
        const std::string& pattern,
        const std::string& path,
        ValidationResult& result
    ) const;

    void ValidateOneOf(
        const nlohmann::json& value,
        const nlohmann::json& oneOf,
        const std::string& path,
        ValidationResult& result,
        int depth
    ) const;

    void ValidateAllOf(
        const nlohmann::json& value,
        const nlohmann::json& allOf,
        const std::string& path,
        ValidationResult& result,
        int depth
    ) const;

    void ValidateAnyOf(
        const nlohmann::json& value,
        const nlohmann::json& anyOf,
        const std::string& path,
        ValidationResult& result,
        int depth
    ) const;

    // Asset validation
    void ValidateAssetPath(
        const std::string& assetPath,
        const std::string& path,
        ValidationResult& result
    ) const;

    // Utility
    std::string GetTypeString(const nlohmann::json& value) const;
    std::string GetSchemaPath(const std::string& schemaId) const;

    // Member variables
    SchemaValidatorConfig m_config;
    std::unordered_map<std::string, CachedSchema> m_schemaCache;
    std::unordered_map<std::string, CustomValidator> m_customValidators;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_registeredIds;
    std::string m_assetRoot;
    bool m_initialized = false;

    // Schema ID patterns for auto-detection
    std::vector<std::pair<std::string, std::string>> m_idPatterns;
};

// ============================================================================
// Global Validator Instance
// ============================================================================

/**
 * @brief Get the global schema validator instance
 */
SchemaValidator& GetSchemaValidator();

/**
 * @brief Initialize the global schema validator
 */
bool InitializeSchemaValidator(const SchemaValidatorConfig& config = {});

/**
 * @brief Shutdown the global schema validator
 */
void ShutdownSchemaValidator();

} // namespace Vehement
