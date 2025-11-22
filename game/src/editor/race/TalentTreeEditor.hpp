#pragma once

/**
 * @file TalentTreeEditor.hpp
 * @brief Visual talent tree editor for the race designer
 */

#include "../../rts/talent/TalentTree.hpp"
#include "../../rts/talent/TalentNode.hpp"
#include <string>
#include <vector>
#include <functional>

namespace Vehement {
namespace Editor {

using namespace RTS::Talent;

// ============================================================================
// Editor Connection
// ============================================================================

struct EditorConnection {
    std::string fromNodeId;
    std::string toNodeId;
    bool isHighlighted = false;

    [[nodiscard]] nlohmann::json ToJson() const;
    static EditorConnection FromJson(const nlohmann::json& j);
};

// ============================================================================
// Editor State
// ============================================================================

struct TalentEditorState {
    std::string selectedNodeId;
    std::string hoveredNodeId;
    bool isDragging = false;
    float dragStartX = 0.0f;
    float dragStartY = 0.0f;
    float viewOffsetX = 0.0f;
    float viewOffsetY = 0.0f;
    float zoomLevel = 1.0f;
    bool showGrid = true;
    bool showConnections = true;
    bool showAgeMarkers = true;
    int currentAge = 0;
};

// ============================================================================
// Talent Tree Editor
// ============================================================================

class TalentTreeEditor {
public:
    using NodeSelectCallback = std::function<void(const std::string& nodeId)>;
    using TreeModifiedCallback = std::function<void()>;

    TalentTreeEditor();
    ~TalentTreeEditor();

    // =========================================================================
    // Initialization
    // =========================================================================

    bool Initialize();
    void Shutdown();

    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Tree Management
    // =========================================================================

    void NewTree();
    void NewTreeForRace(const std::string& raceId);
    bool LoadTree(const std::string& filepath);
    bool SaveTree(const std::string& filepath);

    TalentTreeDefinition& GetTree() { return m_tree; }
    [[nodiscard]] const TalentTreeDefinition& GetTree() const { return m_tree; }

    // =========================================================================
    // Node Operations
    // =========================================================================

    void AddNode(const TalentNode& node);
    void RemoveNode(const std::string& nodeId);
    void UpdateNode(const TalentNode& node);
    void MoveNode(const std::string& nodeId, int x, int y);
    void DuplicateNode(const std::string& nodeId);

    [[nodiscard]] const TalentNode* GetNode(const std::string& nodeId) const;
    [[nodiscard]] std::vector<const TalentNode*> GetAllNodes() const;
    [[nodiscard]] std::vector<const TalentNode*> GetNodesAtPosition(int x, int y) const;

    // =========================================================================
    // Connection Operations
    // =========================================================================

    void AddConnection(const std::string& fromId, const std::string& toId);
    void RemoveConnection(const std::string& fromId, const std::string& toId);
    [[nodiscard]] std::vector<EditorConnection> GetConnections() const;
    [[nodiscard]] bool HasConnection(const std::string& fromId, const std::string& toId) const;

    // =========================================================================
    // Branch Operations
    // =========================================================================

    void AddBranch(const TalentBranch& branch);
    void RemoveBranch(const std::string& branchId);
    void UpdateBranch(const TalentBranch& branch);
    void AssignNodeToBranch(const std::string& nodeId, const std::string& branchId);

    [[nodiscard]] const TalentBranch* GetBranch(const std::string& branchId) const;
    [[nodiscard]] std::vector<const TalentBranch*> GetAllBranches() const;

    // =========================================================================
    // Age Gate Operations
    // =========================================================================

    void SetAgeGate(int age, const std::vector<std::string>& nodeIds, int bonusPoints);
    [[nodiscard]] std::vector<std::string> GetNodesForAge(int age) const;

    // =========================================================================
    // Selection
    // =========================================================================

    void SelectNode(const std::string& nodeId);
    void DeselectNode();
    [[nodiscard]] const TalentNode* GetSelectedNode() const;
    [[nodiscard]] std::string GetSelectedNodeId() const { return m_state.selectedNodeId; }

