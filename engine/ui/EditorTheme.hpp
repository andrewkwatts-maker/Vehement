#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <functional>
#include <imgui.h>

namespace Nova {

/**
 * @brief Unified theme system for all editor UI components
 *
 * Provides consistent styling across:
 * - Node editors (Shader, Animation, Event)
 * - Property panels
 * - Browsers (Asset, Content, Animation)
 * - Inspectors
 * - Toolbars
 * - Status bars
 */
class EditorTheme {
public:
    // =========================================================================
    // Color Categories
    // =========================================================================

    struct Colors {
        // Base colors
        glm::vec4 background{0.10f, 0.10f, 0.12f, 1.0f};
        glm::vec4 backgroundAlt{0.12f, 0.12f, 0.15f, 1.0f};
        glm::vec4 panel{0.15f, 0.15f, 0.18f, 1.0f};
        glm::vec4 panelHeader{0.18f, 0.18f, 0.22f, 1.0f};

        // Window chrome
        glm::vec4 titleBar{0.12f, 0.12f, 0.15f, 1.0f};
        glm::vec4 titleBarActive{0.16f, 0.16f, 0.20f, 1.0f};
        glm::vec4 border{0.25f, 0.25f, 0.30f, 1.0f};
        glm::vec4 borderHighlight{0.35f, 0.35f, 0.40f, 1.0f};

        // Controls
        glm::vec4 button{0.22f, 0.22f, 0.26f, 1.0f};
        glm::vec4 buttonHovered{0.30f, 0.30f, 0.35f, 1.0f};
        glm::vec4 buttonActive{0.35f, 0.35f, 0.40f, 1.0f};
        glm::vec4 buttonDisabled{0.18f, 0.18f, 0.20f, 0.5f};

        glm::vec4 input{0.16f, 0.16f, 0.19f, 1.0f};
        glm::vec4 inputHovered{0.20f, 0.20f, 0.24f, 1.0f};
        glm::vec4 inputActive{0.22f, 0.22f, 0.26f, 1.0f};

        glm::vec4 slider{0.30f, 0.30f, 0.35f, 1.0f};
        glm::vec4 sliderActive{0.40f, 0.60f, 1.0f, 1.0f};

        glm::vec4 checkbox{0.25f, 0.25f, 0.30f, 1.0f};
        glm::vec4 checkmark{0.40f, 0.60f, 1.0f, 1.0f};

        // Headers & Tabs
        glm::vec4 header{0.20f, 0.20f, 0.24f, 1.0f};
        glm::vec4 headerHovered{0.28f, 0.28f, 0.32f, 1.0f};
        glm::vec4 headerActive{0.32f, 0.32f, 0.36f, 1.0f};

        glm::vec4 tab{0.14f, 0.14f, 0.17f, 1.0f};
        glm::vec4 tabHovered{0.24f, 0.24f, 0.28f, 1.0f};
        glm::vec4 tabActive{0.22f, 0.22f, 0.26f, 1.0f};
        glm::vec4 tabUnfocused{0.12f, 0.12f, 0.14f, 1.0f};

        // Selection
        glm::vec4 selection{0.30f, 0.50f, 0.80f, 0.40f};
        glm::vec4 selectionInactive{0.30f, 0.30f, 0.35f, 0.40f};
        glm::vec4 highlight{0.40f, 0.60f, 1.0f, 0.20f};

        // Text
        glm::vec4 text{0.92f, 0.92f, 0.94f, 1.0f};
        glm::vec4 textSecondary{0.70f, 0.70f, 0.72f, 1.0f};
        glm::vec4 textDisabled{0.45f, 0.45f, 0.48f, 1.0f};
        glm::vec4 textHighlight{1.0f, 1.0f, 1.0f, 1.0f};

        // Accent colors
        glm::vec4 accent{0.40f, 0.60f, 1.0f, 1.0f};
        glm::vec4 accentHovered{0.50f, 0.70f, 1.0f, 1.0f};
        glm::vec4 accentActive{0.60f, 0.75f, 1.0f, 1.0f};

