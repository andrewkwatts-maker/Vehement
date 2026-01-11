#include "ModManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <regex>

namespace Nova {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Mod Implementation
// ============================================================================

Mod::Mod(const ModInfo& info, const std::string& path)
    : m_info(info), m_path(path) {}

Mod::~Mod() {
    Unload();
}

bool Mod::Load() {
    if (m_status == ModStatus::Loaded) return true;

    m_status = ModStatus::Loading;

    // Load manifest
    if (!LoadManifest()) {
        m_status = ModStatus::Error;
        return false;
    }

    // Validate dependencies
    if (!ValidateDependencies()) {
        m_status = ModStatus::MissingDependency;
        return false;
    }

    // Load assets
    if (!LoadAssets()) {
        m_status = ModStatus::Error;
        return false;
    }

    // Execute init script if exists
    if (!ExecuteInitScript()) {
        m_status = ModStatus::Error;
        return false;
    }

    m_status = ModStatus::Loaded;
    return true;
}

void Mod::Unload() {
    m_assetOverrides.clear();
    m_loadedScripts.clear();
    m_status = ModStatus::NotLoaded;
}

bool Mod::Reload() {
    Unload();
    return Load();
}

std::string Mod::GetAssetPath(const std::string& relativePath) const {
    return m_path + "/" + relativePath;
}

std::vector<std::string> Mod::GetAssets(const std::string& extension) const {
    std::vector<std::string> assets;

    if (!fs::exists(m_path)) return assets;

    for (const auto& entry : fs::recursive_directory_iterator(m_path)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            if (extension.empty() || entry.path().extension() == extension) {
                // Convert to relative path
                std::string relativePath = path.substr(m_path.length() + 1);
                assets.push_back(relativePath);
            }
        }
    }

    return assets;
}

bool Mod::HasAsset(const std::string& relativePath) const {
    return fs::exists(m_path + "/" + relativePath);
}

std::string Mod::ReadTextFile(const std::string& relativePath) const {
    std::string fullPath = m_path + "/" + relativePath;
    std::ifstream file(fullPath);
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<uint8_t> Mod::ReadBinaryFile(const std::string& relativePath) const {
    std::string fullPath = m_path + "/" + relativePath;
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) return {};

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::vector<std::string> Mod::GetScripts() const {
    return GetAssets(".py");
}

bool Mod::ExecuteScript(const std::string& scriptPath) {
    // Script execution would be handled by PythonEngine
    // For now, just record that we tried to execute it
    m_loadedScripts.push_back(scriptPath);
    return true;
}

ValidationResult Mod::Validate() const {
    ValidationResult result;

    // Check manifest exists
    if (!fs::exists(m_path + "/mod.json")) {
        result.AddError("", "Missing mod.json manifest");
    }

    // Validate JSON configs
    auto& registry = SchemaRegistry::Instance();
    for (const auto& entry : fs::recursive_directory_iterator(m_path)) {
        if (entry.path().extension() == ".json" &&
            entry.path().filename() != "mod.json") {
            std::ifstream file(entry.path());
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());

                std::string type = registry.DetectType(content);
                if (!type.empty()) {
                    auto valResult = registry.Validate(type, content);
                    if (!valResult.valid) {
                        for (const auto& error : valResult.errors) {
                            result.AddError(entry.path().string(), error.message);
                        }
                    }
                }
            }
        }
    }

    return result;
}

