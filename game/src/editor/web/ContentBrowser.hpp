#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>

namespace Vehement {

class Editor;

namespace WebEditor {

class WebView;
class JSBridge;

/**
 * @brief Content item types
 */
enum class ContentType {
    Spell,
    Unit,
    Building,
    TechTree,
    Effect,
    Buff,
    Resource,
    Culture,
    Hero,
    Ability,
    Event,
    Quest,
    Dialog,
    Script,
    Unknown
};

inline const char* ContentTypeToString(ContentType type) {
    switch (type) {
        case ContentType::Spell:    return "spells";
        case ContentType::Unit:     return "units";
        case ContentType::Building: return "buildings";
        case ContentType::TechTree: return "techtrees";
        case ContentType::Effect:   return "effects";
        case ContentType::Buff:     return "buffs";
        case ContentType::Resource: return "resources";
        case ContentType::Culture:  return "cultures";
        case ContentType::Hero:     return "heroes";
        case ContentType::Ability:  return "abilities";
        case ContentType::Event:    return "events";
        case ContentType::Quest:    return "quests";
        case ContentType::Dialog:   return "dialogs";
        case ContentType::Script:   return "scripts";
        default:                    return "unknown";
    }
}

inline ContentType StringToContentType(const std::string& str) {
    if (str == "spells")    return ContentType::Spell;
    if (str == "units")     return ContentType::Unit;
    if (str == "buildings") return ContentType::Building;
    if (str == "techtrees") return ContentType::TechTree;
    if (str == "effects")   return ContentType::Effect;
    if (str == "buffs")     return ContentType::Buff;
    if (str == "resources") return ContentType::Resource;
    if (str == "cultures")  return ContentType::Culture;
    if (str == "heroes")    return ContentType::Hero;
    if (str == "abilities") return ContentType::Ability;
    if (str == "events")    return ContentType::Event;
    if (str == "quests")    return ContentType::Quest;
    if (str == "dialogs")   return ContentType::Dialog;
    if (str == "scripts")   return ContentType::Script;
    return ContentType::Unknown;
}

/**
 * @brief Content item metadata
 */
struct ContentItem {
    std::string id;
    std::string name;
    std::string description;
    ContentType type;
    std::string filePath;
    std::string thumbnailPath;
    std::string lastModified;
    std::vector<std::string> tags;

    // State flags
    bool isDirty = false;
    bool hasErrors = false;
    bool isNew = false;
};

/**
 * @brief Filter options for content browser
 */
struct ContentFilter {
    std::string searchQuery;
    std::vector<ContentType> types;
    std::vector<std::string> tags;
    bool showDirtyOnly = false;
    bool showErrorsOnly = false;

    enum class SortBy {
        Name,
        Type,
        Modified,
        Created
    };
    SortBy sortBy = SortBy::Name;
    bool sortAscending = true;
};

/**
 * @brief HTML-based content browser panel
 *
 * Provides a web-based UI for browsing and managing all JSON configs:
 * - Browse spells, techs, units, buildings, effects
 * - Create, edit, delete, duplicate items
 * - Search and filter by name, type, tags
 * - Drag-drop support for reorganization
 * - Preview panel with thumbnails and metadata
 * - Context menu for quick actions
 *
 * Uses HTML/CSS/JS for the UI, communicating with C++ via JSBridge.
 */
class ContentBrowser {
public:
    explicit ContentBrowser(Editor* editor);
    ~ContentBrowser();

