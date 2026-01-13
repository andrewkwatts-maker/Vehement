/**
 * @file SDFToolbox.hpp
 * @brief SDF Toolbox panel for the Nova3D/Vehement editor
 *
 * Provides quick primitive creation and CSG operations:
 * - Primitive creation with button grid
 * - CSG operations (Union, Subtract, Intersect) with smooth variants
 * - CSG tree visualization
 * - Quick actions (duplicate, mirror, convert)
 * - Parameter quick-edit
 * - Library/asset browser for SDF assets
 * - Tool modes (Create, Edit, CSG)
 * - Full undo/redo support
 */

#pragma once

#include "../sdf/SDFPrimitive.hpp"
#include "../sdf/SDFModel.hpp"
#include "../ui/EditorPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include "EditorCommand.hpp"
#include "CommandHistory.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>

namespace Nova {

// Forward declarations
class Scene;
class SceneNode;
class SDFModel;
class SDFPrimitive;
class CommandHistory;
class Texture;

// =============================================================================
// Tool Modes
// =============================================================================

/**
 * @brief Active tool mode for the SDF toolbox
 */
enum class SDFToolMode : uint8_t {
    Create,     ///< Click to place primitives
    Edit,       ///< Modify selected primitive
    CSG         ///< Combine primitives with CSG operations
};

/**
 * @brief Get display name for tool mode
 */
const char* GetToolModeName(SDFToolMode mode);

/**
 * @brief Get icon for tool mode
 */
const char* GetToolModeIcon(SDFToolMode mode);

// =============================================================================
// Primitive Preset
// =============================================================================

/**
 * @brief Saved preset for a primitive with custom parameters
 */
struct SDFPrimitivePreset {
    std::string name;
    std::string category;
    SDFPrimitiveType type;
    SDFParameters parameters;
    SDFMaterial material;
    bool isFavorite = false;
    std::string iconPath;

    /**
     * @brief Create primitive from this preset
     */
    [[nodiscard]] std::unique_ptr<SDFPrimitive> CreatePrimitive() const;
};

// =============================================================================
// CSG Tree Node (for visualization)
// =============================================================================

/**
 * @brief Node in the CSG tree visualization
 */
struct CSGTreeNode {
    SDFPrimitive* primitive = nullptr;
    std::string displayName;
    CSGOperation operation = CSGOperation::Union;
    bool expanded = true;
    bool selected = false;
    std::vector<CSGTreeNode> children;

    /**
     * @brief Build tree from SDF primitive hierarchy
     */
    static CSGTreeNode BuildFromPrimitive(SDFPrimitive* root);
};

// =============================================================================
// Asset Library Item
// =============================================================================

/**
 * @brief Item in the SDF asset library
 */
struct SDFAssetLibraryItem {
    std::string name;
    std::string path;
    std::string category;
    std::shared_ptr<Texture> thumbnail;
    bool isFavorite = false;
    uint64_t lastUsed = 0;
};

// =============================================================================
// SDF Toolbox Commands
// =============================================================================

/**
 * @brief Command for creating an SDF primitive
 */
class CreateSDFPrimitiveCommand : public ICommand {
public:
    /**
     * @brief Create a primitive creation command
     * @param model The SDF model to add the primitive to
     * @param type Type of primitive to create
     * @param position World position for the primitive
     * @param parameters Initial parameters
     * @param parent Optional parent primitive for CSG hierarchy
     */
    CreateSDFPrimitiveCommand(
        SDFModel* model,
        SDFPrimitiveType type,
        const glm::vec3& position,
        const SDFParameters& parameters = {},
        SDFPrimitive* parent = nullptr
    );

    /**
     * @brief Create from an existing primitive (for paste/duplicate)
     */
    CreateSDFPrimitiveCommand(
        SDFModel* model,
        std::unique_ptr<SDFPrimitive> primitive,
        SDFPrimitive* parent = nullptr
    );

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the created primitive (valid after Execute)
     */
    [[nodiscard]] SDFPrimitive* GetCreatedPrimitive() const { return m_primitivePtr; }

private:
    SDFModel* m_model;
    SDFPrimitiveType m_type;
    glm::vec3 m_position;
    SDFParameters m_parameters;
    SDFPrimitive* m_parent;

