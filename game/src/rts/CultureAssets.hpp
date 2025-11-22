#pragma once

/**
 * @file CultureAssets.hpp
 * @brief Mapping of cultures to visual assets from Vehement2/images/
 *
 * This file defines the texture and visual asset mappings for each culture,
 * utilizing the existing texture library from the Vehement2 game assets.
 * All paths are relative to the game's asset root directory.
 */

#include "Culture.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <memory>

namespace Nova {
    class Texture;
    class TextureManager;
}

namespace Vehement {
namespace RTS {

/**
 * @brief Asset path definitions organized by category
 *
 * All texture paths reference existing assets in:
 * Vehement2/images/
 */
namespace AssetPaths {

    // =========================================================================
    // Base texture directories
    // =========================================================================
    constexpr const char* IMAGES_ROOT = "Vehement2/images/";

    // Category directories
    constexpr const char* BRICKS_DIR = "Vehement2/images/Bricks/";
    constexpr const char* CONCRETE_DIR = "Vehement2/images/Concrete/";
    constexpr const char* METAL_DIR = "Vehement2/images/Metal/";
    constexpr const char* STONE_DIR = "Vehement2/images/Stone/";
    constexpr const char* TEXTILES_DIR = "Vehement2/images/Textiles/";
    constexpr const char* WOOD_DIR = "Vehement2/images/Wood/";
    constexpr const char* FOLIAGE_DIR = "Vehement2/images/Follage/";
    constexpr const char* PEOPLE_DIR = "Vehement2/images/People/";
    constexpr const char* GROUND_DIR = "Vehement2/images/Ground/";
    constexpr const char* OBJECTS_DIR = "Vehement2/images/Objects/";

    // =========================================================================
    // Brick textures - For Fortress culture walls and towers
    // =========================================================================
    namespace Bricks {
        constexpr const char* Rock = "Vehement2/images/Bricks/BricksRock.png";
        constexpr const char* Grey = "Vehement2/images/Bricks/BricksGrey.png";
        constexpr const char* Black = "Vehement2/images/Bricks/BricksBlack.png";
        constexpr const char* Stacked = "Vehement2/images/Bricks/BricksStacked.png";
        constexpr const char* RockFront = "Vehement2/images/Bricks/BricksRockFrontBOT.png";
        constexpr const char* RockFrontTop = "Vehement2/images/Bricks/BricksRockFrontTOP.png";
        constexpr const char* RockFrontLHS = "Vehement2/images/Bricks/BricksRockFrontLHS.png";
        constexpr const char* RockFrontRHS = "Vehement2/images/Bricks/BricksRockFrontRHS.png";

        // Corner pieces for wall construction
        namespace Corners {
            constexpr const char* TopLeft = "Vehement2/images/Bricks/Courners/BricksRockAspheltTL.png";
            constexpr const char* TopRight = "Vehement2/images/Bricks/Courners/BricksRockAspheltTR.png";
            constexpr const char* BottomLeft = "Vehement2/images/Bricks/Courners/BricksRockAspheltBL.png";
            constexpr const char* BottomRight = "Vehement2/images/Bricks/Courners/BricksRockAspheltBR.png";
        }
    }

    // =========================================================================
    // Stone textures - For Fortress and Underground cultures
    // =========================================================================
    namespace Stone {
        constexpr const char* Raw = "Vehement2/images/Stone/StoneRaw.png";
        constexpr const char* Black = "Vehement2/images/Stone/StoneBlack.png";
        constexpr const char* Marble1 = "Vehement2/images/Stone/StoneMarble1.png";
        constexpr const char* Marble2 = "Vehement2/images/Stone/StoneMarble2.png";
    }

    // =========================================================================
    // Metal textures - For Bunker and Industrial cultures
    // =========================================================================
    namespace Metal {
        constexpr const char* Metal1 = "Vehement2/images/Metal/Metal1.png";
        constexpr const char* Metal2 = "Vehement2/images/Metal/Metal2.png";
        constexpr const char* Metal3 = "Vehement2/images/Metal/Metal3.png";
        constexpr const char* Metal4 = "Vehement2/images/Metal/Metal4.png";
        constexpr const char* Tile1 = "Vehement2/images/Metal/MetalTile1.png";
        constexpr const char* Tile2 = "Vehement2/images/Metal/MetalTile2.png";
        constexpr const char* Tile3 = "Vehement2/images/Metal/MetalTile3.png";
        constexpr const char* Tile4 = "Vehement2/images/Metal/MetalTile4.png";
        constexpr const char* ShopFront = "Vehement2/images/Metal/ShopFront.png";
        constexpr const char* ShopFrontB = "Vehement2/images/Metal/ShopFrontB.png";
        constexpr const char* ShopFrontR = "Vehement2/images/Metal/ShopFrontR.png";
        constexpr const char* ShopFrontL = "Vehement2/images/Metal/ShopFrontL.png";
        constexpr const char* ShopFrontT = "Vehement2/images/Metal/ShopFrontT.png";
    }

