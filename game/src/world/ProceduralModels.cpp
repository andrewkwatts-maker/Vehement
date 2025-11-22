#include "ProceduralModels.hpp"
#include "../../../engine/graphics/Mesh.hpp"

#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Vehement {

// ============================================================================
// MeshData Implementation
// ============================================================================

void MeshData::Transform(const glm::mat4& matrix) {
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(matrix)));

    for (size_t i = 0; i < GetVertexCount(); ++i) {
        size_t offset = i * FLOATS_PER_VERTEX;

        // Transform position
        glm::vec4 pos(vertices[offset], vertices[offset + 1], vertices[offset + 2], 1.0f);
        pos = matrix * pos;
        vertices[offset] = pos.x;
        vertices[offset + 1] = pos.y;
        vertices[offset + 2] = pos.z;

        // Transform normal
        glm::vec3 normal(vertices[offset + 3], vertices[offset + 4], vertices[offset + 5]);
        normal = glm::normalize(normalMatrix * normal);
        vertices[offset + 3] = normal.x;
        vertices[offset + 4] = normal.y;
        vertices[offset + 5] = normal.z;
    }
}

void MeshData::GetBounds(glm::vec3& outMin, glm::vec3& outMax) const {
    if (GetVertexCount() == 0) {
        outMin = outMax = glm::vec3(0.0f);
        return;
    }

    outMin = glm::vec3(vertices[0], vertices[1], vertices[2]);
    outMax = outMin;

    for (size_t i = 1; i < GetVertexCount(); ++i) {
        size_t offset = i * FLOATS_PER_VERTEX;
        glm::vec3 pos(vertices[offset], vertices[offset + 1], vertices[offset + 2]);
        outMin = glm::min(outMin, pos);
        outMax = glm::max(outMax, pos);
    }
}

void MeshData::RecalculateNormals() {
    // Reset all normals to zero
    for (size_t i = 0; i < GetVertexCount(); ++i) {
        size_t offset = i * FLOATS_PER_VERTEX + NORMAL_OFFSET;
        vertices[offset] = 0.0f;
        vertices[offset + 1] = 0.0f;
        vertices[offset + 2] = 0.0f;
    }

    // Calculate face normals and accumulate
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        glm::vec3 v0(vertices[i0 * FLOATS_PER_VERTEX],
                     vertices[i0 * FLOATS_PER_VERTEX + 1],
                     vertices[i0 * FLOATS_PER_VERTEX + 2]);
        glm::vec3 v1(vertices[i1 * FLOATS_PER_VERTEX],
                     vertices[i1 * FLOATS_PER_VERTEX + 1],
                     vertices[i1 * FLOATS_PER_VERTEX + 2]);
        glm::vec3 v2(vertices[i2 * FLOATS_PER_VERTEX],
                     vertices[i2 * FLOATS_PER_VERTEX + 1],
                     vertices[i2 * FLOATS_PER_VERTEX + 2]);

        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        // Add to all three vertices
        for (uint32_t idx : {i0, i1, i2}) {
            size_t offset = idx * FLOATS_PER_VERTEX + NORMAL_OFFSET;
            vertices[offset] += normal.x;
            vertices[offset + 1] += normal.y;
            vertices[offset + 2] += normal.z;
        }
    }

    // Normalize all normals
    for (size_t i = 0; i < GetVertexCount(); ++i) {
        size_t offset = i * FLOATS_PER_VERTEX + NORMAL_OFFSET;
        glm::vec3 normal(vertices[offset], vertices[offset + 1], vertices[offset + 2]);
        normal = glm::normalize(normal);
        vertices[offset] = normal.x;
        vertices[offset + 1] = normal.y;
        vertices[offset + 2] = normal.z;
    }
}

void MeshData::FlipNormals() {
    for (size_t i = 0; i < GetVertexCount(); ++i) {
        size_t offset = i * FLOATS_PER_VERTEX + NORMAL_OFFSET;
        vertices[offset] = -vertices[offset];
        vertices[offset + 1] = -vertices[offset + 1];
        vertices[offset + 2] = -vertices[offset + 2];
    }
}

void MeshData::FlipWinding() {
    for (size_t i = 0; i < indices.size(); i += 3) {
        std::swap(indices[i + 1], indices[i + 2]);
    }
}

// ============================================================================
// ProceduralModels Implementation
// ============================================================================

glm::vec3 ProceduralModels::ComputeNormal(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    return glm::normalize(glm::cross(v1 - v0, v2 - v0));
}

void ProceduralModels::AddQuadWithNormal(MeshData& data,
                                         const glm::vec3& v0, const glm::vec3& v1,
                                         const glm::vec3& v2, const glm::vec3& v3,
                                         const glm::vec2& uv0, const glm::vec2& uv1,
                                         const glm::vec2& uv2, const glm::vec2& uv3) {
    glm::vec3 normal = ComputeNormal(v0, v1, v2);
    uint32_t baseIndex = static_cast<uint32_t>(data.GetVertexCount());

    data.AddVertex(v0, normal, uv0);
    data.AddVertex(v1, normal, uv1);
    data.AddVertex(v2, normal, uv2);
    data.AddVertex(v3, normal, uv3);

    data.AddQuad(baseIndex, baseIndex + 1, baseIndex + 2, baseIndex + 3);
}

void ProceduralModels::AddTriangleWithNormal(MeshData& data,
                                             const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                             const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2) {
    glm::vec3 normal = ComputeNormal(v0, v1, v2);
    uint32_t baseIndex = static_cast<uint32_t>(data.GetVertexCount());

    data.AddVertex(v0, normal, uv0);
    data.AddVertex(v1, normal, uv1);
    data.AddVertex(v2, normal, uv2);

    data.AddTriangle(baseIndex, baseIndex + 1, baseIndex + 2);
}

