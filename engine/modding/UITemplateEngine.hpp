#pragma once

#include "UIComponents.hpp"
#include "../graphics/PreviewRenderer.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <variant>

namespace Nova {

/**
 * @brief Data binding types
 */
using BindingValue = std::variant<bool, int, float, std::string, glm::vec2, glm::vec3, glm::vec4>;

/**
 * @brief Data context for template bindings
 */
class DataContext {
public:
    DataContext() = default;
    DataContext(const std::string& name) : m_name(name) {}

    // Set values
    void Set(const std::string& key, bool value) { m_values[key] = value; }
    void Set(const std::string& key, int value) { m_values[key] = value; }
    void Set(const std::string& key, float value) { m_values[key] = value; }
    void Set(const std::string& key, const std::string& value) { m_values[key] = value; }
    void Set(const std::string& key, const glm::vec2& value) { m_values[key] = value; }
    void Set(const std::string& key, const glm::vec3& value) { m_values[key] = value; }
    void Set(const std::string& key, const glm::vec4& value) { m_values[key] = value; }

    // Get values
    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T{}) const {
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            if (const T* val = std::get_if<T>(&it->second)) {
                return *val;
            }
        }
        return defaultValue;
    }

    [[nodiscard]] bool Has(const std::string& key) const {
        return m_values.find(key) != m_values.end();
    }

    [[nodiscard]] BindingValue GetValue(const std::string& key) const {
        auto it = m_values.find(key);
        return it != m_values.end() ? it->second : BindingValue{};
    }

    // Child contexts
    void AddChild(const std::string& name, std::shared_ptr<DataContext> child) {
        m_children[name] = child;
    }

    [[nodiscard]] std::shared_ptr<DataContext> GetChild(const std::string& name) const {
        auto it = m_children.find(name);
        return it != m_children.end() ? it->second : nullptr;
    }

    // Array context for loops
    void AddArrayItem(std::shared_ptr<DataContext> item) {
        m_arrayItems.push_back(item);
    }

    [[nodiscard]] const std::vector<std::shared_ptr<DataContext>>& GetArrayItems() const {
        return m_arrayItems;
    }

    // Events
    using EventHandler = std::function<void(const std::string& eventName, DataContext* context)>;
    void SetEventHandler(EventHandler handler) { m_eventHandler = handler; }
    void TriggerEvent(const std::string& eventName) {
        if (m_eventHandler) m_eventHandler(eventName, this);
    }

    [[nodiscard]] const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
    std::unordered_map<std::string, BindingValue> m_values;
    std::unordered_map<std::string, std::shared_ptr<DataContext>> m_children;
    std::vector<std::shared_ptr<DataContext>> m_arrayItems;
    EventHandler m_eventHandler;
};

/**
 * @brief Template node types
 */
enum class TemplateNodeType {
    Element,        // UI element
    Text,           // Text content
    Binding,        // Data binding {{value}}
    Condition,      // v-if, v-else
    Loop,           // v-for
    Slot,           // Named slot for composition
    Include         // Include another template
};

/**
 * @brief Attribute with optional binding
 */
struct TemplateAttribute {
    std::string name;
    std::string value;
    bool isBound = false;          // true if value is a binding expression
    bool isEvent = false;          // true if @click, @change, etc.
    std::string bindingExpression; // Expression for computed values
};

/**
 * @brief Template node (AST node)
 */
struct TemplateNode {
    TemplateNodeType type = TemplateNodeType::Element;
    std::string tagName;           // For elements
    std::string textContent;       // For text nodes or bindings
    std::vector<TemplateAttribute> attributes;
    std::vector<std::shared_ptr<TemplateNode>> children;

    // Directives
    std::string vIf;               // Condition expression
    std::string vFor;              // Loop expression (e.g., "item in items")
    std::string vModel;            // Two-way binding
    std::string vSlot;             // Slot name
    std::string vInclude;          // Template to include

    // For loop iteration
    std::string loopVariable;      // Variable name in loop
    std::string loopSource;        // Source array name
};

/**
 * @brief Parsed template
 */
class UITemplate {
public:
    UITemplate() = default;
    UITemplate(const std::string& name) : m_name(name) {}

    void SetRoot(std::shared_ptr<TemplateNode> root) { m_root = root; }
    [[nodiscard]] std::shared_ptr<TemplateNode> GetRoot() const { return m_root; }

    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    // Slots
    void DefineSlot(const std::string& name, std::shared_ptr<TemplateNode> defaultContent = nullptr) {
        m_slots[name] = defaultContent;
    }

