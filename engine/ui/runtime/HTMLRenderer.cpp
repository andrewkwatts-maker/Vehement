#include "HTMLRenderer.hpp"
#include "UIWindow.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>
#include <stack>

namespace Engine {
namespace UI {

// Color implementation
Color Color::FromHex(const std::string& hex) {
    Color c;
    std::string h = hex;
    if (h[0] == '#') h = h.substr(1);

    if (h.length() == 3) {
        c.r = static_cast<uint8_t>(std::stoi(std::string(2, h[0]), nullptr, 16));
        c.g = static_cast<uint8_t>(std::stoi(std::string(2, h[1]), nullptr, 16));
        c.b = static_cast<uint8_t>(std::stoi(std::string(2, h[2]), nullptr, 16));
    } else if (h.length() == 6) {
        c.r = static_cast<uint8_t>(std::stoi(h.substr(0, 2), nullptr, 16));
        c.g = static_cast<uint8_t>(std::stoi(h.substr(2, 2), nullptr, 16));
        c.b = static_cast<uint8_t>(std::stoi(h.substr(4, 2), nullptr, 16));
    } else if (h.length() == 8) {
        c.r = static_cast<uint8_t>(std::stoi(h.substr(0, 2), nullptr, 16));
        c.g = static_cast<uint8_t>(std::stoi(h.substr(2, 2), nullptr, 16));
        c.b = static_cast<uint8_t>(std::stoi(h.substr(4, 2), nullptr, 16));
        c.a = static_cast<uint8_t>(std::stoi(h.substr(6, 2), nullptr, 16));
    }
    return c;
}

Color Color::FromRGB(int r, int g, int b) {
    return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

Color Color::FromRGBA(int r, int g, int b, float a) {
    return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(a * 255));
}

std::string Color::ToHex() const {
    char buf[10];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
    return std::string(buf);
}

// DOMElement implementation
DOMElement* DOMElement::FindById(const std::string& id) {
    if (this->id == id) return this;
    for (auto& child : children) {
        if (auto found = child->FindById(id)) return found;
    }
    return nullptr;
}

std::vector<DOMElement*> DOMElement::FindByClass(const std::string& className) {
    std::vector<DOMElement*> result;
    if (HasClass(className)) result.push_back(this);
    for (auto& child : children) {
        auto childResults = child->FindByClass(className);
        result.insert(result.end(), childResults.begin(), childResults.end());
    }
    return result;
}

std::vector<DOMElement*> DOMElement::FindByTagName(const std::string& tagName) {
    std::vector<DOMElement*> result;
    if (this->tagName == tagName) result.push_back(this);
    for (auto& child : children) {
        auto childResults = child->FindByTagName(tagName);
        result.insert(result.end(), childResults.begin(), childResults.end());
    }
    return result;
}

DOMElement* DOMElement::QuerySelector(const std::string& selector) {
    // Simple selector implementation
    if (selector[0] == '#') {
        return FindById(selector.substr(1));
    } else if (selector[0] == '.') {
        auto results = FindByClass(selector.substr(1));
        return results.empty() ? nullptr : results[0];
    } else {
        auto results = FindByTagName(selector);
        return results.empty() ? nullptr : results[0];
    }
}

std::vector<DOMElement*> DOMElement::QuerySelectorAll(const std::string& selector) {
    if (selector[0] == '#') {
        auto elem = FindById(selector.substr(1));
        return elem ? std::vector<DOMElement*>{elem} : std::vector<DOMElement*>{};
    } else if (selector[0] == '.') {
        return FindByClass(selector.substr(1));
    } else {
        return FindByTagName(selector);
    }
}

void DOMElement::SetAttribute(const std::string& name, const std::string& value) {
    attributes[name] = value;
    if (name == "id") id = value;
    else if (name == "class") {
        classes.clear();
        std::istringstream iss(value);
        std::string cls;
        while (iss >> cls) classes.push_back(cls);
    }
}

std::string DOMElement::GetAttribute(const std::string& name) const {
    auto it = attributes.find(name);
    return it != attributes.end() ? it->second : "";
}

void DOMElement::AddClass(const std::string& className) {
    if (!HasClass(className)) {
        classes.push_back(className);
    }
}

void DOMElement::RemoveClass(const std::string& className) {
    classes.erase(std::remove(classes.begin(), classes.end(), className), classes.end());
}

bool DOMElement::HasClass(const std::string& className) const {
    return std::find(classes.begin(), classes.end(), className) != classes.end();
}

void DOMElement::ToggleClass(const std::string& className) {
    if (HasClass(className)) RemoveClass(className);
    else AddClass(className);
}

// Canvas2DContext implementation
Canvas2DContext::Canvas2DContext(int width, int height) : m_width(width), m_height(height) {
    m_stateStack.push_back(m_currentState);
}

Canvas2DContext::~Canvas2DContext() = default;

void Canvas2DContext::Save() {
    m_stateStack.push_back(m_currentState);
}

void Canvas2DContext::Restore() {
    if (m_stateStack.size() > 1) {
        m_stateStack.pop_back();
        m_currentState = m_stateStack.back();
    }
}

void Canvas2DContext::Scale(float x, float y) {
    m_currentState.transform[0] *= x;
    m_currentState.transform[3] *= y;
}

void Canvas2DContext::Rotate(float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    float a = m_currentState.transform[0];
    float b = m_currentState.transform[1];
    float cc = m_currentState.transform[2];
    float d = m_currentState.transform[3];
    m_currentState.transform[0] = a * c + cc * s;
    m_currentState.transform[1] = b * c + d * s;
    m_currentState.transform[2] = cc * c - a * s;
    m_currentState.transform[3] = d * c - b * s;
}

void Canvas2DContext::Translate(float x, float y) {
    m_currentState.transform[4] += m_currentState.transform[0] * x + m_currentState.transform[2] * y;
    m_currentState.transform[5] += m_currentState.transform[1] * x + m_currentState.transform[3] * y;
}

void Canvas2DContext::Transform(float a, float b, float c, float d, float e, float f) {
    float t0 = m_currentState.transform[0] * a + m_currentState.transform[2] * b;
    float t1 = m_currentState.transform[1] * a + m_currentState.transform[3] * b;
    float t2 = m_currentState.transform[0] * c + m_currentState.transform[2] * d;
    float t3 = m_currentState.transform[1] * c + m_currentState.transform[3] * d;
    float t4 = m_currentState.transform[0] * e + m_currentState.transform[2] * f + m_currentState.transform[4];
    float t5 = m_currentState.transform[1] * e + m_currentState.transform[3] * f + m_currentState.transform[5];
    m_currentState.transform[0] = t0;
    m_currentState.transform[1] = t1;
    m_currentState.transform[2] = t2;
    m_currentState.transform[3] = t3;
    m_currentState.transform[4] = t4;
    m_currentState.transform[5] = t5;
}

void Canvas2DContext::SetTransform(float a, float b, float c, float d, float e, float f) {
    m_currentState.transform[0] = a;
    m_currentState.transform[1] = b;
    m_currentState.transform[2] = c;
    m_currentState.transform[3] = d;
    m_currentState.transform[4] = e;
    m_currentState.transform[5] = f;
}

void Canvas2DContext::ResetTransform() {
    SetTransform(1, 0, 0, 1, 0, 0);
}

void Canvas2DContext::SetGlobalAlpha(float alpha) {
    m_currentState.globalAlpha = alpha;
}

void Canvas2DContext::SetGlobalCompositeOperation(const std::string& op) {
    // Store for later use
}

void Canvas2DContext::SetFillStyle(const Color& color) {
    m_currentState.fillStyle = color;
}

void Canvas2DContext::SetStrokeStyle(const Color& color) {
    m_currentState.strokeStyle = color;
}

void Canvas2DContext::SetLineWidth(float width) {
    m_currentState.lineWidth = width;
}

void Canvas2DContext::SetLineCap(const std::string& cap) {}
void Canvas2DContext::SetLineJoin(const std::string& join) {}
void Canvas2DContext::SetMiterLimit(float limit) {}

void Canvas2DContext::SetFont(const std::string& font) {}
void Canvas2DContext::SetTextAlign(const std::string& align) {}
void Canvas2DContext::SetTextBaseline(const std::string& baseline) {}

void Canvas2DContext::FillText(const std::string& text, float x, float y, float maxWidth) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Text;
    cmd.text = text;
    cmd.x = x;
    cmd.y = y;
    cmd.color = m_currentState.fillStyle;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

void Canvas2DContext::StrokeText(const std::string& text, float x, float y, float maxWidth) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Text;
    cmd.text = text;
    cmd.x = x;
    cmd.y = y;
    cmd.color = m_currentState.strokeStyle;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

float Canvas2DContext::MeasureText(const std::string& text) {
    return text.length() * 8.0f; // Simplified
}

void Canvas2DContext::ClearRect(float x, float y, float width, float height) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Quad;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.color = Color(0, 0, 0, 0);
    cmd.opacity = 1.0f;
    m_drawCommands.push_back(cmd);
}

void Canvas2DContext::FillRect(float x, float y, float width, float height) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Quad;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.color = m_currentState.fillStyle;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

void Canvas2DContext::StrokeRect(float x, float y, float width, float height) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Quad;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.color = Color(0, 0, 0, 0);
    cmd.borderColor = m_currentState.strokeStyle;
    cmd.borderWidth = m_currentState.lineWidth;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

void Canvas2DContext::BeginPath() { m_path.clear(); }
void Canvas2DContext::ClosePath() {}

void Canvas2DContext::MoveTo(float x, float y) {
    m_path.push_back({x, y, PathPoint::Move});
}

void Canvas2DContext::LineTo(float x, float y) {
    m_path.push_back({x, y, PathPoint::Line});
}

void Canvas2DContext::BezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
    m_path.push_back({x, y, PathPoint::Curve});
}

