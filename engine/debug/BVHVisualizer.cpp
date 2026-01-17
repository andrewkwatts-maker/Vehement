/**
 * @file BVHVisualizer.cpp
 * @brief Implementation of BVH debug visualization
 */

#include "BVHVisualizer.hpp"
#include "../spatial/SDFBVH.hpp"
#include "../spatial/AABB.hpp"
#include "../scene/Camera.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stack>
#include <queue>

namespace Nova {

// ============================================================================
// Shader Sources
// ============================================================================

static const char* s_lineVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_MVP;

out vec4 v_Color;

void main() {
    gl_Position = u_MVP * vec4(a_Position, 1.0);
    v_Color = a_Color;
}
)";

static const char* s_lineFragmentShader = R"(
#version 330 core

in vec4 v_Color;
out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)";

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/**
 * @brief Compile a shader from source
 */
uint32_t CompileShader(GLenum type, const char* source) {
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check for errors
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

/**
 * @brief Link a shader program
 */
uint32_t LinkProgram(uint32_t vertexShader, uint32_t fragmentShader) {
    uint32_t program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for errors
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

/**
 * @brief Linear interpolation between two colors
 */
glm::vec4 LerpColor(const glm::vec4& a, const glm::vec4& b, float t) {
    return glm::mix(a, b, glm::clamp(t, 0.0f, 1.0f));
}

/**
 * @brief Heat map color (blue -> cyan -> green -> yellow -> red)
 */
glm::vec4 HeatMapColor(float t, const glm::vec4& cold, const glm::vec4& hot) {
    t = glm::clamp(t, 0.0f, 1.0f);

    if (t < 0.25f) {
        // Blue to cyan
        float localT = t / 0.25f;
        return glm::vec4(0.0f, localT, 1.0f, glm::mix(cold.a, hot.a, t));
    } else if (t < 0.5f) {
        // Cyan to green
        float localT = (t - 0.25f) / 0.25f;
        return glm::vec4(0.0f, 1.0f, 1.0f - localT, glm::mix(cold.a, hot.a, t));
    } else if (t < 0.75f) {
        // Green to yellow
        float localT = (t - 0.5f) / 0.25f;
        return glm::vec4(localT, 1.0f, 0.0f, glm::mix(cold.a, hot.a, t));
    } else {
        // Yellow to red
        float localT = (t - 0.75f) / 0.25f;
        return glm::vec4(1.0f, 1.0f - localT, 0.0f, glm::mix(cold.a, hot.a, t));
    }
}

} // anonymous namespace

// ============================================================================
// BVHVisualizer Implementation
// ============================================================================

BVHVisualizer::BVHVisualizer() = default;

BVHVisualizer::~BVHVisualizer() {
    Shutdown();
}

BVHVisualizer::BVHVisualizer(BVHVisualizer&& other) noexcept
    : m_initialized(other.m_initialized)
    , m_shaderProgram(other.m_shaderProgram)
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_uniformMVP(other.m_uniformMVP)
    , m_uniformLineWidth(other.m_uniformLineWidth)
    , m_mainBatch(std::move(other.m_mainBatch))
    , m_highlightBatch(std::move(other.m_highlightBatch))
    , m_rayBatch(std::move(other.m_rayBatch))
    , m_stats(other.m_stats)
    , m_traversalData(std::move(other.m_traversalData))
    , m_accumulatedVisitCounts(std::move(other.m_accumulatedVisitCounts))
    , m_maxVisitCount(other.m_maxVisitCount)
    , m_customColorCallback(std::move(other.m_customColorCallback))
    , m_cachedTreeDepth(other.m_cachedTreeDepth)
    , m_cachedBVH(other.m_cachedBVH)
{
    // Clear moved-from object's GL resources
    other.m_initialized = false;
    other.m_shaderProgram = 0;
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
}

BVHVisualizer& BVHVisualizer::operator=(BVHVisualizer&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_initialized = other.m_initialized;
        m_shaderProgram = other.m_shaderProgram;
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_uniformMVP = other.m_uniformMVP;
        m_uniformLineWidth = other.m_uniformLineWidth;
        m_mainBatch = std::move(other.m_mainBatch);
        m_highlightBatch = std::move(other.m_highlightBatch);
        m_rayBatch = std::move(other.m_rayBatch);
        m_stats = other.m_stats;
        m_traversalData = std::move(other.m_traversalData);
        m_accumulatedVisitCounts = std::move(other.m_accumulatedVisitCounts);
        m_maxVisitCount = other.m_maxVisitCount;
        m_customColorCallback = std::move(other.m_customColorCallback);
        m_cachedTreeDepth = other.m_cachedTreeDepth;
        m_cachedBVH = other.m_cachedBVH;

        other.m_initialized = false;
        other.m_shaderProgram = 0;
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
    }
    return *this;
}

