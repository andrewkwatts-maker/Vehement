#include "UIWidget.hpp"
#include "UITemplate.hpp"
#include <algorithm>
#include <sstream>
#include <regex>

namespace Nova {
namespace UI {

// =============================================================================
// PropertyPath Implementation
// =============================================================================

PropertyPath::PropertyPath(const std::string& path) {
    *this = Parse(path);
}

std::string PropertyPath::ToString() const {
    std::string result;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result += ".";
        result += segments[i];
    }
    return result;
}

PropertyPath PropertyPath::Parse(const std::string& path) {
    PropertyPath result;
    std::stringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
        if (!segment.empty()) {
            result.segments.push_back(segment);
        }
    }
    return result;
}

// =============================================================================
// DataBinding Implementation
// =============================================================================

DataBinding::DataBinding(const std::string& sourcePath, const std::string& targetProperty,
                        BindingMode mode)
    : m_sourcePath(sourcePath)
    , m_targetProperty(targetProperty)
    , m_mode(mode) {
}

void DataBinding::SetConverter(std::function<std::any(const std::any&)> toTarget,
                               std::function<std::any(const std::any&)> toSource) {
    m_toTargetConverter = std::move(toTarget);
    m_toSourceConverter = std::move(toSource);
}

void DataBinding::SetFormatter(const std::string& format) {
    m_format = format;
}

void DataBinding::Bind(void* sourceObject, const Reflect::TypeInfo* sourceType, UIWidget* target) {
    Unbind();

    m_sourceObject = sourceObject;
    m_sourceType = sourceType;
    m_targetWidget = target;

    if (!m_sourceObject || !m_sourceType || !m_targetWidget) {
        return;
    }

    // Initial update
    UpdateTarget();

    // NOTE: Property change subscription is deferred until the reflection system is complete.
    // This would require the Observable pattern to be integrated into the reflection system,
    // allowing automatic updates when source properties change.
}

void DataBinding::Unbind() {
    m_connection.Disconnect();
    m_sourceObject = nullptr;
    m_sourceType = nullptr;
    m_targetWidget = nullptr;
}

void DataBinding::UpdateTarget() {
    if (!m_sourceObject || !m_sourceType || !m_targetWidget) {
        return;
    }

    // Navigate the property path
    PropertyPath path = PropertyPath::Parse(m_sourcePath);
    void* currentObject = m_sourceObject;
    const Reflect::TypeInfo* currentType = m_sourceType;

    for (size_t i = 0; i < path.segments.size(); ++i) {
        const auto* propInfo = currentType->FindProperty(path.segments[i]);
        if (!propInfo || !propInfo->getterAny) {
            return;
        }

        std::any value = propInfo->getterAny(currentObject);

        if (i == path.segments.size() - 1) {
            // Final property - apply to target
            if (m_toTargetConverter) {
                value = m_toTargetConverter(value);
            }

            // Apply to widget property
            if (m_targetProperty == "text") {
                // Convert to string if needed
                if (value.type() == typeid(std::string)) {
                    m_targetWidget->SetText(std::any_cast<std::string>(value));
                } else if (value.type() == typeid(int)) {
                    m_targetWidget->SetText(std::to_string(std::any_cast<int>(value)));
                } else if (value.type() == typeid(float)) {
                    m_targetWidget->SetText(std::to_string(std::any_cast<float>(value)));
                } else if (value.type() == typeid(double)) {
                    m_targetWidget->SetText(std::to_string(std::any_cast<double>(value)));
                }
            } else if (m_targetProperty == "visible") {
                if (value.type() == typeid(bool)) {
                    m_targetWidget->GetStyle().visible = std::any_cast<bool>(value);
                }
            } else if (m_targetProperty == "enabled") {
                if (value.type() == typeid(bool)) {
                    m_targetWidget->SetEnabled(std::any_cast<bool>(value));
                }
            }

            m_targetWidget->MarkDirty();
        } else {
            // Navigate to nested object for next path segment
            // Note: Nested object navigation requires TypeInfo lookup by typeName
            // For now, only single-level property binding is supported
            return; // Nested navigation not yet implemented
        }
    }
}