void Canvas2DContext::QuadraticCurveTo(float cpx, float cpy, float x, float y) {
    m_path.push_back({x, y, PathPoint::Curve});
}

void Canvas2DContext::Arc(float x, float y, float radius, float startAngle, float endAngle, bool counterclockwise) {}
void Canvas2DContext::ArcTo(float x1, float y1, float x2, float y2, float radius) {}
void Canvas2DContext::Ellipse(float x, float y, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool counterclockwise) {}

void Canvas2DContext::Rect(float x, float y, float width, float height) {
    MoveTo(x, y);
    LineTo(x + width, y);
    LineTo(x + width, y + height);
    LineTo(x, y + height);
    ClosePath();
}

void Canvas2DContext::RoundRect(float x, float y, float width, float height, float radius) {
    // Simplified - just a rect for now
    Rect(x, y, width, height);
}

void Canvas2DContext::Fill() {
    // Convert path to draw commands
}

void Canvas2DContext::Stroke() {
    for (size_t i = 1; i < m_path.size(); ++i) {
        if (m_path[i].type == PathPoint::Line) {
            DrawCommand cmd;
            cmd.type = DrawCommand::Type::Line;
            cmd.x = m_path[i-1].x;
            cmd.y = m_path[i-1].y;
            cmd.width = m_path[i].x - m_path[i-1].x;
            cmd.height = m_path[i].y - m_path[i-1].y;
            cmd.color = m_currentState.strokeStyle;
            cmd.borderWidth = m_currentState.lineWidth;
            cmd.opacity = m_currentState.globalAlpha;
            m_drawCommands.push_back(cmd);
        }
    }
}

