#include "EditHistory.hpp"
#include "../world/TileMap.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Vehement {

// =============================================================================
// TilePaintCommand
// =============================================================================

TilePaintCommand::TilePaintCommand()
    : EditCommand(EditCommandType::TilePaint) {
}

TilePaintCommand::TilePaintCommand(TileMap* map)
    : EditCommand(EditCommandType::TilePaint)
    , m_map(map) {
}

void TilePaintCommand::Execute() {
    if (!m_map) return;

    for (const auto& change : m_changes) {
        if (m_map->IsValidPosition(change.position.x, change.position.y)) {
            Tile& tile = m_map->GetTile(change.position.x, change.position.y);
            tile.type = change.newType;
            tile.textureVariant = change.newVariant;
            tile.isWall = change.newIsWall;
            tile.wallHeight = change.newWallHeight;
        }
    }

    m_executed = true;
}

void TilePaintCommand::Undo() {
    if (!m_map) return;

    for (const auto& change : m_changes) {
        if (m_map->IsValidPosition(change.position.x, change.position.y)) {
            Tile& tile = m_map->GetTile(change.position.x, change.position.y);
            tile.type = change.oldType;
            tile.textureVariant = change.oldVariant;
            tile.isWall = change.oldIsWall;
            tile.wallHeight = change.oldWallHeight;
        }
    }

    m_executed = false;
}

bool TilePaintCommand::CanMergeWith(const EditCommand& other) const {
    if (other.GetType() != EditCommandType::TilePaint) {
        return false;
    }

    // Merge if timestamps are close
    uint64_t timeDiff = (other.GetTimestamp() > m_timestamp) ?
                        (other.GetTimestamp() - m_timestamp) :
                        (m_timestamp - other.GetTimestamp());

    return timeDiff < 500;  // 500ms threshold
}

void TilePaintCommand::MergeWith(EditCommand& other) {
    auto* paintCmd = dynamic_cast<TilePaintCommand*>(&other);
    if (!paintCmd) return;

    // Merge changes
    for (const auto& change : paintCmd->m_changes) {
        // Check if we already have a change for this position
        auto it = std::find_if(m_changes.begin(), m_changes.end(),
            [&change](const TileEditData& c) {
                return c.position == change.position;
            });

        if (it != m_changes.end()) {
            // Update new values, keep original old values
            it->newType = change.newType;
            it->newVariant = change.newVariant;
            it->newIsWall = change.newIsWall;
            it->newWallHeight = change.newWallHeight;
        } else {
            m_changes.push_back(change);
        }
    }

    m_description = "Paint " + std::to_string(m_changes.size()) + " tiles";
}

std::string TilePaintCommand::Serialize() const {
    std::ostringstream ss;
    ss << "TILE_PAINT\n";
    ss << m_changes.size() << "\n";

    for (const auto& change : m_changes) {
        ss << change.position.x << " " << change.position.y << " ";
        ss << static_cast<int>(change.oldType) << " " << static_cast<int>(change.newType) << " ";
        ss << static_cast<int>(change.oldVariant) << " " << static_cast<int>(change.newVariant) << " ";
        ss << change.oldIsWall << " " << change.newIsWall << " ";
        ss << change.oldWallHeight << " " << change.newWallHeight << "\n";
    }

    return ss.str();
}

bool TilePaintCommand::Deserialize(const std::string& data) {
    std::istringstream ss(data);
    std::string type;
    ss >> type;

    if (type != "TILE_PAINT") return false;

    size_t count;
    ss >> count;

    m_changes.clear();
    m_changes.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        TileEditData change;
        int oldType, newType, oldVariant, newVariant;

        ss >> change.position.x >> change.position.y;
        ss >> oldType >> newType >> oldVariant >> newVariant;
        ss >> change.oldIsWall >> change.newIsWall;
        ss >> change.oldWallHeight >> change.newWallHeight;

        change.oldType = static_cast<TileType>(oldType);
        change.newType = static_cast<TileType>(newType);
        change.oldVariant = static_cast<uint8_t>(oldVariant);
        change.newVariant = static_cast<uint8_t>(newVariant);

        m_changes.push_back(change);
    }

    return true;
}

void TilePaintCommand::AddTileChange(const TileEditData& change) {
    m_changes.push_back(change);
}

// =============================================================================
// BuildingCommand
// =============================================================================

BuildingCommand::BuildingCommand()
    : EditCommand(EditCommandType::BuildingPlace) {
}

BuildingCommand::BuildingCommand(EditCommandType type, uint32_t buildingId)
    : EditCommand(type)
    , m_buildingId(buildingId) {
}

void BuildingCommand::Execute() {
    // Would interact with world/building system
    m_executed = true;
}

void BuildingCommand::Undo() {
    // Would interact with world/building system
    m_executed = false;
}

