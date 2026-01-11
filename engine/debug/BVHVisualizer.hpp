#pragma once

/**
 * @file BVHVisualizer.hpp
 * @brief Debug visualization for BVH (Bounding Volume Hierarchy) structures
 *
 * Provides comprehensive debug rendering capabilities for SDF BVH structures,
 * including wireframe bounds, ray traversal visualization, heat maps, and
 * interactive node inspection.
 *
 * @section bvhvis_features Features
 * - Render node bounds as wireframe boxes
 * - Color coding by depth level
 * - Highlight leaf nodes vs internal nodes
 * - Show primitive bounds within leaves
 * - Ray visualization (show traversal path)
 * - Statistics overlay (nodes visited, primitives tested)
 * - Interactive: click to expand/collapse nodes
 * - Heat map: show node visit frequency
 *
 * @section bvhvis_usage Usage Example
 *
 * @code{.cpp}
 * Nova::BVHVisualizer visualizer;
 *
 * // Configure visualization
 * Nova::VisualizationOptions options;
 * options.showLeaves = true;
 * options.maxDepth = 5;
 * options.colorMode = Nova::BVHColorMode::Depth;
 *
 * // Render BVH structure
 * visualizer.Render(camera, sdfBvh, options);
 *
 * // Visualize ray traversal
 * Nova::Ray ray{origin, direction};
 * auto result = sdfBvh.Traverse(ray, 100.0f);
 * visualizer.SetTraversalData(ray, result);
 * visualizer.RenderTraversal(camera, sdfBvh, options);
 * @endcode
 */

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdint>
#include <string>
#include <memory>

namespace Nova {

// Forward declarations
class Camera;
class SDFBVH;
struct SDFBVHNode;
struct SDFBVHTraversalResult;
struct Ray;
struct AABB;

/**
 * @brief Color modes for BVH visualization
 */
enum class BVHColorMode {
    Depth,          ///< Color by node depth (gradient from root to leaves)
    NodeType,       ///< Different colors for internal nodes vs leaves
    HeatMap,        ///< Color by visit frequency (requires traversal data)
    PrimitiveCount, ///< Color by number of primitives in leaves
    SAHCost,        ///< Color by estimated SAH cost contribution
    Custom          ///< User-defined color function
};

/**
 * @brief Options for controlling BVH visualization
 */
struct VisualizationOptions {
    // =========================================================================
    // Visibility Controls
    // =========================================================================

    bool enabled = true;              ///< Master enable/disable for visualization
    bool showInternalNodes = true;    ///< Show internal (non-leaf) nodes
    bool showLeaves = true;           ///< Show leaf nodes
    bool showPrimitiveBounds = false; ///< Show bounds of primitives within leaves
    bool showRootOnly = false;        ///< Only show the root node bounds
    bool showStatistics = true;       ///< Show statistics overlay
    bool showRayPath = true;          ///< Show ray traversal path when available
    bool showHitPoints = true;        ///< Show ray-AABB intersection points

    // =========================================================================
    // Depth Controls
    // =========================================================================

    int minDepth = 0;                 ///< Minimum depth to render (0 = root)
    int maxDepth = -1;                ///< Maximum depth to render (-1 = unlimited)

    // =========================================================================
    // Color Settings
    // =========================================================================

    BVHColorMode colorMode = BVHColorMode::Depth;

    /// Color for internal nodes (when using NodeType mode)
    glm::vec4 internalNodeColor{0.2f, 0.6f, 1.0f, 0.5f};

    /// Color for leaf nodes (when using NodeType mode)
    glm::vec4 leafNodeColor{0.2f, 1.0f, 0.2f, 0.7f};

    /// Color for primitive bounds
    glm::vec4 primitiveColor{1.0f, 0.8f, 0.2f, 0.4f};

    /// Color for selected/highlighted nodes
    glm::vec4 highlightColor{1.0f, 1.0f, 0.0f, 1.0f};

    /// Color for ray visualization
    glm::vec4 rayColor{1.0f, 0.0f, 0.0f, 1.0f};

    /// Color for ray hit points
    glm::vec4 hitPointColor{0.0f, 1.0f, 1.0f, 1.0f};

    /// Start color for depth gradient
    glm::vec4 depthColorStart{0.0f, 0.5f, 1.0f, 0.8f};

    /// End color for depth gradient
    glm::vec4 depthColorEnd{1.0f, 0.0f, 0.5f, 0.8f};

    /// Cold color for heat map (low visit count)
    glm::vec4 heatMapCold{0.0f, 0.0f, 1.0f, 0.6f};

    /// Hot color for heat map (high visit count)
    glm::vec4 heatMapHot{1.0f, 0.0f, 0.0f, 1.0f};