// ============================================================================
// Basic Shapes
// ============================================================================

MeshData ProceduralModels::CreateCube(const glm::vec3& size) {
    MeshData data;
    data.Reserve(24, 36);  // 6 faces * 4 vertices, 6 faces * 6 indices

    float hw = size.x * 0.5f;
    float hh = size.y * 0.5f;
    float hd = size.z * 0.5f;

    // Front face (+Z)
    AddQuadWithNormal(data,
        glm::vec3(-hw, -hh, hd), glm::vec3(hw, -hh, hd),
        glm::vec3(hw, hh, hd), glm::vec3(-hw, hh, hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Back face (-Z)
    AddQuadWithNormal(data,
        glm::vec3(hw, -hh, -hd), glm::vec3(-hw, -hh, -hd),
        glm::vec3(-hw, hh, -hd), glm::vec3(hw, hh, -hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Right face (+X)
    AddQuadWithNormal(data,
        glm::vec3(hw, -hh, hd), glm::vec3(hw, -hh, -hd),
        glm::vec3(hw, hh, -hd), glm::vec3(hw, hh, hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Left face (-X)
    AddQuadWithNormal(data,
        glm::vec3(-hw, -hh, -hd), glm::vec3(-hw, -hh, hd),
        glm::vec3(-hw, hh, hd), glm::vec3(-hw, hh, -hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Top face (+Y)
    AddQuadWithNormal(data,
        glm::vec3(-hw, hh, hd), glm::vec3(hw, hh, hd),
        glm::vec3(hw, hh, -hd), glm::vec3(-hw, hh, -hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Bottom face (-Y)
    AddQuadWithNormal(data,
        glm::vec3(-hw, -hh, -hd), glm::vec3(hw, -hh, -hd),
        glm::vec3(hw, -hh, hd), glm::vec3(-hw, -hh, hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    return data;
}

MeshData ProceduralModels::CreateHexPrism(float radius, float height) {
    MeshData data;

    float halfHeight = height * 0.5f;

    // Generate hex vertices (flat-topped)
    std::vector<glm::vec3> topVerts, bottomVerts;
    for (int i = 0; i < 6; ++i) {
        float angle = HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * i;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        topVerts.push_back(glm::vec3(x, halfHeight, z));
        bottomVerts.push_back(glm::vec3(x, -halfHeight, z));
    }

    // Top face (fan from center)
    glm::vec3 topCenter(0, halfHeight, 0);
    glm::vec3 topNormal(0, 1, 0);
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;
        uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

        float u0 = 0.5f;
        float v0 = 0.5f;
        float u1 = 0.5f + 0.5f * std::cos(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * i);
        float v1 = 0.5f + 0.5f * std::sin(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * i);
        float u2 = 0.5f + 0.5f * std::cos(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * next);
        float v2 = 0.5f + 0.5f * std::sin(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * next);

        data.AddVertex(topCenter, topNormal, glm::vec2(u0, v0));
        data.AddVertex(topVerts[i], topNormal, glm::vec2(u1, v1));
        data.AddVertex(topVerts[next], topNormal, glm::vec2(u2, v2));

        data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
    }

    // Bottom face (fan from center, reversed winding)
    glm::vec3 bottomCenter(0, -halfHeight, 0);
    glm::vec3 bottomNormal(0, -1, 0);
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;
        uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

        float u0 = 0.5f;
        float v0 = 0.5f;
        float u1 = 0.5f + 0.5f * std::cos(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * next);
        float v1 = 0.5f + 0.5f * std::sin(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * next);
        float u2 = 0.5f + 0.5f * std::cos(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * i);
        float v2 = 0.5f + 0.5f * std::sin(HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * i);

        data.AddVertex(bottomCenter, bottomNormal, glm::vec2(u0, v0));
        data.AddVertex(bottomVerts[next], bottomNormal, glm::vec2(u1, v1));
        data.AddVertex(bottomVerts[i], bottomNormal, glm::vec2(u2, v2));

        data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
    }

    // Side faces
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;

        float u0 = static_cast<float>(i) / 6.0f;
        float u1 = static_cast<float>(i + 1) / 6.0f;

        AddQuadWithNormal(data,
            bottomVerts[i], bottomVerts[next], topVerts[next], topVerts[i],
            glm::vec2(u0, 0), glm::vec2(u1, 0), glm::vec2(u1, 1), glm::vec2(u0, 1));
    }

    return data;
}

MeshData ProceduralModels::CreateCylinder(float radius, float height, int segments, bool capped) {
    MeshData data;

    float halfHeight = height * 0.5f;
    float angleStep = TWO_PI / segments;

    // Generate ring vertices
    std::vector<glm::vec3> topRing, bottomRing;
    for (int i = 0; i < segments; ++i) {
        float angle = angleStep * i;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        topRing.push_back(glm::vec3(x, halfHeight, z));
        bottomRing.push_back(glm::vec3(x, -halfHeight, z));
    }

    // Side faces
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;

        float u0 = static_cast<float>(i) / segments;
        float u1 = static_cast<float>(i + 1) / segments;

        AddQuadWithNormal(data,
            bottomRing[i], bottomRing[next], topRing[next], topRing[i],
            glm::vec2(u0, 0), glm::vec2(u1, 0), glm::vec2(u1, 1), glm::vec2(u0, 1));
    }

    if (capped) {
        // Top cap
        glm::vec3 topCenter(0, halfHeight, 0);
        glm::vec3 topNormal(0, 1, 0);
        for (int i = 0; i < segments; ++i) {
            int next = (i + 1) % segments;
            uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

            float u1 = 0.5f + 0.5f * std::cos(angleStep * i);
            float v1 = 0.5f + 0.5f * std::sin(angleStep * i);
            float u2 = 0.5f + 0.5f * std::cos(angleStep * next);
            float v2 = 0.5f + 0.5f * std::sin(angleStep * next);

            data.AddVertex(topCenter, topNormal, glm::vec2(0.5f, 0.5f));
            data.AddVertex(topRing[i], topNormal, glm::vec2(u1, v1));
            data.AddVertex(topRing[next], topNormal, glm::vec2(u2, v2));

            data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
        }

        // Bottom cap
        glm::vec3 bottomCenter(0, -halfHeight, 0);
        glm::vec3 bottomNormal(0, -1, 0);
        for (int i = 0; i < segments; ++i) {
            int next = (i + 1) % segments;
            uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

            float u1 = 0.5f + 0.5f * std::cos(angleStep * next);
            float v1 = 0.5f + 0.5f * std::sin(angleStep * next);
            float u2 = 0.5f + 0.5f * std::cos(angleStep * i);
            float v2 = 0.5f + 0.5f * std::sin(angleStep * i);

            data.AddVertex(bottomCenter, bottomNormal, glm::vec2(0.5f, 0.5f));
            data.AddVertex(bottomRing[next], bottomNormal, glm::vec2(u1, v1));
            data.AddVertex(bottomRing[i], bottomNormal, glm::vec2(u2, v2));

            data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
        }
    }

    return data;
}

MeshData ProceduralModels::CreateSphere(float radius, int segments) {
    return CreateUVSphere(radius, segments, segments);
}

MeshData ProceduralModels::CreateUVSphere(float radius, int rings, int segments) {
    MeshData data;

    // Generate vertices
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = PI * static_cast<float>(ring) / rings;
        float y = radius * std::cos(phi);
        float ringRadius = radius * std::sin(phi);

        for (int seg = 0; seg <= segments; ++seg) {
            float theta = TWO_PI * static_cast<float>(seg) / segments;
            float x = ringRadius * std::cos(theta);
            float z = ringRadius * std::sin(theta);

            glm::vec3 pos(x, y, z);
            glm::vec3 normal = glm::normalize(pos);
            glm::vec2 uv(static_cast<float>(seg) / segments, static_cast<float>(ring) / rings);

            data.AddVertex(pos, normal, uv);
        }
    }

    // Generate indices
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            data.AddTriangle(current, next, current + 1);
            data.AddTriangle(current + 1, next, next + 1);
        }
    }

    return data;
}

MeshData ProceduralModels::CreateCone(float radius, float height, int segments, bool capped) {
    MeshData data;

    float halfHeight = height * 0.5f;
    float angleStep = TWO_PI / segments;
    glm::vec3 apex(0, halfHeight, 0);

    // Generate base ring
    std::vector<glm::vec3> baseRing;
    for (int i = 0; i < segments; ++i) {
        float angle = angleStep * i;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        baseRing.push_back(glm::vec3(x, -halfHeight, z));
    }

    // Side faces
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;

        glm::vec3 v0 = baseRing[i];
        glm::vec3 v1 = baseRing[next];

        // Calculate normal for this face
        glm::vec3 edge1 = apex - v0;
        glm::vec3 edge2 = v1 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

        float u0 = static_cast<float>(i) / segments;
        float u1 = static_cast<float>(i + 1) / segments;

        data.AddVertex(v0, normal, glm::vec2(u0, 0));
        data.AddVertex(v1, normal, glm::vec2(u1, 0));
        data.AddVertex(apex, normal, glm::vec2((u0 + u1) * 0.5f, 1));

        data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
    }

    if (capped) {
        // Base cap
        glm::vec3 baseCenter(0, -halfHeight, 0);
        glm::vec3 bottomNormal(0, -1, 0);

        for (int i = 0; i < segments; ++i) {
            int next = (i + 1) % segments;
            uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

            float u1 = 0.5f + 0.5f * std::cos(angleStep * next);
            float v1 = 0.5f + 0.5f * std::sin(angleStep * next);
            float u2 = 0.5f + 0.5f * std::cos(angleStep * i);
            float v2 = 0.5f + 0.5f * std::sin(angleStep * i);

            data.AddVertex(baseCenter, bottomNormal, glm::vec2(0.5f, 0.5f));
            data.AddVertex(baseRing[next], bottomNormal, glm::vec2(u1, v1));
            data.AddVertex(baseRing[i], bottomNormal, glm::vec2(u2, v2));

            data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
        }
    }

    return data;
}

MeshData ProceduralModels::CreateWedge(const glm::vec3& size) {
    MeshData data;

    float hw = size.x * 0.5f;
    float hh = size.y;  // Full height
    float hd = size.z * 0.5f;

    // Vertices:
    // Bottom: 0(-hw, 0, hd), 1(hw, 0, hd), 2(hw, 0, -hd), 3(-hw, 0, -hd)
    // Top: 4(-hw, hh, -hd), 5(hw, hh, -hd)

    // Bottom face
    AddQuadWithNormal(data,
        glm::vec3(-hw, 0, -hd), glm::vec3(hw, 0, -hd),
        glm::vec3(hw, 0, hd), glm::vec3(-hw, 0, hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Back face (vertical)
    AddQuadWithNormal(data,
        glm::vec3(hw, 0, -hd), glm::vec3(-hw, 0, -hd),
        glm::vec3(-hw, hh, -hd), glm::vec3(hw, hh, -hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Ramp face (slope)
    AddQuadWithNormal(data,
        glm::vec3(-hw, 0, hd), glm::vec3(hw, 0, hd),
        glm::vec3(hw, hh, -hd), glm::vec3(-hw, hh, -hd),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Left triangle
    AddTriangleWithNormal(data,
        glm::vec3(-hw, 0, hd), glm::vec3(-hw, hh, -hd), glm::vec3(-hw, 0, -hd),
        glm::vec2(1, 0), glm::vec2(0, 1), glm::vec2(0, 0));

    // Right triangle
    AddTriangleWithNormal(data,
        glm::vec3(hw, 0, -hd), glm::vec3(hw, hh, -hd), glm::vec3(hw, 0, hd),
        glm::vec2(0, 0), glm::vec2(0, 1), glm::vec2(1, 0));

    return data;
}

MeshData ProceduralModels::CreateStairs(float width, float height, int steps) {
    MeshData data;

    if (steps < 1) steps = 1;

    float stepHeight = height / steps;
    float stepDepth = width / steps;
    float halfWidth = width * 0.5f;

    for (int i = 0; i < steps; ++i) {
        float y0 = i * stepHeight;
        float y1 = (i + 1) * stepHeight;
        float z0 = -halfWidth + i * stepDepth;
        float z1 = -halfWidth + (i + 1) * stepDepth;

        // Step top (tread)
        AddQuadWithNormal(data,
            glm::vec3(-halfWidth, y1, z0), glm::vec3(halfWidth, y1, z0),
            glm::vec3(halfWidth, y1, z1), glm::vec3(-halfWidth, y1, z1),
            glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

        // Step front (riser)
        AddQuadWithNormal(data,
            glm::vec3(-halfWidth, y0, z0), glm::vec3(halfWidth, y0, z0),
            glm::vec3(halfWidth, y1, z0), glm::vec3(-halfWidth, y1, z0),
            glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));
    }

    // Side faces (simplified - just triangular profiles)
    // Left side
    for (int i = 0; i < steps; ++i) {
        float y0 = i * stepHeight;
        float y1 = (i + 1) * stepHeight;
        float z0 = -halfWidth + i * stepDepth;
        float z1 = -halfWidth + (i + 1) * stepDepth;

        // Vertical part
        AddQuadWithNormal(data,
            glm::vec3(-halfWidth, 0, z0), glm::vec3(-halfWidth, y1, z0),
            glm::vec3(-halfWidth, y1, z1), glm::vec3(-halfWidth, 0, z1),
            glm::vec2(0, 0), glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0));
    }

    // Right side
    for (int i = 0; i < steps; ++i) {
        float y0 = i * stepHeight;
        float y1 = (i + 1) * stepHeight;
        float z0 = -halfWidth + i * stepDepth;
        float z1 = -halfWidth + (i + 1) * stepDepth;

        AddQuadWithNormal(data,
            glm::vec3(halfWidth, 0, z1), glm::vec3(halfWidth, y1, z1),
            glm::vec3(halfWidth, y1, z0), glm::vec3(halfWidth, 0, z0),
            glm::vec2(0, 0), glm::vec2(0, 1), glm::vec2(1, 1), glm::vec2(1, 0));
    }

    // Bottom face
    AddQuadWithNormal(data,
        glm::vec3(-halfWidth, 0, halfWidth), glm::vec3(halfWidth, 0, halfWidth),
        glm::vec3(halfWidth, 0, -halfWidth), glm::vec3(-halfWidth, 0, -halfWidth),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    return data;
}

MeshData ProceduralModels::CreateTorus(float innerRadius, float outerRadius, int rings, int segments) {
    MeshData data;

    float tubeRadius = outerRadius;

    for (int ring = 0; ring <= rings; ++ring) {
        float theta = TWO_PI * static_cast<float>(ring) / rings;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        for (int seg = 0; seg <= segments; ++seg) {
            float phi = TWO_PI * static_cast<float>(seg) / segments;
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);

            float x = (innerRadius + tubeRadius * cosPhi) * cosTheta;
            float y = tubeRadius * sinPhi;
            float z = (innerRadius + tubeRadius * cosPhi) * sinTheta;

            glm::vec3 center(innerRadius * cosTheta, 0, innerRadius * sinTheta);
            glm::vec3 pos(x, y, z);
            glm::vec3 normal = glm::normalize(pos - center);
            glm::vec2 uv(static_cast<float>(ring) / rings, static_cast<float>(seg) / segments);

            data.AddVertex(pos, normal, uv);
        }
    }

    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            data.AddTriangle(current, next + 1, next);
            data.AddTriangle(current, current + 1, next + 1);
        }
    }

    return data;
}

MeshData ProceduralModels::CreatePlane(float width, float depth, int subdivX, int subdivZ) {
    MeshData data;

    float hw = width * 0.5f;
    float hd = depth * 0.5f;

    float stepX = width / subdivX;
    float stepZ = depth / subdivZ;

    glm::vec3 normal(0, 1, 0);

    // Generate vertices
    for (int z = 0; z <= subdivZ; ++z) {
        for (int x = 0; x <= subdivX; ++x) {
            float px = -hw + x * stepX;
            float pz = -hd + z * stepZ;
            float u = static_cast<float>(x) / subdivX;
            float v = static_cast<float>(z) / subdivZ;

            data.AddVertex(glm::vec3(px, 0, pz), normal, glm::vec2(u, v));
        }
    }

    // Generate indices
    for (int z = 0; z < subdivZ; ++z) {
        for (int x = 0; x < subdivX; ++x) {
            uint32_t bl = z * (subdivX + 1) + x;
            uint32_t br = bl + 1;
            uint32_t tl = bl + subdivX + 1;
            uint32_t tr = tl + 1;

            data.AddQuad(bl, br, tr, tl);
        }
    }

    return data;
}

MeshData ProceduralModels::CreateCapsule(float radius, float height, int segments) {
    MeshData data;

    float cylinderHeight = height - 2.0f * radius;
    if (cylinderHeight < 0) cylinderHeight = 0;

    float halfCylinder = cylinderHeight * 0.5f;
    int halfSegments = segments / 2;

    // Top hemisphere
    for (int ring = 0; ring <= halfSegments; ++ring) {
        float phi = HALF_PI * static_cast<float>(ring) / halfSegments;
        float y = radius * std::cos(phi) + halfCylinder;
        float ringRadius = radius * std::sin(phi);

        for (int seg = 0; seg <= segments; ++seg) {
            float theta = TWO_PI * static_cast<float>(seg) / segments;
            float x = ringRadius * std::cos(theta);
            float z = ringRadius * std::sin(theta);

            glm::vec3 pos(x, y, z);
            glm::vec3 sphereCenter(0, halfCylinder, 0);
            glm::vec3 normal = glm::normalize(pos - sphereCenter);
            glm::vec2 uv(static_cast<float>(seg) / segments, 0.5f + 0.25f * (1.0f - static_cast<float>(ring) / halfSegments));

            data.AddVertex(pos, normal, uv);
        }
    }

    // Cylinder body
    int topHemiVerts = (halfSegments + 1) * (segments + 1);
    for (int seg = 0; seg <= segments; ++seg) {
        float theta = TWO_PI * static_cast<float>(seg) / segments;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);

        glm::vec3 normal(std::cos(theta), 0, std::sin(theta));

        // Top ring
        data.AddVertex(glm::vec3(x, halfCylinder, z), normal, glm::vec2(static_cast<float>(seg) / segments, 0.5f));
        // Bottom ring
        data.AddVertex(glm::vec3(x, -halfCylinder, z), normal, glm::vec2(static_cast<float>(seg) / segments, 0.5f));
    }

    // Bottom hemisphere
    int cylinderVerts = 2 * (segments + 1);
    for (int ring = 0; ring <= halfSegments; ++ring) {
        float phi = HALF_PI + HALF_PI * static_cast<float>(ring) / halfSegments;
        float y = radius * std::cos(phi) - halfCylinder;
        float ringRadius = radius * std::sin(phi);

        for (int seg = 0; seg <= segments; ++seg) {
            float theta = TWO_PI * static_cast<float>(seg) / segments;
            float x = ringRadius * std::cos(theta);
            float z = ringRadius * std::sin(theta);

            glm::vec3 pos(x, y, z);
            glm::vec3 sphereCenter(0, -halfCylinder, 0);
            glm::vec3 normal = glm::normalize(pos - sphereCenter);
            glm::vec2 uv(static_cast<float>(seg) / segments, 0.25f * static_cast<float>(ring) / halfSegments);

            data.AddVertex(pos, normal, uv);
        }
    }

    // Generate indices for top hemisphere
    for (int ring = 0; ring < halfSegments; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            data.AddTriangle(current, next, current + 1);
            data.AddTriangle(current + 1, next, next + 1);
        }
    }

    // Generate indices for cylinder
    for (int seg = 0; seg < segments; ++seg) {
        uint32_t top = topHemiVerts + seg * 2;
        uint32_t bottom = top + 1;
        uint32_t topNext = top + 2;
        uint32_t bottomNext = bottom + 2;

        data.AddTriangle(top, bottom, topNext);
        data.AddTriangle(topNext, bottom, bottomNext);
    }

    // Generate indices for bottom hemisphere
    uint32_t bottomStart = topHemiVerts + cylinderVerts;
    for (int ring = 0; ring < halfSegments; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current = bottomStart + ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            data.AddTriangle(current, next, current + 1);
            data.AddTriangle(current + 1, next, next + 1);
        }
    }

    return data;
}

// ============================================================================
// Building Components
// ============================================================================

MeshData ProceduralModels::CreateWall(float width, float height, float thickness) {
    return CreateCube(glm::vec3(width, height, thickness));
}

MeshData ProceduralModels::CreateWallWithDoor(float width, float height, float doorWidth, float doorHeight,
                                              float doorOffset) {
    MeshData data;

    float hw = width * 0.5f;
    float ht = 0.1f;  // Wall thickness

    // Door boundaries
    float doorLeft = doorOffset - doorWidth * 0.5f;
    float doorRight = doorOffset + doorWidth * 0.5f;

    // Left section
    if (doorLeft > -hw) {
        MeshData left = CreateCube(glm::vec3(doorLeft + hw, height, ht * 2));
        left.Transform(glm::translate(glm::mat4(1.0f), glm::vec3((-hw + doorLeft) * 0.5f, height * 0.5f, 0)));
        data.Merge(left);
    }

    // Right section
    if (doorRight < hw) {
        MeshData right = CreateCube(glm::vec3(hw - doorRight, height, ht * 2));
        right.Transform(glm::translate(glm::mat4(1.0f), glm::vec3((hw + doorRight) * 0.5f, height * 0.5f, 0)));
        data.Merge(right);
    }

    // Top section (above door)
    if (doorHeight < height) {
        MeshData top = CreateCube(glm::vec3(doorWidth, height - doorHeight, ht * 2));
        top.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(doorOffset, doorHeight + (height - doorHeight) * 0.5f, 0)));
        data.Merge(top);
    }

    return data;
}

MeshData ProceduralModels::CreateWallWithWindow(float width, float height, float windowWidth, float windowHeight,
                                                 float windowY, float windowOffset) {
    MeshData data;

    float hw = width * 0.5f;
    float ht = 0.1f;

    float winLeft = windowOffset - windowWidth * 0.5f;
    float winRight = windowOffset + windowWidth * 0.5f;
    float winBottom = windowY;
    float winTop = windowY + windowHeight;

    // Bottom section (below window)
    if (winBottom > 0) {
        MeshData bottom = CreateCube(glm::vec3(width, winBottom, ht * 2));
        bottom.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(0, winBottom * 0.5f, 0)));
        data.Merge(bottom);
    }

    // Top section (above window)
    if (winTop < height) {
        MeshData top = CreateCube(glm::vec3(width, height - winTop, ht * 2));
        top.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(0, winTop + (height - winTop) * 0.5f, 0)));
        data.Merge(top);
    }

    // Left section (beside window)
    if (winLeft > -hw) {
        MeshData left = CreateCube(glm::vec3(winLeft + hw, windowHeight, ht * 2));
        left.Transform(glm::translate(glm::mat4(1.0f), glm::vec3((-hw + winLeft) * 0.5f, winBottom + windowHeight * 0.5f, 0)));
        data.Merge(left);
    }

    // Right section
    if (winRight < hw) {
        MeshData right = CreateCube(glm::vec3(hw - winRight, windowHeight, ht * 2));
        right.Transform(glm::translate(glm::mat4(1.0f), glm::vec3((hw + winRight) * 0.5f, winBottom + windowHeight * 0.5f, 0)));
        data.Merge(right);
    }

    return data;
}

