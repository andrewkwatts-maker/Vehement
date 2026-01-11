#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>
#include <algorithm>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
struct ImGuiContext;

/**
 * @brief UI Style properties that can be customized via JSON/HTML templates
 */
struct UIStyle {
    glm::vec4 backgroundColor{0.1f, 0.1f, 0.12f, 1.0f};
    glm::vec4 textColor{0.95f, 0.95f, 0.95f, 1.0f};
    glm::vec4 borderColor{0.3f, 0.3f, 0.35f, 1.0f};
    glm::vec4 accentColor{0.4f, 0.6f, 1.0f, 1.0f};
    glm::vec4 hoverColor{0.2f, 0.2f, 0.25f, 1.0f};
    glm::vec4 activeColor{0.25f, 0.25f, 0.3f, 1.0f};
    glm::vec4 disabledColor{0.5f, 0.5f, 0.5f, 0.5f};

    float borderWidth = 1.0f;
    float borderRadius = 4.0f;
    float padding = 8.0f;
    float margin = 4.0f;
    float fontSize = 14.0f;
    std::string fontFamily = "default";

    // Serialize to/from JSON
    void ToJson(std::string& out) const;
    static UIStyle FromJson(const std::string& json);
};

/**
 * @brief Base class for all UI components
 */
class UIComponent {
public:
    using Ptr = std::shared_ptr<UIComponent>;
    using EventCallback = std::function<void(UIComponent*)>;

    UIComponent(const std::string& id = "");
    virtual ~UIComponent() = default;

    // Core properties
    void SetId(const std::string& id) { m_id = id; }
    [[nodiscard]] const std::string& GetId() const { return m_id; }

    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    void SetTooltip(const std::string& tooltip) { m_tooltip = tooltip; }
    [[nodiscard]] const std::string& GetTooltip() const { return m_tooltip; }

    // Style
    void SetStyle(const UIStyle& style) { m_style = style; }
    [[nodiscard]] const UIStyle& GetStyle() const { return m_style; }
    UIStyle& GetStyle() { return m_style; }

    // Layout
    void SetSize(const glm::vec2& size) { m_size = size; }
    [[nodiscard]] const glm::vec2& GetSize() const { return m_size; }

    void SetPosition(const glm::vec2& pos) { m_position = pos; }
    [[nodiscard]] const glm::vec2& GetPosition() const { return m_position; }

    // Events
    void OnClick(EventCallback callback) { m_onClick = callback; }
    void OnChange(EventCallback callback) { m_onChange = callback; }
    void OnHover(EventCallback callback) { m_onHover = callback; }
    void OnFocus(EventCallback callback) { m_onFocus = callback; }

    // Rendering
    virtual void Render() = 0;
    virtual void Update(float dt) {}

    // Serialization
    virtual void ToJson(std::string& out) const;
    virtual void FromJson(const std::string& json);

    // Type identification
    [[nodiscard]] virtual const char* GetTypeName() const = 0;

protected:
    void TriggerClick() { if (m_onClick) m_onClick(this); }
    void TriggerChange() { if (m_onChange) m_onChange(this); }
    void TriggerHover() { if (m_onHover) m_onHover(this); }
    void TriggerFocus() { if (m_onFocus) m_onFocus(this); }

    void ApplyStyle();
    void PopStyle();

    std::string m_id;
    bool m_visible = true;
    bool m_enabled = true;
    std::string m_tooltip;
    UIStyle m_style;
    glm::vec2 m_size{0, 0};
    glm::vec2 m_position{0, 0};

    EventCallback m_onClick;
    EventCallback m_onChange;
    EventCallback m_onHover;
    EventCallback m_onFocus;

    int m_styleColorCount = 0;
    int m_styleVarCount = 0;
};

// ============================================================================
// Basic Input Components
// ============================================================================

/**
 * @brief Text label component
 */
class UILabel : public UIComponent {
public:
    UILabel(const std::string& text = "", const std::string& id = "");

