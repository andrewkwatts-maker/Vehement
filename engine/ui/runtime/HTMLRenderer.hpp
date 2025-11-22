#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Engine {
namespace UI {

// Forward declarations
class UIWindow;

/**
 * @brief CSS Box model
 */
struct BoxModel {
    float marginTop = 0, marginRight = 0, marginBottom = 0, marginLeft = 0;
    float paddingTop = 0, paddingRight = 0, paddingBottom = 0, paddingLeft = 0;
    float borderTop = 0, borderRight = 0, borderBottom = 0, borderLeft = 0;
    float width = 0, height = 0;
    float x = 0, y = 0;
};

/**
 * @brief CSS Flexbox properties
 */
struct FlexProperties {
    enum class Direction { Row, RowReverse, Column, ColumnReverse };
    enum class Wrap { NoWrap, Wrap, WrapReverse };
    enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
    enum class AlignItems { FlexStart, FlexEnd, Center, Stretch, Baseline };
    enum class AlignContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, Stretch };

    Direction direction = Direction::Row;
    Wrap wrap = Wrap::NoWrap;
    JustifyContent justifyContent = JustifyContent::FlexStart;
    AlignItems alignItems = AlignItems::Stretch;
    AlignContent alignContent = AlignContent::Stretch;
    float flexGrow = 0;
    float flexShrink = 1;
    float flexBasis = 0;
    bool isFlexBasisAuto = true;
    int order = 0;
    AlignItems alignSelf = AlignItems::Stretch;
    bool alignSelfAuto = true;
};

/**
 * @brief CSS Color
 */
struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    Color() = default;
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

    static Color FromHex(const std::string& hex);
    static Color FromRGB(int r, int g, int b);
    static Color FromRGBA(int r, int g, int b, float a);
    std::string ToHex() const;
};

/**
 * @brief CSS Style properties
 */
struct StyleProperties {
    // Display
    enum class Display { None, Block, Inline, InlineBlock, Flex, Grid };
    Display display = Display::Block;

    // Position
    enum class Position { Static, Relative, Absolute, Fixed, Sticky };
    Position position = Position::Static;
    float top = 0, right = 0, bottom = 0, left = 0;
    bool topAuto = true, rightAuto = true, bottomAuto = true, leftAuto = true;

    // Size
    float width = 0, height = 0;
    float minWidth = 0, minHeight = 0;
    float maxWidth = 0, maxHeight = 0;
    bool widthAuto = true, heightAuto = true;
    bool maxWidthNone = true, maxHeightNone = true;

    // Colors
    Color backgroundColor{0, 0, 0, 0};
    Color color{255, 255, 255, 255};
    Color borderColor{128, 128, 128, 255};

    // Border
    float borderRadius = 0;
    float borderWidth = 0;
    enum class BorderStyle { None, Solid, Dashed, Dotted };
    BorderStyle borderStyle = BorderStyle::None;

    // Font
    std::string fontFamily = "sans-serif";
    float fontSize = 16;
    enum class FontWeight { Normal, Bold, Lighter, Bolder };
    FontWeight fontWeight = FontWeight::Normal;
    enum class FontStyle { Normal, Italic, Oblique };
    FontStyle fontStyle = FontStyle::Normal;

    // Text
    enum class TextAlign { Left, Right, Center, Justify };
    TextAlign textAlign = TextAlign::Left;
    float lineHeight = 1.2f;
    enum class TextDecoration { None, Underline, Overline, LineThrough };
    TextDecoration textDecoration = TextDecoration::None;
    enum class TextOverflow { Clip, Ellipsis };
    TextOverflow textOverflow = TextOverflow::Clip;
    enum class WhiteSpace { Normal, NoWrap, Pre, PreWrap, PreLine };
    WhiteSpace whiteSpace = WhiteSpace::Normal;

    // Overflow
    enum class Overflow { Visible, Hidden, Scroll, Auto };
    Overflow overflow = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;
    Overflow overflowY = Overflow::Visible;

    // Opacity and visibility
    float opacity = 1.0f;
    enum class Visibility { Visible, Hidden, Collapse };
    Visibility visibility = Visibility::Visible;

    // Cursor
    enum class Cursor { Default, Pointer, Text, Move, NotAllowed, Grab, Grabbing };
    Cursor cursor = Cursor::Default;

