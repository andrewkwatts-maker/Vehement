#include "EditorWidgets.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <deque>
#include <chrono>

namespace Nova {
namespace EditorWidgets {

// ============================================================================
// Internal State
// ============================================================================

static struct {
    // Notification queue
    struct Notification {
        std::string title;
        std::string message;
        NotificationType type;
        float duration;
        float elapsed;
    };
    std::deque<Notification> notifications;

    // Dialog state
    bool dialogOpen = false;
    std::string dialogTitle;
    DialogResult dialogResult = DialogResult::None;

    // Search state
    char searchBuffer[256] = "";

    // Layout state
    bool inHorizontalLayout = false;
    float horizontalPos = 0.0f;
} s_state;

// ============================================================================
// Property Editors
// ============================================================================

static void DrawPropertyLabel(const char* label) {
    auto& theme = EditorTheme::Instance();
    float labelWidth = theme.GetSizes().propertyLabelWidth;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
    ImGui::SetNextItemWidth(-1);
}

bool Property(const char* label, int& value, int min, int max, const char* format) {
    DrawPropertyLabel(label);
    return ImGui::DragInt(("##" + std::string(label)).c_str(), &value, 1.0f, min, max, format);
}

bool Property(const char* label, float& value, float min, float max, float speed, const char* format) {
    DrawPropertyLabel(label);
    return ImGui::DragFloat(("##" + std::string(label)).c_str(), &value, speed, min, max, format);
}

bool Property(const char* label, double& value, double min, double max, double speed) {
    float fval = static_cast<float>(value);
    DrawPropertyLabel(label);
    bool changed = ImGui::DragFloat(("##" + std::string(label)).c_str(), &fval, static_cast<float>(speed), static_cast<float>(min), static_cast<float>(max));
    if (changed) value = fval;
    return changed;
}

bool Property(const char* label, bool& value) {
    DrawPropertyLabel(label);
    return ImGui::Checkbox(("##" + std::string(label)).c_str(), &value);
}

bool Property(const char* label, std::string& value, size_t maxLength) {
    DrawPropertyLabel(label);
    char buffer[1024];
    size_t len = std::min(value.size(), sizeof(buffer) - 1);
    memcpy(buffer, value.c_str(), len);
    buffer[len] = '\0';

    bool changed = ImGui::InputText(("##" + std::string(label)).c_str(), buffer, std::min(maxLength, sizeof(buffer)));
    if (changed) value = buffer;
    return changed;
}

bool Property(const char* label, char* buffer, size_t bufferSize) {
    DrawPropertyLabel(label);
    return ImGui::InputText(("##" + std::string(label)).c_str(), buffer, bufferSize);
}

bool Property(const char* label, glm::vec2& value, float min, float max, float speed) {
    DrawPropertyLabel(label);
    return ImGui::DragFloat2(("##" + std::string(label)).c_str(), &value.x, speed, min, max);
}

bool Property(const char* label, glm::vec3& value, float min, float max, float speed) {
    DrawPropertyLabel(label);
    return ImGui::DragFloat3(("##" + std::string(label)).c_str(), &value.x, speed, min, max);
}

bool Property(const char* label, glm::vec4& value, float min, float max, float speed) {
    DrawPropertyLabel(label);
    return ImGui::DragFloat4(("##" + std::string(label)).c_str(), &value.x, speed, min, max);
}

bool ColorProperty(const char* label, glm::vec3& color, bool showAlpha) {
    DrawPropertyLabel(label);
    (void)showAlpha;
    return ImGui::ColorEdit3(("##" + std::string(label)).c_str(), &color.x);
}

bool ColorProperty(const char* label, glm::vec4& color) {
    DrawPropertyLabel(label);
    return ImGui::ColorEdit4(("##" + std::string(label)).c_str(), &color.x);
}

bool AngleProperty(const char* label, float& degrees, float min, float max) {
    DrawPropertyLabel(label);
    return ImGui::SliderFloat(("##" + std::string(label)).c_str(), &degrees, min, max, "%.1f deg");
}

bool EnumProperty(const char* label, int& value, const char* const* names, int count) {
    DrawPropertyLabel(label);

    const char* preview = (value >= 0 && value < count) ? names[value] : "Invalid";
    bool changed = false;

    if (ImGui::BeginCombo(("##" + std::string(label)).c_str(), preview)) {
        for (int i = 0; i < count; i++) {
            bool selected = (value == i);
            if (ImGui::Selectable(names[i], selected)) {
                value = i;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool FlagsProperty(const char* label, uint32_t& flags, const char* const* names, int count) {
    bool changed = false;
    if (CollapsingHeader(label)) {
        ScopedIndent indent;
        for (int i = 0; i < count; i++) {
            bool checked = (flags & (1u << i)) != 0;
            if (ImGui::Checkbox(names[i], &checked)) {
                if (checked) flags |= (1u << i);
                else flags &= ~(1u << i);
                changed = true;
            }
        }
    }
    return changed;
}

bool SliderProperty(const char* label, int& value, int min, int max, const char* format) {
    DrawPropertyLabel(label);
    return ImGui::SliderInt(("##" + std::string(label)).c_str(), &value, min, max, format);
}

bool SliderProperty(const char* label, float& value, float min, float max, const char* format) {
    DrawPropertyLabel(label);
    return ImGui::SliderFloat(("##" + std::string(label)).c_str(), &value, min, max, format);
}

bool AssetProperty(const char* label, std::string& assetPath, const char* filter, const char* assetType) {
    (void)filter;
    (void)assetType;
    DrawPropertyLabel(label);

    float buttonWidth = 24.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - 4.0f);

    char buffer[512];
    strncpy(buffer, assetPath.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    bool changed = ImGui::InputText(("##" + std::string(label)).c_str(), buffer, sizeof(buffer));
    if (changed) assetPath = buffer;

    ImGui::SameLine();
    if (ImGui::Button("...", ImVec2(buttonWidth, 0))) {
        // Open file browser - would integrate with platform dialog
    }
    return changed;
}

bool ObjectProperty(const char* label, uint64_t& objectId, const char* typeName) {
    (void)typeName;
    DrawPropertyLabel(label);

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)objectId);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50.0f);
    ImGui::InputText(("##" + std::string(label)).c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);

    ImGui::SameLine();
    if (ImGui::Button("Pick")) {
        // Open object picker
    }
    return false;
}

bool CurveProperty(const char* label, std::vector<CurvePoint>& curve, float minTime, float maxTime, float minValue, float maxValue) {
    (void)minTime; (void)maxTime; (void)minValue; (void)maxValue;

    bool changed = false;
    if (CollapsingHeader(label)) {
        ScopedIndent indent;

        // Simple curve editor - draw points
        ImVec2 size(ImGui::GetContentRegionAvail().x, 100);
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
            IM_COL32(30, 30, 35, 255));

        // Draw curve line
        if (curve.size() >= 2) {
            for (size_t i = 0; i < curve.size() - 1; i++) {
                float x1 = pos.x + curve[i].time * size.x;
                float y1 = pos.y + (1.0f - curve[i].value) * size.y;
                float x2 = pos.x + curve[i + 1].time * size.x;
                float y2 = pos.y + (1.0f - curve[i + 1].value) * size.y;
                drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(100, 150, 255, 255), 2.0f);
            }
        }

        // Draw points
        for (auto& pt : curve) {
            float x = pos.x + pt.time * size.x;
            float y = pos.y + (1.0f - pt.value) * size.y;
            drawList->AddCircleFilled(ImVec2(x, y), 4.0f, IM_COL32(255, 255, 255, 255));
        }

        ImGui::Dummy(size);
    }
    return changed;
}

bool GradientProperty(const char* label, std::vector<GradientKey>& gradient) {
    bool changed = false;
    if (CollapsingHeader(label)) {
        ScopedIndent indent;

        ImVec2 size(ImGui::GetContentRegionAvail().x, 20);
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw gradient bar
        if (gradient.size() >= 2) {
            for (size_t i = 0; i < gradient.size() - 1; i++) {
                float x1 = pos.x + gradient[i].position * size.x;
                float x2 = pos.x + gradient[i + 1].position * size.x;
                ImU32 c1 = EditorTheme::ToImU32(gradient[i].color);
                ImU32 c2 = EditorTheme::ToImU32(gradient[i + 1].color);
                drawList->AddRectFilledMultiColor(
                    ImVec2(x1, pos.y),
                    ImVec2(x2, pos.y + size.y),
                    c1, c2, c2, c1
                );
            }
        }

        drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(80, 80, 85, 255));

        ImGui::Dummy(size);
    }
    return changed;
}

