#include "EditorTheme.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// EditorTheme Implementation
// ============================================================================

EditorTheme& EditorTheme::Instance() {
    static EditorTheme instance;
    static bool initialized = false;
    if (!initialized) {
        instance.RegisterDefaultPresets();
        instance.ResetToDefault();
        initialized = true;
    }
    return instance;
}

void EditorTheme::Apply() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Apply sizes
    style.WindowRounding = m_sizes.windowRounding;
    style.ChildRounding = m_sizes.childRounding;
    style.FrameRounding = m_sizes.frameRounding;
    style.PopupRounding = m_sizes.popupRounding;
    style.ScrollbarRounding = m_sizes.scrollbarRounding;
    style.GrabRounding = m_sizes.grabRounding;
    style.TabRounding = m_sizes.tabRounding;

    style.WindowBorderSize = m_sizes.windowBorderSize;
    style.ChildBorderSize = m_sizes.childBorderSize;
    style.FrameBorderSize = m_sizes.frameBorderSize;
    style.PopupBorderSize = m_sizes.popupBorderSize;
    style.TabBorderSize = m_sizes.tabBorderSize;

    style.WindowPadding = ImVec2(m_sizes.windowPadding.x, m_sizes.windowPadding.y);
    style.FramePadding = ImVec2(m_sizes.framePadding.x, m_sizes.framePadding.y);
    style.CellPadding = ImVec2(m_sizes.cellPadding.x, m_sizes.cellPadding.y);
    style.ItemSpacing = ImVec2(m_sizes.itemSpacing.x, m_sizes.itemSpacing.y);
    style.ItemInnerSpacing = ImVec2(m_sizes.itemInnerSpacing.x, m_sizes.itemInnerSpacing.y);
    style.TouchExtraPadding = ImVec2(m_sizes.touchExtraPadding.x, m_sizes.touchExtraPadding.y);

    style.IndentSpacing = m_sizes.indentSpacing;
    style.ColumnsMinSpacing = m_sizes.columnsMinSpacing;
    style.ScrollbarSize = m_sizes.scrollbarSize;
    style.GrabMinSize = m_sizes.grabMinSize;

    style.ButtonTextAlign = ImVec2(m_sizes.buttonTextAlign, 0.5f);
    style.SelectableTextAlign = ImVec2(m_sizes.selectableTextAlign, 0.0f);

    // Apply colors
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ToImVec4(m_colors.background);
    colors[ImGuiCol_ChildBg] = ToImVec4(m_colors.backgroundAlt);
    colors[ImGuiCol_PopupBg] = ToImVec4(m_colors.panel);
    colors[ImGuiCol_Border] = ToImVec4(m_colors.border);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_FrameBg] = ToImVec4(m_colors.input);
    colors[ImGuiCol_FrameBgHovered] = ToImVec4(m_colors.inputHovered);
    colors[ImGuiCol_FrameBgActive] = ToImVec4(m_colors.inputActive);

    colors[ImGuiCol_TitleBg] = ToImVec4(m_colors.titleBar);
    colors[ImGuiCol_TitleBgActive] = ToImVec4(m_colors.titleBarActive);
    colors[ImGuiCol_TitleBgCollapsed] = ToImVec4(m_colors.titleBar);

    colors[ImGuiCol_MenuBarBg] = ToImVec4(m_colors.panelHeader);

    colors[ImGuiCol_ScrollbarBg] = ToImVec4(m_colors.scrollbarBg);
    colors[ImGuiCol_ScrollbarGrab] = ToImVec4(m_colors.scrollbarGrab);
    colors[ImGuiCol_ScrollbarGrabHovered] = ToImVec4(m_colors.scrollbarGrabHovered);
    colors[ImGuiCol_ScrollbarGrabActive] = ToImVec4(m_colors.scrollbarGrabActive);

    colors[ImGuiCol_CheckMark] = ToImVec4(m_colors.checkmark);
    colors[ImGuiCol_SliderGrab] = ToImVec4(m_colors.slider);
    colors[ImGuiCol_SliderGrabActive] = ToImVec4(m_colors.sliderActive);

    colors[ImGuiCol_Button] = ToImVec4(m_colors.button);
    colors[ImGuiCol_ButtonHovered] = ToImVec4(m_colors.buttonHovered);
    colors[ImGuiCol_ButtonActive] = ToImVec4(m_colors.buttonActive);

    colors[ImGuiCol_Header] = ToImVec4(m_colors.header);
    colors[ImGuiCol_HeaderHovered] = ToImVec4(m_colors.headerHovered);
    colors[ImGuiCol_HeaderActive] = ToImVec4(m_colors.headerActive);

    colors[ImGuiCol_Separator] = ToImVec4(m_colors.separator);
    colors[ImGuiCol_SeparatorHovered] = ToImVec4(m_colors.separatorHovered);
    colors[ImGuiCol_SeparatorActive] = ToImVec4(m_colors.separatorActive);

    colors[ImGuiCol_ResizeGrip] = ToImVec4(m_colors.resizeGrip);
    colors[ImGuiCol_ResizeGripHovered] = ToImVec4(m_colors.resizeGripHovered);
    colors[ImGuiCol_ResizeGripActive] = ToImVec4(m_colors.resizeGripActive);

    colors[ImGuiCol_Tab] = ToImVec4(m_colors.tab);
    colors[ImGuiCol_TabHovered] = ToImVec4(m_colors.tabHovered);
    colors[ImGuiCol_TabActive] = ToImVec4(m_colors.tabActive);
    colors[ImGuiCol_TabUnfocused] = ToImVec4(m_colors.tabUnfocused);
    colors[ImGuiCol_TabUnfocusedActive] = ToImVec4(m_colors.tabActive);