std::string BuildingCommand::Serialize() const {
    std::ostringstream ss;
    ss << (m_type == EditCommandType::BuildingPlace ? "BUILDING_PLACE" : "BUILDING_REMOVE") << "\n";
    ss << m_buildingId << "\n";
    ss << m_position.x << " " << m_position.y << "\n";
    ss << m_rotation << "\n";
    ss << m_buildingData.size() << "\n";
    ss << m_buildingData;
    return ss.str();
}

bool BuildingCommand::Deserialize(const std::string& data) {
    std::istringstream ss(data);
    std::string type;
    ss >> type;

    if (type == "BUILDING_PLACE") {
        m_type = EditCommandType::BuildingPlace;
    } else if (type == "BUILDING_REMOVE") {
        m_type = EditCommandType::BuildingRemove;
    } else {
        return false;
    }

    ss >> m_buildingId;
    ss >> m_position.x >> m_position.y;
    ss >> m_rotation;

    size_t dataSize;
    ss >> dataSize;
    ss.ignore();

    m_buildingData.resize(dataSize);
    ss.read(&m_buildingData[0], dataSize);

    return true;
}

// =============================================================================
// BatchCommand
// =============================================================================

BatchCommand::BatchCommand()
    : EditCommand(EditCommandType::BatchOperation) {
}

void BatchCommand::Execute() {
    for (auto& cmd : m_commands) {
        cmd->Execute();
    }
    m_executed = true;
}

void BatchCommand::Undo() {
    // Undo in reverse order
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        (*it)->Undo();
    }
    m_executed = false;
}

std::string BatchCommand::Serialize() const {
    std::ostringstream ss;
    ss << "BATCH\n";
    ss << m_description << "\n";
    ss << m_commands.size() << "\n";

    for (const auto& cmd : m_commands) {
        std::string cmdData = cmd->Serialize();
        ss << cmdData.size() << "\n";
        ss << cmdData;
    }

    return ss.str();
}

bool BatchCommand::Deserialize(const std::string& /*data*/) {
    // Would need command factory to deserialize child commands
    return true;
}

void BatchCommand::AddCommand(std::unique_ptr<EditCommand> command) {
    m_commands.push_back(std::move(command));
}

void BatchCommand::Clear() {
    m_commands.clear();
}

// =============================================================================
// EditHistory
// =============================================================================

EditHistory::EditHistory()
    : m_lastAutoSave(std::chrono::steady_clock::now()) {
}

EditHistory::~EditHistory() {
    if (m_config.enableAutoSave) {
        AutoSave();
    }
}

void EditHistory::SetConfig(const EditHistoryConfig& config) {
    m_config = config;
    TrimHistory();
}

// =============================================================================
// Command Execution
// =============================================================================

void EditHistory::ExecuteCommand(std::unique_ptr<EditCommand> command) {
    if (!command) return;

    command->SetTimestamp(GetCurrentTimestamp());

    if (m_inBatch) {
        command->Execute();
        m_currentBatch->AddCommand(std::move(command));
        return;
    }

    // Check if we should merge with previous command
    if (m_config.mergeConsecutive && !m_undoStack.empty() && ShouldMerge(*command)) {
        command->Execute();
        m_undoStack.back()->MergeWith(*command);
    } else {
        command->Execute();
        m_undoStack.push_back(std::move(command));
    }

    // Clear redo stack
    m_redoStack.clear();

    // Trim history if needed
    TrimHistory();

    // Callbacks
    if (m_onCommandExecuted && m_undoStack.back()) {
        m_onCommandExecuted(*m_undoStack.back());
    }

    if (m_onHistoryChanged) {
        m_onHistoryChanged();
    }

    // Check auto-save
    if (m_config.enableAutoSave) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAutoSave);
        if (elapsed.count() >= m_config.autoSaveIntervalSeconds) {
            AutoSave();
        }
    }
}

void EditHistory::BeginBatch(const std::string& description) {
    if (m_inBatch) return;

    m_inBatch = true;
    m_currentBatch = std::make_unique<BatchCommand>();
    m_currentBatch->SetDescription(description);
    m_currentBatch->SetTimestamp(GetCurrentTimestamp());
}

void EditHistory::EndBatch() {
    if (!m_inBatch) return;

    m_inBatch = false;

    if (m_currentBatch && m_currentBatch->GetCommandCount() > 0) {
        m_undoStack.push_back(std::move(m_currentBatch));
        m_redoStack.clear();
        TrimHistory();

        if (m_onHistoryChanged) {
            m_onHistoryChanged();
        }
    }

    m_currentBatch.reset();
}

void EditHistory::CancelBatch() {
    if (!m_inBatch) return;

    // Undo all commands in batch
    if (m_currentBatch) {
        m_currentBatch->Undo();
    }

    m_inBatch = false;
    m_currentBatch.reset();
}

