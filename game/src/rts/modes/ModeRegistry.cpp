#include "ModeRegistry.hpp"
#include "StandardModes.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {

ModeRegistry& ModeRegistry::Instance() {
    static ModeRegistry instance;
    return instance;
}

void ModeRegistry::Initialize() {
    if (m_initialized) return;

    RegisterBuiltInModes();

    // Add default custom mode directories
    m_customModeDirectories.push_back("game/assets/configs/modes");
    m_customModeDirectories.push_back("user/custom_modes");

    // Discover custom modes
    for (const auto& dir : m_customModeDirectories) {
        DiscoverCustomModes(dir);
    }

    m_initialized = true;
}

void ModeRegistry::Shutdown() {
    m_modes.clear();
    m_initialized = false;
}

void ModeRegistry::RegisterMode(const std::string& id, const ModeInfo& info) {
    m_modes[id] = info;

    if (OnModeRegistered) {
        OnModeRegistered(id);
    }
}

void ModeRegistry::UnregisterMode(const std::string& id) {
    auto it = m_modes.find(id);
    if (it != m_modes.end()) {
        m_modes.erase(it);

        if (OnModeUnregistered) {
            OnModeUnregistered(id);
        }
    }
}

bool ModeRegistry::IsModeRegistered(const std::string& id) const {
    return m_modes.find(id) != m_modes.end();
}

std::unique_ptr<GameMode> ModeRegistry::CreateMode(const std::string& id) const {
    auto it = m_modes.find(id);
    if (it == m_modes.end() || !it->second.factory) {
        return nullptr;
    }

    return it->second.factory();
}

void ModeRegistry::DiscoverCustomModes(const std::string& directory) {
    namespace fs = std::filesystem;

    if (!fs::exists(directory)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            const auto& path = entry.path();
            if (path.extension() == ".json" || path.extension() == ".mode") {
                LoadCustomMode(path.string());
            }
        }
    }
}

void ModeRegistry::RefreshModeList() {
    // Remove custom modes
    std::vector<std::string> toRemove;
    for (const auto& [id, info] : m_modes) {
        if (info.isCustom) {
            toRemove.push_back(id);
        }
    }

    for (const auto& id : toRemove) {
        m_modes.erase(id);
    }

    // Re-discover custom modes
    for (const auto& dir : m_customModeDirectories) {
        DiscoverCustomModes(dir);
    }
}