    // Non-copyable
    ContentBrowser(const ContentBrowser&) = delete;
    ContentBrowser& operator=(const ContentBrowser&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the content browser
     * @param configsPath Path to the configs directory
     * @return true if successful
     */
    bool Initialize(const std::string& configsPath);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the panel
     */
    void Render();

    // =========================================================================
    // Content Management
    // =========================================================================

    /**
     * @brief Refresh the content list from disk
     */
    void RefreshContent();

    /**
     * @brief Get all content items
     */
    [[nodiscard]] const std::vector<ContentItem>& GetAllContent() const { return m_allContent; }

    /**
     * @brief Get filtered content items
     */
    [[nodiscard]] std::vector<ContentItem> GetFilteredContent() const;

    /**
     * @brief Get content item by ID
     */
    [[nodiscard]] std::optional<ContentItem> GetContentItem(const std::string& id) const;

    /**
     * @brief Get content items by type
     */
    [[nodiscard]] std::vector<ContentItem> GetContentByType(ContentType type) const;

    // =========================================================================
    // CRUD Operations
    // =========================================================================

    /**
     * @brief Create a new content item
     * @param type Content type
     * @param name Item name
     * @return ID of the created item, or empty string on failure
     */
    std::string CreateItem(ContentType type, const std::string& name);

    /**
     * @brief Duplicate an existing item
     * @param id ID of item to duplicate
     * @return ID of the new item, or empty string on failure
     */
    std::string DuplicateItem(const std::string& id);

    /**
     * @brief Delete an item
     * @param id Item ID
     * @return true if successful
     */
    bool DeleteItem(const std::string& id);

    /**
     * @brief Rename an item
     * @param id Item ID
     * @param newName New name
     * @return true if successful
     */
    bool RenameItem(const std::string& id, const std::string& newName);

    /**
     * @brief Save item changes to disk
     * @param id Item ID
     * @return true if successful
     */
    bool SaveItem(const std::string& id);

    /**
     * @brief Reload item from disk, discarding changes
     * @param id Item ID
     * @return true if successful
     */
    bool ReloadItem(const std::string& id);

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select an item
     * @param id Item ID
     */
    void SelectItem(const std::string& id);

    /**
     * @brief Get selected item ID
     */
    [[nodiscard]] const std::string& GetSelectedId() const { return m_selectedId; }

    /**
     * @brief Get selected item (if any)
     */
    [[nodiscard]] std::optional<ContentItem> GetSelectedItem() const;

    /**
     * @brief Clear selection
     */
    void ClearSelection() { m_selectedId.clear(); }

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Set the content filter
     */
    void SetFilter(const ContentFilter& filter);

    /**
     * @brief Get current filter
     */
    [[nodiscard]] const ContentFilter& GetFilter() const { return m_filter; }

    /**
     * @brief Set search query
     */
    void SetSearchQuery(const std::string& query);

    /**
     * @brief Filter by content type
     */
    void FilterByType(ContentType type);

    /**
     * @brief Filter by tag
     */
    void FilterByTag(const std::string& tag);

    /**
     * @brief Clear all filters
     */
    void ClearFilters();

    // =========================================================================
    // Drag-Drop
    // =========================================================================

    /**
     * @brief Start dragging an item
     */
    void BeginDrag(const std::string& id);

    /**
     * @brief End drag operation
     * @param targetId Target item ID (for reordering or linking)
     */
    void EndDrag(const std::string& targetId);

    /**
     * @brief Check if currently dragging
     */
    [[nodiscard]] bool IsDragging() const { return !m_draggedId.empty(); }

    /**
     * @brief Get dragged item ID
     */
    [[nodiscard]] const std::string& GetDraggedId() const { return m_draggedId; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnItemSelected;
    std::function<void(const std::string&)> OnItemDoubleClicked;
    std::function<void(const std::string&)> OnItemCreated;
    std::function<void(const std::string&)> OnItemDeleted;
    std::function<void(const std::string&, const std::string&)> OnItemMoved;
    std::function<void(const std::string&)> OnItemModified;

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Get preview HTML for an item
     */
    [[nodiscard]] std::string GetPreviewHtml(const std::string& id) const;

    /**
     * @brief Generate thumbnail for an item
     */
    bool GenerateThumbnail(const std::string& id);

private:
    // JavaScript bridge setup
    void SetupJSBridge();
    void RegisterBridgeFunctions();

    // Content loading
    void LoadContentFromDirectory(const std::string& path, ContentType type);
    ContentItem ParseContentFile(const std::string& path, ContentType type);
    std::string GenerateContentId(ContentType type, const std::string& name);

    // Template generation
    std::string GetDefaultTemplate(ContentType type);

    // File operations
    std::string GetContentPath(ContentType type) const;
    std::string GetFilePath(const std::string& id) const;

    Editor* m_editor = nullptr;

    // Web view
    std::unique_ptr<WebView> m_webView;
    std::unique_ptr<JSBridge> m_bridge;

    // Content data
    std::string m_configsPath;
    std::vector<ContentItem> m_allContent;
    std::unordered_map<std::string, size_t> m_contentIndex;  // ID -> index in m_allContent

    // State
    std::string m_selectedId;
    std::string m_draggedId;
    ContentFilter m_filter;
    bool m_needsRefresh = true;

    // UI state
    bool m_showPreviewPanel = true;
    float m_previewPanelWidth = 300.0f;
    bool m_gridView = true;  // Grid vs list view
};

} // namespace WebEditor
} // namespace Vehement
