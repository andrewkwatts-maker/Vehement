/**
 * @file EventNodes.cpp
 * @brief Implementation of visual scripting event nodes
 */

#include "EventNodes.hpp"
#include "../core/json_wrapper.hpp"
#include <sstream>
#include <algorithm>

namespace Nova {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const char* EventDataTypeToString(EventDataType type) {
    switch (type) {
        case EventDataType::Void: return "void";
        case EventDataType::Bool: return "bool";
        case EventDataType::Int: return "int";
        case EventDataType::Float: return "float";
        case EventDataType::String: return "str";
        case EventDataType::Vec2: return "Vec2";
        case EventDataType::Vec3: return "Vec3";
        case EventDataType::Vec4: return "Vec4";
        case EventDataType::Entity: return "Entity";
        case EventDataType::EntityList: return "List[Entity]";
        case EventDataType::Animation: return "Animation";
        case EventDataType::Mesh: return "Mesh";
        case EventDataType::Component: return "Component";
        case EventDataType::Any: return "Any";
        default: return "unknown";
    }
}

bool AreEventTypesCompatible(EventDataType from, EventDataType to) {
    if (from == to) return true;
    if (to == EventDataType::Any) return true;
    if (from == EventDataType::Int && to == EventDataType::Float) return true;
    return false;
}

const char* EventNodeCategoryToString(EventNodeCategory category) {
    switch (category) {
        case EventNodeCategory::EventTrigger: return "Event Triggers";
        case EventNodeCategory::EventCustom: return "Custom Events";
        case EventNodeCategory::FlowControl: return "Flow Control";
        case EventNodeCategory::EntityState: return "Entity State";
        case EventNodeCategory::EntityMesh: return "Mesh";
        case EventNodeCategory::EntityAnimation: return "Animation";
        case EventNodeCategory::EntityComponent: return "Components";
        case EventNodeCategory::EntityMovement: return "Movement";
        case EventNodeCategory::Combat: return "Combat";
        case EventNodeCategory::World: return "World";
        case EventNodeCategory::Terrain: return "Terrain";
        case EventNodeCategory::Math: return "Math";
        case EventNodeCategory::Logic: return "Logic";
        case EventNodeCategory::Comparison: return "Comparison";
        case EventNodeCategory::Variables: return "Variables";
        case EventNodeCategory::Arrays: return "Arrays";
        case EventNodeCategory::Python: return "Python";
        case EventNodeCategory::Debug: return "Debug";
        case EventNodeCategory::UI: return "UI";
        default: return "Unknown";
    }
}

// ============================================================================
// BASE EVENT NODE
// ============================================================================

EventNodeId EventNode::s_nextId = 1;

EventNode::EventNode(const std::string& name) : m_name(name), m_displayName(name) {
    m_id = s_nextId++;
}

EventPin* EventNode::GetInput(const std::string& name) {
    auto it = std::find_if(m_inputs.begin(), m_inputs.end(),
                           [&name](const EventPin& p) { return p.name == name; });
    return it != m_inputs.end() ? &(*it) : nullptr;
}

EventPin* EventNode::GetOutput(const std::string& name) {
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                           [&name](const EventPin& p) { return p.name == name; });
    return it != m_outputs.end() ? &(*it) : nullptr;
}

const EventPin* EventNode::GetInput(const std::string& name) const {
    auto it = std::find_if(m_inputs.begin(), m_inputs.end(),
                           [&name](const EventPin& p) { return p.name == name; });
    return it != m_inputs.end() ? &(*it) : nullptr;
}

const EventPin* EventNode::GetOutput(const std::string& name) const {
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                           [&name](const EventPin& p) { return p.name == name; });
    return it != m_outputs.end() ? &(*it) : nullptr;
}

void EventNode::AddFlowInput(const std::string& name, const std::string& displayName) {
    EventPin pin;
    pin.name = name;
    pin.displayName = displayName.empty() ? name : displayName;
    pin.kind = EventPinKind::Flow;
    pin.direction = EventPinDirection::Input;
    pin.dataType = EventDataType::Void;
    pin.id = s_nextId++;
    m_inputs.push_back(pin);
}

void EventNode::AddFlowOutput(const std::string& name, const std::string& displayName) {
    EventPin pin;
    pin.name = name;
    pin.displayName = displayName.empty() ? name : displayName;
    pin.kind = EventPinKind::Flow;
    pin.direction = EventPinDirection::Output;
    pin.dataType = EventDataType::Void;
    pin.id = s_nextId++;
    m_outputs.push_back(pin);
}

void EventNode::AddDataInput(const std::string& name, EventDataType type, const std::string& displayName) {
    EventPin pin;
    pin.name = name;
    pin.displayName = displayName.empty() ? name : displayName;
    pin.kind = EventPinKind::Data;
    pin.direction = EventPinDirection::Input;
    pin.dataType = type;
    pin.id = s_nextId++;
    m_inputs.push_back(pin);
}

void EventNode::AddDataOutput(const std::string& name, EventDataType type, const std::string& displayName) {
    EventPin pin;
    pin.name = name;
    pin.displayName = displayName.empty() ? name : displayName;
    pin.kind = EventPinKind::Data;
    pin.direction = EventPinDirection::Output;
    pin.dataType = type;
    pin.id = s_nextId++;
    m_outputs.push_back(pin);
}

template<>
void EventNode::SetInputDefault(const std::string& name, const bool& value) {
    if (auto* pin = GetInput(name)) pin->defaultValue = value;
}

template<>
void EventNode::SetInputDefault(const std::string& name, const int& value) {
    if (auto* pin = GetInput(name)) pin->defaultValue = value;
}

template<>
void EventNode::SetInputDefault(const std::string& name, const float& value) {
    if (auto* pin = GetInput(name)) pin->defaultValue = value;
}

template<>
void EventNode::SetInputDefault(const std::string& name, const std::string& value) {
    if (auto* pin = GetInput(name)) pin->defaultValue = value;
}

std::string EventNode::GetInputValue(const std::string& name, EventCompiler& compiler) const {
    const auto* pin = GetInput(name);
    if (!pin) return "None";

    if (pin->IsConnected()) {
        return compiler.GetNodeOutputVariable(pin->connectedNodeId, pin->connectedPinName);
    }

    // Return default value
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) return arg ? "True" : "False";
        else if constexpr (std::is_same_v<T, int>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, float>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, std::string>) return "\"" + arg + "\"";
        else if constexpr (std::is_same_v<T, glm::vec2>) return "Vec2(" + std::to_string(arg.x) + ", " + std::to_string(arg.y) + ")";
        else if constexpr (std::is_same_v<T, glm::vec3>) return "Vec3(" + std::to_string(arg.x) + ", " + std::to_string(arg.y) + ", " + std::to_string(arg.z) + ")";
        else if constexpr (std::is_same_v<T, glm::vec4>) return "Vec4(" + std::to_string(arg.x) + ", " + std::to_string(arg.y) + ", " + std::to_string(arg.z) + ", " + std::to_string(arg.w) + ")";
        return "None";
    }, pin->defaultValue);
}

