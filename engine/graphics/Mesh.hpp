#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Vertex structure for meshes
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 bitangent;

    // For skeletal animation
    glm::ivec4 boneIDs = glm::ivec4(0);
    glm::vec4 boneWeights = glm::vec4(0.0f);
};

/**
 * @brief OpenGL mesh wrapper
 *
 * Handles vertex buffer objects and drawing.
 */
class Mesh {
public:
    Mesh();
    ~Mesh();

    // Delete copy, allow move
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /**
     * @brief Create mesh from vertex and index data
     */
    void Create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    /**
     * @brief Create mesh from raw float data (for procedural geometry)
     */
    void CreateFromRaw(const float* vertices, size_t vertexCount,
                       const uint32_t* indices, size_t indexCount,
                       bool hasNormals = true, bool hasTexCoords = true,
                       bool hasTangents = false);

    /**
     * @brief Draw the mesh
     */
    void Draw() const;

    /**
     * @brief Draw the mesh with instancing
     */
    void DrawInstanced(int instanceCount) const;

    /**
     * @brief Cleanup GPU resources
     */
    void Cleanup();

    /**
     * @brief Check if mesh is valid
     */
    bool IsValid() const { return m_vao != 0; }

    /**
     * @brief Get vertex count
     */
    uint32_t GetVertexCount() const { return m_vertexCount; }

    /**
     * @brief Get index count
     */
    uint32_t GetIndexCount() const { return m_indexCount; }

    /**
     * @brief Get bounding box min
     */
    const glm::vec3& GetBoundsMin() const { return m_boundsMin; }

    /**
     * @brief Get bounding box max
     */
    const glm::vec3& GetBoundsMax() const { return m_boundsMax; }

    // Static factory methods for primitive shapes
    static std::unique_ptr<Mesh> CreateCube(float size = 1.0f);
    static std::unique_ptr<Mesh> CreateSphere(float radius = 1.0f, int segments = 32);
    static std::unique_ptr<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f, int divisionsX = 1, int divisionsY = 1);
    static std::unique_ptr<Mesh> CreateCylinder(float radius = 1.0f, float height = 1.0f, int segments = 32);
    static std::unique_ptr<Mesh> CreateCone(float radius = 1.0f, float height = 1.0f, int segments = 32);
    static std::unique_ptr<Mesh> CreateTorus(float innerRadius = 0.5f, float outerRadius = 1.0f, int rings = 32, int segments = 32);

private:
    void SetupMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void CalculateBounds(const std::vector<Vertex>& vertices);

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_ebo = 0;

    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;

    glm::vec3 m_boundsMin{0};
    glm::vec3 m_boundsMax{0};
};

} // namespace Nova
