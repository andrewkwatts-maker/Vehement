#include "LevelEditor.hpp"
#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_set>
#include <cmath>

namespace Vehement {

// ============================================================================
// Forward declaration placeholders for TileMap and EntityManager
// These would be defined in separate files
// ============================================================================

// Placeholder TileMap interface - would be implemented elsewhere
class TileMap {
public:
    TileType GetTile(int x, int y) const { return TileType::Empty; }
    uint8_t GetVariant(int x, int y) const { return 0; }
    bool IsWall(int x, int y) const { return false; }
    float GetWallHeight(int x, int y) const { return 0.0f; }
    void SetTile(int x, int y, TileType type, uint8_t variant = 0) {}
    void SetWall(int x, int y, bool isWall, float height = 0.0f) {}
    int GetWidth() const { return 100; }
    int GetHeight() const { return 100; }
    bool InBounds(int x, int y) const { return x >= 0 && y >= 0 && x < GetWidth() && y < GetHeight(); }
};

// Placeholder EntityManager interface - would be implemented elsewhere
class EntityManager {
public:
    struct Entity {
        glm::vec2 position;
        bool isZombie;
    };

    std::vector<Entity> GetEntitiesInRadius(const glm::vec2& pos, float radius) const {
        return {};
    }

    bool HasZombiesInRadius(const glm::vec2& pos, float radius) const {
        auto entities = GetEntitiesInRadius(pos, radius);
        for (const auto& e : entities) {
            if (e.isZombie) return true;
        }
        return false;
    }
};

// ============================================================================
// Tile Type Utilities
// ============================================================================

const char* GetTileTexturePath(TileType type) {
    switch (type) {
        // Ground
        case TileType::GroundDirt: return "Ground/GroundDirt.png";
        case TileType::GroundForrest1: return "Ground/GroundForrest1.png";
        case TileType::GroundForrest2: return "Ground/GroundForrest2.png";
        case TileType::GroundGrass1: return "Ground/GroundGrass1.png";
        case TileType::GroundGrass2: return "Ground/GroundGrass2.png";
        case TileType::GroundRocks: return "Ground/GroundRocks.png";

        // Concrete
        case TileType::ConcreteAsphalt1: return "Concrete/ConcreteAshpelt1.png";
        case TileType::ConcreteAsphalt2: return "Concrete/ConcreteAshpelt2.png";
        case TileType::ConcreteAsphalt2Steps1: return "Concrete/ConcreteAshpelt2Steps1.png";
        case TileType::ConcreteAsphalt2Steps2: return "Concrete/ConcreteAshpelt2Steps2.png";
        case TileType::ConcreteBlocks1: return "Concrete/ConcreteBlocks1.png";
        case TileType::ConcreteBlocks2: return "Concrete/ConcreteBlocks2.png";
        case TileType::ConcretePad: return "Concrete/ConcretePad.png";
        case TileType::ConcreteTiles1: return "Concrete/ConcreteTiles1.png";
        case TileType::ConcreteTiles2: return "Concrete/ConcreteTiles2.png";

        // Bricks
        case TileType::BricksBlack: return "Bricks/BricksBlack.png";
        case TileType::BricksGrey: return "Bricks/BricksGrey.png";
        case TileType::BricksRock: return "Bricks/BricksRock.png";
        case TileType::BricksRockFrontBot: return "Bricks/BricksRockFrontBOT.png";
        case TileType::BricksRockFrontLhs: return "Bricks/BricksRockFrontLHS.png";
        case TileType::BricksRockFrontRhs: return "Bricks/BricksRockFrontRHS.png";
        case TileType::BricksRockFrontTop: return "Bricks/BricksRockFrontTOP.png";
        case TileType::BricksStacked: return "Bricks/BricksStacked.png";
        case TileType::BricksCornerBL: return "Bricks/Courners/BricksRockAspheltBL.png";
        case TileType::BricksCornerBLRI: return "Bricks/Courners/BricksRockAspheltBLRI.png";
        case TileType::BricksCornerBLRO: return "Bricks/Courners/BricksRockAspheltBLRO.png";
        case TileType::BricksCornerBR: return "Bricks/Courners/BricksRockAspheltBR.png";
        case TileType::BricksCornerBRRI: return "Bricks/Courners/BricksRockAspheltBRRI.png";
        case TileType::BricksCornerBRRO: return "Bricks/Courners/BricksRockAspheltBRRO.png";
        case TileType::BricksCornerTL: return "Bricks/Courners/BricksRockAspheltTL.png";
        case TileType::BricksCornerTLRI: return "Bricks/Courners/BricksRockAspheltTLRI.png";
        case TileType::BricksCornerTLRO: return "Bricks/Courners/BricksRockAspheltTLRO.png";
        case TileType::BricksCornerTR: return "Bricks/Courners/BricksRockAspheltTR.png";
        case TileType::BricksCornerTRRI: return "Bricks/Courners/BricksRockAspheltTRRI.png";
        case TileType::BricksCornerTRRO: return "Bricks/Courners/BricksRockAspheltTRRO.png";

        // Wood
        case TileType::Wood1: return "Wood/Wood1.png";
        case TileType::WoodCrate1: return "Wood/WoodCrate1.png";
        case TileType::WoodCrate2: return "Wood/WoodCrate2.png";
        case TileType::WoodFlooring1: return "Wood/WoodFlooring1.png";
        case TileType::WoodFlooring2: return "Wood/WoodFlooring2.png";

        // Stone
        case TileType::StoneBlack: return "Stone/StoneBlack.png";
        case TileType::StoneMarble1: return "Stone/StoneMarble1.png";
        case TileType::StoneMarble2: return "Stone/StoneMarble2.png";
        case TileType::StoneRaw: return "Stone/StoneRaw.png";

        // Metal
        case TileType::Metal1: return "Metal/Metal1.png";
        case TileType::Metal2: return "Metal/Metal2.png";
        case TileType::Metal3: return "Metal/Metal3.png";
        case TileType::Metal4: return "Metal/Metal4.png";
        case TileType::MetalTile1: return "Metal/MetalTile1.png";
        case TileType::MetalTile2: return "Metal/MetalTile2.png";
        case TileType::MetalTile3: return "Metal/MetalTile3.png";
        case TileType::MetalTile4: return "Metal/MetalTile4.png";
        case TileType::MetalShopFront: return "Metal/ShopFront.png";
        case TileType::MetalShopFrontB: return "Metal/ShopFrontB.png";
        case TileType::MetalShopFrontL: return "Metal/ShopFrontL.png";
        case TileType::MetalShopFrontR: return "Metal/ShopFrontR.png";
        case TileType::MetalShopFrontT: return "Metal/ShopFrontT.png";

        // Foliage
        case TileType::FoliageBonsai: return "Follage/Bonsai.png";
        case TileType::FoliageBottleBrush: return "Follage/BottleBrush.png";
        case TileType::FoliageCherryTree: return "Follage/CherryTree.png";
        case TileType::FoliagePalm1: return "Follage/Palm1.png";
        case TileType::FoliagePlanterBox: return "Follage/PlanterBox.png";
        case TileType::FoliagePlanterBox2: return "Follage/PlanterBox2.png";
        case TileType::FoliagePlanterBox3: return "Follage/PlanterBox3.png";
        case TileType::FoliagePlanterBox4: return "Follage/PlanterBox4.png";
        case TileType::FoliagePotPlant: return "Follage/PotPlant.png";
        case TileType::FoliageSilverOak: return "Follage/SilverOak.png";
        case TileType::FoliageSilverOakBrown: return "Follage/SilverOakBrown.png";
        case TileType::FoliageTree1: return "Follage/Tree1.png";
        case TileType::FoliageTree2: return "Follage/Tree2.png";
        case TileType::FoliageTree3: return "Follage/Tree3.png";
        case TileType::FoliageYellowTree1: return "Follage/YellowTree1.png";
        case TileType::FoliageShrub1: return "Follage/shrub1.png";

        // Water
        case TileType::Water1: return "Water/Water1.png";

        // Objects
        case TileType::ObjectBarStool: return "Objects/BarStool.png";
        case TileType::ObjectClothesStand: return "Objects/ClothesStand.png";
        case TileType::ObjectClothesStand2: return "Objects/ClothesStand2.png";
        case TileType::ObjectDeskFan: return "Objects/DeskFan.png";
        case TileType::ObjectDeskTop: return "Objects/DeskTop.png";
        case TileType::ObjectDeskTop0: return "Objects/DeskTop0.png";
        case TileType::ObjectDeskTop1: return "Objects/DeskTop1.png";
        case TileType::ObjectDeskTop2: return "Objects/DeskTop2.png";
        case TileType::ObjectDeskTop3: return "Objects/DeskTop3.png";
        case TileType::ObjectDeskTop4: return "Objects/DeskTop4.png";
        case TileType::ObjectGarbage1: return "Objects/Garbage1.png";
        case TileType::ObjectGarbage2: return "Objects/Garbage2.png";
        case TileType::ObjectGarbage3: return "Objects/Garbage3.png";
        case TileType::ObjectGenerator: return "Objects/Generator.png";
        case TileType::ObjectGenerator2: return "Objects/Generator2.png";
        case TileType::ObjectGenerator3: return "Objects/Generator3.png";
        case TileType::ObjectPiping1: return "Objects/Piping1.png";
        case TileType::ObjectPiping3: return "Objects/Piping3.png";
        case TileType::ObjectPiping4: return "Objects/Piping4.png";
        case TileType::ObjectShopFront: return "Objects/ShopFront.png";
        case TileType::ObjectShopSolo: return "Objects/ShopSolo.png";

        // Textiles
        case TileType::TextileBasket: return "Textiles/TextileBasket.png";
        case TileType::TextileCarpet: return "Textiles/TextileCarpet.png";
        case TileType::TextileFabric1: return "Textiles/TextileFabric1.png";
        case TileType::TextileFabric2: return "Textiles/TextileFabric2.png";
        case TileType::TextileRope1: return "Textiles/TextileRope1.png";
        case TileType::TextileRope2: return "Textiles/TextileRope2.png";

        // FadeOut
        case TileType::FadeCornerLargeBL: return "FadeOut/CournerLargeBL.png";
        case TileType::FadeCornerLargeBR: return "FadeOut/CournerLargeBR.png";
        case TileType::FadeCornerLargeTL: return "FadeOut/CournerLargeTL.png";
        case TileType::FadeCornerLargeTR: return "FadeOut/CournerLargeTR.png";
        case TileType::FadeCornerSmallBL: return "FadeOut/CournerSmallBL.png";
        case TileType::FadeCornerSmallBR: return "FadeOut/CournerSmallBR.png";
        case TileType::FadeCornerSmallTL: return "FadeOut/CournerSmallTL.png";
        case TileType::FadeCornerSmallTR: return "FadeOut/CournerSmallTR.png";
        case TileType::FadeFlatB: return "FadeOut/FlatB.png";
        case TileType::FadeFlatL: return "FadeOut/FlatL.png";
        case TileType::FadeFlatR: return "FadeOut/FlatR.png";
        case TileType::FadeFlatT: return "FadeOut/FlatT.png";
        case TileType::FadeLonelyBlockB: return "FadeOut/LonelyBlockB.png";
        case TileType::FadeLonelyBlockL: return "FadeOut/LonelyBlockL.png";
        case TileType::FadeLonelyBlockR: return "FadeOut/LonelyBlockR.png";
        case TileType::FadeLonelyBlockT: return "FadeOut/LonelyBlockT.png";

        default: return "";
    }
}

int GetTileCategory(TileType type) {
    uint16_t typeVal = static_cast<uint16_t>(type);
    return (typeVal >> 8) & 0x0F;
}

const char* GetTileDisplayName(TileType type) {
    switch (type) {
        // Ground
        case TileType::Empty: return "Empty";
        case TileType::GroundDirt: return "Dirt";
        case TileType::GroundForrest1: return "Forest Floor 1";
        case TileType::GroundForrest2: return "Forest Floor 2";
        case TileType::GroundGrass1: return "Grass 1";
        case TileType::GroundGrass2: return "Grass 2";
        case TileType::GroundRocks: return "Rocky Ground";

        // Concrete
        case TileType::ConcreteAsphalt1: return "Asphalt 1";
        case TileType::ConcreteAsphalt2: return "Asphalt 2";
        case TileType::ConcreteAsphalt2Steps1: return "Asphalt Steps 1";
        case TileType::ConcreteAsphalt2Steps2: return "Asphalt Steps 2";
        case TileType::ConcreteBlocks1: return "Concrete Blocks 1";
        case TileType::ConcreteBlocks2: return "Concrete Blocks 2";
        case TileType::ConcretePad: return "Concrete Pad";
        case TileType::ConcreteTiles1: return "Concrete Tiles 1";
        case TileType::ConcreteTiles2: return "Concrete Tiles 2";

        // Bricks
        case TileType::BricksBlack: return "Black Bricks";
        case TileType::BricksGrey: return "Grey Bricks";
        case TileType::BricksRock: return "Rock Bricks";
        case TileType::BricksRockFrontBot: return "Rock Bricks (Bottom)";
        case TileType::BricksRockFrontLhs: return "Rock Bricks (Left)";
        case TileType::BricksRockFrontRhs: return "Rock Bricks (Right)";
        case TileType::BricksRockFrontTop: return "Rock Bricks (Top)";
        case TileType::BricksStacked: return "Stacked Bricks";
        case TileType::BricksCornerBL: return "Brick Corner BL";
        case TileType::BricksCornerBLRI: return "Brick Corner BL Inner";
        case TileType::BricksCornerBLRO: return "Brick Corner BL Outer";
        case TileType::BricksCornerBR: return "Brick Corner BR";
        case TileType::BricksCornerBRRI: return "Brick Corner BR Inner";
        case TileType::BricksCornerBRRO: return "Brick Corner BR Outer";
        case TileType::BricksCornerTL: return "Brick Corner TL";
        case TileType::BricksCornerTLRI: return "Brick Corner TL Inner";
        case TileType::BricksCornerTLRO: return "Brick Corner TL Outer";
        case TileType::BricksCornerTR: return "Brick Corner TR";
        case TileType::BricksCornerTRRI: return "Brick Corner TR Inner";
        case TileType::BricksCornerTRRO: return "Brick Corner TR Outer";

        // Wood
        case TileType::Wood1: return "Wood Planks";
        case TileType::WoodCrate1: return "Wood Crate 1";
        case TileType::WoodCrate2: return "Wood Crate 2";
        case TileType::WoodFlooring1: return "Wood Flooring 1";
        case TileType::WoodFlooring2: return "Wood Flooring 2";

        // Stone
        case TileType::StoneBlack: return "Black Stone";
        case TileType::StoneMarble1: return "Marble 1";
        case TileType::StoneMarble2: return "Marble 2";
        case TileType::StoneRaw: return "Raw Stone";

        // Metal
        case TileType::Metal1: return "Metal Sheet 1";
        case TileType::Metal2: return "Metal Sheet 2";
        case TileType::Metal3: return "Metal Sheet 3";
        case TileType::Metal4: return "Metal Sheet 4";
        case TileType::MetalTile1: return "Metal Tile 1";
        case TileType::MetalTile2: return "Metal Tile 2";
        case TileType::MetalTile3: return "Metal Tile 3";
        case TileType::MetalTile4: return "Metal Tile 4";
        case TileType::MetalShopFront: return "Shop Front";
        case TileType::MetalShopFrontB: return "Shop Front (Bottom)";
        case TileType::MetalShopFrontL: return "Shop Front (Left)";
        case TileType::MetalShopFrontR: return "Shop Front (Right)";
        case TileType::MetalShopFrontT: return "Shop Front (Top)";

        // Foliage
        case TileType::FoliageBonsai: return "Bonsai Tree";
        case TileType::FoliageBottleBrush: return "Bottle Brush";
        case TileType::FoliageCherryTree: return "Cherry Tree";
        case TileType::FoliagePalm1: return "Palm Tree";
        case TileType::FoliagePlanterBox: return "Planter Box";
        case TileType::FoliagePlanterBox2: return "Planter Box 2";
        case TileType::FoliagePlanterBox3: return "Planter Box 3";
        case TileType::FoliagePlanterBox4: return "Planter Box 4";
        case TileType::FoliagePotPlant: return "Pot Plant";
        case TileType::FoliageSilverOak: return "Silver Oak";
        case TileType::FoliageSilverOakBrown: return "Silver Oak (Brown)";
        case TileType::FoliageTree1: return "Tree 1";
        case TileType::FoliageTree2: return "Tree 2";
        case TileType::FoliageTree3: return "Tree 3";
        case TileType::FoliageYellowTree1: return "Yellow Tree";
        case TileType::FoliageShrub1: return "Shrub";

        // Water
        case TileType::Water1: return "Water";

        // Objects
        case TileType::ObjectBarStool: return "Bar Stool";
        case TileType::ObjectClothesStand: return "Clothes Stand";
        case TileType::ObjectClothesStand2: return "Clothes Stand 2";
        case TileType::ObjectDeskFan: return "Desk Fan";
        case TileType::ObjectDeskTop: return "Desktop";
        case TileType::ObjectDeskTop0: return "Desktop 0";
        case TileType::ObjectDeskTop1: return "Desktop 1";
        case TileType::ObjectDeskTop2: return "Desktop 2";
        case TileType::ObjectDeskTop3: return "Desktop 3";
        case TileType::ObjectDeskTop4: return "Desktop 4";
        case TileType::ObjectGarbage1: return "Garbage 1";
        case TileType::ObjectGarbage2: return "Garbage 2";
        case TileType::ObjectGarbage3: return "Garbage 3";
        case TileType::ObjectGenerator: return "Generator";
        case TileType::ObjectGenerator2: return "Generator 2";
        case TileType::ObjectGenerator3: return "Generator 3";
        case TileType::ObjectPiping1: return "Pipes 1";
        case TileType::ObjectPiping3: return "Pipes 3";
        case TileType::ObjectPiping4: return "Pipes 4";
        case TileType::ObjectShopFront: return "Shop Front";
        case TileType::ObjectShopSolo: return "Shop Solo";

        // Textiles
        case TileType::TextileBasket: return "Basket Weave";
        case TileType::TextileCarpet: return "Carpet";
        case TileType::TextileFabric1: return "Fabric 1";
        case TileType::TextileFabric2: return "Fabric 2";
        case TileType::TextileRope1: return "Rope 1";
        case TileType::TextileRope2: return "Rope 2";

        // FadeOut
        case TileType::FadeCornerLargeBL: return "Fade Corner Large BL";
        case TileType::FadeCornerLargeBR: return "Fade Corner Large BR";
        case TileType::FadeCornerLargeTL: return "Fade Corner Large TL";
        case TileType::FadeCornerLargeTR: return "Fade Corner Large TR";
        case TileType::FadeCornerSmallBL: return "Fade Corner Small BL";
        case TileType::FadeCornerSmallBR: return "Fade Corner Small BR";
        case TileType::FadeCornerSmallTL: return "Fade Corner Small TL";
        case TileType::FadeCornerSmallTR: return "Fade Corner Small TR";
        case TileType::FadeFlatB: return "Fade Flat Bottom";
        case TileType::FadeFlatL: return "Fade Flat Left";
        case TileType::FadeFlatR: return "Fade Flat Right";
        case TileType::FadeFlatT: return "Fade Flat Top";
        case TileType::FadeLonelyBlockB: return "Fade Block Bottom";
        case TileType::FadeLonelyBlockL: return "Fade Block Left";
        case TileType::FadeLonelyBlockR: return "Fade Block Right";
        case TileType::FadeLonelyBlockT: return "Fade Block Top";

        default: return "Unknown";
    }
}

// ============================================================================
// LevelEditor Implementation
// ============================================================================

LevelEditor::LevelEditor() = default;

LevelEditor::~LevelEditor() {
    if (m_initialized) {
        Shutdown();
    }
}

void LevelEditor::Initialize(const Config& config) {
    if (m_initialized) {
        return;
    }

    m_config = config;
    m_wallHeight = config.defaultWallHeight;
    m_initialized = true;
}

void LevelEditor::Shutdown() {
    if (!m_initialized) {
        return;
    }

    ClearHistory();
    m_pendingChanges.clear();
    m_currentOperationChanges.clear();
    m_enabled = false;
    m_initialized = false;
}

void LevelEditor::SetEnabled(bool enabled) {
    if (!m_initialized) {
        return;
    }

    if (m_enabled != enabled) {
        // If disabling while drawing, end the operation
        if (m_enabled && m_isDrawing) {
            EndPaint();
        }
        m_enabled = enabled;
    }
}

bool LevelEditor::CanEdit(const glm::vec2& playerPos, const EntityManager& entities) const {
    if (!m_initialized || !m_enabled) {
        return false;
    }

    // Check if any zombies are within the safe radius
    return !entities.HasZombiesInRadius(playerPos, m_config.safeRadius);
}

void LevelEditor::SetTool(Tool tool) {
    if (!m_initialized) {
        return;
    }

    if (m_currentTool != tool) {
        // End any in-progress operation when changing tools
        if (m_isDrawing) {
            EndPaint();
        }

        m_currentTool = tool;
        m_hasPreview = false;

        if (OnToolChanged) {
            OnToolChanged(tool);
        }
    }
}

const char* LevelEditor::GetToolName(Tool tool) noexcept {
    switch (tool) {
        case Tool::Select: return "Select";
        case Tool::Paint: return "Paint";
        case Tool::Erase: return "Erase";
        case Tool::Fill: return "Fill";
        case Tool::Rectangle: return "Rectangle";
        case Tool::Eyedropper: return "Eyedropper";
        default: return "Unknown";
    }
}

void LevelEditor::SetSelectedTile(TileType type, uint8_t variant) {
    m_selectedTile = type;
    m_selectedVariant = variant;
}

void LevelEditor::SetBrushSize(int size) {
    m_brushSize = std::clamp(size, 1, m_config.maxBrushSize);
}

void LevelEditor::SetWallMode(bool isWall) {
    m_wallMode = isWall;
}

void LevelEditor::SetWallHeight(float height) {
    m_wallHeight = std::max(0.0f, height);
}

void LevelEditor::OnMouseDown(const glm::vec2& worldPos, int button) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    if (button == 0) { // Left mouse button
        switch (m_currentTool) {
            case Tool::Paint:
            case Tool::Erase:
                BeginPaint(worldPos);
                break;
            case Tool::Rectangle:
                BeginRectangle(worldPos);
                break;
            case Tool::Fill:
                DoFill(worldPos);
                break;
            case Tool::Eyedropper:
                DoEyedrop(worldPos);
                break;
            case Tool::Select:
                DoSelect(worldPos);
                break;
        }
    } else if (button == 1) { // Right mouse button - quick eyedropper
        DoEyedrop(worldPos);
    }
}