std::string EventNode::ToJson() const {
    std::stringstream ss;
    ss << "{\"type\":\"" << GetTypeName() << "\",\"id\":" << m_id
       << ",\"position\":[" << m_position.x << "," << m_position.y << "]}";
    return ss.str();
}

void EventNode::FromJson(const std::string& jsonStr) {
    auto parsed = Nova::Json::TryParse(jsonStr);
    if (!parsed) {
        return;
    }

    const auto& json = *parsed;

    // Parse node ID
    if (json.contains("id") && json["id"].is_number_unsigned()) {
        m_id = json["id"].get<EventNodeId>();
    }

    // Parse position
    if (json.contains("position") && json["position"].is_array() && json["position"].size() >= 2) {
        m_position.x = json["position"][0].get<float>();
        m_position.y = json["position"][1].get<float>();
    }

    // Parse display name if provided
    if (json.contains("displayName") && json["displayName"].is_string()) {
        m_displayName = json["displayName"].get<std::string>();
    }
}

// ============================================================================
// EVENT TRIGGER NODES
// ============================================================================

OnSpawnNode::OnSpawnNode() : EventNode("OnSpawn") {
    m_displayName = "On Spawn";
    AddFlowOutput("Exec", "");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
}

std::string OnSpawnNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    return "def on_spawn(self):\n";
}

OnDeathNode::OnDeathNode() : EventNode("OnDeath") {
    m_displayName = "On Death";
    AddFlowOutput("Exec", "");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
    AddDataOutput("Killer", EventDataType::Entity, "Killer");
}

std::string OnDeathNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    compiler.SetNodeOutputVariable(m_id, "Killer", "killer");
    return "def on_death(self, killer):\n";
}

OnDamageNode::OnDamageNode() : EventNode("OnDamage") {
    m_displayName = "On Damage";
    AddFlowOutput("Exec", "");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
    AddDataOutput("Attacker", EventDataType::Entity, "Attacker");
    AddDataOutput("Amount", EventDataType::Float, "Damage");
    AddDataOutput("DamageType", EventDataType::String, "Type");
}

std::string OnDamageNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    compiler.SetNodeOutputVariable(m_id, "Attacker", "attacker");
    compiler.SetNodeOutputVariable(m_id, "Amount", "damage_amount");
    compiler.SetNodeOutputVariable(m_id, "DamageType", "damage_type");
    return "def on_damage(self, attacker, damage_amount, damage_type):\n";
}

OnSelectionNode::OnSelectionNode() : EventNode("OnSelection") {
    m_displayName = "On Selection";
    AddFlowOutput("Selected", "On Selected");
    AddFlowOutput("Deselected", "On Deselected");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
}

std::string OnSelectionNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    return "def on_selection_changed(self, selected):\n    if selected:\n";
}

OnCommandNode::OnCommandNode() : EventNode("OnCommand") {
    m_displayName = "On Command";
    AddFlowOutput("Exec", "");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
    AddDataOutput("Command", EventDataType::String, "Command");
    AddDataOutput("Target", EventDataType::Entity, "Target");
    AddDataOutput("Position", EventDataType::Vec3, "Position");
}

std::string OnCommandNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    compiler.SetNodeOutputVariable(m_id, "Command", "command");
    compiler.SetNodeOutputVariable(m_id, "Target", "target");
    compiler.SetNodeOutputVariable(m_id, "Position", "position");
    return "def on_command(self, command, target, position):\n";
}

OnCollisionNode::OnCollisionNode() : EventNode("OnCollision") {
    m_displayName = "On Collision";
    AddFlowOutput("Exec", "");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
    AddDataOutput("Other", EventDataType::Entity, "Other");
    AddDataOutput("Point", EventDataType::Vec3, "Point");
    AddDataOutput("Normal", EventDataType::Vec3, "Normal");
}

std::string OnCollisionNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    compiler.SetNodeOutputVariable(m_id, "Other", "other");
    compiler.SetNodeOutputVariable(m_id, "Point", "hit_point");
    compiler.SetNodeOutputVariable(m_id, "Normal", "hit_normal");
    return "def on_collision(self, other, hit_point, hit_normal):\n";
}

OnTimerNode::OnTimerNode() : EventNode("OnTimer") {
    m_displayName = "On Timer";
    AddFlowOutput("Exec", "");
    AddDataInput("Interval", EventDataType::Float, "Interval (s)");
    AddDataInput("Repeat", EventDataType::Bool, "Repeat");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
    SetInputDefault("Interval", 1.0f);
    SetInputDefault("Repeat", true);
}

std::string OnTimerNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    std::string interval = GetInputValue("Interval", compiler);
    std::string repeat = GetInputValue("Repeat", compiler);
    return "@timer(interval=" + interval + ", repeat=" + repeat + ")\ndef on_timer(self):\n";
}

OnCustomEventNode::OnCustomEventNode() : EventNode("OnCustomEvent") {
    m_displayName = "On Custom Event";
    AddFlowOutput("Exec", "");
    AddDataInput("EventName", EventDataType::String, "Event Name");
    AddDataOutput("Entity", EventDataType::Entity, "Self");
    AddDataOutput("Data", EventDataType::Any, "Event Data");
}

std::string OnCustomEventNode::GenerateCode(EventCompiler& compiler) const {
    compiler.SetNodeOutputVariable(m_id, "Entity", "self");
    compiler.SetNodeOutputVariable(m_id, "Data", "event_data");
    return "@event_handler(\"" + m_eventName + "\")\ndef on_" + m_eventName + "(self, event_data):\n";
}

// ============================================================================
// FLOW CONTROL NODES
// ============================================================================

BranchNode::BranchNode() : EventNode("Branch") {
    m_displayName = "Branch";
    AddFlowInput("Exec", "");
    AddDataInput("Condition", EventDataType::Bool, "Condition");
    AddFlowOutput("True", "True");
    AddFlowOutput("False", "False");
}

std::string BranchNode::GenerateCode(EventCompiler& compiler) const {
    std::string cond = GetInputValue("Condition", compiler);
    return "if " + cond + ":\n";
}

SequenceNode::SequenceNode(int outputs) : EventNode("Sequence"), m_outputCount(outputs) {
    m_displayName = "Sequence";
    AddFlowInput("Exec", "");
    for (int i = 0; i < outputs; i++) {
        AddFlowOutput("Then" + std::to_string(i), "Then " + std::to_string(i));
    }
}

std::string SequenceNode::GenerateCode(EventCompiler& compiler) const {
    return "# Sequence\n";
}

ForEachNode::ForEachNode() : EventNode("ForEach") {
    m_displayName = "For Each";
    AddFlowInput("Exec", "");
    AddDataInput("Array", EventDataType::EntityList, "Array");
    AddFlowOutput("LoopBody", "Loop Body");
    AddFlowOutput("Completed", "Completed");
    AddDataOutput("Element", EventDataType::Entity, "Element");
    AddDataOutput("Index", EventDataType::Int, "Index");
}

