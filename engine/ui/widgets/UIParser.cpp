#include "UITemplate.hpp"
#include "CoreWidgets.hpp"
#include <sstream>
#include <regex>
#include <stack>
#include <cctype>

namespace Nova {
namespace UI {

// =============================================================================
// Color name to value mapping
// =============================================================================

static const std::unordered_map<std::string, glm::vec4> s_namedColors = {
    {"transparent", {0, 0, 0, 0}},
    {"black", {0, 0, 0, 1}},
    {"white", {1, 1, 1, 1}},
    {"red", {1, 0, 0, 1}},
    {"green", {0, 1, 0, 1}},
    {"blue", {0, 0, 1, 1}},
    {"yellow", {1, 1, 0, 1}},
    {"cyan", {0, 1, 1, 1}},
    {"magenta", {1, 0, 1, 1}},
    {"orange", {1, 0.647f, 0, 1}},
    {"purple", {0.5f, 0, 0.5f, 1}},
    {"pink", {1, 0.753f, 0.796f, 1}},
    {"gray", {0.5f, 0.5f, 0.5f, 1}},
    {"grey", {0.5f, 0.5f, 0.5f, 1}},
    {"darkgray", {0.25f, 0.25f, 0.25f, 1}},
    {"lightgray", {0.75f, 0.75f, 0.75f, 1}},
};

// =============================================================================
// UIParser Implementation
// =============================================================================

WidgetPtr UIParser::ParseJSON(const nlohmann::json& json) {
    if (!json.is_object()) {
        return nullptr;
    }

    // Get tag name
    std::string tag = json.value("tag", "div");
    if (json.contains("type")) {
        tag = json["type"].get<std::string>();
    }

    // Create widget
    WidgetPtr widget = UIWidgetFactory::Instance().Create(tag);
    if (!widget) {
        widget = std::make_shared<UIWidget>(tag);
    }

    // Apply ID
    if (json.contains("id")) {
        widget->SetId(json["id"].get<std::string>());
    }

    // Apply classes
    if (json.contains("class")) {
        if (json["class"].is_string()) {
            widget->SetClass(json["class"].get<std::string>());
        } else if (json["class"].is_array()) {
            for (const auto& cls : json["class"]) {
                widget->AddClass(cls.get<std::string>());
            }
        }
    }

    // Apply text content
    if (json.contains("text")) {
        widget->SetText(json["text"].get<std::string>());
    }

    // Apply attributes
    if (json.contains("attrs") || json.contains("attributes")) {
        const auto& attrs = json.contains("attrs") ? json["attrs"] : json["attributes"];
        for (auto it = attrs.begin(); it != attrs.end(); ++it) {
            widget->SetAttribute(it.key(), it.value().get<std::string>());
        }
    }

    // Apply style
    if (json.contains("style")) {
        if (json["style"].is_string()) {
            widget->SetStyle(ParseStyle(json["style"].get<std::string>()));
        } else if (json["style"].is_object()) {
            UIStyle& style = widget->GetStyle();
            for (auto it = json["style"].begin(); it != json["style"].end(); ++it) {
                ParseStyleProperty(style, it.key(), it.value().dump());
            }
        }
    }

    // Apply bindings
    if (json.contains("bindings")) {
        for (const auto& binding : json["bindings"]) {
            std::string source = binding.value("source", "");
            std::string target = binding.value("target", "text");
            std::string modeStr = binding.value("mode", "oneWay");

            BindingMode mode = BindingMode::OneWay;
            if (modeStr == "twoWay") mode = BindingMode::TwoWay;
            else if (modeStr == "oneTime") mode = BindingMode::OneTime;
            else if (modeStr == "oneWayToSource") mode = BindingMode::OneWayToSource;

            widget->BindProperty(target, source, mode);
        }
    }

    // Shorthand bindings with : prefix
    for (auto it = json.begin(); it != json.end(); ++it) {
        const std::string& key = it.key();
        if (key.length() > 1 && key[0] == ':') {
            std::string prop = key.substr(1);
            std::string path = it.value().get<std::string>();
            widget->BindProperty(prop, path);
        }
    }

    // Apply event handlers (from JSON we can only reference by name)
    if (json.contains("events")) {
        for (auto it = json["events"].begin(); it != json["events"].end(); ++it) {
            // Events would need to be registered externally
            // This just marks them for later binding
            widget->SetAttribute("@" + it.key(), it.value().get<std::string>());
        }
    }

    // Shorthand events with @ prefix
    for (auto it = json.begin(); it != json.end(); ++it) {
        const std::string& key = it.key();
        if (key.length() > 1 && key[0] == '@') {
            widget->SetAttribute(key, it.value().get<std::string>());
        }
    }

    // Parse children
    if (json.contains("children")) {
        for (const auto& childJson : json["children"]) {
            if (auto child = ParseJSON(childJson)) {
                widget->AppendChild(child);
            }
        }
    }

    return widget;
}

WidgetPtr UIParser::ParseJSON(const std::string& jsonString) {
    try {
        auto json = nlohmann::json::parse(jsonString);
        return ParseJSON(json);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

WidgetPtr UIParser::ParseHTML(const std::string& html) {
    // Simplified HTML parser
    std::string trimmed = html;

    // Root container for parsed content
    auto root = std::make_shared<UIWidget>("fragment");
    std::stack<WidgetPtr> stack;
    stack.push(root);

    size_t pos = 0;
    while (pos < trimmed.length()) {
        // Skip whitespace
        while (pos < trimmed.length() && std::isspace(trimmed[pos])) pos++;
        if (pos >= trimmed.length()) break;

        if (trimmed[pos] == '<') {
            if (pos + 1 < trimmed.length() && trimmed[pos + 1] == '/') {
                // Closing tag
                size_t tagEnd = trimmed.find('>', pos);
                if (tagEnd != std::string::npos) {
                    // Pop from stack
                    if (stack.size() > 1) {
                        stack.pop();
                    }
                    pos = tagEnd + 1;
                }
            } else if (pos + 1 < trimmed.length() && trimmed[pos + 1] == '!') {
                // Comment - skip
                size_t commentEnd = trimmed.find("-->", pos);
                if (commentEnd != std::string::npos) {
                    pos = commentEnd + 3;
                } else {
                    pos = trimmed.length();
                }
            } else {
                // Opening tag
                size_t tagEnd = trimmed.find('>', pos);
                if (tagEnd != std::string::npos) {
                    std::string tagContent = trimmed.substr(pos + 1, tagEnd - pos - 1);
                    bool selfClosing = !tagContent.empty() && tagContent.back() == '/';
                    if (selfClosing) {
                        tagContent = tagContent.substr(0, tagContent.length() - 1);
                    }

                    // Parse tag name and attributes
                    std::stringstream ss(tagContent);
                    std::string tagName;
                    ss >> tagName;

                    WidgetPtr widget = CreateWidgetFromTag(tagName);

                    // Parse attributes
                    std::string attrStr;
                    std::getline(ss, attrStr);

                    // Simple attribute parsing
                    std::regex attrRegex(R"((\S+)\s*=\s*\"([^\"]*)\")");
                    std::smatch match;
                    std::string::const_iterator searchStart = attrStr.cbegin();
                    while (std::regex_search(searchStart, attrStr.cend(), match, attrRegex)) {
                        std::string attrName = match[1];
                        std::string attrValue = match[2];

                        if (attrName == "id") {
                            widget->SetId(attrValue);
                        } else if (attrName == "class") {
                            widget->SetClass(attrValue);
                        } else if (attrName == "style") {
                            widget->SetStyle(ParseStyle(attrValue));
                        } else if (attrName[0] == ':') {
                            // Binding
                            widget->BindProperty(attrName.substr(1), attrValue);
                        } else if (attrName[0] == '@') {
                            // Event (store as attribute for later binding)
                            widget->SetAttribute(attrName, attrValue);
                        } else {
                            widget->SetAttribute(attrName, attrValue);
                        }

                        searchStart = match.suffix().first;
                    }

                    // Add to parent
                    stack.top()->AppendChild(widget);

                    if (!selfClosing) {
                        stack.push(widget);
                    }

                    pos = tagEnd + 1;
                }
            }
        } else {
            // Text content
            size_t textEnd = trimmed.find('<', pos);
            if (textEnd == std::string::npos) textEnd = trimmed.length();

            std::string text = trimmed.substr(pos, textEnd - pos);
            // Trim whitespace
            size_t start = text.find_first_not_of(" \t\n\r");
            size_t end = text.find_last_not_of(" \t\n\r");

            if (start != std::string::npos && end != std::string::npos) {
                text = text.substr(start, end - start + 1);
                if (!text.empty()) {
                    // Add text node
                    auto textWidget = std::make_shared<UIText>(text);
                    stack.top()->AppendChild(textWidget);
                }
            }

            pos = textEnd;
        }
    }

    // If root has only one child, return that child
    if (root->GetChildren().size() == 1) {
        return root->GetChildren()[0];
    }

    return root;
}

UIStyle UIParser::ParseStyle(const std::string& styleString) {
    UIStyle style;

    // Parse CSS-like style string: "property: value; property: value;"
    std::stringstream ss(styleString);
    std::string declaration;

    while (std::getline(ss, declaration, ';')) {
        size_t colonPos = declaration.find(':');
        if (colonPos != std::string::npos) {
            std::string property = declaration.substr(0, colonPos);
            std::string value = declaration.substr(colonPos + 1);

            // Trim whitespace
            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t\n\r");
                size_t end = s.find_last_not_of(" \t\n\r");
                if (start != std::string::npos && end != std::string::npos) {
                    s = s.substr(start, end - start + 1);
                } else {
                    s.clear();
                }
            };

            trim(property);
            trim(value);

            ParseStyleProperty(style, property, value);
        }
    }

    return style;
}

void UIParser::ParseStyleProperty(UIStyle& style, const std::string& property,
                                  const std::string& value) {
    // Display
    if (property == "display") {
        if (value == "flex") style.display = Display::Flex;
        else if (value == "block") style.display = Display::Block;
        else if (value == "inline") style.display = Display::Inline;
        else if (value == "none") style.display = Display::None;
        else if (value == "grid") style.display = Display::Grid;
    }
    // Position
    else if (property == "position") {
        if (value == "static") style.position = Position::Static;
        else if (value == "relative") style.position = Position::Relative;
        else if (value == "absolute") style.position = Position::Absolute;
        else if (value == "fixed") style.position = Position::Fixed;
    }
    // Flex direction
    else if (property == "flex-direction" || property == "flexDirection") {
        if (value == "row") style.flexDirection = LayoutDirection::Row;
        else if (value == "column") style.flexDirection = LayoutDirection::Column;
        else if (value == "row-reverse") style.flexDirection = LayoutDirection::RowReverse;
        else if (value == "column-reverse") style.flexDirection = LayoutDirection::ColumnReverse;
    }
    // Alignment
    else if (property == "align-items" || property == "alignItems") {
        if (value == "start" || value == "flex-start") style.alignItems = Alignment::Start;
        else if (value == "center") style.alignItems = Alignment::Center;
        else if (value == "end" || value == "flex-end") style.alignItems = Alignment::End;
        else if (value == "stretch") style.alignItems = Alignment::Stretch;
    }
    else if (property == "justify-content" || property == "justifyContent") {
        if (value == "start" || value == "flex-start") style.justifyContent = Alignment::Start;
        else if (value == "center") style.justifyContent = Alignment::Center;
        else if (value == "end" || value == "flex-end") style.justifyContent = Alignment::End;
        else if (value == "space-between") style.justifyContent = Alignment::SpaceBetween;
        else if (value == "space-around") style.justifyContent = Alignment::SpaceAround;
    }
    // Dimensions
    else if (property == "width") {
        style.width = ParseLength(value);
    }
    else if (property == "height") {
        style.height = ParseLength(value);
    }
    else if (property == "min-width" || property == "minWidth") {
        style.minWidth = ParseLength(value);
    }
    else if (property == "min-height" || property == "minHeight") {
        style.minHeight = ParseLength(value);
    }
    else if (property == "max-width" || property == "maxWidth") {
        style.maxWidth = ParseLength(value);
    }
    else if (property == "max-height" || property == "maxHeight") {
        style.maxHeight = ParseLength(value);
    }
    // Position offsets
    else if (property == "top") {
        style.top = ParseLength(value);
    }
    else if (property == "right") {
        style.right = ParseLength(value);
    }
    else if (property == "bottom") {
        style.bottom = ParseLength(value);
    }
    else if (property == "left") {
        style.left = ParseLength(value);
    }
    // Margin
    else if (property == "margin") {
        Length len = ParseLength(value);
        style.margin = BoxSpacing(len);
    }
    else if (property == "margin-top" || property == "marginTop") {
        style.margin.top = ParseLength(value);
    }
    else if (property == "margin-right" || property == "marginRight") {
        style.margin.right = ParseLength(value);
    }
    else if (property == "margin-bottom" || property == "marginBottom") {
        style.margin.bottom = ParseLength(value);
    }
    else if (property == "margin-left" || property == "marginLeft") {
        style.margin.left = ParseLength(value);
    }
    // Padding
    else if (property == "padding") {
        Length len = ParseLength(value);
        style.padding = BoxSpacing(len);
    }
    else if (property == "padding-top" || property == "paddingTop") {
        style.padding.top = ParseLength(value);
    }
    else if (property == "padding-right" || property == "paddingRight") {
        style.padding.right = ParseLength(value);
    }
    else if (property == "padding-bottom" || property == "paddingBottom") {
        style.padding.bottom = ParseLength(value);
    }
    else if (property == "padding-left" || property == "paddingLeft") {
        style.padding.left = ParseLength(value);
    }
    // Colors
    else if (property == "background-color" || property == "backgroundColor" || property == "background") {
        style.backgroundColor = ParseColor(value);
    }
    else if (property == "color") {
        style.color = ParseColor(value);
    }
    // Border
    else if (property == "border-width" || property == "borderWidth") {
        style.border.width = std::stof(value);
    }
    else if (property == "border-color" || property == "borderColor") {
        style.border.color = ParseColor(value);
    }
    else if (property == "border-radius" || property == "borderRadius") {
        style.border.radius = ParseLength(value).value;
    }
    // Text
    else if (property == "font-size" || property == "fontSize") {
        style.fontSize = ParseLength(value).value;
    }
    else if (property == "font-family" || property == "fontFamily") {
        style.fontFamily = value;
    }
    else if (property == "text-align" || property == "textAlign") {
        if (value == "left") style.textAlign = TextAlign::Left;
        else if (value == "center") style.textAlign = TextAlign::Center;
        else if (value == "right") style.textAlign = TextAlign::Right;
        else if (value == "justify") style.textAlign = TextAlign::Justify;
    }
    // Effects
    else if (property == "opacity") {
        style.opacity = std::stof(value);
    }
    else if (property == "z-index" || property == "zIndex") {
        style.zIndex = std::stoi(value);
    }
    else if (property == "visible" || property == "visibility") {
        style.visible = (value != "hidden" && value != "false" && value != "0");
    }
    // Flex
    else if (property == "flex-grow" || property == "flexGrow") {
        style.flexGrow = std::stof(value);
    }
    else if (property == "flex-shrink" || property == "flexShrink") {
        style.flexShrink = std::stof(value);
    }
    else if (property == "gap") {
        style.gap = ParseLength(value).value;
    }
    // Overflow
    else if (property == "overflow") {
        Overflow ov = Overflow::Visible;
        if (value == "hidden") ov = Overflow::Hidden;
        else if (value == "scroll") ov = Overflow::Scroll;
        else if (value == "auto") ov = Overflow::Auto;
        style.overflowX = style.overflowY = ov;
    }
    else if (property == "overflow-x" || property == "overflowX") {
        if (value == "hidden") style.overflowX = Overflow::Hidden;
        else if (value == "scroll") style.overflowX = Overflow::Scroll;
        else if (value == "auto") style.overflowX = Overflow::Auto;
        else style.overflowX = Overflow::Visible;
    }
    else if (property == "overflow-y" || property == "overflowY") {
        if (value == "hidden") style.overflowY = Overflow::Hidden;
        else if (value == "scroll") style.overflowY = Overflow::Scroll;
        else if (value == "auto") style.overflowY = Overflow::Auto;
        else style.overflowY = Overflow::Visible;
    }
}

Length UIParser::ParseLength(const std::string& value) {
    if (value.empty() || value == "auto") {
        return Length::Auto();
    }

    // Try to parse numeric part
    size_t unitStart = 0;
    for (size_t i = 0; i < value.length(); ++i) {
        char c = value[i];
        if (!std::isdigit(c) && c != '.' && c != '-') {
            unitStart = i;
            break;
        }
        unitStart = i + 1;
    }

    float num = 0.0f;
    try {
        num = std::stof(value.substr(0, unitStart));
    } catch (...) {
        return Length::Px(0);
    }

    std::string unit = value.substr(unitStart);

    if (unit.empty() || unit == "px") {
        return Length::Px(num);
    } else if (unit == "%") {
        return Length::Pct(num);
    } else if (unit == "em") {
        return Length::Em(num);
    } else if (unit == "vw") {
        return Length::Vw(num);
    } else if (unit == "vh") {
        return Length::Vh(num);
    }

    return Length::Px(num);
}

glm::vec4 UIParser::ParseColor(const std::string& value) {
    // Named color
    auto it = s_namedColors.find(value);
    if (it != s_namedColors.end()) {
        return it->second;
    }

    // Hex color (#RGB, #RGBA, #RRGGBB, #RRGGBBAA)
    if (!value.empty() && value[0] == '#') {
        std::string hex = value.substr(1);
        unsigned int r = 0, g = 0, b = 0, a = 255;

        if (hex.length() == 3) {
            r = std::stoi(hex.substr(0, 1) + hex.substr(0, 1), nullptr, 16);
            g = std::stoi(hex.substr(1, 1) + hex.substr(1, 1), nullptr, 16);
            b = std::stoi(hex.substr(2, 1) + hex.substr(2, 1), nullptr, 16);
        } else if (hex.length() == 4) {
            r = std::stoi(hex.substr(0, 1) + hex.substr(0, 1), nullptr, 16);
            g = std::stoi(hex.substr(1, 1) + hex.substr(1, 1), nullptr, 16);
            b = std::stoi(hex.substr(2, 1) + hex.substr(2, 1), nullptr, 16);
            a = std::stoi(hex.substr(3, 1) + hex.substr(3, 1), nullptr, 16);
        } else if (hex.length() == 6) {
            r = std::stoi(hex.substr(0, 2), nullptr, 16);
            g = std::stoi(hex.substr(2, 2), nullptr, 16);
            b = std::stoi(hex.substr(4, 2), nullptr, 16);
        } else if (hex.length() == 8) {
            r = std::stoi(hex.substr(0, 2), nullptr, 16);
            g = std::stoi(hex.substr(2, 2), nullptr, 16);
            b = std::stoi(hex.substr(4, 2), nullptr, 16);
            a = std::stoi(hex.substr(6, 2), nullptr, 16);
        }

        return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    // RGB/RGBA functional notation
    if (value.find("rgb") == 0) {
        std::regex rgbRegex(R"(rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+))?\s*\))");
        std::smatch match;
        if (std::regex_match(value, match, rgbRegex)) {
            float r = std::stof(match[1]) / 255.0f;
            float g = std::stof(match[2]) / 255.0f;
            float b = std::stof(match[3]) / 255.0f;
            float a = match[4].matched ? std::stof(match[4]) : 1.0f;
            return glm::vec4(r, g, b, a);
        }
    }

    return glm::vec4(1, 1, 1, 1);
}

WidgetPtr UIParser::CreateWidgetFromTag(const std::string& tag) {
    return UIWidgetFactory::Instance().Create(tag);
}

void UIParser::ApplyAttributes(WidgetPtr widget, const nlohmann::json& attrs) {
    // Implementation in ParseJSON handles this
}

void UIParser::ApplyBindings(WidgetPtr widget, const nlohmann::json& bindings) {
    // Implementation in ParseJSON handles this
}

} // namespace UI
} // namespace Nova