#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview] = ToImVec4(m_colors.accent);
    colors[ImGuiCol_DockingEmptyBg] = ToImVec4(m_colors.backgroundAlt);
#endif

    colors[ImGuiCol_PlotLines] = ToImVec4(m_colors.accent);
    colors[ImGuiCol_PlotLinesHovered] = ToImVec4(m_colors.accentHovered);
    colors[ImGuiCol_PlotHistogram] = ToImVec4(m_colors.accent);
    colors[ImGuiCol_PlotHistogramHovered] = ToImVec4(m_colors.accentHovered);

    colors[ImGuiCol_TableHeaderBg] = ToImVec4(m_colors.panelHeader);
    colors[ImGuiCol_TableBorderStrong] = ToImVec4(m_colors.border);
    colors[ImGuiCol_TableBorderLight] = ToImVec4(m_colors.separator);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ToImVec4(m_colors.backgroundAlt);

    colors[ImGuiCol_TextSelectedBg] = ToImVec4(m_colors.selection);
    colors[ImGuiCol_DragDropTarget] = ToImVec4(m_colors.dragDropTarget);

    colors[ImGuiCol_NavHighlight] = ToImVec4(m_colors.accent);
    colors[ImGuiCol_NavWindowingHighlight] = ToImVec4(glm::vec4(1.0f, 1.0f, 1.0f, 0.70f));
    colors[ImGuiCol_NavWindowingDimBg] = ToImVec4(glm::vec4(0.80f, 0.80f, 0.80f, 0.20f));

    colors[ImGuiCol_ModalWindowDimBg] = ToImVec4(m_colors.modalDim);

    colors[ImGuiCol_Text] = ToImVec4(m_colors.text);
    colors[ImGuiCol_TextDisabled] = ToImVec4(m_colors.textDisabled);
}

