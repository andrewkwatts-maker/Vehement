#include "graphics/Mesh.hpp"

#include <glad/gl.h>
#include <cmath>
#include <algorithm>

namespace Nova {

Mesh::Mesh() = default;

Mesh::~Mesh() {
    Cleanup();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_vertexCount(other.m_vertexCount)
    , m_indexCount(other.m_indexCount)
    , m_boundsMin(other.m_boundsMin)
    , m_boundsMax(other.m_boundsMax)
{
    other.m_vao = other.m_vbo = other.m_ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_vertexCount = other.m_vertexCount;
        m_indexCount = other.m_indexCount;
        m_boundsMin = other.m_boundsMin;
        m_boundsMax = other.m_boundsMax;
        other.m_vao = other.m_vbo = other.m_ebo = 0;
    }
    return *this;
}

void Mesh::Create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    Cleanup();
    CalculateBounds(vertices);
    SetupMesh(vertices, indices);
}

void Mesh::SetupMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // TexCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    // Tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

    // Bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));

    // Bone IDs
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, boneIDs));

    // Bone Weights
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, boneWeights));

    glBindVertexArray(0);
}

void Mesh::CreateFromRaw(const float* vertices, size_t vertexCount,
                         const uint32_t* indices, size_t indexCount,
                         bool hasNormals, bool hasTexCoords, bool hasTangents) {
    // Convert to Vertex format
    int stride = 3;  // position
    if (hasNormals) stride += 3;
    if (hasTexCoords) stride += 2;
    if (hasTangents) stride += 6;

    std::vector<Vertex> vertexData(vertexCount);
    for (size_t i = 0; i < vertexCount; ++i) {
        const float* v = vertices + i * stride;
        int offset = 0;

        vertexData[i].position = glm::vec3(v[0], v[1], v[2]);
        offset += 3;

        if (hasNormals) {
            vertexData[i].normal = glm::vec3(v[offset], v[offset+1], v[offset+2]);
            offset += 3;
        }

        if (hasTexCoords) {
            vertexData[i].texCoords = glm::vec2(v[offset], v[offset+1]);
            offset += 2;
        }

        if (hasTangents) {
            vertexData[i].tangent = glm::vec3(v[offset], v[offset+1], v[offset+2]);
            offset += 3;
            vertexData[i].bitangent = glm::vec3(v[offset], v[offset+1], v[offset+2]);
        }
    }

    std::vector<uint32_t> indexData(indices, indices + indexCount);
    Create(vertexData, indexData);
}

void Mesh::Draw() const {
    if (!m_vao) return;

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    // Note: Deliberately not unbinding VAO - the next draw call will bind its own VAO.
    // Unbinding after every draw is redundant and hurts performance.
}

void Mesh::DrawInstanced(int instanceCount) const {
    if (!m_vao) return;

    glBindVertexArray(m_vao);
    glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr, instanceCount);
}

void Mesh::Cleanup() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
}

void Mesh::CalculateBounds(const std::vector<Vertex>& vertices) {
    if (vertices.empty()) return;

    m_boundsMin = m_boundsMax = vertices[0].position;
    for (const auto& v : vertices) {
        m_boundsMin = glm::min(m_boundsMin, v.position);
        m_boundsMax = glm::max(m_boundsMax, v.position);
    }
}

