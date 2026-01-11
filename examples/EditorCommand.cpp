#include "EditorCommand.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

// =============================================================================
// CommandHistory Implementation
// =============================================================================

CommandHistory::CommandHistory() = default;
CommandHistory::~CommandHistory() = default;

void CommandHistory::ExecuteCommand(std::unique_ptr<IEditorCommand> cmd) {
    if (!cmd) {
        spdlog::warn("Attempted to execute null command");
        return;
    }

    // Execute the command
    cmd->Execute();

    // Add to undo stack
    m_undoStack.push_back(std::move(cmd));

    // Clear redo stack (can't redo after a new command)
    m_redoStack.clear();

    // Enforce max history size
    if (static_cast<int>(m_undoStack.size()) > m_maxHistorySize) {
        m_undoStack.erase(m_undoStack.begin());
    }

    spdlog::debug("Command executed: {}", m_undoStack.back()->GetDescription());
}

void CommandHistory::Undo() {
    if (!CanUndo()) {
        spdlog::warn("Cannot undo: undo stack is empty");
        return;
    }

    // Get command from undo stack
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Undo the command
    cmd->Undo();

    // Move to redo stack
    m_redoStack.push_back(std::move(cmd));

    spdlog::debug("Command undone: {}", m_redoStack.back()->GetDescription());
}

void CommandHistory::Redo() {
    if (!CanRedo()) {
        spdlog::warn("Cannot redo: redo stack is empty");
        return;
    }

    // Get command from redo stack
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Re-execute the command
    cmd->Execute();

    // Move back to undo stack
    m_undoStack.push_back(std::move(cmd));

    spdlog::debug("Command redone: {}", m_undoStack.back()->GetDescription());
}

bool CommandHistory::CanUndo() const {
    return !m_undoStack.empty();
}

bool CommandHistory::CanRedo() const {
    return !m_redoStack.empty();
}

void CommandHistory::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    spdlog::debug("Command history cleared");
}

// =============================================================================
// TerrainPaintCommand Implementation
// =============================================================================

TerrainPaintCommand::TerrainPaintCommand(
    std::vector<int>& terrainTiles,
    int tileIndex,
    int oldValue,
    int newValue
)
    : m_terrainTiles(terrainTiles)
    , m_tileIndex(tileIndex)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
}

void TerrainPaintCommand::Execute() {
    if (m_tileIndex >= 0 && m_tileIndex < static_cast<int>(m_terrainTiles.size())) {
        m_terrainTiles[m_tileIndex] = m_newValue;
    }
}

void TerrainPaintCommand::Undo() {
    if (m_tileIndex >= 0 && m_tileIndex < static_cast<int>(m_terrainTiles.size())) {
        m_terrainTiles[m_tileIndex] = m_oldValue;
    }
}

std::string TerrainPaintCommand::GetDescription() const {
    return "Paint terrain tile";
}

// =============================================================================
// TerrainSculptCommand Implementation
// =============================================================================

TerrainSculptCommand::TerrainSculptCommand(std::vector<float>& terrainHeights)
    : m_terrainHeights(terrainHeights)
{
}

void TerrainSculptCommand::Execute() {
    for (size_t i = 0; i < m_affectedTiles.size(); ++i) {
        int index = m_affectedTiles[i];
        if (index >= 0 && index < static_cast<int>(m_terrainHeights.size())) {
            m_terrainHeights[index] = m_newHeights[i];
        }
    }
}

void TerrainSculptCommand::Undo() {
    for (size_t i = 0; i < m_affectedTiles.size(); ++i) {
        int index = m_affectedTiles[i];
        if (index >= 0 && index < static_cast<int>(m_terrainHeights.size())) {
            m_terrainHeights[index] = m_oldHeights[i];
        }
    }
}

std::string TerrainSculptCommand::GetDescription() const {
    return "Sculpt terrain (" + std::to_string(m_affectedTiles.size()) + " tiles)";
}

