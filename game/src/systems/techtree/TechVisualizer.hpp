#pragma once

/**
 * @file TechVisualizer.hpp
 * @brief Data structures and utilities for tech tree UI visualization
 *
 * Provides:
 * - Node position calculations for graph layout
 * - Connection line generation
 * - Progress visualization data
 * - Highlight path calculation
 * - Layout algorithms (tree, grid, radial)
 */

#include "TechTree.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace TechTree {

// ============================================================================
// Visual Node
// ============================================================================

/**
 * @brief Visual representation of a tech node for UI
 */
struct VisualNode {
    std::string techId;
    std::string name;
    std::string icon;
    std::string description;

    glm::vec2 position{0.0f};           ///< Position in UI space
    glm::vec2 size{100.0f, 80.0f};      ///< Node size

    ResearchStatus status = ResearchStatus::Locked;
    float progress = 0.0f;              ///< Research progress (0.0-1.0)

    TechCategory category = TechCategory::Military;
    TechAge age = TechAge::Stone;
    int tier = 1;

    bool isHighlighted = false;         ///< Part of highlighted path
    bool isSelected = false;            ///< Currently selected
    bool isHovered = false;             ///< Mouse hovering

    // Visual state
    glm::vec4 backgroundColor{0.2f, 0.2f, 0.2f, 1.0f};
    glm::vec4 borderColor{0.5f, 0.5f, 0.5f, 1.0f};
    glm::vec4 textColor{1.0f, 1.0f, 1.0f, 1.0f};

    [[nodiscard]] nlohmann::json ToJson() const;
    static VisualNode FromJson(const nlohmann::json& j);
};

// ============================================================================
// Visual Connection
// ============================================================================

/**
 * @brief Visual representation of a connection between nodes
 */
struct VisualConnection {
    std::string fromTech;
    std::string toTech;

    glm::vec2 startPoint{0.0f};
    glm::vec2 endPoint{0.0f};
    std::vector<glm::vec2> controlPoints;   ///< For curved lines

    bool isHighlighted = false;
    bool isRequired = true;

    float thickness = 2.0f;
    glm::vec4 color{0.5f, 0.5f, 0.5f, 1.0f};

    // Arrow at end
    bool hasArrow = true;
    float arrowSize = 8.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static VisualConnection FromJson(const nlohmann::json& j);
};

// ============================================================================
// Layout Type
// ============================================================================

/**
 * @brief Layout algorithm type for tech tree visualization
 */
enum class LayoutType : uint8_t {
    Tree,           ///< Traditional tree layout (top-to-bottom or left-to-right)
    Grid,           ///< Grid-based layout by tier/category
    Radial,         ///< Radial/circular layout from center
    Force,          ///< Force-directed graph layout
    Custom,         ///< Uses positions defined in tech nodes

    COUNT
};

[[nodiscard]] inline const char* LayoutTypeToString(LayoutType type) {
    switch (type) {
        case LayoutType::Tree:   return "tree";
        case LayoutType::Grid:   return "grid";
        case LayoutType::Radial: return "radial";
        case LayoutType::Force:  return "force";
        case LayoutType::Custom: return "custom";
        default:                 return "unknown";
    }
}

// ============================================================================
// Layout Settings
// ============================================================================

/**
 * @brief Settings for tech tree layout
 */
struct LayoutSettings {
    LayoutType type = LayoutType::Tree;

    // Spacing
    float nodeWidth = 120.0f;
    float nodeHeight = 80.0f;
    float horizontalSpacing = 60.0f;
    float verticalSpacing = 100.0f;
    float tierSpacing = 150.0f;

    // Margins
    float marginLeft = 50.0f;
    float marginTop = 50.0f;
    float marginRight = 50.0f;
    float marginBottom = 50.0f;

    // Tree layout
    bool treeTopToBottom = true;        ///< true = top-to-bottom, false = left-to-right

    // Grid layout
    int gridColumns = 5;                ///< Max nodes per row in grid layout
    bool groupByCategory = true;        ///< Group nodes by category in grid

    // Radial layout
    float radialStartRadius = 100.0f;
    float radialRadiusIncrement = 150.0f;

    // Connection style
    bool curvedConnections = true;
    float connectionCurveStrength = 0.5f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static LayoutSettings FromJson(const nlohmann::json& j);
};

// ============================================================================
// Highlight Path
// ============================================================================

/**
 * @brief A path through the tech tree (e.g., prerequisites to a target)
 */