    void SetText(const std::string& text) { m_text = text; }
    [[nodiscard]] const std::string& GetText() const { return m_text; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Label"; }

private:
    std::string m_text;
};

/**
 * @brief Button component
 */
class UIButton : public UIComponent {
public:
    UIButton(const std::string& label = "Button", const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    [[nodiscard]] const std::string& GetLabel() const { return m_label; }

    void SetIcon(const std::string& icon) { m_icon = icon; }
    [[nodiscard]] const std::string& GetIcon() const { return m_icon; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Button"; }

private:
    std::string m_label;
    std::string m_icon;
};

/**
 * @brief Checkbox component
 */
class UICheckbox : public UIComponent {
public:
    UICheckbox(const std::string& label = "", bool* value = nullptr, const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void BindValue(bool* value) { m_value = value; }
    [[nodiscard]] bool GetValue() const { return m_value ? *m_value : m_internalValue; }
    void SetValue(bool value);

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Checkbox"; }

private:
    std::string m_label;
    bool* m_value = nullptr;
    bool m_internalValue = false;
};

/**
 * @brief Text input component
 */
class UITextInput : public UIComponent {
public:
    UITextInput(const std::string& label = "", const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void SetPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; }
    void SetText(const std::string& text);
    [[nodiscard]] const std::string& GetText() const { return m_text; }

    void SetMultiline(bool multiline) { m_multiline = multiline; }
    void SetMaxLength(size_t maxLength) { m_maxLength = maxLength; }
    void SetPassword(bool password) { m_password = password; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "TextInput"; }

private:
    std::string m_label;
    std::string m_placeholder;
    std::string m_text;
    bool m_multiline = false;
    bool m_password = false;
    size_t m_maxLength = 256;
    char m_buffer[1024] = {0};
};

/**
 * @brief Numeric input component with slider
 */
class UISlider : public UIComponent {
public:
    UISlider(const std::string& label = "", float* value = nullptr, const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void BindValue(float* value) { m_value = value; }
    void SetRange(float min, float max) { m_min = min; m_max = max; }
    void SetStep(float step) { m_step = step; }
    void SetFormat(const std::string& format) { m_format = format; }

    [[nodiscard]] float GetValue() const { return m_value ? *m_value : m_internalValue; }
    void SetValue(float value);

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Slider"; }

private:
    std::string m_label;
    std::string m_format = "%.2f";
    float* m_value = nullptr;
    float m_internalValue = 0.0f;
    float m_min = 0.0f;
    float m_max = 1.0f;
    float m_step = 0.01f;
};

/**
 * @brief Integer slider component
 */
class UISliderInt : public UIComponent {
public:
    UISliderInt(const std::string& label = "", int* value = nullptr, const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void BindValue(int* value) { m_value = value; }
    void SetRange(int min, int max) { m_min = min; m_max = max; }

    [[nodiscard]] int GetValue() const { return m_value ? *m_value : m_internalValue; }
    void SetValue(int value);

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "SliderInt"; }

private:
    std::string m_label;
    int* m_value = nullptr;
    int m_internalValue = 0;
    int m_min = 0;
    int m_max = 100;
};

/**
 * @brief Color picker component
 */
class UIColorPicker : public UIComponent {
public:
    UIColorPicker(const std::string& label = "", glm::vec4* value = nullptr, const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void BindValue(glm::vec4* value) { m_value = value; }
    void SetAlpha(bool alpha) { m_hasAlpha = alpha; }

    [[nodiscard]] const glm::vec4& GetValue() const { return m_value ? *m_value : m_internalValue; }
    void SetValue(const glm::vec4& value);

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "ColorPicker"; }

private:
    std::string m_label;
    glm::vec4* m_value = nullptr;
    glm::vec4 m_internalValue{1.0f, 1.0f, 1.0f, 1.0f};
    bool m_hasAlpha = true;
};

/**
 * @brief Dropdown/Combo box component
 */
class UIDropdown : public UIComponent {
public:
    UIDropdown(const std::string& label = "", const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void SetOptions(const std::vector<std::string>& options) { m_options = options; }
    void AddOption(const std::string& option) { m_options.push_back(option); }
    void ClearOptions() { m_options.clear(); m_selectedIndex = 0; }

    void SetSelectedIndex(int index);
    [[nodiscard]] int GetSelectedIndex() const { return m_selectedIndex; }
    [[nodiscard]] const std::string& GetSelectedOption() const;

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Dropdown"; }

private:
    std::string m_label;
    std::vector<std::string> m_options;
    int m_selectedIndex = 0;
};

/**
 * @brief Vector3 input component
 */
class UIVector3Input : public UIComponent {
public:
    UIVector3Input(const std::string& label = "", glm::vec3* value = nullptr, const std::string& id = "");

    void SetLabel(const std::string& label) { m_label = label; }
    void BindValue(glm::vec3* value) { m_value = value; }
    void SetRange(float min, float max) { m_min = min; m_max = max; }
    void SetSpeed(float speed) { m_speed = speed; }

    [[nodiscard]] const glm::vec3& GetValue() const { return m_value ? *m_value : m_internalValue; }
    void SetValue(const glm::vec3& value);

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Vector3Input"; }

private:
    std::string m_label;
    glm::vec3* m_value = nullptr;
    glm::vec3 m_internalValue{0.0f};
    float m_min = -1000.0f;
    float m_max = 1000.0f;
    float m_speed = 0.1f;
};

// ============================================================================
// Container Components
// ============================================================================

/**
 * @brief Container that holds child components
 */
class UIContainer : public UIComponent {
public:
    UIContainer(const std::string& id = "");

    void AddChild(UIComponent::Ptr child);
    void RemoveChild(const std::string& id);
    void ClearChildren();

    [[nodiscard]] UIComponent::Ptr GetChild(const std::string& id);
    [[nodiscard]] const std::vector<UIComponent::Ptr>& GetChildren() const { return m_children; }

    void Render() override;
    void Update(float dt) override;
    [[nodiscard]] const char* GetTypeName() const override { return "Container"; }

protected:
    std::vector<UIComponent::Ptr> m_children;
};

/**
 * @brief Panel with title and optional collapsibility
 */
class UIPanel : public UIContainer {
public:
    UIPanel(const std::string& title = "Panel", const std::string& id = "");

    void SetTitle(const std::string& title) { m_title = title; }
    [[nodiscard]] const std::string& GetTitle() const { return m_title; }

    void SetCollapsible(bool collapsible) { m_collapsible = collapsible; }
    void SetCollapsed(bool collapsed) { m_collapsed = collapsed; }
    [[nodiscard]] bool IsCollapsed() const { return m_collapsed; }

    void SetClosable(bool closable) { m_closable = closable; }
    [[nodiscard]] bool IsClosable() const { return m_closable; }

    void OnClose(EventCallback callback) { m_onClose = callback; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Panel"; }

private:
    std::string m_title;
    bool m_collapsible = true;
    bool m_collapsed = false;
    bool m_closable = false;
    EventCallback m_onClose;
};

/**
 * @brief Horizontal layout container
 */
class UIHorizontalLayout : public UIContainer {
public:
    UIHorizontalLayout(const std::string& id = "");

    void SetSpacing(float spacing) { m_spacing = spacing; }
    [[nodiscard]] float GetSpacing() const { return m_spacing; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "HorizontalLayout"; }

private:
    float m_spacing = 8.0f;
};

/**
 * @brief Vertical layout container
 */
class UIVerticalLayout : public UIContainer {
public:
    UIVerticalLayout(const std::string& id = "");

    void SetSpacing(float spacing) { m_spacing = spacing; }
    [[nodiscard]] float GetSpacing() const { return m_spacing; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "VerticalLayout"; }

private:
    float m_spacing = 4.0f;
};

/**
 * @brief Tab container
 */
class UITabContainer : public UIComponent {
public:
    UITabContainer(const std::string& id = "");

    void AddTab(const std::string& name, UIComponent::Ptr content);
    void RemoveTab(const std::string& name);
    void SetActiveTab(const std::string& name);
    [[nodiscard]] const std::string& GetActiveTab() const { return m_activeTab; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "TabContainer"; }

private:
    std::vector<std::pair<std::string, UIComponent::Ptr>> m_tabs;
    std::string m_activeTab;
};

/**
 * @brief Scrollable container
 */
class UIScrollView : public UIContainer {
public:
    UIScrollView(const std::string& id = "");

    void SetScrollX(bool scrollX) { m_scrollX = scrollX; }
    void SetScrollY(bool scrollY) { m_scrollY = scrollY; }
    void SetContentSize(const glm::vec2& size) { m_contentSize = size; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "ScrollView"; }

private:
    bool m_scrollX = false;
    bool m_scrollY = true;
    glm::vec2 m_contentSize{0, 0};
};

/**
 * @brief Grid layout container
 */
class UIGridLayout : public UIContainer {
public:
    UIGridLayout(int columns = 2, const std::string& id = "");

    void SetColumns(int columns) { m_columns = columns; }
    [[nodiscard]] int GetColumns() const { return m_columns; }

    void SetCellSize(const glm::vec2& size) { m_cellSize = size; }
    void SetSpacing(const glm::vec2& spacing) { m_spacing = spacing; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "GridLayout"; }

private:
    int m_columns = 2;
    glm::vec2 m_cellSize{100, 100};
    glm::vec2 m_spacing{4, 4};
};

// ============================================================================
// Specialized Components
// ============================================================================

/**
 * @brief Property editor for key-value pairs
 */
class UIPropertyGrid : public UIComponent {
public:
    using PropertyValue = std::variant<bool, int, float, std::string, glm::vec2, glm::vec3, glm::vec4>;

    UIPropertyGrid(const std::string& id = "");

    void AddProperty(const std::string& name, PropertyValue value);
    void SetProperty(const std::string& name, PropertyValue value);
    [[nodiscard]] PropertyValue GetProperty(const std::string& name) const;
    void RemoveProperty(const std::string& name);
    void ClearProperties();

    void OnPropertyChanged(std::function<void(const std::string&, PropertyValue)> callback) {
        m_onPropertyChanged = callback;
    }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "PropertyGrid"; }

private:
    struct Property {
        std::string name;
        PropertyValue value;
        std::string category;
    };
    std::vector<Property> m_properties;
    std::function<void(const std::string&, PropertyValue)> m_onPropertyChanged;
};

/**
 * @brief Tree view component for hierarchical data
 */
class UITreeView : public UIComponent {
public:
    struct TreeNode {
        std::string id;
        std::string label;
        std::string icon;
        bool expanded = false;
        bool selected = false;
        void* userData = nullptr;
        std::vector<std::shared_ptr<TreeNode>> children;
    };

    UITreeView(const std::string& id = "");

    std::shared_ptr<TreeNode> AddNode(const std::string& label, const std::string& parentId = "");
    void RemoveNode(const std::string& id);
    void ClearNodes();

    [[nodiscard]] std::shared_ptr<TreeNode> GetSelectedNode() const { return m_selectedNode; }
    void SetSelectedNode(const std::string& id);

    void OnSelectionChanged(std::function<void(std::shared_ptr<TreeNode>)> callback) {
        m_onSelectionChanged = callback;
    }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "TreeView"; }

private:
    void RenderNode(std::shared_ptr<TreeNode> node);
    std::shared_ptr<TreeNode> FindNode(const std::string& id, std::shared_ptr<TreeNode> root = nullptr);

    std::shared_ptr<TreeNode> m_root;
    std::shared_ptr<TreeNode> m_selectedNode;
    std::function<void(std::shared_ptr<TreeNode>)> m_onSelectionChanged;
};

/**
 * @brief List view with selectable items
 */
class UIListView : public UIComponent {
public:
    struct ListItem {
        std::string id;
        std::string label;
        std::string icon;
        std::string description;
        void* userData = nullptr;
    };

    UIListView(const std::string& id = "");

    void AddItem(const ListItem& item);
    void RemoveItem(const std::string& id);
    void ClearItems();

    void SetSelectedIndex(int index);
    [[nodiscard]] int GetSelectedIndex() const { return m_selectedIndex; }
    [[nodiscard]] const ListItem* GetSelectedItem() const;

    void SetMultiSelect(bool multiSelect) { m_multiSelect = multiSelect; }
    [[nodiscard]] const std::vector<int>& GetSelectedIndices() const { return m_selectedIndices; }

    void OnSelectionChanged(std::function<void(const ListItem*)> callback) {
        m_onSelectionChanged = callback;
    }

    void OnItemDoubleClicked(std::function<void(const ListItem*)> callback) {
        m_onItemDoubleClicked = callback;
    }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "ListView"; }

private:
    std::vector<ListItem> m_items;
    int m_selectedIndex = -1;
    std::vector<int> m_selectedIndices;
    bool m_multiSelect = false;
    std::function<void(const ListItem*)> m_onSelectionChanged;
    std::function<void(const ListItem*)> m_onItemDoubleClicked;
};

/**
 * @brief Image display component
 */
class UIImage : public UIComponent {
public:
    UIImage(const std::string& id = "");

    void SetTexture(uint32_t textureId) { m_textureId = textureId; }
    void SetTexturePath(const std::string& path) { m_texturePath = path; }
    void SetUV(const glm::vec2& uv0, const glm::vec2& uv1) { m_uv0 = uv0; m_uv1 = uv1; }
    void SetTint(const glm::vec4& tint) { m_tint = tint; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "Image"; }

private:
    uint32_t m_textureId = 0;
    std::string m_texturePath;
    glm::vec2 m_uv0{0, 0};
    glm::vec2 m_uv1{1, 1};
    glm::vec4 m_tint{1, 1, 1, 1};
};

/**
 * @brief Progress bar component
 */
class UIProgressBar : public UIComponent {
public:
    UIProgressBar(const std::string& id = "");

    void SetProgress(float progress) { m_progress = std::clamp(progress, 0.0f, 1.0f); }
    [[nodiscard]] float GetProgress() const { return m_progress; }

    void SetLabel(const std::string& label) { m_label = label; }
    void SetShowPercentage(bool show) { m_showPercentage = show; }

    void Render() override;
    [[nodiscard]] const char* GetTypeName() const override { return "ProgressBar"; }

private:
    float m_progress = 0.0f;
    std::string m_label;
    bool m_showPercentage = true;
};

// ============================================================================
// UI System Manager
// ============================================================================

/**
 * @brief Factory for creating UI components from JSON/templates
 */
class UIComponentFactory {
public:
    using CreatorFunc = std::function<UIComponent::Ptr()>;

    static UIComponentFactory& Instance();

    void RegisterType(const std::string& typeName, CreatorFunc creator);
    [[nodiscard]] UIComponent::Ptr Create(const std::string& typeName) const;
    [[nodiscard]] UIComponent::Ptr CreateFromJson(const std::string& json) const;
    [[nodiscard]] UIComponent::Ptr CreateFromTemplate(const std::string& templatePath) const;

    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;

private:
    UIComponentFactory();
    void RegisterBuiltinTypes();

    std::unordered_map<std::string, CreatorFunc> m_creators;
};

/**
 * @brief Theme manager for UI styling
 */
class UIThemeManager {
public:
    static UIThemeManager& Instance();

    void LoadTheme(const std::string& path);
    void SaveTheme(const std::string& path) const;
    void SetTheme(const std::string& name, const UIStyle& style);
    [[nodiscard]] const UIStyle& GetTheme(const std::string& name) const;
    [[nodiscard]] const UIStyle& GetActiveTheme() const { return m_activeTheme; }
    void SetActiveTheme(const std::string& name);

    [[nodiscard]] std::vector<std::string> GetThemeNames() const;

    // Preset themes
    void ApplyDarkTheme();
    void ApplyLightTheme();
    void ApplyHighContrastTheme();

private:
    UIThemeManager();
    void LoadDefaultThemes();

    std::unordered_map<std::string, UIStyle> m_themes;
    UIStyle m_activeTheme;
    std::string m_activeThemeName;
};

} // namespace Nova
