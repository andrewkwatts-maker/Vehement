/**
 * @file EditorSelectionManager.cpp
 * @brief Implementation of multi-selection support with history
 */

#include "EditorSelectionManager.hpp"
#include "../scene/Scene.hpp"
#include "../scene/SceneNode.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace Nova {

// =============================================================================
// Initialization
// =============================================================================

void EditorSelectionManager::Initialize(Scene* scene) {
    m_scene = scene;
    m_selection.clear();
    m_selectionSet.clear();
    ClearHistory();

    spdlog::debug("EditorSelectionManager initialized");
}

void EditorSelectionManager::Shutdown() {
    m_selection.clear();
    m_selectionSet.clear();
    ClearHistory();
    m_listeners.clear();
    m_onSelectionChanged = nullptr;
    m_scene = nullptr;

    spdlog::debug("EditorSelectionManager shutdown");
}

// =============================================================================
// Selection Operations
// =============================================================================

void EditorSelectionManager::Select(SceneNode* node) {
    std::vector<SceneNode*> previous = m_selection;

    m_selection.clear();
    m_selectionSet.clear();

    if (node && PassesFilter(node)) {
        m_selection.push_back(node);
        m_selectionSet.insert(node);
    }

    if (previous != m_selection) {
        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::Select(const std::vector<SceneNode*>& nodes) {
    std::vector<SceneNode*> previous = m_selection;

    m_selection.clear();
    m_selectionSet.clear();

    for (auto* node : nodes) {
        if (node && PassesFilter(node) && m_selectionSet.count(node) == 0) {
            m_selection.push_back(node);
            m_selectionSet.insert(node);
        }
    }

    if (previous != m_selection) {
        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::AddToSelection(SceneNode* node) {
    if (!node || IsSelected(node) || !PassesFilter(node)) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.push_back(node);
    m_selectionSet.insert(node);

    PushToHistory();
    NotifySelectionChanged(previous);
}

void EditorSelectionManager::AddToSelection(const std::vector<SceneNode*>& nodes) {
    std::vector<SceneNode*> previous = m_selection;
    bool changed = false;

    for (auto* node : nodes) {
        if (node && !IsSelected(node) && PassesFilter(node)) {
            m_selection.push_back(node);
            m_selectionSet.insert(node);
            changed = true;
        }
    }

    if (changed) {
        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::RemoveFromSelection(SceneNode* node) {
    if (!node || !IsSelected(node)) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.erase(std::remove(m_selection.begin(), m_selection.end(), node), m_selection.end());
    m_selectionSet.erase(node);

    PushToHistory();
    NotifySelectionChanged(previous);
}

void EditorSelectionManager::ToggleSelection(SceneNode* node) {
    if (!node) {
        return;
    }

    if (IsSelected(node)) {
        RemoveFromSelection(node);
    } else {
        AddToSelection(node);
    }
}

void EditorSelectionManager::ClearSelection() {
    if (m_selection.empty()) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.clear();
    m_selectionSet.clear();

    PushToHistory();
    NotifySelectionChanged(previous);
}

void EditorSelectionManager::SelectAll() {
    SelectionFilter filter;
    filter.type = SelectionFilterType::All;
    SelectAll(filter);
}

void EditorSelectionManager::SelectAll(const SelectionFilter& filter) {
    if (!m_scene) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    m_selection.clear();
    m_selectionSet.clear();

    std::vector<SceneNode*> allNodes;
    if (auto* root = m_scene->GetRoot()) {
        CollectSceneNodes(root, allNodes);
    }

    for (auto* node : allNodes) {
        if (PassesFilterInternal(node, filter)) {
            m_selection.push_back(node);
            m_selectionSet.insert(node);
        }
    }

    if (previous != m_selection) {
        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::InvertSelection() {
    if (!m_scene) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    std::unordered_set<const SceneNode*> previousSet = m_selectionSet;

    m_selection.clear();
    m_selectionSet.clear();

    std::vector<SceneNode*> allNodes;
    if (auto* root = m_scene->GetRoot()) {
        CollectSceneNodes(root, allNodes);
    }

    for (auto* node : allNodes) {
        if (previousSet.count(node) == 0 && PassesFilter(node)) {
            m_selection.push_back(node);
            m_selectionSet.insert(node);
        }
    }

    if (previous != m_selection) {
        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::SelectParent() {
    if (m_selection.empty()) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    std::vector<SceneNode*> parents;

    for (auto* node : m_selection) {
        if (auto* parent = node->GetParent()) {
            if (std::find(parents.begin(), parents.end(), parent) == parents.end() &&
                PassesFilter(parent)) {
                parents.push_back(parent);
            }
        }
    }

    if (!parents.empty()) {
        m_selection = parents;
        m_selectionSet.clear();
        for (auto* node : m_selection) {
            m_selectionSet.insert(node);
        }

        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::SelectChildren(bool recursive) {
    if (m_selection.empty()) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    std::vector<SceneNode*> children;

    for (auto* node : m_selection) {
        for (const auto& child : node->GetChildren()) {
            if (std::find(children.begin(), children.end(), child.get()) == children.end() &&
                PassesFilter(child.get())) {
                children.push_back(child.get());

                if (recursive) {
                    std::vector<SceneNode*> descendants;
                    CollectSceneNodes(child.get(), descendants);
                    for (auto* desc : descendants) {
                        if (std::find(children.begin(), children.end(), desc) == children.end() &&
                            PassesFilter(desc)) {
                            children.push_back(desc);
                        }
                    }
                }
            }
        }
    }

    if (!children.empty()) {
        m_selection = children;
        m_selectionSet.clear();
        for (auto* node : m_selection) {
            m_selectionSet.insert(node);
        }

        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

void EditorSelectionManager::SelectSiblings() {
    if (m_selection.empty()) {
        return;
    }

    std::vector<SceneNode*> previous = m_selection;
    std::vector<SceneNode*> siblings;

    for (auto* node : m_selection) {
        if (auto* parent = node->GetParent()) {
            for (const auto& sibling : parent->GetChildren()) {
                if (std::find(siblings.begin(), siblings.end(), sibling.get()) == siblings.end() &&
                    PassesFilter(sibling.get())) {
                    siblings.push_back(sibling.get());
                }
            }
        }
    }

    if (!siblings.empty() && siblings != m_selection) {
        m_selection = siblings;
        m_selectionSet.clear();
        for (auto* node : m_selection) {
            m_selectionSet.insert(node);
        }

        PushToHistory();
        NotifySelectionChanged(previous);
    }
}

// =============================================================================
// Selection Queries
// =============================================================================

bool EditorSelectionManager::IsSelected(const SceneNode* node) const {
    return m_selectionSet.count(node) > 0;
}

SceneNode* EditorSelectionManager::GetPrimarySelection() const {
    return m_selection.empty() ? nullptr : m_selection.back();
}

SceneNode* EditorSelectionManager::GetFirstSelection() const {
    return m_selection.empty() ? nullptr : m_selection.front();
}

std::vector<SceneNode*> EditorSelectionManager::GetFilteredSelection(const SelectionFilter& filter) const {
    std::vector<SceneNode*> result;
    for (auto* node : m_selection) {
        if (PassesFilterInternal(node, filter)) {
            result.push_back(node);
        }
    }
    return result;
}

// =============================================================================
// Selection History
// =============================================================================

bool EditorSelectionManager::UndoSelection() {
    if (!CanUndoSelection()) {
        return false;
    }

    // Save current to redo stack
    SelectionHistoryEntry redoEntry;
    redoEntry.selection = m_selection;
    m_redoHistory.push_front(redoEntry);

    // Pop from undo stack
    const auto& undoEntry = m_undoHistory.front();
    std::vector<SceneNode*> previous = m_selection;

    m_selection = undoEntry.selection;
    m_selectionSet.clear();
    for (auto* node : m_selection) {
        m_selectionSet.insert(node);
    }

    m_undoHistory.pop_front();

    NotifySelectionChanged(previous, true);
    return true;
}

bool EditorSelectionManager::RedoSelection() {
    if (!CanRedoSelection()) {
        return false;
    }

    // Save current to undo stack
    SelectionHistoryEntry undoEntry;
    undoEntry.selection = m_selection;
    m_undoHistory.push_front(undoEntry);

    // Pop from redo stack
    const auto& redoEntry = m_redoHistory.front();
    std::vector<SceneNode*> previous = m_selection;

    m_selection = redoEntry.selection;
    m_selectionSet.clear();
    for (auto* node : m_selection) {
        m_selectionSet.insert(node);
    }

    m_redoHistory.pop_front();

    NotifySelectionChanged(previous, true);
    return true;
}

bool EditorSelectionManager::CanUndoSelection() const {
    return !m_undoHistory.empty();
}

bool EditorSelectionManager::CanRedoSelection() const {
    return !m_redoHistory.empty();
}

void EditorSelectionManager::ClearHistory() {
    m_undoHistory.clear();
    m_redoHistory.clear();
}

// =============================================================================
// Callbacks
// =============================================================================

void EditorSelectionManager::SetOnSelectionChanged(std::function<void(const SelectionChangedEvent&)> callback) {
    m_onSelectionChanged = std::move(callback);
}

size_t EditorSelectionManager::AddSelectionListener(std::function<void(const SelectionChangedEvent&)> callback) {
    size_t id = m_nextListenerId++;
    m_listeners[id] = std::move(callback);
    return id;
}

void EditorSelectionManager::RemoveSelectionListener(size_t listenerId) {
    m_listeners.erase(listenerId);
}

// =============================================================================
// Filter Management
// =============================================================================

void EditorSelectionManager::ClearFilter() {
    m_activeFilter = SelectionFilter{};
}

bool EditorSelectionManager::PassesFilter(const SceneNode* node) const {
    return PassesFilterInternal(node, m_activeFilter);
}

bool EditorSelectionManager::PassesFilterInternal(const SceneNode* node, const SelectionFilter& filter) const {
    if (!node) {
        return false;
    }

    // Check hidden state
    if (!filter.includeHidden && !node->IsActive()) {
        return false;
    }

    switch (filter.type) {
        case SelectionFilterType::All:
            return true;

        case SelectionFilterType::Meshes:
            return node->HasComponent("MeshRenderer");

        case SelectionFilterType::Lights:
            return node->HasComponent("Light");

        case SelectionFilterType::Cameras:
            return node->HasComponent("Camera");

        case SelectionFilterType::SDFPrimitives:
            return node->HasComponent("SDFRenderer");

        case SelectionFilterType::Empty:
            return node->GetComponentCount() == 0;

        case SelectionFilterType::Custom:
            return filter.customFilter ? filter.customFilter(node) : true;
    }

    return true;
}

// =============================================================================
// Utility
// =============================================================================

bool EditorSelectionManager::GetSelectionCenter(float* outCenter) const {
    if (m_selection.empty() || !outCenter) {
        return false;
    }

    float center[3] = {0.0f, 0.0f, 0.0f};
    int count = 0;

    for (auto* node : m_selection) {
        auto pos = node->GetWorldPosition();
        center[0] += pos.x;
        center[1] += pos.y;
        center[2] += pos.z;
        count++;
    }

    if (count > 0) {
        outCenter[0] = center[0] / count;
        outCenter[1] = center[1] / count;
        outCenter[2] = center[2] / count;
        return true;
    }

    return false;
}

bool EditorSelectionManager::GetSelectionBounds(float* outMin, float* outMax) const {
    if (m_selection.empty() || !outMin || !outMax) {
        return false;
    }

    constexpr float maxFloat = std::numeric_limits<float>::max();
    float minBounds[3] = {maxFloat, maxFloat, maxFloat};
    float maxBounds[3] = {-maxFloat, -maxFloat, -maxFloat};

    for (auto* node : m_selection) {
        auto pos = node->GetWorldPosition();
        minBounds[0] = std::min(minBounds[0], pos.x);
        minBounds[1] = std::min(minBounds[1], pos.y);
        minBounds[2] = std::min(minBounds[2], pos.z);
        maxBounds[0] = std::max(maxBounds[0], pos.x);
        maxBounds[1] = std::max(maxBounds[1], pos.y);
        maxBounds[2] = std::max(maxBounds[2], pos.z);
    }

    outMin[0] = minBounds[0];
    outMin[1] = minBounds[1];
    outMin[2] = minBounds[2];
    outMax[0] = maxBounds[0];
    outMax[1] = maxBounds[1];
    outMax[2] = maxBounds[2];

    return true;
}

void EditorSelectionManager::OnNodeDeleted(SceneNode* deletedNode) {
    if (!deletedNode) {
        return;
    }

    // Remove from current selection
    bool wasSelected = IsSelected(deletedNode);
    if (wasSelected) {
        m_selection.erase(std::remove(m_selection.begin(), m_selection.end(), deletedNode),
                          m_selection.end());
        m_selectionSet.erase(deletedNode);
    }

    // Clean up history entries
    for (auto& entry : m_undoHistory) {
        entry.selection.erase(
            std::remove(entry.selection.begin(), entry.selection.end(), deletedNode),
            entry.selection.end());
    }

    for (auto& entry : m_redoHistory) {
        entry.selection.erase(
            std::remove(entry.selection.begin(), entry.selection.end(), deletedNode),
            entry.selection.end());
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

void EditorSelectionManager::NotifySelectionChanged(const std::vector<SceneNode*>& previous, bool fromUndo) {
    SelectionChangedEvent event;
    event.previousSelection = previous;
    event.newSelection = m_selection;
    event.fromUndo = fromUndo;

    if (m_onSelectionChanged) {
        m_onSelectionChanged(event);
    }

    for (const auto& [id, callback] : m_listeners) {
        if (callback) {
            callback(event);
        }
    }
}

void EditorSelectionManager::PushToHistory() {
    if (!m_historyEnabled) {
        return;
    }

    // Clear redo stack on new selection
    m_redoHistory.clear();

    // Don't push duplicate entries
    if (!m_undoHistory.empty() && m_undoHistory.front().selection == m_selection) {
        return;
    }

    SelectionHistoryEntry entry;
    entry.selection = m_selection;
    m_undoHistory.push_front(entry);

    // Trim history to max size
    while (m_undoHistory.size() > m_maxHistorySize) {
        m_undoHistory.pop_back();
    }
}

void EditorSelectionManager::CollectSceneNodes(SceneNode* node, std::vector<SceneNode*>& outNodes) const {
    if (!node) {
        return;
    }

    // Don't include the root node itself typically
    if (node->GetParent() != nullptr) {
        outNodes.push_back(node);
    }

    for (const auto& child : node->GetChildren()) {
        CollectSceneNodes(child.get(), outNodes);
    }
}

} // namespace Nova
