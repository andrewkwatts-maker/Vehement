#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdint>

namespace Vehement {

/**
 * @brief Layer for organizing scene objects
 */
struct Layer {
    uint32_t id = 0;
    std::string name;
    bool visible = true;
    bool locked = false;
    float opacity = 1.0f;
    uint32_t color = 0xFFFFFFFF;  // RGBA color for layer identification
    std::unordered_set<uint64_t> objects;  // Entity IDs in this layer
    int sortOrder = 0;  // For render order
};

/**
 * @brief Layer System - Organize scene objects into layers
 *
 * Features:
 * - Create, rename, delete layers
 * - Show/hide layers
 * - Lock/unlock layers (prevent editing)
 * - Layer opacity control
 * - Assign objects to layers
 * - Layer sorting/ordering
 * - Bulk operations on layer contents
 * - Layer merge and split
 */
class LayerSystem {
public:
    LayerSystem();
    ~LayerSystem();

    // Delete copy, allow move
    LayerSystem(const LayerSystem&) = delete;
    LayerSystem& operator=(const LayerSystem&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize layer system
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Layer Management
    // =========================================================================

    /**
     * @brief Create new layer
     * @param name Layer name
     * @return Layer ID
     */
    uint32_t CreateLayer(const std::string& name);

    /**
     * @brief Delete layer
     * @param layerId Layer to delete
     * @param deleteObjects If true, delete objects in layer; otherwise move to default layer
     * @return true if deleted
     */
    bool DeleteLayer(uint32_t layerId, bool deleteObjects = false);

    /**
     * @brief Rename layer
     * @param layerId Layer to rename
     * @param newName New name
     * @return true if renamed
     */
    bool RenameLayer(uint32_t layerId, const std::string& newName);

    /**
     * @brief Get layer by ID
     */
    [[nodiscard]] Layer* GetLayer(uint32_t layerId);

    /**
     * @brief Get layer by ID (const)
     */
    [[nodiscard]] const Layer* GetLayer(uint32_t layerId) const;

    /**
     * @brief Get layer by name
     */
    [[nodiscard]] Layer* GetLayerByName(const std::string& name);

    /**
     * @brief Get all layers
     */
    [[nodiscard]] const std::vector<Layer*>& GetLayers() const noexcept { return m_layerOrder; }

    /**
     * @brief Get layer count
     */
    [[nodiscard]] size_t GetLayerCount() const noexcept { return m_layers.size(); }

    // =========================================================================
    // Layer Visibility
    // =========================================================================

    /**
     * @brief Set layer visibility
     * @param layerId Layer to modify
     * @param visible Visibility state
     */
    void SetLayerVisible(uint32_t layerId, bool visible);

    /**
     * @brief Toggle layer visibility
     */
    void ToggleLayerVisible(uint32_t layerId);

    /**
     * @brief Check if layer is visible
     */
    [[nodiscard]] bool IsLayerVisible(uint32_t layerId) const;

    /**
     * @brief Show all layers
     */
    void ShowAllLayers();

    /**
     * @brief Hide all layers except specified
     * @param layerId Layer to keep visible
     */
    void IsolateLayer(uint32_t layerId);

    // =========================================================================
    // Layer Locking
    // =========================================================================

    /**
     * @brief Set layer lock state
     * @param layerId Layer to modify
     * @param locked Lock state
     */
    void SetLayerLocked(uint32_t layerId, bool locked);

    /**
     * @brief Toggle layer lock
     */
    void ToggleLayerLocked(uint32_t layerId);

    /**
     * @brief Check if layer is locked
     */
    [[nodiscard]] bool IsLayerLocked(uint32_t layerId) const;

    /**
     * @brief Lock all layers except specified
     */
    void LockAllExcept(uint32_t layerId);

    // =========================================================================
    // Layer Properties
    // =========================================================================

    /**
     * @brief Set layer opacity
     * @param layerId Layer to modify
     * @param opacity Opacity (0.0 - 1.0)
     */
    void SetLayerOpacity(uint32_t layerId, float opacity);

    /**
     * @brief Set layer color
     * @param layerId Layer to modify
     * @param color RGBA color
     */
    void SetLayerColor(uint32_t layerId, uint32_t color);

    /**
     * @brief Get layer opacity
     */
    [[nodiscard]] float GetLayerOpacity(uint32_t layerId) const;

    /**
     * @brief Get layer color
     */
    [[nodiscard]] uint32_t GetLayerColor(uint32_t layerId) const;

    // =========================================================================
    // Layer Ordering
    // =========================================================================

    /**
     * @brief Move layer up in order
     * @param layerId Layer to move
     */
    void MoveLayerUp(uint32_t layerId);

    /**
     * @brief Move layer down in order
     */
    void MoveLayerDown(uint32_t layerId);

    /**
     * @brief Set layer order index
     */
    void SetLayerOrder(uint32_t layerId, int order);

    /**
     * @brief Get layer order index
     */
    [[nodiscard]] int GetLayerOrder(uint32_t layerId) const;

    // =========================================================================
    // Object Assignment
    // =========================================================================

    /**
     * @brief Add object to layer
     * @param entityId Object to add
     * @param layerId Target layer
     */
    void AddObjectToLayer(uint64_t entityId, uint32_t layerId);

    /**
     * @brief Remove object from layer
     * @param entityId Object to remove
     * @param layerId Layer to remove from
     */
    void RemoveObjectFromLayer(uint64_t entityId, uint32_t layerId);

    /**
     * @brief Move object to different layer
     * @param entityId Object to move
     * @param targetLayerId Target layer
     */
    void MoveObjectToLayer(uint64_t entityId, uint32_t targetLayerId);

    /**
     * @brief Get layer containing object
     * @param entityId Object to query
     * @return Layer ID, or 0 if not found
     */
    [[nodiscard]] uint32_t GetObjectLayer(uint64_t entityId) const;

    /**
     * @brief Check if object is in layer
     */
    [[nodiscard]] bool IsObjectInLayer(uint64_t entityId, uint32_t layerId) const;

    /**
     * @brief Get all objects in layer
     */
    [[nodiscard]] std::vector<uint64_t> GetLayerObjects(uint32_t layerId) const;

    // =========================================================================
    // Active Layer
    // =========================================================================

    /**
     * @brief Set active layer (where new objects are placed)
     * @param layerId Layer to activate
     */
    void SetActiveLayer(uint32_t layerId);

    /**
     * @brief Get active layer ID
     */
    [[nodiscard]] uint32_t GetActiveLayer() const noexcept { return m_activeLayerId; }

    /**
     * @brief Get active layer
     */
    [[nodiscard]] Layer* GetActiveLayerObject();

    // =========================================================================
    // Default Layer
    // =========================================================================

    /**
     * @brief Get default layer ID (always exists)
     */
    [[nodiscard]] uint32_t GetDefaultLayer() const noexcept { return m_defaultLayerId; }

    // =========================================================================
    // Bulk Operations
    // =========================================================================

    /**
     * @brief Select all objects in layer
     * @param layerId Layer to select from
     */
    void SelectLayerObjects(uint32_t layerId);

    /**
     * @brief Delete all objects in layer
     * @param layerId Layer to clear
     */
    void DeleteLayerObjects(uint32_t layerId);

    /**
     * @brief Merge layers
     * @param sourceLayerId Source layer
     * @param targetLayerId Target layer (source will be deleted)
     */
    void MergeLayers(uint32_t sourceLayerId, uint32_t targetLayerId);

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Check if object should be rendered based on layer visibility
     */
    [[nodiscard]] bool ShouldRenderObject(uint64_t entityId) const;

    /**
     * @brief Check if object can be edited based on layer lock state
     */
    [[nodiscard]] bool CanEditObject(uint64_t entityId) const;

    // =========================================================================
    // Import/Export
    // =========================================================================

    /**
     * @brief Export layer configuration to JSON
     */
    [[nodiscard]] std::string ExportLayers() const;

    /**
     * @brief Import layer configuration from JSON
     */
    bool ImportLayers(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(uint32_t)> OnLayerCreated;
    std::function<void(uint32_t)> OnLayerDeleted;
    std::function<void(uint32_t)> OnLayerRenamed;
    std::function<void(uint32_t, bool)> OnLayerVisibilityChanged;
    std::function<void(uint32_t, bool)> OnLayerLockChanged;
    std::function<void(uint32_t)> OnActiveLayerChanged;
    std::function<void(uint64_t, uint32_t)> OnObjectLayerChanged;

private:
    // ID generation
    uint32_t GenerateLayerId();

    // Update layer order
    void UpdateLayerOrder();

    // State
    bool m_initialized = false;

    // Layers
    std::unordered_map<uint32_t, Layer> m_layers;
    std::vector<Layer*> m_layerOrder;  // Sorted by sortOrder
    uint32_t m_defaultLayerId = 1;
    uint32_t m_activeLayerId = 1;
    uint32_t m_nextLayerId = 2;

    // Object to layer mapping (for fast lookup)
    std::unordered_map<uint64_t, uint32_t> m_objectToLayer;
};

/**
 * @brief Layer Panel - UI for layer system
 */
class LayerPanel {
public:
    LayerPanel();
    ~LayerPanel();

    /**
     * @brief Initialize panel
     * @param layerSystem Layer system to manage
     */
    bool Initialize(LayerSystem& layerSystem);

    /**
     * @brief Render the panel UI
     */
    void Render();

    /**
     * @brief Set panel visibility
     */
    void SetVisible(bool visible);

    /**
     * @brief Check if panel is visible
     */
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }

private:
    void RenderToolbar();
    void RenderLayerList();
    void RenderLayerRow(Layer& layer);
    void RenderContextMenu(uint32_t layerId);

    LayerSystem* m_layerSystem = nullptr;
    bool m_visible = true;
    uint32_t m_selectedLayerId = 0;
    bool m_renamingLayer = false;
    char m_renameBuffer[128] = "";
};

} // namespace Vehement
