#include "UITemplateEngine.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <cctype>

namespace Nova {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// TemplateParser Implementation
// ============================================================================

std::shared_ptr<UITemplate> TemplateParser::Parse(const std::string& templateStr) {
    auto templ = std::make_shared<UITemplate>();
    size_t pos = 0;
    SkipWhitespace(templateStr, pos);

    if (pos < templateStr.length()) {
        templ->SetRoot(ParseElement(templateStr, pos));
    }

    return templ;
}

std::shared_ptr<UITemplate> TemplateParser::ParseFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return nullptr;

    std::stringstream buffer;
    buffer << file.rdbuf();

    auto templ = Parse(buffer.str());
    templ->SetName(fs::path(path).stem().string());
    return templ;
}

std::shared_ptr<UITemplate> TemplateParser::ParseJson(const std::string& jsonStr) {
    auto templ = std::make_shared<UITemplate>();

    try {
        json j = json::parse(jsonStr);

        if (j.contains("name")) {
            templ->SetName(j["name"]);
        }

        std::function<std::shared_ptr<TemplateNode>(const json&)> parseNode;
        parseNode = [&parseNode](const json& nodeJson) -> std::shared_ptr<TemplateNode> {
            auto node = std::make_shared<TemplateNode>();

            if (nodeJson.contains("type")) {
                std::string type = nodeJson["type"];
                if (type == "text") node->type = TemplateNodeType::Text;
                else if (type == "binding") node->type = TemplateNodeType::Binding;
                else if (type == "condition") node->type = TemplateNodeType::Condition;
                else if (type == "loop") node->type = TemplateNodeType::Loop;
                else if (type == "slot") node->type = TemplateNodeType::Slot;
                else if (type == "include") node->type = TemplateNodeType::Include;
                else node->type = TemplateNodeType::Element;
            }

            if (nodeJson.contains("tag")) node->tagName = nodeJson["tag"];
            if (nodeJson.contains("text")) node->textContent = nodeJson["text"];
            if (nodeJson.contains("v-if")) node->vIf = nodeJson["v-if"];
            if (nodeJson.contains("v-for")) node->vFor = nodeJson["v-for"];
            if (nodeJson.contains("v-model")) node->vModel = nodeJson["v-model"];
            if (nodeJson.contains("v-slot")) node->vSlot = nodeJson["v-slot"];
            if (nodeJson.contains("v-include")) node->vInclude = nodeJson["v-include"];

            if (nodeJson.contains("attributes") && nodeJson["attributes"].is_object()) {
                for (auto& [key, value] : nodeJson["attributes"].items()) {
                    TemplateAttribute attr;
                    attr.name = key;
                    attr.value = value.is_string() ? value.get<std::string>() : value.dump();
                    attr.isBound = key.length() > 0 && key[0] == ':';
                    attr.isEvent = key.length() > 0 && key[0] == '@';
                    node->attributes.push_back(attr);
                }
            }

            if (nodeJson.contains("children") && nodeJson["children"].is_array()) {
                for (const auto& childJson : nodeJson["children"]) {
                    node->children.push_back(parseNode(childJson));
                }
            }

            return node;
        };

        if (j.contains("template")) {
            templ->SetRoot(parseNode(j["template"]));
        }

        if (j.contains("styles")) {
            templ->SetStyles(j["styles"]);
        }

    } catch (...) {
        return nullptr;
    }

    return templ;
}

void TemplateParser::SkipWhitespace(const std::string& str, size_t& pos) {
    while (pos < str.length() && std::isspace(static_cast<unsigned char>(str[pos]))) {
        pos++;
    }
}

bool TemplateParser::IsVoidElement(const std::string& tagName) {
    static const std::vector<std::string> voidElements = {
        "input", "img", "br", "hr", "meta", "link", "slider", "slider-int",
        "color", "checkbox", "progress", "image"
    };
    return std::find(voidElements.begin(), voidElements.end(), tagName) != voidElements.end();
}

