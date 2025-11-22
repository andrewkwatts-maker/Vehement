#include "TileBrush.hpp"
#include "../world/TileMap.hpp"
#include <cmath>
#include <algorithm>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

TileBrush::TileBrush()
    : m_rng(std::random_device{}()) {
}

// =============================================================================
// Brush Settings
// =============================================================================

void TileBrush::SetSize(int size) noexcept {
    m_size = std::clamp(size, MIN_BRUSH_SIZE, MAX_BRUSH_SIZE);
}

// =============================================================================
// Brush Application
// =============================================================================

std::vector<TileBrushChange> TileBrush::Apply(TileMap& map, int centerX, int centerY) {
    std::vector<TileBrushChange> changes;

    if (m_mode == BrushMode::Sample) {
        Sample(map, centerX, centerY);
        return changes;
    }

    auto tiles = GetAffectedTiles(centerX, centerY);

    for (const auto& pos : tiles) {
        float strength = GetStrengthAt(centerX, centerY, pos.x, pos.y) * m_opacity;

        if (strength <= 0.0f) continue;

        TileBrushChange change;

        switch (m_mode) {
            case BrushMode::Paint:
                change = ApplyPaint(map, pos.x, pos.y, strength);
                break;
            case BrushMode::Erase:
                change = ApplyErase(map, pos.x, pos.y, strength);
                break;
            case BrushMode::Smooth:
                change = ApplySmooth(map, pos.x, pos.y, strength);
                break;
            case BrushMode::Raise:
                change = ApplyRaise(map, pos.x, pos.y, strength);
                break;
            case BrushMode::Lower:
                change = ApplyLower(map, pos.x, pos.y, strength);
                break;
            case BrushMode::Flatten:
                change = ApplyFlatten(map, pos.x, pos.y, strength);
                break;
            case BrushMode::Noise:
                change = ApplyNoise(map, pos.x, pos.y, strength);
                break;
            default:
                continue;
        }

        // Only record if something changed
        if (change.oldType != change.newType ||
            change.oldVariant != change.newVariant ||
            std::abs(change.oldElevation - change.newElevation) > 0.001f ||
            change.wasWall != change.isWall ||
            std::abs(change.oldWallHeight - change.newWallHeight) > 0.001f) {
            changes.push_back(change);
        }
    }

    return changes;
}

std::vector<TileBrushChange> TileBrush::ApplyLine(
    TileMap& map, int startX, int startY, int endX, int endY) {

    std::vector<TileBrushChange> allChanges;

    // Bresenham's line algorithm for smooth strokes
    int dx = std::abs(endX - startX);
    int dy = std::abs(endY - startY);
    int sx = (startX < endX) ? 1 : -1;
    int sy = (startY < endY) ? 1 : -1;
    int err = dx - dy;

    int x = startX;
    int y = startY;

    // Calculate step size to avoid overdrawing
    int step = std::max(1, m_size / 2);
    int stepCounter = 0;

    while (true) {
        if (stepCounter % step == 0) {
            auto changes = Apply(map, x, y);
            allChanges.insert(allChanges.end(), changes.begin(), changes.end());
        }

        if (x == endX && y == endY) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }

        ++stepCounter;
    }

    return allChanges;
}

bool TileBrush::Sample(const TileMap& map, int x, int y) {
    if (!map.IsValidPosition(x, y)) {
        return false;
    }

    const Tile& tile = map.GetTile(x, y);

    m_selectedTile = tile.type;
    m_selectedVariant = tile.textureVariant;
    m_wallMode = tile.isWall;
    m_wallHeight = tile.wallHeight;

    if (m_onSample) {
        m_onSample(m_selectedTile, m_selectedVariant);
    }

    return true;
}

std::vector<glm::ivec2> TileBrush::GetAffectedTiles(int centerX, int centerY) const {
    std::vector<glm::ivec2> tiles;

    int radius = m_size / 2;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = centerX + dx;
            int y = centerY + dy;

            if (IsInBrush(centerX, centerY, x, y)) {
                tiles.emplace_back(x, y);
            }
        }
    }

    return tiles;
}

float TileBrush::GetStrengthAt(int centerX, int centerY, int tileX, int tileY) const {
    float dx = static_cast<float>(tileX - centerX);
    float dy = static_cast<float>(tileY - centerY);
    float radius = static_cast<float>(m_size) / 2.0f;

    float distance;

    switch (m_shape) {
        case BrushShape::Circle:
            distance = std::sqrt(dx * dx + dy * dy);
            break;

        case BrushShape::Square:
            distance = std::max(std::abs(dx), std::abs(dy));
            break;

        case BrushShape::Diamond:
            distance = std::abs(dx) + std::abs(dy);
            radius *= std::sqrt(2.0f);  // Adjust for diamond shape
            break;

        default:
            distance = std::sqrt(dx * dx + dy * dy);
            break;
    }

    if (distance > radius) {
        return 0.0f;
    }

    // Apply falloff
    if (m_falloff > 0.0f) {
        float innerRadius = radius * (1.0f - m_falloff);
        if (distance > innerRadius) {
            float t = (distance - innerRadius) / (radius - innerRadius);
            return 1.0f - t;
        }
    }

    return 1.0f;
}