    std::unique_ptr<SDFPrimitive> m_ownedPrimitive;
    SDFPrimitive* m_primitivePtr = nullptr;
    std::string m_primitiveName;
};

/**
 * @brief Command for deleting an SDF primitive
 */
class DeleteSDFPrimitiveCommand : public ICommand {
public:
    /**
     * @brief Create a primitive deletion command
     * @param model The SDF model containing the primitive
     * @param primitive The primitive to delete
     */
    DeleteSDFPrimitiveCommand(SDFModel* model, SDFPrimitive* primitive);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    SDFModel* m_model;
    std::unique_ptr<SDFPrimitive> m_ownedPrimitive;
    SDFPrimitive* m_primitivePtr = nullptr;
    SDFPrimitive* m_parent = nullptr;
    size_t m_siblingIndex = 0;
    std::string m_primitiveName;
};

/**
 * @brief Command for CSG operations
 */
class CSGOperationCommand : public ICommand {
public:
    /**
     * @brief Create a CSG operation command
     * @param model The SDF model
     * @param primitiveA First operand
     * @param primitiveB Second operand
     * @param operation CSG operation to perform
     * @param smoothness Blend factor for smooth operations
     */
    CSGOperationCommand(
        SDFModel* model,
        SDFPrimitive* primitiveA,
        SDFPrimitive* primitiveB,
        CSGOperation operation,
        float smoothness = 0.0f
    );

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the resulting combined primitive
     */
    [[nodiscard]] SDFPrimitive* GetResultPrimitive() const { return m_resultPrimitive; }

private:
    SDFModel* m_model;
    SDFPrimitive* m_primitiveA;
    SDFPrimitive* m_primitiveB;
    CSGOperation m_operation;
    float m_smoothness;

    // State for undo
    SDFPrimitive* m_originalParentA = nullptr;
    SDFPrimitive* m_originalParentB = nullptr;
    size_t m_originalIndexA = 0;
    size_t m_originalIndexB = 0;
    CSGOperation m_originalOperationB;

    SDFPrimitive* m_resultPrimitive = nullptr;
    std::unique_ptr<SDFPrimitive> m_ownedResult;
};

/**
 * @brief Command for modifying SDF primitive parameters
 */
class ModifySDFParametersCommand : public ICommand {
public:
    /**
     * @brief Create a parameter modification command
     * @param primitive The primitive to modify
     * @param newParams New parameters
     */
    ModifySDFParametersCommand(SDFPrimitive* primitive, const SDFParameters& newParams);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    SDFPrimitive* m_primitive;
    SDFParameters m_oldParams;
    SDFParameters m_newParams;
};

/**
 * @brief Command for transforming SDF primitive
 */
class TransformSDFPrimitiveCommand : public ICommand {
public:
    /**
     * @brief Create a transform command
     * @param primitive The primitive to transform
     * @param newTransform New transform
     */
    TransformSDFPrimitiveCommand(SDFPrimitive* primitive, const SDFTransform& newTransform);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    SDFPrimitive* m_primitive;
    SDFTransform m_oldTransform;
    SDFTransform m_newTransform;
};

/**
 * @brief Command for mirroring SDF primitive
 */
class MirrorSDFPrimitiveCommand : public ICommand {
public:
    enum class Axis { X, Y, Z };

    /**
     * @brief Create a mirror command
     * @param model The SDF model
     * @param primitive The primitive to mirror
     * @param axis Axis to mirror across
     */
    MirrorSDFPrimitiveCommand(SDFModel* model, SDFPrimitive* primitive, Axis axis);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the mirrored copy
     */
    [[nodiscard]] SDFPrimitive* GetMirroredPrimitive() const { return m_mirroredPtr; }

private:
    SDFModel* m_model;
    SDFPrimitive* m_original;
    Axis m_axis;
    std::unique_ptr<SDFPrimitive> m_ownedMirrored;
    SDFPrimitive* m_mirroredPtr = nullptr;
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback signatures for SDF toolbox events
 */
struct SDFToolboxCallbacks {
    /// Called when a primitive is created
    std::function<void(SDFPrimitive*)> onPrimitiveCreated;

    /// Called when a primitive is selected
    std::function<void(SDFPrimitive*)> onPrimitiveSelected;

    /// Called when selection changes
    std::function<void(const std::vector<SDFPrimitive*>&)> onSelectionChanged;

    /// Called when CSG operation is applied
    std::function<void(SDFPrimitive* result)> onCSGApplied;

    /// Called when SDF is converted to mesh
    std::function<void(SDFModel*, const std::string& meshPath)> onConvertedToMesh;

    /// Called when mesh is converted to SDF
    std::function<void(const std::string& meshPath, SDFModel*)> onConvertedFromMesh;

    /// Called when tool mode changes
    std::function<void(SDFToolMode)> onToolModeChanged;

