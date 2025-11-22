#pragma once

#include "engine/animation/AnimationBlendTree.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Vehement {

using Nova::json;
using Nova::BlendTree;
using Nova::BlendNode;
using Nova::AnimationMask;

/**
 * @brief Visual node in the blend tree editor
 */
struct BlendTreeNode {
    std::string id;
    std::string name;
    std::string type;  // "root", "1d", "2d", "clip", "additive"
    glm::vec2 position{0.0f};
    glm::vec2 size{180.0f, 100.0f};
    bool selected = false;
    bool expanded = true;
    uint32_t color = 0x44AA88FF;

    // For blend nodes
    std::string parameter;
    std::string parameterX;
    std::string parameterY;

    // For clip nodes
    std::string clipPath;
    float threshold = 0.0f;
    glm::vec2 position2D{0.0f};

    // Children
    std::vector<std::string> childIds;
};

/**
 * @brief Connection between blend tree nodes
 */
struct BlendTreeConnection {
    std::string parentId;
    std::string childId;
    bool selected = false;
};

/**
 * @brief Parameter slider state
 */
struct ParameterSlider {
    std::string name;
    float value = 0.0f;
    float minValue = -1.0f;
    float maxValue = 1.0f;
    bool isDragging = false;
};

/**
 * @brief Editor action for undo/redo
 */
struct BlendTreeEditorAction {
    enum class Type {
        AddNode,
        RemoveNode,
        MoveNode,
        ModifyNode,
        AddConnection,
        RemoveConnection,
        ModifyParameter
    };

    Type type;
    json beforeData;
    json afterData;
    std::string targetId;
};

/**
 * @brief 2D blend space visualization point
 */
struct BlendSpacePoint {
    std::string clipName;
    glm::vec2 position;
    float weight = 0.0f;
    bool selected = false;
};

/**
 * @brief Visual blend tree editor
 *
 * Features:
 * - Tree node structure visualization
 * - Parameter sliders for live adjustment
 * - Live preview with skeleton
 * - 2D blend space visualization
 * - Undo/redo support
 */
class BlendTreeEditor {
public:
    struct Config {
        glm::vec2 gridSize{20.0f};
        bool snapToGrid = true;
        bool showGrid = true;
        float zoomMin = 0.25f;
        float zoomMax = 4.0f;
        glm::vec2 canvasSize{2000.0f, 2000.0f};
        glm::vec2 blendSpaceSize{300.0f, 300.0f};
    };

    BlendTreeEditor();
    ~BlendTreeEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Load blend tree for editing
     */
    bool LoadBlendTree(const std::string& filepath);
    bool LoadBlendTree(BlendTree* blendTree);

    /**
     * @brief Save blend tree
     */
    bool SaveBlendTree(const std::string& filepath);
    bool SaveBlendTree();

    /**
     * @brief Create new blend tree
     */
    void NewBlendTree(const std::string& type = "simple_1d");

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ExportToJson() const;

    /**
     * @brief Import from JSON
     */
    bool ImportFromJson(const json& data);

    // -------------------------------------------------------------------------
    // Node Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add a new node
     */
    BlendTreeNode* AddNode(const std::string& type, const glm::vec2& position);

    /**
     * @brief Add a clip node
     */
    BlendTreeNode* AddClipNode(const std::string& clipPath, const glm::vec2& position);

    /**
     * @brief Remove node by ID
     */
    bool RemoveNode(const std::string& id);

    /**
     * @brief Get node by ID
     */
    [[nodiscard]] BlendTreeNode* GetNode(const std::string& id);

    /**
     * @brief Get all nodes
     */
    [[nodiscard]] const std::vector<BlendTreeNode>& GetNodes() const { return m_nodes; }

    /**
     * @brief Connect nodes
     */
    bool ConnectNodes(const std::string& parentId, const std::string& childId);

    /**
     * @brief Disconnect nodes
     */
    bool DisconnectNodes(const std::string& parentId, const std::string& childId);

    /**
     * @brief Get all connections
     */
    [[nodiscard]] const std::vector<BlendTreeConnection>& GetConnections() const {
        return m_connections;
    }

    /**
     * @brief Set root node
     */
    void SetRootNode(const std::string& id);

    /**
     * @brief Get root node ID
     */
    [[nodiscard]] const std::string& GetRootNodeId() const { return m_rootNodeId; }

    // -------------------------------------------------------------------------
    // Parameter Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add parameter
     */
    void AddParameter(const std::string& name, float defaultValue = 0.0f,
                      float minValue = -1.0f, float maxValue = 1.0f);