std::string ForEachNode::GenerateCode(EventCompiler& compiler) const {
    std::string arr = GetInputValue("Array", compiler);
    std::string elemVar = compiler.AllocateVariable("elem");
    std::string idxVar = compiler.AllocateVariable("idx");
    compiler.SetNodeOutputVariable(m_id, "Element", elemVar);
    compiler.SetNodeOutputVariable(m_id, "Index", idxVar);
    return "for " + idxVar + ", " + elemVar + " in enumerate(" + arr + "):\n";
}

WhileLoopNode::WhileLoopNode() : EventNode("WhileLoop") {
    m_displayName = "While Loop";
    AddFlowInput("Exec", "");
    AddDataInput("Condition", EventDataType::Bool, "Condition");
    AddFlowOutput("LoopBody", "Loop Body");
    AddFlowOutput("Completed", "Completed");
}

std::string WhileLoopNode::GenerateCode(EventCompiler& compiler) const {
    std::string cond = GetInputValue("Condition", compiler);
    return "while " + cond + ":\n";
}

DelayNode::DelayNode() : EventNode("Delay") {
    m_displayName = "Delay";
    AddFlowInput("Exec", "");
    AddDataInput("Duration", EventDataType::Float, "Duration (s)");
    AddFlowOutput("Completed", "Completed");
    SetInputDefault("Duration", 1.0f);
}

std::string DelayNode::GenerateCode(EventCompiler& compiler) const {
    std::string dur = GetInputValue("Duration", compiler);
    compiler.AddImport("asyncio");
    return "await asyncio.sleep(" + dur + ")\n";
}

// ============================================================================
// ENTITY STATE NODES
// ============================================================================

GetStateNode::GetStateNode() : EventNode("GetState") {
    m_displayName = "Get State";
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Key", EventDataType::String, "Key");
    AddDataOutput("Value", EventDataType::Any, "Value");
}

std::string GetStateNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string key = GetInputValue("Key", compiler);
    std::string outVar = compiler.AllocateVariable("state");
    compiler.SetNodeOutputVariable(m_id, "Value", outVar);
    return outVar + " = " + entity + ".get_state(" + key + ")\n";
}

SetStateNode::SetStateNode() : EventNode("SetState") {
    m_displayName = "Set State";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Key", EventDataType::String, "Key");
    AddDataInput("Value", EventDataType::Any, "Value");
    AddFlowOutput("Exec", "");
}

std::string SetStateNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string key = GetInputValue("Key", compiler);
    std::string value = GetInputValue("Value", compiler);
    return entity + ".set_state(" + key + ", " + value + ")\n";
}

GetHealthNode::GetHealthNode() : EventNode("GetHealth") {
    m_displayName = "Get Health";
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataOutput("Health", EventDataType::Float, "Health");
    AddDataOutput("MaxHealth", EventDataType::Float, "Max Health");
    AddDataOutput("Percentage", EventDataType::Float, "Percentage");
}

std::string GetHealthNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string healthVar = compiler.AllocateVariable("health");
    std::string maxVar = compiler.AllocateVariable("max_health");
    std::string pctVar = compiler.AllocateVariable("health_pct");
    compiler.SetNodeOutputVariable(m_id, "Health", healthVar);
    compiler.SetNodeOutputVariable(m_id, "MaxHealth", maxVar);
    compiler.SetNodeOutputVariable(m_id, "Percentage", pctVar);
    return healthVar + " = " + entity + ".health\n" +
           maxVar + " = " + entity + ".max_health\n" +
           pctVar + " = " + healthVar + " / " + maxVar + " if " + maxVar + " > 0 else 0\n";
}

SetHealthNode::SetHealthNode() : EventNode("SetHealth") {
    m_displayName = "Set Health";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Health", EventDataType::Float, "Health");
    AddFlowOutput("Exec", "");
}

std::string SetHealthNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string health = GetInputValue("Health", compiler);
    return entity + ".health = " + health + "\n";
}

GetPositionNode::GetPositionNode() : EventNode("GetPosition") {
    m_displayName = "Get Position";
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataOutput("Position", EventDataType::Vec3, "Position");
}

std::string GetPositionNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string posVar = compiler.AllocateVariable("pos");
    compiler.SetNodeOutputVariable(m_id, "Position", posVar);
    return posVar + " = " + entity + ".position\n";
}

SetPositionNode::SetPositionNode() : EventNode("SetPosition") {
    m_displayName = "Set Position";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddFlowOutput("Exec", "");
}

std::string SetPositionNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string pos = GetInputValue("Position", compiler);
    return entity + ".position = " + pos + "\n";
}

// ============================================================================
// MESH NODES
// ============================================================================

SetMeshNode::SetMeshNode() : EventNode("SetMesh") {
    m_displayName = "Set Mesh";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("MeshPath", EventDataType::String, "Mesh Path");
    AddFlowOutput("Exec", "");
}

std::string SetMeshNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string path = GetInputValue("MeshPath", compiler);
    return entity + ".set_mesh(" + path + ")\n";
}

SetMaterialNode::SetMaterialNode() : EventNode("SetMaterial") {
    m_displayName = "Set Material";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("MaterialPath", EventDataType::String, "Material");
    AddDataInput("SlotIndex", EventDataType::Int, "Slot");
    AddFlowOutput("Exec", "");
    SetInputDefault("SlotIndex", 0);
}

std::string SetMaterialNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string material = GetInputValue("MaterialPath", compiler);
    std::string slot = GetInputValue("SlotIndex", compiler);
    return entity + ".set_material(" + material + ", " + slot + ")\n";
}

SetScaleNode::SetScaleNode() : EventNode("SetScale") {
    m_displayName = "Set Scale";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Scale", EventDataType::Vec3, "Scale");
    AddFlowOutput("Exec", "");
}

std::string SetScaleNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string scale = GetInputValue("Scale", compiler);
    return entity + ".scale = " + scale + "\n";
}

SetVisibleNode::SetVisibleNode() : EventNode("SetVisible") {
    m_displayName = "Set Visible";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Visible", EventDataType::Bool, "Visible");
    AddFlowOutput("Exec", "");
    SetInputDefault("Visible", true);
}

std::string SetVisibleNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string visible = GetInputValue("Visible", compiler);
    return entity + ".visible = " + visible + "\n";
}

// ============================================================================
// ANIMATION NODES
// ============================================================================

PlayAnimationNode::PlayAnimationNode() : EventNode("PlayAnimation") {
    m_displayName = "Play Animation";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Animation", EventDataType::String, "Animation");
    AddDataInput("Loop", EventDataType::Bool, "Loop");
    AddDataInput("BlendTime", EventDataType::Float, "Blend Time");
    AddFlowOutput("Exec", "");
    AddFlowOutput("OnComplete", "On Complete");
    SetInputDefault("Loop", false);
    SetInputDefault("BlendTime", 0.2f);
}

std::string PlayAnimationNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string anim = GetInputValue("Animation", compiler);
    std::string loop = GetInputValue("Loop", compiler);
    std::string blend = GetInputValue("BlendTime", compiler);
    return entity + ".play_animation(" + anim + ", loop=" + loop + ", blend_time=" + blend + ")\n";
}

