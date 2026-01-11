#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace Nova {
namespace Scripting {

/**
 * @brief Parameter information for API function
 */
struct APIParameter {
    std::string name;
    std::string type;
    std::string description;
    std::string defaultValue;
    bool optional = false;
};

/**
 * @brief Complete API function documentation
 */
struct APIFunctionDoc {
    std::string name;
    std::string qualifiedName;
    std::string signature;
    std::string description;
    std::string longDescription;
    std::string category;
    std::vector<APIParameter> parameters;
    std::string returnType;
    std::string returnDescription;
    std::string example;
    std::vector<std::string> seeAlso;
    std::vector<std::string> tags;
    bool deprecated = false;
    std::string deprecationMessage;
    std::string sinceVersion;
};

/**
 * @brief API class/type documentation
 */
struct APITypeDoc {
    std::string name;
    std::string description;
    std::string category;
    std::vector<APIFunctionDoc> methods;
    std::vector<APIParameter> properties;
    std::string example;
    std::vector<std::string> baseTypes;
};

/**
 * @brief Event type documentation
 */
struct APIEventDoc {
    std::string name;
    std::string description;
    std::string category;
    std::vector<APIParameter> dataFields;
    std::string example;
    std::string handlerSignature;
};

/**
 * @brief Auto-completion item for IDE integration
 */
struct AutoCompleteItem {
    std::string text;
    std::string displayText;
    std::string insertText;
    std::string detail;
    std::string documentation;
    std::string kind;  // "function", "class", "property", "constant"
    int sortOrder = 0;
};

/**
 * @brief Game API documentation and auto-completion provider
 *
 * Generates:
 * - API documentation from C++ bindings
 * - Auto-completion data for editor
 * - Python type stubs (.pyi files)
 * - HTML documentation
 *
 * Usage:
 * @code
 * GameAPI api;
 * api.Initialize();
 *
 * // Register a function
 * APIFunctionDoc func;
 * func.name = "spawn_entity";
 * func.signature = "spawn_entity(type: str, x: float, y: float, z: float) -> int";
 * func.description = "Spawn a new entity";
 * api.RegisterFunction(func);
 *
 * // Get completions
 * auto completions = api.GetCompletions("spawn");
 *
 * // Generate type stubs
 * api.GenerateTypeStubs("game_api.pyi");
 * @endcode
 */
class GameAPI {
public:
    GameAPI();
    ~GameAPI();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize();
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Function Documentation
    // =========================================================================

    /**
     * @brief Register an API function
     */
    void RegisterFunction(const APIFunctionDoc& func);

    /**
     * @brief Get function documentation
     */
    [[nodiscard]] std::optional<APIFunctionDoc> GetFunction(const std::string& name) const;

    /**
     * @brief Get all functions
     */
    [[nodiscard]] const std::vector<APIFunctionDoc>& GetAllFunctions() const { return m_functions; }

    /**
     * @brief Get functions by category
     */
    [[nodiscard]] std::vector<APIFunctionDoc> GetFunctionsByCategory(const std::string& category) const;

    /**
     * @brief Get function categories
     */
    [[nodiscard]] std::vector<std::string> GetFunctionCategories() const;

    // =========================================================================
    // Type Documentation
    // =========================================================================

    /**
     * @brief Register an API type/class
     */
    void RegisterType(const APITypeDoc& type);

    /**
     * @brief Get type documentation
     */
    [[nodiscard]] std::optional<APITypeDoc> GetType(const std::string& name) const;

    /**
     * @brief Get all types
     */
    [[nodiscard]] const std::vector<APITypeDoc>& GetAllTypes() const { return m_types; }

    // =========================================================================
    // Event Documentation
    // =========================================================================

    /**
     * @brief Register an event type
     */
    void RegisterEvent(const APIEventDoc& event);

    /**
     * @brief Get event documentation
     */
    [[nodiscard]] std::optional<APIEventDoc> GetEvent(const std::string& name) const;

    /**
     * @brief Get all events
     */
    [[nodiscard]] const std::vector<APIEventDoc>& GetAllEvents() const { return m_events; }

    // =========================================================================
    // Auto-completion
    // =========================================================================

    /**
     * @brief Get auto-completion items for prefix
     */
    [[nodiscard]] std::vector<AutoCompleteItem> GetCompletions(const std::string& prefix) const;

    /**
     * @brief Get completion items for a context (e.g., after dot)
     */
    [[nodiscard]] std::vector<AutoCompleteItem> GetCompletionsForContext(
        const std::string& context, const std::string& prefix) const;

    /**
     * @brief Get all completion items
     */
    [[nodiscard]] std::vector<AutoCompleteItem> GetAllCompletions() const;

    /**
     * @brief Export completions to JSON for external editors
     */
    bool ExportCompletions(const std::string& filePath) const;

    // =========================================================================
    // Documentation Generation
    // =========================================================================

    /**
     * @brief Generate Python type stubs (.pyi file)
     */
    bool GenerateTypeStubs(const std::string& filePath) const;

    /**
     * @brief Generate HTML documentation
     */
    bool GenerateHTMLDocs(const std::string& outputDir) const;

    /**
     * @brief Generate Markdown documentation
     */
    bool GenerateMarkdownDocs(const std::string& filePath) const;

    /**
     * @brief Get documentation as JSON
     */
    std::string GetDocumentationJSON() const;

    // =========================================================================
    // Search
    // =========================================================================

    /**
     * @brief Search API by query
     */
    struct SearchResult {
        enum class Type { Function, Type, Event, Property };
        Type type;
        std::string name;
        std::string description;
        std::string signature;
        float relevance;
    };

    [[nodiscard]] std::vector<SearchResult> Search(const std::string& query) const;

private:
    // Built-in API registration
    void RegisterBuiltinAPI();
    void RegisterEntityAPI();
    void RegisterCombatAPI();
    void RegisterQueryAPI();
    void RegisterAudioVisualAPI();
    void RegisterUIAPI();
    void RegisterUtilityAPI();
    void RegisterBuiltinTypes();
    void RegisterBuiltinEvents();

    // Helpers
    AutoCompleteItem FunctionToCompletion(const APIFunctionDoc& func) const;
    AutoCompleteItem TypeToCompletion(const APITypeDoc& type) const;
    std::string FormatSignature(const APIFunctionDoc& func) const;
    std::string GenerateStubForFunction(const APIFunctionDoc& func) const;
    std::string GenerateStubForType(const APITypeDoc& type) const;

    // State
    bool m_initialized = false;

    // Documentation
    std::vector<APIFunctionDoc> m_functions;
    std::vector<APITypeDoc> m_types;
    std::vector<APIEventDoc> m_events;

    // Indexes
    std::unordered_map<std::string, size_t> m_functionIndex;
    std::unordered_map<std::string, size_t> m_typeIndex;
    std::unordered_map<std::string, size_t> m_eventIndex;

    // Cached completions
    mutable std::vector<AutoCompleteItem> m_cachedCompletions;
    mutable bool m_completionsCacheDirty = true;
};

} // namespace Scripting
} // namespace Nova