    [[nodiscard]] bool HasSlot(const std::string& name) const {
        return m_slots.find(name) != m_slots.end();
    }

    // Props (expected data bindings)
    void DefineProperty(const std::string& name, const std::string& type, const std::string& defaultValue = "") {
        m_props[name] = {type, defaultValue};
    }

    // Styles
    void SetStyles(const std::string& css) { m_styles = css; }
    [[nodiscard]] const std::string& GetStyles() const { return m_styles; }

private:
    std::string m_name;
    std::shared_ptr<TemplateNode> m_root;
    std::unordered_map<std::string, std::shared_ptr<TemplateNode>> m_slots;
    std::unordered_map<std::string, std::pair<std::string, std::string>> m_props; // name -> (type, default)
    std::string m_styles;
};

/**
 * @brief HTML-like template parser
 */
class TemplateParser {
public:
    /**
     * @brief Parse HTML-like template string
     */
    [[nodiscard]] static std::shared_ptr<UITemplate> Parse(const std::string& templateStr);

    /**
     * @brief Parse template from file
     */
    [[nodiscard]] static std::shared_ptr<UITemplate> ParseFile(const std::string& path);

    /**
     * @brief Parse JSON-based template
     */
    [[nodiscard]] static std::shared_ptr<UITemplate> ParseJson(const std::string& json);

private:
    static std::shared_ptr<TemplateNode> ParseElement(const std::string& html, size_t& pos);
    static std::vector<TemplateAttribute> ParseAttributes(const std::string& attrStr);
    static std::string ParseTagName(const std::string& html, size_t& pos);
    static std::string ParseAttributeValue(const std::string& html, size_t& pos);
    static void SkipWhitespace(const std::string& str, size_t& pos);
    static bool IsVoidElement(const std::string& tagName);
};

/**
 * @brief Expression evaluator for bindings
 */
class ExpressionEvaluator {
public:
    /**
     * @brief Evaluate simple expression against data context
     */
    [[nodiscard]] static BindingValue Evaluate(const std::string& expression, const DataContext& context);

    /**
     * @brief Check if condition is truthy
     */
    [[nodiscard]] static bool EvaluateCondition(const std::string& expression, const DataContext& context);

    /**
     * @brief Format string with bindings (e.g., "Hello {{name}}!")
     */
    [[nodiscard]] static std::string FormatString(const std::string& format, const DataContext& context);

private:
    static std::string EvaluateToString(const BindingValue& value);
    static std::vector<std::string> SplitPath(const std::string& path);
    static BindingValue GetNestedValue(const DataContext& context, const std::vector<std::string>& path);
};

/**
 * @brief Template renderer - converts template to UI components
 *
 * This class handles converting HTML-like templates to UI components,
 * with integrated preview rendering support via PreviewRenderer.
 */
class TemplateRenderer {
public:
    TemplateRenderer();
    ~TemplateRenderer();

    /**
     * @brief Initialize the renderer and preview system
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Render template to UI components
     */
    [[nodiscard]] static UIComponent::Ptr Render(
        std::shared_ptr<UITemplate> templ,
        std::shared_ptr<DataContext> context);

    /**
     * @brief Render template from string
     */
    [[nodiscard]] static UIComponent::Ptr RenderString(
        const std::string& templateStr,
        std::shared_ptr<DataContext> context);

    /**
     * @brief Render template from file
     */
    [[nodiscard]] static UIComponent::Ptr RenderFile(
        const std::string& path,
        std::shared_ptr<DataContext> context);

    /**
     * @brief Register custom component
     */
    static void RegisterComponent(const std::string& tagName,
        std::function<UIComponent::Ptr(const std::vector<TemplateAttribute>&, std::shared_ptr<DataContext>)> factory);

    // =========================================================================
    // Preview Rendering Support
    // =========================================================================

    /**
     * @brief Render a texture preview for template display
     *
     * @param texture Texture to preview
     * @param size Preview size in pixels
     */
    void RenderTexturePreview(std::shared_ptr<Texture> texture, const glm::ivec2& size);

    /**
     * @brief Render a 3D mesh preview for template display
     *
     * @param mesh Mesh to preview
     * @param material Material to apply
     * @param size Preview size in pixels
     */
    void RenderMeshPreview(std::shared_ptr<Mesh> mesh,
                           std::shared_ptr<Material> material,
                           const glm::ivec2& size);

    /**
     * @brief Get the preview texture ID for UI rendering
     */
    [[nodiscard]] uint32_t GetPreviewTextureID() const;

