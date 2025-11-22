#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>

namespace Nova {
    class Mesh;
}

namespace Vehement {

/**
 * @brief Raw mesh data structure for procedural generation
 *
 * Contains interleaved vertex data: position(3) + normal(3) + uv(2) = 8 floats per vertex
 */
struct MeshData {
    std::vector<float> vertices;      // Interleaved: pos(3), normal(3), uv(2)
    std::vector<uint32_t> indices;    // Triangle indices

    // Vertex layout constants
    static constexpr int FLOATS_PER_VERTEX = 8;  // 3 pos + 3 normal + 2 uv
    static constexpr int POSITION_OFFSET = 0;
    static constexpr int NORMAL_OFFSET = 3;
    static constexpr int UV_OFFSET = 6;

    /**
     * @brief Get vertex count
     */
    size_t GetVertexCount() const {
        return vertices.size() / FLOATS_PER_VERTEX;
    }

    /**
     * @brief Get triangle count
     */
    size_t GetTriangleCount() const {
        return indices.size() / 3;
    }

    /**
     * @brief Add a vertex to the mesh
     */
    void AddVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv) {
        vertices.push_back(position.x);
        vertices.push_back(position.y);
        vertices.push_back(position.z);
        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);
        vertices.push_back(uv.x);
        vertices.push_back(uv.y);
    }

    /**
     * @brief Add a triangle (3 indices)
     */
    void AddTriangle(uint32_t i0, uint32_t i1, uint32_t i2) {
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }

    /**
     * @brief Add a quad (2 triangles, 4 vertices referenced by indices)
     */
    void AddQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3) {
        // First triangle
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
        // Second triangle
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i3);
    }

    /**
     * @brief Clear all data
     */
    void Clear() {
        vertices.clear();
        indices.clear();
    }

    /**
     * @brief Reserve space for expected vertices and indices
     */
    void Reserve(size_t vertexCount, size_t indexCount) {
        vertices.reserve(vertexCount * FLOATS_PER_VERTEX);
        indices.reserve(indexCount);
    }

    /**
     * @brief Merge another mesh into this one
     */
    void Merge(const MeshData& other) {
        uint32_t indexOffset = static_cast<uint32_t>(GetVertexCount());

        // Copy vertices
        vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());

        // Copy indices with offset
        for (uint32_t idx : other.indices) {
            indices.push_back(idx + indexOffset);
        }
    }

    /**
     * @brief Transform all vertices
     */
    void Transform(const glm::mat4& matrix);

    /**
     * @brief Calculate bounding box
     */
    void GetBounds(glm::vec3& outMin, glm::vec3& outMax) const;

    /**
     * @brief Recalculate all normals (flat shading)
     */
    void RecalculateNormals();

    /**
     * @brief Flip all normals
     */
    void FlipNormals();

    /**
     * @brief Flip winding order (for back-face culling)
     */
    void FlipWinding();
};

/**
 * @brief Roof types for building generation
 */
enum class RoofType {
    Flat,           // Flat roof
    Gabled,         // Traditional A-frame roof
    Hipped,         // All sides slope down
    Shed,           // Single slope
    Mansard,        // French-style double slope
    Gambrel,        // Barn-style
    Dome,           // Spherical dome
    Pyramidal       // Four-sided pyramid
};

/**
 * @brief Static utility class for generating procedural 3D models
 *
 * All methods return MeshData that can be converted to engine Mesh objects.
 * Coordinates use right-handed Y-up convention:
 * - X: Right
 * - Y: Up
 * - Z: Forward (out of screen)
 */
class ProceduralModels {
public:
    // ========== Basic Shapes ==========

    /**
     * @brief Create a cube/box
     * @param size Box dimensions (width, height, depth)
     * @return Generated mesh data
     */
    static MeshData CreateCube(const glm::vec3& size);

    /**
     * @brief Create a hexagonal prism (flat-topped)
     * @param radius Distance from center to vertex
     * @param height Prism height
     * @return Generated mesh data
     */
    static MeshData CreateHexPrism(float radius, float height);

    /**
     * @brief Create a cylinder
     * @param radius Cylinder radius
     * @param height Cylinder height
     * @param segments Number of segments around circumference
     * @param capped Whether to include top and bottom caps
     * @return Generated mesh data
     */
    static MeshData CreateCylinder(float radius, float height, int segments, bool capped = true);

