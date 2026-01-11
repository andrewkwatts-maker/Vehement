#pragma once

#include "Hierarchy.hpp"
#include <unordered_map>
#include <string>

namespace Vehement {

class LayerSystem;
class SelectionSystem;

/**
 * @brief Extended Hierarchy with layer support
 *
 * Extends the existing Hierarchy panel to:
 * - Group objects by layer
 * - Show layer visibility/lock state
 * - Support multi-selection
 * - Integrate with SelectionSystem
 * - Show object icons based on type
 * - Drag and drop between layers
 */
class HierarchyExtended : public Hierarchy {
public:
    explicit HierarchyExtended(Editor* editor);
    ~HierarchyExtended() override;

    /**
     * @brief Set layer system for layer grouping
     */
    void SetLayerSystem(LayerSystem* layerSystem);

    /**
     * @brief Set selection system for multi-select
     */
    void SetSelectionSystem(SelectionSystem* selectionSystem);

    /**
     * @brief Enable/disable layer grouping
     */
    void SetLayerGroupingEnabled(bool enabled);

    /**
     * @brief Check if layer grouping is enabled
     */
    [[nodiscard]] bool IsLayerGroupingEnabled() const noexcept { return m_layerGroupingEnabled; }

    /**
     * @brief Set display mode
     */
    void SetDisplayMode(const std::string& mode);

    /**
     * @brief Get display mode
     */
    [[nodiscard]] const std::string& GetDisplayMode() const noexcept { return m_displayMode; }

    // Display modes:
    // - "flat" - Flat list of all objects
    // - "hierarchy" - Standard parent-child tree
    // - "layers" - Group by layers
    // - "type" - Group by object type

    /**
     * @brief Refresh hierarchy with layer info
     */
    void RefreshWithLayers();

    /**
     * @brief Override Render to add layer features
     */
    void Render() override;

private:
    // Extended rendering
    void RenderWithLayers();
    void RenderLayerGroup(uint32_t layerId);
    void RenderEntityNode(const EntityInfo& entity, bool inLayer);
    void RenderLayerContextMenu(uint32_t layerId);
    void RenderEntityContextMenu(uint64_t entityId);

    // Multi-selection
    void HandleMultiSelection(uint64_t entityId);
    bool IsInMultiSelectMode() const;

    // Drag and drop
    void HandleDragDrop(uint64_t entityId, uint32_t sourceLayer);

    // Visual helpers
    const char* GetEntityIcon(const std::string& type) const;
    bool ShouldShowEntity(const EntityInfo& entity) const;

    // Layer system integration
    LayerSystem* m_layerSystem = nullptr;
    SelectionSystem* m_selectionSystem = nullptr;

    // Display settings
    bool m_layerGroupingEnabled = true;
    std::string m_displayMode = "layers";

    // UI state
    std::unordered_map<uint32_t, bool> m_layerExpanded;
    bool m_showHiddenLayers = true;
    bool m_showLockedLayers = true;

    // Multi-selection state
    std::vector<uint64_t> m_multiSelection;
    uint64_t m_lastClickedEntity = 0;
};

/**
 * @brief Hierarchy Filter - Advanced filtering for hierarchy
 */
class HierarchyFilter {
public:
    HierarchyFilter() = default;
    ~HierarchyFilter() = default;

    /**
     * @brief Set text filter
     */
    void SetTextFilter(const std::string& text);

    /**
     * @brief Set type filter
     */
    void SetTypeFilter(const std::string& type);

    /**
     * @brief Set layer filter
     */
    void SetLayerFilter(uint32_t layerId);

    /**
     * @brief Clear all filters
     */
    void ClearFilters();

    /**
     * @brief Check if entity passes filter
     */
    [[nodiscard]] bool PassesFilter(const Hierarchy::EntityInfo& entity,
                                     uint32_t entityLayerId) const;

    /**
     * @brief Check if any filter is active
     */
    [[nodiscard]] bool HasActiveFilter() const;

    /**
     * @brief Render filter UI
     */
    void RenderFilterUI();

private:
    std::string m_textFilter;
    std::string m_typeFilter = "all";
    uint32_t m_layerFilter = 0;  // 0 = all layers
    bool m_showOnlySelected = false;
    bool m_showOnlyVisible = false;
};

/**
 * @brief Hierarchy Sort Options
 */
enum class HierarchySortMode : uint8_t {
    None,           // No sorting
    Name,           // Alphabetical by name
    Type,           // Group by type
    CreationTime,   // Order of creation
    Custom          // Custom order (drag to reorder)
};

/**
 * @brief Hierarchy Sorter
 */
class HierarchySorter {
public:
    HierarchySorter() = default;
    ~HierarchySorter() = default;

    /**
     * @brief Set sort mode
     */
    void SetSortMode(HierarchySortMode mode);

    /**
     * @brief Get sort mode
     */
    [[nodiscard]] HierarchySortMode GetSortMode() const noexcept { return m_sortMode; }

    /**
     * @brief Set sort ascending/descending
     */
    void SetAscending(bool ascending);

    /**
     * @brief Sort entity list
     */
    void Sort(std::vector<Hierarchy::EntityInfo>& entities) const;

    /**
     * @brief Render sort UI
     */
    void RenderSortUI();

private:
    HierarchySortMode m_sortMode = HierarchySortMode::Name;
    bool m_ascending = true;
};

} // namespace Vehement
