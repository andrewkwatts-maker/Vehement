/**
 * @file MaterialEditor.hpp
 * @brief Multi-object material editor for Nova3D/Vehement editor
 *
 * Provides unified material editing interface with support for:
 * - Multi-object selection and batch editing
 * - Mixed property indicators for differing values
 * - Batch material assignment
 * - Material comparison view
 * - Shared material management with "Make Unique" option
 * - Full undo/redo integration via EditorCommand system
 */

#pragma once

#include "EditorCommand.hpp"
#include "CommandHistory.hpp"
#include "../graphics/Material.hpp"
#include "../scene/SceneNode.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_set>
#include <variant>

namespace Nova {

// Forward declarations
class SceneNode;
class Material;
class Texture;

// =============================================================================
// Property Value Types
// =============================================================================

/**
 * @brief Variant type for material property values
 */
using MaterialPropertyValue = std::variant<
    float,
    int,
    bool,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    std::string,
    std::shared_ptr<Texture>
>;

/**
 * @brief Material property identifier
 */
enum class MaterialProperty {
    // Basic PBR
    Albedo,
    Metallic,
    Roughness,
    AO,
    Emissive,

    // Optical
    IOR,
    Transmission,
    Thickness,

    // Textures
    AlbedoMap,
    NormalMap,
    MetallicMap,
    RoughnessMap,
    AOMap,
    EmissiveMap,

    // Rendering options
    TwoSided,
    Transparent,

    // Advanced
    ClearCoat,
    ClearCoatRoughness,
    Sheen,
    SheenTint,
    Anisotropic,
    AnisotropicRotation,
    SubsurfaceRadius,
    SubsurfaceColor
};

/**
 * @brief Convert MaterialProperty enum to string
 */
std::string MaterialPropertyToString(MaterialProperty prop);

/**
 * @brief Convert string to MaterialProperty enum
 */
MaterialProperty StringToMaterialProperty(const std::string& str);

// =============================================================================
// Mixed Property State
// =============================================================================

/**
 * @brief Represents the state of a property across multiple selected objects
 *
 * When editing multiple objects, a property can be:
 * - Uniform: All selected objects have the same value
 * - Mixed: Objects have different values
 * - Undefined: Property doesn't apply to some objects
 */
enum class PropertyState {
    Uniform,    ///< All objects have the same value
    Mixed,      ///< Objects have different values
    Undefined   ///< Property not applicable to some objects
};

/**
 * @brief Container for a potentially mixed property value
 */
template<typename T>
struct MixedProperty {
    PropertyState state = PropertyState::Undefined;
    T uniformValue{};                    ///< Value when state is Uniform
    std::optional<T> minValue;           ///< Minimum value when Mixed (for numeric types)
    std::optional<T> maxValue;           ///< Maximum value when Mixed (for numeric types)
    size_t uniqueValueCount = 0;         ///< Number of unique values when Mixed

    /**
     * @brief Check if property has a uniform value
     */
    bool IsUniform() const { return state == PropertyState::Uniform; }

    /**
     * @brief Check if property has mixed values
     */
    bool IsMixed() const { return state == PropertyState::Mixed; }

    /**
     * @brief Check if property is undefined
     */
    bool IsUndefined() const { return state == PropertyState::Undefined; }

    /**
     * @brief Get the uniform value (only valid when IsUniform())
     */
    const T& GetValue() const { return uniformValue; }
};

// =============================================================================
// Material Property Snapshot
// =============================================================================

/**
 * @brief Complete snapshot of a material's properties for undo/redo
 */
struct MaterialSnapshot {
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};
    float ior = 1.5f;
    float transmission = 0.0f;
    float thickness = 1.0f;
    bool twoSided = false;
    bool transparent = false;

    // Texture paths (for restoration)
    std::string albedoMapPath;
    std::string normalMapPath;
    std::string metallicMapPath;
    std::string roughnessMapPath;
    std::string aoMapPath;
    std::string emissiveMapPath;

