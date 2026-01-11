#pragma once

#include "UIWidget.hpp"
#include <variant>

namespace Nova {
namespace UI {

// =============================================================================
// Text Widget - Displays text content
// =============================================================================

class UIText : public UIWidget {
public:
    UIText() : UIWidget("text") {}
    UIText(const std::string& text) : UIWidget("text") { SetText(text); }

    void Render(UIContext& context) override;
};

// =============================================================================
// Label Widget - Static label with optional icon
// =============================================================================

class UILabel : public UIWidget {
public:
    UILabel() : UIWidget("label") {}
    UILabel(const std::string& text) : UIWidget("label") { SetText(text); }

    void SetIcon(const std::string& iconPath) { m_iconPath = iconPath; MarkDirty(); }
    const std::string& GetIcon() const { return m_iconPath; }

    void SetIconPosition(const std::string& pos) { m_iconPosition = pos; }  // "left", "right", "top", "bottom"

    void Render(UIContext& context) override;

private:
    std::string m_iconPath;
    std::string m_iconPosition = "left";
};

// =============================================================================
// Button Widget - Clickable button
// =============================================================================

class UIButton : public UIWidget {
public:
    UIButton() : UIWidget("button") {
        m_style.padding = BoxSpacing::All(8);
        m_style.border = Border(1, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 4.0f);
        m_style.backgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    }

    UIButton(const std::string& text) : UIButton() { SetText(text); }

    void SetIcon(const std::string& iconPath) { m_iconPath = iconPath; MarkDirty(); }

    void Render(UIContext& context) override;

protected:
    void OnFocusChanged(bool focused) override;

private:
    std::string m_iconPath;
};

// =============================================================================
// Input Widget - Text input field
// =============================================================================

class UIInput : public UIWidget {
public:
    UIInput() : UIWidget("input") {
        m_style.padding = BoxSpacing::All(6);
        m_style.border = Border(1, glm::vec4(0.4f, 0.4f, 0.4f, 1.0f), 3.0f);
        m_style.backgroundColor = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
    }

    const std::string& GetValue() const { return m_value; }
    void SetValue(const std::string& value);

    const std::string& GetPlaceholder() const { return m_placeholder; }
    void SetPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; MarkDirty(); }

    const std::string& GetInputType() const { return m_inputType; }
    void SetInputType(const std::string& type) { m_inputType = type; }  // "text", "password", "number", "email"

    int GetMaxLength() const { return m_maxLength; }
    void SetMaxLength(int maxLength) { m_maxLength = maxLength; }

    bool IsReadOnly() const { return m_readOnly; }
    void SetReadOnly(bool readOnly) { m_readOnly = readOnly; }

    void Render(UIContext& context) override;
    void Update(float deltaTime) override;

private:
    std::string m_value;
    std::string m_placeholder;
    std::string m_inputType = "text";
    int m_maxLength = -1;
    bool m_readOnly = false;
    int m_cursorPosition = 0;
    float m_cursorBlink = 0.0f;
};

// =============================================================================
// Checkbox Widget - Boolean toggle
// =============================================================================

class UICheckbox : public UIWidget {
public:
    UICheckbox() : UIWidget("checkbox") {}
    UICheckbox(const std::string& label, bool checked = false)
        : UIWidget("checkbox"), m_checked(checked) {
        SetText(label);
    }

    bool IsChecked() const { return m_checked; }
    void SetChecked(bool checked);

    void Toggle() { SetChecked(!m_checked); }

    void Render(UIContext& context) override;

private:
    bool m_checked = false;
};

// =============================================================================
// Slider Widget - Numeric range slider
// =============================================================================

class UISlider : public UIWidget {
public:
    UISlider() : UIWidget("slider") {
        m_style.height = Length::Px(24);
    }

    float GetValue() const { return m_value; }
    void SetValue(float value);

    float GetMin() const { return m_min; }
    void SetMin(float min) { m_min = min; ClampValue(); }

    float GetMax() const { return m_max; }
    void SetMax(float max) { m_max = max; ClampValue(); }

    float GetStep() const { return m_step; }
    void SetStep(float step) { m_step = step; }

    void SetRange(float min, float max, float step = 0.0f) {
        m_min = min;
        m_max = max;
        m_step = step;
        ClampValue();
    }

    void Render(UIContext& context) override;

private:
    void ClampValue();

    float m_value = 0.0f;
    float m_min = 0.0f;
    float m_max = 100.0f;
    float m_step = 0.0f;
    bool m_dragging = false;
};

// =============================================================================
// Progress Bar Widget
// =============================================================================

class UIProgressBar : public UIWidget {
public:
    UIProgressBar() : UIWidget("progress") {
        m_style.height = Length::Px(20);
        m_style.backgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        m_style.border = Border(0, glm::vec4(0), 4.0f);
    }

