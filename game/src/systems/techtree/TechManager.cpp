#include "TechManager.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {
namespace TechTree {

// ============================================================================
// PlayerTechState
// ============================================================================

nlohmann::json PlayerTechState::ToJson() const {
    nlohmann::json j;

    j["player_id"] = playerId;
    j["culture"] = culture;
    j["current_age"] = TechAgeToShortString(currentAge);

    nlohmann::json treesArray = nlohmann::json::array();
    for (const auto& tree : trees) {
        treesArray.push_back(tree->ToJson());
    }
    j["trees"] = treesArray;

    j["unlocked_buildings"] = std::vector<std::string>(unlockedBuildings.begin(), unlockedBuildings.end());
    j["unlocked_units"] = std::vector<std::string>(unlockedUnits.begin(), unlockedUnits.end());
    j["unlocked_abilities"] = std::vector<std::string>(unlockedAbilities.begin(), unlockedAbilities.end());
    j["unlocked_upgrades"] = std::vector<std::string>(unlockedUpgrades.begin(), unlockedUpgrades.end());
    j["unlocked_spells"] = std::vector<std::string>(unlockedSpells.begin(), unlockedSpells.end());
    j["unlocked_features"] = std::vector<std::string>(unlockedFeatures.begin(), unlockedFeatures.end());

    j["stats"] = {
        {"total_researched", totalTechsResearched},
        {"total_lost", totalTechsLost},
        {"total_time", totalResearchTime},
        {"highest_age", TechAgeToShortString(highestAgeReached)}
    };

    return j;
}

void PlayerTechState::FromJson(const nlohmann::json& j, const std::vector<TechTreeDef*>& treeDefs) {
    if (j.contains("player_id")) playerId = j["player_id"].get<std::string>();
    if (j.contains("culture")) culture = j["culture"].get<std::string>();
    if (j.contains("current_age")) currentAge = StringToTechAge(j["current_age"].get<std::string>());

    if (j.contains("trees")) {
        trees.clear();
        for (size_t i = 0; i < j["trees"].size() && i < treeDefs.size(); ++i) {
            auto tree = std::make_unique<PlayerTechTree>(treeDefs[i], playerId);
            tree->FromJson(j["trees"][i]);
            trees.push_back(std::move(tree));
        }
    }

    auto loadSet = [&j](const std::string& key, std::unordered_set<std::string>& set) {
        if (j.contains(key)) {
            auto vec = j[key].get<std::vector<std::string>>();
            set = std::unordered_set<std::string>(vec.begin(), vec.end());
        }
    };

    loadSet("unlocked_buildings", unlockedBuildings);
    loadSet("unlocked_units", unlockedUnits);
    loadSet("unlocked_abilities", unlockedAbilities);
    loadSet("unlocked_upgrades", unlockedUpgrades);
    loadSet("unlocked_spells", unlockedSpells);
    loadSet("unlocked_features", unlockedFeatures);

    if (j.contains("stats")) {
        const auto& stats = j["stats"];
        if (stats.contains("total_researched")) totalTechsResearched = stats["total_researched"].get<int>();
        if (stats.contains("total_lost")) totalTechsLost = stats["total_lost"].get<int>();
        if (stats.contains("total_time")) totalResearchTime = stats["total_time"].get<float>();
        if (stats.contains("highest_age")) highestAgeReached = StringToTechAge(stats["highest_age"].get<std::string>());
    }
}

// ============================================================================
// TechManager
// ============================================================================

TechManager& TechManager::Instance() {
    static TechManager instance;
    return instance;
}

void TechManager::Initialize() {
    if (m_initialized) return;
    m_initialized = true;
}

void TechManager::Shutdown() {
    m_playerStates.clear();
    m_techTrees.clear();
    m_techToTree.clear();
    m_researchSpeedMultipliers.clear();
    m_initialized = false;
}

bool TechManager::LoadTechTree(const std::string& filePath) {
    auto tree = std::make_unique<TechTreeDef>();
    if (!tree->LoadFromFile(filePath)) {
        return false;
    }

    // Build tech-to-tree mapping
    for (const auto& [techId, node] : tree->GetAllNodes()) {
        m_techToTree[techId] = tree->GetId();
    }

    m_techTrees[tree->GetId()] = std::move(tree);
    return true;
}

int TechManager::LoadTechTreesFromDirectory(const std::string& directory) {
    int count = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() == ".json") {
                if (LoadTechTree(entry.path().string())) {
                    count++;
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }

    return count;
}

void TechManager::RegisterTechTree(std::unique_ptr<TechTreeDef> treeDef) {
    if (!treeDef) return;

    const std::string& treeId = treeDef->GetId();

    // Build tech-to-tree mapping
    for (const auto& [techId, node] : treeDef->GetAllNodes()) {
        m_techToTree[techId] = treeId;
    }

    m_techTrees[treeId] = std::move(treeDef);
}

const TechTreeDef* TechManager::GetTechTree(const std::string& treeId) const {
    auto it = m_techTrees.find(treeId);
    return it != m_techTrees.end() ? it->second.get() : nullptr;
}

std::vector<std::string> TechManager::GetTechTreeIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_techTrees.size());
    for (const auto& [id, tree] : m_techTrees) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<const TechTreeDef*> TechManager::GetTreesForCulture(const std::string& culture) const {
    std::vector<const TechTreeDef*> trees;

    for (const auto& [id, tree] : m_techTrees) {
        if (tree->IsUniversal() || tree->GetCulture() == culture || tree->GetCulture().empty()) {
            trees.push_back(tree.get());
        }
    }

    return trees;
}

