#include "WorldTerrainEditor.hpp"
#include "../Editor.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace Vehement {

// Hash function for position to biome map
static uint64_t HashPosition(const glm::vec3& pos) {
    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));
    return (static_cast<uint64_t>(x) << 40) | (static_cast<uint64_t>(y) << 20) | static_cast<uint64_t>(z);
}

WorldTerrainEditor::WorldTerrainEditor() = default;
WorldTerrainEditor::~WorldTerrainEditor() = default;

void WorldTerrainEditor::Initialize(Editor* editor, World* world, const Config& config, const WorldConfig& worldConfig) {
    TerrainEditor::Initialize(editor, nullptr, config);
    m_world = world;
    m_worldConfig = worldConfig;

    InitializeBiomePresets();
    InitializeDefaultLayers();

    spdlog::info("WorldTerrainEditor initialized");
}

void WorldTerrainEditor::InitializeBiomePresets() {
    m_biomePresets = {
        {BiomeType::Plains, "Plains", "Grasslands with rolling hills",
         Nova::VoxelMaterial::Grass, Nova::VoxelMaterial::Dirt, Nova::VoxelMaterial::Stone,
         glm::vec3(0.3f, 0.6f, 0.2f), glm::vec3(0.5f, 0.4f, 0.3f), glm::vec3(0.5f, 0.5f, 0.5f),
         2.0f, 5.0f, glm::vec3(0.7f, 0.8f, 0.9f), 0.01f, ""},

        {BiomeType::Forest, "Forest", "Dense woodland",
         Nova::VoxelMaterial::Grass, Nova::VoxelMaterial::Dirt, Nova::VoxelMaterial::Stone,
         glm::vec3(0.2f, 0.5f, 0.15f), glm::vec3(0.4f, 0.3f, 0.2f), glm::vec3(0.5f, 0.5f, 0.5f),
         2.0f, 5.0f, glm::vec3(0.5f, 0.6f, 0.5f), 0.02f, "forest_particles"},

        {BiomeType::Desert, "Desert", "Sandy dunes and rock",
         Nova::VoxelMaterial::Sand, Nova::VoxelMaterial::Sand, Nova::VoxelMaterial::Stone,
         glm::vec3(0.9f, 0.85f, 0.6f), glm::vec3(0.85f, 0.8f, 0.55f), glm::vec3(0.6f, 0.5f, 0.4f),
         5.0f, 10.0f, glm::vec3(0.9f, 0.85f, 0.7f), 0.005f, "sand_particles"},

        {BiomeType::Tundra, "Tundra", "Frozen wasteland",
         Nova::VoxelMaterial::Snow, Nova::VoxelMaterial::Ice, Nova::VoxelMaterial::Stone,
         glm::vec3(0.95f, 0.95f, 0.98f), glm::vec3(0.7f, 0.85f, 0.95f), glm::vec3(0.5f, 0.5f, 0.5f),
         3.0f, 5.0f, glm::vec3(0.8f, 0.85f, 0.9f), 0.03f, "snow_particles"},

        {BiomeType::Mountains, "Mountains", "Rocky peaks",
         Nova::VoxelMaterial::Stone, Nova::VoxelMaterial::Stone, Nova::VoxelMaterial::Stone,
         glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.4f, 0.4f, 0.4f), glm::vec3(0.3f, 0.3f, 0.3f),
         1.0f, 3.0f, glm::vec3(0.6f, 0.7f, 0.8f), 0.015f, ""},

        {BiomeType::Ocean, "Ocean", "Deep water",
         Nova::VoxelMaterial::Water, Nova::VoxelMaterial::Sand, Nova::VoxelMaterial::Stone,
         glm::vec3(0.1f, 0.3f, 0.6f), glm::vec3(0.8f, 0.8f, 0.7f), glm::vec3(0.5f, 0.5f, 0.5f),
         10.0f, 20.0f, glm::vec3(0.3f, 0.5f, 0.7f), 0.04f, ""},

        {BiomeType::Beach, "Beach", "Sandy shore",
         Nova::VoxelMaterial::Sand, Nova::VoxelMaterial::Sand, Nova::VoxelMaterial::Stone,
         glm::vec3(0.95f, 0.9f, 0.7f), glm::vec3(0.9f, 0.85f, 0.65f), glm::vec3(0.5f, 0.5f, 0.5f),
         3.0f, 5.0f, glm::vec3(0.8f, 0.85f, 0.9f), 0.01f, ""},

        {BiomeType::Swamp, "Swamp", "Murky wetlands",
         Nova::VoxelMaterial::Mud, Nova::VoxelMaterial::Dirt, Nova::VoxelMaterial::Clay,
         glm::vec3(0.3f, 0.25f, 0.2f), glm::vec3(0.4f, 0.3f, 0.25f), glm::vec3(0.5f, 0.4f, 0.35f),
         2.0f, 4.0f, glm::vec3(0.5f, 0.55f, 0.5f), 0.05f, "fog_particles"},

        {BiomeType::Jungle, "Jungle", "Dense tropical forest",
         Nova::VoxelMaterial::Grass, Nova::VoxelMaterial::Dirt, Nova::VoxelMaterial::Stone,
         glm::vec3(0.15f, 0.5f, 0.1f), glm::vec3(0.4f, 0.3f, 0.2f), glm::vec3(0.5f, 0.5f, 0.5f),
         2.0f, 5.0f, glm::vec3(0.6f, 0.7f, 0.6f), 0.03f, "jungle_particles"},

        {BiomeType::Volcanic, "Volcanic", "Lava and ash",
         Nova::VoxelMaterial::Stone, Nova::VoxelMaterial::Stone, Nova::VoxelMaterial::Lava,
         glm::vec3(0.2f, 0.2f, 0.2f), glm::vec3(0.3f, 0.3f, 0.3f), glm::vec3(0.8f, 0.2f, 0.0f),
         2.0f, 5.0f, glm::vec3(0.3f, 0.2f, 0.2f), 0.02f, "lava_particles"},
    };
}

