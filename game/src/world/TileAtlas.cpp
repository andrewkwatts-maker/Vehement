#include "TileAtlas.hpp"
#include "../../engine/graphics/Texture.hpp"
#include "../../engine/graphics/TextureManager.hpp"
#include <algorithm>

namespace Vehement {

TileAtlas::TileAtlas() {
    // Initialize default region (full texture, white)
    m_defaultRegion.uvMin = glm::vec2(0.0f, 0.0f);
    m_defaultRegion.uvMax = glm::vec2(1.0f, 1.0f);
    m_defaultRegion.atlasIndex = 0;
    m_defaultRegion.valid = false;
}

TileAtlas::~TileAtlas() = default;

bool TileAtlas::Initialize(Nova::TextureManager& textureManager,
                           const TileAtlasConfig& config) {
    m_textureManager = &textureManager;
    m_config = config;

    // Register all texture paths based on Vehement2/images/ structure
    const std::string& base = m_config.textureBasePath;

    // Ground textures
    RegisterTexturePath(TileType::GroundGrass1, base + "Ground/GroundGrass1.png");
    RegisterTexturePath(TileType::GroundGrass2, base + "Ground/GroundGrass2.png");
    RegisterTexturePath(TileType::GroundDirt, base + "Ground/GroundDirt.png");
    RegisterTexturePath(TileType::GroundForest1, base + "Ground/GroundForrest1.png");
    RegisterTexturePath(TileType::GroundForest2, base + "Ground/GroundForrest2.png");
    RegisterTexturePath(TileType::GroundRocks, base + "Ground/GroundRocks.png");

    // Concrete textures
    RegisterTexturePath(TileType::ConcreteAsphalt1, base + "Concrete/ConcreteAshpelt1.png");
    RegisterTexturePath(TileType::ConcreteAsphalt2, base + "Concrete/ConcreteAshpelt2.png");
    RegisterTexturePath(TileType::ConcreteAsphaltSteps1, base + "Concrete/ConcreteAshpelt2Steps1.png");
    RegisterTexturePath(TileType::ConcreteAsphaltSteps2, base + "Concrete/ConcreteAshpelt2Steps2.png");
    RegisterTexturePath(TileType::ConcreteBlocks1, base + "Concrete/ConcreteBlocks1.png");
    RegisterTexturePath(TileType::ConcreteBlocks2, base + "Concrete/ConcreteBlocks2.png");
    RegisterTexturePath(TileType::ConcretePad, base + "Concrete/ConcretePad.png");
    RegisterTexturePath(TileType::ConcreteTiles1, base + "Concrete/ConcreteTiles1.png");
    RegisterTexturePath(TileType::ConcreteTiles2, base + "Concrete/ConcreteTiles2.png");

    // Brick textures - Main
    RegisterTexturePath(TileType::BricksBlack, base + "Bricks/BricksBlack.png");
    RegisterTexturePath(TileType::BricksGrey, base + "Bricks/BricksGrey.png");
    RegisterTexturePath(TileType::BricksRock, base + "Bricks/BricksRock.png");
    RegisterTexturePath(TileType::BricksStacked, base + "Bricks/BricksStacked.png");

    // Brick wall front textures (for wall sides)
    RegisterTexturePath(TileType::BricksRockFrontTop, base + "Bricks/BricksRockFrontTOP.png");
    RegisterTexturePath(TileType::BricksRockFrontBottom, base + "Bricks/BricksRockFrontBOT.png");
    RegisterTexturePath(TileType::BricksRockFrontLeft, base + "Bricks/BricksRockFrontLHS.png");
    RegisterTexturePath(TileType::BricksRockFrontRight, base + "Bricks/BricksRockFrontRHS.png");

    // Brick corners - Outer (RO = Rock Outer)
    RegisterTexturePath(TileType::BricksCornerTopLeftOuter, base + "Bricks/Courners/BricksRockAspheltTLRO.png");
    RegisterTexturePath(TileType::BricksCornerTopRightOuter, base + "Bricks/Courners/BricksRockAspheltTRRO.png");
    RegisterTexturePath(TileType::BricksCornerBottomLeftOuter, base + "Bricks/Courners/BricksRockAspheltBLRO.png");
    RegisterTexturePath(TileType::BricksCornerBottomRightOuter, base + "Bricks/Courners/BricksRockAspheltBRRO.png");

    // Brick corners - Inner (RI = Rock Inner)
    RegisterTexturePath(TileType::BricksCornerTopLeftInner, base + "Bricks/Courners/BricksRockAspheltTLRI.png");
    RegisterTexturePath(TileType::BricksCornerTopRightInner, base + "Bricks/Courners/BricksRockAspheltTRRI.png");
    RegisterTexturePath(TileType::BricksCornerBottomLeftInner, base + "Bricks/Courners/BricksRockAspheltBLRI.png");
    RegisterTexturePath(TileType::BricksCornerBottomRightInner, base + "Bricks/Courners/BricksRockAspheltBRRI.png");

    // Brick corners - Regular
    RegisterTexturePath(TileType::BricksCornerTopLeft, base + "Bricks/Courners/BricksRockAspheltTL.png");
    RegisterTexturePath(TileType::BricksCornerTopRight, base + "Bricks/Courners/BricksRockAspheltTR.png");
    RegisterTexturePath(TileType::BricksCornerBottomLeft, base + "Bricks/Courners/BricksRockAspheltBL.png");
    RegisterTexturePath(TileType::BricksCornerBottomRight, base + "Bricks/Courners/BricksRockAspheltBR.png");

    // Wood textures
    RegisterTexturePath(TileType::Wood1, base + "Wood/Wood1.png");
    RegisterTexturePath(TileType::WoodCrate1, base + "Wood/WoodCrate1.png");
    RegisterTexturePath(TileType::WoodCrate2, base + "Wood/WoodCrate2.png");
    RegisterTexturePath(TileType::WoodFlooring1, base + "Wood/WoodFlooring1.png");
    RegisterTexturePath(TileType::WoodFlooring2, base + "Wood/WoodFlooring2.png");

    // Water textures
    RegisterTexturePath(TileType::Water1, base + "Water/Water1.png");

    // Metal textures
    RegisterTexturePath(TileType::Metal1, base + "Metal/Metal1.png");
    RegisterTexturePath(TileType::Metal2, base + "Metal/Metal2.png");
    RegisterTexturePath(TileType::Metal3, base + "Metal/Metal3.png");
    RegisterTexturePath(TileType::Metal4, base + "Metal/Metal4.png");
    RegisterTexturePath(TileType::MetalTile1, base + "Metal/MetalTile1.png");
    RegisterTexturePath(TileType::MetalTile2, base + "Metal/MetalTile2.png");
    RegisterTexturePath(TileType::MetalTile3, base + "Metal/MetalTile3.png");
    RegisterTexturePath(TileType::MetalTile4, base + "Metal/MetalTile4.png");
    RegisterTexturePath(TileType::MetalShopFront, base + "Metal/ShopFront.png");
    RegisterTexturePath(TileType::MetalShopFrontBottom, base + "Metal/ShopFrontB.png");
    RegisterTexturePath(TileType::MetalShopFrontLeft, base + "Metal/ShopFrontL.png");
    RegisterTexturePath(TileType::MetalShopFrontRight, base + "Metal/ShopFrontR.png");
    RegisterTexturePath(TileType::MetalShopFrontTop, base + "Metal/ShopFrontT.png");

    // Stone textures
    RegisterTexturePath(TileType::StoneBlack, base + "Stone/StoneBlack.png");
    RegisterTexturePath(TileType::StoneMarble1, base + "Stone/StoneMarble1.png");
    RegisterTexturePath(TileType::StoneMarble2, base + "Stone/StoneMarble2.png");
    RegisterTexturePath(TileType::StoneRaw, base + "Stone/StoneRaw.png");

    m_initialized = true;
    return true;
}

bool TileAtlas::LoadTextures() {
    if (!m_initialized || !m_textureManager) {
        return false;
    }

    bool allLoaded = true;

    for (const auto& [type, path] : m_texturePaths) {
        if (!LoadTextureForType(type)) {
            allLoaded = false;
        }
    }

    return allLoaded;
}

bool TileAtlas::LoadTextureForType(TileType type) {
    auto it = m_texturePaths.find(type);
    if (it == m_texturePaths.end()) {
        return false;
    }

    auto texture = m_textureManager->Load(it->second, m_config.sRGB);
    if (texture && texture->IsValid()) {
        m_textures[type] = texture;

        // Set up texture region for non-atlas mode (full texture)
        TextureRegion region;
        region.uvMin = glm::vec2(0.0f, 0.0f);
        region.uvMax = glm::vec2(1.0f, 1.0f);
        region.atlasIndex = 0;
        region.valid = true;
        m_textureRegions[type] = region;

        return true;
    }

    return false;
}

bool TileAtlas::BuildAtlas() {
    // For now, we don't actually pack textures into an atlas
    // Individual textures work well for this use case
    // A proper atlas implementation would use bin packing algorithm

    // Mark atlas mode as available if we have textures
    if (!m_textures.empty()) {
        // In a full implementation, we would:
        // 1. Sort textures by size
        // 2. Use bin packing to arrange them in atlas texture(s)
        // 3. Create atlas texture(s) with proper padding
        // 4. Update m_textureRegions with correct UV coordinates

        // For now, just use individual textures
        m_useAtlas = false;
        return true;
    }

    return false;
}

const TextureRegion& TileAtlas::GetTextureRegion(TileType type) const {
    auto it = m_textureRegions.find(type);
    if (it != m_textureRegions.end()) {
        return it->second;
    }
    return m_defaultRegion;
}

std::string TileAtlas::GetTexturePath(TileType type) const {
    auto it = m_texturePaths.find(type);
    if (it != m_texturePaths.end()) {
        return it->second;
    }
    return "";
}

std::shared_ptr<Nova::Texture> TileAtlas::GetTexture(TileType type) const {
    auto it = m_textures.find(type);
    if (it != m_textures.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Nova::Texture> TileAtlas::GetAtlasTexture(int index) const {
    if (index >= 0 && index < static_cast<int>(m_atlasTextures.size())) {
        return m_atlasTextures[index];
    }
    return nullptr;
}

void TileAtlas::BindTexture(TileType type, uint32_t slot) const {
    auto texture = GetTexture(type);
    if (texture) {
        texture->Bind(slot);
    } else if (m_textureManager) {
        // Bind default white texture as fallback
        auto white = m_textureManager->GetWhite();
        if (white) {
            white->Bind(slot);
        }
    }
}

void TileAtlas::BindAtlas(int atlasIndex, uint32_t slot) const {
    auto atlas = GetAtlasTexture(atlasIndex);
    if (atlas) {
        atlas->Bind(slot);
    }
}

std::vector<TileType> TileAtlas::GetLoadedTypes() const {
    std::vector<TileType> types;
    types.reserve(m_textures.size());
    for (const auto& [type, texture] : m_textures) {
        types.push_back(type);
    }
    return types;
}

bool TileAtlas::HasTexture(TileType type) const {
    return m_textures.find(type) != m_textures.end();
}

void TileAtlas::RegisterTexturePath(TileType type, const std::string& relativePath) {
    m_texturePaths[type] = relativePath;
}

TileType TileAtlas::GetWallSideTexture(TileType wallType, WallOrientation face) {
    return GetWallSideTextureForFace(wallType, face);
}

} // namespace Vehement