void TerrainSculptCommand::AddHeightChange(int tileIndex, float oldHeight, float newHeight) {
    m_affectedTiles.push_back(tileIndex);
    m_oldHeights.push_back(oldHeight);
    m_newHeights.push_back(newHeight);
}

// =============================================================================
// ObjectPlaceCommand Implementation
// =============================================================================

ObjectPlaceCommand::ObjectPlaceCommand(
    std::vector<ObjectData>& objects,
    const ObjectData& objectData
)
    : m_objects(objects)
    , m_objectData(objectData)
{
}

void ObjectPlaceCommand::Execute() {
    m_objects.push_back(m_objectData);
    m_objectIndex = static_cast<int>(m_objects.size()) - 1;
}

void ObjectPlaceCommand::Undo() {
    if (m_objectIndex >= 0 && m_objectIndex < static_cast<int>(m_objects.size())) {
        m_objects.erase(m_objects.begin() + m_objectIndex);
        m_objectIndex = -1;
    }
}

std::string ObjectPlaceCommand::GetDescription() const {
    return "Place object: " + m_objectData.type;
}

// =============================================================================
// ObjectDeleteCommand Implementation
// =============================================================================

ObjectDeleteCommand::ObjectDeleteCommand(
    std::vector<ObjectPlaceCommand::ObjectData>& objects,
    int objectIndex
)
    : m_objects(objects)
    , m_objectIndex(objectIndex)
{
    // Store the object data before deletion
    if (m_objectIndex >= 0 && m_objectIndex < static_cast<int>(m_objects.size())) {
        m_deletedObjectData = m_objects[m_objectIndex];
    }
}

void ObjectDeleteCommand::Execute() {
    if (m_objectIndex >= 0 && m_objectIndex < static_cast<int>(m_objects.size())) {
        m_deletedObjectData = m_objects[m_objectIndex];
        m_objects.erase(m_objects.begin() + m_objectIndex);
        m_isDeleted = true;
    }
}

void ObjectDeleteCommand::Undo() {
    if (m_isDeleted) {
        // Re-insert the object at the original index
        if (m_objectIndex <= static_cast<int>(m_objects.size())) {
            m_objects.insert(m_objects.begin() + m_objectIndex, m_deletedObjectData);
            m_isDeleted = false;
        }
    }
}

std::string ObjectDeleteCommand::GetDescription() const {
    return "Delete object: " + m_deletedObjectData.type;
}

// =============================================================================
// ObjectTransformCommand Implementation
// =============================================================================

ObjectTransformCommand::ObjectTransformCommand(
    std::vector<ObjectPlaceCommand::ObjectData>& objects,
    int objectIndex,
    const glm::vec3& oldPosition,
    const glm::vec3& oldRotation,
    const glm::vec3& oldScale,
    const glm::vec3& newPosition,
    const glm::vec3& newRotation,
    const glm::vec3& newScale
)
    : m_objects(objects)
    , m_objectIndex(objectIndex)
    , m_oldPosition(oldPosition)
    , m_oldRotation(oldRotation)
    , m_oldScale(oldScale)
    , m_newPosition(newPosition)
    , m_newRotation(newRotation)
    , m_newScale(newScale)
{
}

void ObjectTransformCommand::Execute() {
    if (m_objectIndex >= 0 && m_objectIndex < static_cast<int>(m_objects.size())) {
        m_objects[m_objectIndex].position = m_newPosition;
        m_objects[m_objectIndex].rotation = m_newRotation;
        m_objects[m_objectIndex].scale = m_newScale;
    }
}

void ObjectTransformCommand::Undo() {
    if (m_objectIndex >= 0 && m_objectIndex < static_cast<int>(m_objects.size())) {
        m_objects[m_objectIndex].position = m_oldPosition;
        m_objects[m_objectIndex].rotation = m_oldRotation;
        m_objects[m_objectIndex].scale = m_oldScale;
    }
}

std::string ObjectTransformCommand::GetDescription() const {
    return "Transform object";
}