bool BVHVisualizer::Initialize() {
    if (m_initialized) {
        return true;
    }

    SetupShaders();
    if (m_shaderProgram == 0) {
        return false;
    }

    SetupBuffers();
    m_initialized = true;
    return true;
}

void BVHVisualizer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }

    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }

    m_initialized = false;
}

void BVHVisualizer::SetupShaders() {
    uint32_t vertexShader = CompileShader(GL_VERTEX_SHADER, s_lineVertexShader);
    if (vertexShader == 0) {
        return;
    }

    uint32_t fragmentShader = CompileShader(GL_FRAGMENT_SHADER, s_lineFragmentShader);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return;
    }

    m_shaderProgram = LinkProgram(vertexShader, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (m_shaderProgram != 0) {
        m_uniformMVP = glGetUniformLocation(m_shaderProgram, "u_MVP");
    }
}

void BVHVisualizer::SetupBuffers() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(offsetof(LineVertex, position)));
    glEnableVertexAttribArray(0);

    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                          reinterpret_cast<void*>(offsetof(LineVertex, color)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    glBindVertexArray(0);
}

// ============================================================================
// Rendering
// ============================================================================

void BVHVisualizer::Render(const Camera& camera, const SDFBVH& bvh,
                           const VisualizationOptions& options) {
    if (!m_initialized || !options.enabled || !bvh.IsBuilt()) {
        return;
    }

    // Reset statistics
    m_stats.Reset();
    m_stats.totalNodes = static_cast<uint32_t>(bvh.GetNodeCount());

    // Clear batches
    m_mainBatch.vertices.clear();
    m_mainBatch.indices.clear();
    m_mainBatch.lineWidth = options.lineWidth;

    m_highlightBatch.vertices.clear();
    m_highlightBatch.indices.clear();
    m_highlightBatch.lineWidth = options.highlightLineWidth;

    // Update cached tree depth if needed
    if (m_cachedBVH != &bvh) {
        m_cachedBVH = &bvh;
        m_cachedTreeDepth = ComputeTreeDepth(bvh, 0);
    }

    // Build geometry
    BuildNodeGeometry(bvh, options, camera);

    // Setup OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);

    // Render main batch
    if (!m_mainBatch.vertices.empty()) {
        FlushBatch(camera, m_mainBatch);
    }

    // Render highlight batch (selected nodes)
    if (!m_highlightBatch.vertices.empty()) {
        FlushBatch(camera, m_highlightBatch);
    }

    // Render ray visualization if available
    if (options.showRayPath && !m_traversalData.visitedNodes.empty()) {
        RenderTraversal(camera, bvh, options);
    }

    // Restore state
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LINE_SMOOTH);
}