    /// Called when requesting precise positioning dialog
    std::function<bool(glm::vec3& position, glm::vec3& size)> onPrecisePositionDialog;
};

// =============================================================================
// SDF Toolbox Panel
// =============================================================================

/**
 * @brief SDF Toolbox panel for quick primitive creation and CSG operations
 *
 * Features:
 * - Primitive creation button grid
 * - CSG operations (Union, Subtract, Intersect)
 * - Smooth variants with blend factor
 * - CSG tree visualization
 * - Quick actions (duplicate, mirror, convert)
 * - Parameter quick-edit
 * - Preset library
 * - Asset browser with drag-drop
 * - Tool modes (Create, Edit, CSG)
 * - Keyboard shortcuts
 * - Full undo/redo support
 *
 * Usage:
 *   SDFToolbox toolbox;
 *   toolbox.SetActiveModel(&myModel);
 *   toolbox.Callbacks.onPrimitiveCreated = [](SDFPrimitive* p) {
 *       // Handle primitive creation
 *   };
 *   // In render loop:
 *   toolbox.Render();
 */
class SDFToolbox : public EditorPanel {
public:
    SDFToolbox();
    ~SDFToolbox() override;

    // Non-copyable
    SDFToolbox(const SDFToolbox&) = delete;
    SDFToolbox& operator=(const SDFToolbox&) = delete;

    // =========================================================================
    // Model Management
    // =========================================================================

    /**
     * @brief Set the active SDF model to work with
     */
    void SetActiveModel(SDFModel* model);

    /**
     * @brief Get the current active model
     */
    [[nodiscard]] SDFModel* GetActiveModel() const { return m_activeModel; }

    /**
     * @brief Set the scene for conversions
     */
    void SetScene(Scene* scene) { m_scene = scene; }

    // =========================================================================
    // Selection
    // =========================================================================

    /**
     * @brief Select a primitive
     */
    void SelectPrimitive(SDFPrimitive* primitive, bool addToSelection = false);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected primitives
     */
    [[nodiscard]] const std::vector<SDFPrimitive*>& GetSelection() const { return m_selectedPrimitives; }

    /**
     * @brief Get primary (last) selected primitive
     */
    [[nodiscard]] SDFPrimitive* GetPrimarySelection() const;

    // =========================================================================
    // Tool Mode
    // =========================================================================

    /**
     * @brief Set active tool mode
     */
    void SetToolMode(SDFToolMode mode);

    /**
     * @brief Get active tool mode
     */
    [[nodiscard]] SDFToolMode GetToolMode() const { return m_toolMode; }

    /**
     * @brief Set active primitive type for Create mode
     */
    void SetActivePrimitiveType(SDFPrimitiveType type);

    /**
     * @brief Get active primitive type
     */
    [[nodiscard]] SDFPrimitiveType GetActivePrimitiveType() const { return m_activePrimitiveType; }

    // =========================================================================
    // Primitive Creation
    // =========================================================================

    /**
     * @brief Create a primitive at the specified position
     * @param type Primitive type
     * @param position World position
     * @param parameters Optional custom parameters
     * @return Created primitive
     */
    SDFPrimitive* CreatePrimitive(
        SDFPrimitiveType type,
        const glm::vec3& position,
        const SDFParameters& parameters = {}
    );

    /**
     * @brief Create primitive from preset
     * @param preset The preset to use
     * @param position World position
     * @return Created primitive
     */
    SDFPrimitive* CreateFromPreset(
        const SDFPrimitivePreset& preset,
        const glm::vec3& position
    );

    /**
     * @brief Start drag-to-place creation
     */
    void BeginDragCreate(SDFPrimitiveType type, const glm::vec3& startPos);

    /**
     * @brief Update drag-to-place creation
     */
    void UpdateDragCreate(const glm::vec3& currentPos);

    /**
     * @brief Finish drag-to-place creation
     */
    SDFPrimitive* EndDragCreate();

    /**
     * @brief Cancel drag-to-place creation
     */
    void CancelDragCreate();

    /**
     * @brief Check if drag creation is in progress
     */
    [[nodiscard]] bool IsDragCreating() const { return m_isDragCreating; }

    // =========================================================================
    // CSG Operations
    // =========================================================================

    /**
     * @brief Apply CSG operation to selected primitives
     * @param operation CSG operation
     * @param smoothness Blend factor for smooth operations
     * @return Resulting primitive
     */
    SDFPrimitive* ApplyCSGOperation(CSGOperation operation, float smoothness = 0.0f);