    // =========================================================================
    // Wood textures - For Nomad and Forest cultures
    // =========================================================================
    namespace Wood {
        constexpr const char* Wood1 = "Vehement2/images/Wood/Wood1.png";
        constexpr const char* Wood2 = "Vehement2/images/Wood/Wood2.png";
        constexpr const char* WoodOld = "Vehement2/images/Wood/WoodOld.png";
        constexpr const char* WoodFence = "Vehement2/images/Wood/WoodFence.png";
        constexpr const char* WoodPlank = "Vehement2/images/Wood/WoodPlank.png";
    }

    // =========================================================================
    // Textile textures - For Nomad and Merchant cultures
    // =========================================================================
    namespace Textiles {
        constexpr const char* Textile1 = "Vehement2/images/Textiles/Textile1.png";
        constexpr const char* Textile2 = "Vehement2/images/Textiles/Textile2.png";
        constexpr const char* Cloth = "Vehement2/images/Textiles/Cloth.png";
        constexpr const char* Canvas = "Vehement2/images/Textiles/Canvas.png";
    }

    // =========================================================================
    // People textures - For units across all cultures
    // =========================================================================
    namespace People {
        constexpr const char* Person1 = "Vehement2/images/People/Person1.png";
        constexpr const char* Person2 = "Vehement2/images/People/Person2.png";
        constexpr const char* Person3 = "Vehement2/images/People/Person3.png";
        constexpr const char* Person4 = "Vehement2/images/People/Person4.png";
        constexpr const char* Person5 = "Vehement2/images/People/Person5.png";
        constexpr const char* Person6 = "Vehement2/images/People/Person6.png";
        constexpr const char* Person7 = "Vehement2/images/People/Person7.png";
        constexpr const char* Person8 = "Vehement2/images/People/Person8.png";
        constexpr const char* Person9 = "Vehement2/images/People/Person9.png";
        constexpr const char* Shadow = "Vehement2/images/People/PersonShaddow.png";
        constexpr const char* Zombie = "Vehement2/images/People/ZombieA.png";
    }

} // namespace AssetPaths

/**
 * @brief Culture-specific asset collection
 *
 * Groups all visual assets used by a specific culture for easy loading.
 */
struct CultureAssetCollection {
    CultureType culture;

    // Building textures by type
    std::unordered_map<BuildingType, std::string> buildingTextures;

    // Terrain/ground textures for this culture's territory
    std::vector<std::string> groundTextures;

    // Decoration/prop textures
    std::vector<std::string> decorationTextures;

    // Wall segment textures (different angles/connections)
    struct WallTextures {
        std::string horizontal;
        std::string vertical;
        std::string cornerTL;
        std::string cornerTR;
        std::string cornerBL;
        std::string cornerBR;
        std::string gateHorizontal;
        std::string gateVertical;
        std::string tower;
    } walls;

    // Unit textures
    struct UnitTextures {
        std::string worker;
        std::string guard;
        std::string elite;
        std::string scout;
        std::string special;
    } units;

    // UI elements
    struct UITextures {
        std::string banner;
        std::string icon;
        std::string preview;
        std::string background;
    } ui;

    // Color scheme
    struct ColorScheme {
        uint32_t primary;       // Main faction color (RGBA)
        uint32_t secondary;     // Accent color
        uint32_t tertiary;      // Highlight color
        uint32_t text;          // Text color
        uint32_t shadow;        // Shadow/outline color
    } colors;
};

/**
 * @brief Manager for culture-specific visual assets
 *
 * Provides centralized access to all culture visual resources,
 * handles texture loading/caching, and applies visual modifiers.
 */
class CultureAssetManager {
public:
    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static CultureAssetManager& Instance();

    // Non-copyable
    CultureAssetManager(const CultureAssetManager&) = delete;
    CultureAssetManager& operator=(const CultureAssetManager&) = delete;

    /**
     * @brief Initialize asset manager
     * @param textureManager Reference to Nova texture manager
     * @return true if initialization succeeded
     */
    bool Initialize(Nova::TextureManager& textureManager);

    /**
     * @brief Shutdown and release resources
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Get complete asset collection for a culture
     */
    [[nodiscard]] const CultureAssetCollection* GetAssetCollection(CultureType culture) const;