// ============================================================================
// Panels and Headers
// ============================================================================

bool BeginPropertyPanel(const char* name, bool* open, bool defaultOpen) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar;
    (void)defaultOpen;
    return ImGui::BeginChild(name, ImVec2(0, 0), ImGuiChildFlags_Borders, flags) && (open == nullptr || *open);
}

void EndPropertyPanel() {
    ImGui::EndChild();
}

bool CollapsingHeader(const char* label, bool* open, bool defaultOpen) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    bool isOpen = open ? *open : true;
    bool result = ImGui::CollapsingHeader(label, flags);

    if (open) *open = result;
    return result;
}

void SubHeader(const char* label) {
    auto& theme = EditorTheme::Instance();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().textSecondary));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
}

void Separator(const char* label) {
    if (label && label[0]) {
        ImGui::Spacing();
        auto& theme = EditorTheme::Instance();
        ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().textSecondary));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::Separator();
    } else {
        ImGui::Separator();
    }
}

void BeginSection(const char* label) {
    if (label && label[0]) {
        SubHeader(label);
    }
    ImGui::Indent();
}

void EndSection() {
    ImGui::Unindent();
    ImGui::Spacing();
}

// ============================================================================
// Buttons and Actions
// ============================================================================

bool Button(const char* label, ButtonStyle style, const glm::vec2& size) {
    auto& theme = EditorTheme::Instance();
    auto& colors = theme.GetColors();

    ImVec4 bg, bgHover, bgActive;

    switch (style) {
        case ButtonStyle::Primary:
            bg = EditorTheme::ToImVec4(colors.accent);
            bgHover = EditorTheme::ToImVec4(colors.accentHovered);
            bgActive = EditorTheme::ToImVec4(colors.accentActive);
            break;
        case ButtonStyle::Success:
            bg = EditorTheme::ToImVec4(colors.success);
            bgHover = EditorTheme::ToImVec4(EditorTheme::AdjustBrightness(colors.success, 1.2f));
            bgActive = EditorTheme::ToImVec4(EditorTheme::AdjustBrightness(colors.success, 0.8f));
            break;
        case ButtonStyle::Warning:
            bg = EditorTheme::ToImVec4(colors.warning);
            bgHover = EditorTheme::ToImVec4(EditorTheme::AdjustBrightness(colors.warning, 1.2f));
            bgActive = EditorTheme::ToImVec4(EditorTheme::AdjustBrightness(colors.warning, 0.8f));
            break;
        case ButtonStyle::Danger:
            bg = EditorTheme::ToImVec4(colors.error);
            bgHover = EditorTheme::ToImVec4(EditorTheme::AdjustBrightness(colors.error, 1.2f));
            bgActive = EditorTheme::ToImVec4(EditorTheme::AdjustBrightness(colors.error, 0.8f));
            break;
        case ButtonStyle::Ghost:
            bg = ImVec4(0, 0, 0, 0);
            bgHover = EditorTheme::ToImVec4(colors.buttonHovered);
            bgActive = EditorTheme::ToImVec4(colors.buttonActive);
            break;
        case ButtonStyle::Link:
            bg = ImVec4(0, 0, 0, 0);
            bgHover = ImVec4(0, 0, 0, 0);
            bgActive = ImVec4(0, 0, 0, 0);
            break;
        default:
            bg = EditorTheme::ToImVec4(colors.button);
            bgHover = EditorTheme::ToImVec4(colors.buttonHovered);
            bgActive = EditorTheme::ToImVec4(colors.buttonActive);
            break;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgActive);

    bool result = ImGui::Button(label, ImVec2(size.x, size.y));

    ImGui::PopStyleColor(3);
    return result;
}