std::shared_ptr<TemplateNode> TemplateParser::ParseElement(const std::string& html, size_t& pos) {
    SkipWhitespace(html, pos);

    // Check for text node or binding
    if (pos >= html.length() || html[pos] != '<') {
        auto textNode = std::make_shared<TemplateNode>();

        // Find end of text (next < or end)
        size_t start = pos;
        while (pos < html.length() && html[pos] != '<') {
            pos++;
        }

        std::string text = html.substr(start, pos - start);

        // Check for bindings {{}}
        if (text.find("{{") != std::string::npos) {
            textNode->type = TemplateNodeType::Binding;
        } else {
            textNode->type = TemplateNodeType::Text;
        }
        textNode->textContent = text;
        return textNode;
    }

    auto node = std::make_shared<TemplateNode>();
    node->type = TemplateNodeType::Element;

    // Skip <
    pos++;
    SkipWhitespace(html, pos);

    // Check for closing tag
    if (html[pos] == '/') {
        return nullptr;  // Closing tag, handled by parent
    }

    // Check for comment
    if (pos + 2 < html.length() && html.substr(pos, 3) == "!--") {
        // Skip comment
        size_t endComment = html.find("-->", pos);
        if (endComment != std::string::npos) {
            pos = endComment + 3;
        }
        return ParseElement(html, pos);
    }

    // Parse tag name
    node->tagName = ParseTagName(html, pos);
    SkipWhitespace(html, pos);

    // Parse attributes
    std::string attrStr;
    while (pos < html.length() && html[pos] != '>' && html[pos] != '/') {
        attrStr += html[pos];
        pos++;
    }
    node->attributes = ParseAttributes(attrStr);

    // Extract directives from attributes
    for (const auto& attr : node->attributes) {
        if (attr.name == "v-if") node->vIf = attr.value;
        else if (attr.name == "v-for") {
            node->vFor = attr.value;
            // Parse "item in items"
            std::regex forRegex(R"((\w+)\s+in\s+(\w+))");
            std::smatch match;
            if (std::regex_match(attr.value, match, forRegex)) {
                node->loopVariable = match[1].str();
                node->loopSource = match[2].str();
                node->type = TemplateNodeType::Loop;
            }
        }
        else if (attr.name == "v-model") node->vModel = attr.value;
        else if (attr.name == "v-slot" || attr.name.substr(0, 1) == "#") {
            node->vSlot = attr.name[0] == '#' ? attr.name.substr(1) : attr.value;
        }
    }

    // Check for self-closing or void element
    bool selfClosing = false;
    if (pos < html.length() && html[pos] == '/') {
        selfClosing = true;
        pos++;
    }
    if (pos < html.length() && html[pos] == '>') {
        pos++;
    }

    // Parse children if not self-closing
    if (!selfClosing && !IsVoidElement(node->tagName)) {
        while (pos < html.length()) {
            SkipWhitespace(html, pos);

            // Check for closing tag
            if (pos + 1 < html.length() && html[pos] == '<' && html[pos + 1] == '/') {
                // Find end of closing tag
                size_t endPos = html.find('>', pos);
                if (endPos != std::string::npos) {
                    pos = endPos + 1;
                }
                break;
            }

            auto child = ParseElement(html, pos);
            if (child) {
                node->children.push_back(child);
            }
        }
    }

    return node;
}

std::string TemplateParser::ParseTagName(const std::string& html, size_t& pos) {
    std::string tagName;
    while (pos < html.length() &&
           (std::isalnum(static_cast<unsigned char>(html[pos])) || html[pos] == '-' || html[pos] == '_')) {
        tagName += html[pos];
        pos++;
    }
    return tagName;
}

