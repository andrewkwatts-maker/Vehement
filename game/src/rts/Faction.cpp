#include "Faction.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// Unit Stats
// ============================================================================

FactionUnitStats GetFactionUnitStats(FactionUnitType type) {
    FactionUnitStats stats;
    stats.type = type;

    switch (type) {
        // Zombie units
        case FactionUnitType::ZombieWalker:
            stats.name = "Walker";
            stats.description = "Slow but relentless undead";
            stats.health = 50;
            stats.moveSpeed = 2.0f;
            stats.attackDamage = 8.0f;
            stats.experienceValue = 5;
            break;

        case FactionUnitType::ZombieRunner:
            stats.name = "Runner";
            stats.description = "Fast zombie that can sprint";
            stats.health = 30;
            stats.moveSpeed = 6.0f;
            stats.attackDamage = 10.0f;
            stats.experienceValue = 10;
            break;

        case FactionUnitType::ZombieBrute:
            stats.name = "Brute";
            stats.description = "Massive zombie with high damage";
            stats.health = 200;
            stats.armor = 20;
            stats.moveSpeed = 1.5f;
            stats.attackDamage = 30.0f;
            stats.experienceValue = 25;
            break;

        case FactionUnitType::ZombieSpitter:
            stats.name = "Spitter";
            stats.description = "Ranged acid attack";
            stats.health = 40;
            stats.moveSpeed = 2.5f;
            stats.attackDamage = 15.0f;
            stats.attackRange = 8.0f;
            stats.experienceValue = 15;
            break;

        case FactionUnitType::ZombieScreamer:
            stats.name = "Screamer";
            stats.description = "Alerts nearby zombies";
            stats.health = 25;
            stats.moveSpeed = 3.0f;
            stats.attackDamage = 5.0f;
            stats.visionRange = 20.0f;
            stats.experienceValue = 20;
            break;

        // Bandit units
        case FactionUnitType::BanditScout:
            stats.name = "Scout";
            stats.description = "Fast reconnaissance unit";
            stats.health = 40;
            stats.moveSpeed = 7.0f;
            stats.attackDamage = 8.0f;
            stats.visionRange = 15.0f;
            stats.experienceValue = 8;
            stats.loot.Add(ResourceType::Coins, 5);
            break;

        case FactionUnitType::BanditRaider:
            stats.name = "Raider";
            stats.description = "Standard bandit fighter";
            stats.health = 80;
            stats.armor = 10;
            stats.moveSpeed = 4.0f;
            stats.attackDamage = 15.0f;
            stats.experienceValue = 15;
            stats.loot.Add(ResourceType::Coins, 10);
            stats.loot.Add(ResourceType::Metal, 2);
            break;

        case FactionUnitType::BanditArcher:
            stats.name = "Archer";
            stats.description = "Ranged bandit attacker";
            stats.health = 50;
            stats.moveSpeed = 3.5f;
            stats.attackDamage = 12.0f;
            stats.attackRange = 12.0f;
            stats.experienceValue = 12;
            stats.loot.Add(ResourceType::Wood, 3);
            break;

        case FactionUnitType::BanditBoss:
            stats.name = "Boss";
            stats.description = "Bandit leader with powerful gear";
            stats.health = 200;
            stats.armor = 25;
            stats.moveSpeed = 3.0f;
            stats.attackDamage = 25.0f;
            stats.experienceValue = 50;
            stats.loot.Add(ResourceType::Coins, 50);
            stats.loot.Add(ResourceType::Metal, 10);
            break;

        // Wild creatures
        case FactionUnitType::Wolf:
            stats.name = "Wolf";
            stats.description = "Fast pack hunter";
            stats.health = 60;
            stats.moveSpeed = 8.0f;
            stats.attackDamage = 12.0f;
            stats.attackSpeed = 1.5f;
            stats.experienceValue = 10;
            stats.loot.Add(ResourceType::Food, 5);
            break;

        case FactionUnitType::Bear:
            stats.name = "Bear";
            stats.description = "Powerful territorial predator";
            stats.health = 250;
            stats.armor = 15;
            stats.moveSpeed = 4.0f;
            stats.attackDamage = 35.0f;
            stats.experienceValue = 30;
            stats.loot.Add(ResourceType::Food, 15);
            break;

        case FactionUnitType::GiantSpider:
            stats.name = "Giant Spider";
            stats.description = "Venomous ambush predator";
            stats.health = 80;
            stats.moveSpeed = 5.0f;
            stats.attackDamage = 15.0f;
            stats.canClimb = true;
            stats.experienceValue = 20;
            break;

        case FactionUnitType::Wyvern:
            stats.name = "Wyvern";
            stats.description = "Flying dragon-like creature";
            stats.health = 150;
            stats.moveSpeed = 10.0f;
            stats.attackDamage = 25.0f;
            stats.attackRange = 6.0f;
            stats.canFly = true;
            stats.experienceValue = 50;
            break;

        // Guardian units
        case FactionUnitType::StoneGolem:
            stats.name = "Stone Golem";
            stats.description = "Ancient construct of living stone";
            stats.health = 500;
            stats.armor = 50;
            stats.moveSpeed = 2.0f;
            stats.attackDamage = 40.0f;
            stats.experienceValue = 75;
            stats.loot.Add(ResourceType::Stone, 25);
            break;

        case FactionUnitType::SpectralKnight:
            stats.name = "Spectral Knight";
            stats.description = "Ghostly warrior from ages past";
            stats.health = 120;
            stats.armor = 30;
            stats.moveSpeed = 6.0f;
            stats.attackDamage = 30.0f;
            stats.experienceValue = 40;
            break;

        case FactionUnitType::AncientMage:
            stats.name = "Ancient Mage";
            stats.description = "Powerful spellcaster";
            stats.health = 80;
            stats.moveSpeed = 3.0f;
            stats.attackDamage = 50.0f;
            stats.attackRange = 15.0f;
            stats.experienceValue = 60;
            break;

        // Rival kingdom units
        case FactionUnitType::Peasant:
            stats.name = "Peasant";
            stats.description = "Civilian worker";
            stats.health = 30;
            stats.moveSpeed = 3.0f;
            stats.attackDamage = 3.0f;
            stats.experienceValue = 2;
            break;

        case FactionUnitType::Militia:
            stats.name = "Militia";
            stats.description = "Basic trained soldier";
            stats.health = 70;
            stats.armor = 15;
            stats.moveSpeed = 4.0f;
            stats.attackDamage = 12.0f;
            stats.experienceValue = 15;
            break;

        case FactionUnitType::Knight:
            stats.name = "Knight";
            stats.description = "Heavy armored cavalry";
            stats.health = 180;
            stats.armor = 40;
            stats.moveSpeed = 7.0f;
            stats.attackDamage = 28.0f;
            stats.experienceValue = 35;
            break;

        case FactionUnitType::Siege:
            stats.name = "Siege Engine";
            stats.description = "Destroys buildings";
            stats.health = 300;
            stats.armor = 20;
            stats.moveSpeed = 1.5f;
            stats.attackDamage = 100.0f;
            stats.attackRange = 10.0f;
            stats.experienceValue = 50;
            break;

        // Cult units
        case FactionUnitType::Cultist:
            stats.name = "Cultist";
            stats.description = "Dark magic user";
            stats.health = 40;
            stats.moveSpeed = 3.5f;
            stats.attackDamage = 18.0f;
            stats.attackRange = 8.0f;
            stats.experienceValue = 15;
            break;

        case FactionUnitType::DarkPriest:
            stats.name = "Dark Priest";
            stats.description = "Heals and buffs other cultists";
            stats.health = 60;
            stats.moveSpeed = 3.0f;
            stats.attackDamage = 10.0f;
            stats.attackRange = 12.0f;
            stats.experienceValue = 25;
            break;

        case FactionUnitType::DemonSpawn:
            stats.name = "Demon Spawn";
            stats.description = "Summoned fiend";
            stats.health = 150;
            stats.moveSpeed = 5.0f;
            stats.attackDamage = 35.0f;
            stats.experienceValue = 40;
            break;

        // Mutant units
        case FactionUnitType::MutantDog:
            stats.name = "Mutant Dog";
            stats.description = "Twisted canine";
            stats.health = 45;
            stats.moveSpeed = 9.0f;
            stats.attackDamage = 14.0f;
            stats.experienceValue = 8;
            break;

        case FactionUnitType::Abomination:
            stats.name = "Abomination";
            stats.description = "Multi-limbed horror";
            stats.health = 300;
            stats.armor = 10;
            stats.moveSpeed = 3.0f;
            stats.attackDamage = 25.0f;
            stats.attackSpeed = 2.0f;
            stats.experienceValue = 45;
            break;

        case FactionUnitType::ToxicBlob:
            stats.name = "Toxic Blob";
            stats.description = "Spreads poison on death";
            stats.health = 80;
            stats.moveSpeed = 2.0f;
            stats.attackDamage = 10.0f;
            stats.experienceValue = 15;
            break;

        default:
            stats.name = "Unknown";
            break;
    }

    return stats;
}