    /**
     * @brief Create a sphere
     * @param radius Sphere radius
     * @param segments Number of segments (both latitude and longitude)
     * @return Generated mesh data
     */
    static MeshData CreateSphere(float radius, int segments);

    /**
     * @brief Create a UV sphere with separate control
     * @param radius Sphere radius
     * @param rings Horizontal rings
     * @param segments Vertical segments
     * @return Generated mesh data
     */
    static MeshData CreateUVSphere(float radius, int rings, int segments);

    /**
     * @brief Create a cone
     * @param radius Base radius
     * @param height Cone height
     * @param segments Number of segments around base
     * @param capped Whether to include base cap
     * @return Generated mesh data
     */
    static MeshData CreateCone(float radius, float height, int segments, bool capped = true);

    /**
     * @brief Create a wedge (ramp shape)
     * @param size Wedge bounding box size
     * @return Generated mesh data
     */
    static MeshData CreateWedge(const glm::vec3& size);

    /**
     * @brief Create a staircase
     * @param width Staircase width
     * @param height Total staircase height
     * @param steps Number of steps
     * @return Generated mesh data
     */
    static MeshData CreateStairs(float width, float height, int steps);

    /**
     * @brief Create a torus (donut shape)
     * @param innerRadius Inner ring radius
     * @param outerRadius Outer ring radius (tube thickness)
     * @param rings Number of rings around torus
     * @param segments Number of segments per ring
     * @return Generated mesh data
     */
    static MeshData CreateTorus(float innerRadius, float outerRadius, int rings, int segments);

    /**
     * @brief Create a plane/quad
     * @param width Plane width (X)
     * @param depth Plane depth (Z)
     * @param subdivX Subdivisions in X
     * @param subdivZ Subdivisions in Z
     * @return Generated mesh data
     */
    static MeshData CreatePlane(float width, float depth, int subdivX = 1, int subdivZ = 1);

    /**
     * @brief Create a capsule (cylinder with hemisphere caps)
     * @param radius Capsule radius
     * @param height Total height (including caps)
     * @param segments Number of segments
     * @return Generated mesh data
     */
    static MeshData CreateCapsule(float radius, float height, int segments);

    // ========== Building Components ==========

    /**
     * @brief Create a wall segment
     * @param width Wall width
     * @param height Wall height
     * @param thickness Wall thickness
     * @return Generated mesh data
     */
    static MeshData CreateWall(float width, float height, float thickness);

    /**
     * @brief Create a wall with a door opening
     * @param width Wall width
     * @param height Wall height
     * @param doorWidth Door opening width
     * @param doorHeight Door opening height
     * @param doorOffset Horizontal offset of door from wall center
     * @return Generated mesh data
     */
    static MeshData CreateWallWithDoor(float width, float height, float doorWidth, float doorHeight,
                                        float doorOffset = 0.0f);

    /**
     * @brief Create a wall with a window opening
     * @param width Wall width
     * @param height Wall height
     * @param windowWidth Window opening width
     * @param windowHeight Window opening height
     * @param windowY Y position of window bottom
     * @param windowOffset Horizontal offset from center
     * @return Generated mesh data
     */
    static MeshData CreateWallWithWindow(float width, float height, float windowWidth, float windowHeight,
                                          float windowY, float windowOffset = 0.0f);

    /**
     * @brief Create a door frame
     * @param width Door frame width
     * @param height Door frame height
     * @param frameThickness Frame thickness
     * @param frameDepth Frame depth
     * @return Generated mesh data
     */
    static MeshData CreateDoorFrame(float width, float height, float frameThickness = 0.1f,
                                    float frameDepth = 0.1f);

    /**
     * @brief Create a window frame
     * @param width Window width
     * @param height Window height
     * @param frameThickness Frame thickness
     * @return Generated mesh data
     */
    static MeshData CreateWindowFrame(float width, float height, float frameThickness = 0.05f);

    /**
     * @brief Create a roof
     * @param width Roof width
     * @param depth Roof depth
     * @param height Roof height (peak height)
     * @param type Roof style
     * @param overhang Overhang beyond walls
     * @return Generated mesh data
     */
    static MeshData CreateRoof(float width, float depth, float height, RoofType type,
                               float overhang = 0.2f);