void DataBinding::UpdateSource() {
    if (m_mode == BindingMode::OneWay || m_mode == BindingMode::OneTime) {
        return;
    }

    if (!m_sourceObject || !m_sourceType || !m_targetWidget) {
        return;
    }

    // Get the current value from the widget
    std::any widgetValue;
    if (m_targetProperty == "text") {
        widgetValue = m_targetWidget->GetText();
    } else if (m_targetProperty == "visible") {
        widgetValue = m_targetWidget->GetStyle().visible;
    } else if (m_targetProperty == "enabled") {
        widgetValue = m_targetWidget->IsEnabled();
    } else {
        return; // Unsupported target property
    }

    // Apply converter if present
    if (m_toSourceConverter) {
        widgetValue = m_toSourceConverter(widgetValue);
    }

    // Navigate the property path to find the final property
    PropertyPath path = PropertyPath::Parse(m_sourcePath);
    void* currentObject = m_sourceObject;
    const Reflect::TypeInfo* currentType = m_sourceType;

    for (size_t i = 0; i < path.segments.size(); ++i) {
        const auto* propInfo = currentType->FindProperty(path.segments[i]);
        if (!propInfo) {
            return;
        }

        if (i == path.segments.size() - 1) {
            // Final property - set the value
            if (propInfo->setterAny) {
                propInfo->setterAny(currentObject, widgetValue);
            }
        } else {
            // Navigate to nested object
            // Note: Nested object navigation not yet implemented
            return;
        }
    }
}

// =============================================================================
// UIWidget Implementation
// =============================================================================

UIWidget::UIWidget(const std::string& tagName)
    : m_tagName(tagName) {
}

UIWidget::~UIWidget() {
    ClearBindings();
    ClearChildren();
}

// Class management
void UIWidget::AddClass(const std::string& className) {
    if (!HasClass(className)) {
        m_classes.push_back(className);
        MarkDirty();
    }
}

void UIWidget::RemoveClass(const std::string& className) {
    auto it = std::find(m_classes.begin(), m_classes.end(), className);
    if (it != m_classes.end()) {
        m_classes.erase(it);
        MarkDirty();
    }
}

bool UIWidget::HasClass(const std::string& className) const {
    return std::find(m_classes.begin(), m_classes.end(), className) != m_classes.end();
}

void UIWidget::ToggleClass(const std::string& className) {
    if (HasClass(className)) {
        RemoveClass(className);
    } else {
        AddClass(className);
    }
}

void UIWidget::SetClass(const std::string& className) {
    m_classes.clear();
    if (!className.empty()) {
        // Split by spaces
        std::stringstream ss(className);
        std::string cls;
        while (ss >> cls) {
            m_classes.push_back(cls);
        }
    }
    MarkDirty();
}

// Hierarchy
void UIWidget::AppendChild(WidgetPtr child) {
    if (!child) return;

    // Remove from previous parent
    if (auto oldParent = child->GetParent()) {
        oldParent->RemoveChild(child);
    }

    child->m_parent = shared_from_this();
    m_children.push_back(child);

    // Inherit data context if enabled
    if (child->m_inheritDataContext && m_dataContext) {
        child->SetDataContext(m_dataContext, m_dataContextType);
    }

    OnChildAdded(child);
    MarkDirty();
}

void UIWidget::InsertChild(WidgetPtr child, size_t index) {
    if (!child) return;

    if (auto oldParent = child->GetParent()) {
        oldParent->RemoveChild(child);
    }

    child->m_parent = shared_from_this();

    if (index >= m_children.size()) {
        m_children.push_back(child);
    } else {
        m_children.insert(m_children.begin() + index, child);
    }

    if (child->m_inheritDataContext && m_dataContext) {
        child->SetDataContext(m_dataContext, m_dataContextType);
    }

    OnChildAdded(child);
    MarkDirty();
}

void UIWidget::RemoveChild(WidgetPtr child) {
    if (!child) return;

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        (*it)->m_parent.reset();
        OnChildRemoved(*it);
        m_children.erase(it);
        MarkDirty();
    }
}

void UIWidget::RemoveChildAt(size_t index) {
    if (index < m_children.size()) {
        auto child = m_children[index];
        child->m_parent.reset();
        OnChildRemoved(child);
        m_children.erase(m_children.begin() + index);
        MarkDirty();
    }
}

void UIWidget::ClearChildren() {
    for (auto& child : m_children) {
        child->m_parent.reset();
        OnChildRemoved(child);
    }
    m_children.clear();
    MarkDirty();
}

// Query methods
WidgetPtr UIWidget::FindById(const std::string& id) {
    if (m_id == id) {
        return shared_from_this();
    }
    for (auto& child : m_children) {
        if (auto found = child->FindById(id)) {
            return found;
        }
    }
    return nullptr;
}

