#include "PCGScript.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <regex>

// Note: This implementation provides a Python-like scripting interface.
// For actual Python integration, you would use pybind11 or Python C API.
// This implementation uses a lightweight embedded interpreter approach.

namespace Vehement {

// ============================================================================
// Simple Script Interpreter (Python-like syntax)
// ============================================================================

namespace {

// Token types for the script parser
enum class TokenType {
    None, Identifier, Number, String, Operator,
    LParen, RParen, LBracket, RBracket, Comma, Colon,
    Indent, Dedent, Newline, Def, For, If, Elif, Else,
    While, Return, In, Range, And, Or, Not, True, False,
    EndOfFile
};

struct Token {
    TokenType type = TokenType::None;
    std::string value;
    int line = 0;
};

// Simple tokenizer
class Tokenizer {
public:
    explicit Tokenizer(const std::string& source) : m_source(source) {}

    std::vector<Token> Tokenize() {
        std::vector<Token> tokens;
        size_t pos = 0;
        int line = 1;
        int currentIndent = 0;
        std::vector<int> indentStack = {0};

        while (pos < m_source.size()) {
            // Handle newlines
            if (m_source[pos] == '\n') {
                tokens.push_back({TokenType::Newline, "\\n", line});
                pos++;
                line++;

                // Count indentation
                int indent = 0;
                while (pos < m_source.size() && (m_source[pos] == ' ' || m_source[pos] == '\t')) {
                    indent += (m_source[pos] == '\t') ? 4 : 1;
                    pos++;
                }

                // Skip empty lines
                if (pos < m_source.size() && m_source[pos] == '\n') continue;
                if (pos < m_source.size() && m_source[pos] == '#') {
                    while (pos < m_source.size() && m_source[pos] != '\n') pos++;
                    continue;
                }

                // Handle indent/dedent
                if (indent > indentStack.back()) {
                    indentStack.push_back(indent);
                    tokens.push_back({TokenType::Indent, "", line});
                } else {
                    while (indentStack.size() > 1 && indent < indentStack.back()) {
                        indentStack.pop_back();
                        tokens.push_back({TokenType::Dedent, "", line});
                    }
                }
                continue;
            }

            // Skip whitespace
            if (std::isspace(m_source[pos])) {
                pos++;
                continue;
            }

            // Skip comments
            if (m_source[pos] == '#') {
                while (pos < m_source.size() && m_source[pos] != '\n') pos++;
                continue;
            }

            // Strings
            if (m_source[pos] == '"' || m_source[pos] == '\'') {
                char quote = m_source[pos++];
                std::string str;
                while (pos < m_source.size() && m_source[pos] != quote) {
                    if (m_source[pos] == '\\' && pos + 1 < m_source.size()) {
                        pos++;
                        switch (m_source[pos]) {
                            case 'n': str += '\n'; break;
                            case 't': str += '\t'; break;
                            case '\\': str += '\\'; break;
                            default: str += m_source[pos]; break;
                        }
                    } else {
                        str += m_source[pos];
                    }
                    pos++;
                }
                pos++; // Skip closing quote
                tokens.push_back({TokenType::String, str, line});
                continue;
            }

            // Numbers
            if (std::isdigit(m_source[pos]) || (m_source[pos] == '.' && pos + 1 < m_source.size() && std::isdigit(m_source[pos + 1]))) {
                std::string num;
                while (pos < m_source.size() && (std::isdigit(m_source[pos]) || m_source[pos] == '.')) {
                    num += m_source[pos++];
                }
                tokens.push_back({TokenType::Number, num, line});
                continue;
            }

            // Identifiers and keywords
            if (std::isalpha(m_source[pos]) || m_source[pos] == '_') {
                std::string id;
                while (pos < m_source.size() && (std::isalnum(m_source[pos]) || m_source[pos] == '_')) {
                    id += m_source[pos++];
                }

                TokenType type = TokenType::Identifier;
                if (id == "def") type = TokenType::Def;
                else if (id == "for") type = TokenType::For;
                else if (id == "if") type = TokenType::If;
                else if (id == "elif") type = TokenType::Elif;
                else if (id == "else") type = TokenType::Else;
                else if (id == "while") type = TokenType::While;
                else if (id == "return") type = TokenType::Return;
                else if (id == "in") type = TokenType::In;
                else if (id == "range") type = TokenType::Range;
                else if (id == "and") type = TokenType::And;
                else if (id == "or") type = TokenType::Or;
                else if (id == "not") type = TokenType::Not;
                else if (id == "True") type = TokenType::True;
                else if (id == "False") type = TokenType::False;

                tokens.push_back({type, id, line});
                continue;
            }

            // Operators and punctuation
            char c = m_source[pos++];
            std::string op(1, c);

            // Multi-character operators
            if (pos < m_source.size()) {
                std::string op2 = op + m_source[pos];
                if (op2 == "==" || op2 == "!=" || op2 == "<=" || op2 == ">=" ||
                    op2 == "+=" || op2 == "-=" || op2 == "*=" || op2 == "/=") {
                    op = op2;
                    pos++;
                }
            }

            TokenType type = TokenType::Operator;
            if (c == '(') type = TokenType::LParen;
            else if (c == ')') type = TokenType::RParen;
            else if (c == '[') type = TokenType::LBracket;
            else if (c == ']') type = TokenType::RBracket;
            else if (c == ',') type = TokenType::Comma;
            else if (c == ':') type = TokenType::Colon;

            tokens.push_back({type, op, line});
        }

        // Final dedents
        while (indentStack.size() > 1) {
            indentStack.pop_back();
            tokens.push_back({TokenType::Dedent, "", line});
        }

        tokens.push_back({TokenType::EndOfFile, "", line});
        return tokens;
    }

private:
    std::string m_source;
};

// Script value type
struct ScriptValue {
    enum class Type { None, Int, Float, String, Bool, List, Dict, Object };
    Type type = Type::None;