void WorldTerrainEditor::InitializeDefaultLayers() {
    m_terrainLayers = {
        {"Grass", "grass", Nova::VoxelMaterial::Grass, glm::vec3(0.3f, 0.6f, 0.2f), 1.0f, 0.5f,
         0.0f, 50.0f, 0.0f, 0.5f, true},

        {"Dirt", "dirt", Nova::VoxelMaterial::Dirt, glm::vec3(0.5f, 0.4f, 0.3f), 1.0f, 0.5f,
         -10.0f, 10.0f, 0.0f, 1.0f, false},

        {"Stone", "stone", Nova::VoxelMaterial::Stone, glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, 0.5f,
         20.0f, 200.0f, 0.5f, 1.0f, true},

        {"Sand", "sand", Nova::VoxelMaterial::Sand, glm::vec3(0.9f, 0.85f, 0.6f), 1.0f, 0.5f,
         -5.0f, 5.0f, 0.0f, 0.3f, false},

        {"Snow", "snow", Nova::VoxelMaterial::Snow, glm::vec3(0.95f, 0.95f, 0.98f), 1.0f, 0.5f,
         50.0f, 200.0f, 0.0f, 0.7f, false},
    };
}

// =========================================================================
// Editing Mode
// =========================================================================

void WorldTerrainEditor::SetEditingGlobalWorld(bool global) {
    m_editingGlobalWorld = global;
    if (global && m_world) {
        // Set terrain to world's terrain
        SetTerrain(m_world->GetVoxelTerrain());
    }
}

bool WorldTerrainEditor::CanEdit(const glm::vec3& position) const {
    if (m_worldConfig.enableRegionProtection && IsInProtectedRegion(position)) {
        return false;
    }
    return true;
}

// =========================================================================
// Biome System
// =========================================================================

void WorldTerrainEditor::AddBiomePreset(const BiomePreset& preset) {
    m_biomePresets.push_back(preset);
}

void WorldTerrainEditor::SelectBiome(size_t index) {
    if (index < m_biomePresets.size()) {
        m_selectedBiomeIndex = index;
    }
}

const BiomePreset* WorldTerrainEditor::GetSelectedBiome() const {
    if (m_selectedBiomeIndex < m_biomePresets.size()) {
        return &m_biomePresets[m_selectedBiomeIndex];
    }
    return nullptr;
}

void WorldTerrainEditor::PaintBiome(const glm::vec3& position) {
    if (!CanEdit(position)) return;

    ApplyBiomePaint(position);
}

void WorldTerrainEditor::ApplyBiomePaint(const glm::vec3& position) {
    const BiomePreset* biome = GetSelectedBiome();
    if (!biome || !m_terrain) return;

    // Apply brush in a radius
    float radius = m_brush.radius;
    for (float z = -radius; z <= radius; z += 0.5f) {
        for (float x = -radius; x <= radius; x += 0.5f) {
            glm::vec3 offset(x, 0.0f, z);
            if (glm::length(offset) > radius) continue;

            glm::vec3 pos = position + offset;
            uint64_t hash = HashPosition(pos);
            m_biomeMap[hash] = biome->type;

            // Apply biome materials based on height
            float height = m_terrain->GetHeightAt(pos.x, pos.z);
            float depth = height - pos.y;

            Nova::VoxelMaterial material;
            glm::vec3 color;

            if (depth < biome->surfaceDepth) {
                material = biome->surfaceMaterial;
                color = biome->surfaceColor;
            } else if (depth < biome->subsurfaceDepth) {
                material = biome->subsurfaceMaterial;
                color = biome->subsurfaceColor;
            } else {
                material = biome->deepMaterial;
                color = biome->deepColor;
            }

            // Paint material
            m_terrain->PaintMaterial(pos, 1.0f, material, color);
        }
    }
}