void BVHVisualizer::RenderTraversal(const Camera& camera, const SDFBVH& bvh,
                                     const VisualizationOptions& options) {
    if (!m_initialized || m_traversalData.visitedNodes.empty()) {
        return;
    }

    m_rayBatch.vertices.clear();
    m_rayBatch.indices.clear();
    m_rayBatch.lineWidth = options.rayLineWidth;

    const auto& nodes = bvh.GetNodes();

    // Draw the ray
    glm::vec3 rayStart = m_traversalData.ray.origin;
    glm::vec3 rayEnd = m_traversalData.ray.origin +
                       m_traversalData.ray.direction * m_traversalData.maxDistance;

    if (m_traversalData.hasHit) {
        rayEnd = m_traversalData.hitPoint;
    }

    AddLineToBuffer(rayStart, rayEnd, options.rayColor, m_rayBatch);

    // Draw hit points on visited node AABBs
    if (options.showHitPoints) {
        for (size_t i = 0; i < m_traversalData.visitedNodes.size(); ++i) {
            uint32_t nodeIndex = m_traversalData.visitedNodes[i];
            if (nodeIndex >= nodes.size()) continue;

            const auto& node = nodes[nodeIndex];

            // Compute intersection points with the AABB
            if (i < m_traversalData.nodeHitTimes.size()) {
                float tMin = m_traversalData.nodeHitTimes[i].first;
                float tMax = m_traversalData.nodeHitTimes[i].second;

                glm::vec3 entryPoint = rayStart + m_traversalData.ray.direction * tMin;
                glm::vec3 exitPoint = rayStart + m_traversalData.ray.direction * tMax;

                // Draw entry/exit points
                AddPointToBuffer(entryPoint, options.hitPointColor, options.hitPointSize, m_rayBatch);
                AddPointToBuffer(exitPoint, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f),
                                 options.hitPointSize * 0.7f, m_rayBatch);
            }
        }
    }

    // Draw final hit point if there was a hit
    if (m_traversalData.hasHit && options.showHitPoints) {
        AddPointToBuffer(m_traversalData.hitPoint,
                         glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
                         options.hitPointSize * 1.5f, m_rayBatch);

        // Draw normal at hit point
        glm::vec3 normalEnd = m_traversalData.hitPoint + m_traversalData.hitNormal * 0.5f;
        AddLineToBuffer(m_traversalData.hitPoint, normalEnd,
                        glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), m_rayBatch);
    }

    // Update traversal statistics
    m_stats.nodesVisited = static_cast<uint32_t>(m_traversalData.visitedNodes.size());
    m_stats.primitivesTested = static_cast<uint32_t>(m_traversalData.testedPrimitives.size());
    m_stats.rayLength = m_traversalData.hasHit ? m_traversalData.hitDistance : m_traversalData.maxDistance;

    // Render
    if (!m_rayBatch.vertices.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        FlushBatch(camera, m_rayBatch);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}

void BVHVisualizer::RenderStatistics(int screenWidth, int screenHeight) {
    // Note: This would require a text rendering system.
    // For now, we just prepare the statistics which can be queried via GetStats().

    // Future implementation could use ImGui or a custom text renderer
}