    /**
     * @brief Capture snapshot from material
     */
    static MaterialSnapshot Capture(const Material* material);

    /**
     * @brief Apply snapshot to material
     */
    void Apply(Material* material) const;

    bool operator==(const MaterialSnapshot& other) const;
    bool operator!=(const MaterialSnapshot& other) const { return !(*this == other); }
};

// =============================================================================
// Material Editor Commands
// =============================================================================

/**
 * @brief Command for changing a material property on one or more objects
 */
class MaterialPropertyCommand : public ICommand {
public:
    /**
     * @brief Create a single-object property change command
     */
    MaterialPropertyCommand(
        SceneNode* node,
        MaterialProperty property,
        const MaterialPropertyValue& newValue
    );

    /**
     * @brief Create a multi-object property change command
     */
    MaterialPropertyCommand(
        const std::vector<SceneNode*>& nodes,
        MaterialProperty property,
        const MaterialPropertyValue& newValue
    );

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    [[nodiscard]] bool CanMergeWith(const ICommand& other) const override;
    bool MergeWith(const ICommand& other) override;

private:
    struct NodeState {
        SceneNode* node;
        MaterialPropertyValue oldValue;
        MaterialPropertyValue newValue;
    };

    std::vector<NodeState> m_nodeStates;
    MaterialProperty m_property;

    void CaptureOldValues();
    void ApplyValue(SceneNode* node, const MaterialPropertyValue& value);
    MaterialPropertyValue GetValue(SceneNode* node) const;
};

/**
 * @brief Command for assigning a material to one or more objects
 */
class AssignMaterialCommand : public ICommand {
public:
    /**
     * @brief Create a single-object material assignment command
     */
    AssignMaterialCommand(
        SceneNode* node,
        std::shared_ptr<Material> newMaterial
    );

    /**
     * @brief Create a multi-object material assignment command
     */
    AssignMaterialCommand(
        const std::vector<SceneNode*>& nodes,
        std::shared_ptr<Material> newMaterial
    );

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    struct NodeState {
        SceneNode* node;
        std::shared_ptr<Material> oldMaterial;
    };

    std::vector<NodeState> m_nodeStates;
    std::shared_ptr<Material> m_newMaterial;
};

/**
 * @brief Command for making a shared material unique for an object
 */
class MakeUniqueMaterialCommand : public ICommand {
public:
    /**
     * @brief Create command to make material unique for a node
     */
    explicit MakeUniqueMaterialCommand(SceneNode* node);

    /**
     * @brief Create command to make materials unique for multiple nodes
     */
    explicit MakeUniqueMaterialCommand(const std::vector<SceneNode*>& nodes);

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

    /**
     * @brief Get the newly created unique materials
     */
    [[nodiscard]] const std::vector<std::shared_ptr<Material>>& GetUniqueMaterials() const {
        return m_uniqueMaterials;
    }

private:
    struct NodeState {
        SceneNode* node;
        std::shared_ptr<Material> originalMaterial;
        std::shared_ptr<Material> uniqueMaterial;
    };

    std::vector<NodeState> m_nodeStates;
    std::vector<std::shared_ptr<Material>> m_uniqueMaterials;

    std::shared_ptr<Material> CloneMaterial(const std::shared_ptr<Material>& source);
};

/**
 * @brief Command for batch material operations (copy properties from source to targets)
 */
class CopyMaterialPropertiesCommand : public ICommand {
public:
    /**
     * @brief Create command to copy properties from source to targets
     */
    CopyMaterialPropertiesCommand(
        const Material* source,
        const std::vector<SceneNode*>& targets,
        const std::vector<MaterialProperty>& propertiesToCopy
    );

    bool Execute() override;
    bool Undo() override;
    [[nodiscard]] std::string GetName() const override;
    [[nodiscard]] CommandTypeId GetTypeId() const override;

private:
    struct NodeState {
        SceneNode* node;
        MaterialSnapshot oldSnapshot;
    };

