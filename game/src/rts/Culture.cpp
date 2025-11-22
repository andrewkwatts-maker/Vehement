#include "Culture.hpp"
#include <algorithm>
#include <stdexcept>

namespace Vehement {
namespace RTS {

// =============================================================================
// CultureManager Singleton
// =============================================================================

CultureManager& CultureManager::Instance() {
    static CultureManager instance;
    return instance;
}

bool CultureManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    m_cultures.clear();
    m_cultures.reserve(static_cast<size_t>(CultureType::Count));

    // Initialize all cultures
    InitializeFortressCulture();
    InitializeBunkerCulture();
    InitializeNomadCulture();
    InitializeScavengerCulture();
    InitializeMerchantCulture();
    InitializeIndustrialCulture();
    InitializeUndergroundCulture();
    InitializeForestCulture();

    m_initialized = true;
    return true;
}

void CultureManager::Shutdown() {
    m_cultures.clear();
    m_initialized = false;
}

const CultureData* CultureManager::GetCultureData(CultureType type) const {
    auto it = std::find_if(m_cultures.begin(), m_cultures.end(),
        [type](const CultureData& data) { return data.type == type; });

    return (it != m_cultures.end()) ? &(*it) : nullptr;
}

std::string CultureManager::GetBuildingTexture(CultureType culture, BuildingType building) const {
    const CultureData* data = GetCultureData(culture);
    if (!data) {
        return "Vehement2/images/Bricks/BricksGrey.png"; // Default fallback
    }

    auto it = data->buildingTextures.find(building);
    if (it != data->buildingTextures.end()) {
        return it->second;
    }

    // Return default texture based on building category
    switch (building) {
        case BuildingType::Wall:
        case BuildingType::WallGate:
        case BuildingType::Tower:
            return "Vehement2/images/Bricks/BricksRock.png";
        case BuildingType::Bunker:
        case BuildingType::Turret:
            return "Vehement2/images/Concrete/Concrete1.png";
        default:
            return "Vehement2/images/Wood/Wood1.png";
    }
}

float CultureManager::ApplyBonus(CultureType culture, float baseValue,
                                  const std::string& bonusType) const {
    const CultureData* data = GetCultureData(culture);
    if (!data) {
        return baseValue;
    }

    const auto& b = data->bonuses;

    if (bonusType == "buildSpeed") return baseValue * b.buildSpeedMultiplier;
    if (bonusType == "buildCost") return baseValue * b.buildCostMultiplier;
    if (bonusType == "repairSpeed") return baseValue * b.repairSpeedMultiplier;
    if (bonusType == "gatherSpeed") return baseValue * b.gatherSpeedMultiplier;
    if (bonusType == "trade") return baseValue * b.tradeMultiplier;
    if (bonusType == "storage") return baseValue * b.storageMultiplier;
    if (bonusType == "production") return baseValue * b.productionMultiplier;
    if (bonusType == "defense") return baseValue * b.defenseMultiplier;
    if (bonusType == "attack") return baseValue * b.attackMultiplier;
    if (bonusType == "healing") return baseValue * b.healingMultiplier;
    if (bonusType == "wallHP") return baseValue * b.wallHPMultiplier;
    if (bonusType == "towerDamage") return baseValue * b.towerDamageMultiplier;
    if (bonusType == "buildingHP") return baseValue * b.buildingHPMultiplier;
    if (bonusType == "unitSpeed") return baseValue * b.unitSpeedMultiplier;
    if (bonusType == "caravanSpeed") return baseValue * b.caravanSpeedMultiplier;
    if (bonusType == "packingSpeed") return baseValue * b.packingSpeedMultiplier;
    if (bonusType == "vision") return baseValue * b.visionMultiplier;
    if (bonusType == "stealth") return baseValue * b.stealthMultiplier;
    if (bonusType == "detection") return baseValue * b.detectionMultiplier;

    return baseValue;
}

ResourceCost CultureManager::ApplyCostModifier(CultureType culture,
                                                const ResourceCost& baseCost) const {
    const CultureData* data = GetCultureData(culture);
    if (!data) {
        return baseCost;
    }

    return baseCost.Scaled(data->bonuses.buildCostMultiplier);
}

bool CultureManager::CanBuild(CultureType culture, BuildingType building) const {
    const CultureData* data = GetCultureData(culture);
    if (!data) {
        return false;
    }

    // Check if building is restricted for this culture
    auto restrictedIt = std::find(data->restrictedBuildings.begin(),
                                   data->restrictedBuildings.end(),
                                   building);
    if (restrictedIt != data->restrictedBuildings.end()) {
        return false;
    }

    // Check if building is unique to another culture
    for (const auto& otherCulture : m_cultures) {
        if (otherCulture.type != culture) {
            auto uniqueIt = std::find(otherCulture.uniqueBuildings.begin(),
                                       otherCulture.uniqueBuildings.end(),
                                       building);
            if (uniqueIt != otherCulture.uniqueBuildings.end()) {
                return false; // This building is unique to another culture
            }
        }
    }

    return true;
}

std::vector<BuildingType> CultureManager::GetAvailableBuildings(CultureType culture) const {
    std::vector<BuildingType> available;

    for (int i = 0; i < static_cast<int>(BuildingType::Count); ++i) {
        BuildingType building = static_cast<BuildingType>(i);
        if (CanBuild(culture, building)) {
            available.push_back(building);
        }
    }

    // Add unique buildings for this culture
    const CultureData* data = GetCultureData(culture);
    if (data) {
        for (BuildingType unique : data->uniqueBuildings) {
            if (std::find(available.begin(), available.end(), unique) == available.end()) {
                available.push_back(unique);
            }
        }
    }

    return available;
}

// =============================================================================
// Culture Initialization Functions
// =============================================================================

void CultureManager::InitializeFortressCulture() {
    CultureData data;
    data.type = CultureType::Fortress;
    data.name = "Fortress";
    data.description =
        "Masters of stone and steel, the Fortress culture builds imposing "
        "castles and fortifications that stand against the undead hordes. "
        "Their walls are legendary, though their methodical construction "
        "takes time. Once established, a Fortress settlement is nearly "
        "impossible to breach.";
    data.shortDescription = "Strong defenses, slow construction";

    // Bonuses: Strong walls, good tower damage, slow build speed
    data.bonuses.wallHPMultiplier = 1.50f;          // +50% wall HP
    data.bonuses.towerDamageMultiplier = 1.25f;     // +25% tower damage
    data.bonuses.buildingHPMultiplier = 1.20f;      // +20% building HP
    data.bonuses.buildSpeedMultiplier = 0.80f;      // -20% build speed (slower)
    data.bonuses.defenseMultiplier = 1.15f;         // +15% defense
    data.bonuses.unitSpeedMultiplier = 0.95f;       // -5% unit speed (heavy armor)

    // Building textures - Castle/Medieval style
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Stone/StoneMarble1.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Bricks/BricksRock.png";
    data.buildingTextures[BuildingType::WallGate] = "Vehement2/images/Bricks/BricksStacked.png";
    data.buildingTextures[BuildingType::Tower] = "Vehement2/images/Stone/StoneMarble2.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Bricks/BricksGrey.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Stone/StoneRaw.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Bricks/BricksBlack.png";
    data.buildingTextures[BuildingType::Castle] = "Vehement2/images/Stone/StoneMarble1.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person1.png";
    data.guardTexture = "Vehement2/images/People/Person2.png";
    data.eliteTexture = "Vehement2/images/People/Person3.png";
    data.scoutTexture = "Vehement2/images/People/Person4.png";

    // Visual style
    data.primaryColorHex = "#4A4A4A";       // Stone gray
    data.secondaryColorHex = "#8B0000";     // Dark red
    data.previewTexture = "Vehement2/images/Stone/StoneMarble1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Stone Walls - Walls have +50% HP and resist fire damage",
        "Castle Keep - Main building provides defensive aura to nearby structures",
        "Fortified Towers - Towers deal +25% damage and have extended range",
        "Hold the Line - Units near walls gain +20% defense"
    };