BiomeType WorldTerrainEditor::GetBiomeAt(const glm::vec3& position) const {
    uint64_t hash = HashPosition(position);
    auto it = m_biomeMap.find(hash);
    return (it != m_biomeMap.end()) ? it->second : BiomeType::Plains;
}

// =========================================================================
// Texture Layer System
// =========================================================================

void WorldTerrainEditor::AddTerrainLayer(const TerrainLayer& layer) {
    m_terrainLayers.push_back(layer);
}

void WorldTerrainEditor::RemoveTerrainLayer(size_t index) {
    if (index < m_terrainLayers.size()) {
        m_terrainLayers.erase(m_terrainLayers.begin() + index);
        if (m_selectedLayerIndex >= m_terrainLayers.size()) {
            m_selectedLayerIndex = m_terrainLayers.empty() ? 0 : m_terrainLayers.size() - 1;
        }
    }
}

void WorldTerrainEditor::SelectTerrainLayer(size_t index) {
    if (index < m_terrainLayers.size()) {
        m_selectedLayerIndex = index;
    }
}

void WorldTerrainEditor::PaintTerrainLayer(const glm::vec3& position) {
    if (!CanEdit(position)) return;

    ApplyLayerPaint(position);
}

void WorldTerrainEditor::ApplyLayerPaint(const glm::vec3& position) {
    if (m_selectedLayerIndex >= m_terrainLayers.size() || !m_terrain) return;

    const TerrainLayer& layer = m_terrainLayers[m_selectedLayerIndex];
    m_terrain->PaintMaterial(position, m_brush.radius, layer.material, layer.color);
}

void WorldTerrainEditor::AutoApplyLayers(const glm::vec3& minBounds, const glm::vec3& maxBounds) {
    if (!m_terrain) return;

    for (float y = minBounds.y; y <= maxBounds.y; y += 1.0f) {
        for (float z = minBounds.z; z <= maxBounds.z; z += 1.0f) {
            for (float x = minBounds.x; x <= maxBounds.x; x += 1.0f) {
                glm::vec3 pos(x, y, z);
                float height = m_terrain->GetHeightAt(x, z);
                float slope = CalculateSlope(pos);

                // Apply each layer that matches height/slope criteria
                for (const auto& layer : m_terrainLayers) {
                    if (!layer.autoApply) continue;

                    if (height >= layer.heightMin && height <= layer.heightMax &&
                        slope >= layer.slopeMin && slope <= layer.slopeMax) {
                        ApplyLayerToVoxel(pos, layer, 1.0f);
                    }
                }
            }
        }
    }
}

float WorldTerrainEditor::CalculateSlope(const glm::vec3& position) const {
    if (!m_terrain) return 0.0f;

    glm::vec3 normal = m_terrain->GetNormalAt(position);
    float slope = 1.0f - normal.y;  // 0 = flat, 1 = vertical
    return glm::clamp(slope, 0.0f, 1.0f);
}

void WorldTerrainEditor::ApplyLayerToVoxel(const glm::vec3& position, const TerrainLayer& layer, float weight) {
    if (!m_terrain) return;
    m_terrain->PaintMaterial(position, 1.0f, layer.material, layer.color);
}

// =========================================================================
// Region System
// =========================================================================

void WorldTerrainEditor::AddRegion(const TerrainRegion& region) {
    m_regions.push_back(region);
}

void WorldTerrainEditor::RemoveRegion(size_t index) {
    if (index < m_regions.size()) {
        m_regions.erase(m_regions.begin() + index);
    }
}

bool WorldTerrainEditor::IsInProtectedRegion(const glm::vec3& position) const {
    for (const auto& region : m_regions) {
        if (!region.isProtected) continue;

        if (position.x >= region.minXZ.x && position.x <= region.maxXZ.x &&
            position.z >= region.minXZ.y && position.z <= region.maxXZ.y &&
            position.y >= region.minHeight && position.y <= region.maxHeight) {
            return true;
        }
    }
    return false;
}

const TerrainRegion* WorldTerrainEditor::GetRegionAt(const glm::vec3& position) const {
    for (const auto& region : m_regions) {
        if (position.x >= region.minXZ.x && position.x <= region.maxXZ.x &&
            position.z >= region.minXZ.y && position.z <= region.maxXZ.y &&
            position.y >= region.minHeight && position.y <= region.maxHeight) {
            return &region;
        }
    }
    return nullptr;
}

// =========================================================================
// Large-Scale Operations
// =========================================================================

void WorldTerrainEditor::FillRegionFlat(const glm::vec3& minBounds, const glm::vec3& maxBounds, float height) {
    if (!m_terrain) return;

    for (float z = minBounds.z; z <= maxBounds.z; z += 1.0f) {
        for (float x = minBounds.x; x <= maxBounds.x; x += 1.0f) {
            glm::vec3 pos(x, height, z);
            if (!CanEdit(pos)) continue;
            m_terrain->FlattenTerrain(pos, 1.0f, height, 1.0f);
        }
    }
}