    // =========================================================================
    // Line Settings
    // =========================================================================

    float lineWidth = 1.0f;           ///< Width of wireframe lines
    float highlightLineWidth = 3.0f;  ///< Width for highlighted elements
    float rayLineWidth = 2.0f;        ///< Width for ray visualization
    float hitPointSize = 8.0f;        ///< Size of hit point markers

    // =========================================================================
    // Filtering
    // =========================================================================

    /// Only show nodes that were visited in last traversal
    bool showOnlyVisited = false;

    /// Only show nodes containing specific primitive IDs
    std::vector<uint32_t> filterPrimitives;

    /// Custom node filter function (return true to show node)
    std::function<bool(uint32_t nodeIndex, const SDFBVHNode& node)> customFilter;

    // =========================================================================
    // Interactive Options
    // =========================================================================

    /// Enable interactive node selection
    bool enableInteraction = false;

    /// Currently selected node index (-1 for none)
    int32_t selectedNode = -1;

    /// Set of collapsed nodes (not rendered with children)
    std::unordered_set<uint32_t> collapsedNodes;

    // =========================================================================
    // Performance Options
    // =========================================================================

    /// Maximum nodes to render per frame (for large BVHs)
    uint32_t maxNodesPerFrame = 10000;

    /// Use frustum culling for node visibility
    bool useFrustumCulling = true;

    /// LOD: skip nodes smaller than this screen percentage
    float minScreenSizePercent = 0.0f;
};

/**
 * @brief Statistics collected during BVH visualization
 */
struct BVHVisualizationStats {
    uint32_t totalNodes = 0;         ///< Total nodes in BVH
    uint32_t renderedNodes = 0;      ///< Nodes actually rendered
    uint32_t culledNodes = 0;        ///< Nodes culled by frustum/LOD
    uint32_t leafNodes = 0;          ///< Number of leaf nodes rendered
    uint32_t internalNodes = 0;      ///< Number of internal nodes rendered
    uint32_t maxDepthReached = 0;    ///< Maximum depth encountered
    uint32_t primitivesShown = 0;    ///< Primitives rendered (if enabled)

    // Ray traversal statistics (when SetTraversalData used)
    uint32_t nodesVisited = 0;       ///< Nodes visited during traversal
    uint32_t primitivesTested = 0;   ///< Primitives tested during traversal
    uint32_t rayBoxTests = 0;        ///< Ray-box intersection tests
    float rayLength = 0.0f;          ///< Length of the ray

    /// Reset all statistics to zero
    void Reset() {
        totalNodes = 0;
        renderedNodes = 0;
        culledNodes = 0;
        leafNodes = 0;
        internalNodes = 0;
        maxDepthReached = 0;
        primitivesShown = 0;
        nodesVisited = 0;
        primitivesTested = 0;
        rayBoxTests = 0;
        rayLength = 0.0f;
    }
};

/**
 * @brief Data structure for tracking ray traversal through BVH
 */
struct TraversalVisualizationData {
    /// Ray used for traversal
    Ray ray;

    /// Maximum distance for the ray
    float maxDistance = 1000.0f;

    /// Indices of nodes visited during traversal (in order)
    std::vector<uint32_t> visitedNodes;

    /// Entry and exit t values for each visited node
    std::vector<std::pair<float, float>> nodeHitTimes;

    /// Primitive indices that were tested
    std::vector<uint32_t> testedPrimitives;

    /// Visit count per node (for heat map)
    std::unordered_map<uint32_t, uint32_t> nodeVisitCounts;

    /// Final hit result (if any)
    bool hasHit = false;
    float hitDistance = 0.0f;
    glm::vec3 hitPoint{0.0f};
    glm::vec3 hitNormal{0.0f, 1.0f, 0.0f};
    uint32_t hitPrimitiveId = 0;

    /// Clear all traversal data
    void Clear() {
        visitedNodes.clear();
        nodeHitTimes.clear();
        testedPrimitives.clear();
        nodeVisitCounts.clear();
        hasHit = false;
        hitDistance = 0.0f;
        hitPoint = glm::vec3(0.0f);
        hitNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        hitPrimitiveId = 0;
    }
};

/**
 * @brief Vertex structure for line rendering
 */
struct LineVertex {
    glm::vec3 position;
    glm::vec4 color;
};

/**
 * @brief Debug visualizer for BVH structures
 *
 * Provides comprehensive visualization capabilities for debugging and
 * analyzing BVH performance. Supports multiple color modes, ray traversal
 * visualization, and interactive node inspection.
 *
 * @section threading Thread Safety
 * - SetTraversalData and Clear are NOT thread-safe
 * - Render operations should be called from the main/render thread
 * - Use external synchronization for concurrent access
 */
class BVHVisualizer {
public:
    /**
     * @brief Default constructor
     */
    BVHVisualizer();