    // Unique buildings
    data.uniqueBuildings = { BuildingType::Castle };

    // Cannot build mobile/lightweight structures
    data.restrictedBuildings = {
        BuildingType::Yurt,
        BuildingType::MobileWorkshop,
        BuildingType::HiddenEntrance
    };

    // Starting resources (balanced start)
    data.startingResources = ResourceCost(200, 300, 100, 150, 50);
    data.startingBuildings = { BuildingType::Headquarters };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeBunkerCulture() {
    CultureData data;
    data.type = CultureType::Bunker;
    data.name = "Bunker";
    data.description =
        "Utilizing modern military doctrine, the Bunker culture constructs "
        "reinforced concrete emplacements and automated defense systems. "
        "Their turrets and bunkers provide overlapping fields of fire, "
        "creating kill zones that devastate zombie waves.";
    data.shortDescription = "High-tech defenses, automated turrets";

    // Bonuses: Turrets and tech focus
    data.bonuses.towerDamageMultiplier = 1.35f;     // +35% turret/tower damage
    data.bonuses.defenseMultiplier = 1.25f;         // +25% defense in bunkers
    data.bonuses.detectionMultiplier = 1.20f;       // +20% detection range
    data.bonuses.buildCostMultiplier = 1.15f;       // +15% build cost (expensive)
    data.bonuses.productionMultiplier = 1.10f;      // +10% production speed
    data.bonuses.healingMultiplier = 1.15f;         // +15% healing (medics)

    // Building textures - Modern military/industrial
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Concrete/Concrete1.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Concrete/Concrete2.png";
    data.buildingTextures[BuildingType::Bunker] = "Vehement2/images/Concrete/Concrete1.png";
    data.buildingTextures[BuildingType::Turret] = "Vehement2/images/Metal/Metal1.png";
    data.buildingTextures[BuildingType::Tower] = "Vehement2/images/Metal/Metal2.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Concrete/Concrete2.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Metal/Metal3.png";
    data.buildingTextures[BuildingType::Hospital] = "Vehement2/images/Concrete/Concrete1.png";
    data.buildingTextures[BuildingType::ResearchLab] = "Vehement2/images/Metal/Metal4.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person5.png";
    data.guardTexture = "Vehement2/images/People/Person6.png";
    data.eliteTexture = "Vehement2/images/People/Person7.png";
    data.scoutTexture = "Vehement2/images/People/Person8.png";

    // Visual style
    data.primaryColorHex = "#3D3D3D";       // Concrete gray
    data.secondaryColorHex = "#006400";     // Military green
    data.previewTexture = "Vehement2/images/Concrete/Concrete1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Automated Turrets - Turrets fire independently without operators",
        "Reinforced Concrete - Buildings resist explosive damage",
        "Kill Zone - Overlapping turret fire deals bonus damage",
        "Emergency Lockdown - All buildings become invulnerable briefly"
    };