    // Transform
    float transformRotate = 0;
    float transformScaleX = 1, transformScaleY = 1;
    float transformTranslateX = 0, transformTranslateY = 0;
    float transformOriginX = 0.5f, transformOriginY = 0.5f;

    // Box shadow
    bool boxShadow = false;
    float shadowOffsetX = 0, shadowOffsetY = 0;
    float shadowBlur = 0, shadowSpread = 0;
    Color shadowColor{0, 0, 0, 128};

    // Transition
    std::string transitionProperty = "all";
    float transitionDuration = 0;
    std::string transitionTimingFunction = "ease";
    float transitionDelay = 0;

    // Z-index
    int zIndex = 0;
    bool zIndexAuto = true;

    // Flexbox
    FlexProperties flex;

    // Box model
    BoxModel box;

    // Pointer events
    enum class PointerEvents { Auto, None };
    PointerEvents pointerEvents = PointerEvents::Auto;
};

/**
 * @brief DOM Element representation
 */
struct DOMElement {
    std::string tagName;
    std::string id;
    std::vector<std::string> classes;
    std::unordered_map<std::string, std::string> attributes;
    std::string textContent;
    std::string innerHTML;

    StyleProperties computedStyle;
    BoxModel layout;

    DOMElement* parent = nullptr;
    std::vector<std::unique_ptr<DOMElement>> children;

    // State
    bool isHovered = false;
    bool isFocused = false;
    bool isActive = false;
    bool isVisible = true;

    // Event handlers
    std::unordered_map<std::string, std::function<void(const std::string&)>> eventHandlers;

    DOMElement* FindById(const std::string& id);
    std::vector<DOMElement*> FindByClass(const std::string& className);
    std::vector<DOMElement*> FindByTagName(const std::string& tagName);
    DOMElement* QuerySelector(const std::string& selector);
    std::vector<DOMElement*> QuerySelectorAll(const std::string& selector);

    void SetAttribute(const std::string& name, const std::string& value);
    std::string GetAttribute(const std::string& name) const;
    void AddClass(const std::string& className);
    void RemoveClass(const std::string& className);
    bool HasClass(const std::string& className) const;
    void ToggleClass(const std::string& className);
};

/**
 * @brief CSS Rule
 */
struct CSSRule {
    std::string selector;
    StyleProperties properties;
    int specificity = 0;
};

/**
 * @brief Texture for rendering
 */
struct Texture {
    uint32_t id = 0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
};

/**
 * @brief Font glyph data
 */
struct Glyph {
    float advance = 0;
    float bearingX = 0, bearingY = 0;
    float width = 0, height = 0;
    float texX = 0, texY = 0, texWidth = 0, texHeight = 0;
};

/**
 * @brief Font data
 */
struct Font {
    std::string name;
    float size = 16;
    uint32_t textureId = 0;
    int textureWidth = 0, textureHeight = 0;
    std::unordered_map<uint32_t, Glyph> glyphs;
    float lineHeight = 0;
    float ascender = 0, descender = 0;
};

/**
 * @brief Draw command for batched rendering
 */
struct DrawCommand {
    enum class Type { Quad, Text, Line, Image, Clip, PopClip };
    Type type;

    // Geometry
    float x = 0, y = 0, width = 0, height = 0;
    float borderRadius = 0;

    // Color
    Color color;
    Color borderColor;
    float borderWidth = 0;

    // Text specific
    std::string text;
    Font* font = nullptr;

    // Image specific
    uint32_t textureId = 0;
    float texX = 0, texY = 0, texWidth = 1, texHeight = 1;

    // Transform
    float rotation = 0;
    float scaleX = 1, scaleY = 1;
    float originX = 0, originY = 0;

    // Opacity
    float opacity = 1.0f;

    // Clip rect
    float clipX = 0, clipY = 0, clipWidth = 0, clipHeight = 0;
};

/**
 * @brief Canvas 2D context for JavaScript canvas API
 */
class Canvas2DContext {
public:
    Canvas2DContext(int width, int height);
    ~Canvas2DContext();

    // State
    void Save();
    void Restore();

    // Transform
    void Scale(float x, float y);
    void Rotate(float angle);
    void Translate(float x, float y);
    void Transform(float a, float b, float c, float d, float e, float f);
    void SetTransform(float a, float b, float c, float d, float e, float f);
    void ResetTransform();

    // Compositing
    void SetGlobalAlpha(float alpha);
    void SetGlobalCompositeOperation(const std::string& op);