void WorldTerrainEditor::GenerateNoiseInRegion(const glm::vec3& minBounds, const glm::vec3& maxBounds, int seed, float scale, int octaves) {
    if (!m_terrain) return;

    // This would use the terrain generator
    m_terrain->GenerateTerrain(seed, scale, octaves, 0.5f, 2.0f);
}

void WorldTerrainEditor::CopyRegion(const glm::vec3& srcMin, const glm::vec3& srcMax, const glm::vec3& dstMin) {
    if (!m_terrain) return;

    glm::vec3 size = srcMax - srcMin;

    for (float z = 0; z <= size.z; z += 1.0f) {
        for (float y = 0; y <= size.y; y += 1.0f) {
            for (float x = 0; x <= size.x; x += 1.0f) {
                glm::vec3 srcPos = srcMin + glm::vec3(x, y, z);
                glm::vec3 dstPos = dstMin + glm::vec3(x, y, z);

                if (!CanEdit(dstPos)) continue;

                Nova::Voxel voxel = m_terrain->GetVoxel(srcPos);
                m_terrain->SetVoxel(dstPos, voxel);
            }
        }
    }
}

void WorldTerrainEditor::MirrorTerrain(const glm::vec3& center, bool mirrorX, bool mirrorZ) {
    if (!m_terrain) return;

    // Determine region bounds based on current brush radius
    float radius = m_brush.radius * 10.0f;  // Use larger area for mirroring

    for (float z = -radius; z <= radius; z += 1.0f) {
        for (float y = -radius; y <= radius; y += 1.0f) {
            for (float x = -radius; x <= radius; x += 1.0f) {
                glm::vec3 srcOffset(x, y, z);
                glm::vec3 srcPos = center + srcOffset;

                // Calculate mirrored position
                glm::vec3 mirrorOffset = srcOffset;
                if (mirrorX) mirrorOffset.x = -mirrorOffset.x;
                if (mirrorZ) mirrorOffset.z = -mirrorOffset.z;

                glm::vec3 dstPos = center + mirrorOffset;

                // Skip if source and destination are the same
                if (glm::length(srcPos - dstPos) < 0.1f) continue;

                // Skip if destination is not editable
                if (!CanEdit(dstPos)) continue;

                // Copy voxel from source to destination
                Nova::Voxel voxel = m_terrain->GetVoxel(srcPos);
                m_terrain->SetVoxel(dstPos, voxel);
            }
        }
    }

    if (OnTerrainModified) OnTerrainModified();
}

void WorldTerrainEditor::RotateTerrain(const glm::vec3& center, float radius, int quarterTurns) {
    if (!m_terrain) return;

    // Normalize quarter turns to 0-3 range
    quarterTurns = ((quarterTurns % 4) + 4) % 4;
    if (quarterTurns == 0) return;  // No rotation needed

    // Create temporary storage for rotated voxels
    std::unordered_map<uint64_t, Nova::Voxel> tempVoxels;

    // Read all voxels in the region
    for (float z = -radius; z <= radius; z += 1.0f) {
        for (float y = -radius; y <= radius; y += 1.0f) {
            for (float x = -radius; x <= radius; x += 1.0f) {
                glm::vec3 offset(x, y, z);
                if (glm::length(glm::vec2(x, z)) > radius) continue;

                glm::vec3 srcPos = center + offset;
                Nova::Voxel voxel = m_terrain->GetVoxel(srcPos);

                // Calculate rotated position (around Y axis)
                glm::vec3 rotatedOffset = offset;
                for (int i = 0; i < quarterTurns; ++i) {
                    float tempX = rotatedOffset.x;
                    rotatedOffset.x = -rotatedOffset.z;
                    rotatedOffset.z = tempX;
                }

                glm::vec3 dstPos = center + rotatedOffset;
                uint64_t hash = HashPosition(dstPos);
                tempVoxels[hash] = voxel;
            }
        }
    }

    // Apply rotated voxels
    for (const auto& [hash, voxel] : tempVoxels) {
        // Reconstruct position from hash
        int x = static_cast<int>((hash >> 40) & 0xFFFFF);
        int y = static_cast<int>((hash >> 20) & 0xFFFFF);
        int z = static_cast<int>(hash & 0xFFFFF);

        // Handle sign extension for negative values
        if (x >= 0x80000) x -= 0x100000;
        if (y >= 0x80000) y -= 0x100000;
        if (z >= 0x80000) z -= 0x100000;

        glm::vec3 pos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        if (CanEdit(pos)) {
            m_terrain->SetVoxel(pos, voxel);
        }
    }

    if (OnTerrainModified) OnTerrainModified();
}

void WorldTerrainEditor::ScaleHeight(const glm::vec3& minBounds, const glm::vec3& maxBounds, float scale) {
    if (!m_terrain) return;

    for (float z = minBounds.z; z <= maxBounds.z; z += 1.0f) {
        for (float x = minBounds.x; x <= maxBounds.x; x += 1.0f) {
            float height = m_terrain->GetHeightAt(x, z);
            float newHeight = height * scale;

            glm::vec3 pos(x, 0.0f, z);
            if (!CanEdit(pos)) continue;

            m_terrain->FlattenTerrain(pos, 1.0f, newHeight, 1.0f);
        }
    }
}