std::vector<TemplateAttribute> TemplateParser::ParseAttributes(const std::string& attrStr) {
    std::vector<TemplateAttribute> attributes;

    // Parse attributes manually for better MSVC compatibility
    size_t pos = 0;
    while (pos < attrStr.length()) {
        // Skip whitespace
        while (pos < attrStr.length() && std::isspace(static_cast<unsigned char>(attrStr[pos]))) pos++;
        if (pos >= attrStr.length()) break;

        TemplateAttribute attr;

        // Parse attribute name (can start with : @ or #)
        size_t nameStart = pos;
        while (pos < attrStr.length() &&
               (std::isalnum(static_cast<unsigned char>(attrStr[pos])) ||
                attrStr[pos] == '-' || attrStr[pos] == '_' ||
                attrStr[pos] == ':' || attrStr[pos] == '@' || attrStr[pos] == '#')) {
            pos++;
        }
        attr.name = attrStr.substr(nameStart, pos - nameStart);
        if (attr.name.empty()) break;

        // Skip whitespace
        while (pos < attrStr.length() && std::isspace(static_cast<unsigned char>(attrStr[pos]))) pos++;

        // Check for =
        if (pos < attrStr.length() && attrStr[pos] == '=') {
            pos++;
            while (pos < attrStr.length() && std::isspace(static_cast<unsigned char>(attrStr[pos]))) pos++;

            // Parse value
            if (pos < attrStr.length()) {
                char quote = attrStr[pos];
                if (quote == '"' || quote == '\'') {
                    pos++;
                    size_t valueStart = pos;
                    while (pos < attrStr.length() && attrStr[pos] != quote) pos++;
                    attr.value = attrStr.substr(valueStart, pos - valueStart);
                    if (pos < attrStr.length()) pos++; // Skip closing quote
                } else {
                    // Unquoted value
                    size_t valueStart = pos;
                    while (pos < attrStr.length() && !std::isspace(static_cast<unsigned char>(attrStr[pos]))) pos++;
                    attr.value = attrStr.substr(valueStart, pos - valueStart);
                }
            }
        }

        // Check for binding prefix
        if (!attr.name.empty()) {
            if (attr.name[0] == ':') {
                attr.isBound = true;
                attr.name = attr.name.substr(1);
                attr.bindingExpression = attr.value;
            } else if (attr.name[0] == '@') {
                attr.isEvent = true;
                attr.name = attr.name.substr(1);
            }
        }

        attributes.push_back(attr);
    }

    return attributes;
}

// ============================================================================
// ExpressionEvaluator Implementation
// ============================================================================

BindingValue ExpressionEvaluator::Evaluate(const std::string& expression, const DataContext& context) {
    std::string trimmed = expression;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    // Simple path evaluation (e.g., "user.name" or "items[0]")
    auto path = SplitPath(trimmed);
    return GetNestedValue(context, path);
}

bool ExpressionEvaluator::EvaluateCondition(const std::string& expression, const DataContext& context) {
    auto value = Evaluate(expression, context);

    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) return arg;
        else if constexpr (std::is_same_v<T, int>) return arg != 0;
        else if constexpr (std::is_same_v<T, float>) return arg != 0.0f;
        else if constexpr (std::is_same_v<T, std::string>) return !arg.empty();
        else return true;
    }, value);
}

std::string ExpressionEvaluator::FormatString(const std::string& format, const DataContext& context) {
    std::string result = format;
    std::regex bindingRegex(R"(\{\{([^}]+)\}\})");

    std::string output;
    size_t lastPos = 0;
    std::sregex_iterator it(format.begin(), format.end(), bindingRegex);
    std::sregex_iterator end;

    while (it != end) {
        output += format.substr(lastPos, it->position() - lastPos);
        std::string expr = (*it)[1].str();
        output += EvaluateToString(Evaluate(expr, context));
        lastPos = it->position() + it->length();
        ++it;
    }
    output += format.substr(lastPos);

    return output;
}

std::string ExpressionEvaluator::EvaluateToString(const BindingValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) return arg ? "true" : "false";
        else if constexpr (std::is_same_v<T, int>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, float>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, std::string>) return arg;
        else if constexpr (std::is_same_v<T, glm::vec2>)
            return std::to_string(arg.x) + "," + std::to_string(arg.y);
        else if constexpr (std::is_same_v<T, glm::vec3>)
            return std::to_string(arg.x) + "," + std::to_string(arg.y) + "," + std::to_string(arg.z);
        else if constexpr (std::is_same_v<T, glm::vec4>)
            return std::to_string(arg.r) + "," + std::to_string(arg.g) + "," +
                   std::to_string(arg.b) + "," + std::to_string(arg.a);
        else return "";
    }, value);
}

std::vector<std::string> ExpressionEvaluator::SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;

    for (char c : path) {
        if (c == '.' || c == '[') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else if (c == ']') {
            // Index complete
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        parts.push_back(current);
    }

    return parts;
}

BindingValue ExpressionEvaluator::GetNestedValue(const DataContext& context, const std::vector<std::string>& path) {
    if (path.empty()) return BindingValue{};

    // First part is direct lookup
    if (path.size() == 1) {
        return context.GetValue(path[0]);
    }

    // Try to get child context
    auto child = context.GetChild(path[0]);
    if (child) {
        std::vector<std::string> remainingPath(path.begin() + 1, path.end());
        return GetNestedValue(*child, remainingPath);
    }

    // Try array index
    if (std::all_of(path[1].begin(), path[1].end(), ::isdigit)) {
        int index = std::stoi(path[1]);
        const auto& items = context.GetArrayItems();
        if (index >= 0 && static_cast<size_t>(index) < items.size()) {
            std::vector<std::string> remainingPath(path.begin() + 2, path.end());
            if (remainingPath.empty()) {
                // Return entire item context - not directly supported, return empty
                return BindingValue{};
            }
            return GetNestedValue(*items[index], remainingPath);
        }
    }

    return context.GetValue(path[0]);
}