    // No unique buildings, but excellent at standard military structures
    data.uniqueBuildings = {};

    // Cannot build rustic/stealth structures
    data.restrictedBuildings = {
        BuildingType::Yurt,
        BuildingType::HiddenEntrance,
        BuildingType::Bazaar
    };

    // Starting resources (metal-rich)
    data.startingResources = ResourceCost(150, 150, 250, 100, 75);
    data.startingBuildings = { BuildingType::Headquarters };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeNomadCulture() {
    CultureData data;
    data.type = CultureType::Nomad;
    data.name = "Nomad";
    data.description =
        "The wandering tribes have mastered the art of mobility. Their yurts "
        "and mobile workshops can be quickly assembled, disassembled, and "
        "relocated. When the horde grows too large, the Nomads simply pack "
        "up and move to safety, establishing a new camp elsewhere.";
    data.shortDescription = "Fast construction, mobile buildings";

    // Bonuses: Speed and mobility focused
    data.bonuses.buildSpeedMultiplier = 1.50f;      // +50% build speed
    data.bonuses.packingSpeedMultiplier = 2.00f;    // 2x packing/unpacking speed
    data.bonuses.unitSpeedMultiplier = 1.20f;       // +20% unit speed
    data.bonuses.caravanSpeedMultiplier = 1.30f;    // +30% caravan speed
    data.bonuses.buildingHPMultiplier = 0.75f;      // -25% building HP
    data.bonuses.wallHPMultiplier = 0.70f;          // -30% wall HP
    data.bonuses.gatherSpeedMultiplier = 1.15f;     // +15% gathering (nomadic efficiency)

    // Building textures - Tents, cloth, wood
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Textiles/Textile1.png";
    data.buildingTextures[BuildingType::Yurt] = "Vehement2/images/Textiles/Textile2.png";
    data.buildingTextures[BuildingType::MobileWorkshop] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Textiles/Textile1.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Wood/Wood2.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Wood/WoodFence.png";
    data.buildingTextures[BuildingType::Market] = "Vehement2/images/Textiles/Textile2.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person1.png";
    data.guardTexture = "Vehement2/images/People/Person3.png";
    data.eliteTexture = "Vehement2/images/People/Person5.png";
    data.scoutTexture = "Vehement2/images/People/Person7.png";

    // Visual style
    data.primaryColorHex = "#DEB887";       // Burlywood/canvas
    data.secondaryColorHex = "#8B4513";     // Saddle brown
    data.previewTexture = "Vehement2/images/Textiles/Textile1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Pack Up - Buildings can be packed into wagons and relocated",
        "Swift Assembly - Buildings are constructed 50% faster",
        "Caravan Masters - Trade caravans move 30% faster and carry more",
        "Escape Artists - Units gain speed boost when retreating"
    };

