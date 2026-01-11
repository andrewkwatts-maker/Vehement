#include "ScriptValidator.hpp"

#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace Nova {
namespace Scripting {

const std::unordered_set<std::string> ScriptValidator::s_pythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield"
};

ScriptValidator::ScriptValidator() {
    // Initialize default allowed imports (safe modules)
    m_allowedImports = {
        "math", "random", "time", "datetime", "collections",
        "itertools", "functools", "operator", "string", "re",
        "json", "typing", "enum", "dataclasses", "copy",
        "nova", "game", "entity", "combat", "ai"  // Game modules
    };

    // Default blocked imports (dangerous)
    m_blockedImports = {
        "os", "sys", "subprocess", "shutil", "socket", "http",
        "urllib", "requests", "ftplib", "smtplib", "ssl",
        "multiprocessing", "threading", "ctypes", "pickle",
        "marshal", "shelve", "dbm", "sqlite3", "builtins",
        "__builtins__", "importlib", "imp", "code", "codeop",
        "compile", "exec", "eval", "__import__"
    };

    // Security patterns to detect
    m_securityPatterns = {
        {"eval\\s*\\(", "Use of eval() is not allowed", ValidationSeverity::Error},
        {"exec\\s*\\(", "Use of exec() is not allowed", ValidationSeverity::Error},
        {"compile\\s*\\(", "Use of compile() is not allowed", ValidationSeverity::Error},
        {"__import__\\s*\\(", "Use of __import__() is not allowed", ValidationSeverity::Error},
        {"open\\s*\\(", "File access is not allowed", ValidationSeverity::Error},
        {"globals\\s*\\(\\)", "Access to globals() is not allowed", ValidationSeverity::Warning},
        {"locals\\s*\\(\\)", "Access to locals() is not allowed", ValidationSeverity::Warning},
        {"getattr\\s*\\(.+,\\s*['\"]__", "Access to dunder attributes via getattr is suspicious", ValidationSeverity::Warning},
        {"setattr\\s*\\(.+,\\s*['\"]__", "Setting dunder attributes via setattr is suspicious", ValidationSeverity::Warning},
        {"__class__", "Direct access to __class__ is suspicious", ValidationSeverity::Warning},
        {"__bases__", "Direct access to __bases__ is suspicious", ValidationSeverity::Warning},
        {"__subclasses__", "Access to __subclasses__ is not allowed", ValidationSeverity::Error},
        {"__globals__", "Access to __globals__ is not allowed", ValidationSeverity::Error},
        {"__code__", "Access to __code__ is not allowed", ValidationSeverity::Error},
    };
}

ScriptValidator::~ScriptValidator() {
    Shutdown();
}

bool ScriptValidator::Initialize() {
    if (m_initialized) return true;

    // Register game API functions
    RegisterGameAPIFunction("spawn_entity", "spawn_entity(type: str, x: float, y: float, z: float) -> int");
    RegisterGameAPIFunction("despawn_entity", "despawn_entity(entity_id: int) -> None");
    RegisterGameAPIFunction("get_position", "get_position(entity_id: int) -> Vec3");
    RegisterGameAPIFunction("set_position", "set_position(entity_id: int, x: float, y: float, z: float) -> None");
    RegisterGameAPIFunction("damage", "damage(target_id: int, amount: float, source_id: int = 0) -> None");
    RegisterGameAPIFunction("heal", "heal(target_id: int, amount: float) -> None");
    RegisterGameAPIFunction("get_health", "get_health(entity_id: int) -> float");
    RegisterGameAPIFunction("is_alive", "is_alive(entity_id: int) -> bool");
    RegisterGameAPIFunction("find_entities_in_radius", "find_entities_in_radius(x: float, y: float, z: float, radius: float) -> List[int]");
    RegisterGameAPIFunction("get_distance", "get_distance(entity1: int, entity2: int) -> float");
    RegisterGameAPIFunction("play_sound", "play_sound(name: str, x: float = 0, y: float = 0, z: float = 0) -> None");
    RegisterGameAPIFunction("spawn_effect", "spawn_effect(name: str, x: float, y: float, z: float) -> None");
    RegisterGameAPIFunction("show_notification", "show_notification(message: str, duration: float = 3.0) -> None");
    RegisterGameAPIFunction("get_delta_time", "get_delta_time() -> float");
    RegisterGameAPIFunction("get_game_time", "get_game_time() -> float");
    RegisterGameAPIFunction("random", "random() -> float");
    RegisterGameAPIFunction("random_range", "random_range(min: float, max: float) -> float");
    RegisterGameAPIFunction("log", "log(message: str) -> None");

    m_initialized = true;
    return true;
}

