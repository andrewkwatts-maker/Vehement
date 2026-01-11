/**
 * @file EditorCommand.cpp
 * @brief Implementation of editor command classes for undo/redo system
 */

#include "EditorCommand.hpp"
#include "../scene/SceneNode.hpp"
#include "../scene/Scene.hpp"
#include <algorithm>
#include <sstream>

namespace Nova {

// =============================================================================
// Utility Functions
// =============================================================================

TransformState CaptureTransformState(const SceneNode* node) {
    TransformState state;
    if (node) {
        state.position = node->GetPosition();
        state.rotation = node->GetRotation();
        state.scale = node->GetScale();
    }
    return state;
}

void ApplyTransformState(SceneNode* node, const TransformState& state) {
    if (node) {
        node->SetPosition(state.position);
        node->SetRotation(state.rotation);
        node->SetScale(state.scale);
    }
}

// =============================================================================
// TransformCommand Implementation
// =============================================================================

TransformCommand::TransformCommand(SceneNode* node, const TransformState& newState)
    : m_node(node)
    , m_oldState(CaptureTransformState(node))
    , m_newState(newState)
{
}

TransformCommand::TransformCommand(SceneNode* node, const TransformState& oldState, const TransformState& newState)
    : m_node(node)
    , m_oldState(oldState)
    , m_newState(newState)
{
}

bool TransformCommand::Execute() {
    if (!m_node) {
        return false;
    }
    ApplyTransformState(m_node, m_newState);
    return true;
}

bool TransformCommand::Undo() {
    if (!m_node) {
        return false;
    }
    ApplyTransformState(m_node, m_oldState);
    return true;
}

std::string TransformCommand::GetName() const {
    if (!m_node) {
        return "Transform (Invalid)";
    }

    std::ostringstream ss;
    ss << "Transform '" << m_node->GetName() << "'";

    // Describe what changed
    bool hasChange = false;
    if (m_oldState.position != m_newState.position) {
        ss << " [Move]";
        hasChange = true;
    }
    if (m_oldState.rotation != m_newState.rotation) {
        ss << " [Rotate]";
        hasChange = true;
    }
    if (m_oldState.scale != m_newState.scale) {
        ss << " [Scale]";
        hasChange = true;
    }

    if (!hasChange) {
        ss << " (No Change)";
    }

    return ss.str();
}

CommandTypeId TransformCommand::GetTypeId() const {
    return GetCommandTypeId<TransformCommand>();
}

bool TransformCommand::CanMergeWith(const ICommand& other) const {
    // Can only merge with another TransformCommand
    if (other.GetTypeId() != GetTypeId()) {
        return false;
    }

    const auto& otherTransform = static_cast<const TransformCommand&>(other);

    // Must be the same node
    if (otherTransform.m_node != m_node) {
        return false;
    }

    // Must be within time window
    if (!IsWithinMergeWindow()) {
        return false;
    }

    return true;
}

bool TransformCommand::MergeWith(const ICommand& other) {
    if (!CanMergeWith(other)) {
        return false;
    }

    const auto& otherTransform = static_cast<const TransformCommand&>(other);

    // Keep our old state, take their new state
    // This effectively combines: old -> intermediate -> new into old -> new
    m_newState = otherTransform.m_newState;

    // Update timestamp to allow continued merging
    m_timestamp = std::chrono::steady_clock::now();

    return true;
}

// =============================================================================
// CreateObjectCommand Implementation
// =============================================================================

CreateObjectCommand::CreateObjectCommand(Scene* scene, std::unique_ptr<SceneNode> node, SceneNode* parent)
    : m_scene(scene)
    , m_ownedNode(std::move(node))
    , m_parent(parent)
{
    if (m_ownedNode) {
        m_nodePtr = m_ownedNode.get();
        m_nodeName = m_ownedNode->GetName();
    }
}

bool CreateObjectCommand::Execute() {
    if (!m_scene || !m_ownedNode) {
        return false;
    }

    // Determine target parent
    SceneNode* targetParent = m_parent ? m_parent : m_scene->GetRoot();
    if (!targetParent) {
        return false;
    }

    // Transfer ownership to scene
    m_nodePtr = m_ownedNode.get();
    targetParent->AddChild(std::move(m_ownedNode));

    return true;
}

bool CreateObjectCommand::Undo() {
    if (!m_scene || !m_nodePtr) {
        return false;
    }

    // Find the parent and remove the node
    SceneNode* parent = m_nodePtr->GetParent();
    if (!parent) {
        return false;
    }

    // Take ownership back from scene
    m_ownedNode = parent->RemoveChild(m_nodePtr);

    return m_ownedNode != nullptr;
}

std::string CreateObjectCommand::GetName() const {
    return "Create '" + m_nodeName + "'";
}

CommandTypeId CreateObjectCommand::GetTypeId() const {
    return GetCommandTypeId<CreateObjectCommand>();
}

// =============================================================================
// DeleteObjectCommand Implementation
// =============================================================================

DeleteObjectCommand::DeleteObjectCommand(Scene* scene, SceneNode* node)
    : m_scene(scene)
    , m_nodePtr(node)
{
    if (node) {
        m_nodeName = node->GetName();
        m_parent = node->GetParent();

        // Calculate sibling index for proper restoration position
        if (m_parent) {
            const auto& siblings = m_parent->GetChildren();
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == node) {
                    m_siblingIndex = i;
                    break;
                }
            }
        }
    }
}

