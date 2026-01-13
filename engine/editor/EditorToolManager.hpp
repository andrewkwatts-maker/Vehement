/**
 * @file EditorToolManager.hpp
 * @brief Active tool tracking and management
 *
 * This class handles:
 * - Active tool tracking
 * - Tool switching
 * - Tool settings persistence
 * - Tool input routing
 *
 * @author Nova Engine Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class TransformGizmo;
class SceneNode;
class Scene;

// =============================================================================
// Transform Tool Mode
// =============================================================================

/**
 * @brief Active transform manipulation tool
 */
enum class TransformTool : uint8_t {
    Select,     ///< Selection only
    Translate,  ///< Move objects
    Rotate,     ///< Rotate objects
    Scale       ///< Scale objects
};

/**
 * @brief Transform coordinate space
 */
enum class TransformSpace : uint8_t {
    World,  ///< World coordinates
    Local   ///< Object local coordinates
};

/**
 * @brief Snapping configuration
 */
struct SnappingSettings {
    bool enabled = false;
    float translateSnap = 1.0f;   ///< Translation snap increment
    float rotateSnap = 15.0f;     ///< Rotation snap in degrees
    float scaleSnap = 0.1f;       ///< Scale snap increment

    // Grid snapping
    bool snapToGrid = false;
    float gridSize = 1.0f;

    // Surface snapping
    bool snapToSurface = false;
    bool alignToSurfaceNormal = false;

    // Vertex snapping
    bool snapToVertex = false;
    float vertexSnapRadius = 0.5f;
};

// =============================================================================
// Tool Settings
// =============================================================================

/**
 * @brief Per-tool settings that can be persisted
 */
struct ToolSettings {
    TransformTool defaultTool = TransformTool::Select;
    TransformSpace defaultSpace = TransformSpace::World;
    SnappingSettings snapping;

    // Gizmo visual settings
    float gizmoSize = 1.0f;
    float gizmoOpacity = 1.0f;
    bool showGizmoLabels = true;

    // Pivot settings
    bool usePivotCenter = true;   ///< Use selection center or individual pivots
    bool useLocalPivot = false;   ///< Use local origin or bounding box center
};

// =============================================================================
// Tool Changed Event
// =============================================================================

/**
 * @brief Tool change event data
 */
struct ToolChangedEvent {
    TransformTool previousTool;
    TransformTool newTool;
};

/**
 * @brief Space change event data
 */
struct SpaceChangedEvent {
    TransformSpace previousSpace;
    TransformSpace newSpace;
};

// =============================================================================
// Editor Tool Manager
// =============================================================================

/**
 * @brief Manages editor tools and their state
 *
 * Responsibilities:
 * - Tool state management
 * - Tool switching and shortcuts
 * - Snapping configuration
 * - Transform gizmo integration
 * - Tool settings persistence
 *
 * Usage:
 *   EditorToolManager toolMgr;
 *   toolMgr.Initialize();
 *
 *   toolMgr.SetTool(TransformTool::Translate);
 *   toolMgr.SetSnappingEnabled(true);
 *
 *   // In update loop
 *   toolMgr.Update(deltaTime, selectedNodes);
 */
class EditorToolManager {
public:
    EditorToolManager() = default;
    ~EditorToolManager() = default;

