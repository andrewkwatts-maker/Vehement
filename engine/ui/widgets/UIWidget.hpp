#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <any>
#include <optional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "../reflection/Observable.hpp"
#include "../reflection/TypeInfo.hpp"

namespace Nova {
namespace UI {

// Forward declarations
class UIWidget;
class UIContext;
class UIStyleSheet;
class DataBinding;

using WidgetPtr = std::shared_ptr<UIWidget>;
using WidgetWeakPtr = std::weak_ptr<UIWidget>;

// =============================================================================
// Layout and Styling Enums
// =============================================================================

enum class LayoutDirection { Row, Column, RowReverse, ColumnReverse };
enum class Alignment { Start, Center, End, Stretch, SpaceBetween, SpaceAround };
enum class Overflow { Visible, Hidden, Scroll, Auto };
enum class Position { Static, Relative, Absolute, Fixed };
enum class Display { Flex, Block, Inline, None, Grid };
enum class TextAlign { Left, Center, Right, Justify };
enum class FontWeight { Normal, Bold, Light, Medium, SemiBold, ExtraBold };

/**
 * @brief Length value supporting multiple units (px, %, em, auto)
 */
struct Length {
    enum class Unit { Pixels, Percent, Em, Auto, ViewportWidth, ViewportHeight };

    float value = 0.0f;
    Unit unit = Unit::Pixels;

    Length() = default;
    Length(float v, Unit u = Unit::Pixels) : value(v), unit(u) {}

    static Length Px(float v) { return Length(v, Unit::Pixels); }
    static Length Pct(float v) { return Length(v, Unit::Percent); }
    static Length Em(float v) { return Length(v, Unit::Em); }
    static Length Auto() { return Length(0, Unit::Auto); }
    static Length Vw(float v) { return Length(v, Unit::ViewportWidth); }
    static Length Vh(float v) { return Length(v, Unit::ViewportHeight); }

    bool IsAuto() const { return unit == Unit::Auto; }

    float Resolve(float parentSize, float emSize = 16.0f, float viewportW = 1920.0f, float viewportH = 1080.0f) const {
        switch (unit) {
            case Unit::Pixels: return value;
            case Unit::Percent: return value * parentSize / 100.0f;
            case Unit::Em: return value * emSize;
            case Unit::ViewportWidth: return value * viewportW / 100.0f;
            case Unit::ViewportHeight: return value * viewportH / 100.0f;
            case Unit::Auto: return 0.0f;
        }
        return value;
    }
};

/**
 * @brief Box model spacing (margin, padding, border)
 */
struct BoxSpacing {
    Length top, right, bottom, left;

    BoxSpacing() = default;
    BoxSpacing(Length all) : top(all), right(all), bottom(all), left(all) {}
    BoxSpacing(Length vertical, Length horizontal)
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    BoxSpacing(Length t, Length r, Length b, Length l)
        : top(t), right(r), bottom(b), left(l) {}

    static BoxSpacing All(float px) { return BoxSpacing(Length::Px(px)); }
};

/**
 * @brief Border specification
 */
struct Border {
    float width = 0.0f;
    glm::vec4 color{0.0f, 0.0f, 0.0f, 1.0f};
    float radius = 0.0f;  // Corner radius

    Border() = default;
    Border(float w, const glm::vec4& c, float r = 0.0f) : width(w), color(c), radius(r) {}
};

/**
 * @brief Complete style specification for a widget
 */
struct UIStyle {
    // Layout
    Display display = Display::Flex;
    Position position = Position::Static;
    LayoutDirection flexDirection = LayoutDirection::Row;
    Alignment alignItems = Alignment::Stretch;
    Alignment justifyContent = Alignment::Start;
    Alignment alignSelf = Alignment::Stretch;
    float flexGrow = 0.0f;
    float flexShrink = 1.0f;
    Length flexBasis = Length::Auto();
    float gap = 0.0f;

    // Dimensions
    Length width = Length::Auto();
    Length height = Length::Auto();
    Length minWidth = Length::Px(0);
    Length minHeight = Length::Px(0);
    Length maxWidth = Length::Auto();
    Length maxHeight = Length::Auto();

