#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>

namespace Nova {
namespace Scripting {
    class ScriptStorage;
    class GameAPI;
}
}

namespace Vehement {

class Editor;

/**
 * @brief Function category for organizing functions
 */
enum class FunctionCategory {
    AI,
    Combat,
    Events,
    Utility,
    Entity,
    Building,
    Resource,
    UI,
    Audio,
    Custom,
    All
};

/**
 * @brief Information about a Python function
 */
struct FunctionInfo {
    std::string name;
    std::string qualifiedName;  // module.function
    std::string signature;
    std::string description;
    std::string documentation;
    std::string exampleCode;
    std::string filePath;
    FunctionCategory category = FunctionCategory::Custom;
    std::vector<std::string> parameters;
    std::vector<std::string> parameterTypes;
    std::string returnType;
    bool isGameAPI = false;
    bool isBuiltin = false;
    int lineNumber = 0;

    // For drag-drop identification
    std::string GetDragDropId() const { return qualifiedName; }
};

/**
 * @brief Category node for tree view
 */
struct CategoryNode {
    std::string name;
    FunctionCategory category;
    std::vector<FunctionInfo> functions;
    std::vector<CategoryNode> children;
    bool expanded = true;
};

/**
 * @brief Search filter options
 */
struct FunctionSearchFilter {
    std::string searchText;
    FunctionCategory categoryFilter = FunctionCategory::All;
    bool showGameAPI = true;
    bool showBuiltins = true;
    bool showCustom = true;
    bool caseSensitive = false;
};

/**
 * @brief Panel for browsing available Python functions
 *
 * Features:
 * - Tree view organized by category (AI, Combat, Events, Utility)
 * - Search filter with category filtering
 * - Function signature display
 * - Documentation preview
 * - Drag-drop to bind functions to events
 * - Create new function button
 *
 * Usage:
 * @code
 * FunctionBrowser browser;
 * browser.Initialize(editor);
 *
 * // In render loop
 * browser.Render();
 *
 * // Get selected function for binding
 * if (browser.HasSelection()) {
 *     auto func = browser.GetSelectedFunction();
 *     // Use for binding
 * }
 * @endcode
 */
class FunctionBrowser {
public:
    FunctionBrowser();
    ~FunctionBrowser();