    // Non-copyable, non-movable
    EditorToolManager(const EditorToolManager&) = delete;
    EditorToolManager& operator=(const EditorToolManager&) = delete;
    EditorToolManager(EditorToolManager&&) = delete;
    EditorToolManager& operator=(EditorToolManager&&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the tool manager
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Tool Selection
    // =========================================================================

    /**
     * @brief Set the active transform tool
     * @param tool Tool to activate
     */
    void SetTool(TransformTool tool);

    /**
     * @brief Get the active transform tool
     */
    [[nodiscard]] TransformTool GetTool() const { return m_activeTool; }

    /**
     * @brief Cycle to next tool
     */
    void NextTool();

    /**
     * @brief Cycle to previous tool
     */
    void PreviousTool();

    // =========================================================================
    // Transform Space
    // =========================================================================

    /**
     * @brief Set the transform space
     * @param space Space to use
     */
    void SetSpace(TransformSpace space);

    /**
     * @brief Get the current transform space
     */
    [[nodiscard]] TransformSpace GetSpace() const { return m_activeSpace; }

    /**
     * @brief Toggle between World and Local space
     */
    void ToggleSpace();

    // =========================================================================
    // Snapping
    // =========================================================================

    /**
     * @brief Enable/disable snapping
     */
    void SetSnappingEnabled(bool enabled);

    /**
     * @brief Check if snapping is enabled
     */
    [[nodiscard]] bool IsSnappingEnabled() const { return m_settings.snapping.enabled; }

    /**
     * @brief Toggle snapping on/off
     */
    void ToggleSnapping();

    /**
     * @brief Set translation snap increment
     */
    void SetTranslateSnap(float snap) { m_settings.snapping.translateSnap = snap; }

    /**
     * @brief Get translation snap increment
     */
    [[nodiscard]] float GetTranslateSnap() const { return m_settings.snapping.translateSnap; }

    /**
     * @brief Set rotation snap increment (degrees)
     */
    void SetRotateSnap(float snap) { m_settings.snapping.rotateSnap = snap; }

    /**
     * @brief Get rotation snap increment
     */
    [[nodiscard]] float GetRotateSnap() const { return m_settings.snapping.rotateSnap; }

    /**
     * @brief Set scale snap increment
     */
    void SetScaleSnap(float snap) { m_settings.snapping.scaleSnap = snap; }

    /**
     * @brief Get scale snap increment
     */
    [[nodiscard]] float GetScaleSnap() const { return m_settings.snapping.scaleSnap; }

    /**
     * @brief Get full snapping settings
     */
    [[nodiscard]] SnappingSettings& GetSnappingSettings() { return m_settings.snapping; }
    [[nodiscard]] const SnappingSettings& GetSnappingSettings() const { return m_settings.snapping; }

    // =========================================================================
    // Gizmo Integration
    // =========================================================================

    /**
     * @brief Get the transform gizmo
     */
    [[nodiscard]] TransformGizmo* GetGizmo() { return m_gizmo.get(); }
    [[nodiscard]] const TransformGizmo* GetGizmo() const { return m_gizmo.get(); }

    /**
     * @brief Check if gizmo is currently being manipulated
     */
    [[nodiscard]] bool IsGizmoActive() const;

    /**
     * @brief Cancel current gizmo manipulation
     */
    void CancelGizmoManipulation();

    /**
     * @brief Update gizmo with current selection
     * @param selectedNodes Currently selected nodes
     */
    void UpdateGizmo(const std::vector<SceneNode*>& selectedNodes);

    /**
     * @brief Set gizmo visibility
     */
    void SetGizmoVisible(bool visible);

    /**
     * @brief Check if gizmo is visible
     */
    [[nodiscard]] bool IsGizmoVisible() const { return m_gizmoVisible; }

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Get tool settings
     */
    [[nodiscard]] ToolSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const ToolSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Load settings from file
     * @param path Settings file path
     * @return true if load succeeded
     */
    bool LoadSettings(const std::string& path);

    /**
     * @brief Save settings to file
     * @param path Settings file path
     * @return true if save succeeded
     */
    bool SaveSettings(const std::string& path);

    /**
     * @brief Reset to default settings
     */
    void ResetSettings();

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set tool changed callback
     */
    void SetOnToolChanged(std::function<void(const ToolChangedEvent&)> callback);

    /**
     * @brief Set space changed callback
     */
    void SetOnSpaceChanged(std::function<void(const SpaceChangedEvent&)> callback);

    /**
     * @brief Set snapping changed callback
     */
    void SetOnSnappingChanged(std::function<void(bool enabled)> callback);

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update tool state
     * @param deltaTime Frame delta time
     * @param selectedNodes Currently selected nodes
     */
    void Update(float deltaTime, const std::vector<SceneNode*>& selectedNodes);

    /**
     * @brief Render tool overlays (toolbar buttons, etc)
     */
    void RenderToolbar();

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get display name for a tool
     */
    [[nodiscard]] static const char* GetToolName(TransformTool tool);

    /**
     * @brief Get icon character/name for a tool
     */
    [[nodiscard]] static const char* GetToolIcon(TransformTool tool);

    /**
     * @brief Get display name for a space
     */
    [[nodiscard]] static const char* GetSpaceName(TransformSpace space);

    /**
     * @brief Apply snapping to a value
     */
    [[nodiscard]] float SnapValue(float value, float snap) const;

    /**
     * @brief Apply snapping to a position
     */
    [[nodiscard]] glm::vec3 SnapPosition(const glm::vec3& position) const;

    /**
     * @brief Apply snapping to a rotation (euler angles in degrees)
     */
    [[nodiscard]] glm::vec3 SnapRotation(const glm::vec3& rotation) const;

    /**
     * @brief Apply snapping to a scale
     */
    [[nodiscard]] glm::vec3 SnapScale(const glm::vec3& scale) const;

private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================

    void SyncGizmoSettings();
    void NotifyToolChanged(TransformTool previous);
    void NotifySpaceChanged(TransformSpace previous);
    void RenderTransformToolButtons();
    void RenderSnappingControls();
    void RenderSpaceToggle();

    // =========================================================================
    // Member Variables
    // =========================================================================

    bool m_initialized = false;

    // Active state
    TransformTool m_activeTool = TransformTool::Select;
    TransformSpace m_activeSpace = TransformSpace::World;

    // Settings
    ToolSettings m_settings;

    // Transform gizmo
    std::unique_ptr<TransformGizmo> m_gizmo;
    bool m_gizmoVisible = true;

    // Callbacks
    std::function<void(const ToolChangedEvent&)> m_onToolChanged;
    std::function<void(const SpaceChangedEvent&)> m_onSpaceChanged;
    std::function<void(bool)> m_onSnappingChanged;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get display name for transform tool
 */
const char* GetTransformToolName(TransformTool tool);

/**
 * @brief Get icon for transform tool
 */
const char* GetTransformToolIcon(TransformTool tool);

/**
 * @brief Get display name for transform space
 */
const char* GetTransformSpaceName(TransformSpace space);

} // namespace Nova
