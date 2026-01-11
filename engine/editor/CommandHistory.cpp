/**
 * @file CommandHistory.cpp
 * @brief Implementation of command history management for editor undo/redo
 */

#include "CommandHistory.hpp"
#include <algorithm>

namespace Nova {

// =============================================================================
// CommandHistory Implementation
// =============================================================================

CommandHistory::CommandHistory()
    : m_config()
{
}

CommandHistory::CommandHistory(const CommandHistoryConfig& config)
    : m_config(config)
{
}

bool CommandHistory::ExecuteCommand(CommandPtr command) {
    if (!command) {
        return false;
    }

    // If in a transaction, delegate to the composite command
    if (m_activeTransaction) {
        if (command->Execute()) {
            m_activeTransaction->AddCommand(std::move(command));
            return true;
        }
        return false;
    }

    // Try to merge with previous command if enabled
    if (m_config.enableMerging && TryMerge(command)) {
        NotifyHistoryChanged(HistoryEventType::CommandMerged, m_undoStack.back().get());
        return true;
    }

    // Execute the command
    if (!command->Execute()) {
        return false;
    }

    // Clear redo stack on new command (default behavior)
    if (m_config.clearRedoOnExecute && !m_redoStack.empty()) {
        // If saved state was in redo stack, it's now lost
        m_redoStack.clear();
    }

    // Add to undo stack
    m_undoStack.push_back(std::move(command));

    // Enforce maximum undo levels
    EnforceUndoLimit();

    NotifyHistoryChanged(HistoryEventType::CommandExecuted, m_undoStack.back().get());

    return true;
}

bool CommandHistory::Undo() {
    if (!CanUndo()) {
        return false;
    }

    // Pop from undo stack
    CommandPtr command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Execute undo
    if (!command->Undo()) {
        // Undo failed - put command back (state may be inconsistent)
        m_undoStack.push_back(std::move(command));
        return false;
    }

    // Push to redo stack
    const ICommand* cmdPtr = command.get();
    m_redoStack.push_back(std::move(command));

    NotifyHistoryChanged(HistoryEventType::CommandUndone, cmdPtr);

    return true;
}

bool CommandHistory::Redo() {
    if (!CanRedo()) {
        return false;
    }

    // Pop from redo stack
    CommandPtr command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Re-execute command
    if (!command->Execute()) {
        // Redo failed - put command back
        m_redoStack.push_back(std::move(command));
        return false;
    }

    // Push to undo stack
    const ICommand* cmdPtr = command.get();
    m_undoStack.push_back(std::move(command));

    NotifyHistoryChanged(HistoryEventType::CommandRedone, cmdPtr);

    return true;
}

size_t CommandHistory::UndoMultiple(size_t count) {
    size_t undone = 0;
    while (undone < count && CanUndo()) {
        if (!Undo()) {
            break;
        }
        ++undone;
    }
    return undone;
}

size_t CommandHistory::RedoMultiple(size_t count) {
    size_t redone = 0;
    while (redone < count && CanRedo()) {
        if (!Redo()) {
            break;
        }
        ++redone;
    }
    return redone;
}

bool CommandHistory::CanUndo() const {
    return !m_undoStack.empty();
}

bool CommandHistory::CanRedo() const {
    return !m_redoStack.empty();
}

size_t CommandHistory::GetUndoCount() const {
    return m_undoStack.size();
}

size_t CommandHistory::GetRedoCount() const {
    return m_redoStack.size();
}

bool CommandHistory::IsEmpty() const {
    return m_undoStack.empty() && m_redoStack.empty();
}

bool CommandHistory::IsDirty() const {
    // If saved state was lost due to undo limit, always dirty
    if (m_savedStateLost) {
        return true;
    }

    // Compare current position with saved position
    return m_undoStack.size() != m_savedAtIndex;
}

std::string CommandHistory::GetUndoCommandName() const {
    if (m_undoStack.empty()) {
        return "";
    }
    return m_undoStack.back()->GetName();
}

std::string CommandHistory::GetRedoCommandName() const {
    if (m_redoStack.empty()) {
        return "";
    }
    return m_redoStack.back()->GetName();
}

std::vector<std::string> CommandHistory::GetUndoHistory(size_t maxCount) const {
    std::vector<std::string> history;
    size_t count = maxCount > 0 ? std::min(maxCount, m_undoStack.size()) : m_undoStack.size();
    history.reserve(count);

    // Return in reverse order (most recent first)
    for (auto it = m_undoStack.rbegin();
         it != m_undoStack.rend() && history.size() < count;
         ++it) {
        history.push_back((*it)->GetName());
    }

    return history;
}

std::vector<std::string> CommandHistory::GetRedoHistory(size_t maxCount) const {
    std::vector<std::string> history;
    size_t count = maxCount > 0 ? std::min(maxCount, m_redoStack.size()) : m_redoStack.size();
    history.reserve(count);

    // Return in reverse order (next to redo first)
    for (auto it = m_redoStack.rbegin();
         it != m_redoStack.rend() && history.size() < count;
         ++it) {
        history.push_back((*it)->GetName());
    }

    return history;
}

const ICommand* CommandHistory::PeekUndo() const {
    if (m_undoStack.empty()) {
        return nullptr;
    }
    return m_undoStack.back().get();
}

const ICommand* CommandHistory::PeekRedo() const {
    if (m_redoStack.empty()) {
        return nullptr;
    }
    return m_redoStack.back().get();
}

void CommandHistory::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
    m_activeTransaction.reset();
    m_savedAtIndex = 0;
    m_savedStateLost = false;