bool DeleteObjectCommand::Execute() {
    if (!m_scene || !m_nodePtr || !m_parent) {
        return false;
    }

    // Remove from parent and take ownership
    m_ownedNode = m_parent->RemoveChild(m_nodePtr);

    return m_ownedNode != nullptr;
}

bool DeleteObjectCommand::Undo() {
    if (!m_scene || !m_ownedNode || !m_parent) {
        return false;
    }

    // Re-add to parent
    // Note: We can't guarantee exact sibling position without a more complex API
    // For now, we add to the end. A more sophisticated implementation would
    // insert at the original position.
    m_nodePtr = m_ownedNode.get();
    m_parent->AddChild(std::move(m_ownedNode));

    return true;
}

std::string DeleteObjectCommand::GetName() const {
    return "Delete '" + m_nodeName + "'";
}

CommandTypeId DeleteObjectCommand::GetTypeId() const {
    return GetCommandTypeId<DeleteObjectCommand>();
}

// =============================================================================
// RenameCommand Implementation
// =============================================================================

RenameCommand::RenameCommand(SceneNode* node, const std::string& newName)
    : m_node(node)
    , m_newName(newName)
{
    if (node) {
        m_oldName = node->GetName();
    }
}

bool RenameCommand::Execute() {
    if (!m_node) {
        return false;
    }
    m_node->SetName(m_newName);
    return true;
}

bool RenameCommand::Undo() {
    if (!m_node) {
        return false;
    }
    m_node->SetName(m_oldName);
    return true;
}

std::string RenameCommand::GetName() const {
    return "Rename '" + m_oldName + "' to '" + m_newName + "'";
}

CommandTypeId RenameCommand::GetTypeId() const {
    return GetCommandTypeId<RenameCommand>();
}

// =============================================================================
// ReparentCommand Implementation
// =============================================================================

ReparentCommand::ReparentCommand(SceneNode* node, SceneNode* newParent)
    : m_node(node)
    , m_newParent(newParent)
{
    if (node) {
        m_oldParent = node->GetParent();

        // Calculate sibling index
        if (m_oldParent) {
            const auto& siblings = m_oldParent->GetChildren();
            for (size_t i = 0; i < siblings.size(); ++i) {
                if (siblings[i].get() == node) {
                    m_oldSiblingIndex = i;
                    break;
                }
            }
        }
    }
}

bool ReparentCommand::Execute() {
    if (!m_node) {
        return false;
    }

    // SetParent handles the reparenting logic
    m_node->SetParent(m_newParent);
    return true;
}

bool ReparentCommand::Undo() {
    if (!m_node) {
        return false;
    }

    m_node->SetParent(m_oldParent);
    return true;
}

std::string ReparentCommand::GetName() const {
    if (!m_node) {
        return "Reparent (Invalid)";
    }

    std::ostringstream ss;
    ss << "Reparent '" << m_node->GetName() << "'";

    if (m_oldParent) {
        ss << " from '" << m_oldParent->GetName() << "'";
    } else {
        ss << " from root";
    }

    if (m_newParent) {
        ss << " to '" << m_newParent->GetName() << "'";
    } else {
        ss << " to root";
    }

    return ss.str();
}

CommandTypeId ReparentCommand::GetTypeId() const {
    return GetCommandTypeId<ReparentCommand>();
}

// =============================================================================
// CompositeCommand Implementation
// =============================================================================

CompositeCommand::CompositeCommand(const std::string& name)
    : m_name(name)
{
}

CompositeCommand::CompositeCommand(const std::string& name, std::vector<CommandPtr> commands)
    : m_name(name)
    , m_commands(std::move(commands))
{
}

void CompositeCommand::AddCommand(CommandPtr command) {
    if (command) {
        m_commands.push_back(std::move(command));
    }
}

bool CompositeCommand::Execute() {
    m_executedCount = 0;

    for (auto& cmd : m_commands) {
        if (!cmd->Execute()) {
            // Rollback on failure
            for (size_t i = m_executedCount; i > 0; --i) {
                m_commands[i - 1]->Undo();
            }
            return false;
        }
        ++m_executedCount;
    }

    return true;
}

bool CompositeCommand::Undo() {
    // Undo in reverse order
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        if (!(*it)->Undo()) {
            // Partial undo - this is a problematic state
            // In production, you might want to log this and attempt recovery
            return false;
        }
    }

    return true;
}

std::string CompositeCommand::GetName() const {
    if (m_commands.empty()) {
        return m_name + " (Empty)";
    }

    std::ostringstream ss;
    ss << m_name << " (" << m_commands.size() << " operations)";
    return ss.str();
}

CommandTypeId CompositeCommand::GetTypeId() const {
    return GetCommandTypeId<CompositeCommand>();
}

} // namespace Nova