std::unique_ptr<Mesh> Mesh::CreateCube(float size) {
    float h = size * 0.5f;

    std::vector<Vertex> vertices = {
        // Front face
        {{-h, -h,  h}, { 0,  0,  1}, {0, 0}, {1, 0, 0}, {0, 1, 0}},
        {{ h, -h,  h}, { 0,  0,  1}, {1, 0}, {1, 0, 0}, {0, 1, 0}},
        {{ h,  h,  h}, { 0,  0,  1}, {1, 1}, {1, 0, 0}, {0, 1, 0}},
        {{-h,  h,  h}, { 0,  0,  1}, {0, 1}, {1, 0, 0}, {0, 1, 0}},
        // Back face
        {{ h, -h, -h}, { 0,  0, -1}, {0, 0}, {-1, 0, 0}, {0, 1, 0}},
        {{-h, -h, -h}, { 0,  0, -1}, {1, 0}, {-1, 0, 0}, {0, 1, 0}},
        {{-h,  h, -h}, { 0,  0, -1}, {1, 1}, {-1, 0, 0}, {0, 1, 0}},
        {{ h,  h, -h}, { 0,  0, -1}, {0, 1}, {-1, 0, 0}, {0, 1, 0}},
        // Top face
        {{-h,  h,  h}, { 0,  1,  0}, {0, 0}, {1, 0, 0}, {0, 0, -1}},
        {{ h,  h,  h}, { 0,  1,  0}, {1, 0}, {1, 0, 0}, {0, 0, -1}},
        {{ h,  h, -h}, { 0,  1,  0}, {1, 1}, {1, 0, 0}, {0, 0, -1}},
        {{-h,  h, -h}, { 0,  1,  0}, {0, 1}, {1, 0, 0}, {0, 0, -1}},
        // Bottom face
        {{-h, -h, -h}, { 0, -1,  0}, {0, 0}, {1, 0, 0}, {0, 0, 1}},
        {{ h, -h, -h}, { 0, -1,  0}, {1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{ h, -h,  h}, { 0, -1,  0}, {1, 1}, {1, 0, 0}, {0, 0, 1}},
        {{-h, -h,  h}, { 0, -1,  0}, {0, 1}, {1, 0, 0}, {0, 0, 1}},
        // Right face
        {{ h, -h,  h}, { 1,  0,  0}, {0, 0}, {0, 0, -1}, {0, 1, 0}},
        {{ h, -h, -h}, { 1,  0,  0}, {1, 0}, {0, 0, -1}, {0, 1, 0}},
        {{ h,  h, -h}, { 1,  0,  0}, {1, 1}, {0, 0, -1}, {0, 1, 0}},
        {{ h,  h,  h}, { 1,  0,  0}, {0, 1}, {0, 0, -1}, {0, 1, 0}},
        // Left face
        {{-h, -h, -h}, {-1,  0,  0}, {0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{-h, -h,  h}, {-1,  0,  0}, {1, 0}, {0, 0, 1}, {0, 1, 0}},
        {{-h,  h,  h}, {-1,  0,  0}, {1, 1}, {0, 0, 1}, {0, 1, 0}},
        {{-h,  h, -h}, {-1,  0,  0}, {0, 1}, {0, 0, 1}, {0, 1, 0}},
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 0, 2, 3,       // Front
        4, 5, 6, 4, 6, 7,       // Back
        8, 9, 10, 8, 10, 11,    // Top
        12, 13, 14, 12, 14, 15, // Bottom
        16, 17, 18, 16, 18, 19, // Right
        20, 21, 22, 20, 22, 23  // Left
    };

    auto mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Mesh> Mesh::CreateSphere(float radius, int segments) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (int lat = 0; lat <= segments; ++lat) {
        float theta = lat * glm::pi<float>() / segments;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= segments; ++lon) {
            float phi = lon * 2.0f * glm::pi<float>() / segments;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            glm::vec3 normal(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
            glm::vec3 position = normal * radius;
            glm::vec2 texCoord(static_cast<float>(lon) / segments,
                              static_cast<float>(lat) / segments);

            // Calculate tangent
            glm::vec3 tangent(-sinPhi, 0.0f, cosPhi);
            glm::vec3 bitangent = glm::cross(normal, tangent);

            vertices.push_back({position, normal, texCoord, tangent, bitangent});
        }
    }

    for (int lat = 0; lat < segments; ++lat) {
        for (int lon = 0; lon < segments; ++lon) {
            uint32_t current = lat * (segments + 1) + lon;
            uint32_t next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Mesh> Mesh::CreatePlane(float width, float height, int divisionsX, int divisionsY) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    for (int y = 0; y <= divisionsY; ++y) {
        for (int x = 0; x <= divisionsX; ++x) {
            float u = static_cast<float>(x) / divisionsX;
            float v = static_cast<float>(y) / divisionsY;

            glm::vec3 position(-halfW + u * width, 0.0f, -halfH + v * height);
            glm::vec3 normal(0.0f, 1.0f, 0.0f);
            glm::vec2 texCoord(u, v);
            glm::vec3 tangent(1.0f, 0.0f, 0.0f);
            glm::vec3 bitangent(0.0f, 0.0f, 1.0f);

            vertices.push_back({position, normal, texCoord, tangent, bitangent});
        }
    }

    for (int y = 0; y < divisionsY; ++y) {
        for (int x = 0; x < divisionsX; ++x) {
            uint32_t topLeft = y * (divisionsX + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = topLeft + divisionsX + 1;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Mesh> Mesh::CreateCylinder(float radius, float height, int segments) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float halfH = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle);
        float z = std::sin(angle);

        glm::vec3 normal(x, 0.0f, z);
        glm::vec3 tangent(-z, 0.0f, x);

        // Bottom
        vertices.push_back({
            glm::vec3(x * radius, -halfH, z * radius),
            normal,
            glm::vec2(static_cast<float>(i) / segments, 0.0f),
            tangent,
            glm::vec3(0.0f, 1.0f, 0.0f)
        });

        // Top
        vertices.push_back({
            glm::vec3(x * radius, halfH, z * radius),
            normal,
            glm::vec2(static_cast<float>(i) / segments, 1.0f),
            tangent,
            glm::vec3(0.0f, 1.0f, 0.0f)
        });
    }

    // Side indices
    for (int i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 1);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    // Caps
    uint32_t bottomCenter = static_cast<uint32_t>(vertices.size());
    vertices.push_back({glm::vec3(0, -halfH, 0), glm::vec3(0, -1, 0), glm::vec2(0.5f, 0.5f), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1)});

    uint32_t topCenter = static_cast<uint32_t>(vertices.size());
    vertices.push_back({glm::vec3(0, halfH, 0), glm::vec3(0, 1, 0), glm::vec2(0.5f, 0.5f), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1)});

    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle);
        float z = std::sin(angle);

        // Bottom cap
        vertices.push_back({
            glm::vec3(x * radius, -halfH, z * radius),
            glm::vec3(0, -1, 0),
            glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f),
            glm::vec3(1, 0, 0),
            glm::vec3(0, 0, 1)
        });

        // Top cap
        vertices.push_back({
            glm::vec3(x * radius, halfH, z * radius),
            glm::vec3(0, 1, 0),
            glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f),
            glm::vec3(1, 0, 0),
            glm::vec3(0, 0, -1)
        });
    }

    uint32_t capStart = bottomCenter + 2;
    for (int i = 0; i < segments; ++i) {
        // Bottom cap
        indices.push_back(bottomCenter);
        indices.push_back(capStart + i * 2 + 2);
        indices.push_back(capStart + i * 2);

        // Top cap
        indices.push_back(topCenter);
        indices.push_back(capStart + i * 2 + 1);
        indices.push_back(capStart + i * 2 + 3);
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Mesh> Mesh::CreateCone(float radius, float height, int segments) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Apex
    uint32_t apexIndex = 0;
    vertices.push_back({glm::vec3(0, height, 0), glm::vec3(0, 1, 0), glm::vec2(0.5f, 1.0f), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1)});

    // Base vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle);
        float z = std::sin(angle);

        glm::vec3 sideNormal = glm::normalize(glm::vec3(x, radius / height, z));

        vertices.push_back({
            glm::vec3(x * radius, 0, z * radius),
            sideNormal,
            glm::vec2(static_cast<float>(i) / segments, 0.0f),
            glm::vec3(-z, 0, x),
            glm::cross(sideNormal, glm::vec3(-z, 0, x))
        });
    }

    // Side triangles
    for (int i = 0; i < segments; ++i) {
        indices.push_back(apexIndex);
        indices.push_back(1 + i);
        indices.push_back(1 + i + 1);
    }

    // Base cap
    uint32_t baseCenter = static_cast<uint32_t>(vertices.size());
    vertices.push_back({glm::vec3(0, 0, 0), glm::vec3(0, -1, 0), glm::vec2(0.5f, 0.5f), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1)});

    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * glm::two_pi<float>();
        float x = std::cos(angle);
        float z = std::sin(angle);

        vertices.push_back({
            glm::vec3(x * radius, 0, z * radius),
            glm::vec3(0, -1, 0),
            glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f),
            glm::vec3(1, 0, 0),
            glm::vec3(0, 0, 1)
        });
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(baseCenter);
        indices.push_back(baseCenter + 1 + i + 1);
        indices.push_back(baseCenter + 1 + i);
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Mesh> Mesh::CreateTorus(float innerRadius, float outerRadius, int rings, int segments) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float tubeRadius = (outerRadius - innerRadius) * 0.5f;
    float centerRadius = innerRadius + tubeRadius;

    for (int ring = 0; ring <= rings; ++ring) {
        float theta = static_cast<float>(ring) / rings * glm::two_pi<float>();
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        for (int seg = 0; seg <= segments; ++seg) {
            float phi = static_cast<float>(seg) / segments * glm::two_pi<float>();
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);

            glm::vec3 position(
                (centerRadius + tubeRadius * cosPhi) * cosTheta,
                tubeRadius * sinPhi,
                (centerRadius + tubeRadius * cosPhi) * sinTheta
            );

            glm::vec3 normal(
                cosPhi * cosTheta,
                sinPhi,
                cosPhi * sinTheta
            );

            glm::vec2 texCoord(
                static_cast<float>(ring) / rings,
                static_cast<float>(seg) / segments
            );

            glm::vec3 tangent(-sinTheta, 0, cosTheta);
            glm::vec3 bitangent = glm::cross(normal, tangent);

            vertices.push_back({position, normal, texCoord, tangent, bitangent});
        }
    }

    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

} // namespace Nova
