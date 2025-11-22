#pragma once

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <variant>
#include <functional>
#include <glm/glm.hpp>

namespace Vehement {

// Forward declarations
class InGameEditor;
class World;
class MapFile;

/**
 * @brief Trigger variable types
 */
enum class TriggerVariableType : uint8_t {
    Integer,
    Real,
    Boolean,
    String,
    Unit,
    UnitGroup,
    Player,
    Point,
    Region,
    Timer,
    Dialog,
    Sound,
    Effect,
    Ability
};

/**
 * @brief Variable value storage
 */
using TriggerValue = std::variant<
    int32_t,            // Integer
    float,              // Real
    bool,               // Boolean
    std::string,        // String
    uint32_t,           // Unit/Player/Timer/Dialog/Sound/Effect/Ability ID
    std::vector<uint32_t>, // UnitGroup
    glm::vec2,          // Point
    uint32_t            // Region ID
>;

/**
 * @brief Trigger variable definition
 */
struct TriggerVariable {
    std::string name;
    TriggerVariableType type = TriggerVariableType::Integer;
    TriggerValue value;
    bool isArray = false;
    int arraySize = 0;
    bool isGlobal = true;
    std::string comment;
};

/**
 * @brief Event types that can fire triggers
 */
enum class TriggerEventType : uint8_t {
    // Time events
    MapInit,            // Map initialization
    TimerExpires,       // Timer reaches zero
    PeriodicEvent,      // Fires every X seconds

    // Unit events
    UnitEntersRegion,   // Unit enters a region
    UnitLeavesRegion,   // Unit leaves a region
    UnitDies,           // Unit is killed
    UnitSpawns,         // Unit is created
    UnitAttacked,       // Unit takes damage
    UnitStartsAbility,  // Unit begins ability
    UnitFinishesAbility,// Unit completes ability
    UnitAcquiresItem,   // Unit picks up item
    UnitSellsItem,      // Unit sells item

    // Player events
    PlayerLeavesGame,   // Player disconnects
    PlayerChats,        // Player sends message
    PlayerSelectsUnit,  // Player selects unit
    PlayerIssuesOrder,  // Player gives order

    // Building events
    ConstructionStarts, // Building begins
    ConstructionFinishes,// Building completes
    BuildingDestroyed,  // Building is destroyed
    UpgradeStarts,      // Upgrade begins
    UpgradeFinishes,    // Upgrade completes
    ResearchStarts,     // Research begins
    ResearchFinishes,   // Research completes

    // Resource events
    ResourceDepleted,   // Resource node empty
    ResourceGathered,   // Resources collected

    // Game events
    GameOver,           // Game ends
    DialogButtonClicked,// Dialog button pressed
    Custom              // Custom script event
};

/**
 * @brief Condition types for trigger evaluation
 */
enum class TriggerConditionType : uint8_t {
    // Comparison
    IntegerCompare,     // Compare two integers
    RealCompare,        // Compare two reals
    BooleanCompare,     // Compare booleans
    StringCompare,      // Compare strings

    // Unit conditions
    UnitTypeIs,         // Unit is of type
    UnitBelongsTo,      // Unit owned by player
    UnitInRegion,       // Unit is in region
    UnitIsAlive,        // Unit is alive
    UnitHasAbility,     // Unit has ability
    UnitHasItem,        // Unit has item
    UnitHealthPercent,  // Unit health percentage

    // Player conditions
    PlayerHasResources, // Player has X resources
    PlayerHasUnits,     // Player has X units
    PlayerHasBuilding,  // Player has building type
    PlayerIsAlly,       // Players are allied
    PlayerIsEnemy,      // Players are enemies

    // Game conditions
    GameTimeElapsed,    // Game time > X
    VariableIsSet,      // Variable equals value

    // Logic
    And,                // All conditions true
    Or,                 // Any condition true
    Not,                // Negate condition