    // Positioning
    Length top = Length::Auto();
    Length right = Length::Auto();
    Length bottom = Length::Auto();
    Length left = Length::Auto();

    // Box model
    BoxSpacing margin;
    BoxSpacing padding;
    Border border;

    // Background
    glm::vec4 backgroundColor{0.0f};
    std::string backgroundImage;

    // Text
    TextAlign textAlign = TextAlign::Left;
    glm::vec4 color{1.0f};
    float fontSize = 14.0f;
    std::string fontFamily = "default";
    FontWeight fontWeight = FontWeight::Normal;
    float lineHeight = 1.2f;

    // Overflow
    Overflow overflowX = Overflow::Visible;
    Overflow overflowY = Overflow::Visible;

    // Effects
    float opacity = 1.0f;
    int zIndex = 0;
    bool visible = true;

    // Interaction
    bool pointerEvents = true;
    std::string cursor = "default";

    // Transitions
    float transitionDuration = 0.0f;
    std::string transitionProperty = "all";
};

// =============================================================================
// Data Binding System
// =============================================================================

/**
 * @brief Binding expression types
 */
enum class BindingMode {
    OneWay,         // Source -> UI only
    TwoWay,         // Source <-> UI
    OneWayToSource, // UI -> Source only
    OneTime         // Single initial binding
};

/**
 * @brief Property path for nested binding (e.g., "player.stats.health")
 */
struct PropertyPath {
    std::vector<std::string> segments;

    PropertyPath() = default;
    PropertyPath(const std::string& path);

    bool IsEmpty() const { return segments.empty(); }
    std::string ToString() const;

    static PropertyPath Parse(const std::string& path);
};

/**
 * @brief Data binding connection between a data source and widget property
 */
class DataBinding {
public:
    DataBinding(const std::string& sourcePath, const std::string& targetProperty,
                BindingMode mode = BindingMode::OneWay);

    void SetConverter(std::function<std::any(const std::any&)> toTarget,
                     std::function<std::any(const std::any&)> toSource = nullptr);

    void SetFormatter(const std::string& format);

    void Bind(void* sourceObject, const Reflect::TypeInfo* sourceType, UIWidget* target);
    void Unbind();
    void UpdateTarget();
    void UpdateSource();

    const std::string& GetSourcePath() const { return m_sourcePath; }
    const std::string& GetTargetProperty() const { return m_targetProperty; }
    BindingMode GetMode() const { return m_mode; }

private:
    std::string m_sourcePath;
    std::string m_targetProperty;
    BindingMode m_mode;
    std::string m_format;

    void* m_sourceObject = nullptr;
    const Reflect::TypeInfo* m_sourceType = nullptr;
    UIWidget* m_targetWidget = nullptr;

    std::function<std::any(const std::any&)> m_toTargetConverter;
    std::function<std::any(const std::any&)> m_toSourceConverter;

    Reflect::ObserverConnection m_connection;
};

// =============================================================================
// Event System
// =============================================================================

/**
 * @brief UI event data
 */
struct UIEvent {
    enum class Type {
        Click, DoubleClick, MouseDown, MouseUp, MouseMove,
        MouseEnter, MouseLeave, Scroll, DragStart, Drag, DragEnd, Drop,
        KeyDown, KeyUp, KeyPress, Focus, Blur,
        ValueChanged, Submit, Cancel,
        Custom
    };

    Type type;
    UIWidget* target = nullptr;
    UIWidget* currentTarget = nullptr;

    // Mouse data
    glm::vec2 position{0.0f};
    glm::vec2 localPosition{0.0f};
    int button = 0;
    bool ctrlKey = false;
    bool shiftKey = false;
    bool altKey = false;

    // Keyboard data
    int keyCode = 0;
    std::string key;

    // Scroll data
    glm::vec2 delta{0.0f};

    // Custom data
    std::string customType;
    std::any data;

    // Propagation control
    bool propagationStopped = false;
    bool defaultPrevented = false;

