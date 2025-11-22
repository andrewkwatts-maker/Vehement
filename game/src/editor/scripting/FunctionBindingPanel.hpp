#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>
#include <variant>

namespace Vehement {

class Editor;
struct FunctionInfo;

/**
 * @brief Event types that can trigger functions
 */
enum class GameEventType {
    // Entity events
    OnCreate,
    OnDestroy,
    OnTick,
    OnDamage,
    OnDeath,
    OnHeal,
    OnCollision,
    OnTriggerEnter,
    OnTriggerExit,

    // Combat events
    OnAttackStart,
    OnAttackHit,
    OnAttackMiss,
    OnKill,
    OnSpellCast,
    OnSpellHit,
    OnAbilityUse,

    // Building events
    OnBuildStart,
    OnBuildComplete,
    OnBuildingDestroyed,
    OnProductionComplete,
    OnUpgradeComplete,

    // Resource events
    OnResourceGather,
    OnResourceDepleted,
    OnTradeComplete,

    // Player events
    OnLevelUp,
    OnQuestComplete,
    OnAchievementUnlock,
    OnDialogStart,
    OnDialogChoice,

    // World events
    OnDayStart,
    OnNightStart,
    OnSeasonChange,
    OnWorldEvent,

    // Custom
    Custom
};

/**
 * @brief Parameter value types for binding
 */
using ParameterValue = std::variant<
    std::monostate,
    bool,
    int,
    float,
    std::string
>;

/**
 * @brief Parameter mapping mode
 */
enum class ParameterMappingMode {
    Constant,       // Use a fixed value
    EventData,      // Map from event data field
    EntityProperty, // Map from source entity property
    Expression      // Python expression
};

/**
 * @brief Maps a function parameter to a source
 */
struct ParameterMapping {
    std::string parameterName;
    std::string parameterType;
    ParameterMappingMode mode = ParameterMappingMode::Constant;
    ParameterValue constantValue;
    std::string sourceField;       // For EventData mode
    std::string entityProperty;    // For EntityProperty mode
    std::string expression;        // For Expression mode
    bool isOptional = false;
    ParameterValue defaultValue;
};

/**
 * @brief Condition for conditional binding
 */
struct BindingCondition {
    std::string leftOperand;        // Field or expression
    std::string operator_;          // ==, !=, <, >, <=, >=, contains
    std::string rightOperand;       // Value or field
    bool useExpression = false;     // Use full Python expression
    std::string expression;         // Python expression returning bool
};

/**
 * @brief Complete function binding configuration
 */
struct FunctionBinding {
    std::string id;                 // Unique identifier
    std::string name;               // Display name

    // Source
    std::string sourceType;         // Entity type or "*" for all
    uint32_t sourceEntityId = 0;    // Specific entity (0 = use type filter)

    // Event
    GameEventType eventType = GameEventType::OnTick;
    std::string customEventName;    // For Custom event type

    // Function
    std::string functionQualifiedName;
    std::vector<ParameterMapping> parameterMappings;

    // Conditions
    std::vector<BindingCondition> conditions;
    bool requireAllConditions = true;  // AND vs OR

    // Options
    bool enabled = true;
    int priority = 0;               // Higher = executes first
    float cooldown = 0.0f;          // Minimum time between triggers
    int maxTriggers = -1;           // -1 = unlimited
    int currentTriggers = 0;

    // Metadata
    std::string description;
    std::string createdBy;
    std::string lastModified;
};

/**
 * @brief Panel for binding Python functions to game events
 *
 * Features:
 * - Source object selector (entity type or specific entity)
 * - Event type dropdown with all available events
 * - Function selector (integrates with FunctionBrowser)
 * - Parameter mapping UI (constants, event data, entity properties)
 * - Condition editor for conditional execution
 * - Test binding button
 * - Enable/disable toggle
 *
 * Usage:
 * @code
 * FunctionBindingPanel panel;
 * panel.Initialize(editor);
 *
 * // Create a binding
 * FunctionBinding binding;
 * binding.sourceType = "zombie";
 * binding.eventType = GameEventType::OnDamage;
 * binding.functionQualifiedName = "scripts.ai.zombie_rage";
 *
 * panel.AddBinding(binding);
 * @endcode
 */
class FunctionBindingPanel {
public:
    FunctionBindingPanel();
    ~FunctionBindingPanel();