void LevelEditor::OnMouseUp(const glm::vec2& worldPos, int button) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    if (button == 0) {
        switch (m_currentTool) {
            case Tool::Paint:
            case Tool::Erase:
                EndPaint();
                break;
            case Tool::Rectangle:
                EndRectangle();
                break;
            default:
                break;
        }
    }
}

void LevelEditor::OnMouseMove(const glm::vec2& worldPos) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    m_previewPos = worldPos;
    m_hasPreview = true;

    if (m_isDrawing) {
        switch (m_currentTool) {
            case Tool::Paint:
            case Tool::Erase:
                ContinuePaint(worldPos);
                break;
            case Tool::Rectangle:
                UpdateRectangle(worldPos);
                break;
            default:
                break;
        }
    }
}

void LevelEditor::OnKeyPress(int key) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Key shortcuts
    switch (key) {
        case 'B': // Brush tool
        case 'P':
            SetTool(Tool::Paint);
            break;
        case 'E': // Erase tool
            SetTool(Tool::Erase);
            break;
        case 'G': // Fill tool (bucket)
            SetTool(Tool::Fill);
            break;
        case 'R': // Rectangle tool
            SetTool(Tool::Rectangle);
            break;
        case 'I': // Eyedropper
        case 'K':
            SetTool(Tool::Eyedropper);
            break;
        case 'V': // Select tool
            SetTool(Tool::Select);
            break;
        case '[': // Decrease brush size
            SetBrushSize(m_brushSize - 1);
            break;
        case ']': // Increase brush size
            SetBrushSize(m_brushSize + 1);
            break;
        case 'Z': // Undo (would need ctrl modifier check in real impl)
            Undo();
            break;
        case 'Y': // Redo
            Redo();
            break;
        default:
            break;
    }
}