        // Status colors
        glm::vec4 success{0.30f, 0.75f, 0.40f, 1.0f};
        glm::vec4 warning{0.95f, 0.75f, 0.25f, 1.0f};
        glm::vec4 error{0.90f, 0.35f, 0.35f, 1.0f};
        glm::vec4 info{0.40f, 0.70f, 0.95f, 1.0f};

        // Node Editor specific
        glm::vec4 nodeBackground{0.18f, 0.18f, 0.22f, 0.95f};
        glm::vec4 nodeHeader{0.25f, 0.25f, 0.30f, 1.0f};
        glm::vec4 nodeBorder{0.35f, 0.35f, 0.40f, 1.0f};
        glm::vec4 nodeSelected{0.45f, 0.65f, 1.0f, 1.0f};
        glm::vec4 nodeGrid{0.20f, 0.20f, 0.24f, 0.5f};
        glm::vec4 nodeGridMajor{0.25f, 0.25f, 0.30f, 0.8f};
        glm::vec4 connectionLine{0.60f, 0.60f, 0.65f, 1.0f};
        glm::vec4 connectionLineActive{0.50f, 0.70f, 1.0f, 1.0f};

        // Pin type colors (for node editors)
        glm::vec4 pinFloat{0.50f, 0.80f, 0.50f, 1.0f};
        glm::vec4 pinInt{0.30f, 0.70f, 0.90f, 1.0f};
        glm::vec4 pinBool{0.90f, 0.40f, 0.40f, 1.0f};
        glm::vec4 pinString{0.90f, 0.60f, 0.90f, 1.0f};
        glm::vec4 pinVector{0.90f, 0.90f, 0.40f, 1.0f};
        glm::vec4 pinColor{0.90f, 0.50f, 0.20f, 1.0f};
        glm::vec4 pinTexture{0.70f, 0.50f, 0.90f, 1.0f};
        glm::vec4 pinEvent{0.95f, 0.95f, 0.95f, 1.0f};
        glm::vec4 pinExec{0.95f, 0.95f, 0.95f, 1.0f};
        glm::vec4 pinObject{0.40f, 0.80f, 0.80f, 1.0f};

        // Scrollbar
        glm::vec4 scrollbarBg{0.12f, 0.12f, 0.14f, 1.0f};
        glm::vec4 scrollbarGrab{0.30f, 0.30f, 0.35f, 1.0f};
        glm::vec4 scrollbarGrabHovered{0.38f, 0.38f, 0.42f, 1.0f};
        glm::vec4 scrollbarGrabActive{0.45f, 0.45f, 0.50f, 1.0f};

        // Separator
        glm::vec4 separator{0.25f, 0.25f, 0.28f, 1.0f};
        glm::vec4 separatorHovered{0.40f, 0.55f, 0.90f, 1.0f};
        glm::vec4 separatorActive{0.50f, 0.65f, 1.0f, 1.0f};

        // Resize grip
        glm::vec4 resizeGrip{0.25f, 0.25f, 0.28f, 0.0f};
        glm::vec4 resizeGripHovered{0.40f, 0.55f, 0.90f, 0.67f};
        glm::vec4 resizeGripActive{0.50f, 0.65f, 1.0f, 0.95f};

        // Progress bar
        glm::vec4 progressBarBg{0.18f, 0.18f, 0.22f, 1.0f};
        glm::vec4 progressBar{0.40f, 0.60f, 1.0f, 1.0f};

        // Tooltip
        glm::vec4 tooltipBg{0.08f, 0.08f, 0.10f, 0.95f};
        glm::vec4 tooltipBorder{0.30f, 0.30f, 0.35f, 1.0f};

        // Modal overlay
        glm::vec4 modalDim{0.0f, 0.0f, 0.0f, 0.60f};

        // Drag drop
        glm::vec4 dragDropTarget{0.50f, 0.70f, 1.0f, 0.30f};
    };