bool IconButton(const char* icon, const char* tooltip, ButtonStyle style, float size) {
    if (size <= 0.0f) size = ImGui::GetFrameHeight();

    bool result = Button(icon, style, glm::vec2(size, size));

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    return result;
}

bool ToggleButton(const char* label, bool& active, ButtonStyle style, const glm::vec2& size) {
    auto& theme = EditorTheme::Instance();

    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorTheme::ToImVec4(theme.GetColors().accentHovered));
    }

    bool clicked = Button(label, style, size);
    if (clicked) active = !active;

    if (active) {
        ImGui::PopStyleColor(2);
    }

    return clicked;
}

void BeginButtonGroup() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
}

void EndButtonGroup() {
    ImGui::PopStyleVar();
}

void BeginToolbar(const char* id, float height) {
    auto& theme = EditorTheme::Instance();
    if (height <= 0.0f) height = theme.GetSizes().toolbarHeight;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorTheme::ToImVec4(theme.GetColors().panelHeader));

    ImGui::BeginChild(id, ImVec2(0, height), ImGuiChildFlags_None);
}

void EndToolbar() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

bool ToolbarButton(const char* icon, const char* tooltip, bool selected) {
    auto& theme = EditorTheme::Instance();
    float size = theme.GetSizes().toolbarButtonSize;

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTheme::ToImVec4(theme.GetColors().accent));
    }

    bool clicked = ImGui::Button(icon, ImVec2(size, size));

    if (selected) {
        ImGui::PopStyleColor();
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();
    return clicked;
}