bool EditorTheme::Load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        auto loadVec4 = [](const nlohmann::json& obj, const std::string& key, glm::vec4& out) {
            if (obj.contains(key) && obj[key].is_array() && obj[key].size() == 4) {
                out.x = obj[key][0].get<float>();
                out.y = obj[key][1].get<float>();
                out.z = obj[key][2].get<float>();
                out.w = obj[key][3].get<float>();
            }
        };

        if (j.contains("colors")) {
            auto& c = j["colors"];
            loadVec4(c, "background", m_colors.background);
            loadVec4(c, "backgroundAlt", m_colors.backgroundAlt);
            loadVec4(c, "panel", m_colors.panel);
            loadVec4(c, "button", m_colors.button);
            loadVec4(c, "buttonHovered", m_colors.buttonHovered);
            loadVec4(c, "buttonActive", m_colors.buttonActive);
            loadVec4(c, "accent", m_colors.accent);
            loadVec4(c, "text", m_colors.text);
            loadVec4(c, "textDisabled", m_colors.textDisabled);
            loadVec4(c, "success", m_colors.success);
            loadVec4(c, "warning", m_colors.warning);
            loadVec4(c, "error", m_colors.error);
            // ... load more colors as needed
        }

        if (j.contains("sizes")) {
            auto& s = j["sizes"];
            if (s.contains("windowRounding")) m_sizes.windowRounding = s["windowRounding"].get<float>();
            if (s.contains("frameRounding")) m_sizes.frameRounding = s["frameRounding"].get<float>();
            // ... load more sizes as needed
        }

        Apply();
        return true;
    } catch (...) {
        return false;
    }
}

bool EditorTheme::Save(const std::string& path) const {
    try {
        nlohmann::json j;

        auto saveVec4 = [](const glm::vec4& v) {
            return nlohmann::json::array({v.x, v.y, v.z, v.w});
        };

        j["colors"]["background"] = saveVec4(m_colors.background);
        j["colors"]["backgroundAlt"] = saveVec4(m_colors.backgroundAlt);
        j["colors"]["panel"] = saveVec4(m_colors.panel);
        j["colors"]["button"] = saveVec4(m_colors.button);
        j["colors"]["buttonHovered"] = saveVec4(m_colors.buttonHovered);
        j["colors"]["buttonActive"] = saveVec4(m_colors.buttonActive);
        j["colors"]["accent"] = saveVec4(m_colors.accent);
        j["colors"]["text"] = saveVec4(m_colors.text);
        j["colors"]["textDisabled"] = saveVec4(m_colors.textDisabled);
        j["colors"]["success"] = saveVec4(m_colors.success);
        j["colors"]["warning"] = saveVec4(m_colors.warning);
        j["colors"]["error"] = saveVec4(m_colors.error);

        j["sizes"]["windowRounding"] = m_sizes.windowRounding;
        j["sizes"]["frameRounding"] = m_sizes.frameRounding;

        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

void EditorTheme::ResetToDefault() {
    m_colors = Colors{};
    m_sizes = Sizes{};
    m_fonts = Fonts{};

    // Setup default pin colors
    m_pinColors["float"] = m_colors.pinFloat;
    m_pinColors["int"] = m_colors.pinInt;
    m_pinColors["bool"] = m_colors.pinBool;
    m_pinColors["string"] = m_colors.pinString;
    m_pinColors["vec2"] = m_colors.pinVector;
    m_pinColors["vec3"] = m_colors.pinVector;
    m_pinColors["vec4"] = m_colors.pinVector;
    m_pinColors["color"] = m_colors.pinColor;
    m_pinColors["texture"] = m_colors.pinTexture;
    m_pinColors["event"] = m_colors.pinEvent;
    m_pinColors["exec"] = m_colors.pinExec;
    m_pinColors["object"] = m_colors.pinObject;

    // Setup default node category colors
    m_nodeCategoryColors["Math"] = glm::vec4(0.30f, 0.70f, 0.40f, 1.0f);
    m_nodeCategoryColors["Logic"] = glm::vec4(0.70f, 0.30f, 0.30f, 1.0f);
    m_nodeCategoryColors["Input"] = glm::vec4(0.80f, 0.60f, 0.20f, 1.0f);
    m_nodeCategoryColors["Output"] = glm::vec4(0.80f, 0.40f, 0.60f, 1.0f);
    m_nodeCategoryColors["Texture"] = glm::vec4(0.60f, 0.40f, 0.80f, 1.0f);
    m_nodeCategoryColors["Utility"] = glm::vec4(0.50f, 0.50f, 0.50f, 1.0f);
    m_nodeCategoryColors["Event"] = glm::vec4(0.90f, 0.30f, 0.30f, 1.0f);
    m_nodeCategoryColors["Flow"] = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    m_nodeCategoryColors["Variable"] = glm::vec4(0.30f, 0.70f, 0.90f, 1.0f);
    m_nodeCategoryColors["Animation"] = glm::vec4(0.90f, 0.60f, 0.30f, 1.0f);
}

void EditorTheme::SetPreset(const std::string& presetName) {
    auto it = m_presets.find(presetName);
    if (it != m_presets.end()) {
        it->second();
        Apply();
    }
}

std::vector<std::string> EditorTheme::GetPresetNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : m_presets) {
        names.push_back(name);
    }
    return names;
}