void LevelEditor::Undo() {
    if (!CanUndo()) {
        return;
    }

    auto& changes = m_undoStack.back();

    // Swap old and new values to create redo entry
    for (auto& change : changes) {
        std::swap(change.oldType, change.newType);
        std::swap(change.oldVariant, change.newVariant);
        std::swap(change.wasWall, change.isWall);
        std::swap(change.oldWallHeight, change.newWallHeight);
    }

    // Add to redo stack
    m_redoStack.push_back(std::move(changes));
    m_undoStack.pop_back();

    // Add undone changes to pending
    for (const auto& change : m_redoStack.back()) {
        m_pendingChanges.push_back(change);
    }
}

void LevelEditor::Redo() {
    if (!CanRedo()) {
        return;
    }

    auto& changes = m_redoStack.back();

    // Swap old and new values to restore original direction
    for (auto& change : changes) {
        std::swap(change.oldType, change.newType);
        std::swap(change.oldVariant, change.newVariant);
        std::swap(change.wasWall, change.isWall);
        std::swap(change.oldWallHeight, change.newWallHeight);
    }

    // Add back to undo stack
    m_undoStack.push_back(std::move(changes));
    m_redoStack.pop_back();

    // Add redone changes to pending
    for (const auto& change : m_undoStack.back()) {
        m_pendingChanges.push_back(change);
    }
}