    /**
     * @brief Access the underlying PreviewRenderer
     */
    PreviewRenderer* GetPreviewRenderer() { return m_previewRenderer.get(); }
    const PreviewRenderer* GetPreviewRenderer() const { return m_previewRenderer.get(); }

private:
    static UIComponent::Ptr RenderNode(
        std::shared_ptr<TemplateNode> node,
        std::shared_ptr<DataContext> context);

    static UIComponent::Ptr CreateComponent(
        const std::string& tagName,
        const std::vector<TemplateAttribute>& attributes,
        std::shared_ptr<DataContext> context);

    static void ApplyAttributes(
        UIComponent::Ptr component,
        const std::vector<TemplateAttribute>& attributes,
        std::shared_ptr<DataContext> context);

    static std::unordered_map<std::string,
        std::function<UIComponent::Ptr(const std::vector<TemplateAttribute>&, std::shared_ptr<DataContext>)>>& GetCustomComponents();

    // Preview rendering support
    std::unique_ptr<PreviewRenderer> m_previewRenderer;
};

/**
 * @brief Template registry for caching and lookup
 */
class TemplateRegistry {
public:
    static TemplateRegistry& Instance();

    /**
     * @brief Register template
     */
    void Register(const std::string& name, std::shared_ptr<UITemplate> templ);

    /**
     * @brief Register template from string
     */
    void RegisterFromString(const std::string& name, const std::string& templateStr);

    /**
     * @brief Register template from file
     */
    void RegisterFromFile(const std::string& name, const std::string& path);

    /**
     * @brief Get registered template
     */
    [[nodiscard]] std::shared_ptr<UITemplate> Get(const std::string& name) const;

    /**
     * @brief Check if template exists
     */
    [[nodiscard]] bool Has(const std::string& name) const;

    /**
     * @brief Load all templates from directory
     */
    void LoadFromDirectory(const std::string& path, const std::string& extension = ".html");

    /**
     * @brief Get all template names
     */
    [[nodiscard]] std::vector<std::string> GetTemplateNames() const;

    /**
     * @brief Clear all templates
     */
    void Clear();

private:
    TemplateRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<UITemplate>> m_templates;
};

/**
 * @brief Reactive data binding helper
 */
class ReactiveBinding {
public:
    ReactiveBinding(std::shared_ptr<DataContext> context, UIComponent::Ptr component);

    /**
     * @brief Bind property to data
     */
    void Bind(const std::string& propertyName, const std::string& dataPath);

    /**
     * @brief Bind with transform function
     */
    template<typename T, typename U>
    void BindTransform(const std::string& propertyName, const std::string& dataPath,
                       std::function<U(const T&)> transform);

    /**
     * @brief Two-way bind (for inputs)
     */
    void BindTwoWay(const std::string& propertyName, const std::string& dataPath);

    /**
     * @brief Update bindings (call when data changes)
     */
    void Update();

private:
    std::shared_ptr<DataContext> m_context;
    UIComponent::Ptr m_component;

    struct Binding {
        std::string propertyName;
        std::string dataPath;
        bool twoWay = false;
        std::function<BindingValue(const BindingValue&)> transform;
    };
    std::vector<Binding> m_bindings;
};

// ============================================================================
// HTML Tag mappings to UI Components
// ============================================================================

/**
 * Standard tag mappings:
 *
 * <div>         -> UIContainer
 * <panel>       -> UIPanel
 * <row>         -> UIHorizontalLayout
 * <column>      -> UIVerticalLayout
 * <grid>        -> UIGridLayout
 * <scroll>      -> UIScrollView
 * <tabs>        -> UITabContainer
 * <tab>         -> Tab item in UITabContainer
 *
 * <label>       -> UILabel
 * <button>      -> UIButton
 * <checkbox>    -> UICheckbox
 * <input>       -> UITextInput
 * <slider>      -> UISlider
 * <slider-int>  -> UISliderInt
 * <color>       -> UIColorPicker
 * <select>      -> UIDropdown
 * <option>      -> Option in UIDropdown
 * <vec3>        -> UIVector3Input
 *
 * <tree>        -> UITreeView
 * <list>        -> UIListView
 * <properties>  -> UIPropertyGrid
 * <image>       -> UIImage
 * <progress>    -> UIProgressBar
 *
 * Attributes:
 * - id="component-id"
 * - class="style-class"
 * - style="inline styles"
 * - :prop="binding"           (one-way binding)
 * - v-model="binding"         (two-way binding)
 * - v-if="condition"          (conditional rendering)
 * - v-for="item in items"     (loop rendering)
 * - @event="handler"          (event binding)
 * - #slot="slotName"          (named slot)
 */

} // namespace Nova