MeshData ProceduralModels::CreateDoorFrame(float width, float height, float frameThickness, float frameDepth) {
    MeshData data;

    // Left post
    MeshData left = CreateCube(glm::vec3(frameThickness, height, frameDepth));
    left.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(-width * 0.5f - frameThickness * 0.5f, height * 0.5f, 0)));
    data.Merge(left);

    // Right post
    MeshData right = CreateCube(glm::vec3(frameThickness, height, frameDepth));
    right.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(width * 0.5f + frameThickness * 0.5f, height * 0.5f, 0)));
    data.Merge(right);

    // Top beam
    MeshData top = CreateCube(glm::vec3(width + frameThickness * 2, frameThickness, frameDepth));
    top.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(0, height + frameThickness * 0.5f, 0)));
    data.Merge(top);

    return data;
}

MeshData ProceduralModels::CreateWindowFrame(float width, float height, float frameThickness) {
    MeshData data;

    float depth = frameThickness;

    // Left
    MeshData left = CreateCube(glm::vec3(frameThickness, height + frameThickness * 2, depth));
    left.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(-width * 0.5f - frameThickness * 0.5f, height * 0.5f, 0)));
    data.Merge(left);

    // Right
    MeshData right = CreateCube(glm::vec3(frameThickness, height + frameThickness * 2, depth));
    right.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(width * 0.5f + frameThickness * 0.5f, height * 0.5f, 0)));
    data.Merge(right);

    // Top
    MeshData top = CreateCube(glm::vec3(width, frameThickness, depth));
    top.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(0, height + frameThickness * 0.5f, 0)));
    data.Merge(top);

    // Bottom
    MeshData bottom = CreateCube(glm::vec3(width, frameThickness, depth));
    bottom.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(0, -frameThickness * 0.5f, 0)));
    data.Merge(bottom);

    return data;
}