    // =========================================================================
    // View Control
    // =========================================================================

    void SetViewOffset(float x, float y);
    void SetZoomLevel(float zoom);
    void CenterView();
    void ZoomToFit();

    [[nodiscard]] float GetViewOffsetX() const { return m_state.viewOffsetX; }
    [[nodiscard]] float GetViewOffsetY() const { return m_state.viewOffsetY; }
    [[nodiscard]] float GetZoomLevel() const { return m_state.zoomLevel; }

    // =========================================================================
    // Preview
    // =========================================================================

    void SetPreviewAge(int age);
    [[nodiscard]] int GetPreviewAge() const { return m_state.currentAge; }
    [[nodiscard]] std::vector<const TalentNode*> GetAvailableNodesAtAge(int age) const;

    // =========================================================================
    // Validation
    // =========================================================================

    [[nodiscard]] bool Validate() const;
    [[nodiscard]] std::vector<std::string> GetValidationErrors() const;
    [[nodiscard]] bool HasCircularDependency() const;

    // =========================================================================
    // UI State
    // =========================================================================

    TalentEditorState& GetState() { return m_state; }
    void SetShowGrid(bool show) { m_state.showGrid = show; }
    void SetShowConnections(bool show) { m_state.showConnections = show; }
    void SetShowAgeMarkers(bool show) { m_state.showAgeMarkers = show; }

    // =========================================================================
    // Rendering
    // =========================================================================

    void Render();
    void RenderNode(const TalentNode& node);
    void RenderConnection(const EditorConnection& connection);
    void RenderBranch(const TalentBranch& branch);
    void RenderAgeMarker(int age);
    void RenderGrid();
    void RenderNodeProperties();

    // =========================================================================
    // Input Handling
    // =========================================================================

    void OnMouseDown(float x, float y, int button);
    void OnMouseUp(float x, float y, int button);
    void OnMouseMove(float x, float y);
    void OnMouseWheel(float delta);
    void OnKeyDown(int key);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnNodeSelect(NodeSelectCallback callback) { m_onNodeSelect = std::move(callback); }
    void SetOnTreeModified(TreeModifiedCallback callback) { m_onTreeModified = std::move(callback); }

    // =========================================================================
    // Export/Import
    // =========================================================================

    [[nodiscard]] std::string ExportToJson() const;
    bool ImportFromJson(const std::string& json);

private:
    bool m_initialized = false;
    TalentTreeDefinition m_tree;
    TalentEditorState m_state;
    std::vector<EditorConnection> m_connections;

    NodeSelectCallback m_onNodeSelect;
    TreeModifiedCallback m_onTreeModified;

    void MarkModified();
    void RebuildConnections();
    [[nodiscard]] std::pair<float, float> ScreenToWorld(float screenX, float screenY) const;
    [[nodiscard]] std::pair<float, float> WorldToScreen(float worldX, float worldY) const;
};

// ============================================================================
// HTML Bridge
// ============================================================================

class TalentTreeEditorHTMLBridge {
public:
    [[nodiscard]] static TalentTreeEditorHTMLBridge& Instance();

    void Initialize(TalentTreeEditor* editor);

    [[nodiscard]] std::string GetTreeJson() const;
    void SetTreeJson(const std::string& json);
    [[nodiscard]] std::string GetNodesJson() const;
    [[nodiscard]] std::string GetConnectionsJson() const;
    [[nodiscard]] std::string GetSelectedNodeJson() const;

    void SelectNode(const std::string& nodeId);
    void AddNode(const std::string& nodeJson);
    void UpdateNode(const std::string& nodeJson);
    void RemoveNode(const std::string& nodeId);
    void MoveNode(const std::string& nodeId, int x, int y);
    void AddConnection(const std::string& fromId, const std::string& toId);
    void RemoveConnection(const std::string& fromId, const std::string& toId);

private:
    TalentTreeEditorHTMLBridge() = default;
    TalentTreeEditor* m_editor = nullptr;
};

} // namespace Editor
} // namespace Vehement