void TechManager::InitializePlayer(const std::string& playerId, const std::string& culture) {
    auto state = std::make_unique<PlayerTechState>();
    state->playerId = playerId;
    state->culture = culture;

    // Get trees for this culture
    auto treeDefs = GetTreesForCulture(culture);

    for (const auto* treeDef : treeDefs) {
        auto playerTree = std::make_unique<PlayerTechTree>(treeDef, playerId);

        // Set up callback
        playerTree->SetOnResearchEvent([this, playerId](const ResearchEvent& event) {
            if (event.type == ResearchEventType::TechUnlocked) {
                OnTechResearched(playerId, event.techId);
            } else if (event.type == ResearchEventType::TechLost) {
                OnTechLost(playerId, event.techId);
            }
        });

        state->trees.push_back(std::move(playerTree));
    }

    m_researchSpeedMultipliers[playerId] = 1.0f;
    m_playerStates[playerId] = std::move(state);
}

void TechManager::RemovePlayer(const std::string& playerId) {
    m_playerStates.erase(playerId);
    m_researchSpeedMultipliers.erase(playerId);
}

bool TechManager::HasPlayer(const std::string& playerId) const {
    return m_playerStates.find(playerId) != m_playerStates.end();
}

const PlayerTechState* TechManager::GetPlayerState(const std::string& playerId) const {
    auto it = m_playerStates.find(playerId);
    return it != m_playerStates.end() ? it->second.get() : nullptr;
}

TechAge TechManager::GetPlayerAge(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    return state ? state->currentAge : TechAge::Stone;
}

void TechManager::SetPlayerAge(const std::string& playerId, TechAge age) {
    auto it = m_playerStates.find(playerId);
    if (it != m_playerStates.end()) {
        it->second->currentAge = age;
        if (static_cast<int>(age) > static_cast<int>(it->second->highestAgeReached)) {
            it->second->highestAgeReached = age;
        }
    }
}

bool TechManager::HasTech(const std::string& playerId, const std::string& techId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return false;

    for (const auto& tree : state->trees) {
        if (tree->HasTech(techId)) {
            return true;
        }
    }
    return false;
}

bool TechManager::CanResearch(const std::string& playerId, const std::string& techId,
                               const IRequirementContext& context) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return false;

    for (const auto& tree : state->trees) {
        if (tree->CanResearch(techId, context)) {
            return true;
        }
    }
    return false;
}

ResearchStatus TechManager::GetTechStatus(const std::string& playerId, const std::string& techId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return ResearchStatus::Locked;

    for (const auto& tree : state->trees) {
        auto status = tree->GetTechStatus(techId);
        if (status != ResearchStatus::Locked) {
            return status;
        }
    }
    return ResearchStatus::Locked;
}

std::vector<std::string> TechManager::GetResearchedTechs(const std::string& playerId) const {
    std::vector<std::string> techs;

    auto* state = GetPlayerState(playerId);
    if (!state) return techs;

    for (const auto& tree : state->trees) {
        const auto& completed = tree->GetCompletedTechs();
        techs.insert(techs.end(), completed.begin(), completed.end());
    }

    return techs;
}

