#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Vehement {

class MapEditor;

/**
 * @brief Object category for palette
 */
enum class PaletteCategory : uint8_t {
    Units,
    Buildings,
    Doodads,
    Items,
    Special,
    Recent,
    Custom
};

/**
 * @brief Palette entry
 */
struct PaletteEntry {
    std::string id;
    std::string name;
    std::string category;
    std::string iconPath;
    std::string tooltip;
    bool isCustom = false;
};

/**
 * @brief Object Palette - Browse and select objects to place
 *
 * Features:
 * - Categories (units, buildings, doodads, items)
 * - Search/filter
 * - Preview
 * - Recent objects
 * - Custom objects
 */
class ObjectPalette {
public:
    ObjectPalette();
    ~ObjectPalette();

    void Initialize(MapEditor& mapEditor);
    void Shutdown();

    void Update(float deltaTime);
    void Render();

    // Object selection
    void SelectObject(const std::string& id);
    [[nodiscard]] const std::string& GetSelectedObjectId() const { return m_selectedId; }
    [[nodiscard]] const std::string& GetSelectedObjectType() const { return m_selectedType; }

    // Category
    void SetCategory(PaletteCategory category);
    [[nodiscard]] PaletteCategory GetCategory() const { return m_category; }

    // Filter
    void SetFilter(const std::string& filter);
    void ClearFilter();

    // Recent objects
    void AddToRecent(const std::string& id);
    void ClearRecent();

private:
    void RenderCategoryTabs();
    void RenderSearchBar();
    void RenderObjectGrid();
    void RenderPreview();
    void RenderObjectTooltip(const PaletteEntry& entry);

    void LoadPaletteEntries();
    [[nodiscard]] std::vector<PaletteEntry> GetFilteredEntries() const;

    MapEditor* m_mapEditor = nullptr;

    // Palette data
    std::unordered_map<PaletteCategory, std::vector<PaletteEntry>> m_entries;

    // Selection state
    std::string m_selectedId;
    std::string m_selectedType;
    PaletteCategory m_category = PaletteCategory::Units;

    // Filter state
    std::string m_searchFilter;

    // Recent objects
    std::vector<std::string> m_recentObjects;
    static constexpr size_t MAX_RECENT = 10;

    // UI state
    bool m_showPreview = true;
    int m_gridColumns = 4;
    float m_iconSize = 48.0f;
};

} // namespace Vehement