// ============================================================================
// TemplateRenderer Implementation
// ============================================================================

std::unordered_map<std::string,
    std::function<UIComponent::Ptr(const std::vector<TemplateAttribute>&, std::shared_ptr<DataContext>)>>&
TemplateRenderer::GetCustomComponents() {
    static std::unordered_map<std::string,
        std::function<UIComponent::Ptr(const std::vector<TemplateAttribute>&, std::shared_ptr<DataContext>)>> components;
    return components;
}

void TemplateRenderer::RegisterComponent(const std::string& tagName,
    std::function<UIComponent::Ptr(const std::vector<TemplateAttribute>&, std::shared_ptr<DataContext>)> factory) {
    GetCustomComponents()[tagName] = factory;
}

UIComponent::Ptr TemplateRenderer::Render(std::shared_ptr<UITemplate> templ, std::shared_ptr<DataContext> context) {
    if (!templ || !templ->GetRoot()) return nullptr;
    return RenderNode(templ->GetRoot(), context);
}

UIComponent::Ptr TemplateRenderer::RenderString(const std::string& templateStr, std::shared_ptr<DataContext> context) {
    auto templ = TemplateParser::Parse(templateStr);
    return Render(templ, context);
}

UIComponent::Ptr TemplateRenderer::RenderFile(const std::string& path, std::shared_ptr<DataContext> context) {
    auto templ = TemplateParser::ParseFile(path);
    return Render(templ, context);
}

UIComponent::Ptr TemplateRenderer::RenderNode(std::shared_ptr<TemplateNode> node, std::shared_ptr<DataContext> context) {
    if (!node || !context) return nullptr;

    // Check v-if condition
    if (!node->vIf.empty()) {
        if (!ExpressionEvaluator::EvaluateCondition(node->vIf, *context)) {
            return nullptr;
        }
    }

    // Handle text/binding nodes
    if (node->type == TemplateNodeType::Text || node->type == TemplateNodeType::Binding) {
        auto label = std::make_shared<UILabel>();
        std::string text = ExpressionEvaluator::FormatString(node->textContent, *context);
        label->SetText(text);
        return label;
    }

    // Handle loop
    if (node->type == TemplateNodeType::Loop && !node->loopSource.empty()) {
        auto container = std::make_shared<UIVerticalLayout>();

        auto child = context->GetChild(node->loopSource);
        if (child) {
            const auto& items = child->GetArrayItems();
            for (size_t i = 0; i < items.size(); ++i) {
                // Create child context with loop variable
                auto itemContext = std::make_shared<DataContext>(*context);
                itemContext->AddChild(node->loopVariable, items[i]);

                for (const auto& childNode : node->children) {
                    auto rendered = RenderNode(childNode, itemContext);
                    if (rendered) {
                        container->AddChild(rendered);
                    }
                }
            }
        }

        return container;
    }

    // Create component based on tag name
    auto component = CreateComponent(node->tagName, node->attributes, context);
    if (!component) return nullptr;

    // Apply attributes
    ApplyAttributes(component, node->attributes, context);

    // Render children
    if (auto container = std::dynamic_pointer_cast<UIContainer>(component)) {
        for (const auto& childNode : node->children) {
            auto childComponent = RenderNode(childNode, context);
            if (childComponent) {
                container->AddChild(childComponent);
            }
        }
    }

    return component;
}