    int64_t intVal = 0;
    double floatVal = 0.0;
    std::string strVal;
    bool boolVal = false;
    std::vector<ScriptValue> listVal;
    std::unordered_map<std::string, ScriptValue> dictVal;
    void* objectPtr = nullptr;

    ScriptValue() = default;
    ScriptValue(int v) : type(Type::Int), intVal(v) {}
    ScriptValue(float v) : type(Type::Float), floatVal(v) {}
    ScriptValue(double v) : type(Type::Float), floatVal(v) {}
    ScriptValue(const std::string& v) : type(Type::String), strVal(v) {}
    ScriptValue(bool v) : type(Type::Bool), boolVal(v) {}

    bool IsTrue() const {
        switch (type) {
            case Type::None: return false;
            case Type::Int: return intVal != 0;
            case Type::Float: return floatVal != 0.0;
            case Type::String: return !strVal.empty();
            case Type::Bool: return boolVal;
            case Type::List: return !listVal.empty();
            case Type::Dict: return !dictVal.empty();
            default: return objectPtr != nullptr;
        }
    }

    int ToInt() const {
        switch (type) {
            case Type::Int: return static_cast<int>(intVal);
            case Type::Float: return static_cast<int>(floatVal);
            case Type::String: return std::stoi(strVal);
            case Type::Bool: return boolVal ? 1 : 0;
            default: return 0;
        }
    }

    float ToFloat() const {
        switch (type) {
            case Type::Int: return static_cast<float>(intVal);
            case Type::Float: return static_cast<float>(floatVal);
            case Type::String: return std::stof(strVal);
            case Type::Bool: return boolVal ? 1.0f : 0.0f;
            default: return 0.0f;
        }
    }

    std::string ToString() const {
        switch (type) {
            case Type::None: return "None";
            case Type::Int: return std::to_string(intVal);
            case Type::Float: return std::to_string(floatVal);
            case Type::String: return strVal;
            case Type::Bool: return boolVal ? "True" : "False";
            default: return "<object>";
        }
    }
};

} // anonymous namespace

// ============================================================================
// PCGScript Implementation
// ============================================================================

struct PCGScript::Impl {
    std::vector<Token> tokens;
    std::unordered_map<std::string, ScriptValue> globals;
    std::unordered_map<std::string, size_t> functions; // function name -> token index
    PCGContext* currentContext = nullptr;
    std::string scriptName;

    // Built-in function handlers
    ScriptValue CallBuiltin(const std::string& name, const std::vector<ScriptValue>& args);
    ScriptValue CallContextMethod(const std::string& method, const std::vector<ScriptValue>& args);

    // Execution state
    size_t currentToken = 0;
    bool returnFlag = false;
    ScriptValue returnValue;
    int loopDepth = 0;
    bool breakFlag = false;
    bool continueFlag = false;