std::vector<std::pair<glm::ivec2, float>> TileBrush::GetPreviewTiles(
    int centerX, int centerY) const {

    std::vector<std::pair<glm::ivec2, float>> preview;

    auto tiles = GetAffectedTiles(centerX, centerY);

    for (const auto& pos : tiles) {
        float strength = GetStrengthAt(centerX, centerY, pos.x, pos.y) * m_opacity;
        if (strength > 0.0f) {
            preview.emplace_back(pos, strength);
        }
    }

    return preview;
}

// =============================================================================
// Private Helpers - Brush Operations
// =============================================================================

TileBrushChange TileBrush::ApplyPaint(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    Tile& tile = map.GetTile(x, y);

    // Store old values
    change.oldType = tile.type;
    change.oldVariant = tile.textureVariant;
    change.wasWall = tile.isWall;
    change.oldWallHeight = tile.wallHeight;

    // Apply paint based on strength (probabilistic for soft edges)
    bool shouldApply = (strength >= 1.0f) ||
                       (std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng) < strength);

    if (shouldApply) {
        tile.type = m_selectedTile;

        if (m_randomVariants) {
            tile.textureVariant = std::uniform_int_distribution<int>(0, 3)(m_rng);
        } else {
            tile.textureVariant = m_selectedVariant;
        }

        tile.isWall = m_wallMode;
        if (m_wallMode) {
            tile.wallHeight = m_wallHeight;
            tile.isWalkable = false;
            tile.blocksSight = true;
        } else {
            tile.isWalkable = !IsWaterTile(m_selectedTile) || true;  // Water is walkable but slow
            tile.blocksSight = false;
        }
    }

    // Store new values
    change.newType = tile.type;
    change.newVariant = tile.textureVariant;
    change.isWall = tile.isWall;
    change.newWallHeight = tile.wallHeight;

    return change;
}

TileBrushChange TileBrush::ApplyErase(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    Tile& tile = map.GetTile(x, y);

    change.oldType = tile.type;
    change.oldVariant = tile.textureVariant;
    change.wasWall = tile.isWall;
    change.oldWallHeight = tile.wallHeight;

    bool shouldApply = (strength >= 1.0f) ||
                       (std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng) < strength);

    if (shouldApply) {
        tile.type = TileType::None;
        tile.textureVariant = 0;
        tile.isWall = false;
        tile.wallHeight = 0.0f;
        tile.isWalkable = true;
        tile.blocksSight = false;
    }

    change.newType = tile.type;
    change.newVariant = tile.textureVariant;
    change.isWall = tile.isWall;
    change.newWallHeight = tile.wallHeight;

    return change;
}

TileBrushChange TileBrush::ApplySmooth(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    // Get neighboring elevations
    float totalElevation = 0.0f;
    int count = 0;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (map.IsValidPosition(x + dx, y + dy)) {
                const Tile& neighbor = map.GetTile(x + dx, y + dy);
                totalElevation += neighbor.isWall ? neighbor.wallHeight : 0.0f;
                ++count;
            }
        }
    }

    if (count == 0) return change;

    float avgElevation = totalElevation / count;

    Tile& tile = map.GetTile(x, y);
    change.oldElevation = tile.wallHeight;

    // Lerp towards average
    float newElevation = tile.wallHeight + (avgElevation - tile.wallHeight) * strength * 0.5f;
    tile.wallHeight = newElevation;

    change.newElevation = tile.wallHeight;
    change.oldType = change.newType = tile.type;
    change.oldVariant = change.newVariant = tile.textureVariant;

    return change;
}

TileBrushChange TileBrush::ApplyRaise(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    Tile& tile = map.GetTile(x, y);
    change.oldElevation = tile.wallHeight;
    change.wasWall = tile.isWall;
    change.oldWallHeight = tile.wallHeight;

    float delta = m_elevationDelta * strength;

    if (m_absoluteElevation) {
        tile.wallHeight = m_targetElevation;
    } else {
        tile.wallHeight += delta;
    }

    if (tile.wallHeight > 0.0f && !tile.isWall) {
        tile.isWall = true;
    }

    change.newElevation = tile.wallHeight;
    change.isWall = tile.isWall;
    change.newWallHeight = tile.wallHeight;
    change.oldType = change.newType = tile.type;
    change.oldVariant = change.newVariant = tile.textureVariant;

    return change;
}

TileBrushChange TileBrush::ApplyLower(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    Tile& tile = map.GetTile(x, y);
    change.oldElevation = tile.wallHeight;
    change.wasWall = tile.isWall;
    change.oldWallHeight = tile.wallHeight;

    float delta = m_elevationDelta * strength;
    tile.wallHeight = std::max(0.0f, tile.wallHeight - delta);

    if (tile.wallHeight <= 0.0f && tile.isWall) {
        tile.isWall = false;
        tile.isWalkable = true;
        tile.blocksSight = false;
    }

    change.newElevation = tile.wallHeight;
    change.isWall = tile.isWall;
    change.newWallHeight = tile.wallHeight;
    change.oldType = change.newType = tile.type;
    change.oldVariant = change.newVariant = tile.textureVariant;

    return change;
}