void LevelEditor::ClearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void LevelEditor::ApplyChanges(TileMap& map) {
    for (const auto& change : m_pendingChanges) {
        if (map.InBounds(change.x, change.y)) {
            map.SetTile(change.x, change.y, change.newType, change.newVariant);
            map.SetWall(change.x, change.y, change.isWall, change.newWallHeight);
        }
    }

    if (OnChangesApplied && !m_pendingChanges.empty()) {
        OnChangesApplied(m_pendingChanges);
    }
}

void LevelEditor::ClearPendingChanges() {
    m_pendingChanges.clear();
}

int LevelEditor::GetPendingCost() const noexcept {
    return static_cast<int>(m_pendingChanges.size()) * m_config.coinCostPerTile;
}

// ============================================================================
// Private Implementation
// ============================================================================

void LevelEditor::BeginPaint(const glm::vec2& worldPos) {
    m_isDrawing = true;
    m_lastPaintPos = worldPos;
    m_currentOperationChanges.clear();

    glm::ivec2 tile = WorldToTile(worldPos);
    if (m_currentTool == Tool::Erase) {
        EraseTile(tile.x, tile.y);
    } else {
        PaintBrush(tile.x, tile.y);
    }
}

void LevelEditor::ContinuePaint(const glm::vec2& worldPos) {
    if (!m_isDrawing) {
        return;
    }

    // Interpolate between last position and current to avoid gaps
    glm::vec2 delta = worldPos - m_lastPaintPos;
    float distance = glm::length(delta);
    float stepSize = m_tileSize * 0.5f;

    if (distance > stepSize) {
        glm::vec2 dir = delta / distance;
        float traveled = 0.0f;

        while (traveled < distance) {
            glm::vec2 pos = m_lastPaintPos + dir * traveled;
            glm::ivec2 tile = WorldToTile(pos);

            if (m_currentTool == Tool::Erase) {
                EraseTile(tile.x, tile.y);
            } else {
                PaintBrush(tile.x, tile.y);
            }

            traveled += stepSize;
        }
    }

    // Paint at final position
    glm::ivec2 tile = WorldToTile(worldPos);
    if (m_currentTool == Tool::Erase) {
        EraseTile(tile.x, tile.y);
    } else {
        PaintBrush(tile.x, tile.y);
    }

    m_lastPaintPos = worldPos;
}