std::vector<WidgetPtr> UIWidget::FindByClass(const std::string& className) {
    std::vector<WidgetPtr> result;
    if (HasClass(className)) {
        result.push_back(shared_from_this());
    }
    for (auto& child : m_children) {
        auto childResults = child->FindByClass(className);
        result.insert(result.end(), childResults.begin(), childResults.end());
    }
    return result;
}

std::vector<WidgetPtr> UIWidget::FindByTagName(const std::string& tagName) {
    std::vector<WidgetPtr> result;
    if (m_tagName == tagName) {
        result.push_back(shared_from_this());
    }
    for (auto& child : m_children) {
        auto childResults = child->FindByTagName(tagName);
        result.insert(result.end(), childResults.begin(), childResults.end());
    }
    return result;
}

WidgetPtr UIWidget::QuerySelector(const std::string& selector) {
    auto results = QuerySelectorAll(selector);
    return results.empty() ? nullptr : results[0];
}

std::vector<WidgetPtr> UIWidget::QuerySelectorAll(const std::string& selector) {
    std::vector<WidgetPtr> result;

    // Simple selector parsing (id, class, tag)
    if (selector.empty()) return result;

    if (selector[0] == '#') {
        // ID selector
        std::string id = selector.substr(1);
        if (auto found = FindById(id)) {
            result.push_back(found);
        }
    } else if (selector[0] == '.') {
        // Class selector
        result = FindByClass(selector.substr(1));
    } else {
        // Tag selector
        result = FindByTagName(selector);
    }

    return result;
}

// Style
void UIWidget::SetStyleProperty(const std::string& property, const std::any& value) {
    if (property == "width" && value.type() == typeid(Length)) {
        m_style.width = std::any_cast<Length>(value);
    } else if (property == "height" && value.type() == typeid(Length)) {
        m_style.height = std::any_cast<Length>(value);
    } else if (property == "backgroundColor" && value.type() == typeid(glm::vec4)) {
        m_style.backgroundColor = std::any_cast<glm::vec4>(value);
    } else if (property == "color" && value.type() == typeid(glm::vec4)) {
        m_style.color = std::any_cast<glm::vec4>(value);
    } else if (property == "visible" && value.type() == typeid(bool)) {
        m_style.visible = std::any_cast<bool>(value);
    } else if (property == "display" && value.type() == typeid(Display)) {
        m_style.display = std::any_cast<Display>(value);
    } else if (property == "gap" && value.type() == typeid(float)) {
        m_style.gap = std::any_cast<float>(value);
    } else if (property == "fontSize" && value.type() == typeid(float)) {
        m_style.fontSize = std::any_cast<float>(value);
    } else if (property == "flexDirection" && value.type() == typeid(LayoutDirection)) {
        m_style.flexDirection = std::any_cast<LayoutDirection>(value);
    }
    MarkDirty();
}

std::any UIWidget::GetStyleProperty(const std::string& property) const {
    if (property == "width") {
        return m_style.width;
    } else if (property == "height") {
        return m_style.height;
    } else if (property == "backgroundColor") {
        return m_style.backgroundColor;
    } else if (property == "color") {
        return m_style.color;
    } else if (property == "visible") {
        return m_style.visible;
    } else if (property == "display") {
        return m_style.display;
    } else if (property == "gap") {
        return m_style.gap;
    } else if (property == "fontSize") {
        return m_style.fontSize;
    } else if (property == "flexDirection") {
        return m_style.flexDirection;
    }
    return {};
}

// Data binding
void UIWidget::SetDataContext(void* dataContext, const Reflect::TypeInfo* typeInfo) {
    m_dataContext = dataContext;
    m_dataContextType = typeInfo;

    // Update all bindings
    for (auto& binding : m_bindings) {
        binding->Bind(m_dataContext, m_dataContextType, this);
    }

    // Propagate to children that inherit context
    for (auto& child : m_children) {
        if (child->m_inheritDataContext) {
            child->SetDataContext(dataContext, typeInfo);
        }
    }

    OnDataContextChanged();
}

void UIWidget::AddBinding(std::unique_ptr<DataBinding> binding) {
    if (m_dataContext && m_dataContextType) {
        binding->Bind(m_dataContext, m_dataContextType, this);
    }
    m_bindings.push_back(std::move(binding));
}

void UIWidget::RemoveBinding(const std::string& targetProperty) {
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
            [&](const auto& b) { return b->GetTargetProperty() == targetProperty; }),
        m_bindings.end()
    );
}

void UIWidget::ClearBindings() {
    for (auto& binding : m_bindings) {
        binding->Unbind();
    }
    m_bindings.clear();
}