MeshData ProceduralModels::CreateRoof(float width, float depth, float height, RoofType type, float overhang) {
    MeshData data;

    float hw = (width + overhang * 2) * 0.5f;
    float hd = (depth + overhang * 2) * 0.5f;

    switch (type) {
        case RoofType::Flat: {
            data = CreateCube(glm::vec3(width + overhang * 2, 0.1f, depth + overhang * 2));
            data.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.05f, 0)));
            break;
        }

        case RoofType::Gabled: {
            // Two sloped sides
            AddQuadWithNormal(data,
                glm::vec3(-hw, 0, hd), glm::vec3(hw, 0, hd),
                glm::vec3(hw, height, 0), glm::vec3(-hw, height, 0),
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

            AddQuadWithNormal(data,
                glm::vec3(hw, 0, -hd), glm::vec3(-hw, 0, -hd),
                glm::vec3(-hw, height, 0), glm::vec3(hw, height, 0),
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

            // Gable ends (triangles)
            AddTriangleWithNormal(data,
                glm::vec3(-hw, 0, hd), glm::vec3(-hw, height, 0), glm::vec3(-hw, 0, -hd),
                glm::vec2(0, 0), glm::vec2(0.5f, 1), glm::vec2(1, 0));

            AddTriangleWithNormal(data,
                glm::vec3(hw, 0, -hd), glm::vec3(hw, height, 0), glm::vec3(hw, 0, hd),
                glm::vec2(0, 0), glm::vec2(0.5f, 1), glm::vec2(1, 0));
            break;
        }

        case RoofType::Pyramidal: {
            glm::vec3 apex(0, height, 0);

            // Four triangular faces
            AddTriangleWithNormal(data,
                glm::vec3(-hw, 0, hd), glm::vec3(hw, 0, hd), apex,
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0.5f, 1));

            AddTriangleWithNormal(data,
                glm::vec3(hw, 0, hd), glm::vec3(hw, 0, -hd), apex,
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0.5f, 1));

            AddTriangleWithNormal(data,
                glm::vec3(hw, 0, -hd), glm::vec3(-hw, 0, -hd), apex,
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0.5f, 1));

            AddTriangleWithNormal(data,
                glm::vec3(-hw, 0, -hd), glm::vec3(-hw, 0, hd), apex,
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0.5f, 1));
            break;
        }

        case RoofType::Shed: {
            // Single slope
            AddQuadWithNormal(data,
                glm::vec3(-hw, 0, hd), glm::vec3(hw, 0, hd),
                glm::vec3(hw, height, -hd), glm::vec3(-hw, height, -hd),
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

            // Sides
            AddTriangleWithNormal(data,
                glm::vec3(-hw, 0, hd), glm::vec3(-hw, height, -hd), glm::vec3(-hw, 0, -hd),
                glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(1, 0));

            AddTriangleWithNormal(data,
                glm::vec3(hw, 0, -hd), glm::vec3(hw, height, -hd), glm::vec3(hw, 0, hd),
                glm::vec2(0, 0), glm::vec2(0, 1), glm::vec2(1, 0));

            // Back vertical
            AddQuadWithNormal(data,
                glm::vec3(hw, 0, -hd), glm::vec3(-hw, 0, -hd),
                glm::vec3(-hw, height, -hd), glm::vec3(hw, height, -hd),
                glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));
            break;
        }

        default:
            // Default to flat
            data = CreateCube(glm::vec3(width + overhang * 2, 0.1f, depth + overhang * 2));
            break;
    }

    return data;
}