    /**
     * @brief Get building texture path for culture
     */
    [[nodiscard]] std::string GetBuildingTexturePath(CultureType culture, BuildingType building) const;

    /**
     * @brief Get loaded building texture for culture
     */
    [[nodiscard]] std::shared_ptr<Nova::Texture> GetBuildingTexture(CultureType culture, BuildingType building) const;

    /**
     * @brief Get unit texture path for culture
     */
    [[nodiscard]] std::string GetUnitTexturePath(CultureType culture, const std::string& unitType) const;

    /**
     * @brief Get wall texture set for culture
     */
    [[nodiscard]] const CultureAssetCollection::WallTextures* GetWallTextures(CultureType culture) const;

    /**
     * @brief Preload all textures for a culture
     * @param culture Culture to preload
     */
    void PreloadCultureTextures(CultureType culture);

    /**
     * @brief Unload textures for a culture (to free memory)
     */
    void UnloadCultureTextures(CultureType culture);

    /**
     * @brief Get color scheme for culture
     */
    [[nodiscard]] CultureAssetCollection::ColorScheme GetColorScheme(CultureType culture) const;

    /**
     * @brief Apply culture tint to a base color
     * @param culture Culture to get tint from
     * @param baseColor Base color to tint
     * @param intensity Tint intensity (0-1)
     * @return Tinted color
     */
    [[nodiscard]] uint32_t ApplyCultureTint(CultureType culture, uint32_t baseColor, float intensity = 0.3f) const;

private:
    CultureAssetManager() = default;
    ~CultureAssetManager() = default;

    void InitializeFortressAssets();
    void InitializeBunkerAssets();
    void InitializeNomadAssets();
    void InitializeScavengerAssets();
    void InitializeMerchantAssets();
    void InitializeIndustrialAssets();
    void InitializeUndergroundAssets();
    void InitializeForestAssets();

    bool m_initialized = false;
    Nova::TextureManager* m_textureManager = nullptr;

    std::unordered_map<CultureType, CultureAssetCollection> m_assetCollections;
    std::unordered_map<std::string, std::shared_ptr<Nova::Texture>> m_textureCache;
};

/**
 * @brief Static asset mapping tables
 *
 * These tables define the default texture mappings for each culture.
 * Used for initialization and fallback when specific textures aren't found.
 */
namespace DefaultAssets {

    /**
     * @brief Fortress Culture Assets
     * Theme: Medieval European castle architecture
     * Primary materials: Stone, brick, iron
     */
    inline const std::unordered_map<BuildingType, std::string> FortressBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Stone::Marble1 },
        { BuildingType::Barracks, AssetPaths::Bricks::Grey },
        { BuildingType::Workshop, AssetPaths::Stone::Raw },
        { BuildingType::Storage, AssetPaths::Bricks::Black },
        { BuildingType::Wall, AssetPaths::Bricks::Rock },
        { BuildingType::WallGate, AssetPaths::Bricks::Stacked },
        { BuildingType::Tower, AssetPaths::Stone::Marble2 },
        { BuildingType::Bunker, AssetPaths::Stone::Black },
        { BuildingType::Farm, AssetPaths::Wood::Wood1 },
        { BuildingType::Mine, AssetPaths::Stone::Raw },
        { BuildingType::Warehouse, AssetPaths::Bricks::Rock },
        { BuildingType::Market, AssetPaths::Bricks::Grey },
        { BuildingType::Hospital, AssetPaths::Stone::Marble1 },
        { BuildingType::ResearchLab, AssetPaths::Stone::Marble2 },
        { BuildingType::Castle, AssetPaths::Stone::Marble1 },
    };

