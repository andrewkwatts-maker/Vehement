#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Nova {
    class SDFModel;
    class SDFPrimitive;
    class SDFAnimationClip;
    class SDFAnimationStateMachine;
    class SDFAnimationController;
    class SDFPoseLibrary;
    class Renderer;
    class Camera;
    class Mesh;
    class Texture;
    struct SDFTransform;
}

namespace Vehement {

class Editor;

/**
 * @brief Gizmo mode for transform manipulation
 */
enum class SDFGizmoMode {
    None,
    Translate,
    Rotate,
    Scale
};

/**
 * @brief Tool mode for SDF editing
 */
enum class SDFToolMode {
    Select,
    Create,
    Paint,
    Sculpt
};

/**
 * @brief Paint brush settings
 */
struct SDFBrushSettings {
    float radius = 0.1f;
    float hardness = 0.5f;
    float opacity = 1.0f;
    glm::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
    std::string currentLayer;
};

/**
 * @brief In-game/in-editor SDF Model Editor
 *
 * Full-featured editor for creating and editing SDF-based models:
 * - Primitive hierarchy manipulation
 * - Transform gizmos (translate/rotate/scale)
 * - Keyframe animation editing
 * - Pose library management
 * - Texture painting
 * - Live preview with mesh generation
 * - JSON export/import
 */
class SDFModelEditor {
public:
    SDFModelEditor();
    ~SDFModelEditor();

