#pragma once

#include "GameMode.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Vehement {

/**
 * @brief Mode registration info
 */
struct ModeInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    std::string iconPath;
    int minPlayers;
    int maxPlayers;
    bool isBuiltIn;
    bool isCustom;
    std::string author;
    std::string version;
    GameModeFactory factory;
};

/**
 * @brief Mode Registry - Central registry for all game modes
 *
 * Features:
 * - Register built-in and custom modes
 * - Discover modes from files
 * - Create mode instances
 * - Mode validation
 * - Mode categories
 */
class ModeRegistry {
public:
    static ModeRegistry& Instance();

    // Initialization
    void Initialize();
    void Shutdown();

    // Registration
    void RegisterMode(const std::string& id, const ModeInfo& info);
    void UnregisterMode(const std::string& id);
    bool IsModeRegistered(const std::string& id) const;

    // Factory registration
    template<typename T>
    void RegisterModeType(const std::string& id) {
        static_assert(std::is_base_of<GameMode, T>::value, "T must derive from GameMode");

        if (m_modes.find(id) != m_modes.end()) {
            m_modes[id].factory = []() { return std::make_unique<T>(); };
        }
    }

    // Mode creation
    std::unique_ptr<GameMode> CreateMode(const std::string& id) const;

    // Mode discovery
    void DiscoverCustomModes(const std::string& directory);
    void RefreshModeList();

    // Queries
    const ModeInfo* GetModeInfo(const std::string& id) const;
    std::vector<ModeInfo> GetAllModes() const;
    std::vector<ModeInfo> GetModesByCategory(const std::string& category) const;
    std::vector<ModeInfo> GetBuiltInModes() const;
    std::vector<ModeInfo> GetCustomModes() const;
    std::vector<std::string> GetCategories() const;

    // Filtering
    std::vector<ModeInfo> FilterModes(int playerCount) const;
    std::vector<ModeInfo> SearchModes(const std::string& query) const;

    // Validation
    bool ValidateMode(const std::string& id, std::vector<std::string>& errors) const;

    // Custom mode loading
    bool LoadCustomMode(const std::string& filepath);
    bool SaveCustomMode(const GameMode& mode, const std::string& filepath);

    // Events
    std::function<void(const std::string& modeId)> OnModeRegistered;
    std::function<void(const std::string& modeId)> OnModeUnregistered;

private:
    ModeRegistry() = default;
    ~ModeRegistry() = default;
    ModeRegistry(const ModeRegistry&) = delete;
    ModeRegistry& operator=(const ModeRegistry&) = delete;

    void RegisterBuiltInModes();
    ModeInfo CreateModeInfoFromFile(const std::string& filepath);

    std::unordered_map<std::string, ModeInfo> m_modes;
    std::vector<std::string> m_customModeDirectories;
    bool m_initialized = false;
};

// Macro for easy mode registration
#define REGISTER_GAME_MODE(ModeClass, ModeId) \
    namespace { \
        struct ModeClass##Registrar { \
            ModeClass##Registrar() { \
                ModeRegistry::Instance().RegisterModeType<ModeClass>(ModeId); \
            } \
        }; \
        static ModeClass##Registrar s_##ModeClass##Registrar; \
    }

} // namespace Vehement