    std::vector<NodeState> m_nodeStates;
    MaterialSnapshot m_sourceSnapshot;
    std::vector<MaterialProperty> m_propertiesToCopy;
};

// =============================================================================
// Material Comparison
// =============================================================================

/**
 * @brief Result of comparing two materials
 */
struct MaterialComparison {
    struct PropertyDifference {
        MaterialProperty property;
        MaterialPropertyValue valueA;
        MaterialPropertyValue valueB;
        std::string propertyName;
    };

    bool areIdentical = false;
    std::vector<PropertyDifference> differences;
    std::vector<MaterialProperty> matchingProperties;

    /**
     * @brief Get the number of differing properties
     */
    [[nodiscard]] size_t GetDifferenceCount() const { return differences.size(); }

    /**
     * @brief Get the number of matching properties
     */
    [[nodiscard]] size_t GetMatchCount() const { return matchingProperties.size(); }

    /**
     * @brief Get percentage of matching properties
     */
    [[nodiscard]] float GetSimilarity() const;
};

/**
 * @brief Compare two materials and return differences
 */
MaterialComparison CompareMaterials(const Material* a, const Material* b);

// =============================================================================
// Multi-Object Material Editor
// =============================================================================

/**
 * @brief Multi-object material editor with batch editing support
 *
 * This editor allows editing material properties across multiple selected
 * objects simultaneously. When properties differ between objects, the editor
 * shows a "mixed" indicator and allows setting a new value for all objects.
 *
 * Features:
 * - Select multiple objects and edit shared material properties
 * - Show "mixed" indicator for properties that differ
 * - Apply changes to all selected objects
 * - Batch material assignment
 * - Material comparison view
 * - "Make Unique" option to break shared materials
 * - Full undo/redo support via EditorCommand system
 *
 * Usage:
 * @code
 * MaterialEditor editor;
 * editor.SetCommandHistory(&commandHistory);
 *
 * // Select multiple objects
 * editor.SetSelection({node1, node2, node3});
 *
 * // Edit a property (applies to all selected objects)
 * editor.SetPropertyValue(MaterialProperty::Metallic, 0.8f);
 *
 * // Assign same material to all selected objects
 * editor.BatchAssignMaterial(myMaterial);
 *
 * // Render the editor UI
 * editor.RenderUI();
 * @endcode
 */
class MaterialEditor {
public:
    /**
     * @brief Callback signature for property change notifications
     */
    using PropertyChangedCallback = std::function<void(
        MaterialProperty property,
        const std::vector<SceneNode*>& affectedNodes
    )>;

    /**
     * @brief Callback signature for selection change notifications
     */
    using SelectionChangedCallback = std::function<void(
        const std::vector<SceneNode*>& selection
    )>;

    MaterialEditor();
    ~MaterialEditor();

    // Non-copyable
    MaterialEditor(const MaterialEditor&) = delete;
    MaterialEditor& operator=(const MaterialEditor&) = delete;

    // =========================================================================
    // Selection Management
    // =========================================================================

    /**
     * @brief Set the selection to edit
     * @param nodes Vector of scene nodes to edit
     */
    void SetSelection(const std::vector<SceneNode*>& nodes);

    /**
     * @brief Add nodes to the current selection
     * @param nodes Nodes to add
     */
    void AddToSelection(const std::vector<SceneNode*>& nodes);

    /**
     * @brief Remove nodes from the current selection
     * @param nodes Nodes to remove
     */
    void RemoveFromSelection(const std::vector<SceneNode*>& nodes);

    /**
     * @brief Clear the current selection
     */
    void ClearSelection();

    /**
     * @brief Get the current selection
     */
    [[nodiscard]] const std::vector<SceneNode*>& GetSelection() const { return m_selection; }

    /**
     * @brief Get the number of selected objects
     */
    [[nodiscard]] size_t GetSelectionCount() const { return m_selection.size(); }