// ============================================================================
// Faction Implementation
// ============================================================================

Faction::FactionId Faction::s_nextId = 1;

Faction::Faction() : m_id(s_nextId++) {}

Faction::Faction(FactionType type) : m_id(s_nextId++), m_type(type) {
    m_name = FactionTypeToString(type);
    m_hostility = GetDefaultHostility(type);
}

Faction::~Faction() = default;

bool Faction::Initialize(const FactionData& data) {
    m_type = data.type;
    m_name = data.name;
    m_hostility = data.defaultHostility;
    m_behavior = data.defaultBehavior;
    m_canBeAllied = data.canBeAllied;
    m_canBeBribed = data.canBeBribed;
    m_spawnRate = data.baseSpawnRate;
    m_availableUnits = data.availableUnits;
    m_territoryRadius = data.territoryRadius;

    return true;
}

void Faction::Update(float deltaTime) {
    UpdateSpawning(deltaTime);
    UpdateBehavior(deltaTime);
    ProcessAttackQueue(deltaTime);
}

void Faction::SetTerritory(const glm::vec3& min, const glm::vec3& max) {
    m_territoryMin = min;
    m_territoryMax = max;
}

bool Faction::IsInTerritory(const glm::vec3& pos) const {
    return pos.x >= m_territoryMin.x && pos.x <= m_territoryMax.x &&
           pos.y >= m_territoryMin.y && pos.y <= m_territoryMax.y &&
           pos.z >= m_territoryMin.z && pos.z <= m_territoryMax.z;
}