    // Colors and styles
    void SetFillStyle(const Color& color);
    void SetStrokeStyle(const Color& color);

    // Line styles
    void SetLineWidth(float width);
    void SetLineCap(const std::string& cap);
    void SetLineJoin(const std::string& join);
    void SetMiterLimit(float limit);

    // Text
    void SetFont(const std::string& font);
    void SetTextAlign(const std::string& align);
    void SetTextBaseline(const std::string& baseline);
    void FillText(const std::string& text, float x, float y, float maxWidth = -1);
    void StrokeText(const std::string& text, float x, float y, float maxWidth = -1);
    float MeasureText(const std::string& text);

    // Rectangles
    void ClearRect(float x, float y, float width, float height);
    void FillRect(float x, float y, float width, float height);
    void StrokeRect(float x, float y, float width, float height);

    // Paths
    void BeginPath();
    void ClosePath();
    void MoveTo(float x, float y);
    void LineTo(float x, float y);
    void BezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
    void QuadraticCurveTo(float cpx, float cpy, float x, float y);
    void Arc(float x, float y, float radius, float startAngle, float endAngle, bool counterclockwise = false);
    void ArcTo(float x1, float y1, float x2, float y2, float radius);
    void Ellipse(float x, float y, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool counterclockwise = false);
    void Rect(float x, float y, float width, float height);
    void RoundRect(float x, float y, float width, float height, float radius);
    void Fill();
    void Stroke();
    void Clip();

    // Images
    void DrawImage(uint32_t textureId, float x, float y);
    void DrawImage(uint32_t textureId, float x, float y, float width, float height);
    void DrawImage(uint32_t textureId, float sx, float sy, float sWidth, float sHeight, float dx, float dy, float dWidth, float dHeight);

    // Pixel manipulation
    std::vector<uint8_t> GetImageData(float x, float y, float width, float height);
    void PutImageData(const std::vector<uint8_t>& data, float x, float y);

    // Get draw commands
    const std::vector<DrawCommand>& GetDrawCommands() const { return m_drawCommands; }
    void ClearDrawCommands() { m_drawCommands.clear(); }

private:
    struct State {
        float globalAlpha = 1.0f;
        Color fillStyle{0, 0, 0, 255};
        Color strokeStyle{0, 0, 0, 255};
        float lineWidth = 1.0f;
        float transform[6] = {1, 0, 0, 1, 0, 0};
    };

    int m_width, m_height;
    std::vector<State> m_stateStack;
    State m_currentState;
    std::vector<DrawCommand> m_drawCommands;

    // Path data
    struct PathPoint { float x, y; enum Type { Move, Line, Curve } type; };
    std::vector<PathPoint> m_path;
};

/**
 * @brief HTML Renderer for UI
 *
 * Provides lightweight HTML/CSS parsing, layout engine with flexbox subset,
 * CSS styling, canvas 2D rendering, and font/image support.
 */
class HTMLRenderer {
public:
    HTMLRenderer();
    ~HTMLRenderer();

    /**
     * @brief Initialize the renderer
     */
    bool Initialize(int width, int height, float dpiScale);

    /**
     * @brief Shutdown the renderer
     */
    void Shutdown();

    /**
     * @brief Resize the render target
     */
    void Resize(int width, int height);

    /**
     * @brief Set DPI scale
     */
    void SetDPIScale(float scale);

    /**
     * @brief Set viewport mode
     */
    void SetViewportMode(bool fullscreen);

    /**
     * @brief Begin a new frame
     */
    void BeginFrame();

    /**
     * @brief End frame and present
     */
    void EndFrame();

    /**
     * @brief Render a UI window
     */
    void RenderWindow(UIWindow* window);

    /**
     * @brief Render debug overlay
     */
    void RenderDebugOverlay(const std::vector<UIWindow*>& windows);

    // HTML/CSS Loading

    /**
     * @brief Parse HTML string
     */
    std::unique_ptr<DOMElement> ParseHTML(const std::string& html);

    /**
     * @brief Parse CSS string
     */
    std::vector<CSSRule> ParseCSS(const std::string& css);

    /**
     * @brief Load global CSS file
     */
    void LoadGlobalCSS(const std::string& path);

    /**
     * @brief Set CSS variable
     */
    void SetCSSVariable(const std::string& name, const std::string& value);

    /**
     * @brief Get CSS variable
     */
    std::string GetCSSVariable(const std::string& name) const;