// =============================================================================
// Undo / Redo
// =============================================================================

bool EditHistory::Undo() {
    if (m_undoStack.empty()) return false;

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    command->Undo();

    if (m_onUndo) {
        m_onUndo(*command);
    }

    m_redoStack.push_back(std::move(command));

    if (m_onHistoryChanged) {
        m_onHistoryChanged();
    }

    return true;
}

bool EditHistory::Redo() {
    if (m_redoStack.empty()) return false;

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    command->Redo();

    if (m_onRedo) {
        m_onRedo(*command);
    }

    m_undoStack.push_back(std::move(command));

    if (m_onHistoryChanged) {
        m_onHistoryChanged();
    }

    return true;
}

int EditHistory::UndoMultiple(int count) {
    int undone = 0;
    for (int i = 0; i < count && Undo(); ++i) {
        ++undone;
    }
    return undone;
}

int EditHistory::RedoMultiple(int count) {
    int redone = 0;
    for (int i = 0; i < count && Redo(); ++i) {
        ++redone;
    }
    return redone;
}

// =============================================================================
// History Management
// =============================================================================

void EditHistory::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();

    if (m_onHistoryChanged) {
        m_onHistoryChanged();
    }
}

void EditHistory::ClearRedo() {
    m_redoStack.clear();

    if (m_onHistoryChanged) {
        m_onHistoryChanged();
    }
}

std::string EditHistory::GetUndoDescription() const {
    if (m_undoStack.empty()) return "";
    return m_undoStack.back()->GetDescription();
}

std::string EditHistory::GetRedoDescription() const {
    if (m_redoStack.empty()) return "";
    return m_redoStack.back()->GetDescription();
}

std::vector<std::string> EditHistory::GetUndoDescriptions(int maxCount) const {
    std::vector<std::string> descriptions;
    int count = std::min(maxCount, static_cast<int>(m_undoStack.size()));

    for (int i = 0; i < count; ++i) {
        size_t idx = m_undoStack.size() - 1 - i;
        descriptions.push_back(m_undoStack[idx]->GetDescription());
    }

    return descriptions;
}

std::vector<std::string> EditHistory::GetRedoDescriptions(int maxCount) const {
    std::vector<std::string> descriptions;
    int count = std::min(maxCount, static_cast<int>(m_redoStack.size()));

    for (int i = 0; i < count; ++i) {
        size_t idx = m_redoStack.size() - 1 - i;
        descriptions.push_back(m_redoStack[idx]->GetDescription());
    }

    return descriptions;
}

// =============================================================================
// Serialization
// =============================================================================

bool EditHistory::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << ToJson();
    return file.good();
}

bool EditHistory::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    return FromJson(ss.str());
}

std::string EditHistory::ToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"undoStack\": [\n";

    for (size_t i = 0; i < m_undoStack.size(); ++i) {
        if (i > 0) ss << ",\n";
        std::string data = m_undoStack[i]->Serialize();
        ss << "    \"" << data << "\"";
    }

    ss << "\n  ],\n  \"redoStack\": [\n";

    for (size_t i = 0; i < m_redoStack.size(); ++i) {
        if (i > 0) ss << ",\n";
        std::string data = m_redoStack[i]->Serialize();
        ss << "    \"" << data << "\"";
    }

    ss << "\n  ]\n}";
    return ss.str();
}

bool EditHistory::FromJson(const std::string& /*json*/) {
    // Would need command factory to deserialize commands
    return true;
}

void EditHistory::AutoSave() {
    if (!m_config.autoSavePath.empty()) {
        SaveToFile(m_config.autoSavePath);
        m_lastAutoSave = std::chrono::steady_clock::now();
    }
}

bool EditHistory::RecoverFromAutoSave() {
    if (m_config.autoSavePath.empty()) return false;

    std::ifstream file(m_config.autoSavePath);
    if (!file.is_open()) return false;

    return LoadFromFile(m_config.autoSavePath);
}

// =============================================================================
// Private Helpers
// =============================================================================

void EditHistory::TrimHistory() {
    while (static_cast<int>(m_undoStack.size()) > m_config.maxHistorySize) {
        m_undoStack.pop_front();
    }
}

bool EditHistory::ShouldMerge(const EditCommand& newCmd) const {
    if (m_undoStack.empty()) return false;

    const auto& lastCmd = *m_undoStack.back();

    if (!lastCmd.CanMergeWith(newCmd)) return false;

    uint64_t timeDiff = newCmd.GetTimestamp() - lastCmd.GetTimestamp();
    return timeDiff < static_cast<uint64_t>(m_config.mergeTimeThreshold * 1000);
}

uint64_t EditHistory::GetCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count()
    );
}

} // namespace Vehement