void Canvas2DContext::Clip() {}

void Canvas2DContext::DrawImage(uint32_t textureId, float x, float y) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Image;
    cmd.textureId = textureId;
    cmd.x = x; cmd.y = y;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

void Canvas2DContext::DrawImage(uint32_t textureId, float x, float y, float width, float height) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Image;
    cmd.textureId = textureId;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

void Canvas2DContext::DrawImage(uint32_t textureId, float sx, float sy, float sWidth, float sHeight, float dx, float dy, float dWidth, float dHeight) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Image;
    cmd.textureId = textureId;
    cmd.texX = sx; cmd.texY = sy;
    cmd.texWidth = sWidth; cmd.texHeight = sHeight;
    cmd.x = dx; cmd.y = dy;
    cmd.width = dWidth; cmd.height = dHeight;
    cmd.opacity = m_currentState.globalAlpha;
    m_drawCommands.push_back(cmd);
}

std::vector<uint8_t> Canvas2DContext::GetImageData(float x, float y, float width, float height) {
    return std::vector<uint8_t>(static_cast<size_t>(width * height * 4), 0);
}

void Canvas2DContext::PutImageData(const std::vector<uint8_t>& data, float x, float y) {}

// HTMLRenderer implementation
HTMLRenderer::HTMLRenderer() = default;
HTMLRenderer::~HTMLRenderer() { Shutdown(); }

bool HTMLRenderer::Initialize(int width, int height, float dpiScale) {
    m_width = width;
    m_height = height;
    m_dpiScale = dpiScale;

    // Load default font
    LoadFont("default", "", 16);

    return true;
}

void HTMLRenderer::Shutdown() {
    m_fonts.clear();
    m_textures.clear();
    m_globalStyles.clear();
    m_drawCommands.clear();
}

void HTMLRenderer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
}

void HTMLRenderer::SetDPIScale(float scale) {
    m_dpiScale = scale;
}

void HTMLRenderer::SetViewportMode(bool fullscreen) {
    m_fullscreen = fullscreen;
}

void HTMLRenderer::BeginFrame() {
    m_drawCommands.clear();
    m_drawCalls = 0;
    m_triangles = 0;
}

void HTMLRenderer::EndFrame() {
    FlushDrawCommands();
}

void HTMLRenderer::RenderWindow(UIWindow* window) {
    if (!window || !window->IsVisible()) return;

    auto* root = window->GetRootElement();
    if (!root) return;

    float x = static_cast<float>(window->GetX());
    float y = static_cast<float>(window->GetY());

    // Push window clip rect
    PushClipRect(x, y, static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()));

    // Render window background
    DrawRoundedRect(x, y, static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()),
                   4.0f, window->GetBackgroundColor());

    // Render DOM tree
    RenderElement(root, x, y);

    PopClipRect();
}

void HTMLRenderer::RenderDebugOverlay(const std::vector<UIWindow*>& windows) {
    // Draw debug info
    float y = 10;
    std::string debugText = "Windows: " + std::to_string(windows.size());
    DrawText(debugText, 10, y, GetFont("default"), Color::FromRGB(255, 255, 0));
    y += 20;

    debugText = "Draw calls: " + std::to_string(m_drawCalls);
    DrawText(debugText, 10, y, GetFont("default"), Color::FromRGB(255, 255, 0));
}

std::unique_ptr<DOMElement> HTMLRenderer::ParseHTML(const std::string& html) {
    auto root = std::make_unique<DOMElement>();
    root->tagName = "root";

    size_t pos = 0;
    ParseHTMLNode(html, pos, root.get());

    return root;
}