    /**
     * @brief Bunker Culture Assets
     * Theme: Modern military installations
     * Primary materials: Concrete, metal, steel
     */
    inline const std::unordered_map<BuildingType, std::string> BunkerBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Metal::Metal1 },
        { BuildingType::Barracks, AssetPaths::Metal::Metal2 },
        { BuildingType::Workshop, AssetPaths::Metal::Metal3 },
        { BuildingType::Storage, AssetPaths::Metal::Tile1 },
        { BuildingType::Wall, AssetPaths::Metal::Metal4 },
        { BuildingType::WallGate, AssetPaths::Metal::ShopFront },
        { BuildingType::Tower, AssetPaths::Metal::Tile2 },
        { BuildingType::Bunker, AssetPaths::Metal::Metal1 },
        { BuildingType::Turret, AssetPaths::Metal::Tile3 },
        { BuildingType::Farm, AssetPaths::Metal::ShopFrontB },
        { BuildingType::Mine, AssetPaths::Metal::Metal4 },
        { BuildingType::Warehouse, AssetPaths::Metal::Tile4 },
        { BuildingType::Hospital, AssetPaths::Metal::Metal2 },
        { BuildingType::ResearchLab, AssetPaths::Metal::Metal3 },
        { BuildingType::PowerPlant, AssetPaths::Metal::Tile1 },
    };

    /**
     * @brief Nomad Culture Assets
     * Theme: Central Asian yurt camps
     * Primary materials: Cloth, leather, wood
     */
    inline const std::unordered_map<BuildingType, std::string> NomadBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Textiles::Textile1 },
        { BuildingType::Barracks, AssetPaths::Textiles::Textile2 },
        { BuildingType::Workshop, AssetPaths::Wood::Wood1 },
        { BuildingType::Storage, AssetPaths::Wood::Wood2 },
        { BuildingType::Wall, AssetPaths::Wood::WoodFence },
        { BuildingType::WallGate, AssetPaths::Wood::Wood1 },
        { BuildingType::Tower, AssetPaths::Wood::Wood2 },
        { BuildingType::Farm, AssetPaths::Wood::Wood1 },
        { BuildingType::Mine, AssetPaths::Wood::WoodOld },
        { BuildingType::Warehouse, AssetPaths::Textiles::Textile1 },
        { BuildingType::Market, AssetPaths::Textiles::Textile2 },
        { BuildingType::Hospital, AssetPaths::Textiles::Textile1 },
        { BuildingType::Yurt, AssetPaths::Textiles::Textile2 },
        { BuildingType::MobileWorkshop, AssetPaths::Wood::Wood2 },
    };

    /**
     * @brief Scavenger Culture Assets
     * Theme: Post-apocalyptic improvisation
     * Primary materials: Scrap metal, salvaged materials
     */
    inline const std::unordered_map<BuildingType, std::string> ScavengerBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Metal::ShopFront },
        { BuildingType::Barracks, AssetPaths::Metal::ShopFrontB },
        { BuildingType::Workshop, AssetPaths::Metal::Tile1 },
        { BuildingType::Storage, AssetPaths::Metal::ShopFrontR },
        { BuildingType::Wall, AssetPaths::Wood::WoodOld },
        { BuildingType::WallGate, AssetPaths::Metal::ShopFrontL },
        { BuildingType::Tower, AssetPaths::Metal::Tile2 },
        { BuildingType::Farm, AssetPaths::Wood::Wood2 },
        { BuildingType::Mine, AssetPaths::Metal::Tile3 },
        { BuildingType::Warehouse, AssetPaths::Metal::ShopFrontT },
        { BuildingType::Market, AssetPaths::Metal::ShopFrontL },
        { BuildingType::Hospital, AssetPaths::Metal::Tile4 },
    };

    /**
     * @brief Merchant Culture Assets
     * Theme: Silk Road trading posts
     * Primary materials: Colorful textiles, quality wood
     */
    inline const std::unordered_map<BuildingType, std::string> MerchantBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Textiles::Textile1 },
        { BuildingType::Barracks, AssetPaths::Bricks::Stacked },
        { BuildingType::Workshop, AssetPaths::Wood::Wood1 },
        { BuildingType::Storage, AssetPaths::Wood::Wood2 },
        { BuildingType::Wall, AssetPaths::Bricks::Grey },
        { BuildingType::WallGate, AssetPaths::Bricks::Rock },
        { BuildingType::Tower, AssetPaths::Bricks::Stacked },
        { BuildingType::Farm, AssetPaths::Wood::Wood1 },
        { BuildingType::Mine, AssetPaths::Stone::Raw },
        { BuildingType::Warehouse, AssetPaths::Wood::Wood2 },
        { BuildingType::Market, AssetPaths::Textiles::Textile2 },
        { BuildingType::Hospital, AssetPaths::Textiles::Textile1 },
        { BuildingType::Bazaar, AssetPaths::Textiles::Textile2 },
    };

    /**
     * @brief Industrial Culture Assets
     * Theme: Victorian-era factories
     * Primary materials: Metal, concrete, machinery
     */
    inline const std::unordered_map<BuildingType, std::string> IndustrialBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Metal::Metal1 },
        { BuildingType::Barracks, AssetPaths::Metal::Metal2 },
        { BuildingType::Workshop, AssetPaths::Metal::Metal3 },
        { BuildingType::Storage, AssetPaths::Metal::Tile1 },
        { BuildingType::Wall, AssetPaths::Metal::Metal4 },
        { BuildingType::WallGate, AssetPaths::Metal::ShopFront },
        { BuildingType::Tower, AssetPaths::Metal::Tile2 },
        { BuildingType::Farm, AssetPaths::Metal::ShopFrontB },
        { BuildingType::Mine, AssetPaths::Metal::Metal4 },
        { BuildingType::Warehouse, AssetPaths::Metal::Tile3 },
        { BuildingType::Market, AssetPaths::Metal::ShopFrontL },
        { BuildingType::Hospital, AssetPaths::Metal::Metal2 },
        { BuildingType::ResearchLab, AssetPaths::Metal::Tile4 },
        { BuildingType::PowerPlant, AssetPaths::Metal::Metal3 },
        { BuildingType::Factory, AssetPaths::Metal::Metal1 },
    };

    /**
     * @brief Underground Culture Assets
     * Theme: Dwarven mines and bunkers
     * Primary materials: Dark stone, reinforced tunnels
     */
    inline const std::unordered_map<BuildingType, std::string> UndergroundBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Stone::Black },
        { BuildingType::Barracks, AssetPaths::Stone::Marble2 },
        { BuildingType::Workshop, AssetPaths::Stone::Raw },
        { BuildingType::Storage, AssetPaths::Stone::Black },
        { BuildingType::Wall, AssetPaths::Stone::Raw },
        { BuildingType::WallGate, AssetPaths::Stone::Marble1 },
        { BuildingType::Tower, AssetPaths::Stone::Marble2 },
        { BuildingType::Bunker, AssetPaths::Stone::Black },
        { BuildingType::Farm, AssetPaths::Stone::Raw },
        { BuildingType::Mine, AssetPaths::Stone::Black },
        { BuildingType::Warehouse, AssetPaths::Stone::Raw },
        { BuildingType::Hospital, AssetPaths::Stone::Marble1 },
        { BuildingType::ResearchLab, AssetPaths::Stone::Marble2 },
        { BuildingType::HiddenEntrance, AssetPaths::Stone::Raw },
    };

    /**
     * @brief Forest Culture Assets
     * Theme: Elven woodland settlements
     * Primary materials: Wood, foliage, natural materials
     */
    inline const std::unordered_map<BuildingType, std::string> ForestBuildingTextures = {
        { BuildingType::Headquarters, AssetPaths::Wood::Wood1 },
        { BuildingType::Barracks, AssetPaths::Wood::Wood2 },
        { BuildingType::Workshop, AssetPaths::Wood::Wood1 },
        { BuildingType::Storage, AssetPaths::Wood::Wood2 },
        { BuildingType::Wall, AssetPaths::Wood::WoodFence },
        { BuildingType::WallGate, AssetPaths::Wood::Wood1 },
        { BuildingType::Tower, AssetPaths::Wood::Wood2 },
        { BuildingType::Farm, AssetPaths::Wood::Wood1 },
        { BuildingType::Mine, AssetPaths::Wood::WoodOld },
        { BuildingType::Warehouse, AssetPaths::Wood::Wood2 },
        { BuildingType::Market, AssetPaths::Wood::Wood1 },
        { BuildingType::Hospital, AssetPaths::Wood::Wood2 },
    };

    /**
     * @brief Color schemes for each culture
     */
    inline const std::unordered_map<CultureType, CultureAssetCollection::ColorScheme> CultureColors = {
        { CultureType::Fortress, { 0x4A4A4AFF, 0x8B0000FF, 0xC0C0C0FF, 0xFFFFFFFF, 0x000000FF } },
        { CultureType::Bunker, { 0x3D3D3DFF, 0x006400FF, 0x808080FF, 0xFFFFFFFF, 0x000000FF } },
        { CultureType::Nomad, { 0xDEB887FF, 0x8B4513FF, 0xF5DEB3FF, 0x000000FF, 0x4A3000FF } },
        { CultureType::Scavenger, { 0x8B8B7AFF, 0xCD853FFF, 0xA0A090FF, 0xFFFFFFFF, 0x2A2A20FF } },
        { CultureType::Merchant, { 0xFFD700FF, 0x800080FF, 0xFFF8DCFF, 0x000000FF, 0x4A3000FF } },
        { CultureType::Industrial, { 0x4682B4FF, 0xFF4500FF, 0x708090FF, 0xFFFFFFFF, 0x000000FF } },
        { CultureType::Underground, { 0x2F4F4FFF, 0x696969FF, 0x404040FF, 0xC0C0C0FF, 0x000000FF } },
        { CultureType::Forest, { 0x228B22FF, 0x8B4513FF, 0x90EE90FF, 0xFFFFFFFF, 0x003000FF } },
    };

} // namespace DefaultAssets

} // namespace RTS
} // namespace Vehement