    /**
     * @brief Set CSG operation for preview
     */
    void SetCSGPreviewOperation(CSGOperation operation);

    /**
     * @brief Get CSG preview operation
     */
    [[nodiscard]] std::optional<CSGOperation> GetCSGPreviewOperation() const { return m_csgPreviewOperation; }

    /**
     * @brief Clear CSG preview
     */
    void ClearCSGPreview();

    // =========================================================================
    // Quick Actions
    // =========================================================================

    /**
     * @brief Duplicate selected primitives
     * @return Duplicated primitives
     */
    std::vector<SDFPrimitive*> DuplicateSelected();

    /**
     * @brief Mirror selected primitive along axis
     * @param axis Axis to mirror (0=X, 1=Y, 2=Z)
     * @return Mirrored primitive
     */
    SDFPrimitive* MirrorSelected(int axis);

    /**
     * @brief Center selected primitive to origin
     */
    void CenterToOrigin();

    /**
     * @brief Reset transform of selected primitive
     */
    void ResetTransform();

    /**
     * @brief Convert selected SDF to mesh
     * @param settings Mesh generation settings
     * @return Generated mesh path or empty on failure
     */
    std::string ConvertToMesh(const SDFMeshSettings& settings = {});

    /**
     * @brief Convert mesh to SDF (approximation)
     * @param meshPath Path to mesh file
     * @return Created SDF model
     */
    SDFModel* ConvertFromMesh(const std::string& meshPath);

    // =========================================================================
    // Presets
    // =========================================================================

    /**
     * @brief Save current selection as preset
     * @param name Preset name
     * @param category Optional category
     */
    void SavePreset(const std::string& name, const std::string& category = "Custom");

    /**
     * @brief Load preset by name
     * @param name Preset name
     * @return Preset or nullptr if not found
     */
    [[nodiscard]] const SDFPrimitivePreset* GetPreset(const std::string& name) const;

    /**
     * @brief Get all presets
     */
    [[nodiscard]] const std::vector<SDFPrimitivePreset>& GetPresets() const { return m_presets; }

    /**
     * @brief Get presets by category
     */
    [[nodiscard]] std::vector<const SDFPrimitivePreset*> GetPresetsByCategory(const std::string& category) const;

    /**
     * @brief Delete preset by name
     */
    bool DeletePreset(const std::string& name);

    /**
     * @brief Toggle preset favorite status
     */
    void TogglePresetFavorite(const std::string& name);

    /**
     * @brief Get favorite presets
     */
    [[nodiscard]] std::vector<const SDFPrimitivePreset*> GetFavoritePresets() const;

    // =========================================================================
    // Asset Library
    // =========================================================================

    /**
     * @brief Refresh asset library
     */
    void RefreshAssetLibrary();

    /**
     * @brief Get library items
     */
    [[nodiscard]] const std::vector<SDFAssetLibraryItem>& GetLibraryItems() const { return m_libraryItems; }

    /**
     * @brief Load asset from library
     * @param path Asset path
     * @return Loaded model or nullptr
     */
    SDFModel* LoadAsset(const std::string& path);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set command history for undo/redo
     */
    void SetCommandHistory(CommandHistory* history) { m_commandHistory = history; }

    /**
     * @brief Set default position for new primitives
     */
    void SetDefaultPosition(const glm::vec3& position) { m_defaultPosition = position; }

    /**
     * @brief Set grid snap for placement
     */
    void SetSnapToGrid(bool snap, float gridSize = 0.5f);

    /**
     * @brief Get grid snap status
     */
    [[nodiscard]] bool IsSnapToGridEnabled() const { return m_snapToGrid; }