void HTMLRenderer::ParseHTMLNode(const std::string& html, size_t& pos, DOMElement* parent) {
    while (pos < html.length()) {
        // Skip whitespace
        while (pos < html.length() && std::isspace(html[pos])) pos++;
        if (pos >= html.length()) break;

        if (html[pos] == '<') {
            pos++;
            if (pos < html.length() && html[pos] == '/') {
                // Closing tag
                while (pos < html.length() && html[pos] != '>') pos++;
                if (pos < html.length()) pos++;
                return;
            }

            if (pos < html.length() && html[pos] == '!') {
                // Comment or DOCTYPE
                while (pos < html.length() && html[pos] != '>') pos++;
                if (pos < html.length()) pos++;
                continue;
            }

            // Opening tag
            auto element = std::make_unique<DOMElement>();
            element->parent = parent;

            // Parse tag name
            std::string tagName;
            while (pos < html.length() && !std::isspace(html[pos]) && html[pos] != '>' && html[pos] != '/') {
                tagName += std::tolower(html[pos]);
                pos++;
            }
            element->tagName = tagName;

            // Parse attributes
            while (pos < html.length() && html[pos] != '>' && html[pos] != '/') {
                while (pos < html.length() && std::isspace(html[pos])) pos++;
                if (html[pos] == '>' || html[pos] == '/') break;

                std::string attrName;
                while (pos < html.length() && html[pos] != '=' && !std::isspace(html[pos]) && html[pos] != '>' && html[pos] != '/') {
                    attrName += std::tolower(html[pos]);
                    pos++;
                }

                while (pos < html.length() && std::isspace(html[pos])) pos++;

                std::string attrValue;
                if (pos < html.length() && html[pos] == '=') {
                    pos++;
                    while (pos < html.length() && std::isspace(html[pos])) pos++;

                    char quote = 0;
                    if (pos < html.length() && (html[pos] == '"' || html[pos] == '\'')) {
                        quote = html[pos];
                        pos++;
                    }

                    while (pos < html.length()) {
                        if (quote && html[pos] == quote) { pos++; break; }
                        if (!quote && (std::isspace(html[pos]) || html[pos] == '>')) break;
                        attrValue += html[pos];
                        pos++;
                    }
                }

                element->SetAttribute(attrName, attrValue);
            }

            // Self-closing tag
            bool selfClosing = false;
            if (pos < html.length() && html[pos] == '/') {
                selfClosing = true;
                pos++;
            }

            while (pos < html.length() && html[pos] != '>') pos++;
            if (pos < html.length()) pos++;

            // Void elements
            static const std::vector<std::string> voidElements = {
                "area", "base", "br", "col", "embed", "hr", "img", "input",
                "link", "meta", "param", "source", "track", "wbr"
            };
            bool isVoid = std::find(voidElements.begin(), voidElements.end(), tagName) != voidElements.end();

            if (!selfClosing && !isVoid) {
                // Parse children
                ParseHTMLNode(html, pos, element.get());
            }

            parent->children.push_back(std::move(element));
        } else {
            // Text content
            std::string text;
            while (pos < html.length() && html[pos] != '<') {
                text += html[pos];
                pos++;
            }

            // Trim and add if not empty
            size_t start = text.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                size_t end = text.find_last_not_of(" \t\n\r");
                text = text.substr(start, end - start + 1);

                auto textNode = std::make_unique<DOMElement>();
                textNode->tagName = "#text";
                textNode->textContent = text;
                textNode->parent = parent;
                parent->children.push_back(std::move(textNode));
            }
        }
    }
}

std::vector<CSSRule> HTMLRenderer::ParseCSS(const std::string& css) {
    std::vector<CSSRule> rules;

    size_t pos = 0;
    while (pos < css.length()) {
        // Skip whitespace and comments
        while (pos < css.length() && std::isspace(css[pos])) pos++;
        if (pos >= css.length()) break;

        if (css[pos] == '/' && pos + 1 < css.length() && css[pos + 1] == '*') {
            pos += 2;
            while (pos + 1 < css.length() && !(css[pos] == '*' && css[pos + 1] == '/')) pos++;
            pos += 2;
            continue;
        }

        // Parse selector
        std::string selector;
        while (pos < css.length() && css[pos] != '{') {
            selector += css[pos];
            pos++;
        }
        pos++; // Skip '{'

        // Trim selector
        size_t start = selector.find_first_not_of(" \t\n\r");
        size_t end = selector.find_last_not_of(" \t\n\r");
        if (start != std::string::npos) {
            selector = selector.substr(start, end - start + 1);
        }

        CSSRule rule;
        rule.selector = selector;

        // Calculate specificity
        for (char c : selector) {
            if (c == '#') rule.specificity += 100;
            else if (c == '.') rule.specificity += 10;
        }
        if (selector[0] != '#' && selector[0] != '.') rule.specificity += 1;

        // Parse properties
        while (pos < css.length() && css[pos] != '}') {
            while (pos < css.length() && std::isspace(css[pos])) pos++;
            if (css[pos] == '}') break;

            std::string property;
            while (pos < css.length() && css[pos] != ':' && css[pos] != '}') {
                property += css[pos];
                pos++;
            }

            if (css[pos] == ':') {
                pos++; // Skip ':'
                while (pos < css.length() && std::isspace(css[pos])) pos++;

                std::string value;
                while (pos < css.length() && css[pos] != ';' && css[pos] != '}') {
                    value += css[pos];
                    pos++;
                }

                if (css[pos] == ';') pos++;

                // Trim property and value
                start = property.find_first_not_of(" \t\n\r");
                end = property.find_last_not_of(" \t\n\r");
                if (start != std::string::npos) {
                    property = property.substr(start, end - start + 1);
                }

                start = value.find_first_not_of(" \t\n\r");
                end = value.find_last_not_of(" \t\n\r");
                if (start != std::string::npos) {
                    value = value.substr(start, end - start + 1);
                }

                ParseCSSProperty(property, value, rule.properties);
            }
        }
        pos++; // Skip '}'

        rules.push_back(rule);
    }

    return rules;
}

