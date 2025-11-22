#include "PCGPreview.hpp"
#include <chrono>
#include <fstream>
#include <algorithm>

namespace Vehement {

// ============================================================================
// Color Constants
// ============================================================================

namespace {

// Tile colors (RGBA format)
constexpr uint32_t COLOR_GRASS = 0xFF3CB371;       // Medium sea green
constexpr uint32_t COLOR_GRASS2 = 0xFF2E8B57;      // Sea green
constexpr uint32_t COLOR_DIRT = 0xFF8B7355;        // Tan
constexpr uint32_t COLOR_FOREST = 0xFF228B22;      // Forest green
constexpr uint32_t COLOR_ROCKS = 0xFF696969;       // Dim gray
constexpr uint32_t COLOR_ASPHALT = 0xFF3C3C3C;     // Dark gray
constexpr uint32_t COLOR_CONCRETE = 0xFF808080;    // Gray
constexpr uint32_t COLOR_BRICKS = 0xFFB22222;      // Fire brick
constexpr uint32_t COLOR_WOOD = 0xFFDEB887;        // Burlywood
constexpr uint32_t COLOR_WATER = 0xFF4169E1;       // Royal blue
constexpr uint32_t COLOR_METAL = 0xFF708090;       // Slate gray
constexpr uint32_t COLOR_STONE = 0xFFA9A9A9;       // Dark gray
constexpr uint32_t COLOR_WALL = 0xFF8B4513;        // Saddle brown
constexpr uint32_t COLOR_DEFAULT = 0xFF000000;     // Black

// Entity colors
constexpr uint32_t COLOR_ENTITY_NPC = 0xFF00FF00;      // Green
constexpr uint32_t COLOR_ENTITY_ENEMY = 0xFFFF0000;    // Red
constexpr uint32_t COLOR_ENTITY_RESOURCE = 0xFFFFFF00; // Yellow
constexpr uint32_t COLOR_ENTITY_WILDLIFE = 0xFFFF8C00; // Dark orange

// Foliage color
constexpr uint32_t COLOR_FOLIAGE = 0xFF006400;     // Dark green

// Zone colors
constexpr uint32_t COLOR_ZONE_SAFE = 0x4000FF00;     // Transparent green
constexpr uint32_t COLOR_ZONE_DANGER = 0x40FF0000;   // Transparent red
constexpr uint32_t COLOR_ZONE_LOOT = 0x40FFFF00;     // Transparent yellow

} // anonymous namespace

// ============================================================================
// PCGPreview Implementation
// ============================================================================

PCGPreview::PCGPreview() {
    // Enable all features by default
    for (int i = 0; i < static_cast<int>(PCGStage::COUNT); ++i) {
        m_stageEnabled[i] = true;
    }
}

PCGPreview::~PCGPreview() {
    Cancel();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void PCGPreview::SetQuality(PreviewQuality quality) {
    m_quality = quality;
    m_customWidth = 0;
    m_customHeight = 0;
}

void PCGPreview::SetDimensions(int width, int height) {
    m_customWidth = width;
    m_customHeight = height;
}

void PCGPreview::SetFeatureEnabled(PCGStage stage, bool enabled) {
    int idx = static_cast<int>(stage);
    if (idx >= 0 && idx < static_cast<int>(PCGStage::COUNT)) {
        m_stageEnabled[idx] = enabled;
    }
}

bool PCGPreview::IsFeatureEnabled(PCGStage stage) const {
    int idx = static_cast<int>(stage);
    if (idx >= 0 && idx < static_cast<int>(PCGStage::COUNT)) {
        return m_stageEnabled[idx];
    }
    return false;
}

PreviewResult PCGPreview::Generate(PCGPipeline& pipeline) {
    PreviewResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    m_running.store(true);
    m_cancelled.store(false);

    if (m_progressCallback) {
        m_progressCallback(0.0f, "Starting preview generation");
    }

    // Generate with pipeline in preview mode
    auto pipelineResult = pipeline.GeneratePreview();

    if (m_cancelled.load()) {
        result.cancelled = true;
        result.errorMessage = "Generation cancelled";
        m_running.store(false);
        return result;
    }

    if (!pipelineResult.success) {
        result.errorMessage = pipelineResult.errorMessage;
        m_running.store(false);
        return result;
    }

    if (m_progressCallback) {
        m_progressCallback(0.5f, "Rendering preview");
    }

    // Render to image
    const PCGContext* ctx = pipeline.GetLastContext();
    if (ctx) {
        result = GenerateFromContext(*ctx);
    } else {
        result.errorMessage = "No context available";
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.generationTime = std::chrono::duration<float>(endTime - startTime).count();

    m_lastResult = result;
    m_running.store(false);

    if (m_progressCallback) {
        m_progressCallback(1.0f, "Preview complete");
    }

    return result;
}

PreviewResult PCGPreview::GenerateFromContext(const PCGContext& context) {
    PreviewResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    int scaleFactor = GetScaleFactor();
    int width = (m_customWidth > 0) ? m_customWidth : context.GetWidth() / scaleFactor;
    int height = (m_customHeight > 0) ? m_customHeight : context.GetHeight() / scaleFactor;

    width = std::max(16, width);
    height = std::max(16, height);

    result.image.Resize(width, height);

    // Render based on visualization mode
    RenderContext(context, result.image);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.generationTime = std::chrono::duration<float>(endTime - startTime).count();
    result.success = true;

    m_lastResult = result;
    return result;
}

void PCGPreview::RenderContext(const PCGContext& context, PreviewImage& image) {
    switch (m_vizMode) {
        case VisualizationMode::Tiles:
            RenderTiles(context, image);
            break;
        case VisualizationMode::Biomes:
            RenderBiomes(context, image);
            break;
        case VisualizationMode::Elevation:
            RenderElevation(context, image);
            break;
        case VisualizationMode::Roads:
            RenderTiles(context, image);
            RenderRoads(context, image);
            break;
        case VisualizationMode::Buildings:
            RenderTiles(context, image);
            RenderBuildings(context, image);
            break;
        case VisualizationMode::Zones:
            RenderTiles(context, image);
            RenderZones(context, image);
            break;
        case VisualizationMode::Occupancy:
            RenderOccupancy(context, image);
            break;
    }

    // Overlay entities and foliage if enabled
    if (IsFeatureEnabled(PCGStage::Entities)) {
        RenderEntities(context, image);
    }
    if (IsFeatureEnabled(PCGStage::Foliage)) {
        RenderFoliage(context, image);
    }
}

void PCGPreview::RenderTiles(const PCGContext& context, PreviewImage& image) {
    int contextW = context.GetWidth();
    int contextH = context.GetHeight();
    const auto& tiles = context.GetTiles();

    float scaleX = static_cast<float>(contextW) / image.width;
    float scaleY = static_cast<float>(contextH) / image.height;

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int cx = static_cast<int>(x * scaleX);
            int cy = static_cast<int>(y * scaleY);

            if (cx < contextW && cy < contextH) {
                const Tile& tile = tiles[cy * contextW + cx];
                uint32_t color = GetTileColor(tile.type);

                // Darken walls
                if (tile.isWall) {
                    uint32_t r = (color >> 16) & 0xFF;
                    uint32_t g = (color >> 8) & 0xFF;
                    uint32_t b = color & 0xFF;
                    r = r * 2 / 3;
                    g = g * 2 / 3;
                    b = b * 2 / 3;
                    color = 0xFF000000 | (r << 16) | (g << 8) | b;
                }

                image.SetPixel(x, y, color);
            }
        }
    }
}

void PCGPreview::RenderElevation(const PCGContext& context, PreviewImage& image) {
    int contextW = context.GetWidth();
    int contextH = context.GetHeight();
    const auto& elevations = context.GetElevations();

    // Find elevation range
    float minElev = *std::min_element(elevations.begin(), elevations.end());
    float maxElev = *std::max_element(elevations.begin(), elevations.end());
    float range = maxElev - minElev;
    if (range < 0.001f) range = 1.0f;

    float scaleX = static_cast<float>(contextW) / image.width;
    float scaleY = static_cast<float>(contextH) / image.height;

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int cx = static_cast<int>(x * scaleX);
            int cy = static_cast<int>(y * scaleY);

            if (cx < contextW && cy < contextH) {
                float elev = elevations[cy * contextW + cx];
                float normalized = (elev - minElev) / range;

                // Gradient from blue (low) to green (mid) to white (high)
                uint32_t color;
                if (normalized < 0.3f) {
                    // Blue to cyan
                    float t = normalized / 0.3f;
                    uint8_t r = static_cast<uint8_t>(t * 128);
                    uint8_t g = static_cast<uint8_t>(t * 255);
                    uint8_t b = 255;
                    color = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else if (normalized < 0.6f) {
                    // Cyan to green
                    float t = (normalized - 0.3f) / 0.3f;
                    uint8_t r = static_cast<uint8_t>(128 * (1 - t));
                    uint8_t g = 255;
                    uint8_t b = static_cast<uint8_t>(255 * (1 - t));
                    color = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else {
                    // Green to white
                    float t = (normalized - 0.6f) / 0.4f;
                    uint8_t r = static_cast<uint8_t>(255 * t);
                    uint8_t g = 255;
                    uint8_t b = static_cast<uint8_t>(255 * t);
                    color = 0xFF000000 | (r << 16) | (g << 8) | b;
                }

                image.SetPixel(x, y, color);
            }
        }
    }
}

void PCGPreview::RenderBiomes(const PCGContext& context, PreviewImage& image) {
    int contextW = context.GetWidth();
    int contextH = context.GetHeight();

    float scaleX = static_cast<float>(contextW) / image.width;
    float scaleY = static_cast<float>(contextH) / image.height;

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int cx = static_cast<int>(x * scaleX);
            int cy = static_cast<int>(y * scaleY);

            if (cx < contextW && cy < contextH) {
                BiomeType biome = context.GetBiome(cx, cy);
                image.SetPixel(x, y, GetBiomeColor(biome));
            }
        }
    }
}

