#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <memory>

namespace Nova {

class Shader;

/**
 * @brief Debug visualization system
 *
 * Modern replacement for AIE Gizmos. Provides immediate-mode
 * drawing of lines, shapes, and transforms for debugging.
 */
class DebugDraw {
public:
    DebugDraw();
    ~DebugDraw();

    /**
     * @brief Initialize the debug draw system
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Clear all queued draw commands
     */
    void Clear();

    /**
     * @brief Render all queued debug geometry
     */
    void Render(const glm::mat4& projectionView);

    // ========== Line Drawing ==========

    /**
     * @brief Draw a line
     */
    void AddLine(const glm::vec3& start, const glm::vec3& end,
                 const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a line with different colors at each end
     */
    void AddLine(const glm::vec3& start, const glm::vec3& end,
                 const glm::vec4& startColor, const glm::vec4& endColor);

    // ========== Shape Drawing ==========

    /**
     * @brief Draw a transform gizmo (3 axis lines)
     */
    void AddTransform(const glm::mat4& transform, float size = 1.0f);

    /**
     * @brief Draw a grid on the XZ plane
     */
    void AddGrid(int halfExtent = 10, float spacing = 1.0f,
                 const glm::vec4& color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));

    /**
     * @brief Draw an axis-aligned bounding box
     */
    void AddAABB(const glm::vec3& min, const glm::vec3& max,
                 const glm::vec4& color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    /**
     * @brief Draw a box with transform
     */
    void AddBox(const glm::mat4& transform, const glm::vec3& halfExtents,
                const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a sphere (wireframe)
     */
    void AddSphere(const glm::vec3& center, float radius,
                   const glm::vec4& color = glm::vec4(1.0f),
                   int segments = 16);

    /**
     * @brief Draw a circle
     */
    void AddCircle(const glm::vec3& center, float radius,
                   const glm::vec3& normal = glm::vec3(0, 1, 0),
                   const glm::vec4& color = glm::vec4(1.0f),
                   int segments = 32);

    /**
     * @brief Draw a cylinder (wireframe)
     */
    void AddCylinder(const glm::vec3& base, float height, float radius,
                     const glm::vec4& color = glm::vec4(1.0f),
                     int segments = 16);

    /**
     * @brief Draw a cone (wireframe)
     */
    void AddCone(const glm::vec3& apex, const glm::vec3& base, float radius,
                 const glm::vec4& color = glm::vec4(1.0f),
                 int segments = 16);

    /**
     * @brief Draw an arrow
     */
    void AddArrow(const glm::vec3& start, const glm::vec3& end,
                  const glm::vec4& color = glm::vec4(1.0f),
                  float headSize = 0.1f);

    /**
     * @brief Draw a ray (infinite line from point in direction)
     */
    void AddRay(const glm::vec3& origin, const glm::vec3& direction,
                float length = 100.0f,
                const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Draw a plane
     */
    void AddPlane(const glm::vec3& center, const glm::vec3& normal,
                  float size = 1.0f,
                  const glm::vec4& color = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f));

    /**
     * @brief Draw a frustum
     */
    void AddFrustum(const glm::mat4& viewProjection,
                    const glm::vec4& color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    /**
     * @brief Draw a point (rendered as small cross)
     */
    void AddPoint(const glm::vec3& position, float size = 0.1f,
                  const glm::vec4& color = glm::vec4(1.0f));

    // ========== Text Drawing ==========

    /**
     * @brief Draw 3D text at position (requires font system)
     */
    void AddText(const glm::vec3& position, const std::string& text,
                 const glm::vec4& color = glm::vec4(1.0f));

    // ========== Settings ==========

    /**
     * @brief Set line width (may not work on all drivers)
     */
    void SetLineWidth(float width) { m_lineWidth = width; }

    /**
     * @brief Enable/disable depth testing for debug drawing
     */
    void SetDepthTest(bool enabled) { m_depthTest = enabled; }

private:
    struct LineVertex {
        glm::vec3 position;
        glm::vec4 color;
    };

    void FlushLines();

    std::vector<LineVertex> m_lines;

    std::unique_ptr<Shader> m_lineShader;

    uint32_t m_lineVAO = 0;
    uint32_t m_lineVBO = 0;
    size_t m_lineBufferCapacity = 0;

    float m_lineWidth = 1.0f;
    bool m_depthTest = true;
    bool m_initialized = false;

    static constexpr size_t INITIAL_LINE_CAPACITY = 10000;
};

} // namespace Nova