    float GetProgress() const { return m_progress; }
    void SetProgress(float progress) { m_progress = std::clamp(progress, 0.0f, 1.0f); MarkDirty(); }

    const glm::vec4& GetFillColor() const { return m_fillColor; }
    void SetFillColor(const glm::vec4& color) { m_fillColor = color; }

    bool GetShowText() const { return m_showText; }
    void SetShowText(bool show) { m_showText = show; }

    const std::string& GetTextFormat() const { return m_textFormat; }
    void SetTextFormat(const std::string& format) { m_textFormat = format; }  // "{value}%", "{value}/{max}"

    void Render(UIContext& context) override;

private:
    float m_progress = 0.0f;
    glm::vec4 m_fillColor{0.2f, 0.6f, 1.0f, 1.0f};
    bool m_showText = true;
    std::string m_textFormat = "{value}%";
};

// =============================================================================
// Select/Dropdown Widget
// =============================================================================

struct SelectOption {
    std::string value;
    std::string label;
    std::string icon;
    bool disabled = false;
};

class UISelect : public UIWidget {
public:
    UISelect() : UIWidget("select") {
        m_style.padding = BoxSpacing::All(6);
        m_style.border = Border(1, glm::vec4(0.4f, 0.4f, 0.4f, 1.0f), 3.0f);
    }

    void AddOption(const std::string& value, const std::string& label, const std::string& icon = "");
    void AddOption(const SelectOption& option);
    void ClearOptions();
    const std::vector<SelectOption>& GetOptions() const { return m_options; }

    const std::string& GetValue() const { return m_selectedValue; }
    void SetValue(const std::string& value);

    int GetSelectedIndex() const { return m_selectedIndex; }
    void SetSelectedIndex(int index);

    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; MarkDirty(); }

    void Render(UIContext& context) override;

private:
    std::vector<SelectOption> m_options;
    std::string m_selectedValue;
    int m_selectedIndex = -1;
    bool m_isOpen = false;
};

// =============================================================================
// List Widget - Displays a list of items
// =============================================================================

class UIList : public UIWidget {
public:
    UIList() : UIWidget("list") {
        m_style.flexDirection = LayoutDirection::Column;
        m_style.overflowY = Overflow::Auto;
    }

    /**
     * @brief Bind list to an array/vector via reflection
     */
    template<typename T>
    void BindArray(std::vector<T>* array, const Reflect::TypeInfo* itemType);

    void SetItemTemplate(const std::string& templateId);
    void SetItemTemplate(std::function<WidgetPtr(const std::any&, int)> factory);

    int GetSelectedIndex() const { return m_selectedIndex; }
    void SetSelectedIndex(int index);

    bool IsMultiSelect() const { return m_multiSelect; }
    void SetMultiSelect(bool multi) { m_multiSelect = multi; }

    const std::vector<int>& GetSelectedIndices() const { return m_selectedIndices; }

    void Render(UIContext& context) override;
    void Update(float deltaTime) override;

private:
    void RebuildItems();

    std::function<WidgetPtr(const std::any&, int)> m_itemFactory;
    std::string m_templateId;
    int m_selectedIndex = -1;
    std::vector<int> m_selectedIndices;
    bool m_multiSelect = false;

    // Virtualization
    bool m_virtualized = false;
    float m_itemHeight = 30.0f;
    int m_visibleStartIndex = 0;
    int m_visibleCount = 0;
};

// =============================================================================
// Grid Widget - Displays items in a grid layout
// =============================================================================

class UIGrid : public UIWidget {
public:
    UIGrid() : UIWidget("grid") {
        m_style.display = Display::Grid;
    }

    int GetColumns() const { return m_columns; }
    void SetColumns(int cols) { m_columns = cols; MarkDirty(); }

    float GetColumnGap() const { return m_columnGap; }
    void SetColumnGap(float gap) { m_columnGap = gap; }

    float GetRowGap() const { return m_rowGap; }
    void SetRowGap(float gap) { m_rowGap = gap; }

    void Layout(const glm::vec4& parentRect) override;
    void Render(UIContext& context) override;

private:
    int m_columns = 3;
    float m_columnGap = 8.0f;
    float m_rowGap = 8.0f;
};

// =============================================================================
// Tab Container Widget
// =============================================================================

class UITabs : public UIWidget {
public:
    UITabs() : UIWidget("tabs") {
        m_style.flexDirection = LayoutDirection::Column;
    }

    void AddTab(const std::string& id, const std::string& label, WidgetPtr content);
    void RemoveTab(const std::string& id);

    const std::string& GetActiveTab() const { return m_activeTabId; }
    void SetActiveTab(const std::string& id);