StopAnimationNode::StopAnimationNode() : EventNode("StopAnimation") {
    m_displayName = "Stop Animation";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddFlowOutput("Exec", "");
}

std::string StopAnimationNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    return entity + ".stop_animation()\n";
}

BlendAnimationNode::BlendAnimationNode() : EventNode("BlendAnimation") {
    m_displayName = "Blend Animation";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("AnimationA", EventDataType::String, "Animation A");
    AddDataInput("AnimationB", EventDataType::String, "Animation B");
    AddDataInput("BlendFactor", EventDataType::Float, "Blend Factor");
    AddFlowOutput("Exec", "");
    SetInputDefault("BlendFactor", 0.5f);
}

std::string BlendAnimationNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string animA = GetInputValue("AnimationA", compiler);
    std::string animB = GetInputValue("AnimationB", compiler);
    std::string blend = GetInputValue("BlendFactor", compiler);
    return entity + ".blend_animations(" + animA + ", " + animB + ", " + blend + ")\n";
}

SetAnimationSpeedNode::SetAnimationSpeedNode() : EventNode("SetAnimationSpeed") {
    m_displayName = "Set Animation Speed";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Speed", EventDataType::Float, "Speed");
    AddFlowOutput("Exec", "");
    SetInputDefault("Speed", 1.0f);
}

std::string SetAnimationSpeedNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string speed = GetInputValue("Speed", compiler);
    return entity + ".animation_speed = " + speed + "\n";
}

// ============================================================================
// COMPONENT NODES
// ============================================================================

AddComponentNode::AddComponentNode() : EventNode("AddComponent") {
    m_displayName = "Add Component";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("ComponentType", EventDataType::String, "Component Type");
    AddFlowOutput("Exec", "");
    AddDataOutput("Component", EventDataType::Component, "Component");
}

std::string AddComponentNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string compType = GetInputValue("ComponentType", compiler);
    std::string compVar = compiler.AllocateVariable("comp");
    compiler.SetNodeOutputVariable(m_id, "Component", compVar);
    return compVar + " = " + entity + ".add_component(" + compType + ")\n";
}

RemoveComponentNode::RemoveComponentNode() : EventNode("RemoveComponent") {
    m_displayName = "Remove Component";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("ComponentType", EventDataType::String, "Component Type");
    AddFlowOutput("Exec", "");
}

std::string RemoveComponentNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string compType = GetInputValue("ComponentType", compiler);
    return entity + ".remove_component(" + compType + ")\n";
}

HasComponentNode::HasComponentNode() : EventNode("HasComponent") {
    m_displayName = "Has Component";
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("ComponentType", EventDataType::String, "Component Type");
    AddDataOutput("HasComponent", EventDataType::Bool, "Has Component");
}

std::string HasComponentNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string compType = GetInputValue("ComponentType", compiler);
    std::string resultVar = compiler.AllocateVariable("has_comp");
    compiler.SetNodeOutputVariable(m_id, "HasComponent", resultVar);
    return resultVar + " = " + entity + ".has_component(" + compType + ")\n";
}

GetComponentNode::GetComponentNode() : EventNode("GetComponent") {
    m_displayName = "Get Component";
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("ComponentType", EventDataType::String, "Component Type");
    AddDataOutput("Component", EventDataType::Component, "Component");
}

std::string GetComponentNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string compType = GetInputValue("ComponentType", compiler);
    std::string compVar = compiler.AllocateVariable("comp");
    compiler.SetNodeOutputVariable(m_id, "Component", compVar);
    return compVar + " = " + entity + ".get_component(" + compType + ")\n";
}

// ============================================================================
// MOVEMENT NODES
// ============================================================================

MoveToNode::MoveToNode() : EventNode("MoveTo") {
    m_displayName = "Move To";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Target", EventDataType::Vec3, "Target");
    AddFlowOutput("Exec", "");
    AddFlowOutput("OnArrival", "On Arrival");
}

std::string MoveToNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string target = GetInputValue("Target", compiler);
    return entity + ".move_to(" + target + ")\n";
}

StopMovementNode::StopMovementNode() : EventNode("StopMovement") {
    m_displayName = "Stop Movement";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddFlowOutput("Exec", "");
}

std::string StopMovementNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    return entity + ".stop_movement()\n";
}

FollowEntityNode::FollowEntityNode() : EventNode("FollowEntity") {
    m_displayName = "Follow Entity";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Target", EventDataType::Entity, "Target");
    AddDataInput("MinDistance", EventDataType::Float, "Min Distance");
    AddFlowOutput("Exec", "");
    SetInputDefault("MinDistance", 2.0f);
}

std::string FollowEntityNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string target = GetInputValue("Target", compiler);
    std::string dist = GetInputValue("MinDistance", compiler);
    return entity + ".follow(" + target + ", min_distance=" + dist + ")\n";
}

SetSpeedNode::SetSpeedNode() : EventNode("SetSpeed") {
    m_displayName = "Set Speed";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("Speed", EventDataType::Float, "Speed");
    AddFlowOutput("Exec", "");
}

std::string SetSpeedNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string speed = GetInputValue("Speed", compiler);
    return entity + ".movement_speed = " + speed + "\n";
}

// ============================================================================
// COMBAT NODES
// ============================================================================

DealDamageNode::DealDamageNode() : EventNode("DealDamage") {
    m_displayName = "Deal Damage";
    AddFlowInput("Exec", "");
    AddDataInput("Target", EventDataType::Entity, "Target");
    AddDataInput("Amount", EventDataType::Float, "Amount");
    AddDataInput("DamageType", EventDataType::String, "Damage Type");
    AddDataInput("Source", EventDataType::Entity, "Source");
    AddFlowOutput("Exec", "");
    SetInputDefault("DamageType", std::string("physical"));
}

std::string DealDamageNode::GenerateCode(EventCompiler& compiler) const {
    std::string target = GetInputValue("Target", compiler);
    std::string amount = GetInputValue("Amount", compiler);
    std::string dtype = GetInputValue("DamageType", compiler);
    std::string source = GetInputValue("Source", compiler);
    return target + ".take_damage(" + amount + ", " + dtype + ", " + source + ")\n";
}

HealNode::HealNode() : EventNode("Heal") {
    m_displayName = "Heal";
    AddFlowInput("Exec", "");
    AddDataInput("Target", EventDataType::Entity, "Target");
    AddDataInput("Amount", EventDataType::Float, "Amount");
    AddFlowOutput("Exec", "");
}

std::string HealNode::GenerateCode(EventCompiler& compiler) const {
    std::string target = GetInputValue("Target", compiler);
    std::string amount = GetInputValue("Amount", compiler);
    return target + ".heal(" + amount + ")\n";
}

UseAbilityNode::UseAbilityNode() : EventNode("UseAbility") {
    m_displayName = "Use Ability";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddDataInput("AbilityId", EventDataType::String, "Ability ID");
    AddDataInput("Target", EventDataType::Entity, "Target");
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddFlowOutput("Exec", "");
    AddFlowOutput("OnSuccess", "On Success");
    AddFlowOutput("OnFail", "On Fail");
}