std::vector<std::string> TechManager::GetAvailableTechs(const std::string& playerId,
                                                         const IRequirementContext& context) const {
    std::vector<std::string> available;

    auto* state = GetPlayerState(playerId);
    if (!state) return available;

    for (const auto& tree : state->trees) {
        const auto* treeDef = tree->GetTreeDef();
        if (!treeDef) continue;

        for (const auto& [techId, node] : treeDef->GetAllNodes()) {
            if (tree->CanResearch(techId, context)) {
                available.push_back(techId);
            }
        }
    }

    return available;
}

const TechNode* TechManager::GetTechNode(const std::string& techId) const {
    auto it = m_techToTree.find(techId);
    if (it == m_techToTree.end()) return nullptr;

    auto* tree = GetTechTree(it->second);
    return tree ? tree->GetNode(techId) : nullptr;
}

bool TechManager::StartResearch(const std::string& playerId, const std::string& techId,
                                 const IRequirementContext& context) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return false;

    for (auto& tree : it->second->trees) {
        if (tree->CanResearch(techId, context)) {
            return tree->StartResearch(techId, context);
        }
    }

    return false;
}

void TechManager::Update(float deltaTime) {
    for (auto& [playerId, state] : m_playerStates) {
        float speedMult = GetResearchSpeedMultiplier(playerId);

        for (auto& tree : state->trees) {
            tree->UpdateResearch(deltaTime, speedMult);
        }
    }
}

void TechManager::CompleteCurrentResearch(const std::string& playerId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    for (auto& tree : it->second->trees) {
        if (tree->IsResearching()) {
            tree->CompleteCurrentResearch();
            return;
        }
    }
}

std::map<std::string, int> TechManager::CancelResearch(const std::string& playerId, float refundPercent) {
    std::map<std::string, int> refund;

    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return refund;

    for (auto& tree : it->second->trees) {
        if (tree->IsResearching()) {
            return tree->CancelResearch(refundPercent);
        }
    }

    return refund;
}

void TechManager::GrantTech(const std::string& playerId, const std::string& techId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    // Find which tree has this tech
    auto treeIt = m_techToTree.find(techId);
    if (treeIt == m_techToTree.end()) return;

    for (auto& tree : it->second->trees) {
        if (tree->GetTreeDef() && tree->GetTreeDef()->GetId() == treeIt->second) {
            tree->GrantTech(techId);
            return;
        }
    }
}

bool TechManager::LoseTech(const std::string& playerId, const std::string& techId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return false;

    for (auto& tree : it->second->trees) {
        if (tree->HasTech(techId)) {
            return tree->LoseTech(techId);
        }
    }

    return false;
}

bool TechManager::QueueResearch(const std::string& playerId, const std::string& techId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return false;

    auto treeIt = m_techToTree.find(techId);
    if (treeIt == m_techToTree.end()) return false;

    for (auto& tree : it->second->trees) {
        if (tree->GetTreeDef() && tree->GetTreeDef()->GetId() == treeIt->second) {
            return tree->QueueResearch(techId);
        }
    }

    return false;
}

bool TechManager::DequeueResearch(const std::string& playerId, const std::string& techId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return false;

    for (auto& tree : it->second->trees) {
        if (tree->DequeueResearch(techId)) {
            return true;
        }
    }

    return false;
}

void TechManager::ClearResearchQueue(const std::string& playerId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    for (auto& tree : it->second->trees) {
        tree->ClearQueue();
    }
}

std::vector<std::string> TechManager::GetResearchQueue(const std::string& playerId) const {
    std::vector<std::string> queue;

    auto* state = GetPlayerState(playerId);
    if (!state) return queue;

    for (const auto& tree : state->trees) {
        const auto& treeQueue = tree->GetQueue();
        queue.insert(queue.end(), treeQueue.begin(), treeQueue.end());
    }

    return queue;
}

bool TechManager::IsQueued(const std::string& playerId, const std::string& techId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return false;

    for (const auto& tree : state->trees) {
        if (tree->IsQueued(techId)) {
            return true;
        }
    }

    return false;
}

std::string TechManager::GetCurrentResearch(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return "";

    for (const auto& tree : state->trees) {
        if (tree->IsResearching()) {
            return tree->GetCurrentResearch();
        }
    }

    return "";
}

float TechManager::GetCurrentResearchProgress(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return 0.0f;

    for (const auto& tree : state->trees) {
        if (tree->IsResearching()) {
            return tree->GetCurrentProgress();
        }
    }

    return 0.0f;
}