void HTMLRenderer::LoadGlobalCSS(const std::string& path) {
    // Load CSS file and parse
    // For now, just store path
    // In production, would read file and call ParseCSS
}

void HTMLRenderer::SetCSSVariable(const std::string& name, const std::string& value) {
    m_cssVariables[name] = value;
}

std::string HTMLRenderer::GetCSSVariable(const std::string& name) const {
    auto it = m_cssVariables.find(name);
    return it != m_cssVariables.end() ? it->second : "";
}

void HTMLRenderer::ComputeLayout(DOMElement* root, float containerWidth, float containerHeight) {
    if (!root) return;

    root->layout.x = 0;
    root->layout.y = 0;
    root->layout.width = containerWidth;
    root->layout.height = containerHeight;

    if (root->computedStyle.display == StyleProperties::Display::Flex) {
        ComputeFlexLayout(root, containerWidth, containerHeight);
    } else {
        ComputeBlockLayout(root, containerWidth, containerHeight);
    }
}

void HTMLRenderer::ComputeStyles(DOMElement* root, const std::vector<CSSRule>& rules) {
    if (!root) return;

    // Apply matching rules
    for (const auto& rule : rules) {
        // Simple selector matching
        bool matches = false;
        if (rule.selector[0] == '#' && rule.selector.substr(1) == root->id) {
            matches = true;
        } else if (rule.selector[0] == '.' && root->HasClass(rule.selector.substr(1))) {
            matches = true;
        } else if (rule.selector == root->tagName) {
            matches = true;
        }

        if (matches) {
            // Apply properties (simplified)
            root->computedStyle = rule.properties;
        }
    }

    // Recurse to children
    for (auto& child : root->children) {
        ComputeStyles(child.get(), rules);
    }
}

void HTMLRenderer::ComputeFlexLayout(DOMElement* element, float containerWidth, float containerHeight) {
    auto& flex = element->computedStyle.flex;
    bool isRow = flex.direction == FlexProperties::Direction::Row ||
                 flex.direction == FlexProperties::Direction::RowReverse;

    float mainSize = isRow ? containerWidth : containerHeight;
    float crossSize = isRow ? containerHeight : containerWidth;

    // Calculate children sizes
    float totalFlexGrow = 0;
    float totalFixedSize = 0;

    for (auto& child : element->children) {
        if (child->computedStyle.display == StyleProperties::Display::None) continue;

        float childMainSize = isRow ? child->computedStyle.width : child->computedStyle.height;
        if ((isRow && child->computedStyle.widthAuto) || (!isRow && child->computedStyle.heightAuto)) {
            totalFlexGrow += child->computedStyle.flex.flexGrow > 0 ? child->computedStyle.flex.flexGrow : 1;
        } else {
            totalFixedSize += childMainSize;
        }
    }

    float remainingSpace = mainSize - totalFixedSize;
    float offset = 0;

    // Position children
    for (auto& child : element->children) {
        if (child->computedStyle.display == StyleProperties::Display::None) continue;

        float childMainSize;
        if ((isRow && child->computedStyle.widthAuto) || (!isRow && child->computedStyle.heightAuto)) {
            float grow = child->computedStyle.flex.flexGrow > 0 ? child->computedStyle.flex.flexGrow : 1;
            childMainSize = (remainingSpace / totalFlexGrow) * grow;
        } else {
            childMainSize = isRow ? child->computedStyle.width : child->computedStyle.height;
        }

        if (isRow) {
            child->layout.x = element->layout.x + offset;
            child->layout.y = element->layout.y;
            child->layout.width = childMainSize;
            child->layout.height = crossSize;
        } else {
            child->layout.x = element->layout.x;
            child->layout.y = element->layout.y + offset;
            child->layout.width = crossSize;
            child->layout.height = childMainSize;
        }

        offset += childMainSize;

        // Recurse
        ComputeLayout(child.get(), child->layout.width, child->layout.height);
    }
}