void PCGPreview::RenderRoads(const PCGContext& context, PreviewImage& image) {
    // Roads are already rendered in tiles, just highlight them
    // This overlay mode could add road markings or highlights
}

void PCGPreview::RenderBuildings(const PCGContext& context, PreviewImage& image) {
    // Buildings are already rendered in tiles via walls
    // This overlay mode could add building outlines
}

void PCGPreview::RenderZones(const PCGContext& context, PreviewImage& image) {
    int contextW = context.GetWidth();
    int contextH = context.GetHeight();

    float scaleX = static_cast<float>(contextW) / image.width;
    float scaleY = static_cast<float>(contextH) / image.height;

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int cx = static_cast<int>(x * scaleX);
            int cy = static_cast<int>(y * scaleY);

            if (cx < contextW && cy < contextH) {
                std::string zone = context.GetZone(cx, cy);
                if (!zone.empty()) {
                    // Blend zone color with existing
                    uint32_t existing = image.GetPixel(x, y);
                    uint32_t overlay = 0;

                    if (zone.find("safe") != std::string::npos) {
                        overlay = COLOR_ZONE_SAFE;
                    } else if (zone.find("danger") != std::string::npos) {
                        overlay = COLOR_ZONE_DANGER;
                    } else if (zone.find("loot") != std::string::npos) {
                        overlay = COLOR_ZONE_LOOT;
                    }

                    if (overlay != 0) {
                        // Simple alpha blend
                        float alpha = ((overlay >> 24) & 0xFF) / 255.0f;
                        uint8_t r = static_cast<uint8_t>(((existing >> 16) & 0xFF) * (1 - alpha) +
                                                         ((overlay >> 16) & 0xFF) * alpha);
                        uint8_t g = static_cast<uint8_t>(((existing >> 8) & 0xFF) * (1 - alpha) +
                                                         ((overlay >> 8) & 0xFF) * alpha);
                        uint8_t b = static_cast<uint8_t>((existing & 0xFF) * (1 - alpha) +
                                                         (overlay & 0xFF) * alpha);
                        image.SetPixel(x, y, 0xFF000000 | (r << 16) | (g << 8) | b);
                    }
                }
            }
        }
    }
}