void LevelEditor::EndPaint() {
    if (!m_isDrawing) {
        return;
    }

    m_isDrawing = false;

    // Commit current operation to undo stack
    if (!m_currentOperationChanges.empty()) {
        m_undoStack.push_back(std::move(m_currentOperationChanges));
        m_currentOperationChanges.clear();

        // Clear redo stack on new operation
        m_redoStack.clear();

        // Trim undo history if needed
        TrimUndoHistory();
    }
}

void LevelEditor::BeginRectangle(const glm::vec2& worldPos) {
    m_isDrawing = true;
    m_rectStart = worldPos;
    m_rectEnd = worldPos;
    m_currentOperationChanges.clear();
}

void LevelEditor::UpdateRectangle(const glm::vec2& worldPos) {
    if (!m_isDrawing) {
        return;
    }
    m_rectEnd = worldPos;
}

void LevelEditor::EndRectangle() {
    if (!m_isDrawing) {
        return;
    }

    m_isDrawing = false;

    // Convert to tile coordinates
    glm::ivec2 start = WorldToTile(m_rectStart);
    glm::ivec2 end = WorldToTile(m_rectEnd);

    // Ensure start is top-left and end is bottom-right
    int minX = std::min(start.x, end.x);
    int maxX = std::max(start.x, end.x);
    int minY = std::min(start.y, end.y);
    int maxY = std::max(start.y, end.y);

    // Paint all tiles in rectangle
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            PaintTile(x, y);
        }
    }

    // Commit operation
    if (!m_currentOperationChanges.empty()) {
        m_undoStack.push_back(std::move(m_currentOperationChanges));
        m_currentOperationChanges.clear();
        m_redoStack.clear();
        TrimUndoHistory();
    }
}