bool Mod::LoadManifest() {
    std::string manifestPath = m_path + "/mod.json";
    if (!fs::exists(manifestPath)) {
        m_errorMessage = "Missing mod.json";
        return false;
    }

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        m_errorMessage = "Cannot open mod.json";
        return false;
    }

    try {
        json j = json::parse(file);

        if (j.contains("id")) m_info.id = j["id"];
        if (j.contains("name")) m_info.name = j["name"];
        if (j.contains("version")) m_info.version = j["version"];
        if (j.contains("description")) m_info.description = j["description"];
        if (j.contains("author")) m_info.author = j["author"];
        if (j.contains("website")) m_info.website = j["website"];
        if (j.contains("license")) m_info.license = j["license"];
        if (j.contains("category")) m_info.category = j["category"];

        if (j.contains("tags") && j["tags"].is_array()) {
            m_info.tags.clear();
            for (const auto& tag : j["tags"]) {
                m_info.tags.push_back(tag);
            }
        }

        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            m_info.dependencies.clear();
            for (const auto& dep : j["dependencies"]) {
                ModDependency d;
                if (dep.contains("modId")) d.modId = dep["modId"];
                if (dep.contains("minVersion")) d.minVersion = dep["minVersion"];
                if (dep.contains("maxVersion")) d.maxVersion = dep["maxVersion"];
                if (dep.contains("optional")) d.optional = dep["optional"];
                m_info.dependencies.push_back(d);
            }
        }

        if (j.contains("engineMinVersion")) m_info.engineMinVersion = j["engineMinVersion"];
        if (j.contains("engineMaxVersion")) m_info.engineMaxVersion = j["engineMaxVersion"];

        if (j.contains("conflicts") && j["conflicts"].is_array()) {
            m_info.conflicts.clear();
            for (const auto& c : j["conflicts"]) {
                m_info.conflicts.push_back(c);
            }
        }

    } catch (const json::exception& e) {
        m_errorMessage = std::string("JSON parse error: ") + e.what();
        return false;
    }

    return true;
}

bool Mod::ValidateDependencies() {
    auto& manager = ModManager::Instance();

    for (const auto& dep : m_info.dependencies) {
        if (dep.optional) continue;

        if (!manager.IsModLoaded(dep.modId)) {
            m_errorMessage = "Missing dependency: " + dep.modId;
            return false;
        }
    }

    return true;
}

bool Mod::LoadAssets() {
    // Scan for assets that override base game assets
    std::string assetsPath = m_path + "/assets";
    if (!fs::exists(assetsPath)) return true;

    for (const auto& entry : fs::recursive_directory_iterator(assetsPath)) {
        if (entry.is_regular_file()) {
            std::string fullPath = entry.path().string();
            std::string relativePath = fullPath.substr(assetsPath.length() + 1);

            // Normalize path separators
            std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

            m_assetOverrides[relativePath] = fullPath;
        }
    }

    return true;
}

bool Mod::ExecuteInitScript() {
    std::string initScript = m_path + "/scripts/init.py";
    if (fs::exists(initScript)) {
        return ExecuteScript("scripts/init.py");
    }
    return true;
}

// ============================================================================
// ModManager Implementation
// ============================================================================

ModManager& ModManager::Instance() {
    static ModManager instance;
    return instance;
}

void ModManager::SetModsDirectory(const std::string& path) {
    m_modsDirectory = path;
}

void ModManager::SetWorkshopDirectory(const std::string& path) {
    m_workshopDirectory = path;
}

void ModManager::ScanForMods() {
    m_availableMods.clear();

    // Scan mods directory
    if (fs::exists(m_modsDirectory)) {
        for (const auto& entry : fs::directory_iterator(m_modsDirectory)) {
            if (entry.is_directory()) {
                std::string manifestPath = entry.path().string() + "/mod.json";
                if (fs::exists(manifestPath)) {
                    ModInfo info = ParseManifest(manifestPath);
                    if (!info.id.empty()) {
                        m_availableMods[info.id] = info;
                    }
                }
            }
        }
    }

    // Scan workshop directory
    if (fs::exists(m_workshopDirectory)) {
        for (const auto& entry : fs::directory_iterator(m_workshopDirectory)) {
            if (entry.is_directory()) {
                std::string manifestPath = entry.path().string() + "/mod.json";
                if (fs::exists(manifestPath)) {
                    ModInfo info = ParseManifest(manifestPath);
                    if (!info.id.empty() && m_availableMods.find(info.id) == m_availableMods.end()) {
                        m_availableMods[info.id] = info;
                    }
                }
            }
        }
    }
}

