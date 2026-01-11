/**
 * @file MenuSceneMeshes.cpp
 * @brief Implementation of hero and building mesh creation for main menu scene
 */

#include "MenuSceneMeshes.hpp"
#include "graphics/Mesh.hpp"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace MenuScene {

// Helper function to add a box to vertex/index arrays
static void AddBoxToMesh(std::vector<Nova::Vertex>& vertices, std::vector<uint32_t>& indices,
                        const glm::vec3& center, const glm::vec3& halfExtents) {
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
    float x = halfExtents.x, y = halfExtents.y, z = halfExtents.z;
    glm::vec3 c = center;

    // Front face
    vertices.push_back({{c.x - x, c.y - y, c.z + z}, {0,  0,  1}, {0, 0}, {1, 0, 0}, {0, 1, 0}});
    vertices.push_back({{c.x + x, c.y - y, c.z + z}, {0,  0,  1}, {1, 0}, {1, 0, 0}, {0, 1, 0}});
    vertices.push_back({{c.x + x, c.y + y, c.z + z}, {0,  0,  1}, {1, 1}, {1, 0, 0}, {0, 1, 0}});
    vertices.push_back({{c.x - x, c.y + y, c.z + z}, {0,  0,  1}, {0, 1}, {1, 0, 0}, {0, 1, 0}});

    // Back face
    vertices.push_back({{c.x + x, c.y - y, c.z - z}, {0,  0, -1}, {0, 0}, {-1, 0, 0}, {0, 1, 0}});
    vertices.push_back({{c.x - x, c.y - y, c.z - z}, {0,  0, -1}, {1, 0}, {-1, 0, 0}, {0, 1, 0}});
    vertices.push_back({{c.x - x, c.y + y, c.z - z}, {0,  0, -1}, {1, 1}, {-1, 0, 0}, {0, 1, 0}});
    vertices.push_back({{c.x + x, c.y + y, c.z - z}, {0,  0, -1}, {0, 1}, {-1, 0, 0}, {0, 1, 0}});

    // Top face
    vertices.push_back({{c.x - x, c.y + y, c.z + z}, {0,  1,  0}, {0, 0}, {1, 0, 0}, {0, 0, -1}});
    vertices.push_back({{c.x + x, c.y + y, c.z + z}, {0,  1,  0}, {1, 0}, {1, 0, 0}, {0, 0, -1}});
    vertices.push_back({{c.x + x, c.y + y, c.z - z}, {0,  1,  0}, {1, 1}, {1, 0, 0}, {0, 0, -1}});
    vertices.push_back({{c.x - x, c.y + y, c.z - z}, {0,  1,  0}, {0, 1}, {1, 0, 0}, {0, 0, -1}});

    // Bottom face
    vertices.push_back({{c.x - x, c.y - y, c.z - z}, {0, -1,  0}, {0, 0}, {1, 0, 0}, {0, 0, 1}});
    vertices.push_back({{c.x + x, c.y - y, c.z - z}, {0, -1,  0}, {1, 0}, {1, 0, 0}, {0, 0, 1}});
    vertices.push_back({{c.x + x, c.y - y, c.z + z}, {0, -1,  0}, {1, 1}, {1, 0, 0}, {0, 0, 1}});
    vertices.push_back({{c.x - x, c.y - y, c.z + z}, {0, -1,  0}, {0, 1}, {1, 0, 0}, {0, 0, 1}});

    // Right face
    vertices.push_back({{c.x + x, c.y - y, c.z + z}, {1,  0,  0}, {0, 0}, {0, 0, -1}, {0, 1, 0}});
    vertices.push_back({{c.x + x, c.y - y, c.z - z}, {1,  0,  0}, {1, 0}, {0, 0, -1}, {0, 1, 0}});
    vertices.push_back({{c.x + x, c.y + y, c.z - z}, {1,  0,  0}, {1, 1}, {0, 0, -1}, {0, 1, 0}});
    vertices.push_back({{c.x + x, c.y + y, c.z + z}, {1,  0,  0}, {0, 1}, {0, 0, -1}, {0, 1, 0}});

    // Left face
    vertices.push_back({{c.x - x, c.y - y, c.z - z}, {-1,  0,  0}, {0, 0}, {0, 0, 1}, {0, 1, 0}});
    vertices.push_back({{c.x - x, c.y - y, c.z + z}, {-1,  0,  0}, {1, 0}, {0, 0, 1}, {0, 1, 0}});
    vertices.push_back({{c.x - x, c.y + y, c.z + z}, {-1,  0,  0}, {1, 1}, {0, 0, 1}, {0, 1, 0}});
    vertices.push_back({{c.x - x, c.y + y, c.z - z}, {-1,  0,  0}, {0, 1}, {0, 0, 1}, {0, 1, 0}});

    // Add indices for all 6 faces
    for (int i = 0; i < 6; ++i) {
        uint32_t faceBase = baseIndex + i * 4;
        indices.push_back(faceBase + 0);
        indices.push_back(faceBase + 1);
        indices.push_back(faceBase + 2);
        indices.push_back(faceBase + 0);
        indices.push_back(faceBase + 2);
        indices.push_back(faceBase + 3);
    }
}