    // Unique mobile buildings
    data.uniqueBuildings = {
        BuildingType::Yurt,
        BuildingType::MobileWorkshop
    };

    // Cannot build heavy fortifications
    data.restrictedBuildings = {
        BuildingType::Castle,
        BuildingType::Bunker,
        BuildingType::HiddenEntrance,
        BuildingType::Factory
    };

    // Starting resources (balanced, mobile-ready)
    data.startingResources = ResourceCost(250, 100, 100, 200, 100);
    data.startingBuildings = { BuildingType::Headquarters, BuildingType::Yurt };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeScavengerCulture() {
    CultureData data;
    data.type = CultureType::Scavenger;
    data.name = "Scavenger";
    data.description =
        "Born from necessity, the Scavengers build with whatever they can "
        "find. Their makeshift structures may look ramshackle, but they're "
        "quick to erect and cheap to replace. When your base is destroyed, "
        "you simply build another from the ruins.";
    data.shortDescription = "Cheap buildings, fast expansion";

    // Bonuses: Cheap and fast, but weak
    data.bonuses.buildCostMultiplier = 0.60f;       // -40% build cost
    data.bonuses.buildSpeedMultiplier = 1.35f;      // +35% build speed
    data.bonuses.gatherSpeedMultiplier = 1.25f;     // +25% gathering (scavenging)
    data.bonuses.repairSpeedMultiplier = 1.50f;     // +50% repair speed
    data.bonuses.buildingHPMultiplier = 0.65f;      // -35% building HP
    data.bonuses.wallHPMultiplier = 0.60f;          // -40% wall HP
    data.bonuses.defenseMultiplier = 0.85f;         // -15% defense
    data.bonuses.storageMultiplier = 1.20f;         // +20% storage (hoarders)

    // Building textures - Mix of everything, worn look
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Metal/ShopFront.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Wood/WoodOld.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Metal/ShopFrontB.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Metal/MetalTile1.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Metal/ShopFrontR.png";
    data.buildingTextures[BuildingType::Tower] = "Vehement2/images/Metal/MetalTile2.png";
    data.buildingTextures[BuildingType::Market] = "Vehement2/images/Metal/ShopFrontL.png";
    data.buildingTextures[BuildingType::Farm] = "Vehement2/images/Wood/Wood2.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person2.png";
    data.guardTexture = "Vehement2/images/People/Person4.png";
    data.eliteTexture = "Vehement2/images/People/Person6.png";
    data.scoutTexture = "Vehement2/images/People/Person8.png";

    // Visual style
    data.primaryColorHex = "#8B8B7A";       // Rust/worn metal
    data.secondaryColorHex = "#CD853F";     // Rust orange
    data.previewTexture = "Vehement2/images/Metal/MetalTile1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Salvage - Destroyed buildings return 50% resources",
        "Improvised Defense - Can build anywhere without foundations",
        "Scrap Armor - Units gain temporary armor from nearby debris",
        "Rapid Reconstruction - Rebuild destroyed buildings instantly (cooldown)"
    };