void HTMLRenderer::ComputeBlockLayout(DOMElement* element, float containerWidth, float containerHeight) {
    float yOffset = 0;

    for (auto& child : element->children) {
        if (child->computedStyle.display == StyleProperties::Display::None) continue;

        child->layout.x = element->layout.x;
        child->layout.y = element->layout.y + yOffset;

        if (child->computedStyle.widthAuto) {
            child->layout.width = containerWidth;
        } else {
            child->layout.width = child->computedStyle.width;
        }

        if (child->computedStyle.heightAuto) {
            // Auto height - will be computed from content
            child->layout.height = 20; // Default line height
        } else {
            child->layout.height = child->computedStyle.height;
        }

        yOffset += child->layout.height;

        // Recurse
        ComputeLayout(child.get(), child->layout.width, child->layout.height);
    }
}

void HTMLRenderer::RenderElement(DOMElement* element, float offsetX, float offsetY) {
    if (!element || !element->isVisible) return;
    if (element->computedStyle.display == StyleProperties::Display::None) return;
    if (element->computedStyle.visibility != StyleProperties::Visibility::Visible) return;

    float x = offsetX + element->layout.x;
    float y = offsetY + element->layout.y;
    float w = element->layout.width;
    float h = element->layout.height;

    // Apply opacity
    float opacity = element->computedStyle.opacity;

    // Draw background
    if (element->computedStyle.backgroundColor.a > 0) {
        Color bg = element->computedStyle.backgroundColor;
        bg.a = static_cast<uint8_t>(bg.a * opacity);
        if (element->computedStyle.borderRadius > 0) {
            DrawRoundedRect(x, y, w, h, element->computedStyle.borderRadius, bg);
        } else {
            DrawRect(x, y, w, h, bg);
        }
    }

    // Draw border
    if (element->computedStyle.borderWidth > 0 &&
        element->computedStyle.borderStyle != StyleProperties::BorderStyle::None) {
        DrawRectBorder(x, y, w, h, element->computedStyle.borderWidth, element->computedStyle.borderColor);
    }

    // Draw text content
    if (!element->textContent.empty() && element->tagName == "#text") {
        Color textColor = element->computedStyle.color;
        textColor.a = static_cast<uint8_t>(textColor.a * opacity);
        DrawText(element->textContent, x, y, GetFont("default"), textColor);
    }

    // Render children
    for (auto& child : element->children) {
        RenderElement(child.get(), x, y);
    }
}

void HTMLRenderer::ParseCSSProperty(const std::string& property, const std::string& value, StyleProperties& style) {
    // Handle CSS variable substitution
    std::string resolvedValue = value;
    if (value.find("var(--") != std::string::npos) {
        size_t start = value.find("var(--") + 6;
        size_t end = value.find(")", start);
        std::string varName = value.substr(start, end - start);
        resolvedValue = GetCSSVariable(varName);
    }

    if (property == "display") {
        if (resolvedValue == "none") style.display = StyleProperties::Display::None;
        else if (resolvedValue == "block") style.display = StyleProperties::Display::Block;
        else if (resolvedValue == "inline") style.display = StyleProperties::Display::Inline;
        else if (resolvedValue == "inline-block") style.display = StyleProperties::Display::InlineBlock;
        else if (resolvedValue == "flex") style.display = StyleProperties::Display::Flex;
        else if (resolvedValue == "grid") style.display = StyleProperties::Display::Grid;
    }
    else if (property == "position") {
        if (resolvedValue == "static") style.position = StyleProperties::Position::Static;
        else if (resolvedValue == "relative") style.position = StyleProperties::Position::Relative;
        else if (resolvedValue == "absolute") style.position = StyleProperties::Position::Absolute;
        else if (resolvedValue == "fixed") style.position = StyleProperties::Position::Fixed;
        else if (resolvedValue == "sticky") style.position = StyleProperties::Position::Sticky;
    }
    else if (property == "width") {
        style.width = ParseLength(resolvedValue);
        style.widthAuto = (resolvedValue == "auto");
    }
    else if (property == "height") {
        style.height = ParseLength(resolvedValue);
        style.heightAuto = (resolvedValue == "auto");
    }
    else if (property == "background-color" || property == "background") {
        style.backgroundColor = ParseColor(resolvedValue);
    }
    else if (property == "color") {
        style.color = ParseColor(resolvedValue);
    }
    else if (property == "border-radius") {
        style.borderRadius = ParseLength(resolvedValue);
    }
    else if (property == "border-width") {
        style.borderWidth = ParseLength(resolvedValue);
    }
    else if (property == "border-color") {
        style.borderColor = ParseColor(resolvedValue);
    }
    else if (property == "font-size") {
        style.fontSize = ParseLength(resolvedValue);
    }
    else if (property == "opacity") {
        style.opacity = std::stof(resolvedValue);
    }
    else if (property == "flex-direction") {
        if (resolvedValue == "row") style.flex.direction = FlexProperties::Direction::Row;
        else if (resolvedValue == "row-reverse") style.flex.direction = FlexProperties::Direction::RowReverse;
        else if (resolvedValue == "column") style.flex.direction = FlexProperties::Direction::Column;
        else if (resolvedValue == "column-reverse") style.flex.direction = FlexProperties::Direction::ColumnReverse;
    }
    else if (property == "justify-content") {
        if (resolvedValue == "flex-start") style.flex.justifyContent = FlexProperties::JustifyContent::FlexStart;
        else if (resolvedValue == "flex-end") style.flex.justifyContent = FlexProperties::JustifyContent::FlexEnd;
        else if (resolvedValue == "center") style.flex.justifyContent = FlexProperties::JustifyContent::Center;
        else if (resolvedValue == "space-between") style.flex.justifyContent = FlexProperties::JustifyContent::SpaceBetween;
        else if (resolvedValue == "space-around") style.flex.justifyContent = FlexProperties::JustifyContent::SpaceAround;
        else if (resolvedValue == "space-evenly") style.flex.justifyContent = FlexProperties::JustifyContent::SpaceEvenly;
    }
    else if (property == "align-items") {
        if (resolvedValue == "flex-start") style.flex.alignItems = FlexProperties::AlignItems::FlexStart;
        else if (resolvedValue == "flex-end") style.flex.alignItems = FlexProperties::AlignItems::FlexEnd;
        else if (resolvedValue == "center") style.flex.alignItems = FlexProperties::AlignItems::Center;
        else if (resolvedValue == "stretch") style.flex.alignItems = FlexProperties::AlignItems::Stretch;
        else if (resolvedValue == "baseline") style.flex.alignItems = FlexProperties::AlignItems::Baseline;
    }
    else if (property == "flex-grow") {
        style.flex.flexGrow = std::stof(resolvedValue);
    }
    else if (property == "flex-shrink") {
        style.flex.flexShrink = std::stof(resolvedValue);
    }
    else if (property == "z-index") {
        style.zIndex = std::stoi(resolvedValue);
        style.zIndexAuto = false;
    }
}

