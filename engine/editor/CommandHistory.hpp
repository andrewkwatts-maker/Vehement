/**
 * @file CommandHistory.hpp
 * @brief Command stack management for editor undo/redo system
 *
 * Manages command history with undo/redo stacks and command merging support.
 */

#pragma once

#include "EditorCommand.hpp"
#include <deque>
#include <functional>
#include <optional>

namespace Nova {

/**
 * @brief History change event types
 */
enum class HistoryEventType {
    CommandExecuted,    ///< A new command was executed
    CommandUndone,      ///< A command was undone
    CommandRedone,      ///< A command was redone
    HistoryCleared,     ///< History was cleared
    CommandMerged       ///< A command was merged with the previous one
};

/**
 * @brief Callback signature for history change notifications
 */
using HistoryChangeCallback = std::function<void(HistoryEventType, const ICommand*)>;

/**
 * @brief Configuration for CommandHistory behavior
 */
struct CommandHistoryConfig {
    size_t maxUndoLevels = 100;           ///< Maximum number of undo levels (0 = unlimited)
    uint32_t mergeWindowMs = 500;          ///< Time window for command merging in milliseconds
    bool enableMerging = true;             ///< Enable automatic command merging
    bool clearRedoOnExecute = true;        ///< Clear redo stack when new command is executed
};

/**
 * @brief Manages command history for undo/redo operations
 *
 * Thread-safety: This class is NOT thread-safe. All operations should be
 * performed from the main/editor thread.
 *
 * Usage example:
 * @code
 * CommandHistory history;
 *
 * // Execute commands
 * history.ExecuteCommand(std::make_unique<TransformCommand>(node, newState));
 *
 * // Undo/Redo
 * if (history.CanUndo()) history.Undo();
 * if (history.CanRedo()) history.Redo();
 *
 * // Listen for changes
 * history.SetOnHistoryChanged([](HistoryEventType type, const ICommand* cmd) {
 *     UpdateUndoRedoMenuItems();
 * });
 * @endcode
 */
class CommandHistory {
public:
    /**
     * @brief Construct with default configuration
     */
    CommandHistory();

    /**
     * @brief Construct with custom configuration
     * @param config Configuration settings
     */
    explicit CommandHistory(const CommandHistoryConfig& config);

    /**
     * @brief Destructor
     */
    ~CommandHistory() = default;

    // Non-copyable but movable
    CommandHistory(const CommandHistory&) = delete;
    CommandHistory& operator=(const CommandHistory&) = delete;
    CommandHistory(CommandHistory&&) noexcept = default;
    CommandHistory& operator=(CommandHistory&&) noexcept = default;

    // =========================================================================
    // Core Operations
    // =========================================================================

    /**
     * @brief Execute a command and add it to history
     *
     * Executes the command immediately. If execution succeeds, the command
     * is added to the undo stack. May attempt to merge with the previous
     * command if merging is enabled.
     *
     * @param command The command to execute (ownership transferred)
     * @return true if execution succeeded
     */
    bool ExecuteCommand(CommandPtr command);

    /**
     * @brief Undo the most recent command
     * @return true if undo succeeded
     */
    bool Undo();

    /**
     * @brief Redo the most recently undone command
     * @return true if redo succeeded
     */
    bool Redo();

    /**
     * @brief Undo multiple commands
     * @param count Number of commands to undo
     * @return Number of commands actually undone
     */
    size_t UndoMultiple(size_t count);

    /**
     * @brief Redo multiple commands
     * @param count Number of commands to redo
     * @return Number of commands actually redone
     */
    size_t RedoMultiple(size_t count);

    // =========================================================================
    // State Queries
    // =========================================================================

    /**
     * @brief Check if undo is available
     * @return true if there are commands to undo
     */
    [[nodiscard]] bool CanUndo() const;

    /**
     * @brief Check if redo is available
     * @return true if there are commands to redo
     */
    [[nodiscard]] bool CanRedo() const;

    /**
     * @brief Get the number of commands in undo stack
     */
    [[nodiscard]] size_t GetUndoCount() const;

    /**
     * @brief Get the number of commands in redo stack
     */
    [[nodiscard]] size_t GetRedoCount() const;

    /**
     * @brief Check if history is empty (no undo or redo)
     */
    [[nodiscard]] bool IsEmpty() const;

    /**
     * @brief Check if document has unsaved changes
     *
     * Returns true if executed commands since last save mark.
     */
    [[nodiscard]] bool IsDirty() const;

    // =========================================================================
    // Command Information
    // =========================================================================

    /**
     * @brief Get the name of the next command to undo
     * @return Command name or empty string if no undo available
     */
    [[nodiscard]] std::string GetUndoCommandName() const;

    /**
     * @brief Get the name of the next command to redo
     * @return Command name or empty string if no redo available
     */
    [[nodiscard]] std::string GetRedoCommandName() const;

    /**
     * @brief Get names of all commands in undo stack (most recent first)
     * @param maxCount Maximum number of names to return (0 = all)
     * @return Vector of command names
     */
    [[nodiscard]] std::vector<std::string> GetUndoHistory(size_t maxCount = 0) const;

    /**
     * @brief Get names of all commands in redo stack (next to redo first)
     * @param maxCount Maximum number of names to return (0 = all)
     * @return Vector of command names
     */
    [[nodiscard]] std::vector<std::string> GetRedoHistory(size_t maxCount = 0) const;

    /**
     * @brief Peek at the last executed command without removing it
     * @return Pointer to command or nullptr if undo stack is empty
     */
    [[nodiscard]] const ICommand* PeekUndo() const;