    void StopPropagation() { propagationStopped = true; }
    void PreventDefault() { defaultPrevented = true; }
};

using EventHandler = std::function<void(UIEvent&)>;

// =============================================================================
// Base Widget Class
// =============================================================================

/**
 * @brief Base class for all UI widgets
 *
 * Provides HTML-like hierarchical structure with CSS-like styling,
 * reflection-based data binding, and event handling.
 */
class UIWidget : public std::enable_shared_from_this<UIWidget> {
public:
    UIWidget(const std::string& tagName = "div");
    virtual ~UIWidget();

    // =========================================================================
    // Identity
    // =========================================================================

    const std::string& GetId() const { return m_id; }
    void SetId(const std::string& id) { m_id = id; }

    const std::string& GetTagName() const { return m_tagName; }

    const std::vector<std::string>& GetClasses() const { return m_classes; }
    void AddClass(const std::string& className);
    void RemoveClass(const std::string& className);
    bool HasClass(const std::string& className) const;
    void ToggleClass(const std::string& className);
    void SetClass(const std::string& className); // Replaces all classes

    // =========================================================================
    // Hierarchy
    // =========================================================================

    WidgetPtr GetParent() const { return m_parent.lock(); }
    const std::vector<WidgetPtr>& GetChildren() const { return m_children; }

    void AppendChild(WidgetPtr child);
    void InsertChild(WidgetPtr child, size_t index);
    void RemoveChild(WidgetPtr child);
    void RemoveChildAt(size_t index);
    void ClearChildren();

    WidgetPtr FindById(const std::string& id);
    std::vector<WidgetPtr> FindByClass(const std::string& className);
    std::vector<WidgetPtr> FindByTagName(const std::string& tagName);
    WidgetPtr QuerySelector(const std::string& selector);
    std::vector<WidgetPtr> QuerySelectorAll(const std::string& selector);

    // =========================================================================
    // Styling
    // =========================================================================

    UIStyle& GetStyle() { return m_style; }
    const UIStyle& GetStyle() const { return m_style; }
    void SetStyle(const UIStyle& style) { m_style = style; MarkDirty(); }

    void SetStyleProperty(const std::string& property, const std::any& value);
    std::any GetStyleProperty(const std::string& property) const;

    // Computed layout (after layout pass)
    const glm::vec4& GetComputedRect() const { return m_computedRect; }
    glm::vec2 GetComputedSize() const { return {m_computedRect.z, m_computedRect.w}; }
    glm::vec2 GetComputedPosition() const { return {m_computedRect.x, m_computedRect.y}; }

    // =========================================================================
    // Data Binding
    // =========================================================================

    /**
     * @brief Bind this widget to a data context object
     * @param dataContext Pointer to the source object
     * @param typeInfo Reflection info for the source type
     */
    void SetDataContext(void* dataContext, const Reflect::TypeInfo* typeInfo);
    void* GetDataContext() const { return m_dataContext; }
    const Reflect::TypeInfo* GetDataContextType() const { return m_dataContextType; }

    /**
     * @brief Add a data binding for a property
     * @param binding The binding configuration
     */
    void AddBinding(std::unique_ptr<DataBinding> binding);
    void RemoveBinding(const std::string& targetProperty);
    void ClearBindings();
    void UpdateBindings();

    /**
     * @brief Shorthand for common binding patterns
     */
    void BindProperty(const std::string& widgetProperty, const std::string& dataPath,
                     BindingMode mode = BindingMode::OneWay);
    void BindText(const std::string& dataPath, const std::string& format = "");
    void BindVisible(const std::string& dataPath, bool invert = false);
    void BindEnabled(const std::string& dataPath, bool invert = false);

    // =========================================================================
    // Content
    // =========================================================================

    const std::string& GetText() const { return m_text; }
    void SetText(const std::string& text);

    /**
     * @brief Set inner HTML-like content (parses and creates child widgets)
     */
    void SetInnerHTML(const std::string& html);
    std::string GetInnerHTML() const;

    // =========================================================================
    // Events
    // =========================================================================