TileBrushChange TileBrush::ApplyFlatten(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    Tile& tile = map.GetTile(x, y);
    change.oldElevation = tile.wallHeight;
    change.wasWall = tile.isWall;
    change.oldWallHeight = tile.wallHeight;

    // Lerp towards target elevation
    float diff = m_targetElevation - tile.wallHeight;
    tile.wallHeight += diff * strength;

    if (tile.wallHeight > 0.1f) {
        tile.isWall = true;
    } else if (tile.wallHeight <= 0.0f) {
        tile.isWall = false;
        tile.wallHeight = 0.0f;
        tile.isWalkable = true;
        tile.blocksSight = false;
    }

    change.newElevation = tile.wallHeight;
    change.isWall = tile.isWall;
    change.newWallHeight = tile.wallHeight;
    change.oldType = change.newType = tile.type;
    change.oldVariant = change.newVariant = tile.textureVariant;

    return change;
}

TileBrushChange TileBrush::ApplyNoise(TileMap& map, int x, int y, float strength) {
    TileBrushChange change;
    change.x = x;
    change.y = y;

    if (!map.IsValidPosition(x, y)) {
        return change;
    }

    Tile& tile = map.GetTile(x, y);
    change.oldType = tile.type;
    change.oldVariant = tile.textureVariant;
    change.oldElevation = tile.wallHeight;

    float noiseValue = GenerateNoise(static_cast<float>(x), static_cast<float>(y));

    if (m_noiseConfig.applyToVariant) {
        int variantRange = 4;  // Assuming 4 variants
        int variant = static_cast<int>((noiseValue + 1.0f) * 0.5f * variantRange);
        tile.textureVariant = static_cast<uint8_t>(std::clamp(variant, 0, variantRange - 1));
    }

    if (m_noiseConfig.applyToElevation) {
        float elevationChange = noiseValue * m_noiseConfig.amplitude * strength;
        tile.wallHeight += elevationChange;
        tile.wallHeight = std::max(0.0f, tile.wallHeight);

        if (tile.wallHeight > 0.1f && !tile.isWall) {
            tile.isWall = true;
        }
    }

    change.newType = tile.type;
    change.newVariant = tile.textureVariant;
    change.newElevation = tile.wallHeight;
    change.isWall = tile.isWall;
    change.newWallHeight = tile.wallHeight;

    return change;
}

// =============================================================================
// Private Helpers - Utility
// =============================================================================

float TileBrush::GenerateNoise(float x, float y) const {
    // Simple Perlin-like noise implementation
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = m_noiseConfig.frequency;
    float maxValue = 0.0f;

    for (int i = 0; i < m_noiseConfig.octaves; ++i) {
        // Simple hash-based noise
        float fx = x * frequency;
        float fy = y * frequency;

        int xi = static_cast<int>(std::floor(fx));
        int yi = static_cast<int>(std::floor(fy));

        float xf = fx - xi;
        float yf = fy - yi;

        // Smooth interpolation
        float u = xf * xf * (3.0f - 2.0f * xf);
        float v = yf * yf * (3.0f - 2.0f * yf);

        // Hash function for pseudo-random gradient
        auto hash = [seed = m_noiseConfig.seed](int x, int y) -> float {
            uint32_t h = seed;
            h ^= static_cast<uint32_t>(x) * 374761393u;
            h ^= static_cast<uint32_t>(y) * 668265263u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF) * 2.0f - 1.0f;
        };

        float n00 = hash(xi, yi);
        float n10 = hash(xi + 1, yi);
        float n01 = hash(xi, yi + 1);
        float n11 = hash(xi + 1, yi + 1);

        float nx0 = n00 + u * (n10 - n00);
        float nx1 = n01 + u * (n11 - n01);
        float nxy = nx0 + v * (nx1 - nx0);

        total += nxy * amplitude;
        maxValue += amplitude;

        amplitude *= m_noiseConfig.persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

bool TileBrush::IsInBrush(int centerX, int centerY, int tileX, int tileY) const {
    float dx = static_cast<float>(tileX - centerX);
    float dy = static_cast<float>(tileY - centerY);
    float radius = static_cast<float>(m_size) / 2.0f;

    switch (m_shape) {
        case BrushShape::Circle: {
            float distSq = dx * dx + dy * dy;
            return distSq <= radius * radius;
        }

        case BrushShape::Square: {
            return std::abs(dx) <= radius && std::abs(dy) <= radius;
        }

        case BrushShape::Diamond: {
            return (std::abs(dx) + std::abs(dy)) <= radius * std::sqrt(2.0f);
        }

        default:
            return false;
    }
}

} // namespace Vehement