    /**
     * @brief Remove parameter
     */
    bool RemoveParameter(const std::string& name);

    /**
     * @brief Set parameter value
     */
    void SetParameterValue(const std::string& name, float value);

    /**
     * @brief Get parameter value
     */
    [[nodiscard]] float GetParameterValue(const std::string& name) const;

    /**
     * @brief Get all parameters
     */
    [[nodiscard]] const std::vector<ParameterSlider>& GetParameters() const {
        return m_parameters;
    }

    // -------------------------------------------------------------------------
    // Blend Space
    // -------------------------------------------------------------------------

    /**
     * @brief Get blend space points for 2D visualization
     */
    [[nodiscard]] std::vector<BlendSpacePoint> GetBlendSpacePoints() const;

    /**
     * @brief Get current blend position
     */
    [[nodiscard]] glm::vec2 GetCurrentBlendPosition() const;

    /**
     * @brief Get blend weights for all clips
     */
    [[nodiscard]] std::vector<std::pair<std::string, float>> GetBlendWeights() const;

    /**
     * @brief Set blend position (for 2D blend trees)
     */
    void SetBlendPosition(const glm::vec2& position);

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Select node
     */
    void SelectNode(const std::string& id);

    /**
     * @brief Select connection
     */
    void SelectConnection(const std::string& parentId, const std::string& childId);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected node ID
     */
    [[nodiscard]] const std::string& GetSelectedNodeId() const { return m_selectedNodeId; }

    // -------------------------------------------------------------------------
    // View Control
    // -------------------------------------------------------------------------

    /**
     * @brief Set view offset
     */
    void SetViewOffset(const glm::vec2& offset);

    /**
     * @brief Get view offset
     */
    [[nodiscard]] const glm::vec2& GetViewOffset() const { return m_viewOffset; }

    /**
     * @brief Set zoom level
     */
    void SetZoom(float zoom);

    /**
     * @brief Get zoom level
     */
    [[nodiscard]] float GetZoom() const { return m_zoom; }

    /**
     * @brief Zoom to fit all nodes
     */
    void ZoomToFit();

    /**
     * @brief Center view on node
     */
    void CenterOnNode(const std::string& id);

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle mouse click
     */
    void OnMouseDown(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse release
     */
    void OnMouseUp(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse move
     */
    void OnMouseMove(const glm::vec2& position);

    /**
     * @brief Handle key press
     */
    void OnKeyDown(int key);

    /**
     * @brief Handle scroll (for parameter sliders)
     */
    void OnScroll(float delta);

    // -------------------------------------------------------------------------
    // Undo/Redo
    // -------------------------------------------------------------------------

    /**
     * @brief Undo last action
     */
    void Undo();

    /**
     * @brief Redo last undone action
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------

    /**
     * @brief Auto-arrange nodes in tree layout
     */
    void AutoLayout();

    /**
     * @brief Collapse node and its children
     */
    void CollapseNode(const std::string& id);

    /**
     * @brief Expand node and its children
     */
    void ExpandNode(const std::string& id);

    // -------------------------------------------------------------------------
    // Preview
    // -------------------------------------------------------------------------

    /**
     * @brief Start live preview
     */
    void StartPreview();

    /**
     * @brief Stop preview
     */
    void StopPreview();

    /**
     * @brief Update preview
     */
    void UpdatePreview(float deltaTime);

    /**
     * @brief Check if preview is active
     */
    [[nodiscard]] bool IsPreviewActive() const { return m_previewActive; }

    /**
     * @brief Get current preview pose
     */
    [[nodiscard]] const std::vector<glm::mat4>& GetPreviewPose() const { return m_previewPose; }

    // -------------------------------------------------------------------------
    // Animation Mask
    // -------------------------------------------------------------------------

    /**
     * @brief Set animation mask for selected node
     */
    void SetNodeMask(const std::string& nodeId, const AnimationMask& mask);

    /**
     * @brief Get animation mask for node
     */
    [[nodiscard]] AnimationMask GetNodeMask(const std::string& nodeId) const;

    /**
     * @brief Show mask editor
     */
    void ShowMaskEditor(const std::string& nodeId);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Callback when selection changes
     */
    std::function<void(const std::string&)> OnSelectionChanged;

    /**
     * @brief Callback when blend tree is modified
     */
    std::function<void()> OnModified;

    /**
     * @brief Callback when parameter changes
     */
    std::function<void(const std::string&, float)> OnParameterChanged;

    /**
     * @brief Callback to request node details panel update
     */
    std::function<void(const BlendTreeNode*)> OnNodeSelected;

    // -------------------------------------------------------------------------
    // Dirty State
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }

private:
    void RecordAction(BlendTreeEditorAction::Type type, const std::string& target,
                      const json& before, const json& after);
    BlendTreeNode* FindNodeAt(const glm::vec2& position);
    BlendTreeConnection* FindConnectionAt(const glm::vec2& position);
    glm::vec2 ScreenToWorld(const glm::vec2& screenPos) const;
    glm::vec2 WorldToScreen(const glm::vec2& worldPos) const;
    glm::vec2 SnapToGrid(const glm::vec2& position) const;
    std::string GenerateNodeId();
    void UpdateNodeFromBlendTree(BlendTreeNode& node);
    void CalculateBlendWeights();
    void LayoutSubtree(const std::string& nodeId, float x, float& y, float levelHeight);

    Config m_config;
    BlendTree* m_blendTree = nullptr;
    std::string m_filePath;

    // Visual elements
    std::vector<BlendTreeNode> m_nodes;
    std::vector<BlendTreeConnection> m_connections;
    std::string m_rootNodeId;

    // Parameters
    std::vector<ParameterSlider> m_parameters;

    // Selection
    std::string m_selectedNodeId;
    std::string m_selectedConnectionParent;
    std::string m_selectedConnectionChild;

    // View
    glm::vec2 m_viewOffset{0.0f};
    float m_zoom = 1.0f;

    // Interaction state
    bool m_dragging = false;
    bool m_panning = false;
    bool m_creatingConnection = false;
    glm::vec2 m_dragStart{0.0f};
    glm::vec2 m_dragOffset{0.0f};
    std::string m_connectionStartNode;

    // Undo/Redo
    std::vector<BlendTreeEditorAction> m_undoStack;
    std::vector<BlendTreeEditorAction> m_redoStack;
    static constexpr size_t MAX_UNDO_SIZE = 100;

    // Preview
    bool m_previewActive = false;
    float m_previewTime = 0.0f;
    std::vector<glm::mat4> m_previewPose;
    std::vector<std::pair<std::string, float>> m_currentWeights;

    // Node ID counter
    uint32_t m_nodeIdCounter = 0;

    // State
    bool m_dirty = false;
    bool m_initialized = false;
};

/**
 * @brief Node properties panel for blend tree editor
 */
class BlendTreeNodePropertiesPanel {
public:
    BlendTreeNodePropertiesPanel() = default;

