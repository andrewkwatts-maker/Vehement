#pragma once

#include <cstdint>

namespace Vehement {

/**
 * @brief Tile types covering all textures in Vehement2/images
 *
 * Categories:
 * - Ground tiles (grass, dirt, forest, rocks)
 * - Concrete tiles (asphalt, tiles, blocks)
 * - Brick tiles (with all corner variants)
 * - Wood tiles
 * - Water tiles
 * - Metal tiles
 * - Stone tiles
 */
enum class TileType : uint16_t {
    // Empty/None
    None = 0,

    // Ground tiles (Vehement2/images/Ground/)
    GroundGrass1,
    GroundGrass2,
    GroundDirt,
    GroundForest1,
    GroundForest2,
    GroundRocks,

    // Concrete tiles (Vehement2/images/Concrete/)
    ConcreteAsphalt1,
    ConcreteAsphalt2,
    ConcreteAsphaltSteps1,
    ConcreteAsphaltSteps2,
    ConcreteBlocks1,
    ConcreteBlocks2,
    ConcretePad,
    ConcreteTiles1,
    ConcreteTiles2,

    // Brick tiles - Main (Vehement2/images/Bricks/)
    BricksBlack,
    BricksGrey,
    BricksRock,
    BricksStacked,

    // Brick wall front textures (for side faces)
    BricksRockFrontTop,
    BricksRockFrontBottom,
    BricksRockFrontLeft,
    BricksRockFrontRight,

    // Brick corners - Outer (Vehement2/images/Bricks/Courners/)
    BricksCornerTopLeftOuter,
    BricksCornerTopRightOuter,
    BricksCornerBottomLeftOuter,
    BricksCornerBottomRightOuter,

    // Brick corners - Inner
    BricksCornerTopLeftInner,
    BricksCornerTopRightInner,
    BricksCornerBottomLeftInner,
    BricksCornerBottomRightInner,

    // Brick corners - Regular
    BricksCornerTopLeft,
    BricksCornerTopRight,
    BricksCornerBottomLeft,
    BricksCornerBottomRight,

    // Wood tiles (Vehement2/images/Wood/)
    Wood1,
    WoodCrate1,
    WoodCrate2,
    WoodFlooring1,
    WoodFlooring2,

    // Water tiles (Vehement2/images/Water/)
    Water1,

    // Metal tiles (Vehement2/images/Metal/)
    Metal1,
    Metal2,
    Metal3,
    Metal4,
    MetalTile1,
    MetalTile2,
    MetalTile3,
    MetalTile4,
    MetalShopFront,
    MetalShopFrontBottom,
    MetalShopFrontLeft,
    MetalShopFrontRight,
    MetalShopFrontTop,

    // Stone tiles (Vehement2/images/Stone/)
    StoneBlack,
    StoneMarble1,
    StoneMarble2,
    StoneRaw,