    Custom              // Custom script condition
};

/**
 * @brief Action types for trigger execution
 */
enum class TriggerActionType : uint8_t {
    // Unit actions
    CreateUnit,         // Spawn unit
    RemoveUnit,         // Remove unit
    KillUnit,           // Kill unit
    MoveUnit,           // Move unit to point
    OrderUnit,          // Issue order to unit
    SetUnitOwner,       // Change unit owner
    DamageUnit,         // Deal damage
    HealUnit,           // Heal unit
    AddAbility,         // Give unit ability
    RemoveAbility,      // Remove ability
    AddItem,            // Give unit item
    RemoveItem,         // Remove item

    // Player actions
    SetResources,       // Set player resources
    AddResources,       // Add resources
    RemoveResources,    // Remove resources
    SetAlliance,        // Change alliance
    Defeat,             // Defeat player
    Victory,            // Victory for player

    // Camera actions
    PanCamera,          // Move camera
    SetCameraTarget,    // Lock camera to unit
    CinematicMode,      // Enable/disable cinematic
    FadeScreen,         // Fade to black/white

    // Dialog actions
    ShowDialog,         // Display dialog
    HideDialog,         // Hide dialog
    ShowMessage,        // Show floating text
    DisplayText,        // Show text to player
    ClearMessages,      // Clear messages

    // Sound/Music
    PlaySound,          // Play sound effect
    PlayMusic,          // Play music track
    StopMusic,          // Stop music
    SetVolume,          // Set volume

    // Effect actions
    CreateEffect,       // Create visual effect
    DestroyEffect,      // Remove effect
    AddWeather,         // Add weather effect
    RemoveWeather,      // Remove weather

    // Timer actions
    StartTimer,         // Start a timer
    PauseTimer,         // Pause timer
    ResumeTimer,        // Resume timer
    DestroyTimer,       // Remove timer

    // Variable actions
    SetVariable,        // Set variable value
    ModifyVariable,     // Modify variable

    // Control flow
    Wait,               // Pause execution
    RunTrigger,         // Execute another trigger
    EnableTrigger,      // Enable trigger
    DisableTrigger,     // Disable trigger
    IfThenElse,         // Conditional execution
    ForLoop,            // Loop N times
    ForEachUnit,        // Loop over units
    WhileLoop,          // Loop while condition

    // Game actions
    EndGame,            // End the game
    PauseGame,          // Pause game
    ResumeGame,         // Resume game
    SetTimeOfDay,       // Set day/night
    SetGameSpeed,       // Change game speed

    Custom              // Custom script action
};

/**
 * @brief Parameter for trigger components
 */
struct TriggerParameter {
    std::string name;
    TriggerVariableType type;
    TriggerValue value;
    bool isVariable = false;  // True if referencing a variable
    std::string variableName; // Variable name if isVariable
};

/**
 * @brief Event definition
 */
struct TriggerEvent {
    TriggerEventType type;
    std::vector<TriggerParameter> parameters;
    std::string comment;
};

/**
 * @brief Condition definition
 */
struct TriggerCondition {
    TriggerConditionType type;
    std::vector<TriggerParameter> parameters;
    std::vector<TriggerCondition> subConditions; // For And/Or/Not
    std::string comment;
};

/**
 * @brief Action definition
 */
struct TriggerAction {
    TriggerActionType type;
    std::vector<TriggerParameter> parameters;
    std::vector<TriggerAction> subActions; // For loops and conditionals
    std::vector<TriggerAction> elseActions; // For IfThenElse
    std::string comment;
};

/**
 * @brief Complete trigger definition
 */
struct Trigger {
    uint32_t id = 0;
    std::string name;
    std::string comment;
    bool enabled = true;
    bool initiallyOn = true;
    bool runOnce = false;

    std::vector<TriggerEvent> events;
    std::vector<TriggerCondition> conditions;
    std::vector<TriggerAction> actions;