    /**
     * @brief Set node to edit
     */
    void SetNode(BlendTreeNode* node);

    /**
     * @brief Render panel (returns true if modified)
     */
    bool Render();

    /**
     * @brief Get modified node data
     */
    [[nodiscard]] BlendTreeNode GetModifiedNode() const;

    std::function<void(const BlendTreeNode&)> OnNodeModified;

private:
    BlendTreeNode* m_node = nullptr;
    BlendTreeNode m_editNode;
};

/**
 * @brief Parameter panel for blend tree editor
 */
class BlendTreeParameterPanel {
public:
    BlendTreeParameterPanel() = default;

    /**
     * @brief Set parameters to display
     */
    void SetParameters(std::vector<ParameterSlider>* parameters);

    /**
     * @brief Render panel
     */
    void Render();

    /**
     * @brief Handle slider drag
     */
    void OnSliderDrag(const std::string& name, float value);

    std::function<void(const std::string&, float)> OnParameterChanged;

private:
    std::vector<ParameterSlider>* m_parameters = nullptr;
    std::string m_draggingParameter;
};

/**
 * @brief 2D blend space visualization panel
 */
class BlendSpacePanel {
public:
    BlendSpacePanel() = default;

    /**
     * @brief Initialize with size
     */
    void Initialize(const glm::vec2& size);

    /**
     * @brief Set blend space points
     */
    void SetPoints(const std::vector<BlendSpacePoint>& points);

    /**
     * @brief Set current blend position
     */
    void SetCurrentPosition(const glm::vec2& position);

    /**
     * @brief Render panel
     */
    void Render();

    /**
     * @brief Handle mouse click for position setting
     */
    void OnMouseDown(const glm::vec2& position, int button);
    void OnMouseMove(const glm::vec2& position);
    void OnMouseUp(const glm::vec2& position, int button);

    std::function<void(const glm::vec2&)> OnPositionChanged;
    std::function<void(const std::string&)> OnPointSelected;

private:
    glm::vec2 m_size{300.0f, 300.0f};
    std::vector<BlendSpacePoint> m_points;
    glm::vec2 m_currentPosition{0.0f};
    bool m_dragging = false;
    std::string m_selectedPoint;
};

} // namespace Vehement