    /**
     * @brief Check if multiple objects are selected
     */
    [[nodiscard]] bool IsMultiSelection() const { return m_selection.size() > 1; }

    /**
     * @brief Check if any objects are selected
     */
    [[nodiscard]] bool HasSelection() const { return !m_selection.empty(); }

    // =========================================================================
    // Property Access
    // =========================================================================

    /**
     * @brief Get a property value with mixed state information
     * @tparam T Property value type
     * @param property The property to get
     * @return MixedProperty containing value and state
     */
    template<typename T>
    MixedProperty<T> GetProperty(MaterialProperty property) const;

    /**
     * @brief Get property state (uniform, mixed, or undefined)
     */
    [[nodiscard]] PropertyState GetPropertyState(MaterialProperty property) const;

    /**
     * @brief Check if a property has mixed values across selection
     */
    [[nodiscard]] bool IsPropertyMixed(MaterialProperty property) const;

    /**
     * @brief Get all properties that have mixed values
     */
    [[nodiscard]] std::vector<MaterialProperty> GetMixedProperties() const;

    // =========================================================================
    // Property Modification
    // =========================================================================

    /**
     * @brief Set a property value on all selected objects
     * @param property The property to set
     * @param value The new value
     *
     * Creates an undoable command and executes it via CommandHistory.
     */
    void SetPropertyValue(MaterialProperty property, const MaterialPropertyValue& value);

    /**
     * @brief Apply a property change to all selected objects
     * @param property The property to change
     * @param value The new value
     * @param createCommand Whether to create an undo command
     *
     * Use createCommand=false for live preview during drag operations.
     * Call FinalizePropertyChange() when done to create the undo command.
     */
    void ApplyToAll(MaterialProperty property, const MaterialPropertyValue& value, bool createCommand = true);

    /**
     * @brief Finalize a property change after live editing
     *
     * Call this after a series of ApplyToAll calls with createCommand=false
     * to create a single undo command for the entire operation.
     */
    void FinalizePropertyChange(MaterialProperty property);

    /**
     * @brief Reset a property to its default value on all selected objects
     */
    void ResetProperty(MaterialProperty property);

    /**
     * @brief Copy a property value from one object to all others in selection
     * @param sourceNode The node to copy from
     * @param property The property to copy
     */
    void CopyPropertyToAll(SceneNode* sourceNode, MaterialProperty property);

    // =========================================================================
    // Material Assignment
    // =========================================================================

    /**
     * @brief Assign a material to all selected objects
     * @param material The material to assign
     */
    void BatchAssignMaterial(std::shared_ptr<Material> material);

    /**
     * @brief Make materials unique for all selected objects
     *
     * Creates independent copies of shared materials so each object
     * can be edited independently.
     */
    void MakeUnique();

    /**
     * @brief Check if selected objects share any materials
     */
    [[nodiscard]] bool HasSharedMaterials() const;

    /**
     * @brief Get all unique materials in the selection
     */
    [[nodiscard]] std::vector<std::shared_ptr<Material>> GetUniqueMaterials() const;

    /**
     * @brief Get the number of unique materials in the selection
     */
    [[nodiscard]] size_t GetUniqueMaterialCount() const;

    // =========================================================================
    // Material Comparison
    // =========================================================================

    /**
     * @brief Compare materials of two selected nodes
     * @param indexA First node index in selection
     * @param indexB Second node index in selection
     * @return Comparison result
     */
    [[nodiscard]] MaterialComparison CompareSelectedMaterials(size_t indexA, size_t indexB) const;

    /**
     * @brief Get a comparison of all materials in selection against the first
     */
    [[nodiscard]] std::vector<MaterialComparison> CompareAllToFirst() const;

    // =========================================================================
    // Undo/Redo Integration
    // =========================================================================

    /**
     * @brief Set the command history for undo/redo support
     * @param history Pointer to the command history (not owned)
     */
    void SetCommandHistory(CommandHistory* history) { m_commandHistory = history; }

