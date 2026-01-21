#include "UIEditor.hpp"
#include <fstream>
#include <sstream>

namespace Nova {
namespace UI {

// =============================================================================
// Serialization
// =============================================================================

nlohmann::json SerializeWidget(const WidgetPtr& widget) {
    if (!widget) return nullptr;

    nlohmann::json json;
    json["tag"] = widget->GetTagName();

    if (!widget->GetId().empty()) {
        json["id"] = widget->GetId();
    }

    if (!widget->GetClasses().empty()) {
        json["class"] = widget->GetClasses();
    }

    if (!widget->GetText().empty()) {
        json["text"] = widget->GetText();
    }

    // Serialize style
    const UIStyle& style = widget->GetStyle();
    nlohmann::json styleJson;

    // Only serialize non-default values
    if (style.display != Display::Flex) {
        switch (style.display) {
            case Display::Block: styleJson["display"] = "block"; break;
            case Display::Inline: styleJson["display"] = "inline"; break;
            case Display::None: styleJson["display"] = "none"; break;
            case Display::Grid: styleJson["display"] = "grid"; break;
            default: break;
        }
    }

    if (!style.width.IsAuto()) {
        if (style.width.unit == Length::Unit::Pixels) {
            styleJson["width"] = std::to_string(style.width.value) + "px";
        } else if (style.width.unit == Length::Unit::Percent) {
            styleJson["width"] = std::to_string(style.width.value) + "%";
        }
    }

    if (!style.height.IsAuto()) {
        if (style.height.unit == Length::Unit::Pixels) {
            styleJson["height"] = std::to_string(style.height.value) + "px";
        } else if (style.height.unit == Length::Unit::Percent) {
            styleJson["height"] = std::to_string(style.height.value) + "%";
        }
    }

    if (style.backgroundColor.a > 0) {
        char hex[10];
        snprintf(hex, sizeof(hex), "#%02x%02x%02x%02x",
            (int)(style.backgroundColor.r * 255),
            (int)(style.backgroundColor.g * 255),
            (int)(style.backgroundColor.b * 255),
            (int)(style.backgroundColor.a * 255));
        styleJson["backgroundColor"] = hex;
    }

    if (style.padding.top.value > 0 || style.padding.right.value > 0 ||
        style.padding.bottom.value > 0 || style.padding.left.value > 0) {
        styleJson["padding"] = std::to_string(style.padding.top.value) + "px";
    }

    if (style.margin.top.value > 0 || style.margin.right.value > 0 ||
        style.margin.bottom.value > 0 || style.margin.left.value > 0) {
        styleJson["margin"] = std::to_string(style.margin.top.value) + "px";
    }

    if (!styleJson.empty()) {
        json["style"] = styleJson;
    }

    // Serialize children
    if (!widget->GetChildren().empty()) {
        json["children"] = nlohmann::json::array();
        for (const auto& child : widget->GetChildren()) {
            json["children"].push_back(SerializeWidget(child));
        }
    }

    return json;
}

WidgetPtr DeserializeWidget(const nlohmann::json& json) {
    return UIParser::ParseJSON(json);
}

// =============================================================================
// UIEditor Implementation
// =============================================================================

UIEditor::UIEditor() {
    InitializePalette();
    NewDocument();
}

UIEditor::~UIEditor() {
}

void UIEditor::InitializePalette() {
    m_paletteCategories = {
        {
            "Layout",
            {
                {"div", "Container"},
                {"panel", "Panel"},
                {"scrollview", "Scroll View"},
                {"tabs", "Tab Container"},
                {"grid", "Grid"}
            }
        },
        {
            "Basic",
            {
                {"text", "Text"},
                {"label", "Label"},
                {"img", "Image"},
                {"button", "Button"}
            }
        },
        {
            "Input",
            {
                {"input", "Text Input"},
                {"checkbox", "Checkbox"},
                {"select", "Dropdown"},
                {"slider", "Slider"}
            }
        },
        {
            "Display",
            {
                {"progress", "Progress Bar"},
                {"list", "List View"},
                {"tooltip", "Tooltip"},
                {"modal", "Modal Dialog"}
            }
        }
    };
}

void UIEditor::Render() {
    // Main menu bar
    RenderMenuBar();

    // Toolbar
    RenderToolbar();

    // Main docking area
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    // Widget Palette
    if (m_showWidgetPalette) {
        if (ImGui::Begin("Widget Palette", &m_showWidgetPalette, windowFlags)) {
            RenderWidgetPalette();
        }
        ImGui::End();
    }

    // Hierarchy
    if (m_showHierarchy) {
        if (ImGui::Begin("Hierarchy", &m_showHierarchy, windowFlags)) {
            RenderHierarchy();
        }
        ImGui::End();
    }

    // Canvas
    if (ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        RenderCanvas();
    }
    ImGui::End();

    // Property Inspector
    if (m_showInspector) {
        if (ImGui::Begin("Inspector", &m_showInspector, windowFlags)) {
            RenderPropertyInspector();
        }
        ImGui::End();
    }

    // Style Editor
    if (m_showStyleEditor && m_selectedWidget) {
        if (ImGui::Begin("Style", &m_showStyleEditor, windowFlags)) {
            RenderStyleEditor();
        }
        ImGui::End();
    }

    // Binding Editor
    if (m_showBindingEditor && m_selectedWidget) {
        if (ImGui::Begin("Data Bindings", &m_showBindingEditor, windowFlags)) {
            RenderBindingEditor();
        }
        ImGui::End();
    }

    // Preview Controls (when in preview mode)
    if (m_previewMode) {
        if (ImGui::Begin("Preview Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            RenderPreviewControls();
        }
        ImGui::End();
    }
}

void UIEditor::Update(float deltaTime) {
    if (m_rootWidget && m_previewMode) {
        UIContext context;
        m_rootWidget->Update(deltaTime);
    }
}

// =============================================================================
// Document Management
// =============================================================================

void UIEditor::NewDocument() {
    m_rootWidget = std::make_shared<UIPanel>();
    m_rootWidget->SetId("root");
    m_rootWidget->GetStyle().width = Length::Pct(100);
    m_rootWidget->GetStyle().height = Length::Pct(100);
    m_rootWidget->GetStyle().backgroundColor = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f);

    m_currentFilePath.clear();
    m_hasUnsavedChanges = false;
    m_selectedWidget = nullptr;
    ClearUndoHistory();
}

void UIEditor::OpenDocument(const std::filesystem::path& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }

