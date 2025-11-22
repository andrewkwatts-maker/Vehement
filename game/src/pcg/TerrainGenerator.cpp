#include "TerrainGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

TerrainGenerator::TerrainGenerator() {
    // Set default params from string parameters
    m_params.heightScale = GetParamFloat("heightScale", 10.0f);
    m_params.noiseFrequency = GetParamFloat("noiseFrequency", 0.02f);
    m_params.noiseOctaves = GetParamInt("noiseOctaves", 4);
}

PCGStageResult TerrainGenerator::Generate(PCGContext& context, PCGMode mode) {
    PCGStageResult result;
    result.success = true;

    // Update params from string parameters
    m_params.heightScale = GetParamFloat("heightScale", m_params.heightScale);
    m_params.noiseFrequency = GetParamFloat("noiseFrequency", m_params.noiseFrequency);
    m_params.noiseOctaves = GetParamInt("noiseOctaves", m_params.noiseOctaves);
    m_params.applyErosion = GetParamBool("applyErosion", m_params.applyErosion);
    m_params.applySmoothing = GetParamBool("applySmoothing", m_params.applySmoothing);
    m_params.waterLevel = GetParamFloat("waterLevel", m_params.waterLevel);

    try {
        // Generate height map
        GenerateHeightMap(context);

        // Apply erosion (skip in preview mode for speed)
        if (m_params.applyErosion && mode == PCGMode::Final) {
            ApplyErosion(context);
        }

        // Apply smoothing
        if (m_params.applySmoothing) {
            int passes = (mode == PCGMode::Preview) ? 1 : m_params.smoothingPasses;
            for (int i = 0; i < passes; ++i) {
                ApplySmoothing(context);
            }
        }

        // Place water bodies
        if (m_params.generateWaterBodies) {
            GenerateWaterBodies(context);
        }

        // Assign tile types based on elevation and biome
        AssignTileTypes(context);

        result.itemsGenerated = context.GetWidth() * context.GetHeight();

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

void TerrainGenerator::GenerateHeightMap(PCGContext& context) {
    auto& elevations = context.GetElevations();
    int width = context.GetWidth();
    int height = context.GetHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float worldX = static_cast<float>(context.GetWorldX() + x);
            float worldY = static_cast<float>(context.GetWorldY() + y);

            // Get real-world elevation if available
            float realElevation = context.GetElevation(x, y);

            // Generate procedural elevation
            float noise = SampleTerrainNoise(context, worldX, worldY);

            // Blend real and procedural
            float finalElevation;
            if (m_params.useBiomeData && realElevation != 0.0f) {
                // Real data available - use it with some noise variation
                finalElevation = realElevation + noise * m_params.heightScale * 0.1f;
            } else {
                // Pure procedural
                finalElevation = noise * m_params.heightScale;
            }

            elevations[y * width + x] = finalElevation;
        }
    }
}

float TerrainGenerator::SampleTerrainNoise(PCGContext& context, float x, float y) {
    // Multi-octave fractal noise
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = m_params.noiseFrequency;
    float maxValue = 0.0f;

    for (int i = 0; i < m_params.noiseOctaves; ++i) {
        value += context.PerlinNoise(x, y, frequency, 1) * amplitude;
        maxValue += amplitude;
        amplitude *= m_params.noisePersistence;
        frequency *= 2.0f;
    }

    // Normalize to [-1, 1]
    return value / maxValue;
}

void TerrainGenerator::ApplyErosion(PCGContext& context) {
    auto& elevations = context.GetElevations();
    int width = context.GetWidth();
    int height = context.GetHeight();

    // Simple thermal erosion
    std::vector<float> temp(elevations.size());

    for (int iter = 0; iter < m_params.erosionIterations; ++iter) {
        std::copy(elevations.begin(), elevations.end(), temp.begin());

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = y * width + x;
                float center = temp[idx];

                // Check neighbors
                float totalDiff = 0.0f;
                int lowerCount = 0;

                int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
                int dy[] = {0, 0, -1, 1, -1, -1, 1, 1};

                for (int i = 0; i < 8; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    int nidx = ny * width + nx;

                    float diff = center - temp[nidx];
                    if (diff > 0.0f) {
                        totalDiff += diff;
                        lowerCount++;
                    }
                }

                // Erode to lower neighbors
                if (lowerCount > 0) {
                    float erosion = totalDiff * m_params.erosionStrength / lowerCount;
                    elevations[idx] -= erosion;

                    // Distribute to lower neighbors
                    for (int i = 0; i < 8; ++i) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        int nidx = ny * width + nx;

                        float diff = center - temp[nidx];
                        if (diff > 0.0f) {
                            elevations[nidx] += erosion * (diff / totalDiff);
                        }
                    }
                }
            }
        }
    }
}

