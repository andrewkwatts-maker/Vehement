#include "StateMachineEditor.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>

namespace Vehement {

StateMachineEditor::StateMachineEditor() = default;
StateMachineEditor::~StateMachineEditor() = default;

void StateMachineEditor::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

bool StateMachineEditor::LoadStateMachine(const std::string& filepath) {
    auto sm = std::make_unique<DataDrivenStateMachine>();
    if (!sm->LoadFromFile(filepath)) {
        return false;
    }

    m_filePath = filepath;
    return LoadStateMachine(sm.get());
}

bool StateMachineEditor::LoadStateMachine(DataDrivenStateMachine* stateMachine) {
    m_stateMachine = stateMachine;
    m_stateNodes.clear();
    m_transitions.clear();

    if (!stateMachine) {
        return false;
    }

    // Create visual nodes for each state
    const auto& states = stateMachine->GetStates();
    float x = 100.0f;
    float y = 100.0f;
    const float xSpacing = 200.0f;
    const float ySpacing = 150.0f;
    int col = 0;

    for (const auto& state : states) {
        StateNode node;
        node.stateName = state.name;
        node.position = glm::vec2(x + col * xSpacing, y);
        node.isDefault = (state.name == stateMachine->GetDefaultState());

        m_stateNodes.push_back(node);

        col++;
        if (col >= 4) {
            col = 0;
            y += ySpacing;
        }
    }

    // Create visual connections for transitions
    for (const auto& state : states) {
        for (const auto& trans : state.transitions) {
            TransitionConnection conn;
            conn.fromState = state.name;
            conn.toState = trans.targetState;
            conn.condition = trans.condition;
            m_transitions.push_back(conn);
        }
    }

    m_dirty = false;
    return true;
}

bool StateMachineEditor::SaveStateMachine(const std::string& filepath) {
    if (!m_stateMachine) {
        return false;
    }

    // Update state positions in custom data (for editor)
    json editorData;
    editorData["nodePositions"] = json::object();
    for (const auto& node : m_stateNodes) {
        editorData["nodePositions"][node.stateName] = {
            {"x", node.position.x},
            {"y", node.position.y}
        };
    }

    // Save the state machine with editor data
    json output = m_stateMachine->ToJson();
    output["_editor"] = editorData;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << output.dump(2);
    m_filePath = filepath;
    m_dirty = false;
    return true;
}

bool StateMachineEditor::SaveStateMachine() {
    if (m_filePath.empty()) {
        return false;
    }
    return SaveStateMachine(m_filePath);
}

void StateMachineEditor::NewStateMachine() {
    m_stateNodes.clear();
    m_transitions.clear();
    m_selectedState.clear();
    m_filePath.clear();
    m_dirty = false;

    // Add default idle state
    AddState("idle", glm::vec2(400.0f, 300.0f));
    SetDefaultState("idle");
}

json StateMachineEditor::ExportToJson() const {
    if (m_stateMachine) {
        json output = m_stateMachine->ToJson();

        // Add editor layout data
        json editorData;
        editorData["nodePositions"] = json::object();
        for (const auto& node : m_stateNodes) {
            editorData["nodePositions"][node.stateName] = {
                {"x", node.position.x},
                {"y", node.position.y}
            };
        }
        output["_editor"] = editorData;

        return output;
    }

    return json::object();
}

bool StateMachineEditor::ImportFromJson(const json& data) {
    auto sm = std::make_unique<DataDrivenStateMachine>();
    if (!sm->LoadFromJson(data)) {
        return false;
    }

    if (!LoadStateMachine(sm.get())) {
        return false;
    }

    // Load editor layout data
    if (data.contains("_editor") && data["_editor"].contains("nodePositions")) {
        const auto& positions = data["_editor"]["nodePositions"];
        for (auto& node : m_stateNodes) {
            if (positions.contains(node.stateName)) {
                node.position.x = positions[node.stateName].value("x", node.position.x);
                node.position.y = positions[node.stateName].value("y", node.position.y);
            }
        }
    }

    return true;
}

StateNode* StateMachineEditor::AddState(const std::string& name, const glm::vec2& position) {
    // Check if state already exists
    for (const auto& node : m_stateNodes) {
        if (node.stateName == name) {
            return nullptr;
        }
    }

    StateNode node;
    node.stateName = name;
    node.position = m_config.snapToGrid ? SnapToGrid(position) : position;
    node.isDefault = m_stateNodes.empty();

    m_stateNodes.push_back(node);

    // Add to state machine
    if (m_stateMachine) {
        AnimationState state;
        state.name = name;
        state.loop = true;
        m_stateMachine->AddState(state);

        if (node.isDefault) {
            m_stateMachine->SetDefaultState(name);
        }
    }

    m_dirty = true;
    if (OnModified) OnModified();

    return &m_stateNodes.back();
}

bool StateMachineEditor::RemoveState(const std::string& name) {
    // Find and remove node
    auto nodeIt = std::find_if(m_stateNodes.begin(), m_stateNodes.end(),
                                [&name](const StateNode& n) { return n.stateName == name; });

    if (nodeIt == m_stateNodes.end()) {
        return false;
    }

    bool wasDefault = nodeIt->isDefault;
    m_stateNodes.erase(nodeIt);

    // Remove all transitions involving this state
    m_transitions.erase(
        std::remove_if(m_transitions.begin(), m_transitions.end(),
                       [&name](const TransitionConnection& t) {
                           return t.fromState == name || t.toState == name;
                       }),
        m_transitions.end());

    // Remove from state machine
    if (m_stateMachine) {
        m_stateMachine->RemoveState(name);

        // Set new default if needed
        if (wasDefault && !m_stateNodes.empty()) {
            m_stateNodes[0].isDefault = true;
            m_stateMachine->SetDefaultState(m_stateNodes[0].stateName);
        }
    }

    if (m_selectedState == name) {
        ClearSelection();
    }

    m_dirty = true;
    if (OnModified) OnModified();

    return true;
}

StateNode* StateMachineEditor::GetStateNode(const std::string& name) {
    auto it = std::find_if(m_stateNodes.begin(), m_stateNodes.end(),
                           [&name](const StateNode& n) { return n.stateName == name; });
    return it != m_stateNodes.end() ? &(*it) : nullptr;
}

void StateMachineEditor::SetDefaultState(const std::string& name) {
    for (auto& node : m_stateNodes) {
        node.isDefault = (node.stateName == name);
    }

    if (m_stateMachine) {
        m_stateMachine->SetDefaultState(name);
    }

    m_dirty = true;
}

bool StateMachineEditor::RenameState(const std::string& oldName, const std::string& newName) {
    // Check if new name already exists
    for (const auto& node : m_stateNodes) {
        if (node.stateName == newName) {
            return false;
        }
    }

    // Rename node
    auto* node = GetStateNode(oldName);
    if (!node) {
        return false;
    }

    node->stateName = newName;

    // Update transitions
    for (auto& trans : m_transitions) {
        if (trans.fromState == oldName) trans.fromState = newName;
        if (trans.toState == oldName) trans.toState = newName;
    }

    // Update selection
    if (m_selectedState == oldName) {
        m_selectedState = newName;
    }

    m_dirty = true;
    if (OnModified) OnModified();

    return true;
}

TransitionConnection* StateMachineEditor::AddTransition(const std::string& from,
                                                         const std::string& to) {
    // Check if transition already exists
    for (const auto& trans : m_transitions) {
        if (trans.fromState == from && trans.toState == to) {
            return nullptr;
        }
    }

    TransitionConnection conn;
    conn.fromState = from;
    conn.toState = to;
    m_transitions.push_back(conn);

    // Add to state machine
    if (m_stateMachine) {
        auto* state = m_stateMachine->GetState(from);
        if (state) {
            StateTransition trans;
            trans.targetState = to;
            trans.blendDuration = 0.2f;
            state->transitions.push_back(trans);
        }
    }

    m_dirty = true;
    if (OnModified) OnModified();

    return &m_transitions.back();
}

bool StateMachineEditor::RemoveTransition(const std::string& from, const std::string& to) {
    auto it = std::find_if(m_transitions.begin(), m_transitions.end(),
                           [&from, &to](const TransitionConnection& t) {
                               return t.fromState == from && t.toState == to;
                           });

    if (it == m_transitions.end()) {
        return false;
    }

    m_transitions.erase(it);

    // Remove from state machine
    if (m_stateMachine) {
        auto* state = m_stateMachine->GetState(from);
        if (state) {
            state->transitions.erase(
                std::remove_if(state->transitions.begin(), state->transitions.end(),
                               [&to](const StateTransition& t) { return t.targetState == to; }),
                state->transitions.end());
        }
    }

    m_dirty = true;
    if (OnModified) OnModified();

    return true;
}

void StateMachineEditor::SetTransitionCondition(const std::string& from, const std::string& to,
                                                 const std::string& condition) {
    for (auto& trans : m_transitions) {
        if (trans.fromState == from && trans.toState == to) {
            trans.condition = condition;
            break;
        }
    }

    // Update state machine
    if (m_stateMachine) {
        auto* state = m_stateMachine->GetState(from);
        if (state) {
            for (auto& trans : state->transitions) {
                if (trans.targetState == to) {
                    trans.condition = condition;
                    break;
                }
            }
        }
    }

    m_dirty = true;
    if (OnModified) OnModified();
}

void StateMachineEditor::SelectState(const std::string& name) {
    ClearSelection();
    m_selectedState = name;

    for (auto& node : m_stateNodes) {
        node.selected = (node.stateName == name);
    }

    if (OnSelectionChanged) {
        OnSelectionChanged(name);
    }

    if (OnStateSelected && m_stateMachine) {
        OnStateSelected(m_stateMachine->GetState(name));
    }
}

void StateMachineEditor::SelectTransition(const std::string& from, const std::string& to) {
    ClearSelection();
    m_selectedTransitionFrom = from;
    m_selectedTransitionTo = to;

    for (auto& trans : m_transitions) {
        trans.selected = (trans.fromState == from && trans.toState == to);
    }

    if (OnTransitionSelected && m_stateMachine) {
        auto* state = m_stateMachine->GetState(from);
        if (state) {
            for (auto& trans : state->transitions) {
                if (trans.targetState == to) {
                    OnTransitionSelected(&trans);
                    break;
                }
            }
        }
    }
}

void StateMachineEditor::ClearSelection() {
    m_selectedState.clear();
    m_selectedTransitionFrom.clear();
    m_selectedTransitionTo.clear();

    for (auto& node : m_stateNodes) {
        node.selected = false;
    }
    for (auto& trans : m_transitions) {
        trans.selected = false;
    }
}

void StateMachineEditor::SetViewOffset(const glm::vec2& offset) {
    m_viewOffset = offset;
}

void StateMachineEditor::SetZoom(float zoom) {
    m_zoom = std::clamp(zoom, m_config.zoomMin, m_config.zoomMax);
}

void StateMachineEditor::ZoomToFit() {
    if (m_stateNodes.empty()) {
        m_viewOffset = glm::vec2(0.0f);
        m_zoom = 1.0f;
        return;
    }

    glm::vec2 minPos = m_stateNodes[0].position;
    glm::vec2 maxPos = m_stateNodes[0].position + m_stateNodes[0].size;

    for (const auto& node : m_stateNodes) {
        minPos = glm::min(minPos, node.position);
        maxPos = glm::max(maxPos, node.position + node.size);
    }

    glm::vec2 center = (minPos + maxPos) * 0.5f;
    glm::vec2 size = maxPos - minPos;

    m_viewOffset = -center;
    m_zoom = std::min(m_config.canvasSize.x / (size.x + 200.0f),
                      m_config.canvasSize.y / (size.y + 200.0f));
    m_zoom = std::clamp(m_zoom, m_config.zoomMin, m_config.zoomMax);
}

void StateMachineEditor::CenterOnState(const std::string& name) {
    auto* node = GetStateNode(name);
    if (node) {
        m_viewOffset = -node->position - node->size * 0.5f;
    }
}

void StateMachineEditor::OnMouseDown(const glm::vec2& position, int button) {
    glm::vec2 worldPos = ScreenToWorld(position);

    if (button == 0) {  // Left click
        // Check if clicking on a node
        StateNode* clickedNode = FindNodeAt(worldPos);
        if (clickedNode) {
            SelectState(clickedNode->stateName);
            m_dragging = true;
            m_dragStart = worldPos;
            m_dragOffset = clickedNode->position - worldPos;
            return;
        }

        // Check if clicking on a transition
        TransitionConnection* clickedTrans = FindTransitionAt(worldPos);
        if (clickedTrans) {
            SelectTransition(clickedTrans->fromState, clickedTrans->toState);
            return;
        }

        // Clear selection if clicking empty space
        ClearSelection();
    } else if (button == 1) {  // Right click - start panning
        m_panning = true;
        m_dragStart = position;
    } else if (button == 2) {  // Middle click - start creating transition
        StateNode* clickedNode = FindNodeAt(worldPos);
        if (clickedNode) {
            m_creatingTransition = true;
            m_transitionStartState = clickedNode->stateName;
        }
    }
}

void StateMachineEditor::OnMouseUp(const glm::vec2& position, int button) {
    glm::vec2 worldPos = ScreenToWorld(position);

    if (button == 0 && m_dragging) {
        m_dragging = false;
    }

    if (button == 1 && m_panning) {
        m_panning = false;
    }

    if (button == 2 && m_creatingTransition) {
        StateNode* targetNode = FindNodeAt(worldPos);
        if (targetNode && targetNode->stateName != m_transitionStartState) {
            AddTransition(m_transitionStartState, targetNode->stateName);
        }
        m_creatingTransition = false;
        m_transitionStartState.clear();
    }
}

void StateMachineEditor::OnMouseMove(const glm::vec2& position) {
    glm::vec2 worldPos = ScreenToWorld(position);

    if (m_dragging && !m_selectedState.empty()) {
        auto* node = GetStateNode(m_selectedState);
        if (node) {
            glm::vec2 newPos = worldPos + m_dragOffset;
            node->position = m_config.snapToGrid ? SnapToGrid(newPos) : newPos;
            m_dirty = true;
        }
    }

    if (m_panning) {
        glm::vec2 delta = (position - m_dragStart) / m_zoom;
        m_viewOffset += delta;
        m_dragStart = position;
    }
}

void StateMachineEditor::OnKeyDown(int key) {
    // Delete key
    if (key == 127 || key == 8) {
        if (!m_selectedState.empty()) {
            RemoveState(m_selectedState);
        } else if (!m_selectedTransitionFrom.empty()) {
            RemoveTransition(m_selectedTransitionFrom, m_selectedTransitionTo);
        }
    }
}

void StateMachineEditor::Undo() {
    if (m_undoStack.empty()) {
        return;
    }

    EditorAction action = m_undoStack.back();
    m_undoStack.pop_back();

    // Apply reverse action based on action type
    switch (action.type) {
        case EditorAction::Type::AddState: {
            // Reverse of add is remove
            RemoveState(action.targetName);
            break;
        }
        case EditorAction::Type::RemoveState: {
            // Reverse of remove is add - restore from beforeData
            if (action.beforeData.contains("position")) {
                glm::vec2 pos(
                    action.beforeData["position"].value("x", 100.0f),
                    action.beforeData["position"].value("y", 100.0f)
                );
                AddState(action.targetName, pos);
            }
            break;
        }
        case EditorAction::Type::MoveState: {
            // Restore previous position
            auto* node = GetStateNode(action.targetName);
            if (node && action.beforeData.contains("position")) {
                node->position.x = action.beforeData["position"].value("x", node->position.x);
                node->position.y = action.beforeData["position"].value("y", node->position.y);
            }
            break;
        }
        case EditorAction::Type::ModifyState: {
            // Restore previous state data
            if (m_stateMachine && action.beforeData.contains("state")) {
                auto* state = m_stateMachine->GetState(action.targetName);
                if (state) {
                    state->loop = action.beforeData["state"].value("loop", state->loop);
                    state->speed = action.beforeData["state"].value("speed", state->speed);
                }
            }
            break;
        }
        case EditorAction::Type::AddTransition: {
            // Reverse of add is remove
            if (action.beforeData.contains("from") && action.beforeData.contains("to")) {
                RemoveTransition(
                    action.beforeData["from"].get<std::string>(),
                    action.beforeData["to"].get<std::string>()
                );
            }
            break;
        }
        case EditorAction::Type::RemoveTransition: {
            // Reverse of remove is add
            if (action.beforeData.contains("from") && action.beforeData.contains("to")) {
                auto* trans = AddTransition(
                    action.beforeData["from"].get<std::string>(),
                    action.beforeData["to"].get<std::string>()
                );
                if (trans && action.beforeData.contains("condition")) {
                    trans->condition = action.beforeData["condition"].get<std::string>();
                }
            }
            break;
        }
        case EditorAction::Type::ModifyTransition: {
            // Restore previous condition
            if (action.beforeData.contains("from") && action.beforeData.contains("to")) {
                SetTransitionCondition(
                    action.beforeData["from"].get<std::string>(),
                    action.beforeData["to"].get<std::string>(),
                    action.beforeData.value("condition", "")
                );
            }
            break;
        }
        default:
            // Unhandled action types (AddEvent, RemoveEvent, AddParameter, RemoveParameter)
            break;
    }

    m_redoStack.push_back(action);
    m_dirty = true;
}

void StateMachineEditor::Redo() {
    if (m_redoStack.empty()) {
        return;
    }

    EditorAction action = m_redoStack.back();
    m_redoStack.pop_back();

    // Re-apply action based on action type
    switch (action.type) {
        case EditorAction::Type::AddState: {
            // Re-add the state
            if (action.afterData.contains("position")) {
                glm::vec2 pos(
                    action.afterData["position"].value("x", 100.0f),
                    action.afterData["position"].value("y", 100.0f)
                );
                AddState(action.targetName, pos);
            }
            break;
        }
        case EditorAction::Type::RemoveState: {
            // Re-remove the state
            RemoveState(action.targetName);
            break;
        }
        case EditorAction::Type::MoveState: {
            // Apply new position
            auto* node = GetStateNode(action.targetName);
            if (node && action.afterData.contains("position")) {
                node->position.x = action.afterData["position"].value("x", node->position.x);
                node->position.y = action.afterData["position"].value("y", node->position.y);
            }
            break;
        }
        case EditorAction::Type::ModifyState: {
            // Apply new state data
            if (m_stateMachine && action.afterData.contains("state")) {
                auto* state = m_stateMachine->GetState(action.targetName);
                if (state) {
                    state->loop = action.afterData["state"].value("loop", state->loop);
                    state->speed = action.afterData["state"].value("speed", state->speed);
                }
            }
            break;
        }
        case EditorAction::Type::AddTransition: {
            // Re-add the transition
            if (action.afterData.contains("from") && action.afterData.contains("to")) {
                auto* trans = AddTransition(
                    action.afterData["from"].get<std::string>(),
                    action.afterData["to"].get<std::string>()
                );
                if (trans && action.afterData.contains("condition")) {
                    trans->condition = action.afterData["condition"].get<std::string>();
                }
            }
            break;
        }
        case EditorAction::Type::RemoveTransition: {
            // Re-remove the transition
            if (action.afterData.contains("from") && action.afterData.contains("to")) {
                RemoveTransition(
                    action.afterData["from"].get<std::string>(),
                    action.afterData["to"].get<std::string>()
                );
            }
            break;
        }
        case EditorAction::Type::ModifyTransition: {
            // Apply new condition
            if (action.afterData.contains("from") && action.afterData.contains("to")) {
                SetTransitionCondition(
                    action.afterData["from"].get<std::string>(),
                    action.afterData["to"].get<std::string>(),
                    action.afterData.value("condition", "")
                );
            }
            break;
        }
        default:
            // Unhandled action types (AddEvent, RemoveEvent, AddParameter, RemoveParameter)
            break;
    }

    m_undoStack.push_back(action);
    m_dirty = true;
}

void StateMachineEditor::AutoLayout(const std::string& algorithm) {
    if (algorithm == "hierarchical") {
        AutoLayoutHierarchical();
    } else {
        AutoLayoutForceDirected();
    }

    m_dirty = true;
    if (OnModified) OnModified();
}

void StateMachineEditor::AlignStates(const std::string& alignment) {
    if (m_selectedState.empty()) {
        return;
    }

    // For now, just align to grid
    for (auto& node : m_stateNodes) {
        if (node.selected) {
            node.position = SnapToGrid(node.position);
        }
    }

    m_dirty = true;
}

void StateMachineEditor::StartPreview() {
    m_previewPlaying = true;
    m_previewTime = 0.0f;

    if (m_stateMachine) {
        m_stateMachine->Start();
    }
}

void StateMachineEditor::StopPreview() {
    m_previewPlaying = false;
}

void StateMachineEditor::UpdatePreview(float deltaTime) {
    if (!m_previewPlaying || !m_stateMachine) {
        return;
    }

    m_previewTime += deltaTime;
    m_stateMachine->Update(deltaTime);

    // Highlight current state
    const std::string& currentState = m_stateMachine->GetCurrentState();
    for (auto& node : m_stateNodes) {
        node.selected = (node.stateName == currentState);
    }
}

void StateMachineEditor::SetPreviewParameter(const std::string& name, float value) {
    if (m_stateMachine) {
        m_stateMachine->SetFloat(name, value);
    }
}

void StateMachineEditor::RecordAction(EditorAction::Type type, const std::string& target,
                                       const json& before, const json& after) {
    EditorAction action;
    action.type = type;
    action.targetName = target;
    action.beforeData = before;
    action.afterData = after;

    m_undoStack.push_back(action);
    m_redoStack.clear();

    while (m_undoStack.size() > MAX_UNDO_SIZE) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

StateNode* StateMachineEditor::FindNodeAt(const glm::vec2& position) {
    for (auto& node : m_stateNodes) {
        if (position.x >= node.position.x && position.x <= node.position.x + node.size.x &&
            position.y >= node.position.y && position.y <= node.position.y + node.size.y) {
            return &node;
        }
    }
    return nullptr;
}

TransitionConnection* StateMachineEditor::FindTransitionAt(const glm::vec2& position) {
    // Simple distance-based selection for transitions
    constexpr float selectionRadius = 10.0f;

    for (auto& trans : m_transitions) {
        auto* fromNode = GetStateNode(trans.fromState);
        auto* toNode = GetStateNode(trans.toState);

        if (!fromNode || !toNode) continue;

        glm::vec2 start = fromNode->position + fromNode->size * 0.5f;
        glm::vec2 end = toNode->position + toNode->size * 0.5f;

        // Point-to-line distance
        glm::vec2 line = end - start;
        float lineLength = glm::length(line);
        if (lineLength < 0.001f) continue;

        glm::vec2 lineDir = line / lineLength;
        glm::vec2 toPoint = position - start;

        float projection = glm::dot(toPoint, lineDir);
        if (projection < 0 || projection > lineLength) continue;

        glm::vec2 closestPoint = start + lineDir * projection;
        float distance = glm::length(position - closestPoint);

        if (distance < selectionRadius) {
            return &trans;
        }
    }

    return nullptr;
}

glm::vec2 StateMachineEditor::ScreenToWorld(const glm::vec2& screenPos) const {
    return (screenPos / m_zoom) - m_viewOffset;
}

glm::vec2 StateMachineEditor::WorldToScreen(const glm::vec2& worldPos) const {
    return (worldPos + m_viewOffset) * m_zoom;
}

glm::vec2 StateMachineEditor::SnapToGrid(const glm::vec2& position) const {
    return glm::vec2(
        std::round(position.x / m_config.gridSize.x) * m_config.gridSize.x,
        std::round(position.y / m_config.gridSize.y) * m_config.gridSize.y
    );
}

void StateMachineEditor::AutoLayoutHierarchical() {
    if (m_stateNodes.empty() || !m_stateMachine) {
        return;
    }

    // Build adjacency from transitions
    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    std::unordered_map<std::string, int> inDegree;

    for (const auto& node : m_stateNodes) {
        adjacency[node.stateName] = {};
        inDegree[node.stateName] = 0;
    }

    for (const auto& trans : m_transitions) {
        adjacency[trans.fromState].push_back(trans.toState);
        inDegree[trans.toState]++;
    }

    // Topological sort with BFS
    std::vector<std::vector<std::string>> levels;
    std::queue<std::string> queue;

    // Start with default state or states with no incoming edges
    std::string defaultState = m_stateMachine->GetDefaultState();
    if (!defaultState.empty()) {
        queue.push(defaultState);
    }

    for (const auto& [name, degree] : inDegree) {
        if (degree == 0 && name != defaultState) {
            queue.push(name);
        }
    }

    std::unordered_set<std::string> visited;
    while (!queue.empty()) {
        std::vector<std::string> level;
        size_t levelSize = queue.size();

        for (size_t i = 0; i < levelSize; ++i) {
            std::string current = queue.front();
            queue.pop();

            if (visited.count(current)) continue;
            visited.insert(current);
            level.push_back(current);

            for (const auto& neighbor : adjacency[current]) {
                if (!visited.count(neighbor)) {
                    queue.push(neighbor);
                }
            }
        }

        if (!level.empty()) {
            levels.push_back(level);
        }
    }

    // Add any unvisited nodes
    for (const auto& node : m_stateNodes) {
        if (!visited.count(node.stateName)) {
            levels.push_back({node.stateName});
        }
    }

    // Position nodes by level
    const float levelSpacing = 250.0f;
    const float nodeSpacing = 180.0f;
    float x = 100.0f;

    for (const auto& level : levels) {
        float y = 100.0f + (m_stateNodes.size() * nodeSpacing - level.size() * nodeSpacing) * 0.5f;

        for (const auto& stateName : level) {
            auto* node = GetStateNode(stateName);
            if (node) {
                node->position = glm::vec2(x, y);
                y += nodeSpacing;
            }
        }

        x += levelSpacing;
    }
}

void StateMachineEditor::AutoLayoutForceDirected() {
    // Simple force-directed layout
    constexpr int iterations = 100;
    constexpr float repulsion = 5000.0f;
    constexpr float attraction = 0.01f;
    constexpr float damping = 0.9f;

    std::unordered_map<std::string, glm::vec2> velocities;
    for (const auto& node : m_stateNodes) {
        velocities[node.stateName] = glm::vec2(0.0f);
    }

    for (int iter = 0; iter < iterations; ++iter) {
        // Repulsion between all pairs
        for (size_t i = 0; i < m_stateNodes.size(); ++i) {
            for (size_t j = i + 1; j < m_stateNodes.size(); ++j) {
                glm::vec2 diff = m_stateNodes[j].position - m_stateNodes[i].position;
                float dist = glm::length(diff) + 0.1f;
                glm::vec2 force = (diff / dist) * (repulsion / (dist * dist));

                velocities[m_stateNodes[i].stateName] -= force;
                velocities[m_stateNodes[j].stateName] += force;
            }
        }

        // Attraction along edges
        for (const auto& trans : m_transitions) {
            auto* fromNode = GetStateNode(trans.fromState);
            auto* toNode = GetStateNode(trans.toState);

            if (fromNode && toNode) {
                glm::vec2 diff = toNode->position - fromNode->position;
                float dist = glm::length(diff);
                glm::vec2 force = diff * attraction;

                velocities[trans.fromState] += force;
                velocities[trans.toState] -= force;
            }
        }

        // Apply velocities
        for (auto& node : m_stateNodes) {
            node.position += velocities[node.stateName];
            velocities[node.stateName] *= damping;
        }
    }
}

// ============================================================================
// StatePropertiesPanel
// ============================================================================

void StatePropertiesPanel::SetState(AnimationState* state) {
    m_state = state;
    if (state) {
        m_editState = *state;
    }
}

bool StatePropertiesPanel::Render() {
    if (!m_state) {
        return false;
    }

    // This would render UI elements in an actual implementation
    // For now, just return whether changes were made
    return false;
}

AnimationState StatePropertiesPanel::GetModifiedState() const {
    return m_editState;
}

// ============================================================================
// TransitionPropertiesPanel
// ============================================================================

void TransitionPropertiesPanel::SetTransition(StateTransition* transition) {
    m_transition = transition;
    if (transition) {
        m_editTransition = *transition;
    }
}

bool TransitionPropertiesPanel::Render() {
    if (!m_transition) {
        return false;
    }

    // This would render UI elements in an actual implementation
    return false;
}

StateTransition TransitionPropertiesPanel::GetModifiedTransition() const {
    return m_editTransition;
}

} // namespace Vehement