    /**
     * @brief Get the command history
     */
    [[nodiscard]] CommandHistory* GetCommandHistory() const { return m_commandHistory; }

    /**
     * @brief Begin a transaction for grouping multiple operations
     * @param name Transaction name for undo UI
     */
    void BeginTransaction(const std::string& name);

    /**
     * @brief Commit the current transaction
     */
    void CommitTransaction();

    /**
     * @brief Rollback the current transaction
     */
    void RollbackTransaction();

    /**
     * @brief Check if a transaction is active
     */
    [[nodiscard]] bool IsTransactionActive() const;

    // =========================================================================
    // UI Rendering
    // =========================================================================

    /**
     * @brief Render the material editor UI
     *
     * Renders ImGui-based property editors with mixed value indicators.
     */
    void RenderUI();

    /**
     * @brief Render a compact property inspector
     */
    void RenderCompactUI();

    /**
     * @brief Render the material comparison view
     */
    void RenderComparisonView();

    /**
     * @brief Render material assignment panel
     */
    void RenderAssignmentPanel();

    // =========================================================================
    // AI-Powered Material Features
    // =========================================================================

    /**
     * @brief Show AI material generator panel
     *
     * Displays UI for generating PBR materials from reference images or
     * procedural generation. Allows setting parameters and generating
     * material variations.
     */
    void ShowAIMaterialGenerator();

    /**
     * @brief Generate a material from a reference image
     *
     * Uses AI to analyze a reference image and generate PBR material
     * properties (albedo, roughness, metallic, normal map, etc.)
     *
     * @param imagePath Path to the reference image file
     */
    void GenerateMaterialFromImage(const std::string& imagePath);

    /**
     * @brief Suggest material improvements based on analysis
     *
     * Analyzes the currently selected material and provides AI-driven
     * suggestions for improvements based on object type, physical
     * properties, and visual appeal.
     */
    void SuggestMaterialImprovements();

    // =========================================================================
    // Mixed Property UI Helpers
    // =========================================================================

    /**
     * @brief Handle rendering of a potentially mixed float property
     * @param label Property label
     * @param property Property identifier
     * @param min Minimum value
     * @param max Maximum value
     * @return true if value was changed
     */
    bool HandleMixedProperty(
        const char* label,
        MaterialProperty property,
        float min = 0.0f,
        float max = 1.0f
    );

    /**
     * @brief Handle rendering of a potentially mixed color property
     * @param label Property label
     * @param property Property identifier
     * @return true if value was changed
     */
    bool HandleMixedColorProperty(
        const char* label,
        MaterialProperty property
    );

    /**
     * @brief Handle rendering of a potentially mixed bool property
     * @param label Property label
     * @param property Property identifier
     * @return true if value was changed
     */
    bool HandleMixedBoolProperty(
        const char* label,
        MaterialProperty property
    );

    /**
     * @brief Handle rendering of a potentially mixed texture property
     * @param label Property label
     * @param property Property identifier
     * @return true if value was changed
     */
    bool HandleMixedTextureProperty(
        const char* label,
        MaterialProperty property
    );

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for property changes
     */
    void SetOnPropertyChanged(PropertyChangedCallback callback) {
        m_onPropertyChanged = std::move(callback);
    }

