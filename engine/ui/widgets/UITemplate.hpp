#pragma once

#include "UIWidget.hpp"
#include "CoreWidgets.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <filesystem>

namespace Nova {
namespace UI {

// =============================================================================
// Template Slot - Placeholder for dynamic content
// =============================================================================

class UISlot : public UIWidget {
public:
    UISlot(const std::string& name) : UIWidget("slot"), m_slotName(name) {}

    const std::string& GetSlotName() const { return m_slotName; }

    void SetContent(WidgetPtr content);
    WidgetPtr GetContent() const { return m_content; }

private:
    std::string m_slotName;
    WidgetPtr m_content;
};

// =============================================================================
// Template Definition - Reusable widget template
// =============================================================================

/**
 * @brief Represents a reusable UI template that can be instantiated multiple times
 *
 * Templates support:
 * - Slots for content injection
 * - Props for configuration
 * - Default styles
 * - Event forwarding
 */
class UITemplate {
public:
    UITemplate(const std::string& id);

    const std::string& GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    /**
     * @brief Define a prop that can be passed when instantiating
     */
    void DefineProp(const std::string& name, const std::string& type,
                   const std::any& defaultValue = {});

    /**
     * @brief Define a slot where content can be injected
     */
    void DefineSlot(const std::string& name, bool required = false);

    /**
     * @brief Set the template content (root widget structure)
     */
    void SetContent(const nlohmann::json& content);
    void SetContentFromString(const std::string& jsonOrHtml);

    /**
     * @brief Create an instance of this template
     */
    WidgetPtr Instantiate(const std::unordered_map<std::string, std::any>& props = {},
                         const std::unordered_map<std::string, WidgetPtr>& slots = {});

    /**
     * @brief Get default style for the template root
     */
    const UIStyle& GetDefaultStyle() const { return m_defaultStyle; }
    void SetDefaultStyle(const UIStyle& style) { m_defaultStyle = style; }

private:
    struct PropDef {
        std::string name;
        std::string type;
        std::any defaultValue;
        bool required = false;
    };

    struct SlotDef {
        std::string name;
        bool required = false;
    };

    std::string m_id;
    std::string m_name;
    nlohmann::json m_content;
    UIStyle m_defaultStyle;
    std::vector<PropDef> m_props;
    std::vector<SlotDef> m_slots;
};

// =============================================================================
// Template Registry - Global template storage
// =============================================================================

class UITemplateRegistry {
public:
    static UITemplateRegistry& Instance();

    void Register(std::shared_ptr<UITemplate> templ);
    void Unregister(const std::string& id);

    std::shared_ptr<UITemplate> Get(const std::string& id) const;
    bool Has(const std::string& id) const;

    void LoadFromFile(const std::filesystem::path& filepath);
    void LoadFromDirectory(const std::filesystem::path& directory, bool recursive = true);
    void LoadFromJSON(const nlohmann::json& json);

    std::vector<std::string> GetTemplateIds() const;

private:
    UITemplateRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<UITemplate>> m_templates;
};

// =============================================================================
// UI Parser - Parse JSON/HTML-like syntax to widgets
// =============================================================================

/**
 * @brief Parses UI definitions from JSON or HTML-like strings
 */
class UIParser {
public:
    /**
     * @brief Parse a JSON widget definition
     */
    static WidgetPtr ParseJSON(const nlohmann::json& json);
    static WidgetPtr ParseJSON(const std::string& jsonString);

    /**
     * @brief Parse HTML-like markup (simplified subset)
     *
     * Supported syntax:
     * <tagname id="..." class="..." style="..." @click="handler" :prop="binding">
     *   content
     * </tagname>
     *
     * Directives:
     * - @event="handler" - Event binding
     * - :prop="path" - Data binding
     * - v-if="condition" - Conditional rendering
     * - v-for="item in items" - List rendering
     */
    static WidgetPtr ParseHTML(const std::string& html);

    /**
     * @brief Parse a style string (CSS-like)
     */
    static UIStyle ParseStyle(const std::string& styleString);

    /**
     * @brief Parse a style property value
     */
    static void ParseStyleProperty(UIStyle& style, const std::string& property,
                                  const std::string& value);

    /**
     * @brief Parse a length value (e.g., "10px", "50%", "auto")
     */
    static Length ParseLength(const std::string& value);

    /**
     * @brief Parse a color value (e.g., "#ff0000", "rgb(255,0,0)", "red")
     */
    static glm::vec4 ParseColor(const std::string& value);

private:
    static WidgetPtr CreateWidgetFromTag(const std::string& tag);
    static void ApplyAttributes(WidgetPtr widget, const nlohmann::json& attrs);
    static void ApplyBindings(WidgetPtr widget, const nlohmann::json& bindings);
};

// =============================================================================
// UI Stylesheet - CSS-like style definitions
// =============================================================================

/**
 * @brief Style rule with selector
 */
struct StyleRule {
    std::string selector;  // e.g., ".button", "#myId", "panel.large"
    UIStyle style;
    int specificity = 0;
};

/**
 * @brief Collection of style rules (like a CSS stylesheet)
 */
class UIStyleSheet {
public:
    UIStyleSheet(const std::string& id = "");