void BVHVisualizer::RenderAABB(const Camera& camera, const AABB& aabb,
                                const glm::vec4& color, float lineWidth) {
    if (!m_initialized) {
        return;
    }

    RenderBatch batch;
    batch.lineWidth = lineWidth;
    AddAABBToBuffer(aabb, color, batch);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    FlushBatch(camera, batch);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void BVHVisualizer::RenderRay(const Camera& camera, const Ray& ray, float length,
                               const glm::vec4& color, float lineWidth) {
    if (!m_initialized) {
        return;
    }

    RenderBatch batch;
    batch.lineWidth = lineWidth;

    glm::vec3 end = ray.origin + ray.direction * length;
    AddLineToBuffer(ray.origin, end, color, batch);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    FlushBatch(camera, batch);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// Traversal Data
// ============================================================================

void BVHVisualizer::SetTraversalData(const Ray& ray, const SDFBVHTraversalResult& result) {
    m_traversalData.Clear();
    m_traversalData.ray = ray;
    m_traversalData.maxDistance = result.closestT;

    // Copy candidate primitives as visited primitives
    m_traversalData.testedPrimitives = result.candidates;

    // Note: The SDFBVHTraversalResult doesn't directly provide visited nodes,
    // so we store the traversal statistics
    m_traversalData.hasHit = !result.candidates.empty();

    // Build visit counts for heat map
    for (uint32_t nodeIdx : m_traversalData.visitedNodes) {
        m_traversalData.nodeVisitCounts[nodeIdx]++;
    }
}

void BVHVisualizer::SetTraversalData(const TraversalVisualizationData& data) {
    m_traversalData = data;
}

void BVHVisualizer::AccumulateTraversal(const Ray& ray, const SDFBVHTraversalResult& result) {
    // Accumulate visit counts
    for (const auto& [nodeIdx, count] : m_traversalData.nodeVisitCounts) {
        m_accumulatedVisitCounts[nodeIdx] += count;
        m_maxVisitCount = std::max(m_maxVisitCount, m_accumulatedVisitCounts[nodeIdx]);
    }
}

void BVHVisualizer::ClearTraversalData() {
    m_traversalData.Clear();
}

void BVHVisualizer::ResetHeatMap() {
    m_accumulatedVisitCounts.clear();
    m_maxVisitCount = 0;
}

// ============================================================================
// Interactive Features
// ============================================================================

int32_t BVHVisualizer::HandleClick(const Camera& camera, const SDFBVH& bvh,
                                    const glm::vec2& screenPos, const glm::vec2& screenSize,
                                    VisualizationOptions& options) {
    if (!bvh.IsBuilt()) {
        options.selectedNode = -1;
        return -1;
    }

    // Convert screen position to ray
    glm::vec3 rayDir = camera.ScreenToWorldRay(screenPos, screenSize);
    Ray pickRay(camera.GetPosition(), rayDir);
    glm::vec3 invDir = pickRay.GetInverseDirection();

    const auto& nodes = bvh.GetNodes();

    // Find closest intersected node
    int32_t closestNode = -1;
    float closestT = std::numeric_limits<float>::max();

    std::stack<std::pair<uint32_t, int>> nodeStack;
    nodeStack.push({0, 0});

    while (!nodeStack.empty()) {
        auto [nodeIndex, depth] = nodeStack.top();
        nodeStack.pop();

        if (nodeIndex >= nodes.size()) continue;

        const auto& node = nodes[nodeIndex];

        // Check depth limits
        if (options.maxDepth >= 0 && depth > options.maxDepth) continue;
        if (depth < options.minDepth) {
            // Still traverse children but don't consider this node
            if (!node.IsLeaf()) {
                nodeStack.push({node.GetLeftChild(), depth + 1});
                nodeStack.push({node.GetRightChild(), depth + 1});
            }
            continue;
        }

        // Test ray intersection with node AABB
        float tMin, tMax;
        if (node.bounds.IntersectsRay(pickRay.origin, invDir, tMin, tMax)) {
            if (tMin < closestT && tMin > 0.0f) {
                closestT = tMin;
                closestNode = static_cast<int32_t>(nodeIndex);
            }

            // Traverse children if not collapsed
            if (!node.IsLeaf() && !options.collapsedNodes.contains(nodeIndex)) {
                nodeStack.push({node.GetLeftChild(), depth + 1});
                nodeStack.push({node.GetRightChild(), depth + 1});
            }
        }
    }

    options.selectedNode = closestNode;
    return closestNode;
}

void BVHVisualizer::ToggleNodeCollapse(uint32_t nodeIndex, VisualizationOptions& options) {
    if (options.collapsedNodes.contains(nodeIndex)) {
        options.collapsedNodes.erase(nodeIndex);
    } else {
        options.collapsedNodes.insert(nodeIndex);
    }
}

void BVHVisualizer::ExpandAll(VisualizationOptions& options) {
    options.collapsedNodes.clear();
}

void BVHVisualizer::CollapseToDepth(int maxExpandedDepth, const SDFBVH& bvh,
                                     VisualizationOptions& options) {
    if (!bvh.IsBuilt()) return;

    options.collapsedNodes.clear();

    const auto& nodes = bvh.GetNodes();

    std::queue<std::pair<uint32_t, int>> nodeQueue;
    nodeQueue.push({0, 0});

    while (!nodeQueue.empty()) {
        auto [nodeIndex, depth] = nodeQueue.front();
        nodeQueue.pop();

        if (nodeIndex >= nodes.size()) continue;

        const auto& node = nodes[nodeIndex];

        if (depth >= maxExpandedDepth && !node.IsLeaf()) {
            options.collapsedNodes.insert(nodeIndex);
        } else if (!node.IsLeaf()) {
            nodeQueue.push({node.GetLeftChild(), depth + 1});
            nodeQueue.push({node.GetRightChild(), depth + 1});
        }
    }
}

// ============================================================================
// Color Functions
// ============================================================================

glm::vec4 BVHVisualizer::GetNodeColor(uint32_t nodeIndex, const SDFBVHNode& node,
                                       int depth, int maxDepth,
                                       const VisualizationOptions& options) const {
    switch (options.colorMode) {
        case BVHColorMode::Depth: {
            float t = (maxDepth > 0) ? static_cast<float>(depth) / static_cast<float>(maxDepth) : 0.0f;
            return LerpColor(options.depthColorStart, options.depthColorEnd, t);
        }

        case BVHColorMode::NodeType:
            return node.IsLeaf() ? options.leafNodeColor : options.internalNodeColor;

        case BVHColorMode::HeatMap: {
            uint32_t visitCount = 0;
            if (!m_accumulatedVisitCounts.empty()) {
                auto it = m_accumulatedVisitCounts.find(nodeIndex);
                if (it != m_accumulatedVisitCounts.end()) {
                    visitCount = it->second;
                }
            } else {
                auto it = m_traversalData.nodeVisitCounts.find(nodeIndex);
                if (it != m_traversalData.nodeVisitCounts.end()) {
                    visitCount = it->second;
                }
            }

            uint32_t maxCount = m_maxVisitCount > 0 ? m_maxVisitCount : 1;
            float t = static_cast<float>(visitCount) / static_cast<float>(maxCount);
            return HeatMapColor(t, options.heatMapCold, options.heatMapHot);
        }

        case BVHColorMode::PrimitiveCount: {
            if (node.IsLeaf()) {
                // Color based on primitive count (more = hotter)
                float t = std::min(1.0f, static_cast<float>(node.GetPrimitiveCount()) / 8.0f);
                return HeatMapColor(t, options.heatMapCold, options.heatMapHot);
            }
            return options.internalNodeColor;
        }

        case BVHColorMode::SAHCost: {
            // Approximate SAH cost by surface area
            float surfaceArea = node.bounds.GetSurfaceArea();
            float rootArea = m_cachedBVH ? m_cachedBVH->GetBounds().GetSurfaceArea() : surfaceArea;
            float t = rootArea > 0.0f ? (surfaceArea / rootArea) : 0.0f;
            return HeatMapColor(1.0f - t, options.heatMapCold, options.heatMapHot);
        }

        case BVHColorMode::Custom:
            if (m_customColorCallback) {
                return m_customColorCallback(nodeIndex, node, depth);
            }
            return options.internalNodeColor;

        default:
            return options.internalNodeColor;
    }
}

void BVHVisualizer::SetCustomColorCallback(
    std::function<glm::vec4(uint32_t nodeIndex, const SDFBVHNode& node, int depth)> callback) {
    m_customColorCallback = std::move(callback);
}

// ============================================================================
// Debug Helpers
// ============================================================================

std::string BVHVisualizer::GetNodeDescription(const SDFBVH& bvh, uint32_t nodeIndex) const {
    if (!bvh.IsBuilt() || nodeIndex >= bvh.GetNodeCount()) {
        return "Invalid node";
    }

    const auto& node = bvh.GetNodes()[nodeIndex];
    std::ostringstream ss;

    ss << "Node " << nodeIndex << ": ";
    if (node.IsLeaf()) {
        ss << "Leaf with " << node.GetPrimitiveCount() << " primitives";
        ss << " (first=" << node.GetFirstPrimitive() << ")";
    } else {
        ss << "Internal (left=" << node.GetLeftChild()
           << ", right=" << node.GetRightChild() << ")";
    }

    ss << "\nBounds: [" << node.bounds.min.x << ", " << node.bounds.min.y << ", " << node.bounds.min.z
       << "] -> [" << node.bounds.max.x << ", " << node.bounds.max.y << ", " << node.bounds.max.z << "]";

    ss << "\nSurface Area: " << node.bounds.GetSurfaceArea();

    return ss.str();
}

std::vector<std::string> BVHVisualizer::ValidateBVH(const SDFBVH& bvh) const {
    std::vector<std::string> issues;

    if (!bvh.IsBuilt()) {
        issues.push_back("BVH is not built");
        return issues;
    }

    const auto& nodes = bvh.GetNodes();
    const auto& primitives = bvh.GetPrimitives();

    if (nodes.empty()) {
        issues.push_back("BVH has no nodes");
        return issues;
    }

    // Validate root bounds contain all primitives
    const AABB& rootBounds = nodes[0].bounds;
    for (size_t i = 0; i < primitives.size(); ++i) {
        if (!rootBounds.Contains(primitives[i].centroid)) {
            issues.push_back("Primitive " + std::to_string(i) +
                             " centroid outside root bounds");
        }
    }

    // Traverse and validate structure
    std::stack<std::pair<uint32_t, int>> nodeStack;
    nodeStack.push({0, 0});

    std::unordered_set<uint32_t> visited;

    while (!nodeStack.empty()) {
        auto [nodeIndex, depth] = nodeStack.top();
        nodeStack.pop();

        if (visited.contains(nodeIndex)) {
            issues.push_back("Cycle detected at node " + std::to_string(nodeIndex));
            continue;
        }
        visited.insert(nodeIndex);

        if (nodeIndex >= nodes.size()) {
            issues.push_back("Invalid node index: " + std::to_string(nodeIndex));
            continue;
        }

        const auto& node = nodes[nodeIndex];

        // Check for degenerate bounds
        if (!node.bounds.IsValid()) {
            issues.push_back("Node " + std::to_string(nodeIndex) + " has invalid bounds");
        }

        // Check children
        if (!node.IsLeaf()) {
            uint32_t left = node.GetLeftChild();
            uint32_t right = node.GetRightChild();

            if (left >= nodes.size()) {
                issues.push_back("Node " + std::to_string(nodeIndex) +
                                 " has invalid left child: " + std::to_string(left));
            } else {
                // Check child bounds are contained by parent
                if (!node.bounds.Contains(nodes[left].bounds.min) ||
                    !node.bounds.Contains(nodes[left].bounds.max)) {
                    issues.push_back("Node " + std::to_string(nodeIndex) +
                                     " left child bounds exceed parent");
                }
                nodeStack.push({left, depth + 1});
            }

            if (right >= nodes.size()) {
                issues.push_back("Node " + std::to_string(nodeIndex) +
                                 " has invalid right child: " + std::to_string(right));
            } else {
                if (!node.bounds.Contains(nodes[right].bounds.min) ||
                    !node.bounds.Contains(nodes[right].bounds.max)) {
                    issues.push_back("Node " + std::to_string(nodeIndex) +
                                     " right child bounds exceed parent");
                }
                nodeStack.push({right, depth + 1});
            }
        }

        // Check depth
        if (depth > 64) {
            issues.push_back("Excessive depth at node " + std::to_string(nodeIndex) +
                             ": " + std::to_string(depth));
            break;
        }
    }

    return issues;
}

// ============================================================================
// Internal Rendering Helpers
// ============================================================================

void BVHVisualizer::BuildNodeGeometry(const SDFBVH& bvh, const VisualizationOptions& options,
                                       const Camera& camera) {
    if (options.showRootOnly) {
        // Only render root node
        const auto& root = bvh.GetNodes()[0];
        glm::vec4 color = GetNodeColor(0, root, 0, m_cachedTreeDepth, options);
        AddAABBToBuffer(root.bounds, color, m_mainBatch);
        m_stats.renderedNodes = 1;
        return;
    }

    // Traverse tree and build geometry
    TraverseForRender(bvh, 0, 0, options, camera);
}

void BVHVisualizer::TraverseForRender(const SDFBVH& bvh, uint32_t nodeIndex, int depth,
                                       const VisualizationOptions& options,
                                       const Camera& camera) {
    if (nodeIndex >= bvh.GetNodeCount()) return;

    // Check node limit
    if (m_stats.renderedNodes >= options.maxNodesPerFrame) return;

    const auto& node = bvh.GetNodes()[nodeIndex];

    // Check depth limits
    if (options.maxDepth >= 0 && depth > options.maxDepth) return;

    // Check if node should be visible
    bool shouldRender = (depth >= options.minDepth);

    // Filter by node type
    if (shouldRender) {
        if (node.IsLeaf() && !options.showLeaves) {
            shouldRender = false;
        }
        if (!node.IsLeaf() && !options.showInternalNodes) {
            shouldRender = false;
        }
    }

    // Filter by visited status
    if (shouldRender && options.showOnlyVisited) {
        if (!m_traversalData.nodeVisitCounts.contains(nodeIndex) &&
            !m_accumulatedVisitCounts.contains(nodeIndex)) {
            shouldRender = false;
        }
    }

    // Custom filter
    if (shouldRender && options.customFilter) {
        shouldRender = options.customFilter(nodeIndex, node);
    }

    // Frustum culling
    if (shouldRender && options.useFrustumCulling) {
        if (!IsNodeVisible(node.bounds, camera, options)) {
            m_stats.culledNodes++;
            shouldRender = false;
        }
    }

    // Render this node
    if (shouldRender) {
        glm::vec4 color = GetNodeColor(nodeIndex, node, depth, m_cachedTreeDepth, options);

        // Check if this is the selected node
        if (options.selectedNode == static_cast<int32_t>(nodeIndex)) {
            AddAABBToBuffer(node.bounds, options.highlightColor, m_highlightBatch);
        } else {
            AddAABBToBuffer(node.bounds, color, m_mainBatch);
        }

        m_stats.renderedNodes++;

        if (node.IsLeaf()) {
            m_stats.leafNodes++;

            // Render primitive bounds within leaf
            if (options.showPrimitiveBounds) {
                const auto& primitives = bvh.GetPrimitives();
                uint32_t first = node.GetFirstPrimitive();
                uint32_t count = node.GetPrimitiveCount();

                for (uint32_t i = 0; i < count && first + i < primitives.size(); ++i) {
                    AddAABBToBuffer(primitives[first + i].bounds, options.primitiveColor, m_mainBatch);
                    m_stats.primitivesShown++;
                }
            }
        } else {
            m_stats.internalNodes++;
        }

        m_stats.maxDepthReached = std::max(m_stats.maxDepthReached, static_cast<uint32_t>(depth));
    }

    // Traverse children
    if (!node.IsLeaf() && !options.collapsedNodes.contains(nodeIndex)) {
        TraverseForRender(bvh, node.GetLeftChild(), depth + 1, options, camera);
        TraverseForRender(bvh, node.GetRightChild(), depth + 1, options, camera);
    }
}

void BVHVisualizer::AddAABBToBuffer(const AABB& aabb, const glm::vec4& color, RenderBatch& batch) {
    // Get corners of the AABB
    auto corners = aabb.GetCorners();

    uint32_t baseIndex = static_cast<uint32_t>(batch.vertices.size());

    // Add vertices
    for (const auto& corner : corners) {
        batch.vertices.push_back({corner, color});
    }

    // Add line indices for the 12 edges of the box
    // Bottom face
    batch.indices.push_back(baseIndex + 0); batch.indices.push_back(baseIndex + 1);
    batch.indices.push_back(baseIndex + 1); batch.indices.push_back(baseIndex + 3);
    batch.indices.push_back(baseIndex + 3); batch.indices.push_back(baseIndex + 2);
    batch.indices.push_back(baseIndex + 2); batch.indices.push_back(baseIndex + 0);

    // Top face
    batch.indices.push_back(baseIndex + 4); batch.indices.push_back(baseIndex + 5);
    batch.indices.push_back(baseIndex + 5); batch.indices.push_back(baseIndex + 7);
    batch.indices.push_back(baseIndex + 7); batch.indices.push_back(baseIndex + 6);
    batch.indices.push_back(baseIndex + 6); batch.indices.push_back(baseIndex + 4);

    // Vertical edges
    batch.indices.push_back(baseIndex + 0); batch.indices.push_back(baseIndex + 4);
    batch.indices.push_back(baseIndex + 1); batch.indices.push_back(baseIndex + 5);
    batch.indices.push_back(baseIndex + 2); batch.indices.push_back(baseIndex + 6);
    batch.indices.push_back(baseIndex + 3); batch.indices.push_back(baseIndex + 7);
}

void BVHVisualizer::AddLineToBuffer(const glm::vec3& start, const glm::vec3& end,
                                     const glm::vec4& color, RenderBatch& batch) {
    uint32_t baseIndex = static_cast<uint32_t>(batch.vertices.size());

    batch.vertices.push_back({start, color});
    batch.vertices.push_back({end, color});

    batch.indices.push_back(baseIndex);
    batch.indices.push_back(baseIndex + 1);
}

void BVHVisualizer::AddPointToBuffer(const glm::vec3& point, const glm::vec4& color,
                                      float size, RenderBatch& batch) {
    // Draw point as a small cross
    float halfSize = size * 0.01f;

    AddLineToBuffer(point - glm::vec3(halfSize, 0, 0), point + glm::vec3(halfSize, 0, 0), color, batch);
    AddLineToBuffer(point - glm::vec3(0, halfSize, 0), point + glm::vec3(0, halfSize, 0), color, batch);
    AddLineToBuffer(point - glm::vec3(0, 0, halfSize), point + glm::vec3(0, 0, halfSize), color, batch);
}

void BVHVisualizer::FlushBatch(const Camera& camera, const RenderBatch& batch) {
    if (batch.vertices.empty() || batch.indices.empty()) {
        return;
    }

    glUseProgram(m_shaderProgram);

    // Set MVP matrix
    glm::mat4 mvp = camera.GetProjectionView();
    glUniformMatrix4fv(m_uniformMVP, 1, GL_FALSE, glm::value_ptr(mvp));

    // Upload vertex data
    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 batch.vertices.size() * sizeof(LineVertex),
                 batch.vertices.data(),
                 GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 batch.indices.size() * sizeof(uint32_t),
                 batch.indices.data(),
                 GL_DYNAMIC_DRAW);

    // Set line width
    glLineWidth(batch.lineWidth);

    // Draw
    glDrawElements(GL_LINES, static_cast<GLsizei>(batch.indices.size()), GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glUseProgram(0);
}

bool BVHVisualizer::IsNodeVisible(const AABB& bounds, const Camera& camera,
                                   const VisualizationOptions& options) const {
    // Simple frustum check using the camera's built-in method
    glm::vec3 center = bounds.GetCenter();
    glm::vec3 extents = bounds.GetExtents();
    float radius = glm::length(extents);

    if (!camera.IsInFrustum(center, radius)) {
        return false;
    }

    // LOD check - skip very small nodes
    if (options.minScreenSizePercent > 0.0f) {
        // Compute approximate screen size
        glm::vec3 toCamera = center - camera.GetPosition();
        float distance = glm::length(toCamera);

        if (distance > 0.0f) {
            float screenSize = radius / (distance * std::tan(glm::radians(camera.GetFOV() * 0.5f)));
            if (screenSize < options.minScreenSizePercent) {
                return false;
            }
        }
    }

    return true;
}

int BVHVisualizer::ComputeTreeDepth(const SDFBVH& bvh, uint32_t nodeIndex) const {
    if (!bvh.IsBuilt() || nodeIndex >= bvh.GetNodeCount()) {
        return 0;
    }

    const auto& node = bvh.GetNodes()[nodeIndex];

    if (node.IsLeaf()) {
        return 0;
    }

    int leftDepth = ComputeTreeDepth(bvh, node.GetLeftChild());
    int rightDepth = ComputeTreeDepth(bvh, node.GetRightChild());

    return 1 + std::max(leftDepth, rightDepth);
}

} // namespace Nova
