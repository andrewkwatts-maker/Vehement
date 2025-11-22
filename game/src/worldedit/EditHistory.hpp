#pragma once

#include "../world/Tile.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <functional>
#include <chrono>

namespace Vehement {

// Forward declarations
class TileMap;
class Building;

/**
 * @brief Type of world edit command
 */
enum class EditCommandType : uint8_t {
    TilePaint,          ///< Tile painting operation
    TileErase,          ///< Tile erase operation
    BuildingPlace,      ///< Building placement
    BuildingRemove,     ///< Building removal
    EntityPlace,        ///< Entity placement
    EntityRemove,       ///< Entity removal
    RoadDraw,           ///< Road drawing
    RoadErase,          ///< Road erase
    WaterCreate,        ///< Water body creation
    WaterRemove,        ///< Water body removal
    ZoneCreate,         ///< Zone creation
    ZoneRemove,         ///< Zone removal
    ElevationChange,    ///< Elevation modification
    BatchOperation,     ///< Batch of multiple operations
    Custom              ///< Custom edit type
};

/**
 * @brief Get display name for edit command type
 */
inline const char* GetEditCommandTypeName(EditCommandType type) {
    switch (type) {
        case EditCommandType::TilePaint:       return "Paint Tiles";
        case EditCommandType::TileErase:       return "Erase Tiles";
        case EditCommandType::BuildingPlace:   return "Place Building";
        case EditCommandType::BuildingRemove:  return "Remove Building";
        case EditCommandType::EntityPlace:     return "Place Entity";
        case EditCommandType::EntityRemove:    return "Remove Entity";
        case EditCommandType::RoadDraw:        return "Draw Road";
        case EditCommandType::RoadErase:       return "Erase Road";
        case EditCommandType::WaterCreate:     return "Create Water";
        case EditCommandType::WaterRemove:     return "Remove Water";
        case EditCommandType::ZoneCreate:      return "Create Zone";
        case EditCommandType::ZoneRemove:      return "Remove Zone";
        case EditCommandType::ElevationChange: return "Change Elevation";
        case EditCommandType::BatchOperation:  return "Batch Operation";
        case EditCommandType::Custom:          return "Custom";
        default:                               return "Unknown";
    }
}

/**
 * @brief Single tile change record
 */
struct TileEditData {
    glm::ivec2 position{0, 0};
    TileType oldType = TileType::None;
    TileType newType = TileType::None;
    uint8_t oldVariant = 0;
    uint8_t newVariant = 0;
    bool oldIsWall = false;
    bool newIsWall = false;
    float oldWallHeight = 0.0f;
    float newWallHeight = 0.0f;
    float oldElevation = 0.0f;
    float newElevation = 0.0f;
};

/**
 * @brief Base class for edit commands (Command pattern)
 */
class EditCommand {
public:
    EditCommand() = default;
    explicit EditCommand(EditCommandType type) : m_type(type) {}
    virtual ~EditCommand() = default;

    // Command interface
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual void Redo() { Execute(); }

    // Merging support for combining similar commands
    virtual bool CanMergeWith(const EditCommand& other) const { (void)other; return false; }
    virtual void MergeWith(EditCommand& other) { (void)other; }

    // Serialization
    virtual std::string Serialize() const = 0;
    virtual bool Deserialize(const std::string& data) = 0;

    // Properties
    [[nodiscard]] EditCommandType GetType() const noexcept { return m_type; }
    [[nodiscard]] const std::string& GetDescription() const noexcept { return m_description; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    [[nodiscard]] uint64_t GetTimestamp() const noexcept { return m_timestamp; }
    void SetTimestamp(uint64_t ts) noexcept { m_timestamp = ts; }

    [[nodiscard]] bool IsExecuted() const noexcept { return m_executed; }

protected:
    EditCommandType m_type = EditCommandType::Custom;
    std::string m_description;
    uint64_t m_timestamp = 0;
    bool m_executed = false;
};

/**
 * @brief Tile paint command
 */
class TilePaintCommand : public EditCommand {
public:
    TilePaintCommand();
    explicit TilePaintCommand(TileMap* map);