UIComponent::Ptr TemplateRenderer::CreateComponent(const std::string& tagName,
    const std::vector<TemplateAttribute>& attributes, std::shared_ptr<DataContext> context) {

    // Check custom components first
    auto& customComponents = GetCustomComponents();
    auto customIt = customComponents.find(tagName);
    if (customIt != customComponents.end()) {
        return customIt->second(attributes, context);
    }

    // Built-in tag mappings
    static const std::unordered_map<std::string, std::function<UIComponent::Ptr()>> tagMap = {
        // Containers
        {"div", []() { return std::make_shared<UIContainer>(); }},
        {"panel", []() { return std::make_shared<UIPanel>(); }},
        {"row", []() { return std::make_shared<UIHorizontalLayout>(); }},
        {"column", []() { return std::make_shared<UIVerticalLayout>(); }},
        {"grid", []() { return std::make_shared<UIGridLayout>(); }},
        {"scroll", []() { return std::make_shared<UIScrollView>(); }},
        {"tabs", []() { return std::make_shared<UITabContainer>(); }},

        // Basic inputs
        {"label", []() { return std::make_shared<UILabel>(); }},
        {"span", []() { return std::make_shared<UILabel>(); }},
        {"p", []() { return std::make_shared<UILabel>(); }},
        {"button", []() { return std::make_shared<UIButton>(); }},
        {"checkbox", []() { return std::make_shared<UICheckbox>(); }},
        {"input", []() { return std::make_shared<UITextInput>(); }},
        {"slider", []() { return std::make_shared<UISlider>(); }},
        {"slider-int", []() { return std::make_shared<UISliderInt>(); }},
        {"color", []() { return std::make_shared<UIColorPicker>(); }},
        {"select", []() { return std::make_shared<UIDropdown>(); }},
        {"vec3", []() { return std::make_shared<UIVector3Input>(); }},

        // Complex
        {"tree", []() { return std::make_shared<UITreeView>(); }},
        {"list", []() { return std::make_shared<UIListView>(); }},
        {"properties", []() { return std::make_shared<UIPropertyGrid>(); }},
        {"image", []() { return std::make_shared<UIImage>(); }},
        {"progress", []() { return std::make_shared<UIProgressBar>(); }},
    };

    auto it = tagMap.find(tagName);
    if (it != tagMap.end()) {
        return it->second();
    }

    // Default to container
    return std::make_shared<UIContainer>();
}

void TemplateRenderer::ApplyAttributes(UIComponent::Ptr component,
    const std::vector<TemplateAttribute>& attributes, std::shared_ptr<DataContext> context) {

    for (const auto& attr : attributes) {
        if (attr.name == "id") {
            component->SetId(attr.value);
        }
        else if (attr.name == "tooltip") {
            component->SetTooltip(attr.value);
        }
        else if (attr.name == "visible") {
            bool visible = attr.isBound
                ? ExpressionEvaluator::EvaluateCondition(attr.value, *context)
                : (attr.value != "false");
            component->SetVisible(visible);
        }
        else if (attr.name == "enabled") {
            bool enabled = attr.isBound
                ? ExpressionEvaluator::EvaluateCondition(attr.value, *context)
                : (attr.value != "false");
            component->SetEnabled(enabled);
        }
        else if (attr.name == "width") {
            float width = std::stof(attr.value);
            auto size = component->GetSize();
            size.x = width;
            component->SetSize(size);
        }
        else if (attr.name == "height") {
            float height = std::stof(attr.value);
            auto size = component->GetSize();
            size.y = height;
            component->SetSize(size);
        }
        // Component-specific attributes
        else if (auto label = std::dynamic_pointer_cast<UILabel>(component)) {
            if (attr.name == "text") {
                std::string text = attr.isBound
                    ? ExpressionEvaluator::FormatString("{{" + attr.value + "}}", *context)
                    : attr.value;
                label->SetText(text);
            }
        }
        else if (auto button = std::dynamic_pointer_cast<UIButton>(component)) {
            if (attr.name == "label" || attr.name == "text") {
                button->SetLabel(attr.value);
            }
            else if (attr.name == "icon") {
                button->SetIcon(attr.value);
            }
        }
        else if (auto panel = std::dynamic_pointer_cast<UIPanel>(component)) {
            if (attr.name == "title") {
                panel->SetTitle(attr.value);
            }
            else if (attr.name == "collapsible") {
                panel->SetCollapsible(attr.value != "false");
            }
            else if (attr.name == "closable") {
                panel->SetClosable(attr.value == "true");
            }
        }
        else if (auto slider = std::dynamic_pointer_cast<UISlider>(component)) {
            if (attr.name == "label") {
                slider->SetLabel(attr.value);
            }
            else if (attr.name == "min") {
                float min = std::stof(attr.value);
                slider->SetRange(min, 1.0f);  // Will be overridden if max is set
            }
            else if (attr.name == "max") {
                float max = std::stof(attr.value);
                slider->SetRange(0.0f, max);
            }
        }
        else if (auto input = std::dynamic_pointer_cast<UITextInput>(component)) {
            if (attr.name == "label") {
                input->SetLabel(attr.value);
            }
            else if (attr.name == "placeholder") {
                input->SetPlaceholder(attr.value);
            }
            else if (attr.name == "multiline") {
                input->SetMultiline(attr.value == "true");
            }
            else if (attr.name == "password") {
                input->SetPassword(attr.value == "true");
            }
        }
        else if (auto progress = std::dynamic_pointer_cast<UIProgressBar>(component)) {
            if (attr.name == "value" || attr.name == "progress") {
                float value = attr.isBound
                    ? context->Get<float>(attr.value, 0.0f)
                    : std::stof(attr.value);
                progress->SetProgress(value);
            }
            else if (attr.name == "label") {
                progress->SetLabel(attr.value);
            }
        }
    }

    // Handle events
    for (const auto& attr : attributes) {
        if (attr.isEvent) {
            if (attr.name == "click") {
                component->OnClick([context, handler = attr.value](UIComponent* comp) {
                    context->TriggerEvent(handler);
                });
            }
            else if (attr.name == "change") {
                component->OnChange([context, handler = attr.value](UIComponent* comp) {
                    context->TriggerEvent(handler);
                });
            }
        }
    }
}