    // Non-copyable
    SDFModelEditor(const SDFModelEditor&) = delete;
    SDFModelEditor& operator=(const SDFModelEditor&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize editor
     */
    bool Initialize(Editor* editor);

    /**
     * @brief Shutdown editor
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Model Management
    // =========================================================================

    /**
     * @brief Create new model
     */
    void NewModel(const std::string& name = "NewModel");

    /**
     * @brief Load model from file
     */
    bool LoadModel(const std::string& path);

    /**
     * @brief Save model to file
     */
    bool SaveModel(const std::string& path);

    /**
     * @brief Get current model
     */
    [[nodiscard]] Nova::SDFModel* GetModel() { return m_model.get(); }

    /**
     * @brief Set model for editing
     */
    void SetModel(std::unique_ptr<Nova::SDFModel> model);

    /**
     * @brief Check if model has unsaved changes
     */
    [[nodiscard]] bool HasUnsavedChanges() const { return m_dirty; }

    /**
     * @brief Mark model as modified
     */
    void MarkDirty() { m_dirty = true; }

    // =========================================================================
    // Update and Rendering
    // =========================================================================

    /**
     * @brief Update editor
     */
    void Update(float deltaTime);

    /**
     * @brief Render editor UI
     */
    void RenderUI();

    /**
     * @brief Render 3D viewport content
     */
    void Render3D(Nova::Renderer& renderer, Nova::Camera& camera);

    /**
     * @brief Process input
     */
    void ProcessInput();

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select primitive
     */
    void SelectPrimitive(Nova::SDFPrimitive* primitive);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected primitive
     */
    [[nodiscard]] Nova::SDFPrimitive* GetSelectedPrimitive() { return m_selectedPrimitive; }

    /**
     * @brief Select primitive by picking ray
     */
    Nova::SDFPrimitive* PickPrimitive(const glm::vec3& rayOrigin, const glm::vec3& rayDir);

    // =========================================================================
    // Primitive Operations
    // =========================================================================

    /**
     * @brief Add primitive to model
     */
    Nova::SDFPrimitive* AddPrimitive(Nova::SDFPrimitiveType type, Nova::SDFPrimitive* parent = nullptr);

    /**
     * @brief Duplicate selected primitive
     */
    Nova::SDFPrimitive* DuplicateSelected();

    /**
     * @brief Delete selected primitive
     */
    void DeleteSelected();

    /**
     * @brief Group selected primitives
     */
    void GroupSelected();

    /**
     * @brief Ungroup selected
     */
    void UngroupSelected();

    // =========================================================================
    // Animation
    // =========================================================================

    /**
     * @brief Get animation controller
     */
    [[nodiscard]] Nova::SDFAnimationController* GetAnimationController() { return m_animController.get(); }

    /**
     * @brief Get pose library
     */
    [[nodiscard]] Nova::SDFPoseLibrary* GetPoseLibrary() { return m_poseLibrary.get(); }

    /**
     * @brief Save current pose
     */
    void SaveCurrentPose(const std::string& name, const std::string& category = "Default");

    /**
     * @brief Apply pose to model
     */
    void ApplyPose(const std::string& name);

    /**
     * @brief Start recording keyframes
     */
    void StartRecording();

    /**
     * @brief Stop recording
     */
    void StopRecording();

    /**
     * @brief Is recording
     */
    [[nodiscard]] bool IsRecording() const { return m_isRecording; }

    /**
     * @brief Add keyframe at current time
     */
    void AddKeyframe();

    /**
     * @brief Set animation time
     */
    void SetAnimationTime(float time);

    /**
     * @brief Get animation time
     */
    [[nodiscard]] float GetAnimationTime() const { return m_animationTime; }

    /**
     * @brief Play animation
     */
    void PlayAnimation();

    /**
     * @brief Pause animation
     */
    void PauseAnimation();

    /**
     * @brief Stop animation
     */
    void StopAnimation();

    // =========================================================================
    // Tool Modes
    // =========================================================================

    /**
     * @brief Set tool mode
     */
    void SetToolMode(SDFToolMode mode) { m_toolMode = mode; }

    /**
     * @brief Get tool mode
     */
    [[nodiscard]] SDFToolMode GetToolMode() const { return m_toolMode; }

    /**
     * @brief Set gizmo mode
     */
    void SetGizmoMode(SDFGizmoMode mode) { m_gizmoMode = mode; }

    /**
     * @brief Get gizmo mode
     */
    [[nodiscard]] SDFGizmoMode GetGizmoMode() const { return m_gizmoMode; }

    /**
     * @brief Get brush settings
     */
    [[nodiscard]] SDFBrushSettings& GetBrushSettings() { return m_brushSettings; }

    // =========================================================================
    // Export
    // =========================================================================

    /**
     * @brief Export to entity JSON (units/buildings/heroes)
     */
    bool ExportToEntityJson(const std::string& jsonPath);

    /**
     * @brief Import from entity JSON
     */
    bool ImportFromEntityJson(const std::string& jsonPath);

    /**
     * @brief Export mesh to OBJ
     */
    bool ExportMeshOBJ(const std::string& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(Nova::SDFPrimitive*)> OnPrimitiveSelected;
    std::function<void()> OnModelChanged;
    std::function<void(const std::string&)> OnPoseSaved;
    std::function<void(float)> OnAnimationTimeChanged;

private:
    // UI Panels
    void RenderHierarchyPanel();
    void RenderInspectorPanel();
    void RenderToolbarPanel();
    void RenderTimelinePanel();
    void RenderPoseLibraryPanel();
    void RenderPrimitiveCreator();
    void RenderMeshSettingsPanel();
    void RenderPaintPanel();

    // Hierarchy helpers
    void RenderPrimitiveNode(Nova::SDFPrimitive* primitive);

    // Gizmo rendering
    void RenderGizmo(Nova::Renderer& renderer, Nova::Camera& camera);
    void UpdateGizmo(Nova::Camera& camera);

    // Input helpers
    void HandleMouseInput(Nova::Camera& camera);
    void HandleKeyboardInput();

    // Mesh preview
    void UpdateMeshPreview();

    Editor* m_editor = nullptr;
    bool m_initialized = false;
    bool m_dirty = false;

    // Model and animation
    std::unique_ptr<Nova::SDFModel> m_model;
    std::unique_ptr<Nova::SDFAnimationController> m_animController;
    std::unique_ptr<Nova::SDFPoseLibrary> m_poseLibrary;
    std::shared_ptr<Nova::SDFAnimationClip> m_currentClip;
    std::unique_ptr<Nova::SDFAnimationStateMachine> m_stateMachine;

    // Selection
    Nova::SDFPrimitive* m_selectedPrimitive = nullptr;

    // Tools
    SDFToolMode m_toolMode = SDFToolMode::Select;
    SDFGizmoMode m_gizmoMode = SDFGizmoMode::Translate;
    SDFBrushSettings m_brushSettings;

    // Animation state
    float m_animationTime = 0.0f;
    bool m_isPlaying = false;
    bool m_isRecording = false;
    float m_animationSpeed = 1.0f;

    // Gizmo state
    bool m_gizmoActive = false;
    glm::vec3 m_gizmoStartPos;
    glm::quat m_gizmoStartRot;
    glm::vec3 m_gizmoStartScale;
    int m_activeGizmoAxis = -1;

    // Mesh preview
    std::shared_ptr<Nova::Mesh> m_previewMesh;
    bool m_needsMeshUpdate = true;
    int m_meshResolution = 32;  // Lower for preview

    // UI state
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showTimeline = true;
    bool m_showPoseLibrary = true;
    bool m_showMeshSettings = false;
    bool m_showPaintPanel = false;

    // Create dialog state
    bool m_showCreateDialog = false;
    Nova::SDFPrimitiveType m_createType = Nova::SDFPrimitiveType::Sphere;
    char m_createName[64] = "NewPrimitive";

    // File paths
    std::string m_currentFilePath;
    std::string m_lastExportPath;
};

} // namespace Vehement