    /**
     * @brief Set smooth blend factor default
     */
    void SetDefaultSmoothness(float smoothness) { m_defaultSmoothness = smoothness; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    SDFToolboxCallbacks Callbacks;

protected:
    // =========================================================================
    // EditorPanel Overrides
    // =========================================================================

    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderMenuBar() override;
    void OnSearchChanged(const std::string& filter) override;
    void OnInitialize() override;
    void OnShutdown() override;
    void Update(float deltaTime) override;

    bool CanUndo() const override;
    bool CanRedo() const override;
    void OnUndo() override;
    void OnRedo() override;

private:
    // =========================================================================
    // Rendering Sections
    // =========================================================================

    void RenderToolModeSelector();
    void RenderPrimitiveGrid();
    void RenderCSGOperations();
    void RenderCSGTreeView();
    void RenderQuickActions();
    void RenderParameterEditor();
    void RenderPresetLibrary();
    void RenderAssetBrowser();
    void RenderPrimitiveButton(SDFPrimitiveType type, const char* icon, const char* tooltip);
    void RenderCSGTreeNode(const CSGTreeNode& node, int depth = 0);

    // =========================================================================
    // Input Handling
    // =========================================================================

    void HandleKeyboardShortcuts();
    void HandlePrimitiveShortcut(int number);

    // =========================================================================
    // Utility
    // =========================================================================

    glm::vec3 SnapToGrid(const glm::vec3& position) const;
    std::string GeneratePrimitiveName(SDFPrimitiveType type) const;
    void LoadDefaultPresets();
    void SavePresetsToFile();
    void LoadPresetsFromFile();
    void NotifySelectionChanged();
    void RebuildCSGTree();

    const char* GetPrimitiveIcon(SDFPrimitiveType type) const;
    const char* GetPrimitiveName(SDFPrimitiveType type) const;
    const char* GetCSGOperationIcon(CSGOperation op) const;
    const char* GetCSGOperationName(CSGOperation op) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Active model and scene
    SDFModel* m_activeModel = nullptr;
    Scene* m_scene = nullptr;

    // Selection
    std::vector<SDFPrimitive*> m_selectedPrimitives;
    std::unordered_set<SDFPrimitive*> m_selectedSet;

    // Tool mode
    SDFToolMode m_toolMode = SDFToolMode::Create;
    SDFPrimitiveType m_activePrimitiveType = SDFPrimitiveType::Sphere;

    // Creation state
    bool m_isDragCreating = false;
    glm::vec3 m_dragStartPos{0.0f};
    glm::vec3 m_dragCurrentPos{0.0f};
    SDFPrimitive* m_dragPreviewPrimitive = nullptr;

    // CSG state
    std::optional<CSGOperation> m_csgPreviewOperation;
    bool m_smoothCSG = false;
    float m_csgSmoothness = 0.1f;

    // CSG tree
    CSGTreeNode m_csgTreeRoot;
    bool m_csgTreeNeedsRebuild = true;

    // Presets
    std::vector<SDFPrimitivePreset> m_presets;
    std::string m_presetFilter;
    char m_presetFilterBuffer[256] = "";
    std::string m_presetSaveNameBuffer;
    bool m_showPresetSaveDialog = false;

    // Asset library
    std::vector<SDFAssetLibraryItem> m_libraryItems;
    std::string m_libraryFilter;
    char m_libraryFilterBuffer[256] = "";
    std::string m_libraryPath = "assets/sdf";
    bool m_libraryNeedsRefresh = true;

    // Configuration
    glm::vec3 m_defaultPosition{0.0f};
    bool m_snapToGrid = false;
    float m_gridSize = 0.5f;
    float m_defaultSmoothness = 0.1f;

    // UI state
    bool m_showPrimitiveSection = true;
    bool m_showCSGSection = true;
    bool m_showQuickActionsSection = true;
    bool m_showParameterSection = true;
    bool m_showPresetSection = false;
    bool m_showLibrarySection = false;

    // Primitive counters for naming
    std::unordered_map<SDFPrimitiveType, uint32_t> m_primitiveCounters;

    // Command history
    CommandHistory* m_commandHistory = nullptr;

    // Keyboard shortcut state
    static constexpr int NUM_PRIMITIVE_SHORTCUTS = 11;
    SDFPrimitiveType m_shortcutTypes[NUM_PRIMITIVE_SHORTCUTS] = {
        SDFPrimitiveType::Sphere,
        SDFPrimitiveType::Box,
        SDFPrimitiveType::Cylinder,
        SDFPrimitiveType::Capsule,
        SDFPrimitiveType::Cone,
        SDFPrimitiveType::Torus,
        SDFPrimitiveType::Plane,
        SDFPrimitiveType::RoundedBox,
        SDFPrimitiveType::Ellipsoid,
        SDFPrimitiveType::Pyramid,
        SDFPrimitiveType::Prism
    };
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get default parameters for a primitive type
 */
SDFParameters GetDefaultParameters(SDFPrimitiveType type);

/**
 * @brief Estimate size from parameters for UI display
 */
glm::vec3 EstimatePrimitiveSize(SDFPrimitiveType type, const SDFParameters& params);

/**
 * @brief Calculate size from drag distance
 */
SDFParameters CalculateParametersFromDrag(
    SDFPrimitiveType type,
    const glm::vec3& startPos,
    const glm::vec3& endPos
);

} // namespace Nova