void ScriptValidator::Shutdown() {
    m_gameAPIFunctions.clear();
    m_initialized = false;
}

ValidationResult ScriptValidator::Validate(const std::string& code,
                                           const ValidationOptions& options) {
    ValidationResult result;

    if (code.empty()) {
        return result;  // Empty is valid
    }

    // Run validation passes based on options
    if (options.checkSyntax) {
        auto issues = CheckPythonSyntax(code);
        for (auto& issue : issues) {
            result.AddIssue(issue);
        }
    }

    if (options.checkImports) {
        auto issues = CheckImports(code, options);
        for (auto& issue : issues) {
            result.AddIssue(issue);
        }
    }

    if (options.checkTypes) {
        auto issues = CheckTypeHints(code);
        for (auto& issue : issues) {
            result.AddIssue(issue);
        }
    }

    if (options.checkGameAPI) {
        auto issues = CheckGameAPIUsage(code);
        for (auto& issue : issues) {
            result.AddIssue(issue);
        }
    }

    if (options.checkSecurity) {
        auto issues = SecurityScan(code, options);
        for (auto& issue : issues) {
            result.AddIssue(issue);
        }
    }

    if (options.checkStyle) {
        auto issues = CheckStyle(code);
        for (auto& issue : issues) {
            result.AddIssue(issue);
        }
    }

    return result;
}

ValidationResult ScriptValidator::ValidateFile(const std::string& filePath,
                                               const ValidationOptions& options) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        ValidationResult result;
        result.valid = false;
        result.AddIssue({
            ValidationSeverity::Error, 0, 0, 0, "E000",
            "Cannot open file: " + filePath, "file", ""
        });
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return Validate(buffer.str(), options);
}

bool ScriptValidator::CheckSyntax(const std::string& code) {
    auto issues = CheckPythonSyntax(code);
    for (const auto& issue : issues) {
        if (issue.severity == ValidationSeverity::Error) {
            return false;
        }
    }
    return true;
}

bool ScriptValidator::IsSafeToExecute(const std::string& code) {
    ValidationOptions opts;
    opts.checkSyntax = true;
    opts.checkImports = true;
    opts.checkSecurity = true;
    opts.checkTypes = false;
    opts.checkGameAPI = false;
    opts.checkStyle = false;

    auto result = Validate(code, opts);
    return result.valid;
}

std::vector<ValidationIssue> ScriptValidator::CheckPythonSyntax(const std::string& code) {
    std::vector<ValidationIssue> issues;
    ValidationResult tempResult;

    CheckBracketMatching(code, tempResult);
    CheckStringTermination(code, tempResult);
    CheckIndentation(code, tempResult);
    CheckFunctionDefs(code, tempResult);
    CheckClassDefs(code, tempResult);

    return tempResult.issues;
}