// Helper function to add a pyramid to vertex/index arrays
static void AddPyramidToMesh(std::vector<Nova::Vertex>& vertices, std::vector<uint32_t>& indices,
                            const glm::vec3& center, const glm::vec3& halfExtents) {
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
    float x = halfExtents.x, y = halfExtents.y, z = halfExtents.z;
    glm::vec3 c = center;

    // Base vertices
    glm::vec3 v0(c.x - x, c.y - y, c.z + z);
    glm::vec3 v1(c.x + x, c.y - y, c.z + z);
    glm::vec3 v2(c.x + x, c.y - y, c.z - z);
    glm::vec3 v3(c.x - x, c.y - y, c.z - z);
    glm::vec3 apex(c.x, c.y + y, c.z);

    // Bottom face
    glm::vec3 bottomNormal(0, -1, 0);
    vertices.push_back({v0, bottomNormal, {0, 0}, {1, 0, 0}, {0, 0, 1}});
    vertices.push_back({v1, bottomNormal, {1, 0}, {1, 0, 0}, {0, 0, 1}});
    vertices.push_back({v2, bottomNormal, {1, 1}, {1, 0, 0}, {0, 0, 1}});
    vertices.push_back({v3, bottomNormal, {0, 1}, {1, 0, 0}, {0, 0, 1}});

    // Side faces (each triangle shares the apex)
    // Front face
    glm::vec3 frontNormal = glm::normalize(glm::cross(v1 - v0, apex - v0));
    vertices.push_back({v0, frontNormal, {0, 0}, {1, 0, 0}, {0, 1, 0}});
    vertices.push_back({v1, frontNormal, {1, 0}, {1, 0, 0}, {0, 1, 0}});
    vertices.push_back({apex, frontNormal, {0.5f, 1}, {1, 0, 0}, {0, 1, 0}});

    // Right face
    glm::vec3 rightNormal = glm::normalize(glm::cross(v2 - v1, apex - v1));
    vertices.push_back({v1, rightNormal, {0, 0}, {0, 0, -1}, {0, 1, 0}});
    vertices.push_back({v2, rightNormal, {1, 0}, {0, 0, -1}, {0, 1, 0}});
    vertices.push_back({apex, rightNormal, {0.5f, 1}, {0, 0, -1}, {0, 1, 0}});

    // Back face
    glm::vec3 backNormal = glm::normalize(glm::cross(v3 - v2, apex - v2));
    vertices.push_back({v2, backNormal, {0, 0}, {-1, 0, 0}, {0, 1, 0}});
    vertices.push_back({v3, backNormal, {1, 0}, {-1, 0, 0}, {0, 1, 0}});
    vertices.push_back({apex, backNormal, {0.5f, 1}, {-1, 0, 0}, {0, 1, 0}});

    // Left face
    glm::vec3 leftNormal = glm::normalize(glm::cross(v0 - v3, apex - v3));
    vertices.push_back({v3, leftNormal, {0, 0}, {0, 0, 1}, {0, 1, 0}});
    vertices.push_back({v0, leftNormal, {1, 0}, {0, 0, 1}, {0, 1, 0}});
    vertices.push_back({apex, leftNormal, {0.5f, 1}, {0, 0, 1}, {0, 1, 0}});

    // Bottom quad indices
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);

    // Side triangles
    for (int i = 0; i < 4; ++i) {
        uint32_t triBase = baseIndex + 4 + i * 3;
        indices.push_back(triBase + 0);
        indices.push_back(triBase + 1);
        indices.push_back(triBase + 2);
    }
}