void ToolbarSeparator() {
    auto& theme = EditorTheme::Instance();
    ImGui::SameLine();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float height = theme.GetSizes().toolbarButtonSize;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(pos.x, pos.y + 4),
        ImVec2(pos.x, pos.y + height - 4),
        EditorTheme::ToImU32(theme.GetColors().separator),
        1.0f
    );
    ImGui::Dummy(ImVec2(8, height));
    ImGui::SameLine();
}

void ToolbarSpacer() {
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(8, 0));
    ImGui::SameLine();
}

// ============================================================================
// Dropdowns and Selection
// ============================================================================

bool SearchableCombo(const char* label, int& selectedIndex, const std::vector<std::string>& items, const char* previewOverride) {
    const char* preview = previewOverride;
    if (!preview && selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size())) {
        preview = items[selectedIndex].c_str();
    }
    if (!preview) preview = "";

    bool changed = false;
    DrawPropertyLabel(label);

    if (ImGui::BeginCombo(("##" + std::string(label)).c_str(), preview)) {
        // Search filter
        ImGui::InputText("##search", s_state.searchBuffer, sizeof(s_state.searchBuffer));

        std::string filter = s_state.searchBuffer;
        for (auto& c : filter) c = static_cast<char>(tolower(c));

        for (int i = 0; i < static_cast<int>(items.size()); i++) {
            std::string itemLower = items[i];
            for (auto& c : itemLower) c = static_cast<char>(tolower(c));

            if (!filter.empty() && itemLower.find(filter) == std::string::npos) {
                continue;
            }

            bool selected = (selectedIndex == i);
            if (ImGui::Selectable(items[i].c_str(), selected)) {
                selectedIndex = i;
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return changed;
}

bool FilteredListBox(const char* label, int& selectedIndex, const std::vector<std::string>& items, char* filterBuffer, size_t filterSize, float height) {
    bool changed = false;

    ImGui::TextUnformatted(label);
    ImGui::InputText("##filter", filterBuffer, filterSize);

    std::string filter = filterBuffer;
    for (auto& c : filter) c = static_cast<char>(tolower(c));

    if (height <= 0.0f) height = 200.0f;
    ImGui::BeginChild("##listbox", ImVec2(0, height), ImGuiChildFlags_Borders);

    for (int i = 0; i < static_cast<int>(items.size()); i++) {
        std::string itemLower = items[i];
        for (auto& c : itemLower) c = static_cast<char>(tolower(c));

        if (!filter.empty() && itemLower.find(filter) == std::string::npos) {
            continue;
        }

        bool selected = (selectedIndex == i);
        if (ImGui::Selectable(items[i].c_str(), selected)) {
            selectedIndex = i;
            changed = true;
        }
    }

    ImGui::EndChild();
    return changed;
}

bool TagSelector(const char* label, std::vector<std::string>& selectedTags, const std::vector<std::string>& availableTags) {
    bool changed = false;
    DrawPropertyLabel(label);

    // Show selected tags as removable badges
    for (size_t i = 0; i < selectedTags.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        Badge(selectedTags[i].c_str(), EditorTheme::Instance().GetColors().accent);
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            selectedTags.erase(selectedTags.begin() + i);
            changed = true;
            i--;
        }
        ImGui::SameLine();
        ImGui::PopID();
    }

    // Add new tag combo
    if (ImGui::BeginCombo("##addtag", "+", ImGuiComboFlags_NoPreview)) {
        for (const auto& tag : availableTags) {
            bool alreadySelected = std::find(selectedTags.begin(), selectedTags.end(), tag) != selectedTags.end();
            if (!alreadySelected && ImGui::Selectable(tag.c_str())) {
                selectedTags.push_back(tag);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

// ============================================================================
// Tree Views
// ============================================================================

bool TreeNode(const char* label, TreeNodeFlags flags, const char* icon) {
    ImGuiTreeNodeFlags imFlags = ImGuiTreeNodeFlags_OpenOnArrow;

    if (flags & TreeNodeFlags::Selected) imFlags |= ImGuiTreeNodeFlags_Selected;
    if (flags & TreeNodeFlags::OpenOnDoubleClick) imFlags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (flags & TreeNodeFlags::Leaf) imFlags |= ImGuiTreeNodeFlags_Leaf;
    if (flags & TreeNodeFlags::DefaultOpen) imFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (flags & TreeNodeFlags::SpanFullWidth) imFlags |= ImGuiTreeNodeFlags_SpanFullWidth;

    std::string displayLabel;
    if (icon && icon[0]) {
        displayLabel = std::string(icon) + " " + label;
    } else {
        displayLabel = label;
    }

    return ImGui::TreeNodeEx(label, imFlags, "%s", displayLabel.c_str());
}

void TreePop() {
    ImGui::TreePop();
}

bool TreeNodeEx(const char* id, const char* label, void* payload, size_t payloadSize, const char* payloadType, TreeNodeFlags flags, const char* icon) {
    bool open = TreeNode(label, flags, icon);

    if (flags & TreeNodeFlags::AllowDragDrop) {
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(payloadType, payload, payloadSize);
            ImGui::TextUnformatted(label);
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(payloadType)) {
                // Handle drop - caller should check GetTreeDragDropPayload
                (void)p;
            }
            ImGui::EndDragDropTarget();
        }
    }

    (void)id;
    return open;
}

bool IsTreeNodeDragging() {
    return ImGui::GetDragDropPayload() != nullptr;
}

const void* GetTreeDragDropPayload(const char* type, size_t* outSize) {
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (payload && payload->IsDataType(type)) {
        if (outSize) *outSize = payload->DataSize;
        return payload->Data;
    }
    return nullptr;
}

// ============================================================================
// Input Fields
// ============================================================================

bool SearchInput(const char* id, char* buffer, size_t bufferSize, const char* hint) {
    ImGui::PushID(id);

    // Search icon would go here
    ImGui::Text("?");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 24);
    bool changed = ImGui::InputTextWithHint("##search", hint, buffer, bufferSize);

    ImGui::SameLine();
    if (buffer[0] && ImGui::SmallButton("X")) {
        buffer[0] = '\0';
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

bool TextAreaInput(const char* label, std::string& text, const glm::vec2& size, bool readOnly) {
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
    if (readOnly) flags |= ImGuiInputTextFlags_ReadOnly;

    char buffer[65536];
    strncpy(buffer, text.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    bool changed = ImGui::InputTextMultiline(label, buffer, sizeof(buffer), ImVec2(size.x, size.y), flags);
    if (changed) text = buffer;
    return changed;
}

bool CodeInput(const char* label, std::string& code, const char* language, const glm::vec2& size) {
    (void)language; // Would use for syntax highlighting
    return TextAreaInput(label, code, size, false);
}

bool PathInput(const char* label, std::string& path, const char* filter, bool folder) {
    (void)filter; (void)folder;
    return AssetProperty(label, path);
}

// ============================================================================
// Visual Feedback
// ============================================================================

void ProgressBar(float fraction, const glm::vec2& size, const char* overlay) {
    ImGui::ProgressBar(fraction, ImVec2(size.x, size.y), overlay);
}

void Spinner(const char* label, float radius, float thickness) {
    (void)label;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float time = static_cast<float>(ImGui::GetTime());
    int numSegments = 30;
    float start = fmodf(time * 6.0f, 2.0f * 3.14159f);

    drawList->PathArcTo(
        ImVec2(pos.x + radius, pos.y + radius),
        radius - thickness * 0.5f,
        start,
        start + 1.5f * 3.14159f,
        numSegments
    );
    drawList->PathStroke(EditorTheme::ToImU32(EditorTheme::Instance().GetColors().accent), 0, thickness);

    ImGui::Dummy(ImVec2(radius * 2, radius * 2));
}

void Badge(const char* text, const glm::vec4& color) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 textSize = ImGui::CalcTextSize(text);
    float padding = 4.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        pos,
        ImVec2(pos.x + textSize.x + padding * 2, pos.y + textSize.y + padding),
        EditorTheme::ToImU32(color),
        4.0f
    );

    ImGui::SetCursorScreenPos(ImVec2(pos.x + padding, pos.y + padding * 0.5f));
    ImGui::TextUnformatted(text);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + textSize.x + padding * 2 + 4, pos.y));
}

void BeginTooltipEx() {
    ImGui::BeginTooltip();
}

void EndTooltipEx() {
    ImGui::EndTooltip();
}

void SetTooltip(const char* text) {
    ImGui::SetTooltip("%s", text);
}

void InfoMarker(const char* text) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void WarningMarker(const char* text) {
    auto& theme = EditorTheme::Instance();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().warning));
    ImGui::Text("(!)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}

void ErrorMarker(const char* text) {
    auto& theme = EditorTheme::Instance();
    ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().error));
    ImGui::Text("(X)");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", text);
    }
}