    /**
     * @brief Peek at the next command to redo
     * @return Pointer to command or nullptr if redo stack is empty
     */
    [[nodiscard]] const ICommand* PeekRedo() const;

    // =========================================================================
    // History Management
    // =========================================================================

    /**
     * @brief Clear all history (undo and redo stacks)
     */
    void Clear();

    /**
     * @brief Clear only the redo stack
     */
    void ClearRedo();

    /**
     * @brief Mark current state as saved (clears dirty flag)
     *
     * Call this after saving the document. IsDirty() will return false
     * until more commands are executed.
     */
    void MarkSaved();

    /**
     * @brief Set maximum undo levels
     *
     * If current history exceeds new limit, oldest commands are removed.
     *
     * @param maxLevels Maximum undo levels (0 = unlimited)
     */
    void SetMaxUndoLevels(size_t maxLevels);

    /**
     * @brief Get current maximum undo levels
     */
    [[nodiscard]] size_t GetMaxUndoLevels() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const CommandHistoryConfig& GetConfig() const;

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void SetConfig(const CommandHistoryConfig& config);

    /**
     * @brief Enable or disable command merging
     * @param enabled Whether merging is enabled
     */
    void SetMergingEnabled(bool enabled);

    /**
     * @brief Check if command merging is enabled
     */
    [[nodiscard]] bool IsMergingEnabled() const;

    /**
     * @brief Set merge time window
     * @param windowMs Time window in milliseconds
     */
    void SetMergeWindow(uint32_t windowMs);

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for history change notifications
     *
     * Called whenever the history state changes (command executed, undone,
     * redone, or cleared). Useful for updating UI elements.
     *
     * @param callback Function to call on history changes
     */
    void SetOnHistoryChanged(HistoryChangeCallback callback);

    /**
     * @brief Remove the history change callback
     */
    void ClearOnHistoryChanged();

    // =========================================================================
    // Transaction Support
    // =========================================================================

    /**
     * @brief Begin a transaction (group multiple commands)
     *
     * All commands executed during a transaction will be grouped into a
     * single CompositeCommand for undo purposes.
     *
     * @param name Display name for the transaction
     */
    void BeginTransaction(const std::string& name);

    /**
     * @brief Commit the current transaction
     *
     * Finalizes the transaction and adds the composite command to history.
     * Does nothing if no transaction is active.
     *
     * @return true if transaction was committed
     */
    bool CommitTransaction();

    /**
     * @brief Rollback the current transaction
     *
     * Undoes all commands in the current transaction and discards them.
     * Does nothing if no transaction is active.
     *
     * @return true if transaction was rolled back
     */
    bool RollbackTransaction();

    /**
     * @brief Check if a transaction is currently active
     */
    [[nodiscard]] bool IsTransactionActive() const;

    /**
     * @brief Get the name of the current transaction
     * @return Transaction name or empty string if none active
     */
    [[nodiscard]] std::string GetTransactionName() const;

private:
    /**
     * @brief Attempt to merge command with previous
     * @param command The new command
     * @return true if merge occurred
     */
    bool TryMerge(const CommandPtr& command);

    /**
     * @brief Enforce maximum undo levels
     */
    void EnforceUndoLimit();

    /**
     * @brief Notify listeners of history change
     */
    void NotifyHistoryChanged(HistoryEventType type, const ICommand* command);

    // Configuration
    CommandHistoryConfig m_config;

    // Command stacks (deque for efficient front/back operations)
    std::deque<CommandPtr> m_undoStack;
    std::deque<CommandPtr> m_redoStack;

    // Transaction support
    std::unique_ptr<CompositeCommand> m_activeTransaction;

    // Save state tracking
    size_t m_savedAtIndex = 0;  // Index in undo stack when saved
    bool m_savedStateLost = false;  // True if saved state was pushed off stack

    // Callbacks
    HistoryChangeCallback m_onHistoryChanged;
};

/**
 * @brief RAII transaction scope guard
 *
 * Automatically commits or rolls back a transaction when destroyed.
 *
 * Usage:
 * @code
 * {
 *     TransactionScope scope(history, "Multi-Object Transform");
 *     history.ExecuteCommand(...);
 *     history.ExecuteCommand(...);
 *     scope.Commit();  // or scope.Rollback();
 * }  // Auto-commits if not explicitly committed/rolled back
 * @endcode
 */
class TransactionScope {
public:
    /**
     * @brief Create a transaction scope
     * @param history The command history to operate on
     * @param name Transaction name
     * @param rollbackOnException If true, rollback on destruction without commit
     */
    TransactionScope(CommandHistory& history, const std::string& name, bool rollbackOnException = true);

    /**
     * @brief Destructor - commits or rolls back based on state
     */
    ~TransactionScope();

    // Non-copyable, non-movable
    TransactionScope(const TransactionScope&) = delete;
    TransactionScope& operator=(const TransactionScope&) = delete;
    TransactionScope(TransactionScope&&) = delete;
    TransactionScope& operator=(TransactionScope&&) = delete;

    /**
     * @brief Explicitly commit the transaction
     */
    void Commit();

    /**
     * @brief Explicitly rollback the transaction
     */
    void Rollback();

    /**
     * @brief Check if transaction has been finalized
     */
    [[nodiscard]] bool IsFinalized() const { return m_finalized; }

private:
    CommandHistory& m_history;
    bool m_rollbackOnException;
    bool m_finalized = false;
};

} // namespace Nova