    void Execute() override;
    void Undo() override;

    bool CanMergeWith(const EditCommand& other) const override;
    void MergeWith(EditCommand& other) override;

    std::string Serialize() const override;
    bool Deserialize(const std::string& data) override;

    void AddTileChange(const TileEditData& change);
    [[nodiscard]] const std::vector<TileEditData>& GetChanges() const { return m_changes; }
    [[nodiscard]] size_t GetChangeCount() const { return m_changes.size(); }

private:
    TileMap* m_map = nullptr;
    std::vector<TileEditData> m_changes;
};

/**
 * @brief Building place/remove command
 */
class BuildingCommand : public EditCommand {
public:
    BuildingCommand();
    BuildingCommand(EditCommandType type, uint32_t buildingId);

    void Execute() override;
    void Undo() override;

    std::string Serialize() const override;
    bool Deserialize(const std::string& data) override;

    void SetBuildingData(const std::string& data) { m_buildingData = data; }
    [[nodiscard]] uint32_t GetBuildingId() const { return m_buildingId; }

private:
    uint32_t m_buildingId = 0;
    std::string m_buildingData;  // Serialized building state
    glm::ivec2 m_position{0, 0};
    float m_rotation = 0.0f;
};

/**
 * @brief Batch command containing multiple sub-commands
 */
class BatchCommand : public EditCommand {
public:
    BatchCommand();

    void Execute() override;
    void Undo() override;

    std::string Serialize() const override;
    bool Deserialize(const std::string& data) override;

    void AddCommand(std::unique_ptr<EditCommand> command);
    [[nodiscard]] size_t GetCommandCount() const { return m_commands.size(); }
    void Clear();

private:
    std::vector<std::unique_ptr<EditCommand>> m_commands;
};

/**
 * @brief Configuration for edit history
 */
struct EditHistoryConfig {
    int maxHistorySize = 100;           ///< Maximum undo history entries
    bool enableAutoSave = true;         ///< Auto-save history for crash recovery
    std::string autoSavePath;           ///< Path for auto-save files
    int autoSaveIntervalSeconds = 60;   ///< Auto-save interval
    bool mergeConsecutive = true;       ///< Merge consecutive similar operations
    float mergeTimeThreshold = 0.5f;    ///< Time threshold for merging (seconds)
};

/**
 * @brief Edit history manager with undo/redo support
 *
 * Features:
 * - Command pattern for all edit operations
 * - Unlimited undo/redo (with configurable limit)
 * - Batch operations (multiple commands as one)
 * - History serialization for crash recovery
 * - Automatic command merging for paint strokes
 *
 * Usage:
 * 1. Create commands for each edit operation
 * 2. Execute commands through history manager
 * 3. Undo/redo as needed
 * 4. Optionally save history for recovery
 */
class EditHistory {
public:
    using CommandCallback = std::function<void(const EditCommand&)>;

    EditHistory();
    ~EditHistory();

    // Prevent copying, allow moving
    EditHistory(const EditHistory&) = delete;
    EditHistory& operator=(const EditHistory&) = delete;
    EditHistory(EditHistory&&) noexcept = default;
    EditHistory& operator=(EditHistory&&) noexcept = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    void SetConfig(const EditHistoryConfig& config);
    [[nodiscard]] const EditHistoryConfig& GetConfig() const noexcept { return m_config; }

    // =========================================================================
    // Command Execution
    // =========================================================================

    /**
     * @brief Execute a command and add to history
     * @param command Command to execute (takes ownership)
     */
    void ExecuteCommand(std::unique_ptr<EditCommand> command);

    /**
     * @brief Begin a batch operation
     * All commands until EndBatch() are grouped together
     */
    void BeginBatch(const std::string& description = "");