    /**
     * @brief Create a floor/ceiling plane
     * @param width Floor width
     * @param depth Floor depth
     * @param thickness Floor thickness
     * @return Generated mesh data
     */
    static MeshData CreateFloor(float width, float depth, float thickness);

    /**
     * @brief Create a beam/pillar
     * @param width Beam width
     * @param height Beam height
     * @param depth Beam depth
     * @return Generated mesh data
     */
    static MeshData CreateBeam(float width, float height, float depth);

    // ========== Hex-Specific ==========

    /**
     * @brief Create a hex tile (flat-topped hexagon with optional thickness)
     * @param radius Hex radius (center to vertex)
     * @param height Tile thickness/height
     * @return Generated mesh data
     */
    static MeshData CreateHexTile(float radius, float height);

    /**
     * @brief Create a pointed-top hex tile
     * @param radius Hex radius
     * @param height Tile height
     * @return Generated mesh data
     */
    static MeshData CreateHexTilePointy(float radius, float height);

    /**
     * @brief Create a single hex wall segment
     * @param radius Hex radius this wall belongs to
     * @param height Wall height
     * @param side Which side (0-5, starting from right going counter-clockwise)
     * @return Generated mesh data
     */
    static MeshData CreateHexWall(float radius, float height, int side);

    /**
     * @brief Create a hex corner pillar
     * @param radius Hex radius
     * @param height Corner height
     * @param corner Which corner (0-5)
     * @param pillarRadius Pillar thickness
     * @return Generated mesh data
     */
    static MeshData CreateHexCorner(float radius, float height, int corner, float pillarRadius = 0.1f);

    /**
     * @brief Create a hex ramp between two height levels
     * @param radius Hex radius
     * @param startHeight Starting height
     * @param endHeight Ending height
     * @param side Which side the ramp faces (0-5)
     * @return Generated mesh data
     */
    static MeshData CreateHexRamp(float radius, float startHeight, float endHeight, int side);

    /**
     * @brief Create hex stairs
     * @param radius Hex radius
     * @param height Total height
     * @param steps Number of steps
     * @param side Which side (0-5)
     * @return Generated mesh data
     */
    static MeshData CreateHexStairs(float radius, float height, int steps, int side);

    // ========== Utility ==========

    /**
     * @brief Convert MeshData to engine Mesh
     * @param data Source mesh data
     * @return Unique pointer to created Mesh
     */
    static std::unique_ptr<Nova::Mesh> CreateMeshFromData(const MeshData& data);

    /**
     * @brief Get hex vertex position (flat-topped)
     * @param center Hex center
     * @param radius Hex radius
     * @param corner Corner index (0-5)
     * @param y Y height
     * @return Vertex position
     */
    static glm::vec3 GetHexVertex(const glm::vec2& center, float radius, int corner, float y = 0.0f);

    /**
     * @brief Get hex vertex position (pointy-topped)
     * @param center Hex center
     * @param radius Hex radius
     * @param corner Corner index (0-5)
     * @param y Y height
     * @return Vertex position
     */
    static glm::vec3 GetHexVertexPointy(const glm::vec2& center, float radius, int corner, float y = 0.0f);

private:
    // Helper constants
    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float TWO_PI = 2.0f * PI;
    static constexpr float HALF_PI = PI * 0.5f;

    // Hex angle offset for flat-topped hexagon (30 degrees)
    static constexpr float HEX_ANGLE_OFFSET = PI / 6.0f;

    /**
     * @brief Add a quad with computed normals
     */
    static void AddQuadWithNormal(MeshData& data,
                                  const glm::vec3& v0, const glm::vec3& v1,
                                  const glm::vec3& v2, const glm::vec3& v3,
                                  const glm::vec2& uv0, const glm::vec2& uv1,
                                  const glm::vec2& uv2, const glm::vec2& uv3);

    /**
     * @brief Add a triangle with computed normal
     */
    static void AddTriangleWithNormal(MeshData& data,
                                      const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                      const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2);

    /**
     * @brief Compute face normal from 3 vertices
     */
    static glm::vec3 ComputeNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
};

} // namespace Vehement
