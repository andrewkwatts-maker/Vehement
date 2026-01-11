#pragma once

#include "EditorTheme.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>
#include <variant>
#include <optional>
#include <memory>

namespace Nova {

/**
 * @brief Common UI widgets for all editor panels
 *
 * Provides consistent, themed UI components:
 * - Property editors (int, float, vec2/3/4, color, string, enum)
 * - Searchable dropdowns and combo boxes
 * - Tree views with drag-drop
 * - Panels with collapsible headers
 * - Toolbar buttons with icons
 * - Status bars
 * - Modal dialogs
 * - Toast notifications
 * - Progress indicators
 */
namespace EditorWidgets {

// ============================================================================
// Property Editors
// ============================================================================

/**
 * @brief Edit a labeled property with undo support
 * @return true if value changed
 */
bool Property(const char* label, int& value, int min = INT_MIN, int max = INT_MAX, const char* format = "%d");
bool Property(const char* label, float& value, float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f, const char* format = "%.3f");
bool Property(const char* label, double& value, double min = -DBL_MAX, double max = DBL_MAX, double speed = 0.1);
bool Property(const char* label, bool& value);
bool Property(const char* label, std::string& value, size_t maxLength = 256);
bool Property(const char* label, glm::vec2& value, float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f);
bool Property(const char* label, glm::vec3& value, float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f);
bool Property(const char* label, glm::vec4& value, float min = -FLT_MAX, float max = FLT_MAX, float speed = 0.1f);

/**
 * @brief Color property with picker
 */
bool ColorProperty(const char* label, glm::vec3& color, bool showAlpha = false);
bool ColorProperty(const char* label, glm::vec4& color);

/**
 * @brief Angle property (degrees with circular slider option)
 */
bool AngleProperty(const char* label, float& degrees, float min = -360.0f, float max = 360.0f);

/**
 * @brief Enum property with dropdown
 */
bool EnumProperty(const char* label, int& value, const char* const* names, int count);

template<typename E>
bool EnumProperty(const char* label, E& value, const char* const* names, int count) {
    int intVal = static_cast<int>(value);
    bool changed = EnumProperty(label, intVal, names, count);
    if (changed) value = static_cast<E>(intVal);
    return changed;
}

/**
 * @brief Flags property with checkboxes
 */
bool FlagsProperty(const char* label, uint32_t& flags, const char* const* names, int count);

/**
 * @brief Slider property with range
 */
bool SliderProperty(const char* label, int& value, int min, int max, const char* format = "%d");
bool SliderProperty(const char* label, float& value, float min, float max, const char* format = "%.3f");

/**
 * @brief Asset reference property with browse button
 */
bool AssetProperty(const char* label, std::string& assetPath, const char* filter = "*.*", const char* assetType = nullptr);

/**
 * @brief Object reference property
 */
bool ObjectProperty(const char* label, uint64_t& objectId, const char* typeName = nullptr);

/**
 * @brief Curve property with mini editor
 */
struct CurvePoint {
    float time;
    float value;
    float inTangent = 0.0f;
    float outTangent = 0.0f;
};
bool CurveProperty(const char* label, std::vector<CurvePoint>& curve, float minTime = 0.0f, float maxTime = 1.0f, float minValue = 0.0f, float maxValue = 1.0f);

/**
 * @brief Gradient property
 */
struct GradientKey {
    float position;
    glm::vec4 color;
};
bool GradientProperty(const char* label, std::vector<GradientKey>& gradient);

// ============================================================================
// Panels and Headers
// ============================================================================

/**
 * @brief Begin a property panel (for consistent layout)
 */
bool BeginPropertyPanel(const char* name, bool* open = nullptr, bool defaultOpen = true);
void EndPropertyPanel();

/**
 * @brief Collapsible header with arrow
 */
bool CollapsingHeader(const char* label, bool* open = nullptr, bool defaultOpen = false);

/**
 * @brief Subheader within a panel
 */
void SubHeader(const char* label);

/**
 * @brief Horizontal separator with optional label
 */
void Separator(const char* label = nullptr);

/**
 * @brief Begin a section with indentation
 */
void BeginSection(const char* label = nullptr);
void EndSection();

// ============================================================================
// Buttons and Actions
// ============================================================================

/**
 * @brief Standard button styles
 */
enum class ButtonStyle {
    Default,
    Primary,    // Accent colored
    Success,    // Green
    Warning,    // Yellow
    Danger,     // Red
    Ghost,      // Transparent until hover
    Link        // Text-only with underline
};

bool Button(const char* label, ButtonStyle style = ButtonStyle::Default, const glm::vec2& size = glm::vec2(0, 0));
bool IconButton(const char* icon, const char* tooltip = nullptr, ButtonStyle style = ButtonStyle::Default, float size = 0.0f);
bool ToggleButton(const char* label, bool& active, ButtonStyle style = ButtonStyle::Default, const glm::vec2& size = glm::vec2(0, 0));

/**
 * @brief Button group (horizontal)
 */
void BeginButtonGroup();
void EndButtonGroup();

/**
 * @brief Toolbar with icon buttons
 */
void BeginToolbar(const char* id, float height = 0.0f);
void EndToolbar();
bool ToolbarButton(const char* icon, const char* tooltip, bool selected = false);
void ToolbarSeparator();
void ToolbarSpacer();

// ============================================================================
// Dropdowns and Selection
// ============================================================================

/**
 * @brief Searchable combo box
 */
bool SearchableCombo(const char* label, int& selectedIndex, const std::vector<std::string>& items, const char* previewOverride = nullptr);

/**
 * @brief Filterable list box
 */
bool FilteredListBox(const char* label, int& selectedIndex, const std::vector<std::string>& items, char* filterBuffer, size_t filterSize, float height = 0.0f);

/**
 * @brief Tag selection (multiple tags)
 */
bool TagSelector(const char* label, std::vector<std::string>& selectedTags, const std::vector<std::string>& availableTags);

// ============================================================================
// Tree Views
// ============================================================================

/**
 * @brief Tree node flags
 */
enum class TreeNodeFlags : uint32_t {
    None = 0,
    Selected = 1 << 0,
    OpenOnArrow = 1 << 1,
    OpenOnDoubleClick = 1 << 2,
    Leaf = 1 << 3,
    DefaultOpen = 1 << 4,
    SpanFullWidth = 1 << 5,
    AllowDragDrop = 1 << 6
};

inline TreeNodeFlags operator|(TreeNodeFlags a, TreeNodeFlags b) {
    return static_cast<TreeNodeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(TreeNodeFlags a, TreeNodeFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief Tree node with customizable appearance
 */
bool TreeNode(const char* label, TreeNodeFlags flags = TreeNodeFlags::None, const char* icon = nullptr);
void TreePop();

/**
 * @brief Tree node with payload for drag-drop
 */
bool TreeNodeEx(const char* id, const char* label, void* payload, size_t payloadSize, const char* payloadType, TreeNodeFlags flags = TreeNodeFlags::None, const char* icon = nullptr);

/**
 * @brief Check if tree node is being dragged
 */
bool IsTreeNodeDragging();

/**
 * @brief Get drag-drop payload for tree nodes
 */
const void* GetTreeDragDropPayload(const char* type, size_t* outSize = nullptr);

// ============================================================================
// Input Fields
// ============================================================================

/**
 * @brief Search input with icon and clear button
 */
bool SearchInput(const char* id, char* buffer, size_t bufferSize, const char* hint = "Search...");

/**
 * @brief Multi-line text input
 */
bool TextAreaInput(const char* label, std::string& text, const glm::vec2& size = glm::vec2(0, 100), bool readOnly = false);

/**
 * @brief Code input with syntax highlighting (basic)
 */
bool CodeInput(const char* label, std::string& code, const char* language = nullptr, const glm::vec2& size = glm::vec2(0, 200));

/**
 * @brief File path input with browse button
 */
bool PathInput(const char* label, std::string& path, const char* filter = "*.*", bool folder = false);

// ============================================================================
// Visual Feedback
// ============================================================================

/**
 * @brief Progress bar
 */
void ProgressBar(float fraction, const glm::vec2& size = glm::vec2(-1, 0), const char* overlay = nullptr);

/**
 * @brief Loading spinner
 */
void Spinner(const char* label, float radius = 8.0f, float thickness = 2.0f);

/**
 * @brief Badge (small label)
 */
void Badge(const char* text, const glm::vec4& color);

/**
 * @brief Tooltip with rich content
 */
void BeginTooltipEx();
void EndTooltipEx();
void SetTooltip(const char* text);

/**
 * @brief Info/warning/error icons with tooltips
 */
void InfoMarker(const char* text);
void WarningMarker(const char* text);
void ErrorMarker(const char* text);

// ============================================================================
// Status Bar
// ============================================================================

/**
 * @brief Begin status bar at bottom of window
 */
void BeginStatusBar();
void EndStatusBar();
void StatusBarItem(const char* text, float width = 0.0f);
void StatusBarSeparator();

// ============================================================================
// Notifications
// ============================================================================

enum class NotificationType {
    Info,
    Success,
    Warning,
    Error
};

/**
 * @brief Show a toast notification
 */
void ShowNotification(const char* title, const char* message, NotificationType type = NotificationType::Info, float duration = 3.0f);

/**
 * @brief Render pending notifications (call once per frame)
 */
void RenderNotifications();

// ============================================================================
// Dialogs
// ============================================================================

/**
 * @brief Confirmation dialog result
 */
enum class DialogResult {
    None,       // Dialog still open
    Yes,
    No,
    Cancel
};

/**
 * @brief Show a confirmation dialog
 */
DialogResult ConfirmDialog(const char* title, const char* message, bool showCancel = false);

/**
 * @brief Show an input dialog
 */
bool InputDialog(const char* title, const char* label, std::string& value);

/**
 * @brief File open dialog
 */
bool OpenFileDialog(const char* title, std::string& outPath, const char* filter = "*.*", const char* defaultPath = nullptr);

/**
 * @brief File save dialog
 */
bool SaveFileDialog(const char* title, std::string& outPath, const char* filter = "*.*", const char* defaultName = nullptr);

/**
 * @brief Folder picker dialog
 */
bool FolderDialog(const char* title, std::string& outPath, const char* defaultPath = nullptr);

// ============================================================================
// Drag & Drop
// ============================================================================

/**
 * @brief Begin drag source
 */
bool BeginDragSource(const char* type, const void* data, size_t size, const char* previewText = nullptr);
void EndDragSource();

/**
 * @brief Begin drop target
 */
bool BeginDropTarget();
const void* AcceptDropPayload(const char* type, size_t* outSize = nullptr);
void EndDropTarget();

// ============================================================================
// Layout Helpers
// ============================================================================

/**
 * @brief Center the next item horizontally
 */
void CenterNextItem(float itemWidth);

/**
 * @brief Right-align the next item
 */
void RightAlignNextItem(float itemWidth);

/**
 * @brief Get available content width
 */
float GetContentWidth();

/**
 * @brief Get available content height
 */
float GetContentHeight();

/**
 * @brief Horizontal layout
 */
void BeginHorizontal();
void EndHorizontal();

/**
 * @brief Flexible spacer in horizontal layout
 */
void Spring(float flex = 1.0f);

// ============================================================================
// Node Editor Helpers
// ============================================================================

/**
 * @brief Draw a pin (input/output connection point)
 */
void DrawPin(const glm::vec2& pos, float radius, const glm::vec4& color, bool filled = true, bool connected = false);

/**
 * @brief Draw a bezier connection line
 */
void DrawConnection(const glm::vec2& start, const glm::vec2& end, const glm::vec4& color, float thickness = 2.0f);

/**
 * @brief Draw node background
 */
void DrawNodeBackground(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color, float rounding = 8.0f, bool selected = false);

/**
 * @brief Draw node header
 */
void DrawNodeHeader(const glm::vec2& pos, const glm::vec2& size, const char* title, const glm::vec4& color, float rounding = 8.0f);

/**
 * @brief Draw a grid
 */
void DrawGrid(const glm::vec2& offset, float spacing, const glm::vec4& color, const glm::vec4& majorColor, int majorEvery = 4);

// ============================================================================
// Animation / Timeline Helpers
// ============================================================================

/**
 * @brief Draw timeline ruler
 */
void DrawTimelineRuler(float startTime, float endTime, float currentTime, float height = 20.0f);

/**
 * @brief Draw keyframe marker
 */
void DrawKeyframe(const glm::vec2& pos, float size, const glm::vec4& color, bool selected = false);

/**
 * @brief Horizontal scrollable timeline
 */
bool TimelineScrollArea(const char* id, float& scrollX, float& zoom, float totalLength, const glm::vec2& size);

} // namespace EditorWidgets

} // namespace Nova