void TerrainGenerator::ApplySmoothing(PCGContext& context) {
    auto& elevations = context.GetElevations();
    int width = context.GetWidth();
    int height = context.GetHeight();

    std::vector<float> temp(elevations.size());

    // Simple box blur
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            int count = 0;

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;

                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        sum += elevations[ny * width + nx];
                        count++;
                    }
                }
            }

            temp[y * width + x] = sum / count;
        }
    }

    elevations = std::move(temp);
}

void TerrainGenerator::GenerateWaterBodies(PCGContext& context) {
    const auto& elevations = context.GetElevations();
    int width = context.GetWidth();
    int height = context.GetHeight();

    // Find elevation range
    float minElev = *std::min_element(elevations.begin(), elevations.end());
    float maxElev = *std::max_element(elevations.begin(), elevations.end());
    float waterThreshold = minElev + (maxElev - minElev) * m_params.waterLevel;

    // Mark water tiles
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float elev = elevations[y * width + x];

            // Check real-world water data first
            if (context.IsWater(x, y)) {
                context.SetTile(x, y, m_params.waterTile);
                context.MarkOccupied(x, y);
            }
            // Check elevation threshold
            else if (elev < waterThreshold) {
                context.SetTile(x, y, m_params.waterTile);
                context.MarkOccupied(x, y);
            }
        }
    }
}

void TerrainGenerator::AssignTileTypes(PCGContext& context) {
    const auto& elevations = context.GetElevations();
    int width = context.GetWidth();
    int height = context.GetHeight();

    // Find elevation range for tile assignment
    float minElev = *std::min_element(elevations.begin(), elevations.end());
    float maxElev = *std::max_element(elevations.begin(), elevations.end());
    float elevRange = maxElev - minElev;
    if (elevRange < 0.001f) elevRange = 1.0f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Skip already occupied tiles (water, roads, etc.)
            if (context.IsOccupied(x, y)) {
                continue;
            }

            float elev = elevations[y * width + x];
            float normalizedElev = (elev - minElev) / elevRange;

            BiomeType biome = context.GetBiome(x, y);
            TileType tile = SelectTileForBiome(biome, normalizedElev, context);

            context.SetTile(x, y, tile);
        }
    }
}

TileType TerrainGenerator::SelectTileForBiome(BiomeType biome, float elevation, PCGContext& context) {
    // Select tile based on biome and elevation
    switch (biome) {
        case BiomeType::Forest:
            if (elevation < 0.3f) return m_params.grassTile;
            if (elevation < 0.7f) return m_params.forestTile;
            return m_params.rockTile;

        case BiomeType::Desert:
            if (elevation < 0.6f) return m_params.dirtTile;
            return m_params.rockTile;

        case BiomeType::Grassland:
            if (elevation < 0.4f) return m_params.grassTile;
            if (elevation < 0.8f) {
                // Mix grass and dirt
                return context.RandomBool(0.7f) ? m_params.grassTile : m_params.dirtTile;
            }
            return m_params.rockTile;

        case BiomeType::Mountain:
            if (elevation < 0.3f) return m_params.dirtTile;
            if (elevation < 0.5f) return m_params.rockTile;
            return TileType::StoneRaw;

        case BiomeType::Wetland:
            if (elevation < 0.5f) return m_params.grassTile;
            return m_params.forestTile;

        case BiomeType::Urban:
        case BiomeType::Suburban:
        case BiomeType::Commercial:
        case BiomeType::Industrial:
        case BiomeType::Residential:
            // Urban areas default to grass that will be overwritten
            return m_params.grassTile;

        case BiomeType::Park:
            return context.RandomBool(0.8f) ? m_params.grassTile : m_params.forestTile;

        default:
            // Default grass with elevation-based variation
            if (elevation < 0.3f) return m_params.grassTile;
            if (elevation < 0.6f) {
                return context.RandomBool(0.8f) ? m_params.grassTile : m_params.dirtTile;
            }
            if (elevation < 0.85f) return m_params.dirtTile;
            return m_params.rockTile;
    }
}

void TerrainGenerator::GenerateBiomes(PCGContext& context) {
    // Generate procedural biomes if real data not available
    int width = context.GetWidth();
    int height = context.GetHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float worldX = static_cast<float>(context.GetWorldX() + x);
            float worldY = static_cast<float>(context.GetWorldY() + y);

            // Use large-scale noise for biome regions
            float moisture = context.PerlinNoise(worldX * 0.01f, worldY * 0.01f, 1.0f, 2);
            float temperature = context.PerlinNoise(worldX * 0.008f + 1000, worldY * 0.008f + 1000, 1.0f, 2);

            // Map to biome
            BiomeType biome;
            if (moisture < -0.3f) {
                biome = BiomeType::Desert;
            } else if (moisture < 0.0f) {
                biome = BiomeType::Grassland;
            } else if (moisture < 0.3f) {
                biome = (temperature > 0.0f) ? BiomeType::Forest : BiomeType::Grassland;
            } else {
                biome = BiomeType::Wetland;
            }

            // Store biome in zone data
            context.MarkZone(x, y, 1, 1, GetBiomeTypeName(biome));
        }
    }
}

} // namespace Vehement