void ScriptValidator::CheckBracketMatching(const std::string& code, ValidationResult& result) {
    std::vector<std::pair<char, int>> stack;  // bracket, line
    bool inString = false;
    char stringChar = 0;
    int line = 1;
    int column = 0;

    for (size_t i = 0; i < code.length(); ++i) {
        char c = code[i];
        column++;

        if (c == '\n') {
            line++;
            column = 0;
            continue;
        }

        // Handle strings
        if (!inString) {
            if (c == '"' || c == '\'') {
                // Check for triple quotes
                if (i + 2 < code.length() && code[i+1] == c && code[i+2] == c) {
                    i += 2;
                    inString = true;
                    stringChar = c;
                    continue;
                }
                inString = true;
                stringChar = c;
            } else if (c == '#') {
                // Skip comment
                while (i < code.length() && code[i] != '\n') i++;
                if (i < code.length()) {
                    line++;
                    column = 0;
                }
            } else if (c == '(' || c == '[' || c == '{') {
                stack.push_back({c, line});
            } else if (c == ')' || c == ']' || c == '}') {
                char expected = (c == ')') ? '(' : (c == ']') ? '[' : '{';
                if (stack.empty()) {
                    result.AddIssue({
                        ValidationSeverity::Error, line, column, column + 1, "E001",
                        std::string("Unmatched closing bracket '") + c + "'",
                        "syntax", ""
                    });
                } else if (stack.back().first != expected) {
                    result.AddIssue({
                        ValidationSeverity::Error, line, column, column + 1, "E002",
                        std::string("Mismatched bracket: expected '") +
                        (stack.back().first == '(' ? ')' : stack.back().first == '[' ? ']' : '}') +
                        "' but found '" + c + "'",
                        "syntax", ""
                    });
                } else {
                    stack.pop_back();
                }
            }
        } else {
            // In string
            if (c == '\\' && i + 1 < code.length()) {
                i++;  // Skip escaped character
            } else if (c == stringChar) {
                // Check for triple quotes ending
                if (i + 2 < code.length() && code[i+1] == stringChar && code[i+2] == stringChar) {
                    i += 2;
                }
                inString = false;
            }
        }
    }

    // Check for unclosed brackets
    for (const auto& bracket : stack) {
        result.AddIssue({
            ValidationSeverity::Error, bracket.second, 0, 0, "E003",
            std::string("Unclosed bracket '") + bracket.first + "'",
            "syntax", "Add closing bracket"
        });
    }
}

void ScriptValidator::CheckStringTermination(const std::string& code, ValidationResult& result) {
    bool inString = false;
    bool inTripleString = false;
    char stringChar = 0;
    int stringStartLine = 0;
    int line = 1;

    for (size_t i = 0; i < code.length(); ++i) {
        char c = code[i];

        if (c == '\n') {
            // Check for unterminated single-line string
            if (inString && !inTripleString) {
                result.AddIssue({
                    ValidationSeverity::Error, stringStartLine, 0, 0, "E004",
                    "Unterminated string literal",
                    "syntax", "Close the string with matching quote"
                });
                inString = false;
            }
            line++;
            continue;
        }

        if (!inString) {
            if (c == '#') {
                while (i < code.length() && code[i] != '\n') i++;
                continue;
            }
            if (c == '"' || c == '\'') {
                stringChar = c;
                stringStartLine = line;

                // Check for triple quotes
                if (i + 2 < code.length() && code[i+1] == c && code[i+2] == c) {
                    i += 2;
                    inTripleString = true;
                }
                inString = true;
            }
        } else {
            if (c == '\\' && i + 1 < code.length()) {
                i++;
            } else if (c == stringChar) {
                if (inTripleString) {
                    if (i + 2 < code.length() && code[i+1] == stringChar && code[i+2] == stringChar) {
                        i += 2;
                        inString = false;
                        inTripleString = false;
                    }
                } else {
                    inString = false;
                }
            }
        }
    }

    if (inString) {
        result.AddIssue({
            ValidationSeverity::Error, stringStartLine, 0, 0, "E005",
            inTripleString ? "Unterminated triple-quoted string" : "Unterminated string literal",
            "syntax", "Close the string"
        });
    }
}

