/**
 * @file EventNodes.hpp
 * @brief Visual scripting nodes for event-driven entity logic
 *
 * Provides a node-based system for binding events to Python functions,
 * core event types, and entity state management.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <unordered_map>
#include <any>
#include <glm/glm.hpp>

namespace Nova {

// Forward declarations
class Entity;
class EventGraph;
class EventCompiler;

// ============================================================================
// Event Data Types
// ============================================================================

enum class EventDataType {
    Void,
    Bool,
    Int,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Entity,
    EntityList,
    Animation,
    Mesh,
    Component,
    Any
};

const char* EventDataTypeToString(EventDataType type);
bool AreEventTypesCompatible(EventDataType from, EventDataType to);

// ============================================================================
// Event Pin
// ============================================================================

enum class EventPinKind {
    Flow,       // Execution flow (white)
    Data        // Data connection (colored by type)
};

enum class EventPinDirection {
    Input,
    Output
};

struct EventPin {
    std::string name;
    std::string displayName;
    EventDataType dataType = EventDataType::Void;
    EventPinKind kind = EventPinKind::Data;
    EventPinDirection direction = EventPinDirection::Input;

    // Default value for unconnected inputs
    std::variant<bool, int, float, std::string, glm::vec2, glm::vec3, glm::vec4> defaultValue;

    // Connection info
    uint64_t connectedNodeId = 0;
    std::string connectedPinName;

    uint64_t id = 0;
    bool hidden = false;

    [[nodiscard]] bool IsConnected() const { return connectedNodeId != 0; }
    [[nodiscard]] bool IsFlow() const { return kind == EventPinKind::Flow; }
};

// ============================================================================
// Event Node Categories
// ============================================================================

enum class EventNodeCategory {
    // Events (entry points)
    EventTrigger,       // OnSpawn, OnDeath, OnDamage, etc.
    EventCustom,        // Custom named events

    // Flow Control
    FlowControl,        // Branch, Sequence, ForEach, etc.

    // Entity Operations
    EntityState,        // Get/Set state, properties
    EntityMesh,         // Mesh operations
    EntityAnimation,    // Animation control
    EntityComponent,    // Add/Remove components
    EntityMovement,     // Movement, pathfinding

    // Combat
    Combat,             // Damage, healing, abilities

    // World
    World,              // Spawn, destroy, find entities
    Terrain,            // Terrain queries and modification

    // Math/Logic
    Math,               // Math operations
    Logic,              // Boolean logic
    Comparison,         // Comparisons

    // Data
    Variables,          // Get/Set variables
    Arrays,             // Array operations

    // Python Integration
    Python,             // Call Python functions

    // Debug
    Debug,              // Print, breakpoints

    // UI
    UI                  // UI updates, notifications
};

const char* EventNodeCategoryToString(EventNodeCategory category);

// ============================================================================
// Base Event Node
// ============================================================================

using EventNodeId = uint64_t;

class EventNode : public std::enable_shared_from_this<EventNode> {
public:
    using Ptr = std::shared_ptr<EventNode>;

    EventNode(const std::string& name);
    virtual ~EventNode() = default;

    // Identity
    [[nodiscard]] EventNodeId GetId() const { return m_id; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] const std::string& GetDisplayName() const { return m_displayName; }
    void SetDisplayName(const std::string& name) { m_displayName = name; }

    // Category
    [[nodiscard]] virtual EventNodeCategory GetCategory() const = 0;
    [[nodiscard]] virtual const char* GetTypeName() const = 0;
    [[nodiscard]] virtual const char* GetDescription() const { return ""; }

    // Pins
    [[nodiscard]] const std::vector<EventPin>& GetInputs() const { return m_inputs; }
    [[nodiscard]] const std::vector<EventPin>& GetOutputs() const { return m_outputs; }
    [[nodiscard]] EventPin* GetInput(const std::string& name);
    [[nodiscard]] EventPin* GetOutput(const std::string& name);
    [[nodiscard]] const EventPin* GetInput(const std::string& name) const;
    [[nodiscard]] const EventPin* GetOutput(const std::string& name) const;

    // Code generation for Python
    [[nodiscard]] virtual std::string GenerateCode(EventCompiler& compiler) const = 0;

    // Runtime execution (for interpreted mode)
    virtual void Execute(class EventContext& context) const {}

    // Visual position
    void SetPosition(const glm::vec2& pos) { m_position = pos; }
    [[nodiscard]] const glm::vec2& GetPosition() const { return m_position; }

    // Serialization
    [[nodiscard]] virtual std::string ToJson() const;
    virtual void FromJson(const std::string& json);

protected:
    void AddFlowInput(const std::string& name, const std::string& displayName = "");
    void AddFlowOutput(const std::string& name, const std::string& displayName = "");
    void AddDataInput(const std::string& name, EventDataType type, const std::string& displayName = "");
    void AddDataOutput(const std::string& name, EventDataType type, const std::string& displayName = "");

    template<typename T>
    void SetInputDefault(const std::string& name, const T& value);

    [[nodiscard]] std::string GetInputValue(const std::string& name, EventCompiler& compiler) const;

    EventNodeId m_id;
    std::string m_name;
    std::string m_displayName;

    std::vector<EventPin> m_inputs;
    std::vector<EventPin> m_outputs;

    glm::vec2 m_position{0, 0};

    static EventNodeId s_nextId;
};

// ============================================================================
// EVENT TRIGGER NODES
// ============================================================================

class OnSpawnNode : public EventNode {
public:
    OnSpawnNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnSpawn"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called when entity is spawned"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnDeathNode : public EventNode {
public:
    OnDeathNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnDeath"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called when entity dies"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnDamageNode : public EventNode {
public:
    OnDamageNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnDamage"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called when entity takes damage"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnSelectionNode : public EventNode {
public:
    OnSelectionNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnSelection"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called when entity is selected/deselected"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnCommandNode : public EventNode {
public:
    OnCommandNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnCommand"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called when entity receives a command"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnCollisionNode : public EventNode {
public:
    OnCollisionNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnCollision"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called on collision with another entity"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnTimerNode : public EventNode {
public:
    OnTimerNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventTrigger; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnTimer"; }
    [[nodiscard]] const char* GetDescription() const override { return "Called periodically or after delay"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class OnCustomEventNode : public EventNode {
public:
    OnCustomEventNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EventCustom; }
    [[nodiscard]] const char* GetTypeName() const override { return "OnCustomEvent"; }
    [[nodiscard]] const char* GetDescription() const override { return "Listen for custom named event"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;

    void SetEventName(const std::string& name) { m_eventName = name; }
private:
    std::string m_eventName;
};

// ============================================================================
// FLOW CONTROL NODES
// ============================================================================

class BranchNode : public EventNode {
public:
    BranchNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::FlowControl; }
    [[nodiscard]] const char* GetTypeName() const override { return "Branch"; }
    [[nodiscard]] const char* GetDescription() const override { return "Branch based on condition"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SequenceNode : public EventNode {
public:
    SequenceNode(int outputs = 2);
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::FlowControl; }
    [[nodiscard]] const char* GetTypeName() const override { return "Sequence"; }
    [[nodiscard]] const char* GetDescription() const override { return "Execute outputs in sequence"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
private:
    int m_outputCount;
};

class ForEachNode : public EventNode {
public:
    ForEachNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::FlowControl; }
    [[nodiscard]] const char* GetTypeName() const override { return "ForEach"; }
    [[nodiscard]] const char* GetDescription() const override { return "Loop over array elements"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class WhileLoopNode : public EventNode {
public:
    WhileLoopNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::FlowControl; }
    [[nodiscard]] const char* GetTypeName() const override { return "WhileLoop"; }
    [[nodiscard]] const char* GetDescription() const override { return "Loop while condition is true"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class DelayNode : public EventNode {
public:
    DelayNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::FlowControl; }
    [[nodiscard]] const char* GetTypeName() const override { return "Delay"; }
    [[nodiscard]] const char* GetDescription() const override { return "Delay execution"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// ENTITY STATE NODES
// ============================================================================

class GetStateNode : public EventNode {
public:
    GetStateNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityState; }
    [[nodiscard]] const char* GetTypeName() const override { return "GetState"; }
    [[nodiscard]] const char* GetDescription() const override { return "Get entity state value"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetStateNode : public EventNode {
public:
    SetStateNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityState; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetState"; }
    [[nodiscard]] const char* GetDescription() const override { return "Set entity state value"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class GetHealthNode : public EventNode {
public:
    GetHealthNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityState; }
    [[nodiscard]] const char* GetTypeName() const override { return "GetHealth"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetHealthNode : public EventNode {
public:
    SetHealthNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityState; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetHealth"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class GetPositionNode : public EventNode {
public:
    GetPositionNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityState; }
    [[nodiscard]] const char* GetTypeName() const override { return "GetPosition"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetPositionNode : public EventNode {
public:
    SetPositionNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityState; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetPosition"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// ENTITY MESH NODES
// ============================================================================

class SetMeshNode : public EventNode {
public:
    SetMeshNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMesh; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetMesh"; }
    [[nodiscard]] const char* GetDescription() const override { return "Change entity mesh"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetMaterialNode : public EventNode {
public:
    SetMaterialNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMesh; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetMaterial"; }
    [[nodiscard]] const char* GetDescription() const override { return "Change entity material"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetScaleNode : public EventNode {
public:
    SetScaleNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMesh; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetScale"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetVisibleNode : public EventNode {
public:
    SetVisibleNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMesh; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetVisible"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// ANIMATION NODES
// ============================================================================

class PlayAnimationNode : public EventNode {
public:
    PlayAnimationNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityAnimation; }
    [[nodiscard]] const char* GetTypeName() const override { return "PlayAnimation"; }
    [[nodiscard]] const char* GetDescription() const override { return "Play animation clip"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class StopAnimationNode : public EventNode {
public:
    StopAnimationNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityAnimation; }
    [[nodiscard]] const char* GetTypeName() const override { return "StopAnimation"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class BlendAnimationNode : public EventNode {
public:
    BlendAnimationNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityAnimation; }
    [[nodiscard]] const char* GetTypeName() const override { return "BlendAnimation"; }
    [[nodiscard]] const char* GetDescription() const override { return "Blend between animations"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetAnimationSpeedNode : public EventNode {
public:
    SetAnimationSpeedNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityAnimation; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetAnimationSpeed"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// COMPONENT NODES
// ============================================================================

class AddComponentNode : public EventNode {
public:
    AddComponentNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityComponent; }
    [[nodiscard]] const char* GetTypeName() const override { return "AddComponent"; }
    [[nodiscard]] const char* GetDescription() const override { return "Add component to entity"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class RemoveComponentNode : public EventNode {
public:
    RemoveComponentNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityComponent; }
    [[nodiscard]] const char* GetTypeName() const override { return "RemoveComponent"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class HasComponentNode : public EventNode {
public:
    HasComponentNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityComponent; }
    [[nodiscard]] const char* GetTypeName() const override { return "HasComponent"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class GetComponentNode : public EventNode {
public:
    GetComponentNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityComponent; }
    [[nodiscard]] const char* GetTypeName() const override { return "GetComponent"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// MOVEMENT NODES
// ============================================================================

class MoveToNode : public EventNode {
public:
    MoveToNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMovement; }
    [[nodiscard]] const char* GetTypeName() const override { return "MoveTo"; }
    [[nodiscard]] const char* GetDescription() const override { return "Move entity to position"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class StopMovementNode : public EventNode {
public:
    StopMovementNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMovement; }
    [[nodiscard]] const char* GetTypeName() const override { return "StopMovement"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class FollowEntityNode : public EventNode {
public:
    FollowEntityNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMovement; }
    [[nodiscard]] const char* GetTypeName() const override { return "FollowEntity"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class SetSpeedNode : public EventNode {
public:
    SetSpeedNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::EntityMovement; }
    [[nodiscard]] const char* GetTypeName() const override { return "SetSpeed"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// COMBAT NODES
// ============================================================================

class DealDamageNode : public EventNode {
public:
    DealDamageNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Combat; }
    [[nodiscard]] const char* GetTypeName() const override { return "DealDamage"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class HealNode : public EventNode {
public:
    HealNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Combat; }
    [[nodiscard]] const char* GetTypeName() const override { return "Heal"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class UseAbilityNode : public EventNode {
public:
    UseAbilityNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Combat; }
    [[nodiscard]] const char* GetTypeName() const override { return "UseAbility"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class ApplyEffectNode : public EventNode {
public:
    ApplyEffectNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Combat; }
    [[nodiscard]] const char* GetTypeName() const override { return "ApplyEffect"; }
    [[nodiscard]] const char* GetDescription() const override { return "Apply status effect"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// WORLD NODES
// ============================================================================

class SpawnEntityNode : public EventNode {
public:
    SpawnEntityNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::World; }
    [[nodiscard]] const char* GetTypeName() const override { return "SpawnEntity"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class DestroyEntityNode : public EventNode {
public:
    DestroyEntityNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::World; }
    [[nodiscard]] const char* GetTypeName() const override { return "DestroyEntity"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class FindEntitiesNode : public EventNode {
public:
    FindEntitiesNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::World; }
    [[nodiscard]] const char* GetTypeName() const override { return "FindEntities"; }
    [[nodiscard]] const char* GetDescription() const override { return "Find entities in range"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class GetClosestEntityNode : public EventNode {
public:
    GetClosestEntityNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::World; }
    [[nodiscard]] const char* GetTypeName() const override { return "GetClosestEntity"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class BroadcastEventNode : public EventNode {
public:
    BroadcastEventNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::World; }
    [[nodiscard]] const char* GetTypeName() const override { return "BroadcastEvent"; }
    [[nodiscard]] const char* GetDescription() const override { return "Broadcast event to event bus"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// TERRAIN NODES
// ============================================================================

class GetTerrainHeightNode : public EventNode {
public:
    GetTerrainHeightNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Terrain; }
    [[nodiscard]] const char* GetTypeName() const override { return "GetTerrainHeight"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class ModifyTerrainNode : public EventNode {
public:
    ModifyTerrainNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Terrain; }
    [[nodiscard]] const char* GetTypeName() const override { return "ModifyTerrain"; }
    [[nodiscard]] const char* GetDescription() const override { return "Modify terrain using SDF"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class CreateCraterNode : public EventNode {
public:
    CreateCraterNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Terrain; }
    [[nodiscard]] const char* GetTypeName() const override { return "CreateCrater"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// PYTHON INTEGRATION NODES
// ============================================================================

class CallPythonFunctionNode : public EventNode {
public:
    CallPythonFunctionNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Python; }
    [[nodiscard]] const char* GetTypeName() const override { return "CallPythonFunction"; }
    [[nodiscard]] const char* GetDescription() const override { return "Call Python function"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;

    void SetModuleName(const std::string& module) { m_moduleName = module; }
    void SetFunctionName(const std::string& func) { m_functionName = func; }
    void AddParameter(const std::string& name, EventDataType type);

private:
    std::string m_moduleName;
    std::string m_functionName;
};

class ExecutePythonCodeNode : public EventNode {
public:
    ExecutePythonCodeNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Python; }
    [[nodiscard]] const char* GetTypeName() const override { return "ExecutePythonCode"; }
    [[nodiscard]] const char* GetDescription() const override { return "Execute inline Python code"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;

    void SetCode(const std::string& code) { m_code = code; }

private:
    std::string m_code;
};

// ============================================================================
// DEBUG NODES
// ============================================================================

class PrintNode : public EventNode {
public:
    PrintNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Debug; }
    [[nodiscard]] const char* GetTypeName() const override { return "Print"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

class BreakpointNode : public EventNode {
public:
    BreakpointNode();
    [[nodiscard]] EventNodeCategory GetCategory() const override { return EventNodeCategory::Debug; }
    [[nodiscard]] const char* GetTypeName() const override { return "Breakpoint"; }
    [[nodiscard]] std::string GenerateCode(EventCompiler& compiler) const override;
};

// ============================================================================
// EVENT GRAPH
// ============================================================================

class EventGraph {
public:
    EventGraph(const std::string& name = "EventGraph");
    ~EventGraph() = default;

    // Name
    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    // Node management
    void AddNode(EventNode::Ptr node);
    void RemoveNode(EventNodeId id);
    [[nodiscard]] EventNode::Ptr GetNode(EventNodeId id) const;
    [[nodiscard]] const std::vector<EventNode::Ptr>& GetNodes() const { return m_nodes; }

    // Connections
    bool Connect(EventNodeId fromNode, const std::string& fromPin,
                 EventNodeId toNode, const std::string& toPin);
    void Disconnect(EventNodeId toNode, const std::string& toPin);

    // Get entry points (event trigger nodes)
    [[nodiscard]] std::vector<EventNode::Ptr> GetEntryPoints() const;

    // Compilation
    [[nodiscard]] std::string CompileToPython() const;

    // Serialization
    [[nodiscard]] std::string ToJson() const;
    void FromJson(const std::string& json);

    // Variables
    void AddVariable(const std::string& name, EventDataType type, const std::any& defaultValue = {});
    [[nodiscard]] const std::unordered_map<std::string, std::pair<EventDataType, std::any>>& GetVariables() const {
        return m_variables;
    }

private:
    std::string m_name;
    std::vector<EventNode::Ptr> m_nodes;
    std::unordered_map<std::string, std::pair<EventDataType, std::any>> m_variables;
};

// ============================================================================
// EVENT COMPILER
// ============================================================================

class EventCompiler {
public:
    EventCompiler(const EventGraph& graph);

    [[nodiscard]] std::string CompileToPython() const;

    // Code generation helpers
    [[nodiscard]] std::string AllocateVariable(const std::string& prefix = "var");
    void AddImport(const std::string& module);
    void AddCode(const std::string& code, int indent = 0);

    [[nodiscard]] std::string GetNodeOutputVariable(EventNodeId nodeId, const std::string& pinName) const;
    void SetNodeOutputVariable(EventNodeId nodeId, const std::string& pinName, const std::string& varName);

private:
    const EventGraph& m_graph;
    mutable std::unordered_map<uint64_t, std::unordered_map<std::string, std::string>> m_outputVariables;
    mutable std::vector<std::string> m_imports;
    mutable std::vector<std::string> m_code;
    mutable int m_varCounter = 0;
};

// ============================================================================
// EVENT NODE FACTORY
// ============================================================================

class EventNodeFactory {
public:
    using CreatorFunc = std::function<EventNode::Ptr()>;

    static EventNodeFactory& Instance();

    void RegisterNode(const std::string& typeName, EventNodeCategory category,
                      const std::string& displayName, CreatorFunc creator);

    [[nodiscard]] EventNode::Ptr Create(const std::string& typeName) const;
    [[nodiscard]] std::vector<std::string> GetNodeTypes() const;
    [[nodiscard]] std::vector<std::string> GetNodeTypesInCategory(EventNodeCategory category) const;

    void RegisterBuiltinNodes();

private:
    EventNodeFactory() = default;

    struct NodeInfo {
        std::string displayName;
        EventNodeCategory category;
        CreatorFunc creator;
    };

    std::unordered_map<std::string, NodeInfo> m_nodeTypes;
};

} // namespace Nova