    // No truly unique buildings - they improvise everything
    data.uniqueBuildings = {};

    // Cannot build high-end structures
    data.restrictedBuildings = {
        BuildingType::Castle,
        BuildingType::ResearchLab,
        BuildingType::Factory
    };

    // Starting resources (lots of everything, low quality)
    data.startingResources = ResourceCost(300, 200, 200, 250, 25);
    data.startingBuildings = { BuildingType::Headquarters, BuildingType::Storage };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeMerchantCulture() {
    CultureData data;
    data.type = CultureType::Merchant;
    data.name = "Merchant";
    data.description =
        "The Merchants know that gold wins wars. Their vast trade networks "
        "bring in exotic goods and resources from distant lands. With their "
        "bazaars and caravans, they can acquire anything - for the right "
        "price. Their wealth attracts mercenaries and allies alike.";
    data.shortDescription = "Trade bonuses, economic focus";

    // Bonuses: Trade and economy
    data.bonuses.tradeMultiplier = 1.30f;           // +30% trade profits
    data.bonuses.caravanSpeedMultiplier = 1.25f;    // +25% caravan speed
    data.bonuses.storageMultiplier = 1.40f;         // +40% storage capacity
    data.bonuses.gatherSpeedMultiplier = 1.10f;     // +10% gathering
    data.bonuses.buildCostMultiplier = 0.90f;       // -10% build cost (bulk buying)
    data.bonuses.productionMultiplier = 1.15f;      // +15% production
    data.bonuses.defenseMultiplier = 0.90f;         // -10% defense (traders, not fighters)

    // Building textures - Colorful, ornate
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Textiles/Textile1.png";
    data.buildingTextures[BuildingType::Bazaar] = "Vehement2/images/Textiles/Textile2.png";
    data.buildingTextures[BuildingType::Market] = "Vehement2/images/Textiles/Textile1.png";
    data.buildingTextures[BuildingType::Warehouse] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Wood/Wood2.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Bricks/BricksGrey.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Bricks/BricksStacked.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person1.png";
    data.guardTexture = "Vehement2/images/People/Person3.png";
    data.eliteTexture = "Vehement2/images/People/Person5.png";
    data.scoutTexture = "Vehement2/images/People/Person7.png";

    // Visual style
    data.primaryColorHex = "#FFD700";       // Gold
    data.secondaryColorHex = "#800080";     // Purple (royal)
    data.previewTexture = "Vehement2/images/Textiles/Textile1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Trade Routes - Establish profitable routes to other settlements",
        "Bazaar Discounts - Buy rare items at reduced prices",
        "Hire Mercenaries - Spend gold to instantly recruit units",
        "Diplomatic Immunity - Caravans cannot be attacked by NPC factions"
    };

    // Unique trade building
    data.uniqueBuildings = { BuildingType::Bazaar };

    // Cannot build military/stealth structures
    data.restrictedBuildings = {
        BuildingType::Bunker,
        BuildingType::HiddenEntrance,
        BuildingType::Castle,
        BuildingType::Factory
    };