    /**
     * @brief Set callback for selection changes
     */
    void SetOnSelectionChanged(SelectionChangedCallback callback) {
        m_onSelectionChanged = std::move(callback);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Enable/disable live preview during edits
     */
    void SetLivePreview(bool enabled) { m_livePreview = enabled; }
    [[nodiscard]] bool IsLivePreviewEnabled() const { return m_livePreview; }

    /**
     * @brief Set the mixed value indicator text
     */
    void SetMixedValueText(const std::string& text) { m_mixedValueText = text; }
    [[nodiscard]] const std::string& GetMixedValueText() const { return m_mixedValueText; }

    /**
     * @brief Enable/disable showing individual values in mixed state
     */
    void SetShowMixedDetails(bool show) { m_showMixedDetails = show; }
    [[nodiscard]] bool GetShowMixedDetails() const { return m_showMixedDetails; }

private:
    // Selection
    std::vector<SceneNode*> m_selection;
    std::unordered_set<SceneNode*> m_selectionSet;  // For fast lookup

    // Command history
    CommandHistory* m_commandHistory = nullptr;

    // Property editing state
    struct EditState {
        MaterialProperty property;
        MaterialPropertyValue startValue;
        std::vector<MaterialPropertyValue> originalValues;
        bool isDragging = false;
    };
    std::optional<EditState> m_currentEdit;

    // Callbacks
    PropertyChangedCallback m_onPropertyChanged;
    SelectionChangedCallback m_onSelectionChanged;

    // Configuration
    bool m_livePreview = true;
    std::string m_mixedValueText = "Mixed";
    bool m_showMixedDetails = false;

    // UI state
    int m_comparisonSourceIndex = 0;
    int m_comparisonTargetIndex = 1;
    bool m_showAdvancedProperties = false;
    bool m_showAIMaterialGenerator = false;

    // Internal helpers
    void UpdatePropertyCache();
    void NotifyPropertyChanged(MaterialProperty property);
    void NotifySelectionChanged();
    void ExecuteCommand(CommandPtr command);

    // Property value extraction
    MaterialPropertyValue ExtractPropertyValue(const Material* material, MaterialProperty property) const;
    void ApplyPropertyValue(Material* material, MaterialProperty property, const MaterialPropertyValue& value);

    // UI rendering helpers
    void RenderBasicProperties();
    void RenderOpticalProperties();
    void RenderTextureProperties();
    void RenderRenderingOptions();
    void RenderAdvancedProperties();
    void RenderSelectionInfo();
    void RenderBatchOperations();

    // Mixed value UI helpers
    bool RenderMixedFloatSlider(const char* label, MaterialProperty property, float min, float max, const char* format = "%.3f");
    bool RenderMixedColorEdit(const char* label, MaterialProperty property);
    bool RenderMixedCheckbox(const char* label, MaterialProperty property);
    bool RenderMixedTextureSlot(const char* label, MaterialProperty property);
    void RenderMixedIndicator(const char* tooltip);
};

// =============================================================================
// Template Implementations
// =============================================================================

template<typename T>
MixedProperty<T> MaterialEditor::GetProperty(MaterialProperty property) const {
    MixedProperty<T> result;

    if (m_selection.empty()) {
        result.state = PropertyState::Undefined;
        return result;
    }

    // Get values from all selected nodes
    std::vector<T> values;
    values.reserve(m_selection.size());

    for (const auto* node : m_selection) {
        if (node && node->HasMaterial()) {
            auto value = ExtractPropertyValue(node->GetMaterial().get(), property);
            if (std::holds_alternative<T>(value)) {
                values.push_back(std::get<T>(value));
            }
        }
    }

    if (values.empty()) {
        result.state = PropertyState::Undefined;
        return result;
    }

    // Check if all values are the same
    const T& firstValue = values[0];
    bool allSame = true;

    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] != firstValue) {
            allSame = false;
            break;
        }
    }

    if (allSame) {
        result.state = PropertyState::Uniform;
        result.uniformValue = firstValue;
        result.uniqueValueCount = 1;
    } else {
        result.state = PropertyState::Mixed;
        result.uniformValue = firstValue;  // Use first as reference

        // Only calculate unique count for hashable types (arithmetic types)
        if constexpr (std::is_arithmetic_v<T>) {
            result.uniqueValueCount = std::unordered_set<T>(values.begin(), values.end()).size();
            result.minValue = *std::min_element(values.begin(), values.end());
            result.maxValue = *std::max_element(values.begin(), values.end());
        } else {
            // For non-hashable types like glm::vec3, just count as "multiple values"
            result.uniqueValueCount = values.size();
        }
    }

    return result;
}

} // namespace Nova