void UIWidget::UpdateBindings() {
    for (auto& binding : m_bindings) {
        binding->UpdateTarget();
    }
}

void UIWidget::BindProperty(const std::string& widgetProperty, const std::string& dataPath,
                           BindingMode mode) {
    AddBinding(std::make_unique<DataBinding>(dataPath, widgetProperty, mode));
}

void UIWidget::BindText(const std::string& dataPath, const std::string& format) {
    auto binding = std::make_unique<DataBinding>(dataPath, "text", BindingMode::OneWay);
    if (!format.empty()) {
        binding->SetFormatter(format);
    }
    AddBinding(std::move(binding));
}

void UIWidget::BindVisible(const std::string& dataPath, bool invert) {
    auto binding = std::make_unique<DataBinding>(dataPath, "visible", BindingMode::OneWay);
    if (invert) {
        binding->SetConverter([](const std::any& v) {
            if (v.type() == typeid(bool)) {
                return std::any(!std::any_cast<bool>(v));
            }
            return v;
        });
    }
    AddBinding(std::move(binding));
}

void UIWidget::BindEnabled(const std::string& dataPath, bool invert) {
    auto binding = std::make_unique<DataBinding>(dataPath, "enabled", BindingMode::OneWay);
    if (invert) {
        binding->SetConverter([](const std::any& v) {
            if (v.type() == typeid(bool)) {
                return std::any(!std::any_cast<bool>(v));
            }
            return v;
        });
    }
    AddBinding(std::move(binding));
}

// Content
void UIWidget::SetText(const std::string& text) {
    if (m_text != text) {
        m_text = text;
        MarkDirty();
    }
}

void UIWidget::SetInnerHTML(const std::string& html) {
    ClearChildren();
    auto parsed = UIParser::ParseHTML("<root>" + html + "</root>");
    if (parsed) {
        for (auto& child : parsed->GetChildren()) {
            AppendChild(child);
        }
    }
}

std::string UIWidget::GetInnerHTML() const {
    std::string html;
    for (const auto& child : m_children) {
        // Opening tag
        html += "<" + child->GetTagName();

        // ID attribute
        if (!child->GetId().empty()) {
            html += " id=\"" + child->GetId() + "\"";
        }

        // Class attribute
        if (!child->GetClasses().empty()) {
            html += " class=\"";
            for (size_t i = 0; i < child->GetClasses().size(); ++i) {
                if (i > 0) html += " ";
                html += child->GetClasses()[i];
            }
            html += "\"";
        }

        html += ">";

        // Text content
        if (!child->GetText().empty()) {
            html += child->GetText();
        }

        // Recursively serialize children
        html += child->GetInnerHTML();

        // Closing tag
        html += "</" + child->GetTagName() + ">";
    }
    return html;
}

// Events
void UIWidget::AddEventListener(UIEvent::Type type, EventHandler handler) {
    m_eventHandlers[type].push_back(std::move(handler));
}

void UIWidget::AddEventListener(const std::string& customType, EventHandler handler) {
    m_customEventHandlers[customType].push_back(std::move(handler));
}

void UIWidget::RemoveEventListeners(UIEvent::Type type) {
    m_eventHandlers.erase(type);
}

void UIWidget::DispatchEvent(UIEvent& event) {
    event.currentTarget = this;

    // Handle standard events
    auto it = m_eventHandlers.find(event.type);
    if (it != m_eventHandlers.end()) {
        for (auto& handler : it->second) {
            handler(event);
            if (event.propagationStopped) return;
        }
    }

    // Handle custom events
    if (event.type == UIEvent::Type::Custom) {
        auto customIt = m_customEventHandlers.find(event.customType);
        if (customIt != m_customEventHandlers.end()) {
            for (auto& handler : customIt->second) {
                handler(event);
                if (event.propagationStopped) return;
            }
        }
    }

    // Bubble to parent
    if (!event.propagationStopped && !m_parent.expired()) {
        m_parent.lock()->DispatchEvent(event);
    }
}

// State
void UIWidget::SetEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        OnEnabledChanged(enabled);
        MarkDirty();
    }
}

void UIWidget::Focus() {
    if (!m_focused && m_enabled) {
        m_focused = true;
        OnFocusChanged(true);

        UIEvent event;
        event.type = UIEvent::Type::Focus;
        event.target = this;
        DispatchEvent(event);
    }
}

void UIWidget::Blur() {
    if (m_focused) {
        m_focused = false;
        OnFocusChanged(false);

        UIEvent event;
        event.type = UIEvent::Type::Blur;
        event.target = this;
        DispatchEvent(event);
    }
}