std::string UseAbilityNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    std::string ability = GetInputValue("AbilityId", compiler);
    std::string target = GetInputValue("Target", compiler);
    std::string pos = GetInputValue("Position", compiler);
    return entity + ".use_ability(" + ability + ", target=" + target + ", position=" + pos + ")\n";
}

ApplyEffectNode::ApplyEffectNode() : EventNode("ApplyEffect") {
    m_displayName = "Apply Effect";
    AddFlowInput("Exec", "");
    AddDataInput("Target", EventDataType::Entity, "Target");
    AddDataInput("EffectId", EventDataType::String, "Effect ID");
    AddDataInput("Duration", EventDataType::Float, "Duration");
    AddDataInput("Stacks", EventDataType::Int, "Stacks");
    AddFlowOutput("Exec", "");
    SetInputDefault("Duration", 5.0f);
    SetInputDefault("Stacks", 1);
}

std::string ApplyEffectNode::GenerateCode(EventCompiler& compiler) const {
    std::string target = GetInputValue("Target", compiler);
    std::string effect = GetInputValue("EffectId", compiler);
    std::string duration = GetInputValue("Duration", compiler);
    std::string stacks = GetInputValue("Stacks", compiler);
    return target + ".apply_effect(" + effect + ", duration=" + duration + ", stacks=" + stacks + ")\n";
}

// ============================================================================
// WORLD NODES
// ============================================================================

SpawnEntityNode::SpawnEntityNode() : EventNode("SpawnEntity") {
    m_displayName = "Spawn Entity";
    AddFlowInput("Exec", "");
    AddDataInput("EntityType", EventDataType::String, "Entity Type");
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddDataInput("Owner", EventDataType::Entity, "Owner");
    AddFlowOutput("Exec", "");
    AddDataOutput("SpawnedEntity", EventDataType::Entity, "Spawned Entity");
}

std::string SpawnEntityNode::GenerateCode(EventCompiler& compiler) const {
    std::string entityType = GetInputValue("EntityType", compiler);
    std::string pos = GetInputValue("Position", compiler);
    std::string owner = GetInputValue("Owner", compiler);
    std::string spawnVar = compiler.AllocateVariable("spawned");
    compiler.SetNodeOutputVariable(m_id, "SpawnedEntity", spawnVar);
    return spawnVar + " = world.spawn(" + entityType + ", position=" + pos + ", owner=" + owner + ")\n";
}

DestroyEntityNode::DestroyEntityNode() : EventNode("DestroyEntity") {
    m_displayName = "Destroy Entity";
    AddFlowInput("Exec", "");
    AddDataInput("Entity", EventDataType::Entity, "Entity");
    AddFlowOutput("Exec", "");
}

std::string DestroyEntityNode::GenerateCode(EventCompiler& compiler) const {
    std::string entity = GetInputValue("Entity", compiler);
    return "world.destroy(" + entity + ")\n";
}

FindEntitiesNode::FindEntitiesNode() : EventNode("FindEntities") {
    m_displayName = "Find Entities";
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddDataInput("Radius", EventDataType::Float, "Radius");
    AddDataInput("EntityType", EventDataType::String, "Entity Type");
    AddDataInput("Team", EventDataType::Int, "Team (-1 = Any)");
    AddDataOutput("Entities", EventDataType::EntityList, "Entities");
    SetInputDefault("Radius", 10.0f);
    SetInputDefault("Team", -1);
}

std::string FindEntitiesNode::GenerateCode(EventCompiler& compiler) const {
    std::string pos = GetInputValue("Position", compiler);
    std::string radius = GetInputValue("Radius", compiler);
    std::string entityType = GetInputValue("EntityType", compiler);
    std::string team = GetInputValue("Team", compiler);
    std::string resultVar = compiler.AllocateVariable("found");
    compiler.SetNodeOutputVariable(m_id, "Entities", resultVar);
    return resultVar + " = world.find_entities(" + pos + ", " + radius + ", entity_type=" + entityType + ", team=" + team + ")\n";
}

GetClosestEntityNode::GetClosestEntityNode() : EventNode("GetClosestEntity") {
    m_displayName = "Get Closest Entity";
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddDataInput("EntityType", EventDataType::String, "Entity Type");
    AddDataInput("MaxDistance", EventDataType::Float, "Max Distance");
    AddDataOutput("Entity", EventDataType::Entity, "Closest Entity");
    AddDataOutput("Distance", EventDataType::Float, "Distance");
    SetInputDefault("MaxDistance", 100.0f);
}

std::string GetClosestEntityNode::GenerateCode(EventCompiler& compiler) const {
    std::string pos = GetInputValue("Position", compiler);
    std::string entityType = GetInputValue("EntityType", compiler);
    std::string maxDist = GetInputValue("MaxDistance", compiler);
    std::string entityVar = compiler.AllocateVariable("closest");
    std::string distVar = compiler.AllocateVariable("distance");
    compiler.SetNodeOutputVariable(m_id, "Entity", entityVar);
    compiler.SetNodeOutputVariable(m_id, "Distance", distVar);
    return entityVar + ", " + distVar + " = world.get_closest(" + pos + ", entity_type=" + entityType + ", max_distance=" + maxDist + ")\n";
}

BroadcastEventNode::BroadcastEventNode() : EventNode("BroadcastEvent") {
    m_displayName = "Broadcast Event";
    AddFlowInput("Exec", "");
    AddDataInput("EventName", EventDataType::String, "Event Name");
    AddDataInput("Data", EventDataType::Any, "Data");
    AddFlowOutput("Exec", "");
}

std::string BroadcastEventNode::GenerateCode(EventCompiler& compiler) const {
    std::string eventName = GetInputValue("EventName", compiler);
    std::string data = GetInputValue("Data", compiler);
    return "event_bus.broadcast(" + eventName + ", " + data + ")\n";
}

// ============================================================================
// TERRAIN NODES
// ============================================================================

GetTerrainHeightNode::GetTerrainHeightNode() : EventNode("GetTerrainHeight") {
    m_displayName = "Get Terrain Height";
    AddDataInput("Position", EventDataType::Vec2, "Position (XZ)");
    AddDataOutput("Height", EventDataType::Float, "Height");
    AddDataOutput("Normal", EventDataType::Vec3, "Normal");
}

std::string GetTerrainHeightNode::GenerateCode(EventCompiler& compiler) const {
    std::string pos = GetInputValue("Position", compiler);
    std::string heightVar = compiler.AllocateVariable("height");
    std::string normalVar = compiler.AllocateVariable("normal");
    compiler.SetNodeOutputVariable(m_id, "Height", heightVar);
    compiler.SetNodeOutputVariable(m_id, "Normal", normalVar);
    return heightVar + ", " + normalVar + " = terrain.get_height_and_normal(" + pos + ")\n";
}

ModifyTerrainNode::ModifyTerrainNode() : EventNode("ModifyTerrain") {
    m_displayName = "Modify Terrain";
    AddFlowInput("Exec", "");
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddDataInput("Radius", EventDataType::Float, "Radius");
    AddDataInput("Operation", EventDataType::String, "Operation");
    AddDataInput("Strength", EventDataType::Float, "Strength");
    AddFlowOutput("Exec", "");
    SetInputDefault("Radius", 5.0f);
    SetInputDefault("Operation", std::string("add"));
    SetInputDefault("Strength", 1.0f);
}