void PCGPreview::RenderOccupancy(const PCGContext& context, PreviewImage& image) {
    int contextW = context.GetWidth();
    int contextH = context.GetHeight();

    float scaleX = static_cast<float>(contextW) / image.width;
    float scaleY = static_cast<float>(contextH) / image.height;

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            int cx = static_cast<int>(x * scaleX);
            int cy = static_cast<int>(y * scaleY);

            if (cx < contextW && cy < contextH) {
                bool occupied = context.IsOccupied(cx, cy);
                image.SetPixel(x, y, occupied ? 0xFFFF0000 : 0xFF00FF00);
            }
        }
    }
}

void PCGPreview::RenderEntities(const PCGContext& context, PreviewImage& image) {
    const auto& spawns = context.GetEntitySpawns();

    float scaleX = image.width / static_cast<float>(context.GetWidth());
    float scaleY = image.height / static_cast<float>(context.GetHeight());

    for (const auto& spawn : spawns) {
        int x = static_cast<int>(spawn.position.x * scaleX);
        int y = static_cast<int>(spawn.position.z * scaleY);

        // Select color based on entity type
        uint32_t color = COLOR_ENTITY_NPC;
        if (spawn.entityType.find("zombie") != std::string::npos ||
            spawn.entityType.find("enemy") != std::string::npos) {
            color = COLOR_ENTITY_ENEMY;
        } else if (spawn.entityType.find("loot") != std::string::npos ||
                   spawn.entityType.find("ammo") != std::string::npos ||
                   spawn.entityType.find("health") != std::string::npos) {
            color = COLOR_ENTITY_RESOURCE;
        } else if (spawn.entityType.find("crow") != std::string::npos ||
                   spawn.entityType.find("rat") != std::string::npos ||
                   spawn.entityType.find("dog") != std::string::npos) {
            color = COLOR_ENTITY_WILDLIFE;
        }

        // Draw 3x3 marker
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                image.SetPixel(x + dx, y + dy, color);
            }
        }
    }
}