struct HighlightPath {
    std::string targetTech;             ///< Target of the path
    std::vector<std::string> techs;     ///< Techs in the path (in order)
    std::vector<std::pair<std::string, std::string>> connections;   ///< Connections to highlight

    glm::vec4 highlightColor{1.0f, 0.8f, 0.2f, 1.0f};

    [[nodiscard]] bool Contains(const std::string& techId) const;
    [[nodiscard]] bool ContainsConnection(const std::string& from, const std::string& to) const;
};

// ============================================================================
// Tech Tree Visualizer
// ============================================================================

/**
 * @brief Generates and manages visual data for tech tree UI
 *
 * Example usage:
 * @code
 * TechTreeVisualizer visualizer;
 * visualizer.Initialize(&treeDef, &playerTree);
 *
 * // Generate layout
 * visualizer.SetLayoutSettings(settings);
 * visualizer.GenerateLayout();
 *
 * // Get visual data for rendering
 * const auto& nodes = visualizer.GetVisualNodes();
 * const auto& connections = visualizer.GetVisualConnections();
 *
 * // Highlight path to a tech
 * visualizer.HighlightPathTo("tech_iron_weapons");
 *
 * // Update each frame
 * visualizer.Update(deltaTime);
 * @endcode
 */
class TechTreeVisualizer {
public:
    TechTreeVisualizer() = default;
    ~TechTreeVisualizer() = default;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize with tech tree definition and player state
     */
    void Initialize(const TechTreeDef* treeDef, const PlayerTechTree* playerTree);

    /**
     * @brief Update the visualizer with new player state
     */
    void SetPlayerTree(const PlayerTechTree* playerTree);

    /**
     * @brief Clear all visual data
     */
    void Clear();

    // =========================================================================
    // Layout
    // =========================================================================

    /**
     * @brief Set layout settings
     */
    void SetLayoutSettings(const LayoutSettings& settings);

    /**
     * @brief Get current layout settings
     */
    [[nodiscard]] const LayoutSettings& GetLayoutSettings() const { return m_layoutSettings; }

    /**
     * @brief Generate/regenerate the layout
     */
    void GenerateLayout();

    /**
     * @brief Get total bounds of the laid out tree
     */
    [[nodiscard]] glm::vec4 GetBounds() const { return m_bounds; }

    // =========================================================================
    // Visual Data Access
    // =========================================================================

    /**
     * @brief Get all visual nodes
     */
    [[nodiscard]] const std::vector<VisualNode>& GetVisualNodes() const { return m_visualNodes; }

    /**
     * @brief Get all visual connections
     */
    [[nodiscard]] const std::vector<VisualConnection>& GetVisualConnections() const { return m_visualConnections; }

    /**
     * @brief Get visual node by tech ID
     */
    [[nodiscard]] const VisualNode* GetVisualNode(const std::string& techId) const;

    /**
     * @brief Get visual node by tech ID (mutable)
     */
    [[nodiscard]] VisualNode* GetVisualNodeMutable(const std::string& techId);

    /**
     * @brief Get nodes in a specific category
     */
    [[nodiscard]] std::vector<const VisualNode*> GetNodesInCategory(TechCategory category) const;

    /**
     * @brief Get nodes in a specific tier
     */
    [[nodiscard]] std::vector<const VisualNode*> GetNodesInTier(int tier) const;

    /**
     * @brief Get nodes with a specific status
     */
    [[nodiscard]] std::vector<const VisualNode*> GetNodesByStatus(ResearchStatus status) const;

    // =========================================================================
    // State Updates
    // =========================================================================

    /**
     * @brief Update visual state from player tech tree
     */
    void UpdateFromPlayerState();

    /**
     * @brief Update animation state
     */
    void Update(float deltaTime);

    /**
     * @brief Set node selection
     */
    void SelectNode(const std::string& techId);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected node ID
     */
    [[nodiscard]] const std::string& GetSelectedNodeId() const { return m_selectedNodeId; }

    /**
     * @brief Set hovered node
     */
    void SetHoveredNode(const std::string& techId);

    // =========================================================================
    // Path Highlighting
    // =========================================================================

    /**
     * @brief Highlight the path to a target tech
     */
    void HighlightPathTo(const std::string& techId);

    /**
     * @brief Highlight a custom path
     */
    void HighlightPath(const HighlightPath& path);

    /**
     * @brief Clear highlighted path
     */
    void ClearHighlight();

    /**
     * @brief Get current highlighted path
     */
    [[nodiscard]] const HighlightPath& GetHighlightedPath() const { return m_highlightedPath; }

    // =========================================================================
    // Hit Testing
    // =========================================================================