// =========================================================================
// Advanced Tools
// =========================================================================

void WorldTerrainEditor::SimulateErosion(const glm::vec3& minBounds, const glm::vec3& maxBounds, int iterations, float strength) {
    if (!m_terrain) return;

    // Simple hydraulic erosion simulation
    // Water flows downhill, eroding terrain and depositing sediment
    float erosionRate = 0.1f * strength;
    float depositionRate = 0.05f * strength;
    float evaporationRate = 0.02f;

    // Temporary heightmap storage
    std::unordered_map<uint64_t, float> waterMap;
    std::unordered_map<uint64_t, float> sedimentMap;

    for (int iter = 0; iter < iterations; ++iter) {
        // For each position in the region
        for (float z = minBounds.z; z <= maxBounds.z; z += 1.0f) {
            for (float x = minBounds.x; x <= maxBounds.x; x += 1.0f) {
                glm::vec3 pos(x, 0.0f, z);
                if (!CanEdit(pos)) continue;

                uint64_t hash = HashPosition(pos);
                float currentHeight = m_terrain->GetHeightAt(x, z);

                // Add rainfall
                waterMap[hash] += 0.1f;

                // Find lowest neighbor
                float lowestHeight = currentHeight;
                glm::vec2 lowestDir(0.0f);

                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dz == 0) continue;

                        float neighborHeight = m_terrain->GetHeightAt(x + dx, z + dz);
                        if (neighborHeight < lowestHeight) {
                            lowestHeight = neighborHeight;
                            lowestDir = glm::vec2(static_cast<float>(dx), static_cast<float>(dz));
                        }
                    }
                }

                // Water flows downhill
                if (lowestHeight < currentHeight) {
                    float heightDiff = currentHeight - lowestHeight;
                    float water = waterMap[hash];
                    float sediment = sedimentMap[hash];

                    // Erode terrain
                    float erosion = std::min(heightDiff * erosionRate * water, 0.5f);

                    // Apply erosion by lowering the terrain
                    glm::vec3 erodePos(x, currentHeight, z);
                    m_terrain->SmoothTerrain(erodePos, 1.0f, erosion);

                    // Transfer water and sediment to neighbor
                    glm::vec3 neighborPos(x + lowestDir.x, 0.0f, z + lowestDir.y);
                    uint64_t neighborHash = HashPosition(neighborPos);

                    float transferAmount = std::min(water * 0.5f, heightDiff);
                    waterMap[hash] -= transferAmount;
                    waterMap[neighborHash] += transferAmount * 0.9f;

                    sedimentMap[hash] -= sediment * 0.5f;
                    sedimentMap[neighborHash] += erosion + sediment * 0.5f;
                } else {
                    // Deposit sediment in flat areas
                    float sediment = sedimentMap[hash];
                    if (sediment > 0.0f) {
                        float deposit = sediment * depositionRate;
                        glm::vec3 depositPos(x, currentHeight, z);

                        Nova::SDFBrush brush;
                        brush.shape = Nova::SDFBrushShape::Sphere;
                        brush.operation = Nova::SDFOperation::SmoothUnion;
                        brush.position = depositPos;
                        brush.size = glm::vec3(deposit);
                        brush.smoothness = 0.3f;
                        brush.material = Nova::VoxelMaterial::Dirt;
                        m_terrain->ApplyBrush(brush);

                        sedimentMap[hash] -= deposit;
                    }
                }

                // Evaporation
                waterMap[hash] *= (1.0f - evaporationRate);
            }
        }
    }

    if (OnTerrainModified) OnTerrainModified();
}