    NotifyHistoryChanged(HistoryEventType::HistoryCleared, nullptr);
}

void CommandHistory::ClearRedo() {
    m_redoStack.clear();
}

void CommandHistory::MarkSaved() {
    m_savedAtIndex = m_undoStack.size();
    m_savedStateLost = false;
}

void CommandHistory::SetMaxUndoLevels(size_t maxLevels) {
    m_config.maxUndoLevels = maxLevels;
    EnforceUndoLimit();
}

size_t CommandHistory::GetMaxUndoLevels() const {
    return m_config.maxUndoLevels;
}

const CommandHistoryConfig& CommandHistory::GetConfig() const {
    return m_config;
}

void CommandHistory::SetConfig(const CommandHistoryConfig& config) {
    m_config = config;
    EnforceUndoLimit();
}

void CommandHistory::SetMergingEnabled(bool enabled) {
    m_config.enableMerging = enabled;
}

bool CommandHistory::IsMergingEnabled() const {
    return m_config.enableMerging;
}

void CommandHistory::SetMergeWindow(uint32_t windowMs) {
    m_config.mergeWindowMs = windowMs;
}

void CommandHistory::SetOnHistoryChanged(HistoryChangeCallback callback) {
    m_onHistoryChanged = std::move(callback);
}

void CommandHistory::ClearOnHistoryChanged() {
    m_onHistoryChanged = nullptr;
}

void CommandHistory::BeginTransaction(const std::string& name) {
    // Nested transactions not supported - commit current first
    if (m_activeTransaction) {
        CommitTransaction();
    }

    m_activeTransaction = std::make_unique<CompositeCommand>(name);
}

bool CommandHistory::CommitTransaction() {
    if (!m_activeTransaction) {
        return false;
    }

    // Don't add empty transactions
    if (m_activeTransaction->IsEmpty()) {
        m_activeTransaction.reset();
        return true;
    }

    // Move transaction to undo stack
    CommandPtr composite = std::move(m_activeTransaction);

    // Clear redo stack
    if (m_config.clearRedoOnExecute) {
        m_redoStack.clear();
    }

    // Add to undo stack (already executed during transaction)
    m_undoStack.push_back(std::move(composite));

    // Enforce limits
    EnforceUndoLimit();

    NotifyHistoryChanged(HistoryEventType::CommandExecuted, m_undoStack.back().get());

    return true;
}

bool CommandHistory::RollbackTransaction() {
    if (!m_activeTransaction) {
        return false;
    }

    // Undo all commands in the transaction (in reverse order)
    m_activeTransaction->Undo();
    m_activeTransaction.reset();

    return true;
}

bool CommandHistory::IsTransactionActive() const {
    return m_activeTransaction != nullptr;
}

std::string CommandHistory::GetTransactionName() const {
    if (!m_activeTransaction) {
        return "";
    }
    return m_activeTransaction->GetName();
}

bool CommandHistory::TryMerge(const CommandPtr& command) {
    if (m_undoStack.empty()) {
        return false;
    }

    ICommand* lastCommand = m_undoStack.back().get();

    // Check if merge is possible
    if (!lastCommand->CanMergeWith(*command)) {
        return false;
    }

    // Check time window
    if (!lastCommand->IsWithinMergeWindow(m_config.mergeWindowMs)) {
        return false;
    }

    // Attempt merge (command executes as part of merge)
    if (!command->Execute()) {
        return false;
    }

    // Merge into existing command
    if (!lastCommand->MergeWith(*command)) {
        // Merge failed but command was executed - this shouldn't happen
        // if CanMergeWith was implemented correctly
        command->Undo();
        return false;
    }

    return true;
}

void CommandHistory::EnforceUndoLimit() {
    if (m_config.maxUndoLevels == 0) {
        return;  // No limit
    }

    while (m_undoStack.size() > m_config.maxUndoLevels) {
        // Check if we're removing the saved state
        if (m_savedAtIndex > 0) {
            --m_savedAtIndex;
        } else {
            m_savedStateLost = true;
        }

        m_undoStack.pop_front();
    }
}

void CommandHistory::NotifyHistoryChanged(HistoryEventType type, const ICommand* command) {
    if (m_onHistoryChanged) {
        m_onHistoryChanged(type, command);
    }
}

// =============================================================================
// TransactionScope Implementation
// =============================================================================

TransactionScope::TransactionScope(CommandHistory& history, const std::string& name, bool rollbackOnException)
    : m_history(history)
    , m_rollbackOnException(rollbackOnException)
{
    m_history.BeginTransaction(name);
}

TransactionScope::~TransactionScope() {
    if (!m_finalized) {
        if (m_rollbackOnException && std::uncaught_exceptions() > 0) {
            m_history.RollbackTransaction();
        } else {
            m_history.CommitTransaction();
        }
    }
}

void TransactionScope::Commit() {
    if (!m_finalized) {
        m_history.CommitTransaction();
        m_finalized = true;
    }
}

void TransactionScope::Rollback() {
    if (!m_finalized) {
        m_history.RollbackTransaction();
        m_finalized = true;
    }
}

} // namespace Nova