std::string ModifyTerrainNode::GenerateCode(EventCompiler& compiler) const {
    std::string pos = GetInputValue("Position", compiler);
    std::string radius = GetInputValue("Radius", compiler);
    std::string op = GetInputValue("Operation", compiler);
    std::string strength = GetInputValue("Strength", compiler);
    return "terrain.modify(" + pos + ", " + radius + ", operation=" + op + ", strength=" + strength + ")\n";
}

CreateCraterNode::CreateCraterNode() : EventNode("CreateCrater") {
    m_displayName = "Create Crater";
    AddFlowInput("Exec", "");
    AddDataInput("Position", EventDataType::Vec3, "Position");
    AddDataInput("Radius", EventDataType::Float, "Radius");
    AddDataInput("Depth", EventDataType::Float, "Depth");
    AddFlowOutput("Exec", "");
    SetInputDefault("Radius", 3.0f);
    SetInputDefault("Depth", 1.0f);
}

std::string CreateCraterNode::GenerateCode(EventCompiler& compiler) const {
    std::string pos = GetInputValue("Position", compiler);
    std::string radius = GetInputValue("Radius", compiler);
    std::string depth = GetInputValue("Depth", compiler);
    return "terrain.create_crater(" + pos + ", " + radius + ", " + depth + ")\n";
}

// ============================================================================
// PYTHON NODES
// ============================================================================

CallPythonFunctionNode::CallPythonFunctionNode() : EventNode("CallPythonFunction") {
    m_displayName = "Call Python Function";
    AddFlowInput("Exec", "");
    AddDataInput("Module", EventDataType::String, "Module");
    AddDataInput("Function", EventDataType::String, "Function");
    AddFlowOutput("Exec", "");
    AddDataOutput("Result", EventDataType::Any, "Result");
}

void CallPythonFunctionNode::AddParameter(const std::string& name, EventDataType type) {
    AddDataInput(name, type, name);
}

std::string CallPythonFunctionNode::GenerateCode(EventCompiler& compiler) const {
    std::string module = m_moduleName.empty() ? GetInputValue("Module", compiler) : "\"" + m_moduleName + "\"";
    std::string func = m_functionName.empty() ? GetInputValue("Function", compiler) : "\"" + m_functionName + "\"";
    std::string resultVar = compiler.AllocateVariable("result");
    compiler.SetNodeOutputVariable(m_id, "Result", resultVar);

    std::string args;
    for (const auto& input : m_inputs) {
        if (input.name != "Module" && input.name != "Function" && !input.IsFlow()) {
            if (!args.empty()) args += ", ";
            args += GetInputValue(input.name, compiler);
        }
    }

    compiler.AddImport("importlib");
    return "mod = importlib.import_module(" + module + ")\n" +
           resultVar + " = getattr(mod, " + func + ")(" + args + ")\n";
}

ExecutePythonCodeNode::ExecutePythonCodeNode() : EventNode("ExecutePythonCode") {
    m_displayName = "Execute Python Code";
    AddFlowInput("Exec", "");
    AddDataInput("Code", EventDataType::String, "Code");
    AddFlowOutput("Exec", "");
    AddDataOutput("Result", EventDataType::Any, "Result");
}

std::string ExecutePythonCodeNode::GenerateCode(EventCompiler& compiler) const {
    std::string resultVar = compiler.AllocateVariable("result");
    compiler.SetNodeOutputVariable(m_id, "Result", resultVar);
    return m_code.empty() ? "pass\n" : m_code + "\n";
}

// ============================================================================
// DEBUG NODES
// ============================================================================

PrintNode::PrintNode() : EventNode("Print") {
    m_displayName = "Print";
    AddFlowInput("Exec", "");
    AddDataInput("Message", EventDataType::String, "Message");
    AddFlowOutput("Exec", "");
}

std::string PrintNode::GenerateCode(EventCompiler& compiler) const {
    std::string msg = GetInputValue("Message", compiler);
    return "print(" + msg + ")\n";
}

BreakpointNode::BreakpointNode() : EventNode("Breakpoint") {
    m_displayName = "Breakpoint";
    AddFlowInput("Exec", "");
    AddDataInput("Condition", EventDataType::Bool, "Condition");
    AddFlowOutput("Exec", "");
    SetInputDefault("Condition", true);
}

std::string BreakpointNode::GenerateCode(EventCompiler& compiler) const {
    std::string cond = GetInputValue("Condition", compiler);
    return "if " + cond + ": breakpoint()\n";
}

// ============================================================================
// EVENT GRAPH
// ============================================================================

EventGraph::EventGraph(const std::string& name) : m_name(name) {}

void EventGraph::AddNode(EventNode::Ptr node) {
    m_nodes.push_back(node);
}

void EventGraph::RemoveNode(EventNodeId id) {
    m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                                  [id](const EventNode::Ptr& n) { return n->GetId() == id; }),
                  m_nodes.end());
}

EventNode::Ptr EventGraph::GetNode(EventNodeId id) const {
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                           [id](const EventNode::Ptr& n) { return n->GetId() == id; });
    return it != m_nodes.end() ? *it : nullptr;
}

bool EventGraph::Connect(EventNodeId fromNode, const std::string& fromPin,
                          EventNodeId toNode, const std::string& toPin) {
    auto from = GetNode(fromNode);
    auto to = GetNode(toNode);
    if (!from || !to) return false;

    auto* output = from->GetOutput(fromPin);
    auto* input = to->GetInput(toPin);
    if (!output || !input) return false;

    if (output->kind != input->kind) return false;
    if (input->kind == EventPinKind::Data && !AreEventTypesCompatible(output->dataType, input->dataType)) {
        return false;
    }

    input->connectedNodeId = fromNode;
    input->connectedPinName = fromPin;
    return true;
}

void EventGraph::Disconnect(EventNodeId toNode, const std::string& toPin) {
    auto node = GetNode(toNode);
    if (!node) return;

    auto* input = node->GetInput(toPin);
    if (input) {
        input->connectedNodeId = 0;
        input->connectedPinName.clear();
    }
}

std::vector<EventNode::Ptr> EventGraph::GetEntryPoints() const {
    std::vector<EventNode::Ptr> entries;
    for (const auto& node : m_nodes) {
        if (node->GetCategory() == EventNodeCategory::EventTrigger ||
            node->GetCategory() == EventNodeCategory::EventCustom) {
            entries.push_back(node);
        }
    }
    return entries;
}

std::string EventGraph::CompileToPython() const {
    EventCompiler compiler(*this);
    return compiler.CompileToPython();
}

void EventGraph::AddVariable(const std::string& name, EventDataType type, const std::any& defaultValue) {
    m_variables[name] = {type, defaultValue};
}