const ModeInfo* ModeRegistry::GetModeInfo(const std::string& id) const {
    auto it = m_modes.find(id);
    if (it != m_modes.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<ModeInfo> ModeRegistry::GetAllModes() const {
    std::vector<ModeInfo> result;
    result.reserve(m_modes.size());

    for (const auto& [id, info] : m_modes) {
        result.push_back(info);
    }

    // Sort by category then name
    std::sort(result.begin(), result.end(), [](const ModeInfo& a, const ModeInfo& b) {
        if (a.category != b.category) {
            return a.category < b.category;
        }
        return a.name < b.name;
    });

    return result;
}

std::vector<ModeInfo> ModeRegistry::GetModesByCategory(const std::string& category) const {
    std::vector<ModeInfo> result;

    for (const auto& [id, info] : m_modes) {
        if (info.category == category) {
            result.push_back(info);
        }
    }

    std::sort(result.begin(), result.end(), [](const ModeInfo& a, const ModeInfo& b) {
        return a.name < b.name;
    });

    return result;
}

std::vector<ModeInfo> ModeRegistry::GetBuiltInModes() const {
    std::vector<ModeInfo> result;

    for (const auto& [id, info] : m_modes) {
        if (info.isBuiltIn) {
            result.push_back(info);
        }
    }

    return result;
}

std::vector<ModeInfo> ModeRegistry::GetCustomModes() const {
    std::vector<ModeInfo> result;

    for (const auto& [id, info] : m_modes) {
        if (info.isCustom) {
            result.push_back(info);
        }
    }

    return result;
}

std::vector<std::string> ModeRegistry::GetCategories() const {
    std::vector<std::string> result;

    for (const auto& [id, info] : m_modes) {
        if (std::find(result.begin(), result.end(), info.category) == result.end()) {
            result.push_back(info.category);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<ModeInfo> ModeRegistry::FilterModes(int playerCount) const {
    std::vector<ModeInfo> result;

    for (const auto& [id, info] : m_modes) {
        if (playerCount >= info.minPlayers && playerCount <= info.maxPlayers) {
            result.push_back(info);
        }
    }

    return result;
}

std::vector<ModeInfo> ModeRegistry::SearchModes(const std::string& query) const {
    std::vector<ModeInfo> result;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& [id, info] : m_modes) {
        std::string lowerName = info.name;
        std::string lowerDesc = info.description;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerDesc.find(lowerQuery) != std::string::npos) {
            result.push_back(info);
        }
    }

    return result;
}

bool ModeRegistry::ValidateMode(const std::string& id, std::vector<std::string>& errors) const {
    auto mode = CreateMode(id);
    if (!mode) {
        errors.push_back("Failed to create mode instance");
        return false;
    }

    return mode->Validate(errors);
}

bool ModeRegistry::LoadCustomMode(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;

        ModeInfo info;
        info.id = j.value("id", "");
        info.name = j.value("name", "Unnamed Mode");
        info.description = j.value("description", "");
        info.category = j.value("category", "Custom");
        info.iconPath = j.value("icon", "");
        info.minPlayers = j.value("minPlayers", 2);
        info.maxPlayers = j.value("maxPlayers", 8);
        info.isBuiltIn = false;
        info.isCustom = true;
        info.author = j.value("author", "Unknown");
        info.version = j.value("version", "1.0");

        if (info.id.empty()) {
            return false;
        }

        // Create factory that loads settings from file
        std::string modeData = j.dump();
        info.factory = [modeData]() -> std::unique_ptr<GameMode> {
            auto mode = std::make_unique<MeleeMode>();  // Use base mode
            mode->Deserialize(modeData);
            return mode;
        };

        RegisterMode(info.id, info);
        return true;

    } catch (...) {
        return false;
    }
}

bool ModeRegistry::SaveCustomMode(const GameMode& mode, const std::string& filepath) {
    try {
        std::string data = mode.Serialize();

        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << data;
        return true;

    } catch (...) {
        return false;
    }
}

void ModeRegistry::RegisterBuiltInModes() {
    // Melee Mode
    {
        ModeInfo info;
        info.id = "melee";
        info.name = "Melee";
        info.description = "Classic RTS battle - destroy all enemy buildings and units to win";
        info.category = "Standard";
        info.iconPath = "icons/mode_melee.png";
        info.minPlayers = 2;
        info.maxPlayers = 12;
        info.isBuiltIn = true;
        info.isCustom = false;
        info.author = "Vehement";
        info.version = "1.0";
        info.factory = []() { return std::make_unique<MeleeMode>(); };
        RegisterMode(info.id, info);
    }

    // Free For All
    {
        ModeInfo info;
        info.id = "ffa";
        info.name = "Free For All";
        info.description = "Every player for themselves - last player standing wins";
        info.category = "Standard";
        info.iconPath = "icons/mode_ffa.png";
        info.minPlayers = 3;
        info.maxPlayers = 8;
        info.isBuiltIn = true;
        info.isCustom = false;
        info.author = "Vehement";
        info.version = "1.0";
        info.factory = []() { return std::make_unique<FreeForAllMode>(); };
        RegisterMode(info.id, info);
    }

    // Capture The Flag
    {
        ModeInfo info;
        info.id = "ctf";
        info.name = "Capture The Flag";
        info.description = "Capture enemy flags and return them to your base to score";
        info.category = "Objective";
        info.iconPath = "icons/mode_ctf.png";
        info.minPlayers = 4;
        info.maxPlayers = 12;
        info.isBuiltIn = true;
        info.isCustom = false;
        info.author = "Vehement";
        info.version = "1.0";
        info.factory = []() { return std::make_unique<CaptureTheFlagMode>(); };
        RegisterMode(info.id, info);
    }

    // King of the Hill
    {
        ModeInfo info;
        info.id = "koth";
        info.name = "King of the Hill";
        info.description = "Control the central point to accumulate victory points";
        info.category = "Objective";
        info.iconPath = "icons/mode_koth.png";
        info.minPlayers = 2;
        info.maxPlayers = 8;
        info.isBuiltIn = true;
        info.isCustom = false;
        info.author = "Vehement";
        info.version = "1.0";
        info.factory = []() { return std::make_unique<KingOfTheHillMode>(); };
        RegisterMode(info.id, info);
    }

    // Survival
    {
        ModeInfo info;
        info.id = "survival";
        info.name = "Survival";
        info.description = "Work together to survive endless waves of enemies";
        info.category = "Cooperative";
        info.iconPath = "icons/mode_survival.png";
        info.minPlayers = 1;
        info.maxPlayers = 4;
        info.isBuiltIn = true;
        info.isCustom = false;
        info.author = "Vehement";
        info.version = "1.0";
        info.factory = []() { return std::make_unique<SurvivalMode>(); };
        RegisterMode(info.id, info);
    }

    // Tower Defense
    {
        ModeInfo info;
        info.id = "tower_defense";
        info.name = "Tower Defense";
        info.description = "Build towers to defend against waves of creeps";
        info.category = "Cooperative";
        info.iconPath = "icons/mode_td.png";
        info.minPlayers = 1;
        info.maxPlayers = 4;
        info.isBuiltIn = true;
        info.isCustom = false;
        info.author = "Vehement";
        info.version = "1.0";
        info.factory = []() { return std::make_unique<TowerDefenseMode>(); };
        RegisterMode(info.id, info);
    }
}

ModeInfo ModeRegistry::CreateModeInfoFromFile(const std::string& filepath) {
    ModeInfo info;

    try {
        std::ifstream file(filepath);
        if (file.is_open()) {
            nlohmann::json j;
            file >> j;

            info.id = j.value("id", "");
            info.name = j.value("name", "Unknown");
            info.description = j.value("description", "");
            info.category = j.value("category", "Custom");
            info.minPlayers = j.value("minPlayers", 2);
            info.maxPlayers = j.value("maxPlayers", 8);
            info.isBuiltIn = false;
            info.isCustom = true;
            info.author = j.value("author", "Unknown");
            info.version = j.value("version", "1.0");
        }
    } catch (...) {
        // Return default info on error
    }

    return info;
}

} // namespace Vehement