    try {
        nlohmann::json json;
        file >> json;

        auto widget = DeserializeWidget(json);
        if (widget) {
            m_rootWidget = widget;
            m_currentFilePath = filepath;
            m_hasUnsavedChanges = false;
            m_selectedWidget = nullptr;
            ClearUndoHistory();
        }
    } catch (const std::exception& e) {
        // Handle error
    }
}

void UIEditor::SaveDocument() {
    if (m_currentFilePath.empty()) {
        // Would need to show save dialog
        return;
    }
    SaveDocumentAs(m_currentFilePath);
}

void UIEditor::SaveDocumentAs(const std::filesystem::path& filepath) {
    if (!m_rootWidget) return;

    nlohmann::json json = SerializeWidget(m_rootWidget);

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << json.dump(2);
        m_currentFilePath = filepath;
        m_hasUnsavedChanges = false;
    }
}

// =============================================================================
// Selection
// =============================================================================

void UIEditor::SetSelectedWidget(WidgetPtr widget) {
    if (m_selectedWidget != widget) {
        if (m_selectedWidget) {
            m_selectionHistory.push_back(m_selectedWidget);
            if (m_selectionHistory.size() > 20) {
                m_selectionHistory.erase(m_selectionHistory.begin());
            }
        }
        m_selectedWidget = widget;
    }
}

void UIEditor::SelectParent() {
    if (m_selectedWidget) {
        if (auto parent = m_selectedWidget->GetParent()) {
            SetSelectedWidget(parent);
        }
    }
}

void UIEditor::SelectFirstChild() {
    if (m_selectedWidget && !m_selectedWidget->GetChildren().empty()) {
        SetSelectedWidget(m_selectedWidget->GetChildren()[0]);
    }
}

void UIEditor::SelectNextSibling() {
    if (!m_selectedWidget) return;

    auto parent = m_selectedWidget->GetParent();
    if (!parent) return;

    const auto& siblings = parent->GetChildren();
    for (size_t i = 0; i < siblings.size() - 1; ++i) {
        if (siblings[i] == m_selectedWidget) {
            SetSelectedWidget(siblings[i + 1]);
            return;
        }
    }
}