MeshData ProceduralModels::CreateFloor(float width, float depth, float thickness) {
    return CreateCube(glm::vec3(width, thickness, depth));
}

MeshData ProceduralModels::CreateBeam(float width, float height, float depth) {
    return CreateCube(glm::vec3(width, height, depth));
}

// ============================================================================
// Hex-Specific
// ============================================================================

glm::vec3 ProceduralModels::GetHexVertex(const glm::vec2& center, float radius, int corner, float y) {
    float angle = HEX_ANGLE_OFFSET + (TWO_PI / 6.0f) * corner;
    return glm::vec3(
        center.x + radius * std::cos(angle),
        y,
        center.y + radius * std::sin(angle)
    );
}

glm::vec3 ProceduralModels::GetHexVertexPointy(const glm::vec2& center, float radius, int corner, float y) {
    float angle = (TWO_PI / 6.0f) * corner;  // No offset for pointy-top
    return glm::vec3(
        center.x + radius * std::cos(angle),
        y,
        center.y + radius * std::sin(angle)
    );
}

MeshData ProceduralModels::CreateHexTile(float radius, float height) {
    return CreateHexPrism(radius, height);
}

MeshData ProceduralModels::CreateHexTilePointy(float radius, float height) {
    MeshData data;

    float halfHeight = height * 0.5f;
    glm::vec2 center(0, 0);

    // Generate vertices
    std::vector<glm::vec3> topVerts, bottomVerts;
    for (int i = 0; i < 6; ++i) {
        topVerts.push_back(GetHexVertexPointy(center, radius, i, halfHeight));
        bottomVerts.push_back(GetHexVertexPointy(center, radius, i, -halfHeight));
    }

    // Top face
    glm::vec3 topCenter(0, halfHeight, 0);
    glm::vec3 topNormal(0, 1, 0);
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;
        uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

        float angle1 = (TWO_PI / 6.0f) * i;
        float angle2 = (TWO_PI / 6.0f) * next;

        data.AddVertex(topCenter, topNormal, glm::vec2(0.5f, 0.5f));
        data.AddVertex(topVerts[i], topNormal, glm::vec2(0.5f + 0.5f * std::cos(angle1), 0.5f + 0.5f * std::sin(angle1)));
        data.AddVertex(topVerts[next], topNormal, glm::vec2(0.5f + 0.5f * std::cos(angle2), 0.5f + 0.5f * std::sin(angle2)));

        data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
    }

    // Bottom face
    glm::vec3 bottomCenter(0, -halfHeight, 0);
    glm::vec3 bottomNormal(0, -1, 0);
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;
        uint32_t baseIdx = static_cast<uint32_t>(data.GetVertexCount());

        float angle1 = (TWO_PI / 6.0f) * next;
        float angle2 = (TWO_PI / 6.0f) * i;

        data.AddVertex(bottomCenter, bottomNormal, glm::vec2(0.5f, 0.5f));
        data.AddVertex(bottomVerts[next], bottomNormal, glm::vec2(0.5f + 0.5f * std::cos(angle1), 0.5f + 0.5f * std::sin(angle1)));
        data.AddVertex(bottomVerts[i], bottomNormal, glm::vec2(0.5f + 0.5f * std::cos(angle2), 0.5f + 0.5f * std::sin(angle2)));

        data.AddTriangle(baseIdx, baseIdx + 1, baseIdx + 2);
    }

    // Sides
    for (int i = 0; i < 6; ++i) {
        int next = (i + 1) % 6;
        float u0 = static_cast<float>(i) / 6.0f;
        float u1 = static_cast<float>(i + 1) / 6.0f;

        AddQuadWithNormal(data,
            bottomVerts[i], bottomVerts[next], topVerts[next], topVerts[i],
            glm::vec2(u0, 0), glm::vec2(u1, 0), glm::vec2(u1, 1), glm::vec2(u0, 1));
    }

    return data;
}