Color HTMLRenderer::ParseColor(const std::string& value) {
    if (value.empty()) return Color();

    if (value[0] == '#') {
        return Color::FromHex(value);
    }

    if (value.find("rgb") == 0) {
        // Parse rgb() or rgba()
        size_t start = value.find('(') + 1;
        size_t end = value.find(')');
        std::string params = value.substr(start, end - start);

        std::vector<float> values;
        std::istringstream iss(params);
        std::string token;
        while (std::getline(iss, token, ',')) {
            values.push_back(std::stof(token));
        }

        if (values.size() >= 3) {
            int r = static_cast<int>(values[0]);
            int g = static_cast<int>(values[1]);
            int b = static_cast<int>(values[2]);
            float a = values.size() >= 4 ? values[3] : 1.0f;
            return Color::FromRGBA(r, g, b, a);
        }
    }

    // Named colors
    static const std::unordered_map<std::string, Color> namedColors = {
        {"black", Color(0, 0, 0)},
        {"white", Color(255, 255, 255)},
        {"red", Color(255, 0, 0)},
        {"green", Color(0, 128, 0)},
        {"blue", Color(0, 0, 255)},
        {"yellow", Color(255, 255, 0)},
        {"cyan", Color(0, 255, 255)},
        {"magenta", Color(255, 0, 255)},
        {"gray", Color(128, 128, 128)},
        {"grey", Color(128, 128, 128)},
        {"transparent", Color(0, 0, 0, 0)},
    };

    auto it = namedColors.find(value);
    if (it != namedColors.end()) {
        return it->second;
    }

    return Color();
}

float HTMLRenderer::ParseLength(const std::string& value, float parentValue) {
    if (value.empty() || value == "auto") return 0;

    float numValue = std::stof(value);

    if (value.find("px") != std::string::npos) {
        return numValue;
    } else if (value.find("em") != std::string::npos) {
        return numValue * 16; // Base font size
    } else if (value.find("rem") != std::string::npos) {
        return numValue * 16;
    } else if (value.find("%") != std::string::npos) {
        return parentValue * (numValue / 100.0f);
    } else if (value.find("vh") != std::string::npos) {
        return m_height * (numValue / 100.0f);
    } else if (value.find("vw") != std::string::npos) {
        return m_width * (numValue / 100.0f);
    }

    return numValue;
}

bool HTMLRenderer::LoadFont(const std::string& name, const std::string& path, float size) {
    auto font = std::make_unique<Font>();
    font->name = name;
    font->size = size;
    font->lineHeight = size * 1.2f;
    font->ascender = size * 0.8f;
    font->descender = size * 0.2f;

    // Generate basic ASCII glyphs
    for (uint32_t c = 32; c < 127; ++c) {
        Glyph glyph;
        glyph.width = size * 0.6f;
        glyph.height = size;
        glyph.advance = size * 0.6f;
        glyph.bearingX = 0;
        glyph.bearingY = size * 0.8f;
        font->glyphs[c] = glyph;
    }

    m_fonts[name] = std::move(font);
    return true;
}

Font* HTMLRenderer::GetFont(const std::string& name) {
    auto it = m_fonts.find(name);
    if (it != m_fonts.end()) {
        return it->second.get();
    }
    return m_fonts["default"].get();
}