void LevelEditor::DoFill(const glm::vec2& worldPos) {
    if (!m_currentMap) {
        return;
    }

    glm::ivec2 tile = WorldToTile(worldPos);
    TileType targetType = m_currentMap->GetTile(tile.x, tile.y);

    // Don't fill if already the same type
    if (targetType == m_selectedTile) {
        return;
    }

    m_currentOperationChanges.clear();
    FloodFill(tile.x, tile.y, targetType, m_selectedTile);

    // Commit operation
    if (!m_currentOperationChanges.empty()) {
        m_undoStack.push_back(std::move(m_currentOperationChanges));
        m_currentOperationChanges.clear();
        m_redoStack.clear();
        TrimUndoHistory();
    }
}

void LevelEditor::DoEyedrop(const glm::vec2& worldPos) {
    if (!m_currentMap) {
        return;
    }

    glm::ivec2 tile = WorldToTile(worldPos);
    TileType type = m_currentMap->GetTile(tile.x, tile.y);
    uint8_t variant = m_currentMap->GetVariant(tile.x, tile.y);

    SetSelectedTile(type, variant);

    if (OnTilePicked) {
        OnTilePicked(type, variant);
    }
}

void LevelEditor::DoSelect(const glm::vec2& worldPos) {
    // Selection logic would go here
    // For now, just update preview position
    m_previewPos = worldPos;
}

