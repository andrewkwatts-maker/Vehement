#include "ServerConfig.hpp"
#include "../engine/persistence/WorldDatabase.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Nova {

bool ServerConfig::LoadFromDatabase(WorldDatabase* db) {
    if (!db || !db->IsInitialized()) return false;

    // Implementation would query ServerConfig table
    // Simplified for this example
    return true;
}

bool ServerConfig::SaveToDatabase(WorldDatabase* db) {
    if (!db || !db->IsInitialized()) return false;

    // Implementation would insert/update ServerConfig table
    return true;
}

bool ServerConfig::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    try {
        json config;
        file >> config;

        if (config.contains("server_name")) serverName = config["server_name"];
        if (config.contains("server_description")) serverDescription = config["server_description"];
        if (config.contains("server_version")) serverVersion = config["server_version"];
        if (config.contains("max_players")) maxPlayers = config["max_players"];
        if (config.contains("password_protected")) passwordProtected = config["password_protected"];
        if (config.contains("pvp_enabled")) pvpEnabled = config["pvp_enabled"];
        if (config.contains("pve_enabled")) pveEnabled = config["pve_enabled"];
        if (config.contains("friendly_fire")) friendlyFire = config["friendly_fire"];
        if (config.contains("difficulty")) difficulty = config["difficulty"];
        if (config.contains("hardcore_mode")) hardcoreMode = config["hardcore_mode"];
        if (config.contains("game_mode")) gameMode = config["game_mode"];
        if (config.contains("auto_save_enabled")) autoSaveEnabled = config["auto_save_enabled"];
        if (config.contains("auto_save_interval")) autoSaveInterval = config["auto_save_interval"];
        if (config.contains("backup_enabled")) backupEnabled = config["backup_enabled"];
        if (config.contains("backup_interval")) backupInterval = config["backup_interval"];
        if (config.contains("server_port")) serverPort = config["server_port"];

        if (config.contains("whitelist")) {
            whitelist = config["whitelist"].get<std::vector<std::string>>();
        }

        if (config.contains("blacklist")) {
            blacklist = config["blacklist"].get<std::vector<std::string>>();
        }

        if (config.contains("admins")) {
            admins = config["admins"].get<std::map<std::string, int>>();
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

bool ServerConfig::SaveToFile(const std::string& path) {
    json config;

    config["server_name"] = serverName;
    config["server_description"] = serverDescription;
    config["server_version"] = serverVersion;
    config["max_players"] = maxPlayers;
    config["password_protected"] = passwordProtected;
    config["pvp_enabled"] = pvpEnabled;
    config["pve_enabled"] = pveEnabled;
    config["friendly_fire"] = friendlyFire;
    config["difficulty"] = difficulty;
    config["hardcore_mode"] = hardcoreMode;
    config["game_mode"] = gameMode;
    config["auto_save_enabled"] = autoSaveEnabled;
    config["auto_save_interval"] = autoSaveInterval;
    config["backup_enabled"] = backupEnabled;
    config["backup_interval"] = backupInterval;
    config["server_port"] = serverPort;
    config["whitelist"] = whitelist;
    config["blacklist"] = blacklist;
    config["admins"] = admins;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << config.dump(4);
    return true;
}

void ServerConfig::ResetToDefaults() {
    *this = ServerConfig{};
}

bool ServerConfig::Validate(std::string& errorMessage) {
    if (serverName.empty()) {
        errorMessage = "Server name cannot be empty";
        return false;
    }

    if (maxPlayers < 1 || maxPlayers > 1000) {
        errorMessage = "Max players must be between 1 and 1000";
        return false;
    }

    if (autoSaveInterval < 10) {
        errorMessage = "Auto-save interval must be at least 10 seconds";
        return false;
    }

    if (serverPort < 1024 || serverPort > 65535) {
        errorMessage = "Server port must be between 1024 and 65535";
        return false;
    }

    return true;
}

std::string ServerConfig::GetCustomSetting(const std::string& key, const std::string& defaultValue) const {
    auto it = customSettings.find(key);
    return (it != customSettings.end()) ? it->second : defaultValue;
}

void ServerConfig::SetCustomSetting(const std::string& key, const std::string& value) {
    customSettings[key] = value;
}

bool ServerConfig::IsAdmin(const std::string& username) const {
    return admins.find(username) != admins.end();
}

int ServerConfig::GetAdminLevel(const std::string& username) const {
    auto it = admins.find(username);
    return (it != admins.end()) ? it->second : 0;
}

void ServerConfig::AddAdmin(const std::string& username, int permissionLevel) {
    admins[username] = permissionLevel;
}

void ServerConfig::RemoveAdmin(const std::string& username) {
    admins.erase(username);
}

bool ServerConfig::IsWhitelisted(const std::string& username) const {
    return std::find(whitelist.begin(), whitelist.end(), username) != whitelist.end();
}

bool ServerConfig::IsBlacklisted(const std::string& username) const {
    return std::find(blacklist.begin(), blacklist.end(), username) != blacklist.end();
}

void ServerConfig::AddToWhitelist(const std::string& username) {
    if (!IsWhitelisted(username)) {
        whitelist.push_back(username);
    }
}

void ServerConfig::RemoveFromWhitelist(const std::string& username) {
    whitelist.erase(std::remove(whitelist.begin(), whitelist.end(), username), whitelist.end());
}

void ServerConfig::AddToBlacklist(const std::string& username) {
    if (!IsBlacklisted(username)) {
        blacklist.push_back(username);
    }
}

void ServerConfig::RemoveFromBlacklist(const std::string& username) {
    blacklist.erase(std::remove(blacklist.begin(), blacklist.end(), username), blacklist.end());
}

} // namespace Nova