std::string EventGraph::ToJson() const {
    std::stringstream ss;
    ss << "{\"name\":\"" << m_name << "\",\"nodes\":[";
    for (size_t i = 0; i < m_nodes.size(); i++) {
        if (i > 0) ss << ",";
        ss << m_nodes[i]->ToJson();
    }
    ss << "]}";
    return ss.str();
}

void EventGraph::FromJson(const std::string& jsonStr) {
    auto parsed = Nova::Json::TryParse(jsonStr);
    if (!parsed) {
        return;
    }

    const auto& json = *parsed;

    // Parse graph name
    if (json.contains("name") && json["name"].is_string()) {
        m_name = json["name"].get<std::string>();
    }

    // Parse nodes
    if (json.contains("nodes") && json["nodes"].is_array()) {
        auto& factory = EventNodeFactory::Instance();

        for (const auto& nodeJson : json["nodes"]) {
            if (!nodeJson.contains("type") || !nodeJson["type"].is_string()) {
                continue;
            }

            std::string typeName = nodeJson["type"].get<std::string>();
            auto node = factory.Create(typeName);
            if (node) {
                // Restore node state from JSON
                node->FromJson(nodeJson.dump());
                m_nodes.push_back(node);
            }
        }
    }

    // Parse connections (if present)
    if (json.contains("connections") && json["connections"].is_array()) {
        for (const auto& connJson : json["connections"]) {
            if (connJson.contains("fromNode") && connJson.contains("fromPin") &&
                connJson.contains("toNode") && connJson.contains("toPin")) {
                EventNodeId fromNode = connJson["fromNode"].get<EventNodeId>();
                std::string fromPin = connJson["fromPin"].get<std::string>();
                EventNodeId toNode = connJson["toNode"].get<EventNodeId>();
                std::string toPin = connJson["toPin"].get<std::string>();
                Connect(fromNode, fromPin, toNode, toPin);
            }
        }
    }

    // Parse variables (if present)
    if (json.contains("variables") && json["variables"].is_object()) {
        for (auto it = json["variables"].begin(); it != json["variables"].end(); ++it) {
            const std::string& varName = it.key();
            const auto& varInfo = it.value();

            if (varInfo.contains("type") && varInfo["type"].is_string()) {
                std::string typeStr = varInfo["type"].get<std::string>();
                EventDataType type = EventDataType::Any;

                // Map type string to EventDataType
                if (typeStr == "bool") type = EventDataType::Bool;
                else if (typeStr == "int") type = EventDataType::Int;
                else if (typeStr == "float") type = EventDataType::Float;
                else if (typeStr == "str") type = EventDataType::String;
                else if (typeStr == "Vec2") type = EventDataType::Vec2;
                else if (typeStr == "Vec3") type = EventDataType::Vec3;
                else if (typeStr == "Vec4") type = EventDataType::Vec4;
                else if (typeStr == "Entity") type = EventDataType::Entity;
                else if (typeStr == "List[Entity]") type = EventDataType::EntityList;

                AddVariable(varName, type);
            }
        }
    }
}

// ============================================================================
// EVENT COMPILER
// ============================================================================

EventCompiler::EventCompiler(const EventGraph& graph) : m_graph(graph) {}

std::string EventCompiler::AllocateVariable(const std::string& prefix) {
    return prefix + "_" + std::to_string(m_varCounter++);
}

void EventCompiler::AddImport(const std::string& module) {
    if (std::find(m_imports.begin(), m_imports.end(), module) == m_imports.end()) {
        m_imports.push_back(module);
    }
}

void EventCompiler::AddCode(const std::string& code, int indent) {
    std::string indentStr(indent * 4, ' ');
    m_code.push_back(indentStr + code);
}

std::string EventCompiler::GetNodeOutputVariable(EventNodeId nodeId, const std::string& pinName) const {
    auto nodeIt = m_outputVariables.find(nodeId);
    if (nodeIt != m_outputVariables.end()) {
        auto pinIt = nodeIt->second.find(pinName);
        if (pinIt != nodeIt->second.end()) {
            return pinIt->second;
        }
    }
    return "None";
}

void EventCompiler::SetNodeOutputVariable(EventNodeId nodeId, const std::string& pinName, const std::string& varName) {
    m_outputVariables[nodeId][pinName] = varName;
}

std::string EventCompiler::CompileToPython() const {
    std::stringstream ss;

    // Header
    ss << "# Auto-generated by Nova Event Compiler\n";
    ss << "# Graph: " << m_graph.GetName() << "\n\n";

    // Imports
    ss << "from nova import *\n";
    for (const auto& imp : m_imports) {
        ss << "import " << imp << "\n";
    }
    ss << "\n";

    // Variables
    const auto& vars = m_graph.GetVariables();
    if (!vars.empty()) {
        ss << "# Graph Variables\n";
        for (const auto& [name, info] : vars) {
            ss << name << " = None  # " << EventDataTypeToString(info.first) << "\n";
        }
        ss << "\n";
    }

    // Generate code for each entry point
    auto entries = m_graph.GetEntryPoints();
    for (const auto& entry : entries) {
        ss << entry->GenerateCode(const_cast<EventCompiler&>(*this));
        ss << "    pass  # TODO: Generate body from connected nodes\n\n";
    }

    return ss.str();
}

// ============================================================================
// EVENT NODE FACTORY
// ============================================================================

EventNodeFactory& EventNodeFactory::Instance() {
    static EventNodeFactory instance;
    return instance;
}

void EventNodeFactory::RegisterNode(const std::string& typeName, EventNodeCategory category,
                                     const std::string& displayName, CreatorFunc creator) {
    m_nodeTypes[typeName] = {displayName, category, creator};
}

EventNode::Ptr EventNodeFactory::Create(const std::string& typeName) const {
    auto it = m_nodeTypes.find(typeName);
    if (it != m_nodeTypes.end()) {
        return it->second.creator();
    }
    return nullptr;
}

std::vector<std::string> EventNodeFactory::GetNodeTypes() const {
    std::vector<std::string> types;
    for (const auto& [name, info] : m_nodeTypes) {
        types.push_back(name);
    }
    return types;
}

std::vector<std::string> EventNodeFactory::GetNodeTypesInCategory(EventNodeCategory category) const {
    std::vector<std::string> types;
    for (const auto& [name, info] : m_nodeTypes) {
        if (info.category == category) {
            types.push_back(name);
        }
    }
    return types;
}