void WorldTerrainEditor::SimulateThermalErosion(const glm::vec3& minBounds, const glm::vec3& maxBounds, float talusAngle, int iterations) {
    if (!m_terrain) return;

    // Thermal erosion: material falls when slope exceeds talus angle
    // talusAngle is the maximum stable slope (in radians)
    float maxHeightDiff = std::tan(talusAngle);  // Convert angle to height difference per unit distance

    for (int iter = 0; iter < iterations; ++iter) {
        for (float z = minBounds.z; z <= maxBounds.z; z += 1.0f) {
            for (float x = minBounds.x; x <= maxBounds.x; x += 1.0f) {
                glm::vec3 pos(x, 0.0f, z);
                if (!CanEdit(pos)) continue;

                float currentHeight = m_terrain->GetHeightAt(x, z);

                // Check all 8 neighbors
                float totalTransfer = 0.0f;
                std::vector<std::pair<glm::vec2, float>> transfers;

                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dz == 0) continue;

                        float neighborHeight = m_terrain->GetHeightAt(x + dx, z + dz);
                        float heightDiff = currentHeight - neighborHeight;
                        float distance = (std::abs(dx) + std::abs(dz) == 2) ? 1.414f : 1.0f;
                        float slope = heightDiff / distance;

                        // If slope exceeds talus angle, material should fall
                        if (slope > maxHeightDiff) {
                            float excess = (slope - maxHeightDiff) * distance;
                            float transfer = excess * 0.5f;  // Transfer half of excess
                            transfers.emplace_back(glm::vec2(static_cast<float>(dx), static_cast<float>(dz)), transfer);
                            totalTransfer += transfer;
                        }
                    }
                }

                // Apply transfers
                if (totalTransfer > 0.0f) {
                    // Lower current position
                    glm::vec3 lowerPos(x, currentHeight - totalTransfer * 0.5f, z);
                    m_terrain->FlattenTerrain(glm::vec3(x, currentHeight, z), 0.5f, currentHeight - totalTransfer * 0.5f, 0.5f);

                    // Raise neighbor positions
                    for (const auto& [dir, amount] : transfers) {
                        float nx = x + dir.x;
                        float nz = z + dir.y;
                        float neighborHeight = m_terrain->GetHeightAt(nx, nz);

                        glm::vec3 raisePos(nx, neighborHeight, nz);
                        m_terrain->FlattenTerrain(raisePos, 0.5f, neighborHeight + amount * 0.5f, 0.5f);
                    }
                }
            }
        }
    }

    if (OnTerrainModified) OnTerrainModified();
}

void WorldTerrainEditor::CreateRiver(const std::vector<glm::vec3>& path, float width, float depth) {
    if (!m_terrain || path.size() < 2) return;

    for (size_t i = 0; i < path.size() - 1; ++i) {
        glm::vec3 start = path[i];
        glm::vec3 end = path[i + 1];

        float dist = glm::length(end - start);
        int steps = static_cast<int>(dist / (width * 0.25f));

        for (int j = 0; j <= steps; ++j) {
            float t = static_cast<float>(j) / static_cast<float>(steps);
            glm::vec3 pos = glm::mix(start, end, t);

            if (!CanEdit(pos)) continue;

            // Lower terrain
            ApplyLowerTool(pos);
        }
    }
}

void WorldTerrainEditor::CreateRoad(const std::vector<glm::vec3>& path, float width, float smoothness) {
    if (path.size() < 2) return;

    ApplyPathTool(path[0], path[path.size() - 1]);
}

void WorldTerrainEditor::CreatePlateau(const glm::vec3& center, float radius, float height, float cliffSteepness) {
    if (!m_terrain) return;

    for (float z = -radius; z <= radius; z += 1.0f) {
        for (float x = -radius; x <= radius; x += 1.0f) {
            glm::vec3 offset(x, 0.0f, z);
            float dist = glm::length(offset);
            if (dist > radius) continue;

            glm::vec3 pos = center + offset;
            if (!CanEdit(pos)) continue;

            // Flatten top
            if (dist < radius * 0.7f) {
                m_terrain->FlattenTerrain(pos, 1.0f, height, 1.0f);
            } else {
                // Create cliff edge
                float edgeDist = (dist - radius * 0.7f) / (radius * 0.3f);
                float cliffHeight = height * (1.0f - edgeDist * cliffSteepness);
                m_terrain->FlattenTerrain(pos, 1.0f, cliffHeight, 0.5f);
            }
        }
    }
}

void WorldTerrainEditor::CreateValley(const glm::vec3& start, const glm::vec3& end, float width, float depth) {
    if (!m_terrain) return;

    glm::vec3 dir = glm::normalize(end - start);
    float length = glm::length(end - start);

    for (float t = 0.0f; t <= 1.0f; t += 0.05f) {
        glm::vec3 center = glm::mix(start, end, t);

        for (float w = -width; w <= width; w += 1.0f) {
            glm::vec3 perp = glm::cross(dir, glm::vec3(0, 1, 0));
            glm::vec3 pos = center + perp * w;

            if (!CanEdit(pos)) continue;

            float depthFactor = 1.0f - std::abs(w) / width;
            float targetDepth = depth * depthFactor;

            float currentHeight = m_terrain->GetHeightAt(pos.x, pos.z);
            m_terrain->FlattenTerrain(pos, 1.0f, currentHeight - targetDepth, 0.5f);
        }
    }
}

// =========================================================================
// Brush Stroke Recording
// =========================================================================

void WorldTerrainEditor::BeginBrushStroke() {
    m_isRecordingStroke = true;
    m_currentStroke = BrushStroke();
    m_currentStroke.tool = m_brush.tool;
    m_currentStroke.brush = m_brush;
}

void WorldTerrainEditor::AddStrokePosition(const glm::vec3& position) {
    if (m_isRecordingStroke) {
        m_currentStroke.positions.push_back(position);
    }
}

void WorldTerrainEditor::EndBrushStroke() {
    if (m_isRecordingStroke && !m_currentStroke.positions.empty()) {
        // Apply the entire stroke
        for (const auto& pos : m_currentStroke.positions) {
            ApplyBrush(pos);
        }
    }
    m_isRecordingStroke = false;
}