    // =========================================================================
    // Size/Spacing Constants
    // =========================================================================

    struct Sizes {
        // Rounding
        float windowRounding = 6.0f;
        float childRounding = 4.0f;
        float frameRounding = 3.0f;
        float popupRounding = 4.0f;
        float scrollbarRounding = 6.0f;
        float grabRounding = 2.0f;
        float tabRounding = 4.0f;

        // Borders
        float windowBorderSize = 1.0f;
        float childBorderSize = 1.0f;
        float frameBorderSize = 0.0f;
        float popupBorderSize = 1.0f;
        float tabBorderSize = 0.0f;

        // Padding
        glm::vec2 windowPadding{10.0f, 10.0f};
        glm::vec2 framePadding{6.0f, 4.0f};
        glm::vec2 cellPadding{4.0f, 2.0f};
        glm::vec2 itemSpacing{8.0f, 5.0f};
        glm::vec2 itemInnerSpacing{5.0f, 5.0f};
        glm::vec2 touchExtraPadding{0.0f, 0.0f};

        // Indentation
        float indentSpacing = 20.0f;
        float columnsMinSpacing = 6.0f;

        // Scrollbar
        float scrollbarSize = 14.0f;
        float grabMinSize = 10.0f;

        // Widgets
        float buttonTextAlign = 0.5f;
        float selectableTextAlign = 0.0f;

        // Node editor
        float nodeRounding = 8.0f;
        float nodePadding = 8.0f;
        float pinRadius = 6.0f;
        float pinSpacing = 4.0f;
        float linkThickness = 3.0f;
        float linkCurvature = 0.5f;
        float gridSpacing = 32.0f;

        // Toolbar
        float toolbarHeight = 40.0f;
        float toolbarButtonSize = 32.0f;
        float toolbarButtonSpacing = 4.0f;

        // Status bar
        float statusBarHeight = 24.0f;

        // Panel header
        float panelHeaderHeight = 28.0f;

        // Tree view
        float treeIndent = 16.0f;
        float treeRowHeight = 22.0f;

        // Property editor
        float propertyLabelWidth = 120.0f;
        float propertyIndent = 16.0f;
    };

    // =========================================================================
    // Font Configuration
    // =========================================================================

    struct Fonts {
        float defaultSize = 14.0f;
        float smallSize = 12.0f;
        float largeSize = 16.0f;
        float headerSize = 18.0f;
        float titleSize = 20.0f;
        float monoSize = 13.0f;
        float iconSize = 16.0f;

        std::string defaultFont = "Roboto-Regular";
        std::string boldFont = "Roboto-Bold";
        std::string italicFont = "Roboto-Italic";
        std::string monoFont = "JetBrainsMono-Regular";
        std::string iconFont = "FontAwesome";
    };

    // =========================================================================
    // Public Interface
    // =========================================================================

    static EditorTheme& Instance();

    /**
     * @brief Apply theme to ImGui
     */
    void Apply();

    /**
     * @brief Load theme from file
     */
    bool Load(const std::string& path);

    /**
     * @brief Save theme to file
     */
    bool Save(const std::string& path) const;

    /**
     * @brief Reset to default theme
     */
    void ResetToDefault();

    /**
     * @brief Set a preset theme
     */
    void SetPreset(const std::string& presetName);

    /**
     * @brief Get available preset names
     */
    std::vector<std::string> GetPresetNames() const;

    // Accessors
    Colors& GetColors() { return m_colors; }
    const Colors& GetColors() const { return m_colors; }

    Sizes& GetSizes() { return m_sizes; }
    const Sizes& GetSizes() const { return m_sizes; }

    Fonts& GetFonts() { return m_fonts; }
    const Fonts& GetFonts() const { return m_fonts; }

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Convert glm::vec4 to ImVec4
     */
    static ImVec4 ToImVec4(const glm::vec4& v) { return ImVec4(v.x, v.y, v.z, v.w); }

