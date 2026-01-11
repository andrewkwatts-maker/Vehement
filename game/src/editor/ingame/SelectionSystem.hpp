#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <unordered_set>
#include <memory>
#include <functional>

namespace Nova {
    class Renderer;
    class Camera;
}

namespace Vehement {

class World;

/**
 * @brief Selection mode
 */
enum class SelectionMode : uint8_t {
    Replace,    // Replace current selection
    Add,        // Add to selection
    Subtract,   // Remove from selection
    Toggle      // Toggle selected state
};

/**
 * @brief Selected object data
 */
struct SelectedObject {
    uint64_t entityId = 0;
    std::string assetId;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};

/**
 * @brief Selection bounds
 */
struct SelectionBounds {
    glm::vec3 center{0.0f};
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    glm::vec3 size{0.0f};
};

/**
 * @brief Selection System - Manage object selection and multi-selection
 *
 * Features:
 * - Single and multi-selection
 * - Box select (drag rectangle)
 * - Add/remove from selection (Shift/Ctrl)
 * - Selection bounds calculation
 * - Selection highlighting
 * - Copy/paste/duplicate selection
 * - Group operations
 */
class SelectionSystem {
public:
    SelectionSystem();
    ~SelectionSystem();

    // Delete copy, allow move
    SelectionSystem(const SelectionSystem&) = delete;
    SelectionSystem& operator=(const SelectionSystem&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize selection system
     * @param world Game world
     * @return true if successful
     */
    bool Initialize(World& world);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update selection state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render selection visualization
     * @param renderer Renderer
     * @param camera Camera for rendering
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera);

    // =========================================================================
    // Selection Control
    // =========================================================================

    /**
     * @brief Select object by entity ID
     * @param entityId Entity to select
     * @param mode Selection mode
     */
    void SelectObject(uint64_t entityId, SelectionMode mode = SelectionMode::Replace);

    /**
     * @brief Select multiple objects
     * @param entityIds Entities to select
     * @param mode Selection mode
     */
    void SelectObjects(const std::vector<uint64_t>& entityIds, SelectionMode mode = SelectionMode::Replace);

    /**
     * @brief Deselect object
     * @param entityId Entity to deselect
     */
    void DeselectObject(uint64_t entityId);

    /**
     * @brief Clear all selection
     */
    void ClearSelection();

    /**
     * @brief Select all objects in world
     */
    void SelectAll();

    /**
     * @brief Invert selection
     */
    void InvertSelection();

    // =========================================================================
    // Box Select
    // =========================================================================

    /**
     * @brief Start box selection
     * @param screenPos Start position in screen space
     * @param camera Camera for raycasting
     */
    void StartBoxSelect(const glm::vec2& screenPos, const Nova::Camera& camera);

    /**
     * @brief Update box selection
     * @param screenPos Current position in screen space
     * @param camera Camera for raycasting
     */
    void UpdateBoxSelect(const glm::vec2& screenPos, const Nova::Camera& camera);

    /**
     * @brief End box selection
     * @param mode Selection mode
     */
    void EndBoxSelect(SelectionMode mode = SelectionMode::Replace);

    /**
     * @brief Cancel box selection
     */
    void CancelBoxSelect();

    /**
     * @brief Check if box selecting
     */
    [[nodiscard]] bool IsBoxSelecting() const noexcept { return m_isBoxSelecting; }

    // =========================================================================
    // Ray Select
    // =========================================================================

    /**
     * @brief Select object by raycasting
     * @param screenPos Screen position
     * @param camera Camera for raycasting
     * @param mode Selection mode
     * @return true if object was selected
     */
    bool SelectByRay(const glm::vec2& screenPos, const Nova::Camera& camera, SelectionMode mode = SelectionMode::Replace);

    // =========================================================================
    // Query Selection
    // =========================================================================

    /**
     * @brief Check if object is selected
     */
    [[nodiscard]] bool IsSelected(uint64_t entityId) const;

    /**
     * @brief Get selected object count
     */
    [[nodiscard]] size_t GetSelectionCount() const noexcept { return m_selectedObjects.size(); }

    /**
     * @brief Check if has selection
     */
    [[nodiscard]] bool HasSelection() const noexcept { return !m_selectedObjects.empty(); }

    /**
     * @brief Get all selected entity IDs
     */
    [[nodiscard]] std::vector<uint64_t> GetSelectedEntityIds() const;

    /**
     * @brief Get all selected objects
     */
    [[nodiscard]] const std::vector<SelectedObject>& GetSelectedObjects() const noexcept { return m_selectedObjects; }

    /**
     * @brief Get first selected object
     */
    [[nodiscard]] const SelectedObject* GetFirstSelected() const;

    // =========================================================================
    // Selection Bounds
    // =========================================================================