void ScriptValidator::CheckIndentation(const std::string& code, ValidationResult& result) {
    std::vector<int> indentStack = {0};
    int line = 0;
    bool expectIndent = false;

    std::istringstream stream(code);
    std::string lineText;

    while (std::getline(stream, lineText)) {
        line++;

        // Skip empty lines and comments
        size_t firstNonSpace = lineText.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos || lineText[firstNonSpace] == '#') {
            continue;
        }

        // Calculate indent level
        int indent = 0;
        for (size_t i = 0; i < firstNonSpace; ++i) {
            if (lineText[i] == ' ') indent++;
            else if (lineText[i] == '\t') indent += 4;
        }

        if (expectIndent) {
            if (indent <= indentStack.back()) {
                result.AddIssue({
                    ValidationSeverity::Error, line, 1, static_cast<int>(firstNonSpace), "E006",
                    "Expected indented block",
                    "syntax", "Add indentation"
                });
            } else {
                indentStack.push_back(indent);
            }
            expectIndent = false;
        } else if (indent > indentStack.back()) {
            result.AddIssue({
                ValidationSeverity::Error, line, 1, static_cast<int>(firstNonSpace), "E007",
                "Unexpected indent",
                "syntax", "Remove extra indentation"
            });
        } else if (indent < indentStack.back()) {
            while (indentStack.size() > 1 && indent < indentStack.back()) {
                indentStack.pop_back();
            }
            if (indent != indentStack.back()) {
                result.AddIssue({
                    ValidationSeverity::Error, line, 1, static_cast<int>(firstNonSpace), "E008",
                    "Unindent does not match any outer indentation level",
                    "syntax", "Adjust indentation"
                });
            }
        }

        // Check if line ends with colon (expect indent next)
        std::string trimmed = lineText.substr(firstNonSpace);
        if (!trimmed.empty() && trimmed.back() == ':') {
            expectIndent = true;
        }
    }
}

void ScriptValidator::CheckFunctionDefs(const std::string& code, ValidationResult& result) {
    std::regex funcRegex(R"(def\s+(\w+)\s*\(([^)]*)\)\s*(?:->\s*(\w+))?\s*:?)");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());
    int line = 1;

    while (std::regex_search(searchStart, code.cend(), match, funcRegex)) {
        // Count lines to this position
        for (auto it = searchStart; it != match[0].first; ++it) {
            if (*it == '\n') line++;
        }

        std::string funcName = match[1].str();
        std::string params = match[2].str();
        std::string fullMatch = match[0].str();

        // Check if colon is present
        if (fullMatch.back() != ':') {
            result.AddIssue({
                ValidationSeverity::Error, line, 0, 0, "E009",
                "Function definition missing colon",
                "syntax", "Add ':' after function signature"
            });
        }

        // Check for duplicate parameter names
        std::vector<std::string> paramNames;
        std::regex paramRegex(R"((\w+)\s*(?::\s*\w+)?\s*(?:=\s*[^,]+)?)");
        std::smatch paramMatch;
        std::string::const_iterator paramStart(params.cbegin());

        while (std::regex_search(paramStart, params.cend(), paramMatch, paramRegex)) {
            std::string paramName = paramMatch[1].str();
            if (paramName != "self" && std::find(paramNames.begin(), paramNames.end(), paramName) != paramNames.end()) {
                result.AddIssue({
                    ValidationSeverity::Error, line, 0, 0, "E010",
                    "Duplicate parameter name: " + paramName,
                    "syntax", "Rename the duplicate parameter"
                });
            }
            paramNames.push_back(paramName);
            paramStart = paramMatch.suffix().first;
        }

        searchStart = match.suffix().first;
    }
}

void ScriptValidator::CheckClassDefs(const std::string& code, ValidationResult& result) {
    std::regex classRegex(R"(class\s+(\w+)\s*(?:\([^)]*\))?\s*:?)");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());
    int line = 1;

    while (std::regex_search(searchStart, code.cend(), match, classRegex)) {
        for (auto it = searchStart; it != match[0].first; ++it) {
            if (*it == '\n') line++;
        }

        std::string fullMatch = match[0].str();

        if (fullMatch.back() != ':') {
            result.AddIssue({
                ValidationSeverity::Error, line, 0, 0, "E011",
                "Class definition missing colon",
                "syntax", "Add ':' after class definition"
            });
        }

        searchStart = match.suffix().first;
    }
}