void Faction::SpawnUnit(FactionUnitType type, const glm::vec3& position) {
    if (m_currentUnits >= m_maxUnits) return;

    ++m_currentUnits;

    if (m_onSpawn) {
        m_onSpawn(type, position);
    }
}

void Faction::SpawnWave(const AttackWave& wave, const glm::vec3& targetPosition) {
    m_attackTarget = targetPosition;
    m_isAttacking = true;

    for (const auto& [unitType, count] : wave.units) {
        for (int i = 0; i < count; ++i) {
            // Calculate spawn position
            glm::vec3 spawnPos = m_homePosition + glm::vec3(wave.spawnDirection) * 10.0f;
            spawnPos.x += static_cast<float>(i % 5) * 2.0f;
            spawnPos.z += static_cast<float>(i / 5) * 2.0f;

            SpawnUnit(unitType, spawnPos);
        }
    }

    if (m_onAttack) {
        m_onAttack(wave, targetPosition);
    }
}

void Faction::OnUnitDeath(FactionUnitType /* type */) {
    --m_currentUnits;
    if (m_currentUnits < 0) m_currentUnits = 0;
}

void Faction::ModifyRelationship(int delta) {
    int oldRelation = m_playerRelationship;
    m_playerRelationship = std::clamp(m_playerRelationship + delta, -100, 100);

    // Update hostility based on relationship
    if (m_playerRelationship >= 50) {
        m_hostility = Hostility::Friendly;
    } else if (m_playerRelationship >= 0) {
        m_hostility = Hostility::Neutral;
    } else if (m_playerRelationship >= -50) {
        m_hostility = Hostility::Suspicious;
    } else {
        m_hostility = Hostility::Hostile;
    }

    if (m_onRelationChange && oldRelation != m_playerRelationship) {
        m_onRelationChange(oldRelation, m_playerRelationship);
    }
}

bool Faction::CanNegotiate() const {
    // Can't negotiate with mindless factions
    if (m_type == FactionType::Zombies ||
        m_type == FactionType::MutantSwarm ||
        m_type == FactionType::NaturalDisasters) {
        return false;
    }

    // Can't negotiate if too hostile
    if (m_hostility == Hostility::Berserk) {
        return false;
    }

    return true;
}

bool Faction::AttemptBribe(const ResourceCost& /* offer */) {
    if (!m_canBeBribed || !CanNegotiate()) {
        return false;
    }

    // Accept bribe and improve relations
    ModifyRelationship(25);
    return true;
}

bool Faction::FormAlliance() {
    if (!m_canBeAllied || m_playerRelationship < 50) {
        return false;
    }

    m_hostility = Hostility::Friendly;
    m_playerRelationship = 100;
    return true;
}

void Faction::QueueAttack(const AttackWave& wave, float delay) {
    m_attackQueue.push_back({wave, delay});
}

void Faction::CancelAttacks() {
    m_attackQueue.clear();
    m_isAttacking = false;
}