// ============================================================================
// Status Bar
// ============================================================================

void BeginStatusBar() {
    auto& theme = EditorTheme::Instance();
    float height = theme.GetSizes().statusBarHeight;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorTheme::ToImVec4(theme.GetColors().panelHeader));

    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
#ifdef IMGUI_HAS_DOCK
    statusFlags |= ImGuiWindowFlags_NoDocking;
#endif
    ImGui::Begin("##StatusBar", nullptr, statusFlags);
}

void EndStatusBar() {
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void StatusBarItem(const char* text, float width) {
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }
    ImGui::TextUnformatted(text);
    ImGui::SameLine();
}

void StatusBarSeparator() {
    auto& theme = EditorTheme::Instance();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float height = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(pos.x, pos.y + 2),
        ImVec2(pos.x, pos.y + height - 2),
        EditorTheme::ToImU32(theme.GetColors().separator)
    );
    ImGui::Dummy(ImVec2(8, 0));
    ImGui::SameLine();
}

// ============================================================================
// Notifications
// ============================================================================

void ShowNotification(const char* title, const char* message, NotificationType type, float duration) {
    s_state.notifications.push_back({title, message, type, duration, 0.0f});
}

void RenderNotifications() {
    auto& theme = EditorTheme::Instance();
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    float yOffset = 20.0f;
    float xPos = viewport->WorkPos.x + viewport->WorkSize.x - 320.0f;

    for (size_t i = 0; i < s_state.notifications.size(); ) {
        auto& notif = s_state.notifications[i];
        notif.elapsed += ImGui::GetIO().DeltaTime;

        if (notif.elapsed >= notif.duration) {
            s_state.notifications.erase(s_state.notifications.begin() + i);
            continue;
        }

        // Calculate alpha for fade in/out
        float alpha = 1.0f;
        if (notif.elapsed < 0.3f) alpha = notif.elapsed / 0.3f;
        else if (notif.elapsed > notif.duration - 0.3f) alpha = (notif.duration - notif.elapsed) / 0.3f;

        // Get color based on type
        glm::vec4 bgColor;
        switch (notif.type) {
            case NotificationType::Success: bgColor = theme.GetColors().success; break;
            case NotificationType::Warning: bgColor = theme.GetColors().warning; break;
            case NotificationType::Error: bgColor = theme.GetColors().error; break;
            default: bgColor = theme.GetColors().info; break;
        }
        bgColor.w = alpha * 0.95f;

        ImGui::SetNextWindowPos(ImVec2(xPos, viewport->WorkPos.y + yOffset));
        ImGui::SetNextWindowSize(ImVec2(300, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorTheme::ToImVec4(bgColor));

        char windowId[64];
        snprintf(windowId, sizeof(windowId), "##Notification%zu", i);

        if (ImGui::Begin(windowId, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::TextUnformatted(notif.title.c_str());
            if (!notif.message.empty()) {
                ImGui::TextWrapped("%s", notif.message.c_str());
            }

            yOffset += ImGui::GetWindowHeight() + 8.0f;
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        i++;
    }
}

// ============================================================================
// Dialogs
// ============================================================================

DialogResult ConfirmDialog(const char* title, const char* message, bool showCancel) {
    DialogResult result = DialogResult::None;

    ImGui::OpenPopup(title);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", message);
        ImGui::Spacing();

        float buttonWidth = 80.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = buttonWidth * (showCancel ? 3 : 2) + spacing * (showCancel ? 2 : 1);

        CenterNextItem(totalWidth);

        if (Button("Yes", ButtonStyle::Primary, glm::vec2(buttonWidth, 0))) {
            result = DialogResult::Yes;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if (Button("No", ButtonStyle::Default, glm::vec2(buttonWidth, 0))) {
            result = DialogResult::No;
            ImGui::CloseCurrentPopup();
        }

        if (showCancel) {
            ImGui::SameLine();
            if (Button("Cancel", ButtonStyle::Ghost, glm::vec2(buttonWidth, 0))) {
                result = DialogResult::Cancel;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    return result;
}

bool InputDialog(const char* title, const char* label, std::string& value) {
    bool confirmed = false;

    ImGui::OpenPopup(title);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char buffer[256];
        if (ImGui::IsWindowAppearing()) {
            strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
        }

        ImGui::TextUnformatted(label);
        ImGui::InputText("##input", buffer, sizeof(buffer));

        ImGui::Spacing();

        if (Button("OK", ButtonStyle::Primary, glm::vec2(80, 0))) {
            value = buffer;
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (Button("Cancel", ButtonStyle::Default, glm::vec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return confirmed;
}

bool OpenFileDialog(const char* title, std::string& outPath, const char* filter, const char* defaultPath) {
    (void)title; (void)filter; (void)defaultPath;
    // Platform-specific implementation would go here
    outPath = "";
    return false;
}

bool SaveFileDialog(const char* title, std::string& outPath, const char* filter, const char* defaultName) {
    (void)title; (void)filter; (void)defaultName;
    outPath = "";
    return false;
}

bool FolderDialog(const char* title, std::string& outPath, const char* defaultPath) {
    (void)title; (void)defaultPath;
    outPath = "";
    return false;
}

// ============================================================================
// Drag & Drop
// ============================================================================

bool BeginDragSource(const char* type, const void* data, size_t size, const char* previewText) {
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(type, data, size);
        if (previewText) {
            ImGui::TextUnformatted(previewText);
        }
        return true;
    }
    return false;
}

void EndDragSource() {
    ImGui::EndDragDropSource();
}

bool BeginDropTarget() {
    return ImGui::BeginDragDropTarget();
}

const void* AcceptDropPayload(const char* type, size_t* outSize) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type)) {
        if (outSize) *outSize = payload->DataSize;
        return payload->Data;
    }
    return nullptr;
}

void EndDropTarget() {
    ImGui::EndDragDropTarget();
}

// ============================================================================
// Layout Helpers
// ============================================================================

void CenterNextItem(float itemWidth) {
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float offset = (windowWidth - itemWidth) * 0.5f;
    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
}

void RightAlignNextItem(float itemWidth) {
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float offset = windowWidth - itemWidth;
    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
}

float GetContentWidth() {
    return ImGui::GetContentRegionAvail().x;
}

float GetContentHeight() {
    return ImGui::GetContentRegionAvail().y;
}

void BeginHorizontal() {
    s_state.inHorizontalLayout = true;
    s_state.horizontalPos = ImGui::GetCursorPosX();
}

void EndHorizontal() {
    s_state.inHorizontalLayout = false;
}

void Spring(float flex) {
    (void)flex;
    if (s_state.inHorizontalLayout) {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::SameLine();
    }
}

// ============================================================================
// Node Editor Helpers
// ============================================================================

void DrawPin(const glm::vec2& pos, float radius, const glm::vec4& color, bool filled, bool connected) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center(pos.x, pos.y);

    if (filled || connected) {
        drawList->AddCircleFilled(center, radius, EditorTheme::ToImU32(color));
    } else {
        drawList->AddCircle(center, radius, EditorTheme::ToImU32(color), 0, 2.0f);
    }
}

void DrawConnection(const glm::vec2& start, const glm::vec2& end, const glm::vec4& color, float thickness) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float curvature = EditorTheme::Instance().GetSizes().linkCurvature;
    float dx = fabsf(end.x - start.x) * curvature;

    drawList->AddBezierCubic(
        ImVec2(start.x, start.y),
        ImVec2(start.x + dx, start.y),
        ImVec2(end.x - dx, end.y),
        ImVec2(end.x, end.y),
        EditorTheme::ToImU32(color),
        thickness
    );
}

void DrawNodeBackground(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color, float rounding, bool selected) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto& theme = EditorTheme::Instance();

    ImVec2 min(pos.x, pos.y);
    ImVec2 max(pos.x + size.x, pos.y + size.y);

    drawList->AddRectFilled(min, max, EditorTheme::ToImU32(color), rounding);

    if (selected) {
        drawList->AddRect(min, max, EditorTheme::ToImU32(theme.GetColors().nodeSelected), rounding, 0, 2.0f);
    } else {
        drawList->AddRect(min, max, EditorTheme::ToImU32(theme.GetColors().nodeBorder), rounding);
    }
}

void DrawNodeHeader(const glm::vec2& pos, const glm::vec2& size, const char* title, const glm::vec4& color, float rounding) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 min(pos.x, pos.y);
    ImVec2 max(pos.x + size.x, pos.y + size.y);

    drawList->AddRectFilled(min, max, EditorTheme::ToImU32(color), rounding, ImDrawFlags_RoundCornersTop);

    ImVec2 textPos(pos.x + 8, pos.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f);
    drawList->AddText(textPos, EditorTheme::ToImU32(EditorTheme::Instance().GetColors().text), title);
}

void DrawGrid(const glm::vec2& offset, float spacing, const glm::vec4& color, const glm::vec4& majorColor, int majorEvery) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    float startX = fmodf(offset.x, spacing);
    float startY = fmodf(offset.y, spacing);

    int lineIndex = 0;
    for (float x = startX; x < windowSize.x; x += spacing, lineIndex++) {
        ImU32 col = (lineIndex % majorEvery == 0) ? EditorTheme::ToImU32(majorColor) : EditorTheme::ToImU32(color);
        drawList->AddLine(
            ImVec2(windowPos.x + x, windowPos.y),
            ImVec2(windowPos.x + x, windowPos.y + windowSize.y),
            col
        );
    }

    lineIndex = 0;
    for (float y = startY; y < windowSize.y; y += spacing, lineIndex++) {
        ImU32 col = (lineIndex % majorEvery == 0) ? EditorTheme::ToImU32(majorColor) : EditorTheme::ToImU32(color);
        drawList->AddLine(
            ImVec2(windowPos.x, windowPos.y + y),
            ImVec2(windowPos.x + windowSize.x, windowPos.y + y),
            col
        );
    }
}