std::vector<ValidationIssue> ScriptValidator::CheckImports(const std::string& code,
                                                           const ValidationOptions& options) {
    std::vector<ValidationIssue> issues;
    auto imports = ExtractImports(code);

    for (const auto& imp : imports) {
        if (!IsImportAllowed(imp, options)) {
            // Find line number
            std::regex importRegex("import\\s+" + imp + "|from\\s+" + imp);
            std::smatch match;
            int line = 1;

            if (std::regex_search(code, match, importRegex)) {
                for (auto it = code.cbegin(); it != match[0].first; ++it) {
                    if (*it == '\n') line++;
                }
            }

            issues.push_back({
                ValidationSeverity::Error, line, 0, 0, "I001",
                "Import of '" + imp + "' is not allowed",
                "import", "Remove this import or use an allowed alternative"
            });
        }
    }

    return issues;
}

std::vector<std::string> ScriptValidator::ExtractImports(const std::string& code) {
    std::vector<std::string> imports;

    // Match 'import x' and 'from x import y'
    std::regex importRegex(R"((?:from\s+(\w+)|import\s+(\w+)))");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());

    while (std::regex_search(searchStart, code.cend(), match, importRegex)) {
        std::string module = match[1].matched ? match[1].str() : match[2].str();
        if (!module.empty()) {
            imports.push_back(module);
        }
        searchStart = match.suffix().first;
    }

    return imports;
}

bool ScriptValidator::IsImportAllowed(const std::string& module, const ValidationOptions& options) {
    // Check explicit allowlist
    if (!options.allowedImports.empty()) {
        if (std::find(options.allowedImports.begin(), options.allowedImports.end(), module)
            == options.allowedImports.end()) {
            return false;
        }
    }

    // Check explicit blocklist
    if (!options.blockedImports.empty()) {
        if (std::find(options.blockedImports.begin(), options.blockedImports.end(), module)
            != options.blockedImports.end()) {
            return false;
        }
    }

    // Check default lists
    if (m_blockedImports.count(module) > 0) {
        return false;
    }

    return true;
}

std::vector<ValidationIssue> ScriptValidator::CheckTypeHints(const std::string& code) {
    std::vector<ValidationIssue> issues;

    // Check function parameters without type hints (info level)
    std::regex funcRegex(R"(def\s+(\w+)\s*\(([^)]+)\))");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());
    int line = 1;

    while (std::regex_search(searchStart, code.cend(), match, funcRegex)) {
        for (auto it = searchStart; it != match[0].first; ++it) {
            if (*it == '\n') line++;
        }

        std::string params = match[2].str();

        // Check each parameter
        std::regex paramRegex(R"((\w+)\s*(?::\s*(\w+))?\s*(?:=)?)");
        std::smatch paramMatch;
        std::string::const_iterator paramStart(params.cbegin());

        while (std::regex_search(paramStart, params.cend(), paramMatch, paramRegex)) {
            std::string paramName = paramMatch[1].str();
            bool hasType = paramMatch[2].matched;

            if (paramName != "self" && !hasType) {
                issues.push_back({
                    ValidationSeverity::Hint, line, 0, 0, "T001",
                    "Parameter '" + paramName + "' has no type hint",
                    "type", "Add type annotation: " + paramName + ": <type>"
                });
            }
            paramStart = paramMatch.suffix().first;
        }

        searchStart = match.suffix().first;
    }

    return issues;
}