void Faction::UpdateSpawning(float deltaTime) {
    if (m_currentUnits >= m_maxUnits) return;
    if (m_availableUnits.empty()) return;

    m_spawnTimer += deltaTime;

    if (m_spawnTimer >= 1.0f / m_spawnRate) {
        m_spawnTimer = 0.0f;

        // Pick random unit type
        size_t idx = static_cast<size_t>(rand()) % m_availableUnits.size();
        FactionUnitType type = m_availableUnits[idx];

        // Spawn near home
        glm::vec3 spawnPos = m_homePosition;
        spawnPos.x += static_cast<float>(rand() % 20 - 10);
        spawnPos.z += static_cast<float>(rand() % 20 - 10);

        SpawnUnit(type, spawnPos);
    }
}

void Faction::UpdateBehavior(float /* deltaTime */) {
    switch (m_behavior) {
        case FactionBehavior::Passive:
            // Do nothing
            break;

        case FactionBehavior::Patrol:
            // Would update patrol paths
            break;

        case FactionBehavior::Hunt:
            // Actively seek targets
            break;

        case FactionBehavior::Defend:
            // Stay near home
            break;

        case FactionBehavior::Raid:
            // Attack then retreat
            break;

        case FactionBehavior::Siege:
            // Focus on buildings
            break;

        case FactionBehavior::Swarm:
            // Attack with numbers
            break;

        case FactionBehavior::Ambush:
            // Wait for targets
            break;

        case FactionBehavior::Trade:
            // Seek trading partners
            break;
    }
}

void Faction::ProcessAttackQueue(float deltaTime) {
    if (m_attackQueue.empty()) return;

    m_attackQueue.front().delay -= deltaTime;

    if (m_attackQueue.front().delay <= 0) {
        SpawnWave(m_attackQueue.front().wave, m_attackTarget);
        m_attackQueue.erase(m_attackQueue.begin());
    }
}

// ============================================================================
// FactionManager Implementation
// ============================================================================

FactionManager& FactionManager::Instance() {
    static FactionManager instance;
    return instance;
}

bool FactionManager::Initialize() {
    if (m_initialized) return true;

    InitializeFactionData();
    m_initialized = true;
    return true;
}

void FactionManager::Shutdown() {
    m_factions.clear();
    m_factionTemplates.clear();
    m_initialized = false;
}

void FactionManager::Update(float deltaTime) {
    for (auto& faction : m_factions) {
        faction->Update(deltaTime);
    }
}

Faction* FactionManager::GetFaction(Faction::FactionId id) {
    for (auto& faction : m_factions) {
        if (faction->GetId() == id) {
            return faction.get();
        }
    }
    return nullptr;
}

std::vector<Faction*> FactionManager::GetFactionsByType(FactionType type) {
    std::vector<Faction*> result;
    for (auto& faction : m_factions) {
        if (faction->GetType() == type) {
            result.push_back(faction.get());
        }
    }
    return result;
}

const FactionData* FactionManager::GetFactionData(FactionType type) const {
    auto it = m_factionTemplates.find(type);
    return (it != m_factionTemplates.end()) ? &it->second : nullptr;
}

Faction* FactionManager::CreateFaction(FactionType type, const glm::vec3& homePosition) {
    const FactionData* data = GetFactionData(type);
    if (!data) return nullptr;

    auto faction = std::make_unique<Faction>(type);
    faction->Initialize(*data);
    faction->SetHomePosition(homePosition);

    Faction* ptr = faction.get();
    m_factions.push_back(std::move(faction));
    return ptr;
}

void FactionManager::RemoveFaction(Faction::FactionId id) {
    m_factions.erase(
        std::remove_if(m_factions.begin(), m_factions.end(),
            [id](const std::unique_ptr<Faction>& f) { return f->GetId() == id; }),
        m_factions.end());
}

void FactionManager::TriggerGlobalAttack(FactionType attackerType, const glm::vec3& target) {
    auto factions = GetFactionsByType(attackerType);
    for (Faction* f : factions) {
        f->SetAttackTarget(target);

        AttackWave wave;
        wave.name = "Global Attack";
        wave.units = {{FactionUnitType::ZombieWalker, 10}};
        f->QueueAttack(wave, 0.0f);
    }
}

void FactionManager::TriggerDisaster(const std::string& /* disasterType */,
                                      const glm::vec3& /* epicenter */,
                                      float /* radius */, float /* intensity */) {
    // Would create disaster event
}

void FactionManager::SetTimeOfDay(float hour) {
    m_timeOfDay = std::fmod(hour, 24.0f);
    m_isNight = (m_timeOfDay < 6.0f || m_timeOfDay > 20.0f);
}