    // Parsing and execution
    ScriptValue Execute(size_t startToken, size_t endToken);
    ScriptValue EvaluateExpression(size_t& pos);
    ScriptValue EvaluatePrimary(size_t& pos);
    std::vector<ScriptValue> EvaluateArgs(size_t& pos);
};

ScriptValue PCGScript::Impl::CallContextMethod(const std::string& method, const std::vector<ScriptValue>& args) {
    if (!currentContext) return ScriptValue();

    // Tile operations
    if (method == "set_tile" && args.size() >= 3) {
        currentContext->SetTile(args[0].ToInt(), args[1].ToInt(), args[2].ToString());
        return ScriptValue();
    }
    if (method == "get_tile" && args.size() >= 2) {
        TileType t = currentContext->GetTile(args[0].ToInt(), args[1].ToInt());
        return ScriptValue(GetTileTypeName(t));
    }
    if (method == "set_wall" && args.size() >= 4) {
        currentContext->SetWall(args[0].ToInt(), args[1].ToInt(),
                                PCGContext::TileTypeFromName(args[2].ToString()),
                                args[3].ToFloat());
        return ScriptValue();
    }
    if (method == "fill_rect" && args.size() >= 5) {
        currentContext->FillRect(args[0].ToInt(), args[1].ToInt(),
                                  args[2].ToInt(), args[3].ToInt(),
                                  PCGContext::TileTypeFromName(args[4].ToString()));
        return ScriptValue();
    }
    if (method == "draw_line" && args.size() >= 5) {
        currentContext->DrawLine(args[0].ToInt(), args[1].ToInt(),
                                  args[2].ToInt(), args[3].ToInt(),
                                  PCGContext::TileTypeFromName(args[4].ToString()));
        return ScriptValue();
    }
    if (method == "set_elevation" && args.size() >= 3) {
        currentContext->SetElevation(args[0].ToInt(), args[1].ToInt(), args[2].ToFloat());
        return ScriptValue();
    }

    // Geographic data
    if (method == "get_elevation" && args.size() >= 2) {
        return ScriptValue(currentContext->GetElevation(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "get_biome" && args.size() >= 2) {
        return ScriptValue(currentContext->GetBiomeName(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "is_water" && args.size() >= 2) {
        return ScriptValue(currentContext->IsWater(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "is_road" && args.size() >= 2) {
        return ScriptValue(currentContext->IsRoad(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "get_road_type" && args.size() >= 2) {
        return ScriptValue(currentContext->GetRoadTypeName(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "get_population_density" && args.size() >= 2) {
        return ScriptValue(currentContext->GetPopulationDensity(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "get_tree_density" && args.size() >= 2) {
        return ScriptValue(currentContext->GetTreeDensity(args[0].ToInt(), args[1].ToInt()));
    }

    // Random functions
    if (method == "random") {
        if (args.empty()) return ScriptValue(currentContext->Random());
        if (args.size() >= 2) return ScriptValue(currentContext->Random(args[0].ToFloat(), args[1].ToFloat()));
        return ScriptValue(currentContext->Random());
    }
    if (method == "random_int" && args.size() >= 2) {
        return ScriptValue(currentContext->RandomInt(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "random_bool") {
        float prob = args.empty() ? 0.5f : args[0].ToFloat();
        return ScriptValue(currentContext->RandomBool(prob));
    }

    // Noise functions
    if (method == "perlin" && args.size() >= 2) {
        float freq = args.size() > 2 ? args[2].ToFloat() : 1.0f;
        int octaves = args.size() > 3 ? args[3].ToInt() : 1;
        return ScriptValue(currentContext->PerlinNoise(args[0].ToFloat(), args[1].ToFloat(), freq, octaves));
    }
    if (method == "simplex" && args.size() >= 2) {
        float freq = args.size() > 2 ? args[2].ToFloat() : 1.0f;
        int octaves = args.size() > 3 ? args[3].ToInt() : 1;
        return ScriptValue(currentContext->SimplexNoise(args[0].ToFloat(), args[1].ToFloat(), freq, octaves));
    }
    if (method == "worley" && args.size() >= 2) {
        float freq = args.size() > 2 ? args[2].ToFloat() : 1.0f;
        return ScriptValue(currentContext->WorleyNoise(args[0].ToFloat(), args[1].ToFloat(), freq));
    }
    if (method == "ridged" && args.size() >= 2) {
        float freq = args.size() > 2 ? args[2].ToFloat() : 1.0f;
        int octaves = args.size() > 3 ? args[3].ToInt() : 4;
        return ScriptValue(currentContext->RidgedNoise(args[0].ToFloat(), args[1].ToFloat(), freq, octaves));
    }
    if (method == "billow" && args.size() >= 2) {
        float freq = args.size() > 2 ? args[2].ToFloat() : 1.0f;
        int octaves = args.size() > 3 ? args[3].ToInt() : 4;
        return ScriptValue(currentContext->BillowNoise(args[0].ToFloat(), args[1].ToFloat(), freq, octaves));
    }

    // Spawning
    if (method == "spawn_foliage" && args.size() >= 3) {
        float scale = args.size() > 3 ? args[3].ToFloat() : 1.0f;
        currentContext->SpawnFoliage(args[0].ToInt(), args[1].ToInt(), args[2].ToString(), scale);
        return ScriptValue();
    }
    if (method == "spawn_entity" && args.size() >= 3) {
        currentContext->SpawnEntity(args[0].ToInt(), args[1].ToInt(), args[2].ToString());
        return ScriptValue();
    }

    // Utility
    if (method == "in_bounds" && args.size() >= 2) {
        return ScriptValue(currentContext->InBounds(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "is_walkable" && args.size() >= 2) {
        return ScriptValue(currentContext->IsWalkable(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "is_occupied" && args.size() >= 2) {
        return ScriptValue(currentContext->IsOccupied(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "mark_occupied" && args.size() >= 2) {
        currentContext->MarkOccupied(args[0].ToInt(), args[1].ToInt());
        return ScriptValue();
    }
    if (method == "get_zone" && args.size() >= 2) {
        return ScriptValue(currentContext->GetZone(args[0].ToInt(), args[1].ToInt()));
    }
    if (method == "mark_zone" && args.size() >= 5) {
        currentContext->MarkZone(args[0].ToInt(), args[1].ToInt(),
                                  args[2].ToInt(), args[3].ToInt(), args[4].ToString());
        return ScriptValue();
    }
    if (method == "distance" && args.size() >= 4) {
        return ScriptValue(currentContext->Distance(args[0].ToInt(), args[1].ToInt(),
                                                     args[2].ToInt(), args[3].ToInt()));
    }
    if (method == "has_line_of_sight" && args.size() >= 4) {
        return ScriptValue(currentContext->HasLineOfSight(args[0].ToInt(), args[1].ToInt(),
                                                           args[2].ToInt(), args[3].ToInt()));
    }

    // Data storage
    if (method == "set_data" && args.size() >= 2) {
        currentContext->SetData(args[0].ToString(), args[1].ToString());
        return ScriptValue();
    }
    if (method == "get_data" && args.size() >= 1) {
        return ScriptValue(currentContext->GetData(args[0].ToString()));
    }
    if (method == "has_data" && args.size() >= 1) {
        return ScriptValue(currentContext->HasData(args[0].ToString()));
    }

    return ScriptValue();
}

ScriptValue PCGScript::Impl::CallBuiltin(const std::string& name, const std::vector<ScriptValue>& args) {
    if (name == "print") {
        std::string output;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) output += " ";
            output += args[i].ToString();
        }
        std::cout << output << std::endl;
        return ScriptValue();
    }
    if (name == "len" && !args.empty()) {
        if (args[0].type == ScriptValue::Type::String) {
            return ScriptValue(static_cast<int>(args[0].strVal.size()));
        }
        if (args[0].type == ScriptValue::Type::List) {
            return ScriptValue(static_cast<int>(args[0].listVal.size()));
        }
        return ScriptValue(0);
    }
    if (name == "int" && !args.empty()) {
        return ScriptValue(args[0].ToInt());
    }
    if (name == "float" && !args.empty()) {
        return ScriptValue(args[0].ToFloat());
    }
    if (name == "str" && !args.empty()) {
        return ScriptValue(args[0].ToString());
    }
    if (name == "abs" && !args.empty()) {
        if (args[0].type == ScriptValue::Type::Float) {
            return ScriptValue(std::abs(args[0].floatVal));
        }
        return ScriptValue(std::abs(static_cast<int>(args[0].intVal)));
    }
    if (name == "min" && args.size() >= 2) {
        if (args[0].type == ScriptValue::Type::Float || args[1].type == ScriptValue::Type::Float) {
            return ScriptValue(std::min(args[0].ToFloat(), args[1].ToFloat()));
        }
        return ScriptValue(std::min(args[0].ToInt(), args[1].ToInt()));
    }
    if (name == "max" && args.size() >= 2) {
        if (args[0].type == ScriptValue::Type::Float || args[1].type == ScriptValue::Type::Float) {
            return ScriptValue(std::max(args[0].ToFloat(), args[1].ToFloat()));
        }
        return ScriptValue(std::max(args[0].ToInt(), args[1].ToInt()));
    }

    return ScriptValue();
}

// ============================================================================
// PCGScript Public Interface
// ============================================================================

PCGScript::PCGScript() : m_impl(std::make_unique<Impl>()) {}

PCGScript::~PCGScript() = default;

PCGScript::PCGScript(PCGScript&&) noexcept = default;
PCGScript& PCGScript::operator=(PCGScript&&) noexcept = default;

bool PCGScript::InitializePython() {
    // For embedded interpreter, nothing special needed
    return true;
}

void PCGScript::ShutdownPython() {
    // Cleanup if needed
}

bool PCGScript::IsPythonInitialized() {
    return true;
}

void PCGScript::AddSearchPath(const std::string& /*path*/) {
    // Track search paths for future use
}

bool PCGScript::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        SetError("Failed to open file: " + filepath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    m_filepath = filepath;

    return LoadFromString(buffer.str(), std::filesystem::path(filepath).stem().string());
}

bool PCGScript::LoadFromString(const std::string& source, const std::string& name) {
    try {
        m_source = source;
        m_impl->scriptName = name;

        // Tokenize
        Tokenizer tokenizer(source);
        m_impl->tokens = tokenizer.Tokenize();

        // Find function definitions
        m_impl->functions.clear();
        for (size_t i = 0; i < m_impl->tokens.size(); ++i) {
            if (m_impl->tokens[i].type == TokenType::Def && i + 1 < m_impl->tokens.size()) {
                std::string funcName = m_impl->tokens[i + 1].value;
                m_impl->functions[funcName] = i;
            }
        }

        m_loaded = true;
        m_lastError.clear();
        return true;

    } catch (const std::exception& e) {
        SetError(std::string("Parse error: ") + e.what());
        return false;
    }
}

void PCGScript::Unload() {
    m_impl->tokens.clear();
    m_impl->functions.clear();
    m_impl->globals.clear();
    m_source.clear();
    m_filepath.clear();
    m_loaded = false;
}

PCGScriptValidation PCGScript::Validate() {
    PCGScriptValidation result;

    if (!m_loaded) {
        result.errorMessage = "Script not loaded";
        return result;
    }

    // Check for required functions
    result.requiredFunctions = {FUNC_GENERATE};

    bool hasGenerate = HasFunction(FUNC_GENERATE);
    if (!hasGenerate) {
        result.errorMessage = "Missing required function: generate(ctx)";
        return result;
    }

    // Extract metadata from comments
    std::regex nameRegex(R"(#\s*@name\s*:\s*(.+))");
    std::regex versionRegex(R"(#\s*@version\s*:\s*(.+))");
    std::regex authorRegex(R"(#\s*@author\s*:\s*(.+))");
    std::regex descRegex(R"(#\s*@description\s*:\s*(.+))");

    std::smatch match;
    if (std::regex_search(m_source, match, nameRegex)) result.name = match[1];
    if (std::regex_search(m_source, match, versionRegex)) result.version = match[1];
    if (std::regex_search(m_source, match, authorRegex)) result.author = match[1];
    if (std::regex_search(m_source, match, descRegex)) result.description = match[1];

    if (result.name.empty()) result.name = m_impl->scriptName;

    // Warnings
    if (!HasFunction(FUNC_PREVIEW)) {
        result.warnings.push_back("No preview() function defined - preview will use generate()");
    }

    result.valid = true;
    return result;
}

bool PCGScript::HasFunction(const std::string& name) const {
    return m_impl->functions.find(name) != m_impl->functions.end();
}

std::vector<std::string> PCGScript::GetFunctions() const {
    std::vector<std::string> result;
    for (const auto& [name, _] : m_impl->functions) {
        result.push_back(name);
    }
    return result;
}

PCGScriptResult PCGScript::Generate(PCGContext& context) {
    return ExecuteFunction(FUNC_GENERATE, context);
}

PCGScriptResult PCGScript::Preview(PCGContext& context) {
    if (HasFunction(FUNC_PREVIEW)) {
        return ExecuteFunction(FUNC_PREVIEW, context);
    }
    return ExecuteFunction(FUNC_GENERATE, context);
}

PCGScriptResult PCGScript::ExecuteFunction(const std::string& functionName, PCGContext& context) {
    PCGScriptResult result;

    if (!m_loaded) {
        result.errorMessage = "Script not loaded";
        return result;
    }

    auto it = m_impl->functions.find(functionName);
    if (it == m_impl->functions.end()) {
        result.errorMessage = "Function not found: " + functionName;
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Bind context
        m_impl->currentContext = &context;

        // Set up context globals
        m_impl->globals["width"] = ScriptValue(context.GetWidth());
        m_impl->globals["height"] = ScriptValue(context.GetHeight());
        m_impl->globals["seed"] = ScriptValue(static_cast<int>(context.GetSeed()));
        m_impl->globals["world_x"] = ScriptValue(context.GetWorldX());
        m_impl->globals["world_y"] = ScriptValue(context.GetWorldY());

        // Execute function
        m_impl->returnFlag = false;
        m_impl->currentToken = it->second;

        // Find function body (after 'def name(args):')
        size_t pos = it->second;
        while (pos < m_impl->tokens.size() && m_impl->tokens[pos].type != TokenType::Colon) pos++;
        pos++; // Skip colon
        while (pos < m_impl->tokens.size() && m_impl->tokens[pos].type == TokenType::Newline) pos++;

        // Find indented block
        if (pos < m_impl->tokens.size() && m_impl->tokens[pos].type == TokenType::Indent) {
            pos++; // Skip indent token

            size_t blockStart = pos;
            int depth = 1;

            // Find end of block
            while (pos < m_impl->tokens.size() && depth > 0) {
                if (m_impl->tokens[pos].type == TokenType::Indent) depth++;
                else if (m_impl->tokens[pos].type == TokenType::Dedent) depth--;
                pos++;
            }

            // Execute block
            m_impl->Execute(blockStart, pos);
        }

        result.success = true;
        result.tilesModified = context.GetWidth() * context.GetHeight();
        result.entitiesSpawned = static_cast<int>(context.GetEntitySpawns().size());
        result.foliageSpawned = static_cast<int>(context.GetFoliageSpawns().size());

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    m_impl->currentContext = nullptr;

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime = std::chrono::duration<float>(endTime - startTime).count();

    if (m_profilingEnabled) {
        m_profilingData.totalTime += result.executionTime;
        m_profilingData.functionTimes[functionName] += result.executionTime;
        m_profilingData.callCount++;
    }

    return result;
}

PCGScriptResult PCGScript::ExecuteWithTimeout(const std::string& functionName, PCGContext& context,
                                               int /*timeoutMs*/) {
    // For now, just execute normally
    // A proper implementation would use a separate thread with timeout
    return ExecuteFunction(functionName, context);
}

void PCGScript::SetGlobal(const std::string& name, int value) {
    m_impl->globals[name] = ScriptValue(value);
}

void PCGScript::SetGlobal(const std::string& name, float value) {
    m_impl->globals[name] = ScriptValue(value);
}

void PCGScript::SetGlobal(const std::string& name, const std::string& value) {
    m_impl->globals[name] = ScriptValue(value);
}

void PCGScript::SetGlobal(const std::string& name, bool value) {
    m_impl->globals[name] = ScriptValue(value);
}

int PCGScript::GetGlobalInt(const std::string& name, int defaultValue) const {
    auto it = m_impl->globals.find(name);
    return (it != m_impl->globals.end()) ? it->second.ToInt() : defaultValue;
}

float PCGScript::GetGlobalFloat(const std::string& name, float defaultValue) const {
    auto it = m_impl->globals.find(name);
    return (it != m_impl->globals.end()) ? it->second.ToFloat() : defaultValue;
}

std::string PCGScript::GetGlobalString(const std::string& name, const std::string& defaultValue) const {
    auto it = m_impl->globals.find(name);
    return (it != m_impl->globals.end()) ? it->second.ToString() : defaultValue;
}

bool PCGScript::GetGlobalBool(const std::string& name, bool defaultValue) const {
    auto it = m_impl->globals.find(name);
    return (it != m_impl->globals.end()) ? it->second.IsTrue() : defaultValue;
}

void PCGScript::SetProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void PCGScript::SetLogCallback(LogCallback callback) {
    m_logCallback = std::move(callback);
}

void PCGScript::EnableProfiling(bool enabled) {
    m_profilingEnabled = enabled;
}

PCGScript::ProfilingData PCGScript::GetProfilingData() const {
    return m_profilingData;
}

void PCGScript::ClearProfilingData() {
    m_profilingData = ProfilingData{};
}

void PCGScript::SetError(const std::string& error) {
    m_lastError = error;
    if (m_logCallback) {
        m_logCallback(error, 2); // Error level
    }
}

// ============================================================================
// Simplified Script Execution
// ============================================================================

ScriptValue PCGScript::Impl::Execute(size_t startToken, size_t endToken) {
    std::unordered_map<std::string, ScriptValue> locals;

    for (size_t pos = startToken; pos < endToken && !returnFlag;) {
        const Token& token = tokens[pos];

        // Skip newlines
        if (token.type == TokenType::Newline) {
            pos++;
            continue;
        }

        // Handle for loop: for x in range(n):
        if (token.type == TokenType::For) {
            pos++; // Skip 'for'
            std::string varName = tokens[pos++].value;
            pos++; // Skip 'in'

            // Handle range()
            if (tokens[pos].type == TokenType::Range) {
                pos++; // Skip 'range'
                pos++; // Skip '('

                int start = 0, end = 0, step = 1;
                std::vector<ScriptValue> rangeArgs = EvaluateArgs(pos);

                if (rangeArgs.size() == 1) {
                    end = rangeArgs[0].ToInt();
                } else if (rangeArgs.size() == 2) {
                    start = rangeArgs[0].ToInt();
                    end = rangeArgs[1].ToInt();
                } else if (rangeArgs.size() >= 3) {
                    start = rangeArgs[0].ToInt();
                    end = rangeArgs[1].ToInt();
                    step = rangeArgs[2].ToInt();
                }

                // Skip to colon
                while (pos < endToken && tokens[pos].type != TokenType::Colon) pos++;
                pos++; // Skip colon
                while (pos < endToken && tokens[pos].type == TokenType::Newline) pos++;

                // Find loop body
                if (tokens[pos].type == TokenType::Indent) {
                    pos++; // Skip indent
                    size_t bodyStart = pos;
                    int depth = 1;
                    while (pos < endToken && depth > 0) {
                        if (tokens[pos].type == TokenType::Indent) depth++;
                        else if (tokens[pos].type == TokenType::Dedent) depth--;
                        pos++;
                    }
                    size_t bodyEnd = pos;

                    // Execute loop
                    loopDepth++;
                    for (int i = start; (step > 0 ? i < end : i > end) && !returnFlag && !breakFlag; i += step) {
                        globals[varName] = ScriptValue(i);
                        Execute(bodyStart, bodyEnd);
                        if (continueFlag) {
                            continueFlag = false;
                        }
                    }
                    breakFlag = false;
                    loopDepth--;
                }
            }
            continue;
        }

        // Handle if statement
        if (token.type == TokenType::If) {
            pos++; // Skip 'if'
            ScriptValue condition = EvaluateExpression(pos);

            // Skip to colon
            while (pos < endToken && tokens[pos].type != TokenType::Colon) pos++;
            pos++; // Skip colon
            while (pos < endToken && tokens[pos].type == TokenType::Newline) pos++;

            // Find if body
            if (tokens[pos].type == TokenType::Indent) {
                pos++; // Skip indent
                size_t bodyStart = pos;
                int depth = 1;
                while (pos < endToken && depth > 0) {
                    if (tokens[pos].type == TokenType::Indent) depth++;
                    else if (tokens[pos].type == TokenType::Dedent) depth--;
                    pos++;
                }
                size_t bodyEnd = pos;

                if (condition.IsTrue()) {
                    Execute(bodyStart, bodyEnd);
                }

                // Handle elif/else
                while (pos < endToken && (tokens[pos].type == TokenType::Elif ||
                                           tokens[pos].type == TokenType::Else)) {
                    bool isElse = tokens[pos].type == TokenType::Else;
                    pos++; // Skip elif/else

                    ScriptValue elifCond(true);
                    if (!isElse) {
                        elifCond = EvaluateExpression(pos);
                    }

                    // Skip to colon
                    while (pos < endToken && tokens[pos].type != TokenType::Colon) pos++;
                    pos++; // Skip colon
                    while (pos < endToken && tokens[pos].type == TokenType::Newline) pos++;

                    if (tokens[pos].type == TokenType::Indent) {
                        pos++; // Skip indent
                        bodyStart = pos;
                        depth = 1;
                        while (pos < endToken && depth > 0) {
                            if (tokens[pos].type == TokenType::Indent) depth++;
                            else if (tokens[pos].type == TokenType::Dedent) depth--;
                            pos++;
                        }
                        bodyEnd = pos;

                        if (!condition.IsTrue() && elifCond.IsTrue()) {
                            Execute(bodyStart, bodyEnd);
                            condition = ScriptValue(true); // Mark as handled
                        }
                    }
                }
            }
            continue;
        }

        // Handle return
        if (token.type == TokenType::Return) {
            pos++;
            if (pos < endToken && tokens[pos].type != TokenType::Newline) {
                returnValue = EvaluateExpression(pos);
            }
            returnFlag = true;
            return returnValue;
        }

        // Handle assignment or function call
        if (token.type == TokenType::Identifier) {
            std::string name = token.value;
            pos++;

            // Check for method call on ctx
            if (name == "ctx" && pos < endToken && tokens[pos].value == ".") {
                pos++; // Skip '.'
                std::string method = tokens[pos++].value;
                pos++; // Skip '('
                std::vector<ScriptValue> args = EvaluateArgs(pos);
                CallContextMethod(method, args);
            }
            // Assignment
            else if (pos < endToken && tokens[pos].value == "=") {
                pos++; // Skip '='
                globals[name] = EvaluateExpression(pos);
            }
            // Compound assignment
            else if (pos < endToken && (tokens[pos].value == "+=" || tokens[pos].value == "-=" ||
                                         tokens[pos].value == "*=" || tokens[pos].value == "/=")) {
                std::string op = tokens[pos++].value;
                ScriptValue rhs = EvaluateExpression(pos);
                ScriptValue& lhs = globals[name];

                if (op == "+=") {
                    if (lhs.type == ScriptValue::Type::Float || rhs.type == ScriptValue::Type::Float)
                        lhs = ScriptValue(lhs.ToFloat() + rhs.ToFloat());
                    else
                        lhs = ScriptValue(lhs.ToInt() + rhs.ToInt());
                } else if (op == "-=") {
                    if (lhs.type == ScriptValue::Type::Float || rhs.type == ScriptValue::Type::Float)
                        lhs = ScriptValue(lhs.ToFloat() - rhs.ToFloat());
                    else
                        lhs = ScriptValue(lhs.ToInt() - rhs.ToInt());
                } else if (op == "*=") {
                    if (lhs.type == ScriptValue::Type::Float || rhs.type == ScriptValue::Type::Float)
                        lhs = ScriptValue(lhs.ToFloat() * rhs.ToFloat());
                    else
                        lhs = ScriptValue(lhs.ToInt() * rhs.ToInt());
                } else if (op == "/=") {
                    lhs = ScriptValue(lhs.ToFloat() / rhs.ToFloat());
                }
            }
            // Function call
            else if (pos < endToken && tokens[pos].type == TokenType::LParen) {
                pos++; // Skip '('
                std::vector<ScriptValue> args = EvaluateArgs(pos);
                CallBuiltin(name, args);
            }
        } else {
            pos++;
        }
    }

    return ScriptValue();
}

ScriptValue PCGScript::Impl::EvaluateExpression(size_t& pos) {
    ScriptValue left = EvaluatePrimary(pos);

    while (pos < tokens.size()) {
        const Token& op = tokens[pos];

        // Binary operators
        if (op.type == TokenType::Operator) {
            pos++;
            ScriptValue right = EvaluatePrimary(pos);

            if (op.value == "+") {
                if (left.type == ScriptValue::Type::String || right.type == ScriptValue::Type::String) {
                    left = ScriptValue(left.ToString() + right.ToString());
                } else if (left.type == ScriptValue::Type::Float || right.type == ScriptValue::Type::Float) {
                    left = ScriptValue(left.ToFloat() + right.ToFloat());
                } else {
                    left = ScriptValue(left.ToInt() + right.ToInt());
                }
            } else if (op.value == "-") {
                if (left.type == ScriptValue::Type::Float || right.type == ScriptValue::Type::Float)
                    left = ScriptValue(left.ToFloat() - right.ToFloat());
                else
                    left = ScriptValue(left.ToInt() - right.ToInt());
            } else if (op.value == "*") {
                if (left.type == ScriptValue::Type::Float || right.type == ScriptValue::Type::Float)
                    left = ScriptValue(left.ToFloat() * right.ToFloat());
                else
                    left = ScriptValue(left.ToInt() * right.ToInt());
            } else if (op.value == "/") {
                left = ScriptValue(left.ToFloat() / right.ToFloat());
            } else if (op.value == "%") {
                left = ScriptValue(left.ToInt() % right.ToInt());
            } else if (op.value == "<") {
                left = ScriptValue(left.ToFloat() < right.ToFloat());
            } else if (op.value == ">") {
                left = ScriptValue(left.ToFloat() > right.ToFloat());
            } else if (op.value == "<=") {
                left = ScriptValue(left.ToFloat() <= right.ToFloat());
            } else if (op.value == ">=") {
                left = ScriptValue(left.ToFloat() >= right.ToFloat());
            } else if (op.value == "==") {
                left = ScriptValue(left.ToString() == right.ToString());
            } else if (op.value == "!=") {
                left = ScriptValue(left.ToString() != right.ToString());
            } else {
                pos--;
                break;
            }
        } else if (op.type == TokenType::And) {
            pos++;
            ScriptValue right = EvaluatePrimary(pos);
            left = ScriptValue(left.IsTrue() && right.IsTrue());
        } else if (op.type == TokenType::Or) {
            pos++;
            ScriptValue right = EvaluatePrimary(pos);
            left = ScriptValue(left.IsTrue() || right.IsTrue());
        } else {
            break;
        }
    }

    return left;
}

ScriptValue PCGScript::Impl::EvaluatePrimary(size_t& pos) {
    if (pos >= tokens.size()) return ScriptValue();

    const Token& token = tokens[pos];

    // Unary not
    if (token.type == TokenType::Not) {
        pos++;
        ScriptValue val = EvaluatePrimary(pos);
        return ScriptValue(!val.IsTrue());
    }

    // Unary minus
    if (token.type == TokenType::Operator && token.value == "-") {
        pos++;
        ScriptValue val = EvaluatePrimary(pos);
        if (val.type == ScriptValue::Type::Float) {
            return ScriptValue(-val.ToFloat());
        }
        return ScriptValue(-val.ToInt());
    }

    // Parenthesized expression
    if (token.type == TokenType::LParen) {
        pos++;
        ScriptValue val = EvaluateExpression(pos);
        if (pos < tokens.size() && tokens[pos].type == TokenType::RParen) pos++;
        return val;
    }

    // Number
    if (token.type == TokenType::Number) {
        pos++;
        if (token.value.find('.') != std::string::npos) {
            return ScriptValue(std::stof(token.value));
        }
        return ScriptValue(std::stoi(token.value));
    }

    // String
    if (token.type == TokenType::String) {
        pos++;
        return ScriptValue(token.value);
    }

    // Boolean
    if (token.type == TokenType::True) {
        pos++;
        return ScriptValue(true);
    }
    if (token.type == TokenType::False) {
        pos++;
        return ScriptValue(false);
    }

    // Identifier or function call
    if (token.type == TokenType::Identifier) {
        std::string name = token.value;
        pos++;

        // Method call on ctx
        if (name == "ctx" && pos < tokens.size() && tokens[pos].value == ".") {
            pos++; // Skip '.'
            std::string method = tokens[pos++].value;

            // Property access
            if (pos >= tokens.size() || tokens[pos].type != TokenType::LParen) {
                if (method == "width" && currentContext) return ScriptValue(currentContext->GetWidth());
                if (method == "height" && currentContext) return ScriptValue(currentContext->GetHeight());
                if (method == "seed" && currentContext) return ScriptValue(static_cast<int>(currentContext->GetSeed()));
                if (method == "world_x" && currentContext) return ScriptValue(currentContext->GetWorldX());
                if (method == "world_y" && currentContext) return ScriptValue(currentContext->GetWorldY());
                return ScriptValue();
            }

            pos++; // Skip '('
            std::vector<ScriptValue> args = EvaluateArgs(pos);
            return CallContextMethod(method, args);
        }

        // Function call
        if (pos < tokens.size() && tokens[pos].type == TokenType::LParen) {
            pos++; // Skip '('
            std::vector<ScriptValue> args = EvaluateArgs(pos);
            return CallBuiltin(name, args);
        }

        // Variable lookup
        auto it = globals.find(name);
        if (it != globals.end()) {
            return it->second;
        }

        return ScriptValue();
    }

    pos++;
    return ScriptValue();
}

std::vector<ScriptValue> PCGScript::Impl::EvaluateArgs(size_t& pos) {
    std::vector<ScriptValue> args;

    while (pos < tokens.size() && tokens[pos].type != TokenType::RParen) {
        args.push_back(EvaluateExpression(pos));
        if (pos < tokens.size() && tokens[pos].type == TokenType::Comma) {
            pos++;
        }
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::RParen) {
        pos++;
    }

    return args;
}

// ============================================================================
// PCGScriptManager Implementation
// ============================================================================

PCGScriptManager& PCGScriptManager::Instance() {
    static PCGScriptManager instance;
    return instance;
}

PCGScriptManager::~PCGScriptManager() {
    Shutdown();
}

bool PCGScriptManager::Initialize(const std::string& scriptsPath) {
    if (m_initialized) return true;

    if (!PCGScript::InitializePython()) {
        return false;
    }

    if (!scriptsPath.empty()) {
        m_scriptsPath = scriptsPath;
        PCGScript::AddSearchPath(scriptsPath);
    }

    m_initialized = true;
    return true;
}

void PCGScriptManager::Shutdown() {
    m_scripts.clear();
    m_fileModTimes.clear();

    if (m_initialized) {
        PCGScript::ShutdownPython();
        m_initialized = false;
    }
}

PCGScript* PCGScriptManager::GetScript(const std::string& name) {
    auto it = m_scripts.find(name);
    if (it != m_scripts.end()) {
        return it->second.get();
    }

    // Try to load
    std::string filepath = m_scriptsPath + "/" + name + ".py";
    auto script = std::make_unique<PCGScript>();

    if (script->LoadFromFile(filepath)) {
        PCGScript* ptr = script.get();
        m_scripts[name] = std::move(script);

        // Track file modification time
        try {
            auto modTime = std::filesystem::last_write_time(filepath);
            m_fileModTimes[name] = modTime.time_since_epoch().count();
        } catch (...) {}

        return ptr;
    }

    return nullptr;
}

bool PCGScriptManager::ReloadScript(const std::string& name) {
    auto it = m_scripts.find(name);
    if (it == m_scripts.end()) {
        return GetScript(name) != nullptr;
    }

    std::string filepath = m_scriptsPath + "/" + name + ".py";
    return it->second->LoadFromFile(filepath);
}

void PCGScriptManager::ReloadAll() {
    for (auto& [name, script] : m_scripts) {
        std::string filepath = m_scriptsPath + "/" + name + ".py";
        script->LoadFromFile(filepath);
    }
}

std::vector<std::string> PCGScriptManager::GetAvailableScripts() const {
    std::vector<std::string> result;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_scriptsPath)) {
            if (entry.path().extension() == ".py") {
                result.push_back(entry.path().stem().string());
            }
        }
    } catch (...) {}

    return result;
}

void PCGScriptManager::SetScriptsPath(const std::string& path) {
    m_scriptsPath = path;
    PCGScript::AddSearchPath(path);
}

void PCGScriptManager::EnableFileWatching(bool enabled) {
    m_fileWatching = enabled;
}

void PCGScriptManager::CheckForChanges() {
    if (!m_fileWatching) return;

    for (auto& [name, script] : m_scripts) {
        std::string filepath = m_scriptsPath + "/" + name + ".py";

        try {
            auto modTime = std::filesystem::last_write_time(filepath);
            uint64_t newTime = modTime.time_since_epoch().count();

            auto it = m_fileModTimes.find(name);
            if (it != m_fileModTimes.end() && it->second != newTime) {
                script->LoadFromFile(filepath);
                m_fileModTimes[name] = newTime;
            }
        } catch (...) {}
    }
}

} // namespace Vehement