void EventNodeFactory::RegisterBuiltinNodes() {
    // Event Triggers
    RegisterNode("OnSpawn", EventNodeCategory::EventTrigger, "On Spawn", []() { return std::make_shared<OnSpawnNode>(); });
    RegisterNode("OnDeath", EventNodeCategory::EventTrigger, "On Death", []() { return std::make_shared<OnDeathNode>(); });
    RegisterNode("OnDamage", EventNodeCategory::EventTrigger, "On Damage", []() { return std::make_shared<OnDamageNode>(); });
    RegisterNode("OnSelection", EventNodeCategory::EventTrigger, "On Selection", []() { return std::make_shared<OnSelectionNode>(); });
    RegisterNode("OnCommand", EventNodeCategory::EventTrigger, "On Command", []() { return std::make_shared<OnCommandNode>(); });
    RegisterNode("OnCollision", EventNodeCategory::EventTrigger, "On Collision", []() { return std::make_shared<OnCollisionNode>(); });
    RegisterNode("OnTimer", EventNodeCategory::EventTrigger, "On Timer", []() { return std::make_shared<OnTimerNode>(); });
    RegisterNode("OnCustomEvent", EventNodeCategory::EventCustom, "On Custom Event", []() { return std::make_shared<OnCustomEventNode>(); });

    // Flow Control
    RegisterNode("Branch", EventNodeCategory::FlowControl, "Branch", []() { return std::make_shared<BranchNode>(); });
    RegisterNode("Sequence", EventNodeCategory::FlowControl, "Sequence", []() { return std::make_shared<SequenceNode>(); });
    RegisterNode("ForEach", EventNodeCategory::FlowControl, "For Each", []() { return std::make_shared<ForEachNode>(); });
    RegisterNode("WhileLoop", EventNodeCategory::FlowControl, "While Loop", []() { return std::make_shared<WhileLoopNode>(); });
    RegisterNode("Delay", EventNodeCategory::FlowControl, "Delay", []() { return std::make_shared<DelayNode>(); });

    // Entity State
    RegisterNode("GetState", EventNodeCategory::EntityState, "Get State", []() { return std::make_shared<GetStateNode>(); });
    RegisterNode("SetState", EventNodeCategory::EntityState, "Set State", []() { return std::make_shared<SetStateNode>(); });
    RegisterNode("GetHealth", EventNodeCategory::EntityState, "Get Health", []() { return std::make_shared<GetHealthNode>(); });
    RegisterNode("SetHealth", EventNodeCategory::EntityState, "Set Health", []() { return std::make_shared<SetHealthNode>(); });
    RegisterNode("GetPosition", EventNodeCategory::EntityState, "Get Position", []() { return std::make_shared<GetPositionNode>(); });
    RegisterNode("SetPosition", EventNodeCategory::EntityState, "Set Position", []() { return std::make_shared<SetPositionNode>(); });

    // Mesh
    RegisterNode("SetMesh", EventNodeCategory::EntityMesh, "Set Mesh", []() { return std::make_shared<SetMeshNode>(); });
    RegisterNode("SetMaterial", EventNodeCategory::EntityMesh, "Set Material", []() { return std::make_shared<SetMaterialNode>(); });
    RegisterNode("SetScale", EventNodeCategory::EntityMesh, "Set Scale", []() { return std::make_shared<SetScaleNode>(); });
    RegisterNode("SetVisible", EventNodeCategory::EntityMesh, "Set Visible", []() { return std::make_shared<SetVisibleNode>(); });

    // Animation
    RegisterNode("PlayAnimation", EventNodeCategory::EntityAnimation, "Play Animation", []() { return std::make_shared<PlayAnimationNode>(); });
    RegisterNode("StopAnimation", EventNodeCategory::EntityAnimation, "Stop Animation", []() { return std::make_shared<StopAnimationNode>(); });
    RegisterNode("BlendAnimation", EventNodeCategory::EntityAnimation, "Blend Animation", []() { return std::make_shared<BlendAnimationNode>(); });
    RegisterNode("SetAnimationSpeed", EventNodeCategory::EntityAnimation, "Set Animation Speed", []() { return std::make_shared<SetAnimationSpeedNode>(); });

    // Components
    RegisterNode("AddComponent", EventNodeCategory::EntityComponent, "Add Component", []() { return std::make_shared<AddComponentNode>(); });
    RegisterNode("RemoveComponent", EventNodeCategory::EntityComponent, "Remove Component", []() { return std::make_shared<RemoveComponentNode>(); });
    RegisterNode("HasComponent", EventNodeCategory::EntityComponent, "Has Component", []() { return std::make_shared<HasComponentNode>(); });
    RegisterNode("GetComponent", EventNodeCategory::EntityComponent, "Get Component", []() { return std::make_shared<GetComponentNode>(); });

    // Movement
    RegisterNode("MoveTo", EventNodeCategory::EntityMovement, "Move To", []() { return std::make_shared<MoveToNode>(); });
    RegisterNode("StopMovement", EventNodeCategory::EntityMovement, "Stop Movement", []() { return std::make_shared<StopMovementNode>(); });
    RegisterNode("FollowEntity", EventNodeCategory::EntityMovement, "Follow Entity", []() { return std::make_shared<FollowEntityNode>(); });
    RegisterNode("SetSpeed", EventNodeCategory::EntityMovement, "Set Speed", []() { return std::make_shared<SetSpeedNode>(); });

    // Combat
    RegisterNode("DealDamage", EventNodeCategory::Combat, "Deal Damage", []() { return std::make_shared<DealDamageNode>(); });
    RegisterNode("Heal", EventNodeCategory::Combat, "Heal", []() { return std::make_shared<HealNode>(); });
    RegisterNode("UseAbility", EventNodeCategory::Combat, "Use Ability", []() { return std::make_shared<UseAbilityNode>(); });
    RegisterNode("ApplyEffect", EventNodeCategory::Combat, "Apply Effect", []() { return std::make_shared<ApplyEffectNode>(); });

    // World
    RegisterNode("SpawnEntity", EventNodeCategory::World, "Spawn Entity", []() { return std::make_shared<SpawnEntityNode>(); });
    RegisterNode("DestroyEntity", EventNodeCategory::World, "Destroy Entity", []() { return std::make_shared<DestroyEntityNode>(); });
    RegisterNode("FindEntities", EventNodeCategory::World, "Find Entities", []() { return std::make_shared<FindEntitiesNode>(); });
    RegisterNode("GetClosestEntity", EventNodeCategory::World, "Get Closest Entity", []() { return std::make_shared<GetClosestEntityNode>(); });
    RegisterNode("BroadcastEvent", EventNodeCategory::World, "Broadcast Event", []() { return std::make_shared<BroadcastEventNode>(); });

    // Terrain
    RegisterNode("GetTerrainHeight", EventNodeCategory::Terrain, "Get Terrain Height", []() { return std::make_shared<GetTerrainHeightNode>(); });
    RegisterNode("ModifyTerrain", EventNodeCategory::Terrain, "Modify Terrain", []() { return std::make_shared<ModifyTerrainNode>(); });
    RegisterNode("CreateCrater", EventNodeCategory::Terrain, "Create Crater", []() { return std::make_shared<CreateCraterNode>(); });

    // Python
    RegisterNode("CallPythonFunction", EventNodeCategory::Python, "Call Python Function", []() { return std::make_shared<CallPythonFunctionNode>(); });
    RegisterNode("ExecutePythonCode", EventNodeCategory::Python, "Execute Python Code", []() { return std::make_shared<ExecutePythonCodeNode>(); });

    // Debug
    RegisterNode("Print", EventNodeCategory::Debug, "Print", []() { return std::make_shared<PrintNode>(); });
    RegisterNode("Breakpoint", EventNodeCategory::Debug, "Breakpoint", []() { return std::make_shared<BreakpointNode>(); });
}

} // namespace Nova