MeshData ProceduralModels::CreateHexWall(float radius, float height, int side) {
    MeshData data;

    side = side % 6;
    int nextSide = (side + 1) % 6;

    glm::vec2 center(0, 0);
    glm::vec3 v0 = GetHexVertex(center, radius, side, 0);
    glm::vec3 v1 = GetHexVertex(center, radius, nextSide, 0);
    glm::vec3 v2 = GetHexVertex(center, radius, nextSide, height);
    glm::vec3 v3 = GetHexVertex(center, radius, side, height);

    // Calculate wall width for UV
    float wallWidth = glm::length(glm::vec2(v1.x - v0.x, v1.z - v0.z));

    // Front face (outward)
    AddQuadWithNormal(data, v0, v1, v2, v3,
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Back face (inward)
    AddQuadWithNormal(data, v1, v0, v3, v2,
        glm::vec2(1, 0), glm::vec2(0, 0), glm::vec2(0, 1), glm::vec2(1, 1));

    return data;
}

MeshData ProceduralModels::CreateHexCorner(float radius, float height, int corner, float pillarRadius) {
    MeshData data;

    corner = corner % 6;
    glm::vec2 center(0, 0);
    glm::vec3 cornerPos = GetHexVertex(center, radius, corner, 0);

    // Create a small cylinder at the corner
    MeshData pillar = CreateCylinder(pillarRadius, height, 8);
    pillar.Transform(glm::translate(glm::mat4(1.0f), glm::vec3(cornerPos.x, height * 0.5f, cornerPos.z)));

    return pillar;
}

MeshData ProceduralModels::CreateHexRamp(float radius, float startHeight, float endHeight, int side) {
    MeshData data;

    side = side % 6;
    int nextSide = (side + 1) % 6;

    glm::vec2 center(0, 0);

    // Get the two vertices of the edge facing outward
    glm::vec3 v0 = GetHexVertex(center, radius, side, startHeight);
    glm::vec3 v1 = GetHexVertex(center, radius, nextSide, startHeight);

    // Center of hex at both heights
    glm::vec3 centerLow(0, startHeight, 0);
    glm::vec3 centerHigh(0, endHeight, 0);

    // Ramp surface
    AddQuadWithNormal(data, v0, v1, glm::vec3(v1.x, endHeight, v1.z), glm::vec3(v0.x, endHeight, v0.z),
        glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

    // Side triangles
    AddTriangleWithNormal(data,
        glm::vec3(v0.x, startHeight, v0.z), glm::vec3(v0.x, endHeight, v0.z), centerHigh,
        glm::vec2(0, 0), glm::vec2(0, 1), glm::vec2(0.5f, 1));

    AddTriangleWithNormal(data,
        glm::vec3(v1.x, endHeight, v1.z), glm::vec3(v1.x, startHeight, v1.z), centerHigh,
        glm::vec2(1, 1), glm::vec2(1, 0), glm::vec2(0.5f, 1));

    return data;
}

MeshData ProceduralModels::CreateHexStairs(float radius, float height, int steps, int side) {
    MeshData data;

    if (steps < 1) steps = 1;

    side = side % 6;
    int nextSide = (side + 1) % 6;

    glm::vec2 center(0, 0);
    glm::vec3 edgeV0 = GetHexVertex(center, radius, side, 0);
    glm::vec3 edgeV1 = GetHexVertex(center, radius, nextSide, 0);

    float stepHeight = height / steps;

    for (int i = 0; i < steps; ++i) {
        float t0 = static_cast<float>(i) / steps;
        float t1 = static_cast<float>(i + 1) / steps;

        float y0 = i * stepHeight;
        float y1 = (i + 1) * stepHeight;

        // Interpolate positions from center toward edge
        glm::vec3 innerV0 = glm::mix(glm::vec3(0, y1, 0), edgeV0, t0);
        glm::vec3 innerV1 = glm::mix(glm::vec3(0, y1, 0), edgeV1, t0);
        glm::vec3 outerV0 = glm::mix(glm::vec3(0, y1, 0), edgeV0, t1);
        glm::vec3 outerV1 = glm::mix(glm::vec3(0, y1, 0), edgeV1, t1);

        innerV0.y = y1;
        innerV1.y = y1;
        outerV0.y = y1;
        outerV1.y = y1;

        // Step tread (top)
        AddQuadWithNormal(data, innerV0, innerV1, outerV1, outerV0,
            glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));

        // Step riser (front)
        glm::vec3 riserInner0 = innerV0; riserInner0.y = y0;
        glm::vec3 riserInner1 = innerV1; riserInner1.y = y0;

        AddQuadWithNormal(data, riserInner0, riserInner1, innerV1, innerV0,
            glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));
    }

    return data;
}

// ============================================================================
// Utility
// ============================================================================

std::unique_ptr<Nova::Mesh> ProceduralModels::CreateMeshFromData(const MeshData& data) {
    auto mesh = std::make_unique<Nova::Mesh>();

    if (data.GetVertexCount() == 0 || data.indices.empty()) {
        return mesh;
    }

    // Convert to Nova::Vertex format
    std::vector<Nova::Vertex> vertices;
    vertices.reserve(data.GetVertexCount());

    for (size_t i = 0; i < data.GetVertexCount(); ++i) {
        size_t offset = i * MeshData::FLOATS_PER_VERTEX;

        Nova::Vertex v;
        v.position = glm::vec3(data.vertices[offset], data.vertices[offset + 1], data.vertices[offset + 2]);
        v.normal = glm::vec3(data.vertices[offset + 3], data.vertices[offset + 4], data.vertices[offset + 5]);
        v.texCoords = glm::vec2(data.vertices[offset + 6], data.vertices[offset + 7]);
        v.tangent = glm::vec3(0);
        v.bitangent = glm::vec3(0);

        vertices.push_back(v);
    }

    mesh->Create(vertices, data.indices);

    return mesh;
}

} // namespace Vehement