ImU32 EditorTheme::ToImU32(const glm::vec4& v) {
    return IM_COL32(
        static_cast<int>(v.x * 255.0f),
        static_cast<int>(v.y * 255.0f),
        static_cast<int>(v.z * 255.0f),
        static_cast<int>(v.w * 255.0f)
    );
}

glm::vec4 EditorTheme::GetPinColor(const std::string& typeName) const {
    auto it = m_pinColors.find(typeName);
    return it != m_pinColors.end() ? it->second : m_colors.text;
}

glm::vec4 EditorTheme::GetNodeCategoryColor(const std::string& category) const {
    auto it = m_nodeCategoryColors.find(category);
    return it != m_nodeCategoryColors.end() ? it->second : m_colors.nodeHeader;
}

glm::vec4 EditorTheme::Lerp(const glm::vec4& a, const glm::vec4& b, float t) {
    return a + (b - a) * t;
}

glm::vec4 EditorTheme::AdjustBrightness(const glm::vec4& color, float factor) {
    return glm::vec4(
        glm::clamp(color.x * factor, 0.0f, 1.0f),
        glm::clamp(color.y * factor, 0.0f, 1.0f),
        glm::clamp(color.z * factor, 0.0f, 1.0f),
        color.w
    );
}

glm::vec4 EditorTheme::AdjustSaturation(const glm::vec4& color, float factor) {
    float gray = color.x * 0.299f + color.y * 0.587f + color.z * 0.114f;
    return glm::vec4(
        glm::clamp(gray + (color.x - gray) * factor, 0.0f, 1.0f),
        glm::clamp(gray + (color.y - gray) * factor, 0.0f, 1.0f),
        glm::clamp(gray + (color.z - gray) * factor, 0.0f, 1.0f),
        color.w
    );
}