    // Starting resources (gold-rich)
    data.startingResources = ResourceCost(150, 150, 100, 150, 300);
    data.startingBuildings = { BuildingType::Headquarters, BuildingType::Market };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeIndustrialCulture() {
    CultureData data;
    data.type = CultureType::Industrial;
    data.name = "Industrial";
    data.description =
        "The Industrial culture harnesses the power of mass production. "
        "Their factories churn out equipment and supplies at unprecedented "
        "rates. While their pollution draws zombies, their output ensures "
        "they're always ready for the next wave.";
    data.shortDescription = "High production, resource hungry";

    // Bonuses: Production focused
    data.bonuses.productionMultiplier = 1.50f;      // +50% production speed
    data.bonuses.gatherSpeedMultiplier = 1.20f;     // +20% resource gathering
    data.bonuses.buildSpeedMultiplier = 1.15f;      // +15% build speed
    data.bonuses.buildCostMultiplier = 1.10f;       // +10% build cost (industrial scale)
    data.bonuses.detectionMultiplier = 0.85f;       // -15% detection (noisy)
    data.bonuses.stealthMultiplier = 0.70f;         // -30% stealth (pollution/noise)
    data.bonuses.repairSpeedMultiplier = 1.30f;     // +30% repair speed

    // Building textures - Metal, industrial
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Metal/Metal1.png";
    data.buildingTextures[BuildingType::Factory] = "Vehement2/images/Metal/Metal2.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Metal/Metal3.png";
    data.buildingTextures[BuildingType::PowerPlant] = "Vehement2/images/Metal/Metal4.png";
    data.buildingTextures[BuildingType::Warehouse] = "Vehement2/images/Metal/MetalTile1.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Metal/MetalTile2.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Concrete/Concrete1.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Concrete/Concrete2.png";
    data.buildingTextures[BuildingType::ResearchLab] = "Vehement2/images/Metal/MetalTile3.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person2.png";
    data.guardTexture = "Vehement2/images/People/Person4.png";
    data.eliteTexture = "Vehement2/images/People/Person6.png";
    data.scoutTexture = "Vehement2/images/People/Person8.png";

    // Visual style
    data.primaryColorHex = "#4682B4";       // Steel blue
    data.secondaryColorHex = "#FF4500";     // Orange-red (furnace)
    data.previewTexture = "Vehement2/images/Metal/Metal1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Assembly Line - Produce multiple units simultaneously",
        "Automation - Factories operate without workers",
        "Industrial Output - +50% resource production from all sources",
        "Emergency Production - Temporarily double output (causes breakdown)"
    };

    // Unique industrial building
    data.uniqueBuildings = { BuildingType::Factory };

    // Cannot build stealth/mobile structures
    data.restrictedBuildings = {
        BuildingType::HiddenEntrance,
        BuildingType::Yurt,
        BuildingType::MobileWorkshop,
        BuildingType::Bazaar
    };

    // Starting resources (metal-focused)
    data.startingResources = ResourceCost(200, 200, 300, 100, 100);
    data.startingBuildings = { BuildingType::Headquarters, BuildingType::Workshop };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeUndergroundCulture() {
    CultureData data;
    data.type = CultureType::Underground;
    data.name = "Underground";
    data.description =
        "When the dead walk above, the living hide below. The Underground "
        "culture has mastered subterranean construction, creating hidden "
        "bunkers and tunnel networks that zombies cannot find. Their "
        "settlements are invisible until you're right on top of them.";
    data.shortDescription = "Hidden bases, tunnel networks";

    // Bonuses: Stealth and defense
    data.bonuses.stealthMultiplier = 2.00f;         // 2x stealth (hidden from fog of war)
    data.bonuses.defenseMultiplier = 1.50f;         // +50% defense while in bunkers
    data.bonuses.buildSpeedMultiplier = 0.85f;      // -15% build speed (excavation)
    data.bonuses.unitSpeedMultiplier = 0.90f;       // -10% surface movement
    data.bonuses.visionMultiplier = 0.80f;          // -20% vision (underground)
    data.bonuses.detectionMultiplier = 1.30f;       // +30% detection (tunnels for recon)
    data.bonuses.storageMultiplier = 1.25f;         // +25% storage (underground vaults)

    // Building textures - Stone, dark
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Stone/StoneBlack.png";
    data.buildingTextures[BuildingType::HiddenEntrance] = "Vehement2/images/Stone/StoneRaw.png";
    data.buildingTextures[BuildingType::Bunker] = "Vehement2/images/Stone/StoneBlack.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Stone/StoneRaw.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Stone/StoneMarble2.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Stone/StoneBlack.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Stone/StoneRaw.png";
    data.buildingTextures[BuildingType::Tower] = "Vehement2/images/Stone/StoneMarble1.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person1.png";
    data.guardTexture = "Vehement2/images/People/Person3.png";
    data.eliteTexture = "Vehement2/images/People/Person5.png";
    data.scoutTexture = "Vehement2/images/People/Person7.png";

    // Visual style
    data.primaryColorHex = "#2F4F4F";       // Dark slate gray
    data.secondaryColorHex = "#696969";     // Dim gray
    data.previewTexture = "Vehement2/images/Stone/StoneBlack.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Hidden Bases - Buildings invisible on enemy fog of war",
        "Tunnel Network - Units can travel between connected buildings",
        "Ambush - Units emerging from tunnels deal bonus damage",
        "Collapse Tunnel - Destroy tunnel to damage pursuing enemies"
    };