    /**
     * @brief Convert ImVec4 to glm::vec4
     */
    static glm::vec4 FromImVec4(const ImVec4& v) { return glm::vec4(v.x, v.y, v.z, v.w); }

    /**
     * @brief Convert glm::vec4 to ImU32
     */
    static ImU32 ToImU32(const glm::vec4& v);

    /**
     * @brief Get color for pin type
     */
    glm::vec4 GetPinColor(const std::string& typeName) const;

    /**
     * @brief Get color for node category
     */
    glm::vec4 GetNodeCategoryColor(const std::string& category) const;

    /**
     * @brief Interpolate between two colors
     */
    static glm::vec4 Lerp(const glm::vec4& a, const glm::vec4& b, float t);

    /**
     * @brief Adjust color brightness
     */
    static glm::vec4 AdjustBrightness(const glm::vec4& color, float factor);

    /**
     * @brief Adjust color saturation
     */
    static glm::vec4 AdjustSaturation(const glm::vec4& color, float factor);

private:
    EditorTheme() = default;

    void RegisterDefaultPresets();

    Colors m_colors;
    Sizes m_sizes;
    Fonts m_fonts;

    std::unordered_map<std::string, std::function<void()>> m_presets;
    std::unordered_map<std::string, glm::vec4> m_pinColors;
    std::unordered_map<std::string, glm::vec4> m_nodeCategoryColors;
};

// ============================================================================
// Scoped Style Helpers
// ============================================================================

/**
 * @brief RAII helper for pushing/popping ImGui style colors
 */
class ScopedStyleColor {
public:
    ScopedStyleColor(ImGuiCol idx, const glm::vec4& color);
    ScopedStyleColor(ImGuiCol idx, const ImVec4& color);
    ~ScopedStyleColor();

    // Non-copyable
    ScopedStyleColor(const ScopedStyleColor&) = delete;
    ScopedStyleColor& operator=(const ScopedStyleColor&) = delete;

private:
    int m_count = 1;
};

/**
 * @brief RAII helper for pushing/popping ImGui style vars
 */
class ScopedStyleVar {
public:
    ScopedStyleVar(ImGuiStyleVar idx, float value);
    ScopedStyleVar(ImGuiStyleVar idx, const ImVec2& value);
    ~ScopedStyleVar();

    // Non-copyable
    ScopedStyleVar(const ScopedStyleVar&) = delete;
    ScopedStyleVar& operator=(const ScopedStyleVar&) = delete;

private:
    int m_count = 1;
};

/**
 * @brief RAII helper for pushing/popping ImGui ID
 */
class ScopedID {
public:
    explicit ScopedID(int id);
    explicit ScopedID(const char* str);
    explicit ScopedID(const void* ptr);
    ~ScopedID();

    // Non-copyable
    ScopedID(const ScopedID&) = delete;
    ScopedID& operator=(const ScopedID&) = delete;
};

/**
 * @brief RAII helper for disabling widgets
 */
class ScopedDisable {
public:
    explicit ScopedDisable(bool disabled = true);
    ~ScopedDisable();

    // Non-copyable
    ScopedDisable(const ScopedDisable&) = delete;
    ScopedDisable& operator=(const ScopedDisable&) = delete;

private:
    bool m_wasDisabled;
};

/**
 * @brief RAII helper for widget width
 */
class ScopedItemWidth {
public:
    explicit ScopedItemWidth(float width);
    ~ScopedItemWidth();

    // Non-copyable
    ScopedItemWidth(const ScopedItemWidth&) = delete;
    ScopedItemWidth& operator=(const ScopedItemWidth&) = delete;
};

/**
 * @brief RAII helper for indentation
 */
class ScopedIndent {
public:
    explicit ScopedIndent(float indent = 0.0f);
    ~ScopedIndent();

    // Non-copyable
    ScopedIndent(const ScopedIndent&) = delete;
    ScopedIndent& operator=(const ScopedIndent&) = delete;

private:
    float m_indent;
};

} // namespace Nova
