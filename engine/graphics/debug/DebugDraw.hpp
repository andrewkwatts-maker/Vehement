#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Nova {

class Shader;

/**
 * @brief Debug visualization system for Nova3D
 *
 * Provides immediate-mode drawing of lines, shapes, and transforms for
 * debugging purposes. All primitives are batched and rendered efficiently
 * in a single draw call per frame.
 *
 * Usage:
 *   debugDraw.Clear();
 *   debugDraw.AddLine(...);
 *   debugDraw.AddSphere(...);
 *   debugDraw.Render(projectionView);
 */
class DebugDraw {
public:
    DebugDraw();
    ~DebugDraw();

    // Non-copyable, non-movable (owns OpenGL resources)
    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;
    DebugDraw(DebugDraw&&) = delete;
    DebugDraw& operator=(DebugDraw&&) = delete;

    /**
     * @brief Initialize the debug draw system
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup all resources
     */
    void Shutdown();

    /**
     * @brief Clear all queued draw commands
     */
    void Clear();

    /**
     * @brief Render all queued debug geometry
     * @param projectionView Combined projection * view matrix
     */
    void Render(const glm::mat4& projectionView);

    // ========== Line Drawing ==========

    /**
     * @brief Draw a line between two points
     */
    void AddLine(const glm::vec3& start, const glm::vec3& end,
                 const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a line with gradient color
     */
    void AddLine(const glm::vec3& start, const glm::vec3& end,
                 const glm::vec4& startColor, const glm::vec4& endColor);

    /**
     * @brief Draw a polyline (connected line segments)
     */
    void AddPolyline(const std::vector<glm::vec3>& points,
                     const glm::vec4& color = glm::vec4(1.0f),
                     bool closed = false);

    // ========== Shape Drawing ==========

    /**
     * @brief Draw a coordinate axis indicator (RGB = XYZ)
     */
    void AddTransform(const glm::mat4& transform, float size = 1.0f);

    /**
     * @brief Draw a grid on the XZ plane centered at origin
     */
    void AddGrid(int halfExtent = 10, float spacing = 1.0f,
                 const glm::vec4& color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));

    /**
     * @brief Draw an axis-aligned bounding box
     */
    void AddAABB(const glm::vec3& min, const glm::vec3& max,
                 const glm::vec4& color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    /**
     * @brief Draw an oriented bounding box
     */
    void AddBox(const glm::mat4& transform, const glm::vec3& halfExtents,
                const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a wireframe sphere (3 orthogonal circles)
     */
    void AddSphere(const glm::vec3& center, float radius,
                   const glm::vec4& color = glm::vec4(1.0f),
                   int segments = 16);

    /**
     * @brief Draw a circle in 3D space
     */
    void AddCircle(const glm::vec3& center, float radius,
                   const glm::vec3& normal = glm::vec3(0, 1, 0),
                   const glm::vec4& color = glm::vec4(1.0f),
                   int segments = 32);

    /**
     * @brief Draw a wireframe cylinder
     */
    void AddCylinder(const glm::vec3& base, float height, float radius,
                     const glm::vec4& color = glm::vec4(1.0f),
                     int segments = 16);

    /**
     * @brief Draw a wireframe capsule (cylinder with hemispherical caps)
     */
    void AddCapsule(const glm::vec3& start, const glm::vec3& end, float radius,
                    const glm::vec4& color = glm::vec4(1.0f),
                    int segments = 16);

    /**
     * @brief Draw a wireframe cone
     */
    void AddCone(const glm::vec3& apex, const glm::vec3& base, float radius,
                 const glm::vec4& color = glm::vec4(1.0f),
                 int segments = 16);

    /**
     * @brief Draw an arrow with head
     */
    void AddArrow(const glm::vec3& start, const glm::vec3& end,
                  const glm::vec4& color = glm::vec4(1.0f),
                  float headSize = 0.1f);

    /**
     * @brief Draw a ray from origin in direction
     */
    void AddRay(const glm::vec3& origin, const glm::vec3& direction,
                float length = 100.0f,
                const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a plane with normal indicator
     */
    void AddPlane(const glm::vec3& center, const glm::vec3& normal,
                  float size = 1.0f,
                  const glm::vec4& color = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f));

    /**
     * @brief Draw a camera frustum from inverse view-projection
     */
    void AddFrustum(const glm::mat4& viewProjection,
                    const glm::vec4& color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    /**
     * @brief Draw a point marker (3D cross)
     */
    void AddPoint(const glm::vec3& position, float size = 0.1f,
                  const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a triangle outline
     */
    void AddTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec4& color = glm::vec4(1.0f));

    // ========== Curve Drawing ==========

    /**
     * @brief Draw a quadratic Bezier curve
     */
    void AddBezierQuadratic(const glm::vec3& start, const glm::vec3& control,
                            const glm::vec3& end,
                            const glm::vec4& color = glm::vec4(1.0f),
                            int segments = 20);

    /**
     * @brief Draw a cubic Bezier curve
     */
    void AddBezierCubic(const glm::vec3& start, const glm::vec3& control1,
                        const glm::vec3& control2, const glm::vec3& end,
                        const glm::vec4& color = glm::vec4(1.0f),
                        int segments = 20);

    /**
     * @brief Draw an arc (partial circle)
     */
    void AddArc(const glm::vec3& center, float radius,
                const glm::vec3& normal, const glm::vec3& startDir,
                float angleDegrees,
                const glm::vec4& color = glm::vec4(1.0f),
                int segments = 32);

    // ========== Settings ==========

    /**
     * @brief Set line width for rendering
     * @note May not be supported on all OpenGL implementations
     */
    void SetLineWidth(float width) { m_lineWidth = width; }

    /**
     * @brief Get current line width
     */
    float GetLineWidth() const { return m_lineWidth; }

    /**
     * @brief Enable/disable depth testing for debug drawing
     */
    void SetDepthTest(bool enabled) { m_depthTest = enabled; }

    /**
     * @brief Get depth test state
     */
    bool GetDepthTest() const { return m_depthTest; }

    /**
     * @brief Get the number of vertices queued for rendering
     */
    size_t GetVertexCount() const { return m_lines.size(); }

    /**
     * @brief Get the number of lines queued for rendering
     */
    size_t GetLineCount() const { return m_lines.size() / 2; }

    /**
     * @brief Check if the system is initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    struct LineVertex {
        glm::vec3 position;
        glm::vec4 color;
    };

    // Helper to compute perpendicular basis vectors from a direction
    static void ComputeBasis(const glm::vec3& normal, glm::vec3& outRight, glm::vec3& outForward);

    std::vector<LineVertex> m_lines;

    std::unique_ptr<Shader> m_lineShader;

    uint32_t m_lineVAO = 0;
    uint32_t m_lineVBO = 0;
    size_t m_lineBufferCapacity = 0;

    float m_lineWidth = 1.0f;
    bool m_depthTest = true;
    bool m_initialized = false;

    // Performance tuning constants
    static constexpr size_t INITIAL_LINE_CAPACITY = 10000;
    static constexpr size_t MAX_LINES_PER_BATCH = 100000;
};

} // namespace Nova