    // Layout Engine

    /**
     * @brief Compute layout for DOM tree
     */
    void ComputeLayout(DOMElement* root, float containerWidth, float containerHeight);

    /**
     * @brief Compute styles for DOM tree
     */
    void ComputeStyles(DOMElement* root, const std::vector<CSSRule>& rules);

    // Font Management

    /**
     * @brief Load a font
     */
    bool LoadFont(const std::string& name, const std::string& path, float size);

    /**
     * @brief Get font by name
     */
    Font* GetFont(const std::string& name);

    /**
     * @brief Measure text dimensions
     */
    void MeasureText(const std::string& text, Font* font, float& width, float& height);

    // Texture Management

    /**
     * @brief Load texture from file
     */
    uint32_t LoadTexture(const std::string& path);

    /**
     * @brief Create texture from data
     */
    uint32_t CreateTexture(int width, int height, const uint8_t* data);

    /**
     * @brief Delete texture
     */
    void DeleteTexture(uint32_t id);

    /**
     * @brief Get texture info
     */
    bool GetTextureInfo(uint32_t id, int& width, int& height);

    // Canvas 2D

    /**
     * @brief Create a canvas 2D context
     */
    std::unique_ptr<Canvas2DContext> CreateCanvas2D(int width, int height);

    /**
     * @brief Render canvas to texture
     */
    uint32_t RenderCanvasToTexture(Canvas2DContext* canvas);

    // Drawing API (low-level)

    /**
     * @brief Draw a filled rectangle
     */
    void DrawRect(float x, float y, float width, float height, const Color& color);

    /**
     * @brief Draw a rounded rectangle
     */
    void DrawRoundedRect(float x, float y, float width, float height, float radius, const Color& color);

    /**
     * @brief Draw rectangle border
     */
    void DrawRectBorder(float x, float y, float width, float height, float borderWidth, const Color& color);

    /**
     * @brief Draw text
     */
    void DrawText(const std::string& text, float x, float y, Font* font, const Color& color);

    /**
     * @brief Draw image/texture
     */
    void DrawImage(uint32_t textureId, float x, float y, float width, float height, float opacity = 1.0f);

    /**
     * @brief Draw image with source rect
     */
    void DrawImageRect(uint32_t textureId, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh);

    /**
     * @brief Draw a line
     */
    void DrawLine(float x1, float y1, float x2, float y2, float width, const Color& color);

    /**
     * @brief Push clip rect
     */
    void PushClipRect(float x, float y, float width, float height);

    /**
     * @brief Pop clip rect
     */
    void PopClipRect();

    /**
     * @brief Set transform
     */
    void SetTransform(float rotation, float scaleX, float scaleY, float originX, float originY);

    /**
     * @brief Reset transform
     */
    void ResetTransform();

    /**
     * @brief Get render statistics
     */
    void GetStats(int& drawCalls, int& triangles, size_t& textureMemory) const;

private:
    void ParseHTMLNode(const std::string& html, size_t& pos, DOMElement* parent);
    void ParseCSSProperty(const std::string& property, const std::string& value, StyleProperties& style);
    Color ParseColor(const std::string& value);
    float ParseLength(const std::string& value, float parentValue = 0);
    void ComputeFlexLayout(DOMElement* element, float containerWidth, float containerHeight);
    void ComputeBlockLayout(DOMElement* element, float containerWidth, float containerHeight);
    void RenderElement(DOMElement* element, float offsetX, float offsetY);
    void FlushDrawCommands();

    int m_width = 0;
    int m_height = 0;
    float m_dpiScale = 1.0f;
    bool m_fullscreen = false;

    std::vector<CSSRule> m_globalStyles;
    std::unordered_map<std::string, std::string> m_cssVariables;
    std::unordered_map<std::string, std::unique_ptr<Font>> m_fonts;
    std::unordered_map<uint32_t, Texture> m_textures;
    uint32_t m_nextTextureId = 1;

    std::vector<DrawCommand> m_drawCommands;
    std::vector<std::array<float, 4>> m_clipStack;

    // Stats
    int m_drawCalls = 0;
    int m_triangles = 0;
    size_t m_textureMemory = 0;

    // Current transform
    float m_rotation = 0;
    float m_scaleX = 1, m_scaleY = 1;
    float m_originX = 0, m_originY = 0;
};

} // namespace UI
} // namespace Engine