    void AddEventListener(UIEvent::Type type, EventHandler handler);
    void AddEventListener(const std::string& customType, EventHandler handler);
    void RemoveEventListeners(UIEvent::Type type);
    void DispatchEvent(UIEvent& event);

    // Convenience event handlers
    void OnClick(EventHandler handler) { AddEventListener(UIEvent::Type::Click, handler); }
    void OnDoubleClick(EventHandler handler) { AddEventListener(UIEvent::Type::DoubleClick, handler); }
    void OnMouseEnter(EventHandler handler) { AddEventListener(UIEvent::Type::MouseEnter, handler); }
    void OnMouseLeave(EventHandler handler) { AddEventListener(UIEvent::Type::MouseLeave, handler); }
    void OnValueChanged(EventHandler handler) { AddEventListener(UIEvent::Type::ValueChanged, handler); }

    // =========================================================================
    // State
    // =========================================================================

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled);

    bool IsFocused() const { return m_focused; }
    void Focus();
    void Blur();

    bool IsHovered() const { return m_hovered; }
    bool IsPressed() const { return m_pressed; }

    // =========================================================================
    // Attributes (like HTML attributes)
    // =========================================================================

    void SetAttribute(const std::string& name, const std::string& value);
    std::string GetAttribute(const std::string& name) const;
    bool HasAttribute(const std::string& name) const;
    void RemoveAttribute(const std::string& name);

    // =========================================================================
    // Lifecycle
    // =========================================================================

    virtual void Update(float deltaTime);
    virtual void Render(UIContext& context);
    virtual void Layout(const glm::vec4& parentRect);

    bool IsDirty() const { return m_dirty; }
    void MarkDirty();

protected:
    virtual void OnChildAdded(WidgetPtr child) {}
    virtual void OnChildRemoved(WidgetPtr child) {}
    virtual void OnStyleChanged() {}
    virtual void OnDataContextChanged() {}
    virtual void OnFocusChanged(bool focused) {}
    virtual void OnEnabledChanged(bool enabled) {}

    void SetHovered(bool hovered);
    void SetPressed(bool pressed);

    std::string m_id;
    std::string m_tagName;
    std::vector<std::string> m_classes;

    WidgetWeakPtr m_parent;
    std::vector<WidgetPtr> m_children;

    UIStyle m_style;
    glm::vec4 m_computedRect{0.0f};  // x, y, width, height

    std::string m_text;
    std::unordered_map<std::string, std::string> m_attributes;

    // Data binding
    void* m_dataContext = nullptr;
    const Reflect::TypeInfo* m_dataContextType = nullptr;
    std::vector<std::unique_ptr<DataBinding>> m_bindings;
    bool m_inheritDataContext = true;

    // Event handlers
    std::unordered_map<UIEvent::Type, std::vector<EventHandler>> m_eventHandlers;
    std::unordered_map<std::string, std::vector<EventHandler>> m_customEventHandlers;

    // State
    bool m_enabled = true;
    bool m_focused = false;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_dirty = true;
};

// =============================================================================
// Widget Factory for creating widgets from type names
// =============================================================================

class UIWidgetFactory {
public:
    using Creator = std::function<WidgetPtr()>;

    static UIWidgetFactory& Instance();

    void Register(const std::string& tagName, Creator creator);
    WidgetPtr Create(const std::string& tagName);
    bool IsRegistered(const std::string& tagName) const;
    std::vector<std::string> GetRegisteredTags() const;

private:
    UIWidgetFactory() = default;
    std::unordered_map<std::string, Creator> m_creators;
};

/**
 * @brief Helper macro for registering widget types
 */
#define REGISTER_UI_WIDGET(TagName, WidgetClass) \
    namespace { \
        struct WidgetClass##Registrar { \
            WidgetClass##Registrar() { \
                Nova::UI::UIWidgetFactory::Instance().Register(TagName, []() { \
                    return std::make_shared<WidgetClass>(); \
                }); \
            } \
        }; \
        static WidgetClass##Registrar s_##WidgetClass##Registrar; \
    }

} // namespace UI
} // namespace Nova