    // Unique underground structures
    data.uniqueBuildings = { BuildingType::HiddenEntrance };

    // Cannot build surface/mobile structures
    data.restrictedBuildings = {
        BuildingType::Yurt,
        BuildingType::MobileWorkshop,
        BuildingType::Bazaar,
        BuildingType::Castle,
        BuildingType::Factory
    };

    // Starting resources (stone-focused)
    data.startingResources = ResourceCost(150, 300, 150, 100, 50);
    data.startingBuildings = { BuildingType::Headquarters, BuildingType::HiddenEntrance };

    m_cultures.push_back(std::move(data));
}

void CultureManager::InitializeForestCulture() {
    CultureData data;
    data.type = CultureType::Forest;
    data.name = "Forest";
    data.description =
        "Living in harmony with nature, the Forest culture builds among the "
        "trees, using camouflage and natural barriers for protection. Their "
        "settlements blend seamlessly with the wilderness, making them "
        "nearly impossible to spot. Master ambushers and scouts.";
    data.shortDescription = "Camouflage, ambush tactics";

    // Bonuses: Stealth and mobility in terrain
    data.bonuses.stealthMultiplier = 1.60f;         // +60% stealth
    data.bonuses.visionMultiplier = 1.30f;          // +30% vision (scouts)
    data.bonuses.unitSpeedMultiplier = 1.15f;       // +15% unit speed (forest pathfinders)
    data.bonuses.attackMultiplier = 1.20f;          // +20% attack (ambush bonus)
    data.bonuses.gatherSpeedMultiplier = 1.20f;     // +20% gathering (foraging)
    data.bonuses.buildingHPMultiplier = 0.85f;      // -15% building HP (wood structures)
    data.bonuses.wallHPMultiplier = 0.80f;          // -20% wall HP
    data.bonuses.buildCostMultiplier = 0.85f;       // -15% build cost (natural materials)

    // Building textures - Wood, foliage
    data.buildingTextures[BuildingType::Headquarters] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Wall] = "Vehement2/images/Wood/WoodFence.png";
    data.buildingTextures[BuildingType::Tower] = "Vehement2/images/Wood/Wood2.png";
    data.buildingTextures[BuildingType::Barracks] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Workshop] = "Vehement2/images/Wood/Wood2.png";
    data.buildingTextures[BuildingType::Storage] = "Vehement2/images/Wood/Wood1.png";
    data.buildingTextures[BuildingType::Farm] = "Vehement2/images/Follage/Follage1.png";
    data.buildingTextures[BuildingType::Market] = "Vehement2/images/Wood/Wood2.png";

    // Unit textures
    data.workerTexture = "Vehement2/images/People/Person2.png";
    data.guardTexture = "Vehement2/images/People/Person4.png";
    data.eliteTexture = "Vehement2/images/People/Person6.png";
    data.scoutTexture = "Vehement2/images/People/Person9.png";

    // Visual style
    data.primaryColorHex = "#228B22";       // Forest green
    data.secondaryColorHex = "#8B4513";     // Saddle brown
    data.previewTexture = "Vehement2/images/Wood/Wood1.png";

    // Unique abilities
    data.uniqueAbilities = {
        "Camouflage - Buildings harder to spot in forested areas",
        "Ambush Tactics - First attack from stealth deals 2x damage",
        "Nature's Bounty - Farms produce 30% more food",
        "Pathfinders - Units ignore terrain movement penalties"
    };

    // No truly unique buildings - they adapt to the forest
    data.uniqueBuildings = {};

    // Cannot build heavy industrial/underground structures
    data.restrictedBuildings = {
        BuildingType::Factory,
        BuildingType::HiddenEntrance,
        BuildingType::Bunker,
        BuildingType::Castle
    };

    // Starting resources (wood-focused)
    data.startingResources = ResourceCost(350, 100, 50, 200, 50);
    data.startingBuildings = { BuildingType::Headquarters, BuildingType::Farm };

    m_cultures.push_back(std::move(data));
}

} // namespace RTS
} // namespace Vehement