std::vector<ValidationIssue> ScriptValidator::CheckGameAPIUsage(const std::string& code) {
    std::vector<ValidationIssue> issues;

    for (const auto& [name, func] : m_gameAPIFunctions) {
        // Find uses of this function
        std::regex callRegex(name + R"(\s*\()");
        std::smatch match;
        std::string::const_iterator searchStart(code.cbegin());
        int line = 1;

        while (std::regex_search(searchStart, code.cend(), match, callRegex)) {
            for (auto it = searchStart; it != match[0].first; ++it) {
                if (*it == '\n') line++;
            }

            // Could add parameter count validation here
            searchStart = match.suffix().first;
        }
    }

    return issues;
}

std::vector<ValidationIssue> ScriptValidator::SecurityScan(const std::string& code,
                                                           const ValidationOptions& options) {
    std::vector<ValidationIssue> issues;

    for (const auto& pattern : m_securityPatterns) {
        // Skip file access check if allowed
        if (!options.allowFileAccess && pattern.pattern.find("open") != std::string::npos) {
            continue;
        }

        std::regex secRegex(pattern.pattern);
        std::smatch match;
        std::string::const_iterator searchStart(code.cbegin());
        int line = 1;

        while (std::regex_search(searchStart, code.cend(), match, secRegex)) {
            for (auto it = searchStart; it != match[0].first; ++it) {
                if (*it == '\n') line++;
            }

            issues.push_back({
                pattern.severity, line, 0, 0, "S001",
                pattern.description,
                "security", "Remove or replace this code"
            });

            searchStart = match.suffix().first;
        }
    }

    return issues;
}

std::vector<ValidationIssue> ScriptValidator::CheckStyle(const std::string& code) {
    std::vector<ValidationIssue> issues;

    std::istringstream stream(code);
    std::string lineText;
    int line = 0;

    while (std::getline(stream, lineText)) {
        line++;

        // Line length > 79
        if (lineText.length() > 79) {
            issues.push_back({
                ValidationSeverity::Info, line, 80, static_cast<int>(lineText.length()), "W001",
                "Line too long (" + std::to_string(lineText.length()) + " > 79 characters)",
                "style", "Break this line"
            });
        }

        // Trailing whitespace
        if (!lineText.empty() && (lineText.back() == ' ' || lineText.back() == '\t')) {
            issues.push_back({
                ValidationSeverity::Info, line, static_cast<int>(lineText.length()), static_cast<int>(lineText.length()), "W002",
                "Trailing whitespace",
                "style", "Remove trailing whitespace"
            });
        }

        // Mixed tabs and spaces
        bool hasSpaces = lineText.find(' ') != std::string::npos;
        bool hasTabs = lineText.find('\t') != std::string::npos;
        if (hasSpaces && hasTabs) {
            issues.push_back({
                ValidationSeverity::Warning, line, 0, 0, "W003",
                "Mixed tabs and spaces in indentation",
                "style", "Use consistent indentation (spaces recommended)"
            });
        }
    }

    return issues;
}

void ScriptValidator::SetDefaultAllowedImports(const std::vector<std::string>& imports) {
    m_allowedImports.clear();
    for (const auto& imp : imports) {
        m_allowedImports.insert(imp);
    }
}

void ScriptValidator::AddBlockedImport(const std::string& module) {
    m_blockedImports.insert(module);
}

void ScriptValidator::RegisterGameAPIFunction(const std::string& name, const std::string& signature) {
    APIFunction func;
    func.name = name;
    func.signature = signature;

    // Parse signature for parameter types
    std::regex paramRegex(R"((\w+)\s*:\s*(\w+))");
    std::smatch match;
    std::string::const_iterator searchStart(signature.cbegin());

    while (std::regex_search(searchStart, signature.cend(), match, paramRegex)) {
        func.paramTypes.push_back(match[2].str());
        searchStart = match.suffix().first;
    }

    // Extract return type
    std::regex returnRegex(R"(->\s*(\w+))");
    if (std::regex_search(signature, match, returnRegex)) {
        func.returnType = match[1].str();
    }

    m_gameAPIFunctions[name] = func;
}

} // namespace Scripting
} // namespace Nova