    /**
     * @brief Destructor
     */
    ~BVHVisualizer();

    // Non-copyable, movable
    BVHVisualizer(const BVHVisualizer&) = delete;
    BVHVisualizer& operator=(const BVHVisualizer&) = delete;
    BVHVisualizer(BVHVisualizer&&) noexcept;
    BVHVisualizer& operator=(BVHVisualizer&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize OpenGL resources
     *
     * Must be called before any rendering operations.
     * Safe to call multiple times (idempotent).
     *
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Release all OpenGL resources
     */
    void Shutdown();

    /**
     * @brief Check if visualizer is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render BVH structure visualization
     *
     * Renders wireframe boxes for BVH nodes according to the provided options.
     *
     * @param camera Camera for view/projection matrices
     * @param bvh The SDFBVH structure to visualize
     * @param options Visualization options
     */
    void Render(const Camera& camera, const SDFBVH& bvh, const VisualizationOptions& options);

    /**
     * @brief Render only the ray traversal visualization
     *
     * Renders the ray path and visited nodes. Call SetTraversalData first.
     *
     * @param camera Camera for view/projection matrices
     * @param bvh The SDFBVH structure
     * @param options Visualization options
     */
    void RenderTraversal(const Camera& camera, const SDFBVH& bvh, const VisualizationOptions& options);

    /**
     * @brief Render statistics overlay
     *
     * Renders text overlay with BVH and traversal statistics.
     * Requires a text rendering system to be available.
     *
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     */
    void RenderStatistics(int screenWidth, int screenHeight);

    /**
     * @brief Render a single AABB as wireframe
     *
     * Utility function for rendering individual bounds.
     *
     * @param camera Camera for view/projection matrices
     * @param aabb The AABB to render
     * @param color Line color
     * @param lineWidth Line width
     */
    void RenderAABB(const Camera& camera, const AABB& aabb,
                    const glm::vec4& color, float lineWidth = 1.0f);

    /**
     * @brief Render a ray as a line
     *
     * @param camera Camera for view/projection matrices
     * @param ray The ray to render
     * @param length Length of the ray to draw
     * @param color Line color
     * @param lineWidth Line width
     */
    void RenderRay(const Camera& camera, const Ray& ray, float length,
                   const glm::vec4& color, float lineWidth = 2.0f);

    // =========================================================================
    // Traversal Data
    // =========================================================================

    /**
     * @brief Set traversal data for visualization
     *
     * Records the ray and traversal result for later visualization.
     * Computes visit counts for heat map display.
     *
     * @param ray The ray used for traversal
     * @param result The traversal result from SDFBVH::Traverse
     */
    void SetTraversalData(const Ray& ray, const SDFBVHTraversalResult& result);

    /**
     * @brief Set traversal data with explicit visited nodes
     *
     * Use this when you have custom traversal tracking.
     *
     * @param data Complete traversal visualization data
     */
    void SetTraversalData(const TraversalVisualizationData& data);

    /**
     * @brief Accumulate traversal visit counts (for heat map over multiple rays)
     *
     * Adds visit counts from this traversal to existing counts.
     *
     * @param ray The ray used for traversal
     * @param result The traversal result
     */
    void AccumulateTraversal(const Ray& ray, const SDFBVHTraversalResult& result);

    /**
     * @brief Clear all traversal data
     */
    void ClearTraversalData();

    /**
     * @brief Get current traversal data
     */
    [[nodiscard]] const TraversalVisualizationData& GetTraversalData() const noexcept {
        return m_traversalData;
    }

    // =========================================================================
    // Interactive Features
    // =========================================================================

    /**
     * @brief Handle mouse click for node selection
     *
     * Tests ray from screen position against BVH nodes and selects
     * the frontmost intersected node.
     *
     * @param camera Camera for unprojection
     * @param bvh The BVH to query
     * @param screenPos Screen position of click (pixels)
     * @param screenSize Screen dimensions (pixels)
     * @param options Current options (selectedNode will be updated)
     * @return Index of selected node, or -1 if none hit
     */
    int32_t HandleClick(const Camera& camera, const SDFBVH& bvh,
                        const glm::vec2& screenPos, const glm::vec2& screenSize,
                        VisualizationOptions& options);

    /**
     * @brief Toggle collapse state for a node
     *
     * Collapsed nodes render only their own bounds, not children.
     *
     * @param nodeIndex Index of node to toggle
     * @param options Options to update
     */
    void ToggleNodeCollapse(uint32_t nodeIndex, VisualizationOptions& options);

    /**
     * @brief Expand all collapsed nodes
     */
    void ExpandAll(VisualizationOptions& options);

    /**
     * @brief Collapse all nodes to specified depth
     *
     * @param maxExpandedDepth Maximum depth to keep expanded
     * @param bvh The BVH structure
     * @param options Options to update
     */
    void CollapseToDepth(int maxExpandedDepth, const SDFBVH& bvh, VisualizationOptions& options);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get visualization statistics from last render
     */
    [[nodiscard]] const BVHVisualizationStats& GetStats() const noexcept { return m_stats; }

    /**
     * @brief Reset accumulated heat map data
     */
    void ResetHeatMap();

    /**
     * @brief Get maximum visit count in heat map
     */
    [[nodiscard]] uint32_t GetMaxVisitCount() const noexcept { return m_maxVisitCount; }

    // =========================================================================
    // Color Utilities
    // =========================================================================

    /**
     * @brief Get color for a node based on current options
     *
     * @param nodeIndex Index of the node
     * @param node The node data
     * @param depth Node depth in tree
     * @param maxDepth Maximum depth of tree
     * @param options Visualization options
     * @return Color for the node
     */
    [[nodiscard]] glm::vec4 GetNodeColor(uint32_t nodeIndex, const SDFBVHNode& node,
                                          int depth, int maxDepth,
                                          const VisualizationOptions& options) const;

    /**
     * @brief Set custom color callback for Custom color mode
     *
     * @param callback Function that returns color for a node
     */
    void SetCustomColorCallback(
        std::function<glm::vec4(uint32_t nodeIndex, const SDFBVHNode& node, int depth)> callback);

    // =========================================================================
    // Debug Helpers
    // =========================================================================

    /**
     * @brief Get string description of a node (for debug output)
     */
    [[nodiscard]] std::string GetNodeDescription(const SDFBVH& bvh, uint32_t nodeIndex) const;

    /**
     * @brief Validate BVH structure and report issues
     *
     * Checks for common BVH problems like degenerate nodes,
     * bounds consistency, etc.
     *
     * @param bvh The BVH to validate
     * @return Vector of issue descriptions (empty if valid)
     */
    [[nodiscard]] std::vector<std::string> ValidateBVH(const SDFBVH& bvh) const;

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct RenderBatch {
        std::vector<LineVertex> vertices;
        std::vector<uint32_t> indices;
        float lineWidth = 1.0f;
    };

    // =========================================================================
    // Rendering Helpers
    // =========================================================================

    void BuildNodeGeometry(const SDFBVH& bvh, const VisualizationOptions& options,
                           const Camera& camera);

    void TraverseForRender(const SDFBVH& bvh, uint32_t nodeIndex, int depth,
                           const VisualizationOptions& options, const Camera& camera);

    void AddAABBToBuffer(const AABB& aabb, const glm::vec4& color, RenderBatch& batch);

    void AddLineToBuffer(const glm::vec3& start, const glm::vec3& end,
                         const glm::vec4& color, RenderBatch& batch);

    void AddPointToBuffer(const glm::vec3& point, const glm::vec4& color,
                          float size, RenderBatch& batch);

    void FlushBatch(const Camera& camera, const RenderBatch& batch);

    void SetupShaders();
    void SetupBuffers();

    bool IsNodeVisible(const AABB& bounds, const Camera& camera,
                       const VisualizationOptions& options) const;

    int ComputeTreeDepth(const SDFBVH& bvh, uint32_t nodeIndex) const;

    // =========================================================================
    // Data Members
    // =========================================================================

    bool m_initialized = false;

    // OpenGL resources
    uint32_t m_shaderProgram = 0;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_ebo = 0;

    // Shader uniform locations
    int32_t m_uniformMVP = -1;
    int32_t m_uniformLineWidth = -1;

    // Render state
    RenderBatch m_mainBatch;
    RenderBatch m_highlightBatch;
    RenderBatch m_rayBatch;

    // Statistics
    BVHVisualizationStats m_stats;

    // Traversal data
    TraversalVisualizationData m_traversalData;

    // Heat map data
    std::unordered_map<uint32_t, uint32_t> m_accumulatedVisitCounts;
    uint32_t m_maxVisitCount = 0;

    // Custom color callback
    std::function<glm::vec4(uint32_t, const SDFBVHNode&, int)> m_customColorCallback;

    // Cached tree depth
    mutable int m_cachedTreeDepth = -1;
    mutable const SDFBVH* m_cachedBVH = nullptr;
};

} // namespace Nova
