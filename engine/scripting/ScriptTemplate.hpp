#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace Nova {
namespace Scripting {

/**
 * @brief Template variable definition
 */
struct TemplateVariable {
    std::string name;
    std::string defaultValue;
    std::string description;
    std::string type;  // "string", "int", "float", "bool", "select"
    std::vector<std::string> options;  // For select type
    bool required = true;
};

/**
 * @brief Script template definition
 */
struct ScriptTemplate {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::string content;
    std::vector<TemplateVariable> variables;
    std::vector<std::string> tags;

    // Cursor position after generation
    int cursorLine = 0;
    int cursorColumn = 0;
};

/**
 * @brief Manages script templates for various game events and behaviors
 *
 * Provides templates for:
 * - OnCreate, OnTick, OnEvent handlers
 * - AI behavior scripts
 * - Spell effects
 * - Conditions
 *
 * Templates use {{variable}} placeholders that can be filled in.
 *
 * Usage:
 * @code
 * ScriptTemplateManager templates;
 * templates.Initialize("templates/");
 *
 * // Get template
 * auto tmpl = templates.GetTemplate("on_damage");
 *
 * // Fill variables
 * std::unordered_map<std::string, std::string> vars = {
 *     {"entity_type", "zombie"},
 *     {"damage_multiplier", "1.5"}
 * };
 *
 * std::string code = templates.Generate("on_damage", vars);
 * @endcode
 */
class ScriptTemplateManager {
public:
    ScriptTemplateManager();
    ~ScriptTemplateManager();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(const std::string& templatesPath = "");
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Template Access
    // =========================================================================

    /**
     * @brief Get template by ID
     */
    [[nodiscard]] std::optional<ScriptTemplate> GetTemplate(const std::string& id) const;

    /**
     * @brief Get all templates
     */
    [[nodiscard]] const std::vector<ScriptTemplate>& GetAllTemplates() const { return m_templates; }

    /**
     * @brief Get templates by category
     */
    [[nodiscard]] std::vector<ScriptTemplate> GetTemplatesByCategory(const std::string& category) const;

    /**
     * @brief Get template categories
     */
    [[nodiscard]] std::vector<std::string> GetCategories() const;

    /**
     * @brief Search templates by name/description
     */
    [[nodiscard]] std::vector<ScriptTemplate> SearchTemplates(const std::string& query) const;

    // =========================================================================
    // Code Generation
    // =========================================================================

    /**
     * @brief Generate code from template
     * @param templateId Template ID
     * @param variables Variable values to substitute
     * @return Generated code
     */
    std::string Generate(const std::string& templateId,
                        const std::unordered_map<std::string, std::string>& variables = {});

    /**
     * @brief Generate code from template content directly
     */
    std::string GenerateFromContent(const std::string& content,
                                    const std::unordered_map<std::string, std::string>& variables);

    /**
     * @brief Validate variable values for a template
     */
    bool ValidateVariables(const std::string& templateId,
                          const std::unordered_map<std::string, std::string>& variables,
                          std::vector<std::string>& errors);

    // =========================================================================
    // Template Management
    // =========================================================================

    /**
     * @brief Register a custom template
     */
    void RegisterTemplate(const ScriptTemplate& tmpl);

    /**
     * @brief Remove a template
     */
    void RemoveTemplate(const std::string& id);

    /**
     * @brief Load templates from directory
     */
    int LoadTemplates(const std::string& directory);

    /**
     * @brief Save template to file
     */
    bool SaveTemplate(const ScriptTemplate& tmpl, const std::string& path);

    // =========================================================================
    // Built-in Templates
    // =========================================================================

    /**
     * @brief Get OnCreate handler template
     */
    [[nodiscard]] ScriptTemplate GetOnCreateTemplate() const;

    /**
     * @brief Get OnTick handler template
     */
    [[nodiscard]] ScriptTemplate GetOnTickTemplate() const;

    /**
     * @brief Get OnEvent handler template
     */
    [[nodiscard]] ScriptTemplate GetOnEventTemplate() const;

    /**
     * @brief Get AI behavior template
     */
    [[nodiscard]] ScriptTemplate GetAIBehaviorTemplate() const;

    /**
     * @brief Get spell effect template
     */
    [[nodiscard]] ScriptTemplate GetSpellEffectTemplate() const;

    /**
     * @brief Get condition check template
     */
    [[nodiscard]] ScriptTemplate GetConditionTemplate() const;

private:
    // Helpers
    void RegisterBuiltinTemplates();
    std::string SubstituteVariables(const std::string& content,
                                    const std::unordered_map<std::string, std::string>& variables);
    ScriptTemplate ParseTemplateFile(const std::string& path);

    // State
    bool m_initialized = false;
    std::string m_templatesPath;
    std::vector<ScriptTemplate> m_templates;
    std::unordered_map<std::string, size_t> m_templateIndex;
};

} // namespace Scripting
} // namespace Nova