// ============================================================================
// TemplateRegistry Implementation
// ============================================================================

TemplateRegistry& TemplateRegistry::Instance() {
    static TemplateRegistry instance;
    return instance;
}

void TemplateRegistry::Register(const std::string& name, std::shared_ptr<UITemplate> templ) {
    m_templates[name] = templ;
}

void TemplateRegistry::RegisterFromString(const std::string& name, const std::string& templateStr) {
    auto templ = TemplateParser::Parse(templateStr);
    if (templ) {
        templ->SetName(name);
        Register(name, templ);
    }
}

void TemplateRegistry::RegisterFromFile(const std::string& name, const std::string& path) {
    auto templ = TemplateParser::ParseFile(path);
    if (templ) {
        templ->SetName(name);
        Register(name, templ);
    }
}

std::shared_ptr<UITemplate> TemplateRegistry::Get(const std::string& name) const {
    auto it = m_templates.find(name);
    return it != m_templates.end() ? it->second : nullptr;
}

bool TemplateRegistry::Has(const std::string& name) const {
    return m_templates.find(name) != m_templates.end();
}

void TemplateRegistry::LoadFromDirectory(const std::string& path, const std::string& extension) {
    if (!fs::exists(path)) return;

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            std::string name = entry.path().stem().string();
            RegisterFromFile(name, entry.path().string());
        }
    }
}

std::vector<std::string> TemplateRegistry::GetTemplateNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_templates) {
        names.push_back(name);
    }
    return names;
}

void TemplateRegistry::Clear() {
    m_templates.clear();
}

// ============================================================================
// ReactiveBinding Implementation
// ============================================================================

ReactiveBinding::ReactiveBinding(std::shared_ptr<DataContext> context, UIComponent::Ptr component)
    : m_context(context), m_component(component) {}

void ReactiveBinding::Bind(const std::string& propertyName, const std::string& dataPath) {
    m_bindings.push_back({propertyName, dataPath, false, nullptr});
}

void ReactiveBinding::BindTwoWay(const std::string& propertyName, const std::string& dataPath) {
    m_bindings.push_back({propertyName, dataPath, true, nullptr});

    // Set up change callback
    m_component->OnChange([this, dataPath](UIComponent* comp) {
        // Update context from component value
        // This would need component-specific value getters
    });
}

void ReactiveBinding::Update() {
    for (const auto& binding : m_bindings) {
        auto value = ExpressionEvaluator::Evaluate(binding.dataPath, *m_context);

        if (binding.transform) {
            value = binding.transform(value);
        }

        // Apply to component based on property name
        // This is simplified - real implementation would need type checking
        if (binding.propertyName == "visible") {
            m_component->SetVisible(std::get<bool>(value));
        }
        else if (binding.propertyName == "enabled") {
            m_component->SetEnabled(std::get<bool>(value));
        }
        else if (binding.propertyName == "text") {
            if (auto label = std::dynamic_pointer_cast<UILabel>(m_component)) {
                label->SetText(std::get<std::string>(value));
            }
        }
    }
}

} // namespace Nova
