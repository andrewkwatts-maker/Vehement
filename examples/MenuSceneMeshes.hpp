/**
 * @file MenuSceneMeshes.hpp
 * @brief Helper functions for creating hero and building meshes for the main menu scene
 */

#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {
    class Mesh;
}

namespace MenuScene {

/**
 * @brief Create a hero character mesh in heroic pose
 *
 * Creates a stylized humanoid figure with:
 * - Torso and head
 * - Arms (right arm raised holding weapon)
 * - Legs in standing pose
 * - Simple sword/weapon
 *
 * @return Unique pointer to the hero mesh
 */
std::unique_ptr<Nova::Mesh> CreateHeroMesh();

/**
 * @brief Create a medieval house with peaked roof
 *
 * @return Unique pointer to the building mesh
 */
std::unique_ptr<Nova::Mesh> CreateHouseMesh();

/**
 * @brief Create a tall tower with battlement top
 *
 * @return Unique pointer to the tower mesh
 */
std::unique_ptr<Nova::Mesh> CreateTowerMesh();

/**
 * @brief Create a fortress wall section with crenellations
 *
 * @return Unique pointer to the wall mesh
 */
std::unique_ptr<Nova::Mesh> CreateWallMesh();

/**
 * @brief Create multi-biome terrain with gentle hills
 *
 * Creates a subdivided terrain plane with:
 * - Height variation (hills)
 * - Multiple texture regions for biomes
 * - Proper normals for lighting
 *
 * @param gridSize Number of subdivisions (default 20)
 * @param cellSize Size of each cell (default 2.0)
 * @return Unique pointer to the terrain mesh
 */
std::unique_ptr<Nova::Mesh> CreateTerrainMesh(int gridSize = 20, float cellSize = 2.0f);

} // namespace MenuScene