void UIEditor::SelectPrevSibling() {
    if (!m_selectedWidget) return;

    auto parent = m_selectedWidget->GetParent();
    if (!parent) return;

    const auto& siblings = parent->GetChildren();
    for (size_t i = 1; i < siblings.size(); ++i) {
        if (siblings[i] == m_selectedWidget) {
            SetSelectedWidget(siblings[i - 1]);
            return;
        }
    }
}

// =============================================================================
// Editing Operations
// =============================================================================

void UIEditor::DeleteSelected() {
    if (!m_selectedWidget || m_selectedWidget == m_rootWidget) return;

    auto parent = m_selectedWidget->GetParent();
    if (!parent) return;

    auto widget = m_selectedWidget;
    size_t index = 0;
    const auto& children = parent->GetChildren();
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] == widget) {
            index = i;
            break;
        }
    }

    PushUndo("Delete widget",
        [parent, widget, index]() { parent->InsertChild(widget, index); },
        [parent, widget]() { parent->RemoveChild(widget); }
    );

    parent->RemoveChild(m_selectedWidget);
    m_selectedWidget = parent;
    m_hasUnsavedChanges = true;
}

void UIEditor::DuplicateSelected() {
    if (!m_selectedWidget || m_selectedWidget == m_rootWidget) return;

    auto parent = m_selectedWidget->GetParent();
    if (!parent) return;

    nlohmann::json json = SerializeWidget(m_selectedWidget);
    auto duplicate = DeserializeWidget(json);
    if (duplicate) {
        duplicate->SetId(duplicate->GetId() + "_copy");
        parent->AppendChild(duplicate);

        PushUndo("Duplicate widget",
            [parent, duplicate]() { parent->RemoveChild(duplicate); },
            [parent, duplicate]() { parent->AppendChild(duplicate); }
        );

        SetSelectedWidget(duplicate);
        m_hasUnsavedChanges = true;
    }
}

void UIEditor::CopySelected() {
    if (m_selectedWidget) {
        m_clipboard = SerializeWidget(m_selectedWidget);
    }
}

void UIEditor::PasteToSelected() {
    if (m_clipboard.is_null()) return;

    WidgetPtr target = m_selectedWidget ? m_selectedWidget : m_rootWidget;
    auto pasted = DeserializeWidget(m_clipboard);
    if (pasted && target) {
        target->AppendChild(pasted);

        PushUndo("Paste widget",
            [target, pasted]() { target->RemoveChild(pasted); },
            [target, pasted]() { target->AppendChild(pasted); }
        );

        SetSelectedWidget(pasted);
        m_hasUnsavedChanges = true;
    }
}

void UIEditor::CutSelected() {
    CopySelected();
    DeleteSelected();
}

void UIEditor::MoveSelectedUp() {
    if (!m_selectedWidget) return;

    auto parent = m_selectedWidget->GetParent();
    if (!parent) return;

    const auto& children = parent->GetChildren();
    for (size_t i = 1; i < children.size(); ++i) {
        if (children[i] == m_selectedWidget) {
            // Swap with previous sibling
            // Would need to implement swap in UIWidget
            m_hasUnsavedChanges = true;
            return;
        }
    }
}

void UIEditor::MoveSelectedDown() {
    if (!m_selectedWidget) return;

    auto parent = m_selectedWidget->GetParent();
    if (!parent) return;

    const auto& children = parent->GetChildren();
    for (size_t i = 0; i < children.size() - 1; ++i) {
        if (children[i] == m_selectedWidget) {
            // Swap with next sibling
            m_hasUnsavedChanges = true;
            return;
        }
    }
}

// =============================================================================
// Undo/Redo
// =============================================================================

void UIEditor::Undo() {
    if (m_undoStack.empty()) return;

    auto action = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    action.undo();
    m_redoStack.push_back(std::move(action));
    m_hasUnsavedChanges = true;
}

void UIEditor::Redo() {
    if (m_redoStack.empty()) return;

    auto action = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    action.redo();
    m_undoStack.push_back(std::move(action));
    m_hasUnsavedChanges = true;
}

bool UIEditor::CanUndo() const {
    return !m_undoStack.empty();
}

bool UIEditor::CanRedo() const {
    return !m_redoStack.empty();
}