    // Non-copyable
    FunctionBrowser(const FunctionBrowser&) = delete;
    FunctionBrowser& operator=(const FunctionBrowser&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the function browser
     * @param editor Parent editor reference
     * @return true if initialization succeeded
     */
    bool Initialize(Editor* editor);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render the function browser panel
     */
    void Render();

    /**
     * @brief Update state (call each frame)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Function Discovery
    // =========================================================================

    /**
     * @brief Refresh the function list from all sources
     */
    void RefreshFunctions();

    /**
     * @brief Add a custom function to the browser
     */
    void AddFunction(const FunctionInfo& function);

    /**
     * @brief Remove a function by qualified name
     */
    void RemoveFunction(const std::string& qualifiedName);

    /**
     * @brief Get all discovered functions
     */
    [[nodiscard]] const std::vector<FunctionInfo>& GetAllFunctions() const { return m_allFunctions; }

    /**
     * @brief Get functions by category
     */
    [[nodiscard]] std::vector<FunctionInfo> GetFunctionsByCategory(FunctionCategory category) const;

    /**
     * @brief Find function by qualified name
     */
    [[nodiscard]] std::optional<FunctionInfo> FindFunction(const std::string& qualifiedName) const;

    // =========================================================================
    // Search and Filter
    // =========================================================================

    /**
     * @brief Set search filter
     */
    void SetFilter(const FunctionSearchFilter& filter);

    /**
     * @brief Get current filter
     */
    [[nodiscard]] const FunctionSearchFilter& GetFilter() const { return m_filter; }

    /**
     * @brief Clear search filter
     */
    void ClearFilter();

    /**
     * @brief Get filtered functions
     */
    [[nodiscard]] const std::vector<FunctionInfo*>& GetFilteredFunctions() const { return m_filteredFunctions; }

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Check if a function is selected
     */
    [[nodiscard]] bool HasSelection() const { return m_selectedFunction != nullptr; }

    /**
     * @brief Get selected function
     */
    [[nodiscard]] const FunctionInfo* GetSelectedFunction() const { return m_selectedFunction; }

    /**
     * @brief Select function by qualified name
     */
    void SelectFunction(const std::string& qualifiedName);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    // =========================================================================
    // Drag-Drop Support
    // =========================================================================

    /**
     * @brief Check if drag-drop is active
     */
    [[nodiscard]] bool IsDragging() const { return m_isDragging; }

    /**
     * @brief Get function being dragged
     */
    [[nodiscard]] const FunctionInfo* GetDraggedFunction() const { return m_draggedFunction; }

    /**
     * @brief Start drag operation
     */
    void BeginDrag(const FunctionInfo* function);

    /**
     * @brief End drag operation
     */
    void EndDrag();

    // =========================================================================
    // Actions
    // =========================================================================

    /**
     * @brief Open function in script editor
     */
    void OpenInEditor(const FunctionInfo& function);

    /**
     * @brief Create new function with dialog
     */
    void CreateNewFunction();

    /**
     * @brief Duplicate selected function
     */
    void DuplicateSelected();

    /**
     * @brief Delete selected function
     */
    void DeleteSelected();

    // =========================================================================
    // Callbacks
    // =========================================================================

    using SelectionCallback = std::function<void(const FunctionInfo&)>;
    using DragDropCallback = std::function<void(const FunctionInfo&)>;

    void SetOnSelectionChanged(SelectionCallback callback) { m_onSelectionChanged = callback; }
    void SetOnFunctionDropped(DragDropCallback callback) { m_onFunctionDropped = callback; }
    void SetOnFunctionDoubleClicked(SelectionCallback callback) { m_onDoubleClicked = callback; }

    // =========================================================================
    // Static Helpers
    // =========================================================================

    /**
     * @brief Get category name string
     */
    static const char* GetCategoryName(FunctionCategory category);

    /**
     * @brief Parse category from string
     */
    static FunctionCategory ParseCategory(const std::string& name);

    /**
     * @brief Get drag-drop payload type identifier
     */
    static const char* GetDragDropPayloadType() { return "FUNCTION_REF"; }

private:
    // UI rendering
    void RenderToolbar();
    void RenderSearchBar();
    void RenderTreeView();
    void RenderCategoryNode(CategoryNode& node);
    void RenderFunctionItem(FunctionInfo& function);
    void RenderPreviewPanel();
    void RenderDocumentation(const FunctionInfo& function);
    void RenderNewFunctionDialog();
    void RenderContextMenu();

    // Function discovery
    void DiscoverGameAPIFunctions();
    void DiscoverScriptFunctions();
    void DiscoverBuiltinFunctions();
    void ParsePythonFile(const std::string& filePath);
    FunctionInfo ParseFunctionDef(const std::string& source, int lineStart);

    // Tree building
    void BuildCategoryTree();
    void AddToCategory(const FunctionInfo& function);

    // Filtering
    void ApplyFilter();
    bool MatchesFilter(const FunctionInfo& function) const;

    // State
    bool m_initialized = false;
    Editor* m_editor = nullptr;
    Nova::Scripting::ScriptStorage* m_storage = nullptr;
    Nova::Scripting::GameAPI* m_gameAPI = nullptr;

    // Functions
    std::vector<FunctionInfo> m_allFunctions;
    std::vector<FunctionInfo*> m_filteredFunctions;
    std::unordered_map<std::string, size_t> m_functionIndex;  // qualifiedName -> index

    // Category tree
    std::vector<CategoryNode> m_categoryTree;

    // Selection
    const FunctionInfo* m_selectedFunction = nullptr;

    // Drag-drop
    bool m_isDragging = false;
    const FunctionInfo* m_draggedFunction = nullptr;

    // Filter
    FunctionSearchFilter m_filter;
    char m_searchBuffer[256] = {0};

    // Dialogs
    bool m_showNewFunctionDialog = false;
    char m_newFunctionName[128] = {0};
    char m_newFunctionCategory[64] = {0};
    int m_newFunctionCategoryIndex = 0;

    // UI state
    bool m_showPreview = true;
    float m_treeWidth = 250.0f;
    float m_previewHeight = 200.0f;

    // Callbacks
    SelectionCallback m_onSelectionChanged;
    DragDropCallback m_onFunctionDropped;
    SelectionCallback m_onDoubleClicked;

    // Refresh tracking
    float m_refreshTimer = 0.0f;
    static constexpr float REFRESH_INTERVAL = 5.0f;
};

} // namespace Vehement