std::vector<ModInfo> ModManager::GetAvailableMods() const {
    std::vector<ModInfo> mods;
    for (const auto& [_, info] : m_availableMods) {
        mods.push_back(info);
    }
    return mods;
}

std::optional<ModInfo> ModManager::GetModInfo(const std::string& modId) const {
    auto it = m_availableMods.find(modId);
    if (it != m_availableMods.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ModManager::IsModAvailable(const std::string& modId) const {
    return m_availableMods.find(modId) != m_availableMods.end();
}

bool ModManager::LoadAllMods(ProgressCallback progress) {
    AutoSortLoadOrder();

    int total = static_cast<int>(m_loadOrder.size());
    int current = 0;
    bool allSuccess = true;

    for (const auto& modId : m_loadOrder) {
        if (!IsModEnabled(modId)) {
            current++;
            continue;
        }

        if (progress) {
            float p = total > 0 ? static_cast<float>(current) / total : 0.0f;
            progress(p, "Loading " + modId + "...");
        }

        if (!LoadMod(modId)) {
            allSuccess = false;
        }

        current++;
    }

    if (progress) {
        progress(1.0f, "Done");
    }

    return allSuccess;
}

bool ModManager::LoadMod(const std::string& modId) {
    if (m_loadedMods.find(modId) != m_loadedMods.end()) {
        return true;  // Already loaded
    }

    auto infoIt = m_availableMods.find(modId);
    if (infoIt == m_availableMods.end()) {
        return false;  // Not found
    }

    // Determine mod path
    std::string modPath;
    if (fs::exists(m_modsDirectory + "/" + modId)) {
        modPath = m_modsDirectory + "/" + modId;
    } else if (fs::exists(m_workshopDirectory + "/" + modId)) {
        modPath = m_workshopDirectory + "/" + modId;
    } else {
        return false;
    }

    auto mod = std::make_shared<Mod>(infoIt->second, modPath);
    ModStatus previousStatus = mod->GetStatus();

    if (mod->Load()) {
        m_loadedMods[modId] = mod;

        // Register asset overrides
        for (const auto& asset : mod->GetAssets()) {
            std::string fullPath = mod->GetAssetPath(asset);
            if (fs::exists(fullPath)) {
                m_assetOverrides[asset] = {modId, fullPath};
            }
        }

        if (m_onModLoaded) {
            m_onModLoaded({mod, previousStatus, ModStatus::Loaded, "Loaded successfully"});
        }
        return true;
    } else {
        if (m_onModError) {
            m_onModError({mod, previousStatus, mod->GetStatus(), mod->GetErrorMessage()});
        }
        return false;
    }
}

void ModManager::UnloadMod(const std::string& modId) {
    auto it = m_loadedMods.find(modId);
    if (it == m_loadedMods.end()) return;

    auto mod = it->second;
    ModStatus previousStatus = mod->GetStatus();

    // Remove asset overrides from this mod
    for (auto assetIt = m_assetOverrides.begin(); assetIt != m_assetOverrides.end();) {
        if (assetIt->second.first == modId) {
            assetIt = m_assetOverrides.erase(assetIt);
        } else {
            ++assetIt;
        }
    }

    mod->Unload();
    m_loadedMods.erase(it);

    if (m_onModUnloaded) {
        m_onModUnloaded({mod, previousStatus, ModStatus::NotLoaded, "Unloaded"});
    }
}

void ModManager::UnloadAllMods() {
    // Unload in reverse order
    std::vector<std::string> toUnload;
    for (const auto& [id, _] : m_loadedMods) {
        toUnload.push_back(id);
    }

    for (auto it = toUnload.rbegin(); it != toUnload.rend(); ++it) {
        UnloadMod(*it);
    }
}

bool ModManager::ReloadMod(const std::string& modId) {
    UnloadMod(modId);
    return LoadMod(modId);
}

bool ModManager::ReloadAllMods() {
    UnloadAllMods();
    return LoadAllMods();
}

std::shared_ptr<Mod> ModManager::GetMod(const std::string& modId) const {
    auto it = m_loadedMods.find(modId);
    return it != m_loadedMods.end() ? it->second : nullptr;
}

std::vector<std::shared_ptr<Mod>> ModManager::GetLoadedMods() const {
    std::vector<std::shared_ptr<Mod>> mods;
    for (const auto& [_, mod] : m_loadedMods) {
        mods.push_back(mod);
    }
    return mods;
}

bool ModManager::IsModLoaded(const std::string& modId) const {
    return m_loadedMods.find(modId) != m_loadedMods.end();
}

void ModManager::EnableMod(const std::string& modId) {
    m_enabledMods[modId] = true;
}

void ModManager::DisableMod(const std::string& modId) {
    m_enabledMods[modId] = false;
    UnloadMod(modId);
}

bool ModManager::IsModEnabled(const std::string& modId) const {
    auto it = m_enabledMods.find(modId);
    return it != m_enabledMods.end() ? it->second : false;
}

std::vector<std::string> ModManager::GetEnabledMods() const {
    std::vector<std::string> enabled;
    for (const auto& [id, isEnabled] : m_enabledMods) {
        if (isEnabled) enabled.push_back(id);
    }
    return enabled;
}

void ModManager::SetLoadOrder(const std::vector<std::string>& order) {
    m_loadOrder = order;

    // Update load order in mod info
    for (size_t i = 0; i < m_loadOrder.size(); ++i) {
        auto it = m_availableMods.find(m_loadOrder[i]);
        if (it != m_availableMods.end()) {
            it->second.loadOrder = static_cast<int>(i);
        }
    }
}

std::vector<std::string> ModManager::GetLoadOrder() const {
    return m_loadOrder;
}

void ModManager::MoveModUp(const std::string& modId) {
    for (size_t i = 1; i < m_loadOrder.size(); ++i) {
        if (m_loadOrder[i] == modId) {
            std::swap(m_loadOrder[i], m_loadOrder[i - 1]);
            break;
        }
    }
}

void ModManager::MoveModDown(const std::string& modId) {
    for (size_t i = 0; i < m_loadOrder.size() - 1; ++i) {
        if (m_loadOrder[i] == modId) {
            std::swap(m_loadOrder[i], m_loadOrder[i + 1]);
            break;
        }
    }
}

void ModManager::AutoSortLoadOrder() {
    std::vector<std::string> modIds;
    for (const auto& [id, _] : m_availableMods) {
        modIds.push_back(id);
    }

    m_loadOrder = TopologicalSort(modIds);
}

bool ModManager::AreDependenciesMet(const std::string& modId) const {
    auto it = m_availableMods.find(modId);
    if (it == m_availableMods.end()) return false;

    for (const auto& dep : it->second.dependencies) {
        if (dep.optional) continue;
        if (!IsModAvailable(dep.modId)) return false;
    }

    return true;
}

std::vector<ModDependency> ModManager::GetMissingDependencies(const std::string& modId) const {
    std::vector<ModDependency> missing;

    auto it = m_availableMods.find(modId);
    if (it == m_availableMods.end()) return missing;

    for (const auto& dep : it->second.dependencies) {
        if (!IsModAvailable(dep.modId)) {
            missing.push_back(dep);
        }
    }

    return missing;
}

std::vector<std::pair<std::string, std::string>> ModManager::GetConflicts() const {
    std::vector<std::pair<std::string, std::string>> conflicts;

    for (const auto& [id, info] : m_availableMods) {
        if (!IsModEnabled(id)) continue;

        for (const auto& conflictId : info.conflicts) {
            if (IsModEnabled(conflictId)) {
                // Avoid duplicates
                bool found = false;
                for (const auto& [a, b] : conflicts) {
                    if ((a == id && b == conflictId) || (a == conflictId && b == id)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    conflicts.emplace_back(id, conflictId);
                }
            }
        }
    }

    return conflicts;
}

std::string ModManager::ResolveAssetPath(const std::string& path) const {
    auto it = m_assetOverrides.find(path);
    if (it != m_assetOverrides.end()) {
        return it->second.second;
    }
    return path;  // Return original path if not overridden
}

std::unordered_map<std::string, std::string> ModManager::GetAssetOverrides() const {
    std::unordered_map<std::string, std::string> overrides;
    for (const auto& [path, override] : m_assetOverrides) {
        overrides[path] = override.second;
    }
    return overrides;
}

bool ModManager::IsAssetOverridden(const std::string& path) const {
    return m_assetOverrides.find(path) != m_assetOverrides.end();
}

std::string ModManager::GetAssetOverridingMod(const std::string& path) const {
    auto it = m_assetOverrides.find(path);
    return it != m_assetOverrides.end() ? it->second.first : "";
}

bool ModManager::CreateModTemplate(const std::string& modId, const ModInfo& info) {
    std::string modPath = m_modsDirectory + "/" + modId;

    // Create directory structure
    fs::create_directories(modPath);
    fs::create_directories(modPath + "/assets");
    fs::create_directories(modPath + "/assets/textures");
    fs::create_directories(modPath + "/assets/models");
    fs::create_directories(modPath + "/assets/sounds");
    fs::create_directories(modPath + "/configs");
    fs::create_directories(modPath + "/configs/units");
    fs::create_directories(modPath + "/configs/buildings");
    fs::create_directories(modPath + "/scripts");

    // Create mod.json
    json j;
    j["id"] = modId;
    j["name"] = info.name;
    j["version"] = info.version.empty() ? "1.0.0" : info.version;
    j["description"] = info.description;
    j["author"] = info.author;
    j["website"] = info.website;
    j["license"] = info.license.empty() ? "MIT" : info.license;
    j["category"] = info.category;
    j["tags"] = info.tags;

    json deps = json::array();
    for (const auto& dep : info.dependencies) {
        deps.push_back({
            {"modId", dep.modId},
            {"minVersion", dep.minVersion},
            {"optional", dep.optional}
        });
    }
    j["dependencies"] = deps;

    j["engineMinVersion"] = info.engineMinVersion;
    j["conflicts"] = info.conflicts;

    std::ofstream manifestFile(modPath + "/mod.json");
    if (!manifestFile.is_open()) return false;
    manifestFile << j.dump(2);

    // Create init.py template
    std::ofstream initScript(modPath + "/scripts/init.py");
    initScript << "# " << info.name << " - Initialization Script\n";
    initScript << "# This script runs when the mod is loaded\n\n";
    initScript << "def on_load(context):\n";
    initScript << "    \"\"\"Called when mod is loaded.\"\"\"\n";
    initScript << "    print(f\"" << info.name << " loaded!\")\n\n";
    initScript << "def on_unload(context):\n";
    initScript << "    \"\"\"Called when mod is unloaded.\"\"\"\n";
    initScript << "    print(f\"" << info.name << " unloaded!\")\n";

    // Create README
    std::ofstream readme(modPath + "/README.md");
    readme << "# " << info.name << "\n\n";
    readme << info.description << "\n\n";
    readme << "## Author\n" << info.author << "\n\n";
    readme << "## Version\n" << j["version"].get<std::string>() << "\n\n";
    readme << "## Installation\n";
    readme << "Copy this folder to your game's mods directory.\n";

    // Rescan to pick up new mod
    ScanForMods();

    return true;
}

bool ModManager::ExportMod(const std::string& modId, const std::string& outputPath) {
    // ZIP export would require additional library (minizip, etc.)
    // For now, just copy directory
    std::string modPath = m_modsDirectory + "/" + modId;
    if (!fs::exists(modPath)) return false;

    try {
        fs::copy(modPath, outputPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool ModManager::ImportMod(const std::string& zipPath) {
    // ZIP import would require additional library
    // For now, assume it's a directory
    if (!fs::is_directory(zipPath)) return false;

    std::string manifestPath = zipPath + "/mod.json";
    if (!fs::exists(manifestPath)) return false;

    ModInfo info = ParseManifest(manifestPath);
    if (info.id.empty()) return false;

    std::string targetPath = m_modsDirectory + "/" + info.id;

    try {
        fs::copy(zipPath, targetPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        ScanForMods();
        return true;
    } catch (...) {
        return false;
    }
}

ValidationResult ModManager::ValidateMod(const std::string& modId) const {
    auto mod = GetMod(modId);
    if (mod) {
        return mod->Validate();
    }

    ValidationResult result;
    result.AddError("", "Mod not loaded: " + modId);
    return result;
}

std::unordered_map<std::string, ValidationResult> ModManager::ValidateAllMods() const {
    std::unordered_map<std::string, ValidationResult> results;
    for (const auto& [id, mod] : m_loadedMods) {
        results[id] = mod->Validate();
    }
    return results;
}

bool ModManager::SaveConfiguration(const std::string& path) {
    std::string configPath = path.empty() ? m_configPath : path;

    json j;
    j["loadOrder"] = m_loadOrder;

    json enabled = json::object();
    for (const auto& [id, isEnabled] : m_enabledMods) {
        enabled[id] = isEnabled;
    }
    j["enabled"] = enabled;

    std::ofstream file(configPath);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

bool ModManager::LoadConfiguration(const std::string& path) {
    std::string configPath = path.empty() ? m_configPath : path;

    std::ifstream file(configPath);
    if (!file.is_open()) return false;

    try {
        json j = json::parse(file);

        if (j.contains("loadOrder") && j["loadOrder"].is_array()) {
            m_loadOrder.clear();
            for (const auto& id : j["loadOrder"]) {
                m_loadOrder.push_back(id);
            }
        }

        if (j.contains("enabled") && j["enabled"].is_object()) {
            for (auto& [id, isEnabled] : j["enabled"].items()) {
                m_enabledMods[id] = isEnabled;
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

ModInfo ModManager::ParseManifest(const std::string& manifestPath) {
    ModInfo info;

    std::ifstream file(manifestPath);
    if (!file.is_open()) return info;

    try {
        json j = json::parse(file);

        if (j.contains("id")) info.id = j["id"];
        if (j.contains("name")) info.name = j["name"];
        if (j.contains("version")) info.version = j["version"];
        if (j.contains("description")) info.description = j["description"];
        if (j.contains("author")) info.author = j["author"];
        if (j.contains("website")) info.website = j["website"];
        if (j.contains("license")) info.license = j["license"];
        if (j.contains("category")) info.category = j["category"];
        if (j.contains("iconPath")) info.iconPath = j["iconPath"];
        if (j.contains("bannerPath")) info.bannerPath = j["bannerPath"];

        if (j.contains("tags") && j["tags"].is_array()) {
            for (const auto& tag : j["tags"]) {
                info.tags.push_back(tag);
            }
        }

        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            for (const auto& dep : j["dependencies"]) {
                ModDependency d;
                if (dep.contains("modId")) d.modId = dep["modId"];
                if (dep.contains("minVersion")) d.minVersion = dep["minVersion"];
                if (dep.contains("maxVersion")) d.maxVersion = dep["maxVersion"];
                if (dep.contains("optional")) d.optional = dep["optional"];
                info.dependencies.push_back(d);
            }
        }

        if (j.contains("engineMinVersion")) info.engineMinVersion = j["engineMinVersion"];
        if (j.contains("engineMaxVersion")) info.engineMaxVersion = j["engineMaxVersion"];

        if (j.contains("conflicts") && j["conflicts"].is_array()) {
            for (const auto& c : j["conflicts"]) {
                info.conflicts.push_back(c);
            }
        }

        if (j.contains("workshopId")) info.workshopId = j["workshopId"];

    } catch (...) {}

    return info;
}

bool ModManager::CompareVersions(const std::string& version1, const std::string& version2) const {
    // Simple semantic version comparison
    std::regex versionRegex(R"((\d+)\.(\d+)\.(\d+))");
    std::smatch match1, match2;

    if (!std::regex_match(version1, match1, versionRegex) ||
        !std::regex_match(version2, match2, versionRegex)) {
        return version1 < version2;  // Fallback to string comparison
    }

    for (int i = 1; i <= 3; ++i) {
        int v1 = std::stoi(match1[i].str());
        int v2 = std::stoi(match2[i].str());
        if (v1 < v2) return true;
        if (v1 > v2) return false;
    }

    return false;  // Equal
}

std::vector<std::string> ModManager::TopologicalSort(const std::vector<std::string>& modIds) const {
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> graph;

    // Initialize
    for (const auto& id : modIds) {
        inDegree[id] = 0;
        graph[id] = {};
    }

    // Build dependency graph
    for (const auto& id : modIds) {
        auto it = m_availableMods.find(id);
        if (it == m_availableMods.end()) continue;

        for (const auto& dep : it->second.dependencies) {
            if (std::find(modIds.begin(), modIds.end(), dep.modId) != modIds.end()) {
                graph[dep.modId].push_back(id);
                inDegree[id]++;
            }
        }
    }

    // Kahn's algorithm
    std::vector<std::string> result;
    std::vector<std::string> queue;

    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) queue.push_back(id);
    }

    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();
        result.push_back(current);

        for (const auto& neighbor : graph[current]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                queue.push_back(neighbor);
            }
        }
    }

    // If result size != modIds size, there's a cycle
    if (result.size() != modIds.size()) {
        // Return original order
        return modIds;
    }

    return result;
}

// ============================================================================
// ModCreator Implementation
// ============================================================================

ModCreator::ModCreator(const std::string& modId) {
    m_info.id = modId;
    m_info.version = "1.0.0";
}

ModCreator& ModCreator::Name(const std::string& name) {
    m_info.name = name;
    return *this;
}

ModCreator& ModCreator::Description(const std::string& desc) {
    m_info.description = desc;
    return *this;
}

ModCreator& ModCreator::Author(const std::string& author) {
    m_info.author = author;
    return *this;
}

ModCreator& ModCreator::Version(const std::string& version) {
    m_info.version = version;
    return *this;
}

ModCreator& ModCreator::Website(const std::string& url) {
    m_info.website = url;
    return *this;
}

ModCreator& ModCreator::License(const std::string& license) {
    m_info.license = license;
    return *this;
}

ModCreator& ModCreator::Tag(const std::string& tag) {
    m_info.tags.push_back(tag);
    return *this;
}

ModCreator& ModCreator::Category(const std::string& category) {
    m_info.category = category;
    return *this;
}

ModCreator& ModCreator::Dependency(const std::string& modId, const std::string& minVersion) {
    ModDependency dep;
    dep.modId = modId;
    dep.minVersion = minVersion;
    dep.optional = false;
    m_info.dependencies.push_back(dep);
    return *this;
}

ModCreator& ModCreator::OptionalDependency(const std::string& modId) {
    ModDependency dep;
    dep.modId = modId;
    dep.optional = true;
    m_info.dependencies.push_back(dep);
    return *this;
}

ModCreator& ModCreator::Conflicts(const std::string& modId) {
    m_info.conflicts.push_back(modId);
    return *this;
}

ModCreator& ModCreator::MinEngineVersion(const std::string& version) {
    m_info.engineMinVersion = version;
    return *this;
}

ModCreator& ModCreator::MaxEngineVersion(const std::string& version) {
    m_info.engineMaxVersion = version;
    return *this;
}

bool ModCreator::Create(const std::string& outputPath) {
    std::string path = outputPath.empty()
        ? ModManager::Instance().GetModsDirectory() + "/" + m_info.id
        : outputPath;

    return ModManager::Instance().CreateModTemplate(m_info.id, m_info);
}

// ============================================================================
// LocalWorkshop Implementation
// ============================================================================

LocalWorkshop::LocalWorkshop(const std::string& workshopPath)
    : m_workshopPath(workshopPath) {
    fs::create_directories(workshopPath);
}

std::vector<ModInfo> LocalWorkshop::SearchMods(const std::string& query, int page, int pageSize) {
    std::vector<ModInfo> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    if (!fs::exists(m_workshopPath)) return results;

    int skip = page * pageSize;
    int count = 0;

    for (const auto& entry : fs::directory_iterator(m_workshopPath)) {
        if (!entry.is_directory()) continue;

        std::string manifestPath = entry.path().string() + "/mod.json";
        if (!fs::exists(manifestPath)) continue;

        ModInfo info;
        std::ifstream file(manifestPath);
        if (!file.is_open()) continue;

        try {
            json j = json::parse(file);
            if (j.contains("name")) info.name = j["name"];
            if (j.contains("description")) info.description = j["description"];
            if (j.contains("id")) info.id = j["id"];

            std::string lowerName = info.name;
            std::string lowerDesc = info.description;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

            if (lowerName.find(lowerQuery) != std::string::npos ||
                lowerDesc.find(lowerQuery) != std::string::npos) {
                if (count >= skip && count < skip + pageSize) {
                    results.push_back(info);
                }
                count++;
            }
        } catch (...) {}
    }

    return results;
}

std::vector<ModInfo> LocalWorkshop::GetPopularMods(int page, int pageSize) {
    return SearchMods("", page, pageSize);
}

std::vector<ModInfo> LocalWorkshop::GetRecentMods(int page, int pageSize) {
    return SearchMods("", page, pageSize);
}

bool LocalWorkshop::Subscribe(const std::string& workshopId) {
    if (!IsSubscribed(workshopId)) {
        m_subscribed.push_back(workshopId);
    }
    return true;
}

bool LocalWorkshop::Unsubscribe(const std::string& workshopId) {
    m_subscribed.erase(
        std::remove(m_subscribed.begin(), m_subscribed.end(), workshopId),
        m_subscribed.end());
    return true;
}

std::vector<std::string> LocalWorkshop::GetSubscribedMods() {
    return m_subscribed;
}

bool LocalWorkshop::IsSubscribed(const std::string& workshopId) {
    return std::find(m_subscribed.begin(), m_subscribed.end(), workshopId) != m_subscribed.end();
}

bool LocalWorkshop::DownloadMod(const std::string& workshopId, const std::string& targetPath) {
    std::string sourcePath = m_workshopPath + "/" + workshopId;
    if (!fs::exists(sourcePath)) return false;

    try {
        fs::copy(sourcePath, targetPath, fs::copy_options::recursive);
        return true;
    } catch (...) {
        return false;
    }
}

float LocalWorkshop::GetDownloadProgress(const std::string& workshopId) {
    return 1.0f;  // Instant for local
}

bool LocalWorkshop::UploadMod(const std::string& modPath, ModInfo& info) {
    if (!fs::exists(modPath)) return false;

    std::string targetPath = m_workshopPath + "/" + info.id;
    try {
        fs::copy(modPath, targetPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        info.workshopId = info.id;
        return true;
    } catch (...) {
        return false;
    }
}

bool LocalWorkshop::UpdateMod(const std::string& workshopId, const std::string& modPath, const std::string& changeNotes) {
    std::string targetPath = m_workshopPath + "/" + workshopId;
    try {
        fs::remove_all(targetPath);
        fs::copy(modPath, targetPath, fs::copy_options::recursive);
        return true;
    } catch (...) {
        return false;
    }
}

bool LocalWorkshop::RateMod(const std::string& workshopId, int rating) {
    // Store ratings in a simple file for local workshop
    return true;
}

int LocalWorkshop::GetUserRating(const std::string& workshopId) {
    return 0;
}

} // namespace Nova