    // Count for iteration
    COUNT
};

/**
 * @brief Wall orientation for determining which side textures to use
 */
enum class WallOrientation : uint8_t {
    None = 0,
    North = 1 << 0,  // Wall face points north (-Z)
    South = 1 << 1,  // Wall face points south (+Z)
    East  = 1 << 2,  // Wall face points east (+X)
    West  = 1 << 3,  // Wall face points west (-X)
    All   = North | South | East | West
};

inline WallOrientation operator|(WallOrientation a, WallOrientation b) {
    return static_cast<WallOrientation>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline WallOrientation operator&(WallOrientation a, WallOrientation b) {
    return static_cast<WallOrientation>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasOrientation(WallOrientation flags, WallOrientation test) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

/**
 * @brief Animation type for animated tiles
 */
enum class TileAnimation : uint8_t {
    None = 0,
    Water,       // Water ripple animation
    Flicker,     // Light flicker (for metal/industrial)
    Scroll       // Scrolling texture (conveyor belts)
};

/**
 * @brief Represents a single tile in the world
 *
 * Tiles can be either flat ground textures rendered at Y=0
 * or walls that extrude upward in 3D space.
 */
struct Tile {
    TileType type = TileType::None;

    // Wall properties
    bool isWall = false;
    float wallHeight = 2.0f;         // Height of wall extrusion (in world units)
    WallOrientation wallFaces = WallOrientation::All;  // Which faces to render

    // Side texture for walls (uses main type if None)
    TileType wallSideTexture = TileType::None;
    TileType wallTopTexture = TileType::None;

    // Gameplay properties
    bool isWalkable = true;          // Can entities walk on/through this tile
    bool blocksSight = false;        // Does this tile block line of sight
    bool isDamaging = false;         // Does this tile cause damage (e.g., water, fire)
    float damagePerSecond = 0.0f;    // Damage amount if isDamaging
    float movementCost = 1.0f;       // Pathfinding cost multiplier

    // Visual properties
    uint8_t textureVariant = 0;      // Variant index for tiles with multiple looks
    TileAnimation animation = TileAnimation::None;
    float animationSpeed = 1.0f;     // Animation playback speed multiplier
    uint8_t rotation = 0;            // 0, 90, 180, 270 degrees

    // Lighting
    float lightEmission = 0.0f;      // How much light this tile emits (0-1)

    /**
     * @brief Default constructor creates an empty, walkable tile
     */
    Tile() = default;

    /**
     * @brief Construct a ground tile
     */
    static Tile Ground(TileType groundType) {
        Tile t;
        t.type = groundType;
        t.isWall = false;
        t.isWalkable = true;
        return t;
    }

    /**
     * @brief Construct a wall tile
     */
    static Tile Wall(TileType topTexture, TileType sideTexture = TileType::None,
                     float height = 2.0f) {
        Tile t;
        t.type = topTexture;
        t.isWall = true;
        t.wallHeight = height;
        t.wallSideTexture = (sideTexture == TileType::None) ? topTexture : sideTexture;
        t.wallTopTexture = topTexture;
        t.isWalkable = false;
        t.blocksSight = true;
        return t;
    }

    /**
     * @brief Construct a water tile
     */
    static Tile Water() {
        Tile t;
        t.type = TileType::Water1;
        t.isWall = false;
        t.isWalkable = true;
        t.isDamaging = false;  // Not damaging by default, can be set
        t.movementCost = 2.0f; // Slower movement through water
        t.animation = TileAnimation::Water;
        return t;
    }

    /**
     * @brief Check if this tile blocks movement
     */
    bool BlocksMovement() const {
        return !isWalkable || (isWall && wallHeight > 0.0f);
    }

    /**
     * @brief Get effective side texture (falls back to main type)
     */
    TileType GetSideTexture() const {
        return (wallSideTexture != TileType::None) ? wallSideTexture : type;
    }

    /**
     * @brief Get effective top texture (falls back to main type)
     */
    TileType GetTopTexture() const {
        return (wallTopTexture != TileType::None) ? wallTopTexture : type;
    }
};

/**
 * @brief Get the display name for a tile type
 */
inline const char* GetTileTypeName(TileType type) {
    switch (type) {
        case TileType::None: return "None";

        // Ground
        case TileType::GroundGrass1: return "Grass 1";
        case TileType::GroundGrass2: return "Grass 2";
        case TileType::GroundDirt: return "Dirt";
        case TileType::GroundForest1: return "Forest 1";
        case TileType::GroundForest2: return "Forest 2";
        case TileType::GroundRocks: return "Rocks";

        // Concrete
        case TileType::ConcreteAsphalt1: return "Asphalt 1";
        case TileType::ConcreteAsphalt2: return "Asphalt 2";
        case TileType::ConcreteAsphaltSteps1: return "Asphalt Steps 1";
        case TileType::ConcreteAsphaltSteps2: return "Asphalt Steps 2";
        case TileType::ConcreteBlocks1: return "Concrete Blocks 1";
        case TileType::ConcreteBlocks2: return "Concrete Blocks 2";
        case TileType::ConcretePad: return "Concrete Pad";
        case TileType::ConcreteTiles1: return "Tiles 1";
        case TileType::ConcreteTiles2: return "Tiles 2";

        // Bricks
        case TileType::BricksBlack: return "Black Bricks";
        case TileType::BricksGrey: return "Grey Bricks";
        case TileType::BricksRock: return "Rock Bricks";
        case TileType::BricksStacked: return "Stacked Bricks";
        case TileType::BricksRockFrontTop: return "Brick Front Top";
        case TileType::BricksRockFrontBottom: return "Brick Front Bottom";
        case TileType::BricksRockFrontLeft: return "Brick Front Left";
        case TileType::BricksRockFrontRight: return "Brick Front Right";

        // Brick corners
        case TileType::BricksCornerTopLeftOuter: return "Brick Corner TL Outer";
        case TileType::BricksCornerTopRightOuter: return "Brick Corner TR Outer";
        case TileType::BricksCornerBottomLeftOuter: return "Brick Corner BL Outer";
        case TileType::BricksCornerBottomRightOuter: return "Brick Corner BR Outer";
        case TileType::BricksCornerTopLeftInner: return "Brick Corner TL Inner";
        case TileType::BricksCornerTopRightInner: return "Brick Corner TR Inner";
        case TileType::BricksCornerBottomLeftInner: return "Brick Corner BL Inner";
        case TileType::BricksCornerBottomRightInner: return "Brick Corner BR Inner";
        case TileType::BricksCornerTopLeft: return "Brick Corner TL";
        case TileType::BricksCornerTopRight: return "Brick Corner TR";
        case TileType::BricksCornerBottomLeft: return "Brick Corner BL";
        case TileType::BricksCornerBottomRight: return "Brick Corner BR";

        // Wood
        case TileType::Wood1: return "Wood";
        case TileType::WoodCrate1: return "Wood Crate 1";
        case TileType::WoodCrate2: return "Wood Crate 2";
        case TileType::WoodFlooring1: return "Wood Flooring 1";
        case TileType::WoodFlooring2: return "Wood Flooring 2";

        // Water
        case TileType::Water1: return "Water";

        // Metal
        case TileType::Metal1: return "Metal 1";
        case TileType::Metal2: return "Metal 2";
        case TileType::Metal3: return "Metal 3";
        case TileType::Metal4: return "Metal 4";
        case TileType::MetalTile1: return "Metal Tile 1";
        case TileType::MetalTile2: return "Metal Tile 2";
        case TileType::MetalTile3: return "Metal Tile 3";
        case TileType::MetalTile4: return "Metal Tile 4";
        case TileType::MetalShopFront: return "Shop Front";
        case TileType::MetalShopFrontBottom: return "Shop Front Bottom";
        case TileType::MetalShopFrontLeft: return "Shop Front Left";
        case TileType::MetalShopFrontRight: return "Shop Front Right";
        case TileType::MetalShopFrontTop: return "Shop Front Top";

        // Stone
        case TileType::StoneBlack: return "Black Stone";
        case TileType::StoneMarble1: return "Marble 1";
        case TileType::StoneMarble2: return "Marble 2";
        case TileType::StoneRaw: return "Raw Stone";

        default: return "Unknown";
    }
}

/**
 * @brief Check if a tile type is a ground texture
 */
inline bool IsGroundTile(TileType type) {
    return type >= TileType::GroundGrass1 && type <= TileType::GroundRocks;
}

/**
 * @brief Check if a tile type is a wall texture
 */
inline bool IsWallTile(TileType type) {
    return type >= TileType::BricksBlack && type <= TileType::BricksCornerBottomRight;
}

/**
 * @brief Check if a tile type is water
 */
inline bool IsWaterTile(TileType type) {
    return type == TileType::Water1;
}

} // namespace Vehement