// =========================================================================
// Auto-Save
// =========================================================================

void WorldTerrainEditor::SetAutoSaveEnabled(bool enabled) {
    m_worldConfig.enableAutoSave = enabled;
    m_autoSaveTimer = 0.0f;
}

void WorldTerrainEditor::SaveWorldTerrain() {
    if (!m_world || !m_terrain) return;

    std::string filename = "world_terrain.vterrain";
    if (m_terrain->SaveTerrain(filename)) {
        spdlog::info("World terrain saved to {}", filename);
    } else {
        spdlog::error("Failed to save world terrain");
    }
}

bool WorldTerrainEditor::LoadWorldTerrain() {
    if (!m_world || !m_terrain) return false;

    std::string filename = "world_terrain.vterrain";
    if (m_terrain->LoadTerrain(filename)) {
        spdlog::info("World terrain loaded from {}", filename);
        return true;
    } else {
        spdlog::warn("Failed to load world terrain from {}", filename);
        return false;
    }
}

// =========================================================================
// Networked Editing
// =========================================================================

void WorldTerrainEditor::BroadcastEdit(const glm::vec3& position) {
    if (!m_worldConfig.enableNetworkedEditing) return;

    // Prepare edit data for network transmission
    // In a real implementation, this would serialize the edit and send it via the network system
    // For now, we log the edit and prepare the data structure

    struct NetworkTerrainEdit {
        glm::vec3 position;
        TerrainBrush brush;
        uint32_t playerId;
        float timestamp;
    };

    NetworkTerrainEdit edit;
    edit.position = position;
    edit.brush = m_brush;
    edit.playerId = 0;  // Local player ID would come from network system
    edit.timestamp = 0.0f;  // Current time from network system

    // In a real implementation:
    // 1. Serialize the edit to a network packet
    // 2. Send to server via network system
    // 3. Server broadcasts to all other clients
    // Example: m_networkSystem->SendTerrainEdit(edit);

    spdlog::debug("Broadcasting terrain edit at ({}, {}, {})", position.x, position.y, position.z);
}

void WorldTerrainEditor::ReceiveEdit(uint32_t playerId, const glm::vec3& position, const TerrainBrush& brush) {
    m_activeEditors[playerId] = position;

    // Apply edit from other player
    TerrainBrush oldBrush = m_brush;
    m_brush = brush;
    ApplyBrush(position);
    m_brush = oldBrush;
}

// =========================================================================
// Update
// =========================================================================

void WorldTerrainEditor::UpdateWorld(float deltaTime) {
    Update(deltaTime);

    // Auto-save timer
    if (m_worldConfig.enableAutoSave) {
        m_autoSaveTimer += deltaTime;
        if (m_autoSaveTimer >= m_worldConfig.autoSaveInterval) {
            SaveWorldTerrain();
            m_autoSaveTimer = 0.0f;
        }
    }

    // Preview broadcast timer
    if (m_worldConfig.enablePreviewForOthers && m_worldConfig.enableNetworkedEditing) {
        m_previewBroadcastTimer += deltaTime;
        if (m_previewBroadcastTimer >= (1.0f / m_worldConfig.previewUpdateRate)) {
            if (m_hasValidPreview) {
                BroadcastEdit(m_previewPosition);
            }
            m_previewBroadcastTimer = 0.0f;
        }
    }
}

// =========================================================================
// UI
// =========================================================================

void WorldTerrainEditor::RenderWorldUI() {
    RenderUI();  // Base UI

    ImGui::Begin("World Terrain Editor");

    // Mode toggle
    if (ImGui::Checkbox("Edit Global World", &m_editingGlobalWorld)) {
        SetEditingGlobalWorld(m_editingGlobalWorld);
    }

    ImGui::Separator();

    // Panel toggles
    ImGui::Checkbox("Show Biome Panel", &m_showBiomePanel);
    ImGui::SameLine();
    ImGui::Checkbox("Show Layer Panel", &m_showLayerPanel);

    ImGui::Checkbox("Show Region Panel", &m_showRegionPanel);
    ImGui::SameLine();
    ImGui::Checkbox("Show Advanced Tools", &m_showAdvancedTools);

    ImGui::Separator();

    // Auto-save status
    if (m_worldConfig.enableAutoSave) {
        float timeUntilSave = m_worldConfig.autoSaveInterval - m_autoSaveTimer;
        ImGui::Text("Auto-save in: %.1f seconds", timeUntilSave);
    }

    if (ImGui::Button("Save Now")) {
        SaveWorldTerrain();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        LoadWorldTerrain();
    }

    ImGui::End();

    // Render panels
    if (m_showBiomePanel) RenderBiomePanel();
    if (m_showLayerPanel) RenderLayerPanel();
    if (m_showRegionPanel) RenderRegionPanel();
    if (m_showAdvancedTools) RenderAdvancedToolsPanel();
}