float TechManager::GetCurrentResearchRemainingTime(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return 0.0f;

    for (const auto& tree : state->trees) {
        if (tree->IsResearching()) {
            return tree->GetCurrentRemainingTime();
        }
    }

    return 0.0f;
}

bool TechManager::IsResearching(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return false;

    for (const auto& tree : state->trees) {
        if (tree->IsResearching()) {
            return true;
        }
    }

    return false;
}

// Unlock queries
bool TechManager::IsBuildingUnlocked(const std::string& playerId, const std::string& buildingId) const {
    auto* state = GetPlayerState(playerId);
    return state && state->unlockedBuildings.count(buildingId) > 0;
}

bool TechManager::IsUnitUnlocked(const std::string& playerId, const std::string& unitId) const {
    auto* state = GetPlayerState(playerId);
    return state && state->unlockedUnits.count(unitId) > 0;
}

bool TechManager::IsAbilityUnlocked(const std::string& playerId, const std::string& abilityId) const {
    auto* state = GetPlayerState(playerId);
    return state && state->unlockedAbilities.count(abilityId) > 0;
}

bool TechManager::IsUpgradeUnlocked(const std::string& playerId, const std::string& upgradeId) const {
    auto* state = GetPlayerState(playerId);
    return state && state->unlockedUpgrades.count(upgradeId) > 0;
}

bool TechManager::IsSpellUnlocked(const std::string& playerId, const std::string& spellId) const {
    auto* state = GetPlayerState(playerId);
    return state && state->unlockedSpells.count(spellId) > 0;
}

bool TechManager::IsFeatureUnlocked(const std::string& playerId, const std::string& featureId) const {
    auto* state = GetPlayerState(playerId);
    return state && state->unlockedFeatures.count(featureId) > 0;
}

std::vector<std::string> TechManager::GetUnlockedBuildings(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return {};
    return std::vector<std::string>(state->unlockedBuildings.begin(), state->unlockedBuildings.end());
}

std::vector<std::string> TechManager::GetUnlockedUnits(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return {};
    return std::vector<std::string>(state->unlockedUnits.begin(), state->unlockedUnits.end());
}

std::vector<std::string> TechManager::GetUnlockedAbilities(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return {};
    return std::vector<std::string>(state->unlockedAbilities.begin(), state->unlockedAbilities.end());
}

// Modifier queries
float TechManager::GetModifiedStat(const std::string& playerId, const std::string& stat,
                                    float baseValue, const std::string& entityType,
                                    const std::vector<std::string>& entityTags,
                                    const std::string& entityId) const {
    auto* state = GetPlayerState(playerId);
    if (!state) return baseValue;

    return state->activeModifiers.GetModifiedValue(stat, baseValue, entityType, entityTags, entityId);
}

float TechManager::GetStatFlatBonus(const std::string& playerId, const std::string& stat) const {
    auto* state = GetPlayerState(playerId);
    return state ? state->activeModifiers.GetFlatBonus(stat) : 0.0f;
}

float TechManager::GetStatPercentBonus(const std::string& playerId, const std::string& stat) const {
    auto* state = GetPlayerState(playerId);
    return state ? state->activeModifiers.GetPercentBonus(stat) : 0.0f;
}

std::vector<TechModifier> TechManager::GetActiveModifiers(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    return state ? state->activeModifiers.GetAllModifiers() : std::vector<TechModifier>{};
}

void TechManager::SetResearchSpeedMultiplier(const std::string& playerId, float multiplier) {
    m_researchSpeedMultipliers[playerId] = multiplier;
}

float TechManager::GetResearchSpeedMultiplier(const std::string& playerId) const {
    auto it = m_researchSpeedMultipliers.find(playerId);
    return it != m_researchSpeedMultipliers.end() ? it->second : 1.0f;
}

void TechManager::OnTechResearched(const std::string& playerId, const std::string& techId) {
    RebuildUnlocksAndModifiers(playerId);

    if (m_onTechUnlocked) {
        const auto* node = GetTechNode(techId);
        if (node) {
            m_onTechUnlocked(playerId, techId, *node);
        }
    }
}

void TechManager::OnTechLost(const std::string& playerId, const std::string& techId) {
    RebuildUnlocksAndModifiers(playerId);

    if (m_onTechLost) {
        m_onTechLost(playerId, techId);
    }
}