void UIEditor::PushUndo(const std::string& description,
                       std::function<void()> undo,
                       std::function<void()> redo) {
    m_undoStack.push_back({description, std::move(undo), std::move(redo)});
    m_redoStack.clear();

    if (m_undoStack.size() > MaxUndoStackSize) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void UIEditor::ClearUndoHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

// =============================================================================
// ImGui Panels
// =============================================================================

void UIEditor::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) NewDocument();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) { /* Show file dialog */ }
            if (ImGui::MenuItem("Save", "Ctrl+S")) SaveDocument();
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) { /* Show file dialog */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { /* Close editor */ }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, m_selectedWidget != nullptr)) CutSelected();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selectedWidget != nullptr)) CopySelected();
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.is_null())) PasteToSelected();
            if (ImGui::MenuItem("Delete", "Del", false, m_selectedWidget != nullptr)) DeleteSelected();
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selectedWidget != nullptr)) DuplicateSelected();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Widget Palette", nullptr, &m_showWidgetPalette);
            ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Style Editor", nullptr, &m_showStyleEditor);
            ImGui::MenuItem("Binding Editor", nullptr, &m_showBindingEditor);
            ImGui::Separator();
            ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
            ImGui::MenuItem("Snap to Grid", nullptr, &m_snapToGrid);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preview")) {
            if (ImGui::MenuItem("Toggle Preview Mode", "F5")) {
                m_previewMode = !m_previewMode;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void UIEditor::RenderToolbar() {
    ImGui::BeginChild("Toolbar", ImVec2(0, 32), true, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::Button("New")) NewDocument();
    ImGui::SameLine();
    if (ImGui::Button("Save")) SaveDocument();
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    ImGui::BeginDisabled(!CanUndo());
    if (ImGui::Button("Undo")) Undo();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::BeginDisabled(!CanRedo());
    if (ImGui::Button("Redo")) Redo();
    ImGui::EndDisabled();
    ImGui::SameLine();

    ImGui::Separator();
    ImGui::SameLine();

    bool previewActive = m_previewMode;
    if (previewActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button(m_previewMode ? "Stop Preview" : "Preview")) {
        m_previewMode = !m_previewMode;
    }
    if (previewActive) ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    ImGui::Text("Zoom: %.0f%%", m_canvasZoom * 100);
    ImGui::SameLine();
    if (ImGui::Button("-")) m_canvasZoom = std::max(0.25f, m_canvasZoom - 0.25f);
    ImGui::SameLine();
    if (ImGui::Button("+")) m_canvasZoom = std::min(4.0f, m_canvasZoom + 0.25f);
    ImGui::SameLine();
    if (ImGui::Button("100%")) m_canvasZoom = 1.0f;

    ImGui::EndChild();
}

void UIEditor::RenderWidgetPalette() {
    for (const auto& category : m_paletteCategories) {
        if (ImGui::CollapsingHeader(category.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& [type, displayName] : category.widgets) {
                if (ImGui::Selectable(displayName.c_str())) {
                    // Start drag
                    m_dragNewWidgetType = type;
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("WIDGET_TYPE", type.c_str(), type.size() + 1);
                    ImGui::Text("Create %s", displayName.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
}

void UIEditor::RenderHierarchy() {
    std::function<void(WidgetPtr, int)> renderNode = [&](WidgetPtr widget, int depth) {
        if (!widget) return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (widget->GetChildren().empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (widget == m_selectedWidget) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string label = widget->GetTagName();
        if (!widget->GetId().empty()) {
            label += " #" + widget->GetId();
        }
        if (!widget->GetClasses().empty()) {
            label += " ." + widget->GetClasses()[0];
        }

        bool opened = ImGui::TreeNodeEx(widget.get(), flags, "%s", label.c_str());

        if (ImGui::IsItemClicked()) {
            SetSelectedWidget(widget);
        }

        // Drop target for reordering
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
                std::string type((const char*)payload->Data);
                auto newWidget = CreateWidgetFromPalette(type);
                if (newWidget) {
                    widget->AppendChild(newWidget);
                    SetSelectedWidget(newWidget);
                    m_hasUnsavedChanges = true;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (opened) {
            for (const auto& child : widget->GetChildren()) {
                renderNode(child, depth + 1);
            }
            ImGui::TreePop();
        }
    };

    renderNode(m_rootWidget, 0);
}

void UIEditor::RenderCanvas() {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Draw background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(40, 44, 52, 255));

    // Draw grid
    if (m_showGrid) {
        float gridStep = m_gridSize * m_canvasZoom;
        for (float x = fmodf(m_canvasOffset.x, gridStep); x < canvasSize.x; x += gridStep) {
            drawList->AddLine(
                ImVec2(canvasPos.x + x, canvasPos.y),
                ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                IM_COL32(60, 64, 72, 100));
        }
        for (float y = fmodf(m_canvasOffset.y, gridStep); y < canvasSize.y; y += gridStep) {
            drawList->AddLine(
                ImVec2(canvasPos.x, canvasPos.y + y),
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                IM_COL32(60, 64, 72, 100));
        }
    }

    // Draw canvas bounds
    glm::vec2 canvasOrigin(
        canvasPos.x + m_canvasOffset.x + (canvasSize.x - m_canvasSize.x * m_canvasZoom) * 0.5f,
        canvasPos.y + m_canvasOffset.y + (canvasSize.y - m_canvasSize.y * m_canvasZoom) * 0.5f
    );

    drawList->AddRect(
        ImVec2(canvasOrigin.x, canvasOrigin.y),
        ImVec2(canvasOrigin.x + m_canvasSize.x * m_canvasZoom,
               canvasOrigin.y + m_canvasSize.y * m_canvasZoom),
        IM_COL32(100, 100, 100, 255), 0.0f, 0, 2.0f);

    // Render widget preview
    if (m_rootWidget) {
        // Layout pass
        m_rootWidget->Layout(glm::vec4(0, 0, m_canvasSize.x, m_canvasSize.y));

        // Render widgets
        RenderWidgetInCanvas(m_rootWidget, canvasOrigin);

        // Selection overlay
        if (m_selectedWidget && !m_previewMode) {
            const auto& rect = m_selectedWidget->GetComputedRect();
            ImVec2 min(canvasOrigin.x + rect.x * m_canvasZoom,
                      canvasOrigin.y + rect.y * m_canvasZoom);
            ImVec2 max(min.x + rect.z * m_canvasZoom, min.y + rect.w * m_canvasZoom);

            drawList->AddRect(min, max, IM_COL32(0, 150, 255, 255), 0.0f, 0, 2.0f);

            // Resize handles
            float handleSize = 6.0f;
            ImVec2 corners[] = {
                min, ImVec2(max.x, min.y), max, ImVec2(min.x, max.y)
            };
            for (const auto& corner : corners) {
                drawList->AddRectFilled(
                    ImVec2(corner.x - handleSize * 0.5f, corner.y - handleSize * 0.5f),
                    ImVec2(corner.x + handleSize * 0.5f, corner.y + handleSize * 0.5f),
                    IM_COL32(0, 150, 255, 255));
            }
        }
    }

    // Handle drop
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
            std::string type((const char*)payload->Data);
            auto newWidget = CreateWidgetFromPalette(type);
            if (newWidget && m_rootWidget) {
                m_rootWidget->AppendChild(newWidget);
                SetSelectedWidget(newWidget);
                m_hasUnsavedChanges = true;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Handle click to select
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !m_previewMode) {
        ImVec2 mousePos = ImGui::GetMousePos();

        // Convert mouse position to canvas coordinates
        glm::vec2 canvasMousePos(
            (mousePos.x - canvasOrigin.x) / m_canvasZoom,
            (mousePos.y - canvasOrigin.y) / m_canvasZoom
        );

        // Hit test widgets (front-to-back, deepest child first)
        std::function<WidgetPtr(WidgetPtr)> hitTest = [&](WidgetPtr widget) -> WidgetPtr {
            if (!widget || !widget->GetStyle().visible) return nullptr;

            // Check children first (reverse order for front-to-back)
            const auto& children = widget->GetChildren();
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                if (auto hit = hitTest(*it)) {
                    return hit;
                }
            }

            // Check this widget
            const auto& rect = widget->GetComputedRect();
            if (canvasMousePos.x >= rect.x && canvasMousePos.x <= rect.x + rect.z &&
                canvasMousePos.y >= rect.y && canvasMousePos.y <= rect.y + rect.w) {
                return widget;
            }

            return nullptr;
        };

        if (auto hitWidget = hitTest(m_rootWidget)) {
            SetSelectedWidget(hitWidget);
        }
    }

    // Handle pan with middle mouse
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(2)) {
        ImVec2 delta = ImGui::GetMouseDragDelta(2);
        m_canvasOffset.x += delta.x;
        m_canvasOffset.y += delta.y;
        ImGui::ResetMouseDragDelta(2);
    }

    // Handle zoom with scroll
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            m_canvasZoom = std::clamp(m_canvasZoom + wheel * 0.1f, 0.25f, 4.0f);
        }
    }
}

void UIEditor::RenderWidgetInCanvas(WidgetPtr widget, const glm::vec2& offset) {
    if (!widget || !widget->GetStyle().visible) return;

    const auto& rect = widget->GetComputedRect();
    const auto& style = widget->GetStyle();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 min(offset.x + rect.x * m_canvasZoom, offset.y + rect.y * m_canvasZoom);
    ImVec2 max(min.x + rect.z * m_canvasZoom, min.y + rect.w * m_canvasZoom);

    // Background
    if (style.backgroundColor.a > 0) {
        ImU32 bgColor = IM_COL32(
            (int)(style.backgroundColor.r * 255),
            (int)(style.backgroundColor.g * 255),
            (int)(style.backgroundColor.b * 255),
            (int)(style.backgroundColor.a * 255)
        );
        drawList->AddRectFilled(min, max, bgColor, style.border.radius * m_canvasZoom);
    }

    // Border
    if (style.border.width > 0) {
        ImU32 borderColor = IM_COL32(
            (int)(style.border.color.r * 255),
            (int)(style.border.color.g * 255),
            (int)(style.border.color.b * 255),
            (int)(style.border.color.a * 255)
        );
        drawList->AddRect(min, max, borderColor, style.border.radius * m_canvasZoom,
                         0, style.border.width * m_canvasZoom);
    }

    // Text
    if (!widget->GetText().empty()) {
        ImU32 textColor = IM_COL32(
            (int)(style.color.r * 255),
            (int)(style.color.g * 255),
            (int)(style.color.b * 255),
            (int)(style.color.a * 255)
        );
        drawList->AddText(ImVec2(min.x + 4, min.y + 4), textColor, widget->GetText().c_str());
    }

    // Children
    for (const auto& child : widget->GetChildren()) {
        RenderWidgetInCanvas(child, offset);
    }
}

void UIEditor::RenderPropertyInspector() {
    if (!m_selectedWidget) {
        ImGui::TextDisabled("No widget selected");
        return;
    }

    // Identity
    if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
        char idBuffer[256];
        strncpy(idBuffer, m_selectedWidget->GetId().c_str(), sizeof(idBuffer));
        if (ImGui::InputText("ID", idBuffer, sizeof(idBuffer))) {
            m_selectedWidget->SetId(idBuffer);
            m_hasUnsavedChanges = true;
        }

        ImGui::Text("Tag: %s", m_selectedWidget->GetTagName().c_str());

        // Classes
        std::string classStr;
        for (const auto& cls : m_selectedWidget->GetClasses()) {
            if (!classStr.empty()) classStr += " ";
            classStr += cls;
        }
        char classBuffer[512];
        strncpy(classBuffer, classStr.c_str(), sizeof(classBuffer));
        if (ImGui::InputText("Classes", classBuffer, sizeof(classBuffer))) {
            m_selectedWidget->SetClass(classBuffer);
            m_hasUnsavedChanges = true;
        }
    }

    // Content
    if (ImGui::CollapsingHeader("Content", ImGuiTreeNodeFlags_DefaultOpen)) {
        char textBuffer[1024];
        strncpy(textBuffer, m_selectedWidget->GetText().c_str(), sizeof(textBuffer));
        if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(-1, 60))) {
            m_selectedWidget->SetText(textBuffer);
            m_hasUnsavedChanges = true;
        }
    }
}