    // For nested trigger organization
    uint32_t parentGroupId = 0;
};

/**
 * @brief Trigger group for organization
 */
struct TriggerGroup {
    uint32_t id = 0;
    std::string name;
    std::string comment;
    bool expanded = true;
    std::vector<uint32_t> triggerIds;
    std::vector<uint32_t> childGroupIds;
    uint32_t parentGroupId = 0;
};

/**
 * @brief Trigger editor command for undo/redo
 */
class TriggerEditorCommand {
public:
    virtual ~TriggerEditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
};

/**
 * @brief Trigger Editor - WC3-style event/condition/action system
 *
 * Provides a visual trigger editing system similar to Warcraft 3:
 * - Event types (unit enters, timer, player action, etc.)
 * - Conditions (unit type, player, variable check)
 * - Actions (spawn, damage, dialog, camera, etc.)
 * - Variables (integer, real, string, unit, player)
 * - Trigger groups for organization
 * - Enable/disable triggers
 */
class TriggerEditor {
public:
    TriggerEditor();
    ~TriggerEditor();

    // Non-copyable
    TriggerEditor(const TriggerEditor&) = delete;
    TriggerEditor& operator=(const TriggerEditor&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize(InGameEditor& parent);
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // File Operations
    // =========================================================================

    void LoadFromFile(const MapFile& file);
    void SaveToFile(MapFile& file) const;
    void ApplyTriggers(World& world);

    // =========================================================================
    // Update and Render
    // =========================================================================

    void Update(float deltaTime);
    void Render();
    void ProcessInput();

    // =========================================================================
    // Trigger Management
    // =========================================================================

    uint32_t CreateTrigger(const std::string& name);
    void DeleteTrigger(uint32_t id);
    void RenameTrigger(uint32_t id, const std::string& name);
    void EnableTrigger(uint32_t id, bool enabled);
    [[nodiscard]] Trigger* GetTrigger(uint32_t id);
    [[nodiscard]] const std::vector<Trigger>& GetTriggers() const { return m_triggers; }

    void SelectTrigger(uint32_t id);
    [[nodiscard]] uint32_t GetSelectedTriggerId() const noexcept { return m_selectedTriggerId; }
    [[nodiscard]] Trigger* GetSelectedTrigger();

    // =========================================================================
    // Group Management
    // =========================================================================

    uint32_t CreateGroup(const std::string& name, uint32_t parentId = 0);
    void DeleteGroup(uint32_t id);
    void RenameGroup(uint32_t id, const std::string& name);
    void MoveToGroup(uint32_t triggerId, uint32_t groupId);
    [[nodiscard]] TriggerGroup* GetGroup(uint32_t id);
    [[nodiscard]] const std::vector<TriggerGroup>& GetGroups() const { return m_groups; }

    // =========================================================================
    // Variable Management
    // =========================================================================

    void CreateVariable(const TriggerVariable& variable);
    void DeleteVariable(const std::string& name);
    void UpdateVariable(const std::string& name, const TriggerVariable& variable);
    [[nodiscard]] TriggerVariable* GetVariable(const std::string& name);
    [[nodiscard]] const std::vector<TriggerVariable>& GetVariables() const { return m_variables; }

    // =========================================================================
    // Event Management
    // =========================================================================

    void AddEvent(uint32_t triggerId, const TriggerEvent& event);
    void RemoveEvent(uint32_t triggerId, size_t index);
    void UpdateEvent(uint32_t triggerId, size_t index, const TriggerEvent& event);

    // =========================================================================
    // Condition Management
    // =========================================================================

    void AddCondition(uint32_t triggerId, const TriggerCondition& condition);
    void RemoveCondition(uint32_t triggerId, size_t index);
    void UpdateCondition(uint32_t triggerId, size_t index, const TriggerCondition& condition);

    // =========================================================================
    // Action Management
    // =========================================================================

    void AddAction(uint32_t triggerId, const TriggerAction& action);
    void RemoveAction(uint32_t triggerId, size_t index);
    void UpdateAction(uint32_t triggerId, size_t index, const TriggerAction& action);
    void MoveAction(uint32_t triggerId, size_t fromIndex, size_t toIndex);

    // =========================================================================
    // Validation
    // =========================================================================

    [[nodiscard]] bool ValidateTrigger(uint32_t id, std::string& errorMessage) const;
    [[nodiscard]] bool ValidateAll(std::vector<std::string>& errors) const;

    // =========================================================================
    // Testing
    // =========================================================================

    void TestTrigger(uint32_t id);
    void SetDebugMode(bool enabled);
    [[nodiscard]] bool IsDebugMode() const noexcept { return m_debugMode; }

    // =========================================================================
    // Templates
    // =========================================================================

    [[nodiscard]] std::vector<std::string> GetEventTemplates() const;
    [[nodiscard]] std::vector<std::string> GetConditionTemplates() const;
    [[nodiscard]] std::vector<std::string> GetActionTemplates() const;

    TriggerEvent CreateEventFromTemplate(const std::string& templateName) const;
    TriggerCondition CreateConditionFromTemplate(const std::string& templateName) const;
    TriggerAction CreateActionFromTemplate(const std::string& templateName) const;

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    void ExecuteCommand(std::unique_ptr<TriggerEditorCommand> command);
    void Undo();
    void Redo();
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
    void ClearHistory();

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(uint32_t)> OnTriggerCreated;
    std::function<void(uint32_t)> OnTriggerDeleted;
    std::function<void(uint32_t)> OnTriggerSelected;
    std::function<void()> OnTriggersModified;

private:
    // Rendering helpers
    void RenderTriggerTree();
    void RenderTriggerDetails();
    void RenderEventEditor();
    void RenderConditionEditor();
    void RenderActionEditor();
    void RenderVariableManager();
    void RenderDebugPanel();

    // Tree rendering helpers
    void RenderGroupNode(TriggerGroup& group);
    void RenderTriggerNode(Trigger& trigger);

    // Parameter editing
    void RenderParameterEditor(TriggerParameter& param);

    // Syntax generation for preview
    std::string GenerateTriggerSyntax(const Trigger& trigger) const;
    std::string GenerateEventSyntax(const TriggerEvent& event) const;
    std::string GenerateConditionSyntax(const TriggerCondition& condition) const;
    std::string GenerateActionSyntax(const TriggerAction& action) const;

    // ID generation
    uint32_t GenerateTriggerId();
    uint32_t GenerateGroupId();

    // State
    bool m_initialized = false;
    InGameEditor* m_parent = nullptr;

    // Triggers and organization
    std::vector<Trigger> m_triggers;
    std::vector<TriggerGroup> m_groups;
    std::vector<TriggerVariable> m_variables;

    // Selection state
    uint32_t m_selectedTriggerId = 0;
    uint32_t m_selectedGroupId = 0;
    int m_selectedEventIndex = -1;
    int m_selectedConditionIndex = -1;
    int m_selectedActionIndex = -1;

    // Editor state
    bool m_showVariableManager = false;
    bool m_showDebugPanel = false;
    bool m_debugMode = false;

    // Undo/Redo
    std::deque<std::unique_ptr<TriggerEditorCommand>> m_undoStack;
    std::deque<std::unique_ptr<TriggerEditorCommand>> m_redoStack;
    static constexpr size_t MAX_UNDO_HISTORY = 100;

    // ID counters
    uint32_t m_nextTriggerId = 1;
    uint32_t m_nextGroupId = 1;

    // Template definitions
    std::unordered_map<std::string, TriggerEvent> m_eventTemplates;
    std::unordered_map<std::string, TriggerCondition> m_conditionTemplates;
    std::unordered_map<std::string, TriggerAction> m_actionTemplates;
};

// Helper functions for trigger type names
[[nodiscard]] const char* GetEventTypeName(TriggerEventType type);
[[nodiscard]] const char* GetConditionTypeName(TriggerConditionType type);
[[nodiscard]] const char* GetActionTypeName(TriggerActionType type);
[[nodiscard]] const char* GetVariableTypeName(TriggerVariableType type);

} // namespace Vehement