void PCGPreview::RenderFoliage(const PCGContext& context, PreviewImage& image) {
    const auto& spawns = context.GetFoliageSpawns();

    float scaleX = image.width / static_cast<float>(context.GetWidth());
    float scaleY = image.height / static_cast<float>(context.GetHeight());

    for (const auto& spawn : spawns) {
        int x = static_cast<int>(spawn.position.x * scaleX);
        int y = static_cast<int>(spawn.position.z * scaleY);

        // Draw small marker
        image.SetPixel(x, y, COLOR_FOLIAGE);
    }
}

void PCGPreview::SetProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCallback = std::move(callback);
}

void PCGPreview::SetCompletionCallback(CompletionCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completionCallback = std::move(callback);
}

bool PCGPreview::StartAsync(PCGPipeline& pipeline) {
    if (m_running.load()) return false;

    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_thread = std::thread([this, &pipeline]() {
        PreviewResult result = Generate(pipeline);

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_completionCallback) {
            m_completionCallback(result);
        }
    });

    return true;
}

void PCGPreview::Cancel() {
    m_cancelled.store(true);
}

bool PCGPreview::Wait(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();

    while (m_running.load()) {
        if (timeoutMs >= 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= timeoutMs) {
                return false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

void PCGPreview::SetVisualizationMode(VisualizationMode mode) {
    m_vizMode = mode;
}

int PCGPreview::GetScaleFactor() const {
    switch (m_quality) {
        case PreviewQuality::VeryLow: return 8;
        case PreviewQuality::Low: return 4;
        case PreviewQuality::Medium: return 2;
        case PreviewQuality::High: return 1;
        default: return 2;
    }
}

uint32_t PCGPreview::GetTileColor(TileType type) {
    switch (type) {
        case TileType::GroundGrass1: return COLOR_GRASS;
        case TileType::GroundGrass2: return COLOR_GRASS2;
        case TileType::GroundDirt: return COLOR_DIRT;
        case TileType::GroundForest1:
        case TileType::GroundForest2: return COLOR_FOREST;
        case TileType::GroundRocks: return COLOR_ROCKS;
        case TileType::ConcreteAsphalt1:
        case TileType::ConcreteAsphalt2: return COLOR_ASPHALT;
        case TileType::ConcreteBlocks1:
        case TileType::ConcreteBlocks2:
        case TileType::ConcretePad:
        case TileType::ConcreteTiles1:
        case TileType::ConcreteTiles2: return COLOR_CONCRETE;
        case TileType::BricksBlack:
        case TileType::BricksGrey:
        case TileType::BricksRock:
        case TileType::BricksStacked: return COLOR_BRICKS;
        case TileType::Wood1:
        case TileType::WoodFlooring1:
        case TileType::WoodFlooring2: return COLOR_WOOD;
        case TileType::Water1: return COLOR_WATER;
        case TileType::Metal1:
        case TileType::Metal2:
        case TileType::Metal3:
        case TileType::Metal4: return COLOR_METAL;
        case TileType::StoneBlack:
        case TileType::StoneMarble1:
        case TileType::StoneMarble2:
        case TileType::StoneRaw: return COLOR_STONE;
        default: return COLOR_DEFAULT;
    }
}

uint32_t PCGPreview::GetBiomeColor(BiomeType biome) {
    switch (biome) {
        case BiomeType::Urban: return 0xFF808080;       // Gray
        case BiomeType::Suburban: return 0xFFA0A060;    // Olive
        case BiomeType::Rural: return 0xFF90EE90;       // Light green
        case BiomeType::Forest: return 0xFF228B22;      // Forest green
        case BiomeType::Desert: return 0xFFF4A460;      // Sandy brown
        case BiomeType::Grassland: return 0xFF32CD32;   // Lime green
        case BiomeType::Wetland: return 0xFF2F4F4F;     // Dark slate gray
        case BiomeType::Mountain: return 0xFF696969;    // Dim gray
        case BiomeType::Water: return 0xFF4169E1;       // Royal blue
        case BiomeType::Industrial: return 0xFFA9A9A9;  // Dark gray
        case BiomeType::Commercial: return 0xFFFFD700;  // Gold
        case BiomeType::Residential: return 0xFFDDA0DD; // Plum
        case BiomeType::Park: return 0xFF00FF00;        // Green
        default: return 0xFF000000;
    }
}

uint32_t PCGPreview::GetRoadColor(RoadType type) {
    switch (type) {
        case RoadType::Highway: return 0xFF1C1C1C;
        case RoadType::MainRoad: return 0xFF2C2C2C;
        case RoadType::SecondaryRoad: return 0xFF3C3C3C;
        case RoadType::ResidentialStreet: return 0xFF4C4C4C;
        case RoadType::Path: return 0xFF8B7355;
        default: return 0xFF3C3C3C;
    }
}

std::vector<uint8_t> PCGPreview::EncodeToPNG() const {
    // Simple PNG encoding would go here
    // For now, return empty
    return {};
}

bool PCGPreview::SaveToFile(const std::string& filepath) const {
    // Would save to file
    (void)filepath;
    return false;
}

PreviewImage PCGPreview::Upscale(int targetWidth, int targetHeight) const {
    PreviewImage result;
    result.Resize(targetWidth, targetHeight);

    const PreviewImage& src = m_lastResult.image;
    if (!src.IsValid()) return result;

    float scaleX = static_cast<float>(src.width) / targetWidth;
    float scaleY = static_cast<float>(src.height) / targetHeight;

    for (int y = 0; y < targetHeight; ++y) {
        for (int x = 0; x < targetWidth; ++x) {
            int sx = static_cast<int>(x * scaleX);
            int sy = static_cast<int>(y * scaleY);
            result.SetPixel(x, y, src.GetPixel(sx, sy));
        }
    }

    return result;
}

} // namespace Vehement
