#pragma once

#include "Tile.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {
    class Texture;
    class TextureManager;
}

namespace Vehement {

/**
 * @brief UV coordinates for a texture region in the atlas
 */
struct TextureRegion {
    glm::vec2 uvMin{0.0f, 0.0f};  // Bottom-left UV
    glm::vec2 uvMax{1.0f, 1.0f};  // Top-right UV
    int atlasIndex = 0;           // Which atlas texture this region is in
    bool valid = false;

    /**
     * @brief Get UV coordinates for a corner
     * @param corner 0=BL, 1=BR, 2=TR, 3=TL
     */
    glm::vec2 GetUV(int corner) const {
        switch (corner) {
            case 0: return uvMin;                              // Bottom-left
            case 1: return glm::vec2(uvMax.x, uvMin.y);        // Bottom-right
            case 2: return uvMax;                              // Top-right
            case 3: return glm::vec2(uvMin.x, uvMax.y);        // Top-left
            default: return uvMin;
        }
    }

    /**
     * @brief Get UV for a rotated texture
     * @param corner 0=BL, 1=BR, 2=TR, 3=TL
     * @param rotation 0, 90, 180, or 270 degrees
     */
    glm::vec2 GetRotatedUV(int corner, int rotation) const {
        int rotatedCorner = (corner + (rotation / 90)) % 4;
        return GetUV(rotatedCorner);
    }
};

/**
 * @brief Configuration for atlas generation
 */
struct TileAtlasConfig {
    std::string textureBasePath = "Vehement2/images/";
    int atlasSize = 2048;        // Atlas texture size
    int tilePadding = 2;         // Padding between tiles to prevent bleeding
    bool generateMipmaps = true;
    bool sRGB = true;
};

/**
 * @brief Manages tile textures and creates texture atlases
 *
 * Loads all textures from Vehement2/images/ and organizes them
 * into texture atlases for efficient batch rendering.
 */
class TileAtlas {
public:
    TileAtlas();
    ~TileAtlas();

    // Non-copyable
    TileAtlas(const TileAtlas&) = delete;
    TileAtlas& operator=(const TileAtlas&) = delete;

    /**
     * @brief Initialize the atlas with a texture manager
     * @param textureManager The engine's texture manager
     * @param config Atlas configuration
     */
    bool Initialize(Nova::TextureManager& textureManager,
                    const TileAtlasConfig& config = {});

    /**
     * @brief Load all tile textures from the base path
     */
    bool LoadTextures();

    /**
     * @brief Build texture atlas from loaded textures
     * @return true if atlas was built successfully
     */
    bool BuildAtlas();

    /**
     * @brief Get texture region for a tile type
     */
    const TextureRegion& GetTextureRegion(TileType type) const;

    /**
     * @brief Get the texture path for a tile type
     */
    std::string GetTexturePath(TileType type) const;

    /**
     * @brief Get individual texture for a tile type (non-atlas mode)
     */
    std::shared_ptr<Nova::Texture> GetTexture(TileType type) const;

    /**
     * @brief Get atlas texture by index
     */
    std::shared_ptr<Nova::Texture> GetAtlasTexture(int index = 0) const;

    /**
     * @brief Get number of atlas textures
     */
    size_t GetAtlasCount() const { return m_atlasTextures.size(); }

    /**
     * @brief Check if atlas mode is enabled
     */
    bool IsAtlasMode() const { return m_useAtlas; }

    /**
     * @brief Enable/disable atlas mode
     */
    void SetAtlasMode(bool enabled) { m_useAtlas = enabled; }

    /**
     * @brief Bind texture for a tile type
     * @param type The tile type
     * @param slot Texture slot to bind to
     */
    void BindTexture(TileType type, uint32_t slot = 0) const;

    /**
     * @brief Bind atlas texture
     * @param atlasIndex Atlas index
     * @param slot Texture slot to bind to
     */
    void BindAtlas(int atlasIndex, uint32_t slot = 0) const;

    /**
     * @brief Get all loaded texture types
     */
    std::vector<TileType> GetLoadedTypes() const;

    /**
     * @brief Check if a tile type has a loaded texture
     */
    bool HasTexture(TileType type) const;

    /**
     * @brief Get wall side texture type for a given wall type
     * Automatically selects appropriate BricksRockFront variants
     */
    static TileType GetWallSideTexture(TileType wallType, WallOrientation face);

private:
    /**
     * @brief Register texture path for a tile type
     */
    void RegisterTexturePath(TileType type, const std::string& relativePath);

    /**
     * @brief Load a single texture
     */
    bool LoadTextureForType(TileType type);

    Nova::TextureManager* m_textureManager = nullptr;
    TileAtlasConfig m_config;
    bool m_initialized = false;
    bool m_useAtlas = false;

    // Texture storage
    std::unordered_map<TileType, std::shared_ptr<Nova::Texture>> m_textures;
    std::unordered_map<TileType, std::string> m_texturePaths;
    std::unordered_map<TileType, TextureRegion> m_textureRegions;

    // Atlas textures
    std::vector<std::shared_ptr<Nova::Texture>> m_atlasTextures;

    // Default region for missing textures
    TextureRegion m_defaultRegion;
};

/**
 * @brief Helper to get wall side texture based on orientation
 */
inline TileType GetWallSideTextureForFace(TileType baseType, WallOrientation face) {
    // Map base wall types to their directional variants
    if (baseType == TileType::BricksRock || baseType == TileType::BricksBlack ||
        baseType == TileType::BricksGrey || baseType == TileType::BricksStacked) {

        switch (face) {
            case WallOrientation::North:
            case WallOrientation::South:
                return TileType::BricksRockFrontTop;
            case WallOrientation::East:
                return TileType::BricksRockFrontRight;
            case WallOrientation::West:
                return TileType::BricksRockFrontLeft;
            default:
                return TileType::BricksRockFrontTop;
        }
    }

    // Metal shop fronts
    if (baseType == TileType::MetalShopFront) {
        switch (face) {
            case WallOrientation::North: return TileType::MetalShopFrontTop;
            case WallOrientation::South: return TileType::MetalShopFrontBottom;
            case WallOrientation::East: return TileType::MetalShopFrontRight;
            case WallOrientation::West: return TileType::MetalShopFrontLeft;
            default: return TileType::MetalShopFront;
        }
    }

    // Default: use the base type for all sides
    return baseType;
}

} // namespace Vehement
