#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <unordered_set>

namespace Nova {
namespace Scripting {

/**
 * @brief Severity level for validation issues
 */
enum class ValidationSeverity {
    Error,      // Script won't execute
    Warning,    // Script may have issues
    Info,       // Suggestion/style issue
    Hint        // Minor improvement
};

/**
 * @brief Single validation issue
 */
struct ValidationIssue {
    ValidationSeverity severity;
    int line;
    int column;
    int endColumn;
    std::string code;           // Error code (e.g., "E001", "W002")
    std::string message;
    std::string source;         // "syntax", "import", "type", "api", "security"
    std::string suggestion;     // Quick-fix suggestion

    [[nodiscard]] bool IsError() const { return severity == ValidationSeverity::Error; }
};

/**
 * @brief Complete validation result
 */
struct ValidationResult {
    bool valid = true;
    std::vector<ValidationIssue> issues;
    int errorCount = 0;
    int warningCount = 0;

    void AddIssue(const ValidationIssue& issue) {
        issues.push_back(issue);
        if (issue.severity == ValidationSeverity::Error) {
            errorCount++;
            valid = false;
        } else if (issue.severity == ValidationSeverity::Warning) {
            warningCount++;
        }
    }
};

/**
 * @brief Validation options
 */
struct ValidationOptions {
    bool checkSyntax = true;          // Basic Python syntax
    bool checkImports = true;         // Validate imports are allowed
    bool checkTypes = true;           // Type hint validation
    bool checkGameAPI = true;         // Validate game API usage
    bool checkSecurity = true;        // Security scanning
    bool checkStyle = false;          // PEP8 style (optional)

    std::vector<std::string> allowedImports;  // Whitelist
    std::vector<std::string> blockedImports;  // Blacklist
    bool allowFileAccess = false;
    bool allowNetworkAccess = false;
    bool allowOsAccess = false;
    bool allowSubprocess = false;
};

/**
 * @brief Python script validator
 *
 * Performs multiple validation passes:
 * 1. Syntax checking - Parse Python code
 * 2. Import validation - Check allowed/blocked modules
 * 3. Type hint checking - Validate type annotations
 * 4. Game API validation - Check API usage correctness
 * 5. Security scanning - Block dangerous operations
 *
 * Usage:
 * @code
 * ScriptValidator validator;
 * validator.Initialize();
 *
 * ValidationOptions opts;
 * opts.checkSecurity = true;
 *
 * auto result = validator.Validate(code, opts);
 * if (!result.valid) {
 *     for (const auto& issue : result.issues) {
 *         std::cout << issue.line << ": " << issue.message << "\n";
 *     }
 * }
 * @endcode
 */
class ScriptValidator {
public:
    ScriptValidator();
    ~ScriptValidator();

    // Non-copyable
    ScriptValidator(const ScriptValidator&) = delete;
    ScriptValidator& operator=(const ScriptValidator&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate Python code
     * @param code Source code to validate
     * @param options Validation options
     * @return Validation result
     */
    ValidationResult Validate(const std::string& code,
                             const ValidationOptions& options = {});

    /**
     * @brief Validate file
     */
    ValidationResult ValidateFile(const std::string& filePath,
                                  const ValidationOptions& options = {});

    /**
     * @brief Quick syntax check only
     */
    bool CheckSyntax(const std::string& code);

    /**
     * @brief Check if code is safe to execute
     */
    bool IsSafeToExecute(const std::string& code);

    // =========================================================================
    // Individual Checks
    // =========================================================================

    /**
     * @brief Check Python syntax
     */
    std::vector<ValidationIssue> CheckPythonSyntax(const std::string& code);

    /**
     * @brief Check imports
     */
    std::vector<ValidationIssue> CheckImports(const std::string& code,
                                              const ValidationOptions& options);

    /**
     * @brief Check type hints
     */
    std::vector<ValidationIssue> CheckTypeHints(const std::string& code);

    /**
     * @brief Check game API usage
     */
    std::vector<ValidationIssue> CheckGameAPIUsage(const std::string& code);

    /**
     * @brief Security scan
     */
    std::vector<ValidationIssue> SecurityScan(const std::string& code,
                                              const ValidationOptions& options);

    /**
     * @brief Check code style (PEP8)
     */
    std::vector<ValidationIssue> CheckStyle(const std::string& code);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set default allowed imports
     */
    void SetDefaultAllowedImports(const std::vector<std::string>& imports);

    /**
     * @brief Add to default blocked imports
     */
    void AddBlockedImport(const std::string& module);

    /**
     * @brief Register game API function for validation
     */
    void RegisterGameAPIFunction(const std::string& name, const std::string& signature);

    /**
     * @brief Get default options
     */
    [[nodiscard]] ValidationOptions GetDefaultOptions() const { return m_defaultOptions; }

    /**
     * @brief Set default options
     */
    void SetDefaultOptions(const ValidationOptions& options) { m_defaultOptions = options; }

private:
    // Parsing helpers
    struct Token {
        enum Type { Identifier, Keyword, String, Number, Operator, Newline, Indent, Dedent, EndOfFile };
        Type type;
        std::string value;
        int line;
        int column;
    };

    std::vector<Token> Tokenize(const std::string& code);
    bool ParseExpression(const std::vector<Token>& tokens, size_t& pos);

    // Validation passes
    void CheckBracketMatching(const std::string& code, ValidationResult& result);
    void CheckStringTermination(const std::string& code, ValidationResult& result);
    void CheckIndentation(const std::string& code, ValidationResult& result);
    void CheckFunctionDefs(const std::string& code, ValidationResult& result);
    void CheckClassDefs(const std::string& code, ValidationResult& result);

    // Import analysis
    std::vector<std::string> ExtractImports(const std::string& code);
    bool IsImportAllowed(const std::string& module, const ValidationOptions& options);

    // Security patterns
    struct SecurityPattern {
        std::string pattern;
        std::string description;
        ValidationSeverity severity;
    };
    std::vector<SecurityPattern> m_securityPatterns;

    // Game API registry
    struct APIFunction {
        std::string name;
        std::string signature;
        std::vector<std::string> paramTypes;
        std::string returnType;
    };
    std::unordered_map<std::string, APIFunction> m_gameAPIFunctions;

    // State
    bool m_initialized = false;
    ValidationOptions m_defaultOptions;

    // Default lists
    std::unordered_set<std::string> m_allowedImports;
    std::unordered_set<std::string> m_blockedImports;

    // Python keywords
    static const std::unordered_set<std::string> s_pythonKeywords;
};

} // namespace Scripting
} // namespace Nova