    const std::string& GetId() const { return m_id; }

    void AddRule(const std::string& selector, const UIStyle& style);
    void AddRule(const StyleRule& rule);

    void LoadFromString(const std::string& css);
    void LoadFromJSON(const nlohmann::json& json);
    void LoadFromFile(const std::filesystem::path& filepath);

    /**
     * @brief Get matching rules for a widget (sorted by specificity)
     */
    std::vector<const StyleRule*> GetMatchingRules(const UIWidget& widget) const;

    /**
     * @brief Compute the final style for a widget by merging all matching rules
     */
    UIStyle ComputeStyle(const UIWidget& widget) const;

    const std::vector<StyleRule>& GetRules() const { return m_rules; }

private:
    static int CalculateSpecificity(const std::string& selector);
    static bool SelectorMatches(const std::string& selector, const UIWidget& widget);

    std::string m_id;
    std::vector<StyleRule> m_rules;
};

/**
 * @brief Global stylesheet registry
 */
class UIStyleSheetRegistry {
public:
    static UIStyleSheetRegistry& Instance();

    void Add(std::shared_ptr<UIStyleSheet> stylesheet);
    void Remove(const std::string& id);
    std::shared_ptr<UIStyleSheet> Get(const std::string& id) const;

    void SetGlobalStyleSheet(std::shared_ptr<UIStyleSheet> stylesheet);
    std::shared_ptr<UIStyleSheet> GetGlobalStyleSheet() const { return m_globalStyleSheet; }

    UIStyle ComputeStyle(const UIWidget& widget) const;

private:
    UIStyleSheetRegistry() = default;
    std::unordered_map<std::string, std::shared_ptr<UIStyleSheet>> m_stylesheets;
    std::shared_ptr<UIStyleSheet> m_globalStyleSheet;
};

// =============================================================================
// Reactive Computed Property
// =============================================================================

/**
 * @brief A computed property that automatically updates when dependencies change
 */
template<typename T>
class ComputedProperty {
public:
    using ComputeFunc = std::function<T()>;

    ComputedProperty(ComputeFunc compute) : m_compute(std::move(compute)) {}

    const T& Get() {
        if (m_dirty) {
            m_cachedValue = m_compute();
            m_dirty = false;
        }
        return m_cachedValue;
    }

    void Invalidate() { m_dirty = true; }

    Reflect::ObserverConnection OnChanged(std::function<void(const T&, const T&)> callback) {
        return m_observable.OnChanged(std::move(callback));
    }

private:
    ComputeFunc m_compute;
    T m_cachedValue;
    bool m_dirty = true;
    Reflect::Observable<T> m_observable;
};

// =============================================================================
// UI Context - Runtime context for UI rendering
// =============================================================================

/**
 * @brief Runtime context passed during UI rendering and updates
 */
class UIContext {
public:
    UIContext();

    // Viewport
    void SetViewport(float width, float height);
    float GetViewportWidth() const { return m_viewportWidth; }
    float GetViewportHeight() const { return m_viewportHeight; }

    // Focus management
    void SetFocusedWidget(WidgetPtr widget);
    WidgetPtr GetFocusedWidget() const { return m_focusedWidget.lock(); }

    // Hover tracking
    void SetHoveredWidget(WidgetPtr widget);
    WidgetPtr GetHoveredWidget() const { return m_hoveredWidget.lock(); }

    // Modal stack
    void PushModal(WidgetPtr modal);
    void PopModal();
    WidgetPtr GetTopModal() const;

    // Tooltip
    void ShowTooltip(const std::string& text, const glm::vec2& position);
    void HideTooltip();

    // Drag and drop
    void StartDrag(WidgetPtr source, std::any data);
    void EndDrag();
    bool IsDragging() const { return m_isDragging; }
    WidgetPtr GetDragSource() const { return m_dragSource.lock(); }
    const std::any& GetDragData() const { return m_dragData; }

    // Clipboard
    void SetClipboard(const std::string& text);
    std::string GetClipboard() const;

    // Time
    float GetDeltaTime() const { return m_deltaTime; }
    float GetTotalTime() const { return m_totalTime; }
    void Update(float deltaTime);

    // Style sheets
    void AddStyleSheet(std::shared_ptr<UIStyleSheet> stylesheet);
    UIStyle ComputeStyle(const UIWidget& widget) const;

    // Theme colors
    void SetThemeColor(const std::string& name, const glm::vec4& color);
    glm::vec4 GetThemeColor(const std::string& name) const;

private:
    float m_viewportWidth = 1920.0f;
    float m_viewportHeight = 1080.0f;

    WidgetWeakPtr m_focusedWidget;
    WidgetWeakPtr m_hoveredWidget;
    std::vector<WidgetWeakPtr> m_modalStack;

    bool m_isDragging = false;
    WidgetWeakPtr m_dragSource;
    std::any m_dragData;

    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;

    std::vector<std::shared_ptr<UIStyleSheet>> m_styleSheets;
    std::unordered_map<std::string, glm::vec4> m_themeColors;
};

} // namespace UI
} // namespace Nova