    /**
     * @brief Get combined bounds of all selected objects
     */
    [[nodiscard]] SelectionBounds GetSelectionBounds() const;

    /**
     * @brief Get center of selection
     */
    [[nodiscard]] glm::vec3 GetSelectionCenter() const;

    // =========================================================================
    // Copy/Paste/Duplicate
    // =========================================================================

    /**
     * @brief Copy selected objects to clipboard
     */
    void Copy();

    /**
     * @brief Cut selected objects to clipboard (copy and delete)
     */
    void Cut();

    /**
     * @brief Paste objects from clipboard
     * @param offset Offset from original position
     */
    void Paste(const glm::vec3& offset = glm::vec3(2.0f, 0.0f, 0.0f));

    /**
     * @brief Duplicate selected objects
     * @param offset Offset from original position
     */
    void Duplicate(const glm::vec3& offset = glm::vec3(2.0f, 0.0f, 0.0f));

    /**
     * @brief Check if clipboard has data
     */
    [[nodiscard]] bool HasClipboardData() const noexcept { return !m_clipboard.empty(); }

    // =========================================================================
    // Delete
    // =========================================================================

    /**
     * @brief Delete selected objects
     */
    void DeleteSelected();

    // =========================================================================
    // Group Operations
    // =========================================================================

    /**
     * @brief Move all selected objects
     * @param delta Movement delta
     */
    void MoveSelection(const glm::vec3& delta);

    /**
     * @brief Rotate all selected objects
     * @param rotation Rotation quaternion
     * @param aroundCenter If true, rotate around selection center
     */
    void RotateSelection(const glm::quat& rotation, bool aroundCenter = true);

    /**
     * @brief Scale all selected objects
     * @param scale Scale factor
     * @param aroundCenter If true, scale around selection center
     */
    void ScaleSelection(const glm::vec3& scale, bool aroundCenter = true);

    // =========================================================================
    // Visual Settings
    // =========================================================================

    /**
     * @brief Set selection highlight color
     */
    void SetHighlightColor(const glm::vec4& color);

    /**
     * @brief Set selection outline thickness
     */
    void SetOutlineThickness(float thickness);

    /**
     * @brief Enable/disable selection rendering
     */
    void SetRenderSelection(bool enabled);

    // =========================================================================
    // Filter
    // =========================================================================

    /**
     * @brief Set selection filter (only select objects matching filter)
     */
    void SetFilter(std::function<bool(uint64_t)> filter);

    /**
     * @brief Clear selection filter
     */
    void ClearFilter();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(uint64_t)> OnObjectSelected;
    std::function<void(uint64_t)> OnObjectDeselected;
    std::function<void()> OnSelectionChanged;
    std::function<void()> OnSelectionCleared;

private:
    // Selection helpers
    void AddToSelection(uint64_t entityId);
    void RemoveFromSelection(uint64_t entityId);
    void UpdateSelectedObjectData(SelectedObject& obj);
    void RefreshSelectionData();

    // Box select helpers
    std::vector<uint64_t> GetObjectsInBox(const glm::vec2& screenMin, const glm::vec2& screenMax,
                                          const Nova::Camera& camera) const;
    bool IsObjectInScreenRect(uint64_t entityId, const glm::vec2& screenMin,
                              const glm::vec2& screenMax, const Nova::Camera& camera) const;

    // Ray select helpers
    uint64_t RaycastObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance) const;
    bool RayIntersectBounds(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                           const glm::vec3& boundsMin, const glm::vec3& boundsMax,
                           float& t) const;

    // Clipboard helpers
    struct ClipboardEntry {
        std::string assetId;
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };

    // Rendering helpers
    void RenderSelectionOutline(Nova::Renderer& renderer, const Nova::Camera& camera);
    void RenderSelectionBounds(Nova::Renderer& renderer, const Nova::Camera& camera);
    void RenderBoxSelect(Nova::Renderer& renderer);

    // State
    bool m_initialized = false;
    World* m_world = nullptr;

    // Selection data
    std::vector<SelectedObject> m_selectedObjects;
    std::unordered_set<uint64_t> m_selectedSet;  // For fast lookup

    // Box select
    bool m_isBoxSelecting = false;
    glm::vec2 m_boxSelectStart{0.0f};
    glm::vec2 m_boxSelectEnd{0.0f};

    // Clipboard
    std::vector<ClipboardEntry> m_clipboard;

    // Filter
    std::function<bool(uint64_t)> m_filter;
    bool m_hasFilter = false;

    // Visual settings
    glm::vec4 m_highlightColor{1.0f, 0.8f, 0.0f, 1.0f};
    float m_outlineThickness = 2.0f;
    bool m_renderSelection = true;

    // Animation
    float m_pulseTime = 0.0f;
    float m_pulseSpeed = 2.0f;
};

} // namespace Vehement