std::unique_ptr<Nova::Mesh> CreateHeroMesh() {
    std::vector<Nova::Vertex> vertices;
    std::vector<uint32_t> indices;

    // Torso (chest and abdomen)
    AddBoxToMesh(vertices, indices, glm::vec3(0, 1.0f, 0), glm::vec3(0.4f, 0.8f, 0.3f));

    // Head
    AddBoxToMesh(vertices, indices, glm::vec3(0, 2.0f, 0), glm::vec3(0.25f, 0.3f, 0.25f));

    // Right arm (raised holding weapon) - angled upward
    AddBoxToMesh(vertices, indices, glm::vec3(0.6f, 1.7f, 0), glm::vec3(0.15f, 0.6f, 0.15f));

    // Left arm (at side)
    AddBoxToMesh(vertices, indices, glm::vec3(-0.6f, 1.2f, 0), glm::vec3(0.15f, 0.5f, 0.15f));

    // Right leg
    AddBoxToMesh(vertices, indices, glm::vec3(0.2f, 0.4f, 0), glm::vec3(0.15f, 0.4f, 0.15f));

    // Left leg
    AddBoxToMesh(vertices, indices, glm::vec3(-0.2f, 0.4f, 0), glm::vec3(0.15f, 0.4f, 0.15f));

    // Weapon (sword blade)
    AddBoxToMesh(vertices, indices, glm::vec3(0.9f, 2.5f, 0), glm::vec3(0.05f, 0.7f, 0.08f));

    // Sword handle
    AddBoxToMesh(vertices, indices, glm::vec3(0.75f, 1.8f, 0), glm::vec3(0.08f, 0.15f, 0.08f));

    // Shield on left arm
    AddBoxToMesh(vertices, indices, glm::vec3(-0.8f, 1.2f, 0.15f), glm::vec3(0.25f, 0.35f, 0.08f));

    auto mesh = std::make_unique<Nova::Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Nova::Mesh> CreateHouseMesh() {
    std::vector<Nova::Vertex> vertices;
    std::vector<uint32_t> indices;

    // Base (house walls)
    AddBoxToMesh(vertices, indices, glm::vec3(0, 1.5f, 0), glm::vec3(2.0f, 1.5f, 2.0f));

    // Roof (pyramid)
    AddPyramidToMesh(vertices, indices, glm::vec3(0, 3.5f, 0), glm::vec3(2.2f, 1.2f, 2.2f));

    // Door
    AddBoxToMesh(vertices, indices, glm::vec3(0, 0.7f, 2.1f), glm::vec3(0.4f, 0.7f, 0.05f));

    // Window (left)
    AddBoxToMesh(vertices, indices, glm::vec3(-0.8f, 2.0f, 2.05f), glm::vec3(0.3f, 0.3f, 0.05f));

    // Window (right)
    AddBoxToMesh(vertices, indices, glm::vec3(0.8f, 2.0f, 2.05f), glm::vec3(0.3f, 0.3f, 0.05f));

    auto mesh = std::make_unique<Nova::Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Nova::Mesh> CreateTowerMesh() {
    std::vector<Nova::Vertex> vertices;
    std::vector<uint32_t> indices;

    // Tower base
    AddBoxToMesh(vertices, indices, glm::vec3(0, 2.0f, 0), glm::vec3(1.2f, 2.0f, 1.2f));

    // Tower middle
    AddBoxToMesh(vertices, indices, glm::vec3(0, 4.5f, 0), glm::vec3(1.0f, 0.5f, 1.0f));

    // Tower top platform
    AddBoxToMesh(vertices, indices, glm::vec3(0, 5.5f, 0), glm::vec3(1.4f, 0.3f, 1.4f));

    // Spire
    AddPyramidToMesh(vertices, indices, glm::vec3(0, 6.5f, 0), glm::vec3(0.8f, 1.2f, 0.8f));

    // Battlements (4 corners)
    AddBoxToMesh(vertices, indices, glm::vec3( 1.0f, 6.2f,  1.0f), glm::vec3(0.3f, 0.6f, 0.3f));
    AddBoxToMesh(vertices, indices, glm::vec3(-1.0f, 6.2f,  1.0f), glm::vec3(0.3f, 0.6f, 0.3f));
    AddBoxToMesh(vertices, indices, glm::vec3( 1.0f, 6.2f, -1.0f), glm::vec3(0.3f, 0.6f, 0.3f));
    AddBoxToMesh(vertices, indices, glm::vec3(-1.0f, 6.2f, -1.0f), glm::vec3(0.3f, 0.6f, 0.3f));

    // Windows
    for (int i = 0; i < 3; ++i) {
        float height = 1.5f + i * 1.5f;
        AddBoxToMesh(vertices, indices, glm::vec3(0, height, 1.25f), glm::vec3(0.2f, 0.3f, 0.05f));
    }

    auto mesh = std::make_unique<Nova::Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Nova::Mesh> CreateWallMesh() {
    std::vector<Nova::Vertex> vertices;
    std::vector<uint32_t> indices;

    // Main wall
    AddBoxToMesh(vertices, indices, glm::vec3(0, 2.5f, 0), glm::vec3(6.0f, 2.5f, 0.8f));

    // Battlements (crenellations)
    for (int i = -5; i <= 5; i += 2) {
        AddBoxToMesh(vertices, indices, glm::vec3(i * 0.6f, 5.5f, 0), glm::vec3(0.5f, 0.5f, 1.0f));
    }

    // Towers at ends
    AddBoxToMesh(vertices, indices, glm::vec3(-6.5f, 3.5f, 0), glm::vec3(1.2f, 3.5f, 1.5f));
    AddBoxToMesh(vertices, indices, glm::vec3( 6.5f, 3.5f, 0), glm::vec3(1.2f, 3.5f, 1.5f));

    // Tower tops
    AddPyramidToMesh(vertices, indices, glm::vec3(-6.5f, 7.5f, 0), glm::vec3(1.4f, 0.8f, 1.7f));
    AddPyramidToMesh(vertices, indices, glm::vec3( 6.5f, 7.5f, 0), glm::vec3(1.4f, 0.8f, 1.7f));

    auto mesh = std::make_unique<Nova::Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

std::unique_ptr<Nova::Mesh> CreateTerrainMesh(int gridSize, float cellSize) {
    std::vector<Nova::Vertex> vertices;
    std::vector<uint32_t> indices;

    const float halfSize = gridSize * cellSize * 0.5f;

    // Create heightmap with multiple biomes
    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            float px = x * cellSize - halfSize;
            float pz = z * cellSize - halfSize;

            // Create varied height (hills and valleys)
            float height = 0.0f;
            height += std::sin(px * 0.08f) * 2.5f;
            height += std::cos(pz * 0.08f) * 2.0f;
            height += std::sin(px * 0.15f + pz * 0.15f) * 1.5f;
            height = std::max(0.0f, height); // No negative terrain

            glm::vec3 position(px, height, pz);

            // Calculate normal (simple approximation)
            float hL = std::sin((px - cellSize) * 0.08f) + std::cos(pz * 0.08f);
            float hR = std::sin((px + cellSize) * 0.08f) + std::cos(pz * 0.08f);
            float hD = std::sin(px * 0.08f) + std::cos((pz - cellSize) * 0.08f);
            float hU = std::sin(px * 0.08f) + std::cos((pz + cellSize) * 0.08f);

            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * cellSize, hD - hU));

            float u = static_cast<float>(x) / gridSize;
            float v = static_cast<float>(z) / gridSize;
            glm::vec2 texCoord(u, v);

            glm::vec3 tangent(1, 0, 0);
            glm::vec3 bitangent = glm::cross(normal, tangent);

            vertices.push_back({position, normal, texCoord, tangent, bitangent});
        }
    }

    // Create indices for triangles
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            uint32_t i0 = z * (gridSize + 1) + x;
            uint32_t i1 = z * (gridSize + 1) + (x + 1);
            uint32_t i2 = (z + 1) * (gridSize + 1) + (x + 1);
            uint32_t i3 = (z + 1) * (gridSize + 1) + x;

            // First triangle
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            // Second triangle
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    auto mesh = std::make_unique<Nova::Mesh>();
    mesh->Create(vertices, indices);
    return mesh;
}

} // namespace MenuScene