void WorldTerrainEditor::RenderBiomePanel() {
    ImGui::Begin("Biome Painting", &m_showBiomePanel);

    ImGui::Text("Select Biome:");

    for (size_t i = 0; i < m_biomePresets.size(); ++i) {
        bool selected = (i == m_selectedBiomeIndex);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::Button(m_biomePresets[i].name.c_str(), ImVec2(120, 30))) {
            SelectBiome(i);
        }

        if (selected) {
            ImGui::PopStyleColor();
        }

        if ((i + 1) % 3 != 0 && i < m_biomePresets.size() - 1) {
            ImGui::SameLine();
        }
    }

    ImGui::Separator();

    const BiomePreset* biome = GetSelectedBiome();
    if (biome) {
        ImGui::Text("Description: %s", biome->description.c_str());
        ImGui::ColorEdit3("Surface Color", const_cast<float*>(&biome->surfaceColor.x), ImGuiColorEditFlags_NoInputs);
    }

    ImGui::End();
}

void WorldTerrainEditor::RenderLayerPanel() {
    ImGui::Begin("Terrain Layers", &m_showLayerPanel);

    ImGui::Text("Texture Layers:");

    for (size_t i = 0; i < m_terrainLayers.size(); ++i) {
        bool selected = (i == m_selectedLayerIndex);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        ImVec4 col(m_terrainLayers[i].color.r, m_terrainLayers[i].color.g, m_terrainLayers[i].color.b, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);

        if (ImGui::Button(m_terrainLayers[i].name.c_str(), ImVec2(100, 30))) {
            SelectTerrainLayer(i);
        }

        ImGui::PopStyleColor();

        if (selected) {
            ImGui::PopStyleColor();
        }

        if ((i + 1) % 3 != 0 && i < m_terrainLayers.size() - 1) {
            ImGui::SameLine();
        }
    }

    ImGui::Separator();

    if (ImGui::Button("Auto-Apply Layers")) {
        // Apply to visible region
        glm::vec3 min(-100, -50, -100);
        glm::vec3 max(100, 50, 100);
        AutoApplyLayers(min, max);
    }

    ImGui::End();
}

void WorldTerrainEditor::RenderRegionPanel() {
    ImGui::Begin("Terrain Regions", &m_showRegionPanel);

    ImGui::Text("Regions: %zu", m_regions.size());

    for (size_t i = 0; i < m_regions.size(); ++i) {
        const auto& region = m_regions[i];
        ImGui::Text("%s [%s]", region.name.c_str(), region.isProtected ? "Protected" : "Editable");
    }

    ImGui::Separator();

    if (ImGui::Button("Add Region")) {
        TerrainRegion region;
        region.name = "Region " + std::to_string(m_regions.size() + 1);
        region.minXZ = glm::vec2(-10, -10);
        region.maxXZ = glm::vec2(10, 10);
        AddRegion(region);
    }

    ImGui::End();
}

void WorldTerrainEditor::RenderAdvancedToolsPanel() {
    ImGui::Begin("Advanced Tools", &m_showAdvancedTools);

    if (ImGui::CollapsingHeader("Large-Scale Operations")) {
        static float fillHeight = 0.0f;
        ImGui::SliderFloat("Fill Height", &fillHeight, -50.0f, 50.0f);
        if (ImGui::Button("Fill Region Flat")) {
            FillRegionFlat(glm::vec3(-50, 0, -50), glm::vec3(50, 10, 50), fillHeight);
        }

        ImGui::Separator();

        static float heightScale = 1.0f;
        ImGui::SliderFloat("Height Scale", &heightScale, 0.1f, 5.0f);
        if (ImGui::Button("Scale Height")) {
            ScaleHeight(glm::vec3(-50, 0, -50), glm::vec3(50, 50, 50), heightScale);
        }
    }

    if (ImGui::CollapsingHeader("Erosion")) {
        static int erosionIterations = 10;
        static float erosionStrength = 0.5f;

        ImGui::SliderInt("Iterations", &erosionIterations, 1, 100);
        ImGui::SliderFloat("Strength", &erosionStrength, 0.0f, 1.0f);

        if (ImGui::Button("Simulate Erosion")) {
            SimulateErosion(glm::vec3(-50, 0, -50), glm::vec3(50, 50, 50), erosionIterations, erosionStrength);
        }
    }

    if (ImGui::CollapsingHeader("Features")) {
        static float plateauHeight = 10.0f;
        static float plateauRadius = 20.0f;
        static float cliffSteepness = 0.8f;

        ImGui::SliderFloat("Plateau Height", &plateauHeight, 0.0f, 50.0f);
        ImGui::SliderFloat("Plateau Radius", &plateauRadius, 5.0f, 50.0f);
        ImGui::SliderFloat("Cliff Steepness", &cliffSteepness, 0.0f, 1.0f);

        if (ImGui::Button("Create Plateau")) {
            CreatePlateau(glm::vec3(0, 0, 0), plateauRadius, plateauHeight, cliffSteepness);
        }
    }

    ImGui::End();
}

} // namespace Vehement