    /**
     * @brief Find node at a position
     * @return Tech ID of node at position, or empty string if none
     */
    [[nodiscard]] std::string HitTest(const glm::vec2& position) const;

    /**
     * @brief Find connection at a position
     * @return Pair of tech IDs (from, to) or empty strings if none
     */
    [[nodiscard]] std::pair<std::string, std::string> HitTestConnection(const glm::vec2& position, float tolerance = 5.0f) const;

    // =========================================================================
    // Colors
    // =========================================================================

    /**
     * @brief Get color for a research status
     */
    [[nodiscard]] glm::vec4 GetStatusColor(ResearchStatus status) const;

    /**
     * @brief Get color for a category
     */
    [[nodiscard]] glm::vec4 GetCategoryColor(TechCategory category) const;

    /**
     * @brief Set color scheme
     */
    void SetStatusColor(ResearchStatus status, const glm::vec4& color);
    void SetCategoryColor(TechCategory category, const glm::vec4& color);

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Export visual data to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Import visual data from JSON
     */
    void FromJson(const nlohmann::json& j);

private:
    // Layout algorithms
    void LayoutTree();
    void LayoutGrid();
    void LayoutRadial();
    void LayoutForce();
    void LayoutCustom();

    void GenerateConnections();
    void CalculateBounds();
    void UpdateNodeColors();

    // Bezier curve helpers
    glm::vec2 CalculateBezierPoint(const glm::vec2& start, const glm::vec2& end,
                                    const glm::vec2& control, float t) const;
    std::vector<glm::vec2> GenerateCurvePoints(const glm::vec2& start, const glm::vec2& end,
                                                 float curveStrength) const;

    // Data
    const TechTreeDef* m_treeDef = nullptr;
    const PlayerTechTree* m_playerTree = nullptr;

    LayoutSettings m_layoutSettings;

    std::vector<VisualNode> m_visualNodes;
    std::vector<VisualConnection> m_visualConnections;
    std::unordered_map<std::string, size_t> m_nodeIndex;     // techId -> index in m_visualNodes

    // State
    std::string m_selectedNodeId;
    std::string m_hoveredNodeId;
    HighlightPath m_highlightedPath;
    glm::vec4 m_bounds{0.0f};   // x, y, width, height

    // Color schemes
    std::unordered_map<ResearchStatus, glm::vec4> m_statusColors;
    std::unordered_map<TechCategory, glm::vec4> m_categoryColors;

    // Animation state
    float m_animationTime = 0.0f;
};

// ============================================================================
// Mini-map Generator
// ============================================================================

/**
 * @brief Generates minimap data for tech tree overview
 */
class TechTreeMinimap {
public:
    struct MinimapNode {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec4 color;
        ResearchStatus status;
    };

    void Generate(const TechTreeVisualizer& visualizer, const glm::vec2& targetSize);

    [[nodiscard]] const std::vector<MinimapNode>& GetNodes() const { return m_nodes; }
    [[nodiscard]] glm::vec2 GetScale() const { return m_scale; }
    [[nodiscard]] glm::vec2 GetOffset() const { return m_offset; }

    /**
     * @brief Convert minimap position to tree position
     */
    [[nodiscard]] glm::vec2 MinimapToTree(const glm::vec2& minimapPos) const;

    /**
     * @brief Convert tree position to minimap position
     */
    [[nodiscard]] glm::vec2 TreeToMinimap(const glm::vec2& treePos) const;

private:
    std::vector<MinimapNode> m_nodes;
    glm::vec2 m_scale{1.0f};
    glm::vec2 m_offset{0.0f};
    glm::vec2 m_targetSize{200.0f, 150.0f};
};

// ============================================================================
// Progress Indicator Data
// ============================================================================

/**
 * @brief Data for research progress UI indicators
 */
struct ProgressIndicatorData {
    std::string techId;
    std::string techName;
    std::string techIcon;

    float progress = 0.0f;          ///< 0.0 - 1.0
    float remainingTime = 0.0f;     ///< Seconds remaining
    float totalTime = 0.0f;         ///< Total research time

    bool isCurrentResearch = false;
    bool isQueued = false;
    int queuePosition = 0;

    [[nodiscard]] std::string GetProgressText() const;
    [[nodiscard]] std::string GetTimeRemainingText() const;
};

/**
 * @brief Get progress indicator data for current and queued research
 */
[[nodiscard]] std::vector<ProgressIndicatorData> GetProgressIndicators(
    const PlayerTechTree& tree, const TechTreeDef& treeDef);

} // namespace TechTree
} // namespace Vehement