    /**
     * @brief End batch operation
     */
    void EndBatch();

    /**
     * @brief Check if currently in batch mode
     */
    [[nodiscard]] bool IsInBatch() const noexcept { return m_inBatch; }

    /**
     * @brief Cancel current batch (undo all commands in batch)
     */
    void CancelBatch();

    // =========================================================================
    // Undo / Redo
    // =========================================================================

    /**
     * @brief Undo last command
     * @return true if undo was performed
     */
    bool Undo();

    /**
     * @brief Redo last undone command
     * @return true if redo was performed
     */
    bool Redo();

    /**
     * @brief Undo multiple commands
     * @param count Number of commands to undo
     * @return Number of commands actually undone
     */
    int UndoMultiple(int count);

    /**
     * @brief Redo multiple commands
     * @param count Number of commands to redo
     * @return Number of commands actually redone
     */
    int RedoMultiple(int count);

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const noexcept { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const noexcept { return !m_redoStack.empty(); }

    /**
     * @brief Get undo stack size
     */
    [[nodiscard]] size_t GetUndoCount() const noexcept { return m_undoStack.size(); }

    /**
     * @brief Get redo stack size
     */
    [[nodiscard]] size_t GetRedoCount() const noexcept { return m_redoStack.size(); }

    // =========================================================================
    // History Management
    // =========================================================================

    /**
     * @brief Clear all history
     */
    void Clear();

    /**
     * @brief Clear redo stack only
     */
    void ClearRedo();

    /**
     * @brief Get description of command that would be undone
     */
    [[nodiscard]] std::string GetUndoDescription() const;

    /**
     * @brief Get description of command that would be redone
     */
    [[nodiscard]] std::string GetRedoDescription() const;

    /**
     * @brief Get list of undo descriptions
     * @param maxCount Maximum number to return
     */
    [[nodiscard]] std::vector<std::string> GetUndoDescriptions(int maxCount = 10) const;

    /**
     * @brief Get list of redo descriptions
     * @param maxCount Maximum number to return
     */
    [[nodiscard]] std::vector<std::string> GetRedoDescriptions(int maxCount = 10) const;

    // =========================================================================
    // Serialization / Recovery
    // =========================================================================

    /**
     * @brief Save history to file
     * @param path File path
     * @return true if successful
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief Load history from file
     * @param path File path
     * @return true if successful
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Save history to JSON string
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Load history from JSON string
     */
    bool FromJson(const std::string& json);

    /**
     * @brief Trigger auto-save
     */
    void AutoSave();

    /**
     * @brief Check for recovery file and load
     * @return true if recovery was performed
     */
    bool RecoverFromAutoSave();

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnCommandExecuted(CommandCallback callback) {
        m_onCommandExecuted = std::move(callback);
    }
    void SetOnUndo(CommandCallback callback) { m_onUndo = std::move(callback); }
    void SetOnRedo(CommandCallback callback) { m_onRedo = std::move(callback); }
    void SetOnHistoryChanged(std::function<void()> callback) {
        m_onHistoryChanged = std::move(callback);
    }

private:
    void TrimHistory();
    bool ShouldMerge(const EditCommand& newCmd) const;
    uint64_t GetCurrentTimestamp() const;

    EditHistoryConfig m_config;

    // Command stacks
    std::deque<std::unique_ptr<EditCommand>> m_undoStack;
    std::deque<std::unique_ptr<EditCommand>> m_redoStack;

    // Batch mode
    bool m_inBatch = false;
    std::unique_ptr<BatchCommand> m_currentBatch;

    // Auto-save
    std::chrono::steady_clock::time_point m_lastAutoSave;

    // Callbacks
    CommandCallback m_onCommandExecuted;
    CommandCallback m_onUndo;
    CommandCallback m_onRedo;
    std::function<void()> m_onHistoryChanged;
};

} // namespace Vehement