void EditorTheme::RegisterDefaultPresets() {
    // Dark theme (default)
    m_presets["Dark"] = [this]() {
        ResetToDefault();
    };

    // Light theme
    m_presets["Light"] = [this]() {
        m_colors.background = glm::vec4(0.94f, 0.94f, 0.94f, 1.0f);
        m_colors.backgroundAlt = glm::vec4(0.90f, 0.90f, 0.90f, 1.0f);
        m_colors.panel = glm::vec4(0.98f, 0.98f, 0.98f, 1.0f);
        m_colors.panelHeader = glm::vec4(0.88f, 0.88f, 0.88f, 1.0f);
        m_colors.titleBar = glm::vec4(0.80f, 0.80f, 0.80f, 1.0f);
        m_colors.titleBarActive = glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
        m_colors.border = glm::vec4(0.70f, 0.70f, 0.70f, 1.0f);
        m_colors.button = glm::vec4(0.85f, 0.85f, 0.85f, 1.0f);
        m_colors.buttonHovered = glm::vec4(0.78f, 0.78f, 0.78f, 1.0f);
        m_colors.buttonActive = glm::vec4(0.70f, 0.70f, 0.70f, 1.0f);
        m_colors.input = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_colors.inputHovered = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
        m_colors.text = glm::vec4(0.10f, 0.10f, 0.10f, 1.0f);
        m_colors.textSecondary = glm::vec4(0.40f, 0.40f, 0.40f, 1.0f);
        m_colors.textDisabled = glm::vec4(0.60f, 0.60f, 0.60f, 1.0f);
        m_colors.accent = glm::vec4(0.20f, 0.45f, 0.80f, 1.0f);
        m_colors.nodeBackground = glm::vec4(0.95f, 0.95f, 0.95f, 0.95f);
    };

    // Blue accent theme
    m_presets["Blue Accent"] = [this]() {
        ResetToDefault();
        m_colors.accent = glm::vec4(0.30f, 0.55f, 0.95f, 1.0f);
        m_colors.accentHovered = glm::vec4(0.40f, 0.65f, 1.0f, 1.0f);
        m_colors.accentActive = glm::vec4(0.50f, 0.70f, 1.0f, 1.0f);
        m_colors.checkmark = m_colors.accent;
        m_colors.sliderActive = m_colors.accent;
    };

    // Green accent theme
    m_presets["Green Accent"] = [this]() {
        ResetToDefault();
        m_colors.accent = glm::vec4(0.30f, 0.75f, 0.45f, 1.0f);
        m_colors.accentHovered = glm::vec4(0.40f, 0.85f, 0.55f, 1.0f);
        m_colors.accentActive = glm::vec4(0.50f, 0.90f, 0.60f, 1.0f);
        m_colors.checkmark = m_colors.accent;
        m_colors.sliderActive = m_colors.accent;
    };

    // High contrast
    m_presets["High Contrast"] = [this]() {
        m_colors.background = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        m_colors.backgroundAlt = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
        m_colors.panel = glm::vec4(0.08f, 0.08f, 0.08f, 1.0f);
        m_colors.border = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_colors.text = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_colors.textDisabled = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        m_colors.button = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        m_colors.buttonHovered = glm::vec4(0.35f, 0.35f, 0.35f, 1.0f);
        m_colors.accent = glm::vec4(1.0f, 0.85f, 0.0f, 1.0f);
    };
}

// ============================================================================
// Scoped Style Helpers Implementation
// ============================================================================

ScopedStyleColor::ScopedStyleColor(ImGuiCol idx, const glm::vec4& color) {
    ImGui::PushStyleColor(idx, EditorTheme::ToImVec4(color));
}

ScopedStyleColor::ScopedStyleColor(ImGuiCol idx, const ImVec4& color) {
    ImGui::PushStyleColor(idx, color);
}

ScopedStyleColor::~ScopedStyleColor() {
    ImGui::PopStyleColor(m_count);
}

ScopedStyleVar::ScopedStyleVar(ImGuiStyleVar idx, float value) {
    ImGui::PushStyleVar(idx, value);
}

ScopedStyleVar::ScopedStyleVar(ImGuiStyleVar idx, const ImVec2& value) {
    ImGui::PushStyleVar(idx, value);
}

ScopedStyleVar::~ScopedStyleVar() {
    ImGui::PopStyleVar(m_count);
}

ScopedID::ScopedID(int id) { ImGui::PushID(id); }
ScopedID::ScopedID(const char* str) { ImGui::PushID(str); }
ScopedID::ScopedID(const void* ptr) { ImGui::PushID(ptr); }
ScopedID::~ScopedID() { ImGui::PopID(); }

ScopedDisable::ScopedDisable(bool disabled) : m_wasDisabled(disabled) {
    if (disabled) {
        ImGui::BeginDisabled();
    }
}

ScopedDisable::~ScopedDisable() {
    if (m_wasDisabled) {
        ImGui::EndDisabled();
    }
}

ScopedItemWidth::ScopedItemWidth(float width) {
    ImGui::PushItemWidth(width);
}

ScopedItemWidth::~ScopedItemWidth() {
    ImGui::PopItemWidth();
}

ScopedIndent::ScopedIndent(float indent) : m_indent(indent) {
    if (indent > 0.0f) {
        ImGui::Indent(indent);
    } else {
        ImGui::Indent();
        m_indent = ImGui::GetStyle().IndentSpacing;
    }
}

ScopedIndent::~ScopedIndent() {
    ImGui::Unindent(m_indent);
}

} // namespace Nova