void TechManager::RebuildUnlocksAndModifiers(const std::string& playerId) {
    auto it = m_playerStates.find(playerId);
    if (it == m_playerStates.end()) return;

    auto& state = *it->second;

    // Clear current unlocks and modifiers
    state.unlockedBuildings.clear();
    state.unlockedUnits.clear();
    state.unlockedAbilities.clear();
    state.unlockedUpgrades.clear();
    state.unlockedSpells.clear();
    state.unlockedFeatures.clear();
    state.activeModifiers.Clear();

    // Rebuild from all researched techs
    for (const auto& tree : state.trees) {
        for (const auto& techId : tree->GetCompletedTechs()) {
            const auto* node = GetTechNode(techId);
            if (!node) continue;

            // Add unlocks
            const auto& unlocks = node->GetUnlocks();
            for (const auto& b : unlocks.buildings) state.unlockedBuildings.insert(b);
            for (const auto& u : unlocks.units) state.unlockedUnits.insert(u);
            for (const auto& a : unlocks.abilities) state.unlockedAbilities.insert(a);
            for (const auto& up : unlocks.upgrades) state.unlockedUpgrades.insert(up);
            for (const auto& s : unlocks.spells) state.unlockedSpells.insert(s);
            for (const auto& f : unlocks.features) state.unlockedFeatures.insert(f);

            // Add modifiers
            for (auto mod : node->GetModifiers()) {
                mod.sourceId = techId;
                state.activeModifiers.AddModifier(mod);
            }
        }
    }

    if (m_onModifiersChanged) {
        m_onModifiersChanged(playerId);
    }
}

nlohmann::json TechManager::SavePlayerState(const std::string& playerId) const {
    auto* state = GetPlayerState(playerId);
    return state ? state->ToJson() : nlohmann::json{};
}

void TechManager::LoadPlayerState(const std::string& playerId, const nlohmann::json& j) {
    if (j.empty()) return;

    std::string culture = j.contains("culture") ? j["culture"].get<std::string>() : "";
    auto treeDefs = GetTreesForCulture(culture);

    std::vector<TechTreeDef*> treeDefPtrs;
    for (const auto* def : treeDefs) {
        treeDefPtrs.push_back(const_cast<TechTreeDef*>(def));
    }

    auto state = std::make_unique<PlayerTechState>();
    state->FromJson(j, treeDefPtrs);
    m_playerStates[playerId] = std::move(state);

    RebuildUnlocksAndModifiers(playerId);
}

nlohmann::json TechManager::SaveAllState() const {
    nlohmann::json j;

    nlohmann::json playersArray = nlohmann::json::array();
    for (const auto& [playerId, state] : m_playerStates) {
        playersArray.push_back(state->ToJson());
    }
    j["players"] = playersArray;

    return j;
}

void TechManager::LoadAllState(const nlohmann::json& j) {
    if (!j.contains("players")) return;

    for (const auto& playerJson : j["players"]) {
        if (playerJson.contains("player_id")) {
            std::string playerId = playerJson["player_id"].get<std::string>();
            LoadPlayerState(playerId, playerJson);
        }
    }
}

bool TechManager::SaveToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) return false;
        file << SaveAllState().dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool TechManager::LoadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;
        nlohmann::json j;
        file >> j;
        LoadAllState(j);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// TechScript
// ============================================================================

namespace TechScript {

bool HasTech(const std::string& playerId, const std::string& techId) {
    return TechManager::Instance().HasTech(playerId, techId);
}

void GrantTech(const std::string& playerId, const std::string& techId) {
    TechManager::Instance().GrantTech(playerId, techId);
}

void RevokeTech(const std::string& playerId, const std::string& techId) {
    TechManager::Instance().LoseTech(playerId, techId);
}

float GetModifiedStat(const std::string& playerId, const std::string& stat, float baseValue) {
    return TechManager::Instance().GetModifiedStat(playerId, stat, baseValue);
}

bool IsBuildingUnlocked(const std::string& playerId, const std::string& buildingId) {
    return TechManager::Instance().IsBuildingUnlocked(playerId, buildingId);
}

bool IsUnitUnlocked(const std::string& playerId, const std::string& unitId) {
    return TechManager::Instance().IsUnitUnlocked(playerId, unitId);
}

int GetPlayerAge(const std::string& playerId) {
    return static_cast<int>(TechManager::Instance().GetPlayerAge(playerId));
}

void SetPlayerAge(const std::string& playerId, int age) {
    TechManager::Instance().SetPlayerAge(playerId, static_cast<TechAge>(age));
}

} // namespace TechScript

} // namespace TechTree
} // namespace Vehement