    // Non-copyable
    FunctionBindingPanel(const FunctionBindingPanel&) = delete;
    FunctionBindingPanel& operator=(const FunctionBindingPanel&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(Editor* editor);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Rendering
    // =========================================================================

    void Render();
    void Update(float deltaTime);

    // =========================================================================
    // Binding Management
    // =========================================================================

    /**
     * @brief Add a new binding
     * @return Binding ID
     */
    std::string AddBinding(const FunctionBinding& binding);

    /**
     * @brief Remove a binding by ID
     */
    void RemoveBinding(const std::string& id);

    /**
     * @brief Update an existing binding
     */
    void UpdateBinding(const std::string& id, const FunctionBinding& binding);

    /**
     * @brief Get binding by ID
     */
    [[nodiscard]] FunctionBinding* GetBinding(const std::string& id);
    [[nodiscard]] const FunctionBinding* GetBinding(const std::string& id) const;

    /**
     * @brief Get all bindings
     */
    [[nodiscard]] const std::vector<FunctionBinding>& GetAllBindings() const { return m_bindings; }

    /**
     * @brief Get bindings for a specific event type
     */
    [[nodiscard]] std::vector<FunctionBinding*> GetBindingsForEvent(GameEventType eventType);

    /**
     * @brief Get bindings for a specific source type
     */
    [[nodiscard]] std::vector<FunctionBinding*> GetBindingsForSource(const std::string& sourceType);

    /**
     * @brief Clear all bindings
     */
    void ClearBindings();

    // =========================================================================
    // Selection
    // =========================================================================

    [[nodiscard]] bool HasSelection() const { return m_selectedBinding != nullptr; }
    [[nodiscard]] FunctionBinding* GetSelectedBinding() { return m_selectedBinding; }
    void SelectBinding(const std::string& id);
    void ClearSelection();

    // =========================================================================
    // Binding Operations
    // =========================================================================

    /**
     * @brief Enable/disable a binding
     */
    void SetBindingEnabled(const std::string& id, bool enabled);

    /**
     * @brief Test trigger a binding manually
     */
    bool TestBinding(const std::string& id);

    /**
     * @brief Duplicate a binding
     */
    std::string DuplicateBinding(const std::string& id);

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Save bindings to JSON
     */
    bool SaveBindings(const std::string& filePath);

    /**
     * @brief Load bindings from JSON
     */
    bool LoadBindings(const std::string& filePath);

    /**
     * @brief Export binding as standalone config
     */
    std::string ExportBinding(const std::string& id) const;

    /**
     * @brief Import binding from config string
     */
    std::string ImportBinding(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using BindingCallback = std::function<void(const FunctionBinding&)>;
    void SetOnBindingCreated(BindingCallback callback) { m_onBindingCreated = callback; }
    void SetOnBindingModified(BindingCallback callback) { m_onBindingModified = callback; }
    void SetOnBindingDeleted(BindingCallback callback) { m_onBindingDeleted = callback; }

    // =========================================================================
    // Static Helpers
    // =========================================================================

    static const char* GetEventTypeName(GameEventType type);
    static GameEventType ParseEventType(const std::string& name);
    static std::vector<std::string> GetEventDataFields(GameEventType eventType);
    static std::string GetEventDescription(GameEventType eventType);

private:
    // UI rendering
    void RenderToolbar();
    void RenderBindingList();
    void RenderBindingEditor();
    void RenderSourceSelector();
    void RenderEventSelector();
    void RenderFunctionSelector();
    void RenderParameterMappings();
    void RenderConditionEditor();
    void RenderOptionsPanel();
    void RenderTestPanel();
    void RenderNewBindingDialog();
    void RenderImportExportDialog();

    // Parameter mapping UI
    void RenderParameterMapping(ParameterMapping& mapping, int index);
    void RenderConstantValueInput(ParameterMapping& mapping);
    void RenderEventDataSelector(ParameterMapping& mapping);
    void RenderEntityPropertySelector(ParameterMapping& mapping);
    void RenderExpressionInput(ParameterMapping& mapping);

    // Condition UI
    void RenderCondition(BindingCondition& condition, int index);

    // Helpers
    void CreateNewBinding();
    std::string GenerateBindingId();
    void UpdateParameterMappings();
    void ValidateBinding(FunctionBinding& binding);
    bool EvaluateConditions(const FunctionBinding& binding) const;

    // Drag-drop handling
    void HandleFunctionDrop();

    // State
    bool m_initialized = false;
    Editor* m_editor = nullptr;

    // Bindings
    std::vector<FunctionBinding> m_bindings;
    std::unordered_map<std::string, size_t> m_bindingIndex;
    FunctionBinding* m_selectedBinding = nullptr;

    // Editing state
    FunctionBinding m_editingBinding;
    bool m_isEditing = false;

    // Dialogs
    bool m_showNewBindingDialog = false;
    bool m_showImportDialog = false;
    bool m_showExportDialog = false;
    char m_importBuffer[4096] = {0};
    std::string m_exportResult;

    // UI state
    float m_listWidth = 250.0f;
    int m_selectedConditionIndex = -1;
    char m_searchBuffer[256] = {0};

    // Callbacks
    BindingCallback m_onBindingCreated;
    BindingCallback m_onBindingModified;
    BindingCallback m_onBindingDeleted;

    // ID generation
    int m_nextBindingId = 1;
};

} // namespace Vehement