    void Render(UIContext& context) override;

private:
    struct TabInfo {
        std::string id;
        std::string label;
        WidgetPtr content;
    };

    std::vector<TabInfo> m_tabs;
    std::string m_activeTabId;
};

// =============================================================================
// Scroll Container Widget
// =============================================================================

class UIScrollView : public UIWidget {
public:
    UIScrollView() : UIWidget("scrollview") {
        m_style.overflowX = Overflow::Hidden;
        m_style.overflowY = Overflow::Auto;
    }

    glm::vec2 GetScrollOffset() const { return m_scrollOffset; }
    void SetScrollOffset(const glm::vec2& offset);
    void ScrollTo(float x, float y);
    void ScrollToChild(WidgetPtr child);

    bool IsHorizontalScrollEnabled() const { return m_horizontalScroll; }
    void SetHorizontalScrollEnabled(bool enabled) { m_horizontalScroll = enabled; }

    bool IsVerticalScrollEnabled() const { return m_verticalScroll; }
    void SetVerticalScrollEnabled(bool enabled) { m_verticalScroll = enabled; }

    void Render(UIContext& context) override;
    void Update(float deltaTime) override;

private:
    glm::vec2 m_scrollOffset{0.0f};
    glm::vec2 m_contentSize{0.0f};
    bool m_horizontalScroll = false;
    bool m_verticalScroll = true;
    float m_scrollSpeed = 30.0f;
};

// =============================================================================
// Panel/Container Widget
// =============================================================================

class UIPanel : public UIWidget {
public:
    UIPanel() : UIWidget("panel") {
        m_style.flexDirection = LayoutDirection::Column;
        m_style.padding = BoxSpacing::All(8);
    }

    void SetTitle(const std::string& title) { m_title = title; MarkDirty(); }
    const std::string& GetTitle() const { return m_title; }

    bool IsCollapsible() const { return m_collapsible; }
    void SetCollapsible(bool collapsible) { m_collapsible = collapsible; }

    bool IsCollapsed() const { return m_collapsed; }
    void SetCollapsed(bool collapsed);

    void Render(UIContext& context) override;

private:
    std::string m_title;
    bool m_collapsible = false;
    bool m_collapsed = false;
};

// =============================================================================
// Image Widget
// =============================================================================

class UIImage : public UIWidget {
public:
    UIImage() : UIWidget("img") {}
    UIImage(const std::string& src) : UIWidget("img") { SetSource(src); }

    void SetSource(const std::string& src) { m_source = src; MarkDirty(); }
    const std::string& GetSource() const { return m_source; }

    void SetFit(const std::string& fit) { m_fit = fit; }  // "contain", "cover", "fill", "none"
    const std::string& GetFit() const { return m_fit; }

    const glm::vec4& GetTint() const { return m_tint; }
    void SetTint(const glm::vec4& tint) { m_tint = tint; }

    void Render(UIContext& context) override;

private:
    std::string m_source;
    std::string m_fit = "contain";
    glm::vec4 m_tint{1.0f};
};

// =============================================================================
// Tooltip Widget
// =============================================================================

class UITooltip : public UIWidget {
public:
    UITooltip() : UIWidget("tooltip") {
        m_style.position = Position::Absolute;
        m_style.backgroundColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.95f);
        m_style.padding = BoxSpacing::All(6);
        m_style.border = Border(1, glm::vec4(0.3f, 0.3f, 0.3f, 1.0f), 4.0f);
        m_style.zIndex = 1000;
        m_style.visible = false;
    }

    void Show(const glm::vec2& position);
    void Hide();

    float GetDelay() const { return m_delay; }
    void SetDelay(float delay) { m_delay = delay; }

private:
    float m_delay = 0.5f;
    float m_showTimer = 0.0f;
};

// =============================================================================
// Modal/Dialog Widget
// =============================================================================

class UIModal : public UIWidget {
public:
    UIModal() : UIWidget("modal") {
        m_style.position = Position::Fixed;
        m_style.top = Length::Px(0);
        m_style.left = Length::Px(0);
        m_style.width = Length::Pct(100);
        m_style.height = Length::Pct(100);
        m_style.alignItems = Alignment::Center;
        m_style.justifyContent = Alignment::Center;
        m_style.backgroundColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);
        m_style.visible = false;
    }

    void Open();
    void Close();
    bool IsOpen() const { return m_isOpen; }

    void SetCloseOnBackdropClick(bool close) { m_closeOnBackdrop = close; }
    void SetCloseOnEscape(bool close) { m_closeOnEscape = close; }

    void Render(UIContext& context) override;

private:
    bool m_isOpen = false;
    bool m_closeOnBackdrop = true;
    bool m_closeOnEscape = true;
};

} // namespace UI
} // namespace Nova