void LevelEditor::PaintTile(int x, int y) {
    if (!m_currentMap || !m_currentMap->InBounds(x, y)) {
        return;
    }

    // Check if tile already has this value
    TileType currentType = m_currentMap->GetTile(x, y);
    uint8_t currentVariant = m_currentMap->GetVariant(x, y);
    bool currentIsWall = m_currentMap->IsWall(x, y);
    float currentHeight = m_currentMap->GetWallHeight(x, y);

    if (currentType == m_selectedTile &&
        currentVariant == m_selectedVariant &&
        currentIsWall == m_wallMode &&
        (!m_wallMode || currentHeight == m_wallHeight)) {
        return; // No change needed
    }

    // Create change record
    TileChange change;
    change.x = x;
    change.y = y;
    change.oldType = currentType;
    change.newType = m_selectedTile;
    change.oldVariant = currentVariant;
    change.newVariant = m_selectedVariant;
    change.wasWall = currentIsWall;
    change.isWall = m_wallMode;
    change.oldWallHeight = currentHeight;
    change.newWallHeight = m_wallMode ? m_wallHeight : 0.0f;
    change.timestamp = GetTimestamp();

    RecordChange(change);
}

void LevelEditor::PaintBrush(int centerX, int centerY) {
    auto tiles = GetBrushTiles(centerX, centerY);
    for (const auto& [x, y] : tiles) {
        PaintTile(x, y);
    }
}