void FactionManager::InitializeFactionData() {
    for (int i = 0; i < static_cast<int>(FactionType::COUNT); ++i) {
        CreateDefaultFactionData(static_cast<FactionType>(i));
    }
}

void FactionManager::CreateDefaultFactionData(FactionType type) {
    FactionData data;
    data.type = type;
    data.name = FactionTypeToString(type);
    data.description = GetFactionDescription(type);
    data.defaultHostility = GetDefaultHostility(type);

    switch (type) {
        case FactionType::Zombies:
            data.defaultBehavior = FactionBehavior::Swarm;
            data.respawns = true;
            data.canBeAllied = false;
            data.canBeBribed = false;
            data.baseSpawnRate = 0.2f;
            data.availableUnits = {
                FactionUnitType::ZombieWalker,
                FactionUnitType::ZombieRunner,
                FactionUnitType::ZombieBrute,
                FactionUnitType::ZombieSpitter
            };
            data.primaryColor = 0x556B2FFF; // Dark olive
            break;

        case FactionType::Bandits:
            data.defaultBehavior = FactionBehavior::Raid;
            data.respawns = true;
            data.canBeAllied = true;
            data.canBeBribed = true;
            data.baseSpawnRate = 0.05f;
            data.availableUnits = {
                FactionUnitType::BanditScout,
                FactionUnitType::BanditRaider,
                FactionUnitType::BanditArcher
            };
            data.primaryColor = 0x8B0000FF; // Dark red
            break;

        case FactionType::WildCreatures:
            data.defaultBehavior = FactionBehavior::Hunt;
            data.respawns = true;
            data.canBeAllied = false;
            data.canBeBribed = false;
            data.baseSpawnRate = 0.02f;
            data.definesTerritory = true;
            data.availableUnits = {
                FactionUnitType::Wolf,
                FactionUnitType::Bear,
                FactionUnitType::GiantSpider
            };
            data.primaryColor = 0x228B22FF; // Forest green
            break;

        case FactionType::AncientGuardians:
            data.defaultBehavior = FactionBehavior::Defend;
            data.respawns = false;
            data.canBeAllied = false;
            data.canBeBribed = false;
            data.baseSpawnRate = 0.01f;
            data.definesTerritory = true;
            data.availableUnits = {
                FactionUnitType::StoneGolem,
                FactionUnitType::SpectralKnight,
                FactionUnitType::AncientMage
            };
            data.primaryColor = 0x4169E1FF; // Royal blue
            break;

        case FactionType::RivalKingdom:
            data.defaultBehavior = FactionBehavior::Siege;
            data.respawns = true;
            data.canBeAllied = true;
            data.canBeBribed = true;
            data.baseSpawnRate = 0.03f;
            data.availableUnits = {
                FactionUnitType::Peasant,
                FactionUnitType::Militia,
                FactionUnitType::Knight,
                FactionUnitType::Siege
            };
            data.primaryColor = 0x800080FF; // Purple
            break;

        case FactionType::CultOfDarkness:
            data.defaultBehavior = FactionBehavior::Ambush;
            data.respawns = true;
            data.canBeAllied = false;
            data.canBeBribed = false;
            data.baseSpawnRate = 0.04f;
            data.availableUnits = {
                FactionUnitType::Cultist,
                FactionUnitType::DarkPriest,
                FactionUnitType::DemonSpawn
            };
            data.primaryColor = 0x2F2F2FFF; // Dark gray
            break;

        case FactionType::MutantSwarm:
            data.defaultBehavior = FactionBehavior::Swarm;
            data.respawns = true;
            data.canBeAllied = false;
            data.canBeBribed = false;
            data.baseSpawnRate = 0.15f;
            data.availableUnits = {
                FactionUnitType::MutantDog,
                FactionUnitType::Abomination,
                FactionUnitType::ToxicBlob
            };
            data.primaryColor = 0x00FF00FF; // Toxic green
            break;

        case FactionType::Merchants:
            data.defaultBehavior = FactionBehavior::Trade;
            data.defaultHostility = Hostility::Friendly;
            data.respawns = false;
            data.canBeAllied = true;
            data.primaryColor = 0xFFD700FF; // Gold
            break;

        case FactionType::Refugees:
            data.defaultBehavior = FactionBehavior::Passive;
            data.defaultHostility = Hostility::Friendly;
            data.respawns = false;
            data.canBeAllied = true;
            data.primaryColor = 0xD2B48CFF; // Tan
            break;

        default:
            data.defaultBehavior = FactionBehavior::Passive;
            break;
    }

    m_factionTemplates[type] = data;
}

} // namespace RTS
} // namespace Vehement