// ============================================================================
// Timeline Helpers
// ============================================================================

void DrawTimelineRuler(float startTime, float endTime, float currentTime, float height) {
    (void)startTime; (void)endTime; (void)currentTime;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, height);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto& theme = EditorTheme::Instance();

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        EditorTheme::ToImU32(theme.GetColors().panelHeader));

    // Ticks and labels would be drawn here based on time range

    ImGui::Dummy(size);
}

void DrawKeyframe(const glm::vec2& pos, float size, const glm::vec4& color, bool selected) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto& theme = EditorTheme::Instance();

    // Draw diamond shape
    ImVec2 points[4] = {
        ImVec2(pos.x, pos.y - size),
        ImVec2(pos.x + size, pos.y),
        ImVec2(pos.x, pos.y + size),
        ImVec2(pos.x - size, pos.y)
    };

    drawList->AddConvexPolyFilled(points, 4, EditorTheme::ToImU32(color));

    if (selected) {
        drawList->AddPolyline(points, 4, EditorTheme::ToImU32(theme.GetColors().nodeSelected), ImDrawFlags_Closed, 2.0f);
    }
}

bool TimelineScrollArea(const char* id, float& scrollX, float& zoom, float totalLength, const glm::vec2& size) {
    (void)id; (void)totalLength;

    bool changed = false;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::BeginChild(id, ImVec2(size.x, size.y), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    // Handle zoom with ctrl+scroll
    if (ImGui::IsWindowHovered() && io.KeyCtrl) {
        float zoomDelta = io.MouseWheel * 0.1f;
        if (zoomDelta != 0.0f) {
            zoom = glm::clamp(zoom + zoomDelta, 0.1f, 10.0f);
            changed = true;
        }
    }

    // Handle pan
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        scrollX -= io.MouseDelta.x;
        changed = true;
    }

    ImGui::EndChild();
    return changed;
}

} // namespace EditorWidgets
} // namespace Nova