void LevelEditor::EraseTile(int x, int y) {
    // Temporarily store selected tile
    TileType savedTile = m_selectedTile;
    uint8_t savedVariant = m_selectedVariant;
    bool savedWallMode = m_wallMode;

    // Set to empty
    m_selectedTile = TileType::Empty;
    m_selectedVariant = 0;
    m_wallMode = false;

    // Paint (erase) with brush
    PaintBrush(x, y);

    // Restore
    m_selectedTile = savedTile;
    m_selectedVariant = savedVariant;
    m_wallMode = savedWallMode;
}

std::vector<std::pair<int, int>> LevelEditor::GetBrushTiles(int centerX, int centerY) const {
    std::vector<std::pair<int, int>> tiles;

    if (m_brushSize == 1) {
        tiles.emplace_back(centerX, centerY);
        return tiles;
    }

    // Circular brush
    int radius = m_brushSize / 2;
    float radiusSq = static_cast<float>(radius * radius);

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            float distSq = static_cast<float>(dx * dx + dy * dy);
            if (distSq <= radiusSq) {
                tiles.emplace_back(centerX + dx, centerY + dy);
            }
        }
    }

    return tiles;
}

void LevelEditor::FloodFill(int startX, int startY, TileType targetType, TileType fillType) {
    if (!m_currentMap || !m_currentMap->InBounds(startX, startY)) {
        return;
    }

    // Use BFS for flood fill
    std::queue<std::pair<int, int>> queue;
    std::unordered_set<int> visited;

    auto toKey = [width = m_currentMap->GetWidth()](int x, int y) {
        return y * width + x;
    };

    queue.emplace(startX, startY);
    visited.insert(toKey(startX, startY));

    // Limit fill to prevent infinite loops
    const int maxFillTiles = 10000;
    int filledCount = 0;

    while (!queue.empty() && filledCount < maxFillTiles) {
        auto [x, y] = queue.front();
        queue.pop();

        if (m_currentMap->GetTile(x, y) != targetType) {
            continue;
        }

        // Paint this tile
        TileType currentType = m_currentMap->GetTile(x, y);
        uint8_t currentVariant = m_currentMap->GetVariant(x, y);
        bool currentIsWall = m_currentMap->IsWall(x, y);
        float currentHeight = m_currentMap->GetWallHeight(x, y);

        TileChange change;
        change.x = x;
        change.y = y;
        change.oldType = currentType;
        change.newType = fillType;
        change.oldVariant = currentVariant;
        change.newVariant = m_selectedVariant;
        change.wasWall = currentIsWall;
        change.isWall = m_wallMode;
        change.oldWallHeight = currentHeight;
        change.newWallHeight = m_wallMode ? m_wallHeight : 0.0f;
        change.timestamp = GetTimestamp();

        RecordChange(change);
        ++filledCount;

        // Add neighbors
        const int dx[] = {0, 1, 0, -1};
        const int dy[] = {-1, 0, 1, 0};

        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            int key = toKey(nx, ny);

            if (m_currentMap->InBounds(nx, ny) && visited.find(key) == visited.end()) {
                visited.insert(key);
                queue.emplace(nx, ny);
            }
        }
    }
}

glm::ivec2 LevelEditor::WorldToTile(const glm::vec2& worldPos) const {
    return glm::ivec2(
        static_cast<int>(std::floor(worldPos.x / m_tileSize)),
        static_cast<int>(std::floor(worldPos.y / m_tileSize))
    );
}

void LevelEditor::RecordChange(const TileChange& change) {
    m_currentOperationChanges.push_back(change);
    m_pendingChanges.push_back(change);
}

void LevelEditor::TrimUndoHistory() {
    while (m_undoStack.size() > static_cast<size_t>(m_config.maxUndoHistory)) {
        m_undoStack.pop_front();
    }
}

uint64_t LevelEditor::GetTimestamp() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
    );
}

} // namespace Vehement
