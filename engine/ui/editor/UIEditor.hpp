#pragma once

#include "../widgets/UIWidget.hpp"
#include "../widgets/UITemplate.hpp"
#include "../widgets/CoreWidgets.hpp"
#include "../../reflection/TypeRegistry.hpp"
#include <imgui.h>
#include <filesystem>

namespace Nova {
namespace UI {

/**
 * @brief Visual UI Editor for designing and editing UI layouts
 *
 * Features:
 * - Drag and drop widget placement
 * - Property inspector with reflection-based editing
 * - Live preview with data binding
 * - Template save/load
 * - Undo/redo support
 */
class UIEditor {
public:
    UIEditor();
    ~UIEditor();

    /**
     * @brief Render the editor UI using ImGui
     */
    void Render();

    /**
     * @brief Update editor state
     */
    void Update(float deltaTime);

    // =========================================================================
    // Document Management
    // =========================================================================

    void NewDocument();
    void OpenDocument(const std::filesystem::path& filepath);
    void SaveDocument();
    void SaveDocumentAs(const std::filesystem::path& filepath);
    bool HasUnsavedChanges() const { return m_hasUnsavedChanges; }

    // =========================================================================
    // Selection
    // =========================================================================

    void SetSelectedWidget(WidgetPtr widget);
    WidgetPtr GetSelectedWidget() const { return m_selectedWidget; }

    void SelectParent();
    void SelectFirstChild();
    void SelectNextSibling();
    void SelectPrevSibling();

    // =========================================================================
    // Editing Operations
    // =========================================================================

    void DeleteSelected();
    void DuplicateSelected();
    void CopySelected();
    void PasteToSelected();
    void CutSelected();

    void MoveSelectedUp();
    void MoveSelectedDown();

    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    // =========================================================================
    // Preview Mode
    // =========================================================================

    void SetPreviewMode(bool preview) { m_previewMode = preview; }
    bool IsPreviewMode() const { return m_previewMode; }

    void SetPreviewData(void* data, const Reflect::TypeInfo* type);

    // =========================================================================
    // Editor Configuration
    // =========================================================================

    void SetShowGrid(bool show) { m_showGrid = show; }
    void SetGridSize(float size) { m_gridSize = size; }
    void SetSnapToGrid(bool snap) { m_snapToGrid = snap; }

private:
    // =========================================================================
    // ImGui Panels
    // =========================================================================

    void RenderMenuBar();
    void RenderToolbar();
    void RenderWidgetPalette();
    void RenderHierarchy();
    void RenderCanvas();
    void RenderPropertyInspector();
    void RenderStyleEditor();
    void RenderBindingEditor();
    void RenderPreviewControls();

    // =========================================================================
    // Widget Rendering in Canvas
    // =========================================================================

    void RenderWidgetInCanvas(WidgetPtr widget, const glm::vec2& offset);
    void RenderSelectionOverlay();
    void RenderGuides();
    void RenderGrid();

    // =========================================================================
    // Property Inspector Helpers
    // =========================================================================

    void RenderPropertyEditor(const std::string& label, std::any& value, const std::string& type);
    void RenderStylePropertyEditor(UIStyle& style);
    void RenderLengthEditor(const std::string& label, Length& length);
    void RenderColorEditor(const std::string& label, glm::vec4& color);
    void RenderBoxSpacingEditor(const std::string& label, BoxSpacing& spacing);

    // =========================================================================
    // Drag and Drop
    // =========================================================================

    void HandleDragStart(WidgetPtr widget);
    void HandleDragUpdate(const glm::vec2& mousePos);
    void HandleDragEnd();
    void HandleDropOnWidget(WidgetPtr target);

    WidgetPtr CreateWidgetFromPalette(const std::string& type);

    // =========================================================================
    // Undo System
    // =========================================================================

    struct UndoAction {
        std::string description;
        std::function<void()> undo;
        std::function<void()> redo;
    };

    void PushUndo(const std::string& description, std::function<void()> undo, std::function<void()> redo);
    void ClearUndoHistory();

    // =========================================================================
    // State
    // =========================================================================

    // Document
    WidgetPtr m_rootWidget;
    std::filesystem::path m_currentFilePath;
    bool m_hasUnsavedChanges = false;

    // Selection
    WidgetPtr m_selectedWidget;
    std::vector<WidgetWeakPtr> m_selectionHistory;

    // Clipboard
    nlohmann::json m_clipboard;

    // Drag state
    bool m_isDragging = false;
    WidgetPtr m_dragWidget;
    glm::vec2 m_dragOffset{0.0f};
    std::string m_dragNewWidgetType;

    // View
    glm::vec2 m_canvasOffset{0.0f};
    float m_canvasZoom = 1.0f;
    glm::vec2 m_canvasSize{800.0f, 600.0f};

    // Grid
    bool m_showGrid = true;
    float m_gridSize = 8.0f;
    bool m_snapToGrid = true;

    // Preview
    bool m_previewMode = false;
    void* m_previewData = nullptr;
    const Reflect::TypeInfo* m_previewDataType = nullptr;

    // Undo/Redo
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    static constexpr size_t MaxUndoStackSize = 100;

    // UI State
    bool m_showWidgetPalette = true;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showStyleEditor = true;
    bool m_showBindingEditor = false;

    // Widget palette categories
    struct PaletteCategory {
        std::string name;
        std::vector<std::pair<std::string, std::string>> widgets; // {type, displayName}
    };
    std::vector<PaletteCategory> m_paletteCategories;

    void InitializePalette();
};

/**
 * @brief Serialize a widget tree to JSON
 */
nlohmann::json SerializeWidget(const WidgetPtr& widget);

/**
 * @brief Deserialize a widget tree from JSON
 */
WidgetPtr DeserializeWidget(const nlohmann::json& json);

} // namespace UI
} // namespace Nova