void UIWidget::SetHovered(bool hovered) {
    if (m_hovered != hovered) {
        m_hovered = hovered;

        UIEvent event;
        event.type = hovered ? UIEvent::Type::MouseEnter : UIEvent::Type::MouseLeave;
        event.target = this;
        DispatchEvent(event);

        MarkDirty();
    }
}

void UIWidget::SetPressed(bool pressed) {
    if (m_pressed != pressed) {
        m_pressed = pressed;
        MarkDirty();
    }
}

// Attributes
void UIWidget::SetAttribute(const std::string& name, const std::string& value) {
    m_attributes[name] = value;

    // Handle special attributes
    if (name == "id") {
        m_id = value;
    } else if (name == "class") {
        SetClass(value);
    }
}

std::string UIWidget::GetAttribute(const std::string& name) const {
    auto it = m_attributes.find(name);
    return it != m_attributes.end() ? it->second : "";
}

bool UIWidget::HasAttribute(const std::string& name) const {
    return m_attributes.find(name) != m_attributes.end();
}

void UIWidget::RemoveAttribute(const std::string& name) {
    m_attributes.erase(name);
}

// Lifecycle
void UIWidget::Update(float deltaTime) {
    UpdateBindings();

    for (auto& child : m_children) {
        if (child->GetStyle().visible) {
            child->Update(deltaTime);
        }
    }
}

void UIWidget::Render(UIContext& context) {
    if (!m_style.visible) return;

    // Render children
    for (auto& child : m_children) {
        child->Render(context);
    }
}

void UIWidget::Layout(const glm::vec4& parentRect) {
    // Simple flexbox-like layout
    float x = parentRect.x + m_style.margin.left.Resolve(parentRect.z);
    float y = parentRect.y + m_style.margin.top.Resolve(parentRect.w);

    float width = m_style.width.IsAuto() ?
        parentRect.z - m_style.margin.left.Resolve(parentRect.z) - m_style.margin.right.Resolve(parentRect.z) :
        m_style.width.Resolve(parentRect.z);

    float height = m_style.height.IsAuto() ?
        parentRect.w - m_style.margin.top.Resolve(parentRect.w) - m_style.margin.bottom.Resolve(parentRect.w) :
        m_style.height.Resolve(parentRect.w);

    m_computedRect = glm::vec4(x, y, width, height);

    // Layout children
    float contentX = x + m_style.padding.left.Resolve(width);
    float contentY = y + m_style.padding.top.Resolve(height);
    float contentWidth = width - m_style.padding.left.Resolve(width) - m_style.padding.right.Resolve(width);
    float contentHeight = height - m_style.padding.top.Resolve(height) - m_style.padding.bottom.Resolve(height);

    float currentX = contentX;
    float currentY = contentY;
    float lineHeight = 0.0f;

    for (auto& child : m_children) {
        if (!child->GetStyle().visible) continue;

        child->Layout(glm::vec4(currentX, currentY, contentWidth, contentHeight));

        if (m_style.flexDirection == LayoutDirection::Row) {
            currentX += child->GetComputedSize().x + m_style.gap;
            lineHeight = std::max(lineHeight, child->GetComputedSize().y);
        } else {
            currentY += child->GetComputedSize().y + m_style.gap;
        }
    }

    m_dirty = false;
}

void UIWidget::MarkDirty() {
    m_dirty = true;
    if (auto parent = m_parent.lock()) {
        parent->MarkDirty();
    }
}

// =============================================================================
// UIWidgetFactory Implementation
// =============================================================================

UIWidgetFactory& UIWidgetFactory::Instance() {
    static UIWidgetFactory instance;
    return instance;
}

void UIWidgetFactory::Register(const std::string& tagName, Creator creator) {
    m_creators[tagName] = std::move(creator);
}

WidgetPtr UIWidgetFactory::Create(const std::string& tagName) {
    auto it = m_creators.find(tagName);
    if (it != m_creators.end()) {
        return it->second();
    }
    // Default to generic widget
    return std::make_shared<UIWidget>(tagName);
}

bool UIWidgetFactory::IsRegistered(const std::string& tagName) const {
    return m_creators.find(tagName) != m_creators.end();
}

std::vector<std::string> UIWidgetFactory::GetRegisteredTags() const {
    std::vector<std::string> tags;
    tags.reserve(m_creators.size());
    for (const auto& [tag, _] : m_creators) {
        tags.push_back(tag);
    }
    return tags;
}

} // namespace UI
} // namespace Nova