void HTMLRenderer::MeasureText(const std::string& text, Font* font, float& width, float& height) {
    if (!font) {
        width = 0;
        height = 0;
        return;
    }

    width = 0;
    height = font->lineHeight;

    for (char c : text) {
        auto it = font->glyphs.find(static_cast<uint32_t>(c));
        if (it != font->glyphs.end()) {
            width += it->second.advance;
        }
    }
}

uint32_t HTMLRenderer::LoadTexture(const std::string& path) {
    // Simplified - in production would load actual image
    Texture tex;
    tex.id = m_nextTextureId++;
    tex.width = 64;
    tex.height = 64;
    tex.data.resize(tex.width * tex.height * 4, 255);

    m_textures[tex.id] = tex;
    m_textureMemory += tex.data.size();

    return tex.id;
}

uint32_t HTMLRenderer::CreateTexture(int width, int height, const uint8_t* data) {
    Texture tex;
    tex.id = m_nextTextureId++;
    tex.width = width;
    tex.height = height;
    tex.data.assign(data, data + width * height * 4);

    m_textures[tex.id] = tex;
    m_textureMemory += tex.data.size();

    return tex.id;
}

void HTMLRenderer::DeleteTexture(uint32_t id) {
    auto it = m_textures.find(id);
    if (it != m_textures.end()) {
        m_textureMemory -= it->second.data.size();
        m_textures.erase(it);
    }
}

bool HTMLRenderer::GetTextureInfo(uint32_t id, int& width, int& height) {
    auto it = m_textures.find(id);
    if (it != m_textures.end()) {
        width = it->second.width;
        height = it->second.height;
        return true;
    }
    return false;
}

std::unique_ptr<Canvas2DContext> HTMLRenderer::CreateCanvas2D(int width, int height) {
    return std::make_unique<Canvas2DContext>(width, height);
}

uint32_t HTMLRenderer::RenderCanvasToTexture(Canvas2DContext* canvas) {
    // Create texture from canvas draw commands
    // Simplified implementation
    return CreateTexture(64, 64, nullptr);
}

void HTMLRenderer::DrawRect(float x, float y, float width, float height, const Color& color) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Quad;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.color = color;
    cmd.rotation = m_rotation;
    cmd.scaleX = m_scaleX; cmd.scaleY = m_scaleY;
    cmd.originX = m_originX; cmd.originY = m_originY;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::DrawRoundedRect(float x, float y, float width, float height, float radius, const Color& color) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Quad;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.borderRadius = radius;
    cmd.color = color;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::DrawRectBorder(float x, float y, float width, float height, float borderWidth, const Color& color) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Quad;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.color = Color(0, 0, 0, 0);
    cmd.borderColor = color;
    cmd.borderWidth = borderWidth;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::DrawText(const std::string& text, float x, float y, Font* font, const Color& color) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Text;
    cmd.text = text;
    cmd.x = x; cmd.y = y;
    cmd.font = font;
    cmd.color = color;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::DrawImage(uint32_t textureId, float x, float y, float width, float height, float opacity) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Image;
    cmd.textureId = textureId;
    cmd.x = x; cmd.y = y;
    cmd.width = width; cmd.height = height;
    cmd.opacity = opacity;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::DrawImageRect(uint32_t textureId, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Image;
    cmd.textureId = textureId;
    cmd.texX = sx; cmd.texY = sy;
    cmd.texWidth = sw; cmd.texHeight = sh;
    cmd.x = dx; cmd.y = dy;
    cmd.width = dw; cmd.height = dh;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::DrawLine(float x1, float y1, float x2, float y2, float width, const Color& color) {
    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Line;
    cmd.x = x1; cmd.y = y1;
    cmd.width = x2 - x1; cmd.height = y2 - y1;
    cmd.borderWidth = width;
    cmd.color = color;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::PushClipRect(float x, float y, float width, float height) {
    m_clipStack.push_back({x, y, width, height});

    DrawCommand cmd;
    cmd.type = DrawCommand::Type::Clip;
    cmd.clipX = x; cmd.clipY = y;
    cmd.clipWidth = width; cmd.clipHeight = height;
    m_drawCommands.push_back(cmd);
}

void HTMLRenderer::PopClipRect() {
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();

        DrawCommand cmd;
        cmd.type = DrawCommand::Type::PopClip;
        m_drawCommands.push_back(cmd);
    }
}

void HTMLRenderer::SetTransform(float rotation, float scaleX, float scaleY, float originX, float originY) {
    m_rotation = rotation;
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    m_originX = originX;
    m_originY = originY;
}

void HTMLRenderer::ResetTransform() {
    m_rotation = 0;
    m_scaleX = 1;
    m_scaleY = 1;
    m_originX = 0;
    m_originY = 0;
}

void HTMLRenderer::GetStats(int& drawCalls, int& triangles, size_t& textureMemory) const {
    drawCalls = m_drawCalls;
    triangles = m_triangles;
    textureMemory = m_textureMemory;
}

void HTMLRenderer::FlushDrawCommands() {
    // In production, this would batch and submit to GPU
    m_drawCalls = static_cast<int>(m_drawCommands.size());
    m_triangles = m_drawCalls * 2; // 2 triangles per quad

    m_drawCommands.clear();
}

} // namespace UI
} // namespace Engine