void UIEditor::RenderStyleEditor() {
    if (!m_selectedWidget) return;

    UIStyle& style = m_selectedWidget->GetStyle();

    if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* displayItems[] = {"Flex", "Block", "Inline", "None", "Grid"};
        int displayIndex = static_cast<int>(style.display);
        if (ImGui::Combo("Display", &displayIndex, displayItems, IM_ARRAYSIZE(displayItems))) {
            style.display = static_cast<Display>(displayIndex);
            m_hasUnsavedChanges = true;
        }

        const char* directionItems[] = {"Row", "Column", "Row Reverse", "Column Reverse"};
        int dirIndex = static_cast<int>(style.flexDirection);
        if (ImGui::Combo("Direction", &dirIndex, directionItems, IM_ARRAYSIZE(directionItems))) {
            style.flexDirection = static_cast<LayoutDirection>(dirIndex);
            m_hasUnsavedChanges = true;
        }
    }

    if (ImGui::CollapsingHeader("Size", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderLengthEditor("Width", style.width);
        RenderLengthEditor("Height", style.height);
        RenderLengthEditor("Min Width", style.minWidth);
        RenderLengthEditor("Min Height", style.minHeight);
    }

    if (ImGui::CollapsingHeader("Spacing")) {
        RenderBoxSpacingEditor("Margin", style.margin);
        RenderBoxSpacingEditor("Padding", style.padding);
        ImGui::DragFloat("Gap", &style.gap, 1.0f, 0.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("Background")) {
        RenderColorEditor("Background", style.backgroundColor);
    }

    if (ImGui::CollapsingHeader("Border")) {
        ImGui::DragFloat("Width", &style.border.width, 0.1f, 0.0f, 10.0f);
        RenderColorEditor("Color", style.border.color);
        ImGui::DragFloat("Radius", &style.border.radius, 0.5f, 0.0f, 50.0f);
    }

    if (ImGui::CollapsingHeader("Text")) {
        RenderColorEditor("Color", style.color);
        ImGui::DragFloat("Font Size", &style.fontSize, 0.5f, 8.0f, 72.0f);
    }
}

void UIEditor::RenderBindingEditor() {
    if (!m_selectedWidget) return;

    ImGui::Text("Data Bindings");
    ImGui::Separator();

    // Add new binding
    static char sourcePath[256] = "";
    static char targetProp[64] = "text";
    static int bindingMode = 0;

    ImGui::InputText("Source Path", sourcePath, sizeof(sourcePath));
    ImGui::InputText("Target Property", targetProp, sizeof(targetProp));

    const char* modeItems[] = {"One Way", "Two Way", "One Time", "One Way to Source"};
    ImGui::Combo("Mode", &bindingMode, modeItems, IM_ARRAYSIZE(modeItems));

    if (ImGui::Button("Add Binding")) {
        m_selectedWidget->BindProperty(targetProp, sourcePath,
            static_cast<BindingMode>(bindingMode));
        sourcePath[0] = '\0';
    }

    ImGui::Separator();
    ImGui::Text("Current Bindings:");
    // Would list current bindings here
}

void UIEditor::RenderPreviewControls() {
    if (ImGui::Button("Exit Preview")) {
        m_previewMode = false;
    }

    ImGui::Separator();
    ImGui::Text("Preview Data:");
    // Would show data context controls here
}

// =============================================================================
// Helpers
// =============================================================================

void UIEditor::RenderLengthEditor(const std::string& label, Length& length) {
    float value = length.value;
    int unit = static_cast<int>(length.unit);

    ImGui::PushID(label.c_str());

    ImGui::SetNextItemWidth(80);
    bool changed = ImGui::DragFloat("##value", &value, 1.0f);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    const char* unitItems[] = {"px", "%", "em", "auto", "vw", "vh"};
    changed |= ImGui::Combo("##unit", &unit, unitItems, IM_ARRAYSIZE(unitItems));

    ImGui::SameLine();
    ImGui::Text("%s", label.c_str());

    if (changed) {
        length.value = value;
        length.unit = static_cast<Length::Unit>(unit);
        m_hasUnsavedChanges = true;
    }

    ImGui::PopID();
}

void UIEditor::RenderColorEditor(const std::string& label, glm::vec4& color) {
    float col[4] = {color.r, color.g, color.b, color.a};
    if (ImGui::ColorEdit4(label.c_str(), col)) {
        color = glm::vec4(col[0], col[1], col[2], col[3]);
        m_hasUnsavedChanges = true;
    }
}

void UIEditor::RenderBoxSpacingEditor(const std::string& label, BoxSpacing& spacing) {
    if (ImGui::TreeNode(label.c_str())) {
        RenderLengthEditor("Top", spacing.top);
        RenderLengthEditor("Right", spacing.right);
        RenderLengthEditor("Bottom", spacing.bottom);
        RenderLengthEditor("Left", spacing.left);
        ImGui::TreePop();
    }
}

WidgetPtr UIEditor::CreateWidgetFromPalette(const std::string& type) {
    return UIWidgetFactory::Instance().Create(type);
}

void UIEditor::SetPreviewData(void* data, const Reflect::TypeInfo* type) {
    m_previewData = data;
    m_previewDataType = type;

    if (m_rootWidget && data && type) {
        m_rootWidget->SetDataContext(data, type);
    }
}

} // namespace UI
} // namespace Nova
